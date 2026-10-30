# M3.a: emendamento di ADR-08.b dopo i test di U2

Stato: **CHIUSO, review PASS al giro 2**. Unità: emendamento del contratto `docs/contracts/ADR-08-decoder-pack.md` (§1, §2.2, §2.3, §2.4, §2.6, §6, §7) sui fatti emersi dai test di U2 (T-M3A-RAW-01…04). Solo testo del contratto e una sonda usa-e-getta in `tmp/`: nessun codice di prodotto, nessun test del repository scritto o eseguito.

| Campo | Valore |
|---|---|
| Unità | M3.a / ADR-08.b, emendamento dopo U2 |
| Scopo | Verificare i 7 fatti riportati dall'autore dei test di U2 e decidere per ciascuno: regola, stop o limite dichiarato |
| Comando | `g++ -std=c++20 -O1 -Wall -Wextra -o probe probe.cpp -pthread -lraw_r -fopenmp -lcrypto` in `tmp/adr08-u2/probe/` (con `TMPDIR` in quella directory); `bash tmp/adr08-u2/probe/run.sh` (con `ulimit -c 0`); `curl` della specifica DNG in `tmp/adr08-u2/spec/` e `pdftotext -layout`; `grep`/`sed -n` sugli header di `/usr/include/libraw/`; `tools/review.sh adr08-u2-contract1 tmp/adr08-u2-facts1.md`; `python3 -S tools/evidence.py docs/evidence/M3a-ADR08-U2.md` |
| Input | Commit `2c1caaf`; test di U2 non integrati, sola lettura, nel worktree `.claude/worktrees/agent-a2d817e9a3b4e35c7` (`tests/unit/test_raw_decode.cpp`, `tests/fixtures/syn_dng.hpp`, `tmp/u2-shim/shim_decode.cpp`, `tmp/u2-shim-run.log`, `tmp/u2-libraw-probe.log`, `tmp/u2tests-review2.md`); 8 file di PUB-RAW-01 in `/opt/workspace/astroProcessor/data/public/raw-pixls-us/`, sola lettura |
| Versioni | LibRaw 0.21.5 (`LibRaw::version()` = `0.21.5-Release`), pacchetto `libraw23t64` 0.21.5b-1ubuntu1.1, `libraw_r.so.23` SHA-256 `3484c3eecb3614c845cadd4c4de59ff124c1646317e34438e98394e2edab3010`; g++ 15.2.0; Adobe DNG Specification 1.6.0.0 (dicembre 2021), PDF SHA-256 `7f6e7867569c1daeae7f6835cd0a6a769e47e36b109594a8b3cc321d2327e569`, letto il 30 ottobre 2026 da `https://helpx.adobe.com/content/dam/help/en/photoshop/pdf/dng_spec_1_6_0_0.pdf` |
| Ambiente | Linux 7.0.0-38-generic x86_64, macchina di sviluppo |
| Sonda | `tmp/adr08-u2/probe/probe.cpp` (SHA-256 `188816887d9a3a611d0953c1d4160e2f5a3f710b66549ad36736cbb04a0bb509`), `tmp/adr08-u2/probe/run.sh` (`ff577fb6710e069ccadcc751638fad25226e66dfb064d210f1fff37262878ddc`), copia del generatore dei test `tmp/adr08-u2/probe/syn_dng.hpp` (`f031c78bc3167b2f50fb8a02980976b470fca440c7601f31d5b701ed5a827195`, uguale a quello del worktree dei test). Due esecuzioni della sonda, uscita 0: `tmp/adr08-u2/probe-run1.log` (`08ef8377a7ab8d491d9c08d906b9ab0a039f4ef72ee9e8af885a62ce11cb29e0`) e `tmp/adr08-u2/probe-run2.log` (`79e548bcedb734cfe7bbd84e906c0484a644f09ffbaaebf777918bc78f5ee6b1`); differiscono solo nelle righe `raw_image[0..2]` dei casi in virgola mobile con il bit tolto (`diff` dei due log). Fuori da Git (`tmp/`). La sonda legge membri protetti di LibRaw (`tiff_ifd`) e chiama `adjust_bl` solo per capire la libreria: il contratto lo vieta al prodotto |
| Risultato | 7 fatti verificati: 6 confermati, 1 confermato con una correzione (punto 3, momento del bit); contratto emendato |
| Output | File elencati nel blocco generato sotto |
| Ambito | Fatti 1–7 della richiesta e le clausole del contratto che ne dipendono. Fuori: §2.1, §2.5, §2.7–§2.9, §3–§5 del contratto; test e codice di U2 |
| Esito | PASS al giro 2 (giro 1 FAIL con 2 MED e 3 LOW, corretti); un LOW aperto, vedi «Review» |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Fatti verificati e decisioni

