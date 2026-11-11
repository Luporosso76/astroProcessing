# M3.a, unità U4: job di import RAW, record del decoder, export

Stato: **chiusa**. Test: review FAIL al giro 2, arbitraggio con caso nuovo e review sul delta PASS. Codice: review FAIL al giro 2 (6 MED), arbitraggio del coordinatore (limite dichiarato nel contratto, 5 MED chiusi da un agente dei test, 9 mutazioni del coordinatore), review sul delta PASS con soli LOW.

| Campo | Valore |
|---|---|
| Unità | M3.a / U4 (test e codice) |
| Scopo | Tipo di job `io.raw_import` (BASE §3.1), record del decoder di ADR-08.b §2.9 in `details.decoder` e nella chiave del nodo, export F-IO-04 con `black_applied_by_decoder = none`, simboli vietati di §5 in `tools/check_offline.sh` |
| ID | F-IO-02, F-IO-04; T-M3BASE-11, T-M3BASE-12 (variante RAW), T-M3A-RAW-07, 09, 11, 12, 13, T-M3A-02 |
| Comando | Integrazione: build pulita, `tools/check.sh` e `ctest --preset release` completo (blocco generato). Mutazioni: `tools/mutate.sh` sulle patch di `tmp/u4arb-mut/` del worktree |
| Versioni | GCC 15.2.0, CMake 4.2.3, Ninja; LibRaw 0.22.3, CFITSIO 4.7.0, WCSLIB 8.10 (statiche, U12) |
| Ambiente | Linux 7.0.0-38-generic x86_64, 16 CPU; worktree `.claude/worktrees/api-u4` sulla base `9b5624d` |
| Autore e data | Test: agente Claude Sonnet 5.5 (API). Codice: agente Claude Sonnet 5.5 (API). Review: Claude Opus 5.5 (Argon fuori quota). Coordinatore: Claude Opus 5.5. 5 novembre 2026 |

## Decisioni del coordinatore

- La chiave del nodo include il record di §2.9 (contratto invariato). Il costruttore del nodo legge e decodifica il file una volta per ricavare il record (identificazione), poi `make_raw_import_node(digest, record)`; il job decodifica di nuovo nel worker prenotato e ferma con `raw_import.node_mismatch` se la chiave ricostruita differisce. Test che lega il percorso del job alla funzione: `RawImportRecord.T_M3A_RAW_12_job_node_key_equals_key_from_record_and_digest`.
- Limite dichiarato in ADR-08.b (§2.7, fine di §2.8, §7, riga «Emendamento U4 (5 novembre 2026)»): l'identificazione non passa dal pool, dallo scheduler né dal cancel; memoria e tempo limitati dai tetti di §2.1.
- Installazione di WCSLIB resa ripetibile (`cmake/third_party_libs.cmake`, `rm -f` di `libwcs.a` prima di `make install`), scoperta durante le mutazioni; commit separato `bc364d6`.

## Mutazioni (coordinatore)

| Patch | Cosa cambia | Esito |
|---|---|---|
| `raw07_buffer` | tolta la notifica `input.samples` del job | rilevata |
| `offline_args` | `test_raw_import` di nuovo fra i binari di `check_offline.sh` | rilevata (lettura del log: diagnosi «simbolo vietato»; `mutate.sh` esce 2 perché il test Python non stampa `CHECK-RESULT`) |
| `held_bytes_job` | budget del job senza i byte condivisi | rilevata |
| `held_bytes_decoder` | decoder che conta il solo file | rilevata |
| `identification_stop` | arresto dell'identificazione non rilanciato | rilevata |
| `peak_bytes` | picco senza la prenotazione del decoder | rilevata |
| `key_no_record` | record fuori dai parametri del nodo | rilevata |
| `black_applied` | `black_applied_by_decoder = subtracted` | rilevata |
| `details_moved` | record fuori da `details.decoder` | rilevata |

## Limiti

