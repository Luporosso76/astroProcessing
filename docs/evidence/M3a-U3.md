# M3.a, unità U2b e U3: ora di acquisizione, avviso Fujifilm, decoder RAW robusto

Stato: **REVIEW FAIL al giro 2, tetto raggiunto: arbitraggio del coordinatore**. I rilievi aperti riguardano solo casi e asserzioni dei test (sotto, «Review»); nessun rilievo aperto sul codice di prodotto. Unità: U2b e U3 di `plans/M3a.md`, ID F-IO-02, F-IO-04. Proprietario: `decode_raw_buffer` in `src/core/raw_decode.cpp`. Contratto: `docs/contracts/ADR-08-decoder-pack.md` §2.1, §2.5, §2.6, §2.7, §2.8; fatti misurati in `docs/evidence/M3a-ADR08-U3.md` e `docs/evidence/M3a-ADR08-U4.md`. Test: T-M3A-RAW-01 (ora e fuso), T-M3A-RAW-05, 06, 08, 10.

| Campo | Valore |
|---|---|
| Unità | M3.a / U2b, U3 |
| Scopo | Verifica di `TZ=UTC0` e di `mktime` prima di `open_buffer`; avvisi di LibRaw ammessi (con `PARSEFUJI_PROCESSED`) e registrati; limiti `max_file_bytes` e `max_pixels`; `max_raw_memory_mb = M`; gestori di errori di dati e di progresso; tabella dei codici di ritorno; `omp_set_num_threads(1)`; prenotazione nel pool prima di `unpack` |
| Comando | Dev: `tools/check.sh` (`tmp/u3-check.sh`, `tmp/u3-verify-r2.sh`; log `tmp/u3-check.log`, `tmp/u3-check-r2.log`). Release: `tmp/u3-release.sh` (tutto il preset, fuzz compreso, log `tmp/u3-release-ctest.log`), `tmp/u3-verify-r2.sh` (preset release senza `slow`, log `tmp/u3-release-ctest-r2.log`). Fuzz sotto ASan/UBSan: `tmp/u3-fuzz-asan.sh` (`ctest --preset dev-full -L slow -R RAW-10 -j3`). Mutazioni: `python3 -S tmp/u3-mut/make_patches.py`, `bash tmp/u3-mut/run_all.sh` (un `tools/mutate.sh` per patch). Review: `tools/review.sh u3-round1 tmp/u3-facts-round1.md`, `tools/review.sh u3-round2 tmp/u3-facts-round2.md`. Blocco: `python3 -S tools/evidence.py docs/evidence/M3a-U3.md` |
| Input | Base `b4bcd98`; test di `tmp/u2b-u3-tests.patch` del checkout principale (altro agente), asserzioni invariate; FIX-SYN-DNG-01 (`tests/fixtures/syn_dng.hpp`); 8 file di PUB-RAW-01 in `data/public/raw-pixls-us/` tramite il collegamento temporaneo `data` → `/opt/workspace/astroProcessor/data`, sola lettura, SHA-256 verificati da `tests/unit/pub_raw_01.hpp` |
| Versioni | GCC 15.2.0, CMake 4.2.3; LibRaw 0.21.5 di sistema (`libraw_r`), libgomp di GCC; preset `release`, `dev` e `dev-full` (ASan e UBSan); codex-cli con `gpt-6-astra` per la review |
| Ambiente | Linux 7.0.0-38-generic x86_64, 16 CPU, worktree isolato, al massimo 4 job |
| Risultato | Vedi «Rossi prima e dopo», «Fuzz», «Mutazioni» e il blocco generato |
| Output | Log, junit, patch e artefatti citati sotto `tmp/` e `.omc/artifacts/ask/` |
| Ambito | Codice di §2.1, §2.5, §2.6, §2.7, §2.8 del decoder. Fuori: record del decoder e `process_tz` in provenance (§2.9, U4), job di import |
| Esito | Test U2b/U3 da non compilabili a verdi; dev 813 test e release 812 (senza `slow`) verdi dopo il giro 2; fuzz 9/9 verde in release e sotto ASan/UBSan; 24 mutazioni: 19 rilevate, 4 sopravvissute, 1 rilevata ma classificata errore dello strumento; review FAIL al giro 2 sui soli test. Arbitraggio del coordinatore: casi mancanti scritti dall'agente dei test (Sonnet sui crediti API, 0,71 $), `tools/mutate.sh` del coordinatore su m02, m06, m21, m22, m24 tutte rilevate; m10 equivalente in pratica; ADR-08.b emendato (si conserva la prima segnalazione di errore di dati) |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Modifiche

