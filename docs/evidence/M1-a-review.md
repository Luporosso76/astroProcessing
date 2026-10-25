# Review indipendente M1.a, serie 002

Unità: M1.a. Esito: **PARZIALE**. Le verifiche numeriche e statistiche sotto sono confermate; tre rilievi sui contratti d'errore impediscono di chiudere il gate. Questo record rettifica l'interpretazione dei contatori nel report originale, senza riscrivere la serie storica.

Revisore: agente `tests`, sessione `ses_e8c3e6e74ffeorhNv9IT7nj9CR`, GPT-6.1 Sol dichiarato dal runtime, non attestato. Data: 25 ottobre 2026. Il coordinatore persiste l'esito della review; non attribuisce al revisore nuovi benchmark.

## Verifiche effettive

Lettura dei sorgenti e dell'ambiente `tmp/m1/src/{core.cpp,core.rs,numpy_worker.py,run.py}`, `tmp/m1/env.sh`, dei quattro audit, dei report e degli artefatti della serie `tmp/m1/a/series-002`. Lettura mirata del protocollo, già letto integralmente nel precedente incarico. Analisi in sola lettura con libreria standard: hash, JSON e ricalcolo delle statistiche. Nessun worker importato o eseguito, nessuna nuova misura o mutation dinamica.

- Hash di protocollo, sorgenti/binari revisionati, due report e 168 fixture verificati.
- Griglia dei campioni confermata: 7560 warm, 2160 warm-up, 1080 startup, con identificativi univoci.
- Tutti i 360 riepiloghi temporali e tutti i 360 confronti bootstrap ricalcolati: nessuna discrepanza.
- Esempio appaiato C++/Rust, convoluzione dyadic float32: rapporto `0.8472581827335`, intervallo `[0.8184182628593312, 0.8692170846079225]`, coerente con i dati raw e il protocollo gerarchico.
- Nei record: 360 confronti numerici, 36 bound puntuali, 36 fault-stop, sei copie bitwise e 120 esiti sanitizer C++. La review controlla risultati, stderr e hash; non riesegue questi test.
- Formule dei bound, GT e stride coerenti nel dominio dichiarato. Nessun nuovo errore numerico rilevato sui casi finiti eseguiti.

## Rilievi accertati

| ID | Gravità | Evidenza | Correzione richiesta |
|---|---|---|---|
| A-ERR-01 | Media | I tre worker controllano l'input spaziale, ma non tutti i componenti dello spettro `inverse_source` | Verificare reali e immaginari prima del kernel; aggiungere NaN/Inf inverse in entrambe le precisioni, con stop non nullo e nessun output |
| A-ERR-02 | Media | Il ciclo di lettura C++ può terminare con exit zero su richiesta malformata o troncata | Distinguere EOF pulito da errore di parsing; diagnosi e stop non nullo per la richiesta invalida |
| A-ERR-03 | Media | `dispatch_counts` deriva dal candidato richiesto e `other_backend_calls` è costante | Non presentare questi valori come osservazioni. Strumentare i dispatch effettivi e verificare che un tentativo di dispatch alternativo faccia fallire l'audit |

La frase sui contatori osservati in `M1-a.md`, sezione fault-stop, non è quindi sostenuta dall'implementazione revisionata. I 36 processi con exit 2 e nessun output restano fatti verificati; non dimostrano da soli l'assenza di fallback. Non è stato trovato un percorso alternativo realmente eseguito nei sorgenti, ma una costante non ne misura l'assenza.

## Versioni e limiti

Protocollo revisionato: `plans/M1-spikes.md`, SHA-256 `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`. Manifest e comandi originali sono in `M1-a.md` e nella serie 002. I sorgenti e risultati storici restano invariati; le correzioni usano una nuova versione e un nuovo record, senza mescolare i tempi.

Restano distinti: ownership FFI, maschere, copie e allocazioni interne, qualificazione GPU e scelta dello stack. Il dtype esterno di NumPy non attesta la precisione interna della FFT. I due wrapper nativi condividono FFTW. Non è una validazione scientifica M3, una prova universale sulle librerie o una decisione di stack. Costi e token non disponibili.
