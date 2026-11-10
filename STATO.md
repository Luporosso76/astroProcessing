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
| B5 metodi base (`483b671`), B6 `mean` + `sigma_clip` (`b10be73`), B7 livelli del flat per canale CFA (`4431098`) | CHIUSE, review PASS; protocollo v3 SHA `e21760c7…`; release 1374/1374 su B7 |
| B8 `median_ratio` parte 1, B12 analisi: esclusioni | in corso (worktree `m3b-b8`, `m3b-b12`, base `4431098`) |
| B9…B11, B13, B14, B15, B16, B17 | da fare, ordine in `plans/M3b.md` §5 |

## Prossimo passo
0. Fino a quattro unità insieme se non toccano gli stessi file (utente, 10/11 21:45). In corso B8 e B12; poi B9 (dopo B8), B13 (dopo B12), B15 (dopo B12: lettore RAW). Suite fast in 4 shard (3 sfiorano i 600 s col PC carico).
1. Metodo per unità: test rossi → codice (agente diverso) → shard verdi + mutazioni → review (Argon; senza quota: Agent code-reviewer opus) → rebase, verifica dell'albero unito (slow compresi se il codice del master cambia), ff, push.
2. Blocchi evidence dei record che citano `master.cpp` e C-02: da rigenerare al prossimo run completo (codice cambiato dopo l'ultimo junit completo).
## Debiti degli strumenti
- `tools/review.sh` fa `cd` alla radice del checkout principale: controllo dello stato di git, istantanea e delta del giro 2 riguardano il principale, non il worktree in revisione (visto il 9/11 su B15a). Correggere quando Argon torna (passare il percorso del worktree), con selftest. `tools/mutate.sh`: troncamento del junit corretto il 10/11 (review PASS); 7 LOW residui in `.omc/artifacts/ask/review-tools-mutate-round2-*`.

## Non verificato
- `tmp/` 15 GB (u12-build-offline 3,8 GB, ccache 5 GB), `build/` 12 GB: non rimossi.
- Più avanti, non acquisiti: cataloghi M3.p/M3.q da riscegliere (Gaia DR3 è NC, prodotto commerciale: `AGENTS.md`), PUB-DEEP-01 (Zenodo 17843622), emendamenti M3.d/M3.g.
