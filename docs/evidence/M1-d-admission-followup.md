# M1.d v4: partizioni, slot e ammissione fisica distinta

2026-10-25. Autore: modello dichiarato dal runtime `openai/gpt-6.1-sol`.
Preparazione soltanto. Nessun import del candidato, test, worker, benchmark o
preallocazione eseguiti. Nessuna ammissione runtime concessa in questo incarico.
Questa nota non attesta review indipendente o PASS dinamici.

## Scope, snapshot e fonti congelate

Letti inizialmente soltanto i sorgenti correnti assegnati: pool 569 righe,
run 731, disk 223, test_lifecycle 149, README 272. Le successive letture hanno
riguardato quei file, i nuovi sorgenti locali e le copie nel namespace assegnato.
Nessuna lettura degli altri prototipi, rete, installazione, delega, operazione
Git o indice. Nessuna modifica a milestone, progress o protocollo FROZEN.

Prima delle patch sono stati copiati tutti i venti file presenti in
`tmp/m1/d/admission-before-20261025-01/`, inclusi snapshot e receipt precedenti.
Creazione esclusiva, nessuna sovrascrittura. SHA-256 di `snapshot-receipt.json`:

```text
3f9dd2583d04b5f5089ab199944a34d0bbf74c55a39cfc0f95f2a5f5d09d63d8
```

La vecchia preregistrazione da 205 righe non è stata riletta o modificata.
Il suo hash storico rimane registrato nella receipt v3, che non è stata riscritta.
L'hash di preparazione v3 resta
`434be773d877167810143b17320c83ede9b46dba302c485e2f49d3ab3a0bb229`.
La nuova receipt è `tmp/m1/d/admission-static-20261025-01.json`.

## Ricetta e correzioni dei cinque finding

La ricetta diventa `M1.d-isolated-v4`, formato ammissione `M1.d-admission-v4`,
lifecycle `M1.d-synthetic-intermediate-v4`. Sorgenti nuovi: `bounds.py` e
`admission.py`. Tutto resta codice isolato sotto `tmp/m1/d/`.

1. **Due writer interni, quote disgiunte.** `Partitions` riserva atomicamente
   worker, aux, final ed emergency prima di avviare figli. Gli slot sono assegnati
   interamente ai rispettivi proprietari. Il worker non eredita l'arena: riceve
   soltanto il fd del proprio file, identità verificata contro la receipt e
   capacità fissa. Ogni pwrite verifica l'offset contro tale capacità. Non riceve
   il residuo comune. La quota logica del caso resta la richiesta preregistrata;
   la capacità fisica del fd è una riserva separata per il guasto deterministico.
2. **Aux e finalizzazione finite.** Un solo `auxiliary.arena` contiene regioni
   fisse, con header little-endian di 16 byte: lunghezza e stato 0 empty,
   1 committed, 2 intent. Dati e header vengono sincronizzati prima del commit.
   Non si creano file di log/report durante i casi e nessuna regione cresce.
   Ogni riga integrale ha un proprio slot e staging. Samples, report e manifest
   indicizzano le righe complete tramite percorso, lunghezza e SHA-256: non
   duplicano stderr e non eliminano metriche. Manifest conserva tre staging;
   ogni altro JSON uno. Un intent fallito non viene riusato. `emergency.json`,
   `emergency.raw` e `finalization-error.json` non sono consumabili dal worker.
3. **Ammissione unit reale.** La suite richiede proprio output nuovo, quota
   esterna, turno esclusivo, lock, preallocazione e receipt. Un id da solo non
   basta. Senza ammissione `setUpClass` ferma la suite; non produce sette skip.
   Il CLI esegue soltanto i sette test fissi, senza discovery o test aggiuntivi.
4. **Persistenza su file reali.** Le sette fixture sono file da 33 byte,
   preallocati, scritti, fsync e chiusi nella fase unit di ammissione. Il test
   quota legge SHA-256 e st_blocks dal file reale prima e dopo il rifiuto di una
   nuova riserva; non usa un contatore senza file. Il verificatore piccolo dei
   test lifecycle rilegge realmente quei byte. Nei casi scientifici spill-cancel
   ed ENOSPC, il supervisore rilegge indipendentemente il parziale, confronta
   identità/hash/blocchi e charge riportato dal worker. Nessuna equivalenza
   empirica tra fixture piccola e spill fisico da 256 MiB.
