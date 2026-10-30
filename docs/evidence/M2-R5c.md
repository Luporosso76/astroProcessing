# M2-R5c: ambiente FP ai confini dei job e all'avvio dei task (NFR-FBK-01.a, ADR-07.a, G-STOP)

Stato: **ESEGUITO, review chiusa (PASS al giro 2)**, 30 ottobre 2026. Chiude il rilievo 1 (BLOCCANTE) della review del gate M2, giro 8 (`.omc/artifacts/ask/codex-m2gate-round8-20261030-0347.md` nel repository principale). Il rilievo riguardava due limiti di `M2-R5.md`. Con DM spenta e un parametro subnormale, il job riceveva SIGFPE in `run_job_files` durante la lettura del nodo, prima del controllo di MXCSR. Nella residenza a blocchi il controllo del worker seguiva `read_tile`. Autore: agente Claude Opus 5.5. Contratto: [ADR-07.a](../contracts/ADR-07-cpu-simd.md), paragrafo «Ambiente FP» e riga T-M2R5-07; commento del confine in `src/core/include/astro/job.hpp`.

## Ambiente

Linux 7.0.0-38-generic x86_64, 16 CPU con AVX2; GCC 15.2.0 (Ubuntu 15.2.0-16ubuntu1); CMake 4.2.3; Ninja 1.13.2; Python 3.14.4 con `-S`. Base `1c5e1ae` più le modifiche di R5.c non committate, worktree isolato. Piano attivo SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`. Build e test con `CMAKE_BUILD_PARALLEL_LEVEL=4`, `CTEST_PARALLEL_LEVEL=4`, `TMPDIR` e `CCACHE_DIR` sotto `tmp/` del worktree, `ulimit -c 0`. `data` è un collegamento simbolico al `data/` del repository principale, non versionato.

## Stato generato

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-07-cpu-simd.md` | `81e23c10617e9040edb643a05cb560ff154f1505ecd41d8492a2c8cce4264a74` |
| `src/core/calibration_job.cpp` | `beddb1fe3c9ab0ab1a320408c3593d562b34ea491729293a1c4a4c94748baeac` |
| `src/core/include/astro/job.hpp` | `8c60900de1782ff03eb74fb41f0aa6583223a8601008d1f0305dfa4447ff15e3` |
| `src/core/include/astro/scheduler.hpp` | `617d499fe6c5177b3d588e5f00f77a705fbc67a1ebf2b08c8fcfd0f14dd064ad` |
| `src/core/job.cpp` | `9d9a61e9cc58b5bff036faae4889f71c1a0724fbdd78bdbcdc6408c5430d364d` |
| `src/core/master.cpp` | `1aba5a2b6688d8d1c4e198b065589283ce62a35e30bf3866933e1c3d4b35aa9b` |
| `src/core/scheduler.cpp` | `9588b0adfc402c790329ee0f837972128a3548ef82b5012055f228c429d38069` |
| `tests/trace/M2.tsv` | `f559fc60e9888c586cc4917d095f222ffd8fc5d25b0196a67becfa891025bdcc` |
| `tests/unit/test_simd.cpp` | `4f9255394776ce59d6832eeda7971d6fca44f06953977a5f9c15654f39a00536` |

- Test release (`tmp/release-junit.xml`): totali 719, fast 712, slow 7, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 713, fast 713, slow 0, falliti 0
- Tracciabilità (`tests/trace/M2.tsv`, build `build/release`, junit `tmp/release-junit.xml`): trace: 63 ID nello stadio M2, 0 non coperti
<!-- evidence:end -->

## Modifiche

- `src/core/job.cpp`: `require_fp_environment()` all'inizio di `run_job` e di `run_job_files`, prima di `start_job` e di `get_node`. Con un modo vietato lo stop `numeric` `cpu.fp_mode` si propaga dal confine e nessun job esiste. È la regola del confine per un job che non parte (`job.hpp`). Un job avviato non può registrare l'arresto: la sua provenance riscriverebbe i parametri del nodo, e leggerli è proprio l'operazione da evitare.
- `src/core/scheduler.cpp`: `require_fp_environment()` in `worker_loop`, nel thread del worker, dopo il controllo di annullamento e prima della prenotazione del task e di `spec.run_task`. Lo stop segue il percorso degli altri errori dei task (`st.fail`, poi rilancio nel thread del job).
- `src/core/calibration_job.cpp`, `src/core/master.cpp`: `require_fp_environment()` come prima istruzione di `run_calibration_job` e `run_master_job`. Con il budget presente, queste funzioni serializzano la configurazione delle risorse (`resource_config_json`, con la soglia PSI in `double`) prima di chiamare `run_job_files`.
- Il controllo non corregge mai MXCSR. Codice, categoria e contesto dello stop sono quelli di R5. Il controllo nel preflight, nel lavoro dei job e in `begin_kernel_task` resta com'era.

