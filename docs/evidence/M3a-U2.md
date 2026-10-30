# M3.a, unità U2: decoder RAW puro

Stato: **REVIEW PASS** al giro 2. Unità: U2 di `plans/M3a.md` (obblighi 1–6), ID F-IO-02. Proprietario unico: `decode_raw_buffer` in `src/core/raw_decode.cpp`. Contratto: `docs/contracts/ADR-08-decoder-pack.md` §1, §2.2, §2.3, §2.4, §2.6, §6, §7 (emendamento dopo i test di U2, record `docs/evidence/M3a-ADR08-U2.md`; §2.2 emendato dal coordinatore per il valore iniziale di `user_cblack`); ADR-03.b §b.2 (`decoder_state`, da U1). Test: T-M3A-RAW-01, 02, 03, 04.

| Campo | Valore |
|---|---|
| Unità | M3.a / U2 |
| Scopo | Decodifica di un RAW già in memoria con LibRaw: area visibile in f32 uguale al `uint16` letto, pattern Bayer da `filters`, nero e bianco dichiarati e non sottratti, curva di linearizzazione e suo digest, metadati con origine `decoder`, stop di §2.3 e `raw.black_pattern_anchor` di §2.4 prima di produrre l'immagine |
| Comando | Dev: `tools/check.sh` (log `tmp/u2-check.log`; si ferma al ctest per i dati reali assenti), `tools/lint.sh`. Build: `bash tmp/u2-build.sh <preset> all <log>` (4 job). Finale: `bash tmp/u2-final-ctest.sh` (`ctest --preset release` e `--preset dev` con 4 job e junit in `tmp/`, poi `tools/trace.py check --stage M2`); `tools/trace.py check` anche sul junit release (`tmp/u2-trace-check-release.txt`). Mutazioni: `python3 -S tmp/u2-mut/run.py [nomi]` (un `tools/mutate.sh` per patch, preset release, regex `RawDecode`). Review: `tools/review.sh u2-round1 tmp/u2-facts-round1.md`, `tools/review.sh u2-round2 tmp/u2-facts-round2.md`. Sonde: `bash tmp/u2-probe/run.sh p` e `bash tmp/u2-probe/run.sh p2`. Blocco: `python3 -S tools/evidence.py docs/evidence/M3a-U2.md` |
| Input | Codice a `1a99a29`, test di `tmp/u2-tests.patch` del checkout principale (scritti da un altro agente), fixture sintetiche FIX-SYN-DNG-01 (`tests/fixtures/syn_dng.hpp`) |
| Versioni | GCC 15.2.0, CMake 4.2.3; LibRaw 0.21.5 di sistema (`libraw_r.so.23`, pacchetto `libraw23t64` 0.21.5b-1ubuntu1.1, SHA-256 `3484c3eecb3614c845cadd4c4de59ff124c1646317e34438e98394e2edab3010`); preset `release` e `dev` (ASan e UBSan); codex-cli 0.164.0 per la review |
| Ambiente | Linux 7.0.0-38-generic x86_64, 16 CPU, worktree isolato senza il collegamento `data` ai dati reali, 4 job |
| Risultato | Vedi «Rossi prima e dopo», «Mutazioni» e il blocco generato |
| Output | Log, junit, patch e artefatti citati sotto `tmp/` e `.omc/artifacts/ask/` |
| Ambito | §2.2, §2.3, §2.4, §2.6 di ADR-08.b sul prodotto, con i DNG sintetici. Fuori: §2.1 (`max_pixels`), §2.5 (gestori, avvisi; la tabella dei codici di ritorno è scritta ma provata in U3), §2.7, §2.8 (U3); record del decoder, simboli vietati in `check_offline.sh`, dati reali PUB-RAW-01 (U4) |
| Esito | PASS: T-M3A-RAW-01…04 da non collegabili a verdi (25 casi `RawDecode*`, 3 aggiunti dall'autore del codice); nessun altro test cambiato oltre i dati reali assenti; 36 mutazioni, 33 rilevate, 3 sopravvissute con motivo; review PASS al giro 2 con 2 LOW aperti |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Modifiche

Codice di prodotto, 322 righe nuove:

- `src/core/raw_decode.cpp` (268 righe), `src/core/include/astro/raw_decode.hpp` (54 righe, dall'intestazione dei test): `decode_raw_buffer(bytes, hooks)`. Un oggetto `LibRaw` sullo heap per chiamata; bit `LIBRAW_RAWOPTIONS_CONVERTFLOAT_TO_INT` tolto prima di `open_buffer`; parametri utente verificati con `ASTRO_ASSERT` sui valori iniziali (`user_black`, `user_sat` = -1, `user_cblack[0..3]` = -1000001); stop dopo `open_buffer` e prima di `unpack` su `filters = 9`, `raw_count > 1`, `is_floating_point()`, `is_foveon`, `colors ≠ 3`, `pixel_aspect ≠ 1`, `is_fuji_rotated()`, cella non Bayer o `filters` non periodico 2×2; dopo `unpack` e prima di ogni lettura di `raw_image`: `float_image` non nullo, `raw_image` nullo, `filters = 0`; `raw.black_pattern_anchor` per DNG con motivo di nero non invariante sull'asse con margine maggiore di zero; immagine dell'area visibile, `decoder_state` (finestra, indici di colore, nero scalare, per colore e motivo, bianco, `data_maximum` e `linear_max` se non nulli, curva e SHA-256 delle 65 536 voci `uint16` little-endian via `sha256_hex`), metadati `std::optional` con origine `decoder` e `keyword` uguale al campo di LibRaw.
- `CMakeLists.txt`: `pkg_check_modules(LIBRAW REQUIRED IMPORTED_TARGET libraw_r=0.21.5)`. `src/core/CMakeLists.txt`: sorgente e `PkgConfig::LIBRAW` PRIVATE.

Test e documenti:

- `tests/unit/test_raw_decode.cpp`, `tests/fixtures/syn_dng.hpp` dalla patch dei test. Asserzioni invariate tranne la riga di `T_M3A_RAW_02_user_params_initial_values` corretta dal coordinatore (-1 → -1000001). Aggiunti dall'autore del codice (deviazione dichiarata): `T_M3A_RAW_03_filters_zero_with_raw_image_stops`, `T_M3A_RAW_03_cell_with_adjacent_greens_stops`, `T_M3A_RAW_01_timestamp_copied_exactly` (con `TZ=UTC` nel processo il valore atteso è esatto: 1793363696); commenti di due casi allineati al valore iniziale di `user_cblack`; inclusi `<cstdlib>`, `<ctime>`.
- `tests/unit/CMakeLists.txt`: `test_raw_decode` (etichetta `fast`) e `test_raw_decode.dir` in `T-M2F-01.offline`.
- `docs/licenses.md`: righe di `libgomp.so.1`, `libjpeg.so.8`, `liblcms2.so.2`, `libraw_r.so.23` copiate nella tabella «Librerie caricate dai binari» (hash, pacchetti e versioni riletti dal sistema con `sha256sum`, `dpkg -S`, `dpkg-query -W`, uguali all'audit NFR-LIC-01.d); stato di LibRaw e lcms2 nella prima tabella.
- `docs/contracts/ADR-08-decoder-pack.md`: §2.2 emendato dal coordinatore; §6 («Copertura dei DNG sintetici») e §7: iniezioni di `data_maximum` e `is_fuji_rotated()` eseguite sul prodotto in U2 (rilievo LOW del giro 1).

Simboli di LibRaw indefiniti in `raw_decode.cpp.o` (`nm --undefined-only`): `libraw_strerror`, `LibRaw::open_buffer`, `LibRaw::is_floating_point`, `LibRaw::unpack`, il costruttore e il typeinfo di `LibRaw`; `is_fuji_rotated` è inline. Nessuna funzione vietata da §1 e §5. Nessuna firma condivisa cambiata (`codegraph impact` di `make_image` e `sha256_hex`, solo usati).

## Fatti emersi

- Valore iniziale dei parametri utente in LibRaw 0.21.5 (sonda `tmp/u2-probe/p.cpp`): `user_black -1`, `user_cblack -1000001` (×4), `user_sat -1`, `rawparams.options 0x2`. Il contratto diceva -1 per `user_cblack`: arbitrato dal coordinatore, §2.2 e la riga del test emendati. Sonda `tmp/u2-probe/p2.cpp`: `user_cblack = -1` non cambia `cblack` né i campioni dopo `unpack`.
- `other.timestamp` è l'ora di `DateTimeOriginal` interpretata nel fuso locale del processo: con `TZ=UTC` coincide con `timegm` (test `T_M3A_RAW_01_timestamp_copied_exactly`).

## Rossi prima e dopo

Prima del codice `test_raw_decode` compila ma non si collega: `tmp/u2-red-build.log`, riferimento non definito a `astro::decode_raw_buffer`.

| Test ctest | Prima | Dopo |
|---|---|---|
| `RawDecode.T_M3A_RAW_01_*` (8 casi) | non collegabile | verde |
| `RawDecode.T_M3A_RAW_02_*` e `RawDecodeDeathTest.T_M3A_RAW_02_*` (7 casi) | non collegabile | verde |
| `RawDecode.T_M3A_RAW_03_*` (8 casi) | non collegabile | verde |
| `RawDecode.T_M3A_RAW_04_*` (2 casi) | non collegabile | verde |

Suite completa finale: release 719 test, 4 falliti, tutti su dati reali assenti nel worktree (`Fits.REAL_DATA_PUB_LFC_01_light_is_read_with_documented_digest_and_header`, `T-M2E-13.real_palomar_lfc`, `T-M2R2-09.real_palomar_product_masters`, `T-M2R1-09.public_samples`); dev 713 test, 1 fallito (`Fits.REAL_DATA_PUB_LFC_01_...`). Verdi fra gli altri `T-M2F-01.offline`, `T-M2F-02.m2e_network`, `T-M2R0-01.licenses`, `T-M2R0-01.licenses_negative`. Log `tmp/u2-final-release-ctest.log`, `tmp/u2-final-dev-ctest.log`. `tools/lint.sh` verde. `trace.py check --stage M2`: sul junit release 24 ID non coperti, gli stessi dei dati reali assenti di U1; sul junit dev 38 (il junit dev non contiene i test `slow`).

## Mutazioni

`tools/mutate.sh` sul preset `release`, regex `RawDecode`, una patch per stop o controllo, generate da `tmp/u2-mut/run.py`; baseline verde, sorgente ripristinato e verificato con `cmp`. Riepiloghi `tmp/u2-mut/summary-pass1.txt` … `summary-pass4.txt`, log per patch in `tmp/u2-mut/`.

| Patch | Mutazione | Esito |
|---|---|---|
| `m01_xtrans` … `m07_fuji` | soglia o condizione di `raw.xtrans`, `raw.multi_image`, `raw.float_data` (dopo `open_buffer`), `is_foveon`, `colors`, `pixel_aspect`, `is_fuji_rotated()` | RILEVATA ×7 |
| `m08_period`, `m09_cell`, `m35_cell_counts_only` | periodicità non verificata; cella non confrontata; cella verificata per soli conteggi | RILEVATA ×3 |
| `m10_float_image`, `m11_not_cfa`, `m12_filters_zero` | stop dopo `unpack` tolti | RILEVATA ×3 (`m12` dopo il caso aggiunto) |
| `m13_anchor_x`, `m14_anchor_y`, `m15_anchor_dng`, `m16_anchor_margin` | asse x o y ignorato; condizione DNG invertita; margine ignorato | RILEVATA ×4 |
| `m17_convert_float_bit` | bit di conversione non tolto | RILEVATA |
| `m18_user_black`, `m19_user_sat`, `m20_user_cblack` | assert sui parametri utente tolti | RILEVATA ×3 |
| `m21_curve_domain`, `m22_digest_byte_order` | identità controllata sulle prime 4096 voci; byte del digest in big-endian | RILEVATA ×2 |
| `m23_data_maximum`, `m24_linear_max`, `m25_temperature`, `m26_shutter`, `m27_iso`, `m28_timestamp`, `m36_timestamp_truncated` | valore di comodo per un campo assente; timestamp troncato al minuto | RILEVATA ×7 |
| `m29_color_index_source`, `m30_sample_offset`, `m31_black_per_color_source` | indici dai `filters` non iniettati; margine sinistro ignorato nei campioni; nero per colore non dai campi letti | RILEVATA ×3 |
| `m32_window_bounds`, `m33_black_pattern_cap` | controllo dell'area visibile nel buffer grezzo; limite del motivo di nero a `LIBRAW_CBLACK_SIZE − 6` | SOPRAVVISSUTA ×2: invarianti di LibRaw, non raggiungibili da fixture né dai ganci |
| `m34_libraw_stop_table` | categoria di `LIBRAW_FILE_UNSUPPORTED` cambiata | SOPRAVVISSUTA: la tabella dei codici di §2.5 si prova in U3 (T-M3A-RAW-05) |

Al primo passaggio `m13`, `m14` e `m32` non compilavano (`-Werror`); riscritte in forma compilabile e rieseguite (`summary-pass2.txt`).

## Scelte

1. **Tabella dei codici di ritorno di §2.5 già in `libraw_stop`** (funzione, valore e `libraw_strerror` nel contesto): un codice non nullo di `open_buffer` o `unpack` si ferma comunque col codice del contratto. È provata in U3 (T-M3A-RAW-05), non qui.
2. **`filters = 0` con `raw_image` non nullo** → `unsupported`, `raw.cfa_pattern` (cella non conforme di §2.3).
3. **Cella confrontata con le quattro celle Bayer**, non solo per conteggio: una cella RB/GG si ferma con `raw.cfa_pattern`.
4. **Motivo di nero oltre `LIBRAW_CBLACK_SIZE − 6`** (= 4098, il limite di `validate_image`) **o area visibile fuori dal buffer grezzo o da `raw_pitch/2`** → `integrity`, `raw.data_error`, per analogia con `LIBRAW_BAD_CROP`: controlli prima di ogni lettura, contro letture fuori dai limiti.
5. **Motivo con `cblack[4]·cblack[5] = 0`** → 0×0, per l'invariante di §b.2.
6. **Ganci di iniezione**: `before_open` riceve i parametri letti dopo aver tolto il bit; i valori restituiti si riscrivono in LibRaw e l'assert legge LibRaw; le opzioni non si riscrivono. `after_open` e `after_unpack` decidono sui valori restituiti.
7. **`keyword`** = nome del campo di LibRaw; vuota per la fase e per `decoder_state.source`.
8. **`linear_max`** registrato con i quattro valori se almeno uno è non nullo.

## Limiti

- `raw_inset_crops`, `flip` e la parità dei margini non si applicano e non sono ancora registrati: i tipi di U1 non hanno un campo; la registrazione è nel record del decoder di U4 (§2.9). La parità si ricava da `decoder_state.window`.
- H1, H2, H4 e H3 nella forma riformulata sono provate sul prodotto solo con i DNG sintetici di FIX-SYN-DNG-01; i formati di marca restano ipotesi (§7). §7 del contratto dice ancora «provate sulla realizzazione usa-e-getta» (rilievo fuori delta del giro 2): da allineare alla prossima modifica del contratto.
- Le mutazioni `m32`, `m33`, `m34` sopravvivono (sopra).
- I 4 test su dati reali non girano in questo worktree, senza il collegamento `data`: vanno rieseguiti nel checkout principale. Per la stessa ragione `tools/check.sh` si ferma al ctest dev; lint e `trace.py check` sono stati lanciati a parte.
- Una ricompilazione intermedia del solo `test_raw_decode` (dopo l'aggiunta del primo caso) è stata lanciata senza `CCACHE_DIR` nel progetto: ccache può aver scritto nella sua directory predefinita dell'utente.

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-u2-round1-20261030-1652.md` | `tmp/u2-round1.diff` (tutto il lavoro rispetto a `1a99a29`, file nuovi compresi) | FAIL | 2 MED: timestamp verificato solo entro ±14 h; nessuna prova discriminante della cella RB/GG (scelta 3). 3 LOW: commenti del test su -1; commento sul bit di conversione; §6/§7 del contratto con le iniezioni «non eseguite». Tutti corretti |
| 2 | `.omc/artifacts/ask/codex-u2-round2-20261030-1659.md` | `tmp/u2-round2.diff` | PASS | 2 LOW, non corretti (tetto di 2 giri): `TZ` non ripristinata se `decode` lancia e ritorni di `setenv`/`unsetenv` ignorati nel caso del timestamp; riferimento a questo record, assente durante la review (ora presente). 1 fuori delta: stato di H1, H2, H4 in §7 (sopra, «Limiti») |

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `CMakeLists.txt` | `f7fe6082d3f46ee63e7c84598a97bcbd44473b56d87742f8a0f1e839dfe1177f` |
| `docs/contracts/ADR-08-decoder-pack.md` | `8268d69f2d07206c8f86f7c775c6823e82061b173f736cb115929e0f43bc2359` |
| `docs/licenses.md` | `a166e854e75428fa8678b20afe2f44886c57ffdbb85ec76bcf9af61f11c56cd2` |
| `src/core/CMakeLists.txt` | `44de095ca8c5788eeda9cd73dbd1ad92ac29a64306f59994daf63060369e507e` |
| `src/core/include/astro/raw_decode.hpp` | `b815a0b7eec8bb76052c3bc6308d91f7c9a82db4e611f7d1b8624c4acbf8ff6e` |
| `src/core/raw_decode.cpp` | `5b22c188cbe1474546fa66d45e0d87048a05b73eb466233d8981944315b28af7` |
| `tests/fixtures/syn_dng.hpp` | `dd62bccd32bc239e8389eb9aa5b8457e74850030888fc07e6413b9ee8d39162c` |
| `tests/unit/CMakeLists.txt` | `c49c893b1cbe379af40808e62ac8b35375f8a8b26391892420a4bb55e114a239` |
| `tests/unit/test_raw_decode.cpp` | `50357cb9f7128d966e091e0c8aa0f964a4cb807647e112288f6578442f8f5469` |

- Test release (`tmp/release-junit.xml`): totali 769, fast 762, slow 7, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 763, fast 763, slow 0, falliti 0
<!-- evidence:end -->
