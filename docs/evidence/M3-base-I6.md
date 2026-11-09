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
| `docs/contracts/BASE-confini.md` | `2c9bf62e665e3368cfbd52522ae750191c5d6cca75507655f70a990034daa1bb` |
| `src/core/calibration.cpp` | `afb96d94684d9752c001363a90557d2eed55a5ea6caa7faf87863985d9ad2e6b` |
| `src/core/fits_header.cpp` | `ff80ce493bf7c58bef5952e60fbac5bfe1dfe2c65eaa0a785482a0f249dec22d` |
| `src/core/fits_internal.hpp` | `12e42b7ff88eb36cb32625ff8703d860f31c93256f21f2e50e99853c2563d7b2` |
| `src/core/image.cpp` | `ac289188bb822477092d8ec410becc65e1fa024290c598f75c9abe79b0e3a4fe` |
| `src/core/include/astro/image.hpp` | `10df60c894f27ecf61eb43cc8d4efc8d0013bce394bbb044c3711fcf5f327537` |
| `src/core/master.cpp` | `9a2ec6568fcd55144cfc45df100c3e5b38ed7aec37b2a4ef623d271be49d5d8e` |
| `tests/unit/calib_support.hpp` | `f08abb31304517226f6a70c2d21c543dcd22108a036726304f595353856c8a8d` |
| `tests/unit/test_calibration.cpp` | `407b69be394609bc9f01fe47740b3eeca59eb3c438d3a9c1b24b2aa8a4256958` |
| `tests/unit/test_calibration_r32.cpp` | `e90314cc507880b7373f7c3e24e0d2179ce328e1947e306810d289927c10c62c` |
| `tests/unit/test_conformance.cpp` | `ec1c929815e7bfcc4f008873a9412356844953f3dadf097cb6a799621b7e3107` |
| `tests/unit/test_master.cpp` | `6d2a81dae59178c44f0f6a8ffcbffb6c1342522cc4417e9fd7af4e23be4d26bf` |
| `tests/unit/test_measure.cpp` | `358d003a21b6bd1b560f036bb390657de7c904416cdace9813a789bdb54d0419` |

- Test release (`tmp/release-junit.xml`): totali 1221, fast 1199, slow 22, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 1199, fast 1199, slow 0, falliti 0
<!-- evidence:end -->
