#pragma once

// Decoder DNG del progetto (docs/contracts/U13-dng-decoder.md): interfaccia interna fra `decode_raw_buffer` e il parser.

#include <array>
#include <cstdint>
#include <optional>
#include <span>

#include "astro/image.hpp"
#include "astro/memory_pool.hpp"
#include "astro/raw_decode.hpp"

namespace astro::dng_detail {

// Esito dell'instradamento con le posizioni che il decoder del progetto rilegge.
struct route_detail {
  dng_route_result result;
  bool little_endian = true;
  std::uint32_t ifd0 = 0;     // offset della tabella di IFD0 (DNGVersion, DNGBackwardVersion)
  std::uint32_t raw_ifd = 0;  // offset della tabella dell'IFD raw scelto; valido solo con astro_dng
};

// U13 §3. Funzione pura sui byte.
route_detail route(std::span<const std::uint8_t> bytes);

// U13 §4-§7 sul DNG che `route` ha mandato al decoder del progetto. Ogni stop porta nel contesto il decoder e i campi
// dell'instradamento (§8).
image decode(std::span<const std::uint8_t> bytes, const route_detail& route, const raw_decode_hooks& hooks,
             const raw_decode_context& ctx);

// `needed` di §7 dagli stessi controlli di `decode` prima della prenotazione (tag, segmenti, limiti), senza campioni.
std::uint64_t needed_bytes(std::span<const std::uint8_t> bytes, const route_detail& route, std::uint64_t max_pixels);

// ADR-08.b §2.8: pool e byte prenotati dal chiamante insieme, o `held_bytes` senza pool: invalid_input
// raw.decode_context, prima di ogni lettura.
void check_decode_mode(const raw_decode_context& ctx);

// Prenotazione di §2.8 nel punto che precede ogni allocazione della decodifica. Modo del chiamante: `needed` oltre i byte
// forniti è budget sched.job_exceeds_budget (`minimo`, `forniti`), nessuna prenotazione. Modo pool: prenotazione nel pool
// dopo il confronto col budget. Senza pool né byte forniti: nessuna prenotazione.
reservation reserve_decode(std::size_t file_bytes, std::uint64_t needed, const raw_decode_context& ctx);

// Curva di linearizzazione di 65 536 voci (ADR-08.b §2.4): assente se è l'identità, altrimenti SHA-256 delle voci come
// uint16 little-endian in ordine di indice. Stessa regola per i due decoder.
std::optional<std::array<std::uint8_t, 32>> curve_digest(std::span<const std::uint16_t, 0x10000> curve);

}  // namespace astro::dng_detail
