# M3.a, unità U9: F-IO-08, ricollegamento degli originali

Stato: **chiusa**. Test: review FAIL al giro 2 (2 MED), arbitraggio (contratto emendato: `data_version` da una seconda connessione, punti di iniezione del kill), correzioni finali del coordinatore. Codice: review FAIL al giro 2 (2 MED), arbitraggio del coordinatore (criterio di T-M3A-09a allineato ad ADR-05; `input_record::format` obbligatorio; NUL rifiutato da `add_input`), mutazioni del coordinatore, review sul delta (blocco «Review»).

| Campo | Valore |
|---|---|
| Unità | M3.a / U9 (test e codice) |
| Scopo | `input_status` (stato di ogni originale senza scritture), `blocked_slots` (slot dipendenti bloccati con motivo), `relink_input` (unica funzione di ricollegamento, verifica di dimensione e SHA-256, una transazione con la storia in `input_relinks` protetta da trigger), `input_record::format` dichiarato (RL-01…RL-08) |
| ID | F-IO-08; T-M3A-07, T-M3A-08, T-M3A-09 |
| Comando | Integrazione: build pulita, `tools/check.sh` e `ctest --preset release` completo (blocco generato). Mutazioni: `tools/mutate.sh` (patch in `tmp/mut/` e `tmp/u9arb-mut/` del worktree) |
| Versioni | GCC 15.2.0, CMake 4.2.3, Ninja, SQLite di sistema |
| Ambiente | Linux 7.0.0-38-generic x86_64, 16 CPU; worktree `.claude/worktrees/api-u9` sulla base `eccb629` |
| Autore e data | Test e codice: agenti Claude Sonnet 5.5 (API), distinti. Review: Claude Opus 5.5 (Argon fuori quota). Coordinatore: Claude Opus 5.5. 5 novembre 2026 |

## Decisioni del coordinatore

- Assenza di scritture provata con `PRAGMA data_version` letto da una seconda connessione (`sqlite3_total_changes` della connessione del prodotto non è osservabile dai test); limite: una transazione aperta e annullata non cambia `data_version`, quindi gli stop di RL-05 prima del commit si provano col confronto delle tabelle.
- Kill ai punti `relink.inserted`, `relink.before_commit`, `relink.after_commit`; un kill dentro l'esecuzione di `COMMIT` non è raggiungibile da un punto del prodotto (vale l'atomicità del commit di SQLite in WAL, limite dichiarato).
- T-M3A-09a: lo stop del job al confine porta la generazione dello slot a +1 e scrive solo l'artefatto `provenance` (ADR-05); nessun artefatto primario pubblicato.
- `input_record::format` senza default: un formato assente o fuori elenco è `invalid_input` `store.bad_input`. `add_input` rifiuta un percorso con NUL.
- Schema: lo schema 1 è esteso (`inputs.format`, `input_relinks`, trigger) senza cambiare `user_version`: il contratto dichiara che non esiste un progetto rilasciato da migrare.

## Mutazioni

| Patch | Cosa cambia | Esito |
|---|---|---|
| ricollegamento senza verifica del digest | — | rilevata |
| `input_status` che scrive | — | rilevata |
| `INSERT` e `UPDATE` in due transazioni | — | rilevata |
| archi del solo nodo target | — | rilevata (`RelinkNoStat.T_M3A_09f`) |
| `relink_nul_not_rejected` | tolto il controllo del NUL in `relink_input` | rilevata |
| `add_input_nul` | tolto il controllo del NUL in `add_input` | rilevata |
| `add_input_format_unchecked` | formato non controllato | rilevata |
| `job_provenance_not_recorded` | provenance dello stop non scritta (`persist_provenance`) | rilevata |
| `boundary_stop_not_recorded` | provenance non scritta in `stop_before_work` | sopravvive: 09a non attraversa quel percorso (lo stop del job è registrato da `persist_provenance`) |

## Limiti

- Release dell'integrazione U5 + U9: 235 test falliti in un minuto di disco pieno (549 GB esauriti dalle build dei worktree), tutti di scrittura su disco; rieseguiti con `ctest --preset release --rerun-failed` dopo aver liberato spazio: 235 su 235 verdi; il junit `tmp/release-junit.xml` unisce i due giri.
- Stop di RL-05 prima del commit: provati dalle tabelle, non da `data_version` (sopra).
- Kill dentro `COMMIT`: non raggiungibile (sopra).
- 07c si salta (`GTEST_SKIP`) se il test gira da root.
- Codici d'errore nel codice e non nel contratto: `relink.state_missing`, `relink.path_unresolvable`, `input.stat_failed` per lo `stat` del candidato.

## Review

- Test, giro 1 FAIL (5 MED), giro 2 FAIL (2 MED); arbitraggio `.omc/artifacts/ask/review-u9tests-arbitrage-claude.md` FAIL (2 MED chiusi poi dal coordinatore: criterio di 08b, testimoni di scrittura sui due rifiuti fuori da `expect_rejected`).
- Codice, giro 1 e 2 FAIL (2 MED); arbitraggio `.omc/artifacts/ask/review-u9code-arbitrage-claude.md`.

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/F-IO-08-ricollegamento.md` | `9e5498a4661fd7f5685bf37edeb52093b3fd0112a54f05d4219de90b54f7515f` |
| `src/core/hash.cpp` | `cafc181b8501db004dbe56c9f4ba10ea481dd025fce467b93e330610fcc2613f` |
| `src/core/include/astro/hash.hpp` | `2043851aa8b5e01288e40841a44bd77476bfe12f9a6ff2100984117409256481` |
| `src/core/include/astro/sqlite_dag_store.hpp` | `8a170294fae877b41b24ea6c80031cb8b3d6aba5042933ec835354d048e09052` |
| `src/core/sqlite_dag_store.cpp` | `4a7145ae4ff5616dd4bff2361676595a6bf399b932911946311feada7f272ff7` |
| `src/core/sqlite_project.cpp` | `4defa68b1292f66d893ba24b6ca17222a82e126376ab8fc438704515d214b9c5` |
| `src/core/sqlite_relink.cpp` | `1394326d8151c68dc22d815af66a65817fc217ab6b72c0936316559fa5d76f0b` |
| `tests/fixtures/fs_probe.hpp` | `c14734822290c68094bb0f12798193d085e5b55ce380c5063ab4b24ccec401e3` |
| `tests/unit/relink_support.hpp` | `bda3adf4df1f20770469f7d8acf3db5d036d5f669e1e0af97cdaba407c324291` |
| `tests/unit/test_relink.cpp` | `eea897da6d62ba969b397cf3342043c49c20f89e4dacfa52ff59759dfee3585f` |
| `tests/unit/test_relink_blocked.cpp` | `64127d4ff764b4fcd9915a139b2d28e5031b2658986dc5c6e369319f902b63e5` |
| `tests/unit/test_relink_nostat.cpp` | `f384b46e5748ddb13a32cd34170546d5f05231a750bcb0c8887f43d56fa77b07` |
| `tests/unit/test_relink_status.cpp` | `457386c57cbedbb6f31a96ad70d531bf1e7762fbf064fb7d5cb39ac53f61fa9d` |

- Test release (`tmp/release-junit.xml`): totali 1081, fast 1063, slow 18, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 1063, fast 1063, slow 0, falliti 0
<!-- evidence:end -->
