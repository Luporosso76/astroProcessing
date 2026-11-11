# M3.b, unità B5: `single`, `weighted_mean`, `median`, `winsorized`

Stato: **chiusa** il 9 novembre 2026. Review `M3b-B5-round1` FAIL con 1 BLOCCANTE, `M3b-B5-round2` PASS (Claude Opus, contesto separato). Test e codice da agenti distinti. Protocollo emendato a v2 e v3.

| Campo | Valore |
|---|---|
| Unità | M3.b / B5 (test e codice) |
| Scopo | I quattro metodi con piano `V` per metodo (`noise_model`, `empirical_weighted`, `gaussian_reference` con `κ` di B4); costanti dei pesi `W`, `a_f`, `T`, `c_f` nel preflight; dominio `2 max w < W` in aritmetica esatta sui f64; ordinamento per pixel su (valore, indice canonico); accumulo f64 e un solo arrotondamento al `sample_type`; `result_nonfinite` su zero o subnormale |
| ID | T-M3B-C02-01 (casi dyadic, pareggi `−0`/`+0`, `n = 101` con `g` ∈ {1, 2, 50}); T-M3B-C02-03 (S); T-M3B-C02-04 (S); T-M3B-C02-07 (caratterizzazione, nessuna soglia); T-M3B-C02-13; T-M3B-C02-15 (solo `master.result_nonfinite` e `master.param.weights`) |
| Comando | `tools/check.sh`; `tools/mutate.sh`. Albero con B15a: suite fast 1293 test verdi in 3 shard, slow statistici 10/10 (riga di stato del piano). Esiti per test e stime: non verificato qui, rinvio al run del gate |
| Commit | `483b671` (codice, test e emendamento del protocollo), `10e630a` (chiusura) |
| Versioni | non verificato; rinvio al record del gate |
| Ambiente | non verificato; rinvio al record del gate |
| Autore e data | Test e codice: agenti Opus distinti (tabella di `plans/M3b.md`). Review: Claude Opus (Agent `code-reviewer`, sola lettura; Argon senza quota). Coordinatore. 9 novembre 2026 |

## File

Codice: `src/core/master.cpp`, `src/core/include/astro/master.hpp`. Test: `tests/unit/test_master_b5.cpp`, `tests/unit/b5_oracle.hpp`, `tests/unit/test_master_b3.cpp`.

## Test statistici (protocollo `docs/evidence/M3b-protocollo.md`, versione 3, SHA-256 `e21760c7f68590d71bccee78088f1577ea0a1bd7e337bc758c3a4aa7f3e1ffc7`)

| Test | Classe | `m` | Livello per statistica | `K` | `P` | Seed |
|---|---|---|---|---|---|---|
| T-M3B-C02-03 | S, controllo di coerenza | 4 | `2.5 · 10⁻⁴` | 2000 | 4096 | 2026110901, 2026110902 |
| T-M3B-C02-04 | S, controllo di coerenza; (b) con regola su `K · P` | 30 | `3.333 · 10⁻⁵` | per `n`, protocollo §3.2 | per `n`, protocollo §3.2 | 2026110911…2026110915 |
| T-M3B-C02-07 | C, deterministico | — | — | — | — | nessuno |

- Esito, valori fuori dall'intervallo della regola I e margini `τ_k`: non riportati nelle fonti consultate, non verificato qui; rinvio al run completo del gate (`docs/evidence/M3-b.md`). I record di misura dei test C02-03 e C02-04 non esistono ancora.
- T-M3B-C02-07 è una caratterizzazione senza soglia: la stima va nel record del gate, nessuna stima è scritta in una fonte.
- Un fallimento di un test S si indaga, non si ripete con altri seed (protocollo §1).

## Decisioni di unità

Da `docs/decisions/M3b-lacune.md`, «Decisioni successive (B5)»:

- Emendamento del protocollo (v2): `τ_k` è l'ampiezza dell'intervallo della regola I e ogni test S ha il criterio binario «ogni valore del prodotto sta nel suo intervallo». Motivo: con la v1 un errore del prodotto allargava `τ_k`, quindi C02-03 e C02-04 non potevano fallire. Misura dell'autore del codice: senza κ nel piano della mediana, T-M3B-C02-04.n3 passava (media di `V̂` 16.0 contro `E` 7.18; valori fuori dall'intervallo 1048576/1048576). Applicato anche in C-02 §b.9 e §f.9.
- Un solo kernel scalare per pixel (`method_band`) per i quattro metodi, senza AVX2; il percorso risolto resta quello del preflight. T-M2R5-01 riesegue su `avx2` i test fast di `test_master_b5`.
- Segno delle costanti dei pesi: `1 − 2 a_f` ≤ 0, `T` ≤ 0 o `c_f` ≤ 0 in f64 → `invalid_input`, `master.param.weights`, prima di ogni lettura.
- Campione subnormale d'ingresso con uscita f64: resta valido; con uscita f32 vale la regola del subnormale.

## Limiti

- Sul percorso `cpu_avx2` i quattro metodi non sono vettorizzati fra pixel; l'identità dei due percorsi è per costruzione (stesso codice) e provata da T-M2R5-01.
- La classe del subnormale con uscita f64 resta aperta nella calibrazione e nel kernel di `mean` (`convert_finite<double>`): non cambiata in B5.
- `κ` e le costanti `a_f`, `c_f` non sono ancora nel report né nei `details` (fatto in B11).
- LOW residui del giro 2 (`.omc/artifacts/ask/review-M3b-B5-round2-claude-20261109-1502.md`): intestazione del protocollo (corretta), «Applicato in» di `τ_k`, classe del subnormale, commento e nome di un test sul segno, cicli su vettori vuoti, stampa «fuori_intervallo 0/0» ambigua (fuori delta).
- Effetto sulle chiavi: `master_algorithm_version` aumentata in B3 (i nodi `.a` sono `stale`); B8 riporta i parametri nuovi nei parametri del nodo «come in B5 e B6». Dettaglio per B5 non nelle fonti consultate: non verificato qui.

## Review

- Giro 1 FAIL con 1 BLOCCANTE (pesi anche come classe), `.omc/artifacts/ask/review-M3b-B5-round1-claude-20261109-1418.md`; giro 2 PASS con 6 LOW e 1 fuori delta (sopra).

## Blocco generato

Assente. Hash dei file, conteggi dei junit ed esito di `trace.py check` li scrive `tools/evidence.py` da un file `.evidence`, al run completo del gate (`docs/evidence/M3-b.md`); non si scrivono a mano. Il run completo (`ctest --preset dev-full` e `release`, stesso commit) è del gate: qui valgono solo le verifiche d'unità dichiarate sopra.
