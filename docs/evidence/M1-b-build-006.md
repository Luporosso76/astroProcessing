# M1.b: primo BUILD006, FIRSTFAIL durante configure

Esito effettivo: **supervisore BUILD006 exit 2, STOP_FIRSTFAIL**.
Il primo errore registrato è `[Errno 3] No such process` durante la supervisione
del configure CMake. Nessun retry, fix, alias, cambio piattaforma o gate allentato.
Nessuna preview, GUI, oracle006, suite di test o misura di performance eseguiti.
Modello autodichiarato dell'autore: `openai/gpt-6.1-sol`; nessun override modello
o configurazione del failover. Nessuna delega, rete, installazione, Git o indice.

## Ammissione e fonti

Il coordinatore ha ammesso esclusivamente il primo BUILD006. Letta la review
`docs/evidence/M1-b-session-followup-006-review.md`, 17 righe, sessione indipendente
`ses_e88c44174ffezPbywRfQt8wHgI`: READY statico, non PASS build o ammissione GUI.
Nessuna lettura del piano baseline di 4500 righe o modifica a progress.

Verificati PRE/POST tutti i **19 pin byte/hash/mode/nlink**, inclusa preregistrazione:

- manifest006: `ab9994ebc5c73325bac8ba0331d2a8d26fc870fa3f0d98179ff7127ceabbdb7d`;
- run.py: `523afcd6bf7c479385372c2a1ce8575603e615fca7b96b44093d02345b9d5de0`;
- piano: `e1b051c247c32b8620a055032dc6c002d1bdec05d7eaa42da5c37f0ccdd215c8`;
- protocollo di 169 righe: `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.

Ricalcolato l'insieme dei sorgenti letto dal runner: nessuna fonte aggiunta.
Le 509 voci protette dalla preparazione risultano conformi. Il nuovo inventario
PRE/POST copre 518 voci: vecchi fallimenti003/005, oracle004, build005, snapshot,
pin e ricevute non sono stati alterati. Le mappe complete restano nel turno.

Prima del turno e prima del Popen: execution-turn-006, build-006 e runs/build-006
assenti, senza symlink. Oracle006 e preview006 assenti. Directory nuove create
esclusivamente, nessuna rinomina automatica o adozione dei binari precedenti.

## Comando effettivamente eseguito una volta

Directory di lavoro `/opt/workspace/astroProcessor`:

```bash
/usr/bin/python3 -I -B -S tmp/m1/b-viewer/run.py --phase build --build-id build-006 --run-id build-006 --exclusive-admitted
```

HOME/TMP/XDG cache/config/data/state del driver sono privati sotto
`tmp/m1/b-viewer/execution-turn-006/`, verificati owner corrente/mode0700 e path
NOFOLLOW prima del figlio. Il runner crea le directory private omologhe sotto
runs/build-006; TMPDIR dei figli è quel run. Owner/mode verificati dopo lo stop.
File di log/receipt0600. `PATH=/usr/bin:/bin`, `OPENSSL_CONF=/dev/null`,
`LC_ALL=C.UTF-8`, `PYTHONDONTWRITEBYTECODE=1`, `PYTHONNOUSERSITE=1`.
Nessuna variabile DISPLAY/XAUTHORITY o altro metadata di sessione inoltrato,
interrogato o registrato: la build non ne necessita. Nessun file auth/cookie letto.

## Primo errore e processi osservati

| Processo | PID | Esito |
| --- | ---: | --- |
| Supervisore `run.py --phase build` | 833434 | exit2, STOP |
| CMake configure | 833435 | receipt STOP, exit-9 dopo raccolta |
| `cmake --build` del viewer | Non avviato | NOT_RUN |

Diagnosi del supervisore e del configure receipt:

```text
[Errno 3] No such process
```

Il receipt conserva il Popen configure effettivo, non un fallimento precedente
alla creazione del figlio. `watch` passa per il finally di raccolta del proprio
gruppo e wait/reap; il receipt registra exit-9 e `cleanup: wait/reap completed`.
L'exit-9 di cleanup non viene presentata come causa iniziale o prova di OOM.
Il driver ha raccolto il supervisore; nessun PID proprio osservato è residuo.

`configure.log` è presente e **vuoto**: 0 byte. Stderr del supervisore esterno
vuoto; stdout contiene 226 byte e un solo evento STOP.
Non esiste traceback nel raw disponibile. La causa precisa dell'errore ESRCH
non è stata indagata con ulteriori processi, probe o modifiche dopo FIRSTFAIL.
Nessuna diagnosi di race specifica viene attestata senza evidenza.

La directory parziale build-006/CMakeFiles è preservata, inclusi CompilerIdCXX
e tmp. Non sono presenti build.log/build-receipt del target viewer, DSO006,
qt_viewer006 o provenance006. Nessun log o output è stato eliminato o riscritto.

## Versioni: osservato contro richiesto

Runtime reale del supervisore, anche nel manifest-after:
`3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`, Linux x86_64.
La directory CMakeFiles/4.2.3 è stata generata dal nuovo configure: 4.2.3 è il
numero osservato nel suo layout. Nessun nuovo `cmake --version` è stato eseguito.

GNU C++ 15.2.0 e Qt 6.10.2 EXACT restano i vincoli delle fonti CMake immutate,
con `/usr/bin/g++` esplicito nel comando configure. **La loro nuova qualificazione
non è completata:** il log è vuoto e configure non è riuscito. I dati storici
di BUILD004/005 non diventano un configure006 riuscito. Nessun renderer avviato.

## DSO: confronto non applicabile

Le vecchie DSO004 e005 sono state ricalcolate e restano entrambe:
`7f76d4d04ae5a0855dc38dd53d6a4349520fe0171eba2636391ca97c12e00f0a`.
**DSO006 assente**, quindi nessun confronto bit-identico superato o fallito.
Il booleano false nel driver/FIRSTFAIL indica assenza del binario atteso, non
una DSO prodotta con hash diverso. `results-and-artifacts.json` esplicita questo
limite senza alterare le prime ricevute.

Oracle004 resta evidenza storica del solo core004/005 identico. Non viene
adottata come prova di un core006 inesistente o di GUI006. Nessun nuovo caso GT,
caricamento della DSO, paint, export o test del guard XAUTHORITY eseguiti.

## Quota e limiti effettivi

ROOT/tmp misurato con `st_blocks*512`, inode unici `(st_dev,st_ino)`, NOFOLLOW,
senza dereferenziare symlink. Solo metadati delle altre directory per il budget.

| Fotografia/limite | Byte |
| --- | ---: |
| Globale prima della creazione del turno | 954359808 |
| Preflight esterno immediatamente prima del Popen | 954499072 |
| Preflight del runner | 954507264 |
| Riserva build effettiva | 100663296 (96 MiB) |
| Globale runner + riserva | 1055170560 |
| Cap globale invariato | 1073741824 (1 GiB) |
| Picco globale osservato durante esecuzione | 954621952 |
| Globale dopo il driver | 954630144 |

Quota e disponibilità filesystem hanno superato il preflight. Nessun cap
aumentato, taglio di fixture o riserva sostitutiva. Monitor esterno: 3 campioni,
zero campioni persi per race metadata. Scansioni non atomiche, picco osservato;
il receipt finale misura anche la raccolta successiva al report.

RSS tree configure osservato 839680 B, massimo thread osservato 1: campionamento
molto breve, non un picco esatto o prova universale del consumo. Metodo del
runner: somma `max(VmRSS,VmHWM)` sui soli processi propri, best effort.
Limiti immutati: RSS512 MiB, AS768 MiB, file32 MiB, log8 MiB, core dump disabilitato,
deadline180 s per figlio. Budget buffer256 MiB invariato, pipeline non esercitata.
Il primo errore non è la diagnosi del guard risorse né un timeout; il configure
receipt misura 93792420 ns e il driver 280618802 ns. Non sono benchmark o SLA.

## Artefatti e consegna

Sotto `tmp/m1/b-viewer/runs/build-006/`:

- `configure.log`: `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`;
- `configure-receipt.json`: `e0109e84bb09601d8da356dfe9016dc236dcbb2ef907c6e64af768b89f6bf192`;
- `manifest-after.json`: `b8e19a0b5b82cfa94245e7e7cc876b06337950a113bc6dda2b04d23955d075c6`.

Sotto `tmp/m1/b-viewer/execution-turn-006/`:

- `FIRSTFAILSTOP.json`: `6dc02dec4ec8de8f5ab4855a689e9190db6b73f0f8b2b161cafcb8b53087a730`;
- `results-and-artifacts.json`: `5b7fe919dcd9e86021ed7d2eb60639b0039470a546c0f1577fc4c63912239944`.

Conservati comando/ambiente privato, sorgenti PRE/POST, PID, raw/receipt e inventario
dei soli nuovi output parziali. Nessuna modifica retroattiva delle prove precedenti.
**Turno concluso a FIRSTFAIL, slot rilasciato.** Nessun retry o preview ammessi
da questa consegna. M1.b resta aperta, build006 non chiusa e antifallback interno
INCOMPLETE. Il passo successivo richiede un turno distinto; qui nessun fix proposto
come già eseguito o comando successivo avviato automaticamente.
