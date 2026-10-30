# M2-R2r: piano del job del master dalla configurazione delle risorse

Stato: **ESEGUITO, review PASS al giro 2** (`.omc/artifacts/ask/codex-r2r-round2-20261030-0154.md`); commit e push al coordinatore. Unità R2.r di `plans/M2-residui.md` (riga 101), separata da R2 per arbitraggio del coordinatore (`.omc/artifacts/ask/codex-r2-round3-20261029-2240.md`). ID del registro: F-CAL-01.a (piano del job del master), F-RES-03. Contratti: [ADR-06](../contracts/ADR-06-budget-scheduler.md), sezione «Estensione R2.r»; [C-02.a](../contracts/C-02-master.md), voce «Piano del job (R2.r)». Autore: agente Claude Opus 5.5.

## Ambiente

Linux 7.0.0-38-generic x86_64, 16 CPU, 30 GiB; GCC 15.2.0; CMake 4.2.3; Ninja 1.13.2; Python 3.14.4. Base `fe6312f` più modifiche non committate, worktree isolato. Build e test con `TMPDIR=<worktree>/tmp`, `CMAKE_BUILD_PARALLEL_LEVEL=4`, `CTEST_PARALLEL_LEVEL=4` (`ctest -j4` in release), `ulimit -c 0`. Zero warning (`-Werror`), `tools/lint.sh` ok. `data/` del worktree è un collegamento simbolico al `data/` del repository principale (non versionato), per i test lenti.

