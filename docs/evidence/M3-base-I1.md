# M3, base comune, intervento I1: OB-04, OB-09, OB-10 nel runner

Stato: **CHIUSO, review PASS al giro 1**. Unità: M3, base comune, prima unità di codice dei debiti del §4 di `docs/contracts/BASE-confini.md`. Suite: `tests/unit/test_conformance.cpp` (non modificata); elenco dei 28 rossi di partenza in `docs/evidence/M3-base-suite.md`.

| Campo | Valore |
|---|---|
| Unità | M3 / base comune, intervento I1 (codice) |
| Scopo | Portare OB-10 (nodo uguale alla richiesta), OB-04 (ambiente FP prima di ogni nodo) e OB-09 (risorse nel budget, `details.cpu`, `details.resource_plan`) nel proprietario, `run_job_files`, e togliere le copie dagli operatori |
| Comando | Prima: `cmake --preset release`, `cmake --build --preset release`, `ctest --preset release -R "Conformance\|M3BASE" --output-junit tmp/basei1-before-junit.xml` (4 job, log `tmp/basei1-before.log`). Dopo: stessa build, `ctest --preset release --output-junit tmp/release-junit.xml` (4 job, log `tmp/basei1-full1.log`); `tools/check.sh` con `CMAKE_BUILD_PARALLEL_LEVEL=4` e `CTEST_PARALLEL_LEVEL=4` (log `tmp/basei1-check-r1.log`, junit `tmp/dev-junit.xml`); `tools/lint.sh`; `python3 -S tools/trace.py check --stage M2 --map tests/trace/M2.tsv --build-dir build/release --ctest-junit tmp/release-junit.xml`; `tools/mutate.sh` per ogni patch di `tmp/basei1-mut/` (script `tmp/basei1-mut/run.sh`); `python3 -S tools/evidence.py docs/evidence/M3-base-I1.md` |
| Input | Codice a `725bdac` più le modifiche elencate sotto; fixture sintetiche di `tests/fixtures/conformance_inputs.hpp` |
| Versioni | GCC 15.2.0, CMake 4.2.3; preset `release` e `dev` (ASan e UBSan) |
| Ambiente | Linux 7.0.0-38-generic x86_64, 16 CPU, worktree isolato, 4 job |
| Risultato | Release completo: 604 test, 21 falliti, tutti della suite di conformità. Dev completo: 598 test, gli stessi 21. Diventano verdi T-M3BASE-10 per i 6 parametri e T-M3BASE-04 per `cache.rebuild`; nessun altro test cambia stato |
| Output | Log e junit sotto `tmp/` citati nel comando |
| Ambito | OB-04, OB-09, OB-10. Fuori: OB-08 (tipi di import senza `job_plan` né `resource_config`), che lascia rosso T-M3BASE-09 per import, miniatura e ricostruzione |
| Esito | PASS: suite, mutazioni, lint, trace; review indipendente PASS al giro 1 |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Modifiche

Codice di prodotto, 8 file (`git diff --stat`: 117 righe aggiunte, 78 tolte):

- `job.hpp`, `job.cpp`: `job_request` con il costruttore del nodo della richiesta, il codice dell'arresto e la configurazione delle risorse; `job_files_result::plan`; ultimo parametro di `run_job_files`, con valore predefinito vuoto. Nel runner: controllo FP e serializzazione di `budget.resources` nello stesso `prepare` di `prepare_job_start`; dopo `node_of` e prima di `work` costruzione del nodo della richiesta e confronto delle chiavi; in un job riuscito `details.cpu` (backend e `simd` del nodo, capacità della CPU) per un backend `cpu_*` e `details.resource_plan` dal piano dichiarato.
- `cpu_dispatch.hpp`, `cpu_dispatch.cpp`: `cpu_dispatch_json` compone dal valore `simd` e dal backend del nodo; nessun altro chiamante.
- `calibration_job.cpp`, `calibration_blocks.cpp`, `master.cpp`: tolti controllo FP in `work`, lambda di `prepare_job_start`, confronto del nodo, `details.cpu` e `details.resource_plan`; il nodo usato da `work` è quello costruito dal runner.
- `import_job.cpp`: tolto `check_node`; i tre tipi passano il costruttore del nodo al runner; `cache.rebuild` controlla nel proprio `prepare_job_start` solo la causa (`require_rebuild_cause`, confronto di stringhe), senza costruire il nodo.
- `tools/lint-allowlist.txt`: impronta della voce `cpp-catch-all-silent` del `catch (...)` di `run_job_files`, il cui corpo passa `recorded` invece di `budget` (stesso costrutto, `unexpected` è `[[noreturn]]`).
- `docs/contracts/BASE-confini.md`: §2 (stato di OB-04, OB-09, OB-10) e §4 (righe OB-04, OB-09, OB-10).

