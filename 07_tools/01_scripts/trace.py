#!/usr/bin/env python3
"""Matriz de trazabilidad SYS → SW → test, a partir de software.md y de los comentarios de los tests.

Lee las tablas de 01_docs/01_requirements/software.md y los `/* Verifica: SW-nnn */` que van en la línea
anterior a cada TEST(...) de 03_firmware/test/. Imprime la matriz en Markdown y, si existe
GITHUB_STEP_SUMMARY, la agrega al resumen del job de GitHub Actions. Sale con 1 si encuentra un error:

- un test sin `Verifica` o que cita un SW-xx que no existe o está Eliminado;
- un TEST(...) que su TEST_GROUP_RUNNER no corre (citaría algo que nunca se prueba);
- un SW-xx Verificado sin test ni prueba en la tarjeta registrada (`PR #nn` en su verificación), o uno
  Implementado sin test en un componente que se prueba en host;
- una fila mal formada: ID repetido, estado desconocido o traza que no es SYS-xx ni `derivado`.

    python 07_tools/01_scripts/trace.py
    python 07_tools/01_scripts/trace.py --software otro.md --tests otra/carpeta
"""

import argparse
import os
import re
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEFAULT_SOFTWARE = REPO / "01_docs" / "01_requirements" / "software.md"
DEFAULT_TESTS = REPO / "03_firmware" / "test"

COLUMNS = ["ID", "Requisito", "Traza", "Componente", "Verificación", "Estado", "Razón"]
STATES = {"Propuesto", "Implementado", "Verificado", "Eliminado"}
# Lo que compila test/host: ahí un requisito implementado no tiene excusa para no tener test.
HOST_COMPONENTS = {"vehicle_logic"}

SW_ID = re.compile(r"SW-\d{3}")
SYS_ID = re.compile(r"SYS-\d{2}")
ROW = re.compile(r"^\|\s*SW-\d{3}\s*\|")
BOARD_RECORD = re.compile(r"PR #\d+")
VERIFIES = re.compile(r"/\*\s*Verifica:([^*]*)\*/\s*\n\s*TEST\(\s*(\w+)\s*,\s*(\w+)\s*\)")
TEST_DEF = re.compile(r"^TEST\(\s*(\w+)\s*,\s*(\w+)\s*\)", re.M)
TEST_RUN = re.compile(r"RUN_TEST_CASE\(\s*(\w+)\s*,\s*(\w+)\s*\)")


@dataclass
class Requirement:
    id: str
    trace: list[str]
    component: str
    verification: str
    state: str
    tests: list[str] = field(default_factory=list)


def read_requirements(path: Path, errors: list[str]) -> dict[str, Requirement]:
    reqs: dict[str, Requirement] = {}
    for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        # Solo las filas de requisitos: la tabla de rangos (SW-1xx) no empieza con tres dígitos.
        if not ROW.match(line):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        where = f"{path.name}:{n}"
        if len(cells) != len(COLUMNS):
            errors.append(f"{where}: {len(cells)} columnas en lugar de {len(COLUMNS)} (¿un `|` dentro de una celda?)")
            continue
        sw_id, _, trace, component, verification, state, _ = cells
        if sw_id in reqs:
            errors.append(f"{where}: {sw_id} repetido")
            continue
        if state not in STATES:
            errors.append(f"{where}: {sw_id} con estado desconocido: {state}")
        parents = [t.strip() for t in trace.split(",")]
        if parents != ["derivado"] and not all(SYS_ID.fullmatch(t) for t in parents):
            errors.append(f"{where}: {sw_id} con traza inválida: {trace}")
        reqs[sw_id] = Requirement(sw_id, parents, component, verification, state)
    return reqs


def read_tests(root: Path, reqs: dict[str, Requirement], errors: list[str]) -> list[str]:
    names = []
    for path in sorted(root.rglob("*.c")):
        if "build" in path.relative_to(root).parts:
            continue
        text = path.read_text(encoding="utf-8")
        rel = path.relative_to(REPO) if path.is_relative_to(REPO) else path
        cited = {(g, t): SW_ID.findall(ids) for ids, g, t in VERIFIES.findall(text)}
        runs = set(TEST_RUN.findall(text))
        for group, test in TEST_DEF.findall(text):
            name = f"{group}.{test}"
            names.append(name)
            if (group, test) not in runs:
                errors.append(f"{rel}: {name} no está en su TEST_GROUP_RUNNER")
            ids = cited.get((group, test))
            if not ids:
                errors.append(f"{rel}: {name} sin /* Verifica: SW-nnn */ en la línea anterior")
                continue
            for sw_id in ids:
                req = reqs.get(sw_id)
                if req is None:
                    errors.append(f"{rel}: {name} cita {sw_id}, que no existe en software.md")
                elif req.state == "Eliminado":
                    errors.append(f"{rel}: {name} cita {sw_id}, que está Eliminado")
                else:
                    req.tests.append(name)
    return names


def check_states(reqs: dict[str, Requirement], errors: list[str]) -> None:
    for r in reqs.values():
        if r.tests:
            continue
        if r.state == "Verificado" and not BOARD_RECORD.search(r.verification):
            errors.append(f"{r.id} está Verificado pero ningún test lo cita ni registra una prueba en la tarjeta (PR #nn)")
        elif r.state == "Implementado" and r.component in HOST_COMPONENTS:
            errors.append(f"{r.id} está Implementado en {r.component} y no tiene test en host")


def sys_key(parent: str) -> tuple[int, str]:
    # Los SYS en orden numérico; los derivados al final.
    return (int(parent[4:]), parent) if parent.startswith("SYS-") else (10**6, parent)


def render(reqs: dict[str, Requirement], tests: list[str], errors: list[str]) -> str:
    by_state = defaultdict(int)
    for r in reqs.values():
        by_state[r.state] += 1
    states = ", ".join(f"{by_state[s]} {s.lower()}s" for s in ["Verificado", "Implementado", "Propuesto"] if by_state[s])
    out = [
        "### Trazabilidad SYS → SW → test",
        "",
        f"{len(reqs)} SW-xx ({states}); {len(tests)} tests en host.",
        "",
        "| SYS | SW | Componente | Estado | Tests |",
        "| --- | --- | --- | --- | --- |",
    ]
    rows = [(p, r) for r in reqs.values() for p in r.trace]
    for parent, r in sorted(rows, key=lambda pr: (sys_key(pr[0]), pr[1].id)):
        cited = "<br>".join(f"`{t}`" for t in sorted(set(r.tests))) or "—"
        out.append(f"| {parent} | {r.id} | {r.component} | {r.state} | {cited} |")

    untested = [r for r in reqs.values() if not r.tests and r.state != "Eliminado"]
    if untested:
        out += ["", "**Sin test en host** (se verifican en la tarjeta o están pendientes): "
                + ", ".join(f"{r.id} ({r.component}, {r.state.lower()})" for r in untested)]
    if errors:
        out += ["", f"**{len(errors)} errores:**", "", *[f"- {e}" for e in errors]]
    return "\n".join(out) + "\n"


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--software", type=Path, default=DEFAULT_SOFTWARE)
    p.add_argument("--tests", type=Path, default=DEFAULT_TESTS)
    args = p.parse_args()

    errors: list[str] = []
    reqs = read_requirements(args.software, errors)
    tests = read_tests(args.tests, reqs, errors)
    check_states(reqs, errors)

    report = render(reqs, tests, errors)
    print(report)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as f:
            f.write(report)

    if errors:
        print(f"Trazabilidad con {len(errors)} errores:", *errors, sep="\n  ", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