| # | Fatto riportato | Verifica | Decisione nel contratto |
|---|---|---|---|
| 1 | H3 smentita con nero per canale e `ActiveArea` di origine dispari | **Sì.** DNG 1.6.0.0 p. 26: l'origine del motivo di `BlackLevel` è l'angolo in alto a sinistra di `ActiveArea` (p. 21: lo stesso per il motivo CFA). Sonda, motivo 10, 20, 30, 40: H3 errata in ogni sito per origini (1,0) 1248/1248, (0,1) 1240/1240, (1,1) 1209/1209, (3,0) 1248/1248; esatta per (0,0) e (2,2) e, con origine dispari, per motivi invarianti sull'asse spostato (64, 64, 80, 80 con (1,0); 64, 72, 64, 72 con (0,1); uniforme con (1,1)). `cblack[4..9]` resta 2, 2, 10, 20, 30, 40 per ogni origine: LibRaw non riancora il motivo | §2.4: H3 riformulata; stop `unsupported`, `raw.black_pattern_anchor` per DNG con motivo non invariante su un asse con margine maggiore di zero. Scelta (b): la correzione (a) richiede l'origine del file, che sta solo nel membro protetto `tiff_ifd[].t_lm/t_tm` (sonda: 1 per origine 1, 3 per origine 3); un margine pari > 0 non distingue un'origine pari da una dispari arrotondata, quindi lo stop copre anche (2,2): limite dichiarato in §7 |
| 2 | Origine dispari: margine arrotondato al pari, dimensione ridotta di 1 | **Sì.** Origine x 1 → `left_margin` 2, `width` 39 (area 40); x 3 → 4, 39; y 1 → `top_margin` 2, `height` 31 (area 32); origini 0 e 2 invariate; `filters` riallineato (colore di `FC` uguale al colore scritto in ogni sito, 6 geometrie); campioni uguali bit per bit | §2.3: regola esplicita, l'area importata resta l'area visibile di LibRaw; nessun valore importato differisce dal file; la riga/colonna esclusa non si importa e il prodotto non può osservarla (limite §7). Lo stop non è realizzabile (margine pari ambiguo); importare l'area del file richiederebbe internals o un secondo parser TIFF, esclusi |
| 3 | DNG float convertito in silenzio con le opzioni predefinite; il bit va tolto prima di `open_buffer` | **Sì, con una correzione.** `rawparams.options` iniziale `0x2` (bit attivo); con il bit attivo, DNG float a 16, 24, 32 bit: `float_image` nullo, `raw_image` intero, `maximum` 16383 al posto di `WhiteLevel` 4000. Toglierlo prima di `open_buffer` basta; toglierlo dopo `open_buffer` e prima di `unpack` dà lo stesso esito (il «prima di `open_buffer`» non è necessario, è sufficiente). `is_floating_point()` vale 1 già dopo `open_buffer` in tutti i casi. Con il bit tolto `raw_image` non è nullo e ha valori non definiti (32 bit: 12648, 27041, 32757 in `probe-run1.log`; 12648, 14017, 30315 in `probe-run2.log`) | §2.2: bit tolto prima di `open_buffer` (regola). §2.3: stop `raw.float_data` su `is_floating_point() ≠ 0` dopo `open_buffer`, prima di `unpack`, e su `float_image` non nullo; condizioni valutate prima di ogni lettura di `raw_image` |
| 4 | Tabella identica di 4096 voci non dà curva identità | **Sì.** `curve[i] = 4095` per `i ≥ 4096` (0 differenze dalla tabella estesa con l'ultima voce); campione scritto 5000 letto 4095. È la regola della specifica (DNG 1.6.0.0 p. 84: valore oltre la tabella mappato sull'ultima voce). Tabella identica di 65 536 voci: curva identità, 5000 letto 5000 | §2.4: confronto su tutte le 65 536 voci invariato; una tabella identica di N < 65 536 voci è `decoder_curve` (la curva cambia i campioni oltre N − 1), fatto dichiarato. H4 provata sui DNG sintetici |
| 5 | Byte del digest della curva non fissati | **Sì.** Il contratto diceva solo «SHA-256 della tabella» | §2.4: SHA-256 di 131 072 byte, `curve[0..65535]` in ordine, `uint16` little-endian. Sonda: digest di `color.curve` uguale al digest calcolato dalla tabella scritta estesa (identica 4096: `2fe87d20…0264`; `3i + 5`: `a3089d2a…6662`; identica 65 536: `68e41947…246b`) |
| 6 | `sizes.fuji_width` non esiste; `SensorTemperature` sta in `makernotes.common` | **Sì.** `libraw_image_sizes_t` (`libraw_types.h` 215-225) non ha `fuji_width`; il campo è in `libraw_internal_output_params_t` (136-143), parte interna, letto da `is_fuji_rotated()` (`libraw.h` 268-271), copia pubblica `rawdata.ioparams` (riga 983) solo dopo `unpack`. `SensorTemperature` in `libraw_metadata_common_t` (riga 875), membro `common` di `libraw_makernotes_t` (riga 1052); `libraw_imgother_t` (righe 848-860) non ha temperature | §2.3: `is_fuji_rotated()` dopo `open_buffer`. §2.6: `makernotes.common.SensorTemperature`, assente se e solo se vale −1000 dopo `unpack` (sonda: 0 prima di `open_buffer`, −1000 su 8 file senza il dato, 32 su Olympus E-M10); `CameraTemperature` mai usata al suo posto (Canon EOS 6D: 22 con `SensorTemperature` −1000). Ipotesi: −1000 unico marcatore |
| 7 | Metadati non producibili con un DNG sintetico | **Sì.** DNG sintetico: `SensorTemperature` −1000, `data_maximum` 0, `linear_max` 0, `cblack[0..3]` 0 (anche con nero per canale, che va nel motivo), `is_fuji_rotated()` 0. PUB-RAW-01 dopo `unpack`: temperatura solo Olympus (32); `cblack[0..3]` non nulli solo Pentax K-x (1, 10, 0, 10); `linear_max` non nulli su Canon 15070, Nikon 3827, Sony 15360, Olympus 4093, Panasonic 4079; `data_maximum` 0 su tutti; `is_fuji_rotated()` 0 su tutti | §6, paragrafo «Copertura dei DNG sintetici»: in U2 per iniezione sui campi letti (dopo `open_buffer` o dopo `unpack`); su dati reali T-M3A-RAW-09 in U4; `data_maximum` non nullo e `is_fuji_rotated() ≠ 0` solo per iniezione, pianificata, non ancora eseguita sul prodotto (limite §7) |

