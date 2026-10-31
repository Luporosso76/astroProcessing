"""Strumenti comuni dei test T-U12-01…12 (docs/contracts/U12-third-party.md). Ogni errore termina con sys.exit(<diagnosi>)."""
import hashlib
import os
import json
import re
import shlex
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Fatti di §1 e §2 del contratto: unica copia nei test. Una versione nuova li cambia (obbligo 11).
LIBS = {
    "libraw": {
        "name": "LibRaw",
        "version": "0.22.3",
        "tarball": "LibRaw-0.22.3.tar.gz",
        "url": "https://www.libraw.org/data/LibRaw-0.22.3.tar.gz",
        "page": "https://www.libraw.org/news/libraw-0-22-3-release",
        "sha256": "789f3c33466210a1a0907f747a97c715d21bbf4af712f257fcbbd90111b227e4",
        "licenses": ["COPYRIGHT", "LICENSE.LGPL", "LICENSE.CDDL"],
        "chosen_license": "LGPL-2.1",
        "license_field": "LGPL-2.1",
        "license_identity": "LGPL",
        "options": ["--enable-static", "--disable-shared", "--with-pic", "--disable-openmp", "--enable-zlib",
                    "--enable-jpeg", "--disable-lcms", "--disable-examples"],
    },
    "cfitsio": {
        "name": "CFITSIO",
        "version": "4.7.0",
        "tarball": "cfitsio-4.7.0.tar.gz",
        "url": "https://heasarc.gsfc.nasa.gov/FTP/software/fitsio/c/cfitsio-4.7.0.tar.gz",
        "page": "https://heasarc.gsfc.nasa.gov/fitsio/",
        "sha256": "ce573bbea8e75b429f8c3d3e86498741ba3dc9628a1530d2f65268397ad059e8",
        "licenses": ["License.txt"],
        "chosen_license": None,
        "license_field": "NASA",
        "license_identity": "NASA",
        "options": ["-DBUILD_SHARED_LIBS=OFF", "-DUSE_CURL=OFF", "-DUSE_BZIP2=OFF", "-DUSE_PTHREADS=ON", "-DTESTS=OFF",
                    "-DUTILS=OFF", "-DCMAKE_HAVE_GETHOSTBYNAME=OFF", "-DCMAKE_HAVE_CONNECT=OFF"],
    },
    "wcslib": {
        "name": "WCSLIB",
        "version": "8.10",
        "tarball": "wcslib-8.10.tar.bz2",
        "url": "https://www.atnf.csiro.au/computing/software/wcs/wcslib-releases/wcslib-8.10.tar.bz2",
        "page": "https://www.atnf.csiro.au/computing/software/wcs/",
        "sha256": "b394d3306da9a52410f056136911496e599b5c48f1cff2f221563697077bbd8d",
        "licenses": ["COPYING", "COPYING.LESSER"],
        "chosen_license": "LGPL-3.0-or-later",
        "license_field": "LGPL-3.0-or-later",
        "license_identity": "LGPL-3.0-or-later",
        "options": ["--without-cfitsio", "--without-pgplot", "--disable-fortran", "--disable-utils"],
    },
}

# Valori misurati sulle build di §2 (obblighi 5 e 6).
EXPECTED_RUNTIME = {
    "libraw_version_number": 0x001603,
    "libraw_version": "0.22.3-Release",
    "libraw_capabilities": 0xC0,
    "cfitsio_version": "4.070",
    "cfitsio_reentrant": 1,
    "wcslib_version": "8.10",
    "wcslib_vers": (8, 10, 0),
}

NETWORK_NEEDED = re.compile(
    r"^(libcurl|libssl|libgnutls|libssh|libldap|liblber|librtmp|libnghttp|libngtcp|libpsl|libgssapi|libkrb5|libsasl2)")
NETWORK_SYMBOLS = re.compile(
    r"^(socket|connect|bind|listen|accept|accept4|getaddrinfo|gethostbyname|gethostbyname_r|curl_.*)(@.*)?$")


def fail(msg):
    sys.exit("T-U12: " + msg)


