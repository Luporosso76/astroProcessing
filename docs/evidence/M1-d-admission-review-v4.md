# M1.d: review dell'ammissione v4

Esito: preparazione ancora `UNPROVEN`; correzioni necessarie prima di qualsiasi preallocazione o esecuzione. Nessun PASS empirico.

Review indipendente di `ses_e8c11b44bffe2NSoGlS6xqlh6a`, modello autodichiarato `openai/gpt-6-astra`. Il coordinatore registra il verbale il 25 ottobre 2026. Nessuna attestazione del provider; costi/token non esposti.

## Copertura

Letti integralmente nove file autorizzati, 2.945 righe. Ricevuta di preparazione dichiarata `bfdde5de9d413786a8f37495d7cff40023146feaa0259304891f8b6316e2cd60`. Gli hash sono stati letti dalla ricevuta, non ricalcolati dal revisore. Nessun import, test, comando, preallocazione o scrittura; nessun accesso ad altri namespace.

## Riscontri statici

Partizioni worker/aux/final/emergency separate, collegamento dei file reali ai charge e ammissione dei test senza skip chiudono strutturalmente buona parte dei finding precedenti. Non dimostrano la capacità fisica. I digest dei parziali registrati non sono automaticamente confronti con un oracle indipendente.

Restano quattro difetti concreti:

1. **Alto:** `mmap.close()` può fallire con una vista esportata, ma il consumer viene comunque rimosso dal registro. La registrazione deve durare fino alla chiusura riuscita; un errore deve impedire il rilascio.
2. **Medio:** uno slot con intent pendente può essere riutilizzato dopo scrittura/sync falliti. Occorre uno stato terminale non riusabile, conservando header e payload.
3. **Medio:** la suite unit continua dopo un fallimento inatteso. Serve failfast con conteggio dei casi realmente eseguiti e dei non eseguiti.
4. **Medio:** una short write nel caso ENOSPC può far scattare il limite di capacità prima dell'errore atteso. La richiesta successiva deve usare soltanto i byte residui alla soglia, senza aumentare lo slot.

## Budget e prossimo passo

Le capacità statiche 342.052.754 byte bounded, 21.854.124 full e 61.368.947 unit sono coerenti con il layout dichiarato, ma non sono ceiling fisici. Il profilo bounded non entra nell'occupazione comunicata, già prima degli overhead. Non si alza il cap e non si riducono casi o log per trasformare il risultato in PASS.

Prima correggere i quattro difetti mantenendo `UNPROVEN`. Poi identificare in sola lettura il filesystem e la configurazione realmente supportata. La prova preventiva deve coprire il massimo incremento della syscall in corso, non soltanto rilevare un superamento dopo la scrittura. Non serve una garanzia universale su tutti i filesystem; una singola osservazione non dimostra un massimo.

Qualsiasi futura fase osservativa avrà ammissione e receipt proprie. Il suo esito iniziale sarà `OBSERVED_UNREVIEWED`; non abilita automaticamente `run`. Riserve e prove fallite conservate restano occupazione da contabilizzare per i turni successivi.
