#!/usr/bin/env bash
# Review indipendente. Uso: tools/review.sh [--reviewer argon|claude] <slug> <file-fatti.md>
# Revisore (AGENTS.md, "Delega e review"): `argon` (default) è Gemini 4 Argon (High) via `agy` in modalità plan e sandbox;
# `claude` è Claude Opus headless sui crediti API del progetto (tools/agent-api.sh, indipendente dal login), contesto
# separato dall'autore; a credito finito (uscita 4 di agent-api.sh) la stessa review gira sull'abbonamento, e il log lo dice. Se Argon
# non risponde (assente da `agy models`, rifiuto, crediti finiti) lo strumento esce con 4 e la review si rilancia
# esplicitamente con `--reviewer claude`: nessun cambio automatico di revisore.
# Il prompt è il template fisso più il file di fatti dell'unità (ID, contratto, log): niente giudizi dell'autore.
# Il log del revisore va in tmp/review-<slug>-<revisore>.log; a video solo il percorso dell'artefatto e il verdetto.
# Il revisore legge e basta: se durante la review cambia lo stato di git (file tracciati o nuovi), uscita 2.
# Verdetto: l'ultima riga non vuota dell'artefatto inizia con "Verdetto: PASS|FAIL"; tutte le occorrenze di
# "Verdetto: PASS|FAIL" nell'artefatto (anche a metà riga, per le code ripetute dal revisore) devono concordare.
# Un PASS con rilievi [BLOCCANTE] o [MED] è incoerente (uscita 2).
# Uso: tools/review.sh --selftest   prova il giudizio su artefatti sintetici e sulla fixture tests/fixtures/review/.
# Criterio: FAIL se c'è almeno un rilievo BLOCCANTE o MED, PASS solo con LOW o senza rilievi; un verdetto che
# contraddice i rilievi (PASS con BLOCCANTE/MED, FAIL con soli LOW) è incoerente (uscita 2).
# Uscita: 0 PASS, 1 FAIL, 2 errore del revisore, artefatto incoerente o argomenti non validi, 3 limite di giri raggiunto,
# 4 Argon non disponibile (rilanciare con --reviewer claude).
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
  cp -- "$real" "$d/real.md";                                                   expect "artefatto reale di una review" 1 "$d/real.md"
  echo "review selftest: ok"
  exit 0
fi

reviewer=argon
if [ "${1:-}" = "--reviewer" ]; then
  reviewer="${2:-}"
  shift 2 || { echo "uso: tools/review.sh [--reviewer argon|claude] <slug> <file-fatti.md>" >&2; exit 2; }
fi
case "$reviewer" in argon|claude) ;; *) echo "revisore non ammesso: '$reviewer' (argon o claude)" >&2; exit 2 ;; esac
if [ "$#" -ne 2 ]; then
  echo "uso: tools/review.sh [--reviewer argon|claude] <slug> <file-fatti.md>" >&2
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
argon_model="gemini-4-argon-high"
if [ "$reviewer" = argon ]; then
  command -v agy >/dev/null || { echo "agy non trovato nel PATH" >&2; exit 2; }
  if ! timeout 120 agy models < /dev/null 2>/dev/null | grep -q "^${argon_model}[[:space:]]"; then
    echo "Argon ($argon_model) assente da 'agy models': rilanciare con --reviewer claude" >&2
    exit 4
  fi
else
  command -v claude >/dev/null || { echo "claude non trovato nel PATH" >&2; exit 2; }
fi

mkdir -p .omc/artifacts/ask tmp
# Tetto dei giri (AGENTS.md, ciclo di un'unità): 2 giri in tutto per unità, slug ...-roundN (o -reviewN, -contractN);
# oltre il secondo giro la review non parte e la decisione passa al coordinatore (arbitraggio).
if ! [[ "$slug" =~ -(round|review|contract)([0-9]+)$ ]]; then
  echo "slug senza numero di giro: '$slug' (atteso ...-roundN)" >&2
  exit 2
