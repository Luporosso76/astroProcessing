# M3, base comune, intervento I3: OB-05 nello scheduler, OB-11 nella lettura degli input

Stato: **DA ARBITRARE**: review FAIL ai giri 1 e 2; il rilievo MED del giro 2 è corretto dopo il giro 2, senza un terzo giro (tetto di 2). Unità: M3, base comune, terza unità di codice dei debiti del §4 di `docs/contracts/BASE-confini.md`. Suite: `tests/unit/test_conformance.cpp` (non modificata, SHA-256 nel blocco generato); rossi di partenza: i 7 lasciati da I2 (`docs/evidence/M3-base-I2.md`).

| Campo | Valore |
|---|---|
| Unità | M3 / base comune, intervento I3 (codice) |
| Scopo | Portare OB-05 (punto `cpu.task_start` e controllo FP nel worker dello scheduler) e OB-11 (lettura verificata con un solo proprietario, header del piano confrontati, riverifica prima della pubblicazione per ogni tipo) nel proprietario; `check_cancel` nella riverifica del `finalize` del master |
| Comando | Prima: `cmake --preset release`, `cmake --build --preset release -j4`, `ctest --preset release -R "Conformance\|M3BASE" -j4 --output-junit tmp/basei3-before-junit.xml` (log `tmp/basei3-before.log`, elenco `tmp/basei3-fail-before.txt`). Dopo: stessa build; stessa selezione (log `tmp/basei3-after2.log`, `tmp/basei3-fail-after.txt`); selezione mirata `tmp/basei3-focus.sh` (log `tmp/basei3-focus-3.log`); `ctest --preset release -j4 --output-junit tmp/release-junit.xml` (log `tmp/release.log`); `tools/check.sh` con `CMAKE_BUILD_PARALLEL_LEVEL=4` e `CTEST_PARALLEL_LEVEL=4` (log `tmp/basei3-check.log`, junit `tmp/dev-junit.xml`); `tools/lint.sh` (log `tmp/basei3-lint.log`); `python3 -S tools/check_job_types.py .`; `python3 -S tools/trace.py check --stage M2 --map tests/trace/M2.tsv --build-dir build/release --ctest-junit tmp/release-junit.xml` (`tmp/basei3-trace.txt`); `tmp/basei3-mut/run.sh` (`tools/mutate.sh` per ogni patch di `tmp/basei3-mut/`, generate da `tmp/basei3-mut/make.py`); `python3 -S tools/evidence.py docs/evidence/M3-base-I3.md` |
| Input | Codice a `106faf7` più le modifiche elencate sotto; fixture sintetiche di `tests/fixtures/conformance_inputs.hpp` |
| Versioni | GCC 15.2.0, CMake 4.2.3; preset `release` e `dev` (ASan e UBSan) |
| Ambiente | Linux 7.0.0-38-generic x86_64, 16 CPU, worktree isolato senza il collegamento `data` ai dati reali, 4 job |
| Risultato | Suite di conformità: 89 test e 7 rossi prima, 92 test e 1 rosso dopo (T-03, di I4). Release completo, dopo l'ultima correzione: 608 test, 5 falliti: T-03 e i 4 test su dati reali, fermi sul prerequisito mancante (`data/public` assente nel worktree). Dev: 602 test, 2 falliti (T-03 e `Fits.REAL_DATA_PUB_LFC_01`, stesso prerequisito). Test nuovo fuori dalla suite: `Hash.reverify_input_stops_a_file_appended_during_the_reading` |
| Output | Log e junit sotto `tmp/` citati nel comando |
| Ambito | OB-05, OB-11, cancel nella riverifica del master (OB-07). Fuori: OB-03 (T-03), OB-19 |
| Esito | PARZIALE: suite 7 rossi → 1 (T-03, di I4), nessun altro test cambiato oltre i prerequisiti di dati reali; lint e registro dei tipi verdi; mutazioni rilevate tranne quelle dichiarate come limite della prova; review FAIL al giro 2 (MED), corretto dopo il tetto dei giri e da arbitrare |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Modifiche

