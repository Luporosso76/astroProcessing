# M1: test del ciclo di vita dei contesti

## Ambito

Stato: `IN PROGRESS`, consegna `PREP_ONLY_NOT_RUNTIME_PASS`.
Chiudere il finding MED di copertura della review v2, senza cambiare lo store.
Fuori ambito: operazioni reali, archivio, payload scientifici, benchmark e rilascio.

## Prerequisiti

Fonti stabili e hash correnti: `docs/evidence/M1-fixture-context-followup.md`.
La PREP precedente resta immutata e registra la versione respinta dalla review.
Il gate operativo v2 è già presente. I suoi 24 test non provano i rifiuti successivi
all'emissione. Non serve modificare quei test per aggiungere la nuova copertura.
Lo slot F24 attivo esclude qualsiasi esecuzione in questa PREP.

## Deliverable

- `tmp/m1/fixture-store/context-lifecycle-tests/test_context_lifecycle.py`: 18 test di contesto e 5 di supervisione.
- `tmp/m1/fixture-store/context-lifecycle-tests/run_suite.py`: launcher futuro di 47 test.
- Ricevuta PREP nello stesso namespace; record delle verifiche in `docs/evidence/`.

## Ordine e gate

1. Leggere le sezioni di emissione, binding, lease, revoca e i test v2 stabili.
2. Preparare fixture proprie con emissione reale e prerequisiti sintetici dichiarati.
3. Verificare AST, conteggi e hash senza importare i moduli. Gate PREP soltanto.
4. Ottenere review indipendente sui file effettivi. Non vale un'autoreview.
5. Dopo la chiusura dello slot attivo, assegnare turno e distinta ammissione schema 2.
6. Eseguire una sola suite combinata, first-failure, con un solo child proprio.
7. Conservare log, intent, ricevute e file minuti anche in errore. Nessun retry automatico.
8. Se il child resta `PENDING_UNREAPED`, tenere bloccati slot e prenotazione.
   Nessuna raccolta o segnalazione futura basata sul solo PID registrato.

## Contratto scientifico

Questa unità non produce risultati scientifici. Le fixture non attestano closed168,
FULL verify o successo dei consumer. Misura soltanto l'accettazione/rifiuto del
contesto scoped e la conservazione dei piccoli file davanti a operazioni rifiutate.
Nessuna tolleranza numerica o dipendenza da timing. In errore: STOP non nullo.

## Test e misure

18 metodi di contesto, 5 di supervisione e 24 originari da byte autenticati: 47, senza skip.
I subtest non aumentano il conteggio dei metodi. Vedere il record PREP per copertura,
prerequisiti sintetici e limite di 128 fixture. La matrice runtime resta non eseguita;
nessuna prova pianificata viene presentata come eseguita.
Budget derivato 16 MiB, oltre al margine globale 16 MiB e dentro il cap globale 1 GiB.
Guard reale prima del child, cache e temporanei dentro il nuovo run sotto ROOT.

## Evidenza e rischi

`docs/evidence/M1-fixture-context-followup.md` distingue AST riuscito da test non eseguiti.
Il comportamento Linux fdinfo resta da verificare a runtime. I mock delle fonti
scientifiche impediscono di usare questa suite come ammissione a una fase reale.
I budget cooperativi non sono quote OS contro altri writer. Lo slot deve essere esclusivo.
La closure materialize è un prerequisito scientifico sintetico dichiarato.
Le due attese sul Popen hanno timeout finiti entro un unico budget di 120 secondi,
inclusi al massimo 5 secondi di raccolta dopo kill. Non è una garanzia OS su D-state,
fsync o latenza di scheduling. Il registro conserva il Popen irrisolto finché vive
il parent; terminare il parent non trasferisce autorità sul PID né libera lo slot.

## Rollback

Non sono previste cancellazioni o ripristini. I file v2, il suo receipt e il record
da 186 righe restano invariati. Se emerge un bug reale, registrarlo e correggerlo in
un'attività distinta, senza indebolire i test o i gate.

## Chiusura e prossimo passo

Gate runtime aperto. Prima azione minima: review indipendente dei nuovi file e della
ricevuta PREP, in sola lettura. Il comando futuro è nel record PREP; richiede una nuova
ammissione esterna e non può partire dallo stato di questa consegna.
