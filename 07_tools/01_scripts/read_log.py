#!/usr/bin/env python3
"""Lee el log del firmware, en vivo por serial o desde un archivo guardado.

- Colorea por nivel (E/W/I/D/V) y filtra por tag o por nivel mínimo.
- Decodifica los backtraces con addr2line y el ELF del build.
- Guarda el log crudo con fecha y hora para revisarlo después.
- Al terminar resume errores, avisos, reinicios, watchdogs y panics.

Ejemplos:
    python 07_tools/01_scripts/read_log.py                               # en vivo, /dev/ttyUSB0
    python 07_tools/01_scripts/read_log.py --reset --save                # reinicia la tarjeta y guarda en 06_testing/02_bench/logs/
    python 07_tools/01_scripts/read_log.py --file run.log --level W      # archivo, solo avisos y errores
    python 07_tools/01_scripts/read_log.py --file run.log --tag sim --tag app
"""

import argparse
import datetime as dt
import functools
import hashlib
import re
import shutil
import subprocess
import sys
import time
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEFAULT_ELF = REPO / "03_firmware" / "build" / "esp32_dashboard.elf"
DEFAULT_SAVE_DIR = REPO / "06_testing" / "02_bench" / "logs"
ADDR2LINE = "xtensa-esp32-elf-addr2line"

ANSI = re.compile(r"\x1b\[[0-9;]*m")
# Formato de ESP_LOG: "E (37533) task_wdt: mensaje"
ESP_LOG = re.compile(r"^(?P<level>[EWIDV]) \((?P<ms>\d+)\) (?P<tag>[^:]+): (?P<msg>.*)$")
# Pares PC:SP de un backtrace de Xtensa
BACKTRACE_PC = re.compile(r"(0x4[0-9a-fA-F]{7}):0x[0-9a-fA-F]{8}")
# Prefijo de hora que agrega --save; al leer el archivo se separa y se usa como hora de la línea
SAVED_STAMP = re.compile(r"^(\d\d:\d\d:\d\d\.\d{3}) (.*)$")
# Pulso en EN para reiniciar con --reset; el RC del auto-reset necesita tiempo para descargarse
RESET_PULSE_S = 0.2
# ESP-IDF imprime al arrancar el inicio del SHA256 del ELF que se flasheó
ELF_SHA = re.compile(r"ELF file SHA256:\s*([0-9a-fA-F]+)")

LEVELS = "EWIDV"
COLORS = {"E": "\x1b[31m", "W": "\x1b[33m", "I": "\x1b[32m", "D": "\x1b[36m", "V": "\x1b[37m"}
BOLD_RED = "\x1b[1;31m"
DIM = "\x1b[2m"
RESET = "\x1b[0m"

# Eventos que conviene contar aunque no vengan con formato ESP_LOG.
EVENTS = {
    "reinicio": re.compile(r"^rst:0x"),
    "watchdog": re.compile(r"task_wdt: Task watchdog got triggered|Interrupt wdt timeout"),
    "panic": re.compile(r"Guru Meditation Error|abort\(\) was called|assert failed"),
    "lvgl": re.compile(r"\[(Warn|Error)\].*lv_"),
}


class Decoder:
    """Convierte direcciones de un backtrace en función y archivo:línea."""

    def __init__(self, elf: Path):
        self.elf = elf
        self.tool = shutil.which(ADDR2LINE)
        self.verified = False
        self.mismatch = None
        self._local_sha = None

    @property
    def enabled(self) -> bool:
        return bool(self.tool) and self.elf.is_file() and self.mismatch is None

    def reason(self) -> str:
        if not self.tool:
            return f"{ADDR2LINE} no está en el PATH (activa ESP-IDF con get_idf)"
        if self.mismatch:
            return self.mismatch
        return f"no existe el ELF {self.elf}"

    def check_sha(self, flashed: str) -> None:
        """Las direcciones solo valen con el mismo ELF que se flasheó."""
        if not self.tool or not self.elf.is_file():
            return
        if self._local_sha is None:
            with self.elf.open("rb") as f:
                self._local_sha = hashlib.file_digest(f, "sha256").hexdigest()
        # Se reevalúa en cada arranque: un reflasheo con el ELF correcto vuelve a habilitar la decodificación.
        match = self._local_sha.startswith(flashed.lower())
        self.verified = match
        self.mismatch = None if match else (
            f"el ELF local ({self._local_sha[:len(flashed)]}) no es el flasheado ({flashed}); "
            "recompila con el mismo código o pasa el ELF correcto con --elf")

    def decode(self, addrs: list[str]) -> list[str]:
        return self._decode(tuple(addrs))

    @functools.lru_cache(maxsize=64)
    def _decode(self, addrs: tuple[str, ...]) -> list[str]:
        # En un bucle de panics el mismo backtrace se repite: addr2line tarda en leer el ELF cada vez.
        out = subprocess.run(
            [self.tool, "-pfiaC", "-e", str(self.elf), *addrs],
            capture_output=True, text=True, check=False,
        )
        return [line for line in out.stdout.splitlines() if line.strip()]


