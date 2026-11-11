# M3.b, unità B6: `mean` + `sigma_clip` e bit `support_incomplete`

Stato: **chiusa** il 10 novembre 2026. Review `M3b-B6-round1` FAIL con 1 BLOCCANTE, `M3b-B6-round2` PASS (Claude Opus, contesto separato). Test e codice da agenti distinti. Mutazioni 7/7.

| Campo | Valore |
|---|---|
| Unità | M3.b / B6 (test e codice) |
| Scopo | Procedura per pixel con soglie strette; `clip_min_kept` → bit `support_incomplete`, campione e varianza NaN; `master.param.sigma_clip_inert` con confronto esatto sui f64 di `(n − 1)²` contro `κ² n`; piano `frozen_selection`; capability `variance.fixed_weight_approximation`; avviso `frozen_selection_approximation`. Porta nel codice il bit 9 `support_incomplete` di ADR-03.b §b.6 (`reserved_mask = 0xFC00`) |
| ID | T-M3B-C02-01 (parte sigma); T-M3B-C02-02 (appartenenza esatta, casi inerti `n = 10, κ = 3` e `n = 4, κ = 1.5`, `n = 11, κ = 3` ammesso); T-M3B-C02-06 (caratterizzazione); T-M3B-C02-15 (pixel `support_incomplete` senza cambio di metodo); T-M3A-TYP-08 (anticipato da M3.d a M3.b) |
| Comando | `tools/check.sh`; `tools/mutate.sh`. Albero `b10be73`: suite fast 1322 test verdi in 3 shard, slow 39/39, build release compilata (riga di stato del piano). Esiti per test e stime: non verificato qui, rinvio al gate |
| Commit | `b10be73` (codice e test), `d6c0a8f` (chiusura) |
| Versioni | non verificato; rinvio al record del gate |
| Ambiente | non verificato; rinvio al record del gate |
| Autore e data | Test e codice: agenti Opus distinti. Review: Claude Opus (Agent `code-reviewer`, sola lettura). Coordinatore. 10 novembre 2026 |

## File

Codice: `src/core/master.cpp`, `src/core/include/astro/image.hpp`, `src/core/calibration_blocks.cpp`, `src/core/calibration_internal.hpp`, `src/core/calibration_report.cpp`. Test: `tests/unit/test_master_b6.cpp`, `tests/unit/b6_oracle.hpp`, `tests/unit/test_master_b3.cpp`, `tests/unit/test_blocks.cpp`, `tests/unit/test_calibration_job.cpp`, `tests/unit/test_fits.cpp`, `tests/unit/test_image.cpp`.

## Test statistici (protocollo v3, SHA-256 `e21760c7f68590d71bccee78088f1577ea0a1bd7e337bc758c3a4aa7f3e1ffc7`)

| Test | Classe | `K` | `P` | Seed |
|---|---|---|---|---|
| T-M3B-C02-06 | C, caratterizzazione | 2000 | 4096 | 2026110921…2026110923 |

- Stime di C02-06: non scritte in alcuna fonte consultata; rinvio al record del gate (decisione di B6, «Correzioni della review di B6 (giro 1)»: i record di C02-01, 02 e 06 con i commit dei parametri stanno nel record del gate, dal run completo). Nessuna soglia.
- Parametri di `sigma_clip` di C02-01 e C02-02: nel codice del test (`c02_01_clip`, `c02_02_configs` in `tests/unit/test_master_b6.cpp`), fissati prima di ogni misura del prodotto; unico cambio: `clip_min_kept` della configurazione E da `n − 3` a `⌈2n/3⌉`.

## Decisioni di unità

Da `docs/decisions/M3b-lacune.md`, «Decisioni successive (B6)»:

- Il bit 9 e `reserved_mask = 0xFC00` entrano nel codice (non è un confine rifatto: regola già normativa); FITS legge il bit 9, i bit 10–15 restano `fits.invalid_reserved_bits`. T-M3A-TYP-08 anticipato in `docs/test-matrix.md`.
- `rejected_samples` comprende gli `R` dell'ultimo passo che porta `|S|` sotto `clip_min_kept`; i pixel `result_nonfinite` non contribuiscono. Lo stop `sigma_clip_inert` sta in `validate_master_recipe`, prima di ogni lettura.
- `clip_max_iter` senza tetto nel contratto: il codice usa `min(clip_max_iter, n)`, calcolato in f64 prima della conversione.

## Limiti

- L'insieme rifiutato per pixel non è osservabile direttamente: T-M3B-C02-02 lo verifica tramite `M`, `V`, i bit e i conteggi per frame. Nessun canale di diagnosi nuovo nel prodotto.
- I casi con intervalli sovrapposti escono prima della misura e sono contati (C-02.b §b.6; protocollo §1); il loro numero va nel record del gate.
- `pixels_with_rejection` per canale è di B11.
- LOW residui del giro 2 (`.omc/artifacts/ask/review-M3b-B6-round2-claude-20261110-0921.md`): riferimenti di riga per `winsor_g`; ramo `result_nonfinite` da `kept_stats` senza mutazione documentata; `bit_names` e `reserved_mask` non legati da `static_assert`.
- Effetto sulle chiavi: B8 riporta che i parametri nuovi entrano nei parametri del nodo «come in B5 e B6» (`docs/decisions/M3b-lacune.md`, «Decisioni successive (B8)»); i nodi precedenti diventano `stale`. Il dettaglio per B6 non è nelle fonti consultate: non verificato qui.

## Review

- Giro 1 FAIL con 1 BLOCCANTE (`clip_max_iter`, classe cercata), `.omc/artifacts/ask/review-M3b-B6-round1-claude-20261110-0846.md`; giro 2 PASS con 3 LOW (sopra).

## Blocco generato

Assente. Hash dei file, conteggi dei junit ed esito di `trace.py check` li scrive `tools/evidence.py` da un file `.evidence`, al run completo del gate (`docs/evidence/M3-b.md`); non si scrivono a mano. Il run completo (`ctest --preset dev-full` e `release`, stesso commit) è del gate: qui valgono solo le verifiche d'unità dichiarate sopra.
