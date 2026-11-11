# M3.b, unità B2: fixture FIX-SYN-CAL-08

Stato: **chiusa** il 9 novembre 2026. Solo test. Review `M3b-B2-round1` PASS con 1 LOW, corretto.

| Campo | Valore |
|---|---|
| Unità | M3.b / B2 (solo test, senza codice di prodotto) |
| Scopo | Generatore sintetico con verità analitica in aritmetica razionale esatta, oracolo di B5…B9: B, D, F, DF noti; rumore gaussiano di varianza nota, anche eteroschedastico; outlier in posizioni note; flat a livelli diversi per canale con regione di livello e di misura; frame RAW da FIX-SYN-DNG-01 |
| ID | FIX-SYN-CAL-08; generatore del protocollo `docs/evidence/M3b-protocollo.md` §1 |
| Comando | `tools/check.sh` sull'albero dell'unità; `tests/unit/test_syn_cal.cpp`, 13 test (riga di stato del piano). Esito per test: non verificato qui, rinvio al run del gate |
| Commit | `5e413c3` |
| Versioni | non verificato; rinvio al record del gate |
| Ambiente | non verificato; rinvio al record del gate |
| Autore e data | Fixture scritta da chi non scrive l'operatore (Opus, tabella di `plans/M3b.md`). Review: Gemini 4 Argon. Coordinatore. 9 novembre 2026 |

## File

`tests/fixtures/syn_cal.hpp` (usa `tests/fixtures/exact_rational.hpp`), `tests/unit/test_syn_cal.cpp`, `tests/unit/CMakeLists.txt`.

## Limiti

- Mutazioni: non applicabili, l'unità non ha codice di prodotto.
- LOW della review (`weighted_truth_of` non controlla `w_f > 0` prima di dividere per `W`): corretto prima della chiusura (riga di stato del piano).
- Il valore probatorio della fixture si misura nei test che la usano (B5…B9, B11); qui non c'è una misura statistica.

## Review

- Giro 1 PASS (`.omc/artifacts/ask/review-M3b-B2-round1-argon-20261109-0841.md`), un solo LOW.

## Blocco generato

Assente. Hash dei file, conteggi dei junit ed esito di `trace.py check` li scrive `tools/evidence.py` da un file `.evidence`, al run completo del gate (`docs/evidence/M3-b.md`); non si scrivono a mano. Il run completo (`ctest --preset dev-full` e `release`, stesso commit) è del gate: qui valgono solo le verifiche d'unità dichiarate sopra.
