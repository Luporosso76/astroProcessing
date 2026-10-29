# M2.c: store SQLite durevole, snapshot, export e verifica

Stato: **DONE**, 29 ottobre 2026. Autore: agente executor su Claude Sonnet 5.5. Review indipendente: Codex `gpt-6-astra` (reasoning high, sola lettura), cinque giri. Contratto: `docs/contracts/ADR-04-persistenza.md`; piano: `plans/M2.md` (M2.c, F-PRJ-04, F-PRJ-07).

## Comando ed esito

- Dev (ASan + UBSan): `rm -rf build && tools/check.sh > tmp/check-m2c.log 2>&1`. Esito: 190/190 (`tmp/check-m2c.log`, riga "100% tests passed, 0 tests failed out of 190").
- Release: `cmake --preset release && cmake --build --preset release && ctest --preset release > tmp/check-m2c-rel.log 2>&1`. Esito: 189/189 (`tmp/check-m2c-rel.log`).
- I test T-M2C e T-M2B-11 sono 30 casi CTest su 190 (dev) e su 189 (release): tutti Passed in entrambi i log.

## Revisione e ambiente

- Revisione git: `7ce77a385e1e7db4a20e25e10eec6616f32f3f3c` (M2.b). I test sono stati eseguiti sul working tree poi entrato nel commit di chiusura di M2.c, che aggiunge `src/core/sqlite_*.cpp`, `project_export.cpp`, `project_verify.cpp` e `tests/unit/test_store*.cpp`, `test_export.cpp`.
- Linux 7.0.0-38-generic x86_64, GCC 15.2.0 (CXX e C), SQLite 3.46.1, OpenSSL 3.5.5. Preset `dev`: `-fsanitize=address,undefined -fno-sanitize-recover=all`.

## Consegna

- `sqlite_dag_store` (`src/core/sqlite_{dag_store,files,history,internal,project}.cpp`) dietro `dag_store`: transazioni con WAL, schema versionato (`PRAGMA user_version` = 1), scrittura atomica degli artefatti (temporaneo `O_EXCL`, fsync, `renameat2 RENAME_NOREPLACE`, fsync directory), recovery all'apertura, migrazioni con backup, snapshot immutabili con undo e redo, apertura in sola lettura per schema ignoto.
- `src/core/project_export.cpp`, `project_verify.cpp`: export con manifest e marker `COMMIT`, `verify_export` che dichiara incompleto ogni bundle non verificabile.
- Scelte di implementazione e deviazioni dal testo originale dell'ADR sono nella sezione "Scelte di implementazione (M2.c)" di ADR-04.

## Esito per test

Tutti PASS, dev e release.

