# Gate M2: verifica complessiva della milestone

Stato: **PASS per arbitraggio del coordinatore**, 30 ottobre 2026 (giro 10, sotto). Storico: **RIAPERTO** il 29 ottobre 2026. Il PASS del giro 7 (commit `fe94d0b`) è revocato: manca F-IO-10 (import prestacked e calibrato tipizzato), che il registro dell'appendice D impone a M2.c (`M2.c requires_all F-IO-10`, piano attivo riga 4415; foglia a riga 1821). Lo ha rilevato la review del piano M3 (`.omc/artifacts/ask/codex-m3plan-review1-20261029-1424.md`); i sette giri del gate non l'avevano visto. Storico del gate: Autore: agente executor su Claude Sonnet 5.5. Review indipendente: Codex `gpt-6-astra` (reasoning high, sola lettura), giri 1-6 FAIL con correzioni, giro 7 PASS (`.omc/artifacts/ask/codex-m2-gate-review7-20261029-1420.md`). Clausola: `plans/M2.md`, sezione "Gate M2". Contratti toccati: `docs/contracts/ADR-05-dag-provenance.md` (provenance), `ADR-06-budget-scheduler.md` (soglie F-RES-01), `ADR-04-persistenza.md` (T-M2C-11).

Giro 8 (gate rifatto, `plans/M2-residui.md` R6 e sezione 6), 30 ottobre 2026: esecuzione finale provata sul working tree sopra `60a6cc8`, prima del commit, con `tools/check_m2_trace.py` a uscita 0. Riesecuzione identica sul commit del gate `cb35d89` con uscita 0 (sezione «Riesecuzione sul commit del gate»); il gate passa con la review indipendente del record (clausola 4). Le sezioni da «Clausola del gate e record» in poi sono lo storico dei giri 1-7. Il tetto di 3 giri di review vale per il gate rifatto: giro 8 = review 1, giro 9 = review 2, il prossimo è la review 3 e ultima; dopo, arbitraggio del coordinatore.

## Giro 10: chiusura per arbitraggio del coordinatore

