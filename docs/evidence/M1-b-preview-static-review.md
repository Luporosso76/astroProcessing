# M1.b: review statica della preview005 finita

2026-10-25. READY STATIC per BUILD005, non PASS GUI.

Revisore indipendente `ses_e8c3e6e74ffeorhNv9IT7nj9CR`, modello autodichiarato `openai/gpt-6.1-sol`; verbale persistito dal coordinatore. Nessun import, compilazione, GUI, oracle o test eseguiti nella review. Costi/token non esposti.

Copertura: report156, diff356, manifest29/19 pin, header111, host455, runner463; delta piano e I/O pertinenti. Diff ricostruito esattamente dallo snapshot004: cambiano solo piano, header/host Qt e runner. Tutti i19 pin005 corrispondono. Manifest005: `8a2b8b284c6083ef3f8dd1a8b95fe20f8439cce7a19199999f4c004ff3e958df`.

Nessun blocker nel delta: framePainted segue la chiusura del QPainter; callback queued controlla generazione/stato/raster/DPR. Export unico NOFOLLOW/esclusivo/fsync/NOREPLACE e verifica dei byte; exit0 soltanto dopo checkpoint. Distruzione normale cancella/notifica, join e bv_destroy precedono controller_released. Parser richiede checkpoint unici ordinati, nessuno STOP e hash/stato coerenti; diagnostica non JSON ferma conservando il raw. Nessun timer decide il successo. QML limitato e GTK sono rifiutati prima dell'avvio.

Verificati i riferimenti004, exit0 dell'oracle e DSO effettiva SHA `7f76d4d04ae5a0855dc38dd53d6a4349520fe0171eba2636391ca97c12e00f0a`. Prima della preview il core fresco deve coincidere, prima del parser ICC/GUI. Divergenza: STOP/review, non oracle automatico.

Prima ammissione: sola BUILD005. Preview successiva richiede nuova quota/ammissione con Widgets/xcb/DPR1/QtScale1. Scanout, A11y, FPS e antifallback interno restano non qualificati.
