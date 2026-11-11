# M3.b, unità B4: κ di mediana e winsorized

Stato: **chiusa** il 9 novembre 2026. Review `M3b-B4-round1` PASS senza rilievi (Gemini 4 Argon). Test e codice da agenti distinti. Mutazioni 5/5.

| Campo | Valore |
|---|---|
| Unità | M3.b / B4 (test e codice) |
| Scopo | `κ_T(n, g) = Var(T(Z_1…Z_n))` per mediana e winsorized, `n ≤ 1024`, errore relativo ≤ 10⁻⁹; stima d'errore del proprio calcolo; stop `numeric`, `master.kappa_accuracy` prima di ogni lettura di campioni. Funzione pura calcolata nel preflight |
| ID | T-M3B-C02-05 (classe P); T-M3B-C02-15 (solo `master.kappa_accuracy`); C-02.b §b.6 «Metodi robusti», §b.5; ADR-09.b classe P |
| Comando | `tools/check.sh`; `tools/mutate.sh`. Suite fast: 1256 test verdi in 3 shard sull'albero con B3b (riga di stato di B3b). Esiti per test: non verificato qui, rinvio al gate |
| Commit | `38c7cf1` (codice e test), `5b2a1fb` (chiusura nel piano e in STATO) |
| Versioni | non verificato; rinvio al record del gate |
| Ambiente | non verificato; rinvio al record del gate |
| Autore e data | Test e codice: agenti Opus distinti. Review: Gemini 4 Argon. Coordinatore. 9 novembre 2026 |

## File

Codice: `src/core/master_kappa.cpp`, `src/core/include/astro/master_kappa.hpp`, `src/core/master.cpp`, `src/core/include/astro/master.hpp`, `src/core/CMakeLists.txt`. Test: `tests/unit/test_master_kappa.cpp`, `tests/unit/kappa_oracle.hpp` (oracolo in `long double` con quadratura diversa).

## Verifica

- Ancore di T-M3B-C02-05: `κ_med(2) = 1/2`, `κ_med(3) = 1 − √3/π`, winsorized con `n ∈ {2g+1, 2g+2}` uguale alla mediana; punti espliciti: mediana con `n = 101` e winsorized (101, 2) (decisione di B1).
- Errore relativo massimo e stima dell'errore dell'oracolo (≤ 10⁻¹¹ richiesto prima della misura): valori misurati non riportati nelle fonti consultate, non verificato qui, rinvio al run del gate.

## Limiti

- L'errore dell'oracolo è stimato, non dimostrato (C-02.b §b.14; `plans/M3b.md` §11).
- Uso di κ nel piano di varianza: B5.
- Mutazioni 5/5 (riga di stato); elenco per patch non riportato nelle fonti: non verificato qui.

## Review

- Giro 1 PASS, nessun rilievo (`.omc/artifacts/ask/review-M3b-B4-round1-argon-20261109-1017.md`).

## Blocco generato

Assente. Hash dei file, conteggi dei junit ed esito di `trace.py check` li scrive `tools/evidence.py` da un file `.evidence`, al run completo del gate (`docs/evidence/M3-b.md`); non si scrivono a mano. Il run completo (`ctest --preset dev-full` e `release`, stesso commit) è del gate: qui valgono solo le verifiche d'unità dichiarate sopra.
