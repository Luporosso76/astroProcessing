# M1.e: review indipendente del confronto tiny004

25 ottobre 2026. Verbale del coordinatore sulla review `tests`, sessione `ses_e8c3e6e74ffeorhNv9IT7nj9CR`, modello autodichiarato `openai/gpt-6.1-sol`. Costi/token non esposti.

## Risultato

Confermati esattamente dai raw mediane, MAD, estremi e bootstrap della serie `M1-e-confirm-affine.md`: CPU mediana 39523 ns, GPU 408811 ns; statistica appaiata CPU/GPU `0.08539116741822347`, IC95 percentile lineare `[0.040999118692929416, 0.11143010199039907]`. Calcolatore indipendente stdlib, tre cluster, sette coppie per cluster, 2000 ricampionamenti, seed 4059.

Rilievo basso documentale chiuso dal coordinatore dopo lettura `engine.cpp:518-532`: `total_ns` esclude il confronto con GT, che avviene dopo `run()` in `verify_ns`. Include le scansioni contrattuali/finitezza, tracing, kernel e trasferimenti pertinenti. Corretto il testo primario; nessuna modifica a raw, numeri, intervallo o codice.

## Integrità e osservazioni

- Report SHA-256 `b563b4e7a9d9949a506ecb3a9ea5f82658050c340efa93d606954e65bd55b25f`; execution `5e5496ed21af18b3101dbeba9ab5ddbea4108e5c98d08706bcdf46e1b15e1625` verificati.
- Pin delle otto fonti, PREP004, engine, shader, protocolli, indice, descriptor, input/GT, lock e ricevute VALIDATE/FAULTS verificati.
- Ventuno coppie univoche p/r, 42 misure conservate, 54 richieste incluse le warm-up. Sei trace: nove righe CPU e 88 GPU per cluster. Ricalcolati i sei digest; manca un manifest dedicato con digest attesi delle trace.
- Tutti i 42 verdetti di confronto esatto sono riusciti; contatori di copie, submit, fence, timestamp e identità device/coda/driver coerenti con VALIDATE.
- Sei ricevute worker riportano raccolta, exit 0 e cleanup vuoto. Non contengono PID e non sono un nuovo audit OS indipendente.

## Copertura e limiti

Letti 46 righe del record primario precedente alla precisazione, raw report 2279, execution 82 e sei trace per 291 righe; esaminati il ramo confirm e il confronto GT nell'engine. Review precedenti riusate verificando gli hash.

Non rieseguiti candidato, test o compute. Output numerici non persistiti: verifica GT attestata in-process, non ricalcolata sui buffer prodotti. Warm-up dimostrate da fonte/trace, metriche non conservate; sette campioni pilot non usati nel bootstrap.

Il risultato resta condizionato a questo microcaso strumentato e riferimento C++ nativo, non al migliore di tutte le CPU. Nessuna conclusione sulle immagini grandi, scelta stack, altri operatori o completamento M1. Gli interni opachi del driver e una garanzia universale di cleanup non sono provati.
