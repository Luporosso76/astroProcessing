# M1.b: prima preview005, FIRSTFAILSTOP su avvio xcb

Esito effettivo: **preview tentata una volta, supervisore exit 2**.
Il figlio GUI termina con **SIGABRT, exit -6** durante l'avvio della piattaforma
xcb. Nessun frame, callback canvas o export osservati. Non è un timeout né un PASS.
Nessun retry, patch, cambio QPA/renderer/DPR/ICC, installazione o riduzione dei limiti.
Modello dichiarato dal runtime dell'autore: `openai/gpt-6.1-sol`.

## Ammissione e comando effettivo

Il coordinatore ha ammesso una sola preview esclusiva dopo la consegna BUILD005
e la review statica indipendente. Nessun altro candidato o benchmark è stato avviato.
Directory di lavoro: `/opt/workspace/astroProcessor`.

```bash
/usr/bin/python3 -I -B -S tmp/m1/b-viewer/run.py --phase preview --build-id build-005 --run-id widgets-first-frame-xcb-dpr1-005 --candidate qtwidgets-raster --platform xcb --dpr 1 --qt-scale-factor 1 --preview-mode first-frame-export-exit --core-oracle-reference oracle-004 --fault none --exclusive-admitted
```

Nuovi output esclusivi: `tmp/m1/b-viewer/preview-turn-005/` e
`tmp/m1/b-viewer/runs/widgets-first-frame-xcb-dpr1-005/`.
Entrambi assenti prima del turno, anche come symlink. Nessun autorename o riuso.

## Pin e preflight verificati prima del figlio

Manifest005 SHA-256:
`8a2b8b284c6083ef3f8dd1a8b95fe20f8439cce7a19199999f4c004ff3e958df`.
**19 pin byte/hash/mode/nlink conformi PRE/POST**, inclusa preregistrazione005.
Protocollo padre invariato: 169 righe, SHA-256
`1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.

Binari e provenance verificati PRE/POST:

- host 227896 byte: `2b5270700cae7dd8aea444b867ecc6c76d73bf36676a19044b01757a85cd636f`;
- core 27216 byte: `7f76d4d04ae5a0855dc38dd53d6a4349520fe0171eba2636391ca97c12e00f0a`;
- provenance: `be62f8149280c889b7cdb10faa0fe1c98613d1fca77c79ec590783a3b77b9dc6`.

Il runner ha verificato il riferimento esplicito oracle004 e lo ha registrato
nel manifest: stessa DSO, manifest oracle004 SHA
`4197fa708d024b5488c56a945b8b91ae0f4186cde02397ec3e06914e02ff06d6`,
provenance build004 SHA
`3aa7af196f3049055cd01c5e9e152ff0b4c52caf6362c2e984d48cf514afb1c9`.
Si tratta della prova precedente del solo core, non di un oracle005 eseguito.

Inventario PRE/POST: **482 voci protette** conformi, inclusi sorgenti, build005,
build/oracle004, snapshot, report205/165 e ricevute storiche. Nessuna modifica
a piani, progress o altri namespace. Nessun Git, indice, rete o delega.

I percorsi attendibili sono aperti per componenti NOFOLLOW. Directory private
con owner corrente e mode0700; file raw/receipt0600. HOME/TMP/XDG cache/config/
data/state sono privati ROOT, prima nel nuovo outer e poi nel run del figlio.
Nessun profilo personale, file di autorizzazione o configurazione di sistema
letto dall'agente. Nessun tentativo di recuperare credenziali per il display.

Da ambiente sono stati inoltrati soltanto DISPLAY e XDG_RUNTIME_DIR per il caso
xcb; non WAYLAND_DISPLAY o DBUS_SESSION_BUS_ADDRESS. Il ledger registra solo
i nomi e la presenza, mai i valori. Qt ha incluso il display nella propria
diagnosi nativa: quel valore è omesso qui e in chat; il raw0600 rimane integro
nel run privato, come richiesto per FIRSTFAILSTOP. Non è stato alterato o ripulito.
`OPENSSL_CONF=/dev/null`, Python isolato senza bytecode, PATH limitato ai tool
installati. Nessuno screenshot, input globale o titolo di finestra estranea.

## Processi ed errore osservato

| Processo | PID | Exit | Esito e cleanup |
| --- | ---: | ---: | --- |
| Supervisore preview | 285689 | 2 | STOP; wait/reap driver |
| Parser ICC sintetico previsto dal runner | 285690 | 0 | COMPLETED; wait/reap completed |
| Candidato Qt Widgets xcb | 285691 | -6 | SIGABRT; wait/reap completed |

Il parser ha accettato la sola fixture sintetica di 688 byte, SHA-256
`4afc10548a2a6da4b1b9d88f44ad3d0778ece3c9415e59296ae9a7d25c37738e`.
È un controllo del parser, non calibrazione o prova del rendering GUI.

Prime diagnosi native in `preview.log`:

```text
Authorization required, but no authorization protocol specified
```

Segue il fallimento di connessione xcb al display. Qt emette inoltre:

```text
qt.qpa.plugin: From 6.5.0, xcb-cursor0 or libxcb-cursor0 is needed to load the Qt xcb platform plugin.
```

La presenza di questo messaggio **non dimostra l'assenza della libreria**.
Non sono stati eseguiti ulteriori probe di autorizzazione/dipendenze o installazioni.
La diagnosi certa del turno è l'avvio xcb fallito con SIGABRT, preceduto dal
messaggio di autorizzazione. Nessun cambio automatico a Wayland/offscreen o altro
backend. L'elenco delle piattaforme disponibili nel raw non prova tentativi
di selezionarle. Gli interni del framework restano NOT_INSTRUMENTED.

`preview.log`: 581 byte, 9 righe, stdout/stderr nativi combinati. Nessun warning
font osservato in questo log. L'intero raw, incluse diagnosi del plugin e lista
piattaforme, rimane preservato. Stderr del supervisore esterno vuoto; stdout
contiene STOP con `OWN_CHILD_EXIT_-6: see preview.log`.

## Checkpoint e output: cosa non è stato raggiunto

Conteggio di tutti gli 8 checkpoint richiesti: **zero** per recipe,
renderer_observed, input, frame_ready, paint_event_callback, viewport_exported,
first_frame_export_exit e controller_released.

| Verifica | Osservazione effettiva |
| --- | --- |
| Renderer Raster reale | NON OSSERVATO |
| DPR reale | NON OSSERVATO; richiesto 1 |
| Generazione corrente1 | NON OSSERVATA |
| Paint del proprio canvas | NON OSSERVATO |
| Export BGRA+JSON e ACK durevole | NON ESEGUITI |
| `viewport-1.bgra` / `viewport-1.json` | Entrambi assenti; SHA export non applicabile |
| Controller worker join/release | NON OSSERVATI |

L'abort avviene nell'avvio QApplication, prima del checkpoint recipe e della
costruzione Controller nel percorso pinnato. La raccolta dei processi è riuscita,
ma **non equivale** a una prova del nuovo percorso controller_join/release.
Non è stato creato un export parziale: la fixture ICC e i log sono gli output
preservati. Nessun core dump previsto, RLIMIT_CORE=0.

La connessione xcb non è riuscita: nessuna qualifica X11 nativo/XWayland.
Nessun dato di scanout, colore monitor, A11y/IME, prestazioni o latenza presentata.
Zero nuovi casi GT, nessun oracle005, GTK/QML, 100 pan/zoom o benchmark.

## Quota, RSS e limiti

Metrica ROOT/tmp: `st_blocks*512`, inode unici `(st_dev,st_ino)`, NOFOLLOW;
symlink non dereferenziati. Solo metadati delle altre directory per il budget.

| Fotografia/limite | Byte |
| --- | ---: |
| Prima della creazione dell'outer | 943308800 |
| Preflight esterno prima del Popen | 943448064 |
| Riserva preview32 MiB | 33554432 |
| Globale esterno + riserva | 977002496 |
| Preflight del runner | 943456256 |
| Picco globale osservato durante esecuzione | 943517696 |
| Cap invariato1 GiB | 1073741824 |

Monitor esterno: 9 campioni, nessun campione perso per race metadata. Metrica
osservata, non scansione atomica né massimo universale. Il receipt finale
fotografa anche lo spazio dopo raccolta/report.

RSS tree osservato: parser 1015808 B, GUI 36302848 B. Il caso breve può sfuggire
in parte al campionamento: sono osservazioni best effort, non picchi esatti.
Guard RSS 536870912 B, AS 805306368 B, file 33554432 B, log 8388608 B, deadline 180 s.
Budget buffer256 MiB invariato, pipeline frame non esercitata. Nessun guard ha
causato il fallimento e nessun limite è stato aumentato. Runtime supervisore
Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`, Linux x86_64.
Qt6.10.2 rimane il pin della build; qVersion del percorso GUI non raggiunto.
Tempo esterno 699360109 ns, controllo di esecuzione, non benchmark o FPS.

