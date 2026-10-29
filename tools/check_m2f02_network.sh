#!/usr/bin/env bash
# T-M2F-02: il workflow M2.e completo (tests/unit/m2e_workflow) non esegue chiamate di rete.
# Uso: check_m2f02_network.sh <eseguibile m2e_workflow> <directory temporanea nel progetto>
# Prima si verifica che strace rilevi davvero una chiamata di rete (canarino), poi si traccia il workflow:
# il test passa solo se il canarino e' visto e il workflow non produce alcuna riga di rete.
set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "uso: $0 <m2e_workflow> <directory temporanea>" >&2
  exit 2
fi
workflow="$1"
tmp="$2"
strace_bin=/usr/bin/strace
[ -x "$strace_bin" ] || { echo "strace assente: $strace_bin" >&2; exit 2; }
[ -x "$workflow" ] || { echo "eseguibile assente: $workflow" >&2; exit 2; }

rm -rf "$tmp"
mkdir -p "$tmp"

# Canarino: tentativo di connessione su loopback, porta 1 (chiusa); serve solo a provare che strace vede la rete.
"$strace_bin" -f -e trace=network -o "$tmp/canary.log" bash -c 'exec 3<>/dev/tcp/127.0.0.1/1' 2>/dev/null || true
if ! grep -qE '^[0-9]+ +(socket|connect)\(' "$tmp/canary.log"; then
  echo "strace non ha rilevato il canarino di rete: il controllo sarebbe vuoto" >&2
  exit 1
fi

# LeakSanitizer non funziona sotto ptrace: la rilevazione dei leak resta nei test non tracciati dello stesso workflow.
ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0" "$strace_bin" -f -e trace=network -o "$tmp/workflow.strace" \
  "$workflow" "$tmp/work"

if [ ! -s "$tmp/work/store/project.db" ]; then
  echo "il workflow non ha prodotto lo store: traccia non valida" >&2
  exit 1
fi
if grep -E '^[0-9]+ +[a-z_0-9]+\(' "$tmp/workflow.strace" >/dev/null; then
  echo "chiamate di rete nel workflow M2.e:" >&2
  grep -E '^[0-9]+ +[a-z_0-9]+\(' "$tmp/workflow.strace" >&2
  exit 1
fi
echo "T-M2F-02: zero chiamate di rete nel workflow M2.e ($(grep -c . "$tmp/canary.log") righe nel canarino, 0 nel workflow)"
