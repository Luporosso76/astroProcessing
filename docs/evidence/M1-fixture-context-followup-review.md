# Review del delta lifecycle: 47 test predisposti

**READY STATIC**, non PASS runtime né ammissione di fasi store. Revisore indipendente `reviewer`, sessione `ses_e88edd651ffexUJuuZVSYdl3bo`, modelli e sostituti configurati senza override. Modello autodichiarato `openai/gpt-6-astra`, non attestato; costi non esposti.

## Due rilievi chiusi nel codice

- Closure scientifica sintetica predisposta prima dell'emissione `materialize`, con controllo degli argomenti. `admitted_phase`, lease, sigillo e gate restano reali; non si fabbrica un contesto per aggirare i rifiuti.
- Entrambe le wait hanno timeout sulla stessa deadline iniziata prima di Popen. Child irrisolto produce `STOP/PENDING_UNREAPED`, senza liberare slot o prenotazione, autenticare un log ancora modificabile o recuperare autorità dal solo PID. Nessuna promessa di terminazione OS entro un termine rigido.

Cinque nuovi test double del supervisore portano il totale a **24+18+5=47**, con first-failure. Coprono successo, timeout, child irrisolto, errore di segnalazione e deadline esaurita. Non costituiscono osservazione di processi o D-state reali.

## Copertura indipendente e controllo del coordinatore

Il revisore ha letto integralmente test363 righe, launcher260, receipt60, snapshot receipt49, piano73 e follow-up124. Confronto testuale dei18 corpi lifecycle, senza import, test o ricalcolo degli hash. Non ha riletto lo store v2.

Il coordinatore ha poi eseguito una verifica separata con `/usr/bin/python3 -I -B -S` e soli moduli stdlib: lettura FD NOFOLLOW, file regolari nlink1, identità stabile e SHA-256 su **20 file**. Risultato `PASS_READONLY_HASH_AST`: sei pin preservati, cinque copie snapshot verificate, vecchio report139 righe identico,18 corpi lifecycle AST identici, cinque nuovi metodi supervisore e due wait entrambe con timeout. Verificati anche i pin del piano attivo e del protocollo M1. Nessun import candidato o test eseguito.

Pin del delta recensito:

| File | SHA-256 |
|---|---|
| `test_context_lifecycle.py` | `7d68a0d37d4236d0e1324a134679c03910417bec6d517942f6c04365e14bccb5` |
| `run_suite.py` | `7b0bef90fbe061003cbc3284f13020945193b8262e3eed618944f64d95c0d504` |
| `preparation-receipt.json` | `f5e166cf8b24f6695eb7f21219aab538358f0b09357c79c1242da2b565a62133` |

I tre file sono sotto `tmp/m1/fixture-store/context-lifecycle-tests/`. Distinta ammissione pinned schema2 soltanto dopo chiusura del turno F24, nuovo preflight e prenotazione. Flock/fdinfo, emissione effettiva e cleanup reale restano da qualificare. Nessuna operazione archive, verify, retire, materialize o release sulle168 fixture autorizzata da questa review.
