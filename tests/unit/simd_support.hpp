#pragma once

#include <cstdlib>
#include <stdexcept>
#include <string>

#include "astro/cpu_dispatch.hpp"

namespace simd_test {

// Percorso delle ricette dei test di calibrazione e master (T-M2R5-01): ASTRO_TEST_SIMD assente o `scalar` dà
// `scalar`, `avx2` dà `avx2`; ogni altro valore è un errore del test.
inline astro::simd_request requested() {
  const char* v = std::getenv("ASTRO_TEST_SIMD");
  if (v == nullptr || std::string(v) == "scalar") return astro::simd_request::scalar;
  if (std::string(v) == "avx2") return astro::simd_request::avx2;
  throw std::runtime_error(std::string("ASTRO_TEST_SIMD non valido: ") + v);
}

// Sottodirectory dei temporanei: le riesecuzioni avx2 non condividono le directory dei test scalari in parallelo.
inline std::string tmp_subdir() { return requested() == astro::simd_request::avx2 ? "simd-avx2" : ""; }

}  // namespace simd_test
