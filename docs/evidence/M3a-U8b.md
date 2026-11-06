# M3.a, unità U8b: F-IO-03, analisi del batch, gruppi, candidati, ricette e verifica della selezione

Stato: **chiusa**. Test: arbitraggio PASS (3 LOW). Codice: review FAIL al giro 1 (5 MED) e al giro 2 (5 MED), arbitraggio del coordinatore (contratto emendato, codice e test aggiunti da agenti distinti), review sul delta PASS (blocco «Review»).

| Campo | Valore |
|---|---|
| Unità | M3.a / U8b (test e codice) |
| Scopo | `make_observation`, `analyze_batch` (in memoria e sullo store), gruppi esatti di light (CL-07), candidato per ruolo (CL-08), analisi G-CAL per gruppo con le regole di U6 e U7 riusate (CL-09), proposta canonica riproducibile (CL-12), selezione congelata e `verify_selection` (CL-11); record del decoder esteso con `raw_width`, `raw_height`, `cfa_color_index` |
| ID | F-IO-03 (parte U8b); T-M3A-04, T-M3A-05 (parte U8b), T-M3A-06, T-M3A-CAL-09 (parte M3.a) |
| Comando | Integrazione: build pulita, `tools/check.sh` e `ctest --preset release` completo (blocco generato). Mutazioni: `tools/mutate.sh` (patch in `tmp/u8b-mut/` e `tmp/u8b-arb-mut/` del worktree) |
| Versioni | GCC 15.2.0, CMake 4.2.3, Ninja, SQLite di sistema |
| Ambiente | Linux 7.0.0-38-generic x86_64, 16 CPU; worktree `.claude/worktrees/api-u8bc` sulla base U7 |
| Autore e data | Test e codice: agenti Claude Sonnet 5.5 (API), distinti. Review: Claude Opus 5.5 (Argon fuori quota). Coordinatore: Claude Opus 5.5. 5 e 6 novembre 2026 |

## Decisioni del coordinatore

- ADR-08.b §2.9: il record del decoder porta `raw_width`, `raw_height` e `cfa_color_index`, così l'analisi del batch legge lo stato completo dal report persistito senza ridecodificare (CL-12).
- I test di U8b dichiarano `normalization`, `q_min`, `q_rel_sigma_max` (regole di U7: il default non inventa dichiarazioni).
- CL-07: `params_missing` per gruppo; `decoder_black_abs_tol_adu` solo se l'analisi del gruppo ha un ingresso con stato del decoder.
- CL-11: la selezione congelata porta ricetta e parametri applicabili, light e candidati dei ruoli usati (digest e dimensione o chiave del nodo), dichiarazioni, stato e identità del decoder dei light RAW; i frame RAW dei master restano al job del master (M3.b).
- CL-12: con le dichiarazioni termiche si leggono solo i campioni del flat e del bias di una catena `flat.bias` valutata, al momento del controllo (caricamento differito).
- CL-05: `import_stopped` prevale su ogni revisione `user` successiva; i duplicati dello stesso digest si confrontano con le dichiarazioni della revisione dell'import.
- Test: rinominata in `descriptor_json` la funzione locale `master_descriptor` del test di U8a (ambigua col tipo `astro::master_descriptor`); tolta una funzione di test inutilizzata che rompeva `-Werror`.

## Mutazioni

Tutte rilevate con `tools/mutate.sh`, salvo dove indicato.

| Gruppo | Patch |
|---|---|
| Codice (`tmp/u8b-mut/`) | `m1` riferimento dei flat idonei, `m2` scelta automatica solo se unico, `m3` confronto con i flat idonei, `m4` `temperature_policy` mancante, `m5` coppia L–D, `m7` flat idoneo, `m8` `exposure_rel_tol` mancante, `m9` candidato scelto, `m10` tolleranza del nero mancante; `m6` non più applicabile dopo l'arbitraggio, sostituita da `m6b` |
| Arbitraggio (`tmp/u8b-arb-mut/`) | `m6b` portata di `params_missing`, `a1-import-stopped`, `a5-lights`, `a5-declarations`, `a5-candidates`, `a6-flat-gate`, `a6-bias-gate`, `a7-sample-type`, `a7-black-pattern`, `a7-slot-node`, `a8-thermal-input-lambda`, `a9-dup-user-declarations` |