Codice di prodotto, 208 righe aggiunte e 11 tolte (`git diff --numstat -- src CMakeLists.txt`):

- `src/core/raw_decode.cpp`: `decode_raw_buffer(bytes, hooks, ctx)`. In ordine: stop `limit_exceeded`, `raw.max_file_bytes` sulla sola dimensione; `ASTRO_ASSERT` su `getenv("TZ") == "UTC0"` e su `mktime` di 2026-01-15 e 2026-07-15 alle 12:00 (`tm_isdst = -1`) uguale a 1768478400 e 1784116800; `omp_set_num_threads(1)`; gestori `set_dataerror_handler` (prima segnalazione: file se non nullo, offset) e `set_progress_handler` (valore non nullo con cancel richiesto) installati prima di `open_buffer`; codici di `open_buffer` e `unpack` passati dal gancio `libraw_return`; dopo `after_open` lo stop `raw.max_pixels` su `raw_width · raw_height` a 64 bit, limite incluso; `rawparams.max_raw_memory_mb = M`; prenotazione `M · 2^20 + 6 · width · height` nel pool del contesto con il token di cancel, preceduta dallo stop `budget`, `sched.job_exceeds_budget` se `bytes.size()` più la prenotazione supera il budget; gancio `before_unpack_plan` col valore letto da LibRaw; dopo `unpack` lo stop `integrity`, `raw.data_error` se il gestore è stato chiamato (contesto: funzione, valore, testo di LibRaw, file, offset), poi la tabella dei codici; `process_warnings` letto dopo `unpack`, ogni bit fuori dai 9 ammessi ferma con `unsupported`, `raw.warning` e i nomi dei bit (`bit_<n>` per i bit sconosciuti); i bit ammessi in `decoder_state::process_warnings`. La prenotazione e lo stato dei gestori sono dichiarati prima dell'oggetto LibRaw, così si rilasciano dopo `raw_image`.
- `src/core/include/astro/raw_decode.hpp`, `src/core/include/astro/input_format.hpp`: interfaccia uguale alle intestazioni simulate dell'agente dei test (`raw_limits::max_pixels`, `raw_decode_context`, `raw_unpack_callbacks`, `raw_unpack_plan`, ganci `libraw_return`, `with_handlers`, `before_unpack_plan`, `raw_probe_fields::{raw_width, raw_height}`, `raw_unpack_fields::process_warnings`), con commenti.
- `src/core/include/astro/image.hpp`: `decoder_state::process_warnings` (`std::uint32_t`), confrontato nell'uguaglianza.
- `CMakeLists.txt`: `find_package(OpenMP REQUIRED COMPONENTS CXX)`; `src/core/CMakeLists.txt`: `OpenMP::OpenMP_CXX` PRIVATE in `astrocore`.

Test (scritti da un altro agente, asserzioni invariate):

- `tests/unit/CMakeLists.txt`: tolte le opzioni `ASTRO_U2B_RED_TESTS` e `ASTRO_U3_RED_TESTS`; `find_package(OpenMP)` spostato alla radice; `ASTRO_RAW_PIXLS_DIR=${PROJECT_SOURCE_DIR}/data/public/raw-pixls-us` nell'ambiente di `T-M3A-RAW-08.{a_product,b_libraw2}.*` e `T-M3A-RAW-10.*` (richiesta da `pub_raw_01.hpp`, assente nella patch dei test); le cartelle dei quattro eseguibili nuovi in `T-M2F-01.offline`.
- `tests/unit/test_raw_decode_u2b.cpp` riga 2: tolto il riferimento all'opzione (rilievo LOW del giro 1). Nessun'altra modifica ai file della patch.

