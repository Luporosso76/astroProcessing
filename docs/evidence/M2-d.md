# M2.d: budget, pool, scheduler, cancel, checkpoint/resume e spill

Stato: **DONE**, 29 ottobre 2026. Autore: agente executor su Claude Sonnet 5.5. Review indipendente: Codex `gpt-6-astra` (reasoning high, sola lettura), giri 1-3 FAIL con correzioni, giro 4 PASS. Contratto: `docs/contracts/ADR-06-budget-scheduler.md`; piano: `plans/M2.md` (M2.d, F-RES-01, F-RES-02, F-PRJ-05).

## Comando ed esito

- Dev (ASan + UBSan): `rm -rf build && tools/check.sh > tmp/check-m2d.log 2>&1`. Esito: 268/268 (`tmp/check-m2d.log`, riga "100% tests passed, 0 tests failed out of 268"). Il preset `dev` filtra `-L fast`: T-M2D-13 non gira in dev. I 268 comprendono `BuildInfo.SanitizersAreActiveInDevBuild`, assente in release.
- Release: `cmake --preset release && cmake --build --preset release && ctest --preset release > tmp/check-m2d-rel.log 2>&1`. Esito: 268/268 (`tmp/check-m2d-rel.log`, 267 `fast` + 1 `slow`; tempo reale 126,6 s, di cui 106,7 s per `T-M2D-13.m1d_protocol`). `ctest --preset release` non filtra per etichetta: esegue anche lo `slow`.
- I test di M2.d sono 79 casi CTest in più rispetto a M2.c (189 release -> 268): 48 con ID T-M2D, 2 con ID T-M2B-09/12, 29 senza ID.

## Revisione e ambiente

- Revisione git: `eb1f761edc5f1840cf21a6501520febb86bbca45` (M2.c). I test sono stati eseguiti sul working tree poi entrato nel commit di chiusura di M2.d, che aggiunge `src/core/{budget,checkpoint,memory_pool,scheduler,spill}.cpp`, le intestazioni `astro/{budget,cancel,checkpoint,job_plan,memory_pool,scheduler,spill}.hpp` e `tests/unit/test_{budget,pool,spill,scheduler,m1d_protocol}.cpp`, e modifica `hash`, `job`, `provenance`, `sqlite_*` (impostazioni persistenti, snapshot di budget in provenance).
- Linux 7.0.0-38-generic x86_64, 16 CPU, 31,6 GiB di RAM, GCC 15.2.0, SQLite 3.46.1, OpenSSL 3.5.5. Preset `dev`: `-fsanitize=address,undefined -fno-sanitize-recover=all`.

## Consegna

- `budget` (`budget.cpp`): grammatica manuale, valore persistito in `settings` (`budget.manual`) e rivalidato alla riapertura; formula automatica su `/proc/meminfo`, cgroup v2 e PSI, arrotondata al MiB inferiore; `cgroup` validato riga per riga.
- `memory_pool` (`memory_pool.cpp`): prenotazioni tutto-o-niente, sotto-prenotazioni, attesa FIFO stretta con tetto di coda e cancel, stop a ogni allocazione oltre la prenotazione.
- `job_scheduler` (`scheduler.cpp`, `job_plan.hpp`): piano di tile, coda dei task limitata con backpressure, worker limitati da `sched_getaffinity` iniettabile, coda FIFO dei job (capacità 16), stop interno distinto dal cancel dell'utente, `run_branches` per slot in ordine di dipendenza.
- `checkpoint` (`checkpoint.cpp`): scrittura atomica, limite 64 MiB, resume solo con ricetta e backend identici.
- `spill` (`spill.cpp`): file anonimi `O_TMPFILE` in `scratch_dir` validata, quota condivisa, misura della page cache residente.
- Interfaccia e codici: sezione "Interfaccia e codici aggiunti in M2.d" di ADR-06.

## Esito per test

Tutti PASS, release; dev per tutti tranne T-M2D-13.