fi
round="${BASH_REMATCH[2]}"
if [ "${#round}" -gt 2 ] || [ "$round" -gt 2 ]; then
  echo "limite di 2 giri raggiunto per '$slug': fermati e consegna al coordinatore l'ultimo artefatto con i rilievi aperti (arbitraggio)" >&2
  exit 3
fi
log="tmp/review-${slug}-${reviewer}.log"
out=".omc/artifacts/ask/review-${slug}-${reviewer}-$(date +%Y%m%d-%H%M).md"
prompt="$(cat tools/review-template.md "$extra")"

# Istantanea del lavoro non committato a ogni giro (file modificati e nuovi, escluso il collegamento `data`): il delta
# del giro 2 lo calcola lo strumento, non l'autore.
unit="${slug%-*}"
snap="tmp/review-snap/${unit}-${BASH_REMATCH[1]}${round}"
rm -rf "$snap"
mkdir -p "$snap"
{ git diff --name-only HEAD; git ls-files -o --exclude-standard; } | grep -v -x 'data' | grep -v '^tmp/' | sort -u | while IFS= read -r f; do
  [ -f "$f" ] && cp --parents -- "$f" "$snap/"
done
if [ "$round" = 2 ]; then
  prev="tmp/review-snap/${unit}-${BASH_REMATCH[1]}1"
  [ -d "$prev" ] || { echo "manca l'istantanea del giro 1 ($prev): il giro 1 non è passato da questo strumento" >&2; exit 2; }
  delta="tmp/review-delta-${unit}-round2.diff"
  rc=0
  diff -ruN "$prev" "$snap" > "$delta" || rc=$?
  [ "$rc" -le 1 ] || { echo "diff delle istantanee fallito (uscita $rc)" >&2; exit 2; }
  prompt="${prompt}

Delta del giro 2 rispetto al giro 1, calcolato dallo strumento: \`${delta}\`."
fi

git_state() { { git status --porcelain=v1 --untracked-files=all; git diff HEAD; } | sha256sum; }
before="$(git_state)"
prompt="${prompt}

Sei in sola lettura: leggi file ed esegui solo comandi che non modificano nulla (git diff, git log, cat, grep). Scrivi la
review come risposta finale, senza creare file."
rc=0
if [ "$reviewer" = argon ]; then
  TMPDIR="$root/tmp" timeout 3600 agy -p "$prompt" --model "$argon_model" --mode plan --sandbox --print-timeout 3500s \
    --log-file "$log" < /dev/null > "$out" 2>> "$log" || rc=$?
else
  pf="tmp/review-${slug}-claude.prompt"; printf '%s\n' "$prompt" > "$pf"
  ASTRO_API_TIMEOUT=3600 ASTRO_API_READONLY=1 tools/agent-api.sh opus "$root" "$pf" "$log.json" 2> "$log" || rc=$?
  if [ "$rc" -eq 4 ]; then
    echo "credito API finito: review sull'abbonamento" >> "$log"
    rc=0
    TMPDIR="$root/tmp" timeout 3600 env -u ANTHROPIC_API_KEY claude -p --model opus --permission-mode auto \
      --disallowedTools "Edit,Write,NotebookEdit" --output-format text < "$pf" > "$out" 2>> "$log" || rc=$?
  elif [ "$rc" -eq 0 ]; then
    python3 -I -S -c 'import json, sys; sys.stdout.write(str(json.load(open(sys.argv[1], encoding="utf-8"))["result"]))' "$log.json" > "$out" || rc=$?
  fi
fi
[ "$(git_state)" = "$before" ] || { echo "lo stato di git è cambiato durante la review: il revisore non è in sola lettura (log: $log)" >&2; exit 2; }
if [ "$rc" -ne 0 ]; then
  if [ "$reviewer" = argon ]; then
    echo "Argon ha fallito (uscita $rc; rifiuto, crediti o timeout): rilanciare con --reviewer claude; artefatto: $out, log: $log" >&2
    exit 4
  fi
  echo "il revisore Claude ha fallito (uscita $rc o timeout); artefatto: $out, log: $log" >&2
  exit 2
fi
[ -s "$out" ] || { echo "artefatto vuoto o assente: $out" >&2; exit 2; }
echo "artefatto: $out"
verdict_exit "$out"
