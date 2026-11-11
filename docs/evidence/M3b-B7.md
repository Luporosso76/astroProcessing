# M3.b, unità B7: livelli e statistiche del flat per canale

Stato: **chiusa** il 10 novembre 2026. Review `M3b-B7-round1` PASS con 6 LOW corretti, `M3b-B7-round2` PASS con 2 LOW residui (Claude Opus, contesto separato). Test e codice da agenti distinti. Mutazioni 11/11.

| Campo | Valore |
|---|---|
| Unità | M3.b / B7 (test e codice) |
| Scopo | `m_(f,c)` per canale (quattro posizioni del motivo, G1 e G2 separati), tolleranza `flat_level_rel_tol` per canale (con `flat_normalization = none`), `level_bias_estimate` per canale (mediana e massimo sui pixel validi del canale); mono invariato; versione dell'algoritmo del nodo. Corregge `master_row_channels` del job (difetto di B3b) |
| ID | T-M3B-C02-14 (livello fuori tolleranza in un solo canale con livello globale entro tolleranza; mono identico a `.a`); T-M3B-MEM-01 (minimo di memoria di un job CFA); C-02.b §b.2 «Canali CFA», §b.6 «Flat», §b.13 punto 4 |
| Comando | `tools/check.sh`; `tools/mutate.sh`. Albero `4431098`: suite fast 1337 test verdi in 3 shard, release 1374/1374 (fast e slow) (riga di stato del piano) |
| Commit | `4431098` (codice e test), `d70aee3` (chiusura) |
| Versioni | non verificato; rinvio al record del gate |
| Ambiente | non verificato; rinvio al record del gate |
| Autore e data | Test e codice: agenti Opus distinti. Review: Claude Opus (Agent `code-reviewer`, sola lettura). Coordinatore. 10 novembre 2026 |

## File

Codice: `src/core/master.cpp`, `src/core/include/astro/master.hpp`, `src/core/fits.cpp`, `src/core/include/astro/fits.hpp`. Test: `tests/unit/test_master_b7.cpp`, `tests/unit/test_master.cpp` (T-M2R2-08 adattato).

## Decisioni di unità

Da `docs/decisions/M3b-lacune.md`, «Decisioni successive (B7)»:

- La tolleranza fuori limite in un canale dà `unsupported`, `master.variance_domain` con `canale` nel contesto; un canale con livello sotto `flat_min_signal_adu` o senza pixel validi in un frame dà `master.flat_low_signal` con `canale`.
- `master_algorithm_version` = 3: cambia anche la chiave dei nodi mono, che diventano `stale` (`plans/M3b.md` §11).
- Un frame con CFA diverso dal frame 0 si ferma con `incompatible`, `master.mismatch.cfa` prima di calcolare i suoi livelli per canale (correzione del giro 1).
- T-M2R2-08: i NaN sono spostati in modo che nessun frame resti senza pixel R validi; caso verificato invariato.
- `master_row_channels` calcolato dopo il preflight: prima valeva sempre 1 e un master CFA prenotava gli accumulatori per riga di un solo canale (`R = 16·Cr·H·n` sottostimato di `16·H·n` byte).

## Limiti

- Lo stato `flat_level` nell'analisi (parte di T-M3B-C02-14) è di B13.
- Nomi del report per canale (`flat_levels`, `channels[c].flat_level_mean`): provvisori in B7, fissati in B11.
- LOW residui del giro 2 (`.omc/artifacts/ask/review-M3b-B7-round2-claude-20261110-2022.md`; due ripresi nelle decisioni): insieme esatto delle chiavi del contesto negli stop `master.flat_low_signal` CFA e in `expect_cfa_mismatch`; helper unico per il confronto del CFA (oggi in `inspect_frame` e `compare_with_reference`); due commenti ambigui.
- Il peek dell'header e il decode nel job restano coperti da `ASTRO_ASSERT`: una modifica del file fra i due la ferma `read_verified_input` con `integrity`, `input.modified` (verifica diagnostica del revisore, test poi tolto).
- Mutazioni 11/11 (riga di stato); elenco per patch non riportato nelle fonti: non verificato qui.

## Review

- Giro 1 PASS con 6 LOW (`.omc/artifacts/ask/review-M3b-B7-round1-claude-20261110-1949.md`), corretti; giro 2 PASS con 4 LOW nell'artefatto (sopra).

## Blocco generato

Assente. Hash dei file, conteggi dei junit ed esito di `trace.py check` li scrive `tools/evidence.py` da un file `.evidence`, al run completo del gate (`docs/evidence/M3-b.md`); non si scrivono a mano. Il run completo (`ctest --preset dev-full` e `release`, stesso commit) è del gate: qui valgono solo le verifiche d'unità dichiarate sopra.
