# M1: preparazione dello store sintetico

Esito effettivo: `STATIC_PREP_ONLY`, exit 0. Nessuna migrazione ammessa o eseguita.
Non sono stati importati `store.py`, creati/aperti ZIP reali, letti payload fixture,
compilati candidati o eseguiti worker, test e benchmark. Nessuna rete, installazione,
operazione Git, creazione di indici, uso Siril, lettura di segreti o delega.
Le sole scritture sono i sorgenti/documenti del namespace assegnato e questo record.
`docs/progress.md` non è stato modificato. M1 resta aperta.

## Risposta sulla chiusura del runner immutato

Il runner PREP004 non esige tutte le 168 fixture contemporaneamente.
`tmp/m1/e-vulkan/run.py:390-396` autentica indice e manifest JSON, seleziona un solo
case e chiama `verify_originals`. `run.py:261-267` hasha **input e GT interi**, anche
per inverse, e richiede `inverse_source == gt`. Ripete il controllo a `run.py:497`.
`prepare.py:30-35` legge gli hash in chunk da 65536 byte, senza caricare altri payload.
L'import di prepare definisce funzioni/costanti; il suo `main` è protetto a riga 275.

`run.py:242-259` passa input originale in forward e GT in inverse; stride diventa
width in inverse. L'offset GT è `w*h*2*4` in inverse FFT e zero negli altri casi.
`engine.cpp:73-87,477-502` apre soltanto quei percorsi per i dati: legge il prefisso
`stride*h*4` del reale forward oppure `w*h*2*4` del complesso inverse. Legge
`w*h*8` byte di GT razionale per affine/convd/warp, oppure `w*h*2*4` di GT FFT
all'offset indicato. Il GT FFT completo contiene entrambi i segmenti, già manifestati.
CPU e GPU ricevono la stessa chiusura. Batch 1/8 riusa quei buffer, non apre nuove fixture.
Il gate f64 esce prima del caricamento dati (`engine.cpp:490-494`); non converte precisione.

Mapping esplicito di tutte le 66 coppie `(case, direction)`:
`tmp/m1/fixture-store/read-closure.md`, righe 18-83. Sono 18 forward pixelwise e
24 FFT con entrambe le direzioni. Il confronto statico ha verificato ogni riga
contro l'indice, inclusa la linea `id`, senza eseguire tuple. Unione dei payload:
66 file distinti; ogni chiusura ne contiene due, massimo 21 041 152 byte interi.
Anche quando l'engine inverse non legge input originale, il runner lo hasha:
materializzare soltanto GT sarebbe insufficiente. La mappa non modifica le fonti 128
del controller batch, che non è stato letto.

Le dipendenze non-fixture restano persistenti. Il runner hasha gli otto sorgenti
elencati in PREP004: README.md, copy.comp, engine.cpp, fft.comp, point.comp,
prepare.py, run.py e test_followup.py (`run.py:383-384`; PREP004:223-231).
Controlla inoltre i due piani, engine, tre SPIR-V, indice e manifest
(`run.py:385-392`). Per validate/faults/confirm carica reference-lock e pilot,
confronta il binding e hasha pilot (`run.py:411-426`). Faults/confirm verificano
ricevute validate/faults e **tutti gli artefatti da esse elencati**, report compreso
(`run.py:272-316`). Quei percorsi dipendono dagli argomenti del futuro comando:
non sono sostituiti da nomi inventati e non si possono omettere. Rimangono fuori
dalla allowlist di retire/release. I due piccoli file del copy-control vengono
creati dal vecchio runner nel proprio output (`run.py:331-360`), non dallo store.
L'engine GPU legge inoltre lo shader scelto (`engine.cpp:296,509-512`). Librerie e
runtime di sistema non vengono archiviati. Questa PREP non attesta la loro disponibilità futura.

## Fonti e controlli realmente svolti

Comando statico ripetibile dalla radice, eseguito con exit 0 dopo l'ultima modifica ai sorgenti:

