# M3.b, unità B9: `median_ratio`, parte 2: valori e piano

Stato: **chiusa** l'11 novembre 2026. Review `M3b-B9-round1` FAIL con 1 MED di prova, `M3b-B9-round2` PASS (Claude Opus, contesto separato). Test e codice da agenti Opus distinti. Mutazioni 11/11.

| Campo | Valore |
|---|---|
| Unità | M3.b / B9 (test e codice) |
| Scopo | `y_f = offset_adu + r_f (x_f − offset_adu)` fuso nel ciclo per pixel; metodi di B5 e B6 sugli `y_f`; piano che stima `Var(M | r)`; invalidità e saturazione sugli `x_f`; capability `variance.fixed_weight_approximation` per ogni metodo con `median_ratio`; tolto lo stop temporaneo `master.method_not_computable` |
| ID | T-M3B-FLAT-06 (confini ammessi: rapporto 4, `A · ρ_c = budget · m_H,c`; prodotto subnormale → `master.result_nonfinite`); T-M3B-FLAT-01; T-M3B-FLAT-02 (fattore globale senza rumore: (a) `mean`, β qualunque, rapporto fino a 16; (b) ogni metodo con β ≡ 0, rapporto fino a 4); T-M3B-FLAT-03 (S); T-M3B-FLAT-04 (S); T-M3B-FLAT-05 (caratterizzazione); C-02.b-flat §f.1 (fatti 1-4), §f.6, §f.7 |
| Comando | `tools/check.sh`; `tools/mutate.sh`. Albero `db98d12` (con B13): suite fast 1514 test verdi in 4 shard, release 1561/1561 (fast e slow) (riga di stato del piano). Esiti per test e stime: non verificato qui, rinvio al gate |
| Commit | `fb16781` (codice e test), `3925bf0` (chiusura, con B13, B14 e B10) |
| Versioni | non verificato; rinvio al record del gate |
| Ambiente | non verificato; rinvio al record del gate |
| Autore e data | Test e codice: agenti Opus distinti. Review: Claude Opus (Agent `code-reviewer`, sola lettura). Coordinatore. 11 novembre 2026 |

## File

Codice: `src/core/master.cpp`, `src/core/include/astro/master.hpp`. Test: `tests/unit/test_master_b9.cpp`, `tests/unit/b9_oracle.hpp`, `tests/unit/test_master_b8.cpp` (test sostituito: `T_M3B_FLAT_06_admitted_median_ratio_is_combined_in_memory_and_in_the_job`).

## Test statistici (protocollo v3, SHA-256 `e21760c7f68590d71bccee78088f1577ea0a1bd7e337bc758c3a4aa7f3e1ffc7`)

| Test | Classe | `m` | Livello per statistica | `K` | `P` | Seed |
|---|---|---|---|---|---|---|
| T-M3B-FLAT-03 | S, controllo di coerenza | 18 | `5.556 · 10⁻⁵` | 4000 | 1365 per classe | 2026110931 |
| T-M3B-FLAT-04 | S, controllo di coerenza | 8 | `1.25 · 10⁻⁴` | 2000 | 682 o 1364 | 2026110941…2026110944 |
| T-M3B-FLAT-05 | C, caratterizzazione | — | — | 256 | 1023 per classe | formula, protocollo §3.6 |

- Esito, stime, errore Monte Carlo e limiti del protocollo §1 di FLAT-03, 04, 05: non scritti in alcuna fonte consultata. I test li stampano sullo stdout di ctest (decisione di B9, giro 1): vanno nel record del gate dal run slow del gate.
- FLAT-05 è una caratterizzazione senza soglia: nessuna stima inventata.

## Decisioni di unità

Da `docs/decisions/M3b-lacune.md`, «Decisioni successive (B9)»:

- Con `median_ratio` `flat_level_rel_dev`, `flat_levels[].rel_dev`, `flat_level_mean` e `channels[c].flat_level_mean` valgono `null`; `level_bias_estimate` è `null` con motivo `normalized`; `pixels_nonpositive_signal` resta.
- Valori normalizzati nel buffer per pixel già esistente (`sort_slot`): il modello di memoria non cambia. `mean` con `median_ratio` passa dal percorso per pixel (`method_band`); `mean` con `none` resta sui kernel scalar/avx2.
- Pixel `result_nonfinite` se `x − offset` non è finito, se il prodotto è zero, subnormale o non finito con `x ≠ offset`, o se `y` non è finito.
- Oracle di FLAT-02(b) con `sigma_clip`: limite su ogni sottoinsieme tenuto; nei casi diadici la tolleranza 0 presuppone costanti f64 uguali ai razionali esatti, e il test lo asserisce per canale.
- `master_algorithm_version` sale a 4 in B11: gli artefatti `median_ratio` prodotti fra B9 e B11 diventano `stale`.

## Limiti

- L'ipotesi `gaussian_reference_heteroscedastic` e il suo avviso sono di B11.
- Con `sigma_clip` indeciso il limite inferiore di `pixels_nonpositive_signal` può valere 0: la prova in quel canale è il limite superiore (mutazione specifica di `sigma_clip` scoperta, dichiarato).
- LOW residui del giro 2 (`.omc/artifacts/ask/review-M3b-B9-round2-claude-20261111-0159.md`; ripresi in B10): commento storico; esiti `support_incomplete` ammessi contati come verificati senza tetto; `skipped_in[1] == 0` che passa a vuoto con regione di misura vuota; somma dei contatori tautologica.
- Mutazioni 11/11 (riga di stato); elenco per patch non riportato nelle fonti: non verificato qui.

## Review

- Giro 1 FAIL con 1 MED di prova (`.omc/artifacts/ask/review-M3b-B9-round1-claude-20261111-0133.md`), giro 2 PASS con 5 LOW (sopra).

## Blocco generato

Assente. Hash dei file, conteggi dei junit ed esito di `trace.py check` li scrive `tools/evidence.py` da un file `.evidence`, al run completo del gate (`docs/evidence/M3-b.md`); non si scrivono a mano. Il run completo (`ctest --preset dev-full` e `release`, stesso commit) è del gate: qui valgono solo le verifiche d'unità dichiarate sopra.
