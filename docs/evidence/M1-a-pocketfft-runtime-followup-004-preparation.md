# M1.a pocketfft runtime followup 004: sola preparazione

Stato del deliverable: `PREPARED_NOT_EXECUTED_REVIEW_REQUIRED`. Nessun PASS dei test o dei checksum host. Questo documento non chiude il finding MED e non autorizza i 232 casi FFT.

## Fonti e perimetro

- Mandato del coordinatore: nuovo namespace `tmp/m1/fft-pocket/runtime-inventory-followup-004`, preservando 003, report, binario, fonti e piani. Preparazione con AST/hash soltanto; review indipendente prima dei test.
- Review comunicata dal coordinatore: sessione `ses_e8828fcf4ffeiTrPzgs5tRZ8md`. Finding MED su `runtime-inventory-003/verify.py:128-140`: conteggi e inclusione di requisiti già estratti, senza ricostruzione indipendente completa. Negativi di omissione assenti. Il coordinatore riferisce conferma manuale di 5 runtime, 6 ELF, 13 archi NEEDED e nodi versioni; non è un'esecuzione del nuovo verificatore.
- Ancore SHA256 fornite per sei record 003: handoff, report, artifact manifest, inventario normalizzato, verification e final checks. I valori esatti sono in `prepare_static.py`, e il record generato `preparation-manifest.json` associa ciascun valore al percorso. Nessuna attestazione dei runtime host o di tutta la storia.
- Input effettivi della verifica futura: sei stdout readelf, stdout della cache ldconfig e tre tabelle MD5 locali, autenticati tramite l'artifact manifest 003. Le copie salvate restano immutabili.
- Fonte del limite LOW: `runtime-inventory-003/verification-attempt-001-failure.json`, `receipt_basis`: `tool output transcribed, not original stderr file`. La diagnosi riguarda `GPL.` con il punto finale. Lo stderr originale non è disponibile e non viene ricreato.

Non è stato riletto il piano scientifico esteso. Non sono stati modificati progress, index, piani canonici o altri namespace. Nessun accesso nuovo a dati personali, sistema o rete.

## Preparazione consegnata

`verify.py` contiene codice di verifica indipendente dal collector: grammatica stretta per sezioni e righe, conteggi delle sezioni e dei gruppi, ricostruzione degli archi dalla lista NEEDED e dalla cache salvata. Ricostruisce anche requirements per provider e version definitions. Confronta record esatti, comprese molteplicità e campi, anziché soltanto conteggi o sottoinsiemi. Non importa e non chiama il collector o il verificatore 003. Condivide le osservazioni readelf, non costituisce una seconda acquisizione o un decoder ELF indipendente.

Il percorso checksum futuro legge soltanto cinque percorsi canonici espliciti. Confronta SHA256 e stat con l'inventario pinned, ricalcola MD5 dai byte effettivi e confronta la voce esatta della tabella salvata. Infine confronta tutti i record dichiarati, non il solo booleano `match`. Queste letture host non appartengono alla preparazione.

`test_verify.py` prepara 21 casi: tre positivi e 18 negativi. Comprendono omissione di arco, sostituzione con duplicato a conteggio invariato, omissione di un requisito da entrambe le tabelle derivate, omissione di un record provider, digest dichiarato o reale diverso e record checksum omesso. I mock sono copie private in memoria. I checksum positivi dei test sono sintetici e non attestano i runtime reali.

Le attese statiche di 6 oggetti, 13 archi, 13 gruppi provider, 54 requisiti e 5 checksum restano preregistrate, non risultati del nuovo codice. I 379 check storici comprendevano 132 confronti di metadati protetti e non erano 379 check ABI.

## Controlli ammessi in questa consegna

Comando di sola preparazione:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-inventory-followup-004/prepare_static.py
```

`prepare_static.py` analizza AST dei tre script senza importarli. Confronta hash delle sei ancore e di undici altri input nominati, inclusa la ricevuta failure001. Registra hash e conteggi di righe dei sei file sorgente locali nominati. I conteggi indicano file hashati, non copertura di lettura dell'agente.

La prova effettiva del comando è `preparation-checks.json`; il suo stato atteso è `PASS_AST_AND_NAMED_LOCAL_HASHES_ONLY`. Il record non è un test ABI. `preparation-manifest.json` conserva pin, byte e righe degli input e dei sorgenti. Il comando non importa il candidato, non esegue il nuovo parser ELF, non lancia test, worker, build o tool di metadati. Non effettua letture dei runtime host. Se il comando fallisce, questa preparazione non ha il relativo esito statico.

Ambiente dichiarato: Linux, root di lavoro `/opt/workspace/astroProcessor`; invocazione Python isolata `-I -B -S`. Modello dichiarato dall'ambiente: `openai/gpt-6.1-sol`; nessun override manuale. Versione precisa dell'interprete, modello di eventuali sostituzioni, costi e token non esposti da questa operazione non vengono dedotti.

## Turno futuro non eseguito

Piano locale: `runtime-inventory-followup-004/PLAN.md`. Fonte unica di budget, conteggi attesi e comandi: `preregistration.json`. Budget massimo complessivo due minuti, due processi, nessun retry automatico. Log originali e temporanei devono restare nel nuovo namespace; nessuna creazione di cache Python. Il coordinatore verifica prima il margine del budget disco.

Primo comando futuro, dopo review indipendente del testo stabile:

```sh
/usr/bin/timeout --signal=TERM --kill-after=5s 55s /usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-inventory-followup-004/test_verify.py
```

Solo se i test riescono nel budget, eseguire il comando verifier preregistrato e conservare stdout, stderr, exit code e hash. Un errore ferma il ramo; nessun parser alternativo o percorso sostitutivo.

## Gap e limiti ancora aperti

La preparazione non ha eseguito alcuno dei 21 test o il verificatore reale. Una review indipendente resta necessaria prima del turno. La presenza di version nodes non prova simboli esportati, relocation, IFUNC o binding live. La selezione dalla cache salvata resta condizionata e non attesta i symlink attuali. MD5 del database locale non è una firma indipendente. Nessun PASS scientifico, adozione, autorizzazione di distribuzione o conclusione sulla licenza deriva da questo deliverable.

Lo stderr originale di failure001 resta mancante. L'unica prova disponibile è la diagnosi trascritta dichiarata nella ricevuta storica. Non è possibile soddisfare retroattivamente un requisito di stderr originale senza inventare evidenza; questo gap non viene chiamato PASS.
