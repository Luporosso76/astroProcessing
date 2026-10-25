# M1.e: prima VALIDATE GPU confermata indipendentemente

Esito: VALIDATE PRIMARY confermata, READY soltanto per FAULTS sulla stessa ricetta congelata, previa ammissione. Nessun blocker. Non autorizza automaticamente esecuzioni, CONFIRM o altre tuple.

Review di `ses_e8c3e6e74ffeorhNv9IT7nj9CR`, OpenAI GPT-6.1 Sol autodichiarato dal runtime. Verbale registrato dal coordinatore il 25 ottobre 2026; costi/token non esposti. Nessuna esecuzione, import candidato o scrittura da parte del revisore.

## Integrità e identità

Ricevuta `tmp/m1/e-vulkan/validate-affine-004/validate-receipt.json`, SHA-256 ricalcolato `66dce9d16e844ae8d900e77bd626f041a6ef96c4a9812775afdffdff35d542e1`. Verificati sette artefatti della ricevuta, undici hash della verifica, otto fonti correnti, PREP004, engine, shader, indice, descriptor, input/GT, protocolli e lock CPU. Nessuna incompatibilità.

Ready e due contesti GPU concordano sull'identità effettivamente osservata: AMD 1002:1681 integrata, RADV REMBRANDT, UUID `00000000-e500-0000-0000-000000000000`, driver ID3/radv/Mesa26.0.8. Queue family0, flags15, timestamp validBits64 e periodo10ns. LlVMPipe enumerato, non selezionato; non si deduce assenza dall'host.

## Percorso verificato

Affine CPU/GPU su 323 pixel con confronto in-process contro GT indipendente, tolleranza zero. Tracce e contatori concordano: affine un dispatch con due workgroup, upload/download1292/1292 byte; copy-control un dispatch, upload/download200/64. Entrambi una submit accettata, una fence completata e zero chiamate kernel CPU nei percorsi GPU. I contatori seguono chiamate effettive, non costanti. Le barrier sono nel codice hashato ma non hanno eventi runtime individuali.

Ricalcolati dai timestamp gli intervalli GPU upload/kernel/download: affine3440/8400/2080ns, copy2360/6520/2080ns. Gli zeri CPU delle query sono non applicabili. CPU user/system ha risoluzione microsecondi convertita in ns. Nessun rapporto prestazionale, speedup o CI deriva da questi valori diagnostici.

## Copy-control, processi e limiti

Verificati integralmente input, GT, hash e layout: sedici word attive 8x2, stride25, trentaquattro word padding NaN. Shader uint senza aritmetica floating-point. I pattern attivi sono tutti finiti: coperti ±0 e subnormali, non NaN attivi. Il padding NaN non viene letto logicamente.

Gli output prodotti non sono persistiti: la review conferma fixture, tracce e confronto in-process, non ricalcola indipendentemente i buffer GPU. Nessuna prova di aritmetica FTZ, f64 o FloatControls attivati.

Tre worker e driver raccolti, exit0 e cleanup vuoto. Associazione ai PID tramite argv univoci dell'osservatore, non receipt native con PID o audit waitpid completo. Nessun nuovo errore/retry documentato; binding contrattuali invariati, indici mutabili esclusi. La mappa protetta di 145 file non è stata interamente rihashata dal revisore.

Letti report completo, ricevuta, ready/risultati, tre tracce, copy-control, observer, verification e metadata; analizzati percorso GPU e shader pertinenti. Nessun riuso confermativo del pilot o PASS complessivo M1. La prossima fase attende il turno: le regressioni hash occupano attualmente l'unica ammissione.