Altri cambi nel contratto, conseguenza dei fatti: §1 ammette `is_floating_point`, `is_fuji_rotated` e la scrittura di `rawparams.options`, vieta `adjust_bl` e la lettura di membri protetti o interni; §6 dichiara il generatore TIFF a mano dei test (il contratto diceva libtiff), la versione e lo SHA-256 della specifica DNG con le pagine citate, e righe T-M3A-RAW-01…04 aggiornate; §7 aggiorna lo stato di H1–H4 e aggiunge tre limiti.

## Fuori perimetro, osservato con la sonda

- Fujifilm X-A1 e X-E1 di PUB-RAW-01 danno `process_warnings = 0x10000` (`LIBRAW_WARN_PARSEFUJI_PROCESSED`) dopo `unpack`. Il bit è fuori dalla lista ammessa di §2.5: con §2.5 com'è oggi, X-A1 (atteso Bayer in T-M3A-RAW-09) si ferma con `raw.warning`. Annotato nella riga di T-M3A-RAW-09; la decisione spetta a una revisione di §2.5 (U3) prima di U4.
- `plans/M3a.md` U2 punto 4 elenca i codici di stop senza `raw.black_pattern_anchor` (§2.4): da allineare dal coordinatore.

## Limiti

- La sonda prova LibRaw sui DNG scritti dal generatore dei test, non il codice del prodotto, che non esiste ancora.
- Origini misurate, (x, y): (0,0), (1,0), (0,1), (1,1), (3,0), (2,2), più (1,0), (0,1), (1,1) con motivi invarianti; una sola dimensione di area attiva (40 × 32) e un buffer 64 × 48; l'arrotondamento per altre origini è dedotto dalla regolarità, non misurato.
- Sorgente di LibRaw non letta: i meccanismi (arrotondamento, motivo non riancorato, marcatore −1000) sono descritti dagli effetti misurati.
- La specifica letta è la 1.6.0.0; le fixture dichiarano `DNGVersion` 1.4.0.0: che le clausole citate valgano già nella 1.4 è un'ipotesi.
- I log dei test di U2 citati (`tmp/u2-shim-run.log`) stanno nel worktree dei test, fuori da Git.

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-adr08-u2-contract1-20261030-1556.md` | primo giro, testo intero (`tmp/adr08-u2-round1.diff`) | FAIL | 2 MED, 3 LOW: rimozione del bit di conversione senza test discriminante in §6; `data_maximum` non nullo dichiarato «provato» per iniezione mentre è solo pianificato; LOW: «non uniforme» invece di «non invariante sull'asse con margine positivo» in tre punti; valori di `raw_image` di due esecuzioni non nel log conservato; «origini 0–3 per asse» sovrastima la copertura. Tutti corretti |
| 2 | `.omc/artifacts/ask/codex-adr08-u2-contract2-20261030-1601.md` | `tmp/adr08-u2-round2.diff` (contratto) e modifiche del record elencate in `tmp/adr08-u2-facts2.md` | PASS | 1 LOW, non corretto (tetto di 2 giri) |

Punto aperto (LOW del giro 2): «iniezione pianificata, non ancora eseguita sul prodotto» (riga 7 della tabella sopra e §6 del contratto, paragrafo «Copertura dei DNG sintetici») non distingue `data_maximum` dall'iniezione `fuji_width = 100`, già eseguita sulla realizzazione usa-e-getta con test verde (`tmp/u2-shim-run.log` del worktree dei test); la formulazione corretta è «non ancora eseguita sul prodotto», come in §7. Da allineare alla prossima modifica del contratto.

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-08-decoder-pack.md` | `cccff25b9f7bdb7745856ec949b28c7e781fc561b4d6a611a0739c9d2cf47e1d` |

<!-- evidence:end -->
