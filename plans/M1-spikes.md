# M1: preregistrazione degli spike

Stato: **PLANNED**. Tutte le misure e i test sotto sono **NON ESEGUITI**. Sono stati eseguiti soltanto i probe di disponibilità registrati in [M0-environment](../docs/evidence/M0-environment.md). Questa consegna non crea sorgenti né avvia benchmark.

## Avvio e prerequisiti

1. Leggere `AGENTS.md`, `PLANS.md`, [progress](../docs/progress.md), [M0-M1](M0-M1.md) e [matrice test](../docs/test-matrix.md). Procedere quando il coordinatore ha registrato la chiusura M0 e la review dei documenti stabili; non basta la presenza di questo piano.
2. Dopo quel gate, creare solo sotto `tmp/m1/` sorgenti usa e getta, fixture sintetiche e strumenti di misura. Registrare hash del piano, degli input e dei sorgenti prima delle misure: una variazione apre una nuova serie, non corregge retroattivamente i risultati.
3. Eseguire **M1.e prima**, almeno fino al verdetto documentato di fattibilità dei backend. Poi eseguire a, b, c, d, f; c/d/f possono procedere indipendentemente dal compute GPU. Una dipendenza GPU mancante isola quel candidato, non gli spike CPU.
4. Consegnare un record per unità in `docs/evidence/M1-<lettera>.md`, con dati grezzi, comandi, hash, versioni, esiti e limiti. Il gate M1 resta aperto fino alla review indipendente dei risultati e alla decisione tecnica registrata in `docs/decisions/`.

Ambito: selezione tecnica e controlli numerici analitici, non validazione degli operatori astronomici M3. Nessun catalogo reale, fotografia privata, download o codice Siril. Contratti M3 incompleti non impediscono queste prove sintetiche, ma impediscono di chiamarle validazione M3. Licenza del prodotto e distribuzione restano fuori dall'esperimento.

## Candidati e assenze verificate

- Core: C++ con GCC 15.2, Rust 1.99 diretto senza Cargo o dipendenze scaricate, Python 3.14 con NumPy 2.3.5/SciPy 1.16.3. La compilabilità non è ancora provata.
- GUI: Qt6 Widgets e Quick/QML C++ 6.10.2. GTK4 4.22.4 è presente come runtime e introspezione con PyGObject 3.56.2; candidato Python GI condizionato a import e renderer verificati. GTK4 C/C++ escluso dall'attuale build: header e `gtk4.pc` assenti. I launcher `qml`/`qmlscene` sono qtchooser, non runner Qt6 provati: QML si proverà mediante host C++ Qt6.
- PySide assente. PyQt6 presente ma GPL-3.0-only: non viene usato come sostituto implicito del binding mancante. C#/.NET e quindi Avalonia non disponibili per questo confronto; nessun numero attribuito a questi candidati.
- GPU: Vulkan loader/header 1.4.341 presenti, compilatore shader e validatore SPIR-V assenti. OpenCL solo loader, senza header/ICD rilevati. GL/EGL hanno header e strumenti, ma versione del dispositivo, compute e natura hardware non sono ancora verificati. Nessun backend compute pronto dichiarato.
- Persistenza/hash: SQLite 3.46.1, OpenSSL 3.5.5 e hashlib disponibili; BLAKE3 non rilevato. Non si simula un confronto SHA-256/BLAKE3.

Per licenze e provenienza consultare il record M0, non dedurre un'approvazione dai soli nomi: Qt ha moduli con condizioni diverse; FFTW è GPL-2+ e rimane solo confronto locale isolato; la mappa Rust e le dipendenze native Python sono incomplete. L'adozione nel prodotto richiede verifica dei componenti effettivi e dei termini, senza emettere ora un `LICENSE`.

## Protocollo comune

Prima dei futuri comandi configurare l'ambiente nella radice:

```bash
export ROOT=/opt/workspace/astroProcessor
export TMPDIR="$ROOT/tmp" PYTHONDONTWRITEBYTECODE=1 PYTHONNOUSERSITE=1
export XDG_CACHE_HOME="$ROOT/tmp/cache" XDG_CONFIG_HOME="$ROOT/tmp/config"
export CARGO_HOME="$ROOT/tmp/cargo" CARGO_TARGET_DIR="$ROOT/tmp/m1/build/rust"
export MESA_SHADER_CACHE_DIR="$ROOT/tmp/cache/mesa" MESA_SHADER_CACHE_DISABLE=true
export __GL_SHADER_DISK_CACHE=0 __GL_SHADER_DISK_CACHE_PATH="$ROOT/tmp/cache/nvidia"
export OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 MKL_NUM_THREADS=1 NUMEXPR_NUM_THREADS=1
export OPENSSL_CONF=/dev/null
```