## Limiti

- Il motivo di esclusione di un bias incompatibile sia con L–B sia con F–B non è deciso dal contratto e non ha test (si controllano stato e tipo di scelta).
- `coverage` non è nel report di import FITS: nel percorso sullo store non è osservabile.
- Il percorso sullo store per gli slot `master.combine` non è coperto da un test, compreso il caricamento differito dei campioni di un master in slot (serve una fixture con un nodo `master.combine` pubblicato, unità successiva).

## Review

- Test: arbitraggio PASS con 3 LOW.
- Codice, giro 1 FAIL (5 MED: selezione congelata incompleta, letture di campioni oltre CL-12, import fermato riclassificato, `params_missing` e flat idoneo senza test), giro 2 FAIL (5 MED). Arbitraggio: review sul delta `tmp/api/u8bcode-arb-review.json` PASS con 3 LOW (caricamento differito dello store senza test, precedenza di `import_stopped` non scritta, dichiarazioni del duplicato), chiusi.

## Integrazione con U13b (6 novembre 2026)

Unendo U8b e U13b sono emersi difetti che le review separate non potevano vedere: `decoder_state_of_record` non ricostruiva lo stato del record del decoder del progetto (`route`, `dng`); l'identità del pacchetto di decoder mancava nel record del decoder del progetto (ADR-08.b §2.9 emendato: i quattro campi valgono per entrambi i decoder); la selezione congelata non portava `route` e `dng`; `first_state_difference` non li confrontava (un `ASTRO_ASSERT` raggiungibile). Corretti in `.claude/worktrees/opus-int8b13b` da agenti distinti per codice e test (Claude Opus 5.5, API), con mutazioni in `tmp/int-mut/` di quel worktree tutte rilevate; review dell'integrazione `tmp/api/int-u8b13b-review.json` FAIL (2 MED, 3 LOW: rami nuovi senza test) e `tmp/api/int-u8b13b-review2.json` PASS con 1 LOW (copertura parziale dei campi di `dng` nel confronto delle osservazioni, lavoro successivo).

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-08-decoder-pack.md` | `44cc42cda59c6b958f1911a643a07ddc0fe02f7e82aa01885d43b672051a4195` |
| `docs/contracts/F-IO-03-classificazione.md` | `8bfc0f8df2143dbc69babfc8bda69d0b273a663f229b20a12f1252afc74eee1b` |
| `src/core/batch_classification.cpp` | `c80e92738b657bdaf53bd3041ef2af89e533ca029b54f96283e6d290cfa5a446` |
| `src/core/calibration_analysis.cpp` | `bf537426e1c7e07f3563f279b9d98504a4ef3f051aecaf18d8769494ff149966` |
| `src/core/include/astro/batch_classification.hpp` | `3ff40fc084e923fc70ad5c62dfcd60e72b9bada5eb3c928930233d1e8747c98d` |
| `src/core/include/astro/calibration_analysis.hpp` | `479f19a944752ce2682e13c8010c1b60f68a534fff45202affe9bd3cce340eb2` |
| `src/core/raw_decode.cpp` | `e114fbf53cad623be8ab4a4c4fd952fc33227f29e171e8788259f5e0b493029e` |
| `tests/unit/raw_record_keys.hpp` | `ff32b3c8f5e0942397921048068e0cd13b0859bf11f5e81dae13c8f7e9169baf` |
| `tests/unit/test_batch_analysis.cpp` | `46dab6c51f440cb4e09a28ac74712c40b442e4c47c7898e45d1f87913223959e` |
| `tests/unit/test_verify_selection.cpp` | `6f5aaaf67fbc983c4f909bec5178b56ba7191c6c27ccae5e2bff5ab2f859c642` |

- Test release (`tmp/release-junit.xml`): totali 1212, fast 1190, slow 22, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 1190, fast 1190, slow 0, falliti 0
<!-- evidence:end -->
