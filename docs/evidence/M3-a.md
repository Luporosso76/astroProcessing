# Gate M3.a: import RAW, stati di offset e calibrazione, classificazione del batch

Stato: **PASS**, 6 novembre 2026. Clausola: `plans/M3a.md`, sezione «Gate M3.a» (5 punti). Commit del gate: quello che introduce questo record, sopra `bd7793d`.

| Campo | Valore |
|---|---|
| Unità | Gate della slice M3.a |
| Scopo | Verificare i 5 punti del gate M3.a sul lotto finale (unità U0–U13b chiuse, lavoro prima del gate) |
| Comando | Vedi «Esecuzione» |
| Versioni | GCC 15.2.0, CMake e CTest 4.2.3; LibRaw 0.22.3 e libtiff 4.7.2 da `third_party/`; piano attivo SHA-256 `95abbb1a…4094` |
| Ambiente | Linux 7.0.0-38-generic x86_64, AMD Ryzen 7 6800H, 16 CPU; 6 job di build e 4 di test (sotto il tetto del 70%); temporanei in `tmp/` e `build/<preset>/test_tmp/sys` |
| Esito | PASS sui 5 punti |
| Autore e data | Coordinatore (Claude Opus 5.5), 6 novembre 2026 |

## Esecuzione

Build pulite in directory nuove, sul working tree del lotto del gate (HEAD `bd7793d` più le modifiche di questo commit, codice e test compresi):

1. `rm -rf build/dev build/release`; `tools/check.sh` (log `tmp/int-check.log`): uscita 0, lint pulito, dev (ASan/UBSan) 1190/1190.
2. `cmake --preset release && cmake --build --preset release -j6` (log `tmp/int-release-build.log`): uscita 0.
3. `ctest --preset release -j4 --output-junit tmp/release-junit.xml` (log `tmp/int-release-ctest.log`): 1212/1212, slow compresi.
4. Dev con gli slow: `ctest --preset dev-full -j4 --output-junit tmp/dev-full-junit.xml` (log `tmp/gate-devfull.log`), stesso ambiente ASan/UBSan del preset `dev` con filtro `^(fast|slow)$`: il preset `dev` di `tools/check.sh` esegue solo i fast, e senza gli slow (fuzz `T-M3A-RAW-10.*`, `T-U13-11.*`) il trace su dev lascia F-IO-02 e i suoi dipendenti non coperti. Esito: 1211/1212 in 6087,7 s; l'unico non passato è `T-M2R2-09.real_palomar_product_masters`, **timeout a 1800 s** (test M2 sul dato reale, nessun file di M3.a nel suo percorso; ai gate M2 sotto ASan durava 1618 e 1667 s, qui ha girato in parallelo agli altri slow). Rilanciato da solo con lo stesso preset (`ctest --preset dev-full -R '^T-M2R2-09\.real_palomar_product_masters$' --output-junit tmp/dev-junit-palomar.xml`, log `tmp/gate-devfull-palomar.log`): **Passed 1659,84 s**.
5. `python3 -S tools/trace.py check --stage M3.a --map tests/trace/M3a.tsv` su `build/dev` col junit di `dev-full` e su `build/release` col junit release: `trace: 8 ID nello stadio M3.a, 0 non coperti` (blocco generato sotto). La mappa `tests/trace/M3a.tsv` copre anche i prerequisiti ADR-03.b, ADR-08.b, ADR-16.b, D-16 e NFR-LIC-01.d.

## Punto 1: test verdi, tracciabilità, conformità

- Dev e release verdi (sopra); trace check pulito in entrambe le build (blocco generato sotto).
- Suite di conformità su `io.raw_import`: `ConformanceRaw.T_M3BASE_11_12_raw_import_type_is_registered_with_a_raw_role` eseguito, verde.

## Punto 2: criteri di uscita di `plans/M3.md`

