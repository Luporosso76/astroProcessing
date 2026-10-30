# M3.a, unità U0: lettura degli input indipendente dal formato (lacuna L1)

Stato: **REVIEW PASS** al giro 2. Unità: U0 di `plans/M3a.md`, prerequisito di F-IO-02. Contratto: `docs/contracts/BASE-confini.md` §2.5 «Emendamento L1» (OB-11, OB-12 emendato), §3.1 punto 1, §3.2 (T-M3BASE-20…22), §4; `docs/contracts/ADR-08-decoder-pack.md` §2.1.

| Campo | Valore |
|---|---|
| Unità | M3.a / U0 (codice della base) |
| Scopo | Dichiarazione del formato d'ingresso (prefisso dei codici, limite, piano degli header) ed elenco delle dichiarazioni; protocollo di apertura unico con la dichiarazione; piano facoltativo nella lettura verificata; formato del ruolo nella riverifica e nel registro dei tipi |
| Comando | Dev: `tools/check.sh` con `CMAKE_BUILD_PARALLEL_LEVEL=4` e `CTEST_PARALLEL_LEVEL=4` (`tmp/u0-check.sh`, log `tmp/u0-check.log`), poi `tmp/u0-after-check.sh` (build dev, `ctest --preset dev -j4 --output-junit tmp/dev-junit.xml`, `tools/lint.sh`, `python3 -S tools/check_job_types.py .`, `python3 -S tools/trace.py check --stage M2 ...`). Release: `tmp/u0-release.sh` (`cmake --preset release`, build con 4 job, `ctest --preset release -j4 --output-junit tmp/release-junit.xml`, `trace.py check` sul junit release). Mutazioni: `python3 -S tmp/u0-mut/make.py`, poi `tmp/u0-mut/run.sh` (giro 1) e `tmp/u0-mut/run2.sh` (giro 2), ciascuno `tools/mutate.sh` per patch. Blocco: `python3 -S tools/evidence.py docs/evidence/M3a-U0.md` |
| Input | Codice a `57013da` più i test di `tmp/u0-tests.patch` (scritti da un altro agente) e le modifiche elencate sotto; fixture sintetiche della suite |
| Versioni | GCC 15.2.0, CMake 4.2.3; preset `release` e `dev` (ASan e UBSan) |
| Ambiente | Linux 7.0.0-38-generic x86_64, 16 CPU, worktree isolato senza il collegamento `data` ai dati reali, 4 job |
| Risultato | Vedi «Rossi prima e dopo» e il blocco generato |
| Output | Log e junit sotto `tmp/` citati nel comando |
| Ambito | OB-11, OB-12 per l'emendamento L1. Fuori: il tipo di job RAW e le varianti RAW di T-M3BASE-11 e T-M3BASE-12, la prenotazione dei byte del file RAW (OB-08) |
| Esito | PASS: T-M3BASE-20…22 da non compilanti a verdi, nessun altro test cambiato oltre i prerequisiti di dati reali; mutazioni rilevate tranne `m06`, dichiarata; review PASS al giro 2 con un LOW aperto sul piano |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Modifiche

Codice di prodotto (`src/core/`), 2 file nuovi e 12 modificati:

- `include/astro/input_format.hpp`, `input_format.cpp` (nuovi): `struct input_format` (`prefix`, `max_file_bytes`, `header_plan`), `raw_limits::max_file_bytes` = 1 073 741 824, `fits_input_format()` (limite `fits_limits::max_file_bytes`, con piano), `raw_input_format()` (senza piano), `input_formats()`.
- `fits.hpp`, `fits.cpp`: `open_input(path, format)` è il protocollo di apertura unico (la sola `::open` del file); notifica `input.open` (percorso) e `input.open_format` (`<prefisso>\t<percorso>`); codici `input.missing` e `<prefisso>.open`, `.fstat`, `.not_regular`, `.max_file_bytes` col limite del formato prima di ogni lettura e allocazione. `open_fits_input` chiama `open_input` con la dichiarazione FITS. `fits_input` porta la propria dichiarazione (`format()`); gli errori di sistema dopo l'apertura passano da `input_syscall_fail`, che prende il prefisso dall'input.
- `hash.hpp`, `hash.cpp`: `read_verified_input` riceve `const fits_plan*`, presente se e solo se il formato ha il piano (`ASTRO_ASSERT`), e confronta il piano solo se presente; `reverify_input` riceve la dichiarazione del ruolo, uguale a quella dell'input aperto se c'è (`ASTRO_ASSERT`), e riapre con `open_input`.
- `job_types.hpp`, `job_types.cpp`: `job_type_entry::role_formats`; ogni ruolo dei tipi registrati ha la dichiarazione FITS.
- `job_probe.hpp`: documentazione del punto `input.open_format`.
- `calibration_job.cpp`, `calibration_blocks.cpp`, `master.cpp`, `import_job.cpp`, `CMakeLists.txt`: chiamate alle firme nuove (`&plans[i]`, `fits_input_format()`), sorgente nuovo.

