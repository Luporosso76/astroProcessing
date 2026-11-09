#!/usr/bin/env bash
# Lint delle classi di difetto del progetto (regole in tools/lint-rules.yml, eccezioni in tools/lint-allowlist.txt).
# Uso: tools/lint.sh            scansiona src/ tests/ tools/
#      tools/lint.sh --selftest prova le regole su file sintetici in tmp/
# Uscita: 0 pulito, 1 trovati difetti fuori allowlist, 2 errore dello strumento.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

command -v ast-grep >/dev/null || { echo "lint: ast-grep non trovato nel PATH (atteso in ~/.local/bin)" >&2; exit 2; }
command -v jq >/dev/null || { echo "lint: jq non trovato nel PATH" >&2; exit 2; }
jq_bin="${LINT_JQ:-jq}"
rules="$root/tools/lint-rules.yml"
[ -f "$rules" ] || { echo "lint: regole assenti: $rules" >&2; exit 2; }

# scan <allowlist> <percorso>...: stampa i difetti fuori allowlist (TSV: regola, file, riga, testo), ritorna 1 se ce ne sono.
scan() {
  local allow="$1"; shift
  [ -f "$allow" ] || { echo "lint: allowlist assente: $allow" >&2; return 2; }
  local raw status=0
  # ast-grep esce con 1 quando trova difetti di livello error: l'esito vero si legge dal JSON.
  raw="$(ast-grep scan -r "$rules" --no-ignore vcs --json=stream "$@" 2>"$root/tmp/lint-astgrep.err")" || status=$?
  if [ "$status" -ne 0 ] && [ "$status" -ne 1 ]; then
    echo "lint: ast-grep ha fallito (uscita $status):" >&2
    cat "$root/tmp/lint-astgrep.err" >&2
    return 2
  fi
  if [ "$status" -eq 1 ] && ! grep -q 'error(s) found' "$root/tmp/lint-astgrep.err"; then
    echo "lint: ast-grep uscito con 1 senza difetti dichiarati:" >&2
    cat "$root/tmp/lint-astgrep.err" >&2
    return 2
  fi
  local parsed
  parsed="$(printf '%s\n' "$raw" | "$jq_bin" -r 'select(.ruleId != null) |
      [.ruleId, .file, (.range.start.line + 1), (.text | split("\n")[0] | gsub("\\s+"; " ")), (.text | gsub("\\s+"; " "))] | @tsv')" || {
    echo "lint: jq ha fallito sull'output di ast-grep" >&2
    return 2
  }
  # Un'eccezione dell'allowlist vale per file, regola e impronta SHA-256 del testo normalizzato dell'intero costrutto.
  local allowed
  allowed="$(awk -F'\t' '!/^[[:space:]]*#/ && NF >= 4 {print $1 "\t" $2 "\t" $3}' "$allow")" || { echo "lint: allowlist illeggibile: $allow" >&2; return 2; }
  local found=0 rule file line text full digest
  while IFS=$'\t' read -r rule file line text full; do
    [ -n "$rule" ] || continue
    digest="$(printf '%s' "$full" | sha256sum | cut -d' ' -f1)"
    if grep -qxF -- "$file"$'\t'"$rule"$'\t'"$digest" <<<"$allowed"; then
      continue
    fi
    printf '%s\t%s:%s\t%s\t%s\n' "$rule" "$file" "$line" "$text" "$digest"
    found=$((found + 1))
  done <<<"$parsed"
  [ "$found" -eq 0 ]
}