| Criterio | Test (junit release, tutti eseguiti e verdi) |
|---|---|
| Pattern, nero, bianco, crop e stato dichiarati | `RawDecode.T_M3A_RAW_01_*`, 81 test `T_M3A_RAW_*` in tutto |
| Offset noto non sottratto due volte | `RawDecode.T_M3A_RAW_02_*` (7), `OffsetState.T_M3A_C01_01_*` (8), `LightOffsetEntry.T_M3A_C01_03_*` (13), `*.T_M3A_CAL_08_*` (19) |
| Pattern ignoto o X-Trans: stop | `RawDecode.T_M3A_RAW_03_stops_from_fixtures`, `T_M3A_RAW_03_stops_from_injected_fields` (`raw.cfa_pattern`); `RawImport.T_M3A_RAW_09_xtrans_stops_with_raw_xtrans`; `BatchClassification.T_M3A_05a_raw_files_are_unclassified_and_xtrans_stops_only_its_branch` |
| Mismatch dopo la scelta: stop del ramo, progetto riapribile | `VerifySelection.T_M3A_06a_*`, `VerifySelection.T_M3A_CAL_09_*`, `BatchClassification.T_M3A_06b_reopened_project_keeps_inputs_and_revisions_after_import_batch` |
| Fuzz pulito | 20 test fuzz verdi, fra cui `Pack.T_M3A_PACK_09_*` e `PackIndex.T_M3A_PACK_09_*` (manifest, indice, righe), `T-U13-11.fuzz_strips`, `fuzz_tiles`, `fuzz_tiles256`, `fuzz_jxl` (decoder DNG del progetto), i fuzz di FITS e dell'inventario |
| G-PACK su pack sintetico | 60 test `Pack.T_M3A_PACK_*` |

## Punto 3: T-FBK-01 parte M3.a, T-M2F-02, T-M2R0-01

- Decoder che fallisce, nessun percorso alternativo: `DngRoute.T_U13_01_fault_in_inflate_stays_on_the_project_decoder`, `DngRoute.T_U13_01_fault_in_open_buffer_stays_on_libraw`; parte M2 rieseguita: `CalibJob.T_FBK_01_*` (3).
- Pack mancante: `Pack.T_M3A_PACK_02_missing_pack_stops_only_the_dependent_branch`, `Pack.T_M3A_PACK_02_missing_directory_and_missing_manifest_have_the_same_stop`.
- `T-M2F-02.m2e_network`, `T-M2R0-01.licenses`, `T-M2R0-01.licenses_negative`: rieseguiti, verdi (le righe libtiff di `docs/licenses.md` comprese).

## Punto 4: dato pubblico PUB-RAW-01

`RawImport.T_M3A_RAW_09_pub_raw_01_bayer_samples_import` eseguito e verde sugli 8 file di PUB-RAW-01 in `data/public/raw-pixls-us/` (collegamento simbolico `data`, fuori da Git; test in `tests/unit/test_raw_import.cpp`, U4; contratto ADR-08 §6 riga T-M3A-RAW-09).

## Punto 5: review indipendente PASS per ogni unità

Le unità chiuse col PASS di review o per arbitraggio del coordinatore (tetto di 2 giri, `AGENTS.md`) seguito da una review indipendente sul delta o di chiusura. Le review di chiusura per il gate (Claude Opus in sola lettura sulle API, `tools/delegate.sh - review`) hanno verificato, per le unità rimaste con rilievi BLOCCANTE o MED al giro 2, che ogni rilievo sia chiuso o dichiarato come limite.

