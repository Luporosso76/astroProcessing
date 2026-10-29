# ExecPlan M2-residui: ID del registro a stadio M2 non chiusi

Stato: **PLANNED**. Riapre il lavoro di M2 dopo la revoca del PASS del Gate M2 (`docs/evidence/M2-gate.md`, stato RIAPERTO). `plans/M2.md` resta il piano della struttura del codice; per la completezza il gate di M2 passa a questo piano (sezione 6).

## 1. Ambito

- **Fonte unica**: il registro dell'appendice D di `ASTRO_POSTPROCESSING_PLAN_v2.2.1.md` (SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`), nodi D.1–D.4 alle righe 3152-3566 e archi D.6 alle righe 3603-4455. Gli ID con stadio `M2.*` o `M2.entry` sono **63**: 55 foglie e 8 consegne (M2, M2.a…g). L'appendice A di questo piano li elenca tutti col testo di accettazione esatto, estratto con `python3 -S` dal piano e non parafrasato.
- **Causa del difetto**: `plans/M2.md` è stato costruito dal §21.4 (righe 2659-2675) e non dal registro. `docs/test-matrix.md` traccia gli ID di contratto (`T-M2X-NN`), non gli ID del piano, quindi un P0 senza test non risultava.
- **Lavoro**: completare le foglie mancanti e parziali; rieseguire i test delle foglie già eseguite che hanno un prerequisito `requires_all` rimasto aperto (riga 3150: «Nello stesso stadio vale l'ordine topologico degli archi»); rendere ogni ID tracciabile fino a un test eseguito e a un record.
- **Fuori**: ciò che il registro assegna a M3 e oltre: F-CAL-01.b (M3.b); F-CAL-02.b, 03.b, 06.b (M3.c); F-RES-05.b…08.b (M6); stadi `.b` di ADR e contratti; NFR-SEC-04 (M3.s); T-PRJ-01 completo (M3.s); GPU e VRAM produttive (M7); GUI (M4, con stop col proprietario). Nessun ID di M3 viene anticipato e nessun ID di M2 viene rinviato.

## 2. Esito della rilettura del registro

Stato di partenza per ID nella sezione «ID del piano → test» di `docs/test-matrix.md`. Riepilogo:

| Esito | ID |
|---|---|
| Mancanti (nessun test) | F-IO-05, F-IO-10, F-CAL-01.a, F-RES-04, NFR-SIMD-01, C-02.a, NFR-SEC-02 |
| Parziali | F-IO-01, F-PRJ-06, F-RES-03, F-RES-05.a, F-RES-06.a, F-RES-07.a, F-RES-08.a, NFR-REP-01.a, NFR-SEC-03, NFR-LIC-01.c, NFR-PERF-01, ADR-07.a, C-10.a |
| Eseguiti ma da rieseguire (prerequisito interno a M2 non chiuso) | F-CAL-02.a, F-CAL-03.a, F-CAL-06.a, C-01.a, NFR-CPU-01.a, NFR-OFF-01.a, NFR-FBK-01.a, G-STOP, F-PRJ-02 (anche perché il workflow si estende ai master); F-PRJ-01 (ADR-04.a), F-PRJ-03 (ADR-05.a), F-PRJ-04 e F-PRJ-07 (F-PRJ-01, F-PRJ-03), F-RES-01 e F-RES-02 (ADR-06.a), NFR-EXP-01.a (F-IO-01, arco 3967), NFR-SEC-01 (ADR-08.a), NFR-TEST-01.a (ADR-01), NFR-DATA-01.a (F-PRJ-06, arco 4031) |
| Eseguiti, manca solo il verbale della decisione | ADR-01, ADR-02, ADR-03.a, ADR-04.a, ADR-05.a, ADR-06.a, ADR-08.a, ADR-09.a, ADR-13.a, ADR-16.a, ADR-17.a, D-03, D-04, D-05, D-09.a, D-17.a |
| Eseguiti e chiusi | Nessuno: ogni ID eseguito ha almeno un prerequisito interno a M2 non chiuso |

Regola di chiusura. Durante il lavoro un dipendente può partire quando i suoi prerequisiti interni a M2 sono abilitati. Una decisione (ADR, C, D) è abilitata da `APPROVATO`, cioè verbale e contratto passati dalla review (D24); un ID funzionale è abilitato da `CHIUSO`, cioè test passato. Un ID eseguito prima di questa abilitazione è `DA RIESEGUIRE`; il calcolo è transitivo, fatto con `python3 -S` sugli archi. Lo stato finale di ogni ID e di ogni consegna si decide solo al gate, con l'esecuzione finale completa (sezione 6): `CHIUSO`, e `VERIFICATO` per gli ID di decisione.
| Consegne aperte (derivano dalle foglie) | M2, M2.a, M2.b, M2.c, M2.d, M2.e, M2.f, M2.g |

Rilievi **oltre l'elenco dell'audit**, verificati su codice e documenti:

1. **NFR-LIC-01.c parziale.** La riga 2018 chiede «Inventario con versione, hash, moduli e dipendenze transitive prima di adottare ogni libreria». `docs/licenses.md` non ha hash, moduli usati né dipendenze transitive (per esempio le dipendenze di `libcfitsio`). ADR-03.a e ADR-08.a dipendono da NFR-LIC-01.c (archi 4080, 4087).
2. **NFR-SEC-02 senza test.** Il codice è conforme: nessun `system`, `popen`, `exec*`, `posix_spawn` o `dlopen` in `src/core`, verificato con grep (solo `tests/unit/m2d_support.hpp` usa `execv` per i figli dei test). Però `tools/check_offline.sh` non vieta questi simboli e nessun test lo verifica.
3. **Verbale degli stadi `.a` assente.** `docs/decisions/README.md` dà «Proposto» per ADR-03, 05, 08, 09, 13, 16 e 17, mentre `M2.g decided_by` ADR-03.a…17.a (archi 4211-4218) e `decided_by` «richiede approvazione prima della costruzione» (riga 3148). Il piano dà D-03, D-04 e D-05 come «APERTA tecnica» (righe 2869-2871). Nel repository nessun verbale le chiude, e nessuno chiude D-09.a e D-17.a.
4. **Catena topologica di M2.e.** F-CAL-03.a `requires_all` F-CAL-01.a (arco 3721), F-CAL-02.a → F-CAL-03.a e C-01.a (3722, 3723), F-CAL-06.a → F-CAL-02.a (3724). Da F-CAL-06.a dipendono NFR-SIMD-01, NFR-CPU-01.a e NFR-REP-01.a (archi 3999, 3989, 3974), e NFR-OFF-01.a dipende da M2.e (4001). Con F-CAL-01.a mancante, i T-M2E eseguiti con master costruiti dal test vanno rieseguiti con i master costruiti dal prodotto. Stessa cosa per NFR-FBK-01.a → F-RES-03 (4037) e G-STOP → NFR-FBK-01.a (4379).
5. **NFR-REP-01.a include l'export.** La riga 2002 elenca fra le fasi critiche anche export e recovery. L'export è stato introdotto in M2.c ma non ha report; mancano anche i report di import, di master e di recovery.
6. **F-PRJ-06, perdita di energia.** La riga 1832 nomina «crash, kill o perdita di energia». `docs/progress.md` la dichiara fuori dal modello di minaccia, ma il testo P0 la include e manca una prova.
7. **C-10.a, disco pieno.** Il campo 10 (riga 1547) dice «Disco pieno: nessun file completo». ENOSPC non è mai stato iniettato sull'export, né altrove.
8. **F-IO-01, campioni pubblici.** C'è un solo campione pubblico, mono (PUB-LFC-01). Un candidato CFA con licenza CC BY 4.0 è già censito (`docs/datasets/M3-candidates.md` riga 40, Zenodo 17843622).
9. **NFR-DATA-01.a e cache.** La riga 2023 chiede «Export e cache hanno integrità verificabile»: la cache delle thumbnails introdotta da F-IO-05 deve rispettarla.
10. **F-RES-06.a, confine.** La riga 1922 dice «Il confine tra `.a` e `.b` richiede una decisione esplicita», e la decisione non è registrata.
11. **ID di test del piano non tracciati.** T-IO-01 (§15.2, riga 2342) e T-PRJ-01 (registro riga 3547, «incrementale da M2»; F-PRJ-06 `verified_by` T-PRJ-01, arco 3715) non compaiono come nomi nella matrice.

Verifiche sul codice (indice `.codegraph/` presente, grep mirati in `src/core`):
- `calibration_backend = "cpu_scalar"`, nessun simbolo SIMD o `__builtin_cpu_*`.
- `enum class linearity { linear, nonlinear }` senza `unknown` e senza fase di processo.
- `uninterpreted_keywords` copiate nell'output (`calibration.cpp:831`) ma assenti da `calibration_report.cpp`.
- `master_descriptor` solo dichiarato, nessuna funzione di combinazione.
- `calibration_job_options` con default fissi (`worker_count = 1`, `tile_rows = 64`, `queue_capacity = 4`); il job decodifica ogni file intero (`calibration_job.cpp:182-183`) e non usa checkpoint.
- `job_queue_capacity = 16`, `budget_fraction_normal = 0.375`, `budget_fraction_pressure = 0.25` e `budget_psi_threshold = 10.0` sono `constexpr`; la PSI è letta solo nello snapshot del budget.
- `recovery_report` esiste solo in memoria (`sqlite_dag_store.hpp:49`).
- Nessun controllo `st_nlink`.
- Nessun calcolo del minimo fattibile né gestione di ENOSPC.

## 3. Prerequisiti

- M1 DONE; stack in `docs/decisions/ADR-M1-stack.md`. Contratti M2 in `docs/contracts/` (ADR-03, 04, 05, 06, 13, 16, 17).
- Nessuna dipendenza nuova. SIMD con i builtin di GCC 15.2 (`__attribute__((target("avx2")))`, `__builtin_cpu_supports`), senza librerie.
- Build e test della macchina di sviluppo al massimo al 70% di CPU e RAM, temporanei in `tmp/`. È una regola operativa dello sviluppo e non un criterio del prodotto: la riga 2022 lo esclude esplicitamente dal progetto.
- Acquisizione di un campione CFA pubblico (R1): dataset lecito, licenza CC BY 4.0 verificata prima dell'uso, hash registrato prima del test.

## 4. Decisioni dichiarate e riga che le autorizza

Nessuna decisione declassa un requisito o lo rinvia. Ogni decisione sceglie fra percorsi che il piano ammette e va scritta nel contratto dell'unità prima del codice.

| # | ID | Decisione | Autorizzazione nel piano |
|---|---|---|---|
| D1 | F-IO-10 | La fase di processo (`raw`, `calibrated`, `prestacked`) è dichiarata all'import ed è obbligatoria; se è in conflitto con l'header il processo si ferma. La linearità ha tre valori (`linear`, `nonlinear`, `unknown`): `unknown` è un valore esplicito e non viene mai trattato come lineare. Un'operazione incompatibile con la fase o con la linearità si ferma prima del calcolo. | 1821 («con la fase di processo dichiarata», «blocca le operazioni non compatibili», «distingue linearità nota, NL e sconosciuta»), 1622, 2009 |
| D2 | F-IO-05 | L'inventario legge solo gli header di ogni HDU. L'anteprima legge a blocchi di righe gli HDU non compressi. Per un HDU compresso la decodifica è intera, con una prenotazione nel budget; se il budget non basta, stop col minimo fattibile. | 1816 («non caricano obbligatoriamente tutti i pixel», «la memoria resta dentro il budget»), 1919 («elaborato o rifiutato con preflight») |
| D3 | F-IO-05, NFR-DATA-01.a | Le thumbnails sono una cache derivata con chiave (digest della sorgente, parametri, versione dell'algoritmo) e digest verificato a ogni lettura. Il job `thumbnail` riusa una voce valida con la stessa chiave (`cache_hit`) oppure, se la voce è assente, la calcola (`computed`); l'esito finisce nella provenance, come per il riuso dei nodi di ADR-05. Una voce presente ma corrotta non conta come assente: il ramo che la richiede si ferma con `cache.corrupt`, con digest atteso e trovato nel contesto, e la voce non viene servita. La ricostruzione è un'azione separata ed esplicita: un job distinto `cache.rebuild`, registrato nella provenance con la causa. Non fa mai parte dell'esecuzione fallita e nessun percorso ricostruisce da solo. | 1816 («cache derivata e ricostruibile»), 2023 («cache hanno integrità verificabile»), 2009 e NC-21 (errore = stop controllato, nessuna continuazione alternativa), 2565 («cache esterna derivata») |
| D4 | F-IO-01 | Campioni pubblici: PUB-LFC-01 (mono) e un sub CFA Zenodo 17843622 (PUB-CFA-01), solo per import e inventario. R1 cerca anche un FITS RGB pubblico con licenza: se non lo trova, il limite si registra, e RGB e multi-plane restano coperti dalle fixture sintetiche T-M2A-13. | 1812 («fixture sintetiche e campioni pubblici licenziati passano»: non chiede un campione per layout), 2365 |
| D5 | F-CAL-01.a, C-02.a | Perimetro della slice: quattro ruoli (bias, dark, flat, dark-flat), combinazione `mean`, nessuna rejection, nessuna normalizzazione del flat; per il flat, livelli dei frame entro una tolleranza dichiarata. `median`, `sigma_clip` e `median_ratio` si fermano nel preflight con `unsupported`, `master.variance_domain`, e appartengono a F-CAL-01.b: la varianza della mediana su campioni quantizzati, quella dei pixel validi dopo il clipping e quella con scale stimate dai dati non sono derivate. Un gruppo incompatibile o con frame insufficienti si ferma prima del calcolo, senza esclusioni automatiche di frame. Set parziali e gruppi misti appartengono a F-CAL-01.b. | 1840 («La `.a` è la slice sintetica minima, la `.b` completa il requisito»), 1378, 1380 («L'incertezza del master non è sempre `sigma/sqrt(N)`: la correlazione tra frame e la normalizzazione vanno dichiarate»), 1384 («gruppo incompatibile: rifiuto») |
| D6 | F-CAL-01.a, ADR-17.a | La varianza del master è empirica, `s²/n` per pixel sui frame del gruppo, indipendente dal modello di rumore, con ipotesi, dominio ed errore scritti nel contratto C-02.a §6 prima della misura: valore atteso corretto per bias, dark e dark-flat con media stazionaria; per il flat, sovrastima in valore atteso dovuta ai livelli dei frame, limitata sotto ipotesi dichiarate in funzione della tolleranza sui livelli e del segnale atteso, e stimata nel report con una stima plug-in, che non è un limite. Il piano di un singolo pixel è una stima casuale, non una garanzia. Fuori dal dominio dichiarato, stop `unsupported`. | 1380 («L'incertezza del master non è sempre `sigma/sqrt(N)`»), 1845, 2578 |
| D7 | F-RES-03 | L'adattamento avviene nel preflight: il pianificatore sceglie tile, worker, residenza dei frame (intera o a blocchi di righe), residenza dei master e buffer in base al budget. Il piano entra nella ricetta e non cambia durante l'esecuzione. I valori espliciti dell'utente non vengono toccati: se non entrano nel budget, stop. | 1919, 2009 («streaming, spill e checkpoint sono ammessi solo se previsti dalla ricetta»), 2567, `ADR-M1-stack.md` riga 16 |
| D8 | F-RES-03, F-RES-07.a | Un dataset oltre la RAM viene elaborato a blocchi se il formato lo consente (HDU non compresso). Altrimenti è rifiutato nel preflight con il minimo fattibile dichiarato. In streaming il digest degli input si calcola sui byte effettivamente consumati e si confronta prima della pubblicazione. | 1919 («elaborato o rifiutato con preflight»), 1923 («rifiutato con il minimo fattibile dichiarato») |
| D9 | F-RES-04, F-RES-05.a | Il campo VRAM esiste nel monitor e nella configurazione e vale `null` con il motivo «nessun backend GPU nel prodotto fino a M7». | 1921 («VRAM quando disponibile»), 1920, `ADR-M1-stack.md` riga 14 |
| D10 | F-RES-05.a | L'obiettivo RSS è configurabile, misurato e riportato. Il superamento è un evento nel report, senza azioni automatiche. | 1921 («obiettivo RSS best effort»), 2670 («nessuna garanzia hard sull'RSS») |
| D11 | F-RES-06.a | Confine `.a`/`.b`: `.a` campiona la pressione a ogni ammissione, cioè ogni job preso dalla coda e ogni nuova prenotazione del pool, anche durante l'esecuzione. Sopra soglia le ammissioni si fermano; i job in corso proseguono con la loro ricetta; una prenotazione impossibile porta a stop con checkpoint (cancel sicuro); lo spill si usa solo se è dichiarato. `.b` (M6) è il cambio interattivo a caldo di budget e ricetta senza riavvio. | 1922 («Il confine tra `.a` e `.b` richiede una decisione esplicita») |
| D12 | F-RES-06.a | Sotto pressione l'ammissione attende senza timeout. L'attesa finisce solo quando la pressione cala o con un cancel, e il monitor ne espone lo stato. | 2022 («un timeout arbitrario non [è] criterio di progetto»), 1923 («backpressure») |
| D13 | F-RES-07.a, C-10.a | ENOSPC si prova iniettando il guasto nelle chiamate di scrittura del prodotto: spill, checkpoint, artefatti, export e cache. Un filesystem davvero pieno richiederebbe un mount o una modifica di sistema. | `AGENTS.md` («niente modifiche a servizi o configurazioni di sistema»; «iniezione di guasti sui percorsi del prodotto»), 1547 |
| D14 | NFR-PERF-01 | Ogni costante di RAM, tempo o margine diventa un parametro della configurazione delle risorse, persistito e validato, con default dichiarato e origine registrata nella provenance (per esempio «0.375, misurato sul PC di sviluppo, `M2-gate.md`»). `job_queue_capacity` diventa configurabile. L'inventario delle costanti si fa con ast-grep e grep su `src/core`. | 2022 («I limiti sono configurabili o misurati») |
| D15 | F-PRJ-06 | Il report di recovery elenca le fasi sicure e i risultati incompleti. Il resume parte solo su comando esplicito e con ricetta identica; all'apertura non riparte nulla da solo. | 2283 («il prodotto propone il ripristino delle fasi sicure, elenca i risultati incompleti»), 2009 |
| D16 | F-PRJ-06 | Una fase sicura è un nodo DAG con artefatto `completed` e digest riverificato, oppure un checkpoint valido di un nodo (F-RES-06.a). | 1832, 2565, 2566 |
| D17 | F-PRJ-06 | Perdita di energia: si simula il modello POSIX di `fsync` in spazio utente. Un VFS SQLite di test scarta le scritture non sincronizzate e un giornale registra write, fsync e rename degli artefatti. Limite dichiarato: un dispositivo che mente su `fsync` resta fuori dal modello. | 1832 («perdita di energia») |
| D18 | NFR-SEC-03 | Un file gestito dal progetto con `st_nlink ≠ 1`, o una destinazione con lo stesso `(st_dev, st_ino)` di un originale, porta a stop `security`, senza scritture e senza rimozioni. | 2016 («symlink, hardlink, path traversal») |
| D19 | NFR-SIMD-01, ADR-07.a | Due percorsi primari, `cpu_scalar` e `cpu_avx2` (x86-64). Il preflight legge le capacità della CPU e risolve `simd: auto, scalar o avx2` in un percorso concreto, registrato in backend, chiave del nodo e provenance. Un percorso richiesto ma non disponibile porta a stop `unsupported`. Il percorso non cambia durante l'esecuzione; un resume con percorso diverso dà `resume_mismatch`. | 2008 («dispatch nel preflight», «entrambi primari e dichiarati», «Un percorso non sostituisce l'altro a runtime»), 2568, `ADR-M1-stack.md` riga 14, 1366 («SIMD entro tolleranza») |
| D20 | NFR-SIMD-01 | Tolleranze derivate prima della misura e scritte nel contratto ADR-07; se l'implementazione usa uno schema diverso da quello dichiarato qui, il bound si ricava di nuovo prima di misurare. (a) **0 ulp** sui kernel elemento per elemento e sulle accumulazioni per pixel lungo i frame dei master. Sono vettorizzate fra pixel, quindi stesso ordine per pixel, stessa precisione, arrotondamento al più vicino, nessuna FMA e nessuna istruzione approssimata: ogni operazione IEEE dà lo stesso risultato correttamente arrotondato. La mediana di un numero dispari di valori è una selezione, esatta; con un numero pari è la media f64 dei due centrali, non esatta ma bit-identica fra i percorsi perché usa le stesse operazioni IEEE nello stesso ordine. Vale per le mediane della calibrazione (`N(Fc)`) e per il livello dei flat; i master non usano mediane per pixel. (b) **Riduzioni su pixel** (somme per statistiche globali). Per Higham, *Accuracy and Stability of Numerical Algorithms*, 2ª ed., SIAM 2002, cap. 4, §4.2, vale `\|ŝ − s\| ≤ Σ_i γ_(h_i) \|x_i\|`, con `h_i` il numero di addizioni attraversate da `x_i` e `γ_k = k·u/(1−k·u)`, `u = 2^−53`. Schema scalare: somma ricorsiva nell'ordine degli indici, accumulatore inizializzato col primo termine, quindi `h_i ≤ n−1` e `\|ŝ_sc − s\| ≤ γ_(n−1) Σ\|x_i\|`. Schema AVX2: 4 corsie f64, termine `i` nella corsia `i mod 4`; ogni corsia è ricorsiva sui suoi ≤ `⌈n/4⌉` termini, inizializzata col primo; riduzione finale a coppie `(c0+c1)+(c2+c3)`, profondità `⌈log2 4⌉ = 2`. Quindi `h_i ≤ ⌈n/4⌉+1` e `\|ŝ_simd − s\| ≤ γ_(⌈n/4⌉+1) Σ\|x_i\|`. **Underflow graduale.** Ogni arrotondamento al formato `p` segue il modello `fl(x) = x(1+δ) + ε`, con `\|δ\| ≤ u_p`, `\|ε\| ≤ η_p/2` e `η_p` il minimo subnormale (f32: `2^−149`; f64: `2^−1074`) (Higham, §2.1, modello con underflow graduale). Applicato alle somme, aggiunge al più `h·(1+γ_h)·η_64/2` per percorso, con `h` il numero massimo di addizioni attraversate da un termine. È una stima conservativa: un'addizione che va in underflow è esatta, ma il termine resta nel bound. Bound congiunto per disuguaglianza triangolare: `\|ŝ_sc − ŝ_simd\| ≤ (γ_(n−1) + γ_(⌈n/4⌉+1)) Σ\|x_i\| + ((n−1)(1+γ_(n−1)) + (⌈n/4⌉+1)(1+γ_(⌈n/4⌉+1)))·η_64/2`. Il test maggiora `Σ\|x_i\|` con `fl(Σ\|x_i\|)/(1−γ_(n−1))`. A ogni operazione successiva alla riduzione (divisione per `N`, conversione a f32) si aggiunge `\|fl(a) − fl(b)\| ≤ \|a − b\| + u_p(\|a\| + \|b\|) + η_p`. Quindi lo scarto ammesso fra i percorsi è almeno un ulp subnormale `η_p` per ogni conversione. Esempio: `a = 2^−150 − 2^−201` e `b = 2^−150 + 2^−201` danno in f32 `0` e `2^−149`, entro `2^−200 + u_32(\|a\|+\|b\|) + 2^−149`. **Classificazione**: dove lo scarto è 0 ulp, valore e classe (normale, subnormale, zero) coincidono, e con essi il bit della maschera. Sulle riduzioni la classe può differire solo se il valore esatto dista dalla soglia meno del bound congiunto; in quella banda ciascun percorso deve applicare la regola al proprio risultato. (c) **FMA vietata** in entrambi i percorsi (`-ffp-contract=off`, niente `-mfma`), come le istruzioni approssimate (`vrcpps`, `vrsqrtps`); lo verifica T-M2R5-05. (d) **Contro l'oracle**: il percorso scalare ha le tolleranze di T-M2E-02, T-M2E-20 e T-M2R2-02; il percorso AVX2 ha le stesse su (a) e `γ_(⌈n/4⌉+1)` al posto di `γ_(n−1)` su (b). | 2008 («con tolleranze numeriche proprie»), 1366 («SIMD entro tolleranza») |
| D21 | Stadi M2.entry | Un unico verbale, `docs/decisions/M2-entry.md`, chiude in autonomia ADR-01…17.a, C-01.a, C-02.a, C-10.a e D-03…D-17.a sulle evidenze esistenti, senza voto del proprietario. ADR-03.a e ADR-08.a si chiudono dopo NFR-LIC-01.c. | 3150 («Le decisioni tecniche `decided_by` si chiudono autonomamente dopo evidenze»), archi 4080, 4087 |
| D22 | Tracciabilità | Per gli ID di decisione (ADR, C, D), il «test» è la prova che sostiene la decisione più il test di contratto che la esercita. Per le consegne lo stato deriva dalle foglie. | 3148 («Gli aggregati non sono prerequisiti operativi: si usano le foglie»), `plans/M2.md` M2.g («Ogni contratto ha un test che lo esercita») |
| D23 | NFR-SIMD-01, NFR-CPU-01.a, C-01.a | Ambiente in virgola mobile e subnormali, uguali per i due percorsi. FTZ e DAZ sono vietati e l'arrotondamento deve essere al più vicino. Il preflight e ogni worker, all'avvio del task, leggono MXCSR (`_mm_getcsr`: bit FZ 15, DAZ 6, RC 13-14): un valore diverso porta a stop `numeric`, `cpu.fp_mode`, senza esecuzione e senza ripristino automatico del registro. Niente `-ffast-math` né `crtfastmath.o`, verificato sui simboli e sulle opzioni di link. Subnormali in ingresso: sono valori finiti validi e si leggono come sono (ADR-03 marca `source_nonfinite` solo sui non finiti). Subnormali in uscita, regola già in vigore (ADR-16 riga 142, ADR-17 riga 82, `calibration.cpp` `mul_ok`, `div_ok`, `add_ok`, `convert_finite`). Prodotti e quozienti con operandi non nulli e risultato zero o subnormale, e somme di termini non negativi non tutti nulli con quel risultato, danno il pixel `result_nonfinite`. Somme e differenze di valori con segno che danno zero o subnormale sono esatte (Sterbenz, underflow graduale) e restano valide. Ogni non finito dà `result_nonfinite`, salvo `Q` non finito da input validi, che dà `q_nonfinite` (ADR-16 §7). Dà `result_nonfinite` anche ogni valore non nullo che diventa zero o subnormale nella conversione al tipo di uscita. C-02.a applica la stessa regola ai master. | 2008 («Il percorso SIMD e il percorso scalare sono entrambi primari e dichiarati»), 2009 (nessuna degradazione sostitutiva), 1364 (C-01 campo 6: «Nessun epsilon al posto di un pixel invalido») |
| D24 | ID di decisione e contratto | `APPROVATO` significa verbale e contratto passati dalla review: in R0 per gli stadi `.a`, mentre un'estensione successiva va approvata prima del codice della sua unità. `APPROVATO` abilita i dipendenti. `VERIFICATO` si decide al gate: il test di contratto deve passare nell'esecuzione finale. C-02.a è `APPROVATO` in R0, e i suoi test sono T-M2R2. | 3148 («`decided_by` richiede approvazione prima della costruzione»), 3150 |

## 5. Unità, ordine e gate

Ordine di esecuzione: **R0 → R0.b → R1 → R1.b → R4 → R4.b → R3 → R3.b → R2 → R2.b → R5 → R6**. Una fase `.b` contiene le riesecuzioni degli ID il cui ultimo prerequisito si è chiuso nell'unità appena terminata. Si esegue subito, prima di qualunque dipendente (tabella «Fasi di abilitazione» sotto). L'ordine delle unità segue gli stadi M2.entry → M2.a → M2.c → M2.d → M2.e (archi 4219-4231 e 4414-4420). R4 (M2.c) viene dopo R1 perché F-IO-10 è anch'esso M2.c e perché il job di import di R1 è uno dei job multi-nodo su cui si prova il resume. R3 (M2.d) precede R2 (M2.e) perché i master a blocchi usano il pianificatore delle risorse. R5 viene dopo R2 per l'arco NFR-SIMD-01 → F-CAL-06.a (3999). Ogni unità comincia dal contratto o dall'ADR e solo dopo passa al codice. Le tolleranze si scrivono nel contratto prima della misura. Ogni unità chiude con una review indipendente sul diff.

| Unità | ID | Contratto o ADR prima del codice | Uscita verificabile |
|---|---|---|---|
| R0 Ingresso e controlli statici | NFR-LIC-01.c, NFR-SEC-02, ADR-01, ADR-02, ADR-03.a, ADR-04.a, ADR-05.a, ADR-06.a, ADR-07.a, ADR-08.a, ADR-09.a, ADR-13.a, ADR-16.a, ADR-17.a, C-01.a, C-02.a (contratto), C-10.a, D-03, D-04, D-05, D-09.a, D-17.a | `docs/contracts/C-02-master.md` (undici campi, D5, D6); `docs/contracts/ADR-07-cpu-simd.md` (D19, D20, D23); `docs/decisions/M2-entry.md` | T-M2R0-01, 02 e 04 ESEGUITI; verbale e contratti C-02.a e ADR-07 passati dalla review; in R0.b T-M2R0-03 e riesecuzioni |
| R1 Import e inventario | F-IO-01, F-IO-05, F-IO-10, NFR-REP-01.a (import), NFR-DATA-01.a (cache), NFR-SEC-01 (inventario) | Estensioni di ADR-03 (fase, linearità, inventario, anteprima, segnalazione delle keyword), ADR-04 (cache derivata, quota, cleanup confinato), ADR-05 (fase e linearità nella provenance) | T-M2R1-01…12 ESEGUITI in dev e release; T-M2A-01…13 verdi |
| R4 Recovery e sicurezza del filesystem | F-PRJ-06, NFR-REP-01.a (recovery, export), NFR-SEC-03, T-PRJ-01 (parte M2) | Estensione di ADR-04 (fase sicura, report di recovery persistito, comando di resume, report di export, regola hardlink, modello di perdita di energia) e di ADR-05 (provenance del resume) | T-M2R4-01…06 ESEGUITI; T-M2C-01…25 verdi |
| R3 Risorse | F-RES-03, F-RES-04, F-RES-05.a, F-RES-06.a, F-RES-07.a, F-RES-08.a, NFR-PERF-01, C-10.a (disco pieno); riesecuzione di NFR-FBK-01.a e G-STOP | Estensione di ADR-06: configurazione delle risorse, pianificatore, schema del monitor, ammissione sotto pressione (D11, D12), minimo fattibile, ENOSPC, inventario delle costanti (D14) | T-M2R3-01…09 ESEGUITI; T-M2D-01…15 e T-FBK-01 (parte M2) verdi |
| R2 Master del prodotto | F-CAL-01.a, C-02.a, NFR-REP-01.a (master); riesecuzione di F-CAL-03.a, F-CAL-02.a, F-CAL-06.a, C-01.a, NFR-CPU-01.a, F-PRJ-02 | Applicazione di C-02.a; estensione di ADR-16 (descrittore derivato dal nodo del master) e di ADR-17 (varianza del master, master condiviso) | T-M2R2-01…09 ESEGUITI; T-M2E-01…32 e T-M2C-11 rieseguiti sui master del prodotto |
| R5 SIMD | NFR-SIMD-01; regressione di NFR-CPU-01.a sui due percorsi | Applicazione di `docs/contracts/ADR-07-cpu-simd.md`, scritto in R0 | T-M2R5-01…07 ESEGUITI su entrambi i percorsi, in dev e release |
| R6 Tracciabilità e gate | Tutti i 63 ID; riesecuzione di NFR-OFF-01.a | Sezione «ID del piano → test» di `docs/test-matrix.md`; `tools/check_m2_trace.py` | Gate M2 della sezione 6 |

### Fasi di abilitazione (calcolo topologico)

Ho fatto il calcolo con `python3 -S` sugli archi `requires_all` e `decided_by` interni a M2. Le fasi dicono quando un ID può partire o va rieseguito, nell'ordine indicato e prima di ogni dipendente. Non chiudono nulla: lo stato finale si decide al gate. NFR-REP-01.a ha test propri in R1, R4 e R2 e si riesegue in R2.b, dopo F-CAL-06.a (arco 3974).

| Fase | ID abilitati o da rieseguire, in ordine |
|---|---|
| R0 | Lavoro proprio: NFR-LIC-01.c, NFR-SEC-02. `APPROVATO`, in quest'ordine: ADR-08.a, ADR-03.a, C-10.a, D-05, ADR-04.a, ADR-05.a, D-09.a, ADR-09.a, C-02.a, ADR-16.a, C-01.a, D-17.a, ADR-17.a, ADR-06.a, ADR-07.a, ADR-13.a, D-03, ADR-01, D-04, ADR-02 |
| R0.b | NFR-SEC-01, F-PRJ-01, F-PRJ-03, F-PRJ-07, F-PRJ-02, F-PRJ-04, F-RES-01, F-RES-02, NFR-TEST-01.a. T-M2R0-03 (disco pieno di C-10.a) va prima di R1, perché F-IO-01 `requires_all` C-10.a (arco 3682) |
| R1 | F-IO-01, F-IO-05, F-IO-10 |
| R1.b | NFR-EXP-01.a |
| R4 | F-PRJ-06, NFR-SEC-03 |
| R4.b | NFR-DATA-01.a |
| R3 | F-RES-03, F-RES-04, F-RES-05.a, F-RES-06.a, F-RES-07.a, F-RES-08.a, NFR-PERF-01 |
| R3.b | NFR-FBK-01.a, G-STOP |
| R2 | F-CAL-01.a |
| R2.b | F-CAL-03.a, F-CAL-02.a, F-CAL-06.a, NFR-REP-01.a, NFR-CPU-01.a, NFR-OFF-01.a; regressione di T-M2C-11 (F-PRJ-02) sul workflow con i master |
| R5 | NFR-SIMD-01; regressione di NFR-CPU-01.a sui due percorsi |
| Gate | Stato finale di tutti i 63 ID, consegne M2.a…g e M2 comprese: esecuzione finale (sezione 6) |

### R0. Ingresso M2.entry e controlli statici

- `docs/licenses.md`: per ogni libreria collegata, la versione, lo SHA-256 del file `.so` effettivamente caricato, i moduli usati (per esempio solo EVP SHA-256 di libcrypto; per CFITSIO solo le funzioni da memoria) e le dipendenze transitive da `ldd` sui binari del prodotto e dei test, ciascuna con la sua licenza.
- `docs/contracts/C-02-master.md`: undici campi nel perimetro `.a` (D5, D6, D23). Fixture, metriche e tolleranze sono quelle di R2, scritte qui prima del codice.
- `docs/contracts/ADR-07-cpu-simd.md`: percorsi, dispatch nel preflight, ambiente FP e tolleranze (D19, D20, D23). Si scrive qui, prima di qualunque misura di R5, e chiude ADR-07.a con il verbale.
- `docs/decisions/M2-entry.md`: una riga per ognuno dei 21 ID M2.entry, con decisione, fonte (contratto o ADR), evidenza, test che la esercita e limiti. `docs/decisions/README.md` aggiornato sugli stadi `.a`.
- **T-M2R0-01** (NFR-LIC-01.c): ogni libreria elencata da `ldd` sui binari compare in `docs/licenses.md` con hash e licenza; il confronto lo fa uno script. Esito binario.
- **T-M2R0-02** (NFR-SEC-02): `tools/check_noexec.sh`, eseguito da `ctest`, vieta `system`, `popen`, `exec*`, `posix_spawn*`, `fork`, `vfork`, `dlopen` e `dlmopen` in `libastrocore` e negli eseguibili del prodotto. Controllo negativo: un oggetto di prova che chiama `system()` deve far fallire lo script. Esito binario.
- **T-M2R0-04** (ADR-02, D-04, parte M2): `libastrocore` e gli eseguibili del core non hanno dipendenze né simboli Qt (`readelf -d`, `nm`): il core è headless e separato dalla GUI. Insieme al record M1.b, è la prova di `VERIFICATO`. Esito binario.
- **T-M2R0-03** (C-10.a campo 10, fase R0.b): ENOSPC iniettato a ogni scrittura dell'export (file, manifest, `COMMIT`). Risultato atteso: stop `io` con l'errno, nessun bundle completo secondo `verify_export`, destinazione senza `COMMIT`, originali invariati. Esatta.
- Fase R0.b: dopo T-M2R0-03 si rieseguono, nell'ordine della tabella «Fasi di abilitazione», i test registrati in matrice per NFR-SEC-01, F-PRJ-01, F-PRJ-03, F-PRJ-07, F-PRJ-02, F-PRJ-04, F-RES-01, F-RES-02 e NFR-TEST-01.a.
- Record: `docs/evidence/M2-R0.md`. Review: un revisore Claude diverso dall'autore per i testi; Codex per lo script.

### R1. Import, inventario, prestacked, keyword non interpretate, report di import

- **Codice**:
  - API di inventario solo header, basata su `scan_header` con `pread` e memoria dell'header prenotata nel pool.
  - Anteprima a blocchi (media per blocco in f64) per HDU non compressi.
  - Cache delle thumbnails in `<progetto>/cache/thumbnails/`, con chiave, digest, quota e cleanup confinato (`O_NOFOLLOW`, `st_nlink = 1`). Esiti `cache_hit` e `computed` nella provenance; una voce corrotta porta a stop `cache.corrupt`; la ricostruzione si fa solo col job esplicito `cache.rebuild` (D3).
  - Fase e linearità tipizzate (D1) nel modello dell'immagine e nella provenance, con l'origine (`header` o `declared`).
  - Tabella di compatibilità operazione × fase × linearità, applicata nel preflight.
  - Diagnostica C-14 («antenato lineare verificabile assente») su input NL o `unknown`.
  - Elenco delle keyword non interpretate nel report di import, nella provenance dei nodi che le consumano e nel report di calibrazione.
  - Report di import persistito come artefatto `import_report` (JSON più testo).
- **Test** (verità indipendente; tolleranze fissate qui):
  - T-M2R1-01: inventario con un data unit che restituisce EIO a ogni offset di pixel (punto `input.pread` del `fault_injector`). L'inventario riesce, e i byte letti, contati dall'iniettore e non dal codice d'inventario, sono al massimo header + 2880. Esatta.
  - T-M2R1-02: prenotazione di inventario e anteprima ≥ picco osservato dal pool; un budget sotto il minimo porta a stop col minimo nel contesto. Esatta.
  - T-M2R1-03: anteprima confrontata con un oracle del test che calcola la media per blocco in f64 dai valori del generatore. Tolleranza `γ_k · max|x|` con `k` = numero di addendi del blocco, più ½ ulp f32 sull'arrotondamento finale; 0 sui valori dyadic.
  - T-M2R1-04: voce di cache alterata (un byte modificato dal test). Il ramo che la richiede si ferma con `cache.corrupt`, con digest atteso e trovato nel contesto, che il test calcola da sé. Non c'è output, la voce non viene servita e non parte alcuna ricostruzione: il file alterato resta identico byte per byte e la provenance non registra job `cache.rebuild`. Esatta.
  - T-M2R1-11: job esplicito `cache.rebuild` sulla voce corrotta di T-M2R1-04. La nuova voce è identica byte per byte a quella originale (digest registrato prima dell'alterazione) e la provenance registra il job, la causa e la voce sostituita. La richiesta successiva dà `cache_hit`. Esatta.
  - T-M2R1-12: voce assente (mai creata, o cancellata dal test). Il job `thumbnail` la calcola e registra `computed`; con una voce valida registra `cache_hit` e non ricalcola (contatore di esecuzioni). Quota superata, symlink o hardlink nella cache: stop.
  - T-M2R1-05: matrice {raw, calibrated, prestacked} × {linear, nonlinear, unknown} × {creazione del master, calibrazione}. Le operazioni ammesse sono solo quelle della tabella del contratto, scritta prima; le altre si fermano prima del calcolo senza output. Dichiarazione in conflitto con l'header: stop. `unknown` non è mai lineare.
  - T-M2R1-06: un calibrato o un prestacked importato senza RAW si inventaria e si esporta. Il preflight C-14 su input NL e `unknown` mostra il limite e non inverte lo stretch.
  - T-M2R1-07: l'elenco delle keyword non interpretate nei report e nella provenance coincide con l'insieme che il test calcola dalle card generate.
  - T-M2R1-08: il report di import riletto dopo la riapertura è coerente con l'inventario.
  - T-M2R1-09: PUB-LFC-01 e PUB-CFA-01 (hash registrato prima) importati e inventariati; `BAYERPAT` letto; roundtrip bit-esatto.
  - T-M2R1-10: fuzz breve dell'inventario e dell'anteprima (NFR-SEC-01), come T-M2A-06.
- Record: `docs/evidence/M2-R1.md` e `docs/datasets/PUB-CFA-01.md`.

### R4. Recovery, resume delle fasi sicure, report di recovery e di export, hardlink

- **Codice**:
  - Report di recovery persistito nello store, con job interrotti, temporanei e orfani rimossi, artefatti assenti, fasi sicure e risultati incompleti.
  - Comando di resume esplicito (D15, D16), con riuso dei nodi sicuri dopo la riverifica del digest.
  - Report di export dentro il bundle e nel manifest, con i metadati non rappresentabili dichiarati (C-10, campo 7).
  - Regola hardlink (D18): `(st_dev, st_ino)` degli originali registrati all'import; controllo di `st_nlink` su scrittura, export, recovery e cleanup.
- **Test**:
  - T-M2R4-01: SIGKILL del figlio a ogni confine di nodo e dentro ogni nodo di un job multi-nodo (import di R1 su K file più calibrazione). Dopo la riapertura e il resume esplicito, i nodi sicuri non vengono rieseguiti (contatore di esecuzioni per nodo, preso dalla provenance). I digest finali coincidono con quelli di un'esecuzione senza kill in un progetto separato, che fa da oracle. Esatta.
  - T-M2R4-02: resume con ricetta, parametri o backend diversi: `resume_mismatch` e nessun nodo eseguito (T-FBK-01).
  - T-M2R4-03: il report di recovery riletto dopo una seconda riapertura coincide con gli interventi provocati dal test.
  - T-M2R4-04: perdita di energia simulata (D17). A ogni punto di sync si costruisce lo stato durevole e si riapre. Devono valere: `integrity_check` ok, nessun artefatto `completed` senza file durevole col suo digest, nessun output parziale. Invariante esatto.
  - T-M2R4-05: hardlink di un originale in `artifacts/`, nella cache e come destinazione di export, più un file del progetto con `nlink` 2. Risultato atteso: stop `security`, nessuna scrittura né rimozione; dell'originale restano invariati digest, inode, `nlink`, `mtime` e permessi.
  - T-M2R4-06: report di export presente, elencato nel manifest e controllato da `verify_export`.
- Record: `docs/evidence/M2-R4.md`.

### R3. Risorse

- **Codice**:
  - Configurazione delle risorse persistita e validata, come per F-RES-02. Contiene: limite del pool, obiettivo RSS, scratch e sua quota, cache su disco e sua quota, insieme di CPU, worker e tile (`auto` o `n`), capacità della coda, frazioni e soglia PSI con la loro origine, VRAM `null`.
  - Pianificatore deterministico del preflight (D7, D8).
  - Job di calibrazione a blocchi di righe, con lettura a offset dei data unit non compressi e digest sui byte consumati.
  - Checkpoint ogni K blocchi dichiarato nella ricetta; spill solo se è dichiarato.
  - Monitor con pool gestito, picco stimato, RSS (`VmRSS` e `VmHWM` del proprio processo), cache residente, scratch, cache su disco, VRAM e limite scelto con la sua origine. Segnala sia il budget sotto il working set intero (si passa ai blocchi) sia il budget sotto il minimo (stop).
  - Ammissione sotto pressione (D11, D12).
  - `minimum_feasible_bytes` nel contesto dell'errore.
  - ENOSPC (D13); rilascio della quota dopo un `pwrite` fallito, che chiude il limite noto alla riga 123 di ADR-06.
  - Insieme di CPU applicato ai soli thread worker del prodotto, validato contro `sched_getaffinity`.
- **Test**:
  - T-M2R3-01: dataset oltre il budget (riga 2367), con working set ≥ 8 volte un budget manuale. Il risultato è bit-identico a quello con budget ampio, perché T-M2E-16 prova l'indipendenza da tile e worker, e resta entro le tolleranze di T-M2E-02 rispetto alla GT. La somma delle prenotazioni non supera mai il budget (contatori del pool). L'RSS si misura e si riporta senza farne un gate (riga 1919).
  - T-M2R3-02: pianificatore su casi tabellari (budget, CPU, dimensioni) con piano atteso calcolato a mano nel contratto. I valori espliciti dell'utente restano intatti, oppure stop.
  - T-M2R3-03: monitor. Pool e picco coincidono coi contatori indipendenti del test; l'RSS riportato è ≤ `VmHWM` letto dal test subito dopo; la cache residente è la somma delle voci; lo scratch è l'estensione dello spill; VRAM vale `null` col motivo. Le due segnalazioni compaiono.
  - T-M2R3-04: per un job impossibile `minimum_feasible_bytes` è uguale alla formula del contratto; col budget uguale al minimo il job passa, col minimo − 1 si ferma. Esatta.
  - T-M2R3-05: ENOSPC iniettato su spill, checkpoint, commit degli artefatti, export (come regressione di T-M2R0-03) e cache. Risultato atteso: stop `io` con l'errno, nessuna pubblicazione, temporanei rimossi, quota rilasciata; per l'export nessun bundle completo (C-10, campo 10).
  - T-M2R3-06: checkpoint nel job, kill e resume identico danno un output identico all'esecuzione continua; un resume diverso dà `resume_mismatch`.
  - T-M2R3-07: con una sorgente PSI sintetica iniettata che sale durante l'esecuzione, il job in coda non viene ammesso sopra soglia e viene ammesso quando la pressione cala. Il job in corso mantiene la ricetta invariata (digest). Una prenotazione impossibile porta a stop con checkpoint, e gli eventi compaiono nel report.
  - T-M2R3-08 (NFR-PERF-01): ogni parametro inventariato cambia il comportamento come prevede la formula; default e origine finiscono nella provenance; con capacità della coda N si ferma il job N+1.
  - T-M2R3-09 (F-RES-08.a): scratch, quota, cache e insieme di CPU si applicano; i worker restano vincolati all'insieme; una CPU fuori dall'affinità porta a stop; il cleanup è confinato; la configurazione è persistita e rivalidata alla riapertura.
- Record: `docs/evidence/M2-R3.md`. Review: Codex `gpt-6-astra` (concorrenza e risorse).

### R2. Master creati dal prodotto

- **Codice**:
  - Nodi DAG per i master bias, dark, flat e dark-flat, che consumano frame grezzi raggruppati.
  - Accumulatori a blocchi di righe su N frame, senza la pila intera in RAM (C-02, campo 11), pianificati da R3.
  - Report del master: frame usati e scartati, conteggi di rejection per frame, rumore, artefatti (saturazione), stato; provenance di ogni frame e di ogni scarto.
  - Descrittore del master derivato dal suo nodo; il descrittore dichiarato resta per i master esterni (ADR-16).
  - Pipeline completa frame → master → `Q` → light calibrato.
  - Rimuovere il ramo irraggiungibile `!std::isfinite(vq)` → `q_unreliable` in `src/core/calibration.cpp:918`: è codice morto, perché `div_ok` alla riga 913 lo precede e marca già `result_nonfinite`. Aggiungere un test che documenta `result_nonfinite` per `Var(Q)` in overflow (ADR-16 §7).
- **Test**:
  - T-M2R2-01: valori dyadic senza rumore, `mean` esatta, master e piano di varianza. Tolleranza 0.
  - T-M2R2-02: valori non dyadic. `mean` entro `γ_N Σ|x|/N` (f64) più ½ ulp f32; piano di varianza entro il bound di arrotondamento di C-02.a §6.3.
  - T-M2R2-03: outlier iniettati in posizioni note, senza rejection: master e piano di varianza uguali all'oracle razionale su tutti i campioni, outlier compresi, e il report conta zero scarti.
  - T-M2R2-04: Monte Carlo con seed su `mean`: frame con stessa media e stessa varianza, con varianze diverse, e flat con livelli diversi entro la tolleranza. Il riferimento è la varianza vera della simulazione (per il flat, il valore atteso esatto del piano con il termine dei livelli); le soglie sono quelle di C-02.a §9, fissate prima della misura al livello `α/m` (Bonferroni): intervalli chi-quadro (distribuzione esatta e quantili esatti dalla gamma incompleta regolarizzata, invertita per bisezione con la sola libreria standard, errore relativo ≤ 10⁻¹⁰ sul quantile) per il piano con stessa media e stessa varianza e per la varianza empirica dei valori del master; limiti di Chebyshev, con la varianza esatta della forma quadratica gaussiana, per il piano con varianze diverse, per il flat con livelli diversi e per `level_bias_estimate`; più la fixture discriminante fuori dalle ipotesi (`nonnegative_signal` violata).
  - T-M2R2-05: ogni campo di compatibilità del gruppo fuori regola, frame insufficienti, flat saturo o con segnale insufficiente, metodo fuori dal dominio della varianza (`median`, `sigma_clip`, `median_ratio`), flat con livello oltre la tolleranza: stop, nessun master.
  - T-M2R2-06: dopo la riapertura e la ricostruzione, stessi `node_key` e digest (riproducibile dal progetto).
  - T-M2R2-07: uno stack oltre il budget elaborato a blocchi è bit-identico al caso in memoria.
  - T-M2R2-08: report e provenance del master completi e coerenti con l'output.
  - T-M2R2-09: dark e flat grezzi di PUB-LFC-01 combinati dal prodotto. T-M2E-13 si riesegue su questi master; il confronto con la media costruita dal test, con `sample_type = f64` nella fixture, sta entro `γ_N Σ|x|/N` (il percorso f32 è in T-M2R2-02).
  - Riesecuzioni: T-M2E-01…32 sui master del prodotto dove pertinente, T-M2C-11 sull'intero workflow, T-M2F-02.
- Record: `docs/evidence/M2-R2.md`.

### R5. SIMD con dispatch nel preflight

- **Codice**: kernel AVX2 per la calibrazione elemento per elemento (`Lc`, `Fc`, `Q`, `C`, varianza) e per gli accumulatori dei master, in unità di traduzione con `target("avx2")` e `-ffp-contract=off`; kernel scalare compilato senza vettorizzazione; risoluzione del percorso nel preflight (D19); controllo di MXCSR nel preflight e all'avvio di ogni task (D23).
- **Test**:
  - T-M2R5-01: ciascun percorso contro l'oracle (T-M2E-01, 02 e 20; T-M2R2-01 e 02, una volta per percorso).
  - T-M2R5-02: scarto fra i percorsi di 0 ulp sui kernel elemento per elemento, sulle accumulazioni per pixel e sulla mediana. Sulle riduzioni su pixel lo scarto deve stare entro `(γ_(n−1) + γ_(⌈n/4⌉+1)) Σ|x_i|`, più `u_p(|a| + |b|)` per ogni operazione successiva (D20). Le tolleranze sono nel contratto ADR-07 prima della misura. Casi di prova: `n` multiplo e non multiplo di 4, `n < 4`, forte cancellazione e subnormali (vedi T-M2R5-06). Il bound include il termine assoluto `η_p` per ogni conversione e `h·(1+γ_h)·η_64/2` per ogni somma (D20).
  - T-M2R5-03: con capacità della CPU iniettate senza AVX2 e richiesta `avx2`, stop `unsupported` e zero chiamate al percorso scalare (T-FBK-01).
  - T-M2R5-04: il percorso compare in backend, chiave e provenance; un resume con percorso diverso dà `resume_mismatch`.
  - T-M2R5-05: `objdump` non trova istruzioni `vfmadd*`, `vfmsub*`, `vfnmadd*`, `vrcp*` o `vrsqrt*` nei kernel dei due percorsi.
  - T-M2R5-06 (subnormali, D20 e D23): ingressi f32 subnormali (`2^−149`, `2^−126 − 2^−149`) presi come valori validi; prodotti e quozienti con operandi non nulli e risultato subnormale o zero in f32, che entrambi i percorsi marcano `result_nonfinite` con bit della maschera identici; conversioni a f32 attorno a `2^−150` (la coppia `a`, `b` di D20) con scarto entro il bound con `η_32`; riduzioni con termini vicini alla soglia di underflow f64. Esatta su valori e maschere dei kernel a 0 ulp, bound di D20 sulle riduzioni.
  - T-M2R5-07 (D23): con FZ, DAZ o RC diverso da «al più vicino» impostati via `_mm_setcsr` prima del preflight, o in un worker tramite il `fault_injector`, si ha stop `numeric`, `cpu.fp_mode`, con zero chiamate ai kernel e nessun output. Il registro non viene corretto in silenzio. I simboli e le opzioni di link mostrano l'assenza di `crtfastmath.o`. Binaria.
  - Entrambi i percorsi girano sotto ASan e UBSan.
- Record: `docs/evidence/M2-R5.md`. Throughput registrato solo come informazione, senza soglia (riga 2022).

### R6. Tracciabilità e gate rifatto

- La sezione «ID del piano → test» di `docs/test-matrix.md`, scritta insieme a questo piano, si aggiorna a ogni chiusura di unità: test eseguiti, record, stato.
- `tools/check_m2_trace.py` (`python3 -S`, errori con `sys.exit("diagnosi")`). Controlla solo lo stato finale e non legge `git log`: estrae dal registro i 63 ID; verifica che per ognuno la matrice nomini almeno un test esistente (in `ctest -N`, oppure, per le prove M1 di ADR-02 e D-04, il record citato); verifica che ogni test nominato risulti passato nel log dell'esecuzione finale; verifica che il grafo dei prerequisiti interni a M2 sia tutto `CHIUSO` o `VERIFICATO`.
- Durante il lavoro, i test degli ID `DA RIESEGUIRE` si rieseguono nella fase indicata nella tabella «Fasi di abilitazione», come abilitazione operativa. Al gate si riesegue comunque tutto.
- Esecuzione finale del gate (sezione 6, clausola 1).
- Si aggiornano `docs/evidence/M2-gate.md` (nuovo giro), `docs/progress.md`, `plans/M2.md` (prossimo comando) e `PLANS.md` (piani attivi).

## 6. Gate M2 (rifatto)

Il gate passa solo se valgono tutte le condizioni seguenti:

1. **Un'unica esecuzione finale completa** sul commit del gate: build pulita in directory nuove, dev e release; `ctest --preset dev` e `ctest --preset release` completi, slow compresi; `python3 -S tools/check_m2_trace.py` con exit 0. Comandi, commit, versioni, ambiente, log ed esiti vanno registrati in `docs/evidence/M2-gate.md`.
2. **Ogni ID a stadio `M2.*` e `M2.entry`** (63, appendice A) ha nella matrice almeno un test esistente che passa nell'esecuzione finale, e la sua riga rimanda a `M2-gate.md`. Gli ID funzionali e le consegne sono `CHIUSO`; gli ID di decisione sono `APPROVATO` e `VERIFICATO`. Tutti i prerequisiti interni a M2 sono nello stesso stato. Poiché tutti i test si rieseguono dopo l'ultima modifica, l'ordine rispetto ai prerequisiti è garantito per costruzione.
3. La clausola del §21.4 (riga 2675) resta soddisfatta: slice FITS con `L − D`, `Q`, maschere, metadati, incertezza e report; DAG durevole, recovery, cancel e worker; dato astronomico reale.
4. La review indipendente (Codex `gpt-6-astra`, sola lettura) dà PASS sul diff delle unità e sul record del gate.

## 7. Contratto scientifico

Le unità che producono scienza sono R1 (anteprima), R2 (master) e R5 (equivalenza dei percorsi). Modello, unità, ipotesi, casi degeneri e comportamento in errore stanno nei contratti C-02.a, ADR-17 (estensione) e ADR-07 (nuovo), che si scrivono prima del codice. Casi degeneri che i contratti devono coprire:

- `n` sotto il minimo dichiarato per il metodo;
- tutti i campioni di un pixel rigettati, per cui il pixel risulta invalido nella maschera e non riceve mai un valore sostitutivo;
- flat saturo;
- gruppo incompatibile;
- frame non finiti, per cui vale la maschera `source_nonfinite` di ADR-03.

In errore c'è sempre uno stop controllato con codice non nullo, diagnosi e provenance dell'arresto, senza ricette alternative (NFR-FBK-01, riga 2009).

## 8. Evidenza e review

- Un record per unità: `docs/evidence/M2-R0.md` … `M2-R5.md`; R6 aggiorna `M2-gate.md`. Ogni record contiene comandi, hash, versioni, ambiente, esito e limiti, e separa i test eseguiti da quelli pianificati.
- Review indipendente sul diff a fine unità: `codex exec -m gpt-6-astra -c model_reasoning_effort="high" -s read-only … < /dev/null`, con timeout e traccia `-o` in `.omc/artifacts/ask/`. Un revisore Claude diverso dall'autore per contratti e verbali. Al massimo due deleghe contemporanee.
- Modelli: Opus per contratti e decisioni (R0, e i contratti di R2, R3, R5); Sonnet per codice e test; Haiku per ricognizioni.

## 9. Rollback e rischi

- Ogni unità entra in commit propri su `work/m0-m1`. Il rollback è un `git revert` dei commit dell'unità, senza riscrivere la storia.
- **Rischi**:
  - Lo streaming o il SIMD possono rompere il determinismo: T-M2E-16, T-M2R3-01 e T-M2R5-02 lo intercettano.
  - La simulazione della perdita di energia ha una fedeltà limitata al modello POSIX (D17).
  - La licenza del campione CFA può cambiare: si verifica di nuovo all'acquisizione e l'hash si registra prima.
  - La sorgente PSI iniettata non è il segnale reale: lo affiancano le misure S4 già registrate in `M2-gate.md`.
  - Il dataset oltre il budget deve restare entro il 70% della macchina.
- **Invalidazione**: qualunque modifica ai kernel di calibrazione o di combinazione invalida i record T-M2E e T-M2R2, che si rieseguono. Una modifica al pianificatore invalida T-M2R3-01 e 02.

## 10. Chiusura

- Stato del gate: **aperto**. Questo piano è PLANNED.
- **Alternative scartate**:
  - Spostare F-IO-05, F-IO-10 o F-CAL-01.a a M3: il registro le assegna a M2, e «Una funzione P0 mancante blocca la milestone» (`PLANS.md`).
  - Chiudere NFR-SIMD-01 col solo percorso scalare: la riga 2008 chiede due percorsi primari con tolleranze proprie.
  - Adattare il tile durante l'esecuzione: lo vietano la riga 2009 e `ADR-M1-stack.md` riga 16.
  - Provare ENOSPC su un filesystem reale montato apposta: è una modifica di sistema, vietata.
  - Resume automatico all'apertura: la riga 2283 dice «propone».
  - Dichiarare la perdita di energia fuori modello: il testo P0 della riga 1832 la nomina.
- **Prima di R0**: review indipendente di questo piano da parte di un agente Claude diverso dall'autore, sul testo effettivo.
- **Prossimo comando verificabile** (R0 PASS, `docs/evidence/M2-R0.md`; inizio di R0.b, T-M2R0-03 disco pieno sull'export): `ctest --preset release -R 'T-M2R0-03'`, rosso finché il test non è scritto; poi le riesecuzioni di R0.b.

## Appendice A. Testo di accettazione esatto per ogni ID a stadio M2.* e M2.entry

Generata con `python3 -S` dal piano attivo (SHA-256 `95abbb1a…4094`): 63 ID. Per i requisiti è l'ultima colonna della riga; per ADR, decisioni e consegne la riga intera; per i contratti i campi pertinenti alla slice `.a`; per i paragrafi la frase intera. Nessun testo è parafrasato.

### F-IO-01 — registro riga 3156, stadio `M2.a`, «FITS»

- Riga 1812: Importa FITS mono, CFA, RGB e multi-plane; rileva profondità bit, dimensioni, unità e header; i metadati non interpretabili sono conservati e segnalati; fixture sintetiche e campioni pubblici licenziati passano.

### F-IO-05 — registro riga 3160, stadio `M2.a`, «Inventario bounded»

- Riga 1816: Previews e analisi inventario non caricano obbligatoriamente tutti i pixel; la memoria resta dentro il budget; le thumbnails sono cache derivata e ricostruibile.

### F-IO-10 — registro riga 3165, stadio `M2.c`, «Prestacked tipizzato»

- Riga 1821: Importa immagini calibrate o prestacked con la fase di processo dichiarata, senza pretendere i RAW; la provenance distingue prestacked, calibrato e raw e blocca le operazioni non compatibili. Il preflight distingue linearità nota, NL e sconosciuta: C-14 richiede un antenato radiometricamente lineare verificabile. Su input solo NL mostra la limitazione, non inverte uno stretch inventato e non sostituisce l'operatore.
- Riga 1622: Il preflight di F-IO-10 e dell'Advisor deve spiegare che un import soltanto non lineare, senza antenato radiometricamente lineare verificabile, non ammette C-14. Non si ricostruisce un antenato con un'inversione inventata dello stretch e non si usa un algoritmo sostitutivo P0. Il test M3.n copre import NL isolato, NL con antenato valido e linearità sconosciuta; verifica il rifiuto prima del fit e la diagnostica visibile in M4. Si valuta separatamente un eventuale operatore fotografico nativo NL come proposta P1, con contratto, fonti e validazione propri: non è promesso né sostituisce C-14.

### F-PRJ-01 — registro riga 3166, stadio `M2.c`, «Persistenza»

- Riga 1827: Il progetto si salva e si riapre con riferimenti ai file, parametri, versioni algoritmo, risultati e storia; un test di roundtrip ricostruisce lo stesso stato logico.

### F-PRJ-02 — registro riga 3167, stadio `M2.c`, «Originali»

- Riga 1828: Gli originali non vengono mai sovrascritti implicitamente; l'export è esplicito; un test verifica che l'hash della sorgente resti invariato dopo l'intero workflow.

### F-PRJ-03 — registro riga 3168, stadio `M2.b`, «DAG»

- Riga 1829: Pipeline a DAG con nodi, input, output, dipendenze e cache; un cambio di parametro, metadato o maschera invalida solo il nodo e i discendenti, con test di invalidazione selettiva. Il modello DAG precede la persistenza M2.c; l'integrazione durevole è verificata lì, senza prerequisito retrogrado.

### F-PRJ-04 — registro riga 3169, stadio `M2.c`, «Revisioni»

- Riga 1830: Modifica di un parametro a monte, undo, redo e snapshot ripristinano esattamente lo stato logico. I risultati dipendenti si recuperano dalla cache o si ricostruiscono entro il contratto numerico e con le versioni dichiarate; l'eviction non perde la storia. Nessun nodo indipendente viene ricalcolato senza motivo; l'undo ripristina una revisione, non solo una cache.

### F-PRJ-06 — registro riga 3175, stadio `M2.c`, «Recovery»

- Riga 1832: Dopo crash, kill o perdita di energia il progetto si riapre; i risultati temporanei incompleti sono eliminati o marcati; le fasi sicure riprendono; un output parziale non è mai marcato completato.
- Riga 2283: **Recovery.** Dopo un crash, il prodotto propone il ripristino delle fasi sicure, elenca i risultati incompleti e non pubblica un output parziale.

### F-PRJ-07 — registro riga 3176, stadio `M2.c`, «Provenance base estensibile»

- Riga 1833: Log semantici, provenance e manifest esportabile contengono versioni algoritmo, dataset, impostazioni, hash, correzioni applicate o omesse e condizioni di riproducibilità; un altro agente può ricostruire il risultato.

### F-CAL-01.a — registro riga 3179, stadio `M2.e`, «Master sintetici minimi»

- Riga 1840: Crea master bias, dark, flat e dark-flat con combinazione, statistiche e rejection configurabili; gestisce set parziali e gruppi incompatibili; il report mostra frame usati, scartati, rumore, artefatti e stato del master; i master sono riproducibili da progetto. La `.a` è la slice sintetica minima, la `.b` completa il requisito.

### F-CAL-02.a — registro riga 3182, stadio `M2.e`, «Light matched minimal»

- Riga 1841: Corregge il light secondo tutte le otto combinazioni, dark-flat e master tipizzati; evita doppia sottrazione. Omissioni e correzioni parziali sono scelte prima dell'esecuzione; un prerequisito perso dopo la scelta arresta il ramo senza sostituzioni. T-CAL-08 copre otto combinazioni e stati dark-flat. La `.a` limita il contratto al caso matched sintetico, non certifica anticipatamente la `.b`.

### F-CAL-03.a — registro riga 3185, stadio `M2.e`, «Q sintetico»

- Riga 1842: Prepara e normalizza flat mono, CFA e per filtro; applicazione solo nel dominio additivo ammesso del light. Pixel quasi nulli o invalidi sono mascherati, mai divisi per epsilon. Omissione o percorso parziale si scelgono prima dell'esecuzione; dopo errore STOP. Fattori di normalizzazione conservati. La preparazione non dipende dalla calibrazione del light. `.a` copre Q sintetico, `.b` i casi completi.

### F-CAL-06.a — registro riga 3190, stadio `M2.e`, «Varianza minima»

- Riga 1845: Propaga rumore, varianza, pesi, maschere e coverage dalla calibrazione allo stack; documenta covarianza light/flat e master condivisi. Test sintetici verificano le tolleranze. `.a` applica il modello minimo approvato M2.entry; `.b` completa i modelli dichiarati M3, senza sostituzione di un modello fallito.

### F-RES-01 — registro riga 3279, stadio `M2.d`, «Auto»

- Riga 1917: Modalità Automatica: stima il budget dalla memoria disponibile e dalla pressione di OS e altri processi, usando PSI o cgroup quando presenti; evita di consumare tutta la RAM; testata con più budget e carichi concorrenti.

### F-RES-02 — registro riga 3280, stadio `M2.d`, «Manuale»

- Riga 1918: Modalità Manuale: limite in MiB o GiB oppure percentuale; il denominatore è esplicito e la capacità è uno snapshot; conversioni e comportamento sono ambigui zero; il valore è persistente e validato.

### F-RES-03 — registro riga 3281, stadio `M2.d`, «Scheduler»

- Riga 1919: Scheduler e pipeline adattano frame, tile, cache, thread e buffer al budget; usano blocchi, streaming e spill quando serve. Un dataset oltre RAM viene elaborato o rifiutato con preflight; i test verificano le prenotazioni del pool e i fallimenti controllati. Non si garantisce assenza universale di OOM o kill da OS per allocazioni esterne o pressione concorrente.

### F-RES-04 — registro riga 3282, stadio `M2.d`, «Monitor»

- Riga 1920: Monitora memoria gestita, picco stimato, cache residente e limite scelto; segnala quando il budget è troppo restrittivo; distingue pool gestito, RSS, scratch e VRAM.

### F-RES-05.a — registro riga 3284, stadio `M2.d`, «Pool separati»

- Riga 1921: La `.a` separa limite del pool RAM gestito, obiettivo RSS best effort, cache su disco, scratch e VRAM quando disponibile. La `.b` aggiunge coordinamento avanzato e politiche dipendenti dal carico.

### F-RES-06.a — registro riga 3287, stadio `M2.d`, «Checkpoint»

- Riga 1922: La `.a` gestisce pressione e checkpoint, ferma nuove ammissioni, fa spill o cancel sicuro. La `.b` aggiunge cambio interattivo a caldo senza riavvio. Il confine tra `.a` e `.b` richiede una decisione esplicita.

### F-RES-07.a — registro riga 3290, stadio `M2.d`, «Minimo fattibile»

- Riga 1923: La `.a` evita OOM e swap eccessivo con preflight, backpressure e rifiuto motivato; non promette un limite hard per driver o allocazioni esterne; un job impossibile viene rifiutato con il minimo fattibile dichiarato. La `.b` aggiunge politiche avanzate.

### F-RES-08.a — registro riga 3293, stadio `M2.d`, «Quota e cleanup»

- Riga 1924: `.a` configura worker, CPU, scratch, quota, cache e cleanup confinato. `.b` aggiunge tuning CPU avanzato. Il tuning VRAM della GPU produttiva appartiene a NFR-GPU-01 in M7, non anticipa né ritarda la foglia `.b`.

### NFR-EXP-01.a — registro riga 3320, stadio `M2.a`, «FITS minimo»

- Riga 2001: FITS minimo M2; FITS/TIFF completi M3 con provenance, unità, versioni, parametri, hash, correzioni e AI dichiarata. Riapertura conserva metadati; manifest o sidecar per ciò che il formato non rappresenta. M4 verifica UI, M5 gli artefatti distribuiti.

### NFR-REP-01.a — registro riga 3325, stadio `M2.e`, «Report slice»

- Riga 2002: Ogni fase critica produce un report quando introdotta: import, master, calibrazione, debayer, registrazione, resampling, stacking, background, stretch, export, recovery e Advisor. M3 include report, confronti e planner anche non lineari; M4 aggiunge soltanto la presentazione interattiva. Gli stadi sono definiti in appendice D.

### NFR-CPU-01.a — registro riga 3338, stadio `M2.e`, «CPU slice»

- Riga 2006: Tutte le funzioni fondamentali, incluso viewer software e STF, funzionano CPU-only. CPU e renderer software sono percorsi primari scelti nel preflight; non sostituiscono GPU fallite. Le verifiche coprono progressivamente slice, headless completo, GUI e release.

### NFR-SIMD-01 — registro riga 3343, stadio `M2.e`, «Dispatch preflight»

- Riga 2008: SIMD usa dispatch nel preflight. AVX2 non è un requisito universale. Il percorso SIMD e il percorso scalare sono entrambi primari e dichiarati, con tolleranze numeriche proprie. Un percorso non sostituisce l'altro a runtime.

### NFR-OFF-01.a — registro riga 3345, stadio `M2.e`, «Offline slice»

- Riga 2010: Core e Advisor offline, senza account o cloud. M2 verifica solo la slice disponibile; M3 l'Advisor completo, M4 la GUI, M5 il primo avvio. Cataloghi/download opzionali sono opt-in e disattivabili. Colore relativo e solver richiedono il pack locale selezionato; assenza o invalidità producono STOP, non rete implicita o cambio ricetta.

### NFR-SEC-01 — registro riga 3352, stadio `M2.a`, «Limiti parser; estesi a ogni decoder»

- Riga 2014: Parser e decoder hanno limiti di dimensione, profondità, numero di frame e decompressione. Input malformati falliscono in modo controllato, senza overflow o consumo incontrollato.

### NFR-SEC-02 — registro riga 3353, stadio `M2.b`, «Nessun eval; esteso al ruleset»

- Riga 2015: I progetti non eseguono eval, script arbitrari o comandi di sistema. I plugin, quando esistono, sono sandboxati e con API dichiarata. L'Advisor non esegue codice generato.

### NFR-SEC-03 — registro riga 3354, stadio `M2.c`, «Filesystem sicuro»

- Riga 2016: Export e cleanup proteggono gli originali da symlink, hardlink, path traversal e sovrascrittura accidentale. Il cleanup è confinato alle directory dichiarate.

### NFR-LIC-01.c — registro riga 3359, stadio `M2.entry`, «Prima delle adozioni core»

- Riga 2018: Inventario con versione, hash, moduli e dipendenze transitive prima di adottare ogni libreria; SBOM progressivo fino agli artefatti effettivamente distribuiti. M0 approva il piano, non inventa una distinta completa. Un audit non è dichiarato finito finché non è stato fatto. Un catalog pack è un artefatto distribuito: la sua licenza, la sua provenienza e la sua versione entrano nell'SBOM.

### NFR-TEST-01.a — registro riga 3363, stadio `M2.f`, «Infrastruttura»

- Riga 2019: `.a` consegna infrastruttura locale/CI e fixture prima dei kernel; test unitari, property/metamorphic e ground truth crescono con le funzioni. M2 verifica gli invarianti e la varianza della slice senza certificare operatori assenti. `.b` chiude scienza/integrazione headless, `.c` UI, `.d` release. T-FBK-01 cresce a ogni stadio senza essere prerequisito della propria implementazione.

### NFR-PERF-01 — registro riga 3369, stadio `M2.d`, «Budget senza costanti universali»

- Riga 2022: Nessun valore universale di RAM, tempo o margine viene imposto. I limiti sono configurabili o misurati. Un margine del 70 per cento o un timeout arbitrario non sono criteri di progetto.

### NFR-DATA-01.a — registro riga 3371, stadio `M2.c`, «Hash e recovery»

- Riga 2023: Gli originali sono identificati con hash e non vengono modificati. Export e cache hanno integrità verificabile. Un output parziale non è pubblicato come completo. Un bundle multi-file è completo solo con manifest valido, hash degli output verificati e marker di commit presente e coerente con quel manifest.

### NFR-FBK-01.a — registro riga 3376, stadio `M2.d`, «Stop e resume core»

- Riga 2009: Nessun fallback e nessuna degradazione sostitutiva in nessuno stadio. Un errore, una risorsa indisponibile o un gate fallito produce stop controllato, codice di uscita non nullo, messaggio diagnostico e provenance dell'arresto. Il resume riprende solo con ricetta, metodo, backend e parametri identici a quelli del checkpoint valido; qualunque differenza apre una nuova ricetta e non è una prosecuzione. L'Advisor può confrontare candidati già pianificati se il protocollo preregistrato lo consente; dopo un errore non produce un no-op sostitutivo, non promuove un nuovo candidato al posto di quello fallito e non dichiara completa una validazione eseguita su un insieme incompleto. Il budget non autorizza il taglio automatico di risoluzione, candidati o qualità; streaming, spill e checkpoint sono ammessi solo se previsti dalla ricetta. Un'autorizzazione dell'utente non abilita un fallback, nemmeno dichiarato.

### ADR-01 — registro riga 3385, stadio `M2.entry`, «Linguaggio dopo spike»

- Riga 2562: **ADR-01** | Linguaggio e componenti | C++20 o 23, Rust, Python con core nativo, C#/.NET, ibrido | Core C++20 con Qt Widgets come raccomandazione provvisoria; Rust con GTK4 secondo; Rust con Qt e ibrido Python da misurare. Core headless. Proposto, non deciso. | Analisi di architetto e ricerca, nessuno spike. Disaccordo conservato. | M1 | Coordinatore dopo spike misurati e review

### ADR-02 — registro riga 3386, stadio `M2.entry`, «GUI dopo spike»

- Riga 2563: **ADR-02** | GUI e comunicazione con il core | Qt Widgets o QML, GTK4 e libadwaita, egui, Slint, Tauri, Avalonia | Scegliere dopo confronto su viewer, licenza, accessibilità, colore, IME, docking e packaging. API tipizzata o command bus. Nessuna logica scientifica nella GUI. Proposto. | Criteri noti, prove assenti. | M1 | Coordinatore dopo prove e review UX

### ADR-03.a — registro riga 3388, stadio `M2.entry`, «Tipi minimi»

- Riga 2564: **ADR-03** | Modello pixel, precisione e maschere | Array contiguo, tile, piramide, f32, f64, variance e weights | Immagine tipizzata con unità, CFA, geometria, linearità, maschere, coverage, varianza e capability. f32 candidato, f64 per condizionamento. Maschera di editing distinta da invalidità e alpha. Proposto. | Coerenza con i requisiti, nessuna misura di memoria. | M2 | Coordinatore e architettura dopo evidenze

### ADR-04.a — registro riga 3391, stadio `M2.entry`, «Autorità e recovery»

- Riga 2565: **ADR-04** | Persistenza del progetto | SQLite, JSON, ibrido, file binari | SQLite come sola autorità proposta, cache esterna derivata, protocollo atomico, migrazioni con backup, schema sconosciuto in sola lettura. Non approvato. | Transazioni e query a favore, migrazione e formato a sfavore. | M2 | Coordinatore dopo prove di recovery e migrazione

### ADR-05.a — registro riga 3394, stadio `M2.entry`, «DAG minimo»

- Riga 2566: **ADR-05** | DAG, cache e invalidazione | DAG immutabile, grafo mutabile, cache per hash | DAG immutabile con hash di contenuto, metadati, parametri canonici, versione algoritmo, decoder, precisione e backend. Token di generazione contro risultati stale. Il grafo è aciclico ma la completezza si verifica per configurazione (sezione 11.11). La provenienza distingue controllo di regressione e validazione indipendente. Proposto. | Invarianti chiari, costo hash da misurare. | M2 | Coordinatore e architettura dopo evidenze

### ADR-06.a — registro riga 3397, stadio `M2.entry`, «Scheduler»

- Riga 2567: **ADR-06** | RAM, scheduler e spill | Limite hard RSS, pool gestito, ibrido | Pool gestito con prenotazioni, code bounded, backpressure, eviction, spill quota, checkpoint e prevenzione deadlock. RSS best effort e misurato. La prenotazione copre l'intero batch di candidati, non il singolo vincitore. Se il budget non basta, il job si ferma con errore non nullo: non riduce risoluzione, candidati o qualità e non passa a un percorso alternativo (NC-21, NFR-FBK-01). Proposto. | Distinzione corretta, nessuna misura su dataset grandi. | M2 | Coordinatore e architettura dopo misure

### ADR-07.a — registro riga 3400, stadio `M2.entry`, «CPU dopo ricerca M1»

- Riga 2568: **ADR-07** | CPU, SIMD e GPU | CPU-only, Vulkan, OpenCL, CUDA, HIP o ROCm | CPU di riferimento con dispatch SIMD nel preflight; il percorso CPU e quello scalare sono primari. GPU facoltativa, backend da confrontare, nessuna sostituzione automatica della GPU con la CPU. Ricerca GPU in M1, produzione in M7. Proposto. | Nessun benchmark eseguito. | M1 e M7 | Coordinatore dopo misure

### ADR-08.a — registro riga 3403, stadio `M2.entry`, «FITS minimo»

- Riga 2569: **ADR-08** | Import RAW, FITS e XISF | CFITSIO, LibRaw, XISF, implementazioni proprie | Usare librerie mature per I/O, con licenze verificate, e mantenere il core indipendente da Siril. Contratto del decoder esplicito. Proposto. | Librerie candidate, licenze da verificare. | M2 e M3 | Coordinatore dopo audit e prove

### ADR-09.a — registro riga 3406, stadio `M2.entry`, «Calibrazione minima»

- Riga 2570: **ADR-09** | Calibrazione, registrazione e stacking | Formule e ordini alternativi | Applicare i casi delle sezioni 8 e 9, con ground truth, maschere, varianza e report. Nessuna formula universale. Per la deconvoluzione, una sola formulazione P0: `b` fisso stimato a monte, `W` PSD che approssima `C_V^{-1}` sul sottospazio valido, sensibilità a PSF, fondo, `lambda` e rumore, baseline no-op. Proposto. | Matematica controllata documentalmente, validazione assente. | M3 | Coordinatore dopo fonti primarie, prove e review numerica

### ADR-13.a — registro riga 3414, stadio `M2.entry`, «Offline»

- Riga 2574: **ADR-13** | Offline, cataloghi e AI | Offline puro, online opzionale, modelli locali | Core e Advisor offline. Funzioni online opt-in. AI facoltativa con modelli, licenze e provenance dichiarate. I catalog pack sono locali, versionati e richiedono licenza verificata. Proposto. | Requisito chiaro, modelli non scelti. | M2 e M8 | Coordinatore dopo audit e prove; nessun costo nuovo autorizzato implicitamente

### ADR-16.a — registro riga 3421, stadio `M2.entry`, «Ledger minimo»

- Riga 2577: **ADR-16** | Calibrazione incompleta | Richiedere tutti i master (escluso dal mandato), permettere ogni sottoinsieme con matrice, ibrido | Matrice delle otto combinazioni, dark-flat come quarto asse, stati tipizzati dei master, omissione delle correzioni opzionali prima della ricetta, blocco del solo ramo obbligatorio. P0 vincolante per requisito; matrice e default da validare. | Matematica e casi definiti, test assenti. | M3 | Coordinatore dopo prove e review scientifica

### ADR-17.a — registro riga 3424, stadio `M2.entry`, «Modello minimo»

- Riga 2578: **ADR-17** | Covarianza opzionale | Nessuna covarianza, diagonale, NPS locale, densa | Diagonale per pixel più NPS locale, kernel e fattore di apertura, con correlazioni esplicite dei master e incertezza della geometria quando modellata. Nessuna matrice globale densa richiesta. Proposto. | Approssimazione dichiarata, validazione assente. | M3 e M6 | Coordinatore dopo prove e review numerica

### C-01.a — registro riga 3427, stadio `M2.entry`, «Light minimo»

- Riga 1367: 9. Fixture, metriche, soglia e gate | Sintetici con `B`, `d`, `q`, `S` noti, otto combinazioni, dark-flat e stati dei master. Metriche: bias residuo, rumore, flat residuo, perdita di segnale. Soglia dal ground truth. Gate M3.
- Riga 1368: 10. Failure mode, warning e provenance | Prima della scelta, master incompatibile o offset ignoto escludono le correzioni non giustificate; omissione e percorso parziale sono ricette distinte. Dopo la selezione, prerequisito perso o errore producono STOP senza sostituzione. Warning e provenance obbligatori.

### C-02.a — registro riga 3430, stadio `M2.entry`, «Master minimo»

- Riga 1378: 4. Alternative e scelta | Media, mediana, media pesata, sigma clipping, winsorized. La scelta dipende da `N`, dal modello di outlier e dal costo. Per `N` piccolo la mediana non è una prova di identificazione.
- Riga 1380: 6. Numerica, stabilità e varianza | Accumulo in f64, output f32 quando sicuro. L'incertezza del master non è sempre `sigma/sqrt(N)`: la correlazione tra frame e la normalizzazione vanno dichiarate.
- Riga 1383: 9. Fixture, metriche, soglia e gate | Sintetici con bias, dark, flat e outlier iniettati. Metriche: residuo, rumore, struttura spaziale, stabilità. Soglia dal ground truth. Gate M3.
- Riga 1384: 10. Failure mode, warning e provenance | Frame insufficienti, gruppo incompatibile, flat saturo, bassa illuminazione: rifiuto o warning. Provenienza di ogni frame e di ogni scarto.
- Riga 1385: 11. Ottimizzazione | Accumulatori in streaming, nessuna pila completa in RAM, cache del master.

### C-10.a — registro riga 3440, stadio `M2.entry`, «FITS minimal»

- Riga 1547: 10. Failure mode, warning e provenance | Metadato non rappresentabile: sidecar e warning. Disco pieno: nessun file completo. Provenienza con hash degli output.

### D-03 — registro riga 3453, stadio `M2.entry`, «Core»

- Riga 2869: **D-03** | Linguaggio e architettura del core | C++20 o 23, Rust, ibrido Python | C++20 provvisorio; Rust e ibrido da misurare in M1 | APERTA tecnica | Coordinatore dopo spike e review | M1

### D-04 — registro riga 3454, stadio `M2.entry`, «GUI»

- Riga 2870: **D-04** | Toolkit GUI e comunicazione con il core | Qt Widgets, QML, GTK4, egui, Slint, Tauri, Avalonia | Qt Widgets provvisorio; confronto in M1 su viewer, accessibilità, colore e packaging | APERTA tecnica | Coordinatore dopo spike e review | M1

### D-05 — registro riga 3455, stadio `M2.entry`, «Persistenza»

- Riga 2871: **D-05** | Persistenza: SQLite, JSON o ibrido | SQLite, JSON, ibrido | SQLite come sola autorità, backup e schema sconosciuto in sola lettura | APERTA tecnica | Coordinatore dopo prove ADR-04 | M2

### D-09.a — registro riga 3460, stadio `M2.entry`, «Soglie minime»

- Riga 2875: **D-09** | Soglie numeriche per algoritmo | Soglia unica, soglia per algoritmo, nessuna soglia | Definire per algoritmo, con ground truth e dataset, prima dell'implementazione | APERTA tecnica | Coordinatore dopo prove e review numerica indipendente | Prima di ogni funzione

### D-17.a — registro riga 3470, stadio `M2.entry`, «Covarianza minima»

- Riga 2883: **D-17** | Covarianza ammessa per dichiarare una misura | Nessuna, diagonale, diagonale più NPS locale, densa | Diagonale più NPS locale e correlazioni dei master; incertezza della geometria quando modellata | APERTA tecnica | Coordinatore dopo prove e review numerica | M3 e M6

### M2 — registro riga 3489, stadio `M2`, «Fondamenta»

- Riga 2675: Il gate M2 richiede la slice FITS e sintetica con calibrazione `L - D`, flat `Q`, maschere, metadati, incertezza e report. Richiede inoltre DAG durevole, recovery, cancel e worker prima di un carico grande. Una fondazione che non elabora un dato astronomico reale non supera il gate.

### M2.a — registro riga 3490, stadio `M2.a`, «FITS»

- Riga 2667: M2.a Immagine e FITS | Immagine tipizzata, FITS mono e CFA, metadati, maschere, unità | Import e roundtrip FITS con metadati conservati

### M2.b — registro riga 3491, stadio `M2.b`, «DAG»

- Riga 2668: M2.b Hash e DAG | Content hash, nodo, dipendenze, invalidazione, token di generazione | Test di invalidazione selettiva e risultato stale

### M2.c — registro riga 3492, stadio `M2.c`, «Recovery»

- Riga 2669: M2.c Persistenza e recovery | SQLite proposto, scrittura atomica, commit, orfani, migrazione, schema sconosciuto | Crash test e riapertura

### M2.d — registro riga 3493, stadio `M2.d`, «Scheduler»

- Riga 2670: M2.d Scheduler base | Budget Auto e Manuale, prenotazioni, worker, cancel, checkpoint | Limite del pool rispettato, job impossibile rifiutato, fallimenti iniettati gestiti senza pubblicare output parziali; nessuna garanzia hard sull'RSS

### M2.e — registro riga 3494, stadio `M2.e`, «Slice scientifica»

- Riga 2671: M2.e Calibrazione vera | `L - D` con dark matched, `Q = Fc/N(Fc)`, maschere, varianza, report | Sintetico con ground truth e report leggibile

### M2.f — registro riga 3495, stadio `M2.f`, «Infrastruttura fixture»

- Riga 2672: M2.f Fixture e CI | FITS sintetici, dark e flat, maschere, test unitari e property-based | Suite eseguibile e invarianti

### M2.g — registro riga 3496, stadio `M2.g`, «Applicazione contratti minimi già approvati»

- Riga 2673: M2.g Contratti minimi | ADR-03 modello pixel e maschere, ADR-04 persistenza, ADR-05 provenienza e lineage, ADR-06 budget dell'intero batch, ADR-16 stati dei master, ADR-17 rappresentazione della covarianza | Contratti scritti e testati nelle parti che M2 usa; nessuna slice scientifica dipende da un contratto non deciso

### G-STOP — registro riga 3566, stadio `M2.d`, «Stop senza sostituzione»

- Riga 3571: Un predicato non è un'istruzione di fallback. Le scelte sono congelate nella ricetta; una condizione richiesta non soddisfatta attiva G-STOP. DFB usa disponibilità e compatibilità accertate prima della scelta; nessuna combinazione implica da sola calibrazione completa. Gli stati DF e flat sono assi separati.

