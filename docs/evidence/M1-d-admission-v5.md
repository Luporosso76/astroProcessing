# M1.d v5: lifecycle, stop della suite e proposta preventiva ext4

2026-10-25. Autore: modello dichiarato dal runtime `openai/gpt-6.1-sol`.
Preparazione e AST soltanto. Nessun import candidato, test, worker,
preallocazione, admit, run o benchmark eseguito. Nessuna delega, rete, Git,
modifica di sistema, indice, piano congelato o altro prototipo.
Runtime e profilo filesystem restano entrambi `UNPROVEN`.

## Congelamento prima delle patch

Snapshot byte/hash dei 44 file allora presenti:
`tmp/m1/d/admission-v5-before-20261025-01/`. Creazione esclusiva, nessuna
sovrascrittura. SHA-256 di `snapshot-receipt.json`:

```text
afa94047ddd80de6b093531d33f8ad39de53f0f579a1f22a0d77f6feb2654956
```

La verifica finale confronta tutte le copie e i 37 file storici non modificati.
Le sette sorgenti attive cambiano soltanto dopo lo snapshot; i vecchi report,
snapshot e receipt non vengono riscritti. In particolare resta invariato l'hash
di preparazione v4:
`bfdde5de9d413786a8f37495d7cff40023146feaa0259304891f8b6316e2cd60`.
Nessuna rilettura dei vecchi documenti evidence fuori dal namespace assegnato.

Letture iniziali: admission 287 righe, bounds 89, test_lifecycle 242, pool 584,
run 806, README 317. Disk e i nuovi file sono letti nelle verifiche AST/hash.
Il protocollo e il piano scientifico grande non sono stati riletti.

## Quattro finding trattati

### Mapping con viste esportate

`Arena.mappings` è indipendente dal registro fd. `mapping` registra un riferimento
forte al mmap prima di consegnarlo. `close_mapping` elimina la registrazione
soltanto dopo mmap.close riuscito e verifica di `view.closed`.
Se una memoryview esportata causa BufferError, il borrow può chiudere il fd,
ma il mapping rimane registrato. `require_no_consumers`, release e Arena.close
continuano a bloccare. Nessuna constatazione di cleanup riuscito viene inventata.

Regressione predisposta: esportare una memoryview, osservare BufferError,
verificare fd chiuso e mapping vivo, rifiutare il rilascio del file, poi rilasciare
la vista e chiamare esplicitamente close_mapping. Il file viene conservato e
riletto tramite hash indipendente; il solo cleanup verificato riguarda il mapping.

```text
BORROW_FD + REGISTER_MAP
 -> close mmap fallisce: FD_CLOSED / MAP_REGISTERED / RELEASE_FORBIDDEN
 -> memoryview.release
 -> close_mapping riuscito / closed=True
 -> UNREGISTER_MAP / controllo consumer consentito
```

### Slot terminali dopo I/O fallito

Un intent non è scrivibile. Un errore di write, fsync o flush marca il nome nello
stesso registro arena `failed_slots`; il rifiuto vale anche per nuovi oggetti Slot.
Il writer verifica l'header corrente, poi aggiorna `used=target` e stato intent
PRIMA dell'I/O. `replace=True` è esplicito e vale soltanto per uno slot sano,
non è una riparazione. Il commit diventa valido in memoria solo dopo sync e check.

Se l'ultimo fsync fallisce dopo che l'header 1 è stato scritto, il writer resta
terminale: il registro prevale sull'header. Non riscrive 2 per mascherare
l'incertezza, non ripristina il vecchio used e non modifica il payload per riusarlo.
La receipt unit registra i nomi degli slot terminali, inclusi quelli dei fault
intenzionali. Read del payload committed e write vengono rifiutati; la regione
raw rimane disponibile per analisi forense.

Regressioni predisposte: append da 4 byte con prima scrittura di 2 e successivo
ritorno zero; errore al fsync finale con header su disco già `(6,1)`; errore al
flush. Tutte verificano rifiuto di append/replace/read su oggetto vecchio e nuovo,
e identità byte per byte di header/payload dopo i tentativi di riuso.

