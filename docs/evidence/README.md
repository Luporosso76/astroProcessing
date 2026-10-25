# Evidenze delle prove

Qui si registrano solo prove realmente eseguite. Un test pianificato, una suite definita o un risultato atteso non sono evidenza. Se una prova non è stata eseguita, si scrive che non lo è stata.

## Formato del record

Un file per esecuzione, con nome stabile e rintracciabile: `M0-environment.md` per l'audit di ambiente, `M0-catalogs-and-science.md` per la ricerca sui cataloghi, oppure `AAAAMMGG-<unità>-<breve>.md` per gli altri (esempio `20261025-M1a-kernel.md`). Il contenuto segue sempre questi campi:

```text
Unità:            M0.2 / M1.a / ...
Scopo:            cosa si voleva stabilire, in una frase
Comando:          riga esatta, copiabile
Input:            percorsi usati e sha256 di ciascuno
Versioni:         tool, compilatore, runtime, librerie, revisione del piano
Ambiente:         OS, kernel, CPU, RAM, GPU/driver, filesystem
Risultato:        codice di uscita, esito sintetico
Output:           percorso di stdout/stderr o artefatto prodotto
Ambito:           cosa copre la prova e cosa lascia fuori
Esito:            PASS / FAIL / PARZIALE
Limiti:           condizioni, anomalie, misure mancanti, dubbi
Autore e data:    agente, modello dichiarato dal runtime, data
```

Tutti i campi sono obbligatori. Se un dato non è disponibile si scrive `non disponibile`, non si stima.

## Regole

- **Solo eseguito.** Nessun PASS o FAIL anticipato. Una prova fallita resta nel registro: si aggiunge un nuovo record per il tentativo successivo, non si riscrive il vecchio.
- **Riproducibile.** Comando esatto, percorsi e versioni devono permettere a un'altra sessione di ripetere la prova. Gli hash si calcolano, non si copiano da un preventivo.
- **Input immutabili.** Gli originali si leggono e si hashan, non si modificano. Se la prova scrive, l'output va dentro il progetto.
- **Nessun segreto.** Niente credenziali, token, chiavi o dati personali. Niente letture di sistema non necessarie.
- **Costi solo se esposti.** Token e denaro si riportano solo quando il runtime li mostra. Nessuna stima inventata.
- **Di chi è la prova.** Modello e provider sono quelli dichiarati dal runtime, con percorsi e conteggi di righe letti. Non si parla di attestazione.
- **Ambito esplicito.** Una prova su un caso non vale per tutti. La sezione `Ambito` dice cosa non è stato coperto.

## Cosa non è evidenza

Un documento che descrive un test, un benchmark progettato ma non lanciato, un risultato dedotto da una formula senza esecuzione, un'osservazione riportata da terzi senza comando. In questi casi il record non si crea: si annota il limite nell'ExecPlan o nello stato.

## Indice

- `M0-environment.md`: audit di ambiente, disponibilità e inizializzazione del repository. L'errata sulla sintassi shell è in `M0-closure.md`.
- `M0-catalogs-and-science.md`: cataloghi e prerequisiti scientifici, candidati non approvati, nessun pack acquisito.
- `M0-plan-review.md`: PASS indipendente sul delta 2.2.1, con copertura e riserve dichiarate.
- `M0-closure.md`: controlli documentali eseguiti, archivio verificato, minimi M0 e limiti delle review. Non contiene risultati degli spike M1.
- `M0-ops-review.md`: PASS indipendente dei minimi operativi per gli spike locali; copertura sicurezza/privacy limitata ai file verificati.
- `M0-publication.md`: allowlist del commit documentale iniziale, controllo privacy e verifica del push sul repository ufficiale.
- `M1-protocol-review.md`: review indipendente effettivamente svolta e rettifiche preregistrate; non contiene risultati di kernel o benchmark.
- `M1-e.md`: probe GPU effettivi, hardware distinto dal renderer software; compute e speedup non misurati.
- `M1-a.md`: validazioni e misure della serie corrente, serie esplorativa ed errori conservati; gate complessivo parziale.
- `M1-a-review.md`: ricalcolo indipendente dei risultati e tre rilievi sui contratti d'errore; rettifica dei contatori anti-fallback, non nuovi benchmark.
- `M1-e-tools.md`: compilatori shader acquisiti e autenticati nel prefisso locale; nessun kernel GPU qualificato.
- `M1-e-tools-retention.md`: rimosse dieci sole copie estratte ridondanti, con deb e artefatti congelati conservati; nessuna modifica ai tool selezionati.
- `M1-c-review.md`: review statica indipendente del primo harness di persistenza e quattro correzioni richieste; nessun test di persistenza eseguito.
- `M1-c-static-followup.md`: quattro correzioni verificate staticamente sul candidato successivo; ammissione alla prima validazione, non PASS dei test.
- `M1-a-error-contract.md`: preparazione della nuova versione separata dei contratti d'errore; AST e sintassi verificati, esecuzione ancora da svolgere.
- `M1-a-error-review.md`: review statica del candidato; due correzioni confermate e due ulteriori rilievi prima dell'esecuzione.
- `M1-d-review.md`: review statica del pool bounded; tre correzioni richieste, nessun test dinamico eseguito.
- `M1-storage.md`: manutenzione sparse delle sole fixture generate, con verifica dei contenuti e dei limiti fisici; nessuna equivalenza prestazionale I/O dichiarata.
- `M1-c-validation.md`: prima validazione effettiva, 44 casi SQLite e 46 JSON; review indipendente dei risultati distinta dall'esecuzione.
- `M1-c-validation-review.md`: conteggi, esiti e artefatti verificati indipendentemente; nessun nuovo benchmark o backend adottato.
- `M1-a-ownership.md`, `M1-a-ownership-review.md`: ponte C ABI eseguito e review indipendente single-thread; errata descrittiva e limiti espliciti.
- `M1-d-corrections.md`: correzioni statiche del candidato scheduler; regressioni predisposte ma non ancora eseguite.

I record distinguono fatti alla data della prova ed eventuali errata successive. Lo stato aggregato e la prossima azione restano in `docs/progress.md`.
