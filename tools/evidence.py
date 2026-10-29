"""Rigenera il blocco generato di un record di evidenza.

Uso: python3 -S tools/evidence.py <record.md> [--check]
     python3 -S tools/evidence.py --selftest

Il record contiene un blocco fra `<!-- evidence:begin -->` e `<!-- evidence:end -->`; la sua sorgente è il file
accanto al record, con lo stesso nome e suffisso `.evidence` (righe `file <percorso>`, `junit <nome> <percorso>`,
`trace <stadio> <mappa> <directory-build> <junit>`; `#` commenti). Il blocco riporta gli SHA-256 dei file, i conteggi
dei junit (tools/status.py) e l'esito di `trace.py check`.
Senza --check riscrive il blocco; con --check non scrive e ritorna 1 se il blocco non coincide con lo stato attuale.
Uscita: 0 coincide o riscritto, 1 --check con blocco non aggiornato, 2 errore dello strumento.
"""
import hashlib
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(ROOT, "tools"))
import status  # noqa: E402

BEGIN = "<!-- evidence:begin -->"
END = "<!-- evidence:end -->"


class ToolError(Exception):
    pass


def path_of(p):
    return p if os.path.isabs(p) else os.path.join(ROOT, p)


def read(path):
    try:
        with open(path, encoding="utf-8") as fh:
            return fh.read()
    except (OSError, UnicodeDecodeError) as exc:
        raise ToolError(f"lettura fallita {path}: {exc}")


def sha256(path):
    h = hashlib.sha256()
    try:
        with open(path_of(path), "rb") as fh:
            for chunk in iter(lambda: fh.read(1 << 20), b""):
                h.update(chunk)
    except OSError as exc:
        raise ToolError(f"SHA-256 non calcolabile per {path}: {exc}")
    return h.hexdigest()


def parse_spec(text):
    files, junits, traces = [], [], []
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        f = line.split()
        if f[0] == "file" and len(f) == 2:
            files.append(f[1])
        elif f[0] == "junit" and len(f) == 3:
            junits.append((f[1], f[2]))
        elif f[0] == "trace" and len(f) == 5:
            traces.append(tuple(f[1:]))
        else:
            raise ToolError(f"riga {n} del file .evidence non valida: {raw}")
    if not files and not junits and not traces:
        raise ToolError("file .evidence vuoto")
    return files, junits, traces


def run_trace(stage, tmap, build, junit):
    cmd = [sys.executable, "-S", os.path.join(ROOT, "tools", "trace.py"), "check", "--stage", stage,
           "--map", path_of(tmap), "--build-dir", path_of(build), "--ctest-junit", path_of(junit)]
    try:
        res = subprocess.run(cmd, capture_output=True, text=True)
    except OSError as exc:
        raise ToolError(f"trace.py non avviabile: {exc}")
    summary = [ln for ln in res.stdout.splitlines() if ln.startswith("trace: ") and " ID nello stadio " in ln]
    if not summary:
        raise ToolError(f"trace.py senza riepilogo (uscita {res.returncode}): {res.stdout[-300:]} {res.stderr[-300:]}")
    return summary[-1]


def build_block(spec_text):
    files, junits, traces = parse_spec(spec_text)
    lines = [BEGIN, "Blocco generato da `tools/evidence.py`; non modificare a mano.", ""]
    if files:
        lines += ["| File | SHA-256 |", "|---|---|"]
        for f in sorted(files):
            lines.append(f"| `{f}` | `{sha256(f)}` |")
        lines.append("")
    for name, path in junits:
        total, fast, slow, failed = status.counts(path)
        lines.append(f"- Test {name} (`{path}`): totali {total}, fast {fast}, slow {slow}, falliti {len(failed)}")
    for stage, tmap, build, junit in traces:
        lines.append(f"- Tracciabilità (`{tmap}`, build `{build}`, junit `{junit}`): {run_trace(stage, tmap, build, junit)}")
    lines.append(END)
    return "\n".join(lines)


def split_record(text):
    if text.count(BEGIN) != 1 or text.count(END) != 1 or text.index(BEGIN) > text.index(END):
        raise ToolError("il record deve contenere esattamente una coppia di marcatori evidence:begin/evidence:end")
    i = text.index(BEGIN)
    j = text.index(END) + len(END)
    return text[:i], text[i:j], text[j:]


def process(record, check):
    text = read(record)
    spec = read(os.path.splitext(record)[0] + ".evidence")
    before, current, after = split_record(text)
    fresh = build_block(spec)
    if current == fresh:
        return 0
    if check:
        print(f"evidence: il blocco di {record} non coincide con lo stato attuale", file=sys.stderr)
        return 1
    try:
        with open(record, "w", encoding="utf-8") as fh:
            fh.write(before + fresh + after)
    except OSError as exc:
        raise ToolError(f"scrittura fallita {record}: {exc}")
    print(f"evidence: blocco di {record} rigenerato")
    return 0


def selftest():
    tmp = os.path.join(ROOT, "tmp", "evidence-selftest")
    os.makedirs(tmp, exist_ok=True)
    target = os.path.join(tmp, "target.txt")
    junit = os.path.join(tmp, "junit.xml")
    record = os.path.join(tmp, "rec.md")
    with open(target, "w", encoding="utf-8") as fh:
        fh.write("uno\n")
    with open(junit, "w", encoding="utf-8") as fh:
        fh.write('<testsuite><testcase name="a" status="run"><properties>'
                 '<property name="cmake_labels" value="fast"/></properties></testcase></testsuite>')
    with open(os.path.splitext(record)[0] + ".evidence", "w", encoding="utf-8") as fh:
        fh.write(f"file {target}\njunit prova {junit}\n")
    with open(record, "w", encoding="utf-8") as fh:
        fh.write(f"testa\n{BEGIN}\nvecchio\n{END}\ncoda\n")

    def expect(label, got, want):
        if got != want:
            raise ToolError(f"selftest fallito: {label}: uscita {got}, attesa {want}")

    expect("blocco vecchio rilevato", process(record, True), 1)
    expect("rigenerazione", process(record, False), 0)
    expect("blocco aggiornato", process(record, True), 0)
    with open(target, "w", encoding="utf-8") as fh:
        fh.write("due\n")
    expect("file modificato rilevato", process(record, True), 1)
    process(record, False)
    with open(junit, "w", encoding="utf-8") as fh:
        fh.write('<testsuite><testcase name="a" status="run"><properties>'
                 '<property name="cmake_labels" value="slow"/></properties></testcase></testsuite>')
    expect("conteggio modificato rilevato", process(record, True), 1)
    text = read(record)
    if not text.startswith("testa\n") or not text.endswith("coda\n"):
        raise ToolError("selftest fallito: il testo fuori dal blocco è stato alterato")
    print("evidence selftest: ok")
    return 0


def main(argv):
    if argv == ["--selftest"]:
        return selftest()
    check = "--check" in argv
    rest = [a for a in argv if a != "--check"]
    if len(rest) != 1:
        raise ToolError("uso: evidence.py <record.md> [--check] | --selftest")
    return process(rest[0], check)


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except ToolError as exc:
        sys.stderr.write(f"evidence: {exc}\n")
        sys.exit(2)
    except SystemExit as exc:
        if isinstance(exc.code, str):
            sys.stderr.write(f"evidence: {exc.code}\n")
            sys.exit(2)
        raise
    except Exception as exc:  # diagnosi esplicita, niente report di crash
        sys.stderr.write(f"evidence: errore interno {type(exc).__name__}: {exc}\n")
        sys.exit(2)
