# STATO astroProcessor (riferito all'8/11/2026)
## Dove siamo
- M0 PASS. M1 DONE. M2 DONE (gate PASS per arbitraggio 30/10, commit `9d3996c`). M3.a DONE (gate PASS per arbitraggio dell'utente, 8/11/2026, con 1211/1212: T-M2R2-09 in timeout, passato da solo in 1659,84 s; `docs/evidence/M3-a.md`).
- Lotto dei debiti di M3.a chiuso il 9/11 e pubblicato (commit del lotto su `ba385bc`). M4 (GUI): stop con l'utente prima.
- Worktree: i 54 vecchi rimossi l'8/11 (diff e non tracciati archiviati in `tmp/wt-archive/`), rami `worktree-agent-*` cancellati.

## Lotto debiti M3.a (chiuso 9/11)
| Debito | Chi | Stato |
|---|---|---|
| Esecuzione seriale degli slow pesanti (`T-M2R2-09` 1620-1670 s su 1800 s sotto ASan) + `check.sh` controlla anche `tests/trace/M3a.tsv` | `delegate.sh - infra` | integrato: `-O1`, RUN_SERIAL su 9 test, T-M2R2-09 603 s (da 1660); `docs/decisions/M3-dev-preset.md` |
| Stop termici U7 da CAL-02 a CAL-06 (`test_calibration_selection.cpp:539`) | `delegate.sh U7 arb-tests` | integrato, mutazioni 4/4 |
| Slot `master.combine` di U8b senza test (store, caricamento differito) | `delegate.sh U8b tests` | integrato, 8 test, mutazioni 4/5 (1 equivalente) |
| Fixture `relink_support.hpp`: `make_world` non svuota `<nome>_w`, `write_bytes` riscrive in place (:476); `dng_state_changes.hpp` structured binding a 15 nomi | `delegate.sh U9 arb-tests` (structured binding passato agli accenti) | integrato |
| Accenti ASCII («piu'», «e'») in ~111 file + regola in `tools/lint.sh` | `delegate.sh - mech` (+ structured binding `dng_state_changes.hpp`) | integrato (agente in timeout, patch estratta dal coordinatore) |
| Commento `pack_coverage.cpp:28`, `plans/M3.md:61` libtiff 4.7.2 | coordinatore | integrato |
| `tools/check.sh` oltre 600 s nei worktree con build parallele (anche a `-O1`): gli agenti lo eseguono a pezzi; serve una modalità a passi sotto 600 s | `delegate.sh - infra` | da fare, dopo il lotto |
| Flaky `09a_twin` (`tmp/patches/flaky09a.patch`) | — | applicata (verificato 8/11) |

## Prossimo passo
1. Lotto chiuso: review `debt-M3a-round2` PASS; `dev-full` 1221/1221, `release` 1221/1221, `dev` 1199/1199, trace M2 63/0 e M3.a 8/0 scoperti; 48 blocchi evidence rigenerati.
2. `plans/M3b.md` PLANNED (review `M3b-plan-round1` FAIL, `round2` PASS, 1 LOW corretto): 19 unità B0…B17, lacune L1…L11 (L7 chiusa) da decidere in B0. In corso: `check.sh` a passi (worktree `infra-check-steps`, review `check-steps-round1`). Poi B0 (decisioni in `docs/decisions/`, emendamento ADR-06 R2.r), B1, B2/B3.

## Non verificato
- `tmp/` 15 GB (u12-build-offline 3,8 GB, ccache 5 GB), `build/` 12 GB: non rimossi.
- Più avanti, non acquisiti: Gaia DR3 (M3.p), licenza Tycho-2, PUB-DEEP-01 (Zenodo 17843622), emendamenti M3.d/M3.g.
