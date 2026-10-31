# M3.a, unità U1: tipi del decoder

Stato: **REVIEW PASS** al giro 2. Unità: U1 di `plans/M3a.md`, ID F-IO-02 e F-IO-04. Contratto: `docs/contracts/ADR-03-pixel-maschere.md` parte `.b`, §b.1, §b.2, §b.8, §b.9 righe 1-2, §b.10 (T-M3A-TYP-01, 02, 03 e le coppie `decoder_state` di T-M3A-TYP-12), §b.15 (coppia «solo provenienza»).

| Campo | Valore |
|---|---|
| Unità | M3.a / U1 |
| Scopo | Origine `decoder` dei metadati, `capture_time_libraw` con keyword `ASTRTSTP`, tipo `decoder_state` con i suoi invarianti in `validate_image`, rifiuto dello scrittore FITS, `equivalent_content` sul nuovo campo |
| Comando | Dev: `tools/check.sh` (log `tmp/u1-check-dev.log`, fermo al ctest per i dati reali assenti), poi `tmp/u1-after-check.sh`: per `dev` e `release` `cmake --preset`, build con 4 job, `ctest --preset <preset> --output-junit tmp/<preset>-junit.xml` con 4 job; `tools/lint.sh`; `python3 -S tools/check_job_types.py .`; `python3 -S tools/trace.py check --stage M2 ...` sui due junit. Rosso: `tmp/u1-red/red.sh` (il test compilato con `-fsyntax-only` contro `image.hpp` di `HEAD`). Mutazioni: `python3 -S tmp/u1-mut/make.py`, poi `tmp/u1-mut/run.sh` (un `tools/mutate.sh` per patch). Blocco: `python3 -S tools/evidence.py docs/evidence/M3a-U1.md` |
| Input | Codice a `0590a5e` più i test di `tmp/u1-tests.patch` (scritti da un altro agente su `57013da`) e le modifiche elencate sotto; fixture sintetiche della suite (`syn_fits.hpp`) |
| Versioni | GCC 15.2.0, CMake 4.2.3; preset `release` e `dev` (ASan e UBSan) |
| Ambiente | Linux 7.0.0-38-generic x86_64, worktree isolato senza il collegamento `data` ai dati reali, 4 job |
| Risultato | Vedi «Rossi prima e dopo» e il blocco generato |
| Output | Log, junit e patch sotto `tmp/` citati nel comando |
| Ambito | §b.1, §b.2, §b.9 righe 1-2 di ADR-03.b. Fuori: il decoder che riempie `decoder_state` (U2), i record del decoder e dell'import (U4), `demosaic`, `variance_scope`, `coverage`, `measure` e le altre keyword `ASTRD*`/`ASTRVSCP` di §b.8 (M3.d, M3.g, M3.c) |
| Esito | PASS dei test: T-M3A-TYP-01, 02, 03 e 12 (coppie `decoder_state`) da non compilanti a verdi, 33 casi `DecoderTypes`; nessun altro test cambiato oltre i dati reali assenti; 39 mutazioni su 39 rilevate; review PASS al giro 2 senza rilievi aperti |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Modifiche

Codice di prodotto (`src/core/`), 8 file modificati, circa 160 righe:

