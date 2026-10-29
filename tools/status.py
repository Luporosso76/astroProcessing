#!/usr/bin/env python3 -S
"""Conteggi dei test da `ctest --output-junit`, per i record di evidenza.

Uso: tools/status.py [preset=file.xml ...]
Senza argomenti: dev=tmp/dev-junit.xml release=tmp/release-junit.xml.
Le etichette `fast`/`slow` si leggono dalla proprietà `cmake_labels` di ogni testcase del junit.
"""
import os
import sys
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULTS = ["dev=tmp/dev-junit.xml", "release=tmp/release-junit.xml"]


def counts(path):
    """(totali, fast, slow, falliti[]) dal junit; errori di lettura o test senza etichetta fast/slow: sys.exit."""
    full = path if os.path.isabs(path) else os.path.join(ROOT, path)
    try:
        tree = ET.parse(full)
    except (OSError, ET.ParseError) as exc:
        sys.exit(f"status: junit non leggibile {full}: {exc}")
    total = fast = nslow = 0
    failed = []
    for tc in tree.getroot().iter("testcase"):
        name = tc.get("name")
        if not name:
            sys.exit(f"status: {full}: testcase senza nome")
        labels = set()
        for prop in tc.iter("property"):
            if prop.get("name") == "cmake_labels":
                labels.update((prop.get("value") or "").split(";"))
        if "slow" in labels:
            nslow += 1
        elif "fast" in labels:
            fast += 1
        else:
            sys.exit(f"status: {full}: il test {name} non ha l'etichetta fast né slow nel junit")
        total += 1
        bad = tc.get("status", "run") != "run" or any(
            tc.find(t) is not None for t in ("failure", "error", "skipped"))
        if bad:
            failed.append(name)
    if total == 0:
        sys.exit(f"status: nessun testcase in {full}")
    return total, fast, nslow, failed


def report(preset, path):
    total, fast, nslow, failed = counts(path)
    print(f"{preset}: totali {total}, fast {fast}, slow {nslow}, falliti {len(failed)}")
    for n in failed:
        print(f"  FALLITO {n}")
    return len(failed)


def main():
    specs = sys.argv[1:] or DEFAULTS
    failed = 0
    for spec in specs:
        preset, sep, path = spec.partition("=")
        if not sep or not preset or not path:
            sys.exit(f"status: argomento non valido '{spec}' (atteso preset=file.xml)")
        failed += report(preset, path)
    return 1 if failed else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:  # diagnosi esplicita, niente report di crash
        sys.exit(f"status: errore interno {type(exc).__name__}: {exc}")
