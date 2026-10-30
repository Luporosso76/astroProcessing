# M2-R5d: arresto prima del lavoro registrato con la provenance (NFR-FBK-01.a, G-STOP, ADR-07.a)

Stato: **ESEGUITO, review chiusa (PASS al giro 2)**, 30 ottobre 2026. Chiude il rilievo BLOCCANTE della review del gate M2, giro 9 (`.omc/artifacts/ask/codex-m2gate-round9-20261030-0508.md` nel repository principale): dopo R5.c (`M2-R5c.md`) lo stop `cpu.fp_mode` al confine dei job si propagava senza job `failed` e senza provenance dell'arresto, contro NFR-FBK-01.a (`plans/M2-residui.md`, riga 438). Autore: agente Claude Opus 5.5. Contratto: commento «Arresto prima del lavoro» in `src/core/include/astro/job.hpp`, `stop_before_work_record` in `src/core/include/astro/provenance.hpp`, [ADR-05](../contracts/ADR-05-dag-provenance.md) (modello d'errore, paragrafo «Arresto prima del lavoro», riga T-M2B-12), [ADR-07.a](../contracts/ADR-07-cpu-simd.md) (paragrafo «Ambiente FP», riga T-M2R5-07).

## Ambiente

Linux 7.0.0-38-generic x86_64, 16 CPU con AVX2; GCC 15.2.0; CMake 4.2.3; Ninja 1.13.2; Python 3.14.4 con `-S`. Base `8ae2647` più le modifiche di R5.d non committate, worktree isolato. Piano attivo SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`. Build e test con `CMAKE_BUILD_PARALLEL_LEVEL=4`, `CTEST_PARALLEL_LEVEL=4`, `TMPDIR` sotto `tmp/` del worktree, `ulimit -c 0`; ctest solo con i preset. `data` è un collegamento simbolico al `data/` del repository principale, non versionato.

## Stato generato

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-05-dag-provenance.md` | `7b4b4d4a0610799ae5e405e7d8e50fb28061f2100e9441b9b33a65ea48b01bcb` |
| `docs/contracts/ADR-07-cpu-simd.md` | `81e23c10617e9040edb643a05cb560ff154f1505ecd41d8492a2c8cce4264a74` |
| `src/core/calibration_job.cpp` | `beddb1fe3c9ab0ab1a320408c3593d562b34ea491729293a1c4a4c94748baeac` |
| `src/core/import_job.cpp` | `945d3ee4045c7ff15e2292551a5cc34384ef4431b09181cfffe6816c9e0df487` |
| `src/core/include/astro/job.hpp` | `8c60900de1782ff03eb74fb41f0aa6583223a8601008d1f0305dfa4447ff15e3` |
| `src/core/include/astro/provenance.hpp` | `9d95b56f73c0a046c18c737013fa8dfceea31f43a3c959ec89aaa9e54f98417d` |
| `src/core/job.cpp` | `9d9a61e9cc58b5bff036faae4889f71c1a0724fbdd78bdbcdc6408c5430d364d` |
| `src/core/master.cpp` | `1aba5a2b6688d8d1c4e198b065589283ce62a35e30bf3866933e1c3d4b35aa9b` |
| `src/core/provenance.cpp` | `ba81e4469d42f3836dc7aa3ab5ef53509d17ad3526a5dca94e4c206e7e7db55e` |
| `tests/trace/M2.tsv` | `f559fc60e9888c586cc4917d095f222ffd8fc5d25b0196a67becfa891025bdcc` |
| `tests/unit/test_import.cpp` | `1da08927791cc7e1a1df0f238e77bfdb50c724ec1afff7d0ac52eef28f228f79` |
| `tests/unit/test_job.cpp` | `d01fee9cd6c8b2219a033df916676311c5f9fc59c8d9a01fac4d8b64614a6da1` |
| `tests/unit/test_simd.cpp` | `4f9255394776ce59d6832eeda7971d6fca44f06953977a5f9c15654f39a00536` |
| `tests/unit/test_store.cpp` | `3c4a68c93a20f81cd826cf867c7917df63e6b22b44868758396630d53314d9de` |
| `tests/unit/test_store_review.cpp` | `23e1dc408879523e5fe06725dd029ee0968f07ef21d7ee76aac76381987d034e` |

