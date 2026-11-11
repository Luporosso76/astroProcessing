# STATO astroProcessor (riferito all'11/11/2026)
## Dove siamo
- M0 PASS. M1 DONE. M2 DONE (gate PASS per arbitraggio 30/10, commit `9d3996c`). M3.a DONE (gate PASS per arbitraggio dell'utente, 8/11/2026, con 1211/1212: T-M2R2-09 in timeout, passato da solo in 1659,84 s; `docs/evidence/M3-a.md`).
- M4 (GUI): stop con l'utente prima. `tools/check.sh` a passi dal 9/11.
- Regole nuove del 9/11: API sospese (deleghe solo come Agent sull'abbonamento col modello di `tools/model-routing.tsv`, review Argon; niente `delegate.sh`, `agent-api.sh`, `review.sh --reviewer claude`); componenti con uso commerciale gratuito (`AGENTS.md`, «Uso commerciale»). Argon senza quota dal 9/11 11:48 per circa 119 h (fino al 14/11 verso le 10:40): review come Agent `oh-my-claudecode:code-reviewer` (opus, sola lettura) col prompt `tools/review-template.md` + fatti, artefatto salvato a mano in `.omc/artifacts/ask/review-<slug>-claude-*.md`; tetto di 2 giri tenuto dal coordinatore.
## M3.b (`plans/M3b.md`, 20 unità; decisioni in `docs/decisions/M3b-lacune.md`)
| Unità | Stato |
|---|---|
| B0 decisioni ed emendamenti, B1 protocollo (SHA `83e30030…`), B2 fixture `syn_cal.hpp` | CHIUSE |
| B3 tipi e domini (`e66a112`), B4 κ (`38c7cf1`), B3b pianificatore (`855d634`), B15a decoder e preflight RAW (`4ff7f4b`) | CHIUSE, review PASS |
| B5 metodi base (`483b671`), B6 `mean` + `sigma_clip` (`b10be73`), B7 livelli del flat per canale CFA (`4431098`), B12 analisi: esclusioni (`68e7793`), B8 (`7e34936`), B9 (`fb16781`), B13 (`db98d12`), B14 (`64fdf8d`), B10 (`d75552f`), B11 report (`13325e1`) | CHIUSE, review PASS (B14 per arbitraggio); release 1561/1561 su B13; fast 1650 e slow 38/38 su B11 |
| B15 CFA da RAW (`ba6b0e8`), B17 dato reale (`d715e42`) | CHIUSE, review PASS (giro 2); fast 1680/1680 e slow 48/48 su `d715e42` |
| B16 job generale (`bd31fc4` in `m3b-b16`, sopra `eef7fcf`) | review PASS (giro 2); verifica finale in corso su `bd31fc4` (fast 6 shard, release con ctest, slow tutti compresi i fuzz RAW-10) |

## Prossimo passo
0. Fino a quattro unità insieme se non toccano gli stessi file (utente, 10/11 21:45). B16: verifica finale verde → ff, riga di stato nel piano, push. In parallelo (worktree): `m3b-gate` (record M3b-B*, matrice, trace, scheletro `M3-b.md`), `tools-debt` (LOW di review.sh e mutate.sh), `m3c-plan` (bozza `plans/M3c.md`, Opus); poi gate M3.b (review del gate con Fable, utente 11/11). Suite fast in 6 shard (5/5 arrivava a 581 s).
1. Metodo per unità: test rossi → codice (agente diverso) → shard verdi + mutazioni → review (Argon; senza quota: Agent code-reviewer opus) → rebase, verifica dell'albero unito (slow compresi se il codice del master cambia), ff, push.
2. Blocchi evidence dei record che citano `master.cpp` e C-02: da rigenerare al prossimo run completo (codice cambiato dopo l'ultimo junit completo).
## Coda pronta (si riempie a ogni avvio di unità; slot libero → si prende da qui, mai «attendo» con la coda non vuota)
- In corso (3/4): verifica finale B16 (`m3b-b16`); LOW residui di B15/B16/B17 (`m3b-low`, sopra `bd31fc4`); bozza `plans/M3c.md` (`m3c-plan`, Opus). Fatti, non committati: documenti del gate in `m3b-gate` (20 record, matrice, trace, scheletro `M3-b.md`). Pronte dopo: review della bozza M3.c e lacune da decidere; run del gate (un solo `ctest --preset dev-full` e `release` con junit, sul commit con B16 + LOW + documenti + strumenti); test rossi delle prime unità di M3.c a piano rivisto; pulizia di `tmp/` e `build/` (27 GB) a run fermi.
## Debiti degli strumenti
- LOW di `tools/review.sh` e `tools/mutate.sh` chiusi l'11/11 (review `tools-debt` giro 1 FAIL con 1 MED, giro 2 PASS; selftest dei due strumenti uscita 0). Residui: 5 LOW in `.omc/artifacts/ask/review-tools-debt-round2-*`; `tools/check.sh:50` esce con 1 dove `mutate.sh --selftest` dichiara 2 (da uniformare).
## Non verificato
- `tmp/` 15 GB (u12-build-offline 3,8 GB, ccache 5 GB), `build/` 12 GB: non rimossi.
- Più avanti, non acquisiti: cataloghi M3.p/M3.q da riscegliere (Gaia DR3 è NC, prodotto commerciale: `AGENTS.md`), PUB-DEEP-01 (Zenodo 17843622), emendamenti M3.d/M3.g.
