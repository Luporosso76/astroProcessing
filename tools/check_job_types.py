"""Completezza del registro dei tipi di job (docs/contracts/BASE-confini.md §3.1, punto 3).

Uso: python3 -S tools/check_job_types.py <radice>            controllo sul sorgente
     python3 -S tools/check_job_types.py --self-test <tmp>   il controllo fallisce su un tipo non registrato

Confronta tre insiemi letti dal sorgente:
- le costanti `<nome>_operator_id` dichiarate in src/core/include/astro/*.hpp;
- gli operatori delle voci di registered_job_types() in src/core/job_types.cpp;
- le funzioni di src/core/*.cpp che chiamano run_job_files, escluse le composizioni (run_branches), contro le
  funzioni di job chiamate dal registro.
Uscita 0 se coincidono; altrimenti la diagnosi su stderr e uscita non nulla.
"""

import os
import re
import shutil
import sys

CONSTANT = re.compile(r'constexpr\s+const\s+char\s*\*\s*(\w+_operator_id)\s*=\s*"([^"]+)"')
ENTRY = re.compile(r'^\s*\{\s*(\w+_operator_id)\s*,', re.M)
DEFINITION = re.compile(r'^(?:[A-Za-z_][\w:<>]*[\s*&]+)+(\w+)\s*\(')
COMPOSITIONS = {"run_branches", "run_branches_with"}
REGISTRY_CALL = re.compile(r'\b(run_\w+_job)\s*\(')


def read(path):
    try:
        with open(path, encoding="utf-8") as f:
            return f.read()
    except OSError as e:
        sys.exit(f"check_job_types: lettura di {path} non riuscita: {e}")


def constants(root):
    inc = os.path.join(root, "src", "core", "include", "astro")
    found = {}
    for name in sorted(os.listdir(inc)):
        if name.endswith(".hpp"):
            for const, value in CONSTANT.findall(read(os.path.join(inc, name))):
                found[const] = value
    return found


def runner_callers(root):
    core = os.path.join(root, "src", "core")
    callers = set()
    for name in sorted(os.listdir(core)):
        if not name.endswith(".cpp") or name in ("job.cpp", "job_probe.cpp"):
            continue
        current = None
        for line in read(os.path.join(core, name)).splitlines():
            m = DEFINITION.match(line)
            if m and not line.rstrip().endswith(";"):
                current = m.group(1)
            if "run_job_files(" in line and current is not None and current not in COMPOSITIONS:
                callers.add(current)
    return callers


def check(root):
    registry = read(os.path.join(root, "src", "core", "job_types.cpp"))
    declared = constants(root)
    registered = set(ENTRY.findall(registry))
    called = set(REGISTRY_CALL.findall(registry))
    callers = runner_callers(root)
    errors = []
    for const in sorted(set(declared) - registered):
        errors.append(f"operatore non registrato: {const} = \"{declared[const]}\"")
    for const in sorted(registered - set(declared)):
        errors.append(f"voce del registro senza costante dichiarata: {const}")
    for fn in sorted(callers - called):
        errors.append(f"funzione che chiama run_job_files non raggiunta dal registro: {fn}")
    if not declared or not callers:
        errors.append("nessuna costante o nessun chiamante trovato: lettura del sorgente non valida")
    return errors


def self_test(tmp):
    base = os.path.join(tmp, "check_job_types_selftest")
    shutil.rmtree(base, ignore_errors=True)
    inc = os.path.join(base, "src", "core", "include", "astro")
    os.makedirs(inc)
    core = os.path.join(base, "src", "core")
    with open(os.path.join(inc, "a.hpp"), "w", encoding="utf-8") as f:
        f.write('constexpr const char* a_operator_id = "op.a";\nconstexpr const char* b_operator_id = "op.b";\n')
    with open(os.path.join(core, "a.cpp"), "w", encoding="utf-8") as f:
        f.write("job_outcome run_a_job(int x) {\n  return run_job_files(s, n, w);\n}\n"
                "job_outcome run_b_job(int x) {\n  return run_job_files(s, n, w);\n}\n")
    with open(os.path.join(core, "job_types.cpp"), "w", encoding="utf-8") as f:
        f.write("  {a_operator_id, {}, {}, false, [] { run_a_job(1); }},\n")
    errors = check(base)
    expected = {"operatore non registrato: b_operator_id = \"op.b\"",
                "funzione che chiama run_job_files non raggiunta dal registro: run_b_job"}
    if set(errors) != expected:
        sys.exit(f"check_job_types --self-test: diagnosi attese {sorted(expected)}, ottenute {errors}")
    with open(os.path.join(core, "job_types.cpp"), "a", encoding="utf-8") as f:
        f.write("  {b_operator_id, {}, {}, false, [] { run_b_job(1); }},\n")
    if check(base):
        sys.exit(f"check_job_types --self-test: registro completo rifiutato: {check(base)}")
    shutil.rmtree(base)


def main(argv):
    if len(argv) == 3 and argv[1] == "--self-test":
        self_test(argv[2])
        print("check_job_types: self-test superato")
        return
    if len(argv) != 2:
        sys.exit("uso: check_job_types.py <radice> | --self-test <tmp>")
    errors = check(argv[1])
    if errors:
        sys.exit("check_job_types:\n" + "\n".join(errors))
    print("check_job_types: registro completo")


if __name__ == "__main__":
    try:
        main(sys.argv)
    except Exception as e:  # diagnosi su stderr, mai un'eccezione non gestita
        sys.exit(f"check_job_types: errore inatteso: {e}")