5. **FD e mapping.** Il turno deve escludere fd e mapping esterni sui case
   privati. `borrow` e `mapping` tengono registrato il consumer fino alla chiusura
   effettiva, compreso mmap.close. Il test di rilascio tenta l'unlink durante un
   borrow fd e durante un mapping reale: entrambi devono bloccarlo. Il processo
   worker deve essere terminato e tutti i consumer propri chiusi PRIMA
   dell'unlink. La receipt riporta `namespace_allocated_bytes_removed`, non
   spaccia la sparizione del nome per una prova globale di liberazione OS.

Non si effettuano scansioni di fd di processi esterni. L'esclusione dei lettori
esterni è un contratto del case privato amministrato dal coordinatore. Il lock d
non è un lock globale; la scansione a 250 ms rimane diagnostica.

## Bound logico derivato

Gli interi della tabella seguente sono calcoli statici, NON misure fisiche.
Il calcolatore di verifica legge letterali e aritmetica AST; non importa né
chiama `bounds.plan` o altri simboli candidati.

Il profilo `EVENTS` riproduce i call path effettivi: reserve/release per lease,
due job in worker-failure e completion-order, più admission_closed e un
checkpoint_cancel_requested nel caso dynamic-budget. Totale bounded: 40 eventi;
full: sei. Nessun record per tile. Un job non quiescente emette un solo annuncio,
poi attende; il controller può aggiungerne un secondo, poi attende anch'esso.

Con L=65536, R=65536 e J=1048576:

- stdout per caso con e eventi: `(e+3)*L+R`, inclusi due annunci unquiescent,
  un terminale e l'ultima read che può attraversare la guardia;
- stderr per caso: `2*R=131072`, preservando anche la read che supera 65536;
- reservations, duplicato canonico degli eventi: `(E+2*N)*(L+64)`; i 64 byte
  coprono il campo case ASCII con nome scientifico inferiore a 32 caratteri;
- unione degli schemi metadata row/release/retained/stdout/stderr: 42 proprietà,
  verificata sull'AST reale. Chiavi <=64 caratteri, scalari testuali <=256;
  JSON ensure_ascii usa al massimo sei byte per carattere. Counter uint128,
  parametri ASCII, id <=128, massimo due componenti output <=64 ciascuna;
- metadata row: `42*((64+256)*6+5)=80850` byte. Riga integrale:
  `L+6*131072+80850=932818`, inferiore al cap JSON invariato di 1 MiB;
- ogni riga ha uno staging distinto della stessa capacità; nessuna compattazione
  o riduzione dopo un errore. Ogni ref di indice ha cap derivato/verificato di
  1024 byte; samples occupa `N*1024`;
- quattro summary finali da J, tre staging manifest e uno per ciascun altro
  summary: dieci regioni J. Le due receipt lifecycle hanno final e staging;
- emergency: tre regioni J, una regione `2*R+L+R` e relativi header;
- unit-suite.log: sette regioni logiche `2*R+L` nello slot comune, per trace e
  risultato dei sette percorsi chiusi. Il modello copre fino a 64 frame da
  512 byte e 32 tuple diagnostiche con due stringhe da 256 caratteri escaped,
  più un record terminale. I sorgenti Python propri hanno riga massima 176.
  Nessun input dati o plugin esterno entra in quei test.

La normalizzazione dei summary è lossless rispetto agli artefatti conservati:
raw stdout/stderr, eventi duplicati, righe complete, staging e receipt restano.
Una violazione di schema/capacità produce STOP, non un log troncato per fit.
I parametri sono preregistrati, non scelti in base all'esito.

| Fase | Slot arena | Aux | Final | Emergency | Worker | Totale contenuto |
|---|---:|---:|---:|---:|---:|---:|
| validate | 104 | 55437892 | 14701808 | 3407936 | 268505118 | 342052754 |
| measure | 28 | 7957180 | 10489008 | 3407936 | 0 | 21854124 |
| unit | 73 | 16730828 | 41229952 | 3407936 | 231 | 61368947 |

