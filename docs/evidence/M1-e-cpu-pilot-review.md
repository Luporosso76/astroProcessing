# M1.e: review indipendente del primo lock CPU

Esito: READY soltanto per GPU VALIDATE del caso congelato, previa ammissione del coordinatore. Nessun blocker rilevato. Non autorizza FAULTS, CONFIRM, altre tuple o una scelta di stack.

Review di `ses_e8c3e6e74ffeorhNv9IT7nj9CR`, OpenAI GPT-6.1 Sol autodichiarato dal runtime, non attestato. Verbale persistito dal coordinatore il 25 ottobre 2026; costi/token non esposti. Nessuna scrittura o esecuzione candidata da parte del revisore.

## Binding e copertura

Lock verificato: `tmp/m1/e-vulkan/pilot-affine-004/reference-lock.json`, SHA-256 `b479a22a75af76b5e494f23c1af1d0ccdfc19ffe36d1eb6ef043a51343a26e79`.

Letti integralmente report di 155 righe, lock, pilot/report, trace, observer, verification e incidente metadata, PREP004 e preregistrazione; sezioni pertinenti del runner e dell'engine. Altre fonti verificate mediante hash. Ricalcolati otto hash sorgenti, PREP004, engine, tre shader, indice/manifest del caso, protocolli, input/GT e nove artefatti della verifica. Pilot e report sono identici.

Verificati tutti i 323 input f32 e numeratori int64: generatore corretto, GT `(3*u+512)/4096`, numeratori osservati da -1021 a 2045 esattamente rappresentabili. Tolleranza zero invariata. Gli output CPU non sono persistiti: i sette verdetti dipendono dal confronto in-process effettivo, non da un nuovo controllo indipendente dei buffer prodotti.

## Risultati confermati

Nove ingressi CPU, coerenti con due warm-up e sette round. Sette righe del report identiche ai raw. Mediane in ns: totale 38922, kernel 871, GT 17592, CPU user 61000, system 0. Nessun intervallo comparativo o migliore CPU generale deriva da questi dati.

`T_total` include contratto, tracing, kernel e finitezza, esclude GT. CPU user/system include verifica e tracing, con risoluzione microsecondi convertita in ns. I raw warm-up non sono conservati e non sono campioni verificabili separatamente.

Il contatore CPU deriva dal percorso e dal delta per richiesta. Query GPU zero non applicabili, ready con device null; pin AMD destinato alla successiva verifica hardware. Driver raccolto con exit 0, un solo engine CPU osservato e receipt worker reaped senza errori. Manca il PID nella receipt interna: associazione tramite poll esterno, non audit waitpid completo o prova universale anti-orphan.

## Incidente e limiti

Il precedente controllo metadata fallito riguarda l'indice `docs/evidence/README.md`, non il README Vulkan della ricetta. Errore e traceback sono conservati; binding contrattuali attuali verificati immutati. Non è evidenza di un errore numerico ignorato o di un pilot ripetuto. La continuità immediata dei 185 file resta un'attestazione dell'osservatore, non 185 hash ricalcolati nella review.

Un controllo metadata ausiliario del revisore ha avuto KeyError su un campo opzionale; è stato corretto il solo controllo read-only, senza rieseguire candidati. I 18 mock supervisor restano mock, non hardware proof. Riferimento CPU native-only condizionato; nessun riuso confermativo dei sette campioni.
