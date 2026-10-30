#!/usr/bin/env bash
set -euo pipefail

# T-M2R5-05 e parte binaria di T-M2R5-07 (ADR-07): istruzioni fuse o approssimate nei kernel, crtfastmath nei binari.
# Uso: check_simd_binary.sh objects <kernel-scalare.o> <kernel-avx2.o>
#      check_simd_binary.sh fastmath <file-di-build>... -- <binario>...
#      check_simd_binary.sh --self-test objects|fastmath <compilatore C++>
# Ultima riga di ogni esecuzione: CHECK-RESULT ok | defect | tool-error (come tools/check_noexec.sh).
# `defect` (uscita 1) solo per una violazione accertata o un'asserzione del self-test fallita; ogni errore operativo
# (objdump, nm, compilatore, grep >= 2, file assente) è `tool-error` (uscita 2).
objdump_bin="${ASTRO_OBJDUMP:-objdump}"
nm_bin="${ASTRO_NM:-nm}"
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

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Mnemonici vietati: FMA (vfmadd*, vfmsub*, vfnmadd*, vfnmsub*, anche AVX-512) e approssimati (rcp*, rsqrt*, VEX e SSE).
forbidden='^[[:space:]]*[0-9a-f]+:[[:space:]]+(vfn?m(add|sub)[0-9a-z]*|v?rcp[0-9a-z]*|v?rsqrt[0-9a-z]*)([[:space:]]|$)'
packed_ymm='^[[:space:]]*[0-9a-f]+:[[:space:]]+v(add|sub|mul|div)pd[[:space:]].*%ymm'
fastmath_flags='(-ffast-math|-Ofast|-funsafe-math-optimizations|crtfastmath)'

hits=''
# gmatch <ere> <testo>: $hits con le righe che corrispondono; 0 se ce ne sono, 1 se nessuna, grep >= 2 = tool-error.
gmatch() {
  local rc=0
  hits=$(grep -E -- "$1" <<<"$2") || rc=$?
  if [[ $rc -ge 2 ]]; then tool_error "grep fallito (uscita $rc)"; fi
  [[ -n "$hits" ]]
}

disasm() {
  [[ -f "$1" ]] || tool_error "SIMD oggetto assente: $1"
  "$objdump_bin" -d --no-show-raw-insn "$1" || tool_error "SIMD objdump fallito su $1"
}

# Ogni errore di disasm (oggetto assente, objdump fallito) termina lo script con 2 anche dentro una lista `||`.
check_objects() {
  local status=0 scalar avx2
  scalar=$(disasm "$1") || tool_error "SIMD disassemblaggio fallito: $1"
  avx2=$(disasm "$2") || tool_error "SIMD disassemblaggio fallito: $2"
  if gmatch "$forbidden" "$scalar"; then
    echo "SIMD istruzioni vietate in $1: $(head -3 <<<"$hits" | tr '\n' ' ')" >&2; status=1
  fi
  if gmatch "$forbidden" "$avx2"; then
    echo "SIMD istruzioni vietate in $2: $(head -3 <<<"$hits" | tr '\n' ' ')" >&2; status=1
  fi
  if gmatch '%ymm' "$scalar"; then echo "SIMD registri ymm nel kernel scalare $1" >&2; status=1; fi
  if ! gmatch "$packed_ymm" "$avx2"; then echo "SIMD nessuna operazione f64 su 4 corsie nel kernel avx2 $2" >&2; status=1; fi
  return $status
}

