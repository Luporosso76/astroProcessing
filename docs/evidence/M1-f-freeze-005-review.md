# M1.f: review indipendente del freeze 005

Data: 25 ottobre 2026. Record del coordinatore; review in sola lettura dello specialista security, sessione `ses_e8c4fd1faffex3f7sPIaEJ0Tce`, modello autodichiarato `openai/gpt-6-astra`. Nessuna attestazione indipendente del provider o dato di costo disponibile.

## Esito e copertura

READY statico per una nuova esecuzione delle otto regressioni, dopo ammissione del turno. Non è il loro PASS dinamico.

- Verificati diff di 93 righe, tredici hash dei sorgenti e sedici membri dello snapshot precedente. Dieci sorgenti sono identici alla versione 004.
- I tre probe di pin usano ora un'entry consentita e richiedono la diagnosi specifica di guard, freeze o approval errato. Il limite discriminante annotato nella review del turno 004 è corretto nel codice, non ancora qualificato mediante una nuova esecuzione.
- Approval 004 e relativi risultati restano storici immutabili. Non sono stati ricontrollati tutti i 139 artefatti della mappa storica.
- Il modulo `securefs` verificato è importato prima del controllo del protocollo; le entry operative sono caricate dopo. Non si dichiara che nessun modulo locale venga importato prima del controllo.

## Approval separata del coordinatore

Il coordinatore ha creato `tmp/m1/f-hash/review-approval-005.json`, schema 1, stato `APPROVED`, con questi binding:

| Oggetto | SHA-256 |
|---|---|
| Freeze 005 | `6db59077e7bdcdcec612b3e51b14ca963a86b8add66e5d3f5a0d760ea8d8e94a` |
| Guard | `ffd274d0342c42fcc2c2a08f98088a3eea7f4398bf507af08f25ed64c3a6615a` |
| Approval, 327 byte | `d85df7bb7ad27312d191d20b64b45950ac5ecbadda0c99a0c1c152a9ebde443c` |
| Template di invocazione | `3e2d0df8b0000e220f5a78488a80471a9595d7ec0641f171c93b54535e920209` |

L'approval consente le entry `future_regressions`, `prepare_inputs`, `harness`, `worker`, `compare_results`. Il file è regolare, di proprietà dell'utente corrente, con un solo hardlink e mode `0600`; contenuto e hash verificati dopo `fsync`. La creazione dell'approval non ammette automaticamente tutte le ricette o esecuzioni.

Il prossimo turno ammesso può eseguire soltanto le otto regressioni aggiornate. Producer `empty`/`abc`, digest di contenuto, 38 ricette, 11 confronti, serie temporali e cache lookup restano distinti e non eseguiti. Nessuna esecuzione hash è documentata da questo record.
