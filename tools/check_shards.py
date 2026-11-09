#!/usr/bin/env python3 -S
"""Shard della suite fast di `tools/check.sh --step test --shard K/N` e loro unione.

Uso: tools/check_shards.py merge --listing <ctest -N> --dir <dir> --out <junit>
     tools/check_shards.py selftest
L'elenco di `ctest -N -I K,,N` (<dir>/dev-fast-list.K-of-N.txt) è l'unica verità su cosa contiene lo shard K
(ctest vi aggiunge anche le fixture di setup). `merge` unisce <dir>/dev-junit.K-of-N.xml in <out> solo se: ogni shard
esiste, tutti hanno lo stesso N e K è unico, ogni junit coincide esattamente col proprio elenco, l'unione degli elenchi
è uguale all'elenco `ctest -N` della suite fast, nessun test (anche se ripetuto in più shard) è fallito o non
eseguito. Il junit unito ha ogni test una volta. Altrimenti sys.exit con diagnosi; `out` non resta mai su disco.
"""
import glob
import os
import re
import shutil
import sys
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHARD_FILE = re.compile(r"^dev-junit\.([1-9][0-9]{0,2})-of-([1-9][0-9]{0,2})\.xml$")
LISTING_LINE = re.compile(r"^\s*Test\s+#([0-9]+):\s+(\S.*?)\s*$")


def parse_listing(path):
    """[(numero, nome)] dall'uscita di `ctest -N`; l'elenco vuoto o con nomi doppi è un errore."""
    if not os.path.isfile(path):
        sys.exit(f"shard: elenco ctest assente: {path}")
    tests = []
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            m = LISTING_LINE.match(line)
            if m:
                tests.append((int(m.group(1)), m.group(2)))
    if not tests:
        sys.exit(f"shard: nessun test nell'elenco ctest {path}")
    names = [n for _, n in tests]
    dup = sorted({n for n in names if names.count(n) > 1})
    if dup:
        sys.exit(f"shard: nomi di test doppi nell'elenco ctest: {', '.join(dup[:5])}")
    return tests


def read_shard(path):
    try:
        root = ET.parse(path).getroot()
    except (OSError, ET.ParseError) as exc:
        sys.exit(f"shard: junit non leggibile {path}: {exc}")
    return root, root.findall("testcase")


def find_shards(directory):
    """{K: (N, percorso)}: errore se un nome del tipo shard è malformato o se gli N sono diversi o K è doppio."""
    found = {}
    sizes = set()
    for p in sorted(glob.glob(os.path.join(directory, "dev-junit.*-of-*.xml"))):
        m = SHARD_FILE.match(os.path.basename(p))
        if not m:
            sys.exit(f"shard: nome non riconosciuto: {p}")
        k, n = int(m.group(1)), int(m.group(2))
        if not 1 <= k <= n:
            sys.exit(f"shard: {p}: K fuori intervallo 1..{n}")
        sizes.add(n)
        if k in found and len(sizes) == 1:
            sys.exit(f"shard: K={k} doppio in {directory}")
        found[k] = (n, p)
    if len(sizes) > 1:
        sys.exit(f"shard: shard di N diversi in {directory}: N={sorted(sizes)}; "
                 f"rimuovere dev-junit.*-of-*.xml e rilanciare tutti gli shard con lo stesso N")
    return found, (sizes.pop() if sizes else None)