`codegraph impact decode_raw_buffer`: chiamanti solo nei test RAW; `codegraph impact decoder_state`: solo `image.hpp`.

## Rossi prima e dopo

Prima del codice i test di U2b e U3 non compilavano contro il prodotto di U2 (interfaccia assente: `raw_decode_context`, ganci, `process_warnings`) ed erano dietro le opzioni spente.

| Test ctest | Prima | Dopo |
|---|---|---|
| `RawDecodeU2b.*`, `RawDecodeU2bDeathTest.*` (T-M3A-RAW-01, 05) | non compilabili | verdi |
| `RawRobust.T_M3A_RAW_05_*`, `RawRobust.T_M3A_RAW_06_*` | non compilabili | verdi |
| `T-M3A-RAW-08.a_product.*`, `.b_libraw2.*`, `.c_control` (19 test) | non compilabili | verdi |
| `T-M3A-RAW-10.fuzz_*` (9 test, `slow`) | non compilabili | verdi |
| `RawDecode.T_M3A_RAW_01_timestamp_*` (esatti, `main_utc0.cpp`) | verdi anche col prodotto di U2 sotto `TZ=UTC0` | verdi |

Suite: dev 813 test, 0 falliti (`tmp/u3-check-r2.log`, lint ok); release 812 test senza `slow`, 0 falliti (`tmp/u3-release-ctest-r2.log`); release completo del giro 1, 828 test con i 9 fuzz, 0 falliti (`tmp/u3-release-ctest.log`). `trace.py check --stage M2`: 38 ID non coperti sul junit dev, come prima (il junit dev non contiene i `slow`).

## Fuzz (T-M3A-RAW-10)

5 000 mutazioni per seme, seed fissi; un fallimento è un'eccezione diversa da `stop_error`, una prenotazione non rilasciata o un'immagine non valida; un crash o un abort fa fallire il test.

| Esecuzione | Esito | Tempi |
|---|---|---|
| Release, nel ctest completo, 4 processi (`tmp/u3-release-ctest.log`, `tmp/release-junit.xml`) | 9/9 verdi, 0 crash, 0 abort | sintetico 8 s; reali 13 s (X-E1) … 2 059 s (Olympus E-M10) |
| ASan/UBSan, `dev-full`, 3 processi (`tmp/u3-fuzz-asan.log`, `tmp/u3-fuzz-asan-junit.xml`), 4 618 s in tutto | 9/9 verdi, nessuna riga `AddressSanitizer` o `runtime error` | sintetico 153 s; reali 35 s … 2 406 s |

Riepiloghi per seme estratti dai junit in `tmp/u3-fuzz-summary.txt` e `tmp/u3-fuzz-asan-summary.txt`: 0 fallimenti in ognuno. Immagini prodotte sui reali (release): Canon 511, Nikon 286, Sony 4 344, X-A1 3 269, Olympus 369, Pentax 804, Panasonic 2 589, X-E1 4 (mutazioni che tolgono `filters = 9`). Il fuzz release è stato eseguito sul codice del giro 1; la correzione del giro 2 cambia solo il confronto col budget.

## Mutazioni

`tools/mutate.sh` sul preset `release`, una patch per controllo, generate da `tmp/u3-mut/make_patches.py`; sorgente ripristinato e verificato con `cmp` a ogni passaggio. Riepiloghi `tmp/u3-mut/summary-pass1.txt` … `summary-pass5.txt`, log per patch in `tmp/u3-mut/`.

