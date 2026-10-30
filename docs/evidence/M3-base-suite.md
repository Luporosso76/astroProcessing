# M3, base comune: suite di conformità T-M3BASE-01…19 (fase rossa)

Stato: **CHIUSO per arbitraggio dopo il giro 2** (vedi «Arbitraggio»). Unità: M3, base comune, passo 2 di «Base comune prima del codice delle slice» in `plans/M3.md`. Fonte normativa: `docs/contracts/BASE-confini.md` (§3 suite, §4 debiti), record del contratto `docs/evidence/M3-base-contratto.md`. Questa unità scrive solo il registro dei tipi, i punti di osservazione per i test e la suite: i debiti del §4 li paga l'unità di codice successiva.

| Campo | Valore |
|---|---|
| Unità | M3 / base comune, passo 2 (suite di conformità) |
| Scopo | Registro unico dei tipi di job, controllo di completezza, suite T-M3BASE-01…19 sul registro, fase rossa sui debiti |
| Comando | `ctest --preset release` (4 job, junit `tmp/release-junit.xml`, log `tmp/release-full.log`) dopo `cmake --build --preset release` (4 job); `cmake --preset dev`; `cmake --build --preset dev` (4 job); `ctest --preset dev -R "Conformance\|M3BASE"` con `CTEST_PARALLEL_LEVEL=4`, log in `tmp/base-suite-red.log`; `ctest --preset dev` completo (4 job), junit in `tmp/dev-junit.xml`; `tools/lint.sh`; `python3 -S tools/evidence.py docs/evidence/M3-base-suite.md` |
| Input | Codice al commit `05ab379` più le modifiche elencate sotto; fixture sintetiche generate da `tests/fixtures/conformance_inputs.hpp` |
| Versioni | GCC 15.2.0, CMake 4.2.3, preset `dev` (ASan e UBSan) |
| Ambiente | Linux 7.0.0-38-generic x86_64, 16 CPU, worktree isolato |
| Risultato | Suite registrata in ctest: 89 test (86 della suite gtest, più rete, registro e controllo negativo del registro). Verdi: 61, fra cui tutti i casi degli obblighi IMPLEMENTATO NEL PROPRIETARIO. Rossi: 28, tutti su obblighi con debito nel §4; uno (T-04 su `cache.rebuild`) su un punto che la riga OB-04 del §4 non elenca. Suite completa del preset `dev`: 598 test, falliscono solo i 28 rossi attesi. Preset `release` dopo l'arbitraggio: 604 test, falliscono gli stessi 28 |
| Esito | Fase rossa conforme alla consegna: 28 rossi attesi, gli stessi in `dev` e in `release`, nessun altro test fallito; casi verdi su obblighi con debito spiegati sotto. Review FAIL al giro 2, chiusa per arbitraggio con le correzioni dei 2 BLOCCANTI |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Registro dei tipi di job (§3.1)

- `src/core/include/astro/job_types.hpp`, `src/core/job_types.cpp`: `registered_job_types()` è l'unico elenco. Ogni voce porta la costante `*_operator_id`, i ruoli degli input, le varianti dichiarate prima dell'esecuzione (calibrazione: `intera`, senza scratch, e `blocchi`, con checkpoint; per ogni variante `checkpoint` e `scratch`, cioè se il job riceve lo scratch di `resources`), il passaggio dallo scheduler e il costruttore del job minimo (`make`), che restituisce il nodo e una chiamata che passa da `run_job_files`.
- Ricette minime: calibrazione `dark_matched_flat_dark_flat` con master dichiarati (media di 4), modello di rumore, normalizzazione globale, `simd` scalare; master dark di 3 frame, media senza rejection; import, miniatura e ricostruzione della cache con anteprima a blocchi 2x2.
- Precondizione di `cache.rebuild`: la voce di cache esiste. `make` la crea nel progetto prima di ogni esecuzione, fuori dal job e prima che la suite installi la sonda.
- Controllo di completezza: `tools/check_job_types.py` (`python3 -S`) confronta le costanti `*_operator_id` di `src/core/include/astro/`, le voci del registro e le funzioni di `src/core/*.cpp` che chiamano `run_job_files` (escluse le composizioni `run_branches`) con le funzioni di job chiamate dal registro. In ctest come `T-M3BASE.job_types_registry`; `T-M3BASE.job_types_registry_negative` (`--self-test`) prova che un tipo non registrato fa fallire il controllo.
- Input sintetici: `tests/fixtures/conformance_inputs.hpp`, un FITS f32 13x6 per ruolo (header di calibrazione, fase raw, linearità dichiarata), scritto da chi non scrive gli operatori.

