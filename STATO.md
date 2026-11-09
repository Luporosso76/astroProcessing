# STATO astroProcessor (riferito al 9/11/2026)
## Dove siamo
- M0 PASS. M1 DONE. M2 DONE (gate PASS per arbitraggio 30/10, commit `9d3996c`). M3.a DONE (gate PASS per arbitraggio dell'utente, 8/11/2026, con 1211/1212: T-M2R2-09 in timeout, passato da solo in 1659,84 s; `docs/evidence/M3-a.md`).
- M4 (GUI): stop con l'utente prima. `tools/check.sh` a passi dal 9/11.
- Regole nuove del 9/11: API sospese (deleghe solo come Agent sull'abbonamento col modello di `tools/model-routing.tsv`, review Argon; niente `delegate.sh`, `agent-api.sh`, `review.sh --reviewer claude`); componenti con uso commerciale gratuito (`AGENTS.md`, «Uso commerciale»). Argon senza quota dal 9/11 11:48 per circa 119 h (fino al 14/11 verso le 10:40): review come Agent `oh-my-claudecode:code-reviewer` (opus, sola lettura) col prompt `tools/review-template.md` + fatti, artefatto salvato a mano in `.omc/artifacts/ask/review-<slug>-claude-*.md`; tetto di 2 giri tenuto dal coordinatore.

## M3.b (`plans/M3b.md`, 20 unità; decisioni in `docs/decisions/M3b-lacune.md`)
| Unità | Stato |
|---|---|
| B0 decisioni ed emendamenti, B1 protocollo (SHA `83e30030…`), B2 fixture `syn_cal.hpp` | CHIUSE |
| B3 tipi e domini (`e66a112`), B4 κ (`38c7cf1`), B3b pianificatore (`855d634`) | CHIUSE, review PASS |
| B15a stato del decoder, nodo e preflight RAW (`4ff7f4b`) | CHIUSA, review PASS |
| B5 metodi `single`, `weighted_mean`, `median`, `winsorized` | codice in corso (Agent opus distinto, worktree `m3b-b5`, base `4175149`) |
| B6…B14, B15, B16, B17 | da fare, ordine in `plans/M3b.md` §5 |

## Prossimo passo
0. STOP chiesto dall'utente il 9/11 12:33 («appena finisci tutta queste attività fermati»): chiudere B15a e B5 (review, integrazione, push), poi fermarsi; nessuna unità nuova senza il suo via.
1. B5 test e codice (agenti opus distinti); integrare B15a dopo review. Metodo per unità: test rossi → codice (agente diverso) → shard verdi + mutazioni → `tools/review.sh <unità>-roundN` (worktree; non toccare il checkout principale durante la review) → rebase, verifica dell'albero unito, ff, push.
2. Blocchi evidence dei record che citano `master.cpp` e C-02: da rigenerare al prossimo run completo (codice cambiato dopo l'ultimo junit completo).
## Debiti degli strumenti
- `tools/review.sh` fa `cd` alla radice del checkout principale: controllo dello stato di git, istantanea e delta del giro 2 riguardano il principale, non il worktree in revisione (visto il 9/11 su B15a). Correggere quando Argon torna (passare il percorso del worktree), con selftest.

## Non verificato
- `tmp/` 15 GB (u12-build-offline 3,8 GB, ccache 5 GB), `build/` 12 GB: non rimossi.
- Più avanti, non acquisiti: cataloghi M3.p/M3.q da riscegliere (Gaia DR3 è NC, prodotto commerciale: `AGENTS.md`), PUB-DEEP-01 (Zenodo 17843622), emendamenti M3.d/M3.g.
