# M3.entry, ADR-16.b e D-16: matrice completa della calibrazione e default

Stato: **review PASS al giro 3 di 3** (nessun rilievo aperto). Unità: M3.entry, riga «ADR-16.b, D-16» di `plans/M3.md`. Solo testo: nessun codice, nessun test eseguito.

| Campo | Valore |
|---|---|
| Unità | M3.entry / ADR-16.b, D-16 |
| Scopo | Fissare prima del codice la matrice completa (otto combinazioni, assi dark-flat e flat), il default della calibrazione incompleta, P-LOST e il posto dello stato del decoder nei master costruiti da RAW |
| Comando | `python3 -S tools/trace.py ids --stage M3.entry`; `codegraph_explore` su `master_descriptor_from_node`, `calibration.hpp`, `master.hpp`, `calibration_job.cpp`, `node.hpp`; `python3 -S tools/evidence.py docs/evidence/M3-entry-ADR16b.md` |
| Input | Piano attivo (SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`): §9.1–§9.9 (righe 1200-1326), §10.2 C-01 (righe 1355-1369), F-CAL-02 e F-CAL-03 (righe 1841, 1842), §11.11 riga 1980, testo ADR-16 (righe 2577, 2600), D-16 (riga 2882), T-CAL-08 (righe 2344, 3548), appendice D righe 3420-3422, 3468, 3559-3601, 4065, 4103, 4104, 4111, 4370, 4371, 4381-4397, 4411. `plans/M3.md` (tabella M3.entry e righe M3.a-M3.c). Contratti ADR-16.a con le aggiunte di R2, ADR-03.b §b.2, ADR-08.b §2.3-§2.9, ADR-17.a, C-02.a, ADR-06 (resume) |
| Versioni | Worktree da `cb35d89` |
| Ambiente | Linux 7.0.0-38-generic x86_64, macchina di sviluppo |
| Risultato | Sezione «Estensione ADR-16.b» in `docs/contracts/ADR-16-stati-master.md` (§b.1–b.14); verbale `docs/decisions/D-16-calibrazione-incompleta.md`; 10 ID pianificati `T-M3A-CAL-01…10` in `docs/test-matrix.md`; righe aggiornate in `docs/contracts/README.md`, `docs/decisions/README.md`; limite di ADR-03.b §b.11 aggiornato al rinvio deciso |
| Output | File elencati nel blocco generato sotto; artefatti di review in `.omc/artifacts/ask/` |
| Ambito | Contratto e decisione. Fuori: codice, test, dark scalato (F-CAL-05), scelta fra più master per ruolo, sottrazione del nero del decoder (C-01.b), covarianze (ADR-17.b), master `bias_subtracted` (C-02.b) |
| Esito | Review PASS al giro 3 (Codex `gpt-6-astra`); vedi «Review» |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Decisioni

- Tre fasi: analisi (G-CAL, unica fase che esclude un master), selezione (ricetta esplicita scritta nel nodo), esecuzione (ogni differenza è P-LOST, stop senza sostituzione né omissione).
- Ricetta = catena del light (`none`, `bias`, `dark`, `bias_dark_sub`, `dark_sub_partial`) × catena del flat (`none`, `dark_flat`, `bias_dark_flat_sub`, `bias`, `normalize`, `given_q`); nessuna catena sottrae l'offset due volte.
- `F − B` solo con limite termico e budget dichiarati, controllati con `β_c = bound / (q_min · N_c)`.
- Divisione per `Q` solo con offset del light rimosso da un master, o nel percorso parziale esplicito `known_not_removed` (light RAW, residuo `b_L/Q` propagato).
- Default (D-16): priorità fissa per catena, con i soli fatti verificati e le dichiarazioni presenti; mai percorso parziale della divisione; livelli `info` e `warning`, gli errori sono stop.
- Master da RAW: lo stato del decoder dei frame sta nei parametri del nodo `master.combine` (`frame_decoder_states`) e quindi nel descrittore derivato dal nodo e nelle chiavi. Compatibilità col light: ingressi tutti RAW o tutti non RAW; campi strutturali uguali; nero effettivo (H3) entro `decoder_black_abs_tol_adu` sulle coppie additive. Incompatibilità: esclusione o stop, mai correzione automatica.
- Bound numerici fissati prima della misura: `γ_m` per le catene con al più una sottrazione, bound generale misto assoluto/relativo per le catene con due.

## Limiti

- Nero del decoder usato come sentinella dell'offset, non come misura.
- Limite termico del flat e H-D' sono ipotesi dell'utente.
- Light RAW e master esterni non si combinano.
- `corrected_unnormalized` senza predicato nel registro.
- Ricette con `B` in due catene in attesa del modello di ADR-17.b.
- Priorità del default non confrontata su dati reali.

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-adr16b-round1-20261030-0326.md` | `tmp/adr16b-round1.diff` | FAIL | 6 MED: esclusione globale invece che per catena (bias compatibile con L ma non con F; budget termico che escludeva il flat); fase `calibrated` dei master preparati vietata da ADR-03; `β_c` arrotondato non conservativo; errore zero «dyadic» falso con due sottrazioni; identità di LibRaw non confrontata; P-FLAT-RAW/Q calcolati sull'analisi invece che sulla ricetta selezionata |
| 2 | `.omc/artifacts/ask/codex-adr16b-round2-20261030-0334.md` | `tmp/adr16b-round2.diff` | FAIL | 2 MED: matrice di §b.6 letta con il bias presente in entrambe le catene anche quando è `usable` in una sola; limite termico senza dominio (`β⁺ ≥ 1`) e senza prova contro un oracle della perturbazione |
| 3 | `.omc/artifacts/ask/codex-adr16b-round3-20261030-0336.md` | `tmp/adr16b-round3.diff` | PASS | Rilievi dei giri 1 e 2 chiusi; nessun rilievo nuovo |

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-03-pixel-maschere.md` | `329060121da7e443bd06917a9a8bd2dfa2e1857f8cb5fec97c5580030857150e` |
| `docs/contracts/ADR-16-stati-master.md` | `40f8e14cbbd44a13a217207cff3c56db27355f32fbe4672d89a0d6a9a65d8d9a` |
| `docs/contracts/README.md` | `a6315733e2d302ab8fc1607b3ed81ffbc80430c938bfa2bcab33ab69abf3a0e6` |
| `docs/decisions/D-16-calibrazione-incompleta.md` | `17d99880649e716815e5a57741c71362fee41710535c0e2b99662a392649abb7` |
| `docs/decisions/README.md` | `dd6dfd5ba219f73206b060b3e5d692a851e325f567bc63c7fdcbbedf5863810b` |
| `docs/test-matrix.md` | `2d179f1dece0f5bc4bed7b4e3926e4dd12918e8099e5df0b793cb9da9cbcca80` |

<!-- evidence:end -->
