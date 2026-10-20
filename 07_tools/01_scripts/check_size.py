#!/usr/bin/env python3
"""Guardas de tamaño del firmware: falla si la IRAM o la app pasan su límite.

Usa el build de ESP-IDF (correr con el entorno activo: get_idf). Si existe GITHUB_STEP_SUMMARY,
agrega la tabla al resumen del job de GitHub Actions.

    python 07_tools/01_scripts/check_size.py
    python 07_tools/01_scripts/check_size.py --iram-max 85 --app-max 80
"""

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEFAULT_BUILD = REPO / "03_firmware" / "build"

# Límites de CI/CD en el documento vivo: la IRAM ya se llenó una vez (LVGL en IRAM no cupo).
DEFAULT_IRAM_MAX_PCT = 85.0
DEFAULT_APP_MAX_PCT = 80.0


def memory_usage(map_file: Path) -> dict:
    out = subprocess.run(
        [sys.executable, "-m", "esp_idf_size", "--format", "json2", str(map_file)],
        capture_output=True, text=True, check=True,
    )
    return {m["name"]: m for m in json.loads(out.stdout)["layout"]}


def smallest_app_partition(build: Path) -> int:
    sys.path.insert(0, str(Path(os.environ["IDF_PATH"]) / "components" / "partition_table"))
    import gen_esp32part

    table = gen_esp32part.PartitionTable.from_binary((build / "partition_table" / "partition-table.bin").read_bytes())
    return min(p.size for p in table if p.type == gen_esp32part.APP_TYPE)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--build-dir", type=Path, default=DEFAULT_BUILD)
    p.add_argument("--iram-max", type=float, default=DEFAULT_IRAM_MAX_PCT, help="%% máximo de IRAM")
    p.add_argument("--app-max", type=float, default=DEFAULT_APP_MAX_PCT, help="%% máximo de la partición de app")
    args = p.parse_args()
    if "IDF_PATH" not in os.environ:
        sys.exit("IDF_PATH no está definido: activa ESP-IDF (get_idf)")

    build = args.build_dir
    desc = json.loads((build / "project_description.json").read_text())
    mem = memory_usage(build / f"{desc['project_name']}.map")
    app_bytes = (build / desc["app_bin"]).stat().st_size
    part_bytes = smallest_app_partition(build)

    iram = mem["IRAM"]
    rows = [
        ("IRAM", iram["used"], iram["total"], args.iram_max),
        ("DRAM", mem["DRAM"]["used"], mem["DRAM"]["total"], None),
        ("App / partición", app_bytes, part_bytes, args.app_max),
    ]

    lines = ["| Memoria | Usado | Total | % | Límite |", "| --- | --- | --- | --- | --- |"]
    failed = []
    for name, used, total, limit in rows:
        pct = 100.0 * used / total
        over = limit is not None and pct > limit
        if over:
            failed.append(f"{name} en {pct:.1f} % (límite {limit:.0f} %)")
        mark = "" if limit is None else (f"{limit:.0f} % ❌" if over else f"{limit:.0f} % ✅")
        lines.append(f"| {name} | {used:,} B | {total:,} B | {pct:.1f} % | {mark} |")

    report = "\n".join(["### Tamaño del firmware", "", *lines, ""])
    print(report)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as f:
            f.write(report + "\n")

    if failed:
        print("Fuera de límite: " + "; ".join(failed), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
