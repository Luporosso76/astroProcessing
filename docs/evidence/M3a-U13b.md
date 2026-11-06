# M3.a, sotto-unità U13b: decoder DNG del progetto, deflate intero a 32 bit, immagine, livelli, metadati, limiti

Stato: **chiusa**. Test: review PASS al giro 2, casi aggiunti dopo le review del codice da agenti dei test distinti. Codice: review FAIL al giro 1 (2 MED) e al giro 2 (2 MED), arbitraggio del coordinatore, review sul delta PASS al secondo passo (blocco «Review»).

| Campo | Valore |
|---|---|
| Unità | M3.a / U13b (test e codice) |
| Scopo | Sostituire lo stop provvisorio `raw.dng_decoder_incomplete` di U13a con l'immagine: predittori 1, 2, 34892, 34893 su strisce e tasselli (riempimento dei tasselli di bordo decodificato e non importato), `ActiveArea` esatta anche con origine dispari, `LinearizationTable`, campioni f32 esatti con il limite 2^24 sui soli campioni importati, nero, bianco, metadati; memoria di zlib con tetto per flusso e gancio di rifiuto del sistema; cancel prima di ogni segmento; record del decoder per i due decoder (U13 §8, ADR-08.b §2.9) |
| ID | T-U13-03, 04, 05, 07, 08, 09, 10, 11, 12, 24 |
| Comando | Integrazione: build pulita, `tools/check.sh` e `ctest --preset release` completo (blocco generato). Mutazioni: `tools/mutate.sh` (patch in `tmp/u13b-mut/`, `tmp/u13b-r1-mut/`, `tmp/u13b-arb-mut/` del worktree) |
| Versioni | GCC 15.2.0, CMake 4.2.3, Ninja, zlib di sistema 1.3.1, LibRaw 0.22.3 e libtiff 4.7.2 compilate in `third_party/` (libtiff solo come oracolo dei test) |
| Ambiente | Linux 7.0.0-38-generic x86_64, 16 CPU; worktree `.claude/worktrees/opus-u13bc` sulla base U6 + U8a + U13a |
| Autore e data | Test: agente Claude Opus 5.5 (abbonamento); casi dopo le review: agenti Claude Opus 5.5 (API). Libtiff in `third_party/`: agente Claude Sonnet 5.5 (API, infrastruttura). Codice: agente Claude Opus 5.5 (abbonamento). Review: Claude Opus 5.5 (Argon fuori quota). Coordinatore: Claude Opus 5.5. 6 novembre 2026 |

## Decisioni del coordinatore

- Oracolo dei test: libtiff 4.7.2 compilata in `third_party/libtiff/` (firma GPG verificata, solo deflate), al posto della libtiff di sistema; mai collegata dal prodotto fino al writer TIFF di M3.s.
- U13 §4: `cfa_color_index` nella rappresentazione di LibRaw (verde della riga 1 della cella = 3), perché è un campo strutturale di ADR-16.b §b.7; limite 2^24 solo sui campioni importati; BlackLevel con `R · C` > 4098 → `raw.black_level`; metadati malformati mai bloccanti (nome in `metadata_rejected`, campo assente), `camera` dal solo tag valido, ISO con più valori → il primo.
- U13 §5: `funzione` dello stop di memoria = la chiamata di zlib che ha restituito `Z_MEM_ERROR`; memoria negata dal sistema → stesso stop `raw.too_big` con `rifiuto` = `sistema`.
- U13 §8 e ADR-08.b §2.9: il record dei due decoder porta gli stessi campi comuni (compresi `black_applied_by_decoder`, `scale_applied_by_decoder`, `data_state`, `camera_processing`, `metadata_not_kept`, `process_tz`).
- `docs/licenses.md`: l'audit delle librerie di sistema del 30 ottobre è dichiarato storico; nessuna libreria di sistema di quella sezione è collegata.

## Mutazioni

Tutte rilevate con `tools/mutate.sh`.

| Gruppo | Patch |
|---|---|
| Codice (`tmp/u13b-mut/`) | `01-predictor-34893-step`, `02-byte-order`, `03-odd-active-area-rounded`, `04-tile-padding-not-decoded`, `05-limit-2-24`, `06-linearization-last-entry`, `07-curve-extension`, `08-record-rows-per-strip`, `09-record-libraw-route`, `10-cancel-before-segment`, `11-timegm-day` |
| Test dopo il giro 1 (`tmp/u13b-r1-mut/`) | `01-record-no-metadata-not-kept`, `02-exif-pointer-short-accepted`, `03-iso-last-value`, `04-rational-truncated`, `05-black-limit-off-by-one`, `06-black-limit-plus-one` |
| Arbitraggio (`tmp/u13b-arb-mut/`) | `01-system-bad-alloc`, `02-rifiuto-inverted` |

## Limiti

