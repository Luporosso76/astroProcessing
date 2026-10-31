"""T-U12-05 (versioni) e T-U12-06 (capacità): valori visti da un processo che collega gli archivi del progetto.

Uso: test_u12_runtime.py 05|06 <directory di build> <compilatore C++>.
Archivi e include sono quelli del collegamento e della compilazione di astrocore (linked_archives, core_include_options).
I death test su astro::third_party::check e verified() sono in test_third_party_verify.cpp."""
import os
import resource
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from u12_common import EXPECTED_RUNTIME as EXP, ROOT, core_include_options, fail, include_args, linked_archives, rm_tree, run  # noqa: E402


def no_core():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def probe(mode, build, cxx):
    work = os.path.join(ROOT, "tmp", "u12-probe-" + mode)
    rm_tree(work)
    os.makedirs(work)
    archives = linked_archives(build)
    incs = include_args(core_include_options(build, ("libraw/libraw.h", "fitsio.h", "wcslib/wcslib.h")))
    exe = os.path.join(work, "probe")
    cmd = [cxx, "-std=c++20", "-O0"] + incs + [os.path.join(ROOT, "tests", "u12", "u12_runtime_probe.cpp")]
    cmd += [archives[0], archives[1], archives[2], "-ljpeg", "-lz", "-lm", "-lpthread", "-o", exe]
    env = dict(os.environ, TMPDIR=os.path.join(ROOT, "tmp"))
    run(cmd, env=env)
    p = subprocess.run([exe], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, preexec_fn=no_core)
    if p.returncode != 0:
        fail("la sonda è uscita con %d:\n%s" % (p.returncode, p.stdout))
    vals = dict(line.split("=", 1) for line in p.stdout.splitlines() if "=" in line)
    return vals


def expect(vals, key, want, fmt=str):
    got = vals.get(key)
    if got is None or got != fmt(want):
        fail("%s = %s, atteso %s" % (key, got, fmt(want)))


def main(argv):
    if len(argv) != 3 or argv[0] not in ("05", "06"):
        sys.exit("uso: test_u12_runtime.py 05|06 <directory di build> <compilatore C++>")
    mode, build, cxx = argv[0], os.path.abspath(argv[1]), argv[2]
    vals = probe(mode, build, cxx)
    if mode == "05":
        expect(vals, "libraw_version_number", EXP["libraw_version_number"])
        expect(vals, "libraw_version", EXP["libraw_version"])
        expect(vals, "cfitsio_version", EXP["cfitsio_version"])
        expect(vals, "wcslib_version", EXP["wcslib_version"])
        expect(vals, "wcslib_vers", EXP["wcslib_vers"], lambda t: "%d,%d,%d" % t)
    else:
        expect(vals, "libraw_capabilities", EXP["libraw_capabilities"])
        expect(vals, "cfitsio_reentrant", EXP["cfitsio_reentrant"])
    print("T-U12-%s ok" % mode)


if __name__ == "__main__":
    try:
        main(sys.argv[1:])
    except SystemExit:
        raise
    except Exception as e:
        sys.exit("T-U12-runtime: errore non previsto: %r" % (e,))