## Punti di osservazione aggiunti al prodotto

Solo per i test, inerti senza osservatore (`job_probe::active()` evita di calcolare i dettagli costosi). Dichiarati in `src/core/include/astro/job_probe.hpp` e in `job.hpp` (`namespace job_probe`).

| Punto | Dove | Perché serve |
|---|---|---|
| `set_work_wrapper` / `run_work` | `run_job_files` (`job.cpp`) invoca `work` attraverso l'involucro | §3.2: il `work` della suite avvolge quello dell'operatore, conta le invocazioni (T-04, T-10, T-13) e ospita le iniezioni «dentro work» (T-02, T-06, T-09, T-14, T-16, T-17) |
| `node.build` | costruttore di `node_spec` (`node.cpp`) | T-04: «nessuna chiamata al costruttore del nodo» |
| `input.open` | ingresso di `open_fits_input` (`fits.cpp`) | T-10: contatore delle aperture |
| `input.read` | ogni `pread_exact` (`fits.cpp`) | T-07: blocchi letti nel `prepare` dopo il cancel |
| `input.samples` | `decode_fits` (dettaglio `sha256:` dei byte), `read_fits_rows`, `fits_block_source::read_tile` e controlli per campione della P0, ciclo dei campioni di `make_preview` (dettaglio `ino:` del file aperto) | T-11: nessun campione dell'input alterato usato prima dello stop |
| `sched.plan` | ingresso di `job_scheduler::run`, con il `job_plan` serializzato | T-08 e T-09: piano eseguito contro piano registrato e picco; T-11: istante del piano |
| `sched.prepare` | prima di `spec.prepare` | T-07: cancel nel `prepare` |
| `sched.task`, `sched.task_done` | subito prima e dopo `spec.run_task` (`worker_loop`) | T-05: invocazioni di `run_task`; T-07: blocchi elaborati dopo il cancel; T-11, T-15, T-19: iniezioni durante i task |
| `sched.finalize` | prima di `spec.finalize` | T-07: nessun `finalize` |

Fuori da questi punti nessun codice di prodotto è cambiato. Le iniezioni usano anche i seam esistenti: `fault_injector` dello store (T-02, T-13), `cpu_test::set_task_injector` al punto `cpu.task_start` (T-05), MXCSR in un processo figlio con SIGFPE convertito in uscita 136 (T-04, T-05, come `tests/unit/test_simd.cpp`).

## Casi

Ogni caso gira per ogni voce e variante del registro (6 parametri: `io.import`, `io.thumbnail`, `cache.rebuild`, calibrazione `intera` e `blocchi`, `master.combine`); T-05 per i tipi con scheduler, T-15 e T-19 per le varianti con checkpoint, T-03 e T-16 una volta (il workflow di T-16 ha un passo per tipo). Le verifiche comuni (a)-(e) sono in `check_common`.

