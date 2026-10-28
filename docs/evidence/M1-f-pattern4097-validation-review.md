# Review dei risultati pattern4097

Esito: **PASS del turno limitato**, non chiusura M1.f. Review indipendente dell'agente `tests`, sessione `ses_e88f11b0affewIIvJMATMUr1fm`, senza override del modello. Modello effettivo e costi non esposti nel verbale ricevuto; nessuna attestazione inventata.

## Controlli eseguiti

- Ricevuta `tmp/m1/f-hash/turns/freeze005-pattern4097-validation-001/receipt.json`, SHA-256 `90b3ce4ffad738a72337af6b51340660b8ecbe21d4022044bbc58966c8796b35`, verificata.
- Report46 righe, preflight, postflight e61 artefatti: binding e hash/metadati controllati. Tredici sorgenti ricontrollate.
- Quattro invocazioni primarie exit0, due worker exit0, nessun retry registrato. Originale generation1: riletti tutti i4097 byte contro `(73*i+19)%256`, zero mismatch.
- Snapshot distinti, size4097, seals15; controllo dei sigilli prima del digest confermato nel sorgente pinnato. Cache key, ricette e contatori per backend coerenti.
- Digest comune `6dea11fd585c26a1d50de04c9fc6c9b3f6918df71bcd53eda777069cd5512243`, classificato `AGREEMENT_ONLY`, con `expected_sha256=null`.

## Rettifica e limiti

Il report primario attribuiva il worker OpenSSL al PID769609. Il raw distingue harness769609 e worker769610: rettifica applicata soltanto al testo, mantenendo i raw immutati.

Il revisore ha verificato separatamente gli originali pattern4097; non ha riletto i398 file storici. Raccolta dei processi attestata dai controller e campionamento, non nuova osservazione OS esaustiva. Nessun candidato rieseguito, nessun benchmark o test di blackout. Hashlib e CLI possono condividere OpenSSL: l'accordo non prova indipendenza crittografica.

Totali confermati: **6/38 ricette, 3/11 agreement**. I24 casi pattern4097 restanti richiedono un turno distinto, con errori attesi preregistrati e arresto al primo esito inatteso. La review non autorizza altri dataset o misure prestazionali.
