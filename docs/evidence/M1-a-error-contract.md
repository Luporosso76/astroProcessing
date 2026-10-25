# M1.a: preparazione del contratto di errore v3

Consegna stabile del 25 ottobre 2026. **Correzioni preparate; validazione dinamica NON ESEGUITA.** Questa consegna non chiude i tre finding MED né il gate M1.a. Occorrono la sessione test ammessa dal coordinatore e la review indipendente del codice e dei risultati effettivi.

Autore: `openai/gpt-6.1-sol`, identità autodichiarata dal runtime, non attestazione. Nessuna delega. Costi e token non esposti. Sono stati letti AGENTS.md (52 righe), progress (39), piano M1 (169), M1-a (115), M1-e (41), i quattro sorgenti originali (111/136/84/431), audit.py (160) ed env.sh (10). Indici assenti, nessun indice creato.

## Perimetro e provenienza

Nuovo namespace esclusivo: `tmp/m1/a-fault-contract-v3/`. I quattro sorgenti sono stati copiati da `tmp/m1/src/` prima delle patch. Le funzioni del bound puntuale sono copiate da `tmp/m1/tests/audit.py`; il controllo aggiunto di shape/finitezza dell'output rifiuta valori invalidi senza cambiare B.

La serie corretta è **`tmp/m1/a/series-002`**, non `tmp/m1/series-002`. Nessuna scrittura nella serie, nei vecchi sorgenti, nei manifest o nei report M1-a/M1-e. Nessuna modifica a progress, ownership, persistenza, M1.d o tools. Nessuna operazione Git, rete, installazione, accesso a segreti o codice di prodotto.

Hash ricontrollati identici a quelli raccolti prima delle patch: quattro sorgenti originali, audit.py originale, `series-002/cases.json` e `series-002/source-manifest-before-results.json`. I valori completi sono nella ricevuta statica. Non è stato rieseguito l'hash dell'intero albero della serie.

I report preesistenti restano separati, con questi SHA-256 osservati alla consegna:

```text
808aff207794c8a68dc198fe35b00ba70c7ea2e8a52d1166aebaa08e3da67a7a  docs/evidence/M1-a.md
8635bc1b7412a25023908da6f6d4efa2d4d0dddc1f6d066f1107a341b8305c56  docs/evidence/M1-e.md
```

## Correzioni nel nuovo namespace

- `src/core.cpp`, `src/core.rs`, `src/numpy_worker.py`: verifica di ogni componente reale e immaginaria dello spettro inverse prima di qualsiasi kernel, compresa la forward. NaN e ±Inf producono `nonfinite inverse input`, exit 2, prima dell'apertura dell'output. Questa è una proprietà del testo letto, non ancora un risultato di esecuzione.
- Parser C++: una richiesta per riga, esattamente otto campi. EOF pulito e righe solo whitespace terminano senza errore. Richieste non vuote malformate, troncate o con campi aggiuntivi producono `STOP: malformed or truncated request`, exit 2. Una riga completa senza newline finale resta valida. Errore di lettura distinto da EOF: `STOP: request read failure`.
- Eliminato `other_backend_calls=0` dai tre worker. `src/run.py` registra tentativi, argv effettivamente passati a Popen, PID restituiti, invii delle richieste, risposte e uscita dei processi in journal JSONL per worker. I contatori dei launch derivano dagli eventi `process_launched`; i contatori delle richieste dagli invii osservati, non dalla matrice attesa.
- Il dispatcher riconosce il backend dall'argv effettivo rispetto alla mappa congelata. Un argv alternativo o sconosciuto produce un evento di rifiuto e stop prima del launch. L'audit ricontrolla il journal, la coerenza dei PID e la corrispondenza tra tentativi e launch. Un rifiuto resta un errore anche quando il chiamante intercetta l'eccezione.

L'osservabilità riguarda **soltanto il confine del dispatcher strumentato del nostro supervisore**. Non traccia decisioni interne FFTW/NumPy, loader o processi estranei. Zero launch alternativi osservati non significa assenza universale di fallback. Le misure 002 non vengono ricalcolate o riqualificate da questa strumentazione nuova; i vecchi contatori sintetici non diventano osservazioni retroattive.

## Regressioni preparate, tutte NON ESEGUITE

L'entry point è `validate.py`; il main della copia `src/run.py` rifiuta l'avvio e disabilita il benchmark da CLI. I nuovi test leggono i casi e le fixture immutabili della 002. Scrivono esclusivamente sotto il namespace v3 e non rigenerano le fixture della serie.

| Insieme preparato | Conteggio previsto | Criterio |
|---|---:|---|
| Finiti | 120 casi × 3 backend = 360 | Stesse fixture; tolleranza zero dove prevista, B puntuale invariato per convr |
| Vecchi errori | 6 × 2 dtype × 3 backend = 36 | NaN spaziale, shape, budget, size, operazione e precisione; exit 2, diagnosi, nessun output/risposta |
| Spettro inverse | 3 valori × 2 componenti × 2 dtype × 3 backend = 36 | NaN, +Inf, -Inf sull'ultimo bin, separatamente real/imag; diagnosi specifica, exit 2, nessun output/risposta |
| Copia valori speciali | 2 dtype × 3 backend = 6 | ±0, subnormali e normali minimi con confronto bitwise |
| Parser C++ | 11 | Due EOF puliti; sette richieste malformate/troncate più troncamento tra righe; una riga completa senza newline finale |
| Mutation del dispatch | 2 | Da contesto cpp tentare argv rust e numpy; ogni sottoprocesso di probe deve uscire 2, audit FAIL, tentativo registrato e nessun processo alternativo lanciato |