| Caso | Esito per tipo | Debito |
|---|---|---|
| T-M3BASE-01 | verde per tutti | — |
| T-M3BASE-02 | verde per tutti | — |
| T-M3BASE-03 | **rosso** | OB-03: nessun record di arresto del workflow nello store (`workflow.slot_target`, `resume.recipe`) |
| T-M3BASE-04 | **rosso** `cache.rebuild`; verde gli altri | OB-04: `run_cache_rebuild_job` costruisce il nodo nel `prepare` di `prepare_job_start` (`import_job.cpp:388`) prima del controllo FP del runner; 1 `node.build` con MXCSR vietato, in tutti e quattro i modi. Il punto non è nella riga OB-04 del §4 |
| T-M3BASE-05 | **rosso** calibrazione `intera`, `blocchi`, master | OB-05: `run_task` invocato 2 volte con MXCSR vietato in ogni modo; il punto `cpu.task_start` sta in `begin_kernel_task`, dentro `run_task`. Arresto `cpu.fp_mode` e zero kernel come atteso |
| T-M3BASE-06 | verde per tutti | — |
| T-M3BASE-07 | **rosso** import, miniatura, ricostruzione, calibrazione `intera` e `blocchi`; verde master | OB-07. Import, miniatura, ricostruzione: fuori dallo scheduler, nessun punto `sched.prepare`/`sched.task`, il job pubblica (anche OB-08). `intera`: 8 letture nel `prepare` dopo la richiesta (`calibration_job.cpp:265-298`). `blocchi`: 72 letture dopo la richiesta, nella passata P0 (`calibration_blocks.cpp:269-272`, prima del primo controllo a `:300`); la riga del §4 non nomina la P0 |
| T-M3BASE-08 | **rosso** import, miniatura, ricostruzione; verde calibrazione e master | OB-08: nessun `job_plan` allo scheduler (`sched.plan` assente); con il pool occupato il job non entra nella coda FIFO |
| T-M3BASE-09 | **rosso** import, miniatura, ricostruzione; verde calibrazione e master | OB-09 (e OB-08): budget senza `resources`, `details.cpu` assente con backend `cpu_*`, nessun `details.resource_plan` |
| T-M3BASE-10 | **rosso** per tutti | OB-10: `work` invocato 1 volta; il confronto delle chiavi è dentro `work`. Codice `*.node_mismatch`, chiavi nel contesto e zero aperture risultano già corretti |
| T-M3BASE-11 | **rosso** per tutti | OB-11. Import, miniatura, ricostruzione: «cambiato dopo la verifica» senza punto di iniezione (fuori dallo scheduler, anche OB-08). `intera`: file cambiato durante i task pubblicato senza riverifica; BITPIX del piano non riconfrontato (job pubblicato); NAXIS1 e NAXIS2: lo stop `input.geometry_changed` arriva dopo `decode_fits` dell'input. `blocchi`: BITPIX non riconfrontato (job pubblicato). Master: BITPIX non riconfrontato, 6 task e kernel eseguiti prima dello stop; NAXIS1 e NAXIS2 confrontati dopo la decodifica. File assente e byte cambiato (zero campioni dell'input alterato usati prima dello stop): verdi per tutti; NAXIS1 e NAXIS2: verdi per `blocchi` e per i tipi di import |
| T-M3BASE-12 | verde per tutti | — |
| T-M3BASE-13 | verde per tutti | — |
| T-M3BASE-14 | verde per tutti (orfani rimossi alla riapertura: ipotesi di OB-14 verificata) | — |
| T-M3BASE-15 | verde (`blocchi`) | — |
| T-M3BASE-16 | verde | — |
| T-M3BASE-17 | verde per tutti; regola sul sorgente verde | — |
| T-M3BASE-18 | verde (`T-M3BASE-18.network`) | — |
| T-M3BASE-19 | verde (`blocchi`) | vedi sotto |

Test rossi registrati in ctest, senza `DISABLED_`, skip o etichette di esclusione (etichetta `fast` come gli altri). Restano rossi finché l'unità di codice non paga il debito: non sono regressioni. Elenco completo, identico in `dev` (`tmp/base-suite-red.log`) e in `release` (`tmp/release-full.log`):

