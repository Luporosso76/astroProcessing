#!/usr/bin/env bash
# Review indipendente con Codex. Uso: tools/review.sh <slug> <file-fatti.md>
# Il prompt è il template fisso più il file di fatti dell'unità (ID, contratto, log): niente giudizi dell'autore.
# Il log di Codex va in tmp/review-<slug>.log; a video solo il percorso dell'artefatto e il verdetto.
# Verdetto: l'ultima riga non vuota dell'artefatto inizia con "Verdetto: PASS|FAIL"; tutte le occorrenze di
# "Verdetto: PASS|FAIL" nell'artefatto (anche a metà riga, per le code ripetute da Codex) devono concordare.
# Un PASS con rilievi [BLOCCANTE] o [MED] è incoerente (uscita 2).
# Uso: tools/review.sh --selftest   prova il giudizio su artefatti sintetici e sulla fixture tests/fixtures/review/.
# Criterio: FAIL se c'è almeno un rilievo BLOCCANTE o MED, PASS solo con LOW o senza rilievi; un verdetto che
# contraddice i rilievi (PASS con BLOCCANTE/MED, FAIL con soli LOW) è incoerente (uscita 2).
# Uscita: 0 PASS, 1 FAIL, 2 errore di Codex, artefatto incoerente o argomenti non validi.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

count_lines() {
  local n rc=0
  n="$(grep -cE -- "$2" "$1")" || rc=$?
  if [ "$rc" -ge 2 ]; then echo "grep fallito su $1 (uscita $rc)" >&2; exit 2; fi
  printf '%s\n' "$n"
}

# judge <artefatto>: stampa PASS o FAIL e ritorna 0; ritorna 2 con diagnosi se l'artefatto è incoerente.
judge() {
  local f="$1" last values blocking rc=0
  last="$(awk 'NF {l=$0} END {print l}' "$f")"
  if ! printf '%s\n' "$last" | grep -qE '^Verdetto: (PASS|FAIL)'; then
    echo "l'ultima riga non vuota deve iniziare con 'Verdetto: PASS|FAIL': $f" >&2
    return 2
  fi
  values="$(grep -oE 'Verdetto: (PASS|FAIL)' "$f" | sort -u)" || rc=$?
  if [ "$rc" -ge 2 ]; then echo "grep fallito su $f (uscita $rc)" >&2; return 2; fi
  if [ "$(printf '%s\n' "$values" | wc -l)" -ne 1 ]; then
    echo "verdetti discordanti nell'artefatto $f: $(printf '%s' "$values" | tr '\n' ' ')" >&2
    return 2
  fi
  blocking="$(count_lines "$f" '^\[(BLOCCANTE|MED)\]')"
  if [ "$values" = "Verdetto: PASS" ] && [ "$blocking" -ne 0 ]; then
    echo "PASS incoerente: $blocking rilievi BLOCCANTE/MED presenti nell'artefatto $f" >&2
    return 2
  fi
  if [ "$values" = "Verdetto: FAIL" ] && [ "$blocking" -eq 0 ]; then
    echo "FAIL incoerente: nessun rilievo BLOCCANTE/MED nell'artefatto $f" >&2
    return 2
  fi
  printf '%s\n' "${values#Verdetto: }"
}

verdict_exit() {
  local v rc=0
  v="$(judge "$1")" || rc=$?
  [ "$rc" -eq 0 ] || return 2
  echo "Verdetto: $v"
  [ "$v" = "PASS" ]
}

