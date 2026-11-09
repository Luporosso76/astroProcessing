# M3.a, unità U5: G-OFFSET, stato dell'offset e voce `light_offset`

Stato: **chiusa**. Test: review PASS al giro 2. Codice: review FAIL al giro 2 (2 MED), arbitraggio del coordinatore (test portati ai quattro campi di `decoder_identity` e casi di `calib.decoder_state` scritti da un agente dei test; costruttore a due argomenti tolto; tetto del motivo del nero condiviso con `validate_image`), review sul delta FAIL per un MED sul tetto senza prova, chiuso con un caso di confine e due mutazioni.

| Campo | Valore |
|---|---|
| Unità | M3.a / U5 (test e codice) |
| Scopo | Stato dell'offset `removed` / `known_not_removed` / `unknown` per catena del light; `OFFSET` FITS resta `unknown`; piedistallo → `calib.pedestal`; voce `light_offset` col nero per posizione del periodo `lcm(2, rows) × lcm(2, cols)` (H3, ADR-08.b §2.4), `black_applied_by_decoder` e identità del decoder; mai sottratto (C-01.b §1, §2, §10; ADR-16.b §b.5, §b.7, §b.10) |
| ID | G-OFFSET; T-M3A-C01-01, T-M3A-C01-03 |
| Comando | Integrazione: build pulita, `tools/check.sh` e `ctest --preset release` completo (blocco generato). Mutazioni: `tools/mutate.sh` (patch in `tmp/u5mut/` e `tmp/u5arb-mut/` del worktree) |
| Versioni | GCC 15.2.0, CMake 4.2.3, Ninja |
| Ambiente | Linux 7.0.0-38-generic x86_64, 16 CPU; worktree `.claude/worktrees/api-u5c` sulla base di U4 |
| Autore e data | Test e codice: agenti Claude Sonnet 5.5 (API), distinti. Review: Claude Opus 5.5 (Argon fuori quota). Coordinatore: Claude Opus 5.5. 5 novembre 2026 |

## Interfaccia

`src/core/include/astro/calibration_analysis.hpp` è condiviso con U6 e U7: `decoder_identity` ha i quattro campi «Libreria» del record di U4 (`libraw_version`, `libraw_version_number`, `libraw_capabilities`, `libraw_tarball_sha256`), come presuppongono i test di U6 e U7. U5 definisce solo `light_chain`, `offset_state`, `decoder_identity` e i propri tipi (`decoder_black_period`, `offset_remover`, `light_offset_inputs`, `light_offset_entry`); il resto dell'analisi resta a U6 e U7.

## Mutazioni

| Patch | Cosa cambia | Esito |
|---|---|---|
| `state` | stato dell'offset sbagliato per `dark_sub_partial` | rilevata |
| `offset` | `OFFSET` FITS letto come noto | rilevata |
| `lcm` | periodo senza `lcm(2, ·)` | rilevata |
| `subtract` | campione del light modificato dalla funzione | rilevata |
| `decoder_state` (coordinatore) | tolto il controllo di coerenza dello stato | rilevata |
| `cap` (coordinatore) | tetto `>` → `>=` | rilevata |
| `cap_removed` (coordinatore) | tolto il tetto delle celle | rilevata |

## Limiti

- Release dell'integrazione U5 + U9: 235 test falliti in un minuto di disco pieno (549 GB esauriti dalle build dei worktree), tutti di scrittura su disco; rieseguiti con `ctest --preset release --rerun-failed` dopo aver liberato spazio: 235 su 235 verdi; il junit `tmp/release-junit.xml` unisce i due giri.
- `decoder_identity` ha ancora il costruttore vuoto: un'identità con campi vuoti è accettata da `make_light_offset_entry` (LOW della review, per U6, che confronta l'identità).
- Il contesto dello stop `calib.decoder_state` non riporta gli indici colore.
- La mutazione `decoder_state` porta a letture fuori indice («Subprocess aborted») in release: rilevata, ma per un crash e non per un'asserzione.
- Codici d'errore introdotti dai test e non elencati nel contratto: `calib.chain_input`, `calib.chain_input_not_applicable`, `calib.decoder_identity` (`invalid_input`).

## Review

- Test, giro 1 FAIL (2 MED), giro 2 PASS.
- Codice, giro 1 e 2 FAIL (2 MED); arbitraggio: `.omc/artifacts/ask/review-u5code-arbitrage-claude.md` FAIL per il MED sul tetto, chiuso dal caso `LightOffsetEntry.T_M3A_C01_03_state_pattern_cell_cap_boundary` e dalle mutazioni `cap` e `cap_removed`.

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `src/core/CMakeLists.txt` | `5b41d8df67ae8a6f3fa5a5d585f3d1369e8711ce535c1b7b6c1ac22090b7cd5f` |
| `src/core/calibration_analysis.cpp` | `bf537426e1c7e07f3563f279b9d98504a4ef3f051aecaf18d8769494ff149966` |
| `src/core/image.cpp` | `ac289188bb822477092d8ec410becc65e1fa024290c598f75c9abe79b0e3a4fe` |
| `src/core/include/astro/calibration_analysis.hpp` | `479f19a944752ce2682e13c8010c1b60f68a534fff45202affe9bd3cce340eb2` |
| `src/core/include/astro/image.hpp` | `10df60c894f27ecf61eb43cc8d4efc8d0013bce394bbb044c3711fcf5f327537` |
| `tests/unit/CMakeLists.txt` | `245405ec419284a27b5c3fc4ef39a08c05684c6a925f5b32ce0a2d5edefdb3ed` |
| `tests/unit/test_calibration_offset.cpp` | `28335edf3b052a30889adcba1612c8c41dc3f477bc0021a5653df4b64f801a68` |
| `tests/unit/test_u5_spy.cpp` | `cad6b9f7c1035859f6984088f273128c2e8e42bbc34042aab6a08ad3f890b960` |

- Test release (`tmp/release-junit.xml`): totali 1221, fast 1199, slow 22, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 1199, fast 1199, slow 0, falliti 0
<!-- evidence:end -->
