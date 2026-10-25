# M1.c: review indipendente della serie temporale

Data: 25 ottobre 2026. Revisore `tests`, sessione `ses_e8c3e6e74ffeorhNv9IT7nj9CR`, modello autodichiarato `openai/gpt-6.1-sol`. Il coordinatore registra l'esito; nessuna attestazione del provider.

## Esito e materiali

PASS della verifica dei risultati della serie `m1c-v2-01`, non adozione di SQLite o JSON. La review non ha rieseguito candidati, riaperto database o ripetuto i 90 casi di validazione.

- Protocollo `plans/M1-spikes.md`: `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.
- Campioni `tmp/m1-persistence/paired-series-v2-01/measure/samples.json`: `6e98c1e85a11d4f5f864c53e520841cbca8ee37a7a7e7b363d45978fa1650c7a`.
- Sorgenti e ricetta sono quelli di `M1-c-measurement.md`; verificati quattro artefatti, cinque sorgenti e la ricetta. Nessuna sostituzione del backend.
- Ambiente e versioni della misura restano nei manifest della serie e in `M0-environment.md`; non sono una seconda piattaforma verificata.

Operazioni effettive: lettura dei sorgenti pertinenti, JSON e ricevute; verifica SHA-256; ricostruzione con libreria standard di conteggi, coppie e bootstrap. Nessun comando di benchmark eseguito durante la review. Lo script ad hoc della review non è un artefatto pubblicato: non si dichiara riproducibilità completa dal solo clone.

## Riscontri

Sei PID raccolti con exit 0 e stderr vuoto. Schedule di 54 esecuzioni: per candidato tre processi, due warm-up e sette campioni misurati per processo. Le 21 coppie hanno identità coerenti.

| Metrica | Rapporto appaiato SQLite/JSON | IC percentile 95% |
|---|---:|---|
| Totale | 1,090257073430803 | [1,0775372686010884; 1,1048159574823415] |
| Commit | 2,3409999772428116 | [2,232584639428884; 2,472712874854982] |
| Riapertura | 1,0573884217438099 | [1,0331670438649228; 1,0714124574946768] |

Ricalcolati esattamente tutti i rapporti, gli intervalli e le sei mediane: `exp(median(log(T_sqlite[p,r]/T_json[p,r])))`, bootstrap gerarchico appaiato con 2000 repliche, seed 4059 e percentili interpolati linearmente. Confermati ACK, oracle, collegamento alla validazione precedente, 54 blob e 27 manifest JSON. I file `cases.json` e `cases.jsonl` della fase measure sono vuoti per schema: i campioni sono in `samples.json`.

## Limiti

Il totale esclude inizializzazione delle fixture e startup. Il segmento commit inizia prima di SQL COMMIT o rename e include le operazioni successive fino all'ACK; non rappresenta due primitive identiche. La riapertura SQLite copia lo snapshot e usa `integrity_check`.

Tre cluster, cache calde e un solo filesystem non dimostrano superiorità generale, durabilità dopo blackout o comportamento di una release. Nessuna prova legale o scientifica del prodotto. M1.c fornisce evidenze alla decisione ADR; il gate complessivo M1 resta aperto.
