# Contratto BASE: obblighi trasversali e confini condivisi (M3)

Documento normativo. Fissa quale confine condiviso adempie ciascun requisito trasversale dei job e con quale prova. Gli operatori di M3 vi aggiungono solo la loro scienza: un obbligo di questo testo non si riscrive dentro un operatore. Se un operatore ne ha bisogno in forma diversa, il difetto è del confine e si corregge lì (`plans/M3.md`, «Base comune prima del codice delle slice»).

Regole di lettura:

- «deve», «non può» sono normativi. Ogni obbligo ha un identificativo stabile `OB-NN`, che non si riusa e non si rinumera.
- **Proprietario**: il solo confine condiviso che implementa l'obbligo. Una funzionalità che condivide l'obbligo chiama il proprietario e non lo reimplementa.
- **Nasce / si estingue**: il passo del codice in cui l'obbligo comincia a valere e quello in cui è soddisfatto.
- **Stato** al commit `db2a389`, letto sul codice con `codegraph explore` (chiamanti di `run_job_files`, `run_job`, `prepare_job_start`, `job_scheduler::run`, `run_branches`) e `grep` sui sorgenti di `src/core/`. I numeri di riga servono a orientarsi. Tre valori:
  - IMPLEMENTATO NEL PROPRIETARIO: l'obbligo è scritto solo nel proprietario;
  - IMPLEMENTATO ALTROVE: l'obbligo, o una sua parte, è scritto anche o soltanto fuori dal proprietario (elencati i punti);
  - MANCANTE: nessun punto del codice lo adempie.