Impatto letto con `codegraph impact` prima della modifica: `run_job_files` (chiamanti in import, calibrazione, master, `run_branches`, registro dei tipi e i test di store, recovery, simd, import, fase, calibrazione, master), `prepare_job_start`, `job_files_result`. Tutti i test elencati sono nel ctest completo `release` e `dev`.

## Rossi prima e dopo

| # | Test ctest | Debito | Prima | Dopo |
|---|---|---|---|---|
| 1 | `ConformanceWorkflow.T_M3BASE_03_*` | OB-03 | rosso | rosso |
| 2-4 | `T_M3BASE_05_*` (calibrazione `intera`, `blocchi`, master) | OB-05 | rosso | rosso |
| 5 | `T_M3BASE_04_*/cache_rebuild_unica` | OB-04 | rosso | **verde** |
| 6-10 | `T_M3BASE_07_*` (import, miniatura, ricostruzione, calibrazione `intera`, `blocchi`) | OB-07, OB-08 | rosso | rosso |
| 11-13 | `T_M3BASE_08_*` (import, miniatura, ricostruzione) | OB-08 | rosso | rosso |
| 14-16 | `T_M3BASE_09_*` (import, miniatura, ricostruzione) | OB-09 e OB-08 | rosso | rosso |
| 17-22 | `T_M3BASE_10_*` (tutti e 6 i parametri) | OB-10 | rosso | **verde** |
| 23-28 | `T_M3BASE_11_*` (tutti e 6 i parametri) | OB-11 | rosso | rosso |

Totale: 28 rossi prima, 21 dopo, identici in `release` e `dev`. T-M3BASE-09 per import, miniatura e ricostruzione resta rosso per `budget.resources` e `details.resource_plan` (questi tipi non ricevono una `resource_config` e non dichiarano un `job_plan`: OB-08, intervento I2); `details.cpu` non è più fra i motivi (log `tmp/basei1-t09-import.log`). T-M3BASE-09 per calibrazione e master era verde con le copie degli operatori e resta verde col proprietario.

## Mutazioni

`tools/mutate.sh` sul preset `release`, baseline verde sui test selezionati, sorgente ripristinato e verificato con `cmp`:

| Patch | Mutazione | Test selezionati | Esito |
|---|---|---|---|
| `m1-ob10-compare` | confronto delle chiavi del runner disattivato | `T_M3BASE_10` (6) | RILEVATA, 6 falliti |
| `m2-ob04-fp-check` | controllo FP del runner tolto | `T_M3BASE_04` (6) | RILEVATA, 6 falliti |
| `m3-ob04-rebuild-node-first` | nodo di `cache.rebuild` costruito nel `prepare` dell'operatore, prima del controllo FP del runner | `T_M3BASE_04.*cache_rebuild` (1) | RILEVATA, 1 fallito |
| `m4-ob09-resources` | `budget.resources` non serializzato | `T_M3BASE_09.*(calib\|master)` (3) | RILEVATA, 3 falliti |
| `m5-ob09-cpu` | `details.cpu` non composto | `T_M3BASE_09.*(calib\|master)` (3) | RILEVATA, 3 falliti |
| `m6-ob09-plan` | `details.resource_plan` non composto | `T_M3BASE_09.*(calib\|master)` (3) | RILEVATA, 3 falliti |

## Review

`tools/review.sh basei1-round1 tmp/basei1-facts-round1.md` (Codex `gpt-6-astra`, sola lettura), sul delta `tmp/basei1-round1.diff`, con il perimetro chiesto dal coordinatore: solo rilievi BLOCCANTE, ciascuno con una sequenza eseguibile. Artefatto `.omc/artifacts/ask/codex-basei1-round1-20261030-0920.md`: nessun rilievo, `Verdetto: PASS`. Nessun giro 2.

## Ricerca di altre copie

In `src/core/` (unica directory dei sorgenti di prodotto) con `grep` su `node_key`, `node_mismatch`, `require_fp_environment`, `prepare_job_start`, `resource_config_json`, `"resource_plan"`, `"cpu"`, `cpu_dispatch_json`:

- confronto del nodo della richiesta: solo `job.cpp:289-295`. Non sono copie di OB-10 `workflow.cpp:62` (identità del job eseguito da un passo del workflow, dopo il job), `scheduler.cpp:101` (nodo del checkpoint al resume, OB-15), `project_verify.cpp:97` (manifest dell'export), i `dag.artifact_node_mismatch` dello store (artefatto di un altro nodo);
- `require_fp_environment`: runner (`job.cpp:245` per `run_job`, `:279` per `run_job_files`), worker dello scheduler (`scheduler.cpp:235`, OB-05), `begin_kernel_task` (`cpu_dispatch.cpp:97`, OB-05) e le guardie di libreria che il contratto esclude (`calibration_job.cpp:100`, `master.cpp:189`, `:830`, `calibration.cpp:622`);
- `prepare_job_start` fuori dal runner: solo la causa di `cache.rebuild` (`import_job.cpp:393`), senza nodi né numeri in virgola mobile;
- `resource_config_json`, `details.cpu`, `details.resource_plan`: solo nel runner (`job.cpp:281`, `:307`, `:311`) e nella persistenza delle impostazioni (`resources.cpp:313`, non è provenance).

## Scelte

- **Codice di OB-10.** Il §4 proponeva un codice unico `job.node_mismatch` con emendamento dei contratti. ADR-07.a §Aggiunte di R5 e i test esistenti (`test_simd.cpp:585`, `:590`, `test_calibration_job.cpp:324`) citano `calib.node_mismatch` e `master.node_mismatch`; il vincolo dell'unità è che nessun altro test cambi stato. Il codice resta quello del tipo, passato dall'operatore in `job_request::mismatch_code`. Il contesto dell'arresto usa per tutti i tipi le chiavi `nodo_slot` e `nodo_richiesta` (calibrazione e master avevano `nodo_ricetta`; nessun test né contratto cita la chiave).
- **Arresto di OB-10 dentro il lavoro.** Il confronto sta nel `try` del runner prima di `work`: esito con uscita 2 e provenance del nodo dello slot, come le copie che sostituisce e come richiedono i test esistenti che leggono `out.stop`. Anche gli errori del costruttore del nodo (ricetta non valida, `cpu.avx2_unavailable`) restano esiti, come prima.
- **`details.cpu` per ogni backend `cpu_*`.** Il testo di OB-09 lo lega al backend del nodo, non al tipo: il runner lo scrive anche per i tipi di import (backend `cpu_scalar`, senza `simd`) e per i job composti da `run_branches`.

## Limiti

- `job_request` ha un valore predefinito vuoto: un tipo nuovo che non passa il costruttore del nodo non è fermato dal compilatore. Lo rileva T-M3BASE-10 quando il tipo entra nel registro (§3.3).
- Per i tipi di import `budget.resources` e `details.resource_plan` mancano finché non pagano OB-08 (I2).
- La forma di `details.cpu` senza `simd` per i nodi che non portano la richiesta non è descritta da ADR-07.a, che parla di calibrazione e master.
- I record che fissano gli SHA-256 dei file toccati, fra cui `docs/evidence/M3-base-suite.md`, non coincidono più con `tools/evidence.py --check`: descrivono lo stato del loro commit.

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/BASE-confini.md` | `7ce6bd1660ffac3a95ddec20be603a59c0fec6e751c26cacea25e64bc5c114fb` |
| `src/core/calibration_blocks.cpp` | `9e31e40eb78edb9086c62d4a914dc927538dea590c2229e5f6671f05dfebc0ad` |
| `src/core/calibration_job.cpp` | `38e80c8138a4ca1b24a9e9201b1089ff710e89b7fce4f372f6f7fe8df4c307ad` |
| `src/core/cpu_dispatch.cpp` | `2dd162cef227fc5310162865b4579baaa34489238bba06131d72b85e5e4b7644` |
| `src/core/import_job.cpp` | `f9e855ad710c89124e6c93511162be65543ee1241de35e04ac74d2ad02f4bc14` |
| `src/core/include/astro/cpu_dispatch.hpp` | `f0a399e10653c68c71219842076d8e872d9cf30c1f6db05e0ae1ebaebb43acee` |
| `src/core/include/astro/job.hpp` | `e594f0e97270251ee9a73983a0c55d3b3e219615cf6c71872b1fbe0b90f49cc0` |
| `src/core/job.cpp` | `f9681bccbd5d14b4bdec111fb72f1213b5f802cf249a0e78448f8855acc77805` |
| `src/core/master.cpp` | `e8a4233a39cd681bdee040b0cb9921deb1bd14b1e9782c3ea2c19b9996ca6b90` |
| `tests/unit/test_conformance.cpp` | `a75bee61a509be32163cfe862cbb4f25a1a9187a256a4fd7c404c96d6799683b` |
| `tools/lint-allowlist.txt` | `8c0deba2fa1011e5938f8b50a9633acedb5652e513e35080b3bb9bae2ff444b4` |

- Test release (`tmp/release-junit.xml`): totali 604, fast 597, slow 7, falliti 21
- Test dev (`tmp/dev-junit.xml`): totali 598, fast 598, slow 0, falliti 21
<!-- evidence:end -->
