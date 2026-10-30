# M3.a, F-IO-08: contratto del ricollegamento degli originali

Stato: **IN ARBITRAGGIO**: review FAIL al giro 2 con 1 MED e 1 LOW, non corretti; tetto di 2 giri raggiunto (vedi «Review» e «Punti per l'arbitraggio»). Unità: lacuna L3 di `plans/M3a.md`, prerequisito di U9. Solo testo: nessun codice di prodotto, nessun test scritto o eseguito, nessuna build.

| Campo | Valore |
|---|---|
| Unità | M3.a / contratto di F-IO-08 (lacuna L3) |
| Scopo | Estendere ADR-04 con rilevamento degli originali mancanti o spostati, ricollegamento verificato per dimensione e SHA-256, aggiornamento della posizione nello store, interazione con snapshot, undo, redo e recovery, nodi bloccati con motivo e sblocco senza ricalcolo |
| Comando | `python3 -S tools/trace.py ids --stage M3.a`; `codegraph explore` e lettura di `sqlite_dag_store.cpp`, `sqlite_project.cpp`, `sqlite_recovery.cpp`, `sqlite_files.cpp`, `sqlite_history.cpp`, `project_export.cpp`, `hash.cpp`, `hash.hpp`, `fits.hpp`, `import_job.cpp`, `import_job.hpp`, `provenance.cpp`; `tools/review.sh fio08-contract1`, `fio08-contract2`; `python3 -S tools/evidence.py docs/evidence/M3a-F-IO-08-contratto.md` |
| Input | Piano attivo: riga 1819 (F-IO-08), riga 3163, archi righe 3693-3694, 4422. Contratti ADR-04, BASE (OB-11, OB-12, emendamento L1), F-IO-03, ADR-05. `plans/M3a.md` (U9, lacuna L3) |
| Versioni | Worktree da `311fb7a`, con le patch non committate `tmp/m3a-amend.patch` e `tmp/fio03-contract.patch` applicate come contesto |
| Ambiente | Linux 7.0.0-38-generic x86_64, macchina di sviluppo |
| Risultato | Nuovo `docs/contracts/F-IO-08-ricollegamento.md`: obblighi RL-01…RL-08 con proprietario unico, test pianificati T-M3A-07 (a-c), 08 (a-d), 09 (a-f); rinvio in ADR-04 («Estensione F-IO-08»); voce in `docs/contracts/README.md`; riga L3 di `plans/M3a.md` |
| Output | File elencati nel blocco generato sotto; artefatti di review in `.omc/artifacts/ask/`; delta del giro 2 in `tmp/fio08-contract-round2.diff` |
| Ambito | Rilevamento, ricollegamento, blocco e sblocco, storia. Fuori: ricerca automatica (vietata), esecuzione dai percorsi dello store (F-IO-03 CL-04, M3.c), GUI (F-UX-01, M4) |
| Esito | FAIL al giro 2 (1 MED, 1 LOW, non corretti); in attesa dell'arbitraggio del coordinatore |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Obblighi

| RL | Obbligo | Proprietario |
|---|---|---|
| 01 | Identità immutabile, posizione corrente unica in `inputs`, `inputs.format`, storia append-only `input_relinks`, trigger | `schema_v1` (`sqlite_project.cpp`); `add_input` per `format` |
| 02 | Stato degli originali con un solo `stat` sui percorsi registrati, solo a richiesta, nessuna ricerca, nessuna scrittura | `sqlite_dag_store::input_status` |
| 03 | Slot bloccati con motivo, derivati e puri; esecuzione fermata dal confine | `sqlite_dag_store::blocked_slots` |
| 04 | Candidato verificato per dimensione e SHA-256 nel confine; ritorno dell'identità del descrittore verificato | `reverify_input` (`hash.cpp`) |
| 05 | Unica funzione che cambia la posizione, in una transazione, con controlli di percorso e di inode | `sqlite_dag_store::relink_input` |
| 06 | Il ricollegamento non è una mutazione della ricetta: snapshot, undo, redo e recovery invariati | `relink_input` |
| 07 | Sblocco senza ricalcolo | `blocked_slots` |
| 08 | Originali in sola lettura | protocollo di apertura del confine (OB-12) |

Decisioni prese nel contratto:

- File separato con rinvio in ADR-04, per non riaprire il contratto di M2.
- Posizione aggiornata in `inputs` con riga di storia obbligatoria (trigger), invece di righe di posizione append-only: i consumatori di `inputs` restano invariati e l'inode di un file vecchio esce dall'insieme degli originali.
- `inputs.format` registrato, perché il confine apre un input solo con un formato dichiarato.
- Nessun controllo di stato all'apertura dello store né nella recovery; lo stato non si persiste.
- Blocco come esito dell'analisi; l'esecuzione la ferma il confine (OB-11), senza un secondo controllo nel runner.
- Undo e redo non ripristinano una posizione.

## Limiti

- Lo stato del codice è letto al commit `311fb7a`: nessuna build, nessun test eseguito.
- Gli ID T-M3A-07, 08, 09 sono pianificati, non esistono e non sono in `docs/test-matrix.md`.
- Limiti dichiarati nel contratto (§11 e RL-03): contenuto non letto dallo stato; file byte-identico con inode nuovo eseguibile; corsa fra verifica e `COMMIT`; percorsi relativi di `add_input`; percorsi di esecuzione presi dal chiamante.

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-fio08-contract1-20261030-1231.md` | primo giro, testo intero | FAIL | 4 MED, 3 LOW: sostituzione byte-identica `input.modified` non fermata dal confine; normalizzazione lessicale del percorso con symlink e `..`; collegamenti fisici di `project.db`, WAL, SHM non esclusi; T-M3A-08c incoerente con i casi che modificano il file o non lo aprono; LOW: atomicità attribuita ai trigger, «senza fase» per `present = 0`, record citato nel piano non ancora presente |
| 2 | `.omc/artifacts/ask/codex-fio08-contract2-20261030-1237.md` | `tmp/fio08-contract-round2.diff` | FAIL | 1 MED: T-M3A-08c chiede elenco e `mtime` delle directory invariati anche nel caso in cui il test sostituisce il candidato, che cambia la directory. 1 LOW: il caso `relink.identity_changed` non dice che il sostituto è byte-identico con inode diverso. Gli altri rilievi del giro 1 risultano corretti |

## Punti per l'arbitraggio

- MED del giro 2, aperto: in T-M3A-08c l'oracolo delle directory va confrontato con lo stato atteso dopo l'iniezione del test, non con quello iniziale, nel caso di sostituzione fra `stat` e verifica (e di allungamento durante la lettura).
- LOW del giro 2, aperto: in T-M3A-08b il sostituto del caso `relink.identity_changed` deve essere byte-identico, con inode diverso, altrimenti la verifica di RL-04 si ferma prima.
- Interfaccia con U8a: CL-04 di F-IO-03 deve passare `format` ad `add_input` e, punto aperto, registrare percorsi assoluti.

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-04-persistenza.md` | `83861d7d5db28b2f45b7748a0af91044e2cd56565c3fa9295733c0561163401c` |
| `docs/contracts/F-IO-08-ricollegamento.md` | `fa680da12c9d7e1d332bc67a06631d5c843464e238d69f1ce5181e283241111d` |
| `docs/contracts/README.md` | `353e714178f05198679290fc445edac14ca3aa881f4f1d4e91c44d7c8df00b7c` |

<!-- evidence:end -->

## Arbitraggio del coordinatore (dopo il giro 2)

Giro 2 FAIL con 1 MED e 1 LOW; tetto di 2 giri raggiunto. Correzioni minime del coordinatore, lette senza un terzo giro: T-M3A-08c confronta le directory con lo stato registrato dal test dopo il proprio intervento nei casi di sostituzione e allungamento (MED); T-M3A-08b dichiara che il sostituto ha gli stessi byte e un inode diverso (LOW). Decisione sul punto aperto dei percorsi relativi: da U8a `add_input` accetta solo percorsi assoluti.