check_fastmath() {
  local status=0 files=() bins=() seen=0 syms text
  for a in "$@"; do
    if [[ "$a" == "--" ]]; then seen=1; continue; fi
    if [[ $seen -eq 0 ]]; then files+=("$a"); else bins+=("$a"); fi
  done
  [[ ${#files[@]} -gt 0 && ${#bins[@]} -gt 0 ]] || tool_error "uso: $0 fastmath <file-di-build>... -- <binario>..."
  for f in "${files[@]}"; do
    [[ -f "$f" ]] || tool_error "SIMD file di build assente: $f"
    text=$(cat -- "$f") || tool_error "lettura fallita: $f"
    if gmatch "$fastmath_flags" "$text"; then
      echo "SIMD opzioni fast-math in $f: $(head -2 <<<"$hits" | cut -c1-200 | tr '\n' ' ')" >&2; status=1
    fi
  done
  for b in "${bins[@]}"; do
    [[ -f "$b" ]] || tool_error "SIMD binario assente: $b"
    syms=$("$nm_bin" -a "$b") || tool_error "SIMD nm fallito su $b"
    if gmatch '[[:space:]]set_fast_math$' "$syms"; then echo "SIMD crtfastmath.o (set_fast_math) in $b" >&2; status=1; fi
  done
  return $status
}

if [[ "${1:-}" == "--self-test" ]]; then
  [[ $# -eq 3 ]] || tool_error "uso: $0 --self-test objects|fastmath <compilatore C++>"
  cc="${ASTRO_CC:-$3}"
  op mkdir -p "$root/tmp"
  work=$(mktemp -d "$root/tmp/simdcheck.XXXXXX") || tool_error "mktemp fallito"
  printf '#include <immintrin.h>\n__m256d k(__m256d a, __m256d b) { return _mm256_div_pd(_mm256_add_pd(a, b), b); }\n' \
    > "$work/clean_avx2.cpp" || tool_error "scrittura fallita"
  printf 'double k(double a, double b) { return (a + b) / b; }\n' > "$work/clean_scalar.cpp" || tool_error "scrittura fallita"
  op "$cc" -O2 -mavx2 -ffp-contract=off -c "$work/clean_avx2.cpp" -o "$work/clean_avx2.o"
  op "$cc" -O2 -ffp-contract=off -c "$work/clean_scalar.cpp" -o "$work/clean_scalar.o"
  case "$2" in
    objects)
      check_objects "$work/clean_scalar.o" "$work/clean_avx2.o" || selftest_fail "SIMD self-test: oggetti puliti rifiutati"
      printf '#include <immintrin.h>\n__m256d k(__m256d a, __m256d b, __m256d c) { return _mm256_add_pd(_mm256_fmadd_pd(a, b, c), a); }\n' \
        > "$work/fma.cpp" || tool_error "scrittura fallita"
      printf '#include <immintrin.h>\n__m256d k(__m256d a) { return _mm256_cvtps_pd(_mm_rcp_ps(_mm256_cvtpd_ps(_mm256_add_pd(a, a)))); }\n' \
        > "$work/rcp.cpp" || tool_error "scrittura fallita"
      printf '#include <immintrin.h>\n__m256d k(__m256d a, __m128 b, __m128* o) { *o = _mm_rsqrt_ss(b); return _mm256_add_pd(a, a); }\n' \
        > "$work/rsqrt.cpp" || tool_error "scrittura fallita"
      # Ogni sonda ha anche un'operazione f64 ammessa su ymm: il rifiuto viene solo dall'istruzione vietata.
      for probe in fma rcp rsqrt; do
        op "$cc" -O2 -mavx2 -mfma -c "$work/$probe.cpp" -o "$work/$probe.o"
        rc=0; check_objects "$work/clean_scalar.o" "$work/$probe.o" 2> "$work/$probe.err" || rc=$?
        [[ $rc -eq 1 ]] || selftest_fail "SIMD self-test: $probe non rilevato (uscita $rc)"
        gmatch "istruzioni vietate in $work/$probe.o" "$(cat "$work/$probe.err")" ||
          selftest_fail "SIMD self-test: $probe rifiutato per un motivo diverso dall'istruzione vietata"
        if gmatch "nessuna operazione f64" "$(cat "$work/$probe.err")"; then
          selftest_fail "SIMD self-test: la sonda $probe non ha operazioni f64 su ymm"
        fi
      done
      rc=0; ( trap - EXIT; check_objects "$work/assente.o" "$work/clean_avx2.o" ) 2>/dev/null || rc=$?
      [[ $rc -eq 2 ]] || selftest_fail "SIMD self-test: oggetto assente non è tool-error (uscita $rc)"
      rc=0; ( trap - EXIT; objdump_bin=false; check_objects "$work/clean_scalar.o" "$work/clean_avx2.o" ) 2>/dev/null || rc=$?
      [[ $rc -eq 2 ]] || selftest_fail "SIMD self-test: objdump fallito non è tool-error (uscita $rc)"
      rc=0; check_objects "$work/clean_avx2.o" "$work/clean_avx2.o" 2>/dev/null || rc=$?
      [[ $rc -eq 1 ]] || selftest_fail "SIMD self-test: ymm nel kernel scalare non rilevato (uscita $rc)"
      rc=0; check_objects "$work/clean_scalar.o" "$work/clean_scalar.o" 2>/dev/null || rc=$?
      [[ $rc -eq 1 ]] || selftest_fail "SIMD self-test: kernel avx2 senza 4 corsie non rilevato (uscita $rc)"
      echo "self-test ok: vfmadd, rcp, rsqrt, ymm nello scalare e avx2 senza corsie rilevati; oggetto assente e objdump fallito sono tool-error; oggetti puliti accettati"
      ;;
    fastmath)
      printf 'int main() { return 0; }\n' > "$work/main.cpp" || tool_error "scrittura fallita"
      op "$cc" -O2 "$work/main.cpp" -o "$work/clean"
      op "$cc" -O2 -ffast-math "$work/main.cpp" -o "$work/fast"
      printf 'build x: link y\n  LINK_FLAGS = -fno-fast-math -ffp-contract=off\n' > "$work/clean.ninja" || tool_error "scrittura fallita"
      printf 'build x: link y\n  LINK_FLAGS = -Ofast\n' > "$work/fast.ninja" || tool_error "scrittura fallita"
      check_fastmath "$work/clean.ninja" -- "$work/clean" || selftest_fail "SIMD self-test: binario pulito rifiutato"
      rc=0; check_fastmath "$work/clean.ninja" -- "$work/fast" 2>/dev/null || rc=$?
      [[ $rc -eq 1 ]] || selftest_fail "SIMD self-test: set_fast_math non rilevato (uscita $rc)"
      rc=0; check_fastmath "$work/fast.ninja" -- "$work/clean" 2>/dev/null || rc=$?
      [[ $rc -eq 1 ]] || selftest_fail "SIMD self-test: -Ofast nelle opzioni non rilevato (uscita $rc)"
      echo "self-test ok: crtfastmath e -Ofast rilevati; binario e opzioni puliti accettati"
      ;;
    *) tool_error "self-test sconosciuto: $2" ;;
  esac
  exit 0
fi

case "${1:-}" in
  objects) [[ $# -eq 3 ]] || tool_error "uso: $0 objects <kernel-scalare.o> <kernel-avx2.o>"; check_objects "$2" "$3" || found_defect ;;
  fastmath) shift; check_fastmath "$@" || found_defect ;;
  *) tool_error "uso: $0 objects|fastmath|--self-test ..." ;;
esac
