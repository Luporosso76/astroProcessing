# M3.a, unità U1b: capacità di misura `measure`

Stato: **REVIEW PASS** al giro 2. Unità: U1b di `plans/M3a.md`, ID T-M3A-TYP-15 e T-M3A-TYP-16 (stadio M3.a in `docs/test-matrix.md`). Contratto: `docs/contracts/ADR-03-pixel-maschere.md` parte `.b`, §b.13, §b.15, §b.16, §b.17.

| Campo | Valore |
|---|---|
| Unità | M3.a / U1b |
| Scopo | `measure_limit` con i 19 codici di §b.13, `measure_capability`, `image::measure` (facoltativa), `validate_image`, `equivalent_content`, `history_limits`, `measure_limits`, `merge_measure`, `assess_measure`, `require_measure_ready`, keyword FITS `ASTRMCAP` e `ASTRML01`…`ASTRML99` in scrittura e lettura |
| Comando | Dev: `tools/check.sh` (log `tmp/u1b-check.log`, fermo al ctest per i dati reali assenti), poi `tools/lint.sh`. Release: `tmp/u1b-release.sh` (configurazione, build, `ctest --preset release --output-junit tmp/release-junit.xml`; log `tmp/u1b-release.log`). Mutazioni: `python3 -S tmp/u1b-mut/make.py` (patch), `tmp/u1b-mut/run.sh` (un `tools/mutate.sh` per patch, regex `^Measure\.`), m43 con `tmp/u1b-r2.sh`. Blocco: `python3 -S tools/evidence.py docs/evidence/M3a-U1b.md` |
| Input | Codice a `1a99a29` più i test di `tmp/u1b-tests.patch` (scritti da un altro agente) e le modifiche elencate sotto |
| Versioni | GCC 15.2.0, CMake 4.2.3; preset `release` e `dev` (ASan e UBSan) |
| Ambiente | Linux 7.0.0-38-generic x86_64, worktree isolato senza il collegamento `data` ai dati reali, 4 job |
| Risultato | Vedi «Rossi prima e dopo» e il blocco generato |
| Output | Log, junit e patch sotto `tmp/` citati nel comando |
| Ambito | §b.13, §b.15, §b.16. Fuori (perimetro ristretto del piano): `variance_scope` e i codici `variance.scope_unknown` e `variance.marginal` come condizioni calcolate, `demosaic`, l'obbligo di `measure` in `operator_output_invariants_hold`, ogni operatore che aggiunge codici memorizzati (M3.c, M3.d, M3.g, M3.k) |
| Esito | Test T-M3A-TYP-15 e 16: 44 casi dei test scritti più 6 aggiunti, tutti verdi; 43 mutazioni su 43 rilevate; review PASS al giro 2 con un solo rilievo LOW |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Sonnet 5.5), 30 ottobre 2026 |

## Modifiche

Codice di prodotto (`src/core/`), circa 200 righe:

