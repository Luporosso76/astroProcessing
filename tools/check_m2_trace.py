#!/usr/bin/env python3 -S
"""Gate M2: stato finale della tracciabilità ID -> test (plans/M2-residui.md, R6 e sezione 6).

Per ogni ID a stadio M2.* e M2.entry del registro (appendice D) verifica:
  1. la matrice (docs/test-matrix.md) nomina almeno un test presente in `ctest -N` di ogni build;
     solo per le prove M1 di ADR-02 e D-04, se la riga non nomina test, vale il record M1 citato per nome
     semplice e presente in docs/evidence;
  2. ogni test nominato risulta passato nel junit dell'esecuzione finale di ogni build, e ogni junit
     copre per intero i test di `ctest -N` della propria build;
  3. lo stato è CHIUSO (ID funzionali, NFR, G, consegne) o VERIFICATO (ADR, C, decisioni),
     e tutti i prerequisiti interni a M2 sono CHIUSO o VERIFICATO;
  3b. un aggregato (Milestone, Delivery) non richiede test nominati: passa se è CHIUSO e se tutti i suoi
     componenti M2 (prerequisiti del registro, ricorsivi) passano; senza componenti è un errore;
  4. la riga cita il record M2-gate.md per nome semplice, e il file esiste in docs/evidence.
Le due esecuzioni sono dev (Debug) e release (RelWithDebInfo), con build e junit distinti.
Legge solo lo stato finale. Il parsing del registro e il confronto con ctest/junit sono di tools/trace.py.
Uscita 0 solo se nessun ID e nessuna esecuzione ha problemi.
"""
import argparse
import fnmatch
import importlib.util
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLAN = os.path.join(ROOT, "ASTRO_POSTPROCESSING_PLAN_v2.2.1.md")
MATRIX = os.path.join(ROOT, "docs", "test-matrix.md")
EVIDENCE = os.path.join(ROOT, "docs", "evidence")
STAGE = "M2"
GATE_RECORD = "M2-gate.md"
RUNS_KEY = "(esecuzioni)"
RUN_BUILD_TYPES = ("Debug", "RelWithDebInfo")
M1_PROOF_IDS = ("ADR-02", "D-04")
VERIFIED_CLASSES = {"ADR", "ADR-stadio", "C-stadio", "Decisione"}
FINAL_STATES = ("CHIUSO", "VERIFICATO")
MATRIX_SECTION = "## ID del piano"
MATRIX_COLUMNS = 9

ID_TOKEN = re.compile(
    r"(?<![\w\-])(T-[A-Z][A-Z0-9]*-)(\d+)(\.[A-Za-z0-9_]+)?"
    r"((?:…\d+)?(?:,\s*\d+(?:…\d+)?(?![\w\-.]))*)")
FULL_NAME = re.compile(r"(?<![\w.])[A-Za-z][A-Za-z0-9]*\.T_[A-Za-z0-9_]*\*?")
STATE_RE = re.compile(r"^(DA RIESEGUIRE|[A-ZÀ-Ý]+)")
RECORD_RE = re.compile(r"[\w./\-]+\.md")


def load_trace():
    path = os.path.join(ROOT, "tools", "trace.py")
    spec = importlib.util.spec_from_file_location("astro_trace", path)
    if spec is None or spec.loader is None:
        sys.exit(f"check_m2_trace: impossibile caricare {path}")
    mod = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(mod)
    except OSError as exc:
        sys.exit(f"check_m2_trace: impossibile caricare {path}: {exc}")
    return mod


TRACE = load_trace()


