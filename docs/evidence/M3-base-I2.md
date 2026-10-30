# M3, base comune, intervento I2: OB-07 e OB-08 nello scheduler

Stato: **CHIUSO, review PASS al giro 1**. Unità: M3, base comune, seconda unità di codice dei debiti del §4 di `docs/contracts/BASE-confini.md`. Suite: `tests/unit/test_conformance.cpp` (non modificata); rossi di partenza: i 21 lasciati da I1 (`docs/evidence/M3-base-I1.md`).

| Campo | Valore |
|---|---|
| Unità | M3 / base comune, intervento I2 (codice) |
| Scopo | Portare OB-07 (cancel nel `prepare`, `job_context::check_cancel`) e OB-08 (piano dichiarato e picco prenotato dallo scheduler per `io.import`, `io.thumbnail`, `cache.rebuild`) nel proprietario |
| Comando | Prima: `cmake --preset release`, `cmake --build --preset release`, `ctest --preset release -R "Conformance\|M3BASE" --output-junit tmp/basei2-before-junit.xml` (4 job, log `tmp/basei2-before.log`). Dopo: stessa build; `ctest --preset release -R "Conformance\|M3BASE\|Blocks\|blocks"` (log `tmp/basei2-after2.log`); `ctest --preset release --output-junit tmp/release-junit.xml` (4 job, log `tmp/release.log`); `tools/check.sh` con `CMAKE_BUILD_PARALLEL_LEVEL=4` e `CTEST_PARALLEL_LEVEL=4` (log `tmp/basei2-check.log`, junit `tmp/dev-junit.xml`); `tools/lint.sh`; `python3 -S tools/trace.py check --stage M2 --map tests/trace/M2.tsv --build-dir build/release --ctest-junit tmp/release-junit.xml`; `tmp/basei2-mut/run.sh` (`tools/mutate.sh` per ogni patch di `tmp/basei2-mut/`); `python3 -S tools/evidence.py docs/evidence/M3-base-I2.md` |
| Input | Codice a `8c72338` più le modifiche elencate sotto; fixture sintetiche di `tests/fixtures/conformance_inputs.hpp` |
| Versioni | GCC 15.2.0, CMake 4.2.3; preset `release` e `dev` (ASan e UBSan) |
| Ambiente | Linux 7.0.0-38-generic x86_64, 16 CPU, worktree isolato, 4 job |
| Risultato | Release completo: 604 test, 7 falliti, tutti della suite di conformità (T-03, T-05 per tre parametri, T-11 per calibrazione `intera`, `blocchi` e master). Diventano verdi T-07 per 5 parametri, T-08 e T-09 per i tre tipi di I/O, e T-11 per i tre tipi di I/O (vedi «Rossi prima e dopo») |
| Output | Log e junit sotto `tmp/` citati nel comando |
| Ambito | OB-07, OB-08. Fuori: OB-05 (T-05), OB-11 (T-11 di calibrazione e master), OB-03 (T-03) |
| Esito | PASS: suite (21 rossi → 7, nessun altro test cambiato), lint, trace; mutazioni rilevate tranne `m5` (limite della prova, vedi «Mutazioni»); review indipendente PASS al giro 1 |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Modifiche

Codice di prodotto, 14 file (`git diff --stat -- src`: 274 righe aggiunte, 97 tolte):

- `scheduler.hpp`, `scheduler.cpp`: `job_context::check_cancel()`, `cancelled` `sched.cancelled` con annullamento richiesto o job in arresto.
- `calibration_job.cpp`: `check_cancel` prima di ogni ingresso nei due cicli di lettura del `prepare` (lettura e riverifica).
- `fits_blocks.hpp`, `fits_blocks.cpp`: `fits_block_source::verify` riceve un controllo chiamato prima di ogni pezzo letto. `calibration_blocks.cpp`: la passata P0 passa `jc.check_cancel`; i cicli dei blocchi del `prepare` chiamano `check_cancel` al posto del controllo scritto nell'operatore. `master.cpp`: idem prima di ogni frame.
- `inventory.hpp`, `inventory.cpp`: `io_memory`, memoria delle letture presa dal pool con `reserve_or_stop` (chiamate di libreria) o dalla prenotazione di un job con sotto-prenotazioni; `inventory_fits`, `make_preview`, `digest_fits_input` la ricevono, con il token controllato prima di ogni lettura (header) o blocco (digest, fasce); `preview_size_of` e `digest_reserve_bytes` danno i byte per il piano, con la stessa formula usata dalla lettura.
- `thumbnail_cache.hpp`, `thumbnail_cache.cpp`: `get` e `rebuild` ricevono `io_memory`; `thumbnail_cache::peak_bytes` è il picco del task.
- `import_job.hpp`, `import_job.cpp`: i tre job aprono il file, dichiarano il piano (`run_single_task`: un task, un worker, `shared_bytes` del digest, `per_task_bytes` del task) e passano da `job_scheduler::run`; il digest si verifica nel `prepare`, inventario o cache nel task; il piano eseguito va in `job_files_result::plan`; parametro finale `const resource_config*` passato al runner (`job_request::resources`) e allo scheduler (`scheduler_tuning_of`).
- `job_types.cpp`: il registro passa ai tre tipi la configurazione delle risorse senza scratch.
- `docs/contracts/BASE-confini.md`: stato di OB-07 e OB-08 nel §2 e nelle righe del §4.

