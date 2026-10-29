# Contratto ADR-06.a: budget, pool e scheduler

Stadio: ADR-06.a (registro riga 3397). Vincoli fissati in [ADR-M1-stack](../decisions/ADR-M1-stack.md). Piano §20 ADR-06, F-RES-01, F-RES-02, F-RES-03, NFR-FBK-01. Unità: M2.d. Errori: [modello comune](ADR-05-dag-provenance.md#modello-derrore-comune).

Il pool limita le prenotazioni, non l'RSS. L'RSS si misura e si riporta; non è garantito.

## Budget

`astro::budget_mode`: `automatic`, `manual`. Il budget si calcola nel preflight e resta fisso per tutto il job. Il risultato è `budget_snapshot`, registrato nella provenance.

| Campo di `budget_snapshot` | Tipo | Note |
|---|---|---|
| `mode` | `budget_mode` | |
| `budget_bytes` | `uint64_t` | Multiplo di 1 MiB, arrotondato per difetto. |
| `mem_total_bytes`, `mem_available_bytes` | `uint64_t` | Da `/proc/meminfo`. |
| `cgroup_max_bytes`, `cgroup_current_bytes` | `std::optional<uint64_t>` | cgroup v2; `nullopt` se assente o `max`. |
| `psi_some_avg10` | `std::optional<double>` | `/proc/pressure/memory`; `nullopt` se assente. |
| `fraction` | `double` | Solo in `automatic`. |
| `manual` | `std::optional<manual_budget>` | Solo in `manual`. |

I percorsi `/proc` e `/sys/fs/cgroup` sono iniettabili per i test.

### Automatico (F-RES-01)

```
avail  = min(mem_available, cgroup_max − cgroup_current se cgroup_max presente)
f      = 0.375 se psi_some_avg10 assente o < 10.0, altrimenti 0.25
budget = floor_MiB(avail · f)
```

Requisito della frazione normale: dopo il preflight un altro processo può crescere fino a 0.40·`avail` con al massimo 16 MiB di swap oltre il fondo, memory full avg10 < 1 e almeno 3 GiB di MemAvailable. Il gate M2 l'ha misurato sul PC di sviluppo (16 core, 31 GiB, swappiness 120: il perimetro è quello) (`docs/evidence/M2-gate.md`): 0.50 regge 0.25·`avail` in 2 prove su 3 (la terza a 16,5 MiB di swap contro 16) e non regge 0.40 (swap +73 MiB a PSI 0); 0.375 e 0.25 reggono 0.40·`avail` in 3 prove su 3 ciascuna. La frazione normale è 0.375, il valore più alto fra quelli provati che rispetta il requisito. La frazione sotto pressione resta 0.25 e la soglia PSI resta 10.0: il ramo scatta sui file iniettati e sul segnale reale, e con il budget 0.25 un job avviato dopo una pressione reale rispetta tutti i criteri (S4 quinquies: `psi_some_avg10` 17,6, swap 0, full avg10 max 0,83); il valore 10.0 non è confermato come ottimo, perché sul PC misurato lo swap arriva prima che la PSI salga (0.50 con crescita 0.40·`avail`: +73 MiB di swap a PSI 0), quindi è la frazione normale a proteggere il sistema. Il calcolo usa rapporti interi esatti (floor di `avail·3/8` e `avail/4`), senza overflow. La formula non cambia durante il job.

`/proc/meminfo` illeggibile o senza `MemAvailable`: `io`, `budget.meminfo`. cgroup e PSI assenti non sono errori: compaiono `nullopt` nello snapshot. `cgroup_current ≥ cgroup_max`: `budget`, `budget.cgroup_over_limit`, con i due valori nel contesto; la sottrazione non si esegue mai in `uint64_t` senza questo controllo. File cgroup o PSI presenti ma non interpretabili: `invalid_input`, `budget.cgroup_parse` o `budget.psi_parse`.

### Manuale (F-RES-02)

Grammatica esatta, senza spazi: `<intero>MiB`, `<intero>GiB`, `<intero>%:<denominatore>` con denominatore ∈ {`mem_total`, `mem_available`, `cgroup_max`}.

- MiB = 2^20 byte, GiB = 2^30 byte. `MB`, `GB`, `M`, `G`, decimali e unità assenti: `invalid_input`, `budget.syntax`.
- Percentuale fra 1 e 100; denominatore obbligatorio, letto come snapshot nel preflight.
- `cgroup_max` senza limite cgroup: `metadata_missing`, `budget.no_cgroup_limit`.
- Valore zero o maggiore di `mem_total`: `invalid_input`, `budget.out_of_range`.
- Persistito in `settings` come JSON canonico; rivalidato a ogni lettura.

## Pool e prenotazioni

`astro::memory_pool` con `budget_bytes`. `reserve(bytes, label)` restituisce una `reservation` RAII, tutto o niente.

- Somma delle prenotazioni sempre ≤ budget, anche sotto concorrenza (T-M2D-04).
- `pool_buffer` si alloca solo dentro una prenotazione. Superarla: `budget`, `pool.reservation_exceeded`. Indica un picco dichiarato male dall'operatore: si corregge l'operatore.
- Il buffer di lettura degli input (ADR-05) è prenotato come gli altri.

## Job

`astro::job_plan`, dichiarato prima dell'esecuzione:

| Campo | Note |
|---|---|
| `shared_bytes` | Master, buffer comuni, tutto il batch di frame o candidati. |
| `per_task_bytes` | Picco di un task. |
| `worker_count` | Fra 1 e `cpu_count`, numero di CPU in `sched_getaffinity`; fuori intervallo: `invalid_input`, `sched.worker_count`. `sched_getaffinity` fallita o con zero CPU: `io`, `sched.cpu_count`. `std::thread::hardware_concurrency()` non si usa. |
| `queue_capacity` | ≥ 1. |
| `tile_rows` | Altezza del blocco di righe per task. |
| `spill` | `none`, oppure `scratch_dir` e `quota_bytes`. |
| `checkpoint_every` | Numero di task fra due checkpoint; `0` nessun checkpoint. |

Picco = `shared_bytes + worker_count · per_task_bytes`. Il preflight lo prenota per intero.

- Picco > budget: `budget`, `sched.job_exceeds_budget`, con picco e budget nel contesto. Nessun task avviato (T-M2D-05, parte M2 di T-FBK-01).
- Lo scheduler non cambia mai `tile_rows`, `worker_count`, `spill` né il numero di candidati (T-M2D-06).
- Picco ≤ budget ma pool occupato da un altro job: il job attende nella coda FIFO dei job, di capacità `job_queue_capacity` (default 16, configurabile: estensione R3.1). Una sottomissione con `job_queue_capacity` job già in attesa (16 con il default): `budget`, `sched.queue_full`, il job non entra (T-M2D-14).
- Ogni task usa una sotto-prenotazione presa all'avvio, mai in modo incrementale: nessun hold-and-wait.
- Coda dei task bounded: il produttore si blocca quando è piena; nessun task scartato (T-M2D-11).

### Guasto e cancel

- Primo `stop_error` di un worker: registrato; stop interno impostato, visto da `check_cancel` dei task attivi (distinto dal cancel dell'utente); join di tutti i worker; nessuna pubblicazione; job `failed` con quell'errore (T-M2D-07).
- Cancel dell'utente: `cancel_token` controllato a ogni task, a ogni blocco di righe, dopo `finalize` e al confine di pubblicazione (`run_job` riceve il `cancel_token`: richiesto al ritorno del lavoro, nulla si pubblica). Nessuna pubblicazione, temporanei rimossi, job `cancelled`, categoria `cancelled`, `sched.cancelled`, uscita 3 (T-M2D-08).
- Prenotazioni e file di spill rilasciati in ogni caso.

### Checkpoint e resume

`checkpoint_record`: `node_key`, `recipe_digest` (SHA-256 del JCS di `node_spec` più `tile_rows`, `worker_count`, `spill`, `checkpoint_every`), `backend`, indici dei task completati, digest dei risultati parziali. Scritto col protocollo atomico di ADR-04.

A job riuscito il checkpoint lo rimuove il chiamante con `remove_checkpoint`, solo dopo pubblicazione riuscita; dopo stop, cancel o pubblicazione fallita resta. Dimensione massima 64 MiB, applicata da writer e reader (file più grande in lettura incluso): oltre, `limit_exceeded`, `sched.checkpoint_size`, checkpoint precedente invariato.

Resume ammesso solo con `recipe_digest` e `backend` identici: altrimenti `resume_mismatch`, `sched.resume_recipe`. Digest di un parziale diverso: `integrity`, `sched.checkpoint_digest`. Un resume rifiutato non riparte da zero in automatico: una nuova esecuzione è un nuovo job (T-M2D-09).

### Spill

Solo se dichiarato. `scratch_dir` deve esistere, essere una directory non symlink e accettare un file `O_TMPFILE`; altrimenti `io`, `spill.scratch_unusable`. Quota superata: `budget`, `spill.quota_exceeded` (T-M2D-10). Senza spill dichiarato non si scrive mai su disco per mancanza di memoria.

## Test

| ID | Clausola |
|---|---|
| T-M2D-01 | Grammatica manuale: casi validi; `MB`, `GB`, decimali, 0, 101 %, oltre `mem_total` rifiutati. |
| T-M2D-02 | Budget manuale persistito e rivalidato alla riapertura. |
| T-M2D-03 | Automatico su `/proc` e cgroup sintetici: formula, `nullopt` per sorgenti assenti, stop senza `MemAvailable`, con `cgroup_current ≥ cgroup_max` e con file non interpretabili. |
| T-M2D-04 | Prenotazioni concorrenti (property test, 1/2/4 thread): somma mai oltre il budget. |
| T-M2D-05 | Job impossibile rifiutato prima di ogni task. |
| T-M2D-06 | Budget stretto: stop, mai tile o worker ridotti. |
| T-M2D-07 | Eccezione iniettata in un worker: nessuna pubblicazione, prenotazioni rilasciate. |
| T-M2D-08 | Cancel a metà: nessun output, temporanei rimossi, stato `cancelled`, exit 3. |
| T-M2D-09 | Resume identico riprende; ogni differenza di ricetta o backend: `resume_mismatch`. |
| T-M2D-10 | Scratch non scrivibile, symlink, quota superata: stop. |
| T-M2D-11 | Backpressure: produttore bloccato, nessun task perso. |
| T-M2D-12 | Allocazione oltre la prenotazione: stop. |
| T-M2D-13 | Protocollo M1.d completo sul prodotto: pool 32 MiB, tile 2 MiB, 1/2/4 worker, 40 GiB logici, spill fisico; RSS e page cache dello spill misurati. |
| T-M2D-14 | Coda dei job piena: stop `sched.queue_full`. |
| T-M2D-15 | `worker_count` 0 o oltre `cpu_count`, `sched_getaffinity` iniettata con errore o zero CPU: stop. |

## Interfaccia e codici aggiunti in M2.d

API in `src/core/include/astro/`: `budget.hpp`, `memory_pool.hpp`, `spill.hpp`, `checkpoint.hpp`, `scheduler.hpp`, `job_plan.hpp`, `cancel.hpp`. Il budget manuale è in `settings` alla chiave `budget.manual` come `{"spec":"<grammatica>"}`.

- `/proc/self/cgroup` si valida per intero, riga per riga: `<id>:<controller>:<percorso assoluto>`; v1 con id > 0 senza zeri iniziali e controller non vuoti; v2 con id `0`, controller vuoti e al più una riga. Altrimenti `invalid_input`, `budget.cgroup_parse`; assenza di una riga v2 = nessun cgroup.
- Il cgroup considerato è il livello v2, dal processo verso la radice, con meno margine (`limite − uso`) fra quelli con `memory.max` numerico. `cgroup_current ≥ cgroup_max` su uno qualunque dei livelli finiti: `budget.cgroup_over_limit`.
- Il checkpoint di un job sta in `checkpoint_dir/<checkpoint_name>.ckpt.json`; il nome lo sceglie il chiamante. Dopo un arresto o un annullamento il checkpoint resta; a job riuscito si rimuove.
- Il `budget_snapshot` entra nella provenance (`budget`) quando il chiamante lo passa a `run_job`.
- `run_branches` esegue un job per slot in ordine di dipendenza, passando il `cancel_token` a ogni `run_job`; un arresto blocca (`blocked`) i soli slot discendenti, un annullamento lascia `not_started` quelli non partiti.
- Codici non elencati sopra: `pool.exceeds_budget` (richiesta oltre il budget del pool), `spill.not_declared`, `spill.read`, `spill.write`, `spill.short_read`, `spill.offset_overflow`, `sched.queue_capacity`, `sched.tile_rows`, `sched.checkpoint_dir`, `sched.checkpoint_name`, `sched.checkpoint_missing`, `sched.checkpoint_malformed`, `sched.checkpoint_size`, `sched.resume_verifier`, `sched.task_digest`, `budget.cgroup_read`, `budget.setting_malformed`, `store.setting_not_canonical`.
- La quota dello spill conta l'estensione logica massima raggiunta. Dopo una scrittura fallita estensione e quota restano quelle dei byte scritti davvero (estensione R3.2, ENOSPC). La page cache dello spill non è nel budget del pool: `spill_file::resident_bytes()` la misura, `drop_cache()` la restituisce al kernel su richiesta esplicita.
- T-M2D-13 è l'eseguibile `test_m1d_protocol` (ctest label `slow`); gli altri test hanno label `fast`.

## Estensione R3.1: configurazione, pianificatore, monitor, cache su disco

Perimetro dichiarato prima del codice. Questa estensione copre F-RES-04, F-RES-05.a, F-RES-08.a, NFR-PERF-01 e la parte «minimo fattibile» di F-RES-07.a; di F-RES-03 copre il pianificatore con residenza intera dei frame (`frame_residency = whole`). **Non copre**: elaborazione a blocchi di righe oltre il budget (T-M2R3-01), checkpoint e spill collegati al job di prodotto, pressione PSI durante l'esecuzione, ENOSPC (R3.2). Fino a R3.2 un job il cui minimo supera il budget si ferma; non passa ai blocchi.

### Configurazione delle risorse (`resources.hpp`)

`resource_config`, persistita in `settings` alla chiave `resources.config` come JSON canonico, rivalidata a ogni lettura. Classificazione degli errori. In lettura (`load_resource_config`, `parse_resource_config`): una chiave sconosciuta o mancante, un tipo errato, un intero negativo, un intero oltre 32 bit dove il campo è a 32 bit e un numero oltre 2^53 (il parser JSON legge come intero fino a 2^53 incluso e oltre come numero non intero: limite del parser) danno `invalid_input`, `resources.setting_malformed`; un intero di tipo corretto ma fuori dominio (per esempio 0, oppure esattamente 2^53, che il parser legge ancora come intero ma supera il limite configurativo 2^53 − 1) dà `invalid_input`, `resources.<campo>`; `psi_threshold` è un numero reale, quindi un valore fuori da (0, 100], negativo compreso, dà `resources.psi_threshold`. In scrittura (`save_resource_config`, `validate_resource_config`) ogni valore fuori dominio dà `resources.<campo>`, compresi quelli oltre 2^53 − 1 (limite del JSON canonico) e CPU ≥ `CPU_SETSIZE` (1024); `worker_count` e `tile_rows` sono a 32 bit. `save_resource_config` non scrive mai un valore che la lettura rifiuterebbe.

| Campo | Default | Dominio |
|---|---|---|
| `rss_target_bytes` | assente | > 0; obiettivo best effort, mai un limite hard |
| `scratch` (`dir`, `quota_bytes`) | assente | `dir` assoluta, quota > 0 |
| `disk_cache` (`dir`, `quota_bytes`) | assente | `dir` assoluta, quota > 0 |
| `cpu_set` | assente (tutte le CPU dell'affinità) | non vuoto, crescente, senza duplicati |
| `worker_count` | `auto` | `auto` o intero ≥ 1 |
| `tile_rows` | `auto` | `auto` o intero ≥ 1 |
| `job_queue_capacity` | 16 | ≥ 1 |
| `task_queue_capacity` | 4 | ≥ 1 |
| `report_margin_bytes` | 1 MiB | ≥ 1 |
| `fraction_normal` | 3/8 | rapporto `num/den`, 0 < num ≤ den ≤ 1000 |
| `fraction_pressure` | 1/4 | come sopra, ≤ `fraction_normal` |
| `psi_threshold` | 10.0 | finito, 0 < x ≤ 100 |

Ogni parametro di `job_queue_capacity`, `task_queue_capacity`, `report_margin_bytes`, `fraction_normal`, `fraction_pressure`, `psi_threshold` porta una `origin` (testo non vuoto) persistita. Origini dei default: `fraction_normal`, `fraction_pressure`, `psi_threshold`: «default: misurato sul PC di sviluppo, docs/evidence/M2-gate.md» (la soglia 10.0 è dichiarata non confermata come ottimo, vedi sopra); `job_queue_capacity`, `task_queue_capacity`, `report_margin_bytes`: «default dichiarato, non misurato». Un valore impostato dall'utente ha origine «configurato». VRAM: non è un campo configurabile; vale `null` con motivo «nessun backend GPU nel prodotto fino a M7».

`validate_cpu_set(config, affinity)`: una CPU fuori da `sched_getaffinity` porta a `invalid_input`, `sched.cpu_set`. Con `cpu_set`, `worker_count` ≤ numero di CPU dell'insieme (`sched.worker_count`). L'insieme si applica con `pthread_setaffinity_np` ai soli thread worker del job (`scheduler_tuning::cpu_set`); fallimento: `io`, `sched.cpu_set_apply`. Il thread chiamante non cambia affinità.

`budget_tuning` (frazioni e soglia) entra in `compute_automatic_budget`; senza argomento valgono i default. `job_scheduler` riceve `scheduler_tuning { max_queued_jobs, cpu_set }`; senza argomento `max_queued_jobs = job_queue_capacity`.

### Pianificatore (`plan_resources`)

Deterministico, nel preflight; il piano non cambia durante l'esecuzione e i valori espliciti non si toccano. Ingresso: `budget_bytes` B, CPU utilizzabili C (dimensione di `cpu_set`, altrimenti `cpu_count`), larghezza W, altezza H, campioni S = W·H, byte per campione di uscita E, somma dei picchi di lettura dei quattro file P, limite dell'header di uscita HB, margine M (`report_margin_bytes`).

```
out_buffers = S·(2E + 2)
shared      = P + out_buffers + (out_buffers + HB) + 8·S + M
per_task    = 18·W                       (2 double + 1 uint16 per colonna)
tile_rows   = esplicito, oppure min(64, max(1, ceil(H / (4·C))))
T (task)    = ceil(H / tile_rows)
minimum_feasible_bytes = shared + per_task            (un worker)
workers     = esplicito n, oppure min(C, T, floor((B − shared) / per_task))
peak        = shared + workers·per_task
working_set = shared + min(C, T)·per_task
```

Stop `budget`, `sched.job_exceeds_budget` con `minimum_feasible_bytes`, `budget_bytes` e `picco` nel contesto quando: `B < minimum_feasible_bytes` (anche in automatico), oppure `workers` esplicito con `peak > B`. Il piano riporta `frame_residency = whole`, `master_residency = whole`, `budget_below_working_set = (B < working_set)`. I campi esplicitati dall'utente sono elencati in `user_fixed`.

Tabella di riferimento (calcolata a mano; W = 1000, H = 1000, E = 4, P = 4 000 000, HB = 100 000, M = 1 048 576): S = 1 000 000, `out_buffers` = 10 000 000, `shared` = 4 000 000 + 10 000 000 + 10 100 000 + 8 000 000 + 1 048 576 = 33 148 576, `per_task` = 18 000, `minimum_feasible_bytes` = 33 166 576.

| Caso | B | C | esplicito | Atteso |
|---|---|---|---|---|
| a | 104 857 600 | 4 | — | tile 63, T 16, workers 4, `working_set` 33 220 576, sotto working set: no |
| b | 33 220 575 | 16 | — | tile 16, T 63, workers 3, `working_set` 33 436 576, sotto working set: sì |
| c | 33 220 575 | 16 | workers 8 | stop, `peak` 33 292 576, `minimum_feasible_bytes` 33 166 576 |
| d | 104 857 600 | 2 | tile 10 | tile 10, T 100, workers 2 |
| e | 33 166 576 | 16 | — | workers 1 |
| f | 33 166 575 | 16 | — | stop, `minimum_feasible_bytes` 33 166 576 |

### Monitor (`sample_resources`)

Fotografia, a richiesta, di: pool gestito (`budget_bytes`, `reserved_bytes`, `peak_reserved_bytes`, `waiting_count`), picco stimato del piano, RSS (`VmRSS`) e picco RSS (`VmHWM`) del proprio processo da `<proc_root>/self/status` (illeggibile o senza i due campi: `io`, `monitor.status`), cache residente (valore passato dal chiamante; il core non ha ancora una cache residente, quindi il campo riporta quel valore e non è verificato contro voci proprie), scratch (`used_bytes`, `peak_bytes`, `quota_bytes` dello `spill_area`, o `null`), cache su disco (byte misurati con `measure_disk_cache`, quota, o `null`), VRAM (`null` con motivo), limite scelto (`budget_bytes` e modo `automatic`/`manual`). Segnali: `rss_over_target` (solo evento riportato, nessuna azione), `disk_cache_over_quota`, `budget_below_working_set`, `budget_below_minimum` (B < `minimum_feasible_bytes`: distinto dal rifiuto, che è l'errore del preflight). L'RSS non è un gate.

### Cache su disco

`measure_disk_cache(dir)`: somma delle dimensioni dei file regolari direttamente in `dir`, con `O_NOFOLLOW`. Una voce non regolare, un symlink o un file con `st_nlink ≠ 1`: `security`, `cache.entry_unsafe`; directory mancante o symlink: `io`, `cache.dir`. `enforce_disk_cache_quota(dir, quota)`: oltre la quota `budget`, `cache.quota_exceeded`. `remove_disk_cache_entries(dir, names)`: ogni nome `[A-Za-z0-9._-]{1,128}` e diverso da `.` e `..` (altrimenti `invalid_input`, `cache.entry_name`); rimozione con `unlinkat` sul descrittore della directory, solo di file regolari con `st_nlink = 1` (`security`, `cache.entry_unsafe`); voce assente: `metadata_missing`, `cache.entry_missing`. Nessun file fuori da `dir` viene toccato.

### Test R3.1

| ID | Clausola |
|---|---|
| T-M2R3-02 | Pianificatore sui casi a-f della tabella; valori espliciti intatti o stop. |
| T-M2R3-03 | Monitor: pool e picco coincidono con i contatori del test; RSS ≤ `VmHWM` letto dopo; scratch e cache su disco coincidono con le misure del test; VRAM `null` col motivo; i quattro segnali `rss_over_target`, `disk_cache_over_quota`, `budget_below_working_set`, `budget_below_minimum`, ciascuno acceso e spento da un caso; `rss_over_target` solo come evento. |
| T-M2R3-04 | `minimum_feasible_bytes` uguale alla formula; B = minimo passa, minimo − 1 si ferma; il job di calibrazione reale con budget pari al minimo del piano termina e con minimo − 1 si ferma prima di ogni task. |
| T-M2R3-08 | Ogni parametro di tuning cambia il comportamento come da formula (frazioni e soglia nel budget automatico, capacità della coda dei job, capacità della coda dei task, margine nel piano; frazioni e soglia validate anche all'ingresso di `compute_automatic_budget`: `invalid_input`, `budget.tuning`); il `job_plan` passato allo scheduler dal job reale porta capacità della coda dei task, tile, worker e margine configurati (`calibration_job_options::on_job_plan`); default e origine nella provenance del job quando il chiamante passa lo snapshot del budget (senza snapshot nulla viene registrato: limite dichiarato in `calibration.hpp`); persistenza e rivalidazione; domini e limiti di rappresentazione rifiutati con la classificazione sopra, compresi negativi, interi oltre 32 bit e oltre 2^53 − 1. |
| T-M2R3-09 | `cpu_set` applicato ai soli worker (affinità letta dentro il task, thread chiamante invariato); CPU fuori affinità: stop; fallimento reale di `pthread_setaffinity_np` (CPU inesistente dichiarata dall'affinità iniettata): `io`, `sched.cpu_set_apply` con l'errno nel contesto, nessun task e nessun `finalize`, pool rilasciato; quota e cleanup della cache su disco confinati (symlink, hardlink, nome con `/`); configurazione persistita e rivalidata alla riapertura. |

## Estensione R3.2: blocchi di righe, scratch, checkpoint nel job, ENOSPC, pressione

Perimetro dichiarato prima del codice. Copre F-RES-03 (elaborazione a blocchi oltre il budget), F-RES-06.a (pressione durante l'esecuzione, checkpoint e resume nel job, stop con checkpoint; confine `.a`/`.b` di D11), F-RES-07.a (minimo fattibile dei blocchi, ENOSPC) e C-10.a per il disco pieno su scratch e checkpoint. Non copre: ENOSPC sulla cache delle miniature; SIMD (R5); master di calibrazione (R2). L'RSS si misura e si riporta, non è un gate.

### Residenza (pianificatore)

`resource_plan.frame_residency` e `master_residency` valgono entrambi `whole` o entrambi `blocks`. Il pianificatore sceglie `blocks` se è dichiarato un checkpoint oppure `B < W_min`, con `W_min` il `minimum_feasible_bytes` della residenza intera (estensione R3.1, formule invariate); altrimenti `whole`, con il piano di R3.1 invariato. La scelta avviene nel preflight, entra nella ricetta del job (`job_plan.spill` presente se e solo se `blocks`, quindi nel `recipe_digest`) e non cambia durante l'esecuzione. Il risultato non dipende dalla residenza (sezione «Equivalenza»).

- `blocks` richiede `scratch` nella configurazione. Senza `scratch`: con checkpoint dichiarato, `invalid_input`, `calib.checkpoint_scratch`; altrimenti lo stop di R3.1 (`budget`, `sched.job_exceeds_budget`, `minimum_feasible_bytes` = `W_min`), con `residenza_blocchi` = «scratch non dichiarato» nel contesto.
- Byte di staging `Z = S·(2E + 2)` oltre `scratch.quota_bytes`: `budget`, `spill.quota_exceeded` nel preflight, prima della prenotazione.
- `tile_rows` e `worker_count` espliciti restano intatti; il `tile_rows` automatico è quello di R3.1.

Simboli per ingresso `k` (light, dark, flat, dark_flat): `b_k` = |BITPIX| / 8 del primario, `E_k` = byte del tipo decodificato (4 per f32, 8 per f64), `I_k` = 1 se c'è l'HDU INVALID, `V_k` = 1 se la sorgente di varianza è `plane` (HDU VARIANCE letto); `t` = `tile_rows`, `T = ceil(H / t)`, `Cn` = canali di normalizzazione (1 o 4), `HB` = limite dell'header di uscita di R3.1, `M` = `report_margin_bytes`.

```
R_raw   = Σ_k W·(b_k + 2·I_k + E_k·V_k)         byte grezzi per riga dei quattro ingressi
R_band  = Σ_k W·(E_k + 2 + E_k·V_k)             byte decodificati per riga (campioni, maschera, varianza)
G       = Σ_k (1 + I_k + V_k)                   data unit letti
in_band = t·(R_raw + R_band)
per_task_b = in_band + t·W·(2E + 2) + 18·W
TPL     = 2^20 + 2880 + 2·(HB + 3·2880·ceil(8·W / 2880)) + 18·W
shared_b   = in_band + 32·T·G + Cn·4·256·8 + TPL + M
B_min   = shared_b + per_task_b                 (minimum_feasible_bytes con residenza blocks)
workers = esplicito, oppure min(C, T, floor((B − shared_b) / per_task_b))
```

`B < B_min`: `budget`, `sched.job_exceeds_budget`, con `minimum_feasible_bytes` = `B_min`, `budget_bytes` e `residenza` = `blocks`. Worker esplicito con picco > B: stesso stop. `in_band` ospita le passate del prepare e i buffer del finalize; `32·T·G` sono i digest dei blocchi; `Cn·4·256·8` gli istogrammi della selezione; `TPL` maggiora la codifica del modello di header (sotto). `TPL` è un limite ricavato leggendo `encode_fits` (memfile CFITSIO con incremento di 1 MiB, vettore di uscita, immagine di una riga), non misurato. Il pool impone solo i `pool_buffer`: i buffer dei blocchi, delle passate e del kernel sono `pool_buffer` dentro le prenotazioni; `TPL`, i digest, gli istogrammi e il report sono allocazioni contabilizzate nella prenotazione ma non imposte, come le immagini della residenza intera.

Tabella di riferimento (calcolata a mano; ingressi del pianificatore dati come numeri, come in R3.1): W = 1000, H = 1000, S = 10^6, E = 4, C = 4, HB = 2 417 280, M = 1 048 576, P = 50 000 000, quattro ingressi f32 (`b_k` = `E_k` = 4) senza INVALID e con `V_k` = 1, `Cn` = 1, `scratch` dichiarato con quota 10 000 000 salvo dove indicato.

- Residenza intera (R3.1): `W_min` = 50 000 000 + 10 000 000 + (10 000 000 + 2 417 280) + 8 000 000 + 1 048 576 + 18 000 = 81 483 856.
- Blocchi: t = min(64, max(1, ceil(1000 / 16))) = 63, T = 16; `R_raw` = 4·1000·8 = 32 000; `R_band` = 4·1000·10 = 40 000; `G` = 8; `in_band` = 63·72 000 = 4 536 000; `per_task_b` = 4 536 000 + 63·1000·10 + 18 000 = 5 184 000; digest 32·16·8 = 4 096; istogrammi 4·256·8 = 8 192; `TPL` = 1 048 576 + 2 880 + 2·(2 417 280 + 3·2880·3) + 18 000 = 5 955 856; `shared_b` = 4 536 000 + 4 096 + 8 192 + 5 955 856 + 1 048 576 = 11 552 720; `B_min` = 16 736 720; `Z` = 10^6·10 = 10 000 000.

| Caso | B | Variante | Atteso |
|---|---|---|---|
| g | 81 483 856 | — | `whole`, piano di R3.1 |
| h | 81 483 855 | — | `blocks`, t 63, T 16, workers min(4, 16, floor(69 931 135 / 5 184 000) = 13) = 4 |
| i | 16 736 720 | — | `blocks`, workers 1 |
| j | 16 736 719 | — | stop, `minimum_feasible_bytes` 16 736 720 |
| k | 81 483 856 | checkpoint dichiarato | `blocks`, workers 4 |
| l | 81 483 855 | senza `scratch` | stop, `minimum_feasible_bytes` 81 483 856 |
| m | 16 736 720 | quota 9 999 999 | stop `spill.quota_exceeded` |
| n | 16 736 720 | workers 2 espliciti | stop, picco 11 552 720 + 2·5 184 000 = 21 920 720 |
| o | 16 736 720 | checkpoint senza `scratch` | stop `calib.checkpoint_scratch` |

### Lettura a blocchi degli ingressi (`fits_blocks.hpp`)

Solo HDU non compressi (D8; M2 non legge HDU compressi). Per ogni ingresso, nel `prepare` e nell'ordine light, dark, flat, dark_flat:

1. Apertura e analisi degli header con `pread` (stessi limiti e stessi errori di `plan_fits`); i blocchi di header letti sono sottoposti a SHA-256 nell'ordine di lettura.
2. Passata P0: l'intero file letto in ordine, una volta. SHA-256 e dimensione uguali alla ricetta, altrimenti `integrity`, `input.digest_mismatch` (contesto come `read_verified_fits`). SHA-256 di ogni regione di header uguale a quello dei byte analizzati, altrimenti `integrity`, `input.modified`. CHECKSUM e DATASUM, se presenti, verificati con la somma in complemento a uno a 32 bit della convenzione FITS Checksum, con l'accettazione di `fits_verify_chksum`: DATASUM uguale alla somma dei dati, CHECKSUM valido se la somma dell'HDU vale 0 o 0xFFFFFFFF; altrimenti `integrity`, `fits.checksum`, con `hdu`. Ogni campione è validato come in `decode_fits`, con gli stessi codici e lo stesso contesto: `fits.scaling_not_exact` sul primario e `fits.invalid_reserved_bits` su INVALID con l'`indice` globale, `fits.int64_range` sul primario senza contesto. Precedenza degli arresti alla fine di P0, la stessa di `read_verified_fits` più `decode_fits`: digest, header, checksum (primo HDU), primo errore di campione nell'ordine del file. Per ogni data unit letto si registra lo SHA-256 dei byte di ogni blocco di `t` righe.
3. Lettura di un blocco (passate e task): `pread` dei byte del blocco di ciascun data unit letto, SHA-256 uguale a quello registrato in P0, altrimenti `integrity`, `input.consumed_digest`, con `input`, `blocco` e `regione`. Ogni byte che entra nel calcolo è quindi verificato prima dell'uso (D8: digest sui byte consumati, confrontato prima della pubblicazione). Decodifica identica a `decode_fits`: interi `r·BSCALE + BZERO` in doppia precisione convertiti al tipo decodificato; float copiati bit per bit; maschera `raw ^ 0x8000`; campione non finito con maschera nulla marcato `source_nonfinite`.
4. Prima della pubblicazione: il percorso indica ancora lo stesso `(st_dev, st_ino)` e `check_unchanged` passa. File rimosso: `io`, `input.missing`; percorso che indica un altro file, oppure mtime o ctime cambiati: `integrity`, `input.modified`; dimensione cambiata: `integrity`, `input.size_changed`.

L'immagine di soli header (geometria, tipo, layout, CFA, unità, linearità, fase, metadati, card) è quella di `decode_fits` senza campioni.

### Calibrazione a blocchi

- Stessa ricetta, stesso nodo, stessi controlli. I controlli sui metadati sono lo stesso codice della residenza intera, eseguito sulle immagini di soli header. Il controllo dei piani di varianza (`var.plane_values`) scorre i blocchi di ogni ingresso nell'ordine light, dark, flat, dark_flat: stesso primo indice. Limite dichiarato: con più difetti in ingressi diversi l'ordine può differire fra le residenze solo fra `var.plane_values` di un ingresso e un errore dei metadati di varianza di un ingresso successivo (la residenza intera controlla i valori di ciascun ingresso subito dopo i suoi metadati, quella a blocchi dopo i metadati di tutti); in entrambi i casi il job si ferma senza uscita. `calib.nonfinite_unflagged` non può scattare in nessuna delle due residenze sui file decodificati, perché la decodifica marca ogni non finito.
- Normalizzazione: conteggi, `fc_nonpositive` e media e varianza di Welford nella prima passata sui blocchi di flat e dark_flat, in ordine raster: stesse operazioni nello stesso ordine della residenza intera. Mediane di `Fc` e dei termini di varianza per canale con selezione esatta: chiave monotona a 64 bit del double, cifre di 8 bit dalla più significativa, 8 passate; valore k-esimo di `<` sui double finiti, uguale a quello di `median_of`. `+0` e `−0` sono equivalenti per `<`: in entrambe le residenze una mediana nulla vale `+0`. Stessi arresti nello stesso ordine (`calib.normalization_invalid`, `calib.variance_overflow`).
- Kernel: lo stesso `rows_kernel` della residenza intera, su viste dei blocchi con origine di riga.
- Uscita: ogni task scrive nello staging (sotto) i byte FITS big-endian delle proprie righe: campioni, maschera `bits ^ 0x8000`, varianza. Il finalize conta i bit della maschera dallo staging (bit riservati: `invalid_input`, `image.mask_reserved_bits`, come `encode_fits`), compone il report con lo stesso codice e scrive il FITS in streaming: header presi da `encode_fits` di un modello di una riga con gli stessi metadati e la stessa presenza di INVALID, con `NAXIS2` = H; dati dallo staging; riempimento a zero fino al blocco di 2880 byte. L'header composto è riletto con lo stesso parser e confrontato come in `encode_fits`. Lo staging contiene campioni in virgola mobile del tipo dell'uscita: il writer ammette solo l'encoding identità di quel tipo, o nessuno (l'uscita della calibrazione non ne ha); altrimenti `invalid_input`, `fits.encoding`.
- Pubblicazione: lo store accetta un contenuto in streaming (`artifact_content::source`) con digest atteso calcolato da una passata precedente sullo stesso flusso; protocollo atomico e regola hardlink invariati. Senza digest atteso: `invalid_input`, `store.stream_digest`, nessuna scrittura; digest o dimensione diversi: `integrity`, `store.expected_digest_mismatch`; errore della sorgente: propagato invariato. In ogni caso nessun file pubblicato e temporaneo rimosso.

### Equivalenza

I tre artefatti pubblicati (immagine, report JSON, report testuale) sono byte per byte uguali fra residenza `whole` e `blocks`, per ogni `tile_rows` e numero di worker. Argomento: ogni operazione aritmetica è lo stesso codice nello stesso ordine; le parti diverse sono la lettura dei campioni, la selezione delle mediane e la scrittura del FITS, ciascuna confrontata con `decode_fits`, `median_of` ed `encode_fits` nei test. È una prova sulle fixture dei test, non una dimostrazione per ogni ingresso.

### Scratch e checkpoint del job

- Con `blocks` il `job_plan` dichiara `spill = {scratch.dir, scratch.quota_bytes}`. Lo staging (`Z` byte) è un file dell'area di spill: anonimo (`O_TMPFILE`) senza checkpoint; con checkpoint, il file `<nome>.stage` nella directory di scratch, creato o troncato all'avvio e riaperto al resume, con `O_NOFOLLOW`, regolare e `st_nlink = 1` (altrimenti `security`, `spill.staging_unsafe`; assente al resume: `metadata_missing`, `sched.checkpoint_missing`). La quota conta l'estensione dello staging; al resume si addebita l'estensione esistente.
- `calibration_job_options::checkpoint {name, every, resume}`: checkpoint ogni `every` task in `scratch.dir/<nome>.ckpt.json`. Resume: stessi controlli di M2.d (`sched.resume_recipe`, `sched.checkpoint_missing`); il digest parziale di un task è lo SHA-256 dei suoi byte nello staging, riletti e riverificati al resume (`integrity`, `sched.checkpoint_digest`). La verifica dei parziali avviene una volta, dopo `prepare` (che apre lo staging) e prima del primo task.
- A job pubblicato, `run_calibration_job` rimuove checkpoint e staging; dopo stop, annullamento o uccisione del processo restano entrambi. Un'esecuzione nuova con lo stesso nome li sostituisce. Dopo un'uccisione la recovery dello store marca il job `interrupted` (R4); il resume è un job nuovo sullo stesso slot.

### ENOSPC (D13)

Iniezione nelle chiamate di scrittura del prodotto: `spill_area::set_fault_injector` (punto `spill.write`, prima di ogni `pwrite`), `write_checkpoint` con iniettore (punti `checkpoint.<passo>` del protocollo atomico, passato da `job_spec::fault`), iniettore dello store per gli artefatti (esistente). Esito: `io` con `errno` nel contesto, nessuna pubblicazione (solo la provenance dell'arresto), temporanei rimossi (staging anonimo, temporaneo del protocollo atomico), quota rilasciata. Una scrittura che estende il file è esclusiva fino all'esito; se fallisce, l'estensione dichiarata e la quota restano quelle dei byte scritti davvero, anche dopo una scrittura corta, e una scrittura concorrente nel tratto incerto attende l'esito e conserva i suoi byte: il limite di M2.d («un `pwrite` fallito lascia addebitata l'estensione») è chiuso. Con checkpoint, lo staging nominato resta come stato del resume. Un guasto prima del `rename` del protocollo atomico (`checkpoint.create`, `.write`, `.fsync_file`, `.rename`) lascia il checkpoint precedente; un guasto dopo il `rename` riuscito (`checkpoint.fsync_dir`) lascia quello nuovo, già completo. In entrambi i casi il checkpoint presente elenca solo task completati, con i loro byte già scritti nello staging, e il resume identico riprende da lì. Limite del modello: è un arresto del processo, non una perdita di energia (il `rename` non sincronizzato non è trattato qui).

### Pressione durante l'esecuzione (D11, D12)

`pressure_gate` (`scheduler.hpp`): sorgente PSI (`some avg10` da `<proc_root>/pressure/memory`, `nullopt` se il file manca; iniettabile), soglia `psi_threshold` (10.0, soglia inclusa come nel budget) e intervallo di campionamento `pressure_poll_ms` (nuovo parametro di `resource_config`: default 1000, origine «default dichiarato, non misurato», dominio 1…2^53 − 1). Lo scheduler campiona prima della prenotazione del pool e di nuovo appena la prenotazione è concessa (ammissione effettiva): se la pressione è salita durante l'attesa della memoria, la prenotazione si restituisce e l'attesa riprende. In M2 è l'unica prenotazione del pool di un job: il job prenota l'intero picco all'ammissione e i task la suddividono (`carve`). Sopra soglia l'ammissione attende senza timeout, ricampionando a ogni intervallo; l'attesa finisce quando la pressione scende sotto soglia oppure con un annullamento (`cancelled`, `sched.cancelled`, nulla prenotato), controllato almeno ogni 10 ms anche dentro un intervallo lungo (granularità del controllo, non un timeout). Errore della sorgente: `invalid_input`, `budget.psi_parse`. I job già ammessi proseguono con il loro piano. `pressure_gate::waiting_jobs` conta i job in attesa; il monitor lo espone (`pressure`: `waiting_jobs`, `threshold`). Il report del job conserva la prima lettura e, se diversa, quella dell'ammissione (`job_report::admission`, voci `{psi, admitted}`), e conta le letture sopra soglia (`pressure_wait_samples`): la memoria non cresce con la durata dell'attesa. Per la calibrazione le stesse informazioni entrano nei `details` della provenance (`admission`: `samples`, `pressure_wait_samples`). Una prenotazione impossibile durante l'esecuzione (sotto-prenotazione oltre il residuo: `budget`, `pool.reservation_exceeded`) ferma il job; l'ultimo checkpoint resta e il resume con ricetta identica riprende (cancel sicuro). Nessuno spill né cambio di piano a caldo: quello è `.b` (M6).

### Test R3.2

| ID | Clausola |
|---|---|
| T-M2R3-01 | Lettore a blocchi uguale bit per bit a `decode_fits` (BITPIX 8, 16, 32, 64, −32, −64, scale, NaN, INVALID, VARIANCE, `t` = 1, 3, H) e stessi arresti con stesso contesto; digest dei byte consumati (file alterato fra P0 e blocco: `input.consumed_digest`; P0: `input.digest_mismatch`; CHECKSUM e DATASUM validi accettati e alterati rifiutati come `decode_fits`); FITS in streaming uguale byte per byte a `encode_fits`; selezione uguale a `median_of`; pianificatore sui casi g-l; job reale con working set ≥ 8 volte un budget manuale: residenza `blocks`, tre artefatti uguali byte per byte a quelli del budget ampio, picco del pool ≤ budget, uscita entro le tolleranze di T-M2E-02 rispetto all'oracolo su un sottoinsieme di pixel, RSS riportato; CFA con mediane per canale, piani di varianza, INVALID e saturazione uguali fra residenze per più `tile_rows` e worker; stessi arresti fra residenze (`var.plane_values`, `calib.normalization_invalid`); `B_min` passa e `B_min − 1` si ferma; staging oltre quota e assenza di `scratch` fermano il preflight. |
| T-M2R3-05 | ENOSPC su spill, su ogni passo del checkpoint (precedente o nuovo, mai parziale, resume che completa) e sull'artefatto in streaming nel job a blocchi, e sugli artefatti nel job a residenza intera: `io` con errno 28, nessuna pubblicazione, temporanei rimossi, quota rilasciata; quota restituita dopo un `pwrite` fallito che aveva esteso il file, scrittura corta reale (RLIMIT_FSIZE) con estensione e quota dei byte scritti, scrittura concorrente nel tratto incerto conservata; contenuto in streaming senza digest, con digest diverso o con sorgente che fallisce: arresto, nessun file. |
| T-M2R3-06 | Processo ucciso dopo un checkpoint, resume identico: artefatti uguali all'esecuzione continua, task ripresi > 0; resume con worker diversi: `resume_mismatch`; staging alterato su un task completato: `sched.checkpoint_digest`; checkpoint e staging rimossi dopo la pubblicazione e conservati dopo stop e annullamento; checkpoint senza scratch: `calib.checkpoint_scratch`. |
| T-M2R3-07 | Con sorgente PSI sintetica: il job in coda non è ammesso sopra soglia (soglia inclusa) e lo è quando la pressione cala; pressione salita durante l'attesa della memoria ricampionata all'ammissione effettiva; annullamento che interrompe un intervallo di 30 s; il job in corso conserva piano e `recipe_digest`; letture nel report e nella provenance; monitor con i job in attesa; annullamento durante l'attesa; sorgente non interpretabile; prenotazione impossibile durante l'esecuzione: stop con checkpoint e resume che completa; `pressure_poll_ms` persistito con origine e validato. |

## Fuori da M2

Budget del viewer; eviction della cache; batch di ricerca dell'Advisor (ADR-06.b); prevenzione deadlock fra job diversi oltre la coda FIFO; GPU e VRAM.
