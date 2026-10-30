# M3, base comune: contratto degli obblighi trasversali

Stato: **CHIUSO per arbitraggio del coordinatore dopo il giro 2** (vedi «Arbitraggio»). Unità: M3, base comune, punti 1 e 2 di «Base comune prima del codice delle slice» in `plans/M3.md`. Solo testo: nessun codice di prodotto, nessun test scritto o eseguito, nessuna build.

| Campo | Valore |
|---|---|
| Unità | M3 / base comune, passo 1 (contratto) |
| Scopo | Assegnare ogni requisito trasversale dei job a un solo confine condiviso, con lo stato nel codice, la suite di conformità sul registro dei tipi e i debiti |
| Comando | `codegraph explore` sui chiamanti di `run_job_files`, `run_job`, `prepare_job_start`, `job_scheduler::run`, `run_branches`; `grep` su `src/core/` e `tests/unit/`; `tools/review.sh base-round1`, `base-round2`; `python3 -S tools/evidence.py docs/evidence/M3-base-contratto.md` |
| Input | Codice di `src/core/` al commit `db2a389`; contratti ADR-04, ADR-05, ADR-06, ADR-07, ADR-13. Piano attivo: riga 384 (NC-21), righe 2009-2023 (NFR), riga 2567 (ADR-06), righe 3352-3377 (registro). Formato ripreso da `/opt/workspace/meridrive/docs/spec/contratti/moto.md` (sola lettura) |
| Versioni | Worktree da `db2a389` |
| Ambiente | Linux 7.0.0-38-generic x86_64, macchina di sviluppo |
| Risultato | Nuovo `docs/contracts/BASE-confini.md`: OB-01…OB-19, suite T-M3BASE-01…19, debiti; voce in `docs/contracts/README.md` |
| Output | File elencati nel blocco generato sotto; artefatti di review in `.omc/artifacts/ask/` |
| Ambito | Confine dei job. Fuori: i percorsi interni degli operatori, le guardie FP delle funzioni di libreria chiamate fuori da un job, la cache delle miniature |
| Esito | Review FAIL al giro 2; chiuso per arbitraggio del coordinatore con le correzioni dei 3 MED. Vedi «Review» e «Arbitraggio» |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Obblighi e stato

| OB | Proprietario | Stato |
|---|---|---|
| 01 Arresto prima del lavoro | runner (`stop_before_work`) | IMPLEMENTATO NEL PROPRIETARIO |
| 02 Arresto nel lavoro o nella pubblicazione | runner (`stopped`, `unexpected`) | IMPLEMENTATO NEL PROPRIETARIO |
| 03 Arresto fuori da un job (workflow) | workflow | MANCANTE |
| 04 Ambiente FP al confine | runner | IMPLEMENTATO ALTROVE |
| 05 Ambiente FP nel worker | scheduler | IMPLEMENTATO ALTROVE |
| 06 Cancel alla pubblicazione | runner | IMPLEMENTATO NEL PROPRIETARIO |
| 07 Cancel nel lavoro | scheduler | IMPLEMENTATO ALTROVE (con parti mancanti) |
| 08 Picco prenotato prima dei task | scheduler | IMPLEMENTATO ALTROVE |
| 09 Osservazioni comuni nella provenance | runner | IMPLEMENTATO ALTROVE |
| 10 Nodo uguale alla richiesta | runner | IMPLEMENTATO ALTROVE |
| 11 Input verificati | lettura degli input | IMPLEMENTATO ALTROVE |
| 12 Originali in sola lettura | `open_fits_input` | IMPLEMENTATO NEL PROPRIETARIO |
| 13 Una sola esecuzione per job | runner | IMPLEMENTATO NEL PROPRIETARIO |
| 14 Nessuna pubblicazione su errore | runner con `commit_job` | IMPLEMENTATO NEL PROPRIETARIO |
| 15 Resume dei task | scheduler | IMPLEMENTATO NEL PROPRIETARIO |
| 16 Resume del workflow | workflow | IMPLEMENTATO NEL PROPRIETARIO |
| 17 Temporanei in directory dichiarate | scheduler | IMPLEMENTATO NEL PROPRIETARIO |
| 18 Nessuna rete | `libastrocore` | IMPLEMENTATO NEL PROPRIETARIO |
| 19 Checkpoint rimosso solo dopo la pubblicazione | runner | IMPLEMENTATO ALTROVE |

Registro dei tipi di job per la suite: MANCANTE.

## Arbitraggio

Il coordinatore ha chiuso l'unità dopo il giro 2, senza un terzo giro. I 3 MED del giro 2 sono stati corretti con modifiche minime al testo (delta in `tmp/base-arbitrato.diff`):

- OB-14 e verifica comune (c): «solo la provenance» vale con la generazione corrente; con generazione superata gli output restano come cache del nodo (`sqlite_files.cpp:163`). La suite controlla entrambi i casi.
- T-M3BASE-07: si contano i blocchi di righe elaborati dopo il cancel, non le chiamate a `check_cancel`.
- T-M3BASE-16: output rimosso prima della recovery, con riesecuzione del passo; output rimosso dopo la recovery, con stop `io` `resume.artifact_missing`.

Decisioni del coordinatore, scritte nel contratto:

- OB-03: un arresto del workflow senza job si registra nello store come record di arresto a livello di workflow. La chiave è l'identità del workflow (digest della definizione più l'esecuzione); il record contiene solo stringhe e interi; lo schema lo sceglie l'unità di codice. Se lo store non si apre, è un limite dichiarato, con la diagnosi nel codice di uscita.
- OB-11: gli header si leggono prima del digest solo per pianificare la memoria, entro i limiti del budget. Il digest si verifica prima di usare qualunque pixel e prima della pubblicazione. I valori di header usati nel piano si confrontano con quelli del file verificato: una differenza dà uno stop `integrity`.
- OB-14: la rimozione degli orfani alla recovery successiva resta un'ipotesi dichiarata, verificata dalla suite (T-M3BASE-14).

## Limiti

- Lo stato è letto sul solo codice di `src/core/` al commit `db2a389`: nessun test eseguito, nessuna build.
- L'interfaccia del registro è fissata solo nelle proprietà.
- Gli ID `T-M3BASE-NN` sono pianificati: non esistono come test e non sono in `docs/test-matrix.md`.

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-base-round1-20261030-0746.md` | primo giro, testo intero | FAIL | 10 MED, 1 LOW: generazione superata; errore dopo il commit; cancel mancante in `prepare` e nell'import; OB-15 con due proprietari; verifiche comuni applicate a T-03; iniezioni in `work`; MXCSR nel worker; prove di cancel che non isolano l'obbligo; `input.consumed_digest`; prove di resume; percorso dello store |
| 2 | `.omc/artifacts/ask/codex-base-round2-20261030-0754.md` | `tmp/base-round2.diff` | FAIL | 3 MED: (c) e OB-14 con generazione superata; T-M3BASE-07 conta `check_cancel`; T-M3BASE-16 senza la distinzione prima e dopo la recovery. Gli altri rilievi del giro 1 risultano corretti. Corretti per arbitraggio, senza terzo giro |

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/BASE-confini.md` | `2e9428531ed13bc3b2b8e7ce9ddd0b3ffe5c8ec61d9c9272e0c07896b1c21388` |
| `docs/contracts/README.md` | `603cd99cb80049be168c75247bab3a392c6b597095ecf6a79675a7a1cb01173c` |

<!-- evidence:end -->