- Test release (`tmp/release-junit.xml`): totali 666, fast 659, slow 7, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 660, fast 660, slow 0, falliti 0
- Tracciabilità (`tests/trace/M2.tsv`, build `build/release`, junit `tmp/release-junit.xml`): trace: 63 ID nello stadio M2, 0 non coperti
<!-- evidence:end -->

## Modifiche

- `src/core/job.cpp`: `stop_before_work` registra un arresto del confine che precede `work` e lo rilancia invariato. Chiude il job `failed` (`cancelled` per quella categoria), avviandolo se non è avviato. Sullo store durevole scrive come artefatto `provenance` del nodo il record `stop_before_work_record`, costruito dalla sola chiave del nodo, dalle versioni, dalla generazione, dallo stato, dallo stop (categoria, codice, messaggio, contesto) e dall'eventuale `resume`. Non c'è `get_node`, né parametri, né JSON con numeri in virgola mobile. Un'eccezione non `stop_error` si registra come integrity `job.unexpected_exception` e si rilancia invariata. `node_of` sostituisce `*store.get_node(...)`, che con un nodo assente dereferenziava un `optional` vuoto. `prepare_job_start` (pubblica) registra un'eccezione della preparazione di un job durevole. `run_job` e `run_job_files` passano il controllo FP e il rifiuto dello store durevole in `run_job` da questo percorso.
- `src/core/provenance.cpp`: `stop_before_work_json` / `stop_before_work_canonical`, con `versions_json` in comune con `provenance_json` (stessa validazione della revisione).
- `src/core/calibration_job.cpp`, `src/core/master.cpp`: il controllo FP e la serializzazione delle risorse nel budget (`resource_config_json`, soglia PSI `double`) stanno in `prepare_job_start`.
- `src/core/import_job.cpp`: in `run_cache_rebuild_job` la costruzione del nodo (causa non valida: invalid_input `cache.rebuild_cause`) sta in `prepare_job_start`.
- Registrazione fallita: lo stop originale si propaga con la stessa categoria, lo stesso codice e lo stesso contesto, più `registrazione_arresto` con l'errore secondario. La diagnosi va anche su stderr. Se il contesto non si può aggiungere per memoria esaurita, si propaga lo stop originale invariato.

## Classe degli arresti prima del lavoro

| Arresto al confine | Dove | Trattamento | Test |
|---|---|---|---|
| Ambiente FP vietato, numeric `cpu.fp_mode` | `run_job`, `run_job_files`, ingresso di `run_calibration_job` e `run_master_job` (quindi `run_branches`, import, thumbnail, cache rebuild) | registrato | `Simd.T_M2R5_07_*_records_the_stop`, `Simd.T_M2R5_07_every_exception_unmasked_records_the_stop_without_a_signal` |
| Serializzazione delle risorse nel budget fallita (es. soglia PSI non finita, `dag.param_nonfinite`) | `run_calibration_job`, `run_master_job` | registrato | `Simd.T_M2B_12_resources_not_serializable_in_the_budget_record_the_stop_before_work` |
| Store durevole passato a `run_job`, invalid_input `job.durable_store_requires_run_job_files` | `run_job` | registrato (provenance durevole) | `Store.T_M2B_12_durable_store_in_run_job_records_the_stop_before_work`, `Review.P3_run_job_on_a_durable_store_stops_before_the_work` |
| Nodo del job avviato assente (graph `dag.missing_node`, contesto `nodo`) o illeggibile (es. integrity `store.node_row_invalid`), eccezione estranea nella lettura | `run_job`, `run_job_files` | registrato | `Job.T_M2B_12_unreadable_node_after_start_marks_the_job_failed_and_propagates`, `Store.T_M2B_12_unreadable_node_after_start_records_the_stop_before_work` |
| Causa non valida, invalid_input `cache.rebuild_cause` | `run_cache_rebuild_job` | registrato | `Thumbnail.T_M2R1_04_T_M2R1_11_a_corrupt_entry_stops_and_only_the_explicit_rebuild_replaces_it` |
| Registrazione fallita (scrittura della provenance, avvio del job per registrare) | tutti i precedenti | stop originale con `registrazione_arresto` | `Store.T_M2B_12_failed_stop_record_propagates_the_original_stop_with_the_secondary_failure` |
| Avvio del job fallito (`start_job`: slot inesistente o escluso, store in sola lettura, errore SQL) | `run_job`, `run_job_files` | **non registrabile**: è la scrittura del record che fallisce. Si propaga invariato, senza job e senza nuovo tentativo | `Job.T_M2B_12_failed_job_start_propagates_unchanged_without_a_job`, `Store.T_M2B_12_failed_job_start_propagates_unchanged_without_a_job` |
| Store non apribile (`sqlite_dag_store::open`/`create`) | prima di ogni confine | **non registrabile**: non esiste uno store in cui scrivere. L'errore si propaga col suo codice | — (fuori dal confine del job) |

