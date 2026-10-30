#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "astro/fits.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"

namespace astro {

// Righe [row_begin, row_end) decodificate di un ingresso, come le produce decode_fits: campioni del tipo decodificato,
// maschera, varianza (solo se letta). Puntatori a buffer del chiamante.
struct row_band {
  std::uint32_t row_begin = 0;
  std::uint32_t row_end = 0;
  void* samples = nullptr;
  std::uint16_t* mask = nullptr;
  void* variance = nullptr;
};

// Ingresso FITS non compresso letto a blocchi di `tile_rows` righe (ADR-06, estensione R3.2). Ogni byte usato e'
// verificato contro il digest del record prima dell'uso.
class fits_block_source {
 public:
  // Apertura e analisi degli header con pread (stessi limiti ed errori di plan_fits). Nessun campione letto.
  fits_block_source(const std::string& path, const content_digest& expected, const fits_read_options& options,
                    std::uint32_t tile_rows, bool read_variance);
  fits_block_source(fits_block_source&&) noexcept;
  fits_block_source& operator=(fits_block_source&&) noexcept;
  ~fits_block_source();

  // Passata P0 sull'intero file in ordine, con `work` come buffer (almeno 8 byte): digest e dimensione
  // (integrity input.digest_mismatch), header analizzati (integrity input.modified), CHECKSUM/DATASUM (integrity
  // fits.checksum), validazione dei campioni come decode_fits; registra il digest di ogni blocco di ogni data unit letto.
  // `check_cancel`, se presente, e' chiamato prima di ogni lettura e ferma la passata lanciando.
  void verify(std::span<std::byte> work, const std::function<void()>& check_cancel = {});

  // Immagine di soli header, come decode_fits senza campioni (samples, maschera e varianza vuoti).
  const image& header() const noexcept;
  std::uint32_t tile_rows() const noexcept;
  std::uint64_t tile_count() const noexcept;
  bool has_invalid() const noexcept;
  bool reads_variance() const noexcept;
  // Byte grezzi per riga dei data unit letti: |BITPIX|/8 + 2 (INVALID) + E (VARIANCE letta).
  std::uint64_t raw_bytes_per_row() const noexcept;
  // Byte decodificati per riga: E + 2 + E (VARIANCE letta).
  std::uint64_t band_bytes_per_row() const noexcept;
  // Data unit letti: 1 + INVALID + VARIANCE letta.
  std::uint64_t region_count() const noexcept;

  // Legge il blocco `tile` (dopo verify): pread, SHA-256 uguale a quello di P0 (integrity input.consumed_digest),
  // decodifica in `out` (row_begin/row_end impostati dal metodo). `raw` ha almeno righe·raw_bytes_per_row byte.
  void read_tile(std::uint64_t tile, std::span<std::byte> raw, row_band& out) const;

  // Prima della pubblicazione: il percorso indica lo stesso file aperto e il file e' invariato (io input.missing,
  // integrity input.modified).
  void check_identity() const;

  struct impl;

 private:
  std::unique_ptr<impl> impl_;
};

// |BITPIX|/8 del primario dai soli header (pianificatore dei blocchi).
std::uint64_t primary_sample_bytes(const fits_input& input);

// FITS di uscita scritto in streaming, byte per byte uguale a encode_fits della stessa immagine. `header_only` ha
// geometria, tipo, metadati e card dell'uscita (campioni non usati); gli header sono quelli di encode_fits di un modello
// di una riga, con NAXIS2 = altezza e INVALID presente se `with_invalid`. Solo l'encoding in virgola mobile identita' del
// tipo dei campioni (o nessun encoding), perche' lo staging contiene float o double: altrimenti invalid_input fits.encoding.
class fits_stream_writer {
 public:
  fits_stream_writer(const image& header_only, bool with_invalid);

  std::uint64_t file_bytes() const noexcept;

  // Dati big-endian di uscita in uno staging contiguo: campioni [0, S·E), maschera (bits ^ 0x8000) [S·E, S·E + 2S),
  // varianza [S·E + 2S, S·(2E + 2)). `read(offset, dst)` legge dallo staging. `sink` riceve i byte del file in ordine.
  using staging_reader = std::function<void(std::uint64_t offset, std::span<std::byte> dst)>;
  // Lettore sequenziale dei byte del file: ogni chiamata riempie `out` per quanto possibile e restituisce i byte
  // scritti, 0 alla fine. Il writer deve sopravvivere al lettore.
  std::function<std::size_t(std::span<std::uint8_t>)> reader(staging_reader read) const;

 private:
  std::uint64_t data_bytes(std::size_t hdu) const noexcept;
  std::uint64_t padded(std::size_t hdu) const noexcept;
  std::uint64_t staging_offset(std::size_t hdu) const noexcept;

  std::uint64_t samples_ = 0;
  std::uint64_t elem_ = 0;
  std::uint64_t file_bytes_ = 0;
  bool with_invalid_ = false;
  std::vector<std::vector<std::uint8_t>> headers_;  // uno per HDU, gia' imbottiti
};

}  // namespace astro