Impatto letto con `codegraph impact` prima della modifica (`tmp/basei2-impact.txt`): `job_context`, `job_scheduler::run`, `reserve_or_stop`, `digest_fits_input`, `inventory_fits`, `make_preview`, `verify`, `run_import_job`, `run_thumbnail_job`, `run_cache_rebuild_job`, `thumbnail_cache`. Test elencati: `test_scheduler`, `test_m1d_protocol`, `test_resources`, `test_conformance`, `conformance_network`, `test_import`, `test_import_real`, `test_inventory`, `test_recovery`, `test_simd`, `test_blocks`, `test_calibration*`, `test_master`, `test_phase`, `m2e_workflow`. Tutti sono nel ctest completo `release` e `dev`; nessuno è cambiato di stato.

## Rossi prima e dopo

| # | Test ctest | Debito | Prima | Dopo |
|---|---|---|---|---|
| 1 | `ConformanceWorkflow.T_M3BASE_03_*` | OB-03 | rosso | rosso |
| 2-4 | `T_M3BASE_05_*` (calibrazione `intera`, `blocchi`, master) | OB-05 | rosso | rosso |
| 5-7 | `T_M3BASE_07_*` (import, miniatura, ricostruzione) | OB-07, OB-08 | rosso | **verde** |
| 8-9 | `T_M3BASE_07_*` (calibrazione `intera`, `blocchi`) | OB-07 | rosso | **verde** |
| 10-12 | `T_M3BASE_08_*` (import, miniatura, ricostruzione) | OB-08 | rosso | **verde** |
| 13-15 | `T_M3BASE_09_*` (import, miniatura, ricostruzione) | OB-09 via OB-08 | rosso | **verde** |
| 16-18 | `T_M3BASE_11_*` (import, miniatura, ricostruzione) | OB-11, OB-08 | rosso | **verde** |
| 19-21 | `T_M3BASE_11_*` (calibrazione `intera`, `blocchi`, master) | OB-11 | rosso | rosso |

Totale: 21 rossi prima, 7 dopo, identici in `release` e `dev`. I messaggi di fallimento dei 7 rimasti sono identici prima e dopo (`tmp/basei2-fail-before.txt`, `tmp/basei2-fail-after.txt`).

T-11 per i tipi di I/O era rosso per il solo caso «file cambiato dopo la verifica», che si inietta al punto `sched.task`: senza scheduler il punto non esisteva (`docs/evidence/M3-base-suite.md`, riga di T-M3BASE-11). Con lo scheduler il task trova il file cambiato (`input.modified`). I casi dell'header cambiato fra il piano e la verifica restano verdi: il file è aperto prima del piano, e il `prepare` lo trova modificato dopo il ripristino al punto `sched.plan`. Nessuna parte di T-11 per i tipi di I/O resta rossa.

## Mutazioni

`tools/mutate.sh` sul preset `release`, baseline verde sui test selezionati, sorgente ripristinato e verificato con `cmp` (`tmp/basei2-mut/summary.txt`):