| ID | Test (file) | Cosa verifica |
|---|---|---|
| T-M2D-01 | `Budget.T_M2D_01` (4 test, `test_budget.cpp`) | Forme valide accettate; `MB`, `GB`, decimali, altre unità rifiutate; 0, oltre 100 % e valori non rappresentabili fuori intervallo; controllo contro `mem_total`. |
| T-M2D-02 | `Budget.T_M2D_02` (2) | Budget manuale persistito e rivalidato alla riapertura; impostazione manomessa rifiutata in lettura, non riparata. |
| T-M2D-03 | `Budget.T_M2D_03` (8) | Formula senza cgroup e PSI; arrotondamento al MiB; frazione bassa alla soglia di pressione; margine del cgroup; sorgenti cgroup assenti = `nullopt`; `MemAvailable` assente o incompleto: stop; `cgroup_current` >= `cgroup_max`: stop; file cgroup e PSI non interpretabili: stop. |
| T-M2D-04 | `Threads/PoolProperty.T_M2D_04` (1, 2, 4 thread), `Pool.T_M2D_04` (`test_pool.cpp`) | Somma delle prenotazioni mai oltre il budget; sotto-prenotazioni mai oltre la madre sotto concorrenza. |
| T-M2D-05 | `Scheduler.T_M2D_05` (`test_scheduler.cpp`) | Job impossibile rifiutato (`sched.job_exceeds_budget`) prima di ogni task. |
| T-M2D-06 | `Scheduler.T_M2D_06` | Budget stretto: stop; tile e worker non ridotti. |
| T-M2D-07 | `Scheduler.T_M2D_07` (2) | Stop iniettato e eccezione estranea in un worker: nessuna pubblicazione, prenotazioni rilasciate, job `failed`. |
| T-M2D-08 | `Scheduler.T_M2D_08` (3) | Cancel a metà, prima dell'avvio, in attesa del pool: nessun output, nessun temporaneo, `cancelled`, uscita 3. |
| T-M2D-09 | `Resume.T_M2D_09` (6) | Resume identico riprende (anche con worker paralleli, senza ripetere task già nel checkpoint); ogni differenza di ricetta o backend: `resume_mismatch`; parziale con digest diverso: `integrity`; resume rifiutato non riparte da zero; checkpoint malformati: `integrity`. |
| T-M2D-10 | `Spill.T_M2D_10` (8), `Scheduler.T_M2D_10` (2) | Scratch assente, file regolare, symlink, non scrivibile, senza `O_TMPFILE`: stop; quota superata senza scritture; quota condivisa e restituita alla distruzione; scritture concorrenti rispettano la quota; spill non dichiarato rifiutato; quota superata in un task: job `failed`, uscita 2. |
| T-M2D-11 | `Scheduler.T_M2D_11` | Backpressure: produttore bloccato, nessun task perso. |
| T-M2D-12 | `Pool.T_M2D_12` (2) | Allocazione oltre la prenotazione: stop, con prenotazione e quantità nel messaggio. |
| T-M2D-13 | `T-M2D-13.m1d_protocol` (`test_m1d_protocol.cpp`, 4 test `Protocol.T_M2D_13_*`, label `slow`) | Oracle in forma chiusa contro forza bruta su prefissi piccoli; stream da 40 GiB logici con pool da 32 MiB, tile da 2 MiB, 1/2/4 worker; spill fisico da 256 MiB con quota e page cache; guasti a scala di stream: stop senza riduzioni. |
| T-M2D-14 | `Scheduler.T_M2D_14` | Coda dei job piena: `sched.queue_full`, il job non entra; ordine FIFO. |
| T-M2D-15 | `Scheduler.T_M2D_15` (2) | `worker_count` oltre le CPU di `sched_getaffinity` iniettata: stop; affinità in errore o a zero CPU: stop. |
| T-M2B-09 | `Branches.T_M2B_09` (`test_scheduler.cpp`) | Con `run_branches` un job fallito blocca (`blocked`, `blocked_by_slot`) i soli discendenti, che non partono; i rami indipendenti si completano; uscita 2. Chiude la parte lasciata PARZIALE in M2.b. |
| T-M2B-12 | `Scheduler.T_M2B_12` | Prenotazioni, anche quelle proprie del lavoro, rilasciate su successo (0), stop (2, `failed`) e cancel (3, `cancelled`); `reserved_bytes()` = 0. Chiude la parte lasciata PARZIALE in M2.b. |