- `include/astro/image.hpp`, `image.cpp`: `metadata_origin::decoder` e `to_string(metadata_origin)`; `frame_metadata::capture_time_libraw` (`std::optional<metadata_value<std::int64_t>>`); `decoder_window`, `decoder_linearization` (`identity`, `decoder_curve`), `decoder_state` con i campi di §b.2 e uguaglianza che non confronta `source`; `image::decoder_state` (`std::optional<astro::decoder_state>`); `equivalent_content` confronta `decoder_state`; `validate_image` verifica gli invarianti di §b.2 (`invalid_input`, `image.decoder_state`, contesto `campo`): layout `cfa`, fase presente e `raw`, origine `decoder`, finestra uguale alla geometria e dentro il buffer grezzo in aritmetica a 64 bit, motivo del nero `rows · cols` (64 bit) ≤ 4098 con una sola dimensione nulla vietata e dimensione del vettore uguale, impronta della curva presente se e solo se `decoder_curve`, `cfa_color_index` coerente con pattern e offset (0 sul rosso, 2 sul blu, 1 o 3 sui verdi).
- `fits.cpp`, `fits_blocks.cpp`, `fits_internal.hpp`: `reject_decoder_state` (`unsupported`, `fits.decoder_state`) all'inizio di `encode_fits` e del costruttore di `fits_stream_writer`, prima di ogni buffer.
- `fits_header.cpp`: `ASTRTSTP` interpretata (lettura con `try_integer`, senza coercizione da reale; scrittura come intero; confronto in `check_header_matches`).
- `import_job.cpp`, `calibration_internal.hpp`: il nome dell'origine nei report passa da `to_string(metadata_origin)`; prima un'origine diversa da `header` diventava `declared`.

Test:

- `tests/unit/test_decoder_types.cpp` (dalla patch dei test): asserzioni invariate; aggiunti tre casi: `T_M3A_TYP_01_origin_names_are_distinct`, `T_M3A_TYP_02_fits_stream_writer_refuses_an_image_with_decoder_state`, `T_M3A_TYP_03_astrtstp_card_in_conflict_with_the_field_stops_the_writer`.
- `tests/unit/CMakeLists.txt`: tolta l'opzione `ASTRO_U1_RED_TESTS`; il binario è nella build di default e nella scansione di `T-M2F-01.offline`.

Impatto letto con `codegraph impact` prima delle modifiche: `metadata_origin`, `frame_metadata`, `validate_image`, `equivalent_content`, `encode_fits` toccano i test di `test_image`, `test_fits`, `test_inventory`, `test_calibration*`, `test_master`, `test_import_real` e `m2e_workflow`: tutti nel ctest completo di entrambi i preset.

## Rossi prima e dopo

Prima del codice `test_decoder_types.cpp` non compila contro il prodotto: `tmp/u1-red/compile.log`, 137 errori (`image` senza `decoder_state`, `metadata_origin` senza `decoder`, `frame_metadata` senza `capture_time_libraw`).

| Test ctest | Prima | Dopo |
|---|---|---|
| `DecoderTypes.T_M3A_TYP_01_*` (7 casi) | non compila | verde |
| `DecoderTypes.T_M3A_TYP_02_*` (17 casi) | non compila | verde |
| `DecoderTypes.T_M3A_TYP_03_*` (5 casi) | non compila | verde |
| `DecoderTypes.T_M3A_TYP_12_*` (4 casi) | non compila | verde |

Suite completa: release 666 test, 4 falliti, tutti su dati reali assenti nel worktree (`Fits.REAL_DATA_PUB_LFC_01_light_is_read_with_documented_digest_and_header`, `T-M2E-13.real_palomar_lfc`, `T-M2R2-09.real_palomar_product_masters`, `T-M2R1-09.public_samples`); dev 660 test, 1 fallito (`Fits.REAL_DATA_PUB_LFC_01_...`). Lint e registro dei tipi verdi; `trace.py check` sul junit release: 24 ID non coperti, gli stessi dei dati reali assenti.

## Mutazioni

`tools/mutate.sh` sul preset `release`, regex `DecoderTypes` (33 casi), una patch per invariante o controllo, baseline verde, sorgente ripristinato e verificato con `cmp`. Riepiloghi `tmp/u1-mut/summary-pass1.txt` e `tmp/u1-mut/summary-pass2.txt`, log per patch in `tmp/u1-mut/`.

