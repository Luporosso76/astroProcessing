"""T-U12-02: build delle tre librerie dentro build/, senza rete, con hash verificato (obbligo 2).

Modi: `offline` (configura e costruisce da una directory vuota sotto strace, una traccia per fase) e `tamper` (un byte di ciascuno
dei tre tarball alterato in una copia dei sorgenti: la build deve fallire con la diagnosi di hash discordante di tutti e tre). Uso: test_u12_02_build.py <modo> <compilatore C++>."""
import os
import re
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from u12_common import LIBS, ROOT, fail, find_archive, find_files, read_text, require_libs_in_tree, rm_tree, run  # noqa: E402

WORK = {m: os.path.join(ROOT, "tmp", "u12-build-" + m) for m in ("offline", "tamper")}
# Opzioni che scelgono una funzione della libreria: ogni occorrenza deve essere fra quelle di §2.
OPTION_FAMILY = re.compile(r"^(--(enable|disable|with|without)-|-D(USE_|BUILD_|TESTS|UTILS|CMAKE_HAVE_))")
EXCLUDE = {".git", ".claude", ".codegraph", "graphify-out", ".secrets", "build", "tmp", "data", ".omc", ".remember"}


def env_with_tmp():
    env = dict(os.environ)
    env["TMPDIR"] = os.path.join(ROOT, "tmp")
    env["CMAKE_BUILD_PARALLEL_LEVEL"] = "4"
    return env


