#pragma once

// Decoder DNG del progetto (docs/contracts/U13-dng-decoder.md): interfaccia interna fra `decode_raw_buffer` e il parser.

#include <cstdint>
#include <span>

#include "astro/image.hpp"
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

}  // namespace astro::dng_detail