Questa è una garanzia del processo/arena e degli intent presenti su disco,
non un protocollo crash-resume. Un I/O fallito che non persiste alcun bit non è
ricostruibile dal solo file. Namespace già tentati non sono autorizzati al resume;
nessuna promessa power-loss o riparazione automatica.

### Failfast e risultati reali

`TextTestRunner` usa `failfast=True`. `UnitResult` ferma failure, error, skip e
unexpected success; le eccezioni intenzionali catturate dalle assertion non
raggiungono quei callback. La suite usa l'ordine preregistrato, non discovery.

`unit-result.json` ha uno slot finale e uno staging propri. Conserva planned,
tests_run, failures/errors, casi EXECUTED con esito, casi rimasti NOT_RUN e slot
terminali. COMPLETE richiede result.wasSuccessful, tutti i casi avviati e tutti
gli esiti PASS, senza skip. Nessun numero di PASS viene derivato dal solo piano.
I sette test originali restano e precedono i cinque nuovi: conteggio v5 **12**.

### ENOSPC e short-write

Il writer richiede `min(4096-spill_written, residuo_view)` nel caso enospc.
Con una prima pwrite che restituisce 2048, la seconda chiede soltanto 2048;
la successiva iterazione produce esattamente `SPILL_ENOSPC` a 4096.
Lo slot scientifico ENOSPC resta 4096, senza cambio di errore atteso.

La regressione usa un file unit distinto, realmente preallocato da 4096 in una
futura ammissione, impone lo short-write e verifica richieste `(0,4096)` e
`(2048,2048)`, quantità scritta, codice esatto e SHA-256 indipendente del file
dopo fsync/close. Chiama soltanto il writer I/O: non simula un Job completo o
uno spill empirico da 256 MiB. Il file e la fixture normale restano disponibili.

## Conteggi e capacità preregistrate

Ricette: `M1.d-isolated-v5`, `M1.d-admission-v5`, `M1.d-unit-v5`.
La mappa dei 21 bounded e dei 15 errori attesi resta identica alla v4. Restano i
tre full da 40 GiB, worker 1/2/4, pool 32 MiB, tre buffer da 2 MiB per worker,
spill fisico 256 MiB e cap comune tmp 1073741824. Formula/oracle non cambiano.

Calcoli statici da letterali AST, non chiamate a bounds.plan e non misure FS:

| Fase | Slot | Aux | Final | Emergency | Worker | Byte contenuto |
|---|---:|---:|---:|---:|---:|---:|
| validate | 104 | 55437892 | 14701808 | 3407936 | 268505118 | 342052754 |
| measure | 28 | 7957180 | 10489008 | 3407936 | 0 | 21854124 |
| unit | 118 | 28681555 | 65287136 | 3407936 | 4492 | 97381119 |

La derivazione v4 resta storica. V5 mantiene 40 eventi normali bounded e sei full,
righe complete da massimo 932818 byte, raw log/eventi duplicati e relativi staging.
Per unit: N passa da 7 a 12 senza cancellare test. Si aggiungono tre regioni fault
da `33+16` byte e final/staging unit-result da `2*(1048576+16)` byte.
Worker unit è `12*33+4096=4492`. L'incremento rispetto ai 61368947 byte v4 è
36012172; non è spazio ricavato troncando log o riducendo fixture.

I valori sono contenuto/regioni, NON ceiling fisici. Il file da 33 byte può
occupare un blocco intero, ogni file ha un proprio arrotondamento e i metadata
richiedono un bound separato. L'occupazione unit precedente indicata dal
coordinatore resta da conservare nella quota; qui non è stata rivalutata o
rimossa. Il bounded oggi non entra e non è stata tentata alcuna ammissione.

## Unica ricognizione filesystem effettivamente eseguita

Target unico: `/opt/workspace/astroProcessor/tmp/m1/d`. Un solo bundle readonly:
findmnt per target/mount options, statfs tramite stat -f, attributi del solo inode
directory tramite lsattr, versioni dei tool e uname -srvm. Nessun dispositivo raw,
configurazione personale, scrittura FS, ioctl allocante o modifica mount.
L'output è conservato in `tmp/m1/d/fs-metadata-v5-20261025-01.json`.

