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

forbidden_symbols='^(system|popen|exec[a-z]*|fexecve|posix_spawn[a-z_]*|fork|vfork|dlopen|dlmopen)(@.*)?$'

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

undefined_symbols() {
  local bin="$1"
  if [[ ! -f "$bin" ]]; then
    tool_error "NOEXEC binario assente: $bin"
  fi
  if [[ "$bin" == *.a || "$bin" == *.o ]]; then
    "$nm_bin" --undefined-only --format=just-symbols "$bin"
  else
    "$nm_bin" -D --undefined-only --format=just-symbols "$bin"
  fi
}

check() {
  local status=0 syms bad
  for bin in "$@"; do
    syms=$(undefined_symbols "$bin") || tool_error "NOEXEC nm fallito su $bin"
    if gmatch "$forbidden_symbols" "$syms"; then
      echo "NOEXEC simbolo vietato in $bin: $(tr '\n' ' ' <<<"$bad")" >&2; status=1
    fi
  done
  return $status
}

if [[ "${1:-}" == "--self-test" ]]; then
  [[ $# -eq 2 ]] || tool_error "uso: $0 --self-test <compilatore C++>"
  cc="${ASTRO_CC:-$2}"
  op mkdir -p "$root/tmp"
  work=$(mktemp -d "$root/tmp/noexec.XXXXXX") || tool_error "mktemp fallito"
  for call in 'fexecve(0, nullptr, nullptr)' 'posix_spawnp(nullptr, "x", nullptr, nullptr, nullptr, nullptr)' 'system("x")' 'popen("x", "r")' 'execv("x", nullptr)' 'posix_spawn(nullptr, "x", nullptr, nullptr, nullptr, nullptr)' 'fork()' 'vfork()' 'dlopen("x", 0)' 'dlmopen(0, "x", 0)'; do
    printf '#include <cstdio>\n#include <cstdlib>\n#include <unistd.h>\n#include <spawn.h>\n#include <dlfcn.h>\nint probe() { return (int)(long)(%s); }\n' "$call" > "$work/probe.cpp" || tool_error "scrittura fallita"
    op "$cc" -w -c "$work/probe.cpp" -o "$work/probe.o"
    rc=0; check "$work/probe.o" 2>/dev/null || rc=$?
    [[ $rc -eq 1 ]] || selftest_fail "NOEXEC self-test: ${call%%(*} non è stato rilevato (uscita $rc)"
  done
  rc=0; ( trap - EXIT; gmatch '(' "x" ) 2>/dev/null || rc=$?
  [[ $rc -eq 2 ]] || selftest_fail "NOEXEC self-test: un errore di grep non ferma lo script (uscita $rc)"
  printf 'int probe() { return 0; }\n' > "$work/clean.cpp" || tool_error "scrittura fallita"
  op "$cc" -c "$work/clean.cpp" -o "$work/clean.o"
  check "$work/clean.o" || selftest_fail "NOEXEC self-test: l'oggetto pulito è stato rifiutato"
  echo "self-test ok: system, popen, exec*, posix_spawn, fork, vfork, dlopen, dlmopen rilevati; oggetto pulito accettato"
  exit 0
fi

[[ $# -gt 0 ]] || tool_error "uso: $0 <binario|archivio>... | --self-test <compilatore C++>"
check "$@" || found_defect
