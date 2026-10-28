# M2.f: build, test e fixture

Stato: **DONE** (generatori di fixture consegnati con M2.a). 28 ottobre 2026. Ambiente: Linux 7.0.0-38-generic x86_64, GCC 15.2.0, CMake 3.x di distribuzione, Ninja, ccache con cache in `tmp/cache/ccache`.

## Eseguito

- Albero CMake con libreria statica `astrocore`, preset `dev` (Debug, `-fsanitize=address,undefined -fno-sanitize-recover=all`) e `release` (RelWithDebInfo). Comando: `tools/check.sh`.
- GoogleTest 1.17.0 copiato in `third_party/googletest` (provenienza e SHA-256 dei 44 file sorgente in `PROVENANCE.md` e `SHA256SUMS`, verificati); nessun include o libreria da `/usr/local` o `/usr/src` nei comandi di compilazione.
- Test: `dev` 13/13 (6 build info, test che fallisce senza sanitizer verificato per mutazione, 5 dipendenze reali con vettore NIST SHA-256, SQLite in memoria, versione CFITSIO, FFTW f32/f64; T-M2F-01 offline). `release` 12/12 (test sanitizer non compilato).
- T-M2F-01: `tools/check_offline.sh` su `libastrocore` e sugli oggetti dei test. Ha trovato lo `StreamingListener` su socket del runtime di GoogleTest; il runtime è escluso con motivazione in ADR-13, il codice del progetto resta controllato. Mutazione: un oggetto che chiama `socket` viene rifiutato con exit 1.
- Inventario licenze: `docs/licenses.md`, tutte compatibili con GPL-3.0-or-later.
- Dato reale: `docs/datasets/PUB-LFC-01.md`, Zenodo 10.5281/zenodo.3254683, CC BY 4.0, MD5 pubblicato verificato, SHA-256 di 24 FITS registrati.

## Completamento

Generatori `FIX-SYN-FITS-01` in `tests/fixtures/syn_fits.hpp`, verificati in `docs/evidence/M2-a.md`. T-M2F-02 (`strace` sul workflow M2.e) si esegue con M2.e.

## Limiti

Nessun dataset CFA reale ridistribuibile trovato: il debayer di M3.d resterà senza dato reale finché non se ne trova uno.
