#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

export TMPDIR="$root/tmp"
export CCACHE_DIR="$root/tmp/cache/ccache"
mkdir -p "$TMPDIR" "$CCACHE_DIR"

# Il PC è condiviso: build e test usano al massimo il 70% delle CPU; un valore richiesto più alto si rifiuta.
# La build parte dal tetto; ctest senza CTEST_PARALLEL_LEVEL resta seriale.
cap=$(( $(nproc) * 7 / 10 ))
cap=$(( cap > 0 ? cap : 1 ))
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-$cap}"
for var in CMAKE_BUILD_PARALLEL_LEVEL CTEST_PARALLEL_LEVEL; do
  [ -z "${!var+set}" ] && continue
  if ! [[ "${!var}" =~ ^[1-9][0-9]*$ ]] || [ "${!var}" -gt "$cap" ]; then
    echo "$var=${!var} non valido: intero fra 1 e $cap (70% di $(nproc) CPU)" >&2
    exit 2
  fi
done

cmake --preset dev
cmake --build --preset dev
ctest --preset dev

# Ogni test deve essere `fast` o `slow`: un test senza etichetta non girerebbe mai nel ciclo dei preset.
total=$(ctest --test-dir build/dev -N | sed -n 's/^Total Tests: //p')
fast=$(ctest --preset dev -N | sed -n 's/^Total Tests: //p')
slow=$(ctest --test-dir build/dev -L slow -N | sed -n 's/^Total Tests: //p')
if [ "$total" -ne "$((fast + slow))" ]; then
  echo "test senza etichetta fast/slow: totale $total, fast $fast, slow $slow" >&2
  exit 1
fi
