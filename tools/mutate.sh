#!/usr/bin/env bash
# Prova di mutazione: applica una patch, ricompila il preset release (incrementale: solo i target toccati dal file),
# esegue i test indicati e si aspetta FALLIMENTO. Il file è sempre ripristinato e verificato con cmp; dopo il ripristino
# il file ha un timestamp nuovo e la ricompilazione è obbligatoria e verificata, così nessun oggetto mutato sopravvive.
# RILEVATA solo se il junit di ctest mostra almeno un test eseguito e fallito; test non eseguiti o junit assente = errore.
# Uso: tools/mutate.sh <file> <patch> <ctest-regex> <log>
# Uscita: 0 mutazione rilevata (test falliti), 1 mutazione sopravvissuta (test passati),
#         2 errore dello strumento (patch non applicabile, compilazione fallita, nessun test, ripristino).
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

if [ "$#" -ne 4 ]; then
  echo "uso: tools/mutate.sh <file> <patch> <ctest-regex> <log>" >&2
  exit 2
fi
file="$1"; patch_file="$2"; regex="$3"; log="$4"
[ -f "$file" ] || { echo "file assente: $file" >&2; exit 2; }
[ -f "$patch_file" ] || { echo "patch assente: $patch_file" >&2; exit 2; }
command -v patch >/dev/null || { echo "patch non trovato nel PATH" >&2; exit 2; }

export TMPDIR="$root/tmp"
export CCACHE_DIR="$root/tmp/cache/ccache"
mkdir -p "$TMPDIR" "$CCACHE_DIR" "$(dirname "$log")"
source "$root/tools/cpu_cap.sh"

backup="$(mktemp "$root/tmp/mutate-backup.XXXXXX")"
cp -- "$file" "$backup"
restored=0
restore() {
  if [ "$restored" -eq 0 ]; then
    restored=1
    cp -- "$backup" "$file"
    touch -- "$file"
    if cmp -s -- "$backup" "$file"; then
      echo "ripristino verificato con cmp: $file" | tee -a "$log"
      rm -f -- "$backup"
      if ! cmake --build --preset release >> "$log" 2>&1; then
        echo "RICOMPILAZIONE DOPO IL RIPRISTINO FALLITA: vedi $log" | tee -a "$log" >&2
        exit 2
      fi
      echo "ricompilazione dopo il ripristino riuscita" | tee -a "$log"
    else
      echo "RIPRISTINO FALLITO: $file differisce dall'originale salvato in $backup" | tee -a "$log" >&2
      exit 2
    fi
  fi
}
trap restore EXIT

{
  echo "== mutazione $(date -Iseconds)"
  echo "file: $file"
  echo "patch: $patch_file"
  echo "test: $regex"
} > "$log"

# Baseline: la stessa selezione, nello stesso preset e ambiente, sul sorgente originale deve passare tutta.
cmake --preset release >> "$log" 2>&1 || { echo "configurazione release fallita: vedi $log" | tee -a "$log" >&2; exit 2; }
if ! cmake --build --preset release >> "$log" 2>&1; then
  echo "compilazione del sorgente originale fallita: vedi $log" | tee -a "$log" >&2
  exit 2
fi
count="$(ctest --preset release -N -R "$regex" | sed -n 's/^Total Tests: //p')"
if [ -z "$count" ] || [ "$count" -eq 0 ]; then
  echo "nessun test corrisponde a '$regex'" | tee -a "$log" >&2
  exit 2
fi
if ! ctest --preset release -R "$regex" >> "$log" 2>&1; then
  echo "baseline non verde: i $count test selezionati falliscono già sul sorgente originale (vedi $log)" | tee -a "$log" >&2
  exit 2
fi
echo "baseline verde: $count test" >> "$log"

if ! patch --no-backup-if-mismatch "$file" < "$patch_file" >> "$log" 2>&1; then
  echo "patch non applicabile: vedi $log" | tee -a "$log" >&2
  exit 2
fi

cmake --preset release >> "$log" 2>&1 || { echo "configurazione release fallita: vedi $log" | tee -a "$log" >&2; exit 2; }
if ! cmake --build --preset release >> "$log" 2>&1; then
  echo "la mutazione non compila: non è una prova valida (vedi $log)" | tee -a "$log" >&2
  exit 2
fi

count="$(ctest --preset release -N -R "$regex" | sed -n 's/^Total Tests: //p')"
if [ -z "$count" ] || [ "$count" -eq 0 ]; then
  echo "nessun test corrisponde a '$regex'" | tee -a "$log" >&2
  exit 2
fi

junit="$root/tmp/mutate-junit.$$.xml"
rm -f -- "$junit"
status=0
ctest --preset release -R "$regex" --output-junit "$junit" >> "$log" 2>&1 || status=$?
[ -s "$junit" ] || { echo "junit di ctest assente: errore dell'infrastruttura (uscita $status)" | tee -a "$log" >&2; exit 2; }
verdict=0
python3 -S - "$junit" "$count" >> "$log" <<'PY' || verdict=$?
import sys
import xml.etree.ElementTree as ET
try:
    root = ET.parse(sys.argv[1]).getroot()
except (OSError, ET.ParseError) as e:
    print("INFRASTRUTTURA: junit illeggibile: %s" % e)
    sys.exit(2)
expected = int(sys.argv[2])
cases = list(root.iter("testcase"))
notrun = [c.get("name") for c in cases if c.get("status") == "notrun"]
failed = [c for c in cases if c.get("status") == "fail" or c.find("failure") is not None or c.find("error") is not None]
print("junit: %d casi, %d falliti, %d non eseguiti" % (len(cases), len(failed), len(notrun)))
if len(cases) != expected or notrun:
    print("INFRASTRUTTURA: casi %d attesi %d, non eseguiti %s" % (len(cases), expected, notrun))
    sys.exit(2)
detected = 0
for c in failed:
    out = "".join((c.findtext("system-out") or "", c.findtext("system-err") or ""))
    if "CHECK-RESULT: tool-error" in out:
        print("INFRASTRUTTURA: %s ha segnalato un errore dello strumento (CHECK-RESULT: tool-error)" % c.get("name"))
        sys.exit(2)
    if "CHECK-RESULT: defect" in out or "[  FAILED  ]" in out:
        detected += 1
    else:
        print("INFRASTRUTTURA: %s fallito senza marcatore CHECK-RESULT né fallimento gtest" % c.get("name"))
        sys.exit(2)
sys.exit(0 if detected else 1)
PY
rm -f -- "$junit"
if [ "$verdict" -eq 0 ]; then
  echo "RILEVATA: la mutazione fa fallire almeno un test fra i $count selezionati (ctest uscita $status)" | tee -a "$log"
  exit 0
elif [ "$verdict" -eq 1 ]; then
  echo "SOPRAVVISSUTA: i $count test selezionati passano con la mutazione" | tee -a "$log" >&2
  exit 1
fi
echo "errore dell'infrastruttura nell'esecuzione dei test: vedi $log" | tee -a "$log" >&2
exit 2
