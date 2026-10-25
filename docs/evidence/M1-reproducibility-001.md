# M1: archivio delle fonti e dei risultati storici

## Operazione eseguita

Il coordinatore ha creato `artifacts/m1-a-c-sources-and-results-001.zip` con 30 file selezionati e un `MANIFEST.json` interno. Sono inclusi sorgenti e verificatori dello spike numerico storico, protocollo congelato, risultati numerici della serie002, sorgenti della persistenza e risultati delle sue misure.

- Dimensione dei 30 file: 5.486.014 byte.
- Dimensione ZIP: 607.557 byte.
- SHA-256 ZIP: `6c954eb95bd7633a41192c5a9e5dc82718f6c0d82dbdb3874a1a26f9e239ce3c`.
- Ogni membro è stato riletto dallo ZIP e confrontato con il proprio SHA-256. Anche gli originali sono stati ricontrollati dopo la creazione: nessuna modifica.
- Lo staging esclusivo è avvenuto in `tmp/m1-evidence-archive-001/`, dopo preflight del budget. File sincronizzato, pubblicazione senza sovrascrittura e sincronizzazione delle directory.

È un archivio di evidenze, non codice integrato nel prodotto. Non contiene binari, librerie, toolchain, cataloghi o fotografie dell'utente. Non assegna una licenza al prodotto.

## Limiti di riproducibilità

Le grandi fixture non sono incluse. Occorre rigenerarle e verificarne gli hash prima di ripetere gli esperimenti. Due script accessori, `gpu_preflight.py` e `final_checks.py`, sono esclusi per minimizzare metadati personali. L'archivio non costituisce pertanto una riproduzione completa eseguibile direttamente da un clone.

I sorgenti numerici sono quelli delle misure storiche serie002: conservano i difetti del contratto d'errore successivamente corretti nella versione v3. Sono inclusi per verificare l'origine delle misure, non come implementazione corrente raccomandata. Le correzioni v3 e i relativi test sono descritti nei record dedicati; non ereditano i tempi della serie002.

I dati di misura conservano significato e limiti dei rispettivi verbali. L'archiviazione non ripete i benchmark e non chiude M1.

## Verifica di pubblicazione

La scansione preliminare dei 30 file non ha rilevato pattern di chiavi private, token, credenziali URL o percorsi home personali. La review indipendente dell'archivio deve precederne la pubblicazione Git. Gli archivi storici dell'utente in `doc_old/` restano esclusi da questa selezione.
