# Review statica del profilo Qt006

**READY prima BUILD006**, non PASS GUI o ammissione alla preview. Review ordinaria indipendente `reviewer`, sessione `ses_e88c44174ffezPbywRfQt8wHgI`, senza override del modello. Modello autodichiarato `openai/gpt-6-astra`, non attestato; costi non esposti.

Il delta aggiunge cinque righe al launcher: STOP se XAUTHORITY manca o è vuota; inoltro opaco al solo figlio GUI Qt+xcb, dopo il parser ICC. Nessuna nuova lettura del file auth, stampa del valore, selezione alternativa o modifica alle autorizzazioni. La variabile era presente ma omessa nel profilo005: causa plausibile del guasto, non prova che006 riuscirà.

Copertura del revisore: report120 righe, patch67, manifest005/006 da29 righe ciascuno, checks191 e launcher312-468, totale693 righe. Confrontati i19 pin dichiarati,17 invariati. Nessun ricalcolo SHA indipendente, import, esecuzione, socket, lettura di segreti, Git, rete o scrittura. I controlli dei casi assente/vuota e di esclusione dagli altri figli non sono ancora eseguiti.

## Verifica separata del coordinatore

Comando di controllo: `/usr/bin/python3 -I -B -S` con soli hashlib, JSON, os, stat e pathlib. Letture FD NOFOLLOW, file regolari nlink1 e identità stabile: **22 file verificati,19 pin correnti conformi**, manifest005 e006 autenticati,17 pin identici al genitore. Cambiano solo `run.py` e `plans/M1-b-viewer.md`; protocollo M1 ancora conforme.

Manifest006: `ab9994ebc5c73325bac8ba0331d2a8d26fc870fa3f0d98179ff7127ceabbdb7d`. Nessun candidato importato, build o collegamento GUI eseguito da questo controllo. Snapshot32 membri,509 voci protette e vecchi binari non sono stati nuovamente verificati dal coordinatore in questo passaggio; conservazione attestata dal record dell'autore.

## Gate successivi

Distinta ammissione per BUILD006, dopo chiusura del turno candidato attivo e nuovo preflight. Preview006 soltanto dopo build riuscita, provenance conforme e DSO bit-identica004 per il riferimento esplicito oracle004. Nessun oracle006 dedotto, uso automatico di build005 o cambio di piattaforma. FIRSTFAIL005 resta un fallimento conservato. Questa review non legge né copia cookie e non autorizza aggiramenti dell'autenticazione.
