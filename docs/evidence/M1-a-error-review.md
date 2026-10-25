# M1.a: review statica del candidato error-contract v3

- Unità: M1.a, contratti d'errore e supervisione dei processi.
- Revisore: security `ses_e8c4fd1faffex3f7sPIaEJ0Tce`, modello autodichiarato `openai/gpt-6-astra`; esito persistito dal coordinatore il 25 ottobre 2026.
- Operazione: lettura dei delta effettivi dei quattro sorgenti candidati e lettura integrale di audit, validazione, wrapper, controllo statico, handoff, ricevuta e report: 1387 righe. Nessuna importazione, compilazione o esecuzione di worker.
- Versione di ingresso: preparazione descritta in `M1-a-error-contract.md`, SHA-256 `87f4caa029dd7f0c56b752d889e2937763a04326f8c7536d901fe565bd5d3acc`; namespace `tmp/m1/a-fault-contract-v3/` prima della successiva correzione.
- Integrità verificata dal revisore: 8/8 hash candidati e 7/7 originali conformi all'handoff; quattro sorgenti originali conformi al manifest congelato della serie002.
- Esito: **PARZIALE, prima validazione non ancora ammessa senza correzioni**. Nessun test dinamico superato in questo passaggio.

## Riscontri

1. Controlli NaN/Inf su entrambe le componenti dello spettro inverso: chiusi staticamente prima delle FFT e dell'apertura dell'output.
2. Dispatch: eventi di lancio e richieste osservati dal supervisore, non più costanti dichiarate dai worker. Copertura limitata a questi processi; nessuna prova degli automatismi interni di FFTW o NumPy.
3. Parser C++ ancora incompleto: un token `stride` concatenato al percorso può essere spezzato dall'estrazione formattata in due campi. Occorrono esattamente otto token e conversioni con consumo completo, con regressione dedicata.
4. Supervisore: `select` seguito da `readline` può bloccarsi su una riga parziale senza newline. Occorrono letture non bloccanti, buffer limitato, deadline e controllo RSS attivi, oltre al cleanup del figlio se l'handshake fallisce.

I conteggi delle future prove restano pianificati. I risultati numerici della serie002 non sono riscritti né attribuiti al nuovo candidato. La correzione successiva conserva una ricevuta separata; l'esecuzione richiede ammissione del coordinatore. Costi non disponibili; nessuna misura o certificazione scientifica attribuita a questa review.