Codice di prodotto, 16 file (`git diff --numstat -- src`: 220 righe aggiunte, 151 tolte):

- `cpu_dispatch.hpp`, `cpu_dispatch.cpp`: `begin_worker_task` (punto `cpu.task_start`, poi `require_fp_environment`) segna il thread come worker; `begin_kernel_task` richiama il punto solo fuori dai worker e controlla sempre MXCSR.
- `scheduler.cpp`: `worker_loop` chiama `begin_worker_task` al posto di `require_fp_environment`, prima della memoria del task e di `run_task`.
- `job_types.cpp`: `scheduler` vero anche per `io.import`, `io.thumbnail`, `cache.rebuild`.
- `hash.hpp`, `hash.cpp`: `require_input_digest`, `require_planned_header`, `read_verified_input`, `digest_input`, `reverify_input` (con `check_unchanged` del file riaperto, correzione del giro 1); `read_verified_fits` usa `require_input_digest`.
- `fits.hpp`, `fits.cpp`, `fits_scan.cpp`: `fits_plan::bitpix`; `fits_detail::check_path_names` (spostato da `fits_block_source::check_identity`).
- `fits_blocks.hpp`, `fits_blocks.cpp`: `fits_block_source::plan()`; P0 con `require_input_digest`; `check_identity` con `check_path_names`.
- `inventory.cpp`: `digest_fits_input` con `digest_input`.
- `import_job.cpp`: `run_single_task` riceve input, percorso, digest e hook; `prepare` verifica il digest, `finalize` lo riverifica con `reverify_input`; tolti `verify_digest` e i `check_unchanged` dopo il task.
- `calibration_job.cpp`: gli input aperti per il piano restano aperti; `prepare` con `read_verified_input` (tolti i confronti di dimensione e geometria e la riverifica dopo il preflight); `finalize` con `reverify_input` prima di `write_files`.
- `calibration_blocks.cpp`: `require_planned_header` sugli header della sorgente prima della passata P0 (tolti i confronti di dimensione e geometria); `finalize` con `reverify_input` dopo `check_identity`.
- `master.cpp`: `prepare` con `read_verified_input` (tolti digest inline e confronto di geometria); `finalize` con `check_cancel` e `reverify_input`.
- `tests/unit/test_hash.cpp`: test `Hash.reverify_input_stops_a_file_appended_during_the_reading` (dopo il giro 2 della review).
- `docs/contracts/BASE-confini.md`: stato di OB-05 e OB-11 nel §2 e nelle loro righe del §4; riga del limite del `finalize` del master in OB-07. `docs/evidence/M3-base-I2.md`: la sola riga che dichiarava quel limite.

Impatto letto con `codegraph impact` prima delle modifiche: `begin_kernel_task` (calibrazione, master, blocchi; test `test_master`, `test_simd`), `worker_loop`, `read_input_bytes`, `fits_block_source`, `fits_plan` (test `test_blocks`). Tutti nel ctest completo `release` e `dev`; nessuno cambia di stato.

## Rossi prima e dopo

| # | Test ctest | Debito | Prima | Dopo |
|---|---|---|---|---|
| 1 | `ConformanceWorkflow.T_M3BASE_03_*` | OB-03 (I4) | rosso | rosso, messaggi identici |
| 2-4 | `T_M3BASE_05_*` (calibrazione `intera`, `blocchi`, master) | OB-05 | rosso | **verde** |
| 5-7 | `T_M3BASE_05_*` (import, miniatura, ricostruzione) | OB-05 | assente | **verde** (nuovi: `scheduler` vero nel registro) |
| 8-10 | `T_M3BASE_11_*` (calibrazione `intera`, `blocchi`, master) | OB-11 | rosso | **verde** |

Cause dei rossi di partenza (`tmp/basei3-fail-before.txt`): T-05, il punto `cpu.task_start` scattava nel kernel, dopo `sched.task`; T-11 `intera`, nessuna riverifica dopo i task («file cambiato dopo la verifica»); T-11 `intera` e master, campioni decodificati prima del confronto di NAXIS1 e NAXIS2; T-11 su BITPIX, tipo del campione mai confrontato (tutti e tre i tipi).

