# M1.f: stop di consegna e correzione dei soli permessi

Il primo turno ha prodotto `FIRSTFAIL_PREFLIGHT_STOP`, exit1, zero degli otto metodi eseguiti. Il bootstrap non è partito: permessi0664 dei file violavano il controllo già presente contro scrittura di gruppo/altri. Il record `M1-f-regressions.md` e FIRSTFAIL sono conservati, senza trasformare lo stop in PASS.

Il coordinatore, OpenAI GPT-6.1 Sol dichiarato dal runtime, ha corretto soltanto metadati di file della propria consegna. Review indipendente di `ses_e8c4fd1faffex3f7sPIaEJ0Tce`, modello autodichiarato `openai/gpt-6-astra`, 25 ottobre 2026. Nessuna attestazione del provider o stima dei costi.

## Operazione e verifiche

Allowlist: tredici file del freeze004, freeze004 JSON, approval004 JSON, invocazione fidata004 e il protocollo M1 scritto dal progetto. Diciassette file regolari del proprietario corrente, nlink1, aperti tramite descriptor e NOFOLLOW; tutti i byte e pin controllati prima di rimuovere i bit022 tramite fchmod. Ricontrollati byte, hash, inode, UID/GID, nlink, dimensione e mtime; registrato ctime cambiato. Nessuna patch al codice, gate, approval o protocollo; nessun originale utente modificato.

Precisazione della review: **quindici file0664->0644; freeze e invocazione0600->0600**. Non tutti i diciassette hanno cambiato mode. Tutti soddisfano la guardia. Namespace0700 invariato.

Ricevuta immutabile: `tmp/m1/f-hash/turns/delivery-hardening-004/receipt.json`, SHA-256 `4668735bf8f29d9548779c0fd797c4d952b7d5032bf1e34bf347ca6b7617967c`, mode0400. FIRSTFAIL `e30095885a7462b123cc4b7cacf11cea51d17d93019f77251f883a74a6083422` e approval `d61cd9a8a8c0d77cddef3332a9813ebbbc092e16f59bf942856e8414733fdcc1` preservati.

Il revisore ha letto verbale71, receipt198, byte/metadati dei soli17file, approval e FIRSTFAIL. Verificati hash, regular/UID1000/nlink1, identità FD/entry, assenza bit022, ctime attuali conformi al receipt, namespace e FIRSTFAIL0400. La conservazione storica e l'effettiva syscall restano attestazioni del record del coordinatore, non osservazioni retroattive del revisore.

Esito: READY per un nuovo turno degli stessi otto metodi, entry `future_regressions`, con quota e ammissione separate. La correzione dei permessi non è esecuzione dei test né ammissione di producer, hash empty/abc, worker di contenuto o misure. Il secondo turno conserva un proprio output, senza sovrascrivere il primo.
