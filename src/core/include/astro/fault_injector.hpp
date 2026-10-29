#pragma once

#include <functional>
#include <string_view>

namespace astro {

// Punto di iniezione dei guasti: chiamato prima di ogni passo del protocollo con il nome del punto; un valore diverso
// da zero e' l'errno che il passo produce (io), senza eseguire la chiamata di sistema.
using fault_injector = std::function<int(std::string_view point)>;

// Osservatore della scrittura atomica degli artefatti (solo test di perdita di energia, ADR-04): chiamato dopo che il
// passo e' riuscito, con l'evento (create, fsync, rename, unlink, fsync_dir) e i nomi relativi alla directory.
// Per `rename` `other` e' il nome finale; negli altri eventi e' vuoto (per fsync_dir anche `name`).
using fs_observer = std::function<void(std::string_view event, std::string_view name, std::string_view other)>;

}  // namespace astro