Durante lo sviluppo il primo ordine dei controlli faceva fallire `CalibJob.T_M2E_11`, `CalibJob.T_M2E_31` e `Import.T_M2R1_10`, che fissano i codici attesi (`input.missing`, `input.geometry_changed`, `input.size_changed`). L'ordine finale: in `read_verified_input` percorso, lettura, digest, header del piano, `check_unchanged`; in `reverify_input` apertura, dimensione, digest, `check_unchanged` del file riaperto e poi dell'input aperto. Con questo ordine sono verdi (`tmp/basei3-focus-3.log`).

## Mutazioni

`tools/mutate.sh` sul preset `release` con 4 job, una patch per controllo aggiunto, baseline verde sui test selezionati, sorgente ripristinato e verificato con `cmp` (`tmp/basei3-mut/summary-1.txt`, `summary-2.txt` per `m09` e `m14` rilanciate: `m09` non compilava, `m14` con la selezione allargata a `T_M2R1_10`):

| Patch | Mutazione | Test selezionati | Esito |
|---|---|---|---|
| `m01-ob05-worker-seam` | `worker_loop` con `require_fp_environment` al posto di `begin_worker_task` | `T_M3BASE_05` (6) | RILEVATA, 6 falliti |
| `m02-ob05-kernel-seam-once` | punto `cpu.task_start` anche nel kernel dentro i worker | `T_M2R2R_02_automatic_workers` (1) | RILEVATA |
| `m03-ob11-digest` | `require_input_digest` non ferma mai | `T_M3BASE_11` (6) | RILEVATA, 6 falliti |
| `m04-ob11-header-size` | nessun confronto della dimensione del piano | `T_M3BASE_11`, `T_M2E_31` (7) | SOPRAVVISSUTA |
| `m05-ob11-header-geometry` | nessun confronto di larghezza, altezza, piani | `T_M3BASE_11`, `T_M2E_31` (7) | RILEVATA (2 falliti; lo strumento esce con 2 perché il caso `blocchi` termina con un abort del processo invece di un fallimento gtest) |
| `m06-ob11-header-type` | nessun confronto del tipo | `T_M3BASE_11` (6) | SOPRAVVISSUTA |
| `m18-ob11-header-bitpix` | nessun confronto di BITPIX | `T_M3BASE_11` (6) | SOPRAVVISSUTA |
| `m19-ob11-header-type-and-bitpix` | né tipo né BITPIX | `T_M3BASE_11` (6) | RILEVATA |
| `m07-ob11-header-hdus` | nessun confronto di INVALID e VARIANCE | `T_M3BASE_11` (6) | SOPRAVVISSUTA |
| `m08-ob11-read-unchanged` | nessun `check_unchanged` in `read_verified_input` | `T_M3BASE_11`, `T_M2E_31`, `T_M2E_11` (9) | SOPRAVVISSUTA |
| `m16-ob11-read-path-names` | nessun `check_path_names` in `read_verified_input` | `T_M3BASE_11`, `T_M2E_11` (8) | RILEVATA |
| `m17-ob11-reverify-size` | nessun confronto della dimensione in `reverify_input` | `T_M3BASE_11`, `T_M2R1_10` (11) | RILEVATA |
| `m09-ob11-reverify-opened` | `reverify_input` senza `check_unchanged` dell'input aperto | `T_M3BASE_11`, `T_M2R2_07`, `T_M2E_11` (12) | SOPRAVVISSUTA |
| `m10-ob11-intera-reverify` | nessuna riverifica nel `finalize` della calibrazione intera | `T_M3BASE_11`, `T_M2E_11` (8) | RILEVATA |
| `m11-ob11-blocks-header` | nessun confronto degli header prima della passata P0 | `T_M3BASE_11` (6) | RILEVATA (il caso `blocchi` termina con un abort del processo; lo strumento esce con 2) |
| `m12-ob11-blocks-reverify` | nessuna riverifica nel `finalize` a blocchi | `T_M3BASE_11`, `T_M2R3_01` (18) | SOPRAVVISSUTA |
| `m13-ob11-master-reverify` | nessuna riverifica nel `finalize` del master | `T_M3BASE_11`, `T_M2R2_07` (10) | RILEVATA |
| `m14-ob11-io-reverify` | nessuna riverifica nel `finalize` dei tipi di I/O | `T_M3BASE_11`, `T_M2R1_10` (11) | RILEVATA |
| `m15-ob07-master-finalize-cancel` | nessun `check_cancel` nella riverifica del master | `T_M3BASE_07`, `T_M3BASE_11`, `MasterJob` (24) | SOPRAVVISSUTA |
| `m20-ob11-reverify-reopened-unchanged` | `reverify_input` senza `check_unchanged` del file riaperto (correzione del giro 1) | `T_M3BASE_11`, `T_M2R1_10`, `T_M2E_11`, `T_M2R2_07`, `T_M2R3_01` (29): `tmp/basei3-mut/summary-3.txt`; con `Hash.reverify_input_stops_a_file_appended_during_the_reading` (30): `summary-4.txt` | SOPRAVVISSUTA senza il test nuovo; RILEVATA con il test nuovo |