| # | Test ctest | Debito |
|---|---|---|
| 1 | `ConformanceWorkflow.T_M3BASE_03_workflow_refusal_leaves_a_stop_record` | OB-03 |
| 2 | `Types/ConformanceScheduler.T_M3BASE_05_fp_environment_in_the_worker/calib_dark_matched_flat_dark_flat_blocchi` | OB-05 |
| 3 | `Types/ConformanceScheduler.T_M3BASE_05_fp_environment_in_the_worker/calib_dark_matched_flat_dark_flat_intera` | OB-05 |
| 4 | `Types/ConformanceScheduler.T_M3BASE_05_fp_environment_in_the_worker/master_combine_unica` | OB-05 |
| 5 | `Types/Conformance.T_M3BASE_04_fp_environment_at_the_job_boundary/cache_rebuild_unica` | OB-04 (`import_job.cpp:388`) |
| 6 | `Types/Conformance.T_M3BASE_07_cancel_during_the_work/cache_rebuild_unica` | OB-07 e OB-08 |
| 7 | `Types/Conformance.T_M3BASE_07_cancel_during_the_work/calib_dark_matched_flat_dark_flat_blocchi` | OB-07 (P0, `calibration_blocks.cpp:269-272`) |
| 8 | `Types/Conformance.T_M3BASE_07_cancel_during_the_work/calib_dark_matched_flat_dark_flat_intera` | OB-07 (`calibration_job.cpp:265-298`) |
| 9 | `Types/Conformance.T_M3BASE_07_cancel_during_the_work/io_import_unica` | OB-07 e OB-08 |
| 10 | `Types/Conformance.T_M3BASE_07_cancel_during_the_work/io_thumbnail_unica` | OB-07 e OB-08 |
| 11 | `Types/Conformance.T_M3BASE_08_job_peak_reserved_before_the_first_task/cache_rebuild_unica` | OB-08 |
| 12 | `Types/Conformance.T_M3BASE_08_job_peak_reserved_before_the_first_task/io_import_unica` | OB-08 |
| 13 | `Types/Conformance.T_M3BASE_08_job_peak_reserved_before_the_first_task/io_thumbnail_unica` | OB-08 |
| 14 | `Types/Conformance.T_M3BASE_09_common_observations_in_the_provenance/cache_rebuild_unica` | OB-09 e OB-08 |
| 15 | `Types/Conformance.T_M3BASE_09_common_observations_in_the_provenance/io_import_unica` | OB-09 e OB-08 |
| 16 | `Types/Conformance.T_M3BASE_09_common_observations_in_the_provenance/io_thumbnail_unica` | OB-09 e OB-08 |
| 17 | `Types/Conformance.T_M3BASE_10_node_of_the_job_equals_the_request/cache_rebuild_unica` | OB-10 |
| 18 | `Types/Conformance.T_M3BASE_10_node_of_the_job_equals_the_request/calib_dark_matched_flat_dark_flat_blocchi` | OB-10 |
| 19 | `Types/Conformance.T_M3BASE_10_node_of_the_job_equals_the_request/calib_dark_matched_flat_dark_flat_intera` | OB-10 |
| 20 | `Types/Conformance.T_M3BASE_10_node_of_the_job_equals_the_request/io_import_unica` | OB-10 |
| 21 | `Types/Conformance.T_M3BASE_10_node_of_the_job_equals_the_request/io_thumbnail_unica` | OB-10 |
| 22 | `Types/Conformance.T_M3BASE_10_node_of_the_job_equals_the_request/master_combine_unica` | OB-10 |
| 23 | `Types/Conformance.T_M3BASE_11_inputs_verified_before_use/cache_rebuild_unica` | OB-11 e OB-08 |
| 24 | `Types/Conformance.T_M3BASE_11_inputs_verified_before_use/calib_dark_matched_flat_dark_flat_blocchi` | OB-11 |
| 25 | `Types/Conformance.T_M3BASE_11_inputs_verified_before_use/calib_dark_matched_flat_dark_flat_intera` | OB-11 |
| 26 | `Types/Conformance.T_M3BASE_11_inputs_verified_before_use/io_import_unica` | OB-11 e OB-08 |
| 27 | `Types/Conformance.T_M3BASE_11_inputs_verified_before_use/io_thumbnail_unica` | OB-11 e OB-08 |
| 28 | `Types/Conformance.T_M3BASE_11_inputs_verified_before_use/master_combine_unica` | OB-11 |


## Casi verdi su obblighi con debito

La consegna chiede rossi sui debiti di OB-04, 07, 08, 09, 19. Dove il caso è verde, il motivo è stato accertato sul codice prima di proseguire:

