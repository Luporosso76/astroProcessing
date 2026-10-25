# M1: review indipendente del protocollo numerico

## Esito e copertura

Review dello specialista `tests`, sessione `ses_e8c3e6e74ffeorhNv9IT7nj9CR`, modello GPT-6.1 Sol dichiarato dal runtime. Letti integralmente il protocollo di 169 righe e la policy M0 di 26 righe; AGENTS disponibile nel contesto. Nessuna lettura degli spike in scrittura, esecuzione, misura o modifica da parte del revisore.

Il coordinatore ha recepito due rilievi prima delle serie definitive:

1. Gli oracle FFT reali non rilevavano coniugazione dello spettro o perdita della parte immaginaria. Aggiunti modo seno sui due assi, convenzioni di segno esplicite e verifiche forward/inverse indipendenti. Tolleranza zero conservata.
2. Le coppie del bootstrap e i processi warm non erano definiti. Fissati tre cluster di processo, due warm-up e sette round per cluster, statistica dei log-rapporti e ricampionamento appaiato gerarchico. Restano 2000 ricampionamenti, seed 4059 e intervallo percentile 95%; la CPU di riferimento va congelata usando una serie esplorativa distinta.

Il revisore ha verificato matematicamente rappresentabilità dyadica di affine, convoluzione e warp, e coerenza del bound non dyadico sotto le assunzioni dichiarate. Questo non prova che i candidati rispettino tali assunzioni o producano i risultati corretti.

## Versionamento delle serie

La preregistrazione precedente aveva SHA-256 `93468f1ceb4afcd735a822a3b87589e963917260ce6d87cfdf0269c6721cc9ff`. La modifica apre una nuova versione: ogni esecuzione registra l'hash effettivo del protocollo. Eventuali serie precedenti restano esplorative con il proprio hash; non si rietichettano come confermative e non si combinano con la nuova serie.

Stato: rettifiche del protocollo applicate; verifica degli harness e risultati ancora aperta. Nessuna validazione empirica M1 o scientifica M3 deriva da questa review. Costi e token non esposti; nessuna attestazione indipendente del provider.
