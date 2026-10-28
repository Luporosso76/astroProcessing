# M1.b: primo turno build/oracle, FIRSTFAILSTOP

Esito effettivo: **BUILD FAIL, exit 2. ORACLE NOT_RUN**.
Il turno esclusivo autorizzato si ferma al primo fallimento inatteso. Nessun
autofix, retry, rinomina automatica, modifica di gate/fonti o esecuzione della
DSO parziale. Questo record consegna evidenze, non una proposta di correzione.
Modello dichiarato dal runtime dell'autore: `openai/gpt-6.1-sol`.

## Ammissione, fonti e freschezza

Review003 PASS STATIC comunicata dal coordinatore per
`ses_e8c14fabaffeCEsreEF1hHtERa`: non equivale a build o runtime PASS.
Riletta la coda della preregistrazione `plans/M1-b-viewer.md`, righe 123-167,
e i comandi/limiti effettivi del README e del supervisore pinnati.

Manifest sorgenti: `tmp/m1/b-viewer/source-hashes-003.json`, SHA-256
`47c2bade1f71369d582891a058fa344b6eb992962e9fa7742763bb833ecca041`.
**19/19 hash conformi PRE e POST**: 18 file più la preregistrazione propria.
Protocollo padre: 169 righe, SHA PRE/POST
`1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.
Non letto il piano di 4482 righe. Progress e altre unità non modificati.

Verificata prima dell'esecuzione l'assenza, inclusi symlink, di tutti gli output:

- `tmp/m1/b-viewer/build-003/`;
- `tmp/m1/b-viewer/runs/build-003/`;
- `tmp/m1/b-viewer/runs/oracle-003/`.

Anche `execution-turn-003/` è nuovo ed esclusivo. Snapshot precedente, manifest002,
check002, manifest/check/diff003 e report di preparazione/follow-up restano
immutati. Le mappe SHA PRE/POST complete sono nei record del turno.

## Preflight fisico reale

Metrica globale: somma radicata in `tmp/` di `st_blocks*512`, inode unici
`(st_dev,st_ino)`, senza seguire symlink. Sono metadati necessari al budget,
non letture di contenuto delle fixture delle altre unità.

| Quantità | Byte |
| --- | ---: |
| Globale prima del turno | 861204480 |
| Globale al preflight del supervisore pinnato | 861241344 |
| Riserva build dichiarata dal sorgente | 100663296 (96 MiB) |
| Globale + riserva build, preflight pinnato | 961904640 |
| Cap globale invariato | 1073741824 (1 GiB) |
| Spazio libero filesystem al primo preflight | 124891549696 |
| Picco globale osservato durante il processo | 862461952 |
| Globale fotografato dal manifest build alla fine | 862449664 |
| Globale esterno dopo il processo | 862461952 |

Il monitor esterno conserva 134 campioni, zero campioni persi per race metadata.
Il picco è osservato, non una scansione atomica o garanzia universale anti-OOM.
Le differenze di pochi KiB fra fotografie comprendono la scrittura dei receipt.
Nessun cap aumentato e nessuna fixture eliminata o resa più piccola.

La riserva oracle pinnata è 33554432 B (32 MiB), non un budget inventato.
Prima del turno è stata registrata soltanto la somma ipotetica 894758912 B.
**Nessun nuovo preflight oracle e nessun oracle eseguiti:** il prerequisito
build completa non è riuscito. Non si usa il budget ipotetico per aggirarlo.

## Comando effettivamente eseguito

Dalla radice `/opt/workspace/astroProcessor`:

```bash
/usr/bin/python3 -I -B -S tmp/m1/b-viewer/run.py --phase build --build-id build-003 --run-id build-003 --exclusive-admitted
```

Nessuna opzione inventata. I due comandi figli del codice pinnato sono registrati
integralmente nei receipt: configure CMake Release, GCC esplicito, registry utente
e sistema disabilitati; poi `cmake --build ... --parallel 1`.

Ambiente del supervisore: HOME/TMP/XDG cache/config/data/state privati sotto
`tmp/m1/b-viewer/execution-turn-003/`. I figli usano gli omologhi privati sotto
`runs/build-003/`, secondo il codice congelato. `PATH=/usr/bin:/bin`,
`OPENSSL_CONF=/dev/null`, `PYTHONDONTWRITEBYTECODE=1`, `PYTHONNOUSERSITE=1`,
`LC_ALL=C.UTF-8`; nessuna variabile display o credenziale passata dal driver.
Nessuna rete/installazione, servizio/configurazione globale, indice o Git.

I limiti pinnati dei figli restano: RSS 512 MiB best effort, AS 768 MiB,
file 32 MiB e log 8 MiB. Configure/build hanno deadline 180 s ciascuno;
la deadline 600 s dell'oracle non viene utilizzata. Buffer scientifici pianificati
entro 256 MiB invariati, ma qui il viewer non alloca pool/frame perché non gira.

## Processi, versioni ed esiti

| Processo | PID osservato | Exit | Esito |
| --- | ---: | ---: | --- |
| Supervisore `run.py --phase build` | 220927 | 2 | STOP |
| CMake configure | 220928 | 0 | COMPLETED |
| CMake build | 221009 | 2 | STOP, primo errore C++ |

Entrambi i receipt figli registrano `cleanup: wait/reap completed`.
Non sono stati terminati processi esterni. I gruppi gestiti sono le sole nuove
sessioni dei Popen propri, secondo il supervisore pinnato.

Versioni osservate nei dati della build: GNU C++ **15.2.0**, CMake **4.2.3**,
generatore Unix Makefiles. Qt **6.10.2** è il package richiesto EXACT e risolto
con successo a configure; AutogenInfo registra Qt major 6/minor 10. Non è una
verifica della versione del renderer in esecuzione. Runtime supervisore Python
`3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`, Linux x86_64.

Configure riporta librerie OpenGL e header Vulkan trovati: è risoluzione di
dipendenze di build, **non** creazione di un renderer o prova hardware GPU.
I flag target pinnati restano `-O2 -fno-fast-math -ffp-contract=off`; nessuna
qualità o precisione modificata per compilare.

RSS conservativo del tree configure: 92114944 B, massimo thread osservato 8.
Tree build: **471805952 B**, massimo thread osservato 14. Il metodo somma
`max(VmRSS,VmHWM)` dei processi osservati: può sommare picchi non contemporanei,
non è PSS né equivalenza con cache tile. `--parallel 1` non implica che MOC/autogen
non creino thread interni. Nessun guard ha causato il fallimento.

## Primo fallimento inatteso, preservato

Prima diagnosi a `qt_viewer.cpp:146`, in `Controller::admit`:

```text
error: cannot convert ‘<brace-enclosed initializer list>’ to ‘QEvent*’
```

Il compilatore risolve le chiamate non qualificate `event(...)` nei metodi di
Controller verso l'ereditato `QObject::event(QEvent*)`, non verso l'helper libero
`event(QJsonObject)`. La stessa diagnosi ricompare in request/cancel/receive/
painted/exportViewport. È presente anche `-Wmisleading-indentation` a riga 178.
Nessuna di queste righe è stata corretta in questo turno.

`build.log` conserva 7601 byte di stdout/stderr figli **combinati**, come previsto
dal supervisore; `configure.log` 751 byte. Lo stderr del solo supervisore esterno
è vuoto: non significa assenza degli errori del compilatore, presenti nel log
combinato. L'intervallo esterno registrato dal driver per esecuzione/raccolta è
9131247826 ns, non un benchmark né il tempo totale della redazione del record.

## Artefatto binario parziale e prove NON ESEGUITE

Il target `viewer_core` è stato compilato/linkato; MOC/RCC e l'oggetto MOC sono
arrivati al completamento prima dell'errore della translation unit Qt.
`build-003/libviewer_core.so`: 27216 byte logici, SHA-256
`7f76d4d04ae5a0855dc38dd53d6a4349520fe0171eba2636391ca97c12e00f0a`.

È un **artefatto parziale non adottato**. `qt_viewer` e `provenance.json` non
esistono. Non si carica la sola DSO per saltare il gate della build completa.
Pin memfd sealed, ABI queries/offset/alignment/stride e consumo del DSO da parte
di Python: **NOT_RUN**, non dedotti dal suo hash o dalla compilazione.

GT scientifico consumato: **zero casi SYN-VIEW**. Le verifiche CMake del
compilatore/Threads/atomics sono probe di build, non oracle di immagine.
Oracle Fraction/integer, originali e 18 nuove combinazioni zoom/DPR/frame:
**NOT_RUN**. Eviction 168 tile/revisit, cancel in-flight/latest-wins, padding,
invalid e bit float: **NOT_RUN**. Le 11 regressioni della fase oracle: **0/11
eseguite**, non lanciate come suite separata. `runs/oracle-003/` resta assente.

Nessuna preview, widget/GUI launch, import GTK, profilo ICC/Qt in esecuzione,
100 pan/zoom, cattura display, test A11y/IME o statistica di performance.
Il core compilato non rende validati rendering, dati o layout runtime.

## Ricevute e hash

Record del turno: `tmp/m1/b-viewer/execution-turn-003/` contiene preflight,
comando/ambiente, stdout/stderr esterni, exit con 19 SHA POST e record immutabili,
`FIRSTFAILSTOP.json` e catalogo SHA/dimensioni degli artefatti.

- `FIRSTFAILSTOP.json`:
  `1f2eb9611dbb33fdd8644396f724dec5e2cd35459ddb6ad0acc666525fe59b0b`.
- `artifacts.json`:
  `4d258639567424db041eae5252cf0233f1082601b4952db23a236fb8b82be282`.

Sotto `tmp/m1/b-viewer/runs/build-003/`:

- `configure.log`: `a71b63d977e88b8b53b8f2891d848f84f49ec87b77a8ef9a5acf8b0510411b2a`.
- `configure-receipt.json`: `3e1d7b4b8b506da3af89fdf33469ebf6dcfe822c632ef2af54951407d49d2d0b`.
- `build.log`: `376164e8fb6511f0cd858642fd19c87c6cec15600ef941815e75df60dfaa60f6`.
- `build-receipt.json`: `49546a82477887e918123f5f667448d412c2b8a42de9fb5fd1bd01cfc1082459`.
- `manifest-after.json`: `98a203ad150a98f9b925fd841a47ce57556c55a6c7afc33c69f7ce32b4e973b1`.

## Handoff e gate ancora aperti

**Primo gate non chiuso: build Qt completa.** Il turno finisce qui, con fonti e
hash003 immutati e FIRSTFAIL preservato. Nessuna nuova esecuzione autorizzata
da questo record. M1.b resta aperta: renderer antifallback INCOMPLETE, GTK non
ammesso, GUI/interazioni/ICC/colori/font/HiDPI/presentazione/A11y non validati.
Callback, frame offerti e scanout restano distinti. Supervisore comparativo
3p/2 warm-up/7 round e bootstrap 2000/4059 non implementati né simulati.