- Identificazione fuori dal pool (sopra).
- RAW-12: versione della libreria, stato del dato e parità dei margini dei DNG non cambiano per iniezione nel percorso reale; la chiave per campo è provata a livello di record (`make_raw_import_node`).
- Il ramo `output_bytes` del picco (`raw_import_job.cpp`, `std::max`) non è esercitato: un DNG con uscita maggiore di job + decoder resta per un'unità successiva.
- La mutazione `raw07_buffer` prova il conteggio delle notifiche, non il confronto dei campioni; il confronto discrimina alla lettura.
- `mutate.sh` classifica come infrastruttura i test Python che falliscono con `sys.exit("<diagnosi>")`.
- Fuzz reali `T-M3A-RAW-10` (`fuzz_real_0..7`): eseguiti nella release completa dell'integrazione (blocco generato).

## Review

- Test, giro 1 e 2: Claude Opus, FAIL (7 e 4 MED); arbitraggio: `.omc/artifacts/ask/review-u4tests-arbitrage-claude.md`, PASS.
- Codice, giro 1 e 2: Claude Opus, FAIL (BLOCCANTE di prenotazione nel pool corretto al giro 2; 6 MED); arbitraggio: `.omc/artifacts/ask/review-u4code-arbitrage-claude.md`, PASS con 9 LOW, di cui chiusi i LOW su contratto e commenti dei test.

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-08-decoder-pack.md` | `2356f8f7f730000b6e29b63b1e816c9be0000eaeb696569f3ecd09971ac5018b` |
| `src/core/CMakeLists.txt` | `61454d580a7ea49da1a4fc4d40469a1245a49f5fcc551451174bfd04b5284afa` |
| `src/core/include/astro/image.hpp` | `3b8fee06e9e5cc7be11ac2e415b2750a79c19e649b194050d571383f5bbc2aa6` |
| `src/core/include/astro/raw_decode.hpp` | `06f0e923e6462d599b83d59d06b9171e67648e4fd6b0bb3ecb6a76bad4e891e2` |
| `src/core/include/astro/raw_import_job.hpp` | `5a42733edb7fd35c59b0d9755fa5e0c3f622861e147d277153c2aa0b53b3e734` |
| `src/core/job_types.cpp` | `7d8c86945c9b59460586a681c538f51d7aea199a563a9f5d311bc44e79150dcc` |
| `src/core/raw_decode.cpp` | `f0dbed89885480f0f67b34fedc27f3e5a286ccdf4db1e2d742c69c365f31012d` |
| `src/core/raw_import_job.cpp` | `0b667bb77ac0d89b4002c0070bd78bfc31689ef498d80c494603316a3a888226` |
| `tests/u4/test_raw07_offline.py` | `9c9a95ea09f39204064ff7de3f1cc43b3082826d4df2e88fa04e740736157836` |
| `tests/unit/CMakeLists.txt` | `814de53b3ce7b39a30e78f688ca025707511e350a4a1bc01466f070fc1bb7cb5` |
| `tests/unit/raw_import_run.cpp` | `7dfa3731a61351d3b4e079d8906f7e3f9e404d57e894656353f5bce41ef71c65` |
| `tests/unit/raw_record_keys.hpp` | `ff32b3c8f5e0942397921048068e0cd13b0859bf11f5e81dae13c8f7e9169baf` |
| `tests/unit/test_raw_import.cpp` | `b0357db9ecf4a8b934d9b9db05ae925ca654601dad80410318c081e7fe973c6e` |
| `tests/unit/test_raw_import_record.cpp` | `b8e2b539dabe826a949434ba225b6750941a649731b5b12ccecaaf7a9ec98b7c` |
| `tools/check_offline.sh` | `9f9ada7e6c9f201ba5a921d7bc76e932e07c2f67ae4aa8ac9f83b69ad8762b71` |

- Test release (`tmp/release-junit.xml`): totali 1829, fast 1772, slow 57, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 1772, fast 1772, slow 0, falliti 0
<!-- evidence:end -->
