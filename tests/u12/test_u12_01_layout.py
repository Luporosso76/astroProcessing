"""T-U12-01: layout di third_party/<nome>/ (obbligo 1)."""
import os
import re
import subprocess
import sys
import tarfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from u12_common import LIBS, ROOT, fail, read_text, require_libs_in_tree, run, sha256_file  # noqa: E402


def tar_member_bytes(tar_path, basename):
    found = []
    try:
        with tarfile.open(tar_path) as tf:
            for m in tf:
                if m.isfile() and os.path.basename(m.name) == basename:
                    found.append((m.name, tf.extractfile(m).read()))
    except (OSError, tarfile.TarError) as e:
        fail("tarball illeggibile %s: %s" % (tar_path, e))
    if not found:
        fail("%s: nessun membro %s" % (tar_path, basename))
    if len(found) > 1:
        # Più membri omonimi: il file di licenza della radice è quello col percorso più corto.
        found.sort(key=lambda t: len(t[0]))
    return found[0][1]


def git_listed(dirpath):
    out = run(["git", "ls-files", "--cached", "--others", "--exclude-standard", "--", dirpath], cwd=ROOT).stdout
    return sorted(os.path.basename(p) for p in out.split())


def check_lib(key, lib):
    d = os.path.join(ROOT, "third_party", key)
    tarball = os.path.join(d, lib["tarball"])
    if sha256_file(tarball) != lib["sha256"]:
        fail("%s: SHA-256 del tarball diverso da §1 (%s)" % (key, lib["sha256"]))

    p = subprocess.run(["sha256sum", "-c", "SHA256SUMS"], cwd=d, env=dict(os.environ, LC_ALL="C"), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if p.returncode != 0:
        fail("%s: sha256sum -c SHA256SUMS fallito:\n%s" % (key, p.stdout))

    sums = read_text(os.path.join(d, "SHA256SUMS"))
    covered = set()
    for line in sums.splitlines():
        m = re.match(r"^([0-9a-f]{64}) [ *](\S+)$", line)
        if not m:
            fail("%s: riga di SHA256SUMS non nel formato di sha256sum: %r" % (key, line))
        covered.add(m.group(2))
    covered_expected = {lib["tarball"], "PROVENANCE.md"} | set(lib["licenses"])
    if covered != covered_expected:
        fail("%s: SHA256SUMS copre %s, atteso %s" % (key, sorted(covered), sorted(covered_expected)))
    allowed = covered_expected | {"SHA256SUMS"}

    for name in lib["licenses"]:
        path = os.path.join(d, name)
        if not os.path.isfile(path):
            fail("%s: file di licenza assente: %s" % (key, name))
        with open(path, "rb") as f:
            if f.read() != tar_member_bytes(tarball, name):
                fail("%s: %s diverso dal membro omonimo del tarball" % (key, name))

    extra = set(git_listed(os.path.join("third_party", key))) - allowed
    if extra:
        fail("%s: file non ammessi in Git (sorgenti estratti?): %s" % (key, sorted(extra)))

    prov = read_text(os.path.join(d, "PROVENANCE.md"))
    low = prov.lower()
    must = [lib["url"], lib["page"], lib["version"], lib["sha256"]] + lib["options"]
    if lib["chosen_license"]:
        must.append(lib["chosen_license"])
    for needle in must:
        if needle not in prov:
            fail("%s: PROVENANCE.md non contiene %r" % (key, needle))
    for label in ("data di scaricamento", "hash pubblicato", "licenza", "modifiche locali"):
        if label not in low:
            fail("%s: PROVENANCE.md senza il campo %r" % (key, label))
    if not re.search(r"data di scaricamento[^\n]*(\d{4}-\d{2}-\d{2}|\d{1,2} [a-zà-ù]+ \d{4})", low):
        fail("%s: «data di scaricamento» senza una data (AAAA-MM-GG o «G mese AAAA»)" % key)
    if not any("licenza" in ln and lib["license_field"].lower() in ln for ln in low.splitlines()):
        fail("%s: nessuna riga «licenza» con il valore %r" % (key, lib["license_field"]))
    if not re.search(r"hash pubblicato[^\n]*nessuno", low):
        fail("%s: «hash pubblicato» deve dire «nessuno» (§1: nessuna delle fonti ne pubblica uno)" % key)
    if not re.search(r"modifiche locali[^\n]*nessuna", low):
        fail("%s: «modifiche locali» deve dire «nessuna» (obbligo 1)" % key)


def main():
    require_libs_in_tree()
    for key, lib in LIBS.items():
        check_lib(key, lib)
    print("T-U12-01 ok")


if __name__ == "__main__":
    try:
        main()
    except SystemExit:
        raise
    except Exception as e:
        sys.exit("T-U12-01: errore non previsto: %r" % (e,))
