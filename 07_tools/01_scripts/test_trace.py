#!/usr/bin/env python3
"""Pruebas de trace.py: un caso que pasa y uno por cada error que debe detectar.

    python -m unittest discover -s 07_tools/01_scripts -p 'test_*.py'
"""

import importlib.util
import tempfile
import textwrap
import unittest
from pathlib import Path

# Por ruta: `import trace` podría traer el módulo trace de la biblioteca estándar.
_spec = importlib.util.spec_from_file_location("trace_matrix", Path(__file__).with_name("trace.py"))
tr = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(tr)

HEADER = """\
| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
"""

GOOD_REQS = {
    "SW-101": "| SW-101 | El firmware debe A. | SYS-02 | vehicle_logic | Prueba. | Verificado | R. |",
    "SW-102": "| SW-102 | El firmware debe B. | derivado | vehicle_logic | Prueba. | Implementado | R. |",
    "SW-501": "| SW-501 | El firmware debe C. | SYS-03, SYS-04 | ui | Demostración. | Implementado | R. |",
}

GOOD_TEST = """\
/* Verifica: SW-101 */
TEST(boost, a)
{
}

/* Verifica: SW-101, SW-102 */
TEST(boost, b)
{
}

TEST_GROUP_RUNNER(boost)
{
    RUN_TEST_CASE(boost, a);
    RUN_TEST_CASE(boost, b);
}
"""


class TraceTest(unittest.TestCase):
    def run_trace(self, reqs: dict[str, str], test_c: str) -> tuple[dict, list[str]]:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            software = root / "software.md"
            # La tabla de rangos no son requisitos: no debe leerse como fila.
            software.write_text("| SW-1xx | Medidor de boost |\n\n" + HEADER + "\n".join(reqs.values()) + "\n")
            tests = root / "test"
            tests.mkdir()
            (tests / "test_x.c").write_text(textwrap.dedent(test_c))
            errors: list[str] = []
            parsed = tr.read_requirements(software, errors)
            tr.read_tests(tests, parsed, errors)
            tr.check_states(parsed, errors)
            return parsed, errors

    def assert_one_error(self, errors: list[str], text: str) -> None:
        self.assertEqual(1, len(errors), errors)
        self.assertIn(text, errors[0])

    def test_good_input_has_no_errors(self):
        reqs, errors = self.run_trace(GOOD_REQS, GOOD_TEST)
        self.assertEqual([], errors)
        self.assertEqual(["boost.a", "boost.b"], reqs["SW-101"].tests)
        self.assertEqual(["SYS-03", "SYS-04"], reqs["SW-501"].trace)

    def test_test_without_verifica_fails(self):
        _, errors = self.run_trace(GOOD_REQS, GOOD_TEST.replace("/* Verifica: SW-101, SW-102 */\n", ""))
        # Además, SW-102 se queda sin test.
        self.assertTrue(any("boost.b sin /* Verifica" in e for e in errors), errors)

    def test_unknown_sw_fails(self):
        _, errors = self.run_trace(GOOD_REQS, GOOD_TEST.replace("SW-101 */\nTEST(boost, a)", "SW-999 */\nTEST(boost, a)"))
        self.assert_one_error(errors, "cita SW-999, que no existe")

    def test_deleted_sw_cited_fails(self):
        reqs = dict(GOOD_REQS, **{"SW-103": GOOD_REQS["SW-101"].replace("SW-101", "SW-103").replace("Verificado", "Eliminado")})
        test = GOOD_TEST.replace("SW-101 */\nTEST(boost, a)", "SW-101, SW-103 */\nTEST(boost, a)")
        _, errors = self.run_trace(reqs, test)
        self.assert_one_error(errors, "cita SW-103, que está Eliminado")

    def test_test_not_in_runner_fails(self):
        _, errors = self.run_trace(GOOD_REQS, GOOD_TEST.replace("    RUN_TEST_CASE(boost, a);\n", ""))
        self.assert_one_error(errors, "boost.a no está en su TEST_GROUP_RUNNER")

    def test_verified_without_test_fails(self):
        reqs = dict(GOOD_REQS, **{"SW-501": GOOD_REQS["SW-501"].replace("Implementado", "Verificado")})
        _, errors = self.run_trace(reqs, GOOD_TEST)
        self.assert_one_error(errors, "SW-501 está Verificado pero ningún test lo cita")

    def test_verified_on_board_with_pr_is_allowed(self):
        row = GOOD_REQS["SW-501"].replace("Demostración.", "Demostración en la tarjeta (PR #42).")
        reqs = dict(GOOD_REQS, **{"SW-501": row.replace("Implementado", "Verificado")})
        _, errors = self.run_trace(reqs, GOOD_TEST)
        self.assertEqual([], errors)

    def test_implemented_host_component_without_test_fails(self):
        test = GOOD_TEST.replace("SW-101, SW-102 */", "SW-101 */")
        _, errors = self.run_trace(GOOD_REQS, test)
        self.assert_one_error(errors, "SW-102 está Implementado en vehicle_logic y no tiene test en host")

    def test_implemented_ui_without_test_is_allowed(self):
        _, errors = self.run_trace(GOOD_REQS, GOOD_TEST)
        self.assertFalse(any("SW-501" in e for e in errors), errors)

    def test_bad_trace_fails(self):
        reqs = dict(GOOD_REQS, **{"SW-501": GOOD_REQS["SW-501"].replace("SYS-03, SYS-04", "SYS-3")})
        _, errors = self.run_trace(reqs, GOOD_TEST)
        self.assert_one_error(errors, "SW-501 con traza inválida: SYS-3")

    def test_unknown_state_fails(self):
        reqs = dict(GOOD_REQS, **{"SW-501": GOOD_REQS["SW-501"].replace("Implementado", "Hecho")})
        _, errors = self.run_trace(reqs, GOOD_TEST)
        self.assert_one_error(errors, "SW-501 con estado desconocido: Hecho")

    def test_duplicate_id_fails(self):
        reqs = dict(GOOD_REQS, dup=GOOD_REQS["SW-501"])
        _, errors = self.run_trace(reqs, GOOD_TEST)
        self.assert_one_error(errors, "SW-501 repetido")

    def test_row_after_blank_line_fails(self):
        reqs = dict(GOOD_REQS)
        reqs["SW-501"] = "\n" + reqs["SW-501"]
        _, errors = self.run_trace(reqs, GOOD_TEST)
        self.assert_one_error(errors, "fila fuera de su tabla")

    def test_pipe_inside_cell_fails(self):
        reqs = dict(GOOD_REQS, **{"SW-501": GOOD_REQS["SW-501"].replace("R. |", "R \\| x. |")})
        _, errors = self.run_trace(reqs, GOOD_TEST)
        self.assert_one_error(errors, "8 columnas en lugar de 7")