- **OB-04, calibrazione e master.** Le copie di `require_fp_environment` nelle lambda di `prepare_job_start` (`calibration_job.cpp:344`, `master.cpp:1123`) fermano con lo stesso `cpu.fp_mode`, prima del nodo e senza `work`; la copia in `work` (`calibration_job.cpp:144`) non è mai raggiunta. Il debito è la duplicazione: il comportamento al confine è già quello dell'obbligo e la suite, che osserva solo il confine, non distingue la copia dal proprietario.
- **OB-07, master.** Il controllo dell'operatore prima di ogni frame (`master.cpp:1038`) e quello dello scheduler per i task danno zero letture nel `prepare` dopo la richiesta e al più un blocco per worker. Il debito (controllo scritto nell'operatore, non `job_context::check_cancel`) è strutturale.
- **OB-08, calibrazione e master.** Lo stato del contratto dice già che per questi tipi l'obbligo sta nel proprietario.
- **OB-09, calibrazione e master.** Le lambda degli operatori serializzano `budget.resources` e compongono `details.cpu` e `details.resource_plan` (uguale al piano eseguito): il contenuto della provenance è quello dell'obbligo, il debito è il luogo.
- **OB-19.** La rimozione dopo il ritorno del runner (`calibration_job.cpp:350-353`) avviene solo con uscita 0: checkpoint e staging restano dopo stop e cancel, spariscono dopo la pubblicazione. Con un errore a `commit.after_commit` né l'operatore né un runner che rimuova dopo un `commit_job` riuscito li rimuoverebbero, quindi nemmeno quel guasto separa le due sedi.

Per questi obblighi la conformità del luogo resta alla review dell'unità di codice: la suite ne verifica l'effetto, non la sede.

## Scelte di lettura del contratto

- **T-M3BASE-01, nodo assente.** Sullo store durevole la chiave esterna `jobs.node_key → nodes` rifiuta `start_job` se il nodo manca, e l'errore di `start_job` è il limite dichiarato di OB-01: il nodo assente non si può produrre dopo l'avvio. Il ramo di `node_of` si esercita con il nodo presente ma illeggibile (`params_canonical` non analizzabile, trigger di immutabilità tolti nel solo progetto di prova): arresto prima del lavoro registrato e propagato con lo stesso contesto.
- **T-M3BASE-07, task avviati dopo la richiesta.** Un worker che ha superato il proprio controllo prima della richiesta può emettere `sched.task` subito dopo: il caso ammette al più `worker_count − 1` avvii dopo la richiesta (task già ammessi) e al più `worker_count` blocchi elaborati. I blocchi letti nel `prepare` dopo la richiesta sono al più uno, perché il `prepare` gira in un solo thread.
- **T-M3BASE-10.** OB-10 si estingue «prima di `work`»: il caso richiede zero invocazioni del `work` della suite oltre a zero aperture.
- **T-M3BASE-11, file cambiato dopo la verifica.** L'iniezione è al primo `sched.task` (verifica conclusa, task in corso). Vale la riga di T-11 (decisione del coordinatore, «Arbitraggio»): il digest si riverifica prima della pubblicazione per ogni tipo, anche se il calcolo non rilegge il file. La calibrazione `intera` non riverifica e pubblica: rosso, debito OB-11.
- **T-M3BASE-11, header.** Valori del piano mutati senza cambiare la dimensione del file: `NAXIS1`, `NAXIS2`, `BITPIX` (segno invertito: stesso numero di byte). Il file mutato è quello letto per il piano; l'originale si ripristina a `sched.plan` ed è quello verificato. Per i tipi che non leggono header prima del digest (import) il file mutato arriva alla verifica e lo stop è `input.digest_mismatch`.
- **T-M3BASE-02.** Guasto di scrittura al punto `artifact.create` del primo `write_files`: `io` `store.syscall_failed` con `punto` e `errno` nel contesto, stesso messaggio e contesto nella provenance; dopo `commit.after_commit` l'eccezione propagata porta lo stesso codice e il punto.
- **T-M3BASE-08.** Picco = `shared_bytes + worker_count · per_task_bytes` di `sched.plan`. Al primo `sched.task` il pool ha almeno il picco prenotato. Attesa: il pool è occupato lasciando libero il picco meno un byte, quindi una prenotazione parziale entrerebbe; il job deve restare in coda senza task e partire dopo il rilascio. Per i tipi senza `job_plan` l'occupazione è dell'intero pool.
- **T-M3BASE-09.** Snapshot con valori riconoscibili (budget 192 MiB, memoria totale e disponibile non tonde) confrontati nella provenance persistita; `budget.resources` uguale in forma canonica a `resource_config_json` della configurazione che il job riceve (senza scratch per le varianti con `scratch = false`); `details.cpu` con `backend` uguale al nodo, `simd` uguale alla richiesta del nodo e `capabilities.avx2` booleano.
- **T-M3BASE-14, generazione superata.** Arresto obbligatorio `stale_generation` `dag.stale_publish`, uscita 2.
- **T-M3BASE-16.** Rimozione prima e dopo la recovery e alterazione ripetute per ogni output presente del primo passo (tutti i ruoli, provenance compresa).
- **T-M3BASE-19.** Oltre a stop, cancel a metà e pubblicazione: commit fallito a `commit.before_commit` (nulla pubblicato, checkpoint e staging presenti) e cancel al ritorno di `work` (presenti).
- **T-M3BASE-15, backend diverso.** Il costruttore del registro fissa `simd` scalare; il backend diverso si ottiene riscrivendo il checkpoint con `write_checkpoint` e il campo `backend` cambiato. Parziale alterato: tutti i byte dello staging invertiti con XOR.
- **T-M3BASE-17.** La regola sul sorgente è un test della suite (`ConformanceSource.T_M3BASE_17_*`), non una regola di `tools/lint.sh`.
- **T-M3BASE-18.** Eseguibile `conformance_network`: un job riuscito per ogni voce e variante in un solo progetto, sotto `tools/check_m2f02_network.sh` con il suo canarino.

