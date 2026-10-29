#pragma once

#include <functional>
#include <string_view>

namespace astro {

// Punto di iniezione dei guasti: chiamato prima di ogni passo del protocollo con il nome del punto; un valore diverso
// da zero e' l'errno che il passo produce (io), senza eseguire la chiamata di sistema.
using fault_injector = std::function<int(std::string_view point)>;

}  // namespace astro