| ID | Test (file) | Cosa verifica |
|---|---|---|
| T-M2C-01 | `Store.T_M2C_01` (`test_store.cpp`) | Crea, salva, riapre: stesso stato logico. |
| T-M2C-02 | `StoreCrash.T_M2C_02` (`test_store_crash.cpp`) | SIGKILL prima di ciascuno dei 10 punti del protocollo di commit; riapertura coerente, nessun output parziale `completed`, orfani rimossi. |
| T-M2C-03 | `Store.T_M2C_03` (2 test) | Orfani temporanei e finali rimossi solo in `artifacts/`; righe con file assente o dimensione diversa marcate `present = 0`. |
| T-M2C-04 | `Store.T_M2C_04` | `fsync` e `rename` falliti per iniezione: job `failed`, nessuna riga, nessun retry. |
| T-M2C-05 | `Store.T_M2C_05` (2 test) | Schema più recente: sola lettura, `project.db` invariato, nessun `-wal` o `-shm`; WAL pendente: stop. |
| T-M2C-06 | `Store.T_M2C_06`, `StoreCrash.T_M2C_06` | Contenuto del backup e rollback; SIGKILL nel backup e in ogni istruzione della transazione di migrazione: versione 1 intatta, migrazione ripetibile. |
| T-M2C-07 | `Store.T_M2C_07` | Database corrotto: `integrity`, nessuna riparazione. |
| T-M2C-08 | `Store.T_M2C_08` | Secondo processo rifiutato con `store.locked`. |
| T-M2C-09 | `Store.T_M2C_09` | File estranei o `project.lock` assente: stop, nulla creato o modificato. |
| T-M2C-10 | `Store.T_M2C_10` | Job in corso marcati `interrupted`, mai `completed`. |
| T-M2C-11 | `Store.T_M2C_11` | Originali registrati mai toccati dal workflow dello store. |
| T-M2C-12 | `Store.T_M2C_12` | Symlink in `artifacts/`: recovery ferma, nulla cancellato. |
| T-M2C-13 | `Store.T_M2C_13` | Snapshot immutabili. |
| T-M2C-14 | `History.T_M2C_14` | Modifica, undo, redo: stato logico identico, `artifact_id` ricollegato. |
| T-M2C-15 | `History.T_M2C_15` | Undo con job in corso: pubblicazione stale. |
| T-M2C-16 | `History.T_M2C_16` | Undo con artefatto assente: slot `missing`, ricostruzione riusa il nome. |
| T-M2C-17 | `History.T_M2C_17` | Undo e redo oltre i limiti, nodo mancante, sola lettura: stop. |
| T-M2C-18 | `History.T_M2C_18` | Nuovo snapshot dopo undo azzera il redo. |
| T-M2C-19 | `Export.T_M2C_19` | Un altro processo ricostruisce `node_key` e digest dal solo manifest. |
| T-M2C-20 | `Export.T_M2C_20` (`test_export.cpp`) | SIGKILL a ogni passo dell'export (da `export.mkdir` a `export.commit.fsync_dir`, almeno 16 punti): senza `COMMIT` il bundle è `export.incomplete`. |
| T-M2C-21 | `Export.T_M2C_21` | File o manifest alterati dopo `COMMIT`, file in più: incompleto. |
| T-M2C-22 | `Export.T_M2C_22` | Slot non `current`, destinazione esistente, symlink o dentro il progetto: stop. |
| T-M2C-23 | `Store.T_M2C_23` | Output identici riusati; nome col digest ma contenuto diverso o non regolare: `security`. |
| T-M2C-24 | `Export.T_M2C_24` | Manifest malevoli (`..`, assoluti, NUL, duplicati, symlink, non regolari, malformati): stop senza letture fuori dalla directory. |
| T-M2C-25 | `Store.T_M2C_25` | `create` su percorso esistente: `store.target_exists`. |
| T-M2B-11 | `Store.T_M2B_11`, `Dag.T_M2B_11` | Chiave e forma canonica del nodo identiche dopo una migrazione da versione 1 a 2; `user_version` assente dal JCS. |

Test aggiuntivi nati dalle review: `tests/unit/test_store_review.cpp` (19 casi `Review.*`) e `test_store_contract.cpp`; non hanno ID di matrice.

## Crash test fork + SIGKILL

`run_child` (`tests/unit/store_test_support.hpp`) esegue il corpo in un processo figlio; un iniettore conta i punti di guasto e uccide il figlio con SIGKILL al k-esimo. Una prova a secco (`record_points`) registra la sequenza dei punti; il test fallisce se la sequenza attesa cambia. Dopo la morte, il processo padre riapre il progetto e verifica lo stato.

| Scenario | Punti | Verifica dopo la morte |
|---|---|---|
| Commit (T-M2C-02) | `artifact.create`, `.write`, `.fsync_file`, `.rename`, `.fsync_dir`, `commit.begin`, `.row`, `.inserted`, `.before_commit`, `.after_commit` | Fino a `before_commit`: job `interrupted`, nessuna riga artefatto, slot `missing`, nessun file residuo dopo recovery. Dopo `after_commit`: `completed`, slot `current`, digest del file uguale a quello registrato. |
| Commit multi-output e `run_job` (`Review.M16`, `Review.B2`) | punti interni al commit | Tutto o niente; pubblicazione e chiusura del job in un'unica transazione. |
| Migrazione (T-M2C-06) | punti del backup, poi 2 istruzioni `migration.statement` | `user_version` = 1, colonna non presente, stato logico invariato; la migrazione ripetuta riesce a versione 2. |
| Export (T-M2C-20) | tutti i passi, `export.mkdir` incluso | Primo punto: nessuna directory. Intermedi: `export.incomplete`. Ultimo: bundle verificabile con 2 file. Il progetto resta apribile con `sd` `current`. |

## Modello di minaccia

Da ADR-04, sezione "Modello di minaccia". I test e le review giudicano solo dentro questo perimetro.

