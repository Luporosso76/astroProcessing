# M1.b: review indipendente della correzione build004

Data: 25 ottobre 2026. Record del coordinatore della review del coder indipendente, sessione `ses_e8c14fabaffeCEsreEF1hHtERa`, modello autodichiarato `openai/gpt-6.1-sol`. Costi non esposti.

READY statico per tentare BUILD004. Nessun finding nel delta: non è un PASS di compilazione o dell'oracle.

- Verificate tutte le dodici chiamate dell'helper `logEvent`, senza alterare payload JSON o eventi virtuali Qt.
- Blocco `qt_viewer.cpp:177-183`: ordine di lock, wait, stop, job e waiting e durata del lock invariati.
- Ricostruito integralmente il passaggio 408 a 413 righe: soltanto rename e disposizione delle istruzioni.
- Manifest004 SHA-256 `43c90d9ef791f423f6c43c2b8540ef978152996f8e10b998a9a202387f525f52`, diciannove pin conformi, unico sorgente cambiato `qt_viewer.cpp`.
- `run.py` identico a003, nessun manifest003 hardcoded o scelta automatica della revisione più recente; nuovi ID004 espliciti.
- Snapshot003 e 156 voci protette conservano hash e mode; DSO parziale e FIRSTFAIL003 preservati. Output004 assenti alla review.

Copertura: report128 righe, diff125, manifest29 e 184 righe Qt mirate, con confronto testuale integrale. Runner riusato dalla review003 con hash e binding ricontrollati. Il revisore non ha rieseguito AST/XML/bash, importato candidati, compilato, eseguito test, scritto file o usato rete/Git.

La nuova ammissione permette prima BUILD004 e, solo dopo successo completo, provenance conforme e nuovo preflight, ORACLE004 supervisionato con GT e gate invariati. Preview, GTK, interazioni, renderer effettivo e misure restano fuori da questa ammissione. Qualsiasi nuovo fallimento ferma il turno e viene conservato prima di un'ulteriore correzione.
