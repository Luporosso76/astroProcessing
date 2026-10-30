#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "astro/fault_injector.hpp"
#include "astro/hash.hpp"
#include "astro/inventory.hpp"

namespace astro {

constexpr std::uint32_t thumbnail_cache_algorithm_version = 1;

struct cache_options {
  // Obbligatoria e positiva: somma massima delle dimensioni delle voci. Nessun default.
  std::uint64_t quota_bytes = 0;
  // Solo test: punti `cache.entry.*` della scrittura atomica.
  fault_injector fault;
};

enum class cache_outcome { cache_hit, computed, rebuilt };
const char* to_string(cache_outcome outcome) noexcept;

struct thumbnail_entry {
  std::string key;
  // `<chiave>.<digest>.thm`
  std::string name;
  content_digest digest;
  cache_outcome outcome = cache_outcome::computed;
};

struct thumbnail_result {
  thumbnail_entry entry;
  fits_preview preview;
};

// Cache derivata `<progetto>/cache/thumbnails/` (ADR-04, R1). Le operazioni usano un fd di directory aperto con
// O_NOFOLLOW; il contenuto di ogni voce si verifica a ogni lettura.
class thumbnail_cache {
 public:
  // Crea `cache/thumbnails` sotto il progetto se mancano. Symlink o non directory: security.
  thumbnail_cache(const std::filesystem::path& project_root, const cache_options& options);
  thumbnail_cache(const thumbnail_cache&) = delete;
  thumbnail_cache& operator=(const thumbnail_cache&) = delete;
  ~thumbnail_cache();

  static std::string key_of(const content_digest& source, const preview_request& request);

  // Voce valida: cache_hit (nessun calcolo). Assente: calcola, scrive, computed. Presente ma corrotta: integrity
  // cache.corrupt, nessuna modifica. `source` e' il digest gia' verificato del file.
  thumbnail_result get(const fits_input& input, const content_digest& source, const preview_request& request,
                       io_memory memory, const cancel_token* cancel = nullptr);

  // Sostituisce la voce presente (anche corrotta) con quella ricalcolata: rebuilt. Nessuna voce: invalid_input
  // cache.rebuild_nothing. `replaced` riceve nome e digest trovato della voce sostituita.
  thumbnail_result rebuild(const fits_input& input, const content_digest& source, const preview_request& request,
                           io_memory memory, const cancel_token* cancel, std::string& replaced_name,
                           std::string& replaced_found_digest);

  // Picco di memoria di get e rebuild con voci valide per `request`: il massimo fra il calcolo dell'anteprima e la
  // voce con l'anteprima (lettura o serializzazione). Legge i soli header di `input`. Una voce presente di dimensione
  // diversa da quella valida puo' superarlo: dentro un job dello scheduler, budget pool.reservation_exceeded.
  static std::uint64_t peak_bytes(const fits_input& input, const preview_request& request);

  // Elimina i soli `.tmp-*` regolari con st_nlink = 1; symlink o hardlink: security, nulla viene toccato.
  std::vector<std::string> cleanup_temporaries();

  // Esecuzioni del calcolo dell'anteprima da quando esiste l'oggetto.
  std::uint64_t computations() const noexcept { return computations_; }

 private:
  struct impl;
  std::unique_ptr<impl> impl_;
  std::uint64_t computations_ = 0;
};

}  // namespace astro
