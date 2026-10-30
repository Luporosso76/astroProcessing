#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "astro/fault_injector.hpp"
#include "astro/image.hpp"
#include "astro/input_format.hpp"

namespace astro {

class fits_input;

namespace fits_detail {
void read_exact(const fits_input& input, std::uint64_t offset, void* dst, std::size_t n);
// Confronta dimensione, mtime e ctime del file aperto con quelli dell'apertura: dimensione diversa =
// integrity input.size_changed; mtime o ctime diversi = integrity input.modified; fstat fallita io <prefisso>.fstat.
void check_unchanged(const fits_input& input);
// (st_dev, st_ino) del file aperto.
std::pair<std::uint64_t, std::uint64_t> identity_of(const fits_input& input);
// `path` indica il file aperto: assente io input.missing, non accessibile io <prefisso>.fstat, un altro file integrity
// input.modified.
void check_path_names(const std::string& path, const fits_input& input);
}

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
  int bitpix = 0;  // BITPIX del primario: con `type` fissa i byte grezzi per campione
  bool has_invalid = false;
  bool has_variance = false;
  // file + campioni + maschera (2 byte per campione) + varianza se presente.
  std::uint64_t peak_bytes = 0;
};

// Input aperto in sola lettura da open_input, con dimensione fissata da fstat sullo stesso fd e con la dichiarazione
// del formato di apertura: i codici propri del formato (`.fstat`, `.read`) usano il suo prefisso.
class fits_input {
 public:
  fits_input(const fits_input&) = delete;
  fits_input& operator=(const fits_input&) = delete;
  fits_input(fits_input&& other) noexcept;
  fits_input& operator=(fits_input&& other) noexcept;
  ~fits_input();

  std::uint64_t size_bytes() const noexcept { return size_; }
  const input_format& format() const noexcept { return *format_; }

  // Solo test: il punto "input.pread" e' chiamato prima di ogni pread; un valore diverso da zero fa restituire 0 byte
  // (fine file) alla lettura, con la dimensione del file invariata.
  void set_fault_injector(fault_injector injector) { fault_ = std::move(injector); }

  // Solo test: chiamato con offset e lunghezza di ogni lettura prima del pread; un valore diverso da zero e' l'errno
  // dell'errore di I/O che la lettura produce (io, <prefisso>.read), senza eseguire il pread.
  using pread_observer = std::function<int(std::uint64_t offset, std::size_t length)>;
  void set_pread_observer(pread_observer observer) { observer_ = std::move(observer); }

 private:
  friend fits_input open_input(const std::string& path, const input_format& format);
  friend fits_plan plan_fits(const fits_input& input);
  friend std::vector<std::uint8_t> read_input_bytes(const fits_input& input, std::uint64_t budget_bytes);
  friend std::vector<std::uint8_t> read_fits_bytes(const fits_input& input, std::uint64_t budget_bytes);
  friend void fits_detail::read_exact(const fits_input& input, std::uint64_t offset, void* dst, std::size_t n);
  friend void fits_detail::check_unchanged(const fits_input& input);
  friend std::pair<std::uint64_t, std::uint64_t> fits_detail::identity_of(const fits_input& input);
  fits_input(int fd, std::uint64_t size, std::int64_t mtime_ns, std::int64_t ctime_ns,
             const input_format* format) noexcept
      : fd_(fd), size_(size), mtime_ns_(mtime_ns), ctime_ns_(ctime_ns), format_(format) {}

  int fd_ = -1;
  std::uint64_t size_ = 0;
  std::int64_t mtime_ns_ = 0;
  std::int64_t ctime_ns_ = 0;
  const input_format* format_ = nullptr;
  fault_injector fault_;
  pread_observer observer_;
};

// Protocollo di apertura unico degli input, nel formato dichiarato dal chiamante (BASE §2.5, emendamento L1): open
// O_RDONLY, fstat sullo stesso descrittore, file regolare, limite del formato prima di ogni lettura e allocazione.
// Assente: io input.missing; altro errore di open: io <prefisso>.open; fstat: io <prefisso>.fstat; non regolare:
// invalid_input <prefisso>.not_regular; oltre il limite: limit_exceeded <prefisso>.max_file_bytes (dimensione, limite).
fits_input open_input(const std::string& path, const input_format& format);

// open_input con la dichiarazione FITS.
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
  // Fase e linearita' dichiarate all'import (ADR-03, R1): in conflitto con l'header = stop.
  std::optional<process_phase> declared_phase;
  std::optional<linearity_class> declared_linearity;
};

// Fase e linearita' dai soli header (`ASTRPHAS`, `LINEAR` o `options`), con gli stessi limiti e senza leggere campioni.
// Assenti: nullopt. Conflitto con le dichiarazioni: incompatible metadata.declared_conflict.
struct header_peek {
  std::optional<metadata_value<process_phase>> phase;
  std::optional<linearity> lin;
};
header_peek peek_header(const fits_input& input, const fits_read_options& options);

// Decode da buffer in memoria (CFITSIO fits_open_memfile). Rifa' l'analisi sul buffer e controlla il picco
// contro budget_bytes (budget: sched.job_exceeds_budget) prima di allocare i campioni.
image decode_fits(std::span<const std::uint8_t> bytes, std::uint64_t budget_bytes,
                  const fits_read_options& options = {});

// Scrive in memoria (CFITSIO fits_create_memfile). Nessuna API su percorso.
std::vector<std::uint8_t> encode_fits(const image& img);

// Lettura a fasce di righe del primario e di INVALID con pread sullo stesso descrittore (C-02.a, aggiunte di R2).
// Valori e bit uguali a quelli di decode_fits sullo stesso contenuto: campioni convertiti come decode_fits al tipo dei
// campioni e poi a double, bit di INVALID, NaN e infiniti senza bit marcati source_nonfinite. Le verifiche di
// struttura, checksum e scala sono quelle di decode_fits sull'intero file, che il chiamante esegue prima.
struct fits_row_layout {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  int bitpix = 0;
  double bscale = 1.0;
  double bzero = 0.0;
  sample_type type = sample_type::f32;
  std::uint64_t data_offset = 0;
  std::optional<std::uint64_t> invalid_offset;
};

// Solo header, con i limiti di plan_fits. Piu' di un piano: unsupported fits.row_planes.
fits_row_layout plan_fits_rows(const fits_input& input);

// Byte di appoggio richiesti da read_fits_rows per `rows` righe.
std::uint64_t fits_row_scratch_bytes(const fits_row_layout& layout, std::uint32_t rows);

// Righe [y0, y0 + rows): `values` e `bits` hanno rows * width elementi, `scratch` fits_row_scratch_bytes byte.
// Bit riservati in INVALID: invalid_input fits.invalid_reserved_bits.
void read_fits_rows(const fits_input& input, const fits_row_layout& layout, std::uint32_t y0, std::uint32_t rows,
                    std::byte* scratch, double* values, std::uint16_t* bits);

}  // namespace astro
