# M3, base comune, intervento I4: OB-03 nel workflow, OB-19 nel runner dei job

Stato: **REVIEW PASS** al giro 2. Unità: M3, base comune, quarta unità di codice dei debiti del §4 di `docs/contracts/BASE-confini.md`. Suite: `tests/unit/test_conformance.cpp` (non modificata, SHA-256 nel blocco generato); rosso di partenza: T-03, l'unico lasciato da I3 (`docs/evidence/M3-base-I3.md`).

| Campo | Valore |
|---|---|
| Unità | M3 / base comune, intervento I4 (codice) |
| Scopo | Portare OB-03 (record di arresto del workflow nello store per ogni rifiuto di `run_workflow` e `resume_workflow` prima del primo passo) e OB-19 (rimozione di checkpoint e staging nel runner, dopo `commit_job` riuscito) nel proprietario |
| Comando | Prima: `cmake --preset release`, `cmake --build --preset release -j4`, `ctest --preset release -j4 -R "Conformance\|M3BASE" --output-junit tmp/basei4-before-junit.xml` (log `tmp/basei4-before.log`, elenco `tmp/basei4-fail-before.txt`). Dopo: stessa build (`tmp/basei4-build.sh release`); `ctest --preset release -j4 --output-junit tmp/release-junit.xml` (log `tmp/basei4-release.log`); `tools/check.sh` con `CMAKE_BUILD_PARALLEL_LEVEL=4` e `CTEST_PARALLEL_LEVEL=4` (log `tmp/basei4-check.log`, junit `tmp/dev-junit.xml`); `tools/lint.sh` (log `tmp/basei4-lint.log`); `python3 -S tools/check_job_types.py .` (`tmp/basei4-jobtypes.log`); `python3 -S tools/trace.py check --stage M2 --map tests/trace/M2.tsv --build-dir build/release --ctest-junit tmp/release-junit.xml` (`tmp/basei4-trace.txt`); `tmp/basei4-mut/run.sh` (`tools/mutate.sh` per ogni patch di `tmp/basei4-mut/`, generate da `tmp/basei4-mut/make.py`); sequenza finale in `tmp/basei4-final.sh`; `python3 -S tools/evidence.py docs/evidence/M3-base-I4.md` |
| Input | Codice a `03c2efc` più le modifiche elencate sotto; fixture sintetiche di `tests/fixtures/conformance_inputs.hpp` |
| Versioni | GCC 15.2.0, CMake 4.2.3; preset `release` e `dev` (ASan e UBSan) |
| Ambiente | Linux 7.0.0-38-generic x86_64, 16 CPU, worktree isolato senza il collegamento `data` ai dati reali, 4 job |
| Risultato | Suite di conformità: 92 test e 1 rosso prima (T-03), 92 test e 0 rossi dopo. Release completo: 611 test (608 più i 3 nuovi), 4 falliti, tutti su dati reali fermi sul prerequisito mancante (`data/public` assente nel worktree). Dev: 605 test, 1 fallito (`Fits.REAL_DATA_PUB_LFC_01`, stesso prerequisito). Lint e registro dei tipi verdi |
| Output | Log e junit sotto `tmp/` citati nel comando |
| Ambito | OB-03, OB-19. Fuori: le mutazioni sopravvissute di I2 e I3 (riga «Prove» del §4) |
| Esito | PASS: suite 1 rosso → 0, nessun altro test cambiato oltre i prerequisiti di dati reali; mutazioni rilevate tranne `m09`, dichiarata sotto; review PASS al giro 2 |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Modifiche

Codice di prodotto, 7 file modificati e 1 nuovo (`git diff --numstat -- src`: 164 righe aggiunte, 31 tolte; più `src/core/stop_internal.hpp`, 20 righe):