Test aggiuntivi nati dalle review, senza ID di matrice (29): `Scheduler.*` (stop propagato ai task cooperativi, cancel durante `finalize`, cancel al confine di pubblicazione, due job ammessi da soli ma non insieme, piano validato, esecuzione unica dei task, riserva condivisa e `prepare`/`finalize`), `Resume.*` (nome e directory del checkpoint, checkpoint sopravvissuto a una pubblicazione fallita, limite di 64 MiB lato lettura e lato scrittura), `Branches.*` (ordine dei genitori, cancel su rami non avviati, cancel dopo il lavoro, eccezioni estranee, guasti dello scheduler), `Pool.*` (tutto-o-niente, ordine FIFO dei grant, `bad_alloc` senza prenotazioni orfane, coda e cancel, buffer di lettura dell'input), `Spill.*` (file anonimi, round trip e letture brevi, residenza e `drop_cache`, estensioni concorrenti, `mmap` non perso su allocazione fallita), `Budget.*` (snapshot JSON, sorgenti reali di sistema).

## T-M2D-13: misure

Esecuzione di `ctest --preset release`, ultimo blocco di `tmp/m2d-protocol/protocol-report.txt` (il file accumula le esecuzioni: 8 blocchi, il più vecchio è dello sviluppo). Pool da 32 MiB (33 554 432 B), tile da 2 MiB, 20 480 tile, 40 GiB logici (42 949 672 960 B). Per ogni prova: `wrong_tiles` = 0, `exit_status` = 0, `pool_final_reserved` = 0.

### Stream da 40 GiB logici

| Worker | Tempo (s) | GiB/s | Picco prenotato (MiB) | RSS VmHWM (MiB) | Oracle (somma attesa = ottenuta) |
|---|---|---|---|---|---|
| 1 | 80,29 | 0,498 | 14 | 20,9 | 5 481 452 011 520 = 5 481 452 011 520 |
| 2 | 10,48 | 3,815 | 20 | 28,8 | 5 481 452 011 520 = 5 481 452 011 520 |
| 4 | 9,36 | 4,276 | 32 | 38,6 | 5 481 452 011 520 = 5 481 452 011 520 |

- Digest dei parziali identico per 1, 2 e 4 worker: `2287ce4f7e7fd6e262dcc5afb297e5829a5d16d5dbd10e69efbb4127bc460c21`. Inversioni di completamento: 0, 2909, 5147 (i parziali si compongono in ordine di indice, non di completamento).
- Il picco prenotato con 4 worker coincide con il budget del pool (33 554 432 B) senza superarlo; l'RSS resta sotto i 40 MiB contro 40 GiB attraversati.
- Il caso a 1 worker è lento e variabile: negli 8 blocchi del report va da 60,5 s a 101,9 s (0,39-0,71 GiB/s). Non è stato indagato (vedi limiti).

### Spill fisico da 256 MiB

Quota 268 435 456 B, picco spillato 268 435 456 B in tutte le prove, 128 tile, `readback_mismatching_floats` = 0 (rilettura bit a bit identica all'oracle). Digest dei parziali identico in tutte le prove: `5b6be7c47257edd8f1b75d8e3cd1112f8289e7e0dd7709f5d2ac9f044f78f7d3`.

| Worker | `drop_cache` | Tempo (s) | GiB/s | RSS VmHWM (MiB) | Byte spillati (allocati) | Page cache residente dopo scrittura / rilettura (B) |
|---|---|---|---|---|---|---|
| 1 | no | 0,545 | 0,459 | 19,8 | 268 435 456 | 268 435 456 / 268 435 456 |
| 2 | no | 0,262 | 0,955 | 27,7 | 268 435 456 | 268 435 456 / 268 435 456 |
| 4 | no | 0,206 | 1,214 | 39,4 | 268 435 456 | 268 435 456 / 268 435 456 |
| 1 | sì | 0,788 | 0,317 | 19,8 | 268 439 552 | 0 / 0 |
| 2 | sì | 0,497 | 0,503 | 25,4 | 268 439 552 | 0 / 0 |
| 4 | sì | 0,367 | 0,681 | 39,4 | 268 439 552 | 0 / 0 |

- Senza `drop_cache` la page cache dello spill resta residente per intero (256 MiB) sia dopo la scrittura sia dopo la rilettura: lo spill sposta i dati fuori dall'RSS ma non dalla RAM del sistema. Con `drop_cache()` la residenza torna a 0 e il contenuto resta identico.
- I byte allocati con `drop_cache` sono 4096 in più (268 439 552): arrotondamento della pagina dell'estensione, entro la quota.
- Le differenze di `Cached` in `/proc/meminfo` nel report sono di sistema, rumorose e in un caso negative; non sono la misura valida. La misura valida è la residenza per file.
- Quarto test (`faults_at_stream_scale`): guasti iniettati a scala di stream fermano il job senza ridurre tile, worker o risoluzione. Esito PASS; il test non produce numeri nel report.

## Review indipendente (Codex gpt-6-astra, sola lettura)

Artefatti in `.omc/artifacts/ask/codex-m2d-review*.md`. Revisione statica del codice: i giri non hanno compilato né eseguito i test.

- **Giro 1: FAIL.** 2 bloccanti, 8 MED/LOW: `fail()` non segnalava lo stop ai task attivi (join bloccabile); `carve()`/`grant_locked()` aggiornavano i contatori prima dell'allocazione (prenotazioni orfane su `bad_alloc`); cancel durante `finalize` ignorato; checkpoint eliminato prima della pubblicazione; quota dello spill addebitata due volte da estensioni concorrenti; `mmap` perso se `pages` lancia; checkpoint oltre 64 MiB scritto e poi rifiutato dal reader; `/proc/self/cgroup` malformato trattato come assenza; test FIFO e guardia RSS del protocollo che passavano senza misura valida. Corretti, con test dedicati.
- **Giro 2: FAIL.** 2 MED, 3 LOW: `run_branches` non inoltrava il cancel a `run_job`; validazione cgroup incompleta (controller `0:memory:/app`, percorsi vuoti o relativi, duplicati v2); codice `sched.checkpoint_read` invece di `sched.checkpoint_size` oltre il limite; test di pubblicazione fallita che non raggiungeva `complete_job`; test FIFO che non provava l'ordine. Corretti, con test.
- **Giro 3: FAIL.** 1 MED: nel test FIFO `++sequence` precedeva la scrittura di `rank[i]` letto senza sincronizzazione (data race). Corretto: il completamento si segnala dopo la scrittura e si attende quel segnale prima di leggere.
- **Giro 4:** PASS, nessun rilievo residuo (`.omc/artifacts/ask/codex-m2d-review4-20261029-0741.md`): gli accessi atomici a `rank` chiudono la race e il test prova ancora l'ordine FIFO.

## Limiti dichiarati

- **Soglie F-RES-01** (frazione 0.50, frazione sotto pressione 0.25, soglia PSI 10.0): iniziali e invariate; **non confermate** da carichi PSI reali. Restano da confermare al gate con carichi misurati.
  - *Aggiornamento del gate M2 (29 ottobre 2026):* misurate con carichi reali; la frazione normale è corretta da 0.50 a 0.375, 0.25 e 10.0 invariati (perimetro: il PC di sviluppo). Testo storico sopra conservato; misura e decisione in `docs/evidence/M2-gate.md`.
- **Costo per task**: ogni task alloca e libera 3 buffer da 2 MiB. Il caso a 1 worker è lento e variabile (60,5-101,9 s per 40 GiB negli 8 blocchi del report); la causa non è indagata. Il risultato non influenza la correttezza (oracle e digest identici), solo il tempo.
- **Cancel cooperativo**: il cancel e lo stop interno sono osservati a ogni task, a ogni blocco di righe, dopo `finalize` e al confine di pubblicazione; un task che non chiama `check_cancel` non si interrompe finché non ritorna.
- **Resume**: riesegue i task dopo l'ultimo checkpoint; quelli già registrati nel checkpoint non si ripetono. Non riparte da zero in automatico.
- **Page cache dello spill** fuori dal budget del pool, come da contratto. `resident_bytes()` misura la residenza per file; una misura per cgroup (memoria di controllo attribuita al processo) è assente.
- **TSan** non è nei preset. È stato eseguito a mano su `test_pool`, `test_spill`, `test_scheduler`, `test_budget` durante lo sviluppo; l'autore riporta nessun avviso, il log non è stato conservato e il coordinatore non l'ha riprodotto.
- **`pwrite` fallito** lascia addebitata l'estensione fino alla distruzione dell'area di spill (il job è comunque fermo).
- **`ctest --preset release` esegue anche lo `slow`** (T-M2D-13, circa 107 s); il preset `dev` no. T-M2D-13 non è stato eseguito sotto ASan/UBSan.
- Il report del protocollo è un file accumulato: le misure citate sono quelle dell'esecuzione release di chiusura.
