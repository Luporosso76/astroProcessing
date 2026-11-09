#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "astro/memory_pool.hpp"

namespace astro {

class fits_input;
struct fits_plan;
struct input_format;

constexpr std::size_t sha256_chunk_bytes = std::size_t{1} << 20;

// Identità di un input: digest esadecimale minuscolo (64 caratteri) e dimensione. Percorso e mtime non sono identità.
struct content_digest {
  std::string sha256;
  std::uint64_t size_bytes = 0;
  friend bool operator==(const content_digest&, const content_digest&) = default;
};

// SHA-256 incrementale (OpenSSL EVP) con memoria costante.
class sha256_stream {
 public:
  sha256_stream();
  ~sha256_stream();
  sha256_stream(const sha256_stream&) = delete;
  sha256_stream& operator=(const sha256_stream&) = delete;
  void update(std::span<const std::uint8_t> bytes);
  // Una sola volta; dopo finish() update() è vietato.
  content_digest finish();

 private:
  struct state;
  std::unique_ptr<state> state_;
  std::uint64_t size_ = 0;
};

// SHA-256 (OpenSSL EVP) a chunk da 1 MiB; esadecimale minuscolo.
std::string sha256_hex(std::span<const std::uint8_t> bytes);

content_digest digest_of(std::span<const std::uint8_t> bytes);

// true se `text` è un digest SHA-256 esadecimale minuscolo di 64 caratteri.
bool is_sha256_hex(const std::string& text) noexcept;

// Legge l'intero file in un buffer (budget sulla sola dimensione), confronta digest e dimensione con `expected`
// e solo dopo analizza l'header sul buffer verificato (picco contro budget_bytes).
// Diverso: integrity input.digest_mismatch. Il buffer restituito è quello verificato.
std::vector<std::uint8_t> read_verified_fits(const std::string& path, const content_digest& expected,
                                             std::uint64_t budget_bytes);

// Come read_verified_fits, ma il buffer di lettura è prenotato dentro `memory` (budget pool.reservation_exceeded se la
// dimensione del file non entra nel residuo). La prenotazione resta impegnata finché vive il risultato.
struct verified_fits {
  reservation memory;  // dichiarata prima di `bytes`: si rilascia dopo il buffer
  std::vector<std::uint8_t> bytes;
};
verified_fits read_verified_fits(const std::string& path, const content_digest& expected, reservation& memory);

// Lettura verificata degli input dei job (OB-11, docs/contracts/BASE-confini.md): unico punto che confronta digest e
// header con quelli attesi; gli operatori la chiamano e non confrontano digest.

// `found` diverso da `expected` (digest o dimensione): integrity input.digest_mismatch con il percorso.
void require_input_digest(const content_digest& found, const content_digest& expected, const std::string& path);

// Valori di header usati nel piano uguali a quelli del file verificato, con il campo nel contesto: dimensione diversa
// integrity input.size_changed; larghezza, altezza o piani integrity input.geometry_changed; BITPIX, tipo del campione
// (BITPIX e scala), presenza di INVALID o VARIANCE integrity input.header_changed.
void require_planned_header(const fits_plan& planned, const fits_plan& verified, const std::string& path);

// Intero `input`, già aperto (e letto per il piano `planned`, se il formato ne ha uno), entro budget_bytes, prima che
// un campione sia decodificato: `path` indica ancora il file aperto (fits_detail::check_path_names), digest verificato,
// header dei byte verificati uguale a `planned`, file invariato dall'apertura (quindi ogni header letto per il piano è
// del file verificato). `planned` è presente se e solo se il formato dell'input ha il piano degli header; senza piano
// nessuna lettura precede la lettura intera. Il buffer restituito è l'unico da passare alla decodifica.
std::vector<std::uint8_t> read_verified_input(const fits_input& input, const std::string& path,
                                              const content_digest& expected, std::uint64_t budget_bytes,
                                              const fits_plan* planned);

// Digest dell'input aperto a pezzi di `work` (non vuoto), con `check_cancel`, se presente, prima di ogni pezzo.
content_digest digest_input(const fits_input& input, std::span<std::byte> work, const std::function<void()>& check_cancel);

// Riverifica prima della pubblicazione: `path` riaperto con `format`, il formato dichiarato del ruolo (io
// input.missing, limite e codici di quel formato), ha la dimensione attesa (integrity input.size_changed) e il digest
// atteso, letto a pezzi di `work`, ed è invariato durante la lettura; poi `opened`, se presente (aperto con lo stesso
// `format`), è invariato dall'apertura (integrity input.modified, input.size_changed). Restituisce (st_dev, st_ino) del
// descrittore verificato, letti con fstat sullo stesso descrittore.
std::pair<std::uint64_t, std::uint64_t> reverify_input(const std::string& path, const input_format& format,
                                                       const fits_input* opened, const content_digest& expected,
                                                       std::span<std::byte> work,
                                                       const std::function<void()>& check_cancel);

}  // namespace astro