GOOD_HW = {
    "HW-101": "| HW-101 | El hardware debe A. | SYS-10 | fuente | Prueba con fuente. | Propuesto | R. |",
    "HW-301": "| HW-301 | El hardware debe B. | SYS-01, SYS-02 | can | Inspección. | Implementado | R. |",
}


class TraceHardwareTest(unittest.TestCase):
    def run_hw(self, rows: dict[str, str]) -> tuple[dict, list[str]]:
        with tempfile.TemporaryDirectory() as tmp:
            hardware = Path(tmp) / "hardware.md"
            # La tabla de rangos no son requisitos: no debe leerse como fila.
            hardware.write_text("| HW-1xx | Alimentación |\n\n" + HEADER + "\n".join(rows.values()) + "\n")
            errors: list[str] = []
            hw = tr.read_requirements(hardware, errors, "HW")
            tr.check_hardware(hw, errors)
            return hw, errors

    def test_good_input_has_no_errors(self):
        hw, errors = self.run_hw(GOOD_HW)
        self.assertEqual([], errors)
        self.assertEqual(["HW-101", "HW-301"], sorted(hw))
        self.assertEqual(["SYS-01", "SYS-02"], hw["HW-301"].trace)

    def test_verified_without_pr_fails(self):
        _, errors = self.run_hw(dict(GOOD_HW, **{"HW-101": GOOD_HW["HW-101"].replace("Propuesto", "Verificado")}))
        self.assertEqual(1, len(errors), errors)
        self.assertIn("HW-101 está Verificado pero no registra la prueba o el análisis (PR #nn)", errors[0])

    def test_verified_with_pr_is_allowed(self):
        row = GOOD_HW["HW-101"].replace("Prueba con fuente.", "Prueba con fuente (PR #42).")
        _, errors = self.run_hw(dict(GOOD_HW, **{"HW-101": row.replace("Propuesto", "Verificado")}))
        self.assertEqual([], errors)

    def test_bad_trace_names_the_file(self):
        _, errors = self.run_hw(dict(GOOD_HW, **{"HW-101": GOOD_HW["HW-101"].replace("SYS-10", "SW-101")}))
        self.assertEqual(1, len(errors), errors)
        self.assertIn("hardware.md:", errors[0])
        self.assertIn("HW-101 con traza inválida: SW-101", errors[0])

    def test_matrix_lists_hw_and_sys_without_children(self):
        hw, _ = self.run_hw(GOOD_HW)
        reqs, _ = TraceTest().run_trace(GOOD_REQS, GOOD_TEST)
        # SW: SYS-02, 03, 04; HW: SYS-01, 02, 10 → faltan SYS-05 a SYS-09.
        self.assertEqual([f"SYS-0{n}" for n in range(5, 10)], tr.uncovered_sys(reqs, hw))
        report = tr.render(reqs, hw, ["boost.a", "boost.b"], [])
        self.assertIn("| SYS-10 | HW-101 | fuente | Propuesto | — |", report)
        self.assertIn("**SYS-xx sin SW-xx ni HW-xx**", report)

    def test_deleted_requirement_does_not_cover_its_sys(self):
        hw, _ = self.run_hw(dict(GOOD_HW, **{"HW-101": GOOD_HW["HW-101"].replace("Propuesto", "Eliminado")}))
        sw = {"SW-901": tr.Requirement("SW-901", ["SYS-11"], "main", "Prueba.", "Implementado")}
        self.assertIn("SYS-10", tr.uncovered_sys(sw, hw))

if __name__ == "__main__":
    unittest.main()
