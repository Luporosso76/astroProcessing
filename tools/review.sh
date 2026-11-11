#!/usr/bin/env bash
# Review indipendente. Uso: tools/review.sh [--reviewer argon|claude] [--worktree <percorso>] <slug> <file-fatti.md>
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
# `--worktree <percorso>` (assoluto, radice di un worktree git di questo repository, altrimenti uscita 2): stato di git,
# elenco dei file, istantanea e delta si calcolano nel worktree e il revisore parte con cwd lì; artefatti, log e
# istantanee restano nel checkout dello script (le istantanee in tmp/review-snap/<unità>-<tipo><giro>, che porta lo slug).
# Senza l'opzione tutto riguarda il checkout dello script, e se la cartella di partenza sta in un altro worktree dello
# stesso repository lo strumento esce con 2 chiedendo `--worktree`. Con l'opzione lo stato di git è controllato anche
# nel checkout dello script. Le istantanee stanno nel checkout dello script invocato: i due giri vanno lanciati dallo
# stesso script (quello del checkout principale). Non verificato: la lettura del delta da parte di agy in sandbox con
# cwd nel worktree (il percorso del delta è assoluto).
# Uso: tools/review.sh --selftest   prova il giudizio su artefatti sintetici e sulla fixture tests/fixtures/review/.
# Criterio: FAIL se c'è almeno un rilievo BLOCCANTE o MED, PASS solo con LOW o senza rilievi; un verdetto che
# contraddice i rilievi (PASS con BLOCCANTE/MED, FAIL con soli LOW) è incoerente (uscita 2).
# Uscita: 0 PASS, 1 FAIL, 2 errore del revisore, artefatto incoerente o argomenti non validi, 3 limite di giri raggiunto,
# 4 Argon non disponibile (rilanciare con --reviewer claude).
set -euo pipefail

start_pwd="$PWD"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
rootp="$(pwd -P)"

# is_repo_worktree <radice>: vero se è una radice elencata da `git worktree list` di questo repository.
is_repo_worktree() {
  local wl
  wl="$(git -C "$root" worktree list --porcelain)"
  case $'\n'"$wl"$'\n' in *$'\n'"worktree $1"$'\n'*) return 0 ;; esac
  return 1
}

# check_start_dir <cartella>: ritorna 2 se la cartella sta in un worktree di questo repository diverso dal checkout dello script.
check_start_dir() {
  local top
  if top="$(git -C "$1" rev-parse --show-toplevel 2>&1)"; then
    if [ "$top" != "$rootp" ] && is_repo_worktree "$top"; then
      echo "lanciato dal worktree $top ma lo script è di $rootp: senza --worktree la review riguarderebbe il checkout dello script; usare --worktree $top" >&2
      return 2
    fi
  fi
}