# Accenti: parole italiane scritte con l'apostrofo al posto dell'accento («piu'», «e'»). Elenco esplicito, perché
# l'apostrofo delle elisioni («l'immagine») e i letterali carattere («'e'») sono legittimi. Il controllo è testo puro
# (commenti, stringhe, Python, shell): ast-grep non copre tutti i linguaggi di src/ tests/ tools/ allo stesso modo.
accent_words='meta|utilita|piu|gia|puo|unita|identita|linearita|verita|capacita|cosi|affinita|finche|affinche|rappresentabilita|conformita|compatibilita|invalidita|disuniformita|perche|durabilita|purche|cioe|proprieta|profondita|parita|liberta|affidabilita|sara|fallira|girera|rifa|poiche|benche|pero|stabilita|qualita|citta|novita|possibilita|necessita|quantita|attivita|ahime|nonche|ne|e|da|cio'
# Parola isolata seguita da apostrofo; e la stessa dopo l'apostrofo di un'elisione («l'unita'»).
accent_re_word="(^|[^A-Za-z'])(${accent_words})'([^A-Za-z]|\$)"
accent_re_elided="[A-Za-z]'(${accent_words})'([^A-Za-z]|\$)"

# accent_scan <percorso>...: stampa le occorrenze (file:riga:testo), ritorna 1 se ce ne sono, 2 se grep fallisce.
# Questo file è escluso: contiene l'elenco e gli esempi del controllo, quindi le parole con apostrofo per definizione.
accent_scan() {
  local hits st=0
  hits="$(grep -rIEn --exclude-dir=third_party --exclude-dir=build --exclude=lint.sh -e "$accent_re_word" -e "$accent_re_elided" "$@")" || st=$?
  if [ "$st" -eq 1 ]; then return 0; fi
  if [ "$st" -ne 0 ]; then echo "lint: grep degli accenti fallito (uscita $st)" >&2; return 2; fi
  while IFS= read -r hit; do
    printf 'accenti\t%s\n' "$hit"
  done <<<"$hits"
  return 1
}

mkdir -p tmp