Test:

- `tests/unit/test_conformance_format.cpp` (dalla patch dei test): asserzioni invariate; aggiunta la sola sostituzione `operator new(std::size_t, const std::nothrow_t&)`: nel preset `dev` la forma nothrow veniva dal runtime di ASan e il blocco arrivava alla `operator delete` sostituita dal file (`alloc-dealloc-mismatch` dentro gtest, tutti i casi abortiti). Con `ASAN_OPTIONS=alloc_dealloc_mismatch=0`, prima dell'aggiunta, i 9 casi passavano (`tmp/u0-format-noasanmm.log`).
- `tests/unit/CMakeLists.txt`: tolta l'opzione `ASTRO_U0_RED_TESTS`; il binario è nella build di default e nella scansione di `T-M2F-01.offline`.
- `tests/unit/test_hash.cpp`: `HashDeathTest.verified_read_and_reverify_require_the_declared_format_of_the_input` (le due `ASTRO_ASSERT`), `Hash.read_and_path_codes_use_the_prefix_of_the_declared_format` (codice, categoria e contesto di `<prefisso>.read` e `<prefisso>.fstat` dopo l'apertura, per ogni formato); chiamate alle firme nuove.
- `tests/unit/test_conformance_gaps.cpp`: chiamate alle firme nuove, asserzioni invariate.

Impatto letto con `codegraph impact` prima delle modifiche (indice locale del worktree, `codegraph init -i`): `open_fits_input`, `read_verified_input`, `reverify_input`, `job_type_entry` toccano i test di `test_fits`, `test_inventory`, `test_conformance`, `test_conformance_gaps`, `test_simd`, `test_recovery`, `test_import`, `test_calibration*`, `test_master`, `test_phase`, `test_hash` e `m2e_workflow`: tutti nel ctest completo di entrambi i preset.

## Rossi prima e dopo

Prima del codice `test_conformance_format.cpp` non compila contro il prodotto (`astro/input_format.hpp`, `open_input` e `job_type_entry::role_formats` assenti): i casi T-M3BASE-20…22 erano esclusi dalla build con `ASTRO_U0_RED_TESTS`.

| Test ctest | Prima | Dopo |
|---|---|---|
| `ConformanceFormat.T_M3BASE_20_open_protocol_per_declared_format` | non compila | verde |
| `ConformanceFormat.T_M3BASE_20_single_owner_of_the_open_protocol` | non compila | verde |
| `ConformanceFormat.T_M3BASE_21_verified_read_without_header_plan` | non compila | verde |
| `Types/ConformanceFormatRole.T_M3BASE_22_*` (6 istanze, una per voce e variante) | non compila | verde |

Suite completa: release 633 test, 4 falliti, tutti su dati reali assenti nel worktree (`Fits.REAL_DATA_PUB_LFC_01`, `T-M2E-13.real_palomar_lfc`, `T-M2R2-09.real_palomar_product_masters`, `T-M2R1-09.public_samples`); dev 627 test, 1 fallito (`Fits.REAL_DATA_PUB_LFC_01`). Lint e registro dei tipi verdi; `trace.py check` sul junit release: 24 ID non coperti, gli stessi dei dati reali assenti.

## Mutazioni

`tools/mutate.sh` sul preset `release`, 4 job, una patch per controllo aggiunto, baseline verde, sorgente ripristinato e verificato con `cmp` (riepiloghi `tmp/u0-mut/summary-round1.txt` e `tmp/u0-mut/summary-round2.txt`, log per patch in `tmp/u0-mut/`):

| Patch | Mutazione | Test | Esito |
|---|---|---|---|
| `m01-open-limit-fits` | limite FITS per ogni formato | `T_M3BASE_20` | RILEVATA |
| `m02-open-code-fits` | `.open` sempre `fits` | `T_M3BASE_20` | RILEVATA |
| `m03-not-regular-code-fits` | `.not_regular` sempre `fits` | `T_M3BASE_20` | RILEVATA |
| `m04-limit-code-fits` | `.max_file_bytes` sempre `fits` | `T_M3BASE_20` | RILEVATA |
| `m05-probe-format-fits` | `input.open_format` sempre `fits` | `T_M3BASE_20` | RILEVATA |
| `m06-open-fstat-code-fits` | `.fstat` del protocollo sempre `fits` | `ConformanceFormat`, `Hash` | SOPRAVVISSUTA |
| `m11-plan-skipped` | piano mai confrontato | `HashGaps` | RILEVATA |
| `m12-read-assert-removed` | lettura verificata senza controllo del piano del formato | `HashDeathTest` | RILEVATA |
| `m13-reverify-opens-fits` | riverifica con la dichiarazione FITS | `T_M3BASE_21` | RILEVATA |
| `m14-reverify-assert-removed` | riverifica senza controllo del formato dell'input aperto | `HashDeathTest` | RILEVATA |
| `m15-list-without-raw` | elenco senza RAW | `T_M3BASE_20`, `T_M3BASE_21` | RILEVATA |
| `m16-raw-limit-off-by-one` | limite RAW più uno | `T_M3BASE_20` | RILEVATA |
| `m17-raw-with-header-plan` | RAW con piano degli header | `T_M3BASE_20`, `T_M3BASE_21` | RILEVATA |
| `m18-role-declared-raw` | ruolo `dark` dichiarato RAW | `ConformanceFormatRole` | RILEVATA |
| `m19-role-without-format` | ruolo `file` senza formato | `ConformanceFormatRole` | RILEVATA |
| `m20-syscall-helper-fits` | `input_syscall_fail` con prefisso `fits` (giro 2) | `Hash.read_and_path_codes` | RILEVATA |
| `m21-syscall-errno-dropped` | `input_syscall_fail` senza `errno` nel contesto (giro 2) | `Hash.read_and_path_codes` | RILEVATA |

Al giro 1 c'erano anche `m07` (`.read` dell'errore iniettato, RILEVATA), `m08` (`.fstat` di `check_path_names`, RILEVATA), `m09` (`.fstat` di `check_unchanged`, SOPRAVVISSUTA) e `m10` (`.read` del `pread` di controllo, SOPRAVVISSUTA). Al giro 2 quei rami chiamano `input_syscall_fail` senza formato: il testo mutato non esiste più e la classe è coperta da `m20`.

