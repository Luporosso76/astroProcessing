#!/usr/bin/env python3
# T-M3A-RAW-07 (ADR-08.b §2.1, §5): simboli vietati e nessuna apertura per nome.
# Uso: test_raw07_offline.py <check_offline.sh> <compilatore C++> <raw_import_run> <directory temporanea> <binario>...
# 1. Controllo negativo: per ciascun simbolo vietato di §5 un oggetto con quel simbolo indefinito fa fallire
#    check_offline.sh e il simbolo è nominato nell'errore.
# Limite: sugli eseguibili check_offline.sh usa `nm -D` e non vede LibRaw collegata staticamente; per LibRaw vale il
# controllo su libastrocore.a, sugli eseguibili quello su NEEDED e libc.
# 2. Controllo positivo: i binari indicati (libastrocore e gli eseguibili del prodotto) passano.
# 3. Sotto `strace -f -e trace=network,openat`: dopo il marcatore nessuna chiamata di rete e nessun `openat` sotto la
#    directory dell'input diverso dall'input stesso (la riapertura dell'input per la riverifica prima della
#    pubblicazione, OB-11, è un `openat` dello stesso percorso); esattamente due aperture: lettura verificata e riverifica OB-11.
import os
import re
import shutil
import subprocess
import sys

FORBIDDEN = [
    ("libraw_open_file", "libraw_open_file"),
    ("libraw_open_wfile", "libraw_open_wfile"),
    ("_ZN6LibRaw9open_fileEPKcx", "_ZN6LibRaw9open_fileEPKcx"),  # LibRaw 0.22.3: INT64 è long long
    ("_ZN6LibRaw9open_fileEPKwx", "_ZN6LibRaw9open_fileEPKwx"),
    ("_ZN6LibRaw9open_fileEPKcxPi", "_ZN6LibRaw9open_fileEPKcxPi"),  # suffisso fittizio: il divieto è per prefisso
    ("TIFFOpen", "TIFFOpen"),
    ("TIFFOpenW", "TIFFOpenW"),
    ("TIFFOpenExt", "TIFFOpenExt"),
    ("TIFFFdOpen", "TIFFFdOpen"),
]


def die(msg):
    sys.exit("T-M3A-RAW-07: " + msg)


def run(cmd, **kw):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, **kw)
    except OSError as e:
        die("impossibile eseguire %s: %s" % (cmd[0], e))


def main():
    if len(sys.argv) < 6:
        die("uso: test_raw07_offline.py <check_offline.sh> <compilatore> <raw_import_run> <tmp> <binario>...")
    check, cxx, runner, tmp = sys.argv[1:5]
    binaries = sys.argv[5:]
    shutil.rmtree(tmp, ignore_errors=True)
    try:
        os.makedirs(tmp)
    except OSError as e:
        die("directory temporanea %s: %s" % (tmp, e))
    for sym, asm in FORBIDDEN:
        src = os.path.join(tmp, "neg_%s.cpp" % sym)
        obj = os.path.join(tmp, "neg_%s.o" % sym)
        try:
            with open(src, "w") as f:
                f.write('extern "C" void f(void) __asm__("%s");\nvoid use() { f(); }\n' % asm)
        except OSError as e:
            die("scrittura di %s: %s" % (src, e))
        cc = run([cxx, "-c", src, "-o", obj])
        if cc.returncode != 0:
            die("compilazione del controllo negativo %s: %s" % (sym, cc.stderr))
        res = run([check, obj])
        if res.returncode == 0:
            die("check_offline.sh accetta il simbolo vietato %s (atteso fallimento)" % sym)
        if sym not in res.stderr:
            die("check_offline.sh fallisce ma non nomina %s: %s" % (sym, res.stderr.strip()))
    for b in binaries:
        res = run([check, b])
        if res.returncode != 0:
            die("simboli o NEEDED vietati in %s: %s" % (b, res.stderr.strip()))
    strace = "/usr/bin/strace"
    if not os.access(strace, os.X_OK):
        die("strace assente: " + strace)
    canary = run([strace, "-f", "-e", "trace=network", "-o", os.path.join(tmp, "canary.log"), "bash", "-c",
                  "exec 3<>/dev/tcp/127.0.0.1/1"])
    try:
        with open(os.path.join(tmp, "canary.log")) as f:
            canary_text = f.read()
    except OSError as e:
        die("lettura di canary.log: %s" % e)
    if not re.search(r"^\d+ +(socket|connect)\(", canary_text, re.M):
        die("strace non ha rilevato il canarino di rete: il controllo sarebbe vuoto")
    work = os.path.join(tmp, "work")
    log = os.path.join(tmp, "run.strace")
    env = dict(os.environ)
    env["ASAN_OPTIONS"] = env.get("ASAN_OPTIONS", "") + ":detect_leaks=0"
    res = run([strace, "-f", "-e", "trace=network,openat", "-o", log, runner, work], env=env)
    if res.returncode != 0:
        die("raw_import_run: uscita %d: %s" % (res.returncode, res.stderr.strip()))
    inputs = os.path.join(os.path.abspath(work), "inputs")
    input_file = os.path.join(inputs, "file.dng")
    marker = os.path.join(os.path.abspath(work), "MARK_PRODUCT_START")
    try:
        with open(log) as f:
            lines = f.read().splitlines()
    except OSError as e:
        die("lettura della traccia %s: %s" % (log, e))
    start = None
    for i, line in enumerate(lines):
        if marker in line:
            start = i
    if start is None:
        die("marcatore assente dalla traccia: traccia non valida")
    reads = 0
    expected_opens = 2  # lettura verificata + riverifica prima della pubblicazione (BASE-confini.md OB-11)
    for line in lines[start + 1:]:
        if re.match(r"^\d+ +(socket|connect|bind|listen|accept|accept4|sendto|recvfrom|sendmsg|recvmsg)\(", line):
            die("chiamata di rete dopo il marcatore: " + line)
        m = re.match(r'^\d+ +openat\([^,]*, "([^"]*)"', line)
        if m is None:
            continue
        path = m.group(1)
        if path == input_file:
            reads += 1
        elif path == inputs or path.startswith(inputs + "/"):
            die("openat nella directory dell'input diverso dall'input: " + line)
    if reads != expected_opens:
        die("aperture dell'input dopo il marcatore: %d, attese %d (lettura verificata e riverifica OB-11)" % (reads, expected_opens))
    print("T-M3A-RAW-07: %d simboli vietati rifiutati, %d binari puliti, %d aperture dell'input, 0 altre" %
          (len(FORBIDDEN), len(binaries), reads))


main()
