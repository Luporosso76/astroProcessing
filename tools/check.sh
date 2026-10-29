#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

export TMPDIR="$root/tmp"
export CCACHE_DIR="$root/tmp/cache/ccache"
mkdir -p "$TMPDIR" "$CCACHE_DIR"

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