Tutti gli arresti registrati si propagano come eccezione, con la stessa categoria, lo stesso codice e lo stesso contesto. Il chiamante ricava il codice di uscita con `exit_code_for`: 2, oppure 3 per `cancelled`.

## Test eseguiti

| Test | Cosa verifica |
|---|---|
| `Simd.T_M2R5_07_denormal_mask_off_stops_run_job_files_before_the_node_and_records_the_stop` | DM spenta, parametri subnormali, figlio con gestore di SIGFPE. Calibrazione e master con e senza budget (soglia PSI `2^-1074`), import e `run_branches` sullo store durevole: stop `cpu.fp_mode` propagato senza `registrazione_arresto`; per slot, i job `failed` numeric `cpu.fp_mode` sul nodo (2, 2, 1, 1) e la generazione corrispondente; per nodo, solo provenance `stop_before_work` con `mxcsr`, senza `node_spec`, `input_digests`, `budget`, `details`; nessun kernel |
| `Simd.T_M2R5_07_denormal_mask_off_stops_run_job_before_the_node_and_records_the_stop` | Store in memoria: `run_job` e `run_branches` registrano il job `failed`; nessun artefatto, lavoro non eseguito |
| `Simd.T_M2R5_07_job_with_ftz_stops_before_any_kernel_and_records_the_stop` | FZ: calibrazione e master registrano un job `failed` e la sola provenance dell'arresto; nessun kernel, slot `missing` |
| `Simd.T_M2R5_07_every_exception_unmasked_records_the_stop_without_a_signal` | Tutte e sei le eccezioni smascherate (anche PM): stop e registrazione completa sullo store durevole senza segnale |
| `Simd.T_M2B_12_resources_not_serializable_in_the_budget_record_the_stop_before_work` | Soglia PSI NaN con budget: `dag.param_nonfinite` registrato e propagato, per calibrazione e master |
| `Store.T_M2B_12_unreadable_node_after_start_records_the_stop_before_work` | `params_canonical` alterato: job `failed` con la causa della lettura, provenance con chiavi esatte `generation`, `node_key`, `state`, `stop`, `stop_before_work`, `versions` |
| `Store.T_M2B_12_durable_store_in_run_job_records_the_stop_before_work` | Rifiuto dello store durevole in `run_job` registrato con provenance |
| `Store.T_M2B_12_failed_stop_record_propagates_the_original_stop_with_the_secondary_failure` | `fsync` della provenance fallito: stop originale con `registrazione_arresto` = errore io, job `failed` senza provenance; store in sola lettura: `registrazione_arresto` = `schema store.read_only`, nessun job |
| `Store.T_M2B_12_failed_job_start_propagates_unchanged_without_a_job`, `Job.T_M2B_12_failed_job_start_propagates_unchanged_without_a_job` | Slot inesistente e store in sola lettura: errore di avvio invariato, nessun job, nessun contesto aggiunto |
| `Job.T_M2B_12_unreadable_node_after_start_marks_the_job_failed_and_propagates` | Store in memoria con nodo assente (`dag.missing_node`, contesto `nodo`) o lettura che lancia `std::runtime_error` (job `job.unexpected_exception`, eccezione originale invariata), in un figlio con SIGSEGV convertito |
| `Thumbnail.T_M2R1_04_T_M2R1_11_*` (blocco aggiunto) | Causa non valida in `run_cache_rebuild_job`: stop propagato, job registrato con provenance `stop_before_work`, nessuna ricostruzione |
| `Review.P3_run_job_on_a_durable_store_stops_before_the_work` (rinominato da `..._stops_before_starting_the_job`) | Attesa cambiata: job 1 `failed` invece di nessun job |

