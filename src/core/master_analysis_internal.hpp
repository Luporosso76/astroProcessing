#pragma once

#include "astro/error.hpp"

namespace astro::master_detail {

// Classificazione di uno stop del decoder RAW nel lettore delle statistiche dei master (C-02.b §b.2, L2), funzione pura
// del codice, della categoria e del contesto emessi dal decoder. Vero = esito del file (`decode_failure`): un limite o un
// difetto proprio del contenuto, uguale su ogni macchina. Falso = stop: categoria `cancelled` (`raw.cancelled`) o `io`
// (lettura, codici dell'apertura col prefisso del formato), codice fuori da `raw.*`, `raw.decode_context`,
// `raw.libraw_error` (rc sconosciuto) e la memoria negata dal sistema.
// `raw.too_big` è esito solo se il contesto lo attribuisce al file: `rifiuto` = tetto (tetto di zlib del decoder del
// progetto) oppure `valore` = rc di `LIBRAW_TOO_BIG`; con `rifiuto` = sistema, rc di `LIBRAW_UNSUFFICIENT_MEMORY` o
// `LIBRAW_MEMPOOL_OVERFLOW`, o un contesto che non lo dice, è stop.
bool raw_decode_error_is_file_outcome(const stop_error& e);

}  // namespace astro::master_detail