Le directory verranno create dopo M0. Conservare le variabili della sessione grafica senza stamparne valori o leggere profili personali. Non usare drop_caches, installazioni, servizi o configurazioni globali. Dopo M0 verificare il filesystem di `tmp/` e lo spazio disponibile prima delle prove di recovery.

- Limiti di sicurezza della serie iniziale: un candidato alla volta; buffer pianificati entro 256 MiB, scratch fisicamente allocato entro 1 GiB. Il supervisore misura RSS e arresta il proprio figlio oltre 512 MiB osservati; questo controllo è best effort, non garanzia anti-OOM. Se il preflight non può rispettare i limiti, il caso è NOT RUN con causa; non cambia silenziosamente dimensioni o precisione.
- Tre avvii di processo separati per il costo di startup; due warm-up e sette campioni warm per caso. Ordine round-robin dei candidati. Niente altre misure concorrenti. `cold` significa processo nuovo, non cache OS, driver o storage svuotata.
- Registrare wall time monotono, CPU time, picco RSS, byte allocati/copiati/letti/scritti, numero thread, versione/flag, dtype, layout e identificatore del backend. `T_total` va dal preflight del candidato con input host pronto all'output host sincronizzato; comprende allocazione, copie e attese. Separare tempo di startup, caricamento fixture, kernel e confronto GT; riportare anche il tempo esterno dell'intero processo.
- Riportare campioni, mediana, MAD, minimo/massimo. Il rapporto dei tempi è descrittivo; per confronti warm usare bootstrap appaiato, 2000 ricampionamenti, seed 4059, intervallo percentile 95%, dichiarando il limite di sette campioni. Nessuna soglia assoluta di latenza diventa requisito del prodotto.
- Correttezza prima delle misure: un errore numerico o un percorso sostitutivo invalida la serie. Errori attesi sono testati separatamente e contano come riusciti solo con codice non nullo, diagnosi, nessun output pubblicato e contatori degli altri backend a zero. L'harness non ritenta con un altro candidato.

### Fixture condivise e tolleranze

Etichette locali `SYN-*`, non nuovi requisiti o test approvati della matrice. Manifest: formula, shape, dtype, endian, stride, maschera, byte size, hash e oracle. Nessuna fixture esiste ancora.

`SYN-DYADIC`: `x[i] = (((73*i + 19) mod 1024) - 512) / 1024`, indice assoluto row-major. Shape 17×19 per oracle scalare, 257×263 per bordi/stride, 1024×1024 per misura; float32 e float64 in casi distinti. Layout contiguo e riga con 17 elementi di padding. La generazione per indice evita differenze tra RNG dei linguaggi.

GT indipendente: interi/rational esatti, con oracle scalare `fractions.Fraction` sulla shape piccola e formula intera sul caso grande; nessuna uscita candidata è usata come verità. Le operazioni dyadic sotto restano rappresentabili esattamente nei due dtype, quindi tolleranza assoluta **zero** sui valori finiti. Segno dello zero, maschere, NaN e subnormali hanno fixture e asserzioni proprie.

Per il filtro non dyadic, fissare `h = [1,2,4,2,1]/10`, kernel 2D prodotto esterno, somma diretta di m=25 termini, niente fast-math/FMA contratto. Con `u=2^-24` o `2^-53`, `gamma_n=n*u/(1-n*u)`, il limite assoluto è:

`B = gamma_(2m) * sum_j abs(x_j * hhat_j) + sum_j abs(x_j) * abs(hhat_j - h_j)`.

`hhat` è il coefficiente effettivamente rappresentato, `h` il rational esatto. Calcolare B in rational e arrotondarlo verso l'alto prima del confronto; niente errore relativo vicino a zero. Il limite deriva dal conteggio prodotti/somme e dall'errore dei coefficienti, non dai residui osservati. Vale per somma sequenziale, arrotondamento nearest, nessun overflow/underflow; un'implementazione con grafo diverso richiede un limite derivato e registrato prima di misurarla.

## M1.e: GPU, prima unità

