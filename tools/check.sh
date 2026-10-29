#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

export TMPDIR="$root/tmp"
export CCACHE_DIR="$root/tmp/cache/ccache"
mkdir -p "$TMPDIR" "$CCACHE_DIR"

source "$root/tools/cpu_cap.sh"

cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-junit "$root/tmp/dev-junit.xml"

# Ogni test deve essere `fast` o `slow`: un test senza etichetta non girerebbe mai nel ciclo dei preset.
total=$(ctest --test-dir build/dev -N | sed -n 's/^Total Tests: //p')
fast=$(ctest --preset dev -N | sed -n 's/^Total Tests: //p')
slow=$(ctest --test-dir build/dev -L slow -N | sed -n 's/^Total Tests: //p')
if [ "$total" -ne "$((fast + slow))" ]; then
  echo "test senza etichetta fast/slow: totale $total, fast $fast, slow $slow" >&2
  exit 1
fi

tools/lint.sh

# Tracciabilità ID del piano → test (stadio M2): informativa; TRACE_STRICT=1 la rende bloccante.
# Il junit del preset dev contiene solo i test `fast`: i `slow` risultano mancanti finché non si usa il junit release.
trace_status=0
python3 -S tools/trace.py check --stage M2 --map tests/trace/M2.tsv --build-dir "$root/build/dev" \
  --ctest-junit "$root/tmp/dev-junit.xml" \
  > "$root/tmp/trace-check.txt" || trace_status=$?
if ! grep -q '^trace: [0-9]* ID nello stadio' "$root/tmp/trace-check.txt"; then
  echo "trace: errore dello strumento (nessun riepilogo), non una copertura mancante" >&2
  exit 1
fi
tail -n 1 "$root/tmp/trace-check.txt"
if [ "$trace_status" -ne 0 ]; then
  echo "trace: ID non coperti (elenco in tmp/trace-check.txt)" >&2
  if [ "${TRACE_STRICT:-0}" = "1" ]; then
    exit 1
  fi
fi

# Record di evidenza con blocco generato: informativo; EVIDENCE_STRICT=1 lo rende bloccante.
# Il blocco di M2-R0 legge anche tmp/release-junit.xml (`ctest --preset release --output-junit tmp/release-junit.xml`).
shopt -s nullglob
for spec in "$root"/docs/evidence/*.evidence; do
  if [ ! -d "$root/build/release" ] || [ ! -f "$root/tmp/dev-junit.xml" ] || [ ! -f "$root/tmp/release-junit.xml" ]; then
    if [ "${EVIDENCE_STRICT:-0}" = "1" ]; then
      echo "evidence: verifica impossibile (manca la build release o un junit) con EVIDENCE_STRICT=1" >&2
      exit 1
    fi
    echo "evidence: non verificata (manca la build release o un junit)"
    break
  fi
  evidence_status=0
  python3 -S tools/evidence.py "${spec%.evidence}.md" --check || evidence_status=$?
  if [ "$evidence_status" -ne 0 ]; then
    echo "evidence: ${spec%.evidence}.md non aggiornato o non verificabile (uscita $evidence_status)" >&2
    if [ "${EVIDENCE_STRICT:-0}" = "1" ]; then
      exit 1
    fi
  fi
done