## Ricevute e handoff stabile

Sotto `tmp/m1/b-viewer/runs/widgets-first-frame-xcb-dpr1-005/`:

- `preview.log`: `8adefc67174cabe57cc3719c0192e20fba757ebcd3a72725057a4171b88489fc`.
- `preview-receipt.json`: `ad1d53b01f2baa7c35382aecaa25ce460cbfea4f628d1db79805113090138f1d`.
- `icc-parser-receipt.json`: `027787eba61229ef45d219dbb47aa27eb15f06fc784251bc20596161e23acb7e`.
- `manifest-after.json`: `72126ee204875857d7151d15ec13763bd6f4d1370430a6d5ce7c4fba48b1a03f`.

Sotto `tmp/m1/b-viewer/preview-turn-005/`:

- `FIRSTFAILSTOP.json`: `cfb6cadb42e830156eb8f27a551102c2b91721ab9dc9bd0a1163d0056a3f3df0`.
- `results-and-artifacts.json`: `7ca91cc6a8c30de4058241ed842e8daeee579d53465bf88102f167a549ff791b`.

Sono conservati comandi, metadata ambientali senza valori, identity/hash PRE/POST,
raw, exit/PID, checkpoint mancanti, assenza export e cleanup. Nessun processo
proprio osservato risulta residuo al controllo finale. Nessun processo estraneo
è stato segnalato o terminato.

**Turno fermato a FIRSTFAIL, slot rilasciato.** Il ramo preview resta bloccato
dall'avvio xcb fallito. Nessun ulteriore comando o correzione eseguiti.
M1.b rimane aperta; gate antifallback interno INCOMPLETE, nuovo paint/export/
exit/cleanup ancora non provati. La review di questa evidenza è separata dal turno.
