# M1.b: review indipendente della preparazione

Esito: primo build Qt ammissibile dopo le correzioni indicate. Oracle, avvio GUI e misure non sono stati eseguiti. Il gate M1.b resta aperto.

Review del coder indipendente `ses_e8c14fabaffeCEsreEF1hHtERa`; verbale persistito dal coordinatore il 25 ottobre 2026. La review riguarda i file effettivi, non il riassunto dell'autore. Modello/provider e costi non disponibili nel messaggio di consegna: nessuna attestazione o stima aggiunta.

## Copertura effettiva

Letti i quattordici sorgenti, 1.951 righe, insieme ad AGENTS, preregistrazione, report e manifest: 2.321 righe complessive in diciotto file. Verificati 16/16 hash di sorgenti e piani rispetto al manifest `0deee370bd8871323a8ffed27fb46de51f2989a158f1c7b39e805037548acdd4`. Nessun import candidato, build, GUI, test o benchmark; nessuna scrittura da parte del revisore.

## Rilievi prima dell'esecuzione

1. Oracle CDLL diretto senza supervisione di deadline/RSS, isolamento dell'ambiente e raccolta garantita del figlio.
2. Verifica del file della libreria seguita da riapertura per pathname: finestra TOCTOU. Il caricamento deve usare l'oggetto verificato mantenuto aperto.
3. Aperture ICC/export successive alla verifica dei parent per pathname: servono capability directory, operazioni relative al descriptor e controlli sul file effettivamente aperto.
4. `fallback_calls=0` costante non costituisce osservazione. Servono eventi effettivi, oppure il limite di osservabilità deve restare esplicito e il gate aperto.
5. Eccezione nell'avvio/run GTK può lasciare il worker non-daemon senza cancellazione, join e rilascio.

Le correzioni sono assegnate all'autore, in un nuovo snapshot precedente ai dati. Nessun caso viene eliminato. L'oracle deve coprire le scale dichiarate, eviction oltre 146 tile, cancellazione e pubblicazione delle generazioni, non soltanto tre immagini alla stessa scala.

## Riscontri e limiti

Il codice presenta layout ABI coerenti staticamente, generatori e aritmetica compatibili con l'oracle, cache dichiarata di 124.387.328 byte e pubblicazione Qt protetta dalla generazione. Non equivale a verifica dinamica del layout, della memoria, del renderer, di ICC o della presentazione nativa.

GTK resta non ammesso senza prova del rendering primario. Bindings Python non sono una misura di un frontend Rust nativo. Le serie comparative con tre processi, warmup, coppie e bootstrap non sono ancora implementate. Nessun FPS, vincitore GUI o PASS M1 deriva da questa review.