| Patch | Mutazione | Esito |
|---|---|---|
| `m01-layout` | layout non verificato | RILEVATA |
| `m02-phase-absent` | fase assente accettata | RILEVATA |
| `m03-phase-value` | fase diversa da `raw` accettata | RILEVATA |
| `m04-source` | origine di `source` non verificata | RILEVATA |
| `m05-window-width`, `m06-window-height` | larghezza, altezza della finestra non confrontate | RILEVATA, RILEVATA |
| `m07-window-x-32bit`, `m08-window-y-32bit` | somme della finestra a 32 bit | RILEVATA, RILEVATA |
| `m09-window-x-boundary`, `m10-window-y-boundary` | `>` diventa `>=` | RILEVATA, RILEVATA |
| `m11-pattern-one-zero` | una sola dimensione nulla accettata | RILEVATA |
| `m12-pattern-limit-removed`, `m13-pattern-limit-4097`, `m14-pattern-limit-4099` | limite 4098 tolto o spostato | RILEVATA ×3 |
| `m15-pattern-size` | dimensione del vettore non confrontata | RILEVATA |
| `m16-pattern-product-32bit` | prodotto `rows · cols` a 32 bit | RILEVATA |
| `m17-curve-iff`, `m18-curve-removed` | impronta solo in una direzione, o non verificata | RILEVATA, RILEVATA |
| `m19-cfa-green-only-1`, `m20-cfa-green-any` | verde solo 1; verde con qualunque indice | RILEVATA, RILEVATA |
| `m21-cfa-offset-ignored`, `m22-cfa-xy-swapped` | offset ignorati; offset scambiati | RILEVATA, RILEVATA |
| `m23-cfa-check-removed` | coerenza degli indici non verificata | RILEVATA |
| `m38-cell-grbg-wrong`, `m39-cell-bggr-wrong` | tabella del pattern sbagliata | RILEVATA, RILEVATA |
| `m24-validate-not-called` | `validate_image` non verifica lo stato | RILEVATA |
| `m25-equivalent-without-state` | `equivalent_content` senza `decoder_state` | RILEVATA |
| `m26-equality-with-source` | uguaglianza che confronta `source` | RILEVATA |
| `m27-equality-without-index`, `m28-equality-without-curve` | uguaglianza senza un campo | RILEVATA, RILEVATA |
| `m29-origin-name` | `decoder` riportato come `declared` | RILEVATA |
| `m30-encode-accepts-state`, `m31-reject-noop` | `encode_fits` accetta lo stato | RILEVATA, RILEVATA |
| `m32-stream-accepts-state` | `fits_stream_writer` accetta lo stato | RILEVATA |
| `m33-astrtstp-coerced` | `ASTRTSTP` letta con coercizione da reale | RILEVATA |
| `m34-astrtstp-uninterpreted` | `ASTRTSTP` fuori dalle keyword interpretate | RILEVATA |
| `m35-astrtstp-not-written`, `m36-astrtstp-not-read` | non scritta; non letta | RILEVATA, RILEVATA |
| `m37-astrtstp-not-checked` | conflitto card/campo non verificato | RILEVATA |

Al primo passaggio `m24`, `m31` e `m33` non compilavano (`-Werror`: funzione o parametro inutilizzati); riscritte in forma compilabile, sono state rieseguite con `m25` e `m36`, rigenerate dopo un ritocco di formattazione delle righe che mutano.

## Scelte

- **Uguaglianza di `decoder_state` senza `source`.** `metadata_value` già confronta il solo valore; `decoder_state` fa lo stesso, così `image ==` e `equivalent_content` trattano origine e keyword come provenienza (§b.1, §b.15).
- **`ASTRTSTP` fuori dalla tabella `for_each_field`.** La tabella legge gli interi con `as_integral`, che accetta un reale di parte frazionaria nulla (`XORGSUBF`, `YORGSUBF`); §b.8 chiede il tipo senza coercizione, quindi `ASTRTSTP` ha la sua lettura con `try_integer`. Il comportamento delle altre keyword intere non cambia.
- **Anche lo scrittore in streaming rifiuta lo stato.** Copia i campi dell'header senza `decoder_state`: senza il controllo scriverebbe l'immagine perdendo lo stato.
- **Nome dell'origine nei report.** Le due espressioni che riducevano ogni origine diversa da `header` a `declared` passano da `to_string(metadata_origin)`.

