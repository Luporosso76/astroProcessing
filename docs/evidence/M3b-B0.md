# M3.b, unità B0: emendamenti prima del codice

Stato: **chiusa** il 9 novembre 2026. Solo documenti. Review `M3b-B0-round1` FAIL, `M3b-B0-round2` PASS (Gemini 4 Argon).

| Campo | Valore |
|---|---|
| Unità | M3.b / B0 (solo documenti) |
| Scopo | (1) Estensione ADR-06 R2.r, solo testo: buffer di ordinamento per pixel, indicatori di `sigma_clip`, accumulatori del report per riga e costanti delle scale entrano nel modello di memoria del master. (2) Decisioni del coordinatore sulle lacune L1…L6 e L8…L11; L7 chiusa con conferma letterale |
| ID | nessuno nuovo (C-02.b §b.11, §b.13 punto 5; C-02.b-flat §f.13 punto 11) |
| Comando | nessuno: nessun codice, nessun test, nessuna build |
| Commit | `a3197e5` (decisioni ed emendamenti dei contratti) |
| Versioni | non applicabile (documenti) |
| Ambiente | non applicabile (documenti) |
| Autore e data | Coordinatore. Review: Gemini 4 Argon. 9 novembre 2026 |

## Esito

- Decisioni su L1…L6 e L8…L11 in `docs/decisions/M3b-lacune.md`; L7 chiusa, nessun tetto nuovo a `n` per `sigma_clip` e `weighted_mean`.
- Estensione ADR-06 R2.r («piano del job del master») in `docs/contracts/ADR-06-budget-scheduler.md`; il pianificatore e il suo test sono in B3b.
- Emendamenti collegati nei contratti toccati dal commit: `docs/contracts/C-02-master.md`, `ADR-08-decoder-pack.md`, `BASE-confini.md`, `F-IO-03-classificazione.md`.

## Limiti

- Nessuna prova eseguita: un emendamento è testo, non un test. Le conseguenze sul codice e sui test stanno nelle unità B3b, B12, B14, B15a e B16.
- Rilievi del giro 1 (`.omc/artifacts/ask/review-M3b-B0-round1-argon-20261109-0712.md`): corretti prima del giro 2, che non ne riporta.

## Review

- Giro 1 FAIL, giro 2 PASS (`.omc/artifacts/ask/review-M3b-B0-round2-argon-20261109-0730.md`: «Verdetto: PASS», nessun rilievo).

## Blocco generato

Assente. Hash dei file, conteggi dei junit ed esito di `trace.py check` li scrive `tools/evidence.py` da un file `.evidence`, al run completo del gate (`docs/evidence/M3-b.md`); non si scrivono a mano. Il run completo (`ctest --preset dev-full` e `release`, stesso commit) è del gate: qui valgono solo le verifiche d'unità dichiarate sopra.
