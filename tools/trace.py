#!/usr/bin/env python3 -S
"""Tracciabilità ID del piano -> test, dal registro dell'appendice D.

Sottocomandi: ids, prereqs, check, table, selftest. Solo libreria standard.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLAN = os.path.join(ROOT, "ASTRO_POSTPROCESSING_PLAN_v2.2.1.md")
EDGE_TYPES = {"requires_all", "requires_when", "contains", "decided_by", "verified_by"}
PREREQ_TYPES = {"requires_all", "decided_by"}
AGGREGATE_CLASSES = {"Milestone", "Delivery"}
ID_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_.\-]*$")


def read_text(path):
    try:
        with open(path, encoding="utf-8") as fh:
            return fh.read()
    except (OSError, UnicodeDecodeError) as exc:
        sys.exit(f"trace: impossibile leggere {path}: {exc}")


def cells(line):
    parts = [c.strip() for c in line.strip().strip("|").split("|")]
    return [p.strip("*` ") for p in parts]


def parse_registry(text):
    """Ritorna (nodi: {id: {classe, priorita, stadio}}, ordine, archi: [(da, tipo, a)])."""
    lines = text.splitlines()
    start = None
    for i, line in enumerate(lines):
        if line.startswith("### Appendice D"):
            start = i
            break
    if start is None:
        sys.exit("trace: sezione '### Appendice D' non trovata nel piano")
    nodes, order, edges = {}, [], []
    for line in lines[start + 1:]:
        if line.startswith("### "):
            break
        if not line.startswith("|"):
            continue
        c = cells(line)
        if len(c) == 4 and c[1] in EDGE_TYPES:
            edges.append((c[0], c[1], c[2]))
        elif len(c) == 5 and ID_RE.match(c[0]) and c[0] not in ("ID", "Da"):
            if c[0] in nodes:
                sys.exit(f"trace: ID duplicato nel registro: {c[0]}")
            nodes[c[0]] = {"classe": c[1], "priorita": c[2], "stadio": c[3]}
            order.append(c[0])
    if not nodes:
        sys.exit("trace: nessun nodo trovato nell'appendice D")
    for da, tipo, a in edges:
        for x in (da, a):
            if x not in nodes:
                sys.exit(f"trace: arco {da} {tipo} {a}: ID non definito: {x}")
    return nodes, order, edges


def stage_ids(nodes, order, stage):
    ids = [i for i in order if nodes[i]["stadio"] == stage or nodes[i]["stadio"].startswith(stage + ".")]
    if not ids:
        sys.exit(f"trace: nessun ID per lo stadio {stage}")
    return ids


def prereq_map(edges):
    m = {}
    for da, tipo, a in edges:
        if tipo in PREREQ_TYPES:
            m.setdefault(da, []).append(a)
    return m


def closure(pm, start):
    seen, stack = set(), list(pm.get(start, []))
    while stack:
        x = stack.pop()
        if x in seen:
            continue
        seen.add(x)
        stack.extend(pm.get(x, []))
    return seen


def read_map(path):
    m = {}
    for n, line in enumerate(read_text(path).splitlines(), 1):
        if not line.strip() or line.startswith("#") or line.startswith("ID\t"):
            continue
        f = line.split("\t")
        if len(f) < 2 or len(f) > 3:
            sys.exit(f"trace: {path}:{n}: attese 2-3 colonne TSV (ID, test, record)")
        if f[0] in m:
            sys.exit(f"trace: {path}:{n}: ID duplicato {f[0]}")
        tests = [t.strip() for t in f[1].split(",") if t.strip()]
        m[f[0]] = (tests, f[2].strip() if len(f) == 3 else "")
    return m


def read_junit(paths):
    """{nome: True|False}: True solo se passato in tutti i file che lo contengono."""
    res = {}
    for p in paths:
        try:
            tree = ET.parse(p)
        except (OSError, ET.ParseError) as exc:
            sys.exit(f"trace: junit non leggibile {p}: {exc}")
        for tc in tree.getroot().iter("testcase"):
            name = tc.get("name")
            if not name:
                sys.exit(f"trace: {p}: testcase senza nome")
            ok = tc.get("status", "run") == "run" and not any(
                tc.find(t) is not None for t in ("failure", "error", "skipped"))
            res[name] = res.get(name, True) and ok
    if not res:
        sys.exit("trace: nessun testcase nei junit")
    return res


def ctest_names(build_dir):
    exe = shutil.which("ctest")
    if exe is None:
        sys.exit("trace: ctest non trovato nel PATH")
    try:
        out = subprocess.run([exe, "--test-dir", build_dir, "-N"], capture_output=True,
                             text=True, check=True).stdout
    except OSError as exc:
        sys.exit(f"trace: 'ctest -N' non avviabile su {build_dir}: {exc}")
    except subprocess.CalledProcessError as exc:
        sys.exit(f"trace: 'ctest -N' fallito su {build_dir}: {exc}\nstdout: {exc.stdout}\nstderr: {exc.stderr}")
    return set(re.findall(r"^\s*Test\s+#\d+:\s+(.+?)\s*$", out, re.M))


def token_re(token):
    pat = re.escape(re.sub(r"[-_]", "_", token)).replace("_", "[-_]")
    return re.compile(r"(?<![A-Za-z0-9])" + pat + r"(?![0-9])")


def match_tests(token, names):
    if token in names:
        return [token]
    rx = token_re(token)
    return [n for n in names if rx.search(n)]


def evaluate(stage, plan, tmap, junit_paths, build_dir, listed=None):
    nodes, order, edges = parse_registry(read_text(plan))
    ids = stage_ids(nodes, order, stage)
    universe = stage_ids(nodes, order, stage.split(".")[0])
    for i in tmap:
        if i not in nodes:
            sys.exit(f"trace: la mappa contiene un ID sconosciuto al registro: {i}")
    pm = prereq_map(edges)
    results = read_junit(junit_paths)
    known = set(results)
    if listed is None and build_dir:
        listed = ctest_names(build_dir)
    rows, own = {}, {}
    for i in universe:
        tests, record = tmap.get(i, ([], ""))
        aggregate = nodes[i]["classe"] in AGGREGATE_CLASSES
        problems = []
        for t in tests:
            in_junit = match_tests(t, known)
            if listed is None:
                required = in_junit
                if not required:
                    problems.append(f"test {t} assente nel junit")
                    continue
            else:
                required = match_tests(t, listed)
                if not required:
                    problems.append(f"test {t} assente in ctest -N")
                    continue
                missing = [n for n in required if n not in results]
                if missing:
                    problems.append(f"test {t}: nel junit mancano {', '.join(sorted(missing)[:3])}")
                extra = [n for n in in_junit if n not in listed]
                if extra:
                    problems.append(f"test {t}: nel junit {', '.join(sorted(extra)[:3])} non è in ctest -N")
            bad = [n for n in required if n in results and not results[n]]
            if bad:
                problems.append(f"test {t} non passato ({', '.join(sorted(bad)[:3])})")
        if not aggregate and not tests:
            problems.append("nessun test")
        own[i] = not problems
        rows[i] = {"tests": tests, "record": record, "problems": problems, "aggregate": aggregate}
    stage_set = set(universe)
    for i in universe:
        for p in sorted(closure(pm, i) & stage_set):
            if not own[p]:
                rows[i]["problems"].append(f"prerequisito {p} non coperto")
        rows[i]["covered"] = not rows[i]["problems"]
    return nodes, ids, rows


def cmd_ids(a):
    nodes, order, _ = parse_registry(read_text(a.plan))
    for i in stage_ids(nodes, order, a.stage):
        print(i)
    return 0


def cmd_prereqs(a):
    nodes, order, edges = parse_registry(read_text(a.plan))
    if a.id not in nodes:
        sys.exit(f"trace: ID sconosciuto: {a.id}")
    cl = closure(prereq_map(edges), a.id)
    for i in order:
        if i in cl:
            print(i)
    return 0


def common(a):
    if not a.ctest_junit:
        sys.exit("trace: serve --ctest-junit")
    if not a.map:
        sys.exit("trace: serve --map")
    if not a.build_dir:
        sys.exit("trace: serve --build-dir: i test di un token si ricavano da 'ctest -N'")
    return evaluate(a.stage, a.plan, read_map(a.map), a.ctest_junit, a.build_dir)


def cmd_check(a):
    _, ids, rows = common(a)
    miss = [i for i in ids if rows[i]["problems"]]
    for i in miss:
        print(f"{i}: " + "; ".join(rows[i]["problems"]))
    print(f"trace: {len(ids)} ID nello stadio {a.stage}, {len(miss)} non coperti")
    return 1 if miss else 0


def cmd_table(a):
    nodes, ids, rows = common(a)
    print("| ID | Stadio | Test | Esito | Record |")
    print("|---|---|---|---|---|")
    for i in ids:
        r = rows[i]
        if r["covered"]:
            esito = "aggregato: prerequisiti coperti" if r["aggregate"] else "COPERTO"
        else:
            esito = "; ".join(r["problems"])
        tests = ", ".join(r["tests"]) or "—"
        print(f"| {i} | {nodes[i]['stadio']} | {tests} | {esito} | {r['record'] or '—'} |")
    return 0


SYN_PLAN = """### Appendice D. Registro