| Patch | Mutazione | Esito |
|---|---|---|
| `m01` | codice dello stop di `max_file_bytes` | RILEVATA |
| `m02` | `>` → `>=` su `max_file_bytes` | SOPRAVVISSUTA: nessun caso esattamente a 1 GiB |
| `m03`, `m04`, `m05` | `TZ` non verificata; riferimento di gennaio o di luglio saltato | RILEVATA ×3 |
| `m06` | `omp_set_num_threads(1)` tolto | rilevata (`T-M3A-RAW-08.a_product.syn`: «FALLITO: omp_set_num_threads(1) va chiamata …»), ma `tools/mutate.sh` esce con 2: `raw_threads` non stampa il marcatore `CHECK-RESULT` |
| `m07`, `m08` | limite di pixel esclusivo; prodotto a 32 bit | RILEVATA ×2 |
| `m09`, `m11` | M senza le 8 righe; `max_raw_memory_mb` non impostato | RILEVATA ×2 |
| `m10` | `max(1, …)` tolto da M | SOPRAVVISSUTA: M = 0 solo con `raw_width = 0`, che `open_buffer` non accetta |
| `m12`, `m13` | controllo del budget tolto; prenotazione senza i campioni f32 e la maschera | RILEVATA ×2 |
| `m14`, `m15` | errore di dati ignorato; progresso che ignora il cancel | RILEVATA ×2 |
| `m16`, `m17`, `m18` | `PARSEFUJI_PROCESSED` non ammesso; avvisi non controllati; avvisi non registrati | RILEVATA ×3 |
| `m19`, `m20` | riga `LIBRAW_BAD_CROP` della tabella; codice di `unpack` non passato dal gancio | RILEVATA ×2 |
| `m21` | prima segnalazione di errore di dati sovrascritta | SOPRAVVISSUTA: il test accetta il primo o il secondo offset |
| `m22` | `set_dataerror_handler` non registrato | SOPRAVVISSUTA: i test chiamano il gestore tramite il gancio |
| `m23` | `set_progress_handler` non registrato | RILEVATA |
| `m24` | budget senza i byte del file | SOPRAVVISSUTA: nessun test tiene prenotato l'input |

Prime versioni di `m15` e `m17` non compilavano (`-Werror`); riscritte in forma compilabile e rieseguite.

## Scelte da verificare

1. **Nessun pool nel contesto → nessuna prenotazione** (`reserved_bytes = 0` nel piano). Il job di import di U4 deve passare il pool; senza, la prenotazione di §2.8 non avviene.
2. **Budget confrontato con `bytes.size()` più la prenotazione** (rilievo 1 del giro 1): il job tiene già i byte del file; altrimenti `reserve` attenderebbe all'infinito una quantità che il pool non può dare. Con un chiamante che non ha prenotato il file nello stesso pool il controllo è più stretto del necessario di `bytes.size()`.
3. **Gestori installati prima di `open_buffer`**: un errore di dati segnalato durante `open_buffer` riuscito ferma la decodifica dopo `unpack` (come dice §2.5), col contesto `funzione = unpack`; il contesto non distingue la fase della segnalazione.
4. **Prima segnalazione di errore di dati conservata** (file e offset); le successive si ignorano.
5. **Bit di avviso sconosciuti** nominati `bit_<n>` nel contesto, insieme al valore decimale dei bit rifiutati.
6. **`ctx.max_pixels`** è un campo del contesto con valore predefinito `raw_limits::max_pixels`: un chiamante può impostarne un altro (i test lo abbassano).

## Limiti