- `workflow.cpp`, `workflow.hpp`: i controlli prima del primo passo passano in `start_run` e `plan_resume`; `run_workflow` e `resume_workflow` passano ogni loro eccezione a `stop_workflow`, che scrive il record di arresto e la rilancia. Il record è una riga di `settings`: chiave `workflow_stop.<sha256 della definizione canonica data>.<esecuzione>`, con l'esecuzione la prima libera da 1; valore JSON canonico `astro.workflow_stop.v1` con `workflow`, `definition_sha256`, `execution`, `operation` (`run` o `resume`), `category`, `code`, `message`, `context` (coppie `[chiave, valore]`). Solo stringhe e interi.
- `sqlite_dag_store.hpp`, `sqlite_dag_store.cpp`: `insert_setting`, come `put_setting` ma una chiave già presente è `invalid_input` `store.setting_exists` e nulla si scrive; entrambe passano da `write_setting`.
- `stop_internal.hpp` (nuovo), `job.cpp`: `what_of`, `cause_of` e `rethrow_with_record_failure` escono dallo spazio anonimo di `job.cpp` (namespace `astro::stopint`) per essere condivisi col workflow; il comportamento non cambia.
- `job.hpp`, `job.cpp`: `job_files_result::scratch` (directory, nome del checkpoint, nome dello staging); `run_job_files` lo prende al ritorno di `work` e, dopo il `try` che termina con `commit_job` riuscito, chiama `remove_checkpoint` e `remove_named_spill_file`. Un errore della rimozione si propaga col job già `completed`.
- `calibration_job.cpp`: la calibrazione con checkpoint dichiara `scratch` nel risultato di `work`; tolta la rimozione dopo il ritorno di `run_job_files`.
- `tests/unit/test_store.cpp`: tre test nuovi, fuori dalla suite (`Store.insert_setting_refuses_an_existing_key_and_keeps_the_value`, `Store.every_workflow_refusal_leaves_its_own_stop_record`, `Store.workflow_refusal_on_a_read_only_store_propagates_the_original_stop`).
- `docs/contracts/BASE-confini.md`: stato di OB-03 e OB-19 nel §2 e le loro righe del §4.

Impatto letto con `codegraph impact` prima delle modifiche (indice del checkout principale, aggiornato con `codegraph sync`): `job_files_result` (calibrazione, test di `test_simd`, `test_calibration_r32`, `test_calibration_job`, `test_phase`, `test_recovery`, `m2e_workflow`), `run_job_files` (tipi di I/O, calibrazione, master, test dello store), `put_setting` (budget, risorse, workflow), `run_workflow` e `resume_workflow` (`test_recovery`, `test_simd`, `test_conformance`). Tutti nel ctest completo `release` e `dev`; nessuno cambia di stato. I test che verificano i rifiuti del workflow (`Recovery.T_M2R4_02_*`, `Simd.T_M2R5_04_*`) controllano job, artefatti e l'impostazione `workflow.<nome>`, non l'insieme delle impostazioni: il record nuovo non li tocca.

## Rossi prima e dopo

| # | Test ctest | Debito | Prima | Dopo |
|---|---|---|---|---|
| 1 | `ConformanceWorkflow.T_M3BASE_03_workflow_refusal_leaves_a_stop_record` | OB-03 | rosso | **verde** |
| 2 | `Types/ConformanceCheckpoint.T_M3BASE_19_*` (calibrazione `blocchi`) | OB-19 | verde | verde |

Causa del rosso di partenza (`tmp/basei4-fail-before.txt`): nessuna riga nuova nello store dopo il rifiuto `workflow.slot_target` né dopo `resume.recipe`. T-19 era già verde perché la rimozione dell'operatore aveva lo stesso effetto osservabile al confine (`docs/evidence/M3-base-suite.md`, OB-19).

## Mutazioni

`tools/mutate.sh` sul preset `release` con 4 job, una patch per controllo aggiunto, baseline verde sui test selezionati, sorgente ripristinato e verificato con `cmp` (`tmp/basei4-mut/summary.txt`, log per patch in `tmp/basei4-mut/`):