| Unità | Record | Chiusura |
|---|---|---|
| Emendamenti di contratto | `M3a-emendamenti.md` | review PASS al giro 2 |
| ADR-08, U2 | `M3a-ADR08-U2.md` | review PASS al giro 2 |
| ADR-08, U3 | `M3a-ADR08-U3.md` | arbitraggio; review di chiusura PASS (`tmp/api/gate-ADR08-U3-review.json`) |
| ADR-08, U4 | `M3a-ADR08-U4.md` | review PASS al giro 1 |
| Contratto F-IO-03 | `M3a-F-IO-03-contratto.md` | arbitraggio; review di chiusura PASS (`tmp/api/gate-F-IO-03-contratto-review.json`) |
| Contratto F-IO-08 | `M3a-F-IO-08-contratto.md` | arbitraggio; review di chiusura PASS (`tmp/api/gate-F-IO-08-contratto-review.json`) |
| U0 | `M3a-U0.md` | review PASS al giro 2 |
| U1 | `M3a-U1.md` | review PASS al giro 2 |
| U1b | `M3a-U1b.md` | review PASS al giro 2 |
| U2 | `M3a-U2.md` | review PASS al giro 2 |
| U3 | `M3a-U3.md` | arbitraggio; review di chiusura PASS (`tmp/api/gate-U3-review.json`) |
| U4 | `M3a-U4.md` | arbitraggio; review sul delta PASS |
| U5 | `M3a-U5.md` | test PASS al giro 2; codice per arbitraggio, review sul delta PASS |
| U6 | `M3a-U6.md` | test PASS al giro 2; codice per arbitraggio, review sul delta PASS |
| U7 | `M3a-U7.md` | arbitraggio; review sul delta PASS |
| U8a | `M3a-U8a.md` | arbitraggio; review sul delta PASS |
| U8b | `M3a-U8b.md` | arbitraggio; review sul delta PASS; integrazione con U13b PASS |
| U9 | `M3a-U9.md` | arbitraggio; review sul delta PASS |
| U10 | `M3a-U10.md` | arbitraggio; review di chiusura PASS (`tmp/api/gate-U10-review.json`) |
| U11 | `M3a-U11.md` | arbitraggio; review di chiusura PASS (`tmp/api/gate-U11-review.json`) |
| U12-contract | `M3a-U12-contract.md` | arbitraggio; review di chiusura PASS (`tmp/api/gate-U12-contract-review.json`) |
| U12 | `M3a-U12.md` | arbitraggio; review sul delta PASS |
| U13a | `M3a-U13a.md` | test PASS al giro 2; codice per arbitraggio, review sul delta PASS |
| U13b | `M3a-U13b.md` | test PASS al giro 2; codice per arbitraggio, review sul delta PASS; integrazione con U8b PASS |

Lavoro prima del gate (in questo commit): `temperature_abs_tol_c` obbligatorio con entrambe le politiche (ADR-16 §b.9) con i test dei domini e degli stop termici di U7, impalcature di test di U6 e U8a tolte, copertura completa dei campi di `dng_json` (`tests/unit/dng_state_changes.hpp`), test di relink senza riuso dell'inode. Review indipendente PASS (`tmp/api/pregate-review.json`); i LOW sono rinviati.

## Limiti

- `T-M2R2-09.real_palomar_product_masters` sotto ASan dura circa 1620-1670 s contro un timeout di 1800 s: margine dell'8%, superato quando gira in parallelo agli altri slow. Esecuzione del gate: timeout nella suite piena, PASS da solo (sopra). Da risolvere dopo il gate (tempo del test sotto ASan o esecuzione seriale degli slow pesanti), senza alzare il timeout come scorciatoia.
- I limiti dichiarati nei record delle unità restano validi (per esempio `fits.coverage` in U6, cap delle IFD e malloc non provocato dai test in U13).
- Rinviati dopo il gate, senza effetto sui 5 punti: accenti ASCII in circa 111 file; trace check di M3.a dentro `tools/check.sh`; stop termici di U7 da spostare da CAL-02 a CAL-06; commento in `src/core/pack_coverage.cpp:28`; due difetti delle fixture in `tests/unit/relink_support.hpp`; `plans/M3.md:61` cita libtiff 4.7.0; percorso dello slot `master.combine` di U8b senza test.

Conteggi e hash generati:

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/test-matrix.md` | `4c0cdc989add3ce566996917155b30f0dbe2559312c7dfdebf7ef482d5ea02bc` |
| `plans/M3a.md` | `3794610361e92d8e1433a64b8f7df1a29e840ddd02de7b003744819365a3eb91` |
| `tests/trace/M3a.tsv` | `b029c3b8cbd5b5382c21b266867bf9ddc0ae4052d6483d783954f2216bd51fe6` |
| `tools/trace.py` | `ef7185ebc60b152f3b8436c55ef7b94704bfddf33568a9ebf21939702c08f040` |

- Test dev-full (`tmp/dev-full-junit.xml`): totali 1212, fast 1190, slow 22, falliti 1
- Test release (`tmp/release-junit.xml`): totali 1212, fast 1190, slow 22, falliti 0
- Test dev-palomar (`tmp/dev-junit-palomar.xml`): totali 1, fast 0, slow 1, falliti 0
- Tracciabilità (`tests/trace/M3a.tsv`, build `build/dev`, junit `tmp/dev-full-junit.xml`): trace: 8 ID nello stadio M3.a, 0 non coperti
- Tracciabilità (`tests/trace/M3a.tsv`, build `build/release`, junit `tmp/release-junit.xml`): trace: 8 ID nello stadio M3.a, 0 non coperti
<!-- evidence:end -->
