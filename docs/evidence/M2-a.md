# M2.a: immagine tipizzata e FITS

Stato: **DONE**, 28 ottobre 2026. Autore: agente executor su Claude Sonnet 5.5. Review indipendente: agente code-reviewer su Claude Opus 5.5, due verifiche. Ambiente: Linux 7.0.0-38-generic x86_64, GCC 15.2.0, CFITSIO 4.6.3, GoogleTest 1.17.0 (`third_party`).

## Consegna

- `src/core/{error,image,fits,fits_scan,fits_header}.cpp`, `fits_internal.hpp`, header pubblici `astro/{error,image,fits}.hpp`: modello d'errore di ADR-05, tipi di ADR-03, lettura e scrittura FITS solo da memoria con preflight su `fstat`/`pread` dello stesso fd e controllo di budget ripetuto sul buffer letto.
- `tests/fixtures/syn_fits.hpp`: `FIX-SYN-FITS-01`, writer e parser FITS indipendenti (nessun include di `astrocore` o CFITSIO), generatori per indice di light, dark, flat, dark-flat e bias.
- Test: T-M2A-01…13, casi avversari, fuzz per mutazione di byte (6000 file) e strutturale (4000 file) con oracle indipendente sui dati, contatore di allocazione dei campioni per T-M2A-05, lettura di PUB-LFC-01 `ccd.037.0.fits` con SHA-256.

Comando: `tools/check.sh` (dev, ASan+UBSan) e `cmake --preset release && cmake --build --preset release && ctest --preset release`. Esito: dev 77/77, release 76/76 (il test dei sanitizer esiste solo in dev).

## Review

Prima verifica: FAIL, quattro bloccanti riprodotti con programmi in `tmp/review_m2a/`: negazione di INT64_MIN (UB), keyword strutturali duplicate interpretate diversamente da scanner e CFITSIO (dati letti dal padding dell'header), scale BSCALE/BZERO non invertibili riscritte come zeri senza errore, scala ignorata sull'HDU VARIANCE. Più MED su coercizione di tipo delle keyword, arrotondamento silenzioso, perdita di `lin` e metadati dichiarati, picco di memoria e TOCTOU, regola `source_nonfinite`, offset Bayer non dichiarabili, fuzz non strutturale.

Seconda verifica: PASS; probe ripetuti con esito di stop atteso. Residui chiusi: fuzz strutturale con decode ed encode separati e oracle indipendente (ha trovato un ulteriore difetto, PCOUNT/GCOUNT non standard nel primario che spostavano i dati in CFITSIO, ora `unsupported`), chiave di `fits.decode` con le opzioni di lettura (T-M2B-13), test di `verify_structure` per iniezione, `static_assert` su `aligned_buffer::as<T>()`, scale non dyadiche accettate e riscritte bit-esatte.

## Limiti

- `io input.short_read` non testato (richiede un reader iniettabile): riga NON ESEGUITO in `docs/test-matrix.md`.
- Budget come intero semplice fino al `job_plan` di M2.d; digest del buffer in M2.b.
- Nessun dato CFA reale.