| Patch | Mutazione | Test selezionati | Esito |
|---|---|---|---|
| `m01-ob19-runner-no-removal` | `run_job_files` non rimuove checkpoint e staging | `T_M3BASE_19` | RILEVATA |
| `m02-ob19-removal-before-commit` | rimozione subito dopo il ritorno di `work`, prima di cancel e `commit_job` | `T_M3BASE_19` | RILEVATA |
| `m03-ob19-calibration-no-declaration` | la calibrazione non dichiara `scratch` | `T_M3BASE_19` | RILEVATA |
| `m04-ob03-run-no-record` | `run_workflow` rilancia senza `stop_workflow` | `T_M3BASE_03`, `stop_record` | RILEVATA |
| `m05-ob03-resume-no-record` | `resume_workflow` rilancia senza `stop_workflow` | `T_M3BASE_03`, `stop_record` | RILEVATA |
| `m06-ob03-insert-replaces` | `insert_setting` sostituisce una chiave presente | `insert_setting`, `stop_record`, `T_M3BASE_03` | RILEVATA |
| `m07-ob03-execution-always-1` | esecuzione sempre 1 | `stop_record`, `T_M3BASE_03` | RILEVATA |
| `m08-ob03-record-failure-not-annotated` | registrazione fallita: arresto rilanciato senza `registrazione_arresto` | `read_only_store_propagates` | RILEVATA |
| `m09-ob03-only-stop-errors-recorded` | `run_workflow` registra solo gli `stop_error` | `T_M3BASE_03`, `stop_record`, `read_only_store`, `T_M2R4_02` | SOPRAVVISSUTA |
| `m10-ob03-record-without-context` | record senza `context` (correzione del giro 1) | `stop_record` | RILEVATA |

`m09` è un limite della prova: nessun caso fa lanciare a un controllo di `run_workflow` un'eccezione che non sia `stop_error` (store e JSON fermano con `stop_error`; resterebbe un `std::bad_alloc`, che nessun test inietta nel workflow).

## Scelte

- **Sede del record.** Una riga di `settings` invece di una tabella nuova: lo schema resta alla versione 1, nessuna migrazione e nessuno store di versione diversa da rifiutare. Una tabella avrebbe richiesto `project_schema_version = 2`, che i test dello store fissano a 1 (`test_store.cpp`, `test_store_crash.cpp`, `test_store_review.cpp`). I nomi di workflow sono `[a-z0-9_-]`, quindi `workflow_stop.` non collide con `workflow.<nome>`; nessun codice del prodotto enumera `settings`.
- **Identità.** Il digest è quello della definizione data alla chiamata (per `resume.recipe` quella nuova, non quella registrata, che resta nel contesto come `registrata`). L'esecuzione distingue i rifiuti successivi della stessa definizione; `insert_setting` impedisce che un record ne sostituisca un altro.
- **Perimetro dei rifiuti.** Ogni eccezione dei controlli prima del primo passo, compresi `workflow.definition` ed errori dello store; un'eccezione non `stop_error` si registra come `integrity` `job.unexpected_exception` e si rilancia invariata, come `stop_before_work`. Gli arresti dentro `execute` restano fuori: `workflow.step_job` e il punto di test `workflow.step_end` avvengono dopo il job del passo; il punto di test `workflow.step_begin` precede `step.run()`, anche al primo passo, ma è un iniettore solo di test (`workflow::hook`) e la sua esclusione è un limite dichiarato.
- **Errore della rimozione dopo il commit.** Fuori dal `try` del runner: il job è già `completed` e pubblicato, quindi l'errore si propaga invariato come quello a `commit.after_commit` (OB-02), senza chiudere il job una seconda volta.

## Limiti

- La prima esecuzione libera si cerca con una lettura per chiave da 1 in su: costo lineare nel numero di rifiuti della stessa definizione. Due rifiuti concorrenti della stessa definizione nello stesso processo non si sovrascrivono: il secondo trova la chiave occupata e propaga l'arresto originale con `registrazione_arresto`.
- Se lo store non si apre o è in sola lettura, nessun record si scrive: l'arresto si propaga con `registrazione_arresto` e la diagnosi su stderr (limite già dichiarato in OB-03).
- `m09` sopravvissuta (sopra).
- Un guasto iniettato al punto di test `workflow.step_begin` del primo passo ferma il workflow prima di ogni job senza record di arresto del workflow (vedi «Perimetro dei rifiuti»).
- La suite osserva solo il confine e non distingue la sede della rimozione di OB-19: la prova della sede sono `m01` e `m03` insieme (senza runner o senza dichiarazione il caso di pubblicazione fallisce).
- I 4 test su dati reali (`Fits.REAL_DATA_PUB_LFC_01`, `T-M2E-13.real_palomar_lfc`, `T-M2R2-09.real_palomar_product_masters`, `T-M2R1-09.public_samples`) non girano in questo worktree, che non ha il collegamento `data`: esercitano la calibrazione modificata, quindi vanno rieseguiti nel checkout principale. Per la stessa ragione `trace.py check` riporta 24 ID non coperti, come in I3.
- `tools/check.sh` si ferma al ctest dev per il test su dati reali: lint, `check_job_types.py` e `trace.py check` sono stati lanciati a parte.
- `docs/test-matrix.md` dice ancora «rosso atteso» per T-M3BASE-03: fuori dal perimetro di questa unità sui documenti.

