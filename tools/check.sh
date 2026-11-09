#!/usr/bin/env bash
# Controllo di consegna a passi. Senza argomenti esegue tutti i passi in ordine.
# Uso: tools/check.sh [--step <passo>] [--shard K/N]   (--shard solo con --step test)
#      tools/check.sh --list-steps | --selftest | --help
# Passi: build, test, labels, lint, trace, evidence. Il passo `test` a shard (K/N) tiene ogni comando sotto i 600 s;
# trace ed evidence uniscono gli shard in tmp/dev-junit.xml solo se completi e coerenti (tools/check_shards.py).
# Uscita: 0 verde, 1 controllo rosso, 2 uso errato.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

steps=(build test labels lint trace evidence)

usage_error() {
  echo "check: $1" >&2
  echo "passi: ${steps[*]}" >&2
  exit 2
}

step=""
shard=""
for a in "$@"; do
  case "$a" in
    --help|-h|--list-steps|--selftest) [ $# -eq 1 ] || usage_error "$a non si combina con altri argomenti" ;;
  esac
done
while [ $# -gt 0 ]; do
  case "$1" in
    --help|-h)
      echo "Uso: tools/check.sh [--step <passo>] [--shard K/N]; senza argomenti esegue tutti i passi in ordine."
      echo "Passi: ${steps[*]}. --shard K/N (solo con --step test): parte K di N della suite fast, junit tmp/dev-junit.K-of-N.xml."
      echo "trace ed evidence uniscono gli shard in tmp/dev-junit.xml se completi. Altro: --list-steps, --selftest."
      exit 0 ;;
    --list-steps) printf '%s\n' "${steps[@]}"; exit 0 ;;
    --selftest)
      mkdir -p tmp
      for bad in "--step nonesiste" "--step test --shard 0/3" "--step test --shard 4/3" "--step test --shard 1-3" \
                 "--step build --shard 1/2" "--shard 1/2" "--bogus" "--step build --step test" \
                 "--step nonesiste --step lint" "--step test --shard 1/2 --shard 2/2" "--step nonesiste --list-steps" \
                 "--list-steps --step build" "--help --selftest" "--selftest --bogus" "--step" "--step test --shard"; do
        status=0
        # shellcheck disable=SC2086
        out="$("$0" $bad 2>&1)" || status=$?
        [ "$status" -eq 2 ] || { echo "check selftest fallito: «$bad» doveva uscire con 2, uscita $status" >&2; exit 1; }
        printf '%s\n' "$out" | grep -q '^passi: build test labels lint trace evidence$' \
          || { echo "check selftest fallito: «$bad» senza elenco dei passi" >&2; exit 1; }
      done
      python3 -S tools/check_shards.py selftest
      echo "check selftest: ok"
      exit 0 ;;
    --step)
      [ $# -ge 2 ] || usage_error "--step richiede un passo"
      [ -z "$step" ] || usage_error "--step ripetuto"
      step="$2"; shift 2 ;;
    --shard)
      [ $# -ge 2 ] || usage_error "--shard richiede K/N"
      [ -z "$shard" ] || usage_error "--shard ripetuto"
      shard="$2"; shift 2 ;;
    *) usage_error "argomento sconosciuto: $1" ;;
  esac
done

if [ -n "$step" ]; then
  known=0
  for s in "${steps[@]}"; do [ "$s" = "$step" ] && known=1; done
  [ "$known" -eq 1 ] || usage_error "passo sconosciuto: $step"
fi
shard_k=""
shard_n=""
if [ -n "$shard" ]; then
  [ "$step" = "test" ] || usage_error "--shard vale solo con --step test"
  [[ "$shard" =~ ^([1-9][0-9]{0,2})/([1-9][0-9]{0,2})$ ]] || usage_error "--shard $shard non valido: atteso K/N con interi positivi"
  shard_k="${BASH_REMATCH[1]}"
  shard_n="${BASH_REMATCH[2]}"
  [ "$shard_k" -le "$shard_n" ] || usage_error "--shard $shard fuori intervallo: K deve stare fra 1 e N"
fi

export TMPDIR="$root/tmp"
export CCACHE_DIR="$root/tmp/cache/ccache"
mkdir -p "$TMPDIR" "$CCACHE_DIR"

source "$root/tools/cpu_cap.sh"

dev_junit="$root/tmp/dev-junit.xml"

step_build() {
  cmake --preset dev
  cmake --build --preset dev
}

# Senza shard: un solo run, junit tmp/dev-junit.xml, e gli shard residui spariscono. Con shard: il junit dello shard
# sostituisce il precedente e tmp/dev-junit.xml (completo o unito) sparisce, così non resta un junit di un altro run.
step_test() {
  if [ -z "$shard_k" ]; then
    rm -f "$dev_junit" "$root"/tmp/dev-junit.*-of-*.xml
    ctest --preset dev --no-tests=error --output-junit "$dev_junit"
  else
    rm -f "$dev_junit" "$root/tmp/dev-junit.$shard_k-of-$shard_n.xml"
    ctest --preset dev --no-tests=error -I "$shard_k,,$shard_n" --output-junit "$root/tmp/dev-junit.$shard_k-of-$shard_n.xml"
  fi
}