def merge(listing, directory, out):
    if os.path.exists(out):
        os.remove(out)
    tests = parse_listing(listing)
    found, n = find_shards(directory)
    if not found:
        sys.exit(f"shard: nessuno shard dev-junit.K-of-N.xml in {directory}")
    missing = [k for k in range(1, n + 1) if k not in found]
    if missing:
        sys.exit(f"shard: mancano gli shard {', '.join(f'{k}/{n}' for k in missing)}: "
                 f"eseguire tools/check.sh --step test --shard K/{n} per ciascuno")
    by_num = {name: num for num, name in tests}
    union = set()
    cases_by_name = {}
    for k in range(1, n + 1):
        path = found[k][1]
        shard_list = {name for _, name in parse_listing(os.path.join(directory, f"dev-fast-list.{k}-of-{n}.txt"))}
        union |= shard_list
        root, cases = read_shard(path)
        names = []
        for tc in cases:
            name = tc.get("name")
            if not name:
                sys.exit(f"shard: {path}: testcase senza nome")
            if name in names:
                sys.exit(f"shard: test doppio {name} in {path}")
            names.append(name)
            if tc.get("status", "run") != "run" or any(tc.find(t) is not None for t in ("failure", "error", "skipped")):
                sys.exit(f"shard: {path}: test {name} non passato")
            cases_by_name.setdefault(name, tc)
        got = set(names)
        if got != shard_list:
            lack = sorted(shard_list - got)
            extra = sorted(got - shard_list)
            sys.exit(f"shard: {path} non corrisponde all'elenco ctest dello shard {k}/{n} "
                     f"(mancano {len(lack)}: {', '.join(lack[:3])}; in più {len(extra)}: {', '.join(extra[:3])}): "
                     f"junit vecchio o N diverso, rieseguire lo shard {k}/{n}")
    if union != set(by_num):
        lack = sorted(set(by_num) - union)
        extra = sorted(union - set(by_num))
        sys.exit(f"shard: gli elenchi degli shard non coprono la suite fast "
                 f"(senza shard {len(lack)}: {', '.join(lack[:3])}; fuori suite {len(extra)}: {', '.join(extra[:3])})")
    merged = sorted(cases_by_name.values(), key=lambda tc: by_num[tc.get("name")])
    suite = ET.Element("testsuite", {
        "name": "(empty)", "tests": str(len(merged)), "failures": "0", "disabled": "0", "skipped": "0",
        "hostname": "", "time": f"{sum(float(tc.get('time', '0')) for tc in merged):g}",
    })
    suite.extend(merged)
    ET.ElementTree(suite).write(out, encoding="UTF-8", xml_declaration=True)
    return len(merged), n


def case_xml(name):
    return f'<testcase name="{name}" classname="{name}" time="0.5" status="run"/>'