```text
findmnt: / /dev/nvme0n1p2 ext4 rw,relatime rw
statfs: type=ext2/ext3 magic=ef53 block=4096 fragment=4096 name_max=255
lsattr: --------------e-------
findmnt da util-linux 2.41.3
stat (uutils coreutils) 0.10.0
lsattr 1.47.2 (1-Jan-2025)
Linux 7.0.0-38-generic #38-Ubuntu SMP PREEMPT_DYNAMIC Fri Sep  4 09:10:14 UTC 2026 x86_64
```

Findmnt identifica ext4; ef53/type generico appartiene alla stessa famiglia.
Il flag e attesta extents sull'inode directory, non densità o condivisione degli
extents dei file futuri. Nessuna opzione esplicita CoW/compressione appare
nell'output; la loro assenza effettiva non viene dedotta da ciò.
Restano ignoti bigalloc/cluster, inline_data, feature superblocco, extent sharing,
frammentazione, limiti dell'implementazione kernel, journal, ACL/xattr automatici,
DAX/verity/encryption. Fragmentsize 4096 non prova il massimo arrotondamento.
Free/available e inode counters del volume sono registrati, non usati come quota
residua del progetto o come dimostrazione di un massimo per syscall.

## Proposta PREVENTIVA, limitata a un profilo ext4 da verificare

`fs_bounds.py` è un calcolatore di proposta, non un rilevatore o una prova FS.
Il solo profilo candidato compatibile con il tipo osservato richiede:
ext4 sano, blocco E cluster 4096, bigalloc/inline_data disabilitati, niente
CoW/reflink/compressione/DAX/verity/encryption sugli oggetti ammessi; profondità
extent <=5, htree <=3, nuove allocazioni inode/xattr automatiche <=2 blocchi,
metadata globali/journal già preallocati e non espandenti. I requisiti non ancora
osservati devono essere verificati: questo host non è già attestato nel profilo.
La metrica è l'incremento di st_blocks del namespace e dei suoi metadata
attribuibili, non NAND fisica o disponibilità universale del volume.

La proposta usa B=4096, D=5, H=3. Per una mutazione extent riserva
`A=2*D+3=13` blocchi, per una mutazione directory `Q=2*H+2=8`, più X=2 per
inode/xattr. Anche questi massimi strutturali devono essere verificati contro
l'implementazione supportata; non diventano veri perché il calcolatore li usa.

Per range `(offset,length)`, `n=ceil((offset mod B+length)/B)`:

- fallocate su file vuoto: dati n blocchi. Senza ipotesi di densità, metadata
  `D*n+A+X`: al più D nodi per extent di un blocco, più frontiera in mutazione;
- proposta più stretta soltanto con inserimenti ascendenti e occupazione
  minima 170 nei nodi non di frontiera: ricorrenza `q=ceil(q/170)` per cinque
  livelli, somma di `q+1` a ogni livello, più A+X. Il 170 deriva da metà di
  `floor((4096-12)/12)=340` record, NON dalla quota disponibile. Serve prova
  dell'invariante di inserimento del kernel, non una preferenza per il valore;
- overwrite completamente dentro dati preallocati: nessun nuovo dato nel profilo,
  ma fino a `n+2` extents toccati possono richiedere conversioni/split;
  incremento massimo proposto `((n+2)*A+X)*B`;
- create: `(Q+A+X)*B`; mkdir: `(1+Q+2*A+X)*B`;
- rename: `(2*(Q+A)+X)*B`, considerando due parent; unlink: `(Q+A+X)*B`;
- fsync: stesso bound delle conversioni per TUTTI gli intervalli pendenti,
  non solo l'ultima pwrite. Nessuna ipotesi che un errore non abbia allocato.

Esempi aritmetici condizionali, NON picchi misurati:

| Una syscall | Incremento proposto in byte |
|---|---:|
| fallocate 256 MiB, bound generale | 1610674176 |
| fallocate 256 MiB, ipotesi append >=170 | 270123008 |
| overwrite 1 MiB, offset non allineato peggiore | 13799424 |
| create | 94208 |
| mkdir | 151552 |
| rename | 180224 |
| unlink | 94208 |

Il bound generale supera da solo 1 GiB: non ammette quel bootstrap. Non si sceglie
quello stretto perché entra: occorre prima dimostrarne le ipotesi, congelare la
ricetta e sottoporla a review. Se il profilo non è supportato, STOP senza cambio
automatico di FS, backend, dati o profilo di bound.