## Confini coperti

| Confine | Chi lo attraversa | Controllo |
|---|---|---|
| `run_calibration_job`, `run_master_job` | chiamanti dei job di calibrazione e master, con o senza budget | all'ingresso, prima di `resource_config_json` e di `run_job_files` |
| `run_job_files` | `run_calibration_job`, `run_master_job`, `run_import_job`, `run_thumbnail_job`, `run_cache_rebuild_job`, `run_branches(sqlite_dag_store&)` | all'ingresso, prima di `start_job` e `get_node` |
| `run_job` | `run_branches(dag_store&)` | all'ingresso, dopo il rifiuto dello store durevole e prima di `start_job` |
| task di `job_scheduler::run` | residenza intera e a blocchi della calibrazione, bande del master | in `worker_loop`, prima di `run_task`, quindi prima di `read_tile` e `read_fits_rows` |
| workflow e resume | `run_workflow`, `resume_workflow` | ogni passo esegue un job dei confini sopra |
| recovery dello store | `sqlite_dag_store::recover` | nessun valore in virgola mobile: interi, stringhe e file; nessun controllo aggiunto |

## Test eseguiti

Tutti in `tests/unit/test_simd.cpp`, preset release e dev. I quattro test nuovi eseguono i confini in un processo figlio (`EXPECT_EXIT`) con un gestore di SIGFPE che esce con 136 e SIGABRT convertito in uscita 134 (`no_crash_report.hpp`). Senza il controllo il figlio muore col segnale e il test fallisce, senza abbattere il processo dei test e senza core.

| Test | Cosa verifica |
|---|---|
| `Simd.T_M2R5_07_denormal_mask_off_stops_run_job_files_before_the_job_and_the_node` | DM spenta (`0x1e80`), parametri `q_min = 2^-1074` (calibrazione) ed `exposure_rel_tol = 2^-1074` (master). Job di calibrazione e master con budget presente e soglia PSI `2^-1074` nelle risorse, poi senza budget; job di import e `run_branches` sullo store durevole si fermano con `cpu.fp_mode` e la sola DM nel contesto `mxcsr`, registro non corretto; generazione degli slot 0 (nessun job avviato), nessun artefatto, nessun kernel |
| `Simd.T_M2R5_07_denormal_mask_off_stops_run_job_before_the_job_and_the_work` | Store in memoria, nodo con parametro `2^-1074`: `run_job` e `run_branches` si fermano prima di avviare il job; il lavoro, che moltiplica un operando subnormale, non viene eseguito |
| `Simd.T_M2R5_07_denormal_mask_off_in_the_workers_stops_each_block_before_decoding` | Residenza a blocchi (checkpoint, 2 worker, blocchi di 2 righe), light e dark f32 con campioni `2^-149`. Controllo con MXCSR regolare: il job pubblica e il piano ha lo spill. Con DM spenta dall'hook `preflight.done`, i worker la ereditano: stop `cpu.fp_mode` dai worker, sola provenance dell'arresto, nessun kernel dopo il preflight |
| `Simd.T_M2R5_07_denormal_mask_off_in_the_workers_stops_each_master_band_before_reading` | Master dark su 3 frame f32 con campioni subnormali, 2 worker, blocchi di 2 righe. Controllo regolare pubblicato; con DM spenta dall'hook `frames.read`: stop `cpu.fp_mode` dai worker, sola provenance, nessun kernel |
| `Simd.T_M2R5_07_job_with_ftz_stops_before_any_kernel_and_publishes_nothing` (modificato) | Con FZ i job di calibrazione e master si fermano al confine: lo stop si propaga con FZ nel contesto, generazione 0, nessun artefatto, nessun kernel. Prima di R5.c il test si aspettava un job fermato con la provenance dell'arresto |

## Rosso prima del codice

`tmp/r5c-red.log`, con i test scritti e il codice di HEAD. I quattro test nuovi falliscono con uscita 136 (SIGFPE nel figlio). Il test FZ fallisce perché nessuno stop si propaga: il job parte e si ferma dentro il lavoro. Verde dopo il codice: `tmp/r5c-green.log`, 9 test `T_M2R5_07` passati.

## Mutazioni

Una per controllo nuovo, con `tools/mutate.sh` sul preset release e la selezione `T_M2R5_07` (9 test). Ogni volta: baseline verde, patch, ripristino verificato con `cmp`.

