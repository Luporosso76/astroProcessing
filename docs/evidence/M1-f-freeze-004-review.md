# M1.f: delta 004 verificato e approval limitata

Esito della review indipendente: READY per soli otto metodi di regressione, dopo approval separata e assegnazione del turno. Nessun blocker nel delta. Nessun metodo eseguito o eliminato.

Review di `ses_e8c4fd1faffex3f7sPIaEJ0Tce`, modello autodichiarato `openai/gpt-6-astra`; verbale persistito dal coordinatore il 25 ottobre 2026. Nessuna attestazione del provider; costi/token non esposti.

## Riscontri effettivi

I due finding sono chiusi staticamente: contabilità dei link senza attraversamento e regressione agreement sotto percorso ammesso, con `Failure ERR_RESULT_SCHEMA`, diagnostica generation e assenza di output. `SystemExit` non soddisfa il test. Guardie NOFOLLOW/identità dei candidati preservate.

Verificati 13/13 hash, nove sorgenti invariati, delta di 128 righe, snapshot 30/30 e precedente 18/18 in memoria. Freeze003, report storici, manifest001/002 e protocollo preservati. Undici AST validi, nessun import o test candidato. Lettura mirata: report43, diff128, regressioni104-160, contabilità268-289, guard73-104, manifest e bootstrap16; altre verifiche mediante hash e AST, non nuovo audit completo.

## Approval distinta dal turno

Dopo la review il coordinatore ha ricontrollato i byte di freeze, guard e bootstrap e l'assenza dell'approval. Ha creato esclusivamente `tmp/m1/f-hash/review-approval-004.json`, schema1/status APPROVED, con:

- source manifest: `f880d0c7d23838f068254874bff96d04d81e0aae048e80a9755c1e035e7c8e91`;
- guard: `e52324b8c609feb52f15f2134af80c7b9738f50b3daba2ef9356ef708940a649`;
- allowed_entries: soltanto `future_regressions`.

Digest effettivo dell'approval, calcolato dopo la scrittura dei 269 byte: `d61cd9a8a8c0d77cddef3332a9813ebbbc092e16f59bf942856e8414733fdcc1`. Sarà fornito al turno effettivo come pin esterno. Nessun self-hash o autoaggiornamento del freeze. Questa approval non assegna il turno e non abilita producer, empty/abc, worker di contenuto, vettori grandi o misure.

Le regressioni includeranno piccoli figli propri per verificare il cleanup, non engine di contenuto. Finché un'altra esecuzione è ammessa, non si avviano. Dopo il turno servirà la verifica degli esiti; nessun PASS runtime deriva dalla review statica.
