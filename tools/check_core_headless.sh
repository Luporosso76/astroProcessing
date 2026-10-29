#!/usr/bin/env bash
set -euo pipefail

# Ultima riga di ogni esecuzione: CHECK-RESULT ok | defect | tool-error.
# `defect` (uscita 1) solo per una violazione accertata o per un'asserzione del self-test fallita; ogni errore operativo
# (mkdir, compilatore, nm, readelf, grep >= 2, scrittura di file) è `tool-error` (uscita 2). ctest conserva la riga nel
# junit e tools/mutate.sh distingue così un difetto trovato da un errore d'infrastruttura.
# ASTRO_NM, ASTRO_READELF e ASTRO_CC sostituiscono nm, readelf e il compilatore del self-test (prova negativa).
nm_bin="${ASTRO_NM:-nm}"
readelf_bin="${ASTRO_READELF:-readelf}"
work=''
defect=0
finish() {
  local rc=$?
  [[ -z "$work" ]] || rm -rf "$work"
  if [[ $rc -eq 0 ]]; then echo "CHECK-RESULT: ok"
  elif [[ $rc -eq 1 && $defect -eq 1 ]]; then echo "CHECK-RESULT: defect"
  else echo "CHECK-RESULT: tool-error"; fi
}
trap finish EXIT

tool_error() { echo "$1" >&2; exit 2; }
op() { "$@" || tool_error "operazione fallita: $*"; }
found_defect() { defect=1; exit 1; }
selftest_fail() { echo "$1" >&2; found_defect; }

qt_symbols='(^|[^A-Za-z0-9_])(Q[A-Z][A-Za-z0-9_]*::|Qt::|qt_[a-z_]+|qFatal|qWarning|qDebug|qCritical)'
qt_needed='^libQt'

bad=''
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# gmatch <ere> <testo> [grep-opt]: imposta $bad con le righe che corrispondono; ritorna 0 se ce ne sono, 1 se nessuna.
# Un errore di grep (uscita >= 2) ferma lo script con codice 2.
gmatch() {
  local rc=0
  bad=$(grep -E ${3:-} -- "$1" <<<"$2") || rc=$?
  if [[ $rc -ge 2 ]]; then tool_error "grep fallito (uscita $rc)"; fi
  [[ -n "$bad" ]]
}

check() {
  local status=0 syms needed bad
  for bin in "$@"; do
    [[ -f "$bin" ]] || tool_error "HEADLESS binario assente: $bin"
    syms=$("$nm_bin" -C --format=just-symbols "$bin") || tool_error "HEADLESS nm fallito su $bin"
    if gmatch "$qt_symbols" "$syms"; then
      echo "HEADLESS simbolo Qt in $bin: $(sort -u <<<"$bad" | head -5 | tr '\n' ' ')" >&2; status=1
    fi
    if [[ "$bin" != *.a && "$bin" != *.o ]]; then
      needed=$("$readelf_bin" -d "$bin") || tool_error "HEADLESS readelf fallito su $bin"
      needed=$(sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p' <<<"$needed")
      if gmatch "$qt_needed" "$needed"; then
        echo "HEADLESS NEEDED Qt in $bin: $(tr '\n' ' ' <<<"$bad")" >&2; status=1
      fi
    fi
  done
  return $status
}

check_build_files() {
  local status=0 hits rc
  rc=0; hits=$(grep -nE 'Qt[0-9]*|find_package\(Qt' "$root/CMakeLists.txt" "$root/src/core/CMakeLists.txt" "$root/tests/unit/CMakeLists.txt" "$root/CMakePresets.json") || rc=$?
  if [[ $rc -ge 2 ]]; then tool_error "HEADLESS grep dei file di build fallito (uscita $rc)"; fi
  if [[ $rc -eq 0 ]]; then
    echo "HEADLESS riferimento Qt nei file di build del core: $hits" >&2; status=1
  fi
  rc=0; hits=$(grep -rnE '#include *[<"]Q[A-Za-z]+' "$root/src/core" "$root/tests/unit") || rc=$?
  if [[ $rc -ge 2 ]]; then tool_error "HEADLESS grep degli include fallito (uscita $rc)"; fi
  if [[ $rc -eq 0 ]]; then
    echo "HEADLESS include Qt nel core o nei test: $hits" >&2; status=1
  fi
  return $status
}

if [[ "${1:-}" == "--self-test" ]]; then
  [[ $# -eq 2 ]] || tool_error "uso: $0 --self-test <compilatore C++>"
  cc="${ASTRO_CC:-$2}"
  op mkdir -p "$root/tmp"
  work=$(mktemp -d "$root/tmp/headless.XXXXXX") || tool_error "mktemp fallito"
  printf 'class QString { public: static QString fromUtf8(const char*); };\nQString probe() { return QString::fromUtf8("x"); }\n' > "$work/sym.cpp" || tool_error "scrittura fallita"
  op "$cc" -w -c "$work/sym.cpp" -o "$work/sym.o"
  rc=0; check "$work/sym.o" 2>/dev/null || rc=$?
  [[ $rc -eq 1 ]] || selftest_fail "HEADLESS self-test: simbolo Qt non rilevato (uscita $rc)"
  printf 'int neutral_fn() { return 0; }\n' > "$work/stub.cpp" || tool_error "scrittura fallita"
  op "$cc" -w -shared -fPIC -Wl,-soname,libQt6Core.so.6 "$work/stub.cpp" -o "$work/libQt6Core.so.6"
  printf 'int neutral_fn();\nint main() { return neutral_fn(); }\n' > "$work/main.cpp" || tool_error "scrittura fallita"
  op "$cc" -w "$work/main.cpp" -o "$work/main" "$work/libQt6Core.so.6" -Wl,-rpath,"$work"
  rc=0; check "$work/main" 2>/dev/null || rc=$?
  [[ $rc -eq 1 ]] || selftest_fail "HEADLESS self-test: NEEDED Qt non rilevato (uscita $rc)"
  if gmatch "$qt_symbols" "$("$nm_bin" -C --format=just-symbols "$work/main")"; then selftest_fail "HEADLESS self-test: l'eseguibile di prova contiene simboli Qt"; fi
  rc=0; ( trap - EXIT; gmatch '(' "x" ) 2>/dev/null || rc=$?
  [[ $rc -eq 2 ]] || selftest_fail "HEADLESS self-test: un errore di grep non ferma lo script (uscita $rc)"
  printf 'int probe() { return 0; }\n' > "$work/clean.cpp" || tool_error "scrittura fallita"
  op "$cc" -w -c "$work/clean.cpp" -o "$work/clean.o"
  check "$work/clean.o"
  echo "self-test ok: simbolo Qt e NEEDED Qt rilevati; oggetto pulito accettato"
  exit 0
fi

[[ $# -gt 0 ]] || tool_error "uso: $0 <binario|archivio>... | --self-test <compilatore C++>"
check "$@" || found_defect
check_build_files || found_defect
echo "core headless: nessun simbolo, NEEDED o riferimento di build Qt"