if [ "${1:-}" = "--selftest" ]; then
  d="$root/tmp/review-selftest"
  rm -rf "$d"; mkdir -p "$d"
  expect() { # etichetta uscita-attesa file
    local rc=0
    verdict_exit "$3" >/dev/null 2>&1 || rc=$?
    [ "$rc" -eq "$2" ] || { echo "review selftest fallito: $1 (uscita $rc, attesa $2)" >&2; exit 1; }
  }
  printf '[LOW] a — b — c\nVerdetto: PASS\n' > "$d/pass.md";                 expect "PASS con soli LOW" 0 "$d/pass.md"
  printf '[MED] a — b — c\nVerdetto: PASS\n' > "$d/passmed.md";               expect "PASS con MED" 2 "$d/passmed.md"
  printf '[BLOCCANTE] a — b — c\nVerdetto: FAIL\n' > "$d/fail.md";            expect "FAIL" 1 "$d/fail.md"
  printf '[MED] a — b — c\nVerdetto: FAIL\n' > "$d/failmed.md";               expect "FAIL con MED" 1 "$d/failmed.md"
  printf '[LOW] a — b — c\nVerdetto: FAIL\n' > "$d/faillow.md";               expect "FAIL con soli LOW" 2 "$d/faillow.md"
  printf '[MED] x — y — z\nVerdetto: FAIL\n[LOW] x — y — z Verdetto: FAIL\nVerdetto: FAIL\n' > "$d/dup.md"; expect "coda ripetuta concorde" 1 "$d/dup.md"
  printf 'Verdetto: PASS\nVerdetto: FAIL\n' > "$d/discord.md";                expect "verdetti discordanti" 2 "$d/discord.md"
  printf 'Verdetto: FAIL\naltro\n' > "$d/notlast.md";                         expect "verdetto non in coda" 2 "$d/notlast.md"
  printf '[LOW] a — b — c\n' > "$d/none.md";                                  expect "nessun verdetto" 2 "$d/none.md"
  real="$root/tests/fixtures/review/codex-r0-review3.md"
  [ -f "$real" ] || { echo "artefatto reale assente: $real" >&2; exit 2; }
  cp -- "$real" "$d/real.md";                                                   expect "artefatto reale di Codex" 1 "$d/real.md"
  echo "review selftest: ok"
  exit 0
fi

if [ "$#" -ne 2 ]; then
  echo "uso: tools/review.sh <slug> <file-checklist-extra.md>" >&2
  exit 2
fi
slug="$1"
extra="$2"
if ! [[ "$slug" =~ ^[A-Za-z0-9._-]+$ ]]; then
  echo "slug non valido: '$slug' (ammessi lettere, cifre, . _ -)" >&2
  exit 2
fi
[ -f "$extra" ] || { echo "file extra assente: $extra" >&2; exit 2; }
[ -f tools/review-template.md ] || { echo "template assente: tools/review-template.md" >&2; exit 2; }
command -v codex >/dev/null || { echo "codex non trovato nel PATH" >&2; exit 2; }
# gpt-6-astra richiede codex-cli >= 0.164.0; una versione più vecchia nel PATH ferma la review.
codex_version="$(codex --version 2>/dev/null | sed -n 's/^codex-cli //p')"
if [ -z "$codex_version" ] || [ "$(printf '%s\n0.164.0\n' "$codex_version" | sort -V | head -1)" != "0.164.0" ]; then
  echo "codex-cli ${codex_version:-sconosciuta} in $(command -v codex): serve >= 0.164.0 per gpt-6-astra" >&2
  exit 2
fi

mkdir -p .omc/artifacts/ask tmp
log="tmp/review-${slug}.log"
out=".omc/artifacts/ask/codex-${slug}-$(date +%Y%m%d-%H%M).md"
prompt="$(cat tools/review-template.md "$extra")"

if ! TMPDIR="$root/tmp" timeout 3600 codex exec -m gpt-6-astra -c model_reasoning_effort="high" -s read-only \
  -o "$out" "$prompt" < /dev/null > "$log" 2>&1; then
  echo "codex ha fallito (uscita non nulla o timeout); artefatto: $out, log: $log" >&2
  exit 2
fi
[ -s "$out" ] || { echo "artefatto vuoto o assente: $out" >&2; exit 2; }
echo "artefatto: $out"
verdict_exit "$out"
