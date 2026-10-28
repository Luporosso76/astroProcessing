# Store sintetico: review statica iniziale

2026-10-25. Nessuna archive, verify, retire, materialize o release ammessa.

Revisore indipendente `ses_e8c4fd1faffex3f7sPIaEJ0Tce`, modello autodichiarato `openai/gpt-6-astra`; coordinatore persiste l'esito read-only. Letti integralmente report245, piano57, store626, test135, controllo statico105 e mapping83; JSON storage/manifest/indice parsati. Confermati13 hash documentati, inclusi8 pin operativi. Tre AST validi,11 metodi predisposti, zero eseguiti. Nessun payload fixture letto o rihashato, import o processo candidato. Costi/token non esposti.

Un MED prima delle operazioni reali: `store.py:410-568`, gli helper mutanti possono essere chiamati senza i gate CLI di main. Richiesto contesto operativo con fase/ammissione/lease attive e allowlist autorizzata, distinto dai primitivi minuti dei test; regressione del rifiuto senza contesto prima delle scritture. Non si pretende isolamento contro codice ostile dello stesso UID. Correzione assegnata all'autore, non ancora verificata in questo record.

Riscontri positivi:168 percorsi chiusi e SHA concordanti,847288776 byte logici; ZIP con limite effettivo e verifiche su member/header/extent, CRC più SHA e confronto completo con loose; gate CLI per retire/release, intent durevoli e identità rilette. Materialize usa prima archive, mai tentativo loose; chiusura effettiva input+GT su66 tuple, massimo21041152 byte. Reader lease cooperativa e registro esterno necessari, non scoperta universale dei reader.

Le11 regressioni minute risultano candidabili con harness e ammissione separati, ma si chiude prima il MED per qualificare la versione da usare. Nessun archivio reale creato o copia rimossa. Limiti di capacità/freschezza richiedono preflight reale: nessuna compressibilità o quota attuale certificata dalla review.