Aux+final+emergency: validate 73547636 byte, measure 21854124, unit 61368716.
Non si dichiara un bound di 8 MiB. Questi valori derivano dal layout conservativo
che mantiene cap e staging, non da un margine arbitrario di 64 MiB.
Se non entrano nella quota comune, il ramo si ferma: non riduce candidati o log.

Worker validate: un solo 268435456, più 65536, 4096 e marker non-directory di
30 byte, totale 268505118. I residui attesi sono esattamente 69662 byte logici.
Il marker v4 è `b"synthetic non-directoryscratch"`, non il marker v3 da 31 byte;
è solo la fixture di errore, non un dato scientifico. I file delle prove ancora
NOT_RUN sono riserve preallocate, non dati già processati o spill verificati.
Un errore inatteso conserva anche il completo e ferma la sequenza: nessun job
seguente. Non si sommano tre spill da 256 MiB. Full procedural ha worker zero.
Unit riserva sette file da 33 byte, non sette file da 256 MiB.

## Prova fisica: residuo minimo esplicito

`--operation admit` è una fase distinta: verifica quota/turno privato/ceiling,
riserva atomicamente il layout, crea il namespace finito e usa posix_fallocate.
Fsync e fstat devono osservare arena e file effettivamente allocati, non sparse.
Sincronizza tutte le directory, poi registra layout, identità, blocchi, source
hash, plan hash e occupazione osservata nella receipt. Nessun worker o benchmark.
Un filesystem non supportato produce STOP, senza altro filesystem o algoritmo.
File parziali e intent rimangono. L'osservazione termina in
`OBSERVED_UNREVIEWED`, sempre con `proof_status=UNPROVEN`.

Il residuo non è più uno stream illimitato o una quota condivisa. Restano da
dimostrare sul filesystem effettivo due condizioni fisiche precise:

1. il bootstrap di file/directory/fallocate, inclusi arrotondamenti e metadata,
   non supera il ceiling assegnato dal coordinatore durante l'ammissione;
2. le scritture entro estensioni preallocate, la conversione unwritten->written
   e i rename/unlink del namespace finito non richiedono ulteriore allocazione
   oltre il modello provato. CoW, compressione o extent metadata possono
   invalidare l'invarianza: una sola osservazione non li esclude.

I minimi di contenuto sono i tre totali della tabella, NON ceiling fisici.
Non si deduce un bound fisico da fragmentsize o dal solo nome scomparso.
Non è stata eseguita alcuna misura di questi termini. Non sono stati cambiati
filesystem, quote del sistema o feature di mount. Senza quella prova il runtime
resta bloccato, anche se si fornisce un id o si completa la preallocazione.
`PROOF_STATUS` non viene modificato da nessun percorso CLI.

Le API operative, il layout, il writer e la fase osservativa sono implementati;
non si sostituisce la prova mancante con un flag. L'equazione da chiudere è
`baseline propria + quota esterna + massimo fisico della fase <=1073741824`.
La baseline include tutti i vecchi snapshot, receipt, staging e parziali propri.
Il coordinatore deve aggiornare la quota prima di ciascun turno esclusivo.
Il ramo D resta fermo; gli altri rami indipendenti possono proseguire.

## Stato e retention

```text
PREPARED / UNPROVEN
  -> turno esclusivo + quota + ceiling + lock
  -> ADMISSION_CREATE
  -> fallocate + fsync + blocchi osservati
  -> OBSERVED_UNREVIEWED / UNPROVEN
  -> prova filesystem + review + nuova ricetta ammissibile [non abilitato qui]
  -> WORKER su solo fd/capacità assegnati
  -> PROCESS_EXIT / consumer propri chiusi
  -> verifica indipendente / receipt committed
  -> unlink del solo intermedio riuscito
  -> namespace st_blocks osservato / release receipt
  -> prossimo caso già partizionato

errore -> STOP_RETAIN; nessuna pulizia automatica, nessun job seguente
```

Il worker restituisce RAM solo dopo quiescenza. Il disco rimasto passa a charge
persistente, verificato sul file reale dal supervisore. Le receipt fallite e gli
staging restano; non vengono rimpiazzati da un output vuoto. L'unlink di un
intermedio già verificato non è reversibile se un fsync successivo fallisce:
la receipt precedente rimane, la sequenza si ferma. Nessuna garanzia power-loss.

## Preservazione dei test e stato scientifico