| Mutazione | Patch | Test che la rilevano | Esito |
|---|---|---|---|
| `run_job` senza controllo | `tmp/r5c-mut-run_job.patch` | `..._stops_run_job_before_the_job_and_the_work` (uscita 136) | RILEVATA (`tmp/r5c-mut-run_job.log`) |
| `run_job_files` senza controllo | `tmp/r5c-mut-run_job_files.patch` | `..._stops_run_job_files_before_the_job_and_the_node`, `..._job_with_ftz_*` | RILEVATA (`tmp/r5c-mut-run_job_files.log`) |
| `worker_loop` senza controllo | `tmp/r5c-mut-worker.patch` | `..._stops_each_block_before_decoding`, `..._stops_each_master_band_before_reading` (uscita 136) | RILEVATA (`tmp/r5c-mut-worker.log`) |
| `run_calibration_job` senza controllo d'ingresso (giro 2) | `tmp/r5c-mut-cal_entry.patch` | `..._stops_run_job_files_before_the_job_and_the_node` (uscita 136, caso con budget) | RILEVATA (`tmp/r5c-mut-cal_entry.log`) |
| `run_master_job` senza controllo d'ingresso (giro 2) | `tmp/r5c-mut-master_entry.patch` | stesso test (uscita 136, caso con budget) | RILEVATA (`tmp/r5c-mut-master_entry.log`) |

Riepilogo: `tmp/r5c-mutations.log`, 5 mutazioni, 5 RILEVATE.

## Esecuzioni

Sul codice del giro 2, con `ulimit -c 0`:

- `ctest --preset release --output-junit tmp/release-junit.xml` (`CTEST_PARALLEL_LEVEL=4`): 504 test, 0 falliti (`tmp/r5c-release-ctest.log`).
- `tools/check.sh` (`CMAKE_BUILD_PARALLEL_LEVEL=4`, `CTEST_PARALLEL_LEVEL=4`): uscita 0; build dev, `ctest --preset dev` 498 test, 0 falliti; `lint: ok` (`tmp/r5c-check.log`). La riga di trace del preset dev («38 non coperti») usa il junit dev, senza i test `slow`, ed è informativa. Il blocco generato di altri record risulta non aggiornato: `M2-gate.md`, `M2-R0*`, `M2-R1`, `M2-R2*`, `M2-R32`, `M3-entry-*`. È atteso: quei record sono fuori da R5.c e citano file cambiati qui (`tests/trace/M2.tsv`, i sorgenti dei job) o junit diversi.
- `python3 -S tools/trace.py check --stage M2 --map tests/trace/M2.tsv --build-dir build/release --ctest-junit tmp/release-junit.xml`: uscita 0, 63 ID, 0 non coperti (`tmp/r5c-trace.txt`).
- Blocco generato rigenerato con `python3 -S tools/evidence.py` per questo record e per `M2-R5.md`.

## Review

- Giro 1 (`.omc/artifacts/ask/codex-r5c-round1-20261030-0415.md`): FAIL, 2 MED. Primo: con il budget presente, `run_calibration_job` e `run_master_job` serializzano le risorse (soglia PSI `double`) prima di `run_job_files`. Corretto con il controllo all'ingresso delle due funzioni e con il caso «con budget» nel test dei confini (mutazioni del giro 2). Secondo: record incompleto, completato qui. Delta: `tmp/r5c-round2.diff`.
- Giro 2 (`.omc/artifacts/ask/codex-r5c-round2-20261030-0426.md`): **PASS** con un LOW (questo record attribuiva ai worker anche le prove delle altre maschere), corretto nella sezione «Limiti».

## Limiti

- Lo stop dei confini precede il job: non c'è un record `failed` né una provenance dell'arresto nello store. Lo stop arriva al chiamante come eccezione `stop_error`, con categoria, codice e contesto di R5.
- `run_branches` propaga lo stop del primo ramo, come ogni errore di avvio di un job: i rami non partono e non si ottiene un vettore di esiti.
- La DM spenta nei worker si prova ereditata dal thread del job (hook di test dopo il preflight). FZ, DAZ e RC nei worker restano provati con il `fault_injector` di R5, nel kernel; le altre maschere di eccezione solo nello stesso thread (preflight e avvio del task), come dichiarato in `M2-R5.md`.
- `job_scheduler::run` non controlla MXCSR prima di `spec.prepare`: il thread del job è già controllato dal confine e dal lavoro del job, e nessun codice del prodotto cambia MXCSR fra i due punti.
