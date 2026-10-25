# M1.c: review statica prima della prima esecuzione

## Esito

Review indipendente di sicurezza: idoneo soltanto a una prima validazione locale controllata, non alle misure conformi alla preregistrazione. Il coordinatore ha richiesto di correggere i quattro rilievi prima dell'esecuzione. M1.c resta aperta.

Revisore: agente `security`, sessione `ses_e8c4fd1faffex3f7sPIaEJ0Tce`; modello `openai/gpt-6-astra` autodichiarato, senza attestazione indipendente. Lettura integrale di `persist.py` (402 righe), `run.py` (791) e README (165); protocollo righe 24-111. Nessuna esecuzione, importazione, compilazione o misura.

Versioni effettivamente esaminate, precedenti alle correzioni:

- `tmp/m1-persistence/persist.py`: SHA-256 `47c817431517e06db3d2f8952764563f329731b49a7beabf253311ee745a788d`.
- `tmp/m1-persistence/run.py`: SHA-256 `36ff046ddb0b0cd42ae824d72e1067fd75d72e16b15e28f9aa28ae5f41afa50a`.

## Rilievi da chiudere

| Rilievo | Correzione richiesta | Verifica futura |
|---|---|---|
| Campionamento non conforme | Tre repliche di processo, sette round warm appaiati per replica; startup separati e bootstrap gerarchico del protocollo | Ispezionare gli identificativi e ripetere le misure sul nuovo harness |
| Scratch verificato dopo la copia SQLite | Preflight prima delle scritture; categorie di errore e assenza di accessi proibiti asserite | La regressione deve fallire togliendo il preflight, senza dipendere dai privilegi del processo |
| Test fsync basato su conteggi insufficienti | Tracciare chiamate effettive, destinazioni e ordine prima dell'ACK; distinguere fsync JSON da sincronizzazioni interne SQLite non osservate | L'omissione delle sincronizzazioni prescritte deve essere rilevata dalla suite |
| Discendenti della directory non confinati | Casi privati del supervisore e operazioni relative a directory fidate, senza seguire link | Test di symlink e alias senza modificare file estranei alle fixture |

I 38 casi per backend sono presenti nel codice; questo non ne dimostra il superamento. SIGKILL riguarda i figli propri tramite `Popen` e handshake; non sono stati confermati kill verso processi estranei o recuperi dal commit corrotto alla vecchia generazione.

Un nuovo coder, sessione `ses_e8c255272ffeYHdt1kmmJ7SiWJ`, è incaricato delle sole correzioni in `tmp/m1-persistence/`. La consegna e la verifica del nuovo codice restano da registrare. Durante le misure M1.a/e non si eseguono test o benchmark di persistenza concorrenti. SIGKILL non è una prova di perdita di alimentazione; ENOSPC simulato non equivale a fault injection nel VFS SQLite.
