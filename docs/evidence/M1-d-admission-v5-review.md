# M1.d: review statica v5 e limiti del modello ext4

25 ottobre 2026. Verbale del coordinatore sulla review dell'architetto indipendente `ses_e8c11b44bffe2NSoGlS6xqlh6a`, modello autodichiarato `openai/gpt-6-astra`. Nessuna attestazione del provider o costo inventato.

## Esito

Chiusi staticamente i quattro rilievi v4 sul codice v5: mapping ancora registrato dopo `BufferError`, slot terminali non riutilizzabili dopo I/O fallito, suite failfast con casi non avviati `NOT_RUN`, short-write 2048+2048 entro lo slot ENOSPC da 4096 byte.

Dodici regressioni predisposte, sette originali conservate; nessuna eseguita. Invariati 21 bounded, 15 errori attesi, tre full da 40 GiB, pool 32 MiB e spill fisico 256 MiB. Le fixture piccole di lifecycle non sostituiscono il test di spill.

## Blocchi ancora aperti

1. Ext4 con blocco 4096 non prova cluster, feature e occupazione minima di 170 record ipotizzata per i nodi. Le formule di `fs_bounds.py` non sono massimi approvati dell'implementazione host.
2. Il modello proposto per fsync su 256 MiB pendenti produce `((65536 + 2) * 13 + 2) * 4096 = 3489775616` byte. Provare il solo fallocate stretto non rende ammissibile la sequenza sotto il cap 1 GiB. Un diverso ordine di sincronizzazione richiederebbe una ricetta preventiva distinta, non una modifica trasparente dopo un guasto.
3. Prenotazioni preventive ancora da collegare a bootstrap e lock, create/mkdir/fallocate/pwrite, rename/unlink e fsync, con concessioni disgiunte e riserva di finalizzazione. Allocazioni differite non si dichiarano rilasciate al semplice ritorno di pwrite.

Il runtime resta `UNPROVEN`; nessun admit, preallocazione, unit o run autorizzato da questo verbale. Un eventuale bootstrap osservato non proverebbe il massimo filesystem.

## Prossimo passo e copertura

Ricerca circoscritta delle fonti pubbliche ext4 sulle ipotesi mancanti, prima di altri cambi ai writer. Non serve riscrivere i quattro lifecycle già corretti. Gli altri rami proseguono.

Letti evidence 273 righe, diff 1107, receipt 206 ed estratti dei sette Python, 2460 righe complessive. Nessun import, esecuzione, scrittura o altro namespace. Hash consultati nella receipt, non ricalcolati dalla review; capacità corrente di tmp non misurata.