def selftest():
    d = os.path.join(ROOT, "tmp", "check-shards-selftest")
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    listing = os.path.join(d, "listing.txt")
    fixture = "T.setup"
    with open(listing, "w", encoding="utf-8") as fh:
        fh.write("Test project x\n" + "".join(f"  Test #{i}: T.t{i}\n" for i in range(1, 8)) + f"  Test #8: {fixture}\n"
                 + "\nTotal Tests: 8\n")
    tests = parse_listing(listing)
    out = os.path.join(d, "merged.xml")

    def expect_fail(label, fragment):
        try:
            merge(listing, d, out)
        except SystemExit as exc:
            if fragment not in str(exc.code):
                sys.exit(f"selftest fallito: {label}: diagnosi senza \u00ab{fragment}\u00bb: {exc.code}")
            return
        sys.exit(f"selftest fallito: {label}: doveva fallire")

    def reset():
        for p in glob.glob(os.path.join(d, "dev-*")):
            os.remove(p)

    def expected_shard(k, n):
        return {name for num, name in tests if num % n == k % n}

    def shard_names(k, n):
        # La fixture di setup entra in ogni shard che non la possiede, come fa ctest.
        return sorted(expected_shard(k, n) | {fixture})

    def put_list(k, n, names):
        with open(os.path.join(d, f"dev-fast-list.{k}-of-{n}.txt"), "w", encoding="utf-8") as fh:
            fh.write("".join(f"  Test #{i}: {x}\n" for i, x in enumerate(names, 1)))

    def put_junit(k, n, names=None, body=None):
        with open(os.path.join(d, f"dev-junit.{k}-of-{n}.xml"), "w", encoding="utf-8") as fh:
            fh.write(body if body is not None else '<?xml version="1.0"?><testsuite name="(empty)">'
                     + "".join(case_xml(x) for x in (shard_names(k, n) if names is None else names)) + "</testsuite>")

    def fill(n, skip=()):
        for k in range(1, n + 1):
            put_list(k, n, shard_names(k, n))
            if k not in skip:
                put_junit(k, n)

    reset()
    expect_fail("nessuno shard", "nessuno shard")
    fill(3, skip=(2,))
    expect_fail("shard mancante", "mancano gli shard 2/3")
    reset()
    fill(3)
    put_junit(1, 2)
    expect_fail("N diversi", "N diversi")
    reset()
    fill(3)
    put_junit(2, 3, names=shard_names(2, 3) + ["T.t1"])
    expect_fail("test di un altro shard", "non corrisponde")
    reset()
    fill(3)
    put_junit(2, 3, names=shard_names(2, 3) + [fixture])
    expect_fail("test doppio nello stesso shard", "test doppio")
    reset()
    fill(3)
    put_junit(3, 3, names=shard_names(3, 3)[:-1])
    expect_fail("buco", "non corrisponde")
    reset()
    fill(3)
    put_junit(1, 3, body='<testsuite><testcase name="T.t1" status="fail"><failure/></testcase></testsuite>')
    expect_fail("test fallito", "non passato")
    reset()
    fill(3)
    put_junit(1, 3, body="<testsuite>")
    expect_fail("xml rotto", "non leggibile")
    reset()
    fill(3)
    os.remove(os.path.join(d, "dev-fast-list.2-of-3.txt"))
    expect_fail("elenco dello shard assente", "elenco ctest assente")
    for label, fname in (("nome shard con zero iniziale", "dev-junit.01-of-3.xml"),
                         ("nome shard con K zero", "dev-junit.0-of-3.xml"),
                         ("nome shard malformato", "dev-junit.x-of-3.xml")):
        reset()
        fill(3)
        shutil.copy(os.path.join(d, "dev-junit.1-of-3.xml"), os.path.join(d, fname))
        expect_fail(label, "nome non riconosciuto")
    reset()
    fill(3)
    shutil.copy(os.path.join(d, "dev-junit.1-of-3.xml"), os.path.join(d, "dev-junit.4-of-3.xml"))
    expect_fail("K fuori intervallo", "K fuori intervallo")
    reset()
    fill(3)
    put_list(2, 3, shard_names(2, 3) + ["T.estraneo"])
    put_junit(2, 3, names=shard_names(2, 3) + ["T.estraneo"])
    expect_fail("elenco shard con test fuori suite", "fuori suite 1: T.estraneo")
    reset()
    fill(3)
    put_list(1, 3, [x for x in shard_names(1, 3) if x != "T.t4"])
    put_junit(1, 3, names=[x for x in shard_names(1, 3) if x != "T.t4"])
    expect_fail("elenchi che non coprono la suite", "non coprono")
    reset()
    fill(3)
    put_junit(2, 3, body='<testsuite>' + "".join(
        f'<testcase name="{x}" status="run"><failure/></testcase>' if x == fixture else case_xml(x)
        for x in shard_names(2, 3)) + '</testsuite>')
    expect_fail("fixture duplicata fallita in uno shard", "non passato")
    reset()
    fill(3)
    with open(out, "w", encoding="utf-8") as fh:
        fh.write("residuo")
    put_junit(1, 3, body="<testsuite>")
    expect_fail("unione fallita", "non leggibile")
    if os.path.exists(out):
        sys.exit("selftest fallito: il junit unito preesistente non è sparito dopo un'unione fallita")
    reset()
    fill(3)
    count, n = merge(listing, d, out)
    if (count, n) != (8, 3):
        sys.exit(f"selftest fallito: unione corretta: attesi 8 test e N=3, ottenuti {count} e N={n}")
    names = [tc.get("name") for tc in ET.parse(out).getroot().iter("testcase")]
    if names != [f"T.t{i}" for i in range(1, 8)] + [fixture]:
        sys.exit(f"selftest fallito: ordine o doppioni dell'unione: {names}")
    shutil.rmtree(d)
    print("check_shards selftest: ok")


def main(argv):
    if argv[:1] == ["selftest"] and len(argv) == 1:
        selftest()
        return 0
    if argv[:1] == ["merge"]:
        opts = dict(zip(argv[1::2], argv[2::2]))
        if len(argv) != 7 or set(opts) != {"--listing", "--dir", "--out"}:
            sys.exit("uso: check_shards.py merge --listing <ctest -N> --dir <dir> --out <junit>")
        count, n = merge(opts["--listing"], opts["--dir"], opts["--out"])
        print(f"shard: {n} shard uniti, {count} test, nessun buco né doppione -> {opts['--out']}")
        return 0
    sys.exit("uso: check_shards.py merge --listing <ctest -N> --dir <dir> --out <junit> | selftest")


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except Exception as exc:
        sys.exit(f"check_shards: {exc}")