- Rilievi della review aperti sui test (non modificabili dall'autore del codice, segnalati per l'agente dei test): nessun caso con l'input prenotato nel pool (`m24`); gestore di errori di dati provato solo tramite il gancio, non la registrazione in LibRaw (`m22`); nessun caso esattamente a `max_file_bytes` (`m02`); picchi di RSS di T-M3A-RAW-06 in `RecordProperty`, assenti dal junit di ctest perché l'XML di GoogleTest non è abilitato; marcatore `CHECK-RESULT` assente in `raw_threads.cpp` e `fuzz_raw_decode.cpp` (`m06`).
- Il riepilogo del fuzz sintetico nei junit è troncato da ctest (output dei test superati): sono visibili 11 semi sintetici; l'esito del test (uscita 0) copre tutti i semi.
- Il fuzz sotto ASan/UBSan è partito sui binari del giro 1; la ricompilazione del preset dev del giro 2 è avvenuta durante l'esecuzione, quindi i semi avviati dopo hanno usato il codice del giro 2.
- H6 (§2.7) resta non provata: `T-M3A-RAW-08.b_libraw2.*` registra «H6 non provata» sui campioni disponibili, come nella sonda di U3.
- Il collegamento temporaneo `data` è stato tolto alla fine; i test su dati reali richiedono `data/public/raw-pixls-us/` nel checkout.

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-u3-round1-20261030-2038.md` | `tmp/u3-round1.diff` | FAIL | 5 MED: budget senza i byte dell'input (corretto nel prodotto); gestore di errori di dati provato solo via gancio; limite `max_file_bytes` esatto non provato; RSS non registrati; fuzz ASan incompleto (completato). 2 LOW: `CHECK-RESULT` assente; commento obsoleto in `test_raw_decode_u2b.cpp` (corretto) |
| 2 | `.omc/artifacts/ask/codex-u3-round2-20261030-2139.md` | `tmp/review-delta-u3-round2.diff` | FAIL | 4 MED, tutti sui test: caso con input prenotato (`m24`), registrazione del gestore (`m22`), `max_file_bytes` esatto (`m02`), RSS; 2 LOW: `CHECK-RESULT`, conteggio dei riepiloghi del fuzz nei fatti (19 semi e 8 totali, non 27 semi: corretto qui). Tetto di 2 giri raggiunto: arbitraggio del coordinatore |

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `CMakeLists.txt` | `0e29f2648037ec69e84817b30bd02d327b60e24339719d9744fdeaf926b556f1` |
| `docs/contracts/ADR-08-decoder-pack.md` | `b5abcafc55e824f9d7b2ac19a08fae98e66e70e125e33561ee38a8841c93ae98` |
| `src/core/CMakeLists.txt` | `bef3d72e6dc57ba4fc51eb21d27c640dcb3852b6054ec1853dd40609abef94b6` |
| `src/core/include/astro/image.hpp` | `2ae3c4cc2192a8c3b6822ec390dcde0e62dbc2282178162a744d98a0ef20f626` |
| `src/core/include/astro/input_format.hpp` | `e30c81080600aec605fd3730bad533e1b77774d531a26441e4269990b986831f` |
| `src/core/include/astro/raw_decode.hpp` | `fcf18c63c1701e15b61ba50ecc24c443f9fe5cb4b6661386b5e765b36362ba9d` |
| `src/core/raw_decode.cpp` | `c256ae77394996d6f763ccbf02fca67c5fc11d0e85502685299a837f8de17b6c` |
| `tests/unit/CMakeLists.txt` | `65928a9dbb372c3c5ab3391437d077979aa1a2b00ba98ecfb3956ac8ce361af9` |
| `tests/unit/fuzz_raw_decode.cpp` | `77b64344305fea6c59e90d9e3da126c45487fc8b75bd6a0c612981f4620846bc` |
| `tests/unit/main_utc0.cpp` | `c09d5a99f17d099218f9de3a664622749534fe3c3379c81143d669c1ac334081` |
| `tests/unit/pub_raw_01.hpp` | `cd18843a0885f569d49f4c2c34852b3afbf93df329e9967b04a331204de12c24` |
| `tests/unit/raw_threads.cpp` | `cf7b28ff27d1b893b081b3f1c1291e0d26ef9c527d1dad0c3616d2c5ed2fded2` |
| `tests/unit/test_raw_decode.cpp` | `9836b507fe29d2c51793288674d1d91fae061d273d0c814111107e006e8d8570` |
| `tests/unit/test_raw_decode_u2b.cpp` | `2abd37934df5eeb669eb141ee86b79e21a1f358bde3e693ccd288117723d43ce` |
| `tests/unit/test_raw_robust.cpp` | `c1b188f6e5054694a46150a591ba3ab57d5710a0c2da6bab500c32333276fa1b` |

- Test release (`tmp/release-junit.xml`): totali 878, fast 862, slow 16, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 863, fast 863, slow 0, falliti 0
- Test release-giro2 (`tmp/release-junit-r2.xml`): totali 812, fast 812, slow 0, falliti 0
- Test fuzz-asan (`tmp/u3-fuzz-asan-junit.xml`): totali 9, fast 0, slow 9, falliti 0
<!-- evidence:end -->
