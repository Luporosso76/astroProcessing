# M1.f: review indipendente del freeze 003

Esito: quattro guardie precedenti chiuse staticamente, freeze non ancora approvato per l'esecuzione. Due correzioni residue richieste. Nessuna approval esterna creata.

Review di `ses_e8c4fd1faffex3f7sPIaEJ0Tce`, modello autodichiarato `openai/gpt-6-astra`. Verbale persistito dal coordinatore il 25 ottobre 2026; nessuna attestazione del provider, costi/token non esposti.

## Verifiche effettive

Esaminati tredici file congelati, 1.502 righe, più report 57, handoff 23, ricevuta statica 78, bootstrap 16 e manifest snapshot 28 righe. Delta letto dai byte ZIP senza estrazione. Verificati 13/13 hash sorgenti, 18/18 membri snapshot, report e protocollo; undici AST validi. Nessun import candidato, test, producer, worker, OpenSSL, dataset, scrittura o rete.

Freeze verificato: `0b03300a13f10a3d12cf8a156766743967dab6b395c022b91a4510e84843836b`. Il manifest non include sé stesso o l'approval; bootstrap e guard concordano. Nessuna autoapprovazione o circolarità rilevata. `review-approval-003.json` era assente.

## Chiusure statiche e rilievi

Cleanup dei figli e della sessione creata, traversal dirfd/NOFOLLOW e controlli di identità, pin prima delle scritture e validazione incrociata dei record risultano presenti. Non sono prove dinamiche né protezione universale contro un attore ostile con stesso UID.

1. **Medio:** la contabilità globale rifiuta anche symlink estranei senza seguirli. Deve contare i metadati del link, senza attraversarlo o modificarlo. Il rifiuto resta necessario per i percorsi effettivamente utilizzati dal candidato. Non è stata verificata la presenza di link bloccanti in altri namespace.
2. **Basso:** il test agreement colloca i record sotto un percorso non ammesso e ottiene `SystemExit` prima del controllo schema. Deve usare un nuovo percorso ammesso sotto `runs/`, mantenendo gli assert specifici e l'assenza di output.

L'autore produrrà un nuovo freeze, senza sovrascrivere lo storico. Solo dopo la review del delta il coordinatore potrà fornire un'approval limitata al turno concordato: prima regressioni, poi eventuali empty/abc. Nessun vettore 128 MiB o PASS M1.f è autorizzato da questo verbale.