- Predictor 34892 e 34893: generatore dei test e decoder leggono lo stesso testo della specifica; la prova indipendente con libtiff esiste solo per il Predictor 2.
- Correttezza provata solo su DNG sintetici.
- Un `malloc` reale che restituisce `nullptr` non è provocato dai test: la memoria negata dal sistema si prova col gancio (U13 §13); la mutazione `01-system-bad-alloc` cambia insieme la riga del gancio e quella del `malloc`, ed è rilevata solo per la prima: la riga del `malloc` resta senza mutazione rilevata.
- Fuori dalla prenotazione di §7: tabella di linearizzazione, curva estesa, digest (al più circa 384 KiB, U13 §13); ISO 65535 registrato come valore esatto.

## Review

- Test: giro 1 FAIL (1 MED), giro 2 PASS.
- Codice: giro 1 FAIL (2 MED: record del decoder del progetto incompleto; oracolo libtiff passato a `third_party/` senza documenti allineati), giro 2 FAIL (2 MED: `licenses.md` non corretto come classe; memoria negata dal sistema senza test). Arbitraggio: review sul delta `tmp/api/u13bcode-arb-review.json` FAIL (1 MED: nota di stato di `docs/licenses.md` falsa, `libjpeg`, `libz`, `libzstd` restano caricate; 4 LOW documentali), corretti dal coordinatore; verifica `tmp/api/u13b-doc-review.json` PASS (con `ldd` su tutti gli 86 eseguibili di `build/dev`).

## Integrazione con U8b (6 novembre 2026)

Unendo U8b e U13b sono emersi difetti che le review separate non potevano vedere: `decoder_state_of_record` non ricostruiva lo stato del record del decoder del progetto (`route`, `dng`); l'identità del pacchetto di decoder mancava nel record del decoder del progetto (ADR-08.b §2.9 emendato: i quattro campi valgono per entrambi i decoder); la selezione congelata non portava `route` e `dng`; `first_state_difference` non li confrontava (un `ASTRO_ASSERT` raggiungibile). Corretti in `.claude/worktrees/opus-int8b13b` da agenti distinti per codice e test (Claude Opus 5.5, API), con mutazioni in `tmp/int-mut/` di quel worktree tutte rilevate; review dell'integrazione `tmp/api/int-u8b13b-review.json` FAIL (2 MED, 3 LOW: rami nuovi senza test) e `tmp/api/int-u8b13b-review2.json` PASS con 1 LOW (copertura parziale dei campi di `dng` nel confronto delle osservazioni, lavoro successivo).

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-08-decoder-pack.md` | `44cc42cda59c6b958f1911a643a07ddc0fe02f7e82aa01885d43b672051a4195` |
| `docs/contracts/U13-dng-decoder.md` | `396393dd3d01f44f7ab1042cbf1b17a15b2a2aba99fb8d04eed13ad89895ba61` |
| `docs/licenses.md` | `8f26eaa032e5d7c2352f3102542b08caf41880d32d602988c7007111cd8576b1` |
| `src/core/dng_decode.cpp` | `337735e3da8c1237796b971b7a27520a5c3674970e5b6363cacf2fab2ed008e1` |
| `src/core/dng_internal.hpp` | `ac95b16fc39ffd6fd7668cbcaeba341f534b7f4af484811839705e1e8cb3685d` |
| `src/core/include/astro/image.hpp` | `216e96b4fc5088e01775261255f8ecbeda63c35e5925d2523e0a743bb984da03` |
| `src/core/include/astro/raw_decode.hpp` | `0a2b8aa117998b429ac983439f67a3360ba08a481dd2106dddb606f858e99be6` |
| `src/core/raw_decode.cpp` | `e114fbf53cad623be8ab4a4c4fd952fc33227f29e171e8788259f5e0b493029e` |
| `tests/fixtures/syn_dng_deflate.hpp` | `c743a7edc04c127f74d3a45819eeca452d006e2816d4c0914aa3d494cc6ac8ba` |
| `tests/unit/dng_u13b_support.hpp` | `304b89c048e4320aa2894ffd9671f72f9c40ca682d4acd927aeb764894b496ca` |
| `tests/unit/raw_record_keys.hpp` | `ff32b3c8f5e0942397921048068e0cd13b0859bf11f5e81dae13c8f7e9169baf` |
| `tests/unit/test_dng_u13b.cpp` | `a59c3a0e5b7146af100ad0efba3c8d4a90bd1700db5b4927d980d590a0ba4c26` |
| `tests/unit/test_dng_u13b_hooks.cpp` | `c0e3fd9f534c8ceff4ec33ac6863af23661f40f2f7b89c8d4524b832f5dbfa1d` |
| `third_party/libtiff/SHA256SUMS` | `bc546ce0298da64f8a43a2a8662ccb0df3e0e9ef53312623cfb8474086655ab6` |

- Test release (`tmp/release-junit.xml`): totali 1209, fast 1187, slow 22, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 1187, fast 1187, slow 0, falliti 0
<!-- evidence:end -->
