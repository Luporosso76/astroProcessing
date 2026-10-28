# M1: preparazione della sorgente sintetica archive-backed

## Ambito e prerequisiti

Stato dell'unità: PLANNED per ogni operazione sulle fixture. Questa consegna prepara
codice e analisi statica; non ammette archive, verify, retire, materialize o release.
M0 PASS; M1 resta aperta. Fonti operative: `AGENTS.md`, `docs/progress.md`.
La precedente conversione sparse è attestata in `docs/evidence/M1-storage.md`.
L'ammissione architetturale CONDITIONAL riferita dal coordinatore non sostituisce
review indipendente del codice consegnato, regressioni e verifica dei byte.

Obiettivo: conservare tutte le 168 fixture sintetiche tramite archivio primario
persistente, senza alzare 1 GiB, tagliare fixture o modificare pin e record scientifici.
Nessun originale utente entra nell'allowlist. Nessun altro ramo attivo viene letto.

## Deliverable e contratto

- `tmp/m1/fixture-store/store.py`: tool stdlib con cinque fasi separate.
- `tmp/m1/fixture-store/test_store.py`: regressioni preparate, non eseguite.
- `tmp/m1/fixture-store/read-closure.md`: mapping esplicito delle 66 coppie case/direzione.
- `tmp/m1/fixture-store/static_check.py`: controllo ripetibile AST/hash sorgenti/metadati, senza import del tool.
- `docs/evidence/M1-fixture-store-preparation.md`: copertura reale, limiti e protocollo delle ammissioni.

Il runner e l'engine PREP004 restano invariati. Il runner hasha input e GT interi
del solo caso selezionato, anche inverse; non richiede tutte le 168 fixture insieme.
Le dipendenze non-fixture restano persistenti e devono superare i controlli originali.
Le tolleranze, il pool CPU, i pin GPU e il controller delle fonti 128 non cambiano.
La preparazione non produce risultati scientifici né trasferisce tempi precedenti.

## Ordine e gate

1. Review indipendente sul testo effettivo; ammissione distinta delle regressioni minute.
2. Esecuzione e review delle regressioni, ancora non ammesse da questo piano.
3. Ammissione esterna pinned di archive, con turno cooperativo esclusivo e budget massimo finito.
4. Ammissione distinta di verify: tutti i membri decompressi contro SHA/size e byte originali.
5. Audit indipendente della ricevuta verify durevole. Soltanto dopo: distinta ammissione di retire.
6. Per ogni tupla: distinta ammissione di materialize da archivio primario pinned.
7. Coordinatore avvia separatamente il runner immutato, tenendo lease condivisa per tutti i reader.
8. Dopo reap, chiusura reader, ricevute durevoli e review dei risultati: distinta ammissione di release.

Ogni gate fallito lascia conservati archivio, input residui, stage e prove. Nessuna
rigenerazione, sovrascrittura, ricerca alternativa o pulizia automatica. Unlink già
completati prima di un guasto di retire/release non sono reversibili automaticamente.

## Test, rischi e chiusura

Regressioni pianificate: symlink foglia/componente, hardlink, path escape/alias,
membro duplicato/link, SHA errato, cap prima della creazione, target inatteso,
verify fallita prima di retire e release con lease viva. Nessuna è stata eseguita.
La contabilità st_blocks e la lease sono cooperative, non una quota OS. Il rapporto
di compressione e il picco reale restano ignoti fino a un archive ammesso.
I comandi storici 001/002 non saranno immediatamente riproducibili con loose assenti;
nessun ripristino nascosto modifica questa limitazione.

Prossimo passo verificabile: review indipendente di questi deliverable e
dell'evidenza. Il primo comando archive futuro, condizionato ai gate precedenti,
è riportato nell'evidenza; non va eseguito durante PREP.
