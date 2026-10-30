#pragma once

#include <cstddef>
#include <cstdint>

#include "astro/cpu_dispatch.hpp"
#include "calibration_internal.hpp"

// Kernel dei due percorsi (ADR-07, aggiunte di R5). cpu_scalar sta in kernels_scalar.cpp, compilato senza
// vettorizzazione; cpu_avx2 in kernels_avx2.cpp, con le sole funzioni dei kernel in target("avx2").

namespace astro::kernels {

// Righe [y0, y1) della calibrazione (ADR-16, ADR-17) per le due residenze: ingressi e uscita da `io`, indici relativi
// a io.row0. `scratch` ha almeno row_scratch_bytes() byte.
void calibrate_rows_scalar(const prepared_calibration::impl& p, const calib_detail::kernel_io& io, std::uint32_t y0,
                           std::uint32_t y1, void* scratch);
void calibrate_rows_avx2(const prepared_calibration::impl& p, const calib_detail::kernel_io& io, std::uint32_t y0,
                         std::uint32_t y1, void* scratch);

// Banda di righe del master (C-02.a §6, §6.3, §7). `vals[f]` e `bits[f]`: righe [y0, y0 + rows) del frame canonico f,
// contigue. Uscita e statistiche del flat sono dell'immagine intera (indice (y0 + r) * w + x). Righe disgiunte:
// thread-safe. Un level_bias_estimate o un rapporto non rappresentabile: numeric master.report_nonfinite.
struct master_band {
  std::size_t n = 0;
  std::uint32_t w = 0, y0 = 0, rows = 0;
  const double* const* vals = nullptr;
  const std::uint16_t* const* bits = nullptr;
  bool has_sat = false;
  double sat = 0;
  bool f32_out = true;
  void* out = nullptr;
  void* outv = nullptr;
  std::uint16_t* mask = nullptr;
  bool flat = false;
  double offset = 0, eps = 0;
  double* lbe = nullptr;
  double* ratio = nullptr;
  std::uint8_t* nonpositive = nullptr;
};

void master_band_scalar(const master_band& b);
void master_band_avx2(const master_band& b);

// Somme di pixel_sum, n >= 1.
double sum_scalar(const double* x, std::size_t n);
double sum_avx2(const double* x, std::size_t n);

}  // namespace astro::kernels

namespace astro::cpu_detail {

// Contatore delle chiamate ai kernel del percorso (cpu_test::kernel_calls).
void count_kernel_call(cpu_path path) noexcept;

}  // namespace astro::cpu_detail
