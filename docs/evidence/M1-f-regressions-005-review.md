# M1.f: review indipendente delle otto regressioni 005

Data: 25 ottobre 2026. Record del coordinatore della review tests, sessione `ses_e8c3e6e74ffeorhNv9IT7nj9CR`, modello autodichiarato OpenAI GPT-6.1 Sol. Costi e token non esposti.

## Esito

PASS delle otto regressioni effettive, con zero failure, errori e skip. Chiuso dinamicamente il precedente rilievo basso sui probe004: nella versione005 il rifiuto è discriminante rispetto al pin alterato, non soltanto all'entry scelta.

Il revisore ha letto report61 righe, receipt171, raw/log/marker, template16, delta osservatore80 e `future_regressions.py:266-313`. Confronto AST: modificato soltanto il metodo dei probe, altri sette metodi invariati. Verificati hash/metadati di 115 artefatti, venti file preflight e 161 preesistenti, oltre ai binding dei tredici sorgenti, protocollo e approval.

| Probe con entry ammessa `future_regressions` | Exit | Stdout | Stderr |
|---|---|---:|---:|
| Solo guard pin errato | 1 | 0 B | 37 B |
| Solo freeze pin errato | 1 | 0 B | 1098 B |
| Solo approval pin errato | 1 | 0 B | 1104 B |

Decodificati i byte e confermate le diagnosi specifiche e l'assenza dei target. Bootstrap e osservatore exit 0, una sola invocazione, nessun retry o unraisable osservato. Summary005 e summary004 hanno contenuto deterministico uguale ma inode distinti: non è riuso del risultato. Il vecchio FIRSTFAIL al preflight e il successivo PASS004 restano preservati.

I cinque rami cleanup mantengono assertion sull'eccezione originale, returncode e successivo `ChildProcessError`; dieci log figli vuoti, nessun adottato o errore di raccolta registrato. Il campionamento contiene quattro PID, non tutti i cinque figli brevi: non prova esaustivamente ogni fork.

## Prossimo turno e limiti

READY soltanto per producer tiny `empty`/`abc` e rispettivi percorsi primari, chunk1, dopo una nuova ammissione esclusiva. La review non avvia automaticamente tali entry. Approval005 e relativi pin restano quelli documentati in `M1-f-freeze-005-review.md`.

Nessun producer, worker di contenuto, OpenSSL CLI o dataset eseguito da questa prova. I 37 campi negativi e quattro profili sono assertion sintetiche, non le 38 ricette o gli undici agreement. Prestazioni e cache lookup restano mancanti. Il revisore non ha importato o rieseguito candidati, scritto file o letto altri namespace attivi; M1.f resta aperta.