## Rosso prima del codice

`tmp/r5d-red.log`: test scritti, codice di HEAD; 8 test falliti su 21 selezionati. Il caso del nodo assente in memoria esce con 139 (SIGSEGV del figlio). Il blocco di `test_import.cpp`, il test della serializzazione delle risorse e l'attesa nuova di `Review.P3` sono scritti dopo il codice: il loro rosso è dato dalle mutazioni `cache_rebuild_unrecorded`, `*_resources_unrecorded` e `run_job_durable_unrecorded`. Verde: `tmp/r5d-green.log`.

## Mutazioni

Con `tools/mutate.sh` sul preset release e la selezione `T_M2R5_07|T_M2B_12|T_M2R1_11|Review.P3_` (23 test). Ogni volta: baseline verde, patch, ripristino verificato con `cmp`. Patch generate da `tmp/r5d-make-mutations.py` (`tmp/r5d-mut-<nome>.patch`), log `tmp/r5d-mut-<nome>.log`, riepilogo `tmp/r5d-mutations.log`.

| Mutazione | Cosa toglie | Esito |
|---|---|---|
| `no_record` | registrazione dell'arresto (solo rilancio) | RILEVATA |
| `record_with_params` | record costruito col nodo letto e la provenance completa con i parametri | RILEVATA (anche uscita 136, SIGFPE, con DM spenta) |
| `node_read_unrecorded` | registrazione dell'errore di lettura del nodo | RILEVATA |
| `no_record_failure_context` | contesto `registrazione_arresto` | RILEVATA |
| `secondary_replaces_original` | errore secondario propagato al posto dello stop | RILEVATA |
| `start_failure_recorded` | avvio fallito trattato come arresto da registrare (secondo tentativo) | RILEVATA |
| `run_job_durable_unrecorded` | registrazione del rifiuto dello store durevole in `run_job` | RILEVATA |
| `calibration_entry_unrecorded`, `master_entry_unrecorded` | controllo FP non registrato all'ingresso del job di calibrazione e del master | RILEVATA |
| `calibration_resources_unrecorded`, `master_resources_unrecorded` | serializzazione delle risorse fuori da `prepare_job_start` | RILEVATA |
| `cache_rebuild_unrecorded` | causa non valida di `cache.rebuild` fuori da `prepare_job_start` | RILEVATA |

Riepilogo: 12 mutazioni, 12 RILEVATE. Un primo passaggio (`tmp/r5d-mutations-pass1.log`) aveva dato 4 errori dello strumento. In tre casi il blocco di `test_import.cpp` abortiva su un campo assente invece di fallire con gtest, e il test è stato corretto. Nel quarto la patch `secondary_replaces_original` non compilava (funzione inutilizzata) ed è stata riscritta.

## Esecuzioni

Sul codice del giro 2, con `ulimit -c 0`, `CMAKE_BUILD_PARALLEL_LEVEL=4`, `CTEST_PARALLEL_LEVEL=4`:

