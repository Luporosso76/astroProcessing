#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace astro {

constexpr std::size_t sha256_chunk_bytes = std::size_t{1} << 20;

// Identita' di un input: digest esadecimale minuscolo (64 caratteri) e dimensione. Percorso e mtime non sono identita'.
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
  // Una sola volta; dopo finish() update() e' vietato.
  content_digest finish();

 private:
  struct state;
  std::unique_ptr<state> state_;
  std::uint64_t size_ = 0;
};

// SHA-256 (OpenSSL EVP) a chunk da 1 MiB; esadecimale minuscolo.
std::string sha256_hex(std::span<const std::uint8_t> bytes);

content_digest digest_of(std::span<const std::uint8_t> bytes);

// true se `text` e' un digest SHA-256 esadecimale minuscolo di 64 caratteri.
bool is_sha256_hex(const std::string& text) noexcept;

// Legge l'intero file in un buffer (budget sulla sola dimensione), confronta digest e dimensione con `expected`
// e solo dopo analizza l'header sul buffer verificato (picco contro budget_bytes).
// Diverso: integrity input.digest_mismatch. Il buffer restituito e' quello verificato.
std::vector<std::uint8_t> read_verified_fits(const std::string& path, const content_digest& expected,
                                             std::uint64_t budget_bytes);

}  // namespace astro
