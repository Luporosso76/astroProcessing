# M1.f: review indipendente delle otto regressioni reali

25 ottobre 2026. Verbale del coordinatore sulla review `tests`, sessione `ses_e8c3e6e74ffeorhNv9IT7nj9CR`, modello autodichiarato `openai/gpt-6.1-sol`. Costi e token non esposti.

## Esito

Confermati otto metodi effettivi, bootstrap e osservatore exit 0, una invocazione senza retry, zero failure/error/skip. Nessuna diagnostica unraisable osservata; non esiste un suo contatore dedicato. Summary e marker corrispondono ai byte registrati.

Ricalcolati SHA e metadati dei 104 artefatti, 13 sorgenti e 19 file pre/postflight. Freeze004, approval limitata a `future_regressions`, guard, protocollo, comando catturato e literal bootstrap corrispondono ai pin dichiarati. FIRSTFAIL precedente conservato: zero test e zero bootstrap, fermato sui permessi. Verificato lo stato consegnato dopo l'hardening, senza retroattestare il chmod originario.

## Rilievo di copertura

Basso, `future_regressions.py:281-285`: i tre probe con pin errato usano `prepare_inputs`, entry già esclusa dall'approval004. Il solo exit nonzero non discrimina il rifiuto del pin da quello dell'entry. Nessun bypass trovato nell'esame del guard hashato. Il limite non invalida l'8/8 osservato; per la successiva versione predisporre probe con entry ammessa e diagnosi specifica conservata, senza riscrivere questi risultati.

## Cleanup e limiti

Cinque rami cleanup verificano eccezione originale, returncode e successivo `ChildProcessError` su waitpid. Dieci log figli vuoti e due target sintetici intatti. Nessun adottato o errore di raccolta registrato; il campionamento di quattro PID non censisce tutti i figli brevi.

Non prodotti dataset, non eseguiti engine di contenuto e non pubblicati agreement. I record empty sintetici non dimostrano hashing di empty/abc. Le 38 ricette, 11 agreement, misure e cache lookup non sono qualificati da questa review. READY soltanto per una futura ammissione tiny dopo nuova approval entry-specifica e versione congelata; non autorizza l'esecuzione ora.

## Copertura

Letti report 93 righe, receipt 113, test 299, guard 167, supervisore 301, harness 114, fixture 112, schema 103 ed estratti SecureFS; verificati log, ricevute, hardening e FIRSTFAIL. Review in sola lettura, senza import o riesecuzione del candidato. Un confronto ausiliario metadata della review correggeva `size` contro `bytes`, non un test del candidato. Nessuna chiusura complessiva M1.f.