| Patch | Mutazione | Test selezionati | Esito |
|---|---|---|---|
| `m1-ob07-job-check` | `job_context::check_cancel` non ferma mai | `T_M3BASE_07` (6) | RILEVATA, 3 falliti (`intera`, `blocchi`, master) |
| `m2-ob07-intera-read` | nessun controllo nel ciclo di lettura della calibrazione intera | `T_M3BASE_07.*intera` (1) | RILEVATA |
| `m3-ob07-p0` | `verify` della passata P0 senza controllo | `T_M3BASE_07.*blocchi` (1) | RILEVATA |
| `m4-ob07-master` | nessun controllo prima di ogni frame del master | `T_M3BASE_07.*master` (1) | RILEVATA |
| `m5-ob07-io-prepare` | `prepare` dei tipi di I/O senza `check_cancel` e digest senza token | `T_M3BASE_07` dei tipi di I/O (3) | **SOPRAVVISSUTA** |
| `m6-ob08-plan` | piano dei tipi di I/O senza `shared_bytes` e `per_task_bytes` | `T_M3BASE_08` dei tipi di I/O (3) | RILEVATA, 3 falliti |
| `m7-ob08-thumb-peak` | picco di miniatura e ricostruzione sottostimato di un byte | `T_M3BASE_0[89]` di miniatura e ricostruzione (4) | RILEVATA, 4 falliti |

`m5` sopravvive per un limite della prova, non del codice: T-M3BASE-07 richiede il cancel all'ingresso del `prepare` e ammette un blocco letto dopo la richiesta; il file della fixture sta in un blocco di digest e il job ha un solo task, quindi senza i controlli il digest legge quel blocco e lo scheduler ferma il job prima del task (`produce`). Distinguere il controllo richiede un input di più blocchi o un'iniezione dentro il `prepare`: modifica dei test, fuori da questa unità. Per la stessa ragione la suite non distingue i controlli dopo il primo blocco (riverifica della calibrazione intera, pezzi successivi della P0, fasce dell'anteprima, letture dell'inventario).

## Review

`tools/review.sh basei2-round1 tmp/basei2-facts-round1.md` (Codex `gpt-6-astra`, sola lettura), sul delta `tmp/basei2-round1.diff`, con il perimetro chiesto dal coordinatore: solo rilievi BLOCCANTE, ciascuno con una sequenza eseguibile. Artefatto `.omc/artifacts/ask/codex-basei2-round1-20261030-0948.md`: nessun rilievo, `Verdetto: PASS`. Nessun giro 2.

## Ricerca di altre copie

In `src/core/` (unica directory dei sorgenti di prodotto) con `grep`:

- `spec.prepare`: `calibration_job.cpp:264`, `calibration_blocks.cpp:264`, `master.cpp:1037`, `import_job.cpp:206` (i tre tipi di I/O). Tutti chiamano ora `job_context::check_cancel` prima dei blocchi letti; nessun altro `prepare` nel prodotto.
- Cicli di lettura degli input (`read_exact`, `read_tile`, `read_verified_fits`, `read_input_bytes`, `read_fits_rows`): nei task `calibration_blocks.cpp:236`, `master.cpp:1029` con `task_context::check_cancel`; nei `prepare` quelli sopra; in `inventory.cpp` col token. **Non coperto da I2**: la riverifica degli input nel `finalize` del master (`master.cpp:1072`), che leggeva ogni frame per intero senza controllo; corretto dall'intervento I3 (`docs/evidence/M3-base-I3.md`), che vi chiama `job_context::check_cancel` prima di ogni frame e di ogni pezzo letto. Il caso T-M3BASE-07 vieta di eseguire `finalize` dopo il cancel e non lo esercita. Il `finalize` della residenza a blocchi legge lo staging, non gli input.
- Prenotazioni dentro `work` (`reserve_or_stop`, `try_reserve`, `pool.reserve`): restano solo `io_memory::take` sul pool (`inventory.cpp:103`, chiamate di libreria fuori da un job, usate dai test di inventario, anteprima e cache) e la prenotazione del job nello scheduler (`scheduler.cpp:411`). `job_types.cpp:66` crea un pool per la precondizione del registro di `cache.rebuild`, fuori da ogni job.

## Scelte

