# M1.c: verifica statica delle quattro correzioni

- Unità: M1.c, preparazione alla prima validazione controllata.
- Autore della review: agente security `ses_e8c4fd1faffex3f7sPIaEJ0Tce`, modello autodichiarato `openai/gpt-6-astra`; record persistito dal coordinatore il 25 ottobre 2026.
- Operazione eseguita: lettura integrale di `tmp/m1-persistence/persist.py` (562 righe), `run.py` (1115 righe) e `README.md` (239 righe), con confronto rispetto ai quattro rilievi in `M1-c-review.md`. Nessuna importazione o esecuzione del programma.
- Input SHA-256: `persist.py` = `aa1be7debaf126680e1b9b34c986f61e8dfbf450a6746b7d3b3bcb0afe127e67`; `run.py` = `f54caff9c25685502ea035045a9cc91fe1735c32bc7a14bfe6c75f5a13f8fcb2`.
- Ambiente: review dei sorgenti locali; nessuna misura di filesystem o prestazioni eseguita dal revisore in questo passaggio.
- Risultato: quattro rilievi chiusi **staticamente**. Il candidato è ammesso alla prima validazione controllata, non alla dichiarazione di superamento del gate M1.c.

## Riscontri

1. Misure predisposte su tre processi e sette round appaiati, con bootstrap gerarchico; non semplice riuso di un solo processo.
2. Preflight antecedente alle scritture e verifica degli effetti delle condizioni di errore, senza accettare eccezioni generiche come prova sufficiente.
3. Eventi di sincronizzazione distinti per ruolo e ordine, con mutazioni per rilevare omissioni. Le sincronizzazioni interne di SQLite non risultano direttamente osservate.
4. Casi privati del supervisore e accessi relativi a descrittori, controlli su symlink e hardlink prima delle modifiche. Non costituiscono una sandbox contro un processo ostile con lo stesso UID.

## Limiti e passo successivo

La suite predisposta conserva i precedenti 38 casi per backend e aggiunge regressioni: 44 casi SQLite e 46 JSON. Questi conteggi descrivono il codice, **non test già superati**. SIGKILL controllato non prova resistenza a un blackout reale. Prima dell'esecuzione occorrono supervisore libero e preflight della quota scratch; la manutenzione delle fixture resta un'operazione separata.

Nessun benchmark, adozione di backend, validazione scientifica o certificazione legale. Costi e token non disponibili. Lo stato della successiva esecuzione appartiene a `docs/progress.md` e al suo specifico record.
