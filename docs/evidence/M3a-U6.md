# M3.a, unità U6: G-CAL parte 1, stati accettati e compatibilità

Stato: **chiusa**. Test: review PASS al giro 2. Codice: review FAIL al giro 2 (1 MED: rami senza test), arbitraggio del coordinatore (test dei rami aperti, 29 mutazioni, ripieghi sugli enum tolti), review sul delta PASS (blocco «Review»).

| Campo | Valore |
|---|---|
| Unità | M3.a / U6 (test e codice) |
| Scopo | Stati `usable`/`excluded` dei master per ruolo, unità, fase, linearità, layout, piedistallo, coverage (ADR-16.b §b.2; C-01.b §2, §7); compatibilità per coppia e per catena con la lista chiusa dei motivi (§b.3); compatibilità del decoder: campi strutturali, nero effettivo in interi a 64 bit contro `decoder_black_abs_tol_adu` obbligatorio, `decoder_state_mixed` (§b.7); tabella delle operazioni di ADR-03.b §b.14 |
| ID | G-CAL (parte 1); T-M3A-CAL-08, T-M3A-TYP-18, T-M3A-C01-06, T-M3A-C01-07 (parte di analisi) |
| Comando | Integrazione: build pulita, `tools/check.sh` e `ctest --preset release` completo (blocco generato). Mutazioni: `tools/mutate.sh` (patch in `tmp/u6-mut/` e `tmp/u6med2-mut/` del worktree) |
| Versioni | GCC 15.2.0, CMake 4.2.3, Ninja |
| Ambiente | Linux 7.0.0-38-generic x86_64, 16 CPU; worktree `.claude/worktrees/opus-u6c` sulla base U5 |
| Autore e data | Test: agente Claude Opus 5.5 (abbonamento), test dell'arbitraggio: agente Claude Sonnet 5.5 (API). Codice: agente Claude Opus 5.5 (abbonamento). Review: Claude Opus 5.5 (Argon fuori quota). Coordinatore: Claude Opus 5.5. 5 e 6 novembre 2026 |

## Decisioni del coordinatore

- ADR-16 §b.7 (5 novembre 2026): ruolo nella chiave di contesto `role`; `process_warnings` non confrontati; `frame_decoder_states` vuoto → `metadata_missing.frame_decoder_states`, controllato prima di `decoder_state_mixed`; `flat_normalization_declared` su un flat non `calibrated` → `invalid_input` `calib.param_not_applicable.flat_normalization_declared`; fase o unità mancanti: il light si ferma con `metadata_missing` (`calib.metadata.phase`, `calib.metadata.unit`), il master è escluso con `metadata_missing.*`.
- ADR-16 §b.3 (6 novembre 2026): il bias, nella coppia F–B, si confronta col flat candidato anche se escluso (come il dark-flat); il verdetto resta nel report e la catena non conta per `z` di P-DFB (§b.4).
- Enum fuori dai valori dichiarati: `std::abort()`, mai una stringa di ripiego (`to_string` di `light_chain`, `offset_state`, `black_applied_by_decoder`).

## Mutazioni

Tutte rilevate con `tools/mutate.sh`.

| Gruppo | Patch |
|---|---|
| Codice, giro 1 (`tmp/u6-mut/`) | `m1-structural` (campo strutturale non confrontato), `m2-black-ge` (`>=` al posto di `>` sulla tolleranza del nero), `m3-chain-all` (motivo di catena ignorato), `m4-phase-after-lin` (fase controllata dopo la linearità), `m5-calc-before-stop` (calcolo prima dello stop) |
| Arbitraggio (`tmp/u6med2-mut/`) | `m01` normalizzazione del flat; `m02`, `m12` `frame_decoder_states` vuoto; `m03` coverage nella ricetta; `m04a`, `m04b` validazione dei master; `m05a`–`m05c` coverage nella scrittura FITS e nel confronto del contenuto; `m06` ruolo dello slot; `m07a`–`m07e` tolleranze di esposizione e temperatura obbligatorie; `m08`, `m09a` stato del decoder e offset con riferimento RAW; `m09b` offset mai confrontato; `m10a`–`m10h` `imagetyp_conflict`, `shared_input`, campi mancanti; `m11` ordine dei controlli; `m13` regola di `IMAGETYP` resa simmetrica |

## Limiti

- Un'immagine con coverage non si scrive in FITS: stop `unsupported`, `fits.coverage`; la scrittura dell'HDU di coverage arriva con T-M3A-TYP-07 (M3.g), dichiarato in `plans/M3a.md` §U6.
- T-M3A-C01-07: solo la parte di analisi; l'esecuzione è di M3.c.

## Review

- Test, giro 1 FAIL (5 MED), giro 2 PASS.
- Codice, giro 1 FAIL (2 MED), giro 2 FAIL (1 MED). Arbitraggio: review sul delta `tmp/api/u6code-arb-review.json` PASS con 4 LOW (F–B col flat escluso non scritto nel contratto; limite `fits.coverage` non dichiarato; ripiego in `descriptor_for` dei test; caso asimmetrico `Flat Dark` mancante), chiusi dal coordinatore e dall'agente dei test.

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-16-stati-master.md` | `703ce7ac2ed085f4aca2b6fa716bb528c21ba58d15316e20333fbf37c4994ff4` |
| `docs/contracts/C-01-calibrazione-light.md` | `063e14eacdbf25c92b31cf78c4bb6e19934d39fada23a06ba0c24d7fe8212162` |
| `src/core/calibration.cpp` | `552fcc48742a6a543c408e8c7adb2cd20c68bd71d3deb08fa1a4cdd2076d430d` |
| `src/core/calibration_analysis.cpp` | `ef641f3be2d55582cc3096b26ee0d44d8726fbf6d7051b1c5c3f73e81e694e6d` |
| `src/core/fits.cpp` | `ef596caae35a0bf99f0384d53bf2d901fed8609794db13ff32a50f1865789b5b` |
| `src/core/include/astro/calibration_analysis.hpp` | `c88647b30b362c843d9bc26a70f69b9caf8713516c0a05fbe84f806cbd0126e9` |
| `src/core/include/astro/operations.hpp` | `0ff6fe02460c785052085650cbeb2214d49051318f10f5d83c80a4c73010f6df` |
| `src/core/operations.cpp` | `f1f5b17966332891c75e50c0ee8d91e73034f32b19d65c082a66a1b75d1a88ae` |
| `tests/unit/test_calibration.cpp` | `27eba24afcba2e21b4f50e65b2e49608f06e6abe12e902340976d8efb40bb373` |
| `tests/unit/test_calibration_analysis_u6.cpp` | `9e49e992d39bee25961a5320831e1146464d5e89479c262b967ad4a475cbaa0e` |

- Test release (`tmp/release-junit.xml`): totali 1107, fast 1089, slow 18, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 1089, fast 1089, slow 0, falliti 0
<!-- evidence:end -->
