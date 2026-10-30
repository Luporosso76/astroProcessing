# M3.a, F-IO-03: contratto della classificazione batch

Stato: **IN ARBITRAGGIO**: review FAIL al giro 2 con 1 MED, già corretto per l'arbitraggio e non rivisto; tetto di 2 giri raggiunto (vedi «Review» e «Punti per l'arbitraggio»). Unità: lacune L2 e L5 di `plans/M3a.md`, prerequisito di U8. Solo testo: nessun codice di prodotto, nessun test scritto o eseguito, nessuna build.

| Campo | Valore |
|---|---|
| Unità | M3.a / contratto di F-IO-03 (lacune L2, L5) |
| Scopo | Fissare fonte e persistenza della classificazione, gruppi di light, scelta fra più candidati dello stesso ruolo, controlli di binning e filtro, ripartizione del mismatch dopo la scelta fra M3.a, M3.b e M3.c |
| Comando | `python3 -S tools/trace.py ids --stage M3.a`; lettura di `calibration_internal.hpp`, `calibration.cpp`, `sqlite_project.cpp`, `sqlite_dag_store.cpp`, `scheduler.hpp`, `calibration_job.cpp`; `tools/review.sh fio03-contract1`, `fio03-contract2`; `python3 -S tools/evidence.py docs/evidence/M3a-F-IO-03-contratto.md` |
| Input | Piano attivo: riga 1814 (F-IO-03), righe 3687-3688, 3719, 4236 (archi). Contratti ADR-16 (.a, .b), C-02.b, C-01.b, ADR-03.b, ADR-08.b, ADR-04, BASE. `plans/M3a.md` (Lacune L2, L5) |
| Versioni | Worktree da `311fb7a` |
| Ambiente | Linux 7.0.0-38-generic x86_64, macchina di sviluppo |
| Risultato | Nuovo `docs/contracts/F-IO-03-classificazione.md`: obblighi CL-01…CL-12 con proprietario unico, test pianificati T-M3A-04 (a-f), 05 (a-d), 06 (a-b); voce in `docs/contracts/README.md` |
| Output | File elencati nel blocco generato sotto; artefatti di review in `.omc/artifacts/ask/`; delta in `tmp/fio03-contract-round2.diff` e `tmp/fio03-contract-arbitrato.diff` |
| Ambito | Classificazione e proposta prima della scelta, verifica della selezione dopo la scelta. Fuori: gruppi di frame e master proposti (C-02.b, M3.b), Advisor (ADR-15), esecuzione `.b` e P-LOST nel preflight (M3.c), F-IO-08, GUI |
| Esito | FAIL al giro 2 (1 MED, corretto senza nuovo giro); in attesa dell'arbitraggio del coordinatore |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Obblighi

| CL | Obbligo | Proprietario |
|---|---|---|
| 01 | Proposta da regole dichiarate (`imagetyp.v1`, `raw_no_type.v1`, `import_stopped.v1`), mai indovinata | `propose_classification` (`batch_classification.cpp`), riconoscimento con `imagetyp_role` |
| 02 | Revisioni append-only in `input_classifications` (schema 1 esteso) | `sqlite_dag_store::append_classifications` |
| 03 | Revisione corrente = numero massimo; la correzione dell'utente prevale e resta nella storia | `sqlite_dag_store::classifications` |
| 04 | Import per file con i job esistenti in `run_branches`, revisione 1 | `import_batch` |
| 05 | Input analizzati o esclusi con motivo; insieme candidato per C-02.b | `analyze_batch` |
| 06 | Candidati per ruolo: master esterni, slot `current`, da M3.b nodi proposti | `analyze_batch` |
| 07 | Gruppi di light esatti, nessuna tolleranza nuova | `group_lights` |
| 08 | Candidato per ruolo: automatico solo se unico idoneo, altrimenti scelta dell'utente | `choose_candidates` |
| 09 | Solo ricette ammissibili, omissioni motivate | `analyze_batch` (chiama `analyze_calibration`) |
| 10 | Controlli dell'accettazione; binning e filtro espliciti, per i RAW solo dichiarati | `pair_compatibility` (U6) |
| 11 | Dopo la scelta: `verify_selection` su stato e identità del decoder, nessuna sostituzione | `verify_selection` (`calibration_analysis.cpp`) |
| 12 | Determinismo e purezza dell'analisi | `analyze_batch` |

