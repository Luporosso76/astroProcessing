"""T-U12-10, 11, 12: coerenza di docs/licenses.md, costanti e licenze delle dipendenze dirette di sistema.

Uso: test_u12_docs.py 10|11 | 12 <directory di build del preset release>. T-U12-12 legge gli eseguibili del preset
release: gli eseguibili che collegano libastrocore.a (prodotto, o in sua assenza i binari che la collegano: l'unico modo in cui i
NEEDED di libastrocore sono osservabili). Un eseguibile che non collega libastrocore non e' nel perimetro; i runtime dei
sanitizer del preset dev non si distribuiscono.

Formato richiesto dalla cella «Licenza (...)» di ogni riga di «Librerie caricate dai binari»: l'URL del file di licenza
upstream seguito subito da `(SHA-256 <hex>)`; soname, versione installata, URL e hash compaiono insieme in una stessa registrazione di docs/evidence/."""
import os
import re
import shlex
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from u12_common import EXPECTED_RUNTIME as EXP, LIBS, ROOT, fail, find_archive, needed, read_text, require_libs_in_tree, run  # noqa: E402

LICENSES = os.path.join(ROOT, "docs", "licenses.md")
HEX64 = re.compile(r"\b[0-9a-f]{64}\b")
DISTRO_HOSTS = re.compile(r"debian\.org|ubuntu\.com|launchpad\.net|salsa\.debian", re.I)


def section_rows(text, heading_prefix, first_header):
    lines = text.splitlines()
    start = next((i for i, ln in enumerate(lines) if ln.startswith(heading_prefix)), None)
    if start is None:
        fail("docs/licenses.md senza la sezione «%s»" % heading_prefix.lstrip("# "))
    rows, in_table = [], False
    for ln in lines[start + 1:]:
        if ln.startswith("## "):
            break
        if ln.startswith(first_header):
            in_table = True
            continue
        if in_table and ln.startswith("|"):
            cells = [c.strip() for c in ln.strip().strip("|").split("|")]
            if not cells[0].startswith("---"):
                rows.append(cells)
        elif in_table:
            break
    if not rows:
        fail("tabella «%s» assente o vuota" % first_header)
    return rows


def sha_sums(key):
    sums = {}
    for ln in read_text(os.path.join(ROOT, "third_party", key, "SHA256SUMS")).splitlines():
        m = re.match(r"^([0-9a-f]{64}) [ *](\S+)$", ln)
        if m:
            sums[m.group(2)] = m.group(1)
    return sums


def t10():
    require_libs_in_tree()
    rows = section_rows(read_text(LICENSES), "## Librerie compilate nel progetto", "| Libreria |")
    by_name = {r[0].lower(): r for r in rows}
    for key, lib in LIBS.items():
        row = by_name.get(lib["name"].lower())
        if row is None:
            fail("docs/licenses.md: nessuna riga per %s" % lib["name"])
        line = " | ".join(row)
        sums = sha_sums(key)
        prov = read_text(os.path.join(ROOT, "third_party", key, "PROVENANCE.md"))
        if row[1] != lib["version"] or lib["version"] not in prov:
            fail("%s: versione della riga (%s), di PROVENANCE.md e di §1 (%s) non coincidono" % (key, row[1], lib["version"]))
        if sums[lib["tarball"]] not in line or lib["sha256"] not in line:
            fail("%s: SHA-256 del tarball di SHA256SUMS assente dalla riga di docs/licenses.md" % key)
        for name in lib["licenses"]:
            if name not in line or sums[name] not in line:
                fail("%s: file di licenza %s con SHA-256 di SHA256SUMS assente dalla riga" % (key, name))
        if row[2].strip("`") != sums[lib["tarball"]]:
            fail("%s: la colonna del tarball non ha lo SHA-256 di SHA256SUMS" % key)
        for name in lib["licenses"]:
            pair = re.search(r"%s`?\s*\(`?([0-9a-f]{64})`?\)" % re.escape(name), row[3])
            if pair is None or pair.group(1) != sums[name]:
                fail("%s: la riga associa a %s un hash diverso da quello di SHA256SUMS (%s)" % (key, name, sums[name]))
        if lib["license_identity"] not in row[4]:
            fail("%s: colonna «Licenza letta» senza l'identità della licenza (%s): %r" % (key, lib["license_identity"], row[4]))
        listed = set(HEX64.findall(line))
        expected = {sums[lib["tarball"]]} | {sums[n] for n in lib["licenses"]}
        if listed != expected:
            fail("%s: la riga cita hash che non sono in SHA256SUMS: %s" % (key, sorted(listed - expected)))
        if lib["chosen_license"]:
            if lib["chosen_license"] not in row[5] or lib["chosen_license"] not in prov:
                fail("%s: licenza scelta %s assente dalla colonna «Scelta» o da PROVENANCE.md" % (key, lib["chosen_license"]))
        elif row[5].strip("— -") or lib["license_identity"] not in prov:
            fail("%s: nessuna licenza da scegliere: «Scelta» deve essere «—» e PROVENANCE.md deve dire %s" % (key, lib["license_identity"]))
    print("T-U12-10 ok")


