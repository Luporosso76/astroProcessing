#pragma once

#include <cstdint>
#include <vector>

namespace astro {

// Formato d'ingresso dichiarato da chi apre l'input (docs/contracts/BASE-confini.md §2.5, emendamento L1), mai dedotto
// da byte, estensione o da un secondo tentativo. Una costante del core per formato.
struct input_format {
  const char* prefix;            // prefisso dei codici propri del formato: `<prefisso>.open`, `.fstat`, `.read`, ...
  std::uint64_t max_file_bytes;  // limite applicato dall'apertura, prima di ogni lettura e allocazione
  // Vero: gli header si leggono prima del digest per il piano e si riconfrontano sui byte verificati. Falso: nessun
  // byte del file si interpreta prima del digest; il solo valore usato prima e' la dimensione da fstat.
  bool header_plan;
};

namespace raw_limits {
// ADR-08.b §2.1: stima non misurata.
constexpr std::uint64_t max_file_bytes = 1073741824;
constexpr std::uint64_t max_pixels = 268435456;
}  // namespace raw_limits

const input_format& fits_input_format();
const input_format& raw_input_format();

// Elenco delle dichiarazioni, una per formato.
const std::vector<const input_format*>& input_formats();

}  // namespace astro