- `include/astro/image.hpp`, `image.cpp`: `measure_limit` (19 enumeratori nell'ordine canonico di §b.13), `to_string`, `is_derived_limit` (i cinque derivati), `measure_capability`, `measure_acceptance`, `measure_assessment`, `image::measure` (`std::optional`, assente = non valutata), `equivalent_content` confronta `measure` (presenza e codici); `validate_image` rifiuta codici derivati, duplicati o fuori ordine (`invalid_input`, `image.measure`); `history_limits`, `measure_limits` (unione ordinata di storia e codici derivati calcolati: `linearity.nonlinear`, `linearity.unknown`, `variance.absent`), `merge_measure` (unione ordinata senza duplicati; un codice proprio derivato: `invalid_input`, `image.measure`), `assess_measure`, `require_measure_ready` (`incompatible`, `measure.limit_blocked`, codici nel contesto).
- `fits_header.cpp`, `fits_internal.hpp`, `fits.cpp`, `fits_blocks.cpp`: `ASTRMCAP` e `ASTRMLnn` interpretate (non finiscono fra le keyword non interpretate); lettura con gli stop di §b.16 (`fits.measure_keywords` per `ASTRMCAP = F`, `ASTRMLnn` senza `ASTRMCAP`, buchi, ordine, duplicati; `fits.measure_code` per codici derivati o ignoti; `fits.keyword_value` per i tipi sbagliati); scrittura di `ASTRMCAP` e `ASTRML01`… numerate senza buchi quando `measure` è presente; `check_header_matches` confronta `measure`; il lettore a blocchi e lo scrittore in streaming la propagano.

Nessun operatore esistente cambia: `measure` resta facoltativa e nessun operatore la produce ancora.

Test:

- `tests/unit/test_measure.cpp` (dalla patch dei test): asserzioni invariate; il buffer di `ml_key` passa da 16 a 32 byte (`-Wformat-truncation` come errore nel preset release). Casi aggiunti: `T_M3A_TYP_15_read_astrml99_alone_is_a_numbering_gap`, `..._writer_stops_on_a_stale_measure_card_in_header_cards`, `..._stream_writer_keeps_the_measure_cards`, `..._block_source_header_carries_the_measure`, `T_M3A_TYP_16_merge_refuses_a_derived_code_of_its_own`, `T_M3A_TYP_16_variance_present_removes_variance_absent`.
- `tests/unit/CMakeLists.txt`: tolta l'opzione `ASTRO_U1B_RED_TESTS`; il binario è nella build di default e nella scansione di `T-M2F-01.offline`; `ASTRO_TEST_TMP` per il caso del lettore a blocchi.

Impatto letto con `codegraph impact` prima delle modifiche: `validate_image` e `equivalent_content` toccano `test_calibration*`, `test_master`, `test_import_real` e `m2e_workflow`, tutti nel ctest completo dei due preset; le modifiche sono solo additive.

## Rossi prima e dopo

Prima del codice `test_measure.cpp` non compila contro il prodotto (`image` senza `measure`, tipi `measure_limit` assenti); i test erano rossi per costruzione nella fase dell'altro agente (opzione `ASTRO_U1B_RED_TESTS`).

| Test ctest | Prima | Dopo |
|---|---|---|
| `Measure.T_M3A_TYP_15_*` (tabella, `validate_image`, FITS scrittura e lettura, `equivalent_content`) | non compila | verde |
| `Measure.T_M3A_TYP_16_*` (`history_limits`, `measure_limits`, `merge_measure`, `assess_measure`, `require_measure_ready`) | non compila | verde |

Suite completa: dev 737 test, 1 fallito (`Fits.REAL_DATA_PUB_LFC_01_...`); release 743 test, 4 falliti (`Fits.REAL_DATA_PUB_LFC_01_...`, `T-M2E-13.real_palomar_lfc`, `T-M2R2-09.real_palomar_product_masters`, `T-M2R1-09.public_samples`): tutti su dati reali assenti nel worktree. Lint verde. Dopo l'ultimo caso aggiunto: `ctest --preset release -R '^Measure\.'`, 50 test verdi.

## Mutazioni

`tools/mutate.sh` sul preset `release`, regex `^Measure\.`, una patch per controllo, baseline verde, sorgente ripristinato e verificato con `cmp`. Patch generate da `tmp/u1b-mut/make.py`, riepilogo `tmp/u1b-mut/summary.txt` (m01-m42) e `tmp/u1b-mut/m43.log`.

| Controllo | Mutazioni | Esito |
|---|---|---|
| `validate_image` | m01 derivato accettato; m02 duplicato accettato; m03 fuori ordine accettato; m04 non richiamata | RILEVATA ×4 |
| `is_derived_limit` | m05, m06 limiti dell'intervallo spostati | RILEVATA ×2 |
| `equivalent_content` | m07 senza `measure` | RILEVATA |
| `history_limits` | m08 fase `raw` invertita; m09 fase assente come `raw`; m10 ignora `measure` | RILEVATA ×3 |
| `measure_limits` | m11 senza `linearity.nonlinear`; m12 senza `linearity.unknown`; m13 senza `variance.absent`; m43 `variance.absent` sempre; m14 senza ordinamento | RILEVATA ×5 |
| `merge_measure` | m15 senza deduplica; m16 senza ordinamento; m17 senza codici propri; m18 senza storia degli ingressi; m19 codice proprio derivato accettato | RILEVATA ×5 |
| `assess_measure`, `require_measure_ready` | m20 non blocca mai; m21 blocca gli accettati; m22 non si ferma; m23 codice dello stop; m24 senza contesto | RILEVATA ×5 |
| `to_string` | m25 testo di un codice | RILEVATA |
| Lettura FITS | m26 buchi; m27 `ASTRMLnn` senza `ASTRMCAP`; m28 `ASTRMCAP = F`; m29 ordine; m30 duplicato; m31 derivato accettato; m32 ignoto ignorato; m33 `ASTRMCAP` letta come stringa; m34 non assegnata (lettore a blocchi); m35 non assegnata (`decode_fits`); m42 limite delle card 98 | RILEVATA ×11 |
| Scrittura FITS | m36 senza `ASTRMCAP`; m37 numerazione da 0; m38 senza codici; m39 `check_header_matches` senza `measure`; m40 keyword non interpretate; m41 scrittore in streaming senza `measure` | RILEVATA ×6 |

Al primo passaggio m04, m20, m38 e m40 non compilavano (`-Werror`: funzione o parametro inutilizzati, confronto sempre falso); riscritte in forma compilabile. Al primo passaggio m19, m34, m39, m41 e m42 sopravvivevano: mancavano i casi aggiunti sopra, che le uccidono. m43 è stata aggiunta dopo il rilievo del giro 1 (nessun test con varianza presente).

## Scelte

- **`is_derived_limit` pubblica.** Serve a `validate_image`, `merge_measure` e al lettore FITS per riconoscere i derivati: una sola definizione, non tre copie.
- **`merge_measure` rifiuta un codice proprio derivato** (`invalid_input`, `image.measure`): §b.13 dice che un derivato non si memorizza mai; accettarlo e scartarlo sarebbe un fallback silenzioso.
- **Lettura FITS in due passi.** Prima la numerazione (buchi e `ASTRMLnn` senza `ASTRMCAP`), poi il valore di ogni card nell'ordine: un codice derivato o ignoto è `unsupported` anche se preceduto da un'altra violazione di ordine nella stessa riga, perché ogni card è interpretata prima di confrontarla con la precedente.

## Limiti

- Nessun operatore popola ancora `measure`: la cablatura nei produttori è delle unità successive (M3.c, M3.d, M3.g, M3.k). Le funzioni sono provate su immagini costruite nei test.
- `variance.scope_unknown` e `variance.marginal` hanno un codice e un testo ma nessuna condizione calcolata: `variance_scope` non esiste ancora (M3.d). `measure_limits` non li restituisce mai.
- `operator_output_invariants_hold` non richiede `measure` (intervento di base I6).
- I 4 test su dati reali non girano in questo worktree, che non ha il collegamento `data`: vanno rieseguiti nel checkout principale. Per la stessa ragione `tools/check.sh` si ferma al ctest dev; lint è stato lanciato a parte. `trace.py check` non è stato lanciato.
- Il junit del blocco generato è quello delle suite complete, eseguite prima dell'ultimo caso aggiunto (6° caso aggiunto: solo `test_measure`, eseguito a parte).

## Review

- Giro 1: `tools/review.sh u1b-round1 tmp/u1b-facts-round1.md` (Codex `gpt-6-astra`, sola lettura), delta `tmp/u1b-round1.diff`, artefatto `.omc/artifacts/ask/codex-u1b-round1-20261030-1733.md`: FAIL. [MED] nessun test con varianza presente (m13 verificava solo l'omissione). [LOW] riferimenti documentali (record e sezione del piano) da allineare. [LOW] intestazione dei casi aggiunti con storia della modifica.
- Correzione (delta `tmp/u1b-round2.diff`): caso `T_M3A_TYP_16_variance_present_removes_variance_absent` e mutazione m43; intestazione funzionale; questo record.
- Giro 2: `tools/review.sh u1b-round2 tmp/u1b-facts-round2.md`, artefatto `.omc/artifacts/ask/codex-u1b-round2-20261030-1737.md`: PASS. Un [LOW] aperto: la sezione «Casi spostati da U1b» di `plans/M3a.md` manca nel worktree (aggiornamento del coordinatore, da integrare alla chiusura dell'unità).

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-03-pixel-maschere.md` | `ebd000bd2f7ce1ca59fb85ff7cf7494d7379253495257fdd9955d9d440053ccb` |
| `src/core/fits.cpp` | `f6f18576e927dcedd3c8c920f9923058fb2c9e8d7dc5523114272f75ad046967` |
| `src/core/fits_blocks.cpp` | `f6adaddd1dcf88337e9e2874a2f7a336d19645398eb2ca05dde85d90ff066ab7` |
| `src/core/fits_header.cpp` | `d07ef32366ccae41790f4e45567ba416ff9bcc5a0f6ecc2934b55d20d85cb592` |
| `src/core/fits_internal.hpp` | `49820516e090503b4950b7c97a18b7513d7645a8096eb1f8cc8f6c9877ade4e0` |
| `src/core/image.cpp` | `a563aaebbf3523e1ebd7d0c60d660267268d373bbb6f81ceb010d9db92bd959f` |
| `src/core/include/astro/image.hpp` | `3a9a629e910612d85f424f5e327057920028b251067f33467d2a130767cc4e67` |
| `tests/unit/CMakeLists.txt` | `7e155a4514b618f4433889c5237aa4c0cb03fe36da770f71d9ee3defa4f97382` |
| `tests/unit/test_measure.cpp` | `1358f7d21b97180fe744f3b5ef2e22047009a81033aa506072468d56a51df838` |

- Test release (`tmp/release-junit.xml`): totali 805, fast 798, slow 7, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 799, fast 799, slow 0, falliti 0
<!-- evidence:end -->
