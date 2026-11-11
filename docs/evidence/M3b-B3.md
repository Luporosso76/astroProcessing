# M3.b, unità B3: tipi, ricetta, domini, fase e linearità del master

Stato: **chiusa** il 9 novembre 2026. Review `M3b-B3-round1` FAIL, `M3b-B3-round2` PASS (Gemini 4 Argon). Test e codice da agenti distinti. Mutazioni 12/12 rilevate.

| Campo | Valore |
|---|---|
| Unità | M3.b / B3 (test e codice) |
| Scopo | `combine_method` `single`, `weighted_mean`, `winsorized`; `variance_estimator` `noise_model`, `empirical_weighted`, `gaussian_reference`, `frozen_selection`; campi di ricetta (pesi, `analysis`, `winsor_g`, `clip_*`, `offset_adu`, `conversion_gain_e_per_adu`, `read_noise_adu`); validazione strutturale e domini; `require_phase` e `require_linear` via `require_operation_allowed`; nodo, chiave e `master_descriptor_from_node` con `frame_normalization` |
| ID | T-M3B-C02-12; T-M3B-C02-09 (domini e stop); T-M3B-C02-03(c); T-M3B-DSC-01; T-M3B-FLAT-06 (parte parametri); T-M3B-C02-15 (solo `master.param.<nome>`); riesecuzione di T-M2R2-01…09 e `test_master.cpp` |
| Comando | `tools/check.sh`; `tools/mutate.sh`. Suite fast: 1236 test verdi in 3 shard sull'albero con B2 (riga di stato del piano). `ctest --preset release` e `dev-full` completi: non eseguiti per l'unità, rinvio al gate |
| Commit | `e66a112` (codice e test), `fc893d5` (chiusura nel piano e in STATO) |
| Versioni | non verificato; rinvio al record del gate |
| Ambiente | non verificato; rinvio al record del gate |
| Autore e data | Test e codice: agenti Sonnet distinti (tabella di `plans/M3b.md`). Review: Gemini 4 Argon. Coordinatore. 9 novembre 2026 |

## File

Codice: `src/core/master.cpp`, `src/core/include/astro/master.hpp`, `src/core/include/astro/calibration.hpp`, `src/core/calibration_internal.hpp`, `src/core/calibration_job.cpp`, `src/core/batch_classification.cpp`. Test: `tests/unit/test_master_b3.cpp` (nuovo), `tests/unit/test_master.cpp` (attese cambiate di categoria e codice sui casi `phase` e `nonlinear`).

## Decisioni di unità

Da `docs/decisions/M3b-lacune.md`, «Decisioni successive (B3)»:

- `n` sotto il minimo del metodo: `unsupported`, `master.variance_domain`; `master.frames_insufficient` non è più prodotto da `validate_master_recipe`. Emendata la tabella di C-02.a §2.
- `master_algorithm_version` = 2: i master `.a` diventano `stale`.
- Un metodo ammesso dai domini ma senza calcolo ferma `combine_master` e il job con `unsupported`, `master.method_not_computable`; mai calcolato come media. Codice temporaneo, tolto in B9.
- `flat_normalization` fuori da {`none`, `median_ratio`}: `invalid_input`, `master.param.flat_normalization` (L11).

## Limiti

- Il calcolo dei metodi non è in B3 (B5, B6); `median_ratio` non è ammesso (B8).
- `sigma_clip_inert` e i confronti esatti su pesi e κ sono rinviati a B5 e B6.
- Chiave dei master `.a`: cambia (versione 2); i nodi già prodotti diventano `stale` (`plans/M3b.md` §11).
- Mutazioni 12/12 rilevate (riga di stato); elenco per patch non riportato nelle fonti consultate: non verificato qui.

## Review

- Giro 1 FAIL (`.omc/artifacts/ask/review-M3b-B3-round1-argon-20261109-0845.md`): 2 BLOCCANTI (`valid_master_descriptor` in `src/core/sqlite_classification.cpp` accettava per i master esterni valori propri del prodotto, con conversione silenziosa alla rilettura; `offset_adu < 0` rifiutato contro ADR-17.a), 3 MED (test mancanti per i parametri di `single`, per `master.method_not_computable`, validazione di `flat_normalization` nel nodo), 2 LOW (mappa di trace, nome della ricetta). Corretti.
- Giro 2 PASS (`.omc/artifacts/ask/review-M3b-B3-round2-argon-20261109-0917.md`: «Verdetto: PASS», nessun rilievo).

## Blocco generato

Assente. Hash dei file, conteggi dei junit ed esito di `trace.py check` li scrive `tools/evidence.py` da un file `.evidence`, al run completo del gate (`docs/evidence/M3-b.md`); non si scrivono a mano. Il run completo (`ctest --preset dev-full` e `release`, stesso commit) è del gate: qui valgono solo le verifiche d'unità dichiarate sopra.
