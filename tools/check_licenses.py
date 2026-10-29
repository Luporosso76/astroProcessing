import hashlib
import os
import subprocess
import sys

USAGE = "uso: check_licenses.py [--self-test] <licenses.md> <directory binari>"
HEADER = "| Libreria (soname) |"
LICENSE_COLUMN = 5
COLUMNS = 8


def parse_table(text):
    rows = {}
    in_table = False
    for line in text.splitlines():
        if line.startswith(HEADER):
            in_table = True
            continue
        if not in_table:
            continue
        if not line.startswith("|"):
            break
        cells = [c.strip().strip("`") for c in line.strip().strip("|").split("|")]
        if cells[0].startswith("---"):
            continue
        if len(cells) != COLUMNS:
            sys.exit("riga della tabella librerie con %d colonne invece di %d: %s" % (len(cells), COLUMNS, line[:80]))
        rows[cells[0]] = {"path": cells[1], "sha": cells[4], "license": cells[LICENSE_COLUMN]}
    if not rows:
        sys.exit("tabella 'Librerie caricate dai binari' assente")
    return rows


def sha256(path):
    h = hashlib.sha256()
    try:
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                h.update(chunk)
    except OSError as e:
        sys.exit("impossibile calcolare lo SHA-256 di %s: %s" % (path, e))
    return h.hexdigest()


def loaded_libraries(directory):
    libs = {}
    try:
        names = sorted(os.listdir(directory))
    except OSError as e:
        sys.exit("directory dei binari illeggibile %s: %s" % (directory, e))
    for name in names:
        path = os.path.join(directory, name)
        if not os.path.isfile(path) or not os.access(path, os.X_OK):
            continue
        try:
            with open(path, "rb") as f:
                magic = f.read(4)
        except OSError as e:
            sys.exit("file illeggibile %s: %s" % (path, e))
        if magic != b"\x7fELF":
            continue
        try:
            res = subprocess.run(["ldd", path], capture_output=True, text=True)
        except OSError as e:
            sys.exit("impossibile avviare ldd su %s: %s" % (path, e))
        if res.returncode != 0:
            sys.exit("ldd fallito su %s: %s" % (path, res.stderr.strip()))
        for line in res.stdout.splitlines():
            line = line.strip()
            if "=>" in line:
                soname, rest = (s.strip() for s in line.split("=>", 1))
                target = rest.split("(")[0].strip()
                if not target:
                    sys.exit("libreria non risolta da ldd in %s: %s" % (path, line))
            elif line.startswith("/"):
                target = line.split()[0]
                soname = os.path.basename(target)
            else:
                continue
            libs.setdefault(soname, set()).add(os.path.realpath(target))
    if not libs:
        sys.exit("nessun eseguibile ELF in %s" % directory)
    return libs


def check(rows, libs):
    problems = []
    for soname, reals in sorted(libs.items()):
        if len(reals) > 1:
            problems.append("%s: risoluzioni discordanti: %s" % (soname, ", ".join(sorted(reals))))
        row = rows.get(soname)
        if row is None:
            problems.append("%s: assente da licenses.md" % soname)
            continue
        if not row["license"]:
            problems.append("%s: licenza vuota" % soname)
        for real in sorted(reals):
            if row["path"] != real:
                problems.append("%s: file reale %s, in tabella %s" % (soname, real, row["path"]))
            actual = sha256(real)
            if row["sha"] != actual:
                problems.append("%s: SHA-256 di %s è %s, in tabella %s" % (soname, real, actual, row["sha"]))
    return problems


def defect(message):
    sys.stderr.write(message + "\n")
    print("CHECK-RESULT: defect")
    sys.exit(1)


def self_test(text, libs):
    problems = check(parse_table(text), libs)
    if problems:
        defect("self-test: la tabella reale non passa\n" + "\n".join(problems))
    victim = sorted(libs)[0]
    marker = "| `%s` |" % victim
    sha = sha256(next(iter(libs[victim])))
    lines = text.splitlines()

    def blank_license(line):
        cells = line.split("|")
        cells[LICENSE_COLUMN + 1] = " "
        return "|".join(cells)

    cases = {
        "riga mancante": [ln for ln in lines if not ln.startswith(marker)],
        "hash alterato": [ln.replace(sha, "0" * 64) if ln.startswith(marker) else ln for ln in lines],
        "licenza vuota": [blank_license(ln) if ln.startswith(marker) else ln for ln in lines],
    }
    for label, mutated in cases.items():
        if not check(parse_table("\n".join(mutated)), libs):
            defect("self-test: la mutazione '%s' non è stata rilevata" % label)
    other = next(p for soname, ps in sorted(libs.items()) if soname != victim for p in ps)
    doubled = {k: set(v) for k, v in libs.items()}
    doubled[victim].add(other)
    if not any("discordanti" in p for p in check(parse_table(text), doubled)):
        defect("self-test: due file per lo stesso SONAME non sono stati rilevati")
    print("self-test ok: riga mancante, hash alterato, licenza vuota e SONAME con due file rilevati")


def main(argv):
    selftest = len(argv) == 4 and argv[1] == "--self-test"
    if not selftest and len(argv) != 3:
        sys.exit(USAGE)
    doc, directory = argv[-2], argv[-1]
    try:
        with open(doc, encoding="utf-8") as f:
            text = f.read()
    except (OSError, UnicodeDecodeError) as e:
        sys.exit("impossibile leggere %s: %s" % (doc, e))
    libs = loaded_libraries(directory)
    if selftest:
        self_test(text, libs)
        return
    problems = check(parse_table(text), libs)
    if problems:
        defect("LICENZE inventario non conforme:\n" + "\n".join(problems))
    print("licenze ok: %d librerie caricate, tutte con file, hash e licenza" % len(libs))



def run():
    """CHECK-RESULT: ok | defect (uscita 1) | tool-error (uscita 2); tools/mutate.sh legge il marcatore dal junit."""
    try:
        main(sys.argv)
    except SystemExit as exc:
        if exc.code in (None, 0):
            print("CHECK-RESULT: ok")
            raise
        if exc.code == 1 and not isinstance(exc.code, str):
            raise
        sys.stderr.write("%s\n" % exc.code)
        print("CHECK-RESULT: tool-error")
        sys.exit(2)
    print("CHECK-RESULT: ok")


run()
