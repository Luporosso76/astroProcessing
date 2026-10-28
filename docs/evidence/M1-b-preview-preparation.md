# M1.b: preparazione005 del primo frame con uscita controllata

Stato: **PREP ONLY**. Nessuna compilazione, import di candidato, GUI, oracle,
regressione o benchmark eseguiti. Build005 e preview005 non esistono.
Il runtime è nello slot di F; questa consegna non ne usa l'ammissione.
Modello dichiarato dal runtime dell'autore: `openai/gpt-6.1-sol`.

La review indipendente PASS di BUILD004/ORACLE004 è stata comunicata dal
coordinatore. Il report004 e i relativi output restano immutati; non sono una
prova di esecuzione del nuovo host005. La review del delta005 è ancora da svolgere.

## Snapshot e conservazione prima della prima patch

Verificati i 19 hash004 e i mode. Manifest004 SHA-256:
`43c90d9ef791f423f6c43c2b8540ef978152996f8e10b998a9a202387f525f52`.
Snapshot: `tmp/m1/b-viewer/snapshots/before-preview-preparation-005/sources-004.zip`.
SHA-256: `1c416578a75b72fad4cd907739f25cdfd3abbabccb9ec14ea34e535e9341a12a`.

Il ZIP contiene 24 file: 19 fonti/piano004, manifest/diff/check/handoff004 e
report004 di205 righe. Byte e mode Unix sono conservati per ciascun membro.
Receipt dello snapshot:
`198c4d9bfcc85e67e10dc2d52fc6d13e431553fa28e6c7e03741b10e34c5d5e5`.

Inventario PRE/POST: **350 voci protette**, inclusi build003/build004, runs
con oracle004, execution-turn003/004, snapshot, pin e report storici.
Per file regolari si confrontano SHA/mode/nlink; per i symlink delle fixture
si conserva il link senza dereferenziarlo. Nessun chmod o modifica degli output.
I 19 pin004 restano verificabili nel ZIP;15 file correnti conservano anche
l'hash004. I mode di tutte le 19 fonti restano invariati.