`m06` è un limite della prova: un `fstat` sul descrittore appena aperto non fallisce nei test. Il ramo usa la stessa variabile `format` dei rami `.open`, `.not_regular` e `.max_file_bytes`, rilevati da `m02`…`m04`.

## Scelte

- **Sede delle dichiarazioni.** `input_format.cpp`, separato da `fits.cpp`, perché la dichiarazione RAW non dipende da FITS; il protocollo resta in `fits.cpp` con la sola `::open` del file (T-M3BASE-20, proprietario unico). Il tipo dell'input aperto resta `fits_input`, come chiede l'interfaccia dei test.
- **Precondizioni come `ASTRO_ASSERT`.** Un piano presente per un formato senza piano, o assente per FITS, e una riverifica con un formato diverso da quello dell'input aperto sono errori di programmazione del chiamante, non condizioni dei dati: come `digest_input` con `work` vuoto, si fermano con l'asserzione.
- **Formato dei ruoli registrati.** Tutti FITS, perché ogni tipo registrato decodifica con `decode_fits`; gli operatori aprono e riverificano con la dichiarazione FITS (`open_fits_input`, `fits_input_format()`), che T-M3BASE-22 confronta con `role_formats`.

## Limiti

- Nessun tipo registrato ha un ruolo RAW: il percorso RAW è provato al confine senza job (T-M3BASE-20, T-M3BASE-21). Le varianti RAW di T-M3BASE-11 e T-M3BASE-12, che `plans/M3a.md` §U0 elenca, restano all'unità che registra il tipo RAW.
- Il valore di `raw_limits::max_file_bytes` è la stima non misurata di ADR-08.b §2.1.
- Gli errori di `fstat` su un descrittore aperto e del `pread` di controllo non si provocano nei test (`m06`; vedi «Mutazioni»).
- I 4 test su dati reali non girano in questo worktree, che non ha il collegamento `data`: esercitano la lettura FITS modificata e vanno rieseguiti nel checkout principale. Per la stessa ragione `tools/check.sh` si ferma al ctest dev; lint, registro dei tipi e `trace.py check` sono stati lanciati a parte.

