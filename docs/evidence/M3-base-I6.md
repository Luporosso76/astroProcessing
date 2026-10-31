# M3, base comune, intervento I6: `measure` in ogni uscita di operatore (OB-20)

Stato: **chiusa**. Review FAIL al giro 2 per un MED sui test, arbitrata dal coordinatore: test aggiunti da un agente dei test, difetto di prodotto che quei test hanno trovato corretto, review indipendente sul delta PASS. Unità: M3, base comune, riga di debito I6 del §4 di `docs/contracts/BASE-confini.md`; fonte ADR-03.b §b.13.

| Campo | Valore |
|---|---|
| Unità | M3 / base comune, intervento I6 |
| Scopo | `operator_output_invariants_hold` richiede `measure`; calibrazione e master la impostano come unione esatta dei codici ereditati |
| Comando | Build pulita nel checkout principale: `tools/check.sh`; `cmake --preset release && cmake --build --preset release -j6 && ctest --preset release -j4 --output-junit tmp/release-junit.xml`; mutazioni `tools/mutate.sh <file> <patch> OB_20 <log>` |
| Input | Codice a `a18d634` più il diff di I6 |
| Ambiente | Linux x86_64, GCC 15, presets `dev` (ASan e UBSan) e `release`, collegamento `data` presente |
| Risultato | Nel blocco generato sotto (dev e release completi, `slow` compresi in release) |
| Ambito | OB-20 e riga di debito I6. Fuori: `io.import`, `io.thumbnail`, `cache.rebuild` (non pubblicano `image`) |
| Autori e data | Codice: agente Claude Sonnet 5.5 (API) più correzione delle card del coordinatore (Opus 5.5); test: agenti Claude Sonnet 5.5 (API), diversi dall'autore del codice; 31 ottobre 2026 |

## Modifica

- `src/core/image.cpp`: `operator_output_invariants_hold` è falso se `measure` è assente (commento in `image.hpp`).
- `src/core/calibration.cpp` (`prepare_impl`, comune a intera e blocchi): `measure = merge_measure({light, dark, flat, dark_flat}, {})`; il ciclo che copia le card del light scarta, oltre a `ASTRPHAS`, anche `ASTRMCAP` e `ASTRMLnn`, che il writer rigenera da `measure`. Senza questo, con un'unione diversa dal light il FITS pubblicato mescolava card vecchie e nuove ed era rifiutato in lettura (`fits.measure_keywords`).
- `src/core/fits_header.cpp`, `src/core/fits_internal.hpp`: `is_measure_key` esce dal namespace anonimo, comportamento invariato.
- `src/core/master.cpp`: `finish_preflight` imposta `measure = merge_measure(header dei frame, {})`; `header_of` copia `measure`.
- Test: `MeasureOutput.T_M3BASE_OB_20_*`, `Conformance.T_M3BASE_23_OB_20_*` per tipo; unione esatta con codici distinti e sovrapposti, valori attesi scritti a mano: `CalibM2E.T_M3BASE_23_OB_20_output_measure_is_the_exact_union_of_the_four_inputs`, `Master.T_M3BASE_23_OB_20_output_measure_is_the_exact_union_of_the_frames`, `CalibBlocks.T_M3BASE_23_OB_20_published_measure_is_the_exact_union_in_both_residencies`.

## Mutazioni

`tools/mutate.sh`, preset `release`, ripristino verificato con `cmp`. Tutte rilevate; nessuna equivalente.

| Patch | Mutazione | Esito |
|---|---|---|
| `tmp/i6-mut/img.patch` | tolto il controllo `!img.measure` | RILEVATA |
| `tmp/i6-mut/cal.patch` | tolta l'impostazione in `prepare_impl` | RILEVATA |
| `tmp/i6-mut/mas.patch` | tolta l'impostazione in `finish_preflight` | RILEVATA |
| `tmp/i6t-mut/merge-empty.patch` | `merge_measure` restituisce `measure` vuoto | RILEVATA |
| `tmp/i6t-mut/cal-light.patch` | calibrazione che unisce solo il light | RILEVATA |
| `tmp/i6t-mut/header.patch` | `header_of` senza copia di `measure` | RILEVATA |
| `tmp/i6c-mut/m1_keep_measure_cards.patch` | ciclo che non scarta `ASTRMCAP`/`ASTRMLnn` | RILEVATA |

Le prime tre girano sul codice del giro 1; la correzione del giro 2 cambia solo gli argomenti di `merge_measure`, non la presenza dell'impostazione.

## Review

- Giro 1 (`tools/review.sh i6-round1`, `.omc/artifacts/ask/codex-i6-round1-20261031-0215.md`): FAIL. [BLOCCANTE] calibrazione univa solo il light: corretto. [MED] test senza verifica dell'unione dei codici ereditati; [LOW] commento storico in `tests/unit/test_measure.cpp`.
- Giro 2 (`.omc/artifacts/ask/codex-i6-round2-20261031-0239.md`): FAIL, restano il MED e il LOW sui test.
- Arbitraggio del coordinatore: MED e LOW accolti, test aggiunti da un agente dei test; il caso a blocchi ha trovato il difetto delle card, corretto nel prodotto. Review indipendente sul delta (Codex `gpt-6-astra`, `.omc/artifacts/ask/codex-i6-arbitrage.md`): PASS, MED e LOW chiusi, nessun rilievo nuovo.

## Limiti

- Nessuno specifico dell'unità. I casi 3–6 di §b.13 (`demosaic`, `variance_scope`) restano a M3.d.

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/BASE-confini.md` | `1b13b4db031eafeb75f998197e775163547625dfb626ddcbcacd07ebf9eea61e` |
| `src/core/calibration.cpp` | `1c0374933be597aea1eb326b72e5b69ad0cd5fa187be4329d08d935a7c994a51` |
| `src/core/fits_header.cpp` | `0ac963b56356149b128339563f688294664ca9c8770a523bbcf925fc690e1b45` |
| `src/core/fits_internal.hpp` | `e8b62f0bd808166ecaf603449199bfc7df1e3beb69f109a27e0466bfe8d4b09f` |
| `src/core/image.cpp` | `053fe85fa341c93294522f72de472ef37a6156dbbe500a53044da8d62e0bf0dd` |
| `src/core/include/astro/image.hpp` | `2ae3c4cc2192a8c3b6822ec390dcde0e62dbc2282178162a744d98a0ef20f626` |
| `src/core/master.cpp` | `072185946e2fc962dbe555eb86cdd7cd5a6404591c4ab29ae87750597b03d6ba` |
| `tests/unit/calib_support.hpp` | `433850f3376ede604c8ae6d311d9ec84133dc59af4cb345620431646b57ee060` |
| `tests/unit/test_calibration.cpp` | `cc0c13ef54f7e317025996276dcf0048ae2fbf5dfcf894eec3d5a50710ea90af` |
| `tests/unit/test_calibration_r32.cpp` | `181796824ef9a96b4fc3407efec000aa181bd88a88aa660b31217c281b0c9e90` |
| `tests/unit/test_conformance.cpp` | `3b5afd8777a27ca02f4c5a8004a51b2e1dfa1285ae70a3019a19d93b157bdd9d` |
| `tests/unit/test_master.cpp` | `0ebd2a6c43e09f5965de4304e3a4f1e995b270fb88989328c93470777b3b7761` |
| `tests/unit/test_measure.cpp` | `358d003a21b6bd1b560f036bb390657de7c904416cdace9813a789bdb54d0419` |

- Test release (`tmp/release-junit.xml`): totali 902, fast 884, slow 18, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 884, fast 884, slow 0, falliti 0
<!-- evidence:end -->
