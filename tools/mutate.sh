#!/usr/bin/env bash
# Prova di mutazione: applica una patch, ricompila il preset release (incrementale: solo i target toccati dal file),
# esegue i test indicati e si aspetta FALLIMENTO. Il file è sempre ripristinato e verificato con cmp; dopo il ripristino
# il file ha un timestamp nuovo e la ricompilazione è obbligatoria e verificata, così nessun oggetto mutato sopravvive.
# RILEVATA solo se il junit di ctest mostra almeno un test eseguito e fallito; test non eseguiti o junit assente = errore.
# Uso: tools/mutate.sh <file> <patch> <ctest-regex> <log>   |   tools/mutate.sh --selftest
# Il junit è classificato da tools/mutate_classify.py; un output troncato da ctest è errore, mai rilevamento.
# Uscita: 0 mutazione rilevata (test falliti), 1 mutazione sopravvissuta (test passati),
#         2 errore dello strumento (patch non applicabile, compilazione fallita, nessun test, ripristino).
# --selftest: 0 ok; 2 prova fallita (come ogni errore dello strumento: 1 qui non esiste), cartella di lavoro
#         conservata in tmp/ per diagnosi. Richiamato da tools/check.sh --selftest.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
export TMPDIR="$root/tmp"
mkdir -p "$TMPDIR"
# Limiti d'output per test nel junit. Default di ctest: 307200 byte (falliti), 1024 (passati); oltre, ctest scarta la
# coda dell'output e il marcatore gtest finale sparisce. 64 MiB: ben oltre qualunque output di un test del progetto;
# se è superato lo stesso, mutate_classify.py riconosce il troncamento e dà errore.
ctest_limit_args=(--test-output-size-failed 67108864 --test-output-size-passed 67108864)

# classify <junit> <attesi> <script>: stampa l'esito del classificatore e ne ritorna l'uscita. Le uscite 0 e 1 valgono solo
# se l'output contiene la riga «junit:» che il classificatore stampa dopo l'avvio: un avvio fallito (import, interprete) esce
# con 1 senza quella riga e non deve diventare «mutazione sopravvissuta».
classify() {
  local out crc=0
  out="$(python3 -S "$3" "$1" "$2" 2>&1)" || crc=$?
  printf '%s\n' "$out"
  if [ "$crc" -le 1 ] && ! grep -q '^junit: ' <<< "$out"; then
    echo "INFRASTRUTTURA: il classificatore è uscito con $crc senza la riga 'junit:' (avvio fallito)"
    return 2
  fi
  return "$crc"
}

if [ "${1:-}" = "--selftest" ]; then
  [ "$#" -eq 1 ] || { echo "uso: tools/mutate.sh --selftest (senza altri argomenti)" >&2; exit 2; }
  d="$(mktemp -d "$root/tmp/mutate-selftest.XXXXXX")"
  ok=0
  trap 'if [ "$ok" -eq 1 ]; then rm -rf -- "$d" || { echo "rimozione della cartella del selftest fallita: $d" >&2; exit 2; }; else echo "cartella del selftest conservata per diagnosi: $d" >&2; fi' EXIT
  mkdir -p "$d/proj"
  cat > "$d/proj/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.20)