# Ogni test deve essere `fast` o `slow`: un test senza etichetta non girerebbe mai nel ciclo dei preset.
step_labels() {
  local total fast slow
  total=$(ctest --test-dir build/dev -N | sed -n 's/^Total Tests: //p')
  fast=$(ctest --preset dev -N | sed -n 's/^Total Tests: //p')
  slow=$(ctest --test-dir build/dev -L slow -N | sed -n 's/^Total Tests: //p')
  if [ "${total:-0}" -le 0 ] || [ "$total" -ne "$((fast + slow))" ]; then
    echo "test senza etichetta fast/slow: totale $total, fast $fast, slow $slow" >&2
    exit 1
  fi
  echo "labels: ok (totale $total, fast $fast, slow $slow)"
}

step_lint() {
  tools/lint.sh
}

# Se esistono shard li unisce (con gli elenchi `ctest -N -I K,,N`, perché ctest aggiunge a ogni shard le fixture di setup) in tmp/dev-junit.xml (errore se incompleti o incoerenti). Con "require" l'assenza di
# qualunque junit dev è un errore.
prepare_junit() {
  if compgen -G "$root/tmp/dev-junit.*-of-*.xml" >/dev/null; then
    local f base k n
    rm -f "$dev_junit" "$root"/tmp/dev-fast-list.*.txt
    ctest --preset dev -N > "$root/tmp/dev-fast-list.txt"
    for f in "$root"/tmp/dev-junit.*-of-*.xml; do
      base="$(basename "$f")"
      [[ "$base" =~ ^dev-junit\.([1-9][0-9]{0,2})-of-([1-9][0-9]{0,2})\.xml$ ]] || { echo "shard: nome non riconosciuto: $f" >&2; exit 1; }
      k="${BASH_REMATCH[1]}"
      n="${BASH_REMATCH[2]}"
      [ "$k" -le "$n" ] || { echo "shard: $f: K fuori intervallo 1..$n" >&2; exit 1; }
      ctest --preset dev -N -I "$k,,$n" > "$root/tmp/dev-fast-list.$k-of-$n.txt"
    done
    python3 -S tools/check_shards.py merge --listing "$root/tmp/dev-fast-list.txt" --dir "$root/tmp" --out "$dev_junit"
  elif [ "${1:-}" = "require" ] && [ ! -f "$dev_junit" ]; then
    echo "manca tmp/dev-junit.xml: eseguire prima tools/check.sh --step test (con o senza --shard)" >&2
    exit 1
  fi
}

# Tracciabilità ID del piano → test, per stadio (M2, M3.a, M3.b): informativa; TRACE_STRICT=1 la rende bloccante.
# Il junit del preset dev contiene solo i test `fast`: i `slow` risultano mancanti finché non si usa il junit release.
# Un'uscita dello strumento senza riepilogo è un errore dello strumento, distinto da una copertura mancante.
step_trace() {
  prepare_junit require
  local entry stage rest map out trace_status
  for entry in "M2:tests/trace/M2.tsv:trace-check.txt" "M3.a:tests/trace/M3a.tsv:trace-check-m3a.txt" "M3.b:tests/trace/M3b.tsv:trace-check-m3b.txt"; do
    stage="${entry%%:*}"
    rest="${entry#*:}"
    map="${rest%%:*}"
    out="$root/tmp/${rest#*:}"
    trace_status=0
    python3 -S tools/trace.py check --stage "$stage" --map "$map" --build-dir "$root/build/dev" \
      --ctest-junit "$dev_junit" \
      > "$out" || trace_status=$?
    if ! grep -q '^trace: [0-9]* ID nello stadio' "$out"; then
      echo "trace $stage: errore dello strumento (nessun riepilogo), non una copertura mancante" >&2
      exit 1
    fi
    tail -n 1 "$out"
    if [ "$trace_status" -ne 0 ]; then
      echo "trace $stage: ID non coperti (elenco in tmp/$(basename "$out"))" >&2
      if [ "${TRACE_STRICT:-0}" = "1" ]; then
        exit 1
      fi
    fi
  done
}

# Record di evidenza con blocco generato: informativo; EVIDENCE_STRICT=1 lo rende bloccante.
# Il blocco di M2-R0 legge anche tmp/release-junit.xml (`ctest --preset release --output-junit tmp/release-junit.xml`).
step_evidence() {
  prepare_junit
  local spec evidence_status
  shopt -s nullglob
  for spec in "$root"/docs/evidence/*.evidence; do
    if [ ! -d "$root/build/release" ] || [ ! -f "$dev_junit" ] || [ ! -f "$root/tmp/release-junit.xml" ]; then
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
}

if [ -n "$step" ]; then
  "step_$step"
else
  for s in "${steps[@]}"; do
    "step_$s"
  done
fi