- **Dentro:** crash di processo (SIGKILL) in qualunque punto; modifiche accidentali o esterne ai file derivati, rilevate prima dell'uso (identità dev/ino/dimensione/mtime/ctime e digest); symlink e percorsi negli input non fidati (file importati, manifest e contenuto dei bundle di export).
- **Fuori:** processo malevolo con lo stesso UID che manipola i file del progetto durante un'operazione (può comunque riscrivere `project.db`); perdita di corrente e guasti del dispositivo.
- I controlli di identità difendono da modifiche rilevabili, non da una corsa deliberata fra controllo e uso. `SQLITE_OPEN_NOFOLLOW` non è usabile con `/proc/self/fd`: l'identità del temporaneo del backup si controlla subito prima dell'apertura e di nuovo prima della pubblicazione.

## Review indipendente (Codex gpt-6-astra, sola lettura)

Artefatti in `.omc/artifacts/ask/codex-m2c-review*.md`. Revisione statica del codice: i giri non hanno compilato né eseguito i test.

- **Giro 1: FAIL.** 6 bloccanti, 10 MED: schema futuro con WAL pendente aperto in scrittura, `publish` separato da `finish_job`, `stored_file` costruibile a mano, undo che ricollega l'artefatto sbagliato e lascia attivi gli slot nuovi, symlink in `backups/` e temporaneo prevedibile; `strtod` dipendente dalla locale, manifest con `versions={}` accettato, allocazione da dimensioni non fidate, `readdir` che confonde errore ed EOF, `retarget_slot` senza snapshot, retry su EINTR, contesto SQLite perso. Corretti, con test `Review.*`.
- **Giro 2: FAIL.** 4 bloccanti, 3 MED: `stored_file` non legato allo store, `complete_job` senza fsync, cambio di `publish_role` non storicizzato, backup pubblicato per nome senza identità; ruoli duplicati nel commit, controlli finali di `verify_export` limitati ai file aperti, `fstat` fallito mascherato. Corretti (token legati all'istanza, `set_publish_role` come mutazione con snapshot, backup `O_EXCL` con identità).
- **Giro 3: FAIL.** 4 bloccanti, 2 MED: identità acquisita dopo `atomic_write`, modifica in-place con mtime ripristinato, scrittura SQLite sul temporaneo del backup per nome, troncamento del backup dopo `quick_check`; token indicizzati solo per percorso, file extra fra due scansioni. Corretti (ctime, identità dal descrittore, token monouso, ricontrollo completo prima della pubblicazione, elenco finale confrontato col manifest).
- **Giro 4: FAIL.** 2 MED: riscrittura in-place fra digest e pubblicazione non coperta, nel riuso compreso; test di crash della migrazione che interrompeva il backup invece della transazione. Corretti (riletta in streaming dal nome finale con fstat prima e dopo, `store.file_identity_changed`; punti `migration.statement` selezionati per nome; test `Review.T1_*`).
- **Giro 5:** PASS, nessun rilievo residuo dentro il modello di minaccia (`.omc/artifacts/ask/codex-m2c-review5-20261029-0624.md`), dopo la verifica del digest riletto dal nome pubblicato (anche nel riuso) e la selezione per nome dei punti `migration.statement` nel crash test.

## Limiti dichiarati

- **Durabilità contro perdita di corrente non provata**: i test simulano la morte del processo con SIGKILL; `fsync` è invocato ma il suo effetto sul dispositivo non è verificato. Fuori dal modello.
- **Processo malevolo con lo stesso UID fuori dal modello**: può riscrivere `project.db` e vincere corse deliberate fra controllo e uso.
- **Percorso `readdir` senza test di iniezione**: `read_directory` (`src/core/sqlite_internal.cpp`) propaga l'errore di `readdir` come `io`, ma nessun test lo fa fallire; il ramo è coperto solo da lettura del codice.
- Il crash test non prova l'interruzione a metà di un singolo `write` o `fsync`: i punti sono quelli registrati dall'iniettore, fra un passo e il successivo.
- `job_state::queued` non è prodotto in M2 (la recovery lo tratta come `running`); il blocco dei soli dipendenti e il rilascio delle prenotazioni restano di M2.d (T-M2B-09, T-M2B-12).
- T-M2C-11 verifica gli originali sul workflow dello store, non sul workflow completo di M2.e (ADR-04 lo estende a M2.e).
- Il codice non è committato (vedi "Revisione e ambiente").
