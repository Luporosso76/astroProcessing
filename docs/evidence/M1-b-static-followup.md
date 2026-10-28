# M1.b: follow-up statico della review

Consegna: **CORREZIONI PREDISPOSTE, SOLO VERIFICHE STATICHE**. Nessuna build,
compilazione, import/esecuzione di candidato, CDLL, GUI, oracle, regressione o
benchmark effettuata. Il pilot CPU Vulkan resta nell'altro namespace; questo
record non ne legge fonti o risultati e non ne occupa l'ammissione.

Il coordinatore ha comunicato la conclusione della review indipendente del
coder `ses_e8c14fabaffeCEsreEF1hHtERa`. Questo delta risponde ai cinque finding
ricevuti; non è una nuova review indipendente né una validazione dinamica.
Modello dichiarato dal runtime dell'autore: `openai/gpt-6.1-sol`.

## Snapshot precedente alle modifiche

Creato prima della prima patch:
`tmp/m1/b-viewer/snapshots/before-static-followup-003/sources.zip`.
SHA-256: `b5c7b665336ea8f1040f424ace42f7dcce678a26fa169c65d90670d6300fcbd6`.

Il ZIP contiene 18 file, 115807 byte logici: le 14 fonti/documenti originali,
`source-hashes-002.json`, `preparation-checks-002.json`, la preregistrazione
precedente e `docs/evidence/M1-b-preparation.md` di 161 righe. I byte delle 14
fonti e della preregistrazione sono stati confrontati con il manifest002 prima
di modificarli. `receipt.json` accanto al ZIP contiene SHA, dimensione e righe
per ciascun membro. Le date interne ZIP sono fisse per riproducibilità, non
attestazioni del tempo di creazione delle fonti.

Manifest002, check002 e report161 restano **byte-identici** al contenuto
archiviato. Nessuna riscrittura retroattiva. Il piano di 4482 righe, progress,
indici e namespace F/D/GPU non sono stati letti o modificati. Il riferimento
al protocollo padre è riportato dal manifest002, non ricontrollato aprendo
quel piano in questo follow-up.

## Cinque correzioni nel codice

### 1. Oracle realmente affidato a un supervisore

`run.py` aggiunge `--phase oracle`. L'unico comando documentato passa da quella
fase, non da un CDLL Python diretto. Il supervisore crea il proprio Popen;
HOME/cache/config/TMP sono privati nel run. Il figlio richiede directory e
manifest capability ereditati, poi emette `oracle_ready` e aspetta il rilascio
sulla pipe prima di test o caricamento del core.

stdin/stdout del supervisore sono non bloccanti; il receipt ne registra lo
stato interrogato. Deadline oracle 600 s, RSS 512 MiB best effort, AS 768 MiB,
file 32 MiB, log 8 MiB; cap scratch 1 GiB invariato. EOF, handshake rotto,
timeout, spawn fallito e exit non nullo attraversano il finally di raccolta,
chiusura pipe/log e receipt. I byte letti e quelli effettivamente scritti nel
log sono distinti. Non si dichiara un child inesistente se Popen non restituisce
un handle dopo un errore di startup.

Il gruppo terminabile è solo la nuova sessione del proprio Popen; il leader
resta non reaped con WNOWAIT fino al cleanup, evitando riuso del PID. Su Linux
il pre-exec imposta PR_SET_PDEATHSIG e verifica il parent. L'uso di libc già
nel processo per questa API OS non è un loader alternativo del candidato.
Rimangono limiti OS reali: guasto del filesystem può impedire il receipt;
processi non interrompibili o morte del supervisore non hanno una garanzia
universale di finally. Nessuno di questi casi è trasformato in PASS.

### 2. Pin dell'oggetto caricato da ctypes

`session.SealedFile` apre fd-relative con NOFOLLOW e richiede file regolare,
un link, dimensione entro 8 MiB, identità/mtime/ctime/nlink stabili durante la
copia e SHA atteso dal build/supervisore. Copia in memfd anonimo Linux,
applica i seal write/grow/shrink/seal e verifica nuovamente i byte sigillati.
`native.py` passa a CDLL **`/proc/self/fd/N` di quel medesimo oggetto**, non il
pathname del build dopo averlo chiuso. Non usa realpath come soluzione al race.