Le sopravvissute sono limiti della prova, non del codice:

- `m04`: con lo stesso descrittore la dimensione verificata è per costruzione quella del piano (`read_input_bytes` ferma un file cresciuto, il digest uno diverso); nella residenza a blocchi una dimensione diversa dà comunque `input.digest_mismatch` nella passata P0. Nessun caso cambia la dimensione fra piano e verifica senza cambiare il resto.
- `m06`, `m18`: la mutazione di BITPIX della suite (-32 → 32) cambia sia BITPIX sia il tipo (`f32` → `f64`); ciascun confronto copre l'altro, entrambi tolti (`m19`) la suite fallisce. Nessun caso cambia BITPIX a tipo invariato (8, 16, -32).
- `m07`: le fixture non hanno HDU INVALID né VARIANCE.
- `m08`, `m09`: la suite cambia solo NAXIS1, NAXIS2, BITPIX, che il confronto esplicito ferma prima; nessun caso cambia un header fuori da `fits_plan` né sostituisce il file al percorso con uno dal contenuto atteso mentre quello aperto cambia.
- `m12`: nella residenza a blocchi un input cambiato durante i task è fermato prima da `read_tile` (`input.consumed_digest`) o da `check_identity`.
- `m15`: nessun caso richiede il cancel durante un `finalize` (T-M3BASE-07 vieta `finalize` dopo il cancel).

`m05` e `m11`: senza il confronto la residenza a blocchi prosegue con il layout del piano diverso da quello del file e il processo del test termina con SIGABRT (ctest «Subprocess aborted»); la causa precisa dell'abort non è stata letta (*ipotesi*: un `ASTRO_ASSERT` sulle dimensioni dei blocchi).

## Review

- Giro 1: `tools/review.sh basei3-round1 tmp/basei3-facts-round1.md` (Codex `gpt-6-astra`, sola lettura), delta `tmp/basei3-round1.diff`, artefatto `.omc/artifacts/ask/codex-basei3-round1-20261030-1032.md`: FAIL. [BLOCCANTE] `reverify_input` non controllava il file riaperto dopo il digest: un file cresciuto durante la riverifica passava (nella residenza a blocchi `opened` è nullo). [LOW] la descrizione di OB-07 nel contratto colloca ancora la riverifica della calibrazione intera nel `prepare`.
- Correzione: `fits_detail::check_unchanged` del file riaperto dopo il digest (`hash.cpp:145`), commento e testo di OB-11 allineati (delta `tmp/basei3-round2.diff`). Il LOW non è corretto: il perimetro di questa unità sul contratto sono gli stati di OB-05 e OB-11 e la riga del limite del master.
- Giro 2: `tools/review.sh basei3-round2 tmp/basei3-facts-round2.md`, artefatto `.omc/artifacts/ask/codex-basei3-round2-20261030-1039.md`: FAIL. [MED] manca una prova discriminante della correzione (`m20` sopravvissuta). [LOW] lo stesso del giro 1.
- Dopo il giro 2, correzione minima del MED: test `Hash.reverify_input_stops_a_file_appended_during_the_reading` in `tests/unit/test_hash.cpp` (fuori dalla suite di conformità), che appende al file durante la riverifica, senza input aperto, e si aspetta `integrity` `input.size_changed`; `m20` diventa RILEVATA. Questa correzione non è stata rivista: il tetto di 2 giri ferma `review.sh`, l'arbitraggio spetta al coordinatore.