if [ "${1:-}" = "--selftest" ]; then
  d="tmp/lint-selftest"
  rm -rf "$d"
  mkdir -p "$d"
  printf 'void f() {\n  try { g(); } catch (...) {}\n  try { g(); } catch (const E&) { fprintf(stderr, "x"); }\n  try { g(); } catch (...) { continue; }\n}\n' > "$d/bad.cpp"
  printf 'void f() {\n  try { g(); } catch (...) { throw; }\n  try { g(); } catch (const E& e) { return 1; }\n  try { g(); } catch (...) { st.fail(std::current_exception()); }\n}\n' > "$d/good.cpp"
  printf 'a || true\nb || exit 3\n' > "$d/bad.sh"
  printf 'import sys\ntry:\n    x()\nexcept:\n    pass\ntry:\n    x()\nexcept Exception:\n    print(1)\nif __name__ == "__main__":\n    sys.exit(main())\n' > "$d/bad.py"
  printf 'import sys\ntry:\n    x()\nexcept OSError as e:\n    sys.exit(str(e))\nif __name__ == "__main__":\n    try:\n        sys.exit(main())\n    except Exception as e:\n        sys.exit(str(e))\n' > "$d/good.py"
  : > "$d/allow-empty.txt"
  status=0
  out="$(scan "$d/allow-empty.txt" "$d")" || status=$?
  fail() { echo "lint selftest fallito: $1" >&2; echo "$out" >&2; exit 1; }
  [ "$status" -eq 1 ] || fail "la scansione dei file difettosi doveva uscire con 1, uscita $status"
  for expect in "cpp-catch-empty.*bad.cpp:2" "cpp-catch-print-only.*bad.cpp:3" "cpp-catch-all-silent.*bad.cpp:4" \
                "sh-or-true.*bad.sh:1" "py-except-swallow.*bad.py:4" "py-except-swallow.*bad.py:8" "py-main-without-handler.*bad.py:10"; do
    printf '%s\n' "$out" | grep -qE "$expect" || fail "difetto atteso non trovato: $expect"
  done
  for clean in good.cpp good.py "bad.sh:2"; do
    if printf '%s\n' "$out" | grep -q "$clean"; then fail "falso positivo su $clean"; fi
  done
  digest="$(printf '%s' 'a || true' | sha256sum | cut -d' ' -f1)"
  printf 'tmp/lint-selftest/bad.sh\tsh-or-true\t%s\tmotivo di prova\n' "$digest" > "$d/allow-sh.txt"
  status=0
  out="$(scan "$d/allow-sh.txt" "$d/bad.sh")" || status=$?
  [ -z "$out" ] || fail "l'allowlist non ha escluso il difetto"
  [ "$status" -eq 0 ] || fail "la scansione con allowlist doveva uscire con 0, uscita $status"
  printf 'tmp/lint-selftest/bad.sh\tsh-or-true\t%s\tcorpo diverso\n' "$(printf '%s' 'a || true; altro' | sha256sum | cut -d' ' -f1)" > "$d/allow-wrong-hash.txt"
  status=0
  out="$(scan "$d/allow-wrong-hash.txt" "$d/bad.sh")" || status=$?
  [ "$status" -eq 1 ] || fail "un'impronta diversa non deve escludere il difetto (uscita $status)"
  printf 'tmp/lint-selftest/bad.sh\tcpp-catch-empty\t%s\tregola diversa\n' "$digest" > "$d/allow-wrong-rule.txt"
  status=0
  out="$(scan "$d/allow-wrong-rule.txt" "$d/bad.sh")" || status=$?
  [ "$status" -eq 1 ] || fail "una regola diversa non deve escludere il difetto (uscita $status)"
  status=0
  out="$(LINT_JQ=/bin/false; jq_bin=/bin/false; scan "$d/allow-empty.txt" "$d" 2>/dev/null)" || status=$?
  [ "$status" -eq 2 ] || fail "un errore di jq doveva dare uscita 2, uscita $status"
  status=0
  out="$(scan "$d/allow-empty.txt" "$d/good.cpp" "$d/good.py")" || status=$?
  [ "$status" -eq 0 ] || fail "la scansione dei file puliti doveva uscire con 0, uscita $status"
  # Il difetto è composto a runtime: così questo sorgente non contiene la parola con apostrofo che cerca.
  ad="tmp/lint-selftest-accenti"
  rm -rf "$ad"
  mkdir -p "$ad"
  w="pi"
  w2="meta"
  printf "// il conto %su' sbagliato\n// vedi %s' qui\n" "$w" "$w2" > "$ad/bad.cpp"
  printf "// l'unit\303\240 e il valore 'e' sono corretti: piu\303\271 e \303\250\n" > "$ad/good.cpp"
  status=0
  out="$(accent_scan "$ad")" || status=$?
  [ "$status" -eq 1 ] || fail "l'accento con apostrofo doveva dare uscita 1, uscita $status"
  printf '%s\n' "$out" | grep -q 'bad.cpp:1:' || fail "difetto piu' non rilevato"
  printf '%s\n' "$out" | grep -q 'bad.cpp:2:' || fail "secondo difetto meta' non rilevato nello stesso file"
  if printf '%s\n' "$out" | grep -qv $'^accenti\t'; then fail "riga di uscita senza prefisso accenti"; fi
  if printf '%s\n' "$out" | grep -q 'good.cpp'; then fail "falso positivo su good.cpp (elisione o letterale carattere)"; fi
  echo "lint selftest: ok"
  exit 0
fi

allow="$root/tools/lint-allowlist.txt"
rc=0
scan "$allow" src tests tools || rc=$?
[ "$rc" -eq 2 ] && exit 2
[ "$rc" -eq 1 ] && echo "lint: difetti fuori allowlist (tools/lint-allowlist.txt)" >&2
accent_rc=0
accent_scan src tests tools cmake || accent_rc=$?
[ "$accent_rc" -eq 2 ] && exit 2
if [ "$accent_rc" -eq 1 ]; then echo "lint: accenti con apostrofo (usare la lettera accentata)" >&2; fi
if [ "$rc" -eq 0 ] && [ "$accent_rc" -eq 0 ]; then
  echo "lint: ok"
else
  exit 1
fi
