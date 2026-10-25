# M1.a: core C++, Rust e NumPy

Unità: M1.a. Esito: **PARZIALE per il gate complessivo**. Build, validazione numerica, misure preregistrate, B puntuale, fault-stop e sanitizer C++ eseguiti con esito positivo. Restano ownership FFI condivisa, contratto delle maschere e review indipendente. Nessuna decisione di stack o chiusura M1 dichiarata. È selezione tecnica su sintetico analitico, non validazione M3.

Autore: GPT-6.1 Sol, OpenAI, dichiarato dal runtime, non attestato. Nessun subagente, costo o token inventato. Accessi operativi dichiarati nel record [M1-e](M1-e.md); inoltre sono state lette le dichiarazioni FFTW del sistema, non sorgenti Siril o foto private. Nessuna operazione Git, installazione o accesso rete in questo incarico.

## Input, sorgenti e versioni

Piano operativo verificato tramite SHA-256: `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`. La serie corrente `tmp/m1/a/series-002` registra PRIMA dei risultati il protocollo stabile `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.

Sorgenti usa e getta: `tmp/m1/src/{core.cpp,core.rs,numpy_worker.py,run.py}`, ambiente `tmp/m1/env.sh`. Manifest di sorgenti e binari: `series-002/source-manifest-before-results.json`. I percorsi della toolchain Rust non sono pubblicati: `RUST_TOOLCHAIN_DIR` è risolto tramite metadati esistenti e usato direttamente, senza shim rustup o Cargo/download.

Versioni effettive: GCC/G++ **15.2.0** (`15.2.0-16ubuntu1`), rustc **1.99.0** (`b940084d7`, 2026-09-28), Python **3.14.4**, NumPy **2.3.5**, FFTW **3.3.10**. C++ e Rust chiamano direttamente FFTW float/double; NumPy usa `numpy.fft`. Non sono tre implementazioni FFT indipendenti: i primi due condividono la libreria.

## Comandi reali

Eseguiti nella radice, non semplicemente verificati con bash -n:

```bash
source tmp/m1/env.sh
g++ -std=c++20 -O3 -fno-fast-math -ffp-contract=off -Wall -Wextra -Wpedantic tmp/m1/src/core.cpp $(pkg-config --cflags --libs fftw3 fftw3f) -o tmp/m1/build/core-cpp
"$RUST_TOOLCHAIN_DIR/bin/rustc" --edition=2024 -C opt-level=3 -C target-cpu=x86-64 -l fftw3 -l fftw3f tmp/m1/src/core.rs -o tmp/m1/build/core-rust
/usr/bin/python3 -B tmp/m1/src/run.py --unit a --phase validate --output tmp/m1/a/series-001
/usr/bin/python3 -B tmp/m1/src/run.py --unit a --phase measure --output tmp/m1/a/series-001
# Dopo rettifica del protocollo e dell'harness, senza modificare series-001:
/usr/bin/python3 -B tmp/m1/src/run.py --unit a --phase validate --output tmp/m1/a/series-002
/usr/bin/python3 -B tmp/m1/src/run.py --unit a --phase measure --output tmp/m1/a/series-002
```

Le build sono state ripetute dopo la rettifica FFT: exit **0** per C++ e Rust, nessun warning nei rispettivi stderr. Con `/usr/bin/time`, la build corretta C++ ha richiesto **1.53 s**, **218236 KiB** peak RSS; Rust **0.53 s**, **138708 KiB**. Non sono tempi kernel e non selezionano un vincitore. Log: `tmp/m1/a/build-{cpp,rust}-002.{stdout,stderr,time}`. I log iniziali restano separati.

L'ambiente limita i thread dichiarati a uno, usa temporanei/cache sotto `tmp/` e non svuota cache OS. `series-002/versions.json` registra formato IEEE, endian little e versioni. I worker persistenti sono alternati dal supervisore; non eseguono kernel concorrenti. Il supervisore controlla RSS del proprio figlio e una guardia di 180 s per richiesta, non SLA di prodotto; uccide soltanto quel figlio in caso di superamento.

## Serie esplorativa conservata e fallimenti

`series-001` resta legata al protocollo `93468f1ceb4afcd735a822a3b87589e963917260ce6d87cfdf0269c6721cc9ff`. Ha superato **96 casi × 3 candidati = 288 confronti**. Non conteneva seno sui due assi né inverse alimentata con spettro indipendente; non soddisfa il protocollo rettificato e non viene rietichettata.

Il tentativo di misura 001 è fallito con exit **1** PRIMA di produrre campioni: la guardia del manifest ha rilevato il cambiamento di `plans/M1-spikes.md`. Errore conservato in `tmp/m1/a/measure-001.stderr`; status esplicito in `series-001/series-status.json`. Snapshot dei sorgenti precedenti: `series-001/source-snapshot.zip`, SHA-256 `421c1f8fc2bc417ea383f818deb39c72725f1acfe7f223fd496314b6aeb11e99`.

Il primo avvio del nuovo harness ha prodotto `SyntaxError: unmatched ')'`, exit **1**, prima della creazione della serie o di risultati numerici. È stato corretto il delimitatore, senza cambiare formule o tolleranze. Conservati `validate-002.stderr` e `validate-002-retry1.{stdout,stderr}`. Il retry completo è riuscito: nessuna fixture saltata per correggere il test.

## Ground truth e validazione corrente

GT derivato da interi/rational e formula per indice del protocollo, non dall'output di un candidato. Oracle scalare `Fraction` sul 17×19 verificato contro la generazione intera; hash delle fixture immutabili riutilizzate da 001 controllati prima della nuova validazione, senza riutilizzare i vecchi verdetti. `cases.json` elenca input, GT, shape, dtype, stride e relativi SHA-256.

- Immagini 17×19, 257×263, 1024×1024; float32/float64; contigue e padding di riga 17. Affine, convoluzione dyadic 5×5, filtro non dyadic 5×5, warp `(1/4,3/4)`, identità e traslazione intera: **72 casi**.
- FFT 32×32 e 1024×1024; due precisioni e due stride; impulso, costante, coseno e seno separati lungo entrambi gli assi: **48 casi**. Forward con segno negativo, inverse con segno positivo e divisione per N totale.
- Per l'inverse i worker leggono lo spettro analitico indipendente, non la propria forward. L'oracle confronta tutti i valori reali e immaginari di forward e inverse. Le code di riga sentinella non sono dati attivi.
- Tolleranza zero sui casi dyadic e FFT dichiarati. Il caso non dyadic registra coefficienti effettivi e gamma50 PRIMA dei confronti in `bounds-before-results.json`; nessuna epsilon ottenuta dai residui.

L'harness ha usato inizialmente il limite conservativo globale ottenuto dal B preregistrato con `abs(x)<=1/2`: float32 **1.501296397878695e-06** arrotondato verso l'alto, float64 **2.785965902418768e-15**. Le frazioni esatte, i coefficienti hex e gli estremi floor/ceil sono nel manifest del bound. Il confronto include l'incertezza di arrotondamento dell'oracle, con verifica Fraction nei casi ambigui. Non si presenta questo limite globale come identico al limite locale: il supplemento puntuale più stretto sotto qualifica il filtro senza cambiare la formula.

Validazione 002: exit **0**, **120 casi × 3 candidati = 360 confronti PASS** nel dominio sopra e con i limiti dichiarati. Nessun FAIL numerico osservato. Log completo `series-002/validation.jsonl`, riepilogo `validation-summary.json`, oracle e bounds nello stesso percorso. Le voci NOT RUN del riepilogo sono lo snapshot prima degli audit successivi; non è stato riscritto per attribuirgli risultati futuri.

## Misure e gate residui

La serie 002 è terminata con exit **0**: **7560 campioni warm**, **2160 warm-up** e **1080 startup**, su 120 casi e tre candidati. Applica tre cluster di processo p, due warm-up e sette round persistenti per cluster, alternando C++/Rust/NumPy. Registra startup separati, p/r, wall/CPU, T_total/kernel/load, RSS, budget buffer e SHA-256 degli output. Il significato di cold è processo nuovo, non page cache fredda. Nessun errore numerico nei controlli associati ai campioni.

Il bootstrap implementato usa 2000 ricampionamenti, seed 4059, tre cluster con reinserimento e sette coppie di round entro cluster; statistica `exp(median(log(Tref/Tcandidate)))`, percentili lineari 2.5/97.5. Nessun intervallo GPU: quel confronto non è stato eseguito e nessun riferimento CPU è stato selezionato su questi campioni.

Artefatti raw: `series-002/{samples.jsonl,cold-process.jsonl,timing-summary.json,paired-bootstrap.json,measure-summary.json}`. Il riepilogo conserva i NOT RUN dello snapshot di fine misura; gli audit seguenti hanno verbali propri. La guardia del manifest è stata ricontrollata: protocollo, sorgenti e binari della serie invariati.

Comandi di verifica effettivamente eseguiti DOPO il completamento della misura, tutti exit **0**:

```bash
source tmp/m1/env.sh
/usr/bin/python3 -B tmp/m1/tests/audit.py --phase pointwise
/usr/bin/python3 -B tmp/m1/tests/audit.py --phase faults
/usr/bin/python3 -B tmp/m1/tests/check_measurements.py
/usr/bin/python3 -B tmp/m1/tests/backend_identity.py
g++ -std=c++20 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-fast-math -ffp-contract=off -Wall -Wextra -Wpedantic tmp/m1/src/core.cpp $(pkg-config --cflags --libs fftw3 fftw3f) -o tmp/m1/build/core-cpp-asan
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:allocator_may_return_null=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=0 /usr/bin/python3 -B tmp/m1/tests/audit.py --phase asan
```

### Supplementi di correttezza completati

- **B puntuale: 36/36 confronti PASS.** Sono congelati i bound locali da input/coefficienti PRIMA di leggere i nuovi output. Poiché i pesi del filtro sono potenze di due moltiplicate per c0, ogni pixel usa esattamente `B=(gamma50*abs(c0)+abs(c0-1/100))*sum(kj*ki*abs(q_neighbor))/1024`. Le somme sono intere; il coefficiente è Fraction. Un intervallo arrotondato verso l'esterno governa il confronto, con confronto esatto Fraction solo per decisioni ambigue, non cambio di algoritmo del prodotto. Nessuna epsilon ricavata dai residui e nessun indebolimento del bound locale. Tutti gli output di replay hanno lo stesso SHA-256 di TUTTI i campioni warm corrispondenti. Errori massimi diagnostici: float32 **6.556510923072523e-08**, float64 **1.6653345369377348e-16**. Nessun pixel ha richiesto il confronto Fraction di riserva.
- **Fault-stop: 36/36 PASS.** Per ciascun candidato e dtype: NaN, shape invalida, budget ecceduto, dimensione file incoerente, kernel sconosciuto, precisione sconosciuta. Tutti exit **2**, diagnosi presente, nessun output pubblicato. I contatori del supervisore registrano soltanto il processo esplicitamente richiesto, zero chiamate agli altri candidati. Un timeout o kill di guardia non è accettato come successo di questi test. Non è una prova di ogni fallback interno alle librerie.
- **Valori speciali: 6/6 copie bitwise PASS**, per candidato/dtype: +0, -0, subnormale minimo positivo/negativo e normale minimo positivo/negativo. È test di copia/identità, non validazione di tutti gli operatori sui subnormali.
- **ASan/UBSan: 120/120 casi C++ PASS**, incluse FFT e B puntuale; stderr vuoto, nessuna diagnosi sanitizer. Build separata O1, **2.91 s**, peak RSS build **286708 KiB**; nessuna misura O3 contaminata. SHA binario strumentato: `bbc35ec7d3db2d9bcc743a031091aba446350c285ae4652597bfe71005583873`. La libreria FFTW precompilata non è stata ricompilata con sanitizer; nessuna copertura Rust stable presunta.
- **Audit statistico PASS:** p/r, 21 warm e tre startup per candidato/caso, hash invariati, limiti dichiarati e self-test gerarchico con rapporti costanti esatti. Il self-test non sostituisce una review indipendente dell'implementazione statistica.
- **Identità processi:** ciascun worker riporta il candidato richiesto e un thread osservato dopo un replay affine. È snapshot post-kernel, non traccia dei thread transitori di tutti i kernel misurati.

Record dei supplementi: `series-002/audit-{pointwise,faults,asan}/{summary.json,results.json,source-manifest.json}`; bound locali in `audit-pointwise/pointwise-bounds-before-results.json`; conti/limiti in `measurement-audit.json`; identità in `backend-identity/results.json`. Output e errori dei comandi sono in `tmp/m1/a/audit-*.{stdout,stderr}`.

### Tempi osservati, non macchina di riferimento del prodotto

Mediane T_total in millisecondi, 21 campioni per cella. Include preflight, allocazioni/copie esplicite e output host pronto; esclude caricamento fixture, scrittura del risultato e oracle, che restano misurati separatamente. T_kernel ha confini di allocazione diversi nei wrapper: non usarlo come confronto end-to-end.

| Caso 1024×1024, contiguo | C++ | Rust | NumPy |
|---|---:|---:|---:|
| Affine float32 | 2.763 | 1.516 | 2.279 |
| Convoluzione dyadic float32 | 14.762 | 17.479 | 18.451 |
| Filtro non dyadic float32 | 15.203 | 18.053 | 17.143 |
| Warp float32 | 3.140 | 3.702 | 8.405 |
| FFT seno asse x float64, forward + inverse indipendente | 139.466 | 136.713 | 68.320 |
| FFT seno asse y float32, forward + inverse indipendente | 114.000 | 109.784 | 59.592 |

Esempio di statistica appaiata, non rapporto delle mediane: convoluzione dyadic float32, `T_cpp/T_rust` **0.847258**, CI percentile gerarchico **[0.818418, 0.869217]**, tre cluster. Campioni, MAD e intervalli di tutti i casi rimangono nei JSON. I costi FFT includono creazione dei piani FFTW ESTIMATE a ogni chiamata; il riuso dei piani non è stato confrontato. Questa tabella non dimostra una superiorità generale di linguaggio o libreria.

Startup della stessa convoluzione, tre processi nuovi: C++ **2.521/2.355/2.484 ms**, Rust **1.794/1.666/1.568 ms**, NumPy **62.170/65.500/61.896 ms**. Nessuna cache OS è stata svuotata.

Picco RSS massimo osservato nella serie temporizzata: **185348 KiB**. Massimo buffer payload dichiarato dai worker: **92413952 byte**, non inventario completo di tutte le allocazioni interne. Il limite RSS del supervisore è best effort. Scratch fisico dell'intera `tmp/` verificato sotto **1 GiB**, mantenendo la serie 001 e i suoi errori; nessun dataset o qualità ridotto per far passare i casi.

## Primo gate incompleto e handoff

**M1.a resta PARZIALE:** manca la prova di ownership di un buffer condiviso C++/Rust/Python con rilascio una sola volta e double-release rifiutato; manca il contratto/test di maschera incompatibile. I worker comunicano oggi tramite file: non si presenta quel costo come FFI zero-copy. Non è ancora completato l'inventario dei byte copiati e delle allocazioni interne alle librerie. Serve review indipendente dei sorgenti/risultati prima della decisione tecnica.

Il formato IEEE e i risultati entro B sono verificati sui casi eseguiti. Modo di arrotondamento ed exception flags non sono stati tracciati a ogni richiesta: l'ipotesi nearest di processo standard non viene presentata come audit generale dell'ambiente floating-point. I contratti di produzione dovranno renderla esplicita e verificabile.

Prossimo lavoro concreto: aggiungere nell'area isolata un test C ABI di ownership/rilascio e schema maschera, con manifest e fault-stop prima delle relative misure. Nessun codice prodotto. M1.b/c/d/f e review della scelta stack restano distinti; M1.f non è stato avviato in questo incarico. Nessun risultato qui dimostra correttezza M3, supporto L1/L2/L3, adozione lecita di dipendenze o assenza universale di fallback. M1 non è conclusa.

Alla ripresa usare `series-002/cases.json`, manifest e fixture già hashate: non rigenerare input o ripartire da M0. Le directory di serie e audit sono write-once; nuove prove hanno un ID nuovo e riusano gli input immutabili entro il budget, senza sovrascrivere questi risultati.

Verifica degli artefatti eseguita con `source tmp/m1/env.sh; /usr/bin/python3 -B tmp/m1/tests/final_checks.py`, exit **0**: 168 file di fixture distinti ricontrollati via SHA-256, snapshot dei vecchi sorgenti preservato, sorgenti/binari/protocollo correnti invariati, sintassi Python e tre blocchi shell dei due report validi. Il controllo shell non esegue nuovamente i comandi. Ricevuta ripetibile e hash dei due report: `tmp/m1/final-handoff.json`. Scratch allocato al controllo circa **959 MB decimali**, inferiore a 1 GiB; le misure sopra sono già finite, non in background alla consegna.