## Casi non implementabili

- T-M3BASE-01, «nodo dello slot assente dallo store»: vedi sopra. Sostituito dal nodo illeggibile, che esercita lo stesso ramo del runner.

## Limiti

- La suite osserva il confine: obblighi soddisfatti da copie corrette negli operatori risultano verdi (vedi sopra).
- Mutazioni solo sui rossi: nessuna prova di mutazione sui casi verdi; la capacità della suite di rilevare un difetto è mostrata solo dai 28 casi rossi sul codice di oggi.
- I punti di osservazione sono globali al processo: i casi non vanno eseguiti in parallelo nello stesso processo (ctest lancia un processo per test).
- Il controllo di completezza legge il sorgente con espressioni regolari: definizioni di funzione su più righe o macro non sono riconosciute. Il controllo negativo copre il caso del tipo non registrato.
- Un'esecuzione per preset: `dev` (ASan e UBSan) prima e dopo le correzioni, `release` dopo l'arbitraggio.

## Arbitraggio

Il coordinatore ha chiuso l'unità dopo il giro 2, senza un terzo giro, con correzioni minime (delta in `tmp/basesuite-arbitrato.diff`):

- T-M3BASE-02: esito, provenance persistita ed eccezione propagata dopo `commit.after_commit` si confrontano con lo stop che l'iniezione produce, costruito nel test (`store.syscall_failed`, messaggio, contesto completo con `punto`, `errno`, `errno_text`), non con l'esito già trasformato.
- T-M3BASE-09: confrontati anche `mode` e `fraction` dello snapshot; `capabilities.avx2` confrontato con `__builtin_cpu_supports("avx2")`.
- OB-11 (decisione): vale la riga di T-11. Il digest si verifica prima di usare qualunque pixel e prima della pubblicazione per ogni tipo di job, anche se il calcolo non rilegge il file. Testo e «Si estingue» di OB-11 allineati in `docs/contracts/BASE-confini.md`.
- Debiti del §4 integrati con i punti trovati dalla suite: OB-04 `import_job.cpp:388` (nodo costruito nel `prepare` di `prepare_job_start` prima del controllo FP); OB-07 passata P0 di `calibration_blocks.cpp:269-272`. La regola di conteggio di T-M3BASE-07 (al più `worker_count − 1` task già ammessi dopo la richiesta, al più un blocco letto nel `prepare`) è ora nella riga del caso.
- Il blocco generato di `docs/evidence/M3-base-contratto.md` è rigenerato per il nuovo hash del contratto.

