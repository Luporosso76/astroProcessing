#pragma once

#include <optional>
#include <string>

namespace astro {

struct BuildInfo {
  std::string version;
  std::string compiler;
  std::string build_type;
  std::string flags;
  long cplusplus;
  // Identificatore del sorgente (commit git di 40 esadecimali o valore passato a -DASTRO_SOURCE_REVISION); mai vuoto.
  std::string revision;
  // true se al build i percorsi sorgente avevano modifiche non committate; nullopt se la revisione e' stata
  // passata esplicitamente (stato non verificabile).
  std::optional<bool> dirty;
};

BuildInfo build_info();

}  // namespace astro