- **Prova**: il caso della suite di conformità (§3) che verifica l'obbligo per ogni tipo di job registrato. Gli ID `T-M3BASE-NN` sono etichette di lavoro: nessuno esiste ancora come test.
- Il testo afferma soltanto ciò che è stato letto nel codice. Il resto è marcato *ipotesi*, *questione aperta* o *limite*.
- Gli errori seguono il modello comune di [ADR-05](ADR-05-dag-provenance.md#modello-derrore-comune): `stop_error` con categoria, codice e contesto. «Uscita 2» e «uscita 3» sono `exit_code_for` (`job.hpp`): 3 per la categoria `cancelled`, 2 per ogni altra.

## 1. Tipi di job e confini

Tipi di job del prodotto al commit `db2a389`. Tutti passano da `run_job_files` (chiamanti letti con `codegraph explore`). `run_job` (store in memoria) non ha chiamanti nel prodotto oltre `run_branches(dag_store&)`.

| Operatore (`operator_id`) | Funzione del job | Scheduler | Checkpoint |
|---|---|---|---|
| `io.import` | `run_import_job` (`import_job.cpp:338`) | sì, un task (I2) | no |
| `io.thumbnail` | `run_thumbnail_job` (`import_job.cpp:362`) | sì, un task (I2) | no |
| `cache.rebuild` | `run_cache_rebuild_job` (`import_job.cpp:382`) | sì, un task (I2) | no |
| `calib.dark_matched_flat_dark_flat` | `run_calibration_job` (`calibration_job.cpp:139`), residenza intera o a blocchi (`calib_detail::run_block_calibration`, `calibration_blocks.cpp:172`), scelta da `plan_resources` prima dell'esecuzione | sì | sì, se dichiarato in `calibration_job_options::checkpoint` |
| `master.combine` | `run_master_job` (`master.cpp:959`) | sì | no |

`run_branches(sqlite_dag_store&, …)` (`scheduler.cpp:534`) e `run_workflow` / `resume_workflow` (`workflow.cpp:98`, `:113`) compongono job e non sono tipi di job.

Confini condivisi:

| Confine | Dove |
|---|---|
| Runner dei job | `run_job_files`, `prepare_job_start`, `run_job` (`src/core/job.cpp`) |
| Scheduler | `job_scheduler::run`, `worker_loop`, `task_context::check_cancel` (`src/core/scheduler.cpp`) |
| Pool | `memory_pool`, `reservation` (`src/core/memory_pool.cpp`) |
| Lettura degli input | `open_fits_input`, `read_input_bytes`, `fits_detail::check_unchanged` (`src/core/fits.cpp`); `read_verified_fits` (`src/core/hash.cpp`). Dall'emendamento L1 (§2.5) lo stesso confine legge ogni formato dichiarato |
| Provenance | `provenance_canonical`, `stop_before_work_canonical` (`src/core/provenance.cpp`), scritte dal runner |
| Store | `sqlite_dag_store::write_files`, `commit_job` (`src/core/sqlite_files.cpp:65`, `:115`) |
| Workflow | `run_workflow`, `resume_workflow` (`src/core/workflow.cpp`) |
| Libreria del core | `libastrocore`, controllata al link da `tools/check_offline.sh` |

## 2. Obblighi

### 2.1 Arresto e provenance

**OB-01 — Arresto prima del lavoro registrato con provenance.**
- Testo: un errore sollevato fra la chiamata di `run_job_files` (o di `prepare_job_start`) e l'inizio di `work` chiude il job `failed` (`cancelled` per la categoria `cancelled`) con categoria e codice, scrive come artefatto `provenance` del nodo il record `stop_before_work` e si propaga al chiamante con la stessa categoria, lo stesso codice e lo stesso contesto.
- Fonte: NFR-FBK-01.a; ADR-05 §Provenance; contratto di `job.hpp` (arresto prima del lavoro).
- Proprietario: runner dei job (`stop_before_work`, `job.cpp:189`).
- Nasce: ingresso in `prepare_job_start` (`job.cpp:227`) o in `run_job_files` (`job.cpp:270`). Si estingue: record persistito e eccezione propagata (`job.cpp:197-208`); se la registrazione fallisce, l'arresto originale si propaga con il contesto `registrazione_arresto` (`job.cpp:167`).
- Stato: IMPLEMENTATO NEL PROPRIETARIO. `stop_before_work`, `node_of` (`job.cpp:212`), `prepare_job_start`. Usano `prepare_job_start` per il proprio lavoro preliminare: calibrazione (`calibration_job.cpp:343`), master (`master.cpp:1122`), `cache.rebuild` (`import_job.cpp:388`). **Limite** (dichiarato in `job.hpp`): un errore di `start_job` non si registra, perché è la scrittura del record stesso a non riuscire.
- Prova: T-M3BASE-01.

**OB-02 — Arresto nel lavoro o nella pubblicazione registrato con provenance.**
- Testo: uno `stop_error` di `work` o della pubblicazione chiude il job `failed` o `cancelled`, scrive la diagnosi su stderr, restituisce un `job_outcome` con uscita 2 o 3 e persiste una provenance con nodo, versioni, generazione, stato e causa registrati nello store, avvisi, budget e contesto di resume; un'eccezione non `stop_error` chiude il job `failed` `integrity` `job.unexpected_exception`, persiste la provenance e si rilancia invariata.
- Fonte: NFR-FBK-01.a; ADR-05 §Provenance; contratto di `run_job_files` in `job.hpp`.
- Proprietario: runner dei job (`stopped`, `unexpected`, `job.cpp:82`, `:134`).
- Nasce: ingresso in `work` (`job.cpp:280`). Si estingue: `job_outcome` restituito con provenance persistita, oppure con `provenance_not_persisted` valorizzato (`job.cpp:108-121`).
- Stato: IMPLEMENTATO NEL PROPRIETARIO. Fuori da questo obbligo, come dichiara `job.hpp`: un errore sopraggiunto dopo che `commit_job` ha già chiuso il job `completed` e pubblicato gli output (punto `commit.after_commit`, `sqlite_files.cpp:200`) si propaga invariato (`job.cpp:95`), senza cambiare lo stato del job.
- Prova: T-M3BASE-02.

**OB-03 — Arresto fuori da un job registrato con provenance.**
- Testo: un rifiuto di `run_workflow` o di `resume_workflow` che avviene prima di avviare un job (`workflow.slot_target`, `resume.unknown_workflow`, `resume.recipe`, `resume.artifact_missing`, `resume.artifact_digest`) lascia nello store un record di arresto a livello di workflow, con chiave l'identità del workflow (digest della definizione più l'esecuzione) e con categoria, codice e contesto; come `stop_before_work_record` contiene solo stringhe e interi.
- Fonte: NFR-FBK-01 («provenance dell'arresto»); NC-21.
- Proprietario: workflow (`run_workflow`, `resume_workflow`).
- Nasce: ingresso in `run_workflow` / `resume_workflow`. Si estingue: record persistito prima che l'eccezione lasci la funzione.
- Stato: IMPLEMENTATO NEL PROPRIETARIO (intervento I4). `run_workflow` e `resume_workflow` (`workflow.cpp:225`, `:236`) passano ogni eccezione dei controlli prima del primo passo (`start_run`, `plan_resume`, compresi `workflow.definition` ed errori dello store) a `stop_workflow` (`workflow.cpp:59`), che scrive il record e rilancia l'arresto invariato. Schema: nessuna tabella nuova, il record è una riga di `settings` inserita con `insert_setting` (chiave già presente: `invalid_input` `store.setting_exists`, nessuna sostituzione), chiave `workflow_stop.<sha256 della definizione canonica data>.<esecuzione>` con l'esecuzione la prima libera da 1, valore JSON canonico `astro.workflow_stop.v1` con workflow, digest, esecuzione, operazione (`run`, `resume`), categoria, codice, messaggio e contesto in coppie (`workflow.hpp`). Un'eccezione non `stop_error` si registra come `integrity` `job.unexpected_exception`. Registrazione fallita (per esempio store in sola lettura): diagnosi su stderr e arresto originale con il contesto `registrazione_arresto`, come OB-01 (helper condivisi in `stop_internal.hpp`). **Limite**: se lo store non si apre, nessun record si scrive e la diagnosi resta nel codice di uscita. Restano fuori da questo obbligo, come **limite**: gli errori di costruzione di un nodo chiamata dal chiamante prima di creare lo slot (`make_calibration_node`, `make_master_node`, `make_import_node`), che non hanno un nodo a cui legarsi; i rami `blocked` e `not_started` di `run_branches` (`scheduler.cpp:501-516`), che non hanno `outcome` né provenance: l'arresto registrato è quello del ramo padre.
- Prova: T-M3BASE-03.

### 2.2 Ambiente in virgola mobile

**OB-04 — Ambiente FP al confine del job.**
- Testo: con MXCSR diverso dall'ambiente di D23 (FZ o DAZ accesi, arrotondamento non al più vicino, una maschera di eccezione spenta) il job si ferma `numeric` `cpu.fp_mode` prima di leggere il nodo e prima di ogni serializzazione di numeri in virgola mobile, come arresto prima del lavoro (OB-01).
- Fonte: ADR-07.a §Ambiente in virgola mobile, §Aggiunte di R5 («Ambiente FP», R5.c).
- Proprietario: runner dei job (`run_job_files`: `prepare_job_start(store, slot_name, require_fp_environment)`, `job.cpp:274`; `run_job`: `job.cpp:244`).
- Nasce: ingresso nel runner. Si estingue: controllo superato prima di `node_of`, oppure arresto registrato.
- Stato: IMPLEMENTATO NEL PROPRIETARIO (intervento I1, `docs/evidence/M3-base-I1.md`). `run_job_files` controlla l'ambiente nel `prepare` di `prepare_job_start` (`job.cpp:278-279`) prima di serializzare le risorse nel budget (OB-09), di leggere il nodo dello slot (`node_of`, `job.cpp:284`) e di costruire il nodo della richiesta (OB-10); le copie in `work` e nelle lambda di calibrazione e master sono tolte. `run_cache_rebuild_job` controlla nel proprio `prepare_job_start` solo la causa, un confronto di stringhe (`import_job.cpp:393`), e il suo nodo lo costruisce il runner dopo il controllo.
  Non sono copie di questo obbligo le guardie delle funzioni di libreria chiamabili fuori da un job, che ADR-07.a §Aggiunte di R5 richiede nel preflight: `make_calibration_node` (`calibration_job.cpp:100`), `make_master_node` (`master.cpp:830`), `make_plan` (`master.cpp:189`), `prepare_impl` (`calibration.cpp:622`).
- Prova: T-M3BASE-04.

**OB-05 — Ambiente FP nel worker.**
- Testo: ogni task eseguito dallo scheduler controlla MXCSR nel thread del worker prima di eseguire `run_task`, e un modo vietato ferma il task con `numeric` `cpu.fp_mode` senza che il task legga i propri dati.
- Fonte: ADR-07.a §Ambiente in virgola mobile; §Aggiunte di R5 (secondo punto).
- Proprietario: scheduler (`worker_loop`, `scheduler.cpp:234`).
- Nasce: prelievo del task dalla coda. Si estingue: controllo superato prima di `spec.run_task`, oppure task fermato e primo errore registrato nello stato del job.
- Stato: IMPLEMENTATO NEL PROPRIETARIO (intervento I3, `docs/evidence/M3-base-I3.md`). `worker_loop` chiama `begin_worker_task` (`scheduler.cpp:241`, `cpu_dispatch.cpp:100`) dopo il controllo del cancel e prima di prenotare la memoria del task e di `spec.run_task`: il punto di iniezione `cpu.task_start` scatta lì, una volta per task, poi `require_fp_environment`. `begin_kernel_task` (`cpu_dispatch.cpp:106`) nei thread dei worker non richiama più il punto (variabile `thread_local` impostata da `begin_worker_task`); fuori dallo scheduler (chiamate di libreria) lo richiama come prima. Il registro dei tipi segna ora `scheduler` anche per `io.import`, `io.thumbnail` e `cache.rebuild`, che passano dallo scheduler dall'intervento I2: T-M3BASE-05 gira anche per loro.
  Non è una copia di questo obbligo la guardia `require_fp_environment` all'inizio di ogni kernel (`begin_kernel_task`, chiamata da `calibration.cpp:804` e `master.cpp:569`), che ADR-07.a §Aggiunte di R5 richiede per ogni kernel, anche dentro un worker: resta, e la sua rimozione dai worker richiederebbe l'emendamento di ADR-07.a che il §4 prevedeva, non fatto.
- Prova: T-M3BASE-05.

### 2.3 Cancel

**OB-06 — Cancel al confine di pubblicazione.**
- Testo: con il `cancel_token` richiesto al ritorno di `work`, il job si chiude `cancelled` `sched.cancelled` con uscita 3, nessun file è pubblicato e la provenance `cancelled` è persistita.
- Fonte: ADR-06.a §Guasto e cancel; NFR-FBK-01.a.
- Proprietario: runner dei job (`job.cpp:282`).
- Nasce: ritorno di `work`. Si estingue: prima di `commit_job`, con l'arresto registrato da OB-02.
- Stato: IMPLEMENTATO NEL PROPRIETARIO.
- Prova: T-M3BASE-06.

**OB-07 — Cancel durante il lavoro.**
- Testo: durante il lavoro un annullamento richiesto ferma il job con `cancelled` `sched.cancelled` entro il blocco di righe in corso, senza avviare task nuovi e senza eseguire `finalize`.
- Fonte: ADR-06.a §Guasto e cancel.
- Proprietario: scheduler. Punti del proprietario: `worker_loop` all'inizio del task (`scheduler.cpp:229`), `task_context::check_cancel` a ogni blocco di righe (`scheduler.cpp:37`), `produce` (`scheduler.cpp:331`), attesa di ammissione per pressione (`scheduler.cpp:305`), attesa del pool (`memory_pool::reserve` con `cancel`), dopo il join e dopo `finalize` (`scheduler.cpp:442`, `:447`).
- Nasce: ingresso in `job_scheduler::run`. Si estingue: ritorno di `run` o arresto `sched.cancelled`.
- Stato: IMPLEMENTATO NEL PROPRIETARIO (intervento I2, `docs/evidence/M3-base-I2.md`).
  - `job_context::check_cancel()` (`scheduler.cpp:38`) ferma con `cancelled` `sched.cancelled` se l'annullamento è richiesto o il job è in arresto. Lo chiamano i `prepare` a ogni blocco letto: calibrazione intera prima di ogni ingresso e nella lettura (`calibration_job.cpp:270`), e nella riverifica, spostata nel `finalize` dall'intervento I3 (`calibration_job.cpp:292-294`); passata P0 dei blocchi prima di ogni pezzo letto da `fits_block_source::verify`, che riceve il controllo (`calibration_blocks.cpp:273`, `fits_blocks.cpp:216`), e cicli dei blocchi (`calibration_blocks.cpp:300`, `:314`); master prima di ogni frame (`master.cpp:1040`). Le copie scritte negli operatori sono tolte.
  - `io.import`, `io.thumbnail` e `cache.rebuild` passano dallo scheduler (OB-08). Il loro `prepare` chiama `check_cancel` e verifica il digest con `digest_fits_input`, che riceve il token e lo controlla prima di ogni blocco (`import_job.cpp:178`, `inventory.cpp:265`); nel task l'inventario degli header (`inventory.cpp:28`), `make_preview` (`inventory.cpp:223`) e la cache ricevono il token del job e lo controllano prima di ogni lettura o fascia.
  - La riverifica degli input nel `finalize` del master (`master.cpp:1064-1065`, intervento I3) chiama `job_context::check_cancel` prima di ogni frame e prima di ogni pezzo letto. Lo prova `ConformanceGapsMaster.T_M3BASE_07_finalize_reverify_stops_at_the_cancel` (intervento I5).
  - I controlli del `prepare` dei tipi di I/O su un input di più blocchi di digest li prova `ConformanceGapsIo.T_M3BASE_07_prepare_cancel_in_a_multi_block_input` (intervento I5, `docs/evidence/M3-base-I5.md`).
- Prova: T-M3BASE-07.

### 2.4 Budget, pool, risorse

**OB-08 — Picco del job prenotato prima del primo task.**
- Testo: ogni job dichiara un `job_plan` prima dell'esecuzione e il suo picco `shared_bytes + worker_count · per_task_bytes` è prenotato per intero dal pool prima del primo task. Un picco oltre il budget, o non rappresentabile, ferma il job con `budget` `sched.job_exceeds_budget` senza avviare task. `tile_rows`, `worker_count`, `spill` e il numero di candidati restano quelli del piano. Le prenotazioni si rilasciano anche quando il job lancia.
- Fonte: ADR-06.a §Job, §Pool e prenotazioni; NFR-FBK-01 (il budget non autorizza tagli).
- Proprietario: scheduler (`job_scheduler::run`, `scheduler.cpp:349`).
- Nasce: ingresso in `job_scheduler::run`. Si estingue: prenotazione ottenuta (`scheduler.cpp:394`) prima di avviare i worker, oppure arresto prima di ogni task.
- Stato: IMPLEMENTATO NEL PROPRIETARIO (intervento I2, `docs/evidence/M3-base-I2.md`). Master e calibrazione, in entrambe le residenze, come prima. `io.import`, `io.thumbnail` e `cache.rebuild` dichiarano un `job_plan` di un task e un worker (`run_single_task`, `import_job.cpp:192`) e passano da `job_scheduler::run`, che prenota il picco in coda FIFO prima del `prepare` (`scheduler.cpp:411`). `shared_bytes` è il blocco di `digest_fits_input` (`digest_reserve_bytes`), usato nel `prepare`; `per_task_bytes` è `inventory_reserve_bytes` per l'import e `thumbnail_cache::peak_bytes` (`thumbnail_cache.cpp:287`: massimo fra il calcolo dell'anteprima e la voce con l'anteprima) per miniatura e ricostruzione. Inventario, anteprima e cache prendono la memoria dalla prenotazione del task con sotto-prenotazioni (`io_memory`, `inventory.cpp:102`); `reserve_or_stop` resta solo per le chiamate di libreria fuori da un job. Il piano eseguito va in `job_files_result::plan` (OB-09). `worker_count` e `tile_rows` della configurazione non si applicano a questi tipi: un task non si divide fra worker; `tile_rows` è l'altezza della fascia dell'anteprima, 1 per l'import, che non legge campioni. **Limite**: una voce di cache presente con dimensione diversa da quella valida supera il picco e ferma il job `budget` `pool.reservation_exceeded` prima della verifica del suo digest.
- Prova: T-M3BASE-08.

**OB-09 — Osservazioni comuni del job nella provenance.**
- Testo: quando il chiamante passa lo snapshot del budget, la provenance di ogni job, riuscito o fermato dentro `work`, lo porta con la configurazione delle risorse (`budget.resources`). La provenance di un job riuscito porta anche `details.cpu` di ADR-07.a, se il backend del nodo è `cpu_*`, e `details.resource_plan`, se il piano viene da `plan_resources`.
- Fonte: ADR-05 §Provenance; ADR-06.a §Budget, §Estensione R2.r («Job del master», T-M2R2R-05); ADR-07.a §Aggiunte di R5 («Provenance»). Senza snapshot nulla si registra: limite dichiarato in `calibration.hpp` e `master.hpp` (ADR-06.a, T-M2R3-08).
- Proprietario: runner dei job (`run_job_files`, che compone `provenance_record`, `job.cpp:287`).
- Nasce: `prepare_job_start`. Si estingue: provenance scritta da OB-01 o OB-02.
- Stato: IMPLEMENTATO NEL PROPRIETARIO (intervento I1). `run_job_files` riceve la configurazione delle risorse in `job_request::resources` e la serializza in `budget.resources` dopo il proprio controllo FP (`job.cpp:281`); compone `details.cpu` con backend e `simd` del nodo dello slot e le capacità lette dalla CPU (`job.cpp:307`), e `details.resource_plan` dal piano che `work` dichiara in `job_files_result::plan` (`job.cpp:311`). Calibrazione intera e a blocchi e master dichiarano il piano (`calibration_job.cpp:229`, `:333`; `master.cpp:1118`) e non scrivono più queste chiavi. Una chiave `cpu` o `resource_plan` nei `details` dell'operatore ferma il job `invalid_input` `dag.param_duplicate_key`. Dall'intervento I2 `io.import`, `io.thumbnail` e `cache.rebuild` ricevono la `resource_config` e dichiarano il piano di un task (OB-08): T-M3BASE-09 è verde anche per loro. **Limite**: il loro `details.resource_plan` porta i campi di residenza `whole` predefiniti, che non descrivono questi tipi; `details.cpu` c'è, senza `simd` perché il loro nodo non porta la richiesta.
- Prova: T-M3BASE-09.

### 2.5 Identità e input

**OB-10 — Nodo del job uguale alla richiesta.**
- Testo: prima di aprire un input, il job ricostruisce il nodo dalla richiesta e, se la chiave differisce da quella del nodo dello slot, si ferma con `invalid_input` e un codice `*.node_mismatch` che porta le due chiavi nel contesto.
- Fonte: ADR-05 §Nodo; ADR-07.a §Aggiunte di R5 («Ricetta e nodo»).
- Proprietario: runner dei job (`run_job_files`, che legge già il nodo dello slot in `node_of`).
- Nasce: nodo letto (`job.cpp:276`). Si estingue: chiavi confrontate prima di `work`.
- Stato: IMPLEMENTATO NEL PROPRIETARIO (intervento I1). `run_job_files` riceve in `job_request::node` il costruttore del nodo della richiesta, lo chiama dopo `node_of` e prima di `work`, e con chiave diversa si ferma `invalid_input` con il codice della richiesta e le due chiavi (`nodo_slot`, `nodo_richiesta`; `job.cpp:289-295`). L'arresto è dentro il lavoro (OB-02: esito con uscita 2 e provenance del nodo dello slot), come per le copie che sostituisce. Il codice resta quello del tipo citato dai contratti e dai test esistenti (`import.node_mismatch`, `calib.node_mismatch`, `master.node_mismatch`, ADR-07.a §Aggiunte di R5), passato dall'operatore in `job_request::mismatch_code`; `job.node_mismatch` è il valore predefinito di una richiesta senza codice, che nessun tipo del prodotto usa. Le copie `check_node` (import, thumbnail, rebuild), `calibration_job.cpp:146-150` e `master.cpp:964-968` sono tolte. `run_branches` non passa una richiesta: compone job il cui nodo è lo slot stesso.
- Prova: T-M3BASE-10.

**OB-11 — Input verificati prima dell'uso.**
- Testo: i byte di un input che entrano nel calcolo vengono da una lettura il cui digest SHA-256 e la cui dimensione sono uguali a quelli dichiarati nella richiesta. Un digest diverso dà `integrity` `input.digest_mismatch`, un file assente `io` `input.missing`. Il digest si verifica prima di usare qualunque pixel e di nuovo prima della pubblicazione, per ogni tipo di job, anche quando il calcolo non rilegge il file (decisione del coordinatore all'arbitraggio della suite, `docs/evidence/M3-base-suite.md`). Gli header si leggono prima del digest solo per pianificare la memoria, entro i limiti del budget; il digest si verifica prima di usare qualunque pixel, e i valori di header usati nel piano si confrontano con quelli del file verificato: una differenza dà uno stop `integrity`.
- Fonte: NFR-DATA-01; ADR-05 §Identità degli input; ADR-04 (identità dei file).
- Proprietario: lettura degli input (`fits.cpp`, `hash.cpp`).
- Nasce: apertura dell'input (`open_fits_input`). Si estingue: digest verificato prima della decodifica dei campioni; riverifica prima di `commit_job`.
- Stato: IMPLEMENTATO NEL PROPRIETARIO (intervento I3, `docs/evidence/M3-base-I3.md`). La lettura verificata sta in `hash.cpp`, dichiarata in `hash.hpp`; gli operatori la chiamano e non confrontano digest né header:
  - `require_input_digest` (`hash.cpp:87`) è l'unico punto che emette `integrity` `input.digest_mismatch`; lo chiamano le funzioni sotto, `read_verified_fits` (libreria), la passata P0 di `fits_block_source::verify` (`fits_blocks.cpp:320`) e il `prepare` dei tipi di I/O sul digest di `digest_fits_input` (`import_job.cpp:199`);
  - `require_planned_header` (`hash.cpp:95`) confronta i valori di header usati nel piano (`fits_plan`: dimensione, larghezza, altezza, piani, BITPIX, tipo del campione, presenza di INVALID e VARIANCE) con quelli del file verificato, con il campo nel contesto: dimensione diversa `integrity` `input.size_changed`; larghezza, altezza o piani `integrity` `input.geometry_changed`; BITPIX, tipo, INVALID o VARIANCE `integrity` `input.header_changed`;
  - `read_verified_input` (`hash.cpp:112`) controlla che il percorso indichi ancora il file aperto per il piano (`check_path_names`, `fits.cpp:544`: assente `io` `input.missing`, altro file `integrity` `input.modified`), legge l'intero input da quel descrittore, verifica il digest, confronta il piano dei byte verificati, poi che il file sia invariato dall'apertura (`check_unchanged`: ogni header letto per il piano è del file verificato), tutto prima della decodifica. La usano la calibrazione a residenza intera, che tiene aperti gli input del piano (`calibration_job.cpp:273`), e il master (`master.cpp:1044`);
  - nella residenza a blocchi il piano si confronta con gli header analizzati dalla sorgente a blocchi prima della passata P0 (`calibration_blocks.cpp:274`), che poi verifica quegli stessi header e il digest prima di ogni blocco; il digest di ogni blocco riletto resta confrontato con la passata P0 (`input.consumed_digest`);
  - `reverify_input` (`hash.cpp:136`) è la riverifica prima della pubblicazione: riapre il percorso (`io` `input.missing`), ne confronta la dimensione con quella attesa (`integrity` `input.size_changed`) e ne verifica il digest a pezzi con il controllo del cancel prima di ogni pezzo, controlla che il file riaperto non sia cambiato durante la lettura (`check_unchanged`), poi che l'input aperto per il lavoro, se c'è, sia invariato. La chiamano i `finalize` di ogni tipo prima di `write_files`: calibrazione intera (`calibration_job.cpp:293`), blocchi (`calibration_blocks.cpp:387`, dopo `check_identity`), master (`master.cpp:1065`), tipi di I/O (`import_job.cpp:204`, `finalize` aggiunto al job di un task). Le copie sono tolte: riverifica dopo il preflight della calibrazione intera, confronti di dimensione e geometria in calibrazione e master, digest inline e riverifica del master, `verify_digest` e `check_unchanged` dopo il task dei tipi di I/O.
  **Limiti**: nella residenza a blocchi gli header usati prima della sorgente (fase e linearità con `peek_header`, `primary_sample_bytes`) vengono da un'apertura per percorso diversa da quella che la passata P0 verifica, e solo i valori di `fits_plan` sono riconfrontati; per i tipi di I/O il confronto degli header del piano (byte dell'anteprima) è il `check_unchanged` di `digest_fits_input` sullo stesso descrittore, senza confronto esplicito dei valori. La protezione di `check_unchanged` vale nei limiti di `mtime` e `ctime` del filesystem.
- Prova: T-M3BASE-11; per l'emendamento L1, T-M3BASE-20, T-M3BASE-21, T-M3BASE-22.

**Emendamento L1 (M3.a) — lettura degli input indipendente dal formato.** Vale per OB-11 e OB-12, che restano gli stessi obblighi con lo stesso proprietario (lettura degli input); cambia solo il perimetro dei formati.

- Fatti, letti al commit `311fb7a` con `codegraph explore` e `grep` su `src/core/`:
  - l'unica apertura di un input è `open_fits_input` (`fits.cpp:560`): applica il solo limite `fits_limits::max_file_bytes` (`fits.hpp:39`, 2^28 · 18 + 16 MiB) con il codice `fits.max_file_bytes` (`fits.cpp:579-583`) e usa i codici `fits.open`, `fits.fstat`, `fits.not_regular` (`fits.cpp:565`, `:572`, `:576`); l'input aperto ha il tipo `fits_input` (`fits.hpp:57`);
  - `read_verified_input` (`hash.cpp:112`) richiede un `fits_plan` e, dopo il digest, analizza i byte verificati come FITS (`plan_fits(bytes)`, `hash.cpp:118`) per `require_planned_header`;
  - `reverify_input` (`hash.cpp:136`) riapre il percorso con `open_fits_input` (`hash.cpp:138`), quindi con il limite e i codici FITS;
  - `check_path_names` (`fits.cpp:544`) e `check_unchanged` (`fits.cpp:521`) usano solo il descrittore, `stat` e `fstat`; `digest_input` (`hash.cpp:123`) legge tutti i byte con `read_exact` e li passa all'hasher senza interpretarne il formato; per un errore di `stat` o `fstat` diverso da `ENOENT` i primi due emettono `fits.fstat`; le letture con `pread` (`pread_exact` e `read_input_bytes`) emettono `fits.read` per un `pread` fallito (`fits.cpp:468`, `:478`, `:618`).

  Un input RAW non ha quindi un percorso nel confine. ADR-08.b §2.1 chiede il limite `raw.max_file_bytes` (1 073 741 824 byte) prima della lettura e il passaggio a `LibRaw::open_buffer` dello stesso buffer di cui si calcola il digest; un operatore che aprisse o verificasse il file da sé violerebbe OB-11 e OB-12.
- Regola, formato d'ingresso. Il confine legge un input in un formato **dichiarato**, mai dedotto: né dai byte, né dall'estensione, né da un secondo tentativo con un altro formato dopo un errore (NC-21). La dichiarazione di un formato è una costante del core, una per formato; una sola funzione del core elenca le dichiarazioni, come il registro dei tipi (§3.1). Ogni dichiarazione porta:
  1. il prefisso dei codici del formato: `fits`, `raw`;
  2. il limite di dimensione: per FITS `fits_limits::max_file_bytes` (ADR-03); per RAW `raw_limits::max_file_bytes` = 1 073 741 824 (ADR-08.b §2.1);
  3. il piano degli header: per FITS `fits_plan`, letto prima del digest e confrontato con `require_planned_header` sui byte verificati (comportamento invariato); per RAW nessun piano degli header: il solo valore usato prima del digest è la dimensione da `fstat`, e nessun byte del file si interpreta prima del digest (la memoria del decoder si pianifica dopo, sul buffer verificato, ADR-08.b §2.8).

  Il formato di ogni ruolo di input di un tipo di job è dichiarato nella voce del registro (§3.1, punto 1); l'operatore apre il ruolo con quella dichiarazione.
- Regola, apertura. Un solo punto del codice esegue il protocollo di apertura: `open` con `O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOCTTY`, `fstat` sullo stesso descrittore, file regolare, limite del formato prima di ogni lettura e di ogni allocazione del buffer. Esiti: file assente `io` `input.missing` (comune a ogni formato); altro errore di `open` `io` `<prefisso>.open`; errore di `fstat` `io` `<prefisso>.fstat`; file non regolare `invalid_input` `<prefisso>.not_regular`; dimensione oltre il limite `limit_exceeded` `<prefisso>.max_file_bytes`, con dimensione e limite nel contesto. Per FITS i codici risultanti sono quelli di oggi (ADR-03): `open_fits_input` può restare come funzione che chiama il protocollo con la dichiarazione FITS, così chiamanti e test esistenti non cambiano. Una seconda funzione che ricopia il protocollo per un altro formato non è ammessa: sarebbero due proprietari dello stesso obbligo. L'input aperto porta la propria dichiarazione: `check_path_names`, `check_unchanged`, `digest_input`, `read_input_bytes` e `reverify_input` restano funzioni uniche per tutti i formati e usano il prefisso dell'input nei codici propri del formato (`<prefisso>.fstat`, `<prefisso>.read`); i codici `input.*` restano comuni. Il punto `input.open` del `job_probe` (`fits.cpp:561`) resta con il solo percorso, perché T-M3BASE-10 lo conta (`test_conformance.cpp:1192`); un punto distinto riporta percorso e formato di ogni apertura (nome scelto dall'unità di codice).
- Regola, lettura verificata. `read_verified_input` riceve l'input aperto, il percorso, l'identità attesa, il budget e il piano del formato (per FITS il `fits_plan` letto prima; per RAW nessuno). Esegue nell'ordine di oggi: controllo del percorso (`check_path_names`), lettura intera, digest (`require_input_digest`), confronto del piano solo per i formati che ne hanno uno, `check_unchanged`; restituisce il buffer verificato. Quel buffer è l'unico che l'operatore passa alla decodifica (`fits_open_memfile` per FITS, `LibRaw::open_buffer` per RAW, ADR-08.b §2.1): nessuna seconda lettura del file fino alla riverifica. Per un formato senza piano nessuna lettura del file precede la lettura intera.
- Regola, riverifica. `reverify_input` riapre il percorso con la dichiarazione del formato del ruolo, quindi con il limite e i codici di quel formato; confronti e ordine restano quelli di oggi.
- Stato: IMPLEMENTATO NEL PROPRIETARIO (unità U0, `docs/evidence/M3a-U0.md`). Le dichiarazioni `fits_input_format()` e `raw_input_format()` (struttura `input_format`: prefisso, limite, piano degli header) e l'elenco `input_formats()` stanno in `input_format.hpp` e `input_format.cpp`; `open_input` (`fits.cpp`) è il protocollo di apertura unico, e `open_fits_input` lo chiama con la dichiarazione FITS. L'input aperto porta la propria dichiarazione: `check_path_names`, `check_unchanged`, `identity_of`, `read_input_bytes` e le letture con `pread` emettono i codici `.fstat` e `.read` da un'unica funzione che prende il prefisso dall'input, senza formato nel chiamante. Limite delle prove: un errore di `fstat` su un descrittore aperto (nel protocollo dopo `open`, in `check_unchanged`, in `identity_of`) e del `pread` di controllo oltre la fine non si provocano nei test; il codice di questi rami è provato solo attraverso la funzione comune, esercitata dalla lettura con errore iniettato e da `check_path_names`, e per il ramo del protocollo dallo stesso `format` dei rami `.open`, `.not_regular` e `.max_file_bytes`. `read_verified_input` riceve il piano come puntatore, presente se e solo se il formato dell'input ha il piano degli header (`ASTRO_ASSERT`); `reverify_input` riceve la dichiarazione del ruolo, uguale a quella dell'input aperto se c'è (`ASTRO_ASSERT`). `job_type_entry::role_formats` dichiara il formato di ogni ruolo: oggi tutti FITS, perché ogni tipo registrato decodifica FITS. Il punto `input.open_format` del `job_probe` riporta `<prefisso>\t<percorso>`. Nessun tipo registrato ha ancora un ruolo RAW: il percorso RAW è provato al confine senza job (T-M3BASE-20, T-M3BASE-21), e le varianti RAW di T-M3BASE-11 e T-M3BASE-12 arrivano con l'unità che registra il tipo RAW.
- Limiti:
  - per un ruolo il cui formato non ha piano degli header, il caso «header cambiato fra il piano e la verifica» di T-M3BASE-11 non ha oggetto, perché nessun header si legge prima del digest; gli altri casi di T-M3BASE-11 valgono invariati. Per i tipi registrati oggi, tutti con input FITS, T-M3BASE-11 non cambia;
  - il valore del limite RAW è una stima non misurata (ADR-08.b §2.1);
  - la prenotazione dei byte del file nel piano del job RAW (ADR-08.b §2.8, «byte del file già prenotati dalla lettura») resta di OB-08 e dell'unità che registra il tipo; questo emendamento non la fissa.

**OB-12 — Originali in sola lettura.**
- Testo: il prodotto apre un input solo con `open_fits_input`, cioè `O_RDONLY` su un file regolare. Dopo qualunque job, riuscito o fermato, digest, dimensione, inode, `nlink`, `mtime` e permessi di ogni input sono invariati.
  Testo emendato (L1, §2.5): il prodotto apre un input, di qualunque formato, solo con il protocollo di apertura unico del confine, di cui `open_fits_input` è la chiamata con la dichiarazione FITS. Il decoder RAW non apre file: riceve il buffer verificato (ADR-08.b §1, §2.1; T-M3A-RAW-07).
- Fonte: NFR-DATA-01; NFR-SEC-03; AGENTS.md §Perimetro (originali immutabili).
- Proprietario: lettura degli input (`open_fits_input`, `fits.cpp:543`; dall'emendamento L1 il protocollo di apertura unico).
- Nasce: apertura dell'input. Si estingue: chiusura del descrittore.
- Stato: IMPLEMENTATO NEL PROPRIETARIO. Le altre aperture di file in `src/core/` fuori dallo store e dallo scratch riguardano gli artefatti del progetto e la directory di destinazione dell'export (`project_export.cpp:193`, `:241`), la cache delle miniature (`thumbnail_cache.cpp:133`, `:204`), directory e file di sistema in sola lettura (`budget.cpp:70`, `resources.cpp:607`, `:722`). Nessuna apre un input con permessi di scrittura.
- Prova: T-M3BASE-12; per l'emendamento L1, T-M3BASE-20 e T-M3BASE-22.

### 2.6 Nessun fallback, nessuna pubblicazione su errore

**OB-13 — Una sola esecuzione per job.**
- Testo: una chiamata del runner avvia al più un job sullo slot ed esegue `work` al più una volta. Un arresto è definitivo: il runner non ritenta e non chiama un altro `work`, un altro backend o un'altra residenza.
- Fonte: NC-21; NFR-FBK-01.
- Proprietario: runner dei job (`run_job_files`, `job.cpp:270-299`: nessun ciclo, nessun secondo `work`).
- Nasce: ingresso nel runner. Si estingue: ritorno del runner o propagazione dell'arresto.
- Stato: IMPLEMENTATO NEL PROPRIETARIO. **Limite**: il runner non vede i percorsi interni di `work`. Che un operatore non scelga un percorso alternativo dopo un errore resta una responsabilità del singolo operatore, verificata dalla review e dalla quota di T-FBK-01 della sua slice.
- Prova: T-M3BASE-13.

**OB-14 — Nessun artefatto pubblicato su errore.**
- Testo: un job pubblica i propri file solo con un'unica transazione `commit_job`, dopo che `work` è tornato senza errore e senza cancel. Dopo un arresto lo slot non pubblica alcun output del job; con la generazione corrente l'unico artefatto nuovo del nodo è la provenance. Con generazione superata, `commit_job` registra gli output come cache del nodo senza la riga della provenance e senza pubblicarli sullo slot (`sqlite_files.cpp:163`, ADR-04), e il job ha un solo record `failed` `dag.stale_publish`. Un errore dopo un `commit_job` riuscito è fuori dall'obbligo (OB-02).
- Fonte: NFR-DATA-01 («un output parziale non è pubblicato come completo»); ADR-04 §Scrittura atomica; contratto di `run_job_files` in `job.hpp`.
- Proprietario: runner dei job (`job.cpp:285-292`, con `sqlite_dag_store::commit_job`).
- Nasce: primo `write_files` dentro `work`. Si estingue: `commit_job` riuscito, oppure arresto registrato senza `commit_job`.
- Stato: IMPLEMENTATO NEL PROPRIETARIO. **Limiti**:
  - i file scritti da `work` con `write_files` prima di un arresto restano sotto `artifacts/` senza riga finché la recovery successiva non li rimuove come orfani (`sqlite_recovery.cpp:342-351`); è la lettura che questo contratto dà di «temporanei rimossi» in ADR-06.a §Guasto e cancel (*ipotesi* dichiarata, confermata dal coordinatore come ipotesi e verificata da T-M3BASE-14);
  - le voci della cache delle miniature scritte da `thumbnail_cache::get` o `rebuild` prima di un arresto restano nella cache, che non è un artefatto del job e si verifica a ogni lettura (`thumbnail_cache.hpp:41`).
- Prova: T-M3BASE-14.

### 2.7 Checkpoint, resume, temporanei

**OB-15 — Resume dei task solo con ricetta identica.**
- Testo: un resume dei task riprende solo con `node_key`, `recipe_digest`, `backend` e `task_count` uguali al checkpoint e con ogni risultato parziale riverificato; altrimenti si ferma con `resume_mismatch` `sched.resume_recipe`, `metadata_missing` `sched.checkpoint_missing` o `integrity` `sched.checkpoint_digest`, senza ripartire da zero.
- Fonte: NFR-FBK-01; NC-21; ADR-06.a §Checkpoint e resume, §Estensione R3.2 («Scratch e checkpoint del job»).
- Proprietario: scheduler (`load_resume`, `verify_resume_digests`, `scheduler.cpp:88`, `:109`; `write_progress`, `scheduler.cpp:190`).
- Nasce: `job_scheduler::run` con `spec.resume`. Si estingue: confronti superati prima del primo task, oppure arresto.
- Stato: IMPLEMENTATO NEL PROPRIETARIO. Solo la calibrazione dichiara un checkpoint.
- Prova: T-M3BASE-15 (solo tipi che dichiarano checkpoint).

**OB-19 — Checkpoint e staging rimossi solo dopo la pubblicazione.**
- Testo: il checkpoint e lo staging nominato di un job restano dopo stop e cancel e si rimuovono solo dopo un `commit_job` riuscito.
- Fonte: ADR-06.a §Checkpoint e resume («lo rimuove il chiamante […] solo dopo pubblicazione riuscita»), §Estensione R3.2 («Scratch e checkpoint del job»).
- Proprietario: runner dei job (`run_job_files`, l'unico punto che sa se `commit_job` è riuscito).
- Nasce: primo checkpoint scritto (`write_progress`, `scheduler.cpp:190`). Si estingue: rimozione dopo `commit_job` riuscito.
- Stato: IMPLEMENTATO NEL PROPRIETARIO (intervento I4). `work` dichiara checkpoint e staging in `job_files_result::scratch` (la calibrazione con checkpoint, `calibration_job.cpp:341`); `run_job_files` li prende al ritorno di `work` (`job.cpp:312`) e li rimuove con `remove_checkpoint` e `remove_named_spill_file` solo dopo il `try` che termina con `commit_job` riuscito (`job.cpp:339`). Stop, cancel e commit fallito passano dai rami `catch` e non rimuovono nulla. Un errore della rimozione si propaga con il job già `completed` e pubblicato, come l'errore a `commit.after_commit` (OB-02). **Limite** della prova: la suite osserva solo il confine e non distingue la sede della rimozione (vedi `docs/evidence/M3-base-suite.md`).
- Prova: T-M3BASE-19 (solo tipi che dichiarano checkpoint).

**OB-16 — Resume del workflow solo con definizione identica.**
- Testo: `resume_workflow` riprende solo un workflow registrato con definizione canonica identica e con gli slot che puntano ai nodi dei passi. Riusa una fase solo se tutti i suoi output sono presenti con il digest registrato e mette il contesto di resume nella provenance di ogni job eseguito.
- Fonte: NFR-FBK-01; ADR-04 §Estensione R4 («Workflow e resume»).
- Proprietario: workflow (`resume_workflow`, `workflow.cpp:113`).
- Nasce: ingresso in `resume_workflow`. Si estingue: fine di `execute` con `resume_scope` attivo (`workflow.cpp:177`).
- Stato: IMPLEMENTATO NEL PROPRIETARIO. La provenance dei rifiuti è OB-03.
- Prova: T-M3BASE-16.

**OB-17 — Temporanei solo in directory dichiarate.**
- Testo: un job scrive su disco soltanto sotto la radice del progetto (store e cache) e sotto la directory di scratch ricevuta in `resource_config` (spill, staging, checkpoint); nessun sorgente del prodotto usa una directory temporanea di sistema implicita.
- Fonte: ADR-06.a §Spill; AGENTS.md §Perimetro (temporanei nel progetto).
- Proprietario: scheduler, che crea `spill_area` solo da `plan.spill` (`scheduler.cpp:382`) e scrive il checkpoint solo in `spec.checkpoint_dir` (`scheduler.cpp:201`).
- Nasce: `job_scheduler::run` con spill o checkpoint dichiarati. Si estingue: rilascio dell'area di spill a fine `run`; checkpoint e staging fino a OB-19.
- Stato: IMPLEMENTATO NEL PROPRIETARIO. In `src/` nessuna occorrenza di `/tmp`, `temp_directory_path`, `TMPDIR`, `mkstemp` o `tmpfile` (grep al commit `db2a389`). Nessuna regola di lint lo impone.
- Prova: T-M3BASE-17.

### 2.8 Offline

**OB-18 — Nessuna rete.**
- Testo: `libastrocore` e gli eseguibili del prodotto non hanno simboli indefiniti di rete né `NEEDED` vietati, aprono CFITSIO solo da memoria, e un job di ogni tipo registrato non esegue chiamate di sistema di rete.
- Fonte: ADR-13.a §Clausole; NFR-OFF-01.
- Proprietario: libreria del core (`libastrocore`), controllata al link da `tools/check_offline.sh` (T-M2F-01). La clausola CFITSIO sta nella lettura degli input: `fits_open_memfile` (`fits.cpp:89`) e `fits_create_memfile` (`fits.cpp:688`).
- Nasce: link della libreria. Si estingue: controllo statico superato e traccia di rete vuota.
- Stato: IMPLEMENTATO NEL PROPRIETARIO. La traccia a runtime T-M2F-02 (`tools/check_m2f02_network.sh`) esegue `m2e_workflow`, che chiama solo `run_calibration_job` (`tests/unit/m2e_workflow.cpp:45`). Gli altri tipi non sono tracciati.
- Prova: T-M3BASE-18.

**OB-20 — `measure` presente in ogni uscita di operatore.**
- Testo: ogni immagine prodotta da un operatore registrato porta il campo `measure` (anche vuoto) e `operator_output_invariants_hold` è falso se manca; con `demosaic` vale anche la regola dei codici `samples.interpolated` e `variance.fixed_weight_approximation`.
- Fonte: ADR-03.b §b.4 («`operator_output_invariants_hold`: `measure` presente e valido in ogni immagine prodotta da un operatore»).
- Proprietario: `operator_output_invariants_hold` (`image.cpp`), chiamata dalla suite di conformità sull'uscita di ogni tipo registrato; ogni operatore imposta `measure` nella propria uscita.
- Nasce: costruzione dell'immagine di uscita dell'operatore. Si estingue: pubblicazione dell'uscita.
- Stato: DEBITO (intervento I6). Oggi `measure` assente non rende falso l'invariante.
- Prova: casi 1 e 2 della sezione «Casi di `operator_output_invariants_hold` tolti da U1b» di `plans/M3a.md`; casi 3–6 in M3.d, con i tipi `demosaic` e `variance_scope`.

## 3. Suite di conformità

### 3.1 Registro dei tipi di job

La suite non contiene un elenco di tipi. Li legge da **un solo registro nel codice del prodotto**, che oggi è MANCANTE: i test esistenti elencano i tipi a mano. Per esempio, `T_M2R5_07_denormal_mask_off_stops_run_job_files_before_the_node_and_records_the_stop` (`tests/unit/test_simd.cpp:1206`) copre calibrazione, master, import e `run_branches`, ma non `io.thumbnail` né `cache.rebuild`.

Proprietà del registro, che il codice dell'unità implementa senza cambiare questo contratto:

1. Una sola funzione del core restituisce l'elenco dei tipi registrati. Ogni voce porta `operator_id` (la costante dell'operatore), i ruoli degli input con il formato di ciascuno (emendamento L1, §2.5), le varianti dichiarate prima dell'esecuzione (per la calibrazione le due residenze), se il tipo passa dallo scheduler e se dichiara un checkpoint.
2. Ogni voce porta un costruttore del job minimo. Dagli input sintetici della suite (percorso e digest per ruolo, generati in `tests/fixtures/` da chi non scrive l'operatore) costruisce il nodo e una chiamata del job che passa da `run_job_files`. Se il costruttore manca, la suite fallisce per quella voce.
3. Uno strumento di controllo (`python3 -S`) confronta l'insieme degli `operator_id` del registro con le costanti `*_operator_id` dichiarate in `src/core/include/astro/` e con le funzioni che chiamano `run_job_files`, escluse le composizioni di §1. Un tipo non registrato fa fallire `tools/check.sh`.

### 3.2 Casi

Ogni caso gira per ogni voce del registro e per ogni variante. Il costruttore minimo (§3.1) avvolge il `work` dell'operatore in un `work` della suite che il runner invoca: conta le proprie invocazioni e ospita le iniezioni «dentro `work`». Le verifiche comuni a ogni arresto di un job (non si applicano a T-M3BASE-03, che non ha job):

- (a) record del job con stato, categoria e codice attesi;
- (b) artefatto `provenance` del nodo con la stessa causa;
- (c) slot non pubblicato per quel job; con la generazione corrente nessun artefatto nuovo del nodo oltre la provenance; con generazione superata gli output restano come cache del nodo, senza riga `provenance` (OB-14, T-M3BASE-14);
- (d) uscita 2 o 3 da `job_outcome::exit_code`, oppure, per un arresto prima del lavoro, da `exit_code_for` sull'eccezione propagata;
- (e) input invariati (OB-12).

| Caso | OB | Iniezione | Atteso |
|---|---|---|---|
| T-M3BASE-01 | 01 | Eccezione dal `prepare` di `prepare_job_start`; nodo dello slot assente dallo store | Arresto prima del lavoro: (a)-(e), record `stop_before_work: true`, eccezione propagata con stessa categoria, codice e contesto |
| T-M3BASE-02 | 02 | `fault_injector` dello store al primo passo di `write_files`; eccezione non `stop_error` lanciata dentro `work`; guasto al punto `commit.after_commit` | Per le prime due iniezioni (a)-(e); per l'eccezione estranea `integrity` `job.unexpected_exception` e rilancio invariato; dopo `commit.after_commit` errore propagato, job `completed` e output pubblicati |
| T-M3BASE-03 | 03 | `resume_workflow` con definizione diversa; `run_workflow` con slot che punta a un altro nodo | Record di arresto del workflow con chiave l'identità del workflow, categoria, codice e contesto, solo stringhe e interi; nessun job avviato |
| T-M3BASE-04 | 04 | MXCSR vietato in un processo figlio (ognuno dei quattro modi di `require_fp_environment`) | `numeric` `cpu.fp_mode` prima di leggere il nodo; (a)-(e); nessuna chiamata al costruttore del nodo né a `work` |
| T-M3BASE-05 | 05 | Tipi con scheduler: MXCSR vietato impostato nel thread del worker dal punto `cpu.task_start` portato in `worker_loop` prima della guardia (debito di OB-05), in ogni worker | `numeric` `cpu.fp_mode`; contatore delle invocazioni di `run_task` a zero; contatori dei kernel a zero; (a)-(e) |
| T-M3BASE-06 | 06 | `cancel_token` richiesto dal `work` della suite dopo il ritorno del `work` dell'operatore, quindi dopo ogni controllo dell'operatore e dello scheduler | `cancelled` `sched.cancelled`, uscita 3, (a)-(e) |
| T-M3BASE-07 | 07 | `cancel_token` richiesto durante `prepare` e durante il primo blocco di righe, con un punto di iniezione del confine | `cancelled` `sched.cancelled`; con più blocchi ancora da elaborare all'iniezione, blocchi di righe elaborati dopo la richiesta al più uno per worker, contati dal `work` della suite a ogni blocco elaborato, indipendentemente da `check_cancel` (senza i controlli il contatore supera il limite e il caso fallisce); al più un blocco letto nel `prepare` dopo la richiesta (il `prepare` gira in un solo thread); dopo la richiesta al più `worker_count − 1` task avviati, cioè quelli già ammessi da altri worker prima della richiesta, e nessun task nuovo; nessun `finalize` |
| T-M3BASE-08 | 08 | Pool con budget pari al picco del piano meno un byte; pool occupato da un'altra prenotazione | `budget` `sched.job_exceeds_budget` senza task avviati; con pool occupato attesa FIFO e poi esecuzione; `job_plan` registrato uguale a quello eseguito |
| T-M3BASE-09 | 09 | Job riuscito e job fermato in `work`, con `budget_snapshot` e risorse; job senza snapshot | Budget con risorse nella provenance di entrambi; nel riuscito `details.cpu` per i backend `cpu_*` e `details.resource_plan` uguale al piano; senza snapshot nessun `budget` |
| T-M3BASE-10 | 10 | Slot creato con il nodo di una richiesta, job lanciato con una richiesta diversa | `invalid_input` `*.node_mismatch` con le due chiavi, nessun input aperto (contatore delle aperture a zero) |
| T-M3BASE-11 | 11 | Per ogni ruolo di input: file assente; un byte cambiato; file cambiato dopo la verifica e prima della pubblicazione; header cambiato fra il piano e la verifica, per ogni valore usato nel piano | `io` `input.missing`; `integrity` `input.digest_mismatch`; `integrity` `input.modified`, `input.size_changed` o `input.digest_mismatch`; nella variante a blocchi, blocco cambiato dopo la passata P0: `integrity` `input.consumed_digest`; header cambiato: stop `integrity` prima di usare un pixel; (a)-(e) |
| T-M3BASE-12 | 12 | Tutti i casi della suite | (e) dopo ogni caso: digest, dimensione, inode, `nlink`, `mtime`, permessi |
| T-M3BASE-13 | 13 | Stessa iniezione di T-M3BASE-02 | Un solo record di job per lo slot; `work` chiamato una volta (contatore del `work` della suite); backend della provenance uguale a quello del nodo |
| T-M3BASE-14 | 14 | Arresto dopo un `write_files` riuscito dentro `work`; generazione dello slot cambiata durante `work` | Con generazione corrente: solo la provenance come artefatto nuovo; con generazione superata un solo record `failed` `dag.stale_publish`, output come cache del nodo, nessuna riga `provenance` di `completed`, slot non pubblicato; orfani rimossi alla riapertura con recovery |
| T-M3BASE-15 | 15 | Tipi con checkpoint, dopo uno stop a metà dei task: resume con `tile_rows` diverso, con backend diverso, con un parziale alterato; resume senza checkpoint; resume identico | Diversi: `resume_mismatch` `sched.resume_recipe`; parziale alterato: `integrity` `sched.checkpoint_digest`; senza checkpoint: `metadata_missing` `sched.checkpoint_missing`; in questi nessun task eseguito. Identico: `tasks_resumed` uguale ai task del checkpoint e output bit-identico all'esecuzione senza interruzioni |
| T-M3BASE-16 | 16 | Workflow con un passo per tipo, resume dopo arresto del secondo passo; resume con un output del primo passo rimosso prima della recovery (riapertura del progetto dopo la rimozione); resume con un output del primo passo rimosso dopo la recovery; resume con un output del primo passo alterato; resume di un workflow non registrato | Primo passo riusato, contesto di resume nella provenance dei passi rieseguiti; rimosso prima della recovery (output classificato `absent`): primo passo rieseguito; rimosso dopo la recovery (riga ancora `present`): `io` `resume.artifact_missing`, nessun passo eseguito e nessun job nuovo; alterato: `integrity` `resume.artifact_digest`; non registrato: `resume_mismatch` `resume.unknown_workflow` |
| T-M3BASE-17 | 17 | `TMPDIR` e `HOME` puntati a directory vuote sotto `tmp/` | Directory ancora vuote dopo ogni caso; regola di lint su `src/` per `/tmp`, `temp_directory_path`, `TMPDIR`, `mkstemp`, `tmpfile` |
| T-M3BASE-18 | 18 | L'eseguibile della suite sotto `strace -f -e trace=network`, col canarino di `check_m2f02_network.sh` | Zero chiamate di rete per tutti i tipi |
| T-M3BASE-19 | 19 | Tipi con checkpoint: stop a metà, cancel a metà, pubblicazione riuscita | Checkpoint e staging presenti dopo stop e cancel; assenti dopo la pubblicazione |
| T-M3BASE-20 | 11, 12 (L1) | Per ogni formato dell'elenco delle dichiarazioni, sul confine senza job: file sparso sotto `tmp/` di dimensione pari al limite del formato più un byte; file sparso di dimensione pari al limite; directory al posto del file; file senza permesso di lettura (test eseguito da un utente non root); percorso assente | Oltre il limite: `limit_exceeded` `<prefisso>.max_file_bytes` con dimensione e limite nel contesto, lanciato dall'apertura, quindi nessun input restituito, nessuna lettura e nessun buffer; pari al limite: nessuno stop di limite e descrittore con accesso `O_RDONLY` (flag letti da `/proc/self/fdinfo`); directory: `invalid_input` `<prefisso>.not_regular`; senza permesso: `io` `<prefisso>.open`; assente: `io` `input.missing` |
| T-M3BASE-21 | 11 (L1) | Per ogni formato senza piano degli header (oggi RAW), sul confine senza job, con byte arbitrari scritti dal test che non sono un file valido del formato: lettura verificata corretta; un byte cambiato; file cresciuto dopo l'apertura; percorso che indica un altro file; file assente alla riverifica; file cambiato fra la lettura verificata e la riverifica | Corretta: buffer restituito con SHA-256 e dimensione attesi, nessuna lettura del file prima della lettura intera (osservatore delle letture); un byte cambiato: `integrity` `input.digest_mismatch`; cresciuto: `integrity` `input.size_changed`; altro file: `integrity` `input.modified`; assente: `io` `input.missing`; cambiato: `integrity` `input.modified`, `input.size_changed` o `input.digest_mismatch`. I byte non validi provano che il confine non interpreta il file prima del digest |
| T-M3BASE-22 | 11, 12 (L1) | Per ogni voce del registro e ogni ruolo, in un job riuscito della suite | Ogni apertura dell'input del ruolo (punto del `job_probe` con percorso e formato) usa il formato dichiarato dalla voce per quel ruolo; una voce con un ruolo senza formato fa fallire la suite per quella voce |

T-M3BASE-20 e T-M3BASE-21 girano per formato, non per voce del registro, e non hanno un job: non si applicano le verifiche (a)-(d). Per i ruoli di un tipo registrato con un formato senza piano degli header, T-M3BASE-11 omette l'iniezione «header cambiato fra il piano e la verifica» (§2.5, emendamento L1, limiti); nessun altro caso esistente cambia significato.

### 3.3 Ingresso di un operatore nuovo

Un operatore nuovo entra solo aggiungendo la propria voce al registro (§3.1, punti 1 e 2) e passando la suite. Nessun test degli obblighi OB-01…OB-19 si scrive per l'operatore; nessun confine si modifica nella slice. Se l'operatore non passa un caso, si corregge l'operatore o, se il difetto è del confine, il confine (`plans/M3.md`, punto 3).

## 4. Debiti

Modifica minima che porta l'obbligo nel proprietario. Ogni riga è un'unità della base, con contratto e test prima del codice.

| OB | Stato | Dove oggi | Modifica minima |
|---|---|---|---|
| OB-03 | NEL PROPRIETARIO (I4) | — | Pagato: `stop_workflow` scrive in `settings` il record `workflow_stop.<sha256 della definizione>.<esecuzione>` (solo stringhe e interi, `insert_setting` senza sostituzione) per ogni arresto di `run_workflow` e `resume_workflow` prima del primo passo, poi lo propaga; nessuna tabella nuova né migrazione |
| OB-04 | NEL PROPRIETARIO (I1) | — | Pagato: copie in `work` e nelle lambda tolte, nodo di `cache.rebuild` costruito dal runner dopo il controllo FP |
| OB-05 | NEL PROPRIETARIO (I3) | — | Pagato: il punto `cpu.task_start` scatta in `worker_loop` (`begin_worker_task`) prima di `require_fp_environment`; nei worker la guardia di kernel non lo richiama. La guardia FP di kernel resta anche nei worker, come chiede ADR-07.a: nessun emendamento |
| OB-07 | NEL PROPRIETARIO (I2) | — | Pagato: `job_context::check_cancel()` chiamato dai `prepare` a ogni blocco letto (calibrazione intera, P0 e blocchi, master, tipi di I/O); `digest_fits_input`, inventario e cache ricevono il token. Il `finalize` del master controlla il cancel dall'intervento I3. Le prove del cancel nel `finalize` del master e nel `prepare` dei tipi di I/O su più blocchi vengono dall'intervento I5 |
| OB-08 | NEL PROPRIETARIO (I2) | — | Pagato: i tre tipi di I/O dichiarano un `job_plan` di un task e passano da `job_scheduler::run`; il picco copre digest e task, presi dalla prenotazione del job |
| OB-09 | NEL PROPRIETARIO (I1) | — | Pagato: `run_job_files` riceve la `resource_config` (`job_request::resources`) e la serializza dopo il proprio controllo FP; compone `details.cpu` dal nodo e `details.resource_plan` dal piano dichiarato (`job_files_result::plan`). Per i tipi di import `budget.resources` e il piano arrivano con OB-08 |
| OB-10 | NEL PROPRIETARIO (I1) | — | Pagato: `run_job_files` riceve il nodo atteso (`job_request::node`) e lo confronta prima di `work`. Il codice resta quello del tipo (`job_request::mismatch_code`): un codice unico `job.node_mismatch` richiede l'emendamento di ADR-07.a e dei test che citano i codici attuali, fuori da I1 |
| OB-11 | NEL PROPRIETARIO (I3) | — | Pagato: `read_verified_input`, `require_planned_header`, `require_input_digest` e `reverify_input` in `hash.cpp`; riverifica prima della pubblicazione nel `finalize` di ogni tipo; gli operatori non confrontano digest. Restano i limiti di §2.5 (header di fase e linearità nella residenza a blocchi, confronto implicito per i tipi di I/O) |
| OB-11, OB-12 (L1) | NEL PROPRIETARIO (U0) | — | Pagato: dichiarazioni `input_format` ed elenco `input_formats()`; protocollo di apertura unico `open_input`, con `open_fits_input` come chiamata con la dichiarazione FITS; codici propri del formato col prefisso dell'input aperto; piano facoltativo in `read_verified_input`; formato del ruolo in `reverify_input`; `job_type_entry::role_formats` nel registro (§3.1); punto `input.open_format`. Prove T-M3BASE-20…22 (`tests/unit/test_conformance_format.cpp`). Le varianti RAW di T-M3BASE-11 e T-M3BASE-12 restano all'unità che registra il tipo RAW |
| OB-19 | NEL PROPRIETARIO (I4) | — | Pagato: `job_files_result::scratch` dichiara checkpoint e staging; `run_job_files` li rimuove dopo `commit_job` riuscito; tolta la rimozione dalla calibrazione |
| OB-20 | DEBITO (I6) | `operator_output_invariants_hold` non richiede `measure` | L'invariante richiede `measure`; calibrazione, master e tipi di I/O impostano `measure` nelle loro uscite; i test che costruiscono uscite senza `measure` si aggiornano nello stesso intervento |
| Suite | FATTA (`725bdac`) | — | Registro dei tipi (§3.1), strumento di controllo della completezza, suite T-M3BASE-01…19 |
| Prove | FATTA (I5) | — | Casi in `tests/unit/test_conformance_gaps.cpp` che rilevano le nove mutazioni sopravvissute (`m5` di I2; `m04`, `m06`, `m07`, `m08`, `m09`, `m12`, `m15`, `m18` di I3), registrati in `docs/evidence/M3-base-I5.md`; nessuna equivalente. Limite: il caso di `m12` scrive da una mappatura condivisa su una pagina già sporca, perché solo così mtime e ctime restano invariati |

## 5. Limiti

- Lo stato è letto al commit `db2a389` sul solo codice di `src/core/`: nessun test eseguito in questa unità, nessuna build. I file di test citati sono stati letti, non eseguiti.
- Gli obblighi coprono il confine dei job. I percorsi interni degli operatori (scelta di metodi, rejection, soglie) restano nei contratti delle slice.
- Le guardie FP delle funzioni di libreria chiamabili fuori da un job (OB-04) e la cache delle miniature (OB-14) restano fuori da questo contratto.
- L'interfaccia del registro (§3.1) è fissata solo nelle proprietà; firma e posizione le decide l'unità di codice.