- **Piano dei tipi di I/O.** Un task e un worker: il lavoro è una lettura sequenziale di un file. `worker_count` e `tile_rows` della configurazione non si applicano (un `worker_count` fissato dall'utente non è un errore per questi tipi). `tile_rows` è l'altezza della fascia dell'anteprima (`block_height`) per miniatura e ricostruzione, 1 per l'import che non legge campioni; il caso «righe» di T-M3BASE-07 lo usa solo per contare i blocchi rimasti.
- **Ordine del lavoro.** Il file si apre prima del piano (dimensione e, per miniatura e ricostruzione, header per i byte dell'anteprima); il digest si verifica nel `prepare`, prima del task. La lettura degli header per il piano della miniatura avviene prima della verifica del digest, come per la calibrazione e il master.
- **Picco della miniatura.** `max(calcolo dell'anteprima, 2 · voce)`: le due fasi non si sovrappongono (le prenotazioni dell'anteprima si rilasciano prima della serializzazione). La mutazione `m7` mostra che il picco è stretto.
- **Registro.** Il campo `scheduler` dei tre tipi di I/O resta `false`: portarlo a `true` istanzierebbe T-M3BASE-05 per loro (rosso finché OB-05 non sposta `cpu.task_start` nel worker), cioè tre test nuovi. La voce del registro non riflette più il passaggio dallo scheduler: da allineare con OB-05.
- **Testi non aggiornati.** Il contratto è cambiato solo per lo stato di OB-07 e OB-08. Restano da allineare: la tabella del §1 (colonna Scheduler «no» per i tipi di I/O) e lo stato di OB-09, che dice ancora che i tipi di I/O non ricevono una `resource_config`.

## Limiti

- Una voce di cache presente con dimensione diversa da quella valida supera il picco del task: `budget` `pool.reservation_exceeded` prima della verifica del suo digest, invece di `integrity` `cache.corrupt`. Una voce corrotta della stessa dimensione dà `cache.corrupt` come prima.
- `details.resource_plan` dei tipi di I/O porta `frame_residency` e `master_residency` ai valori predefiniti (`whole`), che non descrivono questi tipi.
- Le prove di mutazione dei controlli dei tipi di I/O e dei blocchi successivi al primo non sono discriminanti (vedi «Mutazioni»).

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/BASE-confini.md` | `42e71a3ae24e99fe26b0edf8006d9b1737b8621cde9d04dde2e63d02c93235a4` |
| `src/core/calibration_blocks.cpp` | `52ef8cb2d248f4f2905ec2dbbd4ca3dbfe7912a828326bffee8b496db76d58c5` |
| `src/core/calibration_job.cpp` | `2df6c380778cd9d89fe255edfa0f019369122f2b46d781b7abc172a294f8959e` |
| `src/core/fits_blocks.cpp` | `d8c0d71a430957157b7c8895ded765db1c7d8455dd074d11818a51e87b5a74f1` |
| `src/core/import_job.cpp` | `f524e0c1b79a5b1525845ab9615abb8dffc8abec6380e3d174c161192a57e123` |
| `src/core/include/astro/fits_blocks.hpp` | `49c32447622cdf9f0f2fb5122679e474a36ca492f296cf44e1e7a87d96e4860c` |
| `src/core/include/astro/import_job.hpp` | `58eaa1d71c185f33248804cf2970db132418b5d28bbd9c170b49bc927541bc17` |
| `src/core/include/astro/inventory.hpp` | `64dd96ce0c1656d68806c51bb08d2931d87538f4878e62e073802f127beccb12` |
| `src/core/include/astro/scheduler.hpp` | `617d499fe6c5177b3d588e5f00f77a705fbc67a1ebf2b08c8fcfd0f14dd064ad` |
| `src/core/include/astro/thumbnail_cache.hpp` | `3495a68da974abc44ea5814d8e7e552d3fc0bfa4d3429834c03676904fc2f289` |
| `src/core/inventory.cpp` | `6b2fee4bf475ca554a8647c2418da24741465ac2f63d0cdd7fbae81b30e17ef1` |
| `src/core/job_types.cpp` | `b5bd937c1d3088eeed6b5eaf2c24c92244ee4279852941551ad971f5ecc8d933` |
| `src/core/master.cpp` | `062e3f341aeb95b9f9d8aa9e233a7bfa8cee4f9af0f4d56a56f2ae683940dcb2` |
| `src/core/scheduler.cpp` | `9588b0adfc402c790329ee0f837972128a3548ef82b5012055f228c429d38069` |
| `src/core/thumbnail_cache.cpp` | `732030d91566f407df2125963ec36ce39b0d3476d3d16e1edc69cd8d6905279d` |
| `tests/unit/test_conformance.cpp` | `a75bee61a509be32163cfe862cbb4f25a1a9187a256a4fd7c404c96d6799683b` |

- Test release (`tmp/release-junit.xml`): totali 622, fast 615, slow 7, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 616, fast 616, slow 0, falliti 0
<!-- evidence:end -->