Esito dopo le correzioni: suite `dev` 89 test, 28 rossi; `release` completo 604 test, 28 rossi, gli stessi.

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`; al massimo 2 giri.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-basesuite-round1-20261030-0830.md` | `tmp/basesuite-round1.diff` (unità intera) | FAIL | 7 BLOCCANTI sui test: T-14 stop con generazione superata non obbligatorio; T-19 senza commit fallito né cancel dopo `work`; T-16 solo sull'output primario; T-11 senza osservare l'uso dei campioni; T-09 su presenza e non contenuto; T-08 senza prova del picco intero prenotato; T-02 con codice e contesto non verificati. Corretti nel giro 2 (sezione «Scelte di lettura»), con il punto `input.samples` e il campo `scratch` del registro |
| 2 | `.omc/artifacts/ask/codex-basesuite-round2-20261030-0844.md` | `tmp/basesuite-round2.diff` | FAIL | 2 BLOCCANTI aperti: T-02 confronta la provenance con l'esito già trasformato e non verifica messaggio e contesto completi attesi dall'iniezione (anche dopo `commit.after_commit`, dove `errno_text` non è controllato); T-09 non confronta `fraction` dello snapshot e accetta `capabilities.avx2` di valore qualunque. Corretti per arbitraggio, senza terzo giro |

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/BASE-confini.md` | `573fba6402d89abb5a8df3566792f36a85602519ff6525da755d7d5c0472eccb` |
| `src/core/CMakeLists.txt` | `489359a91b2bc0fc8b811113c20c7bf6da218a745bad446661c2cdbadd90b72a` |
| `src/core/fits.cpp` | `7aecd983c003877de00f87a3ecb1e6b620f75790f1e8a4dd87c2a0835ac472c0` |
| `src/core/fits_blocks.cpp` | `d8c0d71a430957157b7c8895ded765db1c7d8455dd074d11818a51e87b5a74f1` |
| `src/core/include/astro/job.hpp` | `8c60900de1782ff03eb74fb41f0aa6583223a8601008d1f0305dfa4447ff15e3` |
| `src/core/include/astro/job_probe.hpp` | `558d5107416a7c7c1199734dc69f2dbb3ab400d9802f59bca4672ca0fdda9296` |
| `src/core/include/astro/job_types.hpp` | `98d1f80ede5ecb0da92cdb55f2aa8cec050b0cb163b6df1c7968b7367ace70e6` |
| `src/core/inventory.cpp` | `6b2fee4bf475ca554a8647c2418da24741465ac2f63d0cdd7fbae81b30e17ef1` |
| `src/core/job.cpp` | `9d9a61e9cc58b5bff036faae4889f71c1a0724fbdd78bdbcdc6408c5430d364d` |
| `src/core/job_probe.cpp` | `a3f08b593a97b04672f0ae4b089e7cebf7594531979cffd6252aa668e70451bc` |
| `src/core/job_types.cpp` | `fda920302c2e974237f08f4ff6c1a0a86f7da20548d419fdfed899937b5d5255` |
| `src/core/node.cpp` | `b697838dd840f877c2b41a63ab83679d03a2ec6700e784438d052532b8bf7e3c` |
| `src/core/scheduler.cpp` | `9588b0adfc402c790329ee0f837972128a3548ef82b5012055f228c429d38069` |
| `tests/fixtures/conformance_inputs.hpp` | `d680048d26f707487062942ff19145a417364f677fdfc12901a7f4a93a88d94d` |
| `tests/unit/CMakeLists.txt` | `11692a0d0102fa9e69f0e241a2c1ca72a8543622c451f34819721dd20946914e` |
| `tests/unit/conformance_network.cpp` | `72392f9b1d8d37b68d762da2c73f83b26c4428bdeee02d16c572612fff98d9fc` |
| `tests/unit/test_conformance.cpp` | `a75bee61a509be32163cfe862cbb4f25a1a9187a256a4fd7c404c96d6799683b` |
| `tools/check_job_types.py` | `5744c2ceeda8f738b7147003014398fea8f9d5163227f8739ca314e8e2126c7b` |

<!-- evidence:end -->