```bash
PYTHONDONTWRITEBYTECODE=1 OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S tmp/m1/fixture-store/static_check.py
```

Lo script usa AST, non import/exec del tool. Verifica otto pin di sorgenti/JSON
stabili, coerenza dell'allowlist nei due manifest, metadati delle 168 fixture e
mapping delle 66 tuple. Conferma 11 regressioni scritte, **zero eseguite**.
Python osservato: `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`, Linux, UID 1000.
Modello dichiarato dal runtime e qui riportato come self-report: `openai/gpt-6.1-sol`.
Costi e token non sono esposti; nessuna attestazione indipendente viene attribuita all'autore.

Copertura effettiva delle letture integrali, con conteggi delle righe:

| Percorso | Righe | Lettura |
| --- | ---: | --- |
| `AGENTS.md` | 52 | testo integrale |
| `docs/progress.md` | 51 | testo corrente integrale |
| `PLANS.md` | 46 | convenzione necessaria al piano breve |
| `docs/evidence/M1-storage.md` | 125 | testo integrale |
| `tmp/m1-maintenance/sparse_copy.py` | 443 | sorgente integrale |
| `tmp/m1-maintenance/apply-001/report.json` | 9868 | JSON integralmente parsato; preview testuale iniziale parziale |
| `tmp/m1/a/series-001/cases.json` | 1742 | JSON integralmente parsato |
| `tmp/m1/a/series-002/cases.json` | 2174 | JSON integralmente parsato |
| `tmp/m1/e-vulkan/run.py` | 511 | sorgente integrale |
| `tmp/m1/e-vulkan/prepare.py` | 276 | sorgente integrale, dipendenza diretta necessaria |
| `tmp/m1/e-vulkan/engine.cpp` | 566 | sorgente integrale |
| `tmp/m1/e-vulkan/preparation-004.json` | 330 | JSON integrale |
| `tmp/m1/e-vulkan/build-004/cases-index.json` | 644 | JSON integralmente parsato |
| `tmp/m1/fixture-store/store.py` | 626 | nuovo sorgente, AST/hash |
| `tmp/m1/fixture-store/test_store.py` | 135 | nuovo sorgente, AST/hash |
| `tmp/m1/fixture-store/static_check.py` | 105 | nuovo sorgente, AST/hash |
| `tmp/m1/fixture-store/read-closure.md` | 83 | confronto delle 66 righe dati |
| `plans/M1-fixture-store.md` | 57 | nuovo piano breve, hash |

I due manifest attestano 168 percorsi unici, 48 referenze inverse già incluse e
847 288 776 byte logici. L'identità statica corrente coincide con `fixture_final`
del report storage per device/inode/size/mode/uid/gid/nlink/mtime/ctime.
Allocazione corrente delle sole fixture: 562 286 592 byte. Non è una nuova verifica
dei loro byte: i precedenti SHA e le 53 copie sparse byte-verified restano attestazioni
del report storage pinned. Byte di payload letti durante questa PREP: **0**.
Nessun contenuto di F, D, viewer o batch attivi è stato letto; una ricognizione di
nomi e la contabilità globale hanno usato soltanto metadati.

SHA-256 dei deliverable e delle fonti decisive:

| Percorso | SHA-256 |
| --- | --- |
| `tmp/m1/fixture-store/store.py` | `cfa55680472c368ac3d22e82f5c77a8a925a7dcfaf608abf2c0675ebc2019e48` |
| `tmp/m1/fixture-store/test_store.py` | `3243c568c457417d96f9c2616dff872968d29e2f670d28b5358e59cfc1c944fb` |
| `tmp/m1/fixture-store/static_check.py` | `d41139568ea7447aebc4edfe3f0b812553bdf3bd7df9105082d6ba1f8fd6bf84` |
| `tmp/m1/fixture-store/read-closure.md` | `c294abfb5f24f112c2fad558624005267731b91e0b0f891b0f6d3b83d92680ef` |
| `plans/M1-fixture-store.md` | `de17f2218fe582006296f4b971f33d56a29535f1eacc783a27c41f2dbe79df2a` |
| `tmp/m1-maintenance/apply-001/report.json` | `bb725b34c38a70c17c97714d5505bdbcc25fa76fb10f2cba6070d1f04408d2cc` |
| `tmp/m1/a/series-001/cases.json` | `58ba0eed09d2f5e3a77e8b52243ce4c8c6a411cfde405d3688cf8d8f27c1ddef` |
| `tmp/m1/a/series-002/cases.json` | `a7db9f087c5dbf8fc33be673acf876fcc8d822828864a6aec1d8019c044a13b1` |
| `tmp/m1/e-vulkan/run.py` | `f1d6bf74e0937773ee11d5315382d9c660de2df96ed98426c993491ab62c107a` |
| `tmp/m1/e-vulkan/prepare.py` | `37851d20881fe4259cf51e0b3eaeae767d42aa930901ae686e493f2d4c0c05a2` |
| `tmp/m1/e-vulkan/engine.cpp` | `8cc0ff377575a31f68a4396c3a396ffd9c6d0e38162682ff62d27bcf62bbb1e1` |
| `tmp/m1/e-vulkan/preparation-004.json` | `d19f9d7082f9cb31cb21b0a98d6d1a52044cd3474b6fd42099f383c87d4befe0` |
| `tmp/m1/e-vulkan/build-004/cases-index.json` | `c958973283f14d7a0377c8b5e728b2d04918081229ce2d8a953ee81305d4629b` |

## Contratto del tool preparato, ancora da qualificare

Il codice riprende l'attraversamento fd-relative NOFOLLOW e la metrica st_blocks
dalla manutenzione letta, senza importare quel vecchio eseguibile. Usa soltanto
stdlib e fonti passate già pinned; non dipende da futuri scheduler o riscritture.
Ogni componente di path viene aperto NOFOLLOW. I file devono essere regolari,
di proprietà dell'UID corrente e con nlink 1; anche gli inode dell'allowlist devono
essere distinti. Path non canonici, escape, xattr/ACL non attestati e alias producono STOP.
Il manifest nella ricevuta archive contiene origine/report SHA, path, SHA dei byte,
size, mode e identità originale. Non viene modificato alcun manifest scientifico.

Ogni invocazione CLI richiede `--phase`, `--run-id`, `--admission` e
`--admission-sha256`. L'ammissione deve essere un nuovo JSON esterno nello store,
pinned dal coordinatore, con schema 1, status ADMIT, fase/run-id esatti, SHA del tool,
author/coordinator e attestazioni `prepared_code_independent_review: PASS`,
`global_writers_exclusive: true`, `readers_closed: true`. Il pin esterno è la radice
di fiducia: questi campi non autenticano da soli la persona che li ha scritti.
Nessun file ADMIT è stato prodotto. Una PREP, una receipt PASS o un'altra fase non
conferiscono permessi impliciti. Non usare funzioni interne come API per saltare la CLI.

- Archive richiede `archive_limit_bytes`, positivo e al massimo 134 217 728.
  Prenota tutto quel massimo più ricevute/metadati prima di creare lease/run/archive.
  ZIP nuovo esclusivo, ZIP_DEFLATED streamed, senza ZIP64, input buffer 256 KiB.
  Il sink seekable limita l'high-water reale; non stima un rapporto di compressione.
  Ogni sorgente supera SHA/size e identità durante la lettura. Archivio e directory
  ricevono fsync, l'archivio diventa 0444, poi viene scritta la receipt durevole.
- Verify richiede `archive_receipt: {path, sha256}`. Controlla pin e identità dello ZIP,
  limiti della central directory prima di aprirla, copertura esatta 168, nomi unici,
  tipo/modo regolare, header locali/centrali concordi, assenza di link/encryption/extra,
  dimensioni manifestate e extent senza sovrapposizioni. Decomprime **ogni byte**,
  confronta SHA/size e byte originali, ricontrolla identità immutata. CRC non basta.