# resolve_worktree <percorso>: stampa la radice del worktree git (di questo repository) o ritorna 2 con diagnosi.
resolve_worktree() {
  local p="$1" top real
  case "$p" in /*) ;; *) echo "--worktree richiede un percorso assoluto: '$p'" >&2; return 2 ;; esac
  [ -d "$p" ] || { echo "--worktree: cartella assente: $p" >&2; return 2; }
  top="$(git -C "$p" rev-parse --show-toplevel)" || { echo "--worktree: non è un repository git: $p" >&2; return 2; }
  real="$(cd "$p" && pwd -P)"
  [ "$top" = "$real" ] || { echo "--worktree: non è la radice di un worktree ($top): $p" >&2; return 2; }
  is_repo_worktree "$top" || { echo "--worktree: non è un worktree di questo repository: $p" >&2; return 2; }
  printf '%s\n' "$top"
}

# list_files <worktree>: file modificati e nuovi del worktree (relativi alla sua radice, separati da NUL), escluso il
# collegamento `data` e tmp/.
list_files() {
  local f
  ( cd "$1" && { git diff --name-only -z HEAD; git ls-files -z -o --exclude-standard; } | sort -z -u ) | while IFS= read -r -d '' f; do
    case "$f" in data|tmp/*) ;; *) printf '%s\0' "$f" ;; esac
  done
}

# snapshot <worktree> <destinazione assoluta>: copia i file modificati e nuovi esistenti del worktree (i cancellati si saltano).
snapshot() {
  local w="$1" dest="$2" lst f
  mkdir -p "$dest"
  lst="$dest.files"
  list_files "$w" > "$lst"
  while IFS= read -r -d '' f; do
    if [ -f "$w/$f" ]; then (cd "$w" && cp --parents -- "$f" "$dest/"); fi
  done < "$lst"
  rm -f -- "$lst"
}

# git_state <cartella>...: impronta dello stato di git (file tracciati e nuovi) di ogni cartella.
git_state() {
  local d
  for d in "$@"; do
    git -C "$d" status --porcelain=v1 --untracked-files=all
    git -C "$d" diff HEAD
  done | sha256sum
}

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
  mkdir -p "$root/tmp"
  d="$(mktemp -d "$root/tmp/review-selftest.XXXXXX")"
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
  # --worktree: due worktree temporanei (a, b) e un repository estraneo, tutti sotto tmp/ del checkout dello script
  fail() { echo "review selftest fallito: $1" >&2; exit 1; }
  in_list() { grep -xF -- "$2" <<< "$1" > /dev/null; }
  d="$(cd "$d" && pwd -P)"
  wa="$d/wa"; wb="$d/wb"; foreign="$d/foreign"; plain="$d/plain"
  ba="review-selftest-wt-$$-a"; bb="review-selftest-wt-$$-b"
  leftover="$(git -C "$root" for-each-ref --format='%(refname:short)' 'refs/heads/review-selftest-wt-*')"
  [ -z "$leftover" ] || echo "attenzione: rami di selftest rimasti da corse precedenti: $(tr '\n' ' ' <<< "$leftover")" >&2
  cleanup_wt() {
    local bad=0 w b
    for w in "$wa" "$wb"; do
      if [ -d "$w" ]; then git -C "$root" worktree remove --force "$w" || bad=1; fi
    done
    for b in "$ba" "$bb"; do
      if git -C "$root" rev-parse -q --verify "refs/heads/$b" > /dev/null; then git -C "$root" branch -q -D "$b" || bad=1; fi
    done
    [ "$bad" -eq 0 ] || echo "pulizia del selftest incompleta: controllare 'git worktree list' e i rami review-selftest-wt-*" >&2
    return "$bad"
  }
  trap cleanup_wt EXIT
  git -C "$root" worktree add -q -b "$ba" "$wa" HEAD
  git -C "$root" worktree add -q -b "$bb" "$wb" HEAD
  git init -q "$foreign"
  mkdir -p "$plain"
  wt_expect() { # etichetta uscita-attesa diagnosi-attesa percorso
    local rc=0 out
    out="$(GIT_CEILING_DIRECTORIES="$rootp/tmp" resolve_worktree "$4" 2>&1)" || rc=$?
    [ "$rc" -eq "$2" ] || fail "$1 (uscita $rc, attesa $2): $out"
    case "$out" in *"$3"*) ;; *) fail "$1: diagnosi inattesa: $out" ;; esac
  }
  wt_expect "percorso relativo" 2 "percorso assoluto" "tmp/x"
  wt_expect "percorso inesistente" 2 "cartella assente" "$d/inesistente"
  wt_expect "cartella non repository" 2 "non è un repository git" "$plain"
  wt_expect "radice di repository estraneo" 2 "non è un worktree di questo repository" "$foreign"
  wt_expect "sottocartella di worktree" 2 "non è la radice" "$wa/tools"
  wt_expect "worktree valido" 0 "$wa" "$wa"
  printf 'x\n' > "$wa/only-a.txt"; printf 'x\n' > "$wa/è a.txt"; rm -- "$wa/tools/review-template.md"
  printf 'x\n' > "$wb/only-b.txt"
  la="$(list_files "$wa" | tr '\0' '\n')"; lb="$(list_files "$wb" | tr '\0' '\n')"
  in_list "$la" "only-a.txt" || fail "file solo in A assente dall'elenco di A"
  in_list "$la" "è a.txt" || fail "nome non ASCII assente dall'elenco di A"
  in_list "$la" "tools/review-template.md" || fail "file cancellato in A assente dall'elenco di A"
  if in_list "$la" "only-b.txt"; then fail "file solo in B nell'elenco di A"; fi
  in_list "$lb" "only-b.txt" || fail "file solo in B assente dall'elenco di B"
  if in_list "$lb" "only-a.txt"; then fail "file solo in A nell'elenco di B"; fi
  snapshot "$wa" "$d/snap-a"; snapshot "$wb" "$d/snap-b"
  [ -f "$d/snap-a/only-a.txt" ] && [ -f "$d/snap-a/è a.txt" ] || fail "istantanea di A senza i suoi file"
  [ ! -e "$d/snap-a/only-b.txt" ] && [ ! -e "$d/snap-a/tools/review-template.md" ] || fail "istantanea di A con file non suoi o cancellati"
  [ -f "$d/snap-b/only-b.txt" ] && [ ! -e "$d/snap-b/only-a.txt" ] || fail "istantanea di B errata"
  s0="$(git_state "$wa")"
  printf 'x\n' > "$wb/b2.txt"
  [ "$(git_state "$wa")" = "$s0" ] || fail "git_state di A cambiato da una modifica in B"
  printf 'x\n' > "$wa/a2.txt"
  [ "$(git_state "$wa")" != "$s0" ] || fail "git_state di A invariato dopo una modifica in A"
  printf 'x\n' >> "$wa/tools/review.sh"; s3="$(git_state "$wa")"
  printf 'y\n' >> "$wa/tools/review.sh"
  [ "$(git_state "$wa")" != "$s3" ] || fail "git_state di A insensibile al contenuto di un file tracciato modificato"
  rc=0; (cd "$wa" && check_start_dir "$wa") > /dev/null 2>&1 || rc=$?
  [ "$rc" -eq 2 ] || fail "partenza da un altro worktree (uscita $rc, attesa 2)"
  check_start_dir "$root" || fail "partenza dal checkout dello script rifiutata"
  cleanup_wt; trap - EXIT
  rm -rf -- "$d"
  echo "review selftest: ok"
  exit 0
fi

usage="uso: tools/review.sh [--reviewer argon|claude] [--worktree <percorso>] <slug> <file-fatti.md>"
reviewer=argon
wt="$rootp"
dprefix=""
worktree_opt=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    --reviewer) [ "$#" -ge 2 ] || { echo "$usage" >&2; exit 2; }; reviewer="$2"; shift 2 ;;
    --worktree) [ "$#" -ge 2 ] || { echo "$usage" >&2; exit 2; }; wt="$(resolve_worktree "$2")" || exit 2; dprefix="$root/"; worktree_opt=1; shift 2 ;;
    *) break ;;
  esac
done
case "$reviewer" in argon|claude) ;; *) echo "revisore non ammesso: '$reviewer' (argon o claude)" >&2; exit 2 ;; esac
if [ "$#" -ne 2 ]; then
  echo "$usage" >&2
  exit 2
fi
slug="$1"
extra="$2"
case "$extra" in /*) ;; *) extra="$start_pwd/$extra" ;; esac
if ! [[ "$slug" =~ ^[A-Za-z0-9._-]+$ ]]; then
  echo "slug non valido: '$slug' (ammessi lettere, cifre, . _ -)" >&2
  exit 2
fi
[ -f "$extra" ] || { echo "file extra assente: $extra" >&2; exit 2; }
if [ "$worktree_opt" -eq 0 ]; then check_start_dir "$start_pwd" || exit 2; fi
[ -f tools/review-template.md ] || { echo "template assente: tools/review-template.md" >&2; exit 2; }
argon_model="gemini-4-argon-high"
if [ "$reviewer" = argon ]; then
  command -v agy >/dev/null || { echo "agy non trovato nel PATH" >&2; exit 2; }
  models_out="$(timeout 120 agy models < /dev/null)" || models_out=""
  if ! grep -q "^${argon_model}[[:space:]]" <<< "$models_out"; then
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
snapshot "$wt" "$root/$snap"
if [ "$round" = 2 ]; then
  prev="tmp/review-snap/${unit}-${BASH_REMATCH[1]}1"
  [ -d "$prev" ] || { echo "manca l'istantanea del giro 1 ($prev): il giro 1 non è passato da questo strumento" >&2; exit 2; }
  delta="tmp/review-delta-${unit}-round2.diff"
  rc=0
  diff -ruN "$prev" "$snap" > "$delta" || rc=$?
  [ "$rc" -le 1 ] || { echo "diff delle istantanee fallito (uscita $rc)" >&2; exit 2; }
  prompt="${prompt}

Delta del giro 2 rispetto al giro 1, calcolato dallo strumento: \`${dprefix}${delta}\`."
fi

watch=("$wt")
[ "$wt" = "$rootp" ] || watch+=("$rootp")
before="$(git_state "${watch[@]}")"
prompt="${prompt}

Sei in sola lettura: leggi file ed esegui solo comandi che non modificano nulla (git diff, git log, cat, grep). Scrivi la
review come risposta finale, senza creare file."
rc=0
if [ "$reviewer" = argon ]; then
  ( cd "$wt" && TMPDIR="$root/tmp" timeout 3600 agy -p "$prompt" --model "$argon_model" --mode plan --sandbox --print-timeout 3500s \
    --log-file "$root/$log" < /dev/null > "$root/$out" 2>> "$root/$log" ) || rc=$?
else
  pf="tmp/review-${slug}-claude.prompt"; printf '%s\n' "$prompt" > "$pf"
  ASTRO_API_TIMEOUT=3600 ASTRO_API_READONLY=1 tools/agent-api.sh opus "$wt" "$pf" "$log.json" 2> "$log" || rc=$?
  if [ "$rc" -eq 4 ]; then
    echo "credito API finito: review sull'abbonamento" >> "$log"
    rc=0
    ( cd "$wt" && TMPDIR="$root/tmp" timeout 3600 env -u ANTHROPIC_API_KEY claude -p --model opus --permission-mode auto \
      --disallowedTools "Edit,Write,NotebookEdit" --output-format text < "$root/$pf" > "$root/$out" 2>> "$root/$log" ) || rc=$?
  elif [ "$rc" -eq 0 ]; then
    python3 -I -S -c 'import json, sys; sys.stdout.write(str(json.load(open(sys.argv[1], encoding="utf-8"))["result"]))' "$log.json" > "$out" || rc=$?
  fi
fi
[ "$(git_state "${watch[@]}")" = "$before" ] || { echo "lo stato di git è cambiato durante la review: il revisore non è in sola lettura (log: $log)" >&2; exit 2; }
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
