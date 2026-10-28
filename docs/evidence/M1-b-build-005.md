# M1.b: build005 esclusiva, senza oracle o preview

Esito effettivo: **BUILD005 completa, exit 0**. DSO core identica004.
Nessun oracle005, GUI, preview, GTK/QML o benchmark eseguiti. Nessuna patch,
retry, modifica di sorgenti/piano/gate/mode, installazione, rete, Git o indice.
Modello dichiarato dal runtime dell'autore: `openai/gpt-6.1-sol`.

Il coordinatore ha comunicato Dunit12 conclusa exit0, slot esclusivo e review
indipendente TESTS READY STATIC sul delta005. L'ammissione copre **solo la build**;
il successo qui non autorizza l'avvio della finestra.

## Pin, percorsi e immutabilità

Manifest005 SHA-256:
`8a2b8b284c6083ef3f8dd1a8b95fe20f8439cce7a19199999f4c004ff3e958df`.
Verificati byte/hash/mode/nlink delle **19 fonti/preregistrazione PRE e POST**.
Verificato anche l'insieme dei file sorgente letto dal runner, senza file aggiunti.
Protocollo padre: 169 righe, SHA invariato
`1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.
Nessuna lettura del piano baseline di 4.5k righe o modifica a progress.

Le 350 voci protette dalla preparazione, comprese build004/oracle004, snapshot004
e report205, sono conformi PRE/POST. Con gli ulteriori 7 artefatti della consegna005
il controllo del turno copre 357 voci protette. I symlink delle vecchie fixture
sono confrontati come link, non dereferenziati o eliminati.

Verificata l'assenza iniziale di execution-turn-005, build-005, runs/build-005,
runs/oracle-005, runs/widgets-first-frame-xcb-dpr1-005 e di questo report.
Nuovo controllo prima del Popen: output ancora assenti, nessun autorename.
Le directory attendibili sono aperte per componenti con NOFOLLOW; file regolari
e pin sorgenti richiedono un link. Le vecchie fixture intenzionali restano intatte.

## Comando, ambiente e processi

Comando esatto dalla directory `/opt/workspace/astroProcessor`:

```bash
/usr/bin/python3 -I -B -S tmp/m1/b-viewer/run.py --phase build --build-id build-005 --run-id build-005 --exclusive-admitted
```

HOME/TMP/XDG cache/config/data/state del driver sono privati sotto
`tmp/m1/b-viewer/execution-turn-005/`. I path sono stati verificati prima del
figlio: owner corrente, mode0700, directory senza componenti symlink.
I figli del runner usano gli omologhi sotto `runs/build-005/`; TMPDIR è il run.
Owner/mode delle directory private sono verificati anche dopo la build.
`PATH=/usr/bin:/bin`, `OPENSSL_CONF=/dev/null`, `LC_ALL=C.UTF-8`,
`PYTHONDONTWRITEBYTECODE=1`, `PYTHONNOUSERSITE=1`. Nessun DISPLAY, bus o variabile
di sessione personale passato dal driver. Nessun profilo/font personale letto
dall'agente e nessuna istanza QGuiApplication/QApplication del candidato avviata.

| Processo | PID osservato | Exit | Cleanup |
| --- | ---: | ---: | --- |
| Supervisore `run.py --phase build` | 277427 | 0 | wait/reap del driver |
| CMake configure | 277428 | 0 | wait/reap completed |
| CMake build | 277488 | 0 | wait/reap completed |

Il log registra compilazione fresca del core, MOC/RCC, compilazione delle unità
Qt e link finale `[100%] Built target qt_viewer`. Nessun errore o warning nel
log build di 712 byte. Configure conserva 751 byte; stdout/stderr dei figli sono
combinati dal runner. Lo stderr del solo supervisore esterno è vuoto.
Nessuna DSO/eseguibile004 è stata copiata per adottarla come005.

I receipt registrano soltanto Popen propri e la raccolta dei loro gruppi.
Il controllo dei PID osservati non trova residui; i processi rapidi eventualmente
non campionati restano coperti dal wait/reap del supervisore.
Nessun FIRSTFAILSTOP005: non è avvenuto un fallimento inatteso.

## Toolchain e copertura reale

Versioni dai nuovi dati configure/cache: GNU C++ 15.2.0, CMake 4.2.3, Unix Makefiles.
Qt 6.10.2 è richiesto EXACT e risolto da configure; AutogenInfo registra 6/10.
Non è un qVersion di renderer in esecuzione. Runtime supervisore Python
`3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`, Linux x86_64.
Flag target: `-O2 -fno-fast-math -ffp-contract=off -Wall -Wextra -Wpedantic`;
flags.make completi conservati nel catalogo. Nessuna dipendenza nuova.
OpenGL e header Vulkan trovati da configure sono metadati di build, non prove GPU.

Provati compilazione/link del nuovo host e generazione MOC del nuovo segnale.
I probe del compilatore/Threads/atomics di configure sono parte della build,
non casi GT o regressioni scientifiche. Il guard usa libc nel pre-exec per
PR_SET_PDEATHSIG: non carica la DSO candidata per validarla.

**Zero nuovi casi GT consumati.** Oracle005 e 11 regressioni: NON ESEGUITI.
Nessun paint, export, parser ICC del candidato, picker, QML/GTK, 100 pan/zoom,
A11y/IME o controllo di presentazione. La modalità queued/export/exit è stata
compilata, non ancora dimostrata in esecuzione.

## Binari e provenance

- `build-005/libviewer_core.so`,27216 byte:
  `7f76d4d04ae5a0855dc38dd53d6a4349520fe0171eba2636391ca97c12e00f0a`.
  Identico sia allo SHA richiesto sia ai byte hashati della DSO build004 conservata.
- `build-005/qt_viewer`,227896 byte:
  `2b5270700cae7dd8aea444b867ecc6c76d73bf36676a19044b01757a85cd636f`.
- `build-005/provenance.json`,5210 byte:
  `be62f8149280c889b7cdb10faa0fe1c98613d1fca77c79ec590783a3b77b9dc6`.

Provenance e manifest-after riportano BUILD_ONLY; runtime NOT_RUN. Mappa sorgenti
uguale ai 19 pin005 più protocollo; hash dei due binari conformi e ricontrollati.
L'identità del core rende disponibile il riferimento esplicito a oracle004 per
quel solo binario. Non equivale a oracle005 eseguito o nuovo host GUI validato.

## Quota fisica, riserva e limiti invariati

Misura ROOT/tmp: `st_blocks*512`, inode unici `(st_dev,st_ino)`, NOFOLLOW,
nessuna dereferenziazione symlink. Solo metadati delle altre unità per il budget.

| Fotografia/limite | Byte |
| --- | ---: |
| Globale prima della creazione del turno | 940244992 |
| Preflight esterno immediatamente prima della build | 940355584 |
| Preflight del runner | 940363776 |
| Riserva build effettiva dichiarata dal runner | 100663296 (96 MiB) |
| Globale runner + riserva | 1041027072 |
| Cap globale | 1073741824 (1 GiB) |
| Picco globale osservato durante build | 942084096 |
| Fotografia globale successiva, durante raccolta | 942395392 |

Il monitor esterno conserva 179 campioni, zero campioni persi per race metadata.
Le scansioni non sono atomiche e non coprono ogni istante; le fotografie successive
includono ulteriore scrittura di artefatti e l'eventuale preparazione delle altre unità.
Nessun cap aumentato, fixture ridotta/eliminata o riserva arbitraria sostitutiva.

RSS tree conservativo: configure 92229632 B, build 525885440 B, sotto 536870912 B.
Metodo: somma `max(VmRSS,VmHWM)` sui processi propri; picchi non necessariamente
simultanei, non PSS né misura della cache. Thread massimi osservati 8/14.
AS768 MiB, file32 MiB, log8 MiB, deadline180 s per configure/build, tutti invariati.
Budget buffer256 MiB invariato ma non esercitato come pipeline viewer in questo
turno. Cache e RSS non vengono equiparate. Deadline e tempi di esecuzione non
sono SLA o dati per un benchmark.

## Ricevute e stima del passo successivo, NON ESEGUITO

Record sotto `tmp/m1/b-viewer/execution-turn-005/`: preflight, comando/ambiente,
19 pin PRE/POST, stdout/stderr esterni, exit/PID, catalogo artefatti e handoff finale.
`results-and-artifacts.json` SHA-256:
`08a383cdb74a0cb187f32afd14811a06c6561ff6c2ff362ba5133aeaf17ebb48`.

Sotto `tmp/m1/b-viewer/runs/build-005/`:

- `build.log`: `731214a9422a0bc6c18b24bc72fb035e0eaad3e9b7e8c20e79d36a29edb38e1e`.
- `build-receipt.json`: `c4971786e47de64c902a2a62c7185beb1e42607d7edb31417bda01780de8bb55`.
- `configure-receipt.json`: `c8cd0a0ad49b739454c3fd001ee9ef9c0309d46269ba836a549059682e527806`.
- `manifest-after.json`: `2cb14b2d67f63146bbb1db520b4178d6ed0eaec3fd3b988046f1c883927653e5`.

Nuova stima successiva alla build: 942395392 B globali +33554432 B di riserva
preview32 MiB = **975949824 B**, inferiore al cap. È una stima, **non un preflight
di ammissione GUI**; va ripetuta dopo nuova ammissione e prima del relativo Popen.
Il receipt finale registra anche la fotografia dopo la redazione di questo report.

Prossimo comando, solo con futura ammissione GUI e ambiente privato già predisposto,
dalla radice `/opt/workspace/astroProcessor`:

```bash
/usr/bin/python3 -I -B -S tmp/m1/b-viewer/run.py --phase preview --build-id build-005 --run-id widgets-first-frame-xcb-dpr1-005 --candidate qtwidgets-raster --platform xcb --dpr 1 --qt-scale-factor 1 --preview-mode first-frame-export-exit --core-oracle-reference oracle-004 --fault none --exclusive-admitted
```

Il run richiederà nuovi pin/quota/provenance e verifica dei riferimenti004 prima
del parser ICC/GUI. `runs/widgets-first-frame-xcb-dpr1-005` e `runs/oracle-005`
restano assenti. Nessun comando seguente è stato avviato automaticamente.

**Turno BUILD concluso, slot rilasciato.** M1.b resta aperta: antifallback interno
INCOMPLETE, GTK non ammesso, GUI/callback/export/cleanup del nuovo modo ancora
da provare. Xcb rimane UNQUALIFIED rispetto a X11/XWayland; niente scanout,
colore monitor, A11y/IME o prestazioni dimostrati da questa build.
