# M3.b, unità B15a: master da RAW, parte 1: stato del decoder, nodo e preflight

Stato: **chiusa** il 9 novembre 2026. Review `M3b-B15a-round1` FAIL, `M3b-B15a-round2` PASS (revisore Claude Opus, contesto separato; Argon senza esito). Test e codice da agenti distinti. Mutazioni 16/16.

| Campo | Valore |
|---|---|
| Unità | M3.b / B15a (test e codice) |
| Scopo | `frame_decoder_states` nei parametri del nodo (ordine canonico), invarianti del descrittore (`master.decoder_state_param`), `master.decoder_state_mixed`, preflight su campi strutturali e nero (`master.mismatch.<campo>`, `master.mismatch.decoder_black`), `master.metadata.offset_adu`; nel decoder il modo con prenotazione del chiamante e `raw_decode_needed_bytes` (ADR-08.b §2.8). Il nero non si sottrae (C-01.b §1). Nessuna lettura di frame RAW nel job (B16) |
| ID | T-M3A-CAL-07; T-M3B-DEC-01; T-M3B-C02-08 (RAW: nodo con un campo strutturale alla volta, nero alla tolleranza e a tolleranza + 1, a livello di preflight del nodo); ADR-16.b §b.7, §b.11; C-02.b §b.2, §b.7, §b.10 |
| Comando | `tools/check.sh`; `tools/mutate.sh`. Suite fast: 1274 test verdi in 3 shard sull'albero con B3b (riga di stato del piano). Esiti per test: non verificato qui, rinvio al gate |
| Commit | `4ff7f4b` (codice e test), `af8c7d3` (chiusura) |
| Versioni | non verificato; rinvio al record del gate |
| Ambiente | non verificato; rinvio al record del gate |
| Autore e data | Test e codice: agenti Opus distinti (tabella di `plans/M3b.md`). Review: Claude Opus (Agent `code-reviewer`, sola lettura). Coordinatore. 9 novembre 2026 |

## File

Codice: `src/core/master.cpp`, `src/core/include/astro/master.hpp`, `src/core/raw_decode.cpp`, `src/core/include/astro/raw_decode.hpp`, `src/core/dng_decode.cpp`, `src/core/dng_internal.hpp`, `src/core/raw_import_job.cpp`, `src/core/calibration_analysis.cpp`, `src/core/include/astro/calibration_analysis.hpp`, `src/core/image.cpp`, `src/core/include/astro/image.hpp`. Test: `tests/unit/test_master_b15a.cpp`, `tests/unit/test_raw_decode_b15a.cpp`, `tests/unit/test_raw_import.cpp`.

## Decisioni di unità

Da `docs/decisions/M3b-lacune.md`, «Decisioni successive (B15a)»:

- `offset_adu` ammesso solo con `single` o con il flat da RAW; altrove `invalid_input`, `master.param.offset_adu`. `decoder_black_abs_tol_adu` obbligatorio con frame RAW, vietato altrimenti (`master.param.decoder_black_abs_tol_adu`).
- `raw_decode_context` con `pool` e `caller_reserved_bytes` insieme, o con `held_bytes` senza `pool`: `invalid_input`, `raw.decode_context`, prima di ogni lettura.
- Le chiavi dei master da FITS cambiano (due parametri `null` in più): i nodi già pubblicati diventano `stale`. I nodi `master.combine` senza `frame_decoder_states` non hanno descrittore: stop esplicito `master.decoder_state_param`.
- La chiave della calibrazione con `frame_decoder_states` è di M3.c; il controllo sui campioni del nero non sottratto è di B15.

## Limiti

- La prova «prima di ogni allocazione» del modo del chiamante non vede il buffer del segmento del decoder del progetto (`std::vector`) né le allocazioni interne di LibRaw; che il controllo li preceda lo mostrano il codice (un solo punto, `reserve_decode`) e la mutazione che sposta il controllo dopo il tetto di zlib, rilevata dai ganci.
- LOW residui del giro 2 (`.omc/artifacts/ask/review-M3b-B15a-round2-claude-20261109-1221.md`): contesto dello stop confrontato solo nei campi interi (`sinistra` non verificato per tutti gli otto campi); 10 campi DNG su 15 nella chiave; commento di `test_raw_decode_b15a.cpp:252` attribuito alla decisione sbagliata; riga T-M3B-DEC-01 della matrice senza i due casi del giro 2.
- Mutazioni 16/16 (riga di stato); elenco per patch non riportato nelle fonti: non verificato qui.

## Review

- Giro 1 FAIL (`.omc/artifacts/ask/review-M3b-B15a-round1-claude-20261109-1155.md`; l'artefatto di Argon dello stesso giro è vuoto). Giro 2 PASS con 4 LOW (sopra).

## Blocco generato

Assente. Hash dei file, conteggi dei junit ed esito di `trace.py check` li scrive `tools/evidence.py` da un file `.evidence`, al run completo del gate (`docs/evidence/M3-b.md`); non si scrivono a mano. Il run completo (`ctest --preset dev-full` e `release`, stesso commit) è del gate: qui valgono solo le verifiche d'unità dichiarate sopra.
