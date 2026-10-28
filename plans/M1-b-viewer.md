# M1.b: viewer sintetico, preparazione isolata

Stato: **IN PROGRESS, PREPARAZIONE SOLTANTO**. Nessun processo GUI, import
GTK/GL, compilazione o benchmark è autorizzato da questa consegna. Il mandato
specifico del coordinatore prevale sulla pianificazione generale delle misure.
Il protocollo padre resta immutato: `plans/M1-spikes.md`, 169 righe, SHA-256
`1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.

## Ambito e prerequisiti

Sorgenti usa e getta soltanto sotto `tmp/m1/b-viewer/`; nessuna UI di prodotto.
Prerequisiti futuri: review indipendente dei sorgenti, ammissione esclusiva del
coordinatore, quota globale misurata entro 1 GiB, build offline ammessa e renderer
selezionabile senza sostituzioni. Le altre validazioni M1.a restano indipendenti.
Non modificare C, manutenzione storage, vecchie fixture/report o progress.

Ricette distinte, tutte CPU applicative:

1. `qtwidgets-raster`: C++ Qt6 Widgets, `AA_ForceRasterWidgets`, widget e QImage
   con paint engine Raster verificato. Nessun QOpenGLWidget.
2. `qml-software`: host C++ Qt6, `QQuickWindow::setSceneGraphBackend("software")`
   prima dell'applicazione, render loop `basic`, PaintedItem/Image e API Software
   verificate. Nessuna selezione automatica GL e nessun retry.
3. `gtk4-gi-cairo`: Python/PyGObject GI, non Rust o GTK C/C++. Richiede Gsk
   CairoRenderer e API usate realmente disponibili. `GSK_RENDERER=cairo`, GPU
   disabilitate tramite GDK, warning/critical fatali, classe effettiva del
   renderer nativo verificata prima della prima immagine. L'ambiente da solo
   non costituisce una prova contro i retry interni GTK: se la review e il
   successivo gate non escludono sostituzioni, candidato **NOTSUPPORTED**, senza
   timer o verdetto di lentezza. Nessun binding alternativo viene importato.
   Il launcher richiede una prova indipendente sotto `admission/`, legata allo
   SHA del runtime GTK. La preparazione non crea una prova positiva fittizia.

Compositing del desktop e scanout sono fuori dal controllo diretto. Nessuna
GPU fisica è attribuita a questi renderer CPU. QPA xcb, connessione Wayland e
tipo di sessione sono registrati separatamente: xcb non prova X11 nativo, né
una variabile d'ambiente prova l'assenza di XWayland. Niente confronto fra
sessioni diversamente qualificate.

## Contratto sintetico e STF fissato prima dell'esecuzione

Immagine logica 16384×16384 RGB float32, 2000 frame procedurali; tile 256×256.
Per coordinate base intere interne `(x,y)` e frame `f`:

`R=((x+17*f) mod 1024)/1024`, `G=((y+29*f) mod 1024)/1024`,
`B=((x xor y xor f) mod 1024)/1024`.

Sulle linee `x mod 256 == 0` o `y mod 256 == 0` il valore è `(1,1/2,1/4)`.
Se `(17*x+29*y+f) mod 4093 == 0`, stato INVALID e tre NaN float32 canonici
`0x7fc00000`. Fuori immagine: stato PADDING distinto, valori zero ma mai
presentati come pixel scientifici validi. La maschera è indipendente dalla
validità: `(x//256+y//256+f) mod 7 == 0`.

La piramide è decimazione nearest, dichiarata primaria: il texel `(i,j,L)` usa
la coordinata base `(i*2^L,j*2^L)`. Non è un downsample scientifico anti-alias.
Viewport fisso 1024×768 logici; DPR richiesto esattamente 1 o 2, verificato sul
widget/window. Zoom `1/8` iniziale, alternanza `1/8` e `1/4`; picking da pixel
fisico a coordinate base con floor e livello `log2(zoom_denominator/DPR)`.
Gli origin possono attraversare il bordo: nessun clamp nascosto del padding.
Fattori Qt `0.5|1|2` e GDK `1|2`, default 1, sono parametri primari separati dal
DPR totale osservato. Si scelgono prima del run, mai come retry dopo un errore.

STF manuale sintetica, non auto-STF e non codice di Siril/PixInsight:
black=0, white=1, midtones `m=1/4`. Derivazione propria dalla funzione rational
`M_m(x)=((m-1)*x)/((2*m-1)*x-m)`; quindi `M(x)=3*x/(1+2*x)`.
La fonte operativa è questa definizione matematica e il suo oracle Fraction;
nessuna implementazione esterna viene copiata o consultata in rete.
Su un numeratore `N/1024`, quantizzazione display encoded RGB8 half-up:
`floor(255*3*N/(1024+2*N)+1/2)`; lineare `floor(255*N/1024+1/2)`.
Anchor indipendenti: `M(0)=0`, `M(1/4)=1/2`, `M(1/2)=3/4`, `M(1)=1`.
Nessun dato float o tile viene modificato attivando STF, maschera o split A/B.
Split sinistro lineare, destro STF se abilitata. Overlay maschera: media intera
con rosso; INVALID e PADDING restano colori espliciti, alpha sempre 255.

Istogramma: tre distribuzioni a 256 bin dei soli campioni validi del viewport
campionato, prima della STF. Non è l'istogramma dell'intera immagine 3 GiB.
La curva STF è calcolata dalla stessa funzione, non un disegno decorativo.
Il canvas BGRA8 little-endian è uno snapshot pre-compositore, non una prova
di fedeltà del monitor. Nessun profilo personale viene letto.

## Memoria, scheduling e interazione

Cache condivisa come algoritmo C++ nativo per tutti: 146 slot, ciascuno con
RGBf32 e stato validity8. Pool fisso 124387328 B più indice bounded, sotto
128 MiB. Cache privata di ciascun processo; niente immagine completa.
Un worker CPU, una richiesta latest-wins, una consegna completata pending.
Generazione verificata per riga e prima della pubblicazione; risultati stale
scartati, cancel non sostituisce il metodo. Nessuna coda illimitata di task.
Frame BGRA fisico massimo 12 MiB a DPR2, copie di bridge e superfici native
registrate separatamente. Budget buffer 256 MiB, RSS 512 MiB best effort.

Tabella virtualizzata di 2000 frame, tastiera/focus nativi dove disponibili;
etichette accessibili esplicite. Heartbeat UI e timestamp callback permettono
solo diagnosi del thread UI. Non dimostrano screen reader, IME, accessibilità
completa o latenza fisica di presentazione. Tutti questi test sono NON ESEGUITI.

## Oracle, colore ed export

Oracle Python distinto dal generatore C++, rational/integer, con selezione tile,
bit float32, coordinate, validità, padding, maschera, STF e snapshot deterministici.
Missing tile iniettato produce STOP e nessuna nuova immagine, mai rigenerazione
sostitutiva. ICC sintetico matrix/TRC costruito localmente: controllo strutturale
e identità dei byte noti; Qt deve accettarlo anche con QColorSpace nel gate
futuro. ICC corrotto produce STOP. Non è un parser ICC generico e la conformità
colorimetrica, specie GTK, resta non validata.
L'oracle passa soltanto dalla fase supervisionata `run.py --phase oracle`:
proprio Popen, HOME/cache/TMP privati, handshake, pipe non bloccanti, deadline 600 s,
RSS 512 MiB best effort, AS 768 MiB, file 32 MiB, log 8 MiB e receipt/finally di
raccolta. Errori o mancata persistenza dei receipt non diventano PASS.
Tutti i casi originali restano. Prima dei dati sono aggiunti zoom denominatore
4/8/16 × DPR1/2 × frame 0/1/1999, snapshot senza tolleranza, eviction su 168 tile
distinti oltre 146 slot e revisit, cancel dopo quattro righe effettivamente
renderizzate, generazione intermedia obsoleta e completamento della più recente.
Dimensioni/offset/alignment ABI 2, dtype, byte order e stride sono query del core
compilato confrontate con ctypes. BUILD004/ORACLE004 sono stati eseguiti e hanno
review indipendente PASS: `docs/evidence/M1-b-build-oracle-004.md`. Questa prova
non implica esecuzione di oracle005 o validazione GUI.

Export implementabile limitato a viewport BGRA8 + ricetta JSON, con dimensioni,
byte order, alpha opaco, generazione, stato STF/split/maschera e hash. Non è
export scientifico, PNG di produzione o cattura dello scanout; nessun export
dell'immagine float completa è previsto in questo spike.
Un solo export di viewport per run: massimo 12 MiB di payload più ricetta.
Una coppia incompleta dopo errore I/O non è un export valido e non viene riparata.
La validità richiede l'ACK nel receipt riuscito, non la sola presenza dei file.
ICC/export usano capability di directory ereditate: fd-relative, NOFOLLOW,
regolare/un link e temporanei esclusivi. Qt pubblica con renameat2 NOREPLACE,
fsync file/directory e verifica finale; Python linkat/unlinkat è un metodo primario
distinto, non fallback o equivalenza I/O. Errori lasciano output diagnostici/STOP.
La DSO del core viene verificata e copiata in un memfd Linux sigillato; CDLL
carica proprio quel fd. Una versione/hash per processo, fd/handle tenuti insieme
fino all'uscita; niente riapertura del nome del build e nessun ripiego su loader
alternativi. Dipendenze di sistema, Qt/GTK e compositore non sono tutti pinnati.

Il dato `fallback_calls=0` non è una misura ed è rimosso. Lanci Popen e classi
renderer possono essere osservati; fallback interni restano NOT_INSTRUMENTED,
policy FORBIDDEN_EXPECTATION e gate antifallback INCOMPLETE. Nessun nuovo zero
costante o mutante non eseguito viene presentato come prova.
Lifecycle GTK: start/run protetti da finally con cancel/notify/join/release anche
su eccezioni; regressioni senza import GTK predisposte. GTK resta NON AMMESSO.

## Gate futuri e metodologia

Ordine obbligatorio: review delle fonti; capacità/render selection senza retry;
build isolata con riserva fisica misurata; oracle CPU; test errori; correttezza
GUI con DPR/sessione qualificati; soltanto dopo eventuale misura esclusiva.
Ad ogni fonte o parametro cambiato corrisponde una nuova serie/hash.

Preparare 100 pan `(64,32)` e 100 zoom alternati come workload separati. Non
assegnare FPS inventati: frame callback e swap non sono timestamp dello scanout.
Gli eventi scartati/accorpati sono contati, non trasformati in latenza zero.
Il programma manuale non è ancora un supervisore comparativo qualificato.
Se in seguito si confronta lo stesso workload aggregato per p/r, preregistrare
3 processi per candidato, 2 warm-up e 7 round, ordine appaiato e bootstrap
gerarchico 2000/4059 del protocollo padre. Mediana/p95 dei 100 eventi appartengono
al singolo round; non sostituiscono i cluster. Nessuna statistica comparativa
viene prodotta da questa preparazione.

## Evidenza, rollback e prossimo comando

Preparazione e controlli realmente svolti: `docs/evidence/M1-b-preparation.md`.
Quella ricevuta e il manifest002 restano immutati; il nuovo delta è registrato
in `docs/evidence/M1-b-static-followup.md` con snapshot byte/hash e diff.
I futuri artefatti appartengono a una directory nuova `tmp/m1/b-viewer/runs/`;
nessun gate è PASS perché esiste una fonte. In caso di errore fermare solo il
proprio figlio, conservare sorgenti e log, niente cambio renderer/sessione/DPR.
Primo controllo ora: AST/sintassi delle sole fonti005. Dopo review indipendente
e nuova ammissione: build005 fresca, confronto DSO e primo run GUI limitato
descritto sotto. Nessun avvio GUI o oracle implicito.

## Preregistrazione005: primo frame, export e uscita

Stato di questa unità: **PREP ONLY**. Build005, preview e nuovi test NON ESEGUITI.
Fonti/diff/check005 e prova statica: `docs/evidence/M1-b-preview-preparation.md`.
Lo slot attivo di F non autorizza esecuzioni del viewer.

Opzione primaria nuova: `--preview-mode first-frame-export-exit`, nella fase
esistente `--phase preview`, scelta prima del Popen. La modalità `manual`
rimane distinta ed è il default storico: non ha auto-uscita di successo.
Nessun timer trasforma la deadline del supervisore in una preview riuscita.

Primo e unico caso GUI qui preregistrato: **qtwidgets-raster, xcb, DPR1,
QT_SCALE_FACTOR=1**, stato iniziale x2048/y2048, zoom1/8, frame0, STF attiva,
maschera/split disattivi, generazione1. Xcb resta UNQUALIFIED rispetto a X11
nativo/XWayland. Non si attribuisce un adapter fisico o uno scanout.

QML conserva il solo modo manuale: il modo limitato produce NOTSUPPORTED prima
di QApplication. La correlazione tra PaintedItem e swap richiede un gate
dedicato, non il riuso di una label Widgets. GTK resta NON AMMESSO; il nuovo
modo rifiuta ogni candidato diverso da Qt Widgets raster, senza import GTK.

Sequenza obbligatoria del modo limitato:

1. Verificare classe reale QPaintEngine::Raster e DPR del proprio canvas.
   L'ammissione iniziale resta successiva a un paint reale, non a un timer cieco.
2. Attendere il risultato corrente del worker. Nel paint del RasterCanvas,
   catturare la generazione dell'immagine effettivamente usata dal drawImage e
   engine/DPR reali; terminare il QPainter prima di emettere `framePainted`.
3. Il collegamento queued al solo canvas proprio consegna quella generazione
   dopo il ritorno del paintEvent. Scartare callback stale; richiedere immagine,
   stato corrente e generazione1 coerenti. Stato cambiato produce STOP.
4. Invocare una sola volta l'export esistente BGRA+JSON: fd-relative, temporanei
   esclusivi, fsync file/directory, NOREPLACE e verifica finale. Conservare ogni
   output parziale se fallisce. Non cambiare destinazione o profilo ICC.
5. Registrare `first_frame_export_exit` con stato, generazione, SHA del buffer
   proprio, callback/engine/DPR, dimensioni e ACK di durabilità, poi exit(0)
   del solo QApplication. Il ritorno di app.exec svolge la normale distruzione
   stack: Controller cancella, notifica, join e bv_destroy. `controller_released`
   è emesso solo dopo il ritorno effettivo di join e bv_destroy.
6. Il supervisore richiede exit0, sorgenti immutate, ordine univoco di recipe,
   renderer_observed, input, frame_ready, paint_event_callback, viewport_exported,
   first_frame_export_exit e controller_released. Rilegge i due file NOFOLLOW e
   verifica stato, dimensioni e SHA coerenti con i checkpoint. Solo allora
   registra FIRST_FRAME_EXPORTED_AND_CLEAN_EXIT, non una qualifica GUI completa.

Chiusura della finestra prima dell'ACK: exit non riuscita, nessun PASS.
Mancato paint, errore, blocco del worker o callback assente: STOP/deadline del
supervisore, raccolta del proprio gruppo e conservazione di log/output.
Non si cambia QPA, renderer, DPR, metodo o formato dopo un errore.
Il paint osserva il proprio canvas/buffer pre-compositore: clipping, occlusione
e scanout non sono provati. Non cattura desktop, finestre estranee o input globale.

### Ordine e riferimento core esplicito

1. Review005 sul diff reale. Nuova ammissione esclusiva e preflight fisico.
2. Build005 da sorgenti005 in directory nuove, mai una copia di build004.
3. Confrontare la DSO appena compilata con SHA004
   `7f76d4d04ae5a0855dc38dd53d6a4349520fe0171eba2636391ca97c12e00f0a`.
   Se differisce: STOP e nuova review; nessuna adozione o nuovo oracle automatico.
4. Se è identica, scegliere esplicitamente `--core-oracle-reference oracle-004`.
   Il runner verifica i manifest/provenance004 pinnati, exit0 e la stessa DSO
   prima del parser ICC o di qualsiasi GUI. Riusa soltanto la prova di quel
   binario core: nessun oracle005 dichiarato, nessuna equivalenza del nuovo host.
5. Nuovo preflight e solo il caso Widgets xcb/DPR1/QtScale1. Conservare log,
   receipt, export, hash PRE/POST e checkpoint. Primo FAIL inatteso ferma il turno;
   nessun autofix, retry, rinomina o preview successiva.

Comandi futuri dalla radice progetto, dopo le ammissioni richieste, con
HOME/TMP/XDG/cache già privati sotto ROOT prima di ogni figlio:

```bash
/usr/bin/python3 -I -B -S tmp/m1/b-viewer/run.py --phase build --build-id build-005 --run-id build-005 --exclusive-admitted
# Solo dopo build completa, SHA core identico, nuovo preflight e ammissione GUI:
/usr/bin/python3 -I -B -S tmp/m1/b-viewer/run.py --phase preview --build-id build-005 --run-id widgets-first-frame-xcb-dpr1-005 --candidate qtwidgets-raster --platform xcb --dpr 1 --qt-scale-factor 1 --preview-mode first-frame-export-exit --core-oracle-reference oracle-004 --fault none --exclusive-admitted
```

Precondizioni: build-005, runs/build-005 e runs/widgets-first-frame-xcb-dpr1-005
assenti, anche come symlink. Pin005 verificati PRE/POST; protocollo169 invariato.
Riserve reali del runner invariate: build96 MiB, preview32 MiB; cap globale
ROOT/tmp1 GiB fisico con inode unici, buffer256 MiB, RSS512 MiB best effort,
AS768 MiB, file32 MiB, log8 MiB, deadline preview180 s e build180 s per figlio.
Questi limiti non sono SLA né autorizzano installazioni, backend sostitutivi o
riduzioni di qualità se la GUI non entra nei limiti.

Nessun QtTest, xdotool o servizio aggiunto. Nessuna automazione dei 200 eventi,
A11y/IME, picking, 100 pan/zoom o misura di presentazione/performance in questo
modo. Antifallback interno resta INCOMPLETE; GTK e confronti statistici restano
gate futuri. Il record di successo del modo limitato non li chiude.

## Profilo006: autorizzazione della sessione xcb esistente

Stato: **PREP ONLY**, F24 unico runtime. La preview005 rimane FIRSTFAIL immutabile:
`docs/evidence/M1-b-preview-005.md`. MAIN ha registrato soli metadati in
`docs/evidence/M1-b-session-metadata-006.md`: XAUTHORITY presente nell'ambiente
ma non inoltrata dal launcher005, percorso libxcb-cursor presente. Questi dati
non dimostrano caricamento della libreria o successo di una nuova preview.

Il solo figlio GUI Qt con QPA xcb riceve XAUTHORITY già disponibile come stringa
opaca dell'ambiente ereditato. Mancante o vuota: STOP diagnostico prima della
creazione ICC e di ogni figlio del run preview. Il parser ICC separato, build,
oracle, GTK e Qt Wayland non ricevono questa nuova variabile. Non viene cambiato
alcun backend. HOME/TMP/XDG/cache restano privati ROOT.

Non aprire/stat/risolvere il percorso indicato da XAUTHORITY, leggere cookie,
invocare xauth/xhost, copiare credenziali nel workspace o cambiare autorizzazioni.
Non mettere il valore in argv, manifest, receipt, report o log ambientali.
L'uso dell'autenticazione standard da parte di Qt non è un bypass. Un wrapper
che filtra l'ambiente deve conservare XAUTHORITY per il supervisore preview
senza registrarne il valore; in caso contrario il nuovo preflight ferma il run.

Ordine futuro obbligatorio: review indipendente006, ammissione BUILD006 separata,
build fresca e pin/provenance conformi. Confrontare la DSO con lo SHA004 fissato
sopra: se differisce STOP e nuova review, mai adozione o oracle automatico.
Solo se identica: riferimento esplicito oracle004 per quel binario core, poi
nuova ammissione GUI e preflight. Il nuovo host non eredita una qualifica GUI.

Comandi futuri dalla radice `/opt/workspace/astroProcessor`, non eseguiti:

```bash
/usr/bin/python3 -I -B -S tmp/m1/b-viewer/run.py --phase build --build-id build-006 --run-id build-006 --exclusive-admitted
# Solo con successiva ammissione GUI, nuovi pin/quota e XAUTHORITY ereditata:
/usr/bin/python3 -I -B -S tmp/m1/b-viewer/run.py --phase preview --build-id build-006 --run-id widgets-first-frame-xcb-dpr1-006 --candidate qtwidgets-raster --platform xcb --dpr 1 --qt-scale-factor 1 --preview-mode first-frame-export-exit --core-oracle-reference oracle-004 --fault none --exclusive-admitted
```

Output006 nuovi/esclusivi, nessun riuso o rinomina degli output005. Ricetta,
limiti, checkpoint paint/export/exit/cleanup e gate antifallback restano quelli
preregistrati005. Nessun oracle006 dichiarato, nessuna esecuzione autorizzata
da questa preparazione. Prove statiche: `docs/evidence/M1-b-session-followup-006.md`.

## Profilo007: osservazioni proc e raccolta del figlio proprio

Stato: **PREP ONLY**. BUILD006 resta STOP_FIRSTFAIL durante configure;
preview005 e tutti i suoi output restano immutati. La diagnosi storica ESRCH
non contiene traceback: il nuovo codice copre le letture dei discendenti,
senza attribuire retroattivamente una causa precisa a quel tentativo.

Le sole letture children/status dei discendenti assorbono ENOENT/ESRCH come
osservazioni perse, con contatori bounded per tipo/errno e primo/ultimo PID.
EACCES, altri errori e perdita del leader fermano il run. RSS mancante vale
`None`/JSON null, mai zero sintetico. Uno zombie senza RSS è un'osservazione
mancante esplicita. Una somma parziale non qualifica il rispetto del limite;
il receipt distingue NOT_OBSERVED, INCOMPLETE_OBSERVATIONS e superamento osservato.
Il guard resta best effort; nessun limite o contratto scientifico cambia.

Il supervisore conserva il proprio handle Popen e usa WNOWAIT, senza poll/reap
anticipato. Prima del segnale al gruppo verifica il figlio non raccolto e
SID/PGID uguali al suo PID. Ownership persa o metadati negati producono STOP,
senza segnalare un gruppo non verificato. ESRCH dal segnale consente soltanto
il wait del proprio figlio. Errori di cleanup rimangono nel receipt separati
dalla diagnosi primaria e impediscono il successo; non attestano una raccolta
riuscita o l'assenza di residui.

Deliverable: manifest007, diff contro snapshot006, regressioni mock predisposte
e `docs/evidence/M1-b-supervisor-followup-007.md`. Qui soltanto AST/hash;
nessuna suite, build, GUI o candidato runtime ammesso. Prossimo passo:
review indipendente delle fonti effettive e dei pin007. Poi un turno distinto,
con preflight quota e directory nuove, può ammettere le regressioni:

```bash
/usr/bin/python3 -I -B -S tmp/m1/b-viewer/supervisor_regressions.py
```

Il comando è pianificato, non eseguito. Una build007 richiede ammissione
separata dopo i gate; nessun riuso degli output006 o avvio automatico della GUI.

## Profilo008: deadline di raccolta e gate mock integrati

Stato: **PREP ONLY, review indipendente richiesta**. La review007 comunicata
dal coordinatore (`ses_e887a4837ffemKqHRwWie7c2iC`) è NONREADY: wait finale
illimitata e copertura mock dei gate incompleta. Non eseguire il comando
preregistrato sopra prima della nuova review008 e di un turno distinto ammesso.
Snapshot007 stabile conservato prima delle modifiche; evidenze007 immutate.

Il cleanup ha una deadline monotonic propria di 2.0 secondi, senza cambiare
deadline del candidato o cap. Dopo i controlli ownership/WNOWAIT/SID/PGID e il
segnale, wait riceve soltanto il tempo residuo, anche zero. SIGKILL non prova
il reaping: TimeoutExpired produce STOP, exit_code null e PENDING_UNREAPED.
Il receipt conserva separatamente l'eventuale exit osservato con WNOWAIT,
la diagnosi primaria, quella del cleanup e l'obbligo di trattenere lo slot.
Questo limite riguarda l'attesa del figlio, non garantisce tempi massimi
per syscall bloccate nel kernel o persistenza dei receipt.

Prima di ogni Popen si persiste esclusivamente `pending-own-child.json` nel
namespace b-viewer. Solo il wait riuscito sullo stesso handle vivo consente
la rimozione del latch e lo sblocco. Un cleanup incompleto conserva handle
e latch; una rimozione fallita conserva il blocco anche dopo reaping riuscito.
Persistenza iniziale fallita impedisce Popen. Latch presente, anche da un altro
interprete, oppure errore nel verificarlo impediscono nuovi figli. Il latch
non conferisce autorità e non permette di ricostruire handle da PID registrati.
Nessuna rimozione automatica in un nuovo processo e nessun rilascio implicito
dello slot: serve riconciliazione indipendente, senza segnalare PID storici.

Restano invariati i contratti007 delle letture proc e della RSS mancante.
Nuovi casi predisposti attraversano watch con proc sintetico e funzioni reali
di campionamento: RSS oltre cap, RSS parziale oltre cap, spazio oltre CAP,
deadline scaduta, valori esattamente al limite e figlio non raccoglibile.
Deadline residua, latch e blocco dei lanci successivi hanno casi dedicati.
AST/hash soltanto: nessuna regressione, build, GUI o processo candidato eseguiti.

Deliverable008: manifest, diff contro snapshot007, controlli, handoff e
`docs/evidence/M1-b-supervisor-followup-008.md`. Il primo passo verificabile
resta la review indipendente del diff effettivo e dei pin008. Solo dopo review
e ammissione distinta si potrà eseguire la suite mock già indicata, non ora.
Una race proc rimane causa plausibile, non dimostrata, dell'ESRCH006.
