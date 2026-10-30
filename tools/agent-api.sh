#!/usr/bin/env bash
# Agente Claude headless pagato con la chiave API del progetto (.secrets/anthropic-api-key, fuori da git), non con
# l'abbonamento. Solo Haiku e Sonnet: Opus e il coordinamento restano sull'abbonamento.
# Uso: tools/agent-api.sh <haiku|sonnet> <directory-di-lavoro> <file-prompt> <file-esito.json> [budget-usd, default 10]
# La directory di lavoro sta dentro il progetto (di norma un worktree); il prompt arriva da file su stdin.
# Uscita: 0 esito scritto e senza errore; 2 argomenti o chiave non validi; 4 credito API esaurito (il coordinatore
# rilancia il compito sull'abbonamento e lo registra); 5 errore riportato da claude (dettaglio nell'esito).
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
key="$root/.secrets/anthropic-api-key"

if [ "$#" -lt 4 ] || [ "$#" -gt 5 ]; then
  echo "uso: tools/agent-api.sh <haiku|sonnet> <directory-di-lavoro> <file-prompt> <file-esito.json> [budget-usd]" >&2
  exit 2
fi
model="$1"
workdir="$(cd "$2" && pwd)"
prompt="$(cd "$(dirname "$3")" && pwd)/$(basename "$3")"
out="$4"
budget="${5:-10}"

case "$model" in haiku|sonnet) ;; *) echo "modello non ammesso sulle API: '$model' (solo haiku o sonnet)" >&2; exit 2 ;; esac
case "$workdir/" in "$root"/*) ;; *) echo "directory di lavoro fuori dal progetto: $workdir" >&2; exit 2 ;; esac
[ -f "$prompt" ] || { echo "file di prompt assente: $prompt" >&2; exit 2; }
[ -f "$key" ] || { echo "chiave API assente: $key" >&2; exit 2; }
[ "$(stat -c %a "$key")" = 600 ] || { echo "permessi della chiave diversi da 600: $key" >&2; exit 2; }
[[ "$budget" =~ ^[0-9]+([.][0-9]+)?$ ]] || { echo "budget non valido: '$budget'" >&2; exit 2; }
mkdir -p "$(dirname "$out")"

rc=0
(cd "$workdir" && ANTHROPIC_API_KEY="$(cat "$key")" TMPDIR="$root/tmp" timeout 7200 claude -p --model "$model" \
  --permission-mode auto --max-budget-usd "$budget" --output-format json < "$prompt") > "$out" 2> "$out.stderr" || rc=$?

verdict="$(python3 -S - "$out" <<'EOF'
import json, sys
try:
    with open(sys.argv[1], encoding="utf-8") as f:
        d = json.load(f)
except (OSError, ValueError) as e:
    sys.exit(f"esito non leggibile: {e}")
text = str(d.get("result", ""))
if d.get("is_error"):
    print("credito" if "credit balance" in text.lower() else "errore")
else:
    print("ok")
print(f"costo_usd={d.get('total_cost_usd')}", file=sys.stderr)
EOF
)" || { echo "claude è uscito con $rc senza un esito JSON valido: $out, $out.stderr" >&2; exit 5; }

case "$verdict" in
  ok) [ "$rc" -eq 0 ] || { echo "claude è uscito con $rc: $out.stderr" >&2; exit 5; } ;;
  credito) echo "credito API esaurito: rilanciare il compito sull'abbonamento" >&2; exit 4 ;;
  *) echo "claude ha riportato un errore: $out" >&2; exit 5 ;;
esac