def parse_matrix(text):
    """{id: {tests, record, state}} dalla tabella della sezione «ID del piano → test»."""
    lines = text.splitlines()
    start = next((i for i, ln in enumerate(lines) if ln.startswith(MATRIX_SECTION)), None)
    if start is None:
        sys.exit(f"check_m2_trace: sezione '{MATRIX_SECTION}' non trovata nella matrice")
    rows = {}
    header = False
    for n, line in enumerate(lines[start + 1:], start + 2):
        if line.startswith("## "):
            break
        if not line.startswith("|"):
            continue
        parts = [c.strip() for c in line.strip().strip("|").split("|")]
        if parts[0] == "ID":
            header = True
            continue
        if not header or set(line.replace("|", "").strip()) <= set("-: "):
            continue
        if len(parts) != MATRIX_COLUMNS:
            sys.exit(f"check_m2_trace: matrice riga {n}: attese {MATRIX_COLUMNS} colonne, trovate {len(parts)}")
        ident = parts[0].strip("*` ")
        if ident in rows:
            sys.exit(f"check_m2_trace: matrice riga {n}: ID duplicato {ident}")
        rows[ident] = {"tests": parts[4], "record": parts[6], "state": parts[7]}
    if not rows:
        sys.exit("check_m2_trace: nessuna riga nella matrice")
    return rows


def parse_state(cell):
    m = STATE_RE.match(cell.strip().strip("*` "))
    return m.group(1) if m else cell.strip()[:20]


def extract_tokens(cell, listed):
    """Token di test nominati in una cella: `T-XXX-NN`, intervalli `T-XXX-NN…MM`, nomi completi (anche con `*`)."""
    found = []

    def add(tok):
        if tok not in found:
            found.append(tok)

    for m in ID_TOKEN.finditer(cell):
        prefix, num, suffix, rest = m.groups()
        head = re.match(r"…(\d+)", rest)
        if head:
            add(f"{prefix}{num}{suffix or ''}…{head.group(1)}")
            rest = rest[head.end():]
        else:
            add(f"{prefix}{num}{suffix or ''}")
        for lo, hi in re.findall(r"(\d+)(?:…(\d+))?", rest):
            add(f"{prefix}{lo}…{hi}" if hi else f"{prefix}{lo}")
    for m in FULL_NAME.finditer(cell):
        name = m.group(0)
        if "*" in name:
            hits = sorted(n for n in listed if fnmatch.fnmatchcase(n, name))
            for h in hits or [name]:
                add(h)
        else:
            add(name)
    return found


def expand_range(token, listed):
    """(membri esistenti, problema) di un token: un intervallo `T-XXX-NN…MM` vuoto o invertito è un problema."""
    m = re.fullmatch(r"(T-[A-Z][A-Z0-9]*-)(\d+)…(\d+)", token)
    if not m:
        return [token], None
    prefix, lo, hi = m.group(1), m.group(2), m.group(3)
    if int(lo) > int(hi):
        return [], f"intervallo {token} invertito"
    width = len(lo)
    out = []
    for k in range(int(lo), int(hi) + 1):
        tok = f"{prefix}{k:0{width}d}"
        if TRACE.match_tests(tok, listed):
            out.append(tok)
    return out, (None if out else f"intervallo {token} senza test in ctest -N")


def cited_records(cell):
    return RECORD_RE.findall(cell)


def bare_record_problem(rec, evidence_dir):
    """Problema di un record citato per nome semplice in docs/evidence, o None."""
    if "/" in rec or rec.startswith("."):
        return f"record {rec}: ammesso solo il nome semplice di un file in {evidence_dir}"
    if not os.path.isfile(os.path.join(evidence_dir, rec)):
        return f"record {rec} inesistente in {evidence_dir}"
    return None


def row_tests(cell, union):
    """(token di test risolti, problemi di intervalli) della cella «Test eseguiti»."""
    toks, probs = [], []
    for t in extract_tokens(cell, union):
        members, prob = expand_range(t, union)
        if prob:
            probs.append(prob)
        for x in members:
            if x not in toks:
                toks.append(x)
    return toks, probs


def run_completeness(listed, junits, label):
    """Problemi se il junit non coincide con l'insieme dei test di `ctest -N` della build."""
    got = set(TRACE.read_junit(junits))
    missing, extra = sorted(listed - got), sorted(got - listed)
    out = []
    if missing:
        out.append(f"{label}: nel junit mancano {len(missing)} test di ctest -N (es. {', '.join(missing[:3])})")
    if extra:
        out.append(f"{label}: il junit contiene {len(extra)} test assenti da ctest -N (es. {', '.join(extra[:3])})")
    return out