- Retire e materialize richiedono quella stessa archive receipt e una `verify_receipt`
  PASS compatibile, più `independent_review` PASS con SHA della verify receipt e
  reviewer distinto dall'author. L'audit va svolto davvero prima di rilasciare copie.
  Retire richiede inoltre `generated_copies_independent_success: true` e `paths`
  esattamente uguale all'allowlist 168. Precheck di tutti i file prima del primo unlink;
  poi nuovo controllo id/path/SHA e flock esclusivo del singolo file prima di ogni unlink.
  Ogni intenzione di unlink e la directory sono sincronizzate. Nessun proof, log,
  sorgente, manifest o fixture negativa fuori allowlist viene selezionato.
- Materialize richiede case/direction. Apre e autentica prima l'archivio primario,
  non tenta prima loose. Deve trovare assenti entrambi i target storici. Scrive prima
  un intent durevole con i percorsi; i target O_EXCL sono gli stage temporanei
  dichiarati, non copie persistenti da cercare come alternativa. Il turno esclusivo
  vieta reader finché non esiste la receipt PASS. Scrive tutta la chiusura, fsync,
  SHA/size, riapertura e confronto byte per byte con una seconda decompressione.
  Non avvia il runner. Archivio assente/corrotto o spazio insufficiente: STOP, mai rigenerazione.
- Release richiede gli stessi pin archive/verify, la `materialization_receipt` pinned,
  `paths` esattamente uguale alle nuove copie dichiarate e il loro id corrente.
  Richiede inoltre `own_child_reaped`, `readers_closed`, `coordinator_result_review: PASS`,
  `consumer_materialization_receipt_sha256` e `result_receipts`. Ogni voce dei risultati
  deve pin-nare report.json/pilot.json, dichiarare `artifacts` completi path/SHA e
  `complete_artifact_allowlist_reviewed: true`. Il codice controlla case/direction,
  stato riuscito, exit atteso, reap e assenza di errori di cleanup; verifica hash e
  richiede fsync di ogni artefatto e relativa directory prima di cancellare copie.
  Elimina soltanto quelle nuove copie. L'archivio e tutte le ricevute restano.

`readers.lock` è persistente. Ogni fase acquisisce LOCK_EX non bloccante e verifica
l'inode registrato dalla prima archive. Il coordinatore deve tenere LOCK_SH sullo
stesso inode per l'intera vita del proprio child, dei reader e delle relative chiusure.
Il tool non avvia quel child e non può inferire l'uso della lease da vecchi report:
la correlazione nuova materializzazione/nuovo processo richiede il registro esterno
pinned del coordinatore. Un reader non cooperativo non viene scoperto da flock.

## Budget e costo finito

Osservazione metadata prima delle scritture PREP: `/usr/bin/du -s -B1 tmp` ha dato
867 028 992 byte. Non è una prenotazione futura. Il guard del tool scandisce tmp/
fd-relative, somma st_blocks*512 con inode unici e conta i symlink senza seguirli.
Conta sorgenti loose, archivio, stage, vecchi tentativi, ricevute e altri namespace.
Ogni nuova operazione richiede spazio globale e free-space, più margine 16 MiB;
il budget di ricevute/metadati è 4 MiB. Archive prenota il massimo ammesso dello ZIP,
non 847 MB aggiuntivi e non spazio liberabile da un retire futuro.
Con la sola fotografia osservata, 128 MiB ZIP + 4 MiB + 16 MiB darebbero
1 022 218 240 byte, entro 1 GiB. Il preflight reale deve rifare il conto.
Se lo ZIP non entra nel limite o il picco non entra nel cap, STOP conservando tutto;
non si elimina una sorgente per far avanzare archive. La compressione effettiva è ignota.