Mappa CASES identica allo snapshot: 21 bounded, 15 errori attesi. Tre full da
40 GiB, spill fisico da 256 MiB, pool 32 MiB, tile 2 MiB, cap scratch 1 GiB,
formula periodica e oracle non cambiano. Non si promuove 32 MiB a 40 GiB.
Si preservano i sette nomi di test lifecycle; si rafforzano con file/hash reali,
consumer fd/mapping e ammissione fisica. Tutti restano NON ESEGUITI.
Misure formali, tre startup, due warm-up/sette campioni e bootstrap appaiato
restano NON IMPLEMENTATI. RSS rimane best effort, non un limite OOM universale.

## Sorgenti fissati e verifica realmente svolta

SHA-256 relativi a `tmp/m1/d/`:

```text
2fcabd27695dfb5085d8449ad2340fa2e16b57196930716e1d7d988d0c28a1ee  pool.py
12561229f99675b07c27f568f8aaac39d1a7d7dd5e9b62a9c815edd293dfe617  run.py
384414d08513fc0856c246a8348dfb25e1d7ca8c660c043796bcbbaef1458f29  disk.py
7b7e0af077f9365269092d044aa9b26f67beba9699490abfa6557a06fbe83e0a  admission.py
5193760b38874e071220c07cf6db8cc0eb5c158c6929d4e1d5919b4d34881158  bounds.py
58dc11ccd498bcc4b094ada26c622916447ef13a2f437d70c98afd6091bb115b  test_lifecycle.py
7d530de80ab88bf011882531876f96fa58e55f14a9e3cb43332a2b96d3f937af  README.md
```

Byte/righe: pool 26621/584; run 44964/806; disk 9899/201; admission 12914/287;
bounds 5138/89; test_lifecycle 11861/242; README 20431/317.
Ambiente del verificatore statico: Python
`3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`, Linux.

Sono stati eseguiti AST parse sui sei Python, hash e aritmetica da letterali AST.
Un primo helper statico cercava Return solo a livello diretto: exit 1,
`StopIteration`. Corretto il helper per visitare il Return annidato nel try:
exit 0, unione metadata 42. Non era un errore candidato o un test dinamico.
I verificatori finali usano `-I -B -S`, senza site hook installati; anche i figli
e i comandi ufficiali v4 usano `-S`. Nessun import candidato/compile/exec.

La nuova receipt statica conserva comando, hash, confronto con i venti file
congelati, quindici file storici immutati, CASES/costanti preservate, sette test,
profili eventi e calcoli. Il nuovo hash di preparazione include questa evidence;
non viene spacciato per il recipe hash di una serie runtime mai avviata.

## Handoff: primo comando futuro, NON ESEGUITO

Prossimo passo: review statica indipendente e chiusura delle due condizioni
fisiche. Nessun voto del proprietario o cambiamento milestone è necessario.
Soltanto un successivo turno realmente ammesso può avviare l'osservazione.
Il ceiling deve avere una base fisica, non essere il totale logico arrotondato.

```bash
OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S tmp/m1/d/run.py --operation admit --unit d --phase validate --scope bounded --external-scratch-bytes "${EXTERNAL_SCRATCH_BYTES:?serve quota aggiornata}" --exclusive-admission-id "${EXCLUSIVE_ADMISSION_ID:?serve turno esclusivo reale}" --private-no-external-readers --physical-ceiling-bytes "${ADMISSION_PHYSICAL_CEILING_BYTES:?serve ceiling fisico dimostrato}" --output tmp/m1/d/series-admission-v4-01
```

La suite piccola ha un proprio comando di ammissione, mai eseguito:

```bash
OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S tmp/m1/d/test_lifecycle.py --operation admit --external-scratch-bytes "${EXTERNAL_SCRATCH_BYTES:?serve quota aggiornata}" --exclusive-admission-id "${EXCLUSIVE_ADMISSION_ID:?serve turno esclusivo reale}" --private-no-external-readers --physical-ceiling-bytes "${UNIT_ADMISSION_PHYSICAL_CEILING_BYTES:?serve ceiling unit dimostrato}" --output tmp/m1/d/unit-admission-v4-01
```

Questi comandi non avviano i test. `--operation run` rimane UNPROVEN/STOP finché
non esiste una prova fisica completa e una nuova ricetta approvata dalla review.
