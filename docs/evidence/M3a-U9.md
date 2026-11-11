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
| `docs/contracts/F-IO-08-ricollegamento.md` | `668c1e42622e805a9068334b26d8e0b817891d5ebdf6d6cf50e33bdf23ca730d` |
| `src/core/hash.cpp` | `cafc181b8501db004dbe56c9f4ba10ea481dd025fce467b93e330610fcc2613f` |
| `src/core/include/astro/hash.hpp` | `890e62d0d82bf0f9fdd154ccc83fae8e70f8543fe62f46a3fcaf5b2898b41613` |
| `src/core/include/astro/sqlite_dag_store.hpp` | `da0919b01c2c79fcd4778ba7bcd8ca57f60f60e7cce098fdb50f91d51732711b` |
| `src/core/sqlite_dag_store.cpp` | `72d97ae8f15069d3c4bbdf0c46b2ef81a1b9a50b4bc3a6d7d80f7ac3c6b0bd72` |
| `src/core/sqlite_project.cpp` | `7d37ca279a8e717ffd293a8a0c417955851cc763e9150b3cf91eb93c6cc5a73d` |
| `src/core/sqlite_relink.cpp` | `1394326d8151c68dc22d815af66a65817fc217ab6b72c0936316559fa5d76f0b` |
| `tests/fixtures/fs_probe.hpp` | `c14734822290c68094bb0f12798193d085e5b55ce380c5063ab4b24ccec401e3` |
| `tests/unit/relink_support.hpp` | `cea989ed39ee06874c7f68074207269d7235dbc1cf6d6c98a1cc666397b994b4` |
| `tests/unit/test_relink.cpp` | `5c76c008d7e18c35b575ebd3c668750da08f84d995b1ca95e22ec022c092f94d` |
| `tests/unit/test_relink_blocked.cpp` | `e25c319228fe26616f720611cf81e945fda0b4de30c22400ed479f4f157adace` |
| `tests/unit/test_relink_nostat.cpp` | `06a6df43b4b95dc499781a7aa499593cdae4f76106ade27ebea4bf6250c47c92` |
| `tests/unit/test_relink_status.cpp` | `13da8e17de4e6d3ce64ff2a407df1a473ac61f5da84e3666e07a69f0cf9210ec` |

- Test release (`tmp/release-junit.xml`): totali 1829, fast 1772, slow 57, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 1772, fast 1772, slow 0, falliti 0
<!-- evidence:end -->
