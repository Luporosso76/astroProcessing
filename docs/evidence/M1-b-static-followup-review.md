# M1.b: review indipendente del candidato viewer 003

Data: 25 ottobre 2026. Record del coordinatore della review del coder indipendente, sessione `ses_e8c14fabaffeCEsreEF1hHtERa`. Non si attribuisce un'identità di modello non esposta nell'esito; costi e token non disponibili.

## Esito

READY statico per prima build Qt ABI 2 e successivo oracle supervisionato, solo se build, integrità e preflight riescono. Nessuna preview GUI, GTK o misura comparativa ammessa da questa review.

Copertura dichiarata: quindici sorgenti nuovi o modificati, 2721 righe; diff di 1738 righe; follow-up di 190 righe; piano di 167 righe; manifest di 28 righe. Diciannove hash verificati e otto AST analizzati. Nessun import dei candidati, build o test eseguito dal revisore.

## Rilievi statici chiusi

1. Oracle caricato soltanto in un figlio supervisionato: letture non bloccanti, limiti, deadline, cleanup e ricevuta separata.
2. Libreria verificata caricata attraverso lo stesso oggetto verificato e sigillato; il caricamento non riapre soltanto un pathname controllato in precedenza.
3. ICC ed export usano capability di directory, apertura relativa con controlli sui link, pubblicazione senza sovrascrittura e sincronizzazione.
4. Rimossi i contatori costanti presentati come osservazioni di fallback. Gli interni non strumentati restano esplicitamente sconosciuti; il relativo gate non è dichiarato chiuso.
5. Cleanup GTK con cancellazione e raccolta del worker anche sul percorso d'errore. GTK resta non ammesso per assenza della qualificazione runtime richiesta.

ABI C/Python, generatore, quantizzazione e cache sono coerenti staticamente. L'oracle predisposto amplia scale, DPR, frame, eviction e cancellazione senza cambiare GT o tolleranze. Il manifest consegnato `tmp/m1/b-viewer/source-hashes-003.json` ha SHA-256 `47c2bade1f71369d582891a058fa344b6eb992962e9fa7742763bb833ecca041`.

## Limiti e prossima prova

Queste conclusioni non dimostrano ABI compilata, output numerico effettivo, rendering selezionato, ICC, interazione, accessibilità o prestazioni. La prima build e l'oracle hanno un turno esclusivo distinto. Preview, 100 pan/100 zoom e confronto statistico richiedono le rispettive ammissioni; bootstrap comparativo ancora non implementato.