- Tetto delle review (proprietario, 30 ottobre 2026; `AGENTS.md`, ciclo di un'unità, punto 4): 2 giri per unità e per gate. Il gate rifatto li ha usati: giro 8 (review 1, FAIL) e giro 9 (review 2, FAIL con un solo rilievo BLOCCANTE). Non c'è una terza review: chiude il coordinatore.
- Rilievo del giro 9: lo stop `cpu.fp_mode` prima dell'avvio del job non lasciava provenance (NFR-FBK-01.a). Corretto da R5.d (`a9bf82d`, `docs/evidence/M2-R5d.md`): uno stop prima del lavoro chiude il job `failed` (`cancelled` per quella categoria) e, su store durevole, scrive `stop_before_work_record` con sole stringhe e interi; review dell'unità PASS al giro 2, 12 mutazioni su 12 rilevate.
- Esecuzione finale del coordinatore, 30 ottobre 2026, 6 job, `ulimit -c 0`, build pulite in directory nuove:
  1. `tools/check.sh` (log `tmp/gate10-check.log`): uscita 0, `lint: ok`.
  2. `ctest --preset dev-full -j6 --output-junit tmp/dev-junit.xml` (log `tmp/gate10-dev.log`): 516/516, 1690,9 s.
  3. `cmake --preset release`, `cmake --build --preset release -j6`, `ctest --preset release -j6 --output-junit tmp/release-junit.xml` (log `tmp/gate10-rel.log`): 515/515, 114,8 s.
  4. `python3 -S tools/check_m2_trace.py --run build/dev:tmp/dev-junit.xml --run build/release:tmp/release-junit.xml` (`tmp/gate10-trace.txt`): **uscita 0**, `check_m2_trace: 63 ID nello stadio M2, 0 con problemi, 0 problemi di esecuzione`.
- Revisioni delle build: dev `a9bf82d`, release `9d3996c` (entrambe `dirty=false`). Fra i due commit cambiano solo `AGENTS.md`, `plans/M3.md` e `tools/review.sh`, nessuno compilato né usato da ctest: prodotto e test sono gli stessi. Commit del gate: `9d3996c`.
- Clausola 4: le review del gate (giri 8 e 9) e le review delle unità che ne hanno chiuso i rilievi (R5.c, R6.c, R5.d, tutte PASS) coprono il delta; la chiusura senza terza review è l'arbitraggio previsto dal tetto.
- Esito: **Gate M2 PASS** per arbitraggio del coordinatore. Limiti accettati: sezione «Limiti residui accettati per M2» e limiti dei record delle unità (`M2-R5d.md`: provenance in memoria per `run_job`, stop di workflow prima di ogni job senza job, stop `cpu.fp_mode` dentro il lavoro con provenance completa).

## Giro 9 (review 2 di 3 del gate rifatto): correzioni della review del giro 8 e riesecuzione

- Review del giro 8 (Codex `gpt-6-astra`, sola lettura, `.omc/artifacts/ask/codex-m2gate-round8-20261030-0347.md`): FAIL, 1 BLOCCANTE, 1 MED, 2 LOW.
  - BLOCCANTE: con DM spenta e parametri subnormali il job riceveva SIGFPE nel confine `run_job_files`, prima del controllo dell'ambiente FP; NFR-FBK-01.a, ADR-07.a e G-STOP non potevano chiudersi con quel limite. Corretto da R5.c (`463ccf9`, `docs/evidence/M2-R5c.md`): controllo all'ingresso di `run_job`, `run_job_files`, `run_calibration_job`, `run_master_job` e in ogni task dello scheduler prima della lettura dei dati; review PASS al giro 2.
  - MED: `check_m2_trace.py` tollerava membri inesistenti degli intervalli. Corretto da R6.c (`e55c512`, sezione «Correzioni dopo la review del giro 8»); review PASS al giro 1.
  - LOW: descrizione di T-M2R3-07 nella tabella della clausola 3 e ordine delle esecuzioni nei limiti; corretti nel testo (`e55c512`).
- Commit del gate: `463ccf9` (`git status` pulito, salvo il collegamento simbolico `data` non versionato).
- Riesecuzione del coordinatore, 30 ottobre 2026, 6 job, `ulimit -c 0`, build pulite in directory nuove:
  1. `tools/check.sh` (log `tmp/gate9-check.log`): uscita 0, `lint: ok`.
  2. `ctest --preset dev-full -j6 --output-junit tmp/dev-junit.xml` (log `tmp/gate9-dev.log`): 505/505, 1738,5 s.
  3. `cmake --preset release`, `cmake --build --preset release -j6`, `ctest --preset release -j6 --output-junit tmp/release-junit.xml` (log `tmp/gate9-rel.log`): 504/504, 121,0 s.
  4. `python3 -S tools/check_m2_trace.py --run build/dev:tmp/dev-junit.xml --run build/release:tmp/release-junit.xml` (`tmp/gate9-trace.txt`): **uscita 0**, `check_m2_trace: 63 ID nello stadio M2, 0 con problemi, 0 problemi di esecuzione`.
- Il blocco generato del giro 8 riporta conteggi e hash di questa esecuzione.
- Esito: clausole 1-3 soddisfatte sul commit `463ccf9`; clausola 4 nella review del giro 9.

## Giro 8 (review 1 di 3 del gate rifatto): esecuzione finale del gate rifatto (R6)

- Unità: M2, R6 (`plans/M2-residui.md`, sezione 6, clausole 1-3).
- Scopo: un'unica esecuzione completa dev e release, slow compresi, e stato finale della tracciabilità dei 63 ID a stadio `M2.*` e `M2.entry`.
- Commit del gate: `cb35d8988601d81a61f36820830d4e4782a4e265` (`cb35d89`). Prova preliminare sul working tree sopra `60a6cc8`; modifiche del giro solo a documenti, `tests/trace/M2.tsv` e `CMakePresets.json` (preset `dev-full`), nessun codice di prodotto né test.
- Versioni: piano attivo SHA-256 `95abbb1a…4094`; GCC 15.2.0, CMake e CTest 4.2.3, Ninja 1.13.2, ccache 4.12.3, Python 3.14.4; CFITSIO 4.6.3, SQLite 3.46.1, OpenSSL 3.5.5, FFTW 3.3.10.
- Ambiente: Linux 7.0.0-38-generic x86_64, AMD Ryzen 7 6800H, 16 CPU, 30 GiB RAM; 4 job per build e test (sotto il tetto del 70%); `ulimit -c 0`; temporanei in `tmp/` e `build/<preset>/test_tmp/sys`; `data` collegamento simbolico non versionato ai dati pubblici del progetto.

### Comandi

1. Build pulita in directory nuove (`tmp/gate-final.sh`): `rm -rf build/dev build/release`; per `dev` e `release`: `cmake --preset <p>` e `cmake --build --preset <p>` con `CMAKE_BUILD_PARALLEL_LEVEL=4` e `CCACHE_DIR=tmp/cache/ccache-gate` nuovo. Log `tmp/gate-final-dev-build.log`, `tmp/gate-final-release-build.log`: zero warning.
2. Release: `ctest --preset release -j4 --output-junit tmp/release-junit.xml` (log `tmp/gate-final-release-test.log`): 500/500, 155,7 s.
3. Lint: `CMAKE_BUILD_PARALLEL_LEVEL=4 CTEST_PARALLEL_LEVEL=4 tools/check.sh` (log `tmp/gate-final-check.log`): uscita 0, `lint: ok`, dev fast 494/494. Rigenera `source_revision.cpp` dallo stato di Git e ricollega i binari dev (39 passi): l'esecuzione dev del punto 4 gira su quei binari.
4. Dev: `ctest --preset dev-full -j4 --output-junit tmp/dev-junit.xml` (`tmp/gate-devfull.sh`, log `tmp/gate-final-devfull-test.log`): 501/501, 1726,7 s. Il preset `dev` filtra `fast`, e `-L` da riga di comando si somma al filtro invece di sostituirlo (`ctest --preset dev -L 'fast|slow' -N` elenca 494 test): `dev-full` è il preset `dev` con filtro `^(fast|slow)$`, stesso ambiente ASan, UBSan e temporanei.
5. Gate: `python3 -S tools/check_m2_trace.py --run build/dev:tmp/dev-junit.xml --run build/release:tmp/release-junit.xml` (uscita in `tmp/gate-final-check-m2-trace.txt`): **uscita 0**, `check_m2_trace: 63 ID nello stadio M2, 0 con problemi, 0 problemi di esecuzione`. Prova negativa: con la matrice di prima del giro (`--matrix tmp/test-matrix.before-r6.md`) uscita 1, 63 ID con problemi.
6. `python3 -S tools/trace.py check --stage M2 --map tests/trace/M2.tsv` su `build/dev` e su `build/release` con il junit della propria build: 63 ID, 0 non coperti in entrambe. Matrice e mappa risolvono gli stessi test per ogni ID (`tmp/r6-compare.py`, 0 righe diverse).

Conteggi e hash generati:

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/test-matrix.md` | `c39129e4917f3c2ec58fe376cec980e3a976735762a7677f42d41ff28bd75c1e` |
| `tests/trace/M2.tsv` | `f559fc60e9888c586cc4917d095f222ffd8fc5d25b0196a67becfa891025bdcc` |
| `tools/check_m2_trace.py` | `2ff5df7acdcc07ef45edd4c80fdb6ed7c7d5decd9e5f4629cc1751842c216a9b` |
| `tools/trace.py` | `ef7185ebc60b152f3b8436c55ef7b94704bfddf33568a9ebf21939702c08f040` |

- Test dev (`tmp/dev-junit.xml`): totali 598, fast 598, slow 0, falliti 7
- Test release (`tmp/release-junit.xml`): totali 604, fast 597, slow 7, falliti 7
- Tracciabilità (`tests/trace/M2.tsv`, build `build/dev`, junit `tmp/dev-junit.xml`): trace: 63 ID nello stadio M2, 38 non coperti
- Tracciabilità (`tests/trace/M2.tsv`, build `build/release`, junit `tmp/release-junit.xml`): trace: 63 ID nello stadio M2, 0 non coperti
<!-- evidence:end -->

Il test presente solo in dev è `BuildInfo.SanitizersAreActiveInDevBuild`. I sette `slow` girano ora anche sotto ASan e UBSan: T-M2E-13 1578 s, T-M2R2-09 1622 s, T-M2D-13 417 s, T-M2R2-04 308 s, T-M2E-21 (master del prodotto) 254 s, T-M2E-21-26 102 s, T-M2R1-09 11 s.

### Tracciabilità: correzioni del giro

- Stati: i 63 ID passano a `CHIUSO` (funzionali, NFR, G, consegne, aggregati) o `VERIFICATO` (ADR, C, D, già `APPROVATO` in R0); ogni riga della matrice cita `M2-gate.md`. Un ID è salito solo con i test della riga esistenti in `ctest -N` e passati in entrambi i junit.
- C-02.a: la riga nominava i test solo fra i pianificati. Ora T-M2R2-01…09 e T-M2R2R-02…05 (R2 e R2.r), come `tests/trace/M2.tsv`.
- D-03: «come ADR-01» diventa T-M2F-01 (build C++20 del core headless, arco 4078), come la mappa e il verbale `docs/decisions/M2-entry.md` (riga 18).
- D-09.a: la riga diceva «tolleranze derivate prima della misura in T-M2E». Ora nomina i test del verbale (T-M2E-01, `CalibM2E.T_M2E_02_nondyadic_cancellation_even_and_odd_n`, che contiene la prova di T-M2E-20, T-M2E-21, 26) e quelli delle tolleranze preregistrate di R1 (T-M2R1-03), R2 (T-M2R2-01…04, 09, C-02 §9) e R5 (T-M2R5-01, 02, 06, ADR-07). La mappa è aggiornata.
- F-PRJ-06: `T-PRJ-01` è l'ID di un test del registro (riga 3547, stadio M3.s, «incrementale da M2»; F-PRJ-06 `verified_by` T-PRJ-01, arco 3715), non un nome di ctest. La sua parte M2 è T-M2R4-01…04, già nominati nella riga. Il riferimento passa dalla colonna dei test a quella dello stato; il test completo resta di M3.s.
- Sette righe nominavano test diversi da quelli di `tests/trace/M2.tsv` (F-CAL-06.a, NFR-CPU-01.a, ADR-09.a, ADR-16.a, ADR-17.a, C-01.a, D-17.a). Ognuna ora nomina l'unione delle due, tutta passata in entrambe le esecuzioni.

### Clausola 3 (§21.4, riga 2675): test che la provano

| Parte | Test (passati in dev e release) |
|---|---|
| Slice FITS, `L − D` con dark matched | T-M2E-01, 02, 03; `CalibM2E.T_M2E_02_nondyadic_cancellation_even_and_odd_n` |
| `Q = Fc/N(Fc)` | T-M2E-04…07 (`CalibM2E.T_M2E_04_05_*`, `CalibM2E.T_M2E_04_var_q_overflow_*`) |
| Maschere | T-M2E-04, 05, 24; T-M2A-01…13 (ADR-03); T-M2R5-06 (bit identici fra i percorsi) |
| Metadati | T-M2A-03 (card conservate in ordine, non interpretate elencate), T-M2A-07 (pattern CFA e offset); T-M2R1-07 (keyword non interpretate in report e provenance) |
| Incertezza | T-M2E-21…26 (`T-M2E-21-26.monte_carlo`), `MasterPipeline.T_M2E_20_*`, `T-M2E-21.product_masters_monte_carlo`, T-M2R2-04 |
| Report | T-M2E-12, 26; T-M2R1-08; T-M2R4-03, 06; T-M2R2-06, 08 |
| DAG durevole | T-M2B-01…13; T-M2C-01…25 |
| Recovery | T-M2C-02, 03, 10, 12; T-M2R4-01…04 (SIGKILL a ogni punto, perdita di energia simulata) |
| Cancel | T-M2D-08; T-M2R3-07 (attesa e annullamento sotto pressione PSI prima dell'ammissione) |
| Worker | T-M2D-04, 07, 15; T-M2R3-09 (worker su insieme di CPU); T-M2R3-06 (checkpoint e resume nel job) |
| Dato astronomico reale | T-M2E-13 (PUB-LFC-01, calibrazione), T-M2R2-09 (master del prodotto su PUB-LFC-01), T-M2R1-09 (campioni pubblici PUB-LFC-01 e PUB-CFA-01) |

Nessuna parte della clausola resta senza test.

### Riesecuzione sul commit del gate

Coordinatore, 30 ottobre 2026, sul commit `cb35d89` (`git status` pulito, salvo `data`), 6 job per build e test (nessun altro carico, sotto il tetto del 70%), `ulimit -c 0`:

1. `rm -rf build/dev build/release`; `tools/check.sh` (log `tmp/gate-cb-check.log`): uscita 0, `lint: ok`, dev fast 494/494.
2. `ctest --preset dev-full -j6 --output-junit tmp/dev-junit.xml` (log `tmp/gate-cb-dev.log`): 501/501, 1685,4 s.
3. `cmake --preset release`, `cmake --build --preset release -j6`, `ctest --preset release -j6 --output-junit tmp/release-junit.xml` (log `tmp/gate-cb-rel.log`): 500/500, 131,2 s.
4. `python3 -S tools/check_m2_trace.py --run build/dev:tmp/dev-junit.xml --run build/release:tmp/release-junit.xml` (`tmp/gate-cb-trace.txt`): **uscita 0**, `check_m2_trace: 63 ID nello stadio M2, 0 con problemi, 0 problemi di esecuzione`.

Il blocco generato sopra riporta i conteggi e gli hash di questa esecuzione.

### Esito e limiti

- Esito: clausole 1-3 soddisfatte sul commit `cb35d89`; la review del giro 8 è FAIL (sezione «Giro 9»), quindi il commit del gate passa a `463ccf9`.
- La prova preliminare era sul working tree (revisione sporca in `source_revision.cpp`); la riesecuzione sul commit `cb35d89` ha working tree pulito, salvo il collegamento simbolico `data` non versionato.
- Release e dev non sono simultanee, ciascuna sui binari della propria build: nella prova preliminare release prima di `tools/check.sh` e dev dopo; nella riesecuzione sul commit `tools/check.sh`, poi dev, poi release.
- Limiti degli ID, nella colonna di stato della matrice: F-IO-01 (RGB e multi-plane solo sintetici), F-IO-05 (HDU compressi rifiutati), F-RES-07.a (ENOSPC sulla cache delle miniature non provato), NFR-SEC-03 (inode degli originali non confrontati nella cache), NFR-PERF-01 e F-RES-01 (soglie misurate su un solo PC), F-PRJ-06 (T-PRJ-01 completo in M3.s).
- Autore e data: agente general-purpose su Claude Opus 5.5, 30 ottobre 2026. Review indipendente: da fare.

## Clausola del gate e record

Il gate non ripete le prove delle unità: le richiama. Ogni riga rimanda al record dove sono comando, ambiente, esito e limiti.

| Elemento della clausola | Prova | Record |
|---|---|---|
| Slice FITS: L−D, Q, maschere, metadati, incertezza, report | T-M2E-01…32, `CalibM2E.*`, `CalibJob.*`, `CalibStats.*` | `docs/evidence/M2-e.md` |
| FITS tipizzato, immagine, stop su input non conforme | T-M2A-01…13, `Fits.T_M2A_short_read_with_unchanged_size_stops_as_io` | `docs/evidence/M2-a.md` |
| Hash, nodo DAG, slot, provenance | T-M2B-01…13 | `docs/evidence/M2-b.md` |
| DAG durevole, recovery, snapshot, export, originali intatti | T-M2C-01…25 (T-M2C-11 esteso all'intera calibrazione) | `docs/evidence/M2-c.md` |
| Cancel, worker, budget, checkpoint/resume, spill | T-M2D-01…15 | `docs/evidence/M2-d.md` |
| Dato astronomico reale con licenza verificata | T-M2E-13 su PUB-LFC-01 (Palomar LFC, CC BY 4.0) | `docs/evidence/M2-e.md`, `docs/datasets/PUB-LFC-01.md` |
| Nessun fallback, nessuna rete | T-FBK-01 (parte M2), T-M2F-02 | `docs/evidence/M2-e.md`, `docs/evidence/M2-f.md` |
| Contratti minimi, build, fixture | T-M2F-01 | `docs/evidence/M2-g.md`, `docs/evidence/M2-f.md` |
| Soglie F-RES-01 con carichi reali | misura di questo record | sezione "Misura F-RES-01" |

Lo stato di ogni ID (pianificato o eseguito) è in `docs/test-matrix.md`.

## Correzioni nate dalle review del gate

Artefatti in `.omc/artifacts/ask/codex-m2-gate-review{1,2,3,4}-*.md` (Codex `gpt-6-astra`, reasoning high, sola lettura). Le revisioni sono statiche: i giri non hanno compilato né eseguito i test. Le correzioni sono nel working tree sopra `c788a17` (`git diff HEAD`), non committate.

### Giro 1: FAIL (4 MED, 3 LOW)

| Rilievo | Correzione e dove | Test |
|---|---|---|
| NaN nella mediana delle varianze (`nth_element` su un ordinamento non valido) | Ogni termine di varianza è validato prima dell'inserimento; stop `calib.variance_overflow` (`src/core/calibration.cpp`) | `CalibM2E.T_M2E_29_nonfinite_variance_term_stops_before_the_median` |
| Provenance restituita solo in memoria | `run_job_files` scrive la provenance con il protocollo atomico: nel commit per il successo, come artefatto `provenance` del nodo per arresto e cancel (`src/core/job.cpp`) | `CalibJob.T_M2B_10_provenance_is_persisted_for_success_stop_and_cancel_and_survives_reopen` |
| Soglie F-RES-01 non misurate | Misura con carichi reali, frazione normale 0.50 → 0.375 (`src/core/budget.cpp`, `budget.hpp`, ADR-06) | `Budget.T_M2D_03_*` aggiornati ai nuovi valori; misura in questo record |
| T-M2C-11 limitato allo store | Esteso ai quattro input dell'intera calibrazione (digest, permessi, inode, mtime) | `CalibJob.T_M2C_11_the_four_inputs_are_untouched_by_the_whole_calibration_workflow` |
| LOW: `short_read` e conteggi non aggiornati | `M2-a.md`, `test-matrix.md`, `progress.md` allineati | `Fits.T_M2A_short_read_with_unchanged_size_stops_as_io` (punto `input.pread`) |
| LOW: `names_errno` non asseriva `EIO` | Asserzione su `errno` nel contesto dell'errore | `Export.T_M2A_readdir_failure_while_verifying_stops_as_io_and_names_errno` |
| LOW: `DIR*` perso se il callback lancia | `DIR*` sotto RAII (`src/core/sqlite_internal.cpp`) | nessun test dedicato: il ramo passa dal callback `dir.readdir` del test sopra |

### Giro 2: FAIL (2 MED, 2 LOW)

| Rilievo | Correzione e dove | Test |
|---|---|---|
| Su `dag.stale_publish` provenance `completed` e poi `failed` per lo stesso job | Generazione superata: nessun record `completed`, un solo record `failed` `dag.stale_publish` (`job.cpp`) | `Review.P1_stale_generation_during_publication_leaves_one_failed_provenance_after_reopen` |
| `run_job` su store durevole teneva la provenance solo in memoria | `run_job` con `sqlite_dag_store` si ferma prima del lavoro (`invalid_input`, `job.durable_store_requires_run_job_files`); la provenance durevole la scrive solo `run_job_files` | `Review.P2_completed_and_stopped_jobs_on_a_durable_store_persist_the_returned_provenance`, `Review.P3_run_job_on_a_durable_store_stops_before_the_work` (dopo R5.d registra lo stop come job `failed`) |
| LOW: limite di T-M2C-11 ancora in `M2-c.md` | Limite e tabella aggiornati | vedi giro 1 |
| LOW: riferimenti a comandi e log delle nuove prove | Aggiunti in `M2-a.md` e `M2-c.md`; la mutazione non è documentata nei log, quindi la dichiarazione è "dedotta da lettura del codice" | nessuno |

### Giro 3: FAIL (3 MED, 2 LOW)

| Rilievo | Correzione e dove | Test |
|---|---|---|
| Provenance discorde dopo commit stale seguito da un errore (`commit.after_commit`) | La provenance rispecchia sempre stato e causa già registrati nel job, riletti dallo store (`settle`, `persisted_cause`); l'errore successivo va in `job_outcome::subsequent_error` | `Review.P5_error_after_a_stale_commit_does_not_replace_the_persisted_cause` |
| `run_branches` senza percorso durevole | Overload `run_branches(sqlite_dag_store&, std::vector<branch_files_job>, ...)`: ogni ramo è un `run_job_files`, un record di provenance per job (`scheduler.hpp`, `scheduler.cpp`) | `Review.P7_branches_on_a_durable_store_keep_order_blocking_and_one_provenance_per_job`, `Review.P7_cancel_on_a_durable_store_stops_later_branches_and_persists_one_cancelled_provenance` |
| Eccezione non `stop_error` senza provenance | Job `failed` `job.unexpected_exception` (categoria `integrity`), provenance persistita, eccezione originale rilanciata invariata | `Review.P6_foreign_exception_persists_a_coherent_provenance_and_propagates_unchanged` |
| LOW: righe 47-48 della matrice dichiaravano mutazioni fallite | Distinte deduzione statica e prova eseguita | nessuno |
| LOW: conteggi 318/320 contro 322/324 | Allineati | nessuno |

Il test `Review.P4_failed_stop_provenance_write_is_declared_in_the_outcome_with_the_original_cause_primary` copre `job_outcome::provenance_not_persisted` (scrittura della provenance d'arresto fallita: la causa primaria resta, il guasto è dichiarato nell'esito e su stderr). L'assegnazione al giro 2 o 3 non è ricostruibile dai rilievi: la riporto qui senza attribuirla.

### Giro 4: FAIL (2 MED, 2 LOW)

| Rilievo | Correzione e dove | Test |
|---|---|---|
| Gestione secondaria che poteva mascherare l'eccezione originale (`settle()` fuori dal `try`, catch limitato a `stop_error`) | Tutta la gestione secondaria è protetta; il guasto è diagnosticato su stderr (ammesso da ADR-05:90) e si rilancia sempre l'eccezione originale (`job.cpp`) | `Review.P8_secondary_non_stop_exception_never_masks_the_original`, `Review.P8_non_stop_exception_writing_the_stop_provenance_is_declared_in_the_outcome` |
| Ramo pressione non provato sul segnale reale | S4 ter e S4 quater (sezioni seguenti): il ramo scatta sul segnale reale (`psi_some_avg10` 17,7 e 14,4, frazione 0.25); il criterio sul job fallisce in entrambe (full 1,05): 0.25 e 10.0 restano non confermati | misura, non test di unità |
| LOW: tolleranza "senza swap" non dichiarata | Commento di `budget.hpp` e ADR-06: entro 16 MiB di swap oltre il fondo, sul PC misurato (primo PASS a 0.375: 10,38 MiB) | nessuno |
| LOW: conteggi superati | Conteggi dai log del giro 5: dev 328/328, release 330/330 | nessuno |

### Giro 5: FAIL (2 MED, 2 LOW)

Artefatto: `.omc/artifacts/ask/codex-m2-gate-review5-20261029-1343.md`.

| Rilievo | Correzione e dove | Test |
|---|---|---|
| `describe()` `noexcept` allocava una `std::string`: un `bad_alloc` nella diagnosi secondaria portava a `std::terminate` | Diagnosi senza allocazioni (`job.cpp`) | regressione con `operator new` che fallisce a comando (vedi giro 6) |
| Soglie: S4 ter non conforme al criterio preregistrato | S4 quater: FAIL, poi rivelato difetto dell'harness; S4 quinquies: PASS (giro 6) | misura |
| LOW: in `M2-gate.md` i due percorsi della provenance non scrivibile erano confusi | Tabella dei limiti: percorso d'arresto (campo dell'esito) distinto da quello dell'eccezione estranea (solo stderr) | nessuno |
| LOW: il tetto del 70% in `tools/check.sh` non era imposto | `CMAKE_BUILD_PARALLEL_LEVEL` parte dal tetto e un valore oltre il tetto ferma lo script (uscita 2); `CTEST_PARALLEL_LEVEL`, se impostato, è validato allo stesso modo; senza, ctest resta seriale | provato a mano con 9 combinazioni |

### Giro 6

Nuovo test: `JobAlloc.secondary_failure_with_exhausted_memory_still_rethrows_the_original_exception` (`tests/unit/test_job_alloc.cpp`, eseguibile separato con `operator new` globale che fallisce a comando). L'autore dichiara la mutazione (una `std::string` reintrodotta in `what_of` sotto `noexcept`: SIGABRT, rc 134); non c'è un log conservato. Build: dev 329/329, release 331/331 (328 fast + 3 slow), zero warning (`tmp/gate6-dev.log`, `tmp/gate6-rel.log`). Review: FAIL (`.omc/artifacts/ask/codex-m2-gate-review6-20261029-1404.md`), 3 MED e 1 LOW:

| Rilievo | Correzione e dove | Test |
|---|---|---|
| `stopped()`: con un arresto originario e memoria esaurita, la costruzione dell'esito poteva lanciare `bad_alloc` e mascherare l'arresto | Tutta la costruzione dell'esito è protetta; in caso di guasto diagnosi senza allocazioni e `std::rethrow_exception(original)` (`job.cpp`) | `JobAlloc.secondary_failure_with_exhausted_memory_still_returns_the_original_stop`; mutazione con log in `tmp/mut-stopped.log` |
| Harness: la finestra del job partiva dal campione precedente del watchdog, quindi il full 1,05 di S4 ter e S4 quater poteva precedere il job | `begin_window_with(campione fresco)` letto dopo l'avvio del job; S0, S1 e S3 non erano affetti (finestra aperta dopo la baseline, a riposo) | S4 quinquies |
| Soglie: serve una prova conforme | S4 quinquies preregistrata e superata (sezione seguente) | misura |
| LOW: progress e matrice non distinguevano soglie misurate e confermate | Allineati | nessuno |

Altro nel giro: i death test (`test_error`, `test_image`) generavano report di crash del sistema a ogni `ctest`; ora l'abort voluto termina con `_exit(134)` (`tests/unit/no_crash_report.hpp`). Verifica: nessuna invocazione di apport in `/var/log/apport.log` durante la build del giro 7.

### Giro 7

Build: dev 330/330, release 332/332 (329 fast + 3 slow), zero warning (`tmp/gate7-dev.log`, `tmp/gate7-rel.log`). Review: **PASS**, 4 LOW (`.omc/artifacts/ask/codex-m2-gate-review7-20261029-1420.md`): l'harness accettava il ritorno della PSI entro 120 s invece dei 60 preregistrati (la prova registrata, 29,9 s, è conforme; l'harness resta fuori da Git); intestazione e riferimenti aggiornati; attribuzione dei vecchi massimi distinta dal difetto accertato; prossimo comando di `plans/M2.md` aggiornato.

## Correzioni dopo la review del giro 8

### R6.c: ogni membro di un intervallo deve avere un test

Rilievo MED (`.omc/artifacts/ask/codex-m2gate-round8-20261030-0347.md`, punto 2): `tools/check_m2_trace.py` teneva dei token di un intervallo solo i membri con un test e dava errore solo a intervallo interamente vuoto, quindi `T-M2R2-01…10` passava con `01…09`.

- Strumento: `expand_range` segnala ogni membro senza test in `ctest -N` (`intervallo X: membri senza test in ctest -N: <membri>`); l'intervallo invertito resta un errore. Test in `tests/tools/test_check_m2_trace.py`: membro mancante, intervallo completo, più membri mancanti, intervallo interamente mancante, invertito; scritti prima della correzione e rossi (2 errori, 1 fallimento).
- Stato reale: il controllo severo ha trovato un solo buco, T-M2E-05, senza test proprio (la prova è in `CalibM2E.T_M2E_04_05_q_bits_separate_and_together`). Celle riscritte in `docs/test-matrix.md`: F-CAL-03.a, NFR-CPU-01.a, ADR-09.a, C-01.a (`T-M2E-01…04, 06…32` o `T-M2E-04, 06, 07` più il nome completo). T-M2E-20 ha un test proprio (`MasterPipeline.T_M2E_20_*`) e resta nell'intervallo. `tests/trace/M2.tsv` già elencava membri e nome completo: invariato.
- Prove: `python3 -S tools/check_m2_trace.py --run build/dev:tmp/dev-junit.xml --run build/release:tmp/release-junit.xml` sulle build e i junit dell'esecuzione finale: uscita 0, 63 ID, 0 con problemi. 10 mutazioni manuali sul controllo, tutte rilevate (`tmp/r6c-mutations.log`). `tools/check.sh` dev: uscita 0, 494 test fast. Review: `r6c-round1` PASS senza rilievi (`.omc/artifacts/ask/codex-r6c-round1-20261030-0404.md`).
- I blocchi generati di `M2-gate.md` e `M3-entry-ADR{03,08,16}b.md` sono rigenerati (cambiano gli hash di matrice e strumento). Limite: la riesecuzione completa di ctest sul commit non è rifatta per questa correzione; i junit sono quelli dell'esecuzione finale `cb35d89`.

## Misura F-RES-01

Fonti: `tmp/fres01/plan2.md` (criteri del giro 2 e correzioni S4 bis e S4 ter, con gli orari di scrittura, fissati prima di ogni giro), `tmp/fres01/README.md` (ambiente e comandi), `tmp/fres01/results{,2,3,4}.jsonl` e `dry*.jsonl`.

### Ambiente

- Linux 7.0.0-38-generic x86_64, g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0, 16 core, `MemTotal` 31 591 684 kB (31 GiB), `SwapTotal` 91 644 344 kB (circa 87 GiB) con circa 12 GiB già usati da altri processi, `vm.swappiness` 120 (`cat /proc/sys/vm/swappiness`).
- PC condiviso (browser aperti): `MemAvailable` iniziale tra 18,9 e 22,3 GiB nei giri. Limite del proprietario: 70% di CPU e RAM; l'harness usa 4 thread di carico e 1 thread per il job, `nice`, RSS massimo del processo di prova 22 GiB (guard).
- Commit di partenza `c788a171d60eeb57fe0958539f4fc260c5098042`, working tree con modifiche non committate; libreria `build/release/src/core/libastrocore.a` compilata il 29 ottobre 2026 alle 12:35:49.
- Compilazione dell'harness (dalla radice del progetto): `export TMPDIR=$PWD/tmp; nice -n 10 g++ -std=c++20 -O2 -Wall -Wextra -pthread -Isrc/core/include tmp/fres01/fres.cpp build/release/src/core/libastrocore.a $(pkg-config --libs cfitsio sqlite3 openssl) -o tmp/fres01/fres`. Esecuzione: `cd tmp/fres01 && ./fres dry`, `nice -n 5 ./fres all`, `./fres one <scenario> <rep>`.
- Watchdog (campionamento 200 ms): memory some avg10 > 25, full avg10 > 5, `pswpout` > +64 MiB dall'inizio della finestra, `MemAvailable` < 3 GiB, RSS dell'harness > 22 GiB, con SIGKILL ai figli ed esito ABORT. Baseline di 30 s prima di ogni scenario, pausa di 30 s fra scenari.
- Criteri PASS (identici in tutti i giri): job completato; `pswpout` oltre il fondo ≤ 16 MiB; memory full avg10 massimo del job < 1.0; `MemAvailable` minima ≥ 3 GiB.

### Giro 1 (`results.jsonl`, frazione 0.50)

Prima del giro il requisito era: la frazione normale regge una crescita concorrente dopo il preflight. S1a = 0.25·`avail0`, S1b = 0.40·`avail0`.

| Scenario | Rep | `avail0` (GiB) | Swap oltre il fondo (MiB) | PSI some/full max | `MemAvailable` min (MiB) | Esito |
|---|---|---|---|---|---|---|
| S0 (nessun carico) | 1 | 17,6 | 0,02 | 0 / 0 | 9020 | PASS |
| S0 | 2 | 18,0 | 9,83 | 0 / 0 | 8905 | PASS |
| S0 | 3 | 17,9 | 0 | 0 / 0 | 8926 | PASS |
| S1a | 1 | 19,3 | 16,54 | 0,18 / 0,18 | 5475 | **FAIL** (16,5 MiB > 16) |
| S1a | 2 | 20,0 | 7,85 | 0,18 / 0,18 | 5040 | PASS |
| S1a | 3 | 19,9 | 0,01 | 0 / 0 | 5226 | PASS |
| S1b | 1 | 20,8 | 73,23 | 0 / 0 | 3278 | **ABORT** (swap +71,6 MiB > 64) |

Con `vm.swappiness` 120 il kernel scrive swap prima che `MemAvailable` si esaurisca e prima che la PSI salga: a 0.50 la protezione non può venire dalla PSI. Il file `tmp/fres01/results.before-fix.jsonl` (6 righe, 12:52) è una copia precedente dello stesso file: non è citato nei risultati.

### Giro 2, S3 (`results2.jsonl`): frazione corretta

Criteri fissati alle 12:57, prima della misura: crescita concorrente 0.40·`avail0` dopo il preflight (come S1b), f ∈ {0.375, 0.25}, 3 ripetizioni ciascuna, si sceglie la f più grande con 3 PASS su 3.

| f | Rep | `avail0` (GiB) | Budget (MiB) | Swap oltre il fondo (MiB) | PSI some max | `MemAvailable` min (MiB) | Esito |
|---|---|---|---|---|---|---|---|
| 0.375 | 1 | 20,0 | 7669 | 10,38 | 0 | 4551 | PASS |
| 0.375 | 2 | 20,3 | 7810 | 0,05 | 0 | 4501 | PASS |
| 0.375 | 3 | 20,4 | 7850 | 0 | 0 | 4375 | PASS |
| 0.25 | 1 | 20,4 | 5219 | 0 | 0 | 7153 | PASS |
| 0.25 | 2 | 20,7 | 5303 | 0 | 0 | 7281 | PASS |
| 0.25 | 3 | 20,6 | 5274 | 0 | 0 | 6890 | PASS |

Entrambe le frazioni passano 3 su 3: si sceglie 0.375.

### Giro 2, S4: ramo sotto pressione sul segnale reale

Disegno (`plan2.md`): pressione confinata in uno scope utente transitorio (`systemd-run --user --scope -p MemoryHigh=1G -p MemorySwapMax=0`) con un processo che tocca 1,5 GiB anonimi; lo stallo resta nel cgroup ma entra nella PSI di sistema, che il prodotto legge da `/proc/pressure/memory`. La soglia 10.0 non si conferma come valore ottimo: la prova mostra solo che il ramo scatta sul segnale reale.

| Prova | Esito | Causa |
|---|---|---|
| S4 | ABORT del watchdog: full avg10 14,72 > 5, `MemAvailable` 19,8 GiB | Il guard misurava lo scope: a PC scarico l'unico task in stallo rende "full" globale pari a "some". Difetto dell'harness. |
| S4 bis (`results3.jsonl`) | **Incidente dell'harness, nessuna prova** | Il watchdog si è bloccato (`waitpid` sotto mutex, lettura di `/proc/<pid>/status` del processo in stallo): PSI memory di sistema attorno a 70 per circa 280 s (some max 74,08, full max 70,04, durata 324,6 s); processo ucciso a mano. `MemAvailable` minima 19,9 GiB, swap 0: il PC non è stato messo in difficoltà, ma la prova non dimostra nulla. |
| S4 ter (`results4.jsonl`) | Una sola esecuzione, vedi sotto | |

### S4 ter: harness corretto, una sola esecuzione (approvata dal proprietario)

Correzioni obbligatorie (13:30, prima del rilancio): il watchdog non tocca mai il processo in stallo; legge solo `/proc/pressure/memory`, `/proc/meminfo`, `/proc/vmstat` e `memory.current` e `memory.events` del cgroup dello scope; uccide lo scope scrivendo `1` in `cgroup.kill`. Il campionatore gira in un thread proprio, senza lock condivisi con l'orchestratore. Prova a secco (`dry4.jsonl`): hog bloccato con SIGSTOP e ucciso via `cgroup.kill` in 0,010 s; guard di durata a 1 s scattato a 1,05 s con scope chiuso in 0,81 s. Rete di sicurezza esterna a 45 s: mai intervenuta.

| Grandezza | Valore |
|---|---|
| Snapshot reale di `compute_automatic_budget()` | `psi_some_avg10` 17,7, `fraction` 0.25, budget 5 129 633 792 byte (`mem_available_bytes` 20 522 266 624) |
| Durata dello scope, chiuso dall'orchestratore | 2,71 s (non da un guard) |
| Scope dopo la chiusura | `inactive`, `memory.current` 0 |
| PSI some < 1 dopo la chiusura | 29,9 s |
| Job col budget 0.25 | completato (22,5 s), swap oltre il fondo 0 |
| `MemAvailable` minima (intera prova) | 15 671 MiB (circa 15,3 GiB) |
| Fase dello scope | some max 17,7, full max 17,34, `MemAvailable` min 19 572 MiB |
| Fase del job | some avg10 max 1,07, full avg10 max 1,05 |

**Esito formale: FAIL per il criterio "job: memory full avg10 max < 1.0"** (1,05): la media a 10 s era ancora in discesa dalla fase dello scope, perché il job parte appena some < 1. In più, l'harness classifica la prova come **ABORT** (`memory full avg10=17.34 > 5`): il guard globale full > 5 era rimasto attivo, S4 bis lo aveva sostituito ma S4 ter non lo ripeteva, quindi la preregistrazione era ambigua. Nessun guard ha ucciso nulla: lo scope è stato chiuso dall'orchestratore. La prova non è stata ripetuta.

**Deviazione dichiarata**: lo snapshot è preso subito dopo `cgroup.kill` invece che prima, come in `plan2.md`. La PSI avg10 decade lentamente, quindi lo snapshot dopo il kill vede ancora la pressione (17,7) ed è più sicuro per il PC.

Cosa la prova mostra e cosa no: il ramo di pressione scatta sul segnale reale (frazione 0.25 con PSI 17,7) e il job a 0.25 completa senza swap. Non mostra che 10.0 sia la soglia ottima, né che il criterio sul full del job regga dopo una pressione appena cessata. La spiegazione "media ancora in discesa" è smentita da S4 quater, dove il job parte a PSI già a fondo.

### S4 quinquies: finestra del job corretta, una sola esecuzione

Preregistrata in `plan2.md` (sezione "S4 quinquies", 14:08). Procedura e criteri identici a S4 quater; la finestra del job si apre con un campione fresco letto dopo l'avvio del job. Risultato: `results6.jsonl`.

| Voce | Valore |
|---|---|
| Snapshot reale | `psi_some_avg10` 17,6, frazione 0.25, budget 5 413 797 888 byte |
| Fase dello scope | 3,96 s, chiusa dall'orchestratore con `cgroup.kill`; some max 17,6, full max 16,73; `MemAvailable` min 20 653 MiB; swap 0 |
| Ritorno della PSI | some e full avg10 < 1 dopo 29,9 s |
| Fase del job | primo campione full avg10 0,83; completato in 22,8 s; swap oltre il fondo 0; `MemAvailable` min 16 554 MiB; some max 0,87, full max 0,83 |

**Esito: PASS.** Il ramo sotto pressione scatta sul segnale reale e il job con il budget 0.25 rispetta tutti i criteri. Il difetto dell'harness è accertato (in S4 ter e S4 quater la finestra del job partiva da un campione precedente all'avvio); che proprio quel campione sia l'origine del full 1,05 è probabile ma non dimostrato: i dati di quelle esecuzioni non registrano il primo campione.

### S4 quater: correzioni di procedura, una sola esecuzione

Preregistrata in `plan2.md` (sezione "S4 quater", 13:52) prima dell'esecuzione: il job parte solo a some e full avg10 < 1; nella fase dello scope valgono solo i guard dello scope; criteri PASS identici a S4 ter. Rete di sicurezza esterna a 45 s, mai intervenuta. Risultato: `results5.jsonl`.

| Voce | Valore |
|---|---|
| Snapshot reale | `psi_some_avg10` 14,39, frazione 0.25, budget 4 852 809 728 byte |
| Fase dello scope | 5,9 s, chiusa dall'orchestratore con `cgroup.kill`; some max 14,39, full max 14,12; `MemAvailable` min 18 296 MiB; swap 0 |
| Ritorno della PSI | some e full avg10 < 1 dopo 28,0 s |
| Fase del job | completato in 22,4 s; swap oltre il fondo 0; `MemAvailable` min 15 389 MiB; some avg10 max 1,06, **full avg10 max 1,05** |

**Esito: FAIL** per il criterio "job: memory full avg10 max < 1.0" (1,05). L'attribuzione dello stallo al job, scritta qui in un primo momento, non è dimostrata: la finestra partiva da un campione precedente all'avvio del job (difetto accertato), e S4 quinquies, con la finestra corretta, ha full 0,83.

### Decisione

Casa del valore: `docs/contracts/ADR-06-budget-scheduler.md`, formula F-RES-01 (righe 27-31). Frazione normale **0.50 → 0.375**, confermata da S3. Frazione sotto pressione 0.25 e soglia PSI 10.0 invariate, confermate da S4 quinquies. Il requisito è dichiarato nell'ADR: dopo il preflight un altro processo può crescere fino a 0.40·`avail` con al massimo 16 MiB di swap oltre il fondo, memory full avg10 < 1 e almeno 3 GiB di `MemAvailable`.

Perimetro: questo PC (16 core, 31 GiB, swappiness 120, swap con altri processi). Su altre macchine la misura va ripetuta.

### Harness e hash

L'harness non è codice di prodotto: resta in `tmp/fres01/`, fuori da Git. Hash SHA-256 (`sha256sum`):

| File | SHA-256 |
|---|---|
| `fres.cpp` (versione finale, con S4v) | `f0797b56533db47683f77670f6063f07904ae082dc0debb8162a3419fe56bda6` |
| `fres.round5.cpp` (con S4q, prima di S4v) | `0a1464897749467eef8ffbe702cd5dc1d96701a009b7827dc4df7de83ba6c00b` |
| `fres.round4.cpp` (con S4t, prima di S4q) | `16d09fb453ad7696140bef6bda28a85dbecbd88d5e651df8eeed52acb69adad2` |
| `fres.round1.cpp` | `3ab49d5ae021211d7329bed37e7a1fbb20a736618824448e8ffdf5c0b7f5921d` |
| `fres.round2.cpp` | `f501b6e9707d6899ab8ddb2056043ae2e5eb0033ac1704f05ca95a65d02caa67` |
| `fres.round3.cpp` | `a1e4bbe9b23d6830a097fd6d7363a6dba090dc813a9e441bec6fffee09d713a2` |
| `results.jsonl` | `49c93e1d057e449654b4b662e2b47986acb2cd463d005abb0334fb474816f7a5` |
| `results.before-fix.jsonl` | `7c2e4c34a83f3250c27f4d8703a7ec1fe766a5da726e6d21e9fa8469d4609d02` |
| `results2.jsonl` | `caf5be175ce1358a31996dbfbad5d81f697439a8a264b97756186e428ac2a76f` |
| `results3.jsonl` | `5a3029ba045ed0e7dec7943ca56c3ec1940a6e1a060718ded06ee0182a5ce31f` |
| `results4.jsonl` | `3622f860a855272076c90e04adf158a3ceb4a3c94d38b71f1b5943963737cd83` |
| `results6.jsonl` | `ac2ba0427d37f7cc8fa5597130f06e1fab8349ca3dfdf3168150c8a3e8877bbc` |
| `results5.jsonl` | `d8aa555722c766b05afe30db24f027552a98083276fd2137f9e87bcf558d09a4` |
| `dry.jsonl` | `feaed71c0cb12121026cbcc59b66200cb6e2c138290c362066563a1948a83ea4` |
| `dry2.jsonl` | `f4598e2e8a77f490a7e3bc64a437c45f1df90100178b4709b1278d55755f3c74` |
| `dry4.jsonl` | `bc735962b9aaf11a248df6b2275832f1feac3589c8b3fefe99c3317e2329cadf` |

### Limiti della misura

- Campione piccolo: 3 ripetizioni per frazione, un solo PC, carico di fondo non controllato (browser aperti, circa 12 GiB di swap già in uso).
- Il primo PASS a 0.375 ha usato 10,38 MiB di swap oltre il fondo: "senza swap" significa "entro 16 MiB oltre il fondo", non zero.
- S4 è passata una sola volta (S4 quinquies), dopo tre esecuzioni invalidate da difetti dell'harness (S4 bis, S4 ter, S4 quater): una ripetizione, non tre. La soglia 10.0 è confermata come soglia che scatta sul segnale reale, non come valore ottimo.
- 0.25 regge con margine maggiore (swap 0, `MemAvailable` min oltre 6,8 GiB) ma lascia inutilizzata più memoria: 0.375 è il valore più alto fra quelli provati che rispetta il requisito, non un massimo dimostrato.
- La soglia PSI 10.0 non è ottimizzata. Sul PC misurato lo swap arriva prima della PSI (S1b: +73 MiB a PSI 0): a proteggere il sistema è la frazione normale.

## Build e test

- Dev (ASan + UBSan): `rm -rf build/dev build/release && tools/check.sh > tmp/gate5-dev.log 2>&1`. `tools/check.sh` limita la parallelizzazione di build e test al 70% delle CPU (`CMAKE_BUILD_PARALLEL_LEVEL`, 11 su 16 core). Esito: dev 328/328, zero warning.
- Release: `CMAKE_BUILD_PARALLEL_LEVEL=11 cmake --preset release && CMAKE_BUILD_PARALLEL_LEVEL=11 cmake --build --preset release && ctest --preset release > tmp/gate5-rel.log 2>&1`. Esito: release 330/330 (327 fast + 3 slow), zero warning (comprende i tre test `slow`: T-M2D-13, T-M2E-13, T-M2E-21/26).
- Il preset `dev` filtra `-L fast`; `ctest --preset release` esegue anche gli `slow`.
- Ambiente: Linux 7.0.0-38-generic x86_64, 16 CPU, GCC 15.2.0, SQLite 3.46.1, OpenSSL 3.5.5.
- Giri precedenti, dai rilievi di Codex: 318/320 (giro 2), 322/324 (giro 3), 326/328 (giro 4).

## Limiti residui accettati per M2

| Limite | Passa a |
|---|---|
| Due percorsi distinti. Arresto (`stop_error`): una provenance non scrivibile è dichiarata in `job_outcome::provenance_not_persisted` e su stderr. Eccezione estranea: `unexpected()` rilancia l'eccezione originale e non restituisce un esito, quindi una provenance non scrivibile è diagnosticata solo su stderr (ADR-05:90) | M3 |
| Nessun test per `settle()` fallito: manca un punto di iniezione; il ramo è coperto solo da lettura del codice | M3 (serve un punto di iniezione) |
| Soglie F-RES-01 misurate su un solo PC (0.375 con 3 ripetizioni, 0.25/10.0 con una sola esecuzione valida); 10.0 non ottimizzata | Ripetere su altre macchine prima di una release |
| Checkpoint/resume non collegato alla calibrazione (`checkpoint_every` 0); master non combinati dal prodotto; overscan non ritagliato; stima di N non valida su flat vignettato (nessun gate); T-M2E-13 e T-M2E-21/26 non eseguiti sotto ASan/UBSan (`M2-e.md`) | M3 (M3.c per dark scalato e combinazioni) |
| Costo per task con 1 worker, cancel cooperativo, resume dei task dopo l'ultimo checkpoint, page cache dello spill fuori dal budget, TSan non nei preset (`M2-d.md`) | Invariati |
| Durabilità contro perdita di corrente e processo malevolo con lo stesso UID: fuori dal modello (`M2-c.md`) | Invariato |

Chiuso dal gate: `run_branches` ha ora un percorso durevole (overload per `sqlite_dag_store`); le soglie F-RES-01 sono misurate sul PC di sviluppo.
