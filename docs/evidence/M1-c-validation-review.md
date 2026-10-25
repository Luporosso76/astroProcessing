# M1.c: review indipendente della prima validazione

Data: 25 ottobre 2026. **PASS della coerenza delle evidenze di validazione**, non delle prestazioni né della decisione SQLite/JSON. Il coordinatore registra il risultato del revisore; non gli attribuisce una nuova esecuzione della suite.

Revisore: agente `tests`, sessione `ses_e8c3e6e74ffeorhNv9IT7nj9CR`, modello dichiarato `openai/gpt-6.1-sol`. Nessuna attestazione indipendente del provider; costi e token non esposti. Sola lettura di sorgenti, ricevute, manifest e risultati. Nessuna importazione del modulo sotto prova, esecuzione di worker, scrittura, rete o operazione Git.

## Materiali e controlli

- `M1-c-validation.md`: esecuzione originale e limiti; candidato già sottoposto alla review statica `M1-c-static-followup.md`.
- `tmp/m1-persistence/paired-series-v2-01/`: confrontati `cases.json`, `cases.jsonl`, recovery, manifest, stdout/stderr e ricevuta dell'auditor. I nomi e gli hash completi sono nel record dell'esecuzione.
- Verificati gli hash dei cinque artefatti, dei sorgenti e delle ricevute confrontati dal revisore. La continuità degli altri 127 artefatti è riportata dalla mappa del manutentore: il revisore **non li ha tutti ricalcolati**.

La verifica indipendente ha ricalcolato conteggi, identificativi univoci, codici di uscita e concordanza fra rappresentazioni JSON/JSONL. Non ha rilanciato i comandi di validazione né misurato tempi.

## Riscontri

- **44/44 casi SQLite e 46/46 casi JSON**, tutti presenti e superati; conservati i 38 casi originari per backend.
- **18 crash controllati per backend**, 36 totali, con `SIGKILL` esclusivamente sui figli del supervisore e handshake del punto di arresto.
- **164 processi** registrati: 74 con uscita `0`, 36 con `-9`, 48 con `2` e 6 con `3`; gli ultimi corrispondono ai conflitti previsti, non a successi nascosti.
- I mutanti che omettono una sincronizzazione hanno prodotto `DURABILITY_ORDER`, uscita `2` e nessun ACK: due casi SQLite e quattro JSON.
- Originali sintetici in sola lettura: **90 identità conservate**. Commit corrente corrotto e schema sconosciuto non attivano caricamento di una generazione o di un backend alternativo.
- Le tre correzioni del candidato scheduler sono state verificate **staticamente** nello stesso passaggio, non eseguite: il relativo ramo resta distinto da questo PASS.

## Limiti e seguito

Un `SIGKILL` non dimostra resistenza a un blackout. Le sincronizzazioni interne di SQLite non sono osservate dall'instrumentazione Python. I test di permessi e spazio esaurito sono simulazioni dichiarate; non dimostrano ogni configurazione del filesystem o un ambiente ostile con lo stesso UID. I 46,6 secondi del comando originale non sono un benchmark comparativo.

Restano necessari le misure M1.c preregistrate, la review dei relativi campioni e il confronto delle conseguenze architetturali. Nessun backend di progetto è adottato con questo record.
