# GoogleTest 1.17.0

Copia della sola componente `googletest` (senza googlemock, documentazione, esempi e test interni) dal pacchetto Ubuntu `googletest` 1.17.0-1build1, percorso `/usr/src/googletest/googletest`. Licenza BSD-3-Clause in `LICENSE`. Uso: solo test, mai nel prodotto. Hash dei file in `SHA256SUMS` (`sha256sum -c SHA256SUMS` dalla directory). Non si modifica, salvo la modifica locale sotto: un aggiornamento sostituisce l'intera copia, riapplica la modifica e rigenera gli hash.

## Modifica locale

`src/gtest-port.cc`, costruttore di `CapturedStream` (Linux): il file temporaneo della cattura di stdout/stderr (usata anche dai death test) nasce in `TempDir()`, cioè in `TEST_TMPDIR`/`TMPDIR`, al posto di `"/tmp/"` fisso. I preset di test portano quelle variabili in `build/<preset>/test_tmp/sys`: nessun temporaneo dei test in `/tmp`. SHA-256 originale del file: `5efb91d426e321925787a13a5915c082db6c237eda42f288bacf86a1d1ae601f`.