def configure_args(src, build, cxx):
    return ["cmake", "-S", src, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=RelWithDebInfo", "-DCMAKE_CXX_COMPILER=" + cxx]


def check_connect_log(log, phase):
    try:
        with open(log, encoding="utf-8", errors="replace") as f:
            lines = f.read().splitlines()
    except OSError as e:
        fail("log di strace (%s) illeggibile: %s" % (phase, e))
    inet = [ln for ln in lines if re.search(r"sa_family=AF_INET6?\b", ln)]
    if inet:
        fail("connect di rete durante %s: %s" % (phase, inet[:3]))


def under(path, build):
    root = os.path.realpath(build)
    real = os.path.realpath(path)
    return real == root or real.startswith(root + os.sep)


def step_command(ninja_lines, name, step):
    """Comando del passo <step> di ExternalProject <name> in build.ninja (ultimo `COMMAND =` della sua regola)."""
    head = re.compile(r"^build \S*-stamp/%s-%s\b[^:]*:\s*CUSTOM_COMMAND" % (re.escape(name), step))
    idx = [i for i, ln in enumerate(ninja_lines) if head.match(ln)]
    if len(idx) != 1:
        fail("%s: passo %s trovato %d volte in build.ninja" % (name, step, len(idx)))
    for ln in ninja_lines[idx[0] + 1:]:
        if ln.startswith("build "):
            break
        if ln.startswith("  COMMAND = "):
            return ln[len("  COMMAND = "):]
    fail("%s: passo %s senza COMMAND in build.ninja" % (name, step))


def check_external_projects(build):
    """Configurazione generata da ExternalProject_Add per ciascuna libreria (obbligo 2): estrazione, configurazione,
    compilazione e installazione, ognuna dentro build/ (directory di lavoro e prefisso letti dai passi effettivi)."""
    infos = find_files(build, lambda n, p: n.endswith("-urlinfo.txt"))
    ninja_lines = read_text(os.path.join(build, "build.ninja")).splitlines()
    for key, lib in LIBS.items():
        mine = [p for p in infos if lib["tarball"] in read_text(p)]
        if len(mine) != 1:
            fail("%s: attesi un solo ExternalProject con URL su %s, trovati %d" % (key, lib["tarball"], len(mine)))
        info = read_text(mine[0])
        if "third_party/%s/%s" % (key, lib["tarball"]) not in info:
            fail("%s: l'URL non punta a third_party/%s/%s" % (key, key, lib["tarball"]))
        if re.search(r"https?://|ftp://", info):
            fail("%s: URL di rete nella configurazione di ExternalProject" % key)
        if "SHA256=" + lib["sha256"] not in info:
            fail("%s: URL_HASH SHA256=%s assente" % (key, lib["sha256"]))
        name = os.path.basename(mine[0])[: -len("-urlinfo.txt")]
        cfgs = find_files(build, lambda n, p: n == name + "-cfgcmd.txt")
        if len(cfgs) != 1:
            fail("%s: attesi un solo %s-cfgcmd.txt, trovati %d" % (key, name, len(cfgs)))
        cfg = read_text(cfgs[0])
        for opt in lib["options"]:
            if opt not in cfg:
                fail("%s: opzione %s assente dal comando di configurazione" % (key, opt))
        extra = sorted({t for t in re.split(r"[\s;]+", cfg) if OPTION_FAMILY.match(t) and t not in lib["options"]})
        if extra:
            fail("%s: opzioni non previste da §2: %s" % (key, extra))
        for field in ("source_dir", "work_dir"):
            m = re.search(r"^%s=(\S+)$" % field, info, re.M)
            if m is None or not under(m.group(1), build):
                fail("%s: estrazione (%s) fuori da %s: %s" % (key, field, build, m.group(1) if m else "assente"))
        for step in ("configure", "build", "install"):
            cmd = step_command(ninja_lines, name, step)
            cd = re.match(r"cd (\S+) &&", cmd)
            if cd is None or not under(cd.group(1), build):
                fail("%s: il passo %s non gira dentro %s: %s" % (key, step, build, cmd[:200]))
            if step == "configure":
                prefix = re.search(r"(?:--prefix=|-DCMAKE_INSTALL_PREFIX=)([^\s;']+)", cmd)
                if prefix is None or not under(prefix.group(1), build):
                    fail("%s: prefisso d'installazione fuori da %s: %s" % (key, build, prefix.group(1) if prefix else "assente"))


def offline(cxx):
    require_libs_in_tree()
    work = WORK["offline"]
    rm_tree(work)
    build = os.path.join(work, "build")
    os.makedirs(work)
    env = env_with_tmp()
    log_cfg = os.path.join(work, "connect-configure.log")
    log_build = os.path.join(work, "connect-build.log")
    run(["strace", "-f", "-qq", "-e", "trace=connect", "-o", log_cfg] + configure_args(ROOT, build, cxx), env=env)
    run(["strace", "-f", "-qq", "-e", "trace=connect", "-o", log_build, "cmake", "--build", build, "-j4"], env=env)
    check_connect_log(log_cfg, "la configurazione")
    check_connect_log(log_build, "la build")
    check_external_projects(build)
    for archive in ("libraw_r.a", "libcfitsio.a", "libwcs.a"):
        if not find_archive(build, archive):
            fail("%s non costruito sotto %s" % (archive, build))
    print("T-U12-02 offline ok")


def tamper(cxx):
    require_libs_in_tree()
    work = WORK["tamper"]
    rm_tree(work)
    src = os.path.join(work, "src")
    build = os.path.join(work, "build")
    shutil.copytree(ROOT, src, ignore=lambda d, names: [n for n in names if d == ROOT and n in EXCLUDE], symlinks=True)
    for key, lib in LIBS.items():
        tarball = os.path.join(src, "third_party", key, lib["tarball"])
        try:
            with open(tarball, "r+b") as f:
                f.seek(4096)
                b = f.read(1)
                f.seek(4096)
                f.write(bytes([b[0] ^ 0xFF]))
        except OSError as e:
            fail("impossibile alterare %s: %s" % (tarball, e))
    env = env_with_tmp()
    run(configure_args(src, build, cxx), env=env)
    p = run(["cmake", "--build", build, "-j4", "--", "-k", "0"], env=env, check=False)
    if p.returncode == 0:
        fail("la build è riuscita con tre tarball alterati di un byte")
    mismatches = re.findall(r"hash of\s+(\S+)\s+does not match expected value\s+expected:\s*'?([0-9a-fA-F]{64})", p.stdout)
    for key, lib in LIBS.items():
        if not any(os.path.basename(path) == lib["tarball"] and exp.lower() == lib["sha256"] for path, exp in mismatches):
            fail("%s: nessuna diagnosi di SHA-256 discordante per %s (atteso %s):\n%s" % (key, lib["tarball"], lib["sha256"], p.stdout[-2000:]))
    print("T-U12-02 tamper ok")


def main(argv):
    if len(argv) != 2 or argv[0] not in ("offline", "tamper"):
        sys.exit("uso: test_u12_02_build.py offline|tamper <compilatore C++>")
    {"offline": offline, "tamper": tamper}[argv[0]](argv[1])


if __name__ == "__main__":
    try:
        main(sys.argv[1:])
    except SystemExit:
        raise
    except Exception as e:
        sys.exit("T-U12-02: errore non previsto: %r" % (e,))