Protocollo padre:169 righe, SHA-256 invariato
`1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.
Non letto il piano di4.5k righe. Progress e altre unità non modificati.
Nessun Git, indice, rete, installazione, delega o configurazione di sistema.

## Delta effettivo: quattro file

- `qt_viewer.hpp:79`: segnale del solo RasterCanvas `framePainted`, con
  generazione, engine e DPR del paint reale.
- `qt_viewer.cpp:275`: cattura della generazione dell'immagine usata dal canvas,
  verifica Raster/DPR esistente, completamento del QPainter e poi segnale.
- `qt_viewer.cpp:410`: handler queued, collegato prima di mostrare la finestra.
  Richiede generazione corrente1 e stato iniziale invariato; scarta callback stale.
  Usa l'export esistente, registra stato/hash/ACK e invoca solo `app.exit(0)`.
- `qt_viewer.cpp:142`: log `controller_released` dopo join e bv_destroy effettivi.
  Righe452-454: chiusura prima dell'export produce STOP, non exit0 valido.
- `run.py:63,89`: riferimento core004 esplicito e verifica parent dei checkpoint,
  dei due file esportati e del cleanup. Opzioni a righe325-326.
- `plans/M1-b-viewer.md:171`: preregistrazione di modalità, ordine, gate, limiti
  e comandi futuri. La sezione matematica non cambia.

Core/ABI2, oracle/GT, generatori, cache, maschera, cancellazione e policy di
rendering restano identici. Corpi C++ di canvas/admit/request/cancel/run/receive/
painted/exportViewport/workload verificati byte-identici al004. Il destructor
conserva la stessa sequenza di cleanup e aggiunge soltanto il log successivo.
AST Python di allocation/sources/pid_tree/metrics/child_limits/watch invariati.
RunDirectory e l'API di export durevole non sono stati riscritti.

## Semantica del modo esplicito

`--phase preview --preview-mode first-frame-export-exit`, selezionato prima
del Popen. Il modo `manual` rimane separato e non acquisisce auto-uscita.
Primo caso futuro: **Qt Widgets raster, xcb, DPR1, QtScale1**. Connessione xcb
UNQUALIFIED rispetto a X11 nativo/XWayland; nessuna attribuzione dello scanout.

L'ammissione Renderer deriva già da un paint verificato del proprio canvas.
L'unico singleShot esistente rimane quello che accoda l'ammissione dopo quel
paint; non decide il successo. Nessun nuovo timer di successo è stato aggiunto.
Il nuovo segnale contiene la generazione dell'immagine effettivamente usata
dal drawImage, con engine/DPR catturati nello stesso paint sul thread GUI.
Il collegamento queued opera dopo il ritorno del paintEvent, non durante il
QPainter. Generazioni obsolete non provocano export.

Solo immagine corrente1, x2048/y2048, zoom1/8, frame0, STF1, mask0/split0.
L'handler usa una sola volta `exportViewport`: BGRA+JSON tramite capability,
temporanei esclusivi, fsync file/directory, NOREPLACE e verifica finale.
Il checkpoint `first_frame_export_exit` registra stato, SHA del proprio QImage,
generazione, dimensioni, callback/engine/DPR reali, QPA e ACK dopo l'export.
Poi QApplication esce con0; la normale distruzione stack cancella/notifica
il worker, attende join e libera l'engine. Nessun processo esterno viene chiuso.

Il supervisore richiede exit0, sorgenti immutate e ordine univoco:
recipe, renderer_observed, input, frame_ready, paint_event_callback,
viewport_exported, first_frame_export_exit, controller_released. Rilegge i due
output NOFOLLOW e confronta stato/dimensioni/hash. Il log di questo modo è
richiesto JSON: righe diagnostiche non JSON causano STOP con raw preservato.
Un ACK isolato o due file presenti non bastano a registrare successo.

Paint assente, errore o worker bloccato: nessuna auto-uscita riuscita. Il guard
del supervisore termina soltanto il proprio gruppo, registra STOP e conserva
log/output. Nessun retry o cambio di QPA, renderer, DPR o qualità.
Una finestra chiusa prima dell'export non diventa un falso PASS.

QML limitato è **NOTSUPPORTED prima di QApplication**: resta il manuale.
Una correlazione PaintedItem/swap dedicata non si simula con una label Widgets.
GTK resta NON AMMESSO e il nuovo modo lo rifiuta prima di import o avvio.
Nessuna dipendenza QtTest/xdotool o nuovo servizio. Nessun input globale,
grab/screenshot desktop, titolo di finestra estranea o profilo personale.
L'export è il buffer sintetico proprio, non un'immagine dello schermo.

## Comandi futuri e riferimento al solo core004

Prima: review005, nuova ammissione esclusiva, preflight e directory fresche.
Da radice progetto, con HOME/TMP/XDG/cache già privati sotto ROOT:

```bash
/usr/bin/python3 -I -B -S tmp/m1/b-viewer/run.py --phase build --build-id build-005 --run-id build-005 --exclusive-admitted
```

Confrontare la DSO fresca con SHA004
`7f76d4d04ae5a0855dc38dd53d6a4349520fe0171eba2636391ca97c12e00f0a`.
Se differisce: STOP e nuova review, nessuna adozione o esecuzione automatica
di un nuovo oracle. Se è identica, scegliere **esplicitamente** il riferimento
oracle004 e ottenere nuova ammissione GUI/preflight prima del comando:

```bash
/usr/bin/python3 -I -B -S tmp/m1/b-viewer/run.py --phase preview --build-id build-005 --run-id widgets-first-frame-xcb-dpr1-005 --candidate qtwidgets-raster --platform xcb --dpr 1 --qt-scale-factor 1 --preview-mode first-frame-export-exit --core-oracle-reference oracle-004 --fault none --exclusive-admitted
```

Il runner verifica SHA fissati del manifest oracle004 e della provenance
build004, exit0 e identità del nuovo core prima del parser ICC o della GUI.
Nessuna selezione latest o riferimento implicito. La prova riusata riguarda
solo quel binario core: **nessun oracle005 eseguito o dichiarato**.
L'eseguibile host005 resta nuovo e non è qualificato dall'oracle004.

Limiti invariati: riserva build96 MiB, preview32 MiB; ROOT/tmp cap1 GiB fisico
con inode unici; buffer256 MiB, RSS512 MiB best effort, AS768 MiB, file32 MiB,
log8 MiB, deadline preview180 s e build180 s per figlio. Non sono SLA.
Nessuna quota globale ricampionata nello slot F. Non si riducono fixture,
precisione o risoluzione per rispettare i limiti; il run fallisce se non entra.

## Controlli statici realmente svolti

8 AST Python SYNTAX_OK, XML QRC e riferimenti,3 blocchi bash verificati con
`bash --noprofile --norc -n` (README e preregistrazione). Confronti hash/mode
e corpi delle funzioni indicati sopra. Firma `exit(int retcode = 0)` controllata
in lettura nell'header QtCore installato, senza eseguire Qt o un compilatore.
Runtime dei controlli: Python
`3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.

Artefatti sotto `tmp/m1/b-viewer/`:

- `source-hashes-005.json`,19 pin:
  `8a2b8b284c6083ef3f8dd1a8b95fe20f8439cce7a19199999f4c004ff3e958df`.
- `source-diff-005.patch`,356 righe su quattro file:
  `71c1bf00d3c2ae057402777c5c3a57aaf893141fe1a5e1bf787657f7ed569f92`.
- `preparation-checks-005.json`:
  `d951e6c3726c353f22527e4290a9ef064f7912466693df06c8208e1253590190`.

Build-005, runs/build-005 e runs/widgets-first-frame-xcb-dpr1-005 restano assenti,
anche come symlink. Build, import, test e preview005: **NON ESEGUITI**.

La consegna è pronta per review del diff, non dichiara READY runtime o PASS GUI.
Clipping/occlusione/scanout, colore del monitor, A11y/IME/picking,200 eventi,
100 pan/zoom e prestazioni restano fuori da questo modo. Gate antifallback
INCOMPLETE invariato; nessun FPS o prova di presentazione inventati.
