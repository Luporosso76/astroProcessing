# M3.b, unità B3b: pianificatore di memoria del master

Stato: **chiusa** il 9 novembre 2026. Review `M3b-B3b-round1` PASS con 2 LOW, corretti (Gemini 4 Argon). Test e codice da agenti distinti. Mutazioni 5/5.

| Campo | Valore |
|---|---|
| Unità | M3.b / B3b (test e codice) |
| Scopo | Il pianificatore del master conta i buffer nuovi (ordinamento per pixel, indicatori di `sigma_clip`, accumulatori del report per riga, costanti delle scale) per ogni metodo; per i frame RAW (L10, opzione C) la quota di spill `Z = n·S·6`, non memoria del pool, e stop prima di ogni lettura se lo spill non è dichiarato o la quota non basta |
| ID | T-M3B-MEM-01 (piano non rappresentabile, minimo − 1 → stop prima di ogni prenotazione); T-M2R2R-01…05 rieseguiti; C-02.b §b.11, §b.13 punto 5; C-02.b-flat §f.13 punto 11; ADR-06 R2.r (estensione di B0) |
| Comando | `tools/check.sh`; `tools/mutate.sh`. Suite fast: 1256 test verdi in 3 shard sull'albero con B4. Esiti per test: non verificato qui, rinvio al gate |
| Commit | `855d634` (codice e test nello stesso commit, come chiede il piano), `4175149` (chiusura) |
| Versioni | non verificato; rinvio al record del gate |
| Ambiente | non verificato; rinvio al record del gate |
| Autore e data | Test e codice: agenti Sonnet distinti. Review: Gemini 4 Argon. Coordinatore. 9 novembre 2026 |

## File

Codice: `src/core/resources.cpp`, `src/core/include/astro/resources.hpp`, `src/core/master.cpp`. Test: `tests/unit/test_resources.cpp`, `tests/unit/test_master.cpp`.

## Limiti

- Difetto del pianificatore trovato dopo la chiusura e corretto in B7: `master_row_channels` del job valeva sempre 1 prima che il preflight impostasse `per_channel`, quindi un master CFA sottostimava `R = 16·Cr·H·n` di `16·H·n` byte (`docs/decisions/M3b-lacune.md`, «Decisioni successive (B7)»).
- La lettura dei frame RAW nel job non è in B3b: è in B16.
- Mutazioni 5/5 (riga di stato); elenco per patch non riportato nelle fonti: non verificato qui.

## Review

- Giro 1 PASS (`.omc/artifacts/ask/review-M3b-B3b-round1-argon-20261109-1027.md`). LOW: commento di `plan_resources` ancora «nessuno spill» per il modello del master; guardia `!config.scratch` in `plan_master` dopo gli stop di memoria e senza asserzione dedicata. Corretti.

## Blocco generato

Assente. Hash dei file, conteggi dei junit ed esito di `trace.py check` li scrive `tools/evidence.py` da un file `.evidence`, al run completo del gate (`docs/evidence/M3-b.md`); non si scrivono a mano. Il run completo (`ctest --preset dev-full` e `release`, stesso commit) è del gate: qui valgono solo le verifiche d'unità dichiarate sopra.
