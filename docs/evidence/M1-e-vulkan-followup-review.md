# M1.e: review indipendente delle correzioni Vulkan

- Autore del verbale: coordinatore, GPT-6.1 Sol, 25 ottobre 2026.
- Revisore: agente `security`, sessione `ses_e8c4fd1faffex3f7sPIaEJ0Tce`, modello autodichiarato `openai/gpt-6-astra`; nessuna attestazione indipendente del provider. Costi e token non disponibili.
- Metodo: lettura statica e confronto dei byte/hash; nessuna esecuzione di candidato, test, GPU, rete o Git da parte del revisore.
- Esito: due rilievi MED e un LOW precedenti chiusi staticamente. Primo run delle regressioni supervisor ammesso; nessun PASS dinamico anticipato.

## Materiali verificati

Letti integralmente il diff di 1.065 righe, runner di 511 righe, test di 255 righe, report di 112 righe e ricevuta di 330 righe. Engine letto alle righe 190-566 e in tutti i delta; README, preparazione e piano verificati attraverso il diff.

Ricevuta `tmp/m1/e-vulkan/preparation-003.json`, SHA-256 `9b858ee2a5d31fe5b25d0450a1b5d5ed4b49ede613354f541fc77fad76ae34e5`: confermati otto sorgenti, piano, binario, indice e tre shader. Confermate dodici copie snapshot, quattro artefatti precedenti e ricevuta 002 invariati. Nessun nuovo pattern di segreti o percorsi home personali nei testi scansionati.

## Chiusure e rilievo residuo

1. Cleanup del costruttore incompleto e tentativi indipendenti di chiusura di tutti i propri worker: presenti; errori originali e di cleanup conservati.
2. Fase `validate` numerica/copy-control distinta dalle statistiche; `confirm` richiede ricevute validate/faults compatibili prima del lancio. Identità reale device/driver/coda confrontata con il binding congelato.
3. CPU time ricavato dai delta effettivi `ru_utime` e `ru_stime`; conversione microsecondi/ns dichiarata senza attribuire accuratezza nanosecondo.
4. Copy-control implementato con shader uint32, upload di 200 byte e download di 64 byte, copie e submit/fence osservati al confine applicativo. Non prova gli interni del driver; controllo bitwise non estende il contratto aritmetico ai subnormali.
5. Nuovo LOW, `test_followup.py:187-192`: il test mismatch omette `cpu_reference_lock_sha256`, quindi termina su `CPU freeze/recipe binding required` prima del ramo dichiarato. Correzione richiesta: binding completo, sola ricetta/GT alterata; test separato per il binding incompleto. Cambiare soltanto la regex non chiude il rilievo.

## Prossimi gate

Correzione e intera suite supervisor, poi verifica degli esiti. Seguono ammissioni distinte per pilot CPU con lock, GPU `validate`, `faults` e infine confronto confermativo. Nessuna di queste prove risulta eseguita in questa review. Nessun cambio di backend o precisione è autorizzato dopo un errore; nessuno speedup, rilascio o adozione di stack è attestato.