Fd e CDLL restano associati per l'intera vita del processo: una sola versione
SHA ammessa evita anche il riuso del nome fd nella cache di dlopen. Ogni
Native.close libera subito engine/cache; il pin bounded viene chiuso all'uscita.
Linux memfd/seals/procfs e mapping eseguibile sono prerequisiti, non percorsi
da sostituire dopo un errore. La copia è RAM anonima bounded, non un temporaneo
su filesystem esterno. Dipendenze di sistema e loader completi non sono tutti
pinnati: nessuna promessa di sandbox OS o supply-chain audit.

### 3. Capability directory per ICC ed export

`run_io.cpp/.hpp` usa un fd di directory attendibile ricevuto dal supervisore,
duplicato CLOEXEC, con owner/mode, parent PID, device/inode/fase verificati.
ICC è il solo nome `synthetic.icc` aperto con openat/NOFOLLOW; tipo, nlink,
dimensione e SHA della fixture nota sono verificati prima di QColorSpace.
Gli argomenti testuali non sono riaperti come percorsi e non abilitano ICC utente.

Export Qt: temporaneo esclusivo regolare/un link, write-all, fsync file,
renameat2 NOREPLACE, fsync directory, riapertura relativa allo stesso fd e
verifica di identità/dimensione/byte prima dell'ACK. ENOSYS/EEXIST sono STOP,
senza overwrite o metodo alternativo. Temporanei e output falliti si conservano.
GTK/Python usa anch'esso l'autorità fd: linkat/unlinkat no-overwrite è un metodo
primario diverso e dichiarato, non equivalenza I/O con renameat2.

Una coppia BGRA/JSON da sola non è validazione o durabilità confermata: servono
ACK riuscito e receipt del processo coerente. Coppie parziali, temporanei o
residui con link extra non vengono riparati e non sono un export valido.

### 4. Rimossa la falsa osservazione di fallback

Nessun `fallback_calls=0` nel codice attivo. Policy:
`FORBIDDEN_EXPECTATION`; osservazione interna: `NOT_INSTRUMENTED`; gate:
**INCOMPLETE**. I receipt registrano i Popen restituiti realmente e il loro argv;
le GUI dichiarano classe/engine osservati, non un contatore interno inventato.
Adapter fisico non interrogato e compositore fuori controllo sono espliciti.

Non è stato scritto o eseguito un mutante di dispatch interno: si adotta il
ramo richiesto dalla review che etichetta aspettative e limiti, non una falsa
misura. Nessuna selezione di un altro backend dopo errore. Il gate antifallback
resta aperto, anche se la classe renderer attesa viene osservata in futuro.

### 5. Ownership del worker GTK protetta

Il costruttore Viewer non apre più il core né avvia un thread. `run_managed`
protegge start_worker/app.run con finally; `release_worker` imposta closed,
cancella, notifica, fa join prima del release, anche su eccezione di cancel.
Le callback Python segnalano fallimento e fanno uscire app.run verso quel
finally, invece di usare os._exit per saltarlo. Anche timer e buffer display
vengono rilasciati nel percorso di close.

Regressioni senza GI predisposte per startup, app.run, cancel e successo.
Sono test di ownership con oggetti finti dichiarati, non una prova del runtime
GTK. GTK resta **NON AMMESSO** senza dimostrazione del pin rendering/no-retry;
nessun binding sostitutivo, nessun benchmark Python chiamato Rust nativo.

## Oracle completato nel sorgente, prima dei dati

I vecchi punti, anchor STF, tre snapshot iniziali, stale generation e missing
tile restano. `pixel`, `quantized` e `snapshot` hanno lo stesso AST precedente;
tutte le tuple numeriche dei casi precedenti sono ancora presenti.

