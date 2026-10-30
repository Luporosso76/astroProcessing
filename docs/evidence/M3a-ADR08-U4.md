# M3.a: emendamento di ADR-08.b prima di U3/U4 (avviso Fujifilm e fuso dell'ora)

Stato: **CHIUSO, review PASS al giro 1**. Unità: emendamento del contratto `docs/contracts/ADR-08-decoder-pack.md` (§2.5, §2.6, §2.9, §6 righe T-M3A-RAW-01, 05, 09, 12, §7) su due decisioni: ammissione di `LIBRAW_WARN_PARSEFUJI_PROCESSED` e fuso orario di `capture_time_libraw`. Solo testo del contratto e una sonda usa-e-getta in `tmp/`: nessun codice di prodotto, nessun test del repository scritto o eseguito.

| Campo | Valore |
|---|---|
| Unità | M3.a / ADR-08.b, emendamento U4 |
| Scopo | Decidere se `PARSEFUJI_PROCESSED` (0x10000) altera ciò che il decoder importa; rendere `capture_time_libraw` indipendente dalla macchina |
| Comando | `bash tmp/adr08-u4/run.sh > tmp/adr08-u4/run1.log 2>&1` (compila `g++ -std=c++20 -O1 -Wall -Wextra -rdynamic -o probe probe.cpp $(pkg-config --cflags --libs libraw_r) -ldl` e `g++ -std=c++20 -O2 -Wall -Wextra -o rafcheck rafcheck.cpp`, `ulimit -c 0`, `TMPDIR` in `tmp/adr08-u4/`); `tools/review.sh adr08-u4-round1 tmp/adr08-u4-facts1.md`; `python3 -S tools/evidence.py docs/evidence/M3a-ADR08-U4.md` |
| Input | Commit `a869c9e`; 8 file di PUB-RAW-01 in `/opt/workspace/astroProcessor/data/public/raw-pixls-us/`, sola lettura, SHA-256 stampati nel log e uguali agli 8 valori di `docs/datasets/PUB-RAW-01.md`; DNG sintetici 64 × 48 con `DateTimeOriginal` scritti dal generatore dei test (`tests/fixtures/syn_dng.hpp`, copiato in `tmp/adr08-u4/`, SHA-256 `dd62bccd32bc239e8389eb9aa5b8457e74850030888fc07e6413b9ee8d39162c`) |
| Versioni | LibRaw 0.21.5 (`pkg-config libraw_r` 0.21.5), pacchetto `libraw23t64` 0.21.5b-1ubuntu1.1, `libraw_r.so.23.0.0` SHA-256 `3484c3eecb3614c845cadd4c4de59ff124c1646317e34438e98394e2edab3010`; `libc6` 2.43-2ubuntu2.4; `tzdata` 2026e-0ubuntu0.26.04.1; g++ 15.2.0 |
| Ambiente | Linux 7.0.0-38-generic x86_64, macchina di sviluppo; `/etc/localtime` → `/usr/share/zoneinfo/Europe/Rome` |
| Sonda | `tmp/adr08-u4/probe.cpp` (`1b1b689f86698418d03f6c85fcfb1659468882a0f1cd043fc447a80902bd79ee`): LibRaw diretta, `mktime` e `localtime` interposte nell'eseguibile (registrano campi in ingresso e in uscita e risultato, inoltrano a libc con `dlsym(RTLD_NEXT, …)`); `tmp/adr08-u4/rafcheck.cpp` (`d79df52ff474a4ed718c74550fe08c98292fe335de3dc8ec3e790f86cc974e7a`): lettore RAF che non usa LibRaw; `tmp/adr08-u4/run.sh` (`568211e35059a9f713cef8e0fc14d747e36b173d066249c2ff4ba3b67607da72`). Log, uscita 0: `tmp/adr08-u4/run1.log` (`ea7af5cdb16d32d7bab4ec8b2119817db0b4c161fa92f153cc525cb5518801d6`). Fuori da Git (`tmp/`) |
| Fonti lette | LibRaw, `https://www.libraw.org/docs/API-datastruct-eng.html` (30 ottobre 2026): `LIBRAW_WARN_PARSEFUJI_PROCESSED` «Not really a warning, but flag that fuji parser was used»; `timestamp` «Date of shooting». Manuale di glibc, `https://sourceware.org/glibc/manual/latest/html_node/Environment-Access.html` e `…/Broken_002ddown-Time.html` (30 ottobre 2026): `getenv` «MT-Safe env»; `setenv` «MT-Unsafe const:env»; «Modifications of environment variables are not allowed in multi-threaded programs»; `mktime`, `localtime_r`, `timegm` «MT-Safe env locale». `/usr/include/libraw/libraw_const.h` riga 695 (`LIBRAW_WARN_PARSEFUJI_PROCESSED = 1 << 16`), `libraw_types.h` riga 854 (`time_t timestamp`, nessun campo con l'ora in forma di testo o con il fuso); `nm -D libraw_r.so.23`: fra le funzioni di tempo importa solo `mktime` e `localtime` |
| Risultato | `PARSEFUJI_PROCESSED` ammesso: ciò che il decoder importa da X-A1 coincide col file. Ora: processo con `TZ=UTC0` impostata dal `main` e verificata dal decoder |
| Output | File elencati nel blocco generato sotto |
| Ambito | §2.5, §2.6, §2.9, §6, §7 del contratto. Fuori: codice e test di U2 e U3, che vanno adeguati (sotto, «Test da cambiare») |
| Esito | PASS al giro 1 con 2 LOW, corretti dopo il PASS senza un secondo giro (vedi «Review») |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Misure: `PARSEFUJI_PROCESSED`

Modo `probe warn` sugli 8 file. `process_warnings` vale 0 prima di `open_buffer` in tutti i file.

| File | dopo `open_buffer` | dopo `unpack` |
|---|---|---|
| Fujifilm X-A1 | `0x10000` | `0x10000` |
| Fujifilm X-E1 | `0x10000` | `0x10000` |
| Canon EOS 6D, Nikon D750, Sony ILCE-7S, Olympus E-M10, Pentax K-x, Panasonic DMC-GH4 | `0x0` | `0x0` |

Il bit nasce in `open_buffer` (identificazione del file), non in `unpack`, e `unpack` non lo cambia.

Confronto fra LibRaw e il file, X-A1 (X-E1 fra parentesi dove diverso). Il lettore indipendente (`rafcheck`) legge gli offset dell'intestazione RAF (byte 92 e 100, big-endian), i record dell'intestazione CFA e il sotto-IFD `0xF000` della sezione CFA (TIFF little-endian), poi spacchetta i campioni a 12 bit con due ordini di bit e li confronta con il dump di `raw_image` (`probe dump`, `raw_height` righe di `raw_width` campioni, letti con `raw_pitch`).

| Grandezza | LibRaw | File (lettore indipendente) |
|---|---|---|
| Buffer grezzo | `raw_width` × `raw_height` = 5120 × 3288, `raw_pitch` 10240 | `0xF001` × `0xF002` = 5120 × 3288; `0x100` = 3288, 5120; `0xF003` = 12 bit; `0xF008` = 25 251 840 byte = 5120 · 3288 · 12 / 8 |
| Campioni | `raw_image` | ordine LSB (bit meno significativi prima): **0** campioni diversi su 16 834 560 (X-E1: 0); ordine MSB: 16 223 171 diversi (X-E1: 16 245 043). Valori fra 0 e 4095 (X-E1: 0 e 4094) |
| Nero | `black` 0, `cblack[0..3]` 0, `cblack[4]` = `cblack[5]` = 2, motivo 255 255 255 255 (X-E1: 6 × 6, 36 valori 255) | `0xF00A`: 4 valori 255 (X-E1: 36 valori 255) |
| Bianco | `maximum` 4095, `data_maximum` 0, `linear_max` 0 | 12 bit (`0xF003`): 2^12 − 1 = 4095 |
| Crop di default | `raw_inset_crops[0]` = (28, 12, 4896, 3264) (X-E1: (30, 12, 4894, 3264)) | `0x110` top 12, left 28; `0x111` 3264 × 4896 (uguali in X-E1) |
| Area visibile | 4952 × 3288, margini 0, 0 | nessun tag letto dalla sonda la descrive |
| Altro | curva identità (0 voci diverse), `is_fuji_rotated()` 0, `flip` 0, `pixel_aspect` 1, `filters` `0xb4b4b4b4` (X-E1: 9, X-Trans) | — |

Per X-E1 il crop di default di LibRaw (30, 4894) differisce dai tag `0x110`/`0x111` (28, 4896): è un metadato registrato e non applicato (§2.3), e X-E1 si ferma comunque con `raw.xtrans`.

## Misure: ora di acquisizione

Modo `probe ts`. In tutte le decodifiche LibRaw chiama `mktime` solo durante `open_buffer`, mai in `unpack`; `localtime` non è mai chiamata (0 righe nel log). Le chiamate vanno a coppie: la prima riceve i campi civili dell'ora del file con `tm_isdst = -1`, la seconda gli stessi campi come normalizzati dalla prima. Coppie: 1 sul DNG sintetico, 2 su Panasonic, 4 su Sony, 3 sugli altri file di marca; in ogni decodifica tutte le coppie partono dagli stessi campi. `other.timestamp` è il risultato ed è uguale dopo `open_buffer` e dopo `unpack`.

DNG sintetico, `DateTimeOriginal` scritto, `other.timestamp` per valore di `TZ`:

| `DateTimeOriginal` | `UTC`, `UTC0`, vuota | `Europe/Rome` | `TZ` assente | `America/New_York` | `timegm` dei campi |
|---|---|---|---|---|---|
| 2026:10:30 12:34:56 | 1793363696 | 1793360096 | 1793360096 | 1793378096 | 1793363696 |
| 2026:07:15 22:00:00 | 1784152800 | 1784145600 | 1784145600 | 1784167200 | 1784152800 |
| 2026:03:29 02:30:00 (ora inesistente a Roma) | 1774751400 | 1774747800 | 1774747800 | 1774765800 | 1774751400 |
| 2026:10:25 02:30:00 (ora doppia a Roma) | 1792895400 | 1792891800 | 1792891800 | 1792909800 | 1792895400 |

Con l'ora inesistente e `Europe/Rome`, `mktime` normalizza i campi a 03:30:00 (`isdst` 1); la ricostruzione `timegm(localtime_r(timestamp))` dà 1774755000, non 1774751400 (errore +3600 s). In tutti gli altri casi la ricostruzione dà il valore di `timegm`.

PUB-RAW-01, campi passati alla prima chiamata di `mktime` e `other.timestamp` con `UTC` / `Europe/Rome` (= `TZ` assente):

| File | Campi | `UTC` | `Europe/Rome` |
|---|---|---|---|
| Fujifilm X-A1 | 2016-12-15 18:52:29 | 1481827949 | 1481824349 |
| Olympus E-M10 | 2017-03-18 15:54:01 | 1489852441 | 1489848841 |
| Sony ILCE-7S | 2017-07-13 16:58:20 | 1499965100 | 1499957900 |
| Panasonic DMC-GH4 | 2018-05-10 08:42:37 | 1525941757 | 1525934557 |
| Fujifilm X-E1 | 2019-04-22 07:23:07 | 1555917787 | 1555910587 |
| Canon EOS 6D | 2017-01-08 11:18:13 | 1483874293 | 1483870693 |
| Pentax K-x | 2017-01-12 19:32:52 | 1484249572 | 1484245972 |
| Nikon D750 | 2017-01-15 21:37:53 | 1484516273 | 1484512673 |

Valori di riferimento della verifica del decoder (Python `calendar.timegm`): 2026-01-15 12:00:00 → 1768478400; 2026-07-15 12:00:00 → 1784116800.

## Decisioni

| # | Questione | Decisione nel contratto | Motivo |
|---|---|---|---|
| 1 | `PARSEFUJI_PROCESSED` | Ammesso in §2.5 e registrato in provenance come gli altri avvisi ammessi. X-A1 si decodifica; X-E1 resta `raw.xtrans` prima di `unpack` | Documentazione: segnala il parser usato, non un'elaborazione. Il bit nasce in `open_buffer`. Su X-A1 campioni, dimensioni del buffer, nero, bianco e crop di default coincidono con il file letto senza LibRaw; nessun RAF senza il bit è disponibile per un confronto diretto |
| 2 | Fuso di `capture_time_libraw` | §2.6: ogni eseguibile che collega il decoder imposta `TZ=UTC0` nel `main` prima dei thread e non modifica più l'ambiente; il decoder verifica `getenv("TZ") == "UTC0"` e `mktime` sui due riferimenti prima di `open_buffer`, `ASTRO_ASSERT` altrimenti; il valore (`timegm` dei campi civili, orologio della camera senza fuso) entra nel record (§2.9) e nella chiave | Sotto `UTC0` il valore è `timegm` dei campi in tutti i casi misurati, compresa l'ora inesistente e quella doppia; `setenv` dopo la nascita dei thread è vietato da glibc, la lettura di `TZ` nei worker è sicura senza modifiche. Scartate: ricostruzione con `localtime_r` (errata nell'ora inesistente, misurato); valore «locale» fuori da chiave e provenance (resterebbe in `ASTRTSTP` diverso fra macchine); lettura dell'ora dal file (secondo parser, escluso da §2.3) |

## Test da cambiare

- `tests/unit/raw_threads.cpp` (U3): `expected_for("3")` (X-A1) passa da `raw.warning` a decodifica riuscita.
- `tests/unit/test_raw_robust.cpp` (U3), `T_M3A_RAW_05_other_warning_bits_stop_with_the_name`: togliere `LIBRAW_WARN_PARSEFUJI_PROCESSED` dall'elenco dei bit non ammessi; aggiungere il caso «bit iniettato da solo → immagine con il bit registrato».
- T-M3A-RAW-09 (U4): X-A1 con decodifica riuscita e `0x10000` fra gli avvisi registrati.
- `tests/unit/test_raw_decode.cpp` (U2): `T_M3A_RAW_01_timestamp_copied_exactly` non imposta più `TZ` con `setenv` a metà test; il `main` dell'eseguibile (oggi `GTest::gtest_main`) imposta `TZ=UTC0` prima dei thread; aggiungere l'ora inesistente e l'ora doppia con valore esatto. Il test dei metadati con tolleranza di ±14 h diventa uguaglianza esatta (1793363696). Nuovo death test in un processo separato con `TZ` diversa da `UTC0`: `ASTRO_ASSERT` prima di `open_buffer`.
- Ogni altro eseguibile di test che chiama il decoder (U3: `test_raw_robust`, `raw_threads`, `fuzz_raw_decode`) ha bisogno dello stesso `main`.
- T-M3A-RAW-11 e T-M3A-RAW-12: il record contiene la riga «Ora di acquisizione»; l'iniezione di un'ora diversa cambia la chiave.

## Limiti

- La sonda prova LibRaw direttamente, non il codice del prodotto.
- La coincidenza col file per `PARSEFUJI_PROCESSED` è provata su due RAF a 12 bit non compressi; l'area visibile 4952 × 3288 di X-A1 non è confrontata con un tag del file.
- Il tag da cui LibRaw prende l'ora è noto solo per il DNG sintetico (`DateTimeOriginal`, il solo tag d'ora scritto); per i file di marca la sonda vede i campi passati a `mktime`.
- Le ore misurate cadono fra il 2016 e il 2026; le ore fino al 1970-01-01 00:00:00 compreso danno `other.timestamp ≤ 0` e si leggono assenti (§2.6, non misurato).

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-adr08-u4-round1-20261030-1829.md` | `tmp/adr08-u4-round1.diff`, fatti `tmp/adr08-u4-facts1.md` | PASS | 2 LOW: (1) il criterio di T-M3A-RAW-05 diceva «Stop» anche per il caso ammesso `PARSEFUJI_PROCESSED`; (2) il limite di §7 «correttezza bit per bit solo sui DNG sintetici» non teneva conto del confronto sui due RAF di §2.5. Corretti dopo il PASS con due modifiche di una frase ciascuna (`tmp/adr08-u4-low.diff`, rispetto a `tmp/adr08-u4/after-round1.md`); non rivisti da un secondo giro |

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-08-decoder-pack.md` | `8268d69f2d07206c8f86f7c775c6823e82061b173f736cb115929e0f43bc2359` |

<!-- evidence:end -->