**Domanda.** Un backend hardware esplicito esegue affine/convoluzione con correttezza controllata e riduce il costo totale rispetto alla CPU allo stesso dtype? Il costo di startup annulla il beneficio per un'immagine o per batch di otto?

**Preflight.** Dopo M0 usare `vulkaninfo --summary` e `glxinfo -B` soltanto per capacità, con l'ambiente sopra. Registrare API, device type, driver, ID dell'adattatore, memoria/limiti, dtype e tool shader. La presenza di `lvp` nei manifest non prova una GPU: un device CPU/software non può qualificare il caso hardware. Niente `vkcube` come benchmark compute. Per OpenGL richiedere compute 4.3 o capacità equivalente esplicita, identità del renderer e possibilità di impedire la sostituzione con software; altrimenti quel candidato resta escluso.

Vulkan compute è attualmente **BLOCKED per tool shader mancanti**; OpenCL per header/ICD mancanti. Non generare SPIR-V a mano e non scaricare shader precompilati per aggirare il blocco. OpenGL compute è un esperimento distinto, dichiarato prima della sua esecuzione, non un ripiego dopo errore Vulkan. Se nessun backend è utilizzabile, consegnare il verdetto di fattibilità senza inventare speedup e proseguire con i rami CPU.

**Dati/GT.** `SYN-DYADIC`, shape 257×263 e 1024×1024, float32; batch 1 e 8 eseguiti sequenzialmente, non tutti residenti. Affine `y=3*x/4+1/8`; convoluzione 5×5 con `h=[1,4,6,4,1]/16`, zero padding esplicito. GT rational, errore zero su questi dati esatti. Copia host-device-host deve preservare bit, inclusi ±0; subnormali e non finiti sono test separati, senza restringere il dominio dopo un fallimento. Float64 è un caso futuro distinto se supportato, non conversione automatica in float32.

**Metriche/gate.** Tempi di compilazione shader, allocazione, trasferimenti, kernel, sincronizzazione e T_total; byte host/VRAM; rispetto dei bordi non multipli del workgroup. Speedup riferito alla migliore CPU corretta dello stesso caso, misurata poi in M1.a; M1.e resta aperta sul confronto fino a quel risultato. Beneficio warm dimostrato solo se l'intervallo preregistrato del rapporto CPU/GPU è interamente sopra 1; startup e batch rimangono risultati separati. Assenza di misura non dimostra assenza di beneficio: giustifica il rinvio, non l'adozione.

**Guasti.** ID adattatore inesistente, allocazione richiesta oltre il budget dichiarato, errore di compilazione shader e fallimento di una submission nell'adapter del prototipo. Verificare stop e zero chiamate alla CPU o ad altro device. L'iniezione software non prova il distacco fisico del dispositivo: quella prova resta assente, non PASS. Una libreria con fallback non impedibile è esclusa.

**Artefatti.** `tmp/m1/e/{capabilities.json,samples.jsonl,errors.json,manifest.json}`. Uscita: backend candidato con limiti oppure rinvio motivato; nessuna abilitazione GPU di prodotto.

## M1.a: core e kernel

**Domanda.** Quali costi numerici, di memoria, build e interoperabilità hanno C++, Rust e NumPy nativo sugli stessi dati?

**Dati/GT.** `SYN-DYADIC`; affine e convoluzione esatta di M1.e; filtro non dyadic con limite B sopra; warp bilineare a offset `(1/4,3/4)`, zero padding e pesi rational, più identità e traslazione intera. Output e maschere attesi da indice/rational, senza fit o registrazione astronomica.

**FFT.** Fixture 32×32 e 1024×1024: impulso all'origine, costante 1 e modo di periodo quattro `[1,0,-1,0]`. GT spettrale sparso esatto, convenzione forward non normalizzata e inverse con divisione per N totale; confronto completo dei bin, tolleranza zero su questi soli casi dyadic. FFTW per C++ e Rust via FFI locale, NumPy FFT come implementazione distinta. Nessuna implementazione Rust FFT esterna installata. La precisione su spettri generali resta NON VALIDATA: questi casi non chiudono un contratto FFT scientifico. Un residuo non nullo apre un'analisi, non una tolleranza tarata sull'output.

**Errori/metriche.** Dimensioni incoerenti, maschera errata, NaN e buffer fuori budget devono essere rifiutati prima del kernel; nessuna sanitizzazione implicita. Misurare T_total/kernel, RSS, copie, thread, determinismo dei risultati, errori assoluti/ULP diagnostici, warning e costo di build. Confrontare ownership C++/Rust e accesso Python a buffer nativo con un solo proprietario e rilascio conteggiato; copia necessaria dichiarata e misurata. Sanitizer C++ separati dalla serie ottimizzata; nessuna copertura sanitizer Rust stable presunta.