Con L = byte della chiusura selezionata, L massimo 21 041 152: materialize prenota
L più arrotondamenti, 4 MiB e margine. Archive conserva tutte le 168 fixture, non solo
i 66 file usati da questo indice. Il massimo di scrittura ZIP è 128 MiB; gli originali
restano presenti durante archive e verify. Dopo retire non si promette una quota
ad altri rami: lo spill fisico 256 MiB richiede un'ammissione e un preflight propri.

Per archive i manifest danno 3334 chunk sorgente da al massimo 256 KiB:
847 288 776 byte letti e hashati, più hash dello ZIP limitato. Verify legge quel ZIP,
decomprime 847 288 776 byte e confronta altrettanti byte loose. Retire rilegge SHA
loose due volte e lo ZIP una volta. Materialize hasha lo ZIP, decomprime 2L, scrive L
e rilegge lo stage 3L fra SHA e confronto. Release hasha ZIP e nuove copie due volte,
più gli artefatti esplicitamente dichiarati. Non sono tempi misurati o garanzie di CPU.
Le scansioni metadata hanno limite 250000 inode; un albero più grande produce STOP.

RLIMIT_AS 512 MiB limita lo spazio virtuale del processo, non impone una quota RSS
globale. ru_maxrss viene verificato fra chunk/scansioni: controllo RSS best effort,
non prova di assenza di picchi intermedi. zlib/zipfile mantengono buffer interni stdlib;
256 KiB è il buffer di trasferimento esplicito, non un limite a tutta la heap.
Né scansione st_blocks né margine impediscono scritture esterne, blocchi di file
scollegati ancora aperti, delayed allocation o allocazioni nascoste del filesystem.
Per ammettere operazioni distruttive servono turno globale esclusivo e reader
cooperativi chiusi, senza attribuire a queste verifiche il valore di una quota OS.

## Limiti aperti e prossimo comando futuro

Review indipendente del codice, esecuzione delle 11 regressioni e review dei loro
risultati restano aperte. Le regressioni coprono helper reali con dati minuti propri,
non costituiscono ancora un test integrato delle cinque CLI, dei crash o del power-loss.
Le fixture reali non sono state compresse; spazio ZIP e durata restano ignoti.
Immutabilità 0444, pin SHA e cooperazione non equivalgono a un immutable flag OS.
Un errore conserva tutti gli input residui/stage/log/proof, senza retry o cleanup.
Una prima archive fallita può lasciare lease/ZIP parziale e impedisce un secondo
archive automatico: la recovery richiede una nuova decisione esterna e conserva la prova.
Un guasto dopo unlink non ripristina magicamente quel pathname; l'archivio autorevole resta.
Se fallisce anche la scrittura della STOP receipt, il processo esce non zero e
diagnostica entrambi gli errori; non si dichiara una ricevuta durevole inesistente.

Con loose 001/002 assenti, i vecchi comandi non saranno immediatamente riproducibili.
Non vengono modificati, rietichettati o resi funzionanti tramite un restore nascosto.
L'archivio primario e la materializzazione esplicita richiedono nuova provenienza I/O.
Extraction, SHA e confronto scaldano la cache; i loro tempi restano preparazione.
Non vanno trasferiti ai vecchi tempi, al cold-process, al kernel o al total_ns scientifico.

Soltanto dopo review indipendente, regressioni ammesse e riuscite, review dei risultati
e **distinta ammissione archive** esterna, il primo comando archive sarebbe:

```bash
PYTHONDONTWRITEBYTECODE=1 OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S tmp/m1/fixture-store/store.py --phase archive --run-id archive-001 --admission tmp/m1/fixture-store/admit-archive-001.json --admission-sha256 "$ADMISSION_SHA256"
```

Il file indicato e il suo SHA non esistono in questa consegna. La variabile deve
provenire dall'ammissione del coordinatore, non da un'autorizzazione generata dal tool.
Verify, retire, materialize, runner e release restano separati e non ammessi da quel comando.