## Review

- Giro 1: `tools/review.sh u0-round1 tmp/u0-facts-round1.md` (Codex `gpt-6-astra`, sola lettura), delta `tmp/u0-round1.diff`, artefatto `.omc/artifacts/ask/codex-u0-round1-20261030-1339.md`: FAIL. [MED] i prefissi dei rami di errore di sistema dopo l'apertura (`pread`, `fstat` di `check_unchanged` e `identity_of`, `fstat` del protocollo, `pread` di controllo) non hanno prova discriminante; `m06`, `m09`, `m10` sopravvissute. [LOW] `plans/M3a.md` §U0 elenca ancora T-M3BASE-11/12 sulla variante RAW. [LOW] il contratto citava questo record, non ancora scritto.
- Correzione (delta `tmp/u0-round2.diff`): gli errori di sistema su un input aperto passano tutti da `input_syscall_fail`, che prende il prefisso dall'input (nessun formato nel chiamante); `Hash.read_and_path_codes_use_the_prefix_of_the_declared_format` verifica anche il contesto (`errno`, `percorso`); mutazioni `m20` e `m21` RILEVATE; il ramo `fstat` del protocollo (`m06`) è dichiarato come limite nel contratto e qui. Record scritto. Il piano non è stato toccato: il rinvio delle varianti RAW di T-M3BASE-11/12 è dichiarato nel contratto e nei «Limiti». Dopo il delta: dev 627 test e release 633 con i soli fallimenti su dati reali, lint e registro verdi.
- Giro 2: `tools/review.sh u0-round2 tmp/u0-facts-round2.md`, delta `tmp/u0-round2.diff`, artefatto `.omc/artifacts/ask/codex-u0-round2-20261030-1351.md`: PASS. Resta [LOW] `plans/M3a.md` §U0: allineare il piano al rinvio delle varianti RAW di T-M3BASE-11/12, fuori dal perimetro documentale di questa unità.

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/BASE-confini.md` | `573fba6402d89abb5a8df3566792f36a85602519ff6525da755d7d5c0472eccb` |
| `src/core/fits.cpp` | `f6f18576e927dcedd3c8c920f9923058fb2c9e8d7dc5523114272f75ad046967` |
| `src/core/hash.cpp` | `f8507547472b5b58b882ae069256530cb1a091b879a373a749afbfde563d0823` |
| `src/core/include/astro/fits.hpp` | `37d4c4a21797c3a54c1d1bdc80c2e12b383d00d729571a3e41ef8fe07976abd7` |
| `src/core/include/astro/hash.hpp` | `35127c7ac5255cc715ad016da94c4958136dd39a2342ee716324f3db2d3d4a24` |
| `src/core/include/astro/input_format.hpp` | `e30c81080600aec605fd3730bad533e1b77774d531a26441e4269990b986831f` |
| `src/core/include/astro/job_types.hpp` | `98d1f80ede5ecb0da92cdb55f2aa8cec050b0cb163b6df1c7968b7367ace70e6` |
| `src/core/input_format.cpp` | `343f626aebe72b9496007a01d144a40fd24613d94b58f814a9afcdd608570704` |
| `src/core/job_types.cpp` | `fda920302c2e974237f08f4ff6c1a0a86f7da20548d419fdfed899937b5d5255` |
| `tests/unit/test_conformance_format.cpp` | `34d9def5399ec94003cb775a88f9ae2697ed5e26fb6fddcb4d3e6264dad7910e` |
| `tests/unit/test_conformance_gaps.cpp` | `c451f8af129f3194e2824541571ecc2f0cd7f389a9b36de80c855c48df46a9af` |
| `tests/unit/test_hash.cpp` | `77efb55f69446ee0b932b63687f7d6f6e187e78eddad6f7805741213902fb6c3` |

- Test release (`tmp/release-junit.xml`): totali 867, fast 851, slow 16, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 852, fast 852, slow 0, falliti 0
<!-- evidence:end -->