def evaluate_run(plan_path, tmap, junits, listed):
    """Problemi di copertura per ID in una build, da trace.evaluate; i prerequisiti sono verificati per stato altrove."""
    _, ids, res = TRACE.evaluate(STAGE, plan_path, tmap, junits, None, listed=listed)
    return {i: [p for p in res[i]["problems"] if not p.startswith("prerequisito ")] for i in ids}


def evaluate_gate(plan_path, rows, runs, evidence_dir, m1_ids=M1_PROOF_IDS):
    """Ritorna (ID dello stadio M2 in ordine di registro, {id o RUNS_KEY: [problemi]}).

    runs: lista di (nomi di ctest -N, [junit]) una per build; ogni test nominato deve esistere e passare in tutte,
    e ogni junit deve coprire per intero la propria build.
    """
    nodes, order, edges = TRACE.parse_registry(TRACE.read_text(plan_path))
    ids = TRACE.stage_ids(nodes, order, STAGE)
    for i in rows:
        if i not in ids:
            sys.exit(f"check_m2_trace: la matrice contiene un ID che non è dello stadio {STAGE}: {i}")
    union = set().union(*(listed for listed, _ in runs)) if runs else set()
    problems = {i: [] for i in ids}
    problems[RUNS_KEY] = []
    for k, (listed, junits) in enumerate(runs, 1):
        problems[RUNS_KEY].extend(run_completeness(listed, junits, f"esecuzione {k}"))
    tmap = {}
    for i in ids:
        if i not in rows:
            problems[i].append("nessuna riga nella matrice")
            continue
        recs = cited_records(rows[i]["record"])
        if GATE_RECORD not in recs:
            problems[i].append(f"la riga non cita il record {GATE_RECORD}")
        elif not os.path.isfile(os.path.join(evidence_dir, GATE_RECORD)):
            problems[i].append(f"il record {GATE_RECORD} manca in {evidence_dir}")
        toks, range_problems = row_tests(rows[i]["tests"], union)
        problems[i].extend(range_problems)
        tmap[i] = (toks, rows[i]["record"])
        if toks:
            continue
        if i in m1_ids:
            m1 = [r for r in recs if os.path.basename(r).startswith("M1-")]
            if not m1:
                problems[i].append("nessun test e nessun record M1 citato")
            problems[i].extend(p for p in (bare_record_problem(r, evidence_dir) for r in m1) if p)
        elif nodes[i]["classe"] not in TRACE.AGGREGATE_CLASSES:
            problems[i].append("nessun test")
    for listed, junits in runs:
        res = evaluate_run(plan_path, tmap, junits, listed)
        for i in tmap:
            for p in res[i]:
                if p != "nessun test" and p not in problems[i]:
                    problems[i].append(p)
    pm = TRACE.prereq_map(edges)
    stage_set = set(ids)
    states = {i: parse_state(rows[i]["state"]) for i in ids if i in rows}
    for i in ids:
        if i not in rows:
            continue
        expected = "VERIFICATO" if nodes[i]["classe"] in VERIFIED_CLASSES else "CHIUSO"
        if states[i] != expected:
            problems[i].append(f"stato {states[i]}, atteso {expected}")
        for p in pm.get(i, []):
            if p not in stage_set:
                continue
            if states.get(p) not in FINAL_STATES:
                problems[i].append(f"prerequisito {p} in stato {states.get(p, 'assente')}")
    own = {i: list(p) for i, p in problems.items()}
    for i in ids:
        if i not in rows or nodes[i]["classe"] not in TRACE.AGGREGATE_CLASSES:
            continue
        parts = sorted(TRACE.closure(pm, i) & stage_set)
        if not parts:
            problems[i].append("aggregato senza componenti M2 nel registro")
        problems[i].extend(f"componente {c} non passa" for c in parts if own.get(c))
    return ids, {i: p for i, p in problems.items() if p}


