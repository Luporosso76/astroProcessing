# M2-R0b: disco pieno sull'export (T-M2R0-03)

Stato: **ESEGUITO, in attesa di review indipendente**, 29 ottobre 2026. Autore: agente executor (Claude Sonnet 5.5). Piano: `plans/M2-residui.md`, fase R0.b. Contratto: [ADR-04](../contracts/ADR-04-persistenza.md), sezione «Aggiunte di R0.b e R1».

## Stato generato

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-04-persistenza.md` | `6681f95533f13d72b158069994c9a7e71e8e948b3af409e40cfd76c1d3b29a7a` |
| `src/core/project_export.cpp` | `6b67fa9bb3fdd89b747e55d0faec1106a503f79ff8ea9ad2d10c8906080c7d00` |
| `tests/unit/test_export.cpp` | `c0aea6beac546120af806e9c9c1bbcfa3067f339fcea357ce7e515d4c22b68ff` |

- Test release (`tmp/release-junit.xml`): totali 500, fast 493, slow 7, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 501, fast 494, slow 7, falliti 0
- Tracciabilità (`tests/trace/M2.tsv`, build `build/release`, junit `tmp/release-junit.xml`): trace: 63 ID nello stadio M2, 0 non coperti
<!-- evidence:end -->

## Test eseguiti

| ID | Test | Esito |
|---|---|---|
| T-M2R0-03 | `Export.T_M2R0_03_enospc_at_every_export_write_point_stops_io_and_leaves_no_complete_bundle` | ENOSPC iniettato a ognuno degli N punti dell'export (`export.mkdir`, `export.file.*`, `export.manifest.*`, `export.commit.*`, ottenuti da una prova a vuoto): stop `io`, errno 28 nel contesto, codice `export.write` per tutti i punti, `export.mkdir` compreso (la causa iniettata resta nel contesto); nessun `COMMIT`, nessun `.tmp-*`; `verify_export` dichiara incompleta la destinazione esistente; file di `artifacts/` e slot invariati; un export successivo senza guasto riesce e verifica |

Riesecuzioni di R0.b (ordine della tabella delle fasi di abilitazione): i test registrati in matrice per NFR-SEC-01 (T-M2A-05, 06, 11, 12), F-PRJ-01 (T-M2C-01), F-PRJ-03 (T-M2B-03, 04, 05, 09), F-PRJ-07 (T-M2B-10, T-M2C-19, 20, 21), F-PRJ-02 (T-M2C-11), F-PRJ-04 (T-M2C-13…18), F-RES-01 (T-M2D-03), F-RES-02 (T-M2D-01, 02) e NFR-TEST-01.a (T-M2F-01) sono stati rieseguiti nell'esecuzione completa dei preset `dev` (fast) e `release` (tutti i test) di questa unità: tutti verdi (conteggi nel blocco generato di `M2-R1.md`). La chiusura resta al gate.

## Modifica al prodotto

`src/core/project_export.cpp`: se la scrittura di `COMMIT` fallisce dopo il rename (punto `export.commit.fsync_dir`), `COMMIT` viene rimosso (`unlinkat`), altrimenti l'export riportato fallito risulterebbe completo per `verify_export`. Un errore di rimozione diverso da ENOENT è aggiunto al contesto dell'arresto originale (`commit_rimozione_errno`). Il file è condiviso con R4: le modifiche sono il ramo di `COMMIT` e il punto `export.mkdir` (rilancio come `export.write`).

## Prova che il test fallisce senza ciò che verifica

Mutazione `tmp/r1/mut/22-export-keeps-commit.patch` (rimozione di `COMMIT` disattivata), log `tmp/r1-mutations-22-export-keeps-commit.log`: il test **fallisce** (RILEVATA).

## Limiti

- Iniezione nel fault injector del prodotto, non filesystem pieno (D13). Un ENOSPC reale su `mkdirat` produce `export.write` nel ramo dopo l'iniezione (non provato, nessun filesystem pieno).
- Il punto `export.commit.fsync_dir` è dopo il rename: lo stato dopo il guasto è «COMMIT rimosso», non «mai scritto». Un `unlinkat` non riuscito lascia un bundle completo con export dichiarato fallito: l'errore è nel contesto, non è corretto.
- Review indipendente: da fare.