## Ricerca di altre copie

In `src/core/` con `grep` dopo la modifica: `input.digest_mismatch` compare solo in `hash.cpp`; `input.geometry_changed` solo in `require_planned_header`; `read_verified_fits` non ha chiamanti nel prodotto (resta per i test di libreria `test_hash`, `test_pool`, `test_job`); `check_unchanged` resta in `inventory.cpp` (inventario, anteprima, digest: letture di libreria sullo stesso descrittore) e in `fits_block_source::check_identity`.

## Scelte

- **Guardia FP di kernel nei worker.** Resta: ADR-07.a §Aggiunte di R5 chiede il controllo all'inizio di ogni kernel. Il §4 del contratto proponeva di tenerla solo fuori dallo scheduler con un emendamento dell'ADR; non è fatto. Nei worker il punto `cpu.task_start` scatta una volta per task, nel worker (`T_M2R2R_02` conta un'affinità per task).
- **Input aperti dal piano alla lettura.** Calibrazione intera e master leggono i byte dallo stesso descrittore da cui hanno letto gli header, dopo aver controllato che il percorso lo indichi ancora; `check_unchanged` dopo la verifica lega al file verificato ogni valore di header letto prima (fase, linearità, layout delle righe), non solo quelli di `fits_plan`.
- **Riverifica per percorso.** `reverify_input` riapre il percorso: un file rimosso dà `input.missing`, uno riscritto `input.size_changed` o `input.digest_mismatch`; poi controlla che l'input aperto per il lavoro sia invariato. Il buffer è del chiamante (`buffer` del `finalize` a blocchi, blocco di digest dei tipi di I/O, chunk del master, al più 1 MiB dalla memoria condivisa nella calibrazione intera): nessuna prenotazione nuova nel piano.
- **Codici.** Larghezza, altezza e piani restano `input.geometry_changed`, dimensione `input.size_changed`, come prima e come fissano i test di M2; BITPIX, tipo, INVALID e VARIANCE `input.header_changed` (nuovo).
- **BITPIX nel piano.** Il tipo del campione non distingue BITPIX 8, 16 e -32 (tutti `f32`): `fits_plan::bitpix` rende confrontabili i byte grezzi per campione.
- **Registro.** `scheduler` vero per i tre tipi di I/O allinea il registro (scelta rinviata da I2): T-M3BASE-05 gira anche per loro.

## Limiti

- Residenza a blocchi: fase e linearità (`peek_header`) e `primary_sample_bytes` vengono da aperture per percorso diverse da quella che la passata P0 verifica; sono riconfrontati solo i valori di `fits_plan` (con BITPIX).
- Tipi di I/O: gli header del piano dell'anteprima non sono riconfrontati per valore; li lega al file verificato il `check_unchanged` di `digest_fits_input` sullo stesso descrittore.
- `check_unchanged` vale nei limiti di `mtime` e `ctime` del filesystem.
- I 4 test su dati reali (`Fits.REAL_DATA_PUB_LFC_01`, `T-M2E-13.real_palomar_lfc`, `T-M2R2-09.real_palomar_product_masters`, `T-M2R1-09.public_samples`) non girano in questo worktree, che non ha il collegamento `data`: esercitano import, calibrazione e master modificati, quindi vanno rieseguiti nel checkout principale. Per la stessa ragione `trace.py check` riporta 24 ID non coperti, tutti a valle di T-M2E-13, T-M2R2-09 e T-M2R1-09.
- `tools/check.sh` si ferma al ctest dev per il rosso T-03 (come in I2): lint, `check_job_types.py` e `trace.py check` sono stati lanciati a parte.
- La descrizione di OB-07 nel contratto (§2.3) colloca ancora la riverifica della calibrazione intera nel `prepare` (rilievo LOW della review, fuori dal perimetro di questa unità sul contratto); la riga di OB-07 nel §4 cita ancora il limite del `finalize` del master, ora pagato.

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/BASE-confini.md` | `573fba6402d89abb5a8df3566792f36a85602519ff6525da755d7d5c0472eccb` |
| `docs/evidence/M3-base-I2.md` | `e9a4573698c5e731823f54bd00bbeeb6b78cec551467381e242c2f746e0c32ea` |
| `src/core/calibration_blocks.cpp` | `4a87981915c214997d1864f602a53b51f636012efd08f97be7f217bb640f1e21` |
| `src/core/calibration_job.cpp` | `beddb1fe3c9ab0ab1a320408c3593d562b34ea491729293a1c4a4c94748baeac` |
| `src/core/cpu_dispatch.cpp` | `73c737a9f1ce03e297be90126d47d85c1fe5f451fbdacde579478bb5bff367ae` |
| `src/core/fits.cpp` | `f6f18576e927dcedd3c8c920f9923058fb2c9e8d7dc5523114272f75ad046967` |
| `src/core/fits_blocks.cpp` | `f6adaddd1dcf88337e9e2874a2f7a336d19645398eb2ca05dde85d90ff066ab7` |
| `src/core/fits_scan.cpp` | `0caccd3aac283689838f4fa060c5c1fd28fbc1d04351a60d5e5088b3f1d84b0e` |
| `src/core/hash.cpp` | `f8507547472b5b58b882ae069256530cb1a091b879a373a749afbfde563d0823` |
| `src/core/import_job.cpp` | `945d3ee4045c7ff15e2292551a5cc34384ef4431b09181cfffe6816c9e0df487` |
| `src/core/include/astro/cpu_dispatch.hpp` | `21c32fbd9c9416789ec7d31bf3c561d631b460a9ae9ce4162a1e7aab4fb7adb2` |
| `src/core/include/astro/fits.hpp` | `37d4c4a21797c3a54c1d1bdc80c2e12b383d00d729571a3e41ef8fe07976abd7` |
| `src/core/include/astro/fits_blocks.hpp` | `49c32447622cdf9f0f2fb5122679e474a36ca492f296cf44e1e7a87d96e4860c` |
| `src/core/include/astro/hash.hpp` | `35127c7ac5255cc715ad016da94c4958136dd39a2342ee716324f3db2d3d4a24` |
| `src/core/inventory.cpp` | `6b2fee4bf475ca554a8647c2418da24741465ac2f63d0cdd7fbae81b30e17ef1` |
| `src/core/job_types.cpp` | `fda920302c2e974237f08f4ff6c1a0a86f7da20548d419fdfed899937b5d5255` |
| `src/core/master.cpp` | `1aba5a2b6688d8d1c4e198b065589283ce62a35e30bf3866933e1c3d4b35aa9b` |
| `src/core/scheduler.cpp` | `9588b0adfc402c790329ee0f837972128a3548ef82b5012055f228c429d38069` |
| `tests/unit/test_conformance.cpp` | `a75bee61a509be32163cfe862cbb4f25a1a9187a256a4fd7c404c96d6799683b` |
| `tests/unit/test_hash.cpp` | `77efb55f69446ee0b932b63687f7d6f6e187e78eddad6f7805741213902fb6c3` |

- Test release (`tmp/release-junit.xml`): totali 805, fast 798, slow 7, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 799, fast 799, slow 0, falliti 0
<!-- evidence:end -->