- `ctest --preset release --output-junit tmp/release-junit.xml`: 515 test (slow compresi), 0 falliti (`tmp/r5d-release-ctest.log`).
- `tools/check.sh`: uscita 0; build dev, `ctest --preset dev` 509 test, 0 falliti; `lint: ok` (`tmp/r5d-check.log`). Il controllo dei blocchi generati, informativo, segnalava non aggiornati anche questo record, il cui blocco è stato rigenerato dopo (`python3 -S tools/evidence.py docs/evidence/M2-R5d.md --check` esce con 0), e altri record fuori da R5.d che citano file cambiati qui o junit diversi. La riga di trace del preset dev usa il junit dev, senza i test `slow`, ed è informativa.
- `python3 -S tools/trace.py check --stage M2 --map tests/trace/M2.tsv --build-dir build/release --ctest-junit tmp/release-junit.xml`: uscita 0, 63 ID, 0 non coperti (`tmp/r5d-trace.txt`).
- Blocco generato con `python3 -S tools/evidence.py docs/evidence/M2-R5d.md`.
- Non eseguiti qui: la build pulita da zero (dev e release) e l'esecuzione finale del gate, che restano al coordinatore.

## Review

- Giro 1 (`.omc/artifacts/ask/codex-r5d-round1-20261030-0642.md`): FAIL, 4 MED. (1) Il divieto di un secondo avvio non era verificato direttamente: ora `Job.T_M2B_12_failed_job_start_*` conta le chiamate a `start_job` (una) e `Store.T_M2B_12_failed_job_start_*` verifica che non si tenti alcuna registrazione (stderr vuoto, contesto invariato). (2) Il contesto originale non vuoto e il confronto integrale: nuovo `Job.T_M2B_12_failed_record_keeps_the_original_context_and_adds_only_the_secondary_failure`; i controlli della provenance confrontano per intero contesto e messaggio (`expect_stop_before_work` in `test_store.cpp`, parametro `expected` di `only_stop_before_work_provenance` in `test_simd.cpp`). (3) `cancelled`, `resume` e rifiuti: nuovi `Provenance.T_M2B_12_stop_before_work_record_text_and_rejections` (testo canonico esatto, stati e categorie incoerenti, revisione assente) e `Store.T_M2B_12_cancelled_stop_before_work_keeps_the_resume_context`. (4) Record completato. Delta: `tmp/r5d-round2.diff` (solo test; i file del giro 1 ricostruiti in `tmp/r5d-round1/`). Mutazione `start_failure_recorded` rieseguita sui soli `T_M2B_12`: RILEVATA (`tmp/r5d-mut-start_failure_recorded-round2.log`).
- Giro 2 (`.omc/artifacts/ask/codex-r5d-round2-20261030-0651.md`): **PASS** con un LOW (resoconto degli avvisi di `evidence` in `check.sh`), corretto nella sezione «Esecuzioni».

## Limiti

- Lo store in memoria non persiste provenance. Per `run_job` il record di un arresto prima del lavoro è il job `failed` con categoria, codice e chiave del nodo. Lo stop propagato non porta un `job_outcome`.
- Un fallimento dell'avvio del job e uno store non apribile non si registrano (tabella sopra): la diagnosi è l'errore propagato, col codice di uscita di `exit_code_for`.
- Gli arresti a livello di workflow prima di ogni job (`resume_workflow` con ricetta diversa, definizione non valida) non appartengono a un job e restano come prima: stop propagato, nessun job.
- La registrazione con l'ambiente FP vietato è provata con DM spenta, con FZ e con tutte le eccezioni smascherate. DAZ e RC da soli non generano segnali e il record non contiene numeri in virgola mobile.
- Uno stop `cpu.fp_mode` dentro `work` (worker o preflight, dopo un cambio di MXCSR a job avviato) scrive ancora la provenance completa del job, con i parametri letti prima. Nessun codice del prodotto cambia MXCSR durante un job; i test dei worker usano parametri normali.
