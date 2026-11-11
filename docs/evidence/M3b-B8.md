# M3.b, unità B8: `median_ratio`, parte 1: parametri, costanti, preflight

Stato: **chiusa** l'11 novembre 2026. Review `M3b-B8-round1` FAIL con 2 MED di prova, `M3b-B8-round2` PASS (Claude Opus, contesto separato). Test e codice da agenti distinti. Mutazioni 15/15.

| Campo | Valore |
|---|---|
| Unità | M3.b / B8 (test e codice) |
| Scopo | `m_H,c`, `r_(f,c)` nell'ordine canonico; parametri `flat_additive_bound_adu`, `flat_norm_rel_budget`, `flat_level_rel_tol` (presenza e divieti per metodo); controlli `master.flat_low_signal`, `master.flat_scale_nonfinite`, `max m ≤ 4 min m` esatto per mediana, winsorized, sigma_clip (`master.variance_domain`), `A · ρ_c ≤ budget · m_H,c` esatto (`master.flat_norm_budget`); `median_ratio` ammesso in `validate_master_recipe` |
| ID | T-M3B-FLAT-06 (parte stop); T-M3B-FLAT-01 (costanti dyadic: livelli `(1, 3)` → `m_H = 3/2`, `r = (3/2, 1/2)`); C-02.b-flat §f.1, §f.2, §f.10, §f.13 punti 1-2, 4-5, 8 |
| Comando | `tools/check.sh`; `tools/mutate.sh`. Albero `7e34936`: suite fast 1452 test verdi in 4 shard, release 1491/1491 (fast e slow) (riga di stato del piano) |
| Commit | `7e34936` (codice e test), `d6b699e` (chiusura) |
| Versioni | non verificato; rinvio al record del gate |
| Ambiente | non verificato; rinvio al record del gate |
| Autore e data | Test e codice: agenti Opus distinti. Review: Claude Opus (Agent `code-reviewer`, sola lettura). Coordinatore. 11 novembre 2026 |

## File

Codice: `src/core/master.cpp`, `src/core/include/astro/master.hpp`. Test: `tests/unit/test_master_b8.cpp` (24 test `MasterB8`), `tests/unit/master_support.hpp`, `tests/unit/test_master.cpp`, `tests/unit/test_master_b3.cpp` (attese di T-M2R2-05 su `median_ratio` aggiornate).

## Decisioni di unità

Da `docs/decisions/M3b-lacune.md`, «Decisioni successive (B8)»:

- Controllo 3 in f64 (`hi ≤ 4·lo`, esatto); controllo 4 in aritmetica diadica esatta (`dyadic` su `wide_uint`); `A = -0.0` ammesso.
- Contesto uniforme di `master.flat_scale_nonfinite`: `costante` ∈ {`1/m_f`, `r_f`, `somma`, `m_H`}, più `frame`, `m_f` o `valore`, e `canale` per un CFA.
- `m̄_c` non si calcola con `median_ratio`; i campi del report su di esso sono `null` (deciso in B9).
- Stop temporaneo `master.method_not_computable` per `median_ratio` ammesso ma non ancora calcolato; tolto in B9.
- La chiave del nodo cambia per ogni master, anche mono e senza `median_ratio` (due chiavi `null` in più); `master_algorithm_version` resta 3; i nodi già prodotti diventano `stale` (`plans/M3b.md` §11).

## Limiti

- Nessun calcolo di `median_ratio` in B8 (B9); nessun avviso dedicato oltre `flat_norm_scales_estimated` (decisione di L11).
- Le casistiche ammesse al confine (rapporto 4, `A · ρ_c = budget · m_H,c`) sono in B9.
- LOW residui del giro 2 (`.omc/artifacts/ask/review-M3b-B8-round2-claude-20261110-2355.md`): commento su `m̄_c` (letto anche dal report), commento del campo, contesto di `master.flat_low_signal` per «nessun pixel valido». Corretti dal coordinatore.
- Mutazioni 15/15 (riga di stato); elenco per patch non riportato nelle fonti: non verificato qui.

## Review

- Giro 1 FAIL con 2 MED di prova (`.omc/artifacts/ask/review-M3b-B8-round1-claude-20261110-2307.md`), giro 2 PASS con 3 LOW (sopra).

## Blocco generato

Assente. Hash dei file, conteggi dei junit ed esito di `trace.py check` li scrive `tools/evidence.py` da un file `.evidence`, al run completo del gate (`docs/evidence/M3-b.md`); non si scrivono a mano. Il run completo (`ctest --preset dev-full` e `release`, stesso commit) è del gate: qui valgono solo le verifiche d'unità dichiarate sopra.