def t11():
    require_libs_in_tree()
    cpp = read_text(os.path.join(ROOT, "tests", "u12", "u12_versions_static.cpp"))
    major, minor, micro = (int(x) for x in LIBS["cfitsio"]["version"].split("."))
    lr = tuple(int(x) for x in LIBS["libraw"]["version"].split("."))
    wc = LIBS["wcslib"]["version"]
    checks = [
        ("LIBRAW_MAKE_VERSION(%d, %d, %d)" % lr, "LibRaw"),
        ("CFITSIO_MAJOR == %d" % major, "CFITSIO_MAJOR"),
        ("CFITSIO_MINOR == %d" % minor, "CFITSIO_MINOR"),
        ("CFITSIO_MICRO == %d" % micro, "CFITSIO_MICRO"),
        ("WCSLIB_VERSION == %s" % wc, "WCSLIB"),
    ]
    for needle, what in checks:
        if needle not in cpp:
            fail("u12_versions_static.cpp: costante %s non aggiornata (atteso %r)" % (what, needle))
    if EXP["libraw_version_number"] != (lr[0] << 16) | (lr[1] << 8) | lr[2]:
        fail("EXPECTED_RUNTIME.libraw_version_number non corrisponde alla versione di LibRaw")
    if EXP["libraw_version"] != "%s-Release" % LIBS["libraw"]["version"]:
        fail("EXPECTED_RUNTIME.libraw_version non corrisponde alla versione di LibRaw")
    if EXP["cfitsio_version"] != "%.3f" % (major + minor / 100.0 + micro / 10000.0):
        fail("EXPECTED_RUNTIME.cfitsio_version non corrisponde a %%.3f della versione di CFITSIO")
    if EXP["wcslib_version"] != wc or EXP["wcslib_vers"] != (8, 10, 0):
        fail("EXPECTED_RUNTIME.wcslib_version non corrisponde alla versione di WCSLIB")
    for key, lib in LIBS.items():
        prov = read_text(os.path.join(ROOT, "third_party", key, "PROVENANCE.md"))
        if lib["version"] not in prov or lib["sha256"] not in prov:
            fail("%s: PROVENANCE.md non riporta versione e SHA-256 delle costanti dei test" % key)
    print("T-U12-11 ok")


LICENSE_REF = re.compile(r"(https?://[^\s)|`]+)[\s`]*\(\s*SHA-256[: ]+`?([0-9a-f]{64})`?\s*\)")


def evidence_records():
    """Registrazioni di docs/evidence/: ogni paragrafo (blocco senza righe vuote) e ogni riga di tabella."""
    root = os.path.join(ROOT, "docs", "evidence")
    records = []
    for dirpath, _, names in os.walk(root):
        for n in sorted(names):
            if not n.endswith(".md"):
                continue
            text = read_text(os.path.join(dirpath, n))
            records += [blk for blk in re.split(r"\n\s*\n", text) if blk.strip()]
            records += [ln for ln in text.splitlines() if ln.startswith("|")]
    if not records:
        fail("docs/evidence/ senza prove")
    return records


def release_artifacts(build):
    """Eseguibili del preset release il cui comando di collegamento contiene libastrocore.a: i NEEDED di libastrocore
    (un archivio non ne ha) si osservano su di loro; nessun eseguibile che non la colleghi entra nel perimetro."""
    out = run(["ninja", "-C", build, "-t", "commands"]).stdout
    exes = []
    for ln in out.splitlines():
        if " -o " not in ln or " -c " in ln:
            continue
        toks = shlex.split(ln)
        if not any(os.path.basename(t) == "libastrocore.a" for t in toks):
            continue
        target = toks[toks.index("-o") + 1]
        path = target if os.path.isabs(target) else os.path.join(build, target)
        if not path.endswith((".a", ".so")) and ".so." not in path:
            exes.append(path)
    if not exes:
        fail("nessun eseguibile del preset release collega libastrocore.a in %s" % build)
    for e in exes:
        if not os.path.isfile(e):
            fail("eseguibile %s non costruito" % e)
    return sorted(set(exes))


def t12(build):
    rows = section_rows(read_text(LICENSES), "## Librerie caricate dai binari", "| Libreria (soname) |")
    by_soname = {r[0].strip("`"): r for r in rows}
    if not find_archive(build, "libastrocore.a"):
        fail("libastrocore.a assente sotto %s: preset release non costruito" % build)
    records = evidence_records()
    sonames = sorted({n for exe in release_artifacts(build) for n in needed(exe)})
    for so in sonames:
        row = by_soname.get(so)
        if row is None:
            fail("docs/licenses.md: nessuna riga per %s (NEEDED di un eseguibile)" % so)
        lib_sha = row[4].strip("`")
        refs = [(u, h) for u, h in LICENSE_REF.findall(row[5]) if not DISTRO_HOSTS.search(u) and h != lib_sha]
        if not refs:
            fail("%s: la cella della licenza non ha «<URL upstream> (SHA-256 <hash>)» di un file di licenza upstream" % so)
        for url, h in refs:
            if not any(so in r and row[3] in r and url in r and h in r for r in records):
                fail("%s: nessuna registrazione di docs/evidence/ associa soname, versione installata (%s), URL (%s) e SHA-256 (%s) "
                     "del file di licenza" % (so, row[3], url, h))
    print("T-U12-12 ok (%d soname)" % len(sonames))


def main(argv):
    if argv[:1] in (["10"], ["11"]) and len(argv) == 1:
        {"10": t10, "11": t11}[argv[0]]()
    elif argv[:1] == ["12"] and len(argv) == 2:
        t12(os.path.abspath(argv[1]))
    else:
        sys.exit("uso: test_u12_docs.py 10|11 | 12 <directory di build>")


if __name__ == "__main__":
    try:
        main(sys.argv[1:])
    except SystemExit:
        raise
    except Exception as e:
        sys.exit("T-U12-docs: errore non previsto: %r" % (e,))
