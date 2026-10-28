# M1.d: rettifica del prerequisito e unit lifecycle separata

25 ottobre 2026. Decisione tecnica del coordinatore dopo chiarimento dell'architetto indipendente `ses_e8c11b44bffe2NSoGlS6xqlh6a`, modello autodichiarato `openai/gpt-6-astra`. Preparazione di una nuova ricetta, nessun test eseguito o gate M1.d chiuso.

## Contratto originario e requisito aggiunto

`plans/M1-spikes.md:115-121` richiede hard cap 32 MiB del pool, saldo finale zero, RSS best effort, scratch fisicamente allocato entro 1 GiB e preflight dello spazio. Non richiede un teorema universale sul massimo metadata di ogni syscall prima dei test unitari.

Il modello Gamma v5 è una proposta locale più forte, non un nuovo requisito scientifico implicito. L'architetto ha esplicitamente rettificato la propria precedente interpretazione troppo rigida. Un controllo osservato resta diverso da una quota OS: un eventuale superamento del cap rimane violazione, mai PASS cancellato dallo STOP successivo. Nessun limite di pool, dati, casi o qualità viene allentato.

## Nuova ricetta primaria autorizzata alla sola preparazione

Separare le dodici regressioni lifecycle in `tmp/m1/d-unit/`, riusando le API reali e gli assert v5. Non eseguire il vecchio entrypoint disabilitando `UNPROVEN`, né sostituire il lifecycle con un simulatore.

Layout dedicato giustificato da inventario: dodici fixture da 33 byte, un ENOSPC da 4096, tre regioni fault con header, receipt, tracce complete, suite-result, diagnostica e staging dei percorsi falliti. Conteggi, schemi, campi e revisioni simultanee devono limitare realmente l'overhead; il numero dodici da solo non limita le diagnostiche. Nessun taglio dei log o numero di riserva scelto soltanto perché entra.

Prima di una futura esecuzione: preregistrazione, review mirata, turno esclusivo e baseline fisica aggiornata, riserva conservativa documentata con arrotondamenti/finalizzazione, controlli prima di ciascun caso e dopo le mutazioni, STOP al primo imprevisto preservando le prove. Non serve attestare un minimo di 170 record negli extent per verificare ordine e retention di un file da 33 byte. Non si promette una garanzia assoluta tra due osservazioni.

## Ciò che resta obbligatorio

La unit non prova lo spill fisico 256 MiB, i 21 bounded con 15 errori attesi, il pool sotto carico o i tre stream completi da 40 GiB. Tutti restano necessari, non sostituiti dai dodici test. Nessuna preallocazione o esecuzione è autorizzata da questo solo verbale.

Le prove full procedural senza spill sono separabili, ma l'harness corrente richiede il bounded completo: non aggirarlo. Anche un eventuale driver standalone full richiederebbe preregistrazione e prerequisiti pertinenti, senza qualificare l'intera M1.d.

Copertura del chiarimento: rilette cinquanta righe del protocollo, riusate evidence v5 e review dei sorgenti già esaminate. Nessuna misura di quota o verifica dinamica effettuata dall'architetto. Fonti ext4 del record `M1-d-ext4-sources.md` restano circoscritte e non certificano l'implementazione host.