**Gate/artefatti.** Tutti i casi dichiarati corretti e limiti rispettati, build riproducibile senza rete, costi FFI e licenze registrati. `tmp/m1/a/{samples.jsonl,errors.json,ownership.json,manifest.json,build.log}`. Nessuna adozione scientifica M3.

## M1.b: viewer e GUI

**Domanda.** Quale toolkit gestisce immagini logiche grandi con buffer bounded, trasformazioni corrette e backend/display espliciti?

**Dati/GT.** `SYN-VIEW`: immagine procedurale 16384×16384 RGB float32, griglia/colori rational con coordinate note; tile 256×256, nessuna immagine completa da 3 GiB allocata. Viewport 1024×768; 100 passi di pan e 100 zoom alternati, scale 1 e 2. Cache tile entro 128 MiB; contare payload e risorse native separatamente. GT di selezione tile, coordinate e valori prima della conversione colore esatto. Errore di picking limitato a mezzo pixel fisico per il solo arrotondamento alla griglia, non tolleranza sui dati scientifici.

Provare Qt Widgets CPU e QML con backend software esplicito come ricette distinte; renderer differente genera un nuovo caso, non un retry. GTK GI entra solo dopo verifica di import, renderer selezionabile e assenza di fallback: la libreria presente non prova queste proprietà. X11 nativo e Wayland sono casi separati; la presenza delle due variabili non prova due sessioni native. Backend indisponibile: NOT RUN, non esecuzione tramite XWayland chiamata X11 nativo.

**Metriche/errori.** Timestamp di input e frame presentato, mediana/p95 dei 100 eventi, dropped frame, RSS, tile caricati/evinti, copie core/viewer, ownership, HiDPI, tastiera/focus e testo IME mediante eventi locali del prototipo, non input alla sessione dell'utente. Profilo ICC sintetico valido e profilo corrotto per testare il preflight; nessun profilo personale letto. Profilo/display non verificato significa nessuna conclusione colorimetrica. ICC corrotto e tile mancante devono produrre errore e nessuna immagine sostitutiva. Una cattura offscreen non prova latenza di presentazione, accessibilità o resa del display.

**Gate/artefatti.** Correttezza di viewport/cache e ownership, risultati per display realmente provato, assenze e costi registrati; nessuna soglia assoluta di fluidità inventata. `tmp/m1/b/{events.jsonl,tiles.json,ownership.json,capabilities.json,manifest.json}`; screenshot solo della finestra sintetica, se necessari e autorizzati nella successiva esecuzione.

## M1.c: persistenza e crash

**Domanda.** SQLite e manifest JSON atomico mantengono la stessa semantica di commit, recovery, concorrenza e schema sconosciuto?

**Dati/GT.** `SYN-STATE`: 256 record da circa 1 KiB, ID interi, ricette, parent hash e payload ASCII deterministico; generazioni 1 e 2 con checksum logico noto. JSON con ordinamento chiavi dichiarato, UTF-8 stretto, divieto di NaN e chiavi duplicate. SQLite WAL con `synchronous=FULL`, impostazioni effettive registrate. JSON: temporaneo nello stesso filesystem, fsync file, rename, fsync directory, writer lock esplicito. Confrontare contenuti logici, non byte diversi fra formati.

**Guasti/GT.** Per ogni candidato, tre ripetizioni ai sei punti: dopo scrittura, dopo sync del file/journal, prima del commit/rename, dopo commit/rename, prima dell'ack di durabilità e dopo ack; handshake figlio/supervisore, SIGKILL soltanto al figlio creato. Mappare i punti sulle primitive reali: non attribuire al writer SQLite un fsync directory controllato dal test se lo fa internamente. Prima dell'ack sono validi solo uno stato precedente o nuovo interamente coerente; dopo ack deve riapparire il nuovo. Zero stati parziali/orfani pubblicati. SIGKILL non simula perdita di alimentazione: power-loss resta NON ESEGUITO.

Due writer concorrenti devono serializzare o ricevere conflitto esplicito, senza lost update. Testare file troncato, percorso scratch non scrivibile, spazio prenotabile insufficiente simulato nel writer, migrazione riuscita/fallita e schema futuro letto senza mutare file/sidecar, verificati via hash. Non riempire il filesystem per simulare ENOSPC e non convertire un JSON guasto in SQLite.