def run(cmd, cwd=None, env=None, check=True):
    env = dict(os.environ if env is None else env, LC_ALL="C")
    try:
        p = subprocess.run(cmd, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    except OSError as e:
        fail("comando non avviabile %s: %s" % (cmd[0], e))
    if check and p.returncode != 0:
        fail("%s uscito con %d:\n%s" % (" ".join(cmd[:4]), p.returncode, p.stdout[-3000:]))
    return p


def rm_tree(path):
    """Cancella `path`; l'assenza e' ammessa, ogni altro errore e' una diagnosi col percorso."""
    if not os.path.lexists(path):
        return
    try:
        shutil.rmtree(path)
    except OSError as e:
        fail("impossibile cancellare %s: %s" % (path, e))


def sha256_file(path):
    h = hashlib.sha256()
    try:
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                h.update(chunk)
    except OSError as e:
        fail("impossibile leggere %s: %s" % (path, e))
    return h.hexdigest()


def read_text(path):
    try:
        with open(path, encoding="utf-8") as f:
            return f.read()
    except OSError as e:
        fail("impossibile leggere %s: %s" % (path, e))


def require_dir(path, what):
    if not os.path.isdir(path):
        fail("%s assente: %s" % (what, os.path.relpath(path, ROOT)))


def require_libs_in_tree():
    """Prerequisito comune: third_party/<nome>/ con tarball e SHA256SUMS."""
    for key, lib in LIBS.items():
        d = os.path.join(ROOT, "third_party", key)
        require_dir(d, "directory della libreria (U12 non ancora nel progetto)")
        for name in (lib["tarball"], "SHA256SUMS", "PROVENANCE.md"):
            if not os.path.isfile(os.path.join(d, name)):
                fail("third_party/%s/%s assente" % (key, name))


def find_files(build_dir, predicate, skip=()):
    found = []
    for dirpath, dirnames, filenames in os.walk(build_dir):
        dirnames[:] = [d for d in dirnames if d not in skip]
        for n in filenames:
            p = os.path.join(dirpath, n)
            if predicate(n, p):
                found.append(p)
    return sorted(found)


def find_archive(build_dir, name):
    return find_files(build_dir, lambda n, p: n == name and not os.path.islink(p))


def is_elf(path):
    try:
        with open(path, "rb") as f:
            return f.read(4) == b"\x7fELF"
    except OSError as e:
        fail("impossibile leggere %s: %s" % (path, e))


def product_executables(build_dir):
    """Eseguibili ELF del prodotto e dei test: `src/` e `tests/` della directory di build."""
    exes = []
    for sub in ("src", "tests"):
        root = os.path.join(build_dir, sub)
        if not os.path.isdir(root):
            continue
        exes += find_files(root, lambda n, p: os.access(p, os.X_OK) and ".so" not in n and not n.endswith((".a", ".o"))
                           and os.path.isfile(p) and is_elf(p), skip=("CMakeFiles",))
    if not exes:
        fail("nessun eseguibile ELF in %s/{src,tests}: build non eseguita?" % build_dir)
    return exes


def needed(path):
    out = run(["readelf", "-d", path]).stdout
    return re.findall(r"\(NEEDED\)\s+Shared library: \[([^\]]+)\]", out)


def ldd_closure(path):
    out = run(["ldd", path]).stdout
    libs = []
    for line in out.splitlines():
        if "not found" in line:
            fail("ldd: libreria non trovata per %s: %s" % (path, line.strip()))
        m = re.match(r"\s*(\S+)\s+=>\s+(\S+)", line)
        if m:
            libs.append(m.group(1))
    return libs


def undefined_symbols(path):
    out = run(["nm", "--undefined-only", "--format=just-symbols", path]).stdout
    return sorted(set(out.split()))


def required_arg(argv, n, usage):
    if len(argv) != n:
        sys.exit("uso: " + usage)
    return argv


PRODUCT_LINK_TARGET = "test_third_party_verify"
LIB_ARCHIVES = ("libraw_r.a", "libcfitsio.a", "libwcs.a")


def linked_archives(build):
    """Archivi di LibRaw, CFITSIO e WCSLIB nel comando di collegamento di un eseguibile che collega astrocore."""
    lines = [ln for ln in run(["ninja", "-C", build, "-t", "commands", PRODUCT_LINK_TARGET]).stdout.splitlines()
             if " -o " in ln and " -c " not in ln]
    if not lines:
        fail("nessun comando di collegamento di %s in %s (target assente?)"
             % (PRODUCT_LINK_TARGET, build))
    if len(lines) != 1:
        fail("%s ha %d comandi di collegamento in %s, atteso uno" % (PRODUCT_LINK_TARGET, len(lines), build))
    found = {}
    for tok in shlex.split(lines[0]):
        base = os.path.basename(tok)
        if base in LIB_ARCHIVES:
            path = os.path.realpath(tok if os.path.isabs(tok) else os.path.join(build, tok))
            found.setdefault(base, set()).add(path)
    for name in LIB_ARCHIVES:
        if name not in found:
            fail("il collegamento di %s non include %s: astrocore non collega l'archivio del progetto" % (PRODUCT_LINK_TARGET, name))
        if len(found[name]) != 1:
            fail("archivio %s con piu' copie nel collegamento: %s" % (name, sorted(found[name])))
    paths = [next(iter(found[n])) for n in LIB_ARCHIVES]
    for path in paths:
        if not os.path.isfile(path):
            fail("archivio %s non costruito" % path)
    return paths


ANCHOR_HEADER = {"wcslib/wcsconfig.h": "wcslib/wcslib.h"}


def unit_include_options(entry, build):
    """Opzioni -I / -isystem di una unità di compilazione, nell'ordine del comando: [(flag, directory assoluta)]."""
    toks = shlex.split(entry.get("command", ""))
    opts = []
    for i, t in enumerate(toks):
        if t.startswith("-I") and len(t) > 2:
            flag, d = "-I", t[2:]
        elif t.startswith("-isystem") and len(t) > 8:
            flag, d = "-isystem", t[8:]
        elif t in ("-I", "-isystem") and i + 1 < len(toks):
            flag, d = t, toks[i + 1]
        else:
            continue
        opts.append((flag, os.path.normpath(os.path.join(entry.get("directory", build), d))))
    return opts


def core_include_options(build, rels):
    """Opzioni di inclusione delle unita' di src/core che includono davvero gli header `rels` (il decoder include
    libraw, l'unita' CFITSIO fitsio.h): ordine e tipo dei comandi di compilazione, nessun riordino. Le unita' che
    includono lo stesso header devono risolverlo nella stessa directory."""
    path = os.path.join(build, "compile_commands.json")
    try:
        with open(path, encoding="utf-8") as f:
            db = json.load(f)
    except (OSError, ValueError) as e:
        fail("compile_commands.json illeggibile in %s: %s" % (build, e))
    core = [e for e in db if os.sep + os.path.join("src", "core") + os.sep in e.get("file", "")]
    if not core:
        fail("nessun comando di compilazione di src/core in %s" % path)
    merged = []
    for rel in rels:
        anchor = ANCHOR_HEADER.get(rel, rel)
        pat = re.compile(r'#\s*include\s*[<"]%s[>"]' % re.escape(anchor))
        users = [e for e in core if pat.search(read_text(os.path.join(e.get("directory", build), e["file"])))]
        if not users:
            fail("nessuna unita' di src/core include %s: libreria non nel progetto" % anchor)
        resolved = set()
        for e in users:
            opts = unit_include_options(e, build)
            hit = next((d for _, d in opts if os.path.isfile(os.path.join(d, rel))), None)
            if hit is None:
                fail("%s: header %s non raggiungibile dalle opzioni di inclusione" % (e["file"], rel))
            resolved.add(hit)
        if len(resolved) != 1:
            fail("le unita' di src/core che includono %s lo risolvono in directory diverse: %s" % (rel, sorted(resolved)))
        for opt in unit_include_options(users[0], build):
            if opt not in merged:
                merged.append(opt)
    return merged


def include_args(options):
    args = []
    for flag, d in options:
        args += [flag, d]
    return args