## Stato generato

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-06-budget-scheduler.md` | `4c24c69f4559e3ddc9db4de78358c58326790438f350e1b286f8e2988f53b44e` |
| `docs/contracts/C-02-master.md` | `c34b078781862e905ddd53734909d34113f893367a92ad8bf9f37a514336876d` |
| `src/core/include/astro/master.hpp` | `f6214e982869003b763d78c8098bccbaed28c527ef6197fad68907f83f42815f` |
| `src/core/include/astro/resources.hpp` | `74b04a71563aa738636ae48a224c634b063058fe0d6269a80034329e4ecfc030` |
| `src/core/master.cpp` | `4bd0c1c467fbbf0e8e9b20c4c89f8edeea103c7d57ca6de40297e04bb8fa30f4` |
| `src/core/resources.cpp` | `b4cec8995f8457d770c2ce47ff6afc1c282c804b5709c291973473c26b3006de` |
| `tests/trace/M2.tsv` | `f559fc60e9888c586cc4917d095f222ffd8fc5d25b0196a67becfa891025bdcc` |
| `tests/unit/test_calibration_real.cpp` | `4d6dbaf66b1b6f70d88516acb1a425e7c23917eccc06c4c39d8a1e71d25434c5` |
| `tests/unit/test_master.cpp` | `ead7cd1bcbf9c7063a86b6bff2749d88d5a2444c9a507869af797484d04a1650` |
| `tests/unit/test_resources.cpp` | `6fc874ef4942a393e2cac3e8331174f9b2eb2aa6334b8844dc02be7b03108cab` |
| `tests/unit/test_simd.cpp` | `4f9255394776ce59d6832eeda7971d6fca44f06953977a5f9c15654f39a00536` |

- Test release (`tmp/release-junit.xml`): totali 515, fast 508, slow 7, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 516, fast 509, slow 7, falliti 0
- Tracciabilità (`tests/trace/M2.tsv`, build `build/release`, junit `tmp/release-junit.xml`): trace: 63 ID nello stadio M2, 0 non coperti
<!-- evidence:end -->

## Comandi

- Dev: `CMAKE_BUILD_PARALLEL_LEVEL=4 CTEST_PARALLEL_LEVEL=4 tools/check.sh` (log `tmp/r2r-check.log`).
- Release: `tmp/r2r-release.sh` (build release, `ctest --preset release -j4 --output-junit tmp/release-junit.xml`, slow compresi; log `tmp/r2r-release.log`).
- Tracciabilità: `python3 -S tools/trace.py check --stage M2 --map tests/trace/M2.tsv --ctest-junit tmp/release-junit.xml --build build/release` (uscita in `tmp/r2r-trace.txt`).
- Mutazioni: `tmp/r2r-mutate-all.sh` (una chiamata di `tools/mutate.sh` per patch di `tmp/r2r-mut/`, generate da `tmp/r2r-mkpatches.py`), riepilogo `tmp/r2r-mutations.log`.
- Rosso prima del codice: `tmp/r2r-red.log` (intestazioni già estese, job ancora sulle opzioni esplicite, pianificatore senza il modello del master).

## Cosa fa il codice

- `src/core/resources.cpp`, `astro/resources.hpp`: `planner_input.frame_count`; con `frame_count` > 0 `plan_resources` e `compute_plan_bounds` usano il modello del master di ADR-06 (estensione R2.r): `shared`, `task(t)` = `master_task_bytes`, tile automatico ridotto dal budget prima dei worker, minimo con un worker e una riga, stop `budget sched.job_exceeds_budget` con `residenza = master` e `non rappresentabile` per ogni grandezza oltre 2^64 − 1; checkpoint, campi della residenza a blocchi e `out_sample_bytes` > 16 rifiutati.
- `src/core/master.cpp`, `astro/master.hpp`: `master_job_options.resources` sostituisce worker, tile e coda espliciti; il job costruisce l'ingresso del pianificatore (picco del frame più grande, limite dell'header, numero di frame, CPU da `cpu_set` o affinità), passa allo scheduler il piano scelto e `scheduler_tuning_of(resources)`, registra `details.resource_plan` e, con lo snapshot del budget, `budget.resources`. Costanti `master_digest_chunk_bytes` e `master_stats_bytes_per_pixel` condivise da pianificatore e job.

## Test eseguiti

| ID | Test | Cosa verifica |
|---|---|---|
| T-M2R2R-01 | `Resources.T_M2R2R_01_*` | Casi p-z della tabella di ADR-06 calcolata a mano (tile, task, worker, picco, `shared`, `per_task`, sotto working set, residenze, `user_fixed`); stop con `picco`, minimo, budget e residenza; piano non rappresentabile (13 106 frame, larghezza 65 536, tile 2^32 − 1; 2^62 frame con tile automatico) con `non rappresentabile` nel contesto, anche da `compute_plan_bounds`; limiti del modello coerenti col piano; margine configurato; checkpoint, ciascuno dei quattro campi della residenza a blocchi e `out_sample_bytes` 17 rifiutati; 2^62 CPU sintetiche danno `t0` = 1 senza divisione per zero (calibrazione e master); `frame_count` = 0 lascia il modello della calibrazione |
| T-M2R2R-02 | `MasterJob.T_M2R2R_02_*` | 16 frame 384×320 misti: budget ampio (tile e worker automatici > 1), budget pari al minimo (tile 1, un worker), `cpu_set` di una CPU (un worker, e l'affinità letta in ogni task del master, nel thread del worker, è quella sola CPU); `job_plan` dello scheduler uguale a `plan_resources` sugli ingressi ricavati dal test; picco del pool ≤ budget; master bit-identico al calcolo in memoria, stesso digest nei tre casi |
| T-M2R2R-03 | `MasterJob.T_M2R2R_03_*` | `task_queue_capacity` 7 (default 4) e margine 2 MiB nel `job_plan`; `shared_bytes` cresce di 1 MiB rispetto al default; capacità nella provenance |
| T-M2R2R-04 | `MasterJob.T_M2R2R_04_*` | Budget = minimo − 1 in automatico, due worker espliciti al minimo, tile esplicito 2^32 − 1: `budget sched.job_exceeds_budget` con il minimo atteso e `residenza = master`, nessun `job_plan`, picco del pool 0, nessun master, slot `missing` |
| T-M2R2R-05 | `MasterJob.T_M2R2R_05_*` | Con lo snapshot: `budget.resources` uguale a `resource_config_json` (origini configurate e di default) e `details.resource_plan` uguale al piano; senza snapshot nessun `budget`, piano presente |

Adattati senza cambiare ciò che verificano: `MasterJob.T_M2R2_07_*` (opzioni da `resources`, minimo letto da `minimum_feasible_bytes`), le opzioni del master in `test_calibration_real.cpp` e `test_simd.cpp`. Tutti gli altri test del preset si rieseguono invariati in dev e release.

## Mutazioni

17 mutazioni sui comportamenti nuovi, tutte RILEVATE (preset release, `tmp/r2r-mutations.log`; patch e log in `tmp/r2r-mut/` per m01-m13 e in `tmp/r2r-mut2/` per m14-m17, aggiunte dopo il giro 1).

| ID | Punto mutato | Test che la rileva |
|---|---|---|
| m01 | `plan_resources` non passa al modello del master | `T_M2R2R_01` |
| m02 | tile automatico non ridotto dal budget | `T_M2R2R_01` |
| m03 | tile automatico calcolato per un worker anche con worker espliciti | `T_M2R2R_01` |
| m04 | `non rappresentabile` sostituito da `0` nel contesto | `T_M2R2R_01` |
| m05 | checkpoint accettato col modello del master | `T_M2R2R_01` |
| m06 | limite di `out_sample_bytes` a 17 | `T_M2R2R_01` |
| m07 | margine fisso a 1 MiB invece di quello configurato | `T_M2R2R_01`, `T_M2R2R_03` |
| m08 | `budget_below_working_set` sempre falso | `T_M2R2R_01` |
| m09 | job con un worker invece del piano | `T_M2R2R_02` |
| m10 | CPU del job dall'affinità anche con `cpu_set` | `T_M2R2R_02` |
| m11 | coda dei task fissa a 4 | `T_M2R2R_03` |
| m12 | configurazione non registrata nella provenance | `T_M2R2R_05` |
| m13 | piano non registrato nella provenance | `T_M2R2R_03`, `T_M2R2R_05` |
| m14 | scheduler del master senza `scheduler_tuning_of(resources)` | `T_M2R2R_02` |
| m15 | `block_band_bytes_per_row` accettato col modello del master | `T_M2R2R_01` |
| m16 | `block_regions` accettato col modello del master | `T_M2R2R_01` |
| m17 | `normalization_channels` ≠ 1 accettato col modello del master | `T_M2R2R_01` |

## Decisioni dove il piano taceva

- Il tile automatico del master si riduce col budget prima dei worker: è il solo modo di avere come minimo un worker e una riga, il minimo già dichiarato da C-02.a in R2. Il `t0` resta quello di R3.1.
- Un solo ingresso: `plan_resources` sceglie il modello da `frame_count`, così il monitor (`compute_plan_bounds`) riceve le grandezze del master senza un'altra funzione.
- Nessuna opzione esplicita fuori dalla `resource_config`: worker e tile espliciti sono quelli della configurazione, come per la calibrazione con `resources`.
- Il piano non rappresentabile è uno stop `budget`, come in R2, e non `invalid_input` come nel modello della calibrazione: è un piano oltre ogni budget.

## Limiti

- Il job del master non ha checkpoint, spill né porta della pressione PSI: un minimo oltre il budget si ferma (ADR-06, estensione R2.r).
- La configurazione entra nella provenance solo con lo snapshot del budget; senza snapshot non viene registrata (come per la calibrazione).
- Un `working_set` non rappresentabile ferma anche un piano che entrerebbe con meno worker; serve `C·task(t0)` oltre 2^64 (limite dichiarato nel contratto).
- `job_queue_capacity` arriva allo scheduler del job tramite `scheduler_tuning_of`; ogni job del master ha il proprio scheduler, quindi la coda dei job non è esercitata qui (è provata in T-M2R3-08).
- La guardia su `task(tile)` non rappresentabile in `plan_master` non è raggiungibile (`tile ≤ t0`) e non ha un test.

## Review

| Giro | Artefatto | Verdetto | Rilievi |
|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-r2r-round1-20261030-0143.md` | FAIL | MED: affinità di `cpu_set` non osservata nei worker del master; MED: tre guardie della residenza a blocchi senza test; LOW: `4·cpus` non controllato |
| 2 | `.omc/artifacts/ask/codex-r2r-round2-20261030-0154.md` | PASS | nessuno |

Correzioni del giro 2 (delta `tmp/r2r-round2.diff`): affinità letta nel task del master (seam `cpu.task_start`) in T-M2R2R-02, mutazione m14; casi per `block_band_bytes_per_row`, `block_regions`, `normalization_channels`, mutazioni m15-m17; `t0` come `ceil(ceil(H / C) / 4)` nei due modelli, con un caso a 2^62 CPU. Fatti: `tmp/r2r-facts-round1.md`, `tmp/r2r-facts-round2.md`.