Decisioni prese nel contratto:

- `IMAGETYP` è fonte della sola proposta, con origine dichiarata; il ruolo entra in una ricetta solo con la selezione esplicita.
- I RAW sono sempre «da classificare».
- Undo e redo non coprono la classificazione.
- La correzione dell'utente non scavalca `imagetyp_conflict`.
- Con due o più candidati idonei nello stesso ruolo sceglie l'utente; la scelta automatica per punteggio resta all'Advisor.
- Ripartizione di L5: file cambiato → base (OB-11); stato del decoder → `verify_selection` in M3.a, chiamata dal preflight `.b` in M3.c; frame del master → M3.b; effetti e resume → M3.c.

## Punti per l'arbitraggio

- MED del giro 2 (T-M3A-04c senza copie con stesso ruolo e dichiarazioni diverse): corretto con l'aggiunta del caso e del criterio (`tmp/fio03-contract-arbitrato.diff`), senza un terzo giro.
- Rilievo fuori delta del giro 2, aperto: CL-08 ammette un dark-flat «per solo stato» quando non ci sono flat idonei, anche se esiste un flat candidato escluso. ADR-16.b §b.3 confronta invece il dark-flat col flat candidato anche se escluso. Va distinto «flat assente» da «flat presente ed escluso», con un caso in T-M3A-04d, in un'unità successiva o all'arbitraggio.
- Stima di ~550 righe di prodotto per U8: il contratto propone la divisione in U8a e U8b (§13).

## Limiti

- Lo stato del codice è letto al commit `311fb7a`: nessuna build, nessun test eseguito.
- Gli ID T-M3A-04, 05, 06 sono pianificati, non esistono e non sono in `docs/test-matrix.md`.
- Ipotesi H-IT1 (spazi finali di `IMAGETYP`) non provata.

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-fio03-contract1-20261030-1214.md` | primo giro, testo intero | FAIL | 6 MED, 2 LOW: `E` vuoto confondeva assente ed escluso (P-DF-BAD); duplicati dipendenti dall'ordine di registrazione; riproducibilità della proposta con scelte non persistite; dichiarazioni correnti nel preflight invece di quelle congelate; prova di purezza aggirabile dal WAL; stato dopo l'esecuzione in T-M3A-06b incompatibile con generazioni e pubblicazione; registrazione chiamata pura; rinvio §13 |
| 2 | `.omc/artifacts/ask/codex-fio03-contract2-20261030-1218.md` | `tmp/fio03-contract-round2.diff` | FAIL | 1 MED: copie con stesso ruolo e dichiarazioni diverse non coperte da T-M3A-04c. I rilievi del giro 1 risultano corretti. 1 fuori delta (dark-flat con flat escluso) |

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/F-IO-03-classificazione.md` | `3c606dc71a2afa641d2c468f507d8487335b48efbbdef3231352736d2c7c419e` |
| `docs/contracts/README.md` | `353e714178f05198679290fc445edac14ca3aa881f4f1d4e91c44d7c8df00b7c` |

<!-- evidence:end -->

## Arbitraggio del coordinatore (dopo il giro 2)

Giro 2 FAIL con 1 MED; tetto di 2 giri raggiunto. La correzione dell'autore dopo il giro (T-M3A-04c: copie con lo stesso digest, stesso ruolo e dichiarazioni diverse → `duplicate_digest_conflict`) è letta dal coordinatore e accettata senza un terzo giro. Rilievo fuori delta del giro 2 corretto dal coordinatore: in CL-08 il dark-flat si confronta coi flat candidati anche se esclusi (ADR-16.b §b.3), e solo senza alcun flat candidato basta il suo stato; caso aggiunto in T-M3A-04d. U8 si divide in U8a e U8b (`plans/M3a.md`).