## Review

- Giro 1: `tools/review.sh basei4-round1 tmp/basei4-facts-round1.md` (Codex `gpt-6-astra`, sola lettura), delta `tmp/basei4-round1.diff`, artefatto `.omc/artifacts/ask/codex-basei4-round1-20261030-1132.md`: FAIL. [MED] il contesto persistito del record non è verificato da nessun test (né il nuovo né `check_workflow_stop_record` della suite) e i tipi delle foglie JSON non sono controllati. [LOW] la motivazione dell'esclusione dei punti `workflow.step_*` è sbagliata: `workflow.step_begin` precede `step.run()`.
- Correzione (delta `tmp/basei4-round2.diff`): `Store.every_workflow_refusal_leaves_its_own_stop_record` confronta il contesto del record con quello dell'eccezione, coppia per coppia, e controlla che ogni foglia del JSON sia stringa o intero; mutazione `m10-ob03-record-without-context` (record senza `context`) aggiunta. Motivazione dell'esclusione corretta in «Scelte» e dichiarata nei «Limiti». Dopo il delta: `m10` RILEVATA, release 611 test e dev 605 con i soli fallimenti su dati reali, lint verde.
- Giro 2: `tools/review.sh basei4-round2 tmp/basei4-facts-round2.md`, delta `tmp/basei4-round2.diff`, artefatto `.omc/artifacts/ask/codex-basei4-round2-20261030-1140.md`: PASS, nessun rilievo.

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/BASE-confini.md` | `1b13b4db031eafeb75f998197e775163547625dfb626ddcbcacd07ebf9eea61e` |
| `src/core/calibration_job.cpp` | `beddb1fe3c9ab0ab1a320408c3593d562b34ea491729293a1c4a4c94748baeac` |
| `src/core/include/astro/job.hpp` | `8c60900de1782ff03eb74fb41f0aa6583223a8601008d1f0305dfa4447ff15e3` |
| `src/core/include/astro/sqlite_dag_store.hpp` | `83e225a4f173a1cfc3001b46b22394338edd1d6519130ada960957dec48b9505` |
| `src/core/include/astro/workflow.hpp` | `9afee58d34b3b806c1183e6eefe236fd5c5d43b14a73377623f9b8713df94358` |
| `src/core/job.cpp` | `9d9a61e9cc58b5bff036faae4889f71c1a0724fbdd78bdbcdc6408c5430d364d` |
| `src/core/sqlite_dag_store.cpp` | `e9575e27461fb15a7bf18889abab11aa06ecb1e01cfadaa223885a2ac680d961` |
| `src/core/stop_internal.hpp` | `583e74243a3e8c28fa0cc15e918639a0acc083e95a69ded2241066b1a52eb9c4` |
| `src/core/workflow.cpp` | `9eaadacfce9fb87b2d5dc4998411efc27d15c4d6084cd48595ae446a50ffa84a` |
| `tests/unit/test_conformance.cpp` | `3b5afd8777a27ca02f4c5a8004a51b2e1dfa1285ae70a3019a19d93b157bdd9d` |
| `tests/unit/test_store.cpp` | `3c4a68c93a20f81cd826cf867c7917df63e6b22b44868758396630d53314d9de` |

- Test release (`tmp/release-junit.xml`): totali 902, fast 884, slow 18, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 884, fast 884, slow 0, falliti 0
<!-- evidence:end -->