## Limiti

- Nessun produttore di `decoder_state` esiste ancora: il decoder è U2. I tipi sono provati su immagini costruite nei test.
- `capture_time_libraw` non entra nei report di import, del nodo, della calibrazione e del master (`import_job.cpp`, `node.cpp`, `calibration_internal.hpp`, `master.cpp`): quali campi vadano nel record del decoder lo decide U4 (ADR-08.b §2.9). Una card `ASTRTSTP` letta da FITS non compare più fra le keyword non interpretate.
- Un'operazione che copiasse un'immagine con `decoder_state` cambiandone la fase si ferma su `image.decoder_state`; nessuna operazione esistente riceve oggi immagini con lo stato.
- I 4 test su dati reali non girano in questo worktree, che non ha il collegamento `data`: vanno rieseguiti nel checkout principale. Per la stessa ragione `tools/check.sh` si ferma al ctest dev; lint, registro dei tipi e `trace.py check` sono stati lanciati a parte.

## Review

- Giro 1: `tools/review.sh u1-round1 tmp/u1-facts-round1.md` (Codex `gpt-6-astra`, sola lettura), delta `tmp/u1-round1.diff`, artefatto `.omc/artifacts/ask/codex-u1-round1-20261030-1458.md`: FAIL. [MED] il log dev citato era parziale (il ctest dev era ancora in esecuzione durante la review). [LOW] tre commenti del test raccontavano l'autore del caso.
- Correzione (delta `tmp/u1-round2.diff`): commenti ridotti al comportamento verificato; ctest dev e release rieseguiti a fine esecuzione, log completi citati sopra.
- Giro 2: `tools/review.sh u1-round2 tmp/u1-facts-round2.md`, delta `tmp/u1-round2.diff`, artefatto `.omc/artifacts/ask/codex-u1-round2-20261030-1516.md`: PASS, nessun rilievo.

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-03-pixel-maschere.md` | `ebd000bd2f7ce1ca59fb85ff7cf7494d7379253495257fdd9955d9d440053ccb` |
| `src/core/calibration_internal.hpp` | `9e536cf219d574cfae3959259d9396228b21e48bee8bc481034dca6106a83a4c` |
| `src/core/fits.cpp` | `f6f18576e927dcedd3c8c920f9923058fb2c9e8d7dc5523114272f75ad046967` |
| `src/core/fits_blocks.cpp` | `f6adaddd1dcf88337e9e2874a2f7a336d19645398eb2ca05dde85d90ff066ab7` |
| `src/core/fits_header.cpp` | `0ac963b56356149b128339563f688294664ca9c8770a523bbcf925fc690e1b45` |
| `src/core/fits_internal.hpp` | `e8b62f0bd808166ecaf603449199bfc7df1e3beb69f109a27e0466bfe8d4b09f` |
| `src/core/image.cpp` | `053fe85fa341c93294522f72de472ef37a6156dbbe500a53044da8d62e0bf0dd` |
| `src/core/import_job.cpp` | `945d3ee4045c7ff15e2292551a5cc34384ef4431b09181cfffe6816c9e0df487` |
| `src/core/include/astro/image.hpp` | `2ae3c4cc2192a8c3b6822ec390dcde0e62dbc2282178162a744d98a0ef20f626` |
| `tests/unit/CMakeLists.txt` | `65928a9dbb372c3c5ab3391437d077979aa1a2b00ba98ecfb3956ac8ce361af9` |
| `tests/unit/test_decoder_types.cpp` | `63adee44ced66be345321731e77d2b4a352d9055410f057890ae82a1c457dc46` |

- Test release (`tmp/release-junit.xml`): totali 878, fast 862, slow 16, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 863, fast 863, slow 0, falliti 0
<!-- evidence:end -->
