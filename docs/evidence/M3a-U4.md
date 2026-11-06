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
| `docs/contracts/ADR-08-decoder-pack.md` | `e9f7a6f719f7f406de8a5fab80db9d6b43f6b07fb126d9ac53c4437052f38168` |
| `src/core/CMakeLists.txt` | `0dffca21be9c9367d55971c144601d5b6872f22c2f0beeb6b33deeab6fb529de` |
| `src/core/include/astro/image.hpp` | `4f6007bbe26d4ab7946fd413451017bc73e6227ef0cb3255d16998106151b116` |
| `src/core/include/astro/raw_decode.hpp` | `ef65ebd958672813ff0bc931b2e2faf84fe58191cd8543623a5f8d10ef496a14` |
| `src/core/include/astro/raw_import_job.hpp` | `0e8b7a356955bd6f6e5f83ccf40b3c62ddbbddbb12db7592ecc038a76c75255c` |
| `src/core/job_types.cpp` | `5041b210e2aeb2f3c76f1f81e5fbb02932505886e55df9f4d2754953a047ecdc` |
| `src/core/raw_decode.cpp` | `7f3aa00829bf1911fe9400f87626ff2df01fd95a5cb1cbc6eea7b1bd75f42042` |
| `src/core/raw_import_job.cpp` | `f18c33016ec80c8866cd845d641bf2b53fef762e012702c2393e73cccc79d8ae` |
| `tests/u4/test_raw07_offline.py` | `9c9a95ea09f39204064ff7de3f1cc43b3082826d4df2e88fa04e740736157836` |
| `tests/unit/CMakeLists.txt` | `d757bc22a9c63246d14d3e63180545d0fc3874340cddb088ca85763e9861cf26` |
| `tests/unit/raw_import_run.cpp` | `7dfa3731a61351d3b4e079d8906f7e3f9e404d57e894656353f5bce41ef71c65` |
| `tests/unit/raw_record_keys.hpp` | `d2eb00da645acd8b7036adb978479d4eb0578ffd6a0444ceab455f4fd49f8c6d` |
| `tests/unit/test_raw_import.cpp` | `5284ec3d606223fa2261ddc928af0b859e1f45c45b5a34472b6ba25571aa0933` |
| `tests/unit/test_raw_import_record.cpp` | `476049bd106a85d443abbc49f9af05d27e3150300aeef6d9964206db6cd918e6` |
| `tools/check_offline.sh` | `9f9ada7e6c9f201ba5a921d7bc76e932e07c2f67ae4aa8ac9f83b69ad8762b71` |

- Test release (`tmp/release-junit.xml`): totali 979, fast 961, slow 18, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 961, fast 961, slow 0, falliti 0
<!-- evidence:end -->
