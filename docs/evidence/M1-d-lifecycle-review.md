# M1.d: review statica del lifecycle e dell'ammissione disco

Data: 25 ottobre 2026. Revisore indipendente `architect`, sessione `ses_e8c11b44bffe2NSoGlS6xqlh6a`, modello autodichiarato `openai/gpt-6-astra`. Verbale registrato dal coordinatore.

Esito: ammissione runtime BLOCCATA. Letti integralmente sette file autorizzati, 2241 righe. Nessun import, test, worker, benchmark o comando candidato eseguito. Ricetta riportata nella receipt di preparazione: `434be773d877167810143b17320c83ede9b46dba302c485e2f49d3ab3a0bb229`; non è una ricetta runtime verificata. Sorgenti, ambiente e hash della consegna sono in `M1-d-lifecycle-preregistration.md`.

## Rilievi da chiudere

1. Alto: scansione prima della scrittura non equivale a prenotazione. Worker e supervisore possono consumare lo stesso residuo. Occorrono partizioni preventive per worker, ausiliari e finalizzazione.
2. Alto: l'errore può esaurire il ledger necessario a salvare samples e report. Riserva diagnostica e staging devono esistere prima dell'avvio, senza tagliare i log necessari.
3. Medio: i sette test lifecycle accettano una stringa senza verificare quota, bound e lock; senza ID risultano saltati. Richiedono ammissione propria, non un PASS con test saltati.
4. Medio: quota persistente e `spill-cancel` non collegano ancora indipendentemente file reale, blocchi e charge. Aggiungere verifica del parziale dopo fsync/chiusura e regressione con piccolo file reale.
5. Medio: diminuzione dei blocchi raggiungibili dopo unlink non esclude fd o mapping ancora aperti. Il contratto deve escludere lettori esterni e chiudere tutti quelli controllati prima di attribuire un rilascio fisico.

## Bound e riscontri

Un solo spill completo da 256 MiB, nessuna seconda copia al rename. I tre percorsi full da 40 GiB non aggiungono spill. I residui attesi sommano 69662 byte logici; la preregistrazione sovrastima di un byte, senza effetto sulla sicurezza. Un errore inatteso può conservare l'intero spill: la serie deve fermarsi prima di ammettere altro.

I 7 MiB logici validate e 5 MiB measure conteggiati per JSON non dimostrano un cap fisico. Restano da includere eventi, duplicazioni, stderr fino a 131072 byte, staging falliti e finalizzazione. Non è dimostrato un deficit inevitabile: è incompleta la prova del massimo simultaneo sul filesystem supportato.

I sette test sono piccoli test d'ordine con due mutation, non sette spill completi. Lo spy osserva syscall reali e rileva unlink prematuro; non dimostra l'autoprotezione di codice mutato senza spy. La regressione weakref del worker osserva bytearray reali, ma non è stata eseguita.

La receipt durevole precede l'unlink nel percorso esaminato. Un errore successivo all'unlink non ricrea il file: conservare tale limite, senza promettere che ogni errore preservi sempre il completo.

## Azione

Preparare una nuova ricetta con bound fisico, partizione delle risorse, ammissione dei piccoli test e verifica dei parziali. Conservare 21 bounded, 15 errori attesi, tre full da 40 GiB e spill fisico da 256 MiB. Il blocco `UNPROVEN` resta attivo finché la prova non è completa; nessuna modifica del limite per far passare il test.