Aggiunti 18 snapshot exact: denominatore zoom 4/8/16 × DPR1/2 × frame 0/1/1999,
con varianti STF/mask/split. Cache: 168 tile distinti contro 146 slot, conteggi
di eviction e revisit con confronto completo contro oracle indipendente.
Cancellazione in-flight: barriera diagnostica dopo quattro righe effettive,
richiesta intermedia superata, risultato obsoleto assente e ultima generazione
confrontata byte per byte. La barriera è disabilitata nelle GUI.

ABI **2** espone query reali C++ per sizeof, alignof, offsetof e layout/stride;
il binding le confronta con ctypes prima dell'engine. Controlla anche dimensioni,
pixel bytes, puntatore e inventario tile prima di copiare un frame. Sono
verifiche runtime future, non risultati ottenuti dal parsing AST.

Le 11 regressioni future comprendono handshake/nonblocking/reaping, timeout,
broken pipe, exit non nullo, spawn e RSS simulati etichettati, pin con nome
sostituito/seal, capability contro parent rinominato/symlink, export duplicato,
ICC symlink/hardlink/corrotto e cleanup del worker. Il probe I/O C++ usa il
binario Qt prima di QApplication: non crea una GUI, ma richiede comunque nuova
ammissione come tutto l'oracle. **Tutti questi casi sono NON ESEGUITI.**

I corpi C++ del generatore pixel, quantizzazione STF e cache-get sono verificati
byte-identici al prima. Restano immagine virtuale 16384², pool 124387328 B sotto
128 MiB, viewport 1024×768, 2000 righe virtuali, 100 pan/100 zoom, dati float32
immutabili e invalidità/maschera/alpha distinti. Nessuna tolleranza allentata.

## Controlli realmente svolti e artefatti

Eseguiti: **8 AST Python SYNTAX_OK**, parsing delle stringhe dei child predisposti,
XML QRC e **2 blocchi bash validi** con `bash --noprofile --norc -n`.
C++/CMake/QML non compilati né eseguiti. Nessun modulo dello spike importato.
Runtime dei controlli: Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`;
non è una nuova misura/version qualification del compilatore C++.

Letti per hash/AST: 18 file di fonti/documentazione e la sola preregistrazione
propria di 167 righe. Conteggi individuali in `preparation-checks-003.json`.
Scope fisico delle sole 18 fonti: 184320 B. La quota globale non è ricampionata
per non leggere i namespace attivi; nessuna ammissione di build deriva da questo
numero. Verificata solo disponibilità filesystem per i piccoli artefatti statici.

SHA-256, percorsi sotto `tmp/m1/b-viewer/`:

- `source-hashes-003.json`: `47c2bade1f71369d582891a058fa344b6eb992962e9fa7742763bb833ecca041`.
- `preparation-checks-003.json`: `6f12c74fc75c37533464fc0b3566358087f0e6debe1fa7de43c84970700509ad`.
- `source-diff-003.patch`: `47368b422a8e0461a058e2e6e43663969695a1d73b7fc9bd98632b26269fdd36`.

Il diff unificato di 88942 B riguarda fonti e preregistrazione, non questo record
auto-referenziale. Il manifest contiene ogni hash corrente; conserva il padre
solo come riferimento ereditato dal002. Snapshot, manifest002 e report161 sono
conservati, non rinominati come risultati nuovi. Le cinque correzioni richiedono
ancora review del diff effettivo e verifica dinamica.

## Primo passo futuro ammissibile e limiti aperti

Dopo nuova ammissione esclusiva del coordinatore: build ABI 2 in `build-003`,
poi **fase oracle supervisionata** in `oracle-003`. Comandi esatti nel README;
nessun comando va eseguito ora, nessun voto owner richiesto.

ICC è una fixture nota, non un parser generico o calibrazione del monitor.
Font, IME, accessibilità, picking subpixel e HiDPI GUI restano non validati.
Frame offerti, callback e scanout non sono equivalenti; cache e RSS non sono
equivalenti. `xcb` non prova X11 nativo rispetto a XWayland; connessione Wayland
non prova esposizione/presentazione nativa. Il compositore resta esterno.
Supervisore comparativo 3 processi/2 warm-up/7 round e bootstrap appaiato
2000/4059 ancora **NON IMPLEMENTATI**. Nessun FPS, PASS GUI o misura simulata.
