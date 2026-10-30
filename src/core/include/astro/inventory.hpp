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

// Memoria delle letture: il pool, con reserve_or_stop, per le chiamate fuori da un job; la prenotazione di un job dello
// scheduler, gia' presa prima del primo task, con sotto-prenotazioni (oltre il residuo: budget
// pool.reservation_exceeded).
class io_memory {
 public:
  io_memory(memory_pool& pool) noexcept : pool_(&pool) {}
  io_memory(reservation& job) noexcept : job_(&job) {}
  reservation take(std::uint64_t bytes, const char* label) const;
  // Budget del pool, oppure byte non impegnati della prenotazione del job.
  std::uint64_t budget_bytes() const noexcept;

 private:
  memory_pool* pool_ = nullptr;
  reservation* job_ = nullptr;
};

// Annullamento richiesto: cancelled sched.cancelled. `cancel` nullo: nessun controllo.
void check_read_cancel(const cancel_token* cancel);

// Prenotazione di inventario: 4096 + min(file_bytes / 80, max_hdus * max_header_cards) * 128.
std::uint64_t inventory_reserve_bytes(std::uint64_t file_bytes);

// Prenota inventory_reserve_bytes in `memory` (dal pool senza attesa; sotto il minimo: budget
// sched.job_exceeds_budget con `minimo` e `budget`), poi legge con pread i soli header, controllando `cancel` prima di
// ogni lettura.
fits_inventory inventory_fits(const fits_input& input, const fits_read_options& options, io_memory memory,
                              const cancel_token* cancel = nullptr);

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
fits_preview make_preview(const fits_input& input, const preview_request& request, io_memory memory,
                          const cancel_token* cancel = nullptr);

// Byte che make_preview prenota per `request` e campioni dell'anteprima, dai soli header di `input` (nessuna
// prenotazione). Blocco fuori intervallo: invalid_input preview.block.
struct preview_size {
  std::uint64_t reserve_bytes = 0;
  std::uint64_t samples = 0;
};
preview_size preview_size_of(const fits_input& input, const preview_request& request);

// Byte del blocco di lettura di digest_fits_input per un file di `file_bytes` byte.
std::uint64_t digest_reserve_bytes(std::uint64_t file_bytes);

// Digest SHA-256 dell'intero file in streaming (memoria costante: blocchi da 1 MiB, prenotati in `memory`), con
// `cancel` controllato prima di ogni blocco.
content_digest digest_fits_input(const fits_input& input, io_memory memory, const cancel_token* cancel = nullptr);

}  // namespace astro