project(mutate_selftest NONE)
enable_testing()
add_test(NAME long_fail COMMAND sh -c "i=0; while [ $i -lt 20000 ]; do echo riga-di-riempimento-$i; i=$((i+1)); done; echo '[  FAILED  ] Suite.Caso'; exit 1")
add_test(NAME early_fail COMMAND sh -c "echo '[  FAILED  ] Suite.Caso'; i=0; while [ $i -lt 20000 ]; do echo riga-di-riempimento-$i; i=$((i+1)); done; exit 1")
add_test(NAME plain_fail COMMAND sh -c "echo 'timeout simulato senza marcatore'; exit 1")
add_test(NAME pass COMMAND sh -c "echo ok")
add_test(NAME tool_error COMMAND sh -c "echo 'CHECK-RESULT: tool-error'; exit 1")
add_test(NAME defect COMMAND sh -c "echo 'CHECK-RESULT: defect'; exit 1")
EOF
  st_fail() { echo "mutate selftest fallito: $1" >&2; [ ! -f "$d/out.txt" ] || cat "$d/out.txt" >&2; exit 2; }
  (cd "$d/proj" && cmake -S . -B ../b >/dev/null) || st_fail "configurazione del progetto di prova"
  run() { # <regex> <n> <default|limit> [script]: ctest nel progetto di prova e classificazione del junit
    rm -f -- "$d/j.xml" "$d/out.txt"
    local cr=0
    if [ "$3" = default ]; then
      (cd "$d/b" && ctest -R "$1" --output-junit "$d/j.xml" >/dev/null 2>&1) || cr=$?
    else
      (cd "$d/b" && ctest -R "$1" --output-junit "$d/j.xml" "${ctest_limit_args[@]}" >/dev/null 2>&1) || cr=$?
    fi
    [ -s "$d/j.xml" ] || st_fail "junit assente per '$1' (ctest uscita $cr)"
    rc=0; classify "$d/j.xml" "$2" "${4:-$root/tools/mutate_classify.py}" >"$d/out.txt" 2>&1 || rc=$?
  }
  expect() { # <uscita> <descrizione> [testo atteso nella diagnosi]
    [ "$rc" -eq "$1" ] || st_fail "$2 (uscita $rc, atteso $1)"
    [ -z "${3:-}" ] || grep -q -- "$3" "$d/out.txt" || st_fail "$2 (diagnosi senza «$3»)"
  }
  run '^long_fail$' 1 default;   expect 2 "output troncato (marcatore in coda) col default di ctest" "troncato"
  run '^early_fail$' 1 default;  expect 2 "output troncato (marcatore in testa, quindi sopravvive) col default di ctest" "troncato"
  run '^long_fail$' 1 limit;     expect 0 "fallimento gtest con output lungo deve essere rilevato"
  run '^early_fail$' 1 limit;    expect 0 "fallimento gtest con marcatore in testa deve essere rilevato"
  run '^plain_fail$' 1 limit;    expect 2 "fallimento senza marcatore resta errore d'infrastruttura" "senza marcatore"
  run '^pass$' 1 limit;          expect 1 "test passato resta non rilevato"
  run '^(pass|long_fail)$' 2 limit; expect 0 "un solo fallito su due basta a rilevare"
  run '^(pass|plain_fail)$' 2 limit; expect 2 "nessun marcatore su un fallito su due resta errore" "senza marcatore"
  run '^(long_fail|plain_fail)$' 2 limit; expect 2 "un fallito marcato e uno senza marcatore resta errore" "senza marcatore"
  run '^tool_error$' 1 limit;    expect 2 "tool-error è errore dello strumento" "tool-error"
  run '^defect$' 1 limit;        expect 0 "CHECK-RESULT: defect è rilevato"
  printf 'import modulo_inesistente_per_il_selftest\n' > "$d/stub.py"
  run '^pass$' 1 limit "$d/stub.py"; expect 2 "avvio fallito del classificatore non è sopravvivenza" "avvio fallito"
  ok=1
  echo "mutate selftest: ok"
  exit 0
fi

if [ "$#" -ne 4 ]; then
  echo "uso: tools/mutate.sh <file> <patch> <ctest-regex> <log>" >&2
  exit 2
fi
file="$1"; patch_file="$2"; regex="$3"; log="$4"
[ -f "$file" ] || { echo "file assente: $file" >&2; exit 2; }
[ -f "$patch_file" ] || { echo "patch assente: $patch_file" >&2; exit 2; }
command -v patch >/dev/null || { echo "patch non trovato nel PATH" >&2; exit 2; }

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
ctest --preset release -R "$regex" --output-junit "$junit" "${ctest_limit_args[@]}" >> "$log" 2>&1 || status=$?
[ -s "$junit" ] || { echo "junit di ctest assente: errore dell'infrastruttura (uscita $status)" | tee -a "$log" >&2; exit 2; }
verdict=0
classify "$junit" "$count" "$root/tools/mutate_classify.py" >> "$log" || verdict=$?
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