def build_type(build_dir):
    """CMAKE_BUILD_TYPE dalla cache di CMake della build."""
    path = os.path.join(build_dir, "CMakeCache.txt")
    for line in TRACE.read_text(path).splitlines():
        if line.startswith("CMAKE_BUILD_TYPE:"):
            return line.split("=", 1)[1].strip()
    sys.exit(f"check_m2_trace: CMAKE_BUILD_TYPE assente in {path}")


def any_same_file(paths):
    """True se due percorsi indicano lo stesso file o la stessa directory (link simbolici e hard link inclusi)."""
    for a in range(len(paths)):
        for b in range(a + 1, len(paths)):
            try:
                if os.path.samefile(paths[a], paths[b]):
                    return True
            except OSError as exc:
                sys.exit(f"check_m2_trace: impossibile confrontare {paths[a]} e {paths[b]}: {exc}")
    return False


def read_bytes(path):
    try:
        with open(path, "rb") as fh:
            return fh.read()
    except OSError as exc:
        sys.exit(f"check_m2_trace: impossibile leggere {path}: {exc}")


def validate_runs(pairs):
    """Le esecuzioni sono dev e release: build e junit distinti (anche via alias) e tipi di build dei preset."""
    if len(pairs) != len(RUN_BUILD_TYPES):
        sys.exit("check_m2_trace: servono due esecuzioni (--run BUILD:JUNIT per dev e release)")
    builds = [os.path.realpath(b) for b, _ in pairs]
    junits = [os.path.realpath(j) for _, j in pairs]
    if any_same_file(builds):
        sys.exit(f"check_m2_trace: build ripetuta in --run: {builds}")
    if any_same_file(junits):
        sys.exit(f"check_m2_trace: junit ripetuto in --run (stesso file o hard link): {junits}")
    if len({read_bytes(j) for j in junits}) != len(junits):
        sys.exit(f"check_m2_trace: junit con contenuto identico in --run: {junits}")
    types = sorted(build_type(b) for b in builds)
    if types != sorted(RUN_BUILD_TYPES):
        sys.exit(f"check_m2_trace: tipi di build {types}, attesi {sorted(RUN_BUILD_TYPES)} (preset dev e release)")
    return list(zip(builds, junits))


def parse_runs(specs):
    pairs = []
    for spec in specs:
        build, sep, junit = spec.rpartition(":")
        if not sep or not build or not junit:
            sys.exit(f"check_m2_trace: --run atteso nella forma BUILD:JUNIT, ricevuto '{spec}'")
        pairs.append((build, junit))
    return [(TRACE.ctest_names(build), [junit]) for build, junit in validate_runs(pairs)]


def report(ids, problems):
    """Stampa i problemi (prima quelli delle esecuzioni, poi per ID) e il riepilogo; ritorna il codice di uscita."""
    for p in problems.get(RUNS_KEY, []):
        print(f"{RUNS_KEY}: {p}")
    bad = [i for i in ids if i in problems]
    for i in bad:
        print(f"{i}: " + "; ".join(problems[i]))
    print(f"check_m2_trace: {len(ids)} ID nello stadio {STAGE}, {len(bad)} con problemi, "
          f"{len(problems.get(RUNS_KEY, []))} problemi di esecuzione")
    return 1 if problems else 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--plan", default=PLAN)
    ap.add_argument("--matrix", default=MATRIX)
    ap.add_argument("--evidence-dir", default=EVIDENCE)
    ap.add_argument("--run", action="append", default=[], metavar="BUILD:JUNIT",
                    help="directory di build e junit dell'esecuzione finale; due volte (dev e release)")
    args = ap.parse_args(argv)
    runs = parse_runs(args.run)
    rows = parse_matrix(TRACE.read_text(args.matrix))
    ids, problems = evaluate_gate(args.plan, rows, runs, args.evidence_dir)
    return report(ids, problems)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:  # diagnosi esplicita, niente report di crash
        sys.exit(f"check_m2_trace: errore interno {type(exc).__name__}: {exc}")
