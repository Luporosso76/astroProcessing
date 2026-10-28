# Review della copertura lifecycle del contesto store

Esito: **NON READY per la suite42**. Review ordinaria indipendente dell'agente `reviewer`, sessione `ses_e88edd651ffexUJuuZVSYdl3bo`, senza override del modello e con sostituti gestiti dalla configurazione. Modello autodichiarato `openai/gpt-6-astra`, non attestato; costi non esposti.

## Rilievi prima dell'esecuzione

1. `context-lifecycle-tests/test_context_lifecycle.py:54-69`: la fixture sostituisce `_phase_bindings`, ma l'emittente reale invoca prima `closure` per `materialize`. Con `PINS={}` manca `INDEX`: il test fallirebbe nella preparazione, prima del controllo lifecycle previsto. Predisporre anche la closure scientifica sintetica, mantenendo reali emissione e gate.
2. `context-lifecycle-tests/run_suite.py:164-171`: due `wait()` senza timeout dopo kill rendono illimitato il cleanup. Attendere entro un limite e registrare separatamente raccolta riuscita e child irrisolto. Quest'ultimo impone STOP e non libera lo slot: nessuna garanzia universale di terminazione OS.

Sono correzioni delle nuove fixture e del supervisore, non evidenza di un difetto scientifico nello store v2. La fonte v2 e i suoi24 test restano immutati. Non si rimuovono scenari per ottenere PASS.

## Copertura e limiti

Letti integralmente test271 righe, launcher197, receipt47, piano65, report139. Riutilizzata la precedente lettura dello store v2 e dei24 test. Il revisore ha confrontato i valori di hash dichiarati, senza ricalcolarli sui byte attuali; non viene attestata una nuova verifica dei sei pin.

Positivi statici: sorgenti autenticati e compilati dagli stessi byte, contesti emessi dall'API reale, diagnosi specifiche e invarianti dei piccoli file, prerequisiti scientifici sintetici espliciti. Il budget16MiB è aritmetico, non quota fisica dimostrata.

Zero import candidati, test, processi, ZIP, letture del payload delle168 fixture, scritture del revisore, Git, rete, indici o deleghe. Occorrono nuovo delta, review e distinta ammissione. Nessuna fase archive, verify, retire, materialize o release è autorizzata da questo verbale.
