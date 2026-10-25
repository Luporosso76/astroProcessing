# M1.d: review statica del primo scheduler bounded

- Unità: M1.d, pool, backpressure e spill.
- Revisore: tests `ses_e8c3e6e74ffeorhNv9IT7nj9CR`, modello autodichiarato OpenAI GPT-6.1 Sol. Esito persistito dal coordinatore il 25 ottobre 2026.
- Operazioni eseguite: lettura integrale di `tmp/m1/d/pool.py` (516 righe), `run.py` (579) e `README.md` (166), protocollo righe 113-122; parsing AST, conteggi, hash e piccole derivazioni numeriche. Nessuna importazione dei moduli, worker, test o benchmark.
- Protocollo: `plans/M1-spikes.md`, SHA-256 `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.
- Hash individuali dei tre sorgenti: non riportati nel verbale ricevuto; la successiva correzione deve conservare lo snapshot precedente e identificarlo.
- Esito: **FAIL della preparazione statica per la prima validazione**, tre correzioni richieste. Non è un fallimento di una prova dinamica mai eseguita.

## Rilievi

1. **Alto:** il preflight confronta l'hash del protocollo con il piano scientifico. Verificare il file corretto; l'eventuale verifica del piano richiede un hash distinto.
2. **Alto:** un errore worker conservato con traceback può trattenere i tre bytearray dopo la restituzione della prenotazione. Eliminare i riferimenti proprietari dei payload prima di rendere la quota riutilizzabile; aggiungere una verifica dinamica della loro vita effettiva.
3. **Medio:** scrivere e rileggere 256 MiB non dimostra altrettanta allocazione fisica. Verificare `st_blocks * 512` prima di pubblicare lo spill completato; insufficienza produce stop, non modifica di dati, filesystem o backend.

## Riscontri e limiti

Prenotazioni bounded derivate: 10555906, 16851970 e 29444098 byte per 1/2/4 worker, entro 32 MiB. Con quattro worker e halo da 1 MiB la richiesta di 37832706 byte deve essere rifiutata senza ridimensionamento. Coda bounded, ammissione atomica e controllo della generazione risultano coerenti staticamente.

Sono predisposti 21 casi validate, 15 con errore atteso; gli stream bounded elaborano 32 MiB per configurazione. I tre casi full richiedono 40 GiB ciascuno e non sono stati eseguiti. Nessuna serie formale o bootstrap è ancora implementato. Nessun limite RSS universale, prova astronomica o PASS empirico. Le correzioni procedono nel solo namespace assegnato, senza bloccare i rami indipendenti.
