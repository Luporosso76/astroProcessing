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
- La quota dello spill conta l'estensione logica massima raggiunta. Limite: un `pwrite` fallito lascia addebitata l'estensione fino alla distruzione dell'area (il job è comunque fermo). La page cache dello spill non è nel budget del pool: `spill_file::resident_bytes()` la misura, `drop_cache()` la restituisce al kernel su richiesta esplicita.
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

## Fuori da M2

Budget del viewer; eviction della cache; batch di ricerca dell'Advisor (ADR-06.b); prevenzione deadlock fra job diversi oltre la coda FIFO; GPU e VRAM.
