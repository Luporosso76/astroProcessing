#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "astro/image.hpp"

namespace astro {

namespace fits_limits {
constexpr std::uint32_t max_naxis = 3;
constexpr std::uint32_t max_axis_length = 65536;
constexpr std::uint32_t max_planes = 16;
constexpr std::uint64_t max_samples = std::uint64_t{1} << 28;
constexpr std::uint32_t max_hdus = 8;
constexpr std::uint32_t max_header_cards = 10000;
// campioni f64 + INVALID (2 byte) + VARIANCE f64 (8 byte) = 18 byte per campione, piu' 16 MiB di header.
constexpr std::uint64_t max_file_bytes = max_samples * 18 + (std::uint64_t{16} << 20);
}  // namespace fits_limits

struct fits_plan {
  std::uint64_t file_bytes = 0;
  std::uint64_t samples = 0;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint32_t planes = 0;
  sample_type type = sample_type::f32;
  bool has_invalid = false;
  bool has_variance = false;
  // file + campioni + maschera (2 byte per campione) + varianza se presente.
  std::uint64_t peak_bytes = 0;
};

// File aperto in sola lettura, con dimensione fissata da fstat sullo stesso fd.
class fits_input {
 public:
  fits_input(const fits_input&) = delete;
  fits_input& operator=(const fits_input&) = delete;
  fits_input(fits_input&& other) noexcept;
  fits_input& operator=(fits_input&& other) noexcept;
  ~fits_input();

  std::uint64_t size_bytes() const noexcept { return size_; }

 private:
  friend fits_input open_fits_input(const std::string& path);
  friend fits_plan plan_fits(const fits_input& input);
  friend std::vector<std::uint8_t> read_input_bytes(const fits_input& input, std::uint64_t budget_bytes);
  friend std::vector<std::uint8_t> read_fits_bytes(const fits_input& input, std::uint64_t budget_bytes);
  fits_input(int fd, std::uint64_t size) noexcept : fd_(fd), size_(size) {}

  int fd_ = -1;
  std::uint64_t size_ = 0;
};

// open + fstat. Non regolare: invalid_input fits.not_regular; oltre max_file_bytes: limit_exceeded fits.max_file_bytes.
fits_input open_fits_input(const std::string& path);

// Legge con pread i soli header e applica i limiti prima di qualunque allocazione dei campioni.
fits_plan plan_fits(const fits_input& input);

// Piano sul file in memoria (stessi limiti).
fits_plan plan_fits(std::span<const std::uint8_t> bytes);

// Legge l'intero file senza analizzarne l'header (per verificare il digest prima di interpretarlo). Dimensione oltre
// budget_bytes: budget sched.job_exceeds_budget. Dimensione letta diversa da quella di fstat: integrity input.size_changed.
std::vector<std::uint8_t> read_input_bytes(const fits_input& input, std::uint64_t budget_bytes);

// Prenota il picco contro budget_bytes (budget: sched.job_exceeds_budget), poi legge l'intero file.
// Dimensione letta diversa da quella di fstat: integrity input.size_changed.
std::vector<std::uint8_t> read_fits_bytes(const fits_input& input, std::uint64_t budget_bytes);

// Valori dichiarati dalla richiesta di lettura (ricetta). Gli offset CFA si riferiscono ai dati come
// memorizzati; ROWORDER si registra senza trasformare i dati.
struct fits_read_options {
  std::optional<std::uint8_t> declared_bayer_x_offset;
  std::optional<std::uint8_t> declared_bayer_y_offset;
};

// Decode da buffer in memoria (CFITSIO fits_open_memfile). Rifa' l'analisi sul buffer e controlla il picco
// contro budget_bytes (budget: sched.job_exceeds_budget) prima di allocare i campioni.
image decode_fits(std::span<const std::uint8_t> bytes, std::uint64_t budget_bytes,
                  const fits_read_options& options = {});

// Scrive in memoria (CFITSIO fits_create_memfile). Nessuna API su percorso.
std::vector<std::uint8_t> encode_fits(const image& img);

}  // namespace astro