class Printer:
    def __init__(self, args, decoder: Decoder, color: bool):
        self.tags = set(args.tag or [])
        self.max_level = LEVELS.index(args.level)
        self.decoder = decoder
        self.color = color
        self.levels = Counter()
        self.events = Counter()
        self.first_seen = {}

    def paint(self, text: str, code: str) -> str:
        return f"{code}{text}{RESET}" if self.color else text

    def handle(self, raw: str, stamp: str) -> None:
        line = ANSI.sub("", raw).rstrip("\r\n")
        if not line:
            return
        sha = ELF_SHA.search(line)
        if sha:
            self.decoder.check_sha(sha.group(1))
        is_event = False
        for name, pattern in EVENTS.items():
            if pattern.search(line):
                is_event = True
                self.events[name] += 1
                self.first_seen.setdefault(name, f"{stamp} {line}")
        # Un crash se muestra siempre, aunque haya filtro de tag o de nivel.
        always = is_event or "Backtrace:" in line

        m = ESP_LOG.match(line)
        if m:
            level = m["level"]
            self.levels[level] += 1
            if not always and LEVELS.index(level) > self.max_level:
                return
            if not always and self.tags and m["tag"].strip() not in self.tags:
                return
            print(f"{self.paint(stamp, DIM)} {self.paint(line, COLORS[level])}")
            return
        if self.tags and not always:
            return
        print(f"{self.paint(stamp, DIM)} {self.paint(line, BOLD_RED) if is_event else line}")

        if "Backtrace:" in line and self.decoder.enabled:
            addrs = BACKTRACE_PC.findall(line)
            for decoded in self.decoder.decode(addrs):
                print(self.paint(f"    --- {decoded}", COLORS["W"]))

    def summary(self) -> None:
        print()
        print("Resumen")
        print(f"  errores: {self.levels['E']}  avisos: {self.levels['W']}  info: {self.levels['I']}")
        for name in EVENTS:
            if self.events[name]:
                print(f"  {name}: {self.events[name]}  (primero: {self.first_seen[name]})")
        if not self.decoder.enabled:
            print(f"  backtraces sin decodificar: {self.decoder.reason()}")
        elif self.events["watchdog"] or self.events["panic"]:
            if not self.decoder.verified:
                print("  ojo: el log no trae el SHA256 del ELF; la decodificación supone que el ELF local es el flasheado")


def join_backtrace(entries):
    """Une el backtrace cuando la terminal lo partió en varias líneas. entries: pares (hora, línea)."""
    pending = None
    for stamp, line in entries:
        if pending is not None:
            if re.match(r"^\s*[0-9a-fA-Fx:]+(\s|$)", line) and "(" not in line:
                pending = (pending[0], pending[1].rstrip("\r\n") + line.strip() + "\n")
                continue
            yield pending
            pending = None
        if "Backtrace:" in line:
            pending = (stamp, line)
        else:
            yield stamp, line
    if pending is not None:
        yield pending


def file_entries(f):
    """Líneas de un log; si lo guardó --save, separa la hora del prefijo."""
    for n, raw in enumerate(f, 1):
        saved = SAVED_STAMP.match(raw.rstrip("\r\n"))
        if saved:
            yield saved.group(1), saved.group(2) + "\n"
        else:
            yield f"#{n:05d}", raw


def read_file(path: Path, printer: Printer) -> None:
    with path.open(encoding="utf-8", errors="replace") as f:
        for stamp, line in join_backtrace(file_entries(f)):
            printer.handle(line, stamp)


def read_serial(args, printer: Printer) -> None:
    try:
        import serial
    except ImportError:
        sys.exit("Falta pyserial: pip install -r 07_tools/01_scripts/requirements.txt (o usa el python de ESP-IDF tras get_idf)")

    save = None
    if args.save is not None:
        target = Path(args.save) if args.save else DEFAULT_SAVE_DIR / f"{dt.datetime.now():%Y%m%d-%H%M%S}.log"
        target.parent.mkdir(parents=True, exist_ok=True)
        save = target.open("w", encoding="utf-8")
        print(f"Guardando en {target}")

    port = serial.Serial(args.port, args.baud, timeout=0.2)
    if args.reset:
        # Igual que idf.py monitor: RTS controla EN a través del circuito de auto-reset.
        port.dtr = False
        port.rts = True
        time.sleep(RESET_PULSE_S)
        port.rts = False

    print(f"Leyendo {args.port} a {args.baud} baudios; Ctrl+C para salir")
    try:
        while True:
            raw = port.readline()
            if not raw.endswith(b"\n"):
                continue  # timeout sin línea completa
            line = raw.decode("utf-8", errors="replace")
            stamp = dt.datetime.now().time().isoformat(timespec="milliseconds")
            if save:
                save.write(f"{stamp} {ANSI.sub('', line)}")
            printer.handle(line, stamp)
    except KeyboardInterrupt:
        pass
    finally:
        port.close()
        if save:
            save.close()


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    src = p.add_mutually_exclusive_group()
    src.add_argument("--port", default="/dev/ttyUSB0", help="puerto serie (por defecto /dev/ttyUSB0)")
    src.add_argument("--file", type=Path, help="leer un log guardado en lugar del puerto")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--reset", action="store_true", help="reiniciar la tarjeta al conectar")
    p.add_argument("--save", nargs="?", const="", help="guardar el log crudo (ruta opcional)")
    p.add_argument("--tag", action="append", help="mostrar solo este tag de ESP_LOG (se puede repetir)")
    p.add_argument("--level", choices=list(LEVELS), default="V", help="nivel máximo a mostrar (E < W < I < D < V)")
    p.add_argument("--elf", type=Path, default=DEFAULT_ELF, help="ELF para decodificar backtraces")
    p.add_argument("--no-color", action="store_true")
    args = p.parse_args()

    printer = Printer(args, Decoder(args.elf), color=not args.no_color and sys.stdout.isatty())
    try:
        if args.file:
            read_file(args.file, printer)
        else:
            read_serial(args, printer)
    finally:
        printer.summary()


if __name__ == "__main__":
    main()