#### D.1 NODI

| ID | Classe | Priorità | Stadio | Descrizione |
|---|---|---|---|---|
| F-A | F | P0 | M2.a | a |
| F-B | F | P0 | M2.b | b |
| F-C | F | P0 | M2.c | c |
| ADR-X | ADR | R | M2.entry | x |
| M2.a | Delivery | P0 | M2.a | agg |
| F-Z | F | P0 | M3.a | z |

#### D.6 ARCHI

| Da | Tipo | A | Predicato |
|---|---|---|---|
| F-B | requires_all | F-A | - |
| F-C | requires_all | F-B | - |
| F-C | decided_by | ADR-X | - |
| M2.a | requires_all | F-A | - |
| F-Z | requires_all | F-C | - |

### Altro
"""
SYN_JUNIT = """<testsuite><testcase name="Suite.T_X_01_ok" status="run"/>
<testcase name="Suite.T_X_02_ko" status="run"><failure/></testcase>
<testcase name="Suite.T_X_03_ok" status="run"/></testsuite>"""


def write_file(path, body):
    try:
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(body)
    except OSError as exc:
        sys.exit(f"trace: scrittura fallita {path}: {exc}")


def selftest(_a):
    tmp = os.path.join(ROOT, "tmp", "trace-selftest")
    try:
        os.makedirs(tmp, exist_ok=True)
    except OSError as exc:
        sys.exit(f"trace: impossibile creare {tmp}: {exc}")
    plan, junit, tsv = (os.path.join(tmp, n) for n in ("plan.md", "junit.xml", "map.tsv"))
    write_file(plan, SYN_PLAN)
    write_file(junit, SYN_JUNIT)

    def run(mapbody):
        write_file(tsv, mapbody)
        return evaluate("M2", plan, read_map(tsv), [junit], None)

    nodes, order, edges = parse_registry(SYN_PLAN)
    check(stage_ids(nodes, order, "M2") == ["F-A", "F-B", "F-C", "ADR-X", "M2.a"], "ids")
    check(closure(prereq_map(edges), "F-Z") == {"F-C", "F-B", "F-A", "ADR-X"}, "prereqs")
    check(match_tests("T-X-01", {"Suite.T_X_01_ok", "Suite.T_X_011_no"}) == ["Suite.T_X_01_ok"], "match")
    _, _, rows = run("F-A\tT-X-01\t\nF-B\tT-X-03\t\nF-C\tT-X-03\t\nADR-X\tT-X-03\t\nM2.a\t\t\n")
    check(all(r["covered"] for r in rows.values()), f"tutto coperto: {rows}")
    _, _, rows = run("F-A\tT-X-02\t\nF-B\tT-X-03\t\nF-C\t\t\nADR-X\tT-X-09\t\n")
    check(not rows["F-A"]["covered"] and "non passato" in rows["F-A"]["problems"][0], "test fallito")
    check(any("prerequisito F-A" in p for p in rows["F-B"]["problems"]), "prerequisito non coperto")
    check("nessun test" in rows["F-C"]["problems"], "ID senza test")
    check(any("assente nel junit" in p for p in rows["ADR-X"]["problems"]), "test assente")
    check(not rows["M2.a"]["covered"] and any("F-A" in p for p in rows["M2.a"]["problems"]), "aggregato")
    def run_listed(mapbody, listed, junit_body):
        write_file(tsv, mapbody)
        jx = os.path.join(tmp, "junit2.xml")
        write_file(jx, junit_body)
        return evaluate("M2", plan, read_map(tsv), [jx], None, listed=listed)

    case = lambda n, st="run": f'<testcase name="{n}" status="{st}"/>'
    wrap = lambda body: f"<testsuite>{body}</testsuite>"
    group = {"S.T_X_01_a", "S.T_X_01_b"}
    mp = "F-A\tT-X-01\t\n"
    _, _, rows = run_listed(mp, group, wrap(case("S.T_X_01_a") + case("S.T_X_01_b")))
    check(rows["F-A"]["covered"], "gruppo completo")
    _, _, rows = run_listed(mp, group, wrap(case("S.T_X_01_a")))
    check(any("mancano" in p for p in rows["F-A"]["problems"]), "gruppo incompleto")
    _, _, rows = run_listed(mp, group, wrap(case("S.T_X_01_a") + case("S.T_X_01_c")))
    check(any("mancano" in p for p in rows["F-A"]["problems"]) and any("non è in ctest -N" in p for p in rows["F-A"]["problems"]), "nome rinominato")
    _, _, rows = run_listed(mp, group, wrap(case("S.T_X_01_a") + case("S.T_X_01_b", "fail")))
    check(any("non passato" in p for p in rows["F-A"]["problems"]), "un membro del gruppo fallito")
    _, _, rows = run_listed("F-A\tS.T_X_01_a\t\n", group, wrap(case("S.T_X_01_a")))
    check(rows["F-A"]["covered"], "nome ctest esatto ammesso nella mappa")
    _, _, rows = run_listed("F-A\tS.T_X_01_zz\t\n", group, wrap(case("S.T_X_01_a")))
    check(any("assente in ctest -N" in p for p in rows["F-A"]["problems"]), "nome ctest esatto inesistente")
    write_file(tsv, "F-B\tT-X-03\t\n")
    _, ids_b, rows = evaluate("M2.b", plan, read_map(tsv), [junit], None)
    check(ids_b == ["F-B"], "lo stadio M2.b emette solo i propri ID")
    check(any("prerequisito F-A" in p for p in rows["F-B"]["problems"]), "prerequisito di un'altra sottofase (F-A in M2.a)")
    print("trace selftest: ok")
    return 0


def check(cond, what):
    if not cond:
        sys.exit(f"trace selftest fallito: {what}")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--plan", default=PLAN)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("ids")
    p.add_argument("--stage", required=True)
    p.set_defaults(fn=cmd_ids)
    p = sub.add_parser("prereqs")
    p.add_argument("id")
    p.set_defaults(fn=cmd_prereqs)
    for name, fn in (("check", cmd_check), ("table", cmd_table)):
        p = sub.add_parser(name)
        p.add_argument("--stage", required=True)
        p.add_argument("--map")
        p.add_argument("--ctest-junit", action="append")
        p.add_argument("--build-dir", help="directory di build: tutti i test che corrispondono a un token (ctest -N) devono essere nel junit e passati")
        p.set_defaults(fn=fn)
    p = sub.add_parser("selftest")
    p.set_defaults(fn=selftest)
    args = ap.parse_args()
    return args.fn(args)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:  # diagnosi esplicita, niente report di crash
        sys.exit(f"trace: errore interno {type(exc).__name__}: {exc}")
