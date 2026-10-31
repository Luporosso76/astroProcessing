"""T-U12-04, 07, 08, 09: controlli ELF e di archivio sulla directory di build (obblighi 4, 7, 8, 9).

Uso: test_u12_binaries.py 04|07|08|09 <directory di build>."""
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from u12_common import (NETWORK_NEEDED, NETWORK_SYMBOLS, fail, find_archive, find_files, ldd_closure,  # noqa: E402
                        needed, product_executables, run, undefined_symbols)

PINNED = re.compile(r"^(libraw|libcfitsio|libwcs)")


def require_archive(build, name):
    found = find_archive(build, name)
    if not found:
        fail("archivio %s assente sotto %s: libreria non ancora costruita nel progetto" % (name, build))
    return found


def t04(build):
    for exe in product_executables(build):
        bad = [n for n in needed(exe) + ldd_closure(exe) if PINNED.match(os.path.basename(n))]
        if bad:
            fail("%s collega dinamicamente %s (atteso: collegamento statico)" % (os.path.relpath(exe, build), sorted(set(bad))))
    print("T-U12-04 ok")


def t07(build):
    for exe in product_executables(build):
        bad = [n for n in ldd_closure(exe) if NETWORK_NEEDED.match(os.path.basename(n))]
        if bad:
            fail("%s carica librerie di rete: %s" % (os.path.relpath(exe, build), sorted(set(bad))))
    for archive in require_archive(build, "libcfitsio.a"):
        bad = [s for s in undefined_symbols(archive) if NETWORK_SYMBOLS.match(s)]
        if bad:
            fail("%s ha simboli di rete indefiniti: %s" % (archive, bad))
    print("T-U12-07 ok")


def t08(build):
    for archive in require_archive(build, "libraw_r.a") + require_archive(build, "libastrocore.a"):
        bad = [s for s in undefined_symbols(archive) if re.match(r"^(GOMP_|omp_)", s)]
        if bad:
            fail("%s ha simboli OpenMP indefiniti: %s" % (archive, bad[:5]))
    try:
        with open(os.path.join(build, "compile_commands.json"), encoding="utf-8") as f:
            db = json.load(f)
    except (OSError, ValueError) as e:
        fail("compile_commands.json illeggibile in %s: %s" % (build, e))
    pat = re.compile(r"-fopenmp|-lgomp|libgomp|OpenMP")
    bad = sorted({e["file"] for e in db if pat.search(e.get("command", ""))})
    if bad:
        fail("OpenMP nei comandi di compilazione di: %s" % bad[:5])
    links = run(["ninja", "-C", build, "-t", "commands"]).stdout
    bad = sorted({ln[:160] for ln in links.splitlines() if pat.search(ln) and " -o " in ln and " -c " not in ln})
    if bad:
        fail("OpenMP nei comandi di collegamento: %s" % bad[:3])
    for exe in product_executables(build):
        if any(n.startswith("libgomp") for n in needed(exe) + ldd_closure(exe)):
            fail("%s dipende da libgomp" % os.path.relpath(exe, build))
    print("T-U12-08 ok")


def t09(build):
    objs = find_files(build, lambda n, p: re.search(r"rawspeed_xmldata.*\.(o|lo|obj)$", n) is not None)
    if objs:
        fail("oggetti compilati da RawSpeed/rawspeed_xmldata.cpp: %s" % objs[:3])
    compile_lines = run(["ninja", "-C", build, "-t", "commands"]).stdout.splitlines()
    for makefile in find_files(build, lambda n, p: n == "Makefile"):
        with open(makefile, encoding="utf-8", errors="replace") as f:
            compile_lines += f.read().splitlines()
    bad = [ln[:160] for ln in compile_lines if "rawspeed_xmldata" in ln and " -c " in ln]
    if bad:
        fail("rawspeed_xmldata.cpp nei comandi di compilazione: %s" % bad[:3])
    archives = require_archive(build, "libraw_r.a")
    exes = product_executables(build)
    for target in archives + exes:
        syms = run(["nm", "-A", target]).stdout
        if "_rawspeed_data_xml" in syms:
            fail("simbolo _rawspeed_data_xml in %s" % target)
        with open(target, "rb") as f:
            if b"<Camera" in f.read():
                fail("stringa <Camera nei dati di %s (dati RawSpeed CC-BY-SA nel binario)" % target)
    print("T-U12-09 ok")


def main(argv):
    if len(argv) != 2 or argv[0] not in ("04", "07", "08", "09"):
        sys.exit("uso: test_u12_binaries.py 04|07|08|09 <directory di build>")
    {"04": t04, "07": t07, "08": t08, "09": t09}[argv[0]](os.path.abspath(argv[1]))


if __name__ == "__main__":
    try:
        main(sys.argv[1:])
    except SystemExit:
        raise
    except Exception as e:
        sys.exit("T-U12-binari: errore non previsto: %r" % (e,))
