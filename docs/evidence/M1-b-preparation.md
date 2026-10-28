# M1.b: consegna di preparazione del viewer

Esito della consegna: **FONTI PREPARATE PER REVIEW**, non M1.b PASS.
Eseguiti soltanto parsing AST Python, sintassi shell/XML, letture statiche di
API locali e contabilità filesystem. **Nessuna build, compilazione C++,
esecuzione GUI, import GTK/GL o prova numerica eseguita. Nessuna misura.**
Le altre validazioni M1.a sono in corso: questa consegna non ne occupa il turno.

Ambito autorizzato: nuovo `tmp/m1/b-viewer/`, preregistrazione aggiuntiva
`plans/M1-b-viewer.md`, questo record. Non modificati progress, protocollo
congelato, C, storage o risultati precedenti. La contabilità globale usa soltanto
metadati `st_blocks`, non legge contenuti delle fixture o dei sorgenti attivi a/d/f.
Nessuna rete, installazione, Git, Siril, indice, licenza di prodotto o delega.
Modello dichiarato dal runtime: `openai/gpt-6.1-sol`.

## Fonti lette e vincolo del protocollo

- `AGENTS.md`: 52 righe; `PLANS.md`: 46; `docs/progress.md`: 42.
- Sezione M1.b del protocollo: righe 89-101. Il file intero è soltanto riletto
  come byte per hash/line count nei controlli statici: **169 righe**, SHA-256
  `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.
- Nessuna lettura del piano scientifico lungo richiesta da questa preparazione.
- Preregistrazione propria, prima di qualunque futura esecuzione:
  `plans/M1-b-viewer.md`, 139 righe. Fissa SYN-VIEW, piramide, STF, oracle, limiti
  di osservabilità, gate renderer e metodologie future; non cambia il padre.

API Qt controllate nei veri header Qt6 locali: QSGRendererInterface (99 righe),
QQuickPaintedItem (110), sezione QQuickWindow (54), QAbstractItemModel (35),
più dichiarazioni mirate di QColorSpace, QPaintDevice/QImage, QFile, QString,
QVariant e QLineEdit. Letture di API, non esecuzioni dei toolkit.

GTK non ha header/dev `gtk4.pc` disponibili per un'app C/C++ in questo incarico.
Sono stati letti staticamente simboli della libreria
`/usr/lib/x86_64-linux-gnu/libgtk-4.so.1.2200.4` con `nm -D --defined-only` e
stringhe dei typelib Gtk/Gdk con `strings`, senza importare GI o GTK. Sono
presenti `gsk_cairo_renderer_get_type/new`, `gtk_native_get_renderer`,
`gtk_drawing_area_set_draw_func`, `gtk_column_view_new` e `gdk_surface_get_scale`.
Non è stato trovato un setter pubblico `gtk_native_set_renderer`.
Queste osservazioni non provano l'esposizione GI, la riuscita del renderer o
l'assenza di retry interni. Le firme Cairo usate sono state cercate anche nel
vero stub locale `cairo/__init__.pyi`; nessun import Cairo eseguito.

## Consegna effettiva

14 file di sorgenti/documentazione sotto `tmp/m1/b-viewer/`:

| File | Righe | Funzione presente nel sorgente |
| --- | ---: | --- |
| `core_api.h` | 38 | ABI C interna allo spike |
| `core.cpp` | 189 | Dato procedurale, cache tile, piramide, display STF/mask/split, istogramma, cancel |
| `qt_viewer.hpp` | 106 | Modelli/controller/canvas Qt |
| `qt_viewer.cpp` | 413 | UI Widgets e host QML, worker bounded, renderer/DPR check, eventi/export |
| `Main.qml` | 100 | TableView virtuale, controlli, viewport e istogramma reali |
| `viewer.qrc` | 1 | Risorsa QML locale |
| `CMakeLists.txt` | 18 | Build locale GCC 15.2.0/Qt6 6.10.2, nessun download |
| `native.py` | 78 | ctypes con verifica versione/dimensione ABI; nessun load all'import |
| `icc_fixture.py` | 53 | ICC sintetico matrix/TRC fisso e verifica dei byte noti |
| `oracle.py` | 113 | Oracle Fraction/integer e test CPU futuri, non eseguiti |
| `session.py` | 96 | I/O dei run privati, export e gate della prova GTK |
| `gtk_viewer.py` | 343 | UI GTK4/GI reale, ColumnView/DrawingArea e worker ctypes |
| `run.py` | 267 | Supervisore futuro build/preview, mai measure |
| `README.md` | 136 | Comandi futuri, copertura e limiti |

La UI non usa immagini decorative come GT. Il generatore definisce una sorgente
16384² RGBf32 e 2000 frame, senza allocare l'immagine float 3 GiB. La cache ha
146 tile 256², RGBf32 più validity8: pool 124387328 B e indice fisso, sotto
128 MiB. Un worker, una richiesta latest-wins e una consegna pending per processo.
Il core è lo stesso C++ per i tre front-end; GTK attraversa ctypes e GI, quindi
non è Rust GTK nativo e conserva un confondente di binding/modello/copiatore.

STF manuale preregistrata: `M(x)=3*x/(1+2*x)`, parametri (0, 1/4, 1), integer
half-up per RGB8 display. Non è auto-STF; nessun adattamento ai residui osservati.
Le modifiche STF/mask/split non scrivono i tile float. INVALID e PADDING hanno
stati distinti; alpha display sempre 255. Istogramma del viewport prima della
STF e curva derivata dalla formula, non promessa scientifica sul frame completo.

## Candidati e ammissione non ancora verificata

- **Qt Widgets C++ raster**: selezione prima dell'applicazione, engine Raster
  verificato sul painter. Nessun QOpenGLWidget o sostituzione PyQt/PySide.
- **QtQuick/QML C++ software primario**: selezione esplicita prima della GUI,
  PaintedItem/Image e loop basic; controllo API Software, DPR e thread effettivi.
  Errore significa STOP. Non parte dal renderer GL automatico per poi ripiegare.
- **GTK4 via Python GI, Cairo condizionato**: UI implementata, ma candidato
  **NON AMMESSO** finché manca la prova indipendente strict/no-retry legata allo
  SHA del runtime. Il launcher e il front-end richiedono la prova prima
  dell'import GTK. Non è stata creata una prova con un booleano true fittizio.
  API assente o policy non provabile produce NOTSUPPORTED, non un dato GTK lento.

Per GTK la presenza statica di Cairo non risolve la selezione del renderer
Gtk.Native: il getter non permette di imporre un'istanza. GSK_RENDERER, GPU
disable, warning fatali e verifica della classe sono predisposti, ma **non
attestati sufficienti** da questa consegna. Questo è un gate tecnico aperto,
non una richiesta rituale di approvazione o una sostituzione con un altro toolkit.

Tutti dichiarano CPU per il rendering applicativo, non controllo del compositore.
`xcb` può essere XWayland: nessuna etichetta X11 nativo dedotta dalle variabili.
Connessione Wayland, scala/esposizione e presentazione nativa sono prove distinte.
Fattore del toolkit e DPR totale sono parametri separati fissati prima del run;
DPR diverso da quello richiesto produce STOP, mai riduzione automatica.

## Controlli realmente eseguiti

Comando statico: `/usr/bin/python3 -I -B -S` con `ast.parse` sui sei `.py`,
`xml.etree.ElementTree` sul QRC, `bash -n` sui due blocchi del README, hashing
SHA-256 dei file. Nessun modulo dello spike viene importato: leggere AST non
chiama `profile()`, non carica la libreria C++ e non interpreta QML.

Risultati: **6 AST SYNTAX_OK**, XML QRC valido con risorsa esistente, **2 blocchi
bash sintatticamente validi**. C++/CMake/QML semanticamente NON VERIFICATI:
non si spaccia una ricerca di firme per una compilazione o un test GUI.
Runtime dei soli controlli: Python `3.14.4`, build dichiarata GCC `15.2.0`.

Prima del controllo finale: allocazione globale radicata `tmp/`
**857169920 B**, contro **1073741824 B** di cap. È stata verificata una riserva
di **1048576 B** per sorgenti/check statici; non è ammissione per compilare.
Stessa allocazione al termine dei check prima dei nuovi JSON. Scope fisico
`b-viewer/` prima di quei JSON: **143360 B**, compreso lo snapshot statico iniziale.
Metrica: `st_blocks*512`, inode unici, symlink non seguiti; soltanto metadati
delle altre unità. Nessun pool tile, frame o fixture ICC materializzato.

Manifest corrente dei 14 file più due piani:
`tmp/m1/b-viewer/source-hashes-002.json`, SHA-256
`0deee370bd8871323a8ffed27fb46de51f2989a158f1c7b39e805037548acdd4`.
Controlli correnti: `preparation-checks-002.json`, SHA-256
`de752b579e4f25f88270854df3fe07b965404b0c6034ff616d5c94048d842720`.
Le prime copie `source-hashes.json`/`preparation-checks.json` sono conservate:
snapshot di sintassi precedente all'esposizione esplicita dei fattori toolkit,
non una serie eseguita o una validazione rinominata. Per la review usare `002`.

## Parziale o non provato: nessun PASS implicito

Build, import GI, capacità renderer, oracle CPU, errore ICC e missing tile:
**NON ESEGUITI**. Il profilo ha costruzione e controllo della fixture nota;
accettazione da QColorSpace e conformità/colorimetria ICC restano da provare.
Nessuna lettura di ICC personali o monitor, nessun font scaricato.

Esistono heartbeat UI, callback paint/DrawingArea/frameSwapped e annullamento
per generazione; nessuno prova latenza fisica di presentazione o screen reader.
Campi IME, focus, tastiera e nomi accessibili sono predisposti, non testati.
Picking è un sample inspector: il bound di mezzo pixel fisico non ha ancora un
test GUI. Le superfici native e copie interne dei toolkit restano
NOT_INSTRUMENTED, non zero. Il guard RSS 512 MiB è best effort; non è una
garanzia anti-OOM. Export limitato a un viewport BGRA+JSON/run, non scientifico,
non PNG e non acquisizione scanout; coppia incompleta non valida.

I 100 pan e 100 zoom sono richieste offerte a 20 ms, accorpabili dal worker.
Non esiste ancora il supervisore comparativo 3p/2 warm-up/7 round e bootstrap
2000/4059. Nessuna mediana/p95/FPS o comparazione viene inventata. La futura
preregistrazione deve distinguere workload aggregato p/r ed eventi scartati,
oppure dichiarare non applicabile quel confronto prima di misurare.

## Primo controllo ancora incompleto

**Build C++/Qt isolata, poi oracle CPU**, soltanto dopo review dei sorgenti e
ammissione esclusiva del coordinatore. Il README contiene i comandi non eseguiti.
Il launcher richiede nuova riserva fisica: 96 MiB build, 32 MiB preview, cap
globale invariato. Run/build esistenti non vengono sovrascritti; un errore si
conserva e richiede una nuova versione/serie. Per GTK resta prima aperta la prova
indipendente della selezione senza retry. Non avviare GUI, benchmark o measure
per chiudere questo record di preparazione.
