#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "astro/cancel.hpp"
#include "astro/fits.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "astro/memory_pool.hpp"

namespace astro {

struct hdu_inventory {
  std::uint32_t index = 0;
  std::string extname;
  int bitpix = 0;
  std::vector<std::uint64_t> axes;
  double bscale = 1.0;
  double bzero = 0.0;
  std::uint64_t header_offset = 0;
  std::uint64_t data_offset = 0;
  std::uint64_t data_bytes = 0;
  std::uint32_t card_count = 0;
};

// Inventario dei soli header (ADR-03, R1): nessun byte di data unit viene letto.
struct fits_inventory {
  std::uint64_t file_bytes = 0;
  std::uint64_t header_bytes_read = 0;
  std::uint64_t reserved_bytes = 0;
  fits_plan plan;
  std::vector<hdu_inventory> hdus;
  // Primario.
  plane_layout layout = plane_layout::mono;
  std::optional<cfa_layout> cfa;
  std::optional<pixel_unit> unit;
  std::optional<linearity> lin;
  linearity_class lin_class = linearity_class::unknown;
  // Origine della linearita': assente quando `lin_class` e' `unknown` e nessuno l'ha dichiarata.
  std::optional<metadata_origin> lin_origin;
  std::optional<metadata_value<process_phase>> phase;
  std::optional<std::uint32_t> binning_x;
  std::optional<std::uint32_t> binning_y;
  frame_metadata metadata;
  std::vector<std::string> header_cards;
  std::vector<std::string> uninterpreted_keywords;
};

// Prenotazione senza attesa di `bytes` nel pool. Oltre il budget: budget sched.job_exceeds_budget con `minimo` e
// `budget`; budget sufficiente ma memoria ora non disponibile: budget sched.pool_unavailable.
reservation reserve_or_stop(memory_pool& pool, std::uint64_t bytes, const char* label);

// Prenotazione di inventario: 4096 + min(file_bytes / 80, max_hdus * max_header_cards) * 128.
std::uint64_t inventory_reserve_bytes(std::uint64_t file_bytes);

// Prenota inventory_reserve_bytes nel pool (senza attesa; sotto il minimo: budget sched.job_exceeds_budget con
// `minimo` e `budget`), poi legge con pread i soli header.
fits_inventory inventory_fits(const fits_input& input, const fits_read_options& options, memory_pool& pool);

struct preview_request {
  std::uint32_t block_width = 0;
  std::uint32_t block_height = 0;
};

struct fits_preview {
  std::uint32_t source_width = 0;
  std::uint32_t source_height = 0;
  std::uint32_t planes = 0;
  std::uint32_t block_width = 0;
  std::uint32_t block_height = 0;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  // Piano per piano, riga per riga; f32 arrotondato dalla media f64 dei campioni finiti del blocco.
  std::vector<float> samples;
  // Campioni finiti sommati in ogni blocco; 0 = blocco senza campioni finiti (campione NaN).
  std::vector<std::uint32_t> finite_counts;
  std::uint64_t reserved_bytes = 0;
  std::uint64_t observed_peak_bytes = 0;
};

// Media per blocco dei campioni del primario, a fasce di righe (ADR-03, R1). Il pool riceve una sola prenotazione.
fits_preview make_preview(const fits_input& input, const preview_request& request, memory_pool& pool,
                          const cancel_token* cancel = nullptr);

// Digest SHA-256 dell'intero file in streaming (memoria costante: blocchi da 1 MiB, prenotati nel pool).
content_digest digest_fits_input(const fits_input& input, memory_pool& pool);

}  // namespace astro
