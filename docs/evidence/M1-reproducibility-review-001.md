# Review indipendente dell'archivio M1 a/c

Esito: PASS di pubblicazione limitato a `artifacts/m1-a-c-sources-and-results-001.zip` e `M1-reproducibility-001.md`.

Revisore: agente `security`, sessione `ses_e8c4fd1faffex3f7sPIaEJ0Tce`, modello `openai/gpt-6-astra` autodichiarato. Il coordinatore registra il verbale; non attribuisce al revisore scritture o prove ulteriori.

## Verifiche

- ZIP di 607557 byte, SHA-256 `6c954eb95bd7633a41192c5a9e5dc82718f6c0d82dbdb3874a1a26f9e239ce3c`.
- Trentuno membri: trenta file dichiarati e `MANIFEST.json`, senza duplicati o membri aggiuntivi.
- Dimensioni e digest conformi per 30/30 file; payload totale 5486014 byte.
- Percorsi relativi senza traversal, symlink, cifratura o file speciali.
- Tutti i membri testuali UTF-8; JSON e JSONL leggibili. Nessun binario, header o libreria incorporati.
- Scansione completa senza chiavi private, token, credenziali URL, email o home personali espansi rilevati.
- Sorgenti numerici coerenti con il manifest storico; difetti precedenti, esclusione della v3 e non trasferibilità dei tempi dichiarati.

## Copertura e limiti

Il revisore ha letto e analizzato in memoria tutti i membri, 25756 righe, e le 25 righe del record di preparazione. Nessuna estrazione, importazione, esecuzione o rete.

Gli originali in `tmp/` non sono stati riletti dal revisore. La verifica di immutabilità prima/dopo, lo staging e gli fsync sono operazioni del coordinatore. Fixture e script accessori esclusi impediscono una riproduzione completa direttamente dal clone. Nessuna licenza di prodotto assegnata; questa review non certifica universalmente la titolarità dei sorgenti.

Nessun finding bloccante; nessuna correzione richiesta. Il PASS non chiude M1 e non si estende ad altri file o a future versioni dell'archivio.