I probe mutation devono fallire intenzionalmente con `STOP: dispatch audit failed`. La regressione esterna accetta quel fallimento soltanto se il journal prova tentativo e rifiuto, senza alcun `process_launched` né summary di validazione accettata. Non viene eseguito un backend sostitutivo come risposta al guasto.

I bound puntuali vengono congelati prima dei nuovi output. Gli output finiti superati vengono rimossi solo dall'area v3, uno alla volta; gli input e i risultati 002 restano immutabili. Nessuna soglia allentata, candidato rimosso o misura di prestazione nuova. Nessuna analisi dei tempi 002 in questo incarico.

## Controlli realmente eseguiti

Ambiente: Linux nella workspace `/opt/workspace/astroProcessor`; Python **3.14.4**, Bash **5.3.9(1)-release**. Non sono state verificate di nuovo le versioni GCC/Rust/FFTW/NumPy, né importato NumPy. Le versioni storiche in M1-a non sono attestazioni di una build v3.

```bash
/usr/bin/python3 -B tmp/m1/a-fault-contract-v3/check_static.py --output tmp/m1/a-fault-contract-v3/static-checks-final.json
```

Exit **0**: cinque file Python superano `ast.parse`; `validate.sh` supera `bash -n`; sette hash originali ricontrollati. Il checker importa solo librerie standard per parsing, hash, metadati e invocazione di `bash -n`. Non importa né esegue alcun modulo candidato. Il controllo iniziale, precedente al checker ripetibile, resta in `static-checks.json`.

Ricevuta completa: `tmp/m1/a-fault-contract-v3/static-checks-final.json`, SHA-256 **`f1be7647f51b24c76f49a6606c1b97f3d51b5fdaaa66f9e22970c57061b777a4`**. Include hash dei sorgenti, originali verificati e limiti. Nessuna compilazione o verifica sintattica C++/Rust, import dei candidati, kernel, regressione dinamica o benchmark eseguito.

Alla misura della ricevuta, blocchi allocati dei file regolari sotto tmp: **1,014,726,656 byte**, contro **1,073,741,824 byte**. `du -s -B1 tmp` successivo: **1,014,939,648 byte**; namespace v3: **94,208 byte**. Sono fotografie, non una prenotazione contro altri autori. La preparazione aggiunge solo sorgenti e piccoli metadati, nessuna build o fixture numerica.

SHA-256 dei file nuovi, percorsi relativi al namespace v3:

```text
191afce535d5e00d637271278b15182f55e3cb97d6b5f14410eb3c8838272c05  src/audit_reference.py
5612dfcb1ab17e2178227a39ccce0274b06ea202b920a1b6f3e921bdccea9e3d  src/core.cpp
3a441ef30a0a89df7bf3967f8af98ccdd8632239e0e71ea136a8c77c04c95a19  src/core.rs
0f2df76ac2aefe93c8ce8284348be17e18d810e6c2a2205061b40788e7b2093f  src/numpy_worker.py
3f585423e0e26ddbef50b83d2eb7b0281a738d20a0be5be5b0c3de3377c5c019  src/run.py
1d8012fb1cfe5b1d9d1c45e039d8ddd689f1cf7aff6d304e0d2460e7754e6ba4  validate.py
7bb3718f5332ab82f51480bb674334f08114cdf0e1a0ee8053690acbb66b4667  validate.sh
665840ce31bd088127a3117fd410e7eaa1b6d22c7bcc0ec49828bb0e3506a02c  check_static.py
```

## Primo comando incompleto

**NON ESEGUITO.** Prima occorre che il coordinatore ammetta una sessione test esclusiva e imposti `RUST_TOOLCHAIN_DIR` alla toolchain diretta già disponibile. Il flag esplicita quella precondizione, non acquisisce un lock da solo.

```bash
bash tmp/m1/a-fault-contract-v3/validate.sh --admitted-test-session
```

Lo script verifica il budget globale con 48 MiB di margine, crea una build v3 nuova, congela gli hash, registra versioni/flag e avvia soltanto la validazione. Il validatore ricontrolla il budget prima degli output grandi e richiede 40 MiB liberi all'avvio. I controlli quota sono best effort e non coordinano scrittori esterni. Se manca spazio, toolchain o una dipendenza, stop: nessun cambio di algoritmo, backend, dtype o fixture. Build e directory di risultati sono write-once; un tentativo parziale resta conservato, non viene sovrascritto automaticamente.

Dopo una build già completata, la sola riga futura di validazione è `/usr/bin/python3 -B tmp/m1/a-fault-contract-v3/validate.py --admitted-test-session --output tmp/m1/a-fault-contract-v3/validation-session-001`, purché l'output non esista. L'esito dinamico e la review indipendente restano da registrare in un record successivo, senza riscrivere questa evidenza statica.
