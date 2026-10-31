"""T-U12-03: nessuna lettura della versione di sistema (obbligo 3).

Uso: test_u12_03_headers.py <radice dei sorgenti> <directory di build> <compilatore C++>."""
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from u12_common import ROOT, core_include_options, fail, find_files, include_args, read_text, run  # noqa: E402

LIB_WORDS = re.compile(r"libraw|raw_r|cfitsio|fitsio|wcslib|libwcs", re.I)
LOOKUP = re.compile(r"\b(pkg_check_modules|pkg_search_module|find_package|find_path|find_library|find_file)\s*\(([^)]*)\)", re.I)
SYSTEM_HEADER = re.compile(r"(^|/)(libraw/libraw\.h|fitsio\.h|wcslib[^/]*/(wcslib|wcs)\.h)$")


def cmake_files(src):
    files = [os.path.join(src, "CMakeLists.txt")]
    for sub in ("cmake", "src", "tests", os.path.join("third_party", "libraw"), os.path.join("third_party", "cfitsio"),
                os.path.join("third_party", "wcslib")):
        d = os.path.join(src, sub)
        if os.path.isdir(d):
            files += find_files(d, lambda n, p: n == "CMakeLists.txt" or n.endswith(".cmake"))
    return files


def scan_cmake(src):
    bad = []
    for path in cmake_files(src):
        text = "\n".join(re.sub(r"#.*", "", ln) for ln in read_text(path).splitlines())
        for m in LOOKUP.finditer(text):
            if LIB_WORDS.search(m.group(2)):
                bad.append("%s: %s(%s)" % (os.path.relpath(path, src), m.group(1), " ".join(m.group(2).split())))
    if bad:
        fail("ricerca di LibRaw/CFITSIO/WCSLIB di sistema nei CMake:\n  " + "\n  ".join(bad))


def scan_compile_commands(build):
    path = os.path.join(build, "compile_commands.json")
    try:
        with open(path, encoding="utf-8") as f:
            db = json.load(f)
    except (OSError, ValueError) as e:
        fail("compile_commands.json illeggibile in %s: %s" % (build, e))
    bad = sorted({e["file"] for e in db if re.search(r"/usr/include/(libraw|wcslib)", e.get("command", ""))})
    if bad:
        fail("-I/-isystem verso /usr/include/libraw o /usr/include/wcslib nei comandi di: %s" % bad[:5])


def scan_depfiles(build):
    out = run(["ninja", "-C", build, "-t", "deps"]).stdout
    root = os.path.realpath(build)
    deps = set()
    for line in out.splitlines():
        if line.startswith("    "):
            deps.add(line.strip())
    outside, inside = [], []
    for d in deps:
        if not SYSTEM_HEADER.search(d):
            continue
        real = os.path.realpath(d if os.path.isabs(d) else os.path.join(build, d))
        (inside if real.startswith(root + os.sep) else outside).append(d)
    if outside:
        fail("header di LibRaw/CFITSIO/WCSLIB risolti fuori da build/: %s" % sorted(outside)[:5])
    for needle in ("libraw/libraw.h", "fitsio.h"):
        if not any(d.endswith(needle) for d in inside):
            fail("nessuna unità di traduzione del progetto include %s da build/ (file .d)" % needle)


def static_asserts(build, cxx):
    incs = include_args(core_include_options(build, ("libraw/libraw.h", "fitsio.h", "wcslib/wcslib.h", "wcslib/wcsconfig.h")))
    cmd = [cxx, "-std=c++20", "-fsyntax-only"] + incs + [os.path.join(ROOT, "tests", "u12", "u12_versions_static.cpp")]
    run(cmd)


def main(argv):
    if len(argv) != 3:
        sys.exit("uso: test_u12_03_headers.py <sorgenti> <build> <compilatore C++>")
    src, build, cxx = argv
    scan_cmake(src)
    scan_compile_commands(build)
    scan_depfiles(build)
    static_asserts(build, cxx)
    print("T-U12-03 ok")


if __name__ == "__main__":
    try:
        main(sys.argv[1:])
    except SystemExit:
        raise
    except Exception as e:
        sys.exit("T-U12-03: errore non previsto: %r" % (e,))
