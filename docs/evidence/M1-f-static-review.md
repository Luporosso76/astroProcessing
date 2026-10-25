# M1.f: prima review statica dell'identità degli input

Data: 25 ottobre 2026. Revisore `security`, sessione `ses_e8c4fd1faffex3f7sPIaEJ0Tce`, modello autodichiarato `openai/gpt-6-astra`. Verbale persistito dal coordinatore.

Esito: quattro rilievi medi da correggere prima dell'ammissione pertinente. Nessun producer, worker, fixture, import candidato o benchmark eseguito.

## Copertura e identità

Letti integralmente sei moduli Python (724 righe), README (78), ambiente (7), report (61), handoff (33), manifest corrente (83), oltre a 46 righe di log. Verificati 8/8 hash sorgenti, i due manifest di preparazione e l'hash effettivo del protocollo. Le versioni dichiarate sono quelle di `M1-f-preparation.md`; disponibilità e sintassi non attestano runtime o provider crittografico.

Operazioni: lettura e verifica hash, senza comando di test. Input sotto `tmp/m1/f-hash/`; manifest corrente `preparation-manifest-002`, SHA-256 `88f3eebe229646aaa0d8e999bc65e1ed18f962d257edbcb3d1d16c7edb9ec033`. `datasets/` e `runs/` erano assenti alla review.

## Rilievi

1. `harness.py:68-82`: eccezioni dopo Popen possono lasciare worker e OpenSSL attivi. Occorrono cleanup garantito della propria sessione e raccolta del figlio.
2. `harness.py:42-58`, `prepare_inputs.py:23-28`, `fixture.py:27-28`, `worker.py:64-83`: directory preesistenti e symlink nei componenti possono deviare scritture. Servono traversal tramite fd fidati, controlli di identità e hardlink prima delle mutazioni.
3. `harness.py:49-58`, `protocol.py:33`, `prepare_inputs.py:14-28`: protocollo e sorgenti vengono registrati, non confrontati con il manifest revisionato prima dell'esecuzione.
4. `compare_results.py:18-35`: il confronto non rifiuta tutte le incoerenze di generation, schema, serializer, backend, coppia di engine e formato del digest. Validazione incrociata necessaria prima di `AGREEMENT_ONLY`.

## Riscontri e limiti

Corretti staticamente la cattura dei byte sintetici noti in memfd sigillato, la scelta primaria dell'engine senza retry, gli oracle letterali empty/abc e la distinzione fra agreement e indipendenza crittografica. Restano 38 ricette predisposte e 11 agreement, non eseguiti. Nessuna garanzia di snapshot temporale per input arbitrari, isolamento del sistema o limite RSS universale.

Il coordinatore ha assegnato la correzione in una nuova versione, conservando questa consegna. Misure, bootstrap e cache lookup non sono ancora implementati; nessun PASS M1.f.
