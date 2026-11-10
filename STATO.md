# STATO astroProcessor (riferito al 10/11/2026)
## Dove siamo
- M0 PASS. M1 DONE. M2 DONE (gate PASS per arbitraggio 30/10, commit `9d3996c`). M3.a DONE (gate PASS per arbitraggio dell'utente, 8/11/2026, con 1211/1212: T-M2R2-09 in timeout, passato da solo in 1659,84 s; `docs/evidence/M3-a.md`).
- M4 (GUI): stop con l'utente prima. `tools/check.sh` a passi dal 9/11.
- Regole nuove del 9/11: API sospese (deleghe solo come Agent sull'abbonamento col modello di `tools/model-routing.tsv`, review Argon; niente `delegate.sh`, `agent-api.sh`, `review.sh --reviewer claude`); componenti con uso commerciale gratuito (`AGENTS.md`, «Uso commerciale»). Argon senza quota dal 9/11 11:48 per circa 119 h (fino al 14/11 verso le 10:40): review come Agent `oh-my-claudecode:code-reviewer` (opus, sola lettura) col prompt `tools/review-template.md` + fatti, artefatto salvato a mano in `.omc/artifacts/ask/review-<slug>-claude-*.md`; tetto di 2 giri tenuto dal coordinatore.

## M3.b (`plans/M3b.md`, 20 unità; decisioni in `docs/decisions/M3b-lacune.md`)
| Unità | Stato |
|---|---|
| B0 decisioni ed emendamenti, B1 protocollo (SHA `83e30030…`), B2 fixture `syn_cal.hpp` | CHIUSE |
| B3 tipi e domini (`e66a112`), B4 κ (`38c7cf1`), B3b pianificatore (`855d634`), B15a decoder e preflight RAW (`4ff7f4b`) | CHIUSE, review PASS |
| B5 metodi `single`, `weighted_mean`, `median`, `winsorized` (`483b671`) | CHIUSA, review PASS; protocollo v3 SHA `e21760c7…` |
| B6 `mean` + `sigma_clip`, bit `support_incomplete` (`b10be73`) | CHIUSA, review PASS; slow 39/39 |
| B7…B14, B15, B16, B17 | da fare, ordine in `plans/M3b.md` §5 |

## Prossimo passo
0. FERMO su richiesta dell'utente (10/11 12:24): B6 chiusa e pubblicata; nessuna unità nuova senza il suo via. Prossima unità quando riparte: B7 (livelli e statistiche del flat per canale, opus).
1. Metodo per unità: test rossi → codice (agente diverso) → shard verdi + mutazioni → review (Argon; senza quota: Agent code-reviewer opus) → rebase, verifica dell'albero unito (slow compresi se il codice del master cambia), ff, push.
2. Blocchi evidence dei record che citano `master.cpp` e C-02: da rigenerare al prossimo run completo (codice cambiato dopo l'ultimo junit completo).
## Debiti degli strumenti
- `tools/review.sh` fa `cd` alla radice del checkout principale: controllo dello stato di git, istantanea e delta del giro 2 riguardano il principale, non il worktree in revisione (visto il 9/11 su B15a). Correggere quando Argon torna (passare il percorso del worktree), con selftest. `tools/mutate.sh` senza `--test-output-size-failed`: il junit perde `[  FAILED  ]` e la mutazione esce con 2 anche se rilevata (B5).

## Non verificato
- `tmp/` 15 GB (u12-build-offline 3,8 GB, ccache 5 GB), `build/` 12 GB: non rimossi.
- Più avanti, non acquisiti: cataloghi M3.p/M3.q da riscegliere (Gaia DR3 è NC, prodotto commerciale: `AGENTS.md`), PUB-DEEP-01 (Zenodo 17843622), emendamenti M3.d/M3.g.