**Metriche/gate.** Recovery esatta per tutti i punti, numero byte e fsync, tempo commit/riapertura, RSS, orfani diagnosticati, complessità di lock/schema. La tolleranza logica è zero. `tmp/m1/c/{cases.jsonl,recovery.json,manifest.json}` più database/manifest per caso entro il limite scratch. Scelta motivata solo per le proprietà effettivamente provate.

## M1.d: pool, scheduler e spill

**Domanda.** Prenotazioni e spill mantengono un hard cap del pool e risultati deterministici sotto concorrenza e rifiuto?

**Dati/GT.** `SYN-STREAM`: formula per indice di SYN-DYADIC, 40 GiB logici generati per tile, superiori alla RAM totale fotografata nell'audit, mai materializzati interamente. Pool B=32 MiB, tile da 2 MiB, uno/due/quattro worker; ogni worker prenota input, output e scratch, tre buffer da 2 MiB. Anche gli altri buffer gestiti sono prenotati; quattro worker lasciano 8 MiB del pool per queste riserve. GT del checksum/somma da periodicità e conteggio intero, verificato con Fraction sul prefisso piccolo. Un caso distinto da 256 MiB produce spill reale dentro `tmp/`; massimo scratch fisico comune 1 GiB. Il caso da 40 GiB prova generazione/streaming logici, non lettura di un file fisico da 40 GiB o prestazioni disco oltre RAM.

**Errori.** Job la cui riserva supera B, due job ammessi individualmente ma non insieme, tile con halo contato, cancellazione durante spill, scratch non disponibile, guasto del worker. Rifiutare prima di iniziare il job incompatibile; nessuna riduzione di tile o worker dopo un errore. Verificare restituzione delle riserve una volta sola, zero deadlock e zero cambio di backend. Tempi massimi di attesa servono solo da guardia del supervisore, dichiarata nel record, non come SLA del prodotto.

**Metriche/gate.** `reserved_bytes <= B` in ogni evento; saldo finale zero; GT identico per ordine di completamento; throughput, RSS di baseline/picco, byte spill, tempi di attesa e contatori di cancellazione. RSS è best effort, distinto dal limite esatto del pool; nessuna promessa di OOM universale. `tmp/m1/d/{reservations.jsonl,samples.jsonl,faults.json,manifest.json}`. Se un caso supera la guardia è FAIL/NOT RUN con causa, mai PASS per riduzione implicita del dataset.

## M1.f: hash e provenance

**Domanda.** SHA-256 streaming è riproducibile e quale costo aggiunge a import, lettura e verifica di mutazioni?

**Dati/GT.** `SYN-HASH`: 0, 3 (`abc`), 4097 byte, 16 e 128 MiB, pattern byte da indice; manifest prima di ogni prova. Vettori esatti: empty `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`; `abc` `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`. Confrontare hashlib e OpenSSL, senza presumere indipendenza se hashlib usa lo stesso backend; registrare provider effettivo e mantenere i vettori come oracle esterno al confronto. Nessun risultato BLAKE3.

**Casi/errori.** Chunk 1 byte solo sui piccoli vettori, 1 e 8 MiB sui grandi; digest identici, tolleranza zero. Copia/import sotto `tmp/` con hash sorgente invariato; mutation di un byte soltanto nella fixture scratch deve essere rilevata. Testare troncamento, dimensione dichiarata errata e metadati mtime/size non sufficienti a provare l'identità; nessuna cache basata solo su mtime. Gli originali di progetto si hashan soltanto, non si mutano per il test. Specificare separatamente digest contenuto, ricetta canonica e file sorgente: stesso pixel buffer non implica stesso originale.

**Metriche/gate.** MB/s, T_total, costo rispetto a lettura senza hash, CPU, RSS, chunk size, byte effettivi e risultati cold-process/warm; mai chiamare fredda la page cache non controllata. Zero mutazioni degli originali e zero accettazioni di fixture corrotta. `tmp/m1/f/{digests.json,samples.jsonl,mutations.json,manifest.json}`. Uscita: strategia SHA-256 misurata o rischio documentato, non scelta di un algoritmo assente.

## Comandi pianificati e artefatti