Protocollo preventivo proposto: prima di OGNI syscall prenotare atomicamente
Gamma della richiesta, sommato a Gamma dell'altro writer interno e a una riserva
di finalizzazione non consumabile. Verificare PRIMA
`occupazione confermata + quota esterna + prenotazioni pendenti <=1 GiB`.
Solo allora emettere la syscall. Dopo la syscall riconciliare i metadata reali;
su errore non dichiarare restituito il budget indeterminato e fermare il ramo.
Path.mkdir(parents=True) e i loop di pwrite/fsync vanno espansi in syscall native,
non contati come una sola operazione. La finalizzazione richiede il bound della
sequenza residua, non soltanto di una pwrite isolata.

Questo controller metadata non è ancora collegato ai writer: serve review del
modello e delle feature prima di farlo. Perciò require_reviewed_profile blocca
admit PRIMA di lock/create/fallocate; PROOF_STATUS blocca ancora run/unit.
Nessun controllo du successivo viene spacciato per prenotazione preventiva,
nessuna osservazione singola viene chiamata massimo e nessun flag CLI abilita
UNPROVEN->PASS. Nessuna quota o configurazione di sistema è stata cambiata.

## Verifiche statiche e consegna

AST parse effettivo dei sette Python, confronto CASES/costanti scientifiche con
lo snapshot, conservazione dei sette nomi originali, conteggio 12 e aritmetica
da letterali AST. Le funzioni candidate, bounds.plan e preventive_increment
non sono state chiamate. Sono soltanto regressioni predisposte.
Il verificatore usa Python stdlib `-I -B -S`; nessun site hook o test runner.
Ambiente: Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.

SHA-256 relativi a `tmp/m1/d/`:

```text
83a7cbb0a0c46557a5d7e6ef8bfdcfd76d7ce77c518760719da0de160a3ef1d2  pool.py
806fa1cb2feb7681fd7ba9b9a19f48433f552d3861261737a17874c41b25352b  run.py
082d74aa9a9bee617918a215859d0e3519c580e0ff9e0d2dab8f7b703671a8d8  disk.py
05858eee29dcbb296a687f33a68adf2576904a39d5ee6a71500e4c2b3d2ffa08  admission.py
086872c3548ddf32c6437c7df038eb4c43a49dfd965a60e88c5dd69d86e1961c  bounds.py
256643a59ee1c635b85c7f2652b18df57be0bacf82accdccafa04b97c5bc7fbb  fs_bounds.py
dfa39b1541f762ce6c2d2e8a1c402a6d3edfaae50010912eae9496f00a57a75a  test_lifecycle.py
14544257aefa0c0179531b7b37952acfb86bbd4e416fd21b5d634a1bc87c6f28  README.md
a74cf8a756dd1ead2150b247017c26140dacfe39ddcda248e95b89c626ec1c3e  fs-metadata-v5-20261025-01.json
```

Manifest statico: `tmp/m1/d/admission-v5-static-20261025-01.json`.
Diff unificato non Git: `tmp/m1/d/admission-v5-diff-20261025-01.patch`.
Il nuovo hash di preparazione include questa evidence, non è un recipe hash
runtime o un certificato filesystem. Nessun report v4 viene sostituito.

## Prossimo passo minimo ammissibile

Consegna stabile per review indipendente del diff v5 e delle ipotesi Gamma.
Poi servono riscontri readonly sulle feature mancanti e il binding preventivo
delle prenotazioni per syscall, con quota aggiornata dal coordinatore. Nessuna
richiesta al proprietario per questi passi tecnici. Nessuna azione su progress.
L'occupazione unit v4 resta conservata; oggi non si ammettono né il nuovo unit
né il bounded. Gli altri rami, incluso il pilot autorizzato altrove, proseguono
senza accessi da questo autore.

Primo comando di sola verifica artefatti, non ancora eseguito alla stesura:

```bash
sha256sum tmp/m1/d/admission-v5-diff-20261025-01.patch tmp/m1/d/admission-v5-static-20261025-01.json
```

Non eseguire admit/prealloc/unit/run. Le misure formali, warm-up/campioni e
bootstrap appaiato restano NON IMPLEMENTATI; RSS non è una garanzia OOM universale.
