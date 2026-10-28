# M1.b: build004 e oracle004 supervisionato

Esito: **build completa exit 0, oracle supervisionato exit 0**.
Tutte le 11 regressioni e i casi numerici correnti sono conclusi senza skip.
Questo esito non chiude M1.b e non prova rendering GUI o prestazioni.
Nessuna correzione, retry, modifica a sorgenti/gate/mode o preview in questo turno.
Autore: modello dichiarato dal runtime `openai/gpt-6.1-sol`.

## Ammissione e pin

Il coordinatore ha comunicato slot esclusivo libero dopo F005 e review004 READY
di `ses_e8c14fabaffeCEsreEF1hHtERa`, modello GPT6.1Sol autodichiarato.
La review non sostituisce gli esiti runtime registrati qui.

Manifest004 SHA-256:
`43c90d9ef791f423f6c43c2b8540ef978152996f8e10b998a9a202387f525f52`.
Diff004 SHA-256:
`63d153a8a278383d1e63f7b7c2e0cf5d44a248ac319a566728b9a636a7d4cbed`.
**19 pin byte/hash/mode conformi**, prima e dopo build, prima e dopo oracle.
Protocollo padre: 169 righe, SHA-256 invariato
`1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.
Il piano di 4482 righe non è stato letto. Piani, progress e altre unità non modificati.

Prima del turno verificata l'assenza, anche come symlink, di execution-turn004,
build004, runs/build004, runs/oracle004 e di questo report. Prima di ogni fase,
nuovo controllo dell'assenza dei relativi output. Directory nuove create
esclusivamente; nessuna rinomina automatica e nessun output003 adottato come004.

Le 156 voci protette dal follow-up precedente, più 5 artefatti/report004,
rimangono conformi per hash/mode: **161 voci**. Restano intatti failedbuild003,
execution-turn003, snapshot003, report183 e DSO parziale003.
Mappe complete PRE/POST in `tmp/m1/b-viewer/execution-turn-004/`.

## Comandi e ambiente effettivi

Directory di lavoro `/opt/workspace/astroProcessor`, in quest'ordine:

```bash
/usr/bin/python3 -I -B -S tmp/m1/b-viewer/run.py --phase build --build-id build-004 --run-id build-004 --exclusive-admitted
/usr/bin/python3 -I -B -S tmp/m1/b-viewer/run.py --phase oracle --build-id build-004 --run-id oracle-004 --exclusive-admitted
```

Fra i comandi: verifica exit/provenance/hash della build e **nuovo preflight
oracle**. Nessun flag aggiunto al runner e nessuna selezione latest.
HOME/TMP/XDG cache/config/data/state del supervisore esterno sono privati sotto
`execution-turn-004/`; i figli usano directory private sotto il rispettivo run,
come registra il sorgente pinnato. Nessun display o bus personale ereditato.
`PATH=/usr/bin:/bin`, `OPENSSL_CONF=/dev/null`, `LC_ALL=C.UTF-8`,
`PYTHONDONTWRITEBYTECODE=1`, `PYTHONNOUSERSITE=1`. I manifest conservano anche
`QML_DISABLE_DISK_CACHE=1` e le impostazioni GIO/GTK locali del runner.

Nessuna rete, installazione, Git, indice, lettura di profili personali o modifica
di sistema. Per il budget globale sono stati letti soltanto metadati filesystem
delle altre directory tmp, non i loro sorgenti/dataset o risultati.

## Quota fisica e limiti

Metrica: `st_blocks*512` sotto ROOT/tmp, inode unici `(st_dev,st_ino)`, directory
aperte NOFOLLOW, symlink non seguiti. Cap invariato **1073741824 B**.

| Fotografia | Byte |
| --- | ---: |
| Globale prima della creazione del turno | 864333824 |
| Preflight esterno build | 864407552 |
| Preflight build del runner | 864415744 |
| Riserva build pinnata | 100663296 (96 MiB) |
| Totale preflight runner build + riserva | 965079040 |
| Picco globale osservato durante build | 866103296 |
| Nuovo preflight esterno oracle | 866131968 |
| Riserva oracle pinnata | 33554432 (32 MiB) |
| Totale nuovo preflight esterno + riserva oracle | 899686400 |
| Preflight oracle del runner | 866140160 |
| Picco globale osservato durante oracle | 866398208 |

Monitor esterno: 185 campioni build e 810 oracle; zero campioni persi per race
metadata. Le fotografie non sono scansioni atomiche e non sono una prova
universale anti-OOM. I KiB di differenza includono receipt e log. La raccolta
successiva degli artefatti/report aggiunge spazio, misurato nella ricevuta finale.
Nessun cap aumentato e nessuna fixture eliminata, ridotta o cambiata.

Limiti invariati: RSS512 MiB best effort, AS768 MiB, file32 MiB, log8 MiB;
deadline180 s per configure/build, 600 s per oracle. La deadline non è uno SLA.
Memfd è RAM anonima, non scratch su filesystem esterno; RSS e quota scratch
sono misure distinte. Pool tile124387328 B e ricetta del generatore invariati.

## Build completa e versioni osservate

| Processo | PID | Exit | Cleanup |
| --- | ---: | ---: | --- |
| Supervisore build | 234126 | 0 | wait/reap driver |
| CMake configure | 234127 | 0 | wait/reap completed |
| CMake build | 234200 | 0 | wait/reap completed |
| Supervisore oracle | 235541 | 0 | wait/reap driver |
| Figlio oracle Python | 235542 | 0 | wait/reap completed |

CMake **4.2.3**, GNU C++ **15.2.0**, generatore Unix Makefiles: dati reali di
CMakeCache/configure. Qt **6.10.2** richiesto EXACT e risolto da configure;
AutogenInfo registra major6/minor10. Non è un qVersion di renderer in esecuzione.
Python runtime: `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`, Linux x86_64.
Configure trova OpenGL e header Vulkan: dipendenze di build, non prova renderer.

Il log termina con `[100%] Built target qt_viewer`, senza errori o warning.
La compilazione fresca del core è registrata: lo SHA uguale alla DSO003 non
deriva da una copia o riuso dell'output fallito.

- `build-004/libviewer_core.so`, 27216 byte:
  `7f76d4d04ae5a0855dc38dd53d6a4349520fe0171eba2636391ca97c12e00f0a`.
- `build-004/qt_viewer`, 214928 byte:
  `eaa2e5c85fc288e003b58ca72c6f21878577a06b9bd28f99aca21b6fee4cc6e1`.
- `build-004/provenance.json`, SHA PRE/POST oracle:
  `3aa7af196f3049055cd01c5e9e152ff0b4c52caf6362c2e984d48cf514afb1c9`.

RSS conservativo del tree configure: 93868032 B; build: 520265728 B; oracle: 217673728 B.
Massimo thread osservato: 8/14/2 rispettivamente. Metodo pinnato: somma di
`max(VmRSS,VmHWM)` sui processi propri, con possibili picchi non contemporanei;
non PSS, non cache tile. Tutti sotto il guard di 536870912 B, senza riduzioni.

## Oracle: risultati reali e limiti della registrazione

Figlio 235542 con capability directory/pipe, parent PID e binding DSO verificati.
Handshake ready/released e stdin/stdout non bloccanti risultano veri nel receipt.
Log combinato di 5188 byte, exit 0, evento finale `oracle_completed`.
L'assenza di output del supervisore oracle non indica mancata esecuzione:
il runner ritorna prima del proprio emit finale e conserva tutto nel log figlio.

Il log riporta **11/11 regressioni ok**, zero skip: broken handshake, directory
capability C++/export duplicato, ICC symlink/hardlink/corrotto, handshake valido,
timeout, exit non nullo, capability Python, pin sealed, RSS simulato, spawn
simulato e ownership worker. I fake di RSS/spawn/lifecycle sono dichiarati tali.
Le exit2/7/-9 dei figli negativi sono **attese dalle regressioni**, non fallback
o fallimenti inattesi del turno. Dieci Popen reali nelle regressioni hanno receipt
di raccolta; il caso spawn simulato registra zero Popen e cleanup senza handle.

Le 18 combinazioni zoom denominatore4/8/16 × DPR1/2 × frame0/1/1999 compaiono
tutte come eventi riusciti. Ogni confronto BGRA, istogramma e inventario tile è
exact; nessun epsilon introdotto. Eventi successivi confermano cancel in-flight,
latest-wins generazione1022 ed eviction:168 tile distinti,146 slot,22 eviction
prima del revisit. Il revisit richiede anche 12 reload/12 eviction e snapshot exact.

I tre snapshot originali, 18 punti campionati con 54 parole RGB float confrontate
per bit, stale generation, missing tile e anchor STF precedono quei messaggi.
Sono asserzioni effettive del codice pinnato raggiunte con successo, **non eventi
raw individuali**. Il percorso completo conclude 23 confronti di snapshot: 3
originali, 18 scale, ultima generazione e revisit. Invalidi, padding, maschere e
split restano distinti. Python avviato senza `-O`, ambiente isolato: assert attivi.

GT consumato: SYN-VIEW sintetico, oracle indipendente integer/Fraction;
**zero dataset camera/raw**. Non è un test di input scientifici reali.

Il binding Native ha eseguito ABI2/versione/dimensioni, 25 offset campi, 2 alignment
e 11 valori layout/stride per ciascuno dei 2 engine. Non ci sono dump raw di ogni
valore: il successo è attestato dal percorso pinnato, dai confronti runtime e
dalla conclusione dell'oracle, non da un controllo AST chiamato test numerico.

Osservazione esterna limitata alle mappe dei processi propri: il figlio scientifico
235542 mappa `/memfd:m1-viewer-verified (deleted)`, device00:01/inode9952429,
anche eseguibile. Native verifica SHA, copia/seal e carica quel fd, non riapre il
nome mutabile della DSO. La regressione del pin conferma sigilli e stabilità
dei byte dopo sostituzione del nome. Non è un audit di tutte le dipendenze.

I 5 probe Qt delle regressioni sono PID 235549/235550/235552/235557/235558:
solo I/O sintetico o rigetto ICC, prima di QApplication. Nessuna finestra,
widget, display capture o profilo personale. Il loader di questi eseguibili
mappa la DSO tramite RPATH del build: **non è il pin memfd Python**. Non si
estende quindi la prova del pin al loader/dependency graph Qt completo.

Mappe osservate anche per `libcrypto.so.3` e `libssl.so.3` installate sotto
`/usr/lib/x86_64-linux-gnu/`: dipendenze CPU di Python/hash/import dei test e Qt,
non un renderer o traffico di rete. Si registrano i path/ABI3 effettivi, non una
versione patch OpenSSL dedotta. Configurazione OpenSSL fissata a `/dev/null`.
Nessuna import-trace completa o sandbox supply-chain viene dichiarata.

## Evidenze, cleanup e handoff

Sotto `tmp/m1/b-viewer/`:

- `runs/build-004/build.log`:
  `731214a9422a0bc6c18b24bc72fb035e0eaad3e9b7e8c20e79d36a29edb38e1e`.
- `runs/build-004/build-receipt.json`:
  `82b9d03ee480c7c5ab64bf63b93cb11ef9633e896e540df8e21700839bc4cc92`.
- `runs/oracle-004/oracle.log`:
  `04d67344710f1d176815c9d4a1ba576ab572fc81d3b523119e246d15271ffa6f`.
- `runs/oracle-004/oracle-receipt.json`:
  `3a710ce9d30f1b3c5a87180c2f0e04c1971d3447e43f9ed67679540735016488`.
- `runs/oracle-004/manifest-after.json`:
  `4197fa708d024b5488c56a945b8b91ae0f4186cde02397ec3e06914e02ff06d6`.
- `execution-turn-004/results-and-artifacts.json`:
  `cc840976a7c1eff789c289583a110de9603ffec255c49924fbd9d9f6df4336d2`.

Il catalogo comprende hash/dimensioni/mode, receipt negativi attesi e link delle
sole fixture di regressione; i symlink non sono dereferenziati nell'inventario.
Driver conserva comandi/ambiente/PID osservati, mappe selezionate, output esterni,
nuovi preflight e 19 pin PRE/POST. Log figli combinano stdout/stderr; stderr dei
due supervisori esterni vuoto. Non esiste FIRSTFAILSTOP004: nessun FAIL inatteso.

Receipt attestano wait/reap; il controllo finale dei soli PID osservati non trova
processi residui. I Popen rapidi possono sfuggire al campionamento esterno, ma
sono nei receipt del supervisore. Nessun processo estraneo è stato segnalato.
Sorgenti e artefatti003 restano intatti; niente autofix, gate allentati o retry.

**Turno concluso, slot runtime libero.** M1.b resta aperta: renderer antifallback
INCOMPLETE, GTK non ammesso, GUI/IME/A11y/picker/pan/zoom e presentazione non
provati. I probe ICC sintetici non sono calibrazione o qualifica colorimetrica.
Tempi receipt sono controllo deadline/esecuzione, non FPS, benchmark o statistiche.
Nessuna preview automatica. La review di questa evidenza resta indipendente.