I percorsi e le opzioni sotto sono **interfacce del prototipo da creare dopo M0**, non API già presenti. Nessuno di questi comandi è stato eseguito. L'harness dovrà supportare `--unit`, `--phase`, `--backend`, `--output`; gli errori di capacità non devono lanciare un backend diverso. Prima registrare la build e i controlli numerici, poi attivare la fase measure della stessa ricetta.

`RUST_TOOLCHAIN_DIR` identifica la directory della toolchain esistente, risolta tramite soli metadati tecnici e verificata prima della build. Non si pubblica il percorso personale e non si usa lo shim rustup per avviare installazioni implicite.

```bash
# Build solo dopo chiusura M0 e creazione dei sorgenti isolati.
g++ -std=c++20 -O3 -fno-fast-math -ffp-contract=off tmp/m1/src/core.cpp $(pkg-config --cflags --libs fftw3 fftw3f) -o tmp/m1/build/core-cpp
"$RUST_TOOLCHAIN_DIR/bin/rustc" --edition=2024 -C opt-level=3 -C target-cpu=x86-64 -l fftw3 -l fftw3f tmp/m1/src/core.rs -o tmp/m1/build/core-rust
cmake -S tmp/m1/src/gui -B tmp/m1/build/gui -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build tmp/m1/build/gui --parallel 2

# Primo lavoro eseguibile del pacchetto: fattibilità GPU, senza misura implicita.
/usr/bin/python3 -B tmp/m1/src/run.py --unit e --phase preflight --backend vulkan --output tmp/m1/e/vulkan
# OpenGL è un altro caso esplicito; non parte come retry del precedente.
/usr/bin/python3 -B tmp/m1/src/run.py --unit e --phase preflight --backend opengl --output tmp/m1/e/opengl

# Ogni comando esegue il proprio insieme preregistrato, validazione prima di misura.
/usr/bin/python3 -B tmp/m1/src/run.py --unit a --phase validate --output tmp/m1/a
/usr/bin/python3 -B tmp/m1/src/run.py --unit a --phase measure --output tmp/m1/a
QT_QPA_PLATFORM=wayland /usr/bin/python3 -B tmp/m1/src/run.py --unit b --phase validate --backend qtwidgets-cpu --output tmp/m1/b/widgets-wayland
QT_QPA_PLATFORM=xcb /usr/bin/python3 -B tmp/m1/src/run.py --unit b --phase validate --backend qtwidgets-cpu --output tmp/m1/b/widgets-x11
/usr/bin/python3 -B tmp/m1/src/run.py --unit c --phase validate --output tmp/m1/c
/usr/bin/python3 -B tmp/m1/src/run.py --unit d --phase validate --output tmp/m1/d
/usr/bin/python3 -B tmp/m1/src/run.py --unit f --phase validate --output tmp/m1/f
```

Per e, b, c, d, f la fase measure si invoca solo dopo il corrispondente validate riuscito, ripetendo backend/output e tutti i parametri congelati. Il backend QML-software e GTK-GI avranno comandi separati con selezione del renderer verificata prima di misurare; non sono già coperti dai due comandi Widgets. Ogni manifest deve riportare anche i casi previsti ma NOT RUN, con causa, anziché eliminarli dalla tabella.

## Uscita, rischi e ripresa

Una raccomandazione confronta correttezza, costo totale, RAM, ownership, build/debug, licenze e disponibilità. Candidati mancanti restano assenti; differenze statisticamente non risolte restano tali. Non si sceglie un vincitore su medie inventate o sola familiarità. Nessuno spike dimostra compatibilità Linux L1/L2/L3 senza eseguirlo su quelle macchine/sessioni.

Rischi che invalidano una serie: revisione del protocollo senza nuovo hash, dtype cambiato, backend sostituito, cache non descritta, driver software chiamato GPU, GT derivato dal candidato, differenza di workload fra tempi confrontati. Correggere e aprire una nuova serie mantenendo quella fallita. Per rollback interrompere solo i processi figli dello spike e conservare log/fixture; nessun reset Git o cancellazione di file preesistenti. Il codice usa e getta non viene integrato nel prodotto.

**Prossimo comando verificabile adesso:** `git symbolic-ref --short HEAD` deve riportare `work/m0-m1`; leggere poi l'evento di chiusura M0 in `docs/progress.md`. Se l'evento manca, questo piano resta PLANNED. Dopo quell'evento il prossimo incarico crea il prototipo isolato e parte dal preflight e, senza attendere altre approvazioni rituali.
