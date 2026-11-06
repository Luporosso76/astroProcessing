#!/usr/bin/env bash
# Delega headless con il modello scelto dalla tabella, mai a mano:
#   tools/delegate.sh <unità|-> <ruolo> <worktree> <prompt.md> <uscita.json>
# Ruoli: tests, code, arb-tests, arb-code (modello dell'unità in tools/model-routing.tsv);
# mech = haiku, infra = sonnet, review = opus (unità «-»). Unità o ruolo sconosciuti: uscita 2, nessun lancio.
set -euo pipefail
[ "$#" -eq 5 ] || { echo "uso: tools/delegate.sh <unità|-> <ruolo> <worktree> <prompt.md> <uscita.json>" >&2; exit 2; }
unit="$1"; role="$2"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
table="$root/tools/model-routing.tsv"
case "$role" in
  mech) model=haiku ;;
  infra) model=sonnet ;;
  review) model=opus ;;
  tests|arb-tests|code|arb-code)
    col=2; case "$role" in code|arb-code) col=3 ;; esac
    model="$(awk -F'\t' -v u="$unit" -v c="$col" '$1 == u { print $c }' "$table")"
    [ -n "$model" ] || { echo "delegate: unità '$unit' assente da tools/model-routing.tsv" >&2; exit 2; } ;;
  *) echo "delegate: ruolo '$role' sconosciuto" >&2; exit 2 ;;
esac
echo "delegate: $unit $role -> $model" >&2
ro=0; [ "$role" = review ] && ro=1
ASTRO_API_READONLY="$ro" exec "$root/tools/agent-api.sh" "$model" "$3" "$4" "$5"
