"""Test di tools/check_m2_trace.py su registro, matrice, junit e record sintetici."""
import importlib.util
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
WORK = os.path.join(ROOT, "tmp", "check-m2-trace-test")


def load():
    spec = importlib.util.spec_from_file_location("check_m2_trace", os.path.join(ROOT, "tools", "check_m2_trace.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


PLAN = """### Appendice D. Registro

#### D.1 NODI

| ID | Classe | Priorità | Stadio | Descrizione |
|---|---|---|---|---|
| F-A | F | P0 | M2.a | a |
| F-B | F-slice | P0 | M2.b | b |
| ADR-X | ADR | R | M2.entry | x |
| D-Y | Decisione | R | M2.entry | y |
| M2.a | Delivery | P0 | M2.a | agg |
| M2.z | Delivery | P0 | M2.z | vuoto |
| F-Z | F | P0 | M3.a | z |
| F-W | F | P0 | M3.a | w |

#### D.6 ARCHI

| Da | Tipo | A | Predicato |
|---|---|---|---|
| F-B | requires_all | F-A | - |
| F-B | decided_by | ADR-X | - |
| M2.a | requires_all | F-A | - |
| D-Y | requires_all | ADR-X | - |
| F-Z | requires_all | F-B | - |
| F-A | requires_all | F-W | - |

### Altro
"""

HEAD = ("| ID | Stadio | Reg. | Acc. | Test eseguiti | Test pianificati | Record | Stato | Unità |\n"
        "|---|---|---|---|---|---|---|---|---|\n")
GATE = "`M2-gate.md`"


def row(i, stadio, tests, record, state):
    return f"| {i} | {stadio} | 1 | 2 | {tests} | — | {record} | {state} | R0 |\n"


def rows(**over):
    base = {
        "F-A": row("F-A", "M2.a", "T-X-01, 02", f"`M2-a.md`, {GATE}", "CHIUSO"),
        "F-B": row("F-B", "M2.b", "T-X-03…04", GATE, "CHIUSO (gate)"),
        "ADR-X": row("ADR-X", "M2.entry", "viewer M1.b", f"`M1-closure-001.md`, {GATE}", "VERIFICATO"),
        "D-Y": row("D-Y", "M2.entry", "`S.T_X_09_*`", GATE, "VERIFICATO"),
        "M2.a": row("M2.a", "M2.a", "aggregato", GATE, "CHIUSO"),
        "M2.z": row("M2.z", "M2.z", "aggregato", GATE, "CHIUSO"),
    }
    base.update(over)
    return base


def matrix(**over):
    return "## ID del piano → test\n\ntesto\n\n" + HEAD + "".join(rows(**over).values()) + "\nfine\n"


LISTED = {"S.T_X_01_a", "S.T_X_02_a", "S.T_X_03_a", "S.T_X_04_a", "S.T_X_09_a", "S.T_X_09_b"}


def junit(names=None, fail=()):
    names = sorted(LISTED if names is None else names)
    body = "".join(
        f'<testcase name="{n}" status="run">' + ("<failure/>" if n in fail else "") + "</testcase>" for n in names)
    return f"<testsuite>{body}</testsuite>"


class GateTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.m = load()
        cls.ev = os.path.join(WORK, "ev")
        cls.plan = os.path.join(WORK, "plan.md")
        os.makedirs(cls.ev, exist_ok=True)
        with open(cls.plan, "w", encoding="utf-8") as fh:
            fh.write(PLAN)
        for n in ("M1-closure-001.md", "M2-a.md", "M2-gate.md"):
            with open(os.path.join(cls.ev, n), "w", encoding="utf-8") as fh:
                fh.write("x\n")

    def write_junit(self, name, body):
        path = os.path.join(WORK, name)
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(body)
        return path

    def gate(self, mx, junits=None, listed=None):
        listed = LISTED if listed is None else listed
        junits = junits or [junit(), junit()]
        runs = [(listed, [self.write_junit(f"j{k}.xml", b)]) for k, b in enumerate(junits)]
        return self.m.evaluate_gate(self.plan, self.m.parse_matrix(mx), runs, self.ev, m1_ids=("ADR-X",))

    def test_all_green(self):
        ids, problems = self.gate(matrix())
        self.assertEqual(ids, ["F-A", "F-B", "ADR-X", "D-Y", "M2.a", "M2.z"])
        self.assertEqual(list(problems), ["M2.z"])

    def test_extract_tokens(self):
        t = self.m.extract_tokens("T-X-01, 02; T-Y-05…07 (nota 12), `S.T_X_09_*`, `T-Z-01.neg`, viewer M1.b", LISTED)
        self.assertEqual(t, ["T-X-01", "T-X-02", "T-Y-05…07", "T-Z-01.neg", "S.T_X_09_a", "S.T_X_09_b"])

    def test_extract_mixed_list(self):
        t = self.m.extract_tokens("T-M2E-01…03, 08…11, 18, 19; T-Q-02…04 (T-Q-07, 09)", set())
        self.assertEqual(t, ["T-M2E-01…03", "T-M2E-08…11", "T-M2E-18", "T-M2E-19", "T-Q-02…04", "T-Q-07", "T-Q-09"])

    def test_state_parse(self):
        p = self.m.parse_state
        self.assertEqual(p("CHIUSO (gate)"), "CHIUSO")
        self.assertEqual(p("DA RIESEGUIRE: x CHIUSO"), "DA RIESEGUIRE")
        self.assertEqual(p("ESEGUITO in R3; CHIUSO solo al gate"), "ESEGUITO")

    def test_state_not_final(self):
        mx = matrix(**{"F-A": row("F-A", "M2.a", "T-X-01", GATE, "ESEGUITO in R1; CHIUSO solo al gate")})
        _, pr = self.gate(mx)
        self.assertTrue(any("stato ESEGUITO" in p for p in pr["F-A"]))
        self.assertTrue(any("prerequisito F-A" in p for p in pr["F-B"]))
        self.assertTrue(any("prerequisito F-A" in p for p in pr["M2.a"]))

    def test_decision_needs_verificato(self):
        _, pr = self.gate(matrix(**{"D-Y": row("D-Y", "M2.entry", "`S.T_X_09_*`", GATE, "CHIUSO")}))
        self.assertTrue(any("atteso VERIFICATO" in p for p in pr["D-Y"]))

    def test_functional_needs_chiuso(self):
        _, pr = self.gate(matrix(**{"F-A": row("F-A", "M2.a", "T-X-01", GATE, "VERIFICATO")}))
        self.assertTrue(any("atteso CHIUSO" in p for p in pr["F-A"]))

    def test_missing_row(self):
        r = rows()
        del r["D-Y"]
        mx = "## ID del piano → test\n\n" + HEAD + "".join(r.values())
        _, pr = self.gate(mx)
        self.assertEqual(pr["D-Y"], ["nessuna riga nella matrice"])

    def test_unknown_row(self):
        mx = matrix(**{"F-Z": row("F-Z", "M3.a", "T-X-01", GATE, "CHIUSO")})
        with self.assertRaises(SystemExit) as cm:
            self.gate(mx)
        self.assertIn("F-Z", str(cm.exception))
        self.assertIn("non è dello stadio", str(cm.exception))

    def test_duplicate_row(self):
        mx = matrix() + HEAD + row("F-A", "M2.a", "T-X-01", "x", "CHIUSO")
        with self.assertRaises(SystemExit):
            self.m.parse_matrix(mx)

    def test_malformed_row(self):
        mx = matrix() + "| F-Q | M2.a | solo tre |\n"
        with self.assertRaises(SystemExit):
            self.m.parse_matrix(mx)

    def test_no_test_named(self):
        _, pr = self.gate(matrix(**{"F-A": row("F-A", "M2.a", "—", GATE, "CHIUSO")}))
        self.assertTrue(any("nessun test" in p for p in pr["F-A"]))

    def test_named_test_nonexistent(self):
        _, pr = self.gate(matrix(**{"F-A": row("F-A", "M2.a", "T-X-01, T-Q-77", GATE, "CHIUSO")}))
        self.assertTrue(any("T-Q-77" in p and "ctest -N" in p for p in pr["F-A"]))

    def test_range_with_one_missing_member_is_error(self):
        _, pr = self.gate(matrix(**{"F-A": row("F-A", "M2.a", "T-X-01…05", GATE, "CHIUSO")}))
        self.assertEqual([p for p in pr["F-A"] if "T-X-01…05" in p],
                         ["intervallo T-X-01…05: membri senza test in ctest -N: T-X-05"])

    def test_range_with_all_members_present(self):
        _, pr = self.gate(matrix(**{"F-A": row("F-A", "M2.a", "T-X-01…04", GATE, "CHIUSO")}))
        self.assertNotIn("F-A", pr)

    def test_range_with_several_missing_members_names_all(self):
        _, pr = self.gate(matrix(**{"F-A": row("F-A", "M2.a", "T-X-01…09", GATE, "CHIUSO")}))
        self.assertEqual([p for p in pr["F-A"] if "T-X-01…09" in p],
                         ["intervallo T-X-01…09: membri senza test in ctest -N: T-X-05, T-X-06, T-X-07, T-X-08"])

    def test_range_fully_missing_has_no_test(self):
        _, pr = self.gate(matrix(**{"F-A": row("F-A", "M2.a", "T-Q-01…09", GATE, "CHIUSO")}))
        self.assertTrue(any("nessun test" in p for p in pr["F-A"]))

    def test_failed_in_second_run_only(self):
        _, pr = self.gate(matrix(), junits=[junit(), junit(fail=("S.T_X_03_a",))])
        self.assertTrue(any("non passato" in p for p in pr["F-B"]))

    def test_missing_from_junit(self):
        _, pr = self.gate(matrix(), junits=[junit(), junit(names=LISTED - {"S.T_X_01_a"})])
        self.assertTrue(any("mancano" in p for p in pr["F-A"]))

    def test_m1_record_exception(self):
        _, pr = self.gate(matrix())
        self.assertNotIn("ADR-X", pr)

    def test_exception_only_for_m1_ids(self):
        mx = matrix(**{"D-Y": row("D-Y", "M2.entry", "viewer M1.b", f"`M1-closure-001.md`, {GATE}", "VERIFICATO")})
        _, pr = self.gate(mx)
        self.assertTrue(any("nessun test" in p for p in pr["D-Y"]))

    def test_m1_no_record_cited(self):
        mx = matrix(**{"ADR-X": row("ADR-X", "M2.entry", "viewer M1.b", GATE, "VERIFICATO")})
        _, pr = self.gate(mx)
        self.assertTrue(any("nessun record M1" in p for p in pr["ADR-X"]))

    def test_range_upper_bound_included(self):
        _, pr = self.gate(matrix(), junits=[junit(), junit(fail=("S.T_X_04_a",))])
        self.assertTrue(any("non passato" in p for p in pr["F-B"]))

    def test_unresolved_glob_reported(self):
        _, pr = self.gate(matrix(), listed=LISTED - {"S.T_X_09_a", "S.T_X_09_b"})
        self.assertTrue(any("S.T_X_09_*" in p for p in pr["D-Y"]))

    def test_prereq_outside_stage_not_counted(self):
        ids, pr = self.gate(matrix())
        self.assertNotIn("F-A", pr)

    def test_report_exit_code(self):
        self.assertEqual(self.m.report(["F-A"], {}), 0)
        self.assertEqual(self.m.report(["F-A"], {"F-A": ["x"]}), 1)

    def test_cli_rejects_repeated_build(self):
        with self.assertRaises(SystemExit) as cm:
            b = os.path.join(WORK, "b1")
            os.makedirs(b, exist_ok=True)
            self.m.parse_runs([f"{b}:a.xml", f"{b}:b.xml"])
        self.assertIn("ripetuta", str(cm.exception))

    def test_aggregate_ok_without_named_test(self):
        _, pr = self.gate(matrix(), )
        self.assertNotIn("M2.a", pr)

    def test_aggregate_with_failing_component(self):
        _, pr = self.gate(matrix(), junits=[junit(), junit(fail=("S.T_X_01_a",))])
        self.assertIn("componente F-A non passa", pr["M2.a"])

    def test_aggregate_without_components(self):
        _, pr = self.gate(matrix())
        self.assertEqual(pr["M2.z"], ["aggregato senza componenti M2 nel registro"])

    def test_aggregate_not_chiuso(self):
        _, pr = self.gate(matrix(**{"M2.a": row("M2.a", "M2.a", "aggregato", GATE, "APERTO")}))
        self.assertIn("stato APERTO, atteso CHIUSO", pr["M2.a"])

    def test_empty_range_next_to_valid_token(self):
        mx = matrix(**{"F-A": row("F-A", "M2.a", "T-X-01, T-Q-01…09", GATE, "CHIUSO")})
        _, pr = self.gate(mx)
        self.assertEqual([p for p in pr["F-A"] if "T-Q-01…09" in p], ["intervallo T-Q-01…09: membri senza test in ctest -N: "
                          + ", ".join(f"T-Q-{k:02d}" for k in range(1, 10))])

    def test_inverted_range(self):
        _, pr = self.gate(matrix(**{"F-A": row("F-A", "M2.a", "T-X-01, T-X-04…03", GATE, "CHIUSO")}))
        self.assertTrue(any("invertito" in p for p in pr["F-A"]))

    def test_empty_range_not_hidden_by_m1_record(self):
        mx = matrix(**{"ADR-X": row("ADR-X", "M2.entry", "T-Q-01…09", f"`M1-closure-001.md`, {GATE}", "VERIFICATO")})
        _, pr = self.gate(mx)
        self.assertTrue(any("senza test" in p for p in pr["ADR-X"]))

    def test_m1_record_not_needed_with_tests(self):
        mx = matrix(**{"ADR-X": row("ADR-X", "M2.entry", "T-X-01", GATE, "VERIFICATO")})
        _, pr = self.gate(mx)
        self.assertNotIn("ADR-X", pr)

    def test_gate_record_with_path_not_accepted(self):
        mx = matrix(**{"F-A": row("F-A", "M2.a", "T-X-01", "`inesistente/M2-gate.md`", "CHIUSO")})
        _, pr = self.gate(mx)
        self.assertTrue(any("non cita" in p for p in pr["F-A"]))

    def test_m1_record_with_path_not_accepted(self):
        mx = matrix(**{"ADR-X": row("ADR-X", "M2.entry", "viewer M1.b", f"`../ev/M1-closure-001.md`, {GATE}", "VERIFICATO")})
        _, pr = self.gate(mx)
        self.assertTrue(any("nome semplice" in p for p in pr["ADR-X"]))

    def test_validate_runs_rules(self):
        d = {}
        for name, kind in (("b1", "Debug"), ("b2", "RelWithDebInfo"), ("b3", "Release")):
            d[name] = os.path.join(WORK, name)
            os.makedirs(d[name], exist_ok=True)
            with open(os.path.join(d[name], "CMakeCache.txt"), "w", encoding="utf-8") as fh:
                fh.write(f"CMAKE_BUILD_TYPE:STRING={kind}\n")
        alias = os.path.join(WORK, "alias")
        if not os.path.islink(alias):
            os.symlink(d["b1"], alias)
        j1 = self.write_junit("v1.xml", "<testsuite>uno</testsuite>")
        j2 = self.write_junit("v2.xml", "<testsuite>due</testsuite>")
        hard = os.path.join(WORK, "v1-hard.xml")
        if os.path.exists(hard):
            os.remove(hard)
        os.link(j1, hard)
        copy = self.write_junit("v1-copy.xml", "<testsuite>uno</testsuite>")
        ok = self.m.validate_runs([(d["b1"], j1), (d["b2"], j2)])
        self.assertEqual(len(ok), 2)
        for pairs, msg in (
            ([(d["b1"], j1), (alias, j2)], "build ripetuta"),
            ([(d["b1"], j1), (d["b2"], j1)], "junit ripetuto"),
            ([(d["b1"], j1), (d["b2"], hard)], "junit ripetuto"),
            ([(d["b1"], j1), (d["b2"], copy)], "contenuto identico"),
            ([(d["b1"], j1), (d["b3"], j2)], "tipi di build"),
            ([(d["b1"], j1), (d["b1"], j2)], "build ripetuta"),
            ([(d["b1"], j1)], "due esecuzioni"),
            ([(d["b1"], j1), (d["b2"], os.path.join(WORK, "assente.xml"))], "impossibile"),
        ):
            with self.assertRaises(SystemExit) as cm:
                self.m.validate_runs(pairs)
            self.assertIn(msg, str(cm.exception))

    def test_read_bytes_unreadable(self):
        with self.assertRaises(SystemExit) as cm:
            self.m.read_bytes(os.path.join(WORK, "assente.xml"))
        self.assertIn("impossibile leggere", str(cm.exception))

    def test_junit_must_cover_whole_build(self):
        ids, pr = self.gate(matrix(), listed=LISTED | {"S.T_X_77_a"})
        self.assertTrue(any("mancano 1 test" in p for p in pr["(esecuzioni)"]))

    def test_junit_with_foreign_tests(self):
        _, pr = self.gate(matrix(), junits=[junit(names=LISTED | {"Altro.T_Y_01"}), junit()])
        self.assertTrue(any("assenti da ctest -N" in p for p in pr["(esecuzioni)"]))

    def test_m1_record_missing_file(self):
        mx = matrix(**{"ADR-X": row("ADR-X", "M2.entry", "viewer M1.b", f"`M1-nope.md`, {GATE}", "VERIFICATO")})
        _, pr = self.gate(mx)
        self.assertTrue(any("M1-nope.md" in p for p in pr["ADR-X"]))

    def test_gate_record_not_cited(self):
        _, pr = self.gate(matrix(**{"F-A": row("F-A", "M2.a", "T-X-01", "`M2-a.md`", "CHIUSO")}))
        self.assertTrue(any("M2-gate.md" in p for p in pr["F-A"]))

    def test_gate_record_file_missing(self):
        runs = [(LISTED, [self.write_junit("j0.xml", junit())])] * 2
        _, pr = self.m.evaluate_gate(self.plan, self.m.parse_matrix(matrix()), runs, os.path.join(WORK, "vuota"), m1_ids=("ADR-X",))
        self.assertTrue(any("manca" in p and "M2-gate.md" in p for p in pr["F-A"]))

    def test_cli_requires_two_runs(self):
        with self.assertRaises(SystemExit) as cm:
            self.m.main(["--run", "build/dev:tmp/j.xml"])
        self.assertIn("due esecuzioni", str(cm.exception))


if __name__ == "__main__":
    try:
        unittest.main(verbosity=1)
    except Exception as exc:  # diagnosi esplicita, niente report di crash
        sys.exit(f"test_check_m2_trace: errore interno {type(exc).__name__}: {exc}")
