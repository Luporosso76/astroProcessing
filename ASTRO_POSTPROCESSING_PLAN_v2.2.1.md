# Piano consolidato di post-processing astrofotografico

**Versione documento:** 2.2.1
**Data:** 25 ottobre 2026
**Stato:** consolidamento mirato del mandato `RICHIESTA_REVISIONE_PLAN_2_2.md`. D-19 e D-20 APPROVATE P0; D-21 APPROVATA come preferenza per pack inclusi, verifiche legali e tecniche ancora da eseguire. Esecuzione Linux M0..M9 autorizzata nei permessi effettivi; M10 non autorizzata. Review indipendente della 2.2.1: PENDING. Nessun risultato applicativo acquisito da questa revisione.
**Sostituisce come documento di lavoro:** `ASTRO_POSTPROCESSING_PLAN_v2.2.md`, conservato intatto come storico; SHA-256 `8fa0f881bd932c1e468449dea74037bad8cd88bdc8937c7077d1f233aa1f29c1`.
**Documenti storici di contesto:** `ASTRO_POSTPROCESSING_PLAN.md` versione 2.0 e `ASTRO_POSTPROCESSING_MASTER_SPEC_CODEX_PLAN.md`, consultati per contesto, non modificati.
**Input di revisione letti integralmente:** piano 2.2 (4452 righe) e mandato `RICHIESTA_REVISIONE_PLAN_2_2.md` (122 righe). Le letture e le review delle versioni precedenti restano attribuite ai rispettivi autori, non a questa revisione.
**Lingua di lavoro:** italiano normale, con nomi tecnici e simboli inglesi quando sono più precisi.
**Piattaforma iniziale:** Linux soltanto. Windows e macOS restano possibilità future e non ricevono lavoro prima di una autorizzazione separata.

---

## 0. Perimetro e metodo di questa revisione

### 0.1 Perimetro operativo

Questa stesura è un lavoro documentale limitato a `ASTRO_POSTPROCESSING_PLAN_v2.2.1.md`. Non esegue codice applicativo, test software, rete, installazioni, Git, spike o benchmark. Il mandato generale autorizza invece l'esecuzione successiva Linux M0..M9 secondo 0.8: il limite di questa delega non revoca quel mandato. Lo storico 2.2 resta intatto. Il coordinatore gestisce istruzioni persistenti, ExecPlan e stati operativi in incarichi separati.

I due input correnti, piano 2.2 e mandato di 122 righe, sono stati letti per intero e non sono stati modificati da questo autore. L'hash del piano 2.2 è in appendice A. Il master e i piani precedenti restano storia consultabile.

NC-01 consente la sola lettura dei metadati tecnici non segreti strettamente necessari a individuare e invocare tool, agenti e compilatori già disponibili. Non autorizza dati personali, credenziali, token, chiavi o modifiche di sistema. Build, fixture, cache configurabili e temporanei restano nel workspace. Un permesso mancante è un blocco reale da registrare, non da aggirare.

NC-01 governa le operazioni degli agenti di sviluppo durante questa revisione, non il comportamento dell'applicazione finale. L'app finale legge i percorsi scelti dall'utente e applica NC-17: lo scratch è una preferenza dell'utente, non il workspace dello sviluppatore. Le due regole restano separate e nessuna espande l'altra. Questa separazione è un requisito di chiarezza, non una licenza a leggere file personali durante lo sviluppo.

### 0.2 Gerarchia delle fonti

1. Le istruzioni nuove del proprietario prevalgono su ogni documento precedente.
2. Questa revisione interpreta e organizza quelle istruzioni.
3. Le decisioni DEC-01 a DEC-12 restano vincolanti come decisioni storiche.
4. I vincoli NC-01 a NC-21 raccolgono le istruzioni nuove e prevalgono dove sono più stringenti.
5. Il piano 2.1, il piano 2.0 e la specifica master restano storia consultabile, non normativa dove dicono cose diverse.

La richiesta 2.2 aveva ampliato il perimetro fotografico P0 della 2.1. Il mandato corrente richiede una revisione mirata della 2.2, non una riscrittura: conserva quel perimetro e recepisce decisioni e autonomia operative.

### 0.3 Cosa è stato fatto e cosa non è stato fatto

La redazione 2.2.1 ha letto integralmente piano 2.2 e mandato corrente, copiato lo storico senza sovrascriverlo e applicato patch mirate al nuovo file. Ha conservato contratti scientifici e archi canonici, aggiornando decisioni, priorità approvate, responsabilità operative e limiti richiesti. Non ha prodotto misure empiriche, consultato nuova letteratura o eseguito verifiche su immagini.

Le verifiche 2.2.1 sono documentali: conservazione degli identificativi e degli archi, unicità, chiusura degli estremi e predicati, cicli, copertura transitiva P0, sezioni, diff e hash dello storico. Stadi e formule non modificati conservano le analisi storiche, non una nuova validazione. Non sono stati eseguiti test del software, prove su immagini, benchmark, misure di memoria, prove di packaging o audit legali.

La revisione non eredita il PASS della 2.2 come prova della 2.2.1. Il verbale storico resta in 24.6 con la propria copertura. La nuova review indipendente è PENDING; l'autore non è revisore indipendente del proprio diff.

### 0.4 Analisi specialistiche storiche della 2.2

Le analisi seguenti sono riportate dalla redazione storica 2.2. I modelli erano autodichiarati dal runtime, senza attestazione indipendente del provider. La 2.2.1 le conserva come input storici, non come risultati propri, e non le ha riavviate.

| Dominio | Agente dichiarato | Modello dichiarato | Sessione dichiarata | Copertura effettiva | Limite |
|---|---|---|---|---|---|
| Test e QA scientifico | `tests` | GPT-6.1 Sol | `ses_e8ce4ed7bffeT52fyV77LFXJ80` | Piano 2.1 integrale e richiesta 2.2 integrale; due passaggi | modello dichiarato dal runtime, nessuna attestazione indipendente; nessun test eseguito |
| Architettura e correzione del registro | `architect` | GPT-6 Astra | `ses_e8ce49151ffeojaK2Bx05jtpFQ` | Piano 2.1 e richiesta 2.2 integrali; correzioni mirate del testo 2.2, nodi e archi canonici, verifiche documentali | autore delle correzioni, non revisore indipendente di quel delta; nessuna attestazione indipendente |
| Design e UX | `design` | GPT-6 Astra, non Gemini | `ses_e8ce44d56ffeW2pLxwuZ3G5Ma5` | Piano 2.1 integrale e richiesta 2.2 integrale | il modello è GPT-6 Astra secondo la configurazione effettiva; nessuna attestazione del provider |
| Sicurezza | `security` | GPT-6 Astra | `ses_e8ce40264ffef0EDgjtA9tadYG` | Richiesta 2.2 integrale; piano 2.1 righe 1..739 e sezioni 12, 13, 17..21, 23 integrali, resto in delta | audit read-only parziale, non review completa del piano |
| Revisione critica indipendente | `reviewer` | GLM-5.3 Flash free | `ses_e8cdf2e19ffecLi3MCW67kXmhH` | Richiesta 2.2 integrale; sezioni 2.1 5, 10, 11.7..11.10, 13, 20, 21, 23, 24 integrali, più 14, 15 e 22; non ha letto il piano 2.1 integrale e non ha letto il nuovo documento | analisi completata sul perimetro dichiarato; finding adjudicati dal coordinatore, non esito di review finale |
| Redazione documentale | `docs` | deepseek-v4.1-flash-free su provider `opencode` | `ses_e8cdc8815ffeAPmj7EzQKXPTZl` | prima stesura e correzioni documentali; successivamente registro corretto dall'architetto | modello esposto dal runtime; nessuna attestazione indipendente |
| Review indipendente delle proposte e del testo | `architect` | GPT-6 Astra | `ses_e8cd7a18effet7SEK9Mx757TMg` | richiesta integrale; parti dichiarate della 2.1; intero testo 2.2 precorrezione di 3136 righe; successive riletture mirate, inclusa intera appendice D corretta | PASS documentale; nessuna nuova lettura integrale dell'ultima versione, nessuna validazione empirica |
| Coordinamento e verifica | non un subagente | GPT-6.1 Sol | sessione del coordinatore, non riportata qui | input 2.1 e richiesta integrali; arbitraggio dei rilievi, patch, controlli indipendenti del registro e dei sei hash storici | verifiche documentali, non validazione empirica né attestazione del provider |

Il catalogo del runtime ha esposto 27 modelli. Non significa che 27 modelli siano stati provati. Gli incarichi elencati non dimostrano disponibilità universale, stabilità nel tempo o qualità uniforme. I prezzi e i consumi reali di token non sono esposti dal runtime e non vengono inventati.

Le analisi di tests, design, security e reviewer e le review indipendenti sono state svolte in sola lettura. Docs, l'architetto autore delle correzioni e il coordinatore hanno modificato soltanto il documento v2.2. Nessuno di questi incarichi ha eseguito test applicativi, accessi di rete, installazioni o benchmark. I rilievi sono stati accettati, rettificati o respinti con motivazione: analisi preliminare in 2.7, chiusura del testo in 24.6. Nessun finding è accolto per autorità del modello.

Non esiste uno specialista di astrofisica dedicato. La copertura scientifica è assunta da `tests`, `architect` e dalla redazione. È una copertura, non una certificazione astrofisica. La lacuna resta un rischio dichiarato e richiede una revisione scientifica indipendente prima di implementare algoritmi critici.

### 0.5 Fonti e stato della letteratura

Questa revisione non ha consultato nuove fonti web. Le verifiche di prodotto della 2.0 restano storiche e non sono state rifatte. La bibliografia algoritmica è una lista di candidati non letti integralmente. Nessun articolo è dichiarato letto se non lo è stato.

La validazione della letteratura è un gate futuro, prima dell'implementazione di ogni operatore critico. Le formule di questa revisione sono modelli indipendenti di fisica del sensore, statistica, geometria e teoria dei segnali, non derivazioni da codice o documentazione implementativa di concorrenti.

Il mandato autorizza verifiche e scelte tecniche autonome, non concede diritti su librerie, cataloghi o dataset. Licenze e disponibilità restano da verificare prima dell'adozione.

### 0.6 Come leggere il documento

Il documento è autosufficiente. Una nuova sessione può partire da qui senza conoscere la conversazione precedente. La sequenza consigliata è:

1. sezioni 1, 3, 4 e 5 per visione, vincoli, priorità, MVP e matrice delle decisioni;
2. sezioni 7 e 8 per architettura e scienza;
3. sezione 9 per la calibrazione flessibile;
4. sezione 10 per i contratti scientifici, compresi i nuovi C-11 a C-19;
5. sezioni 11, 12 e 13 per requisiti, NFR e Advisor di strategia;
6. sezioni 20, 21 e 23 per ADR, roadmap e decisioni;
7. sezione 24 per change log, checklist e stato onesto;
8. appendice D per il registro completo delle dipendenze.

### 0.7 Terminologia minima

- **Segnale:** grandezza fisica o stimata che l'operatore vuole misurare, per esempio il flusso integrato `S`.
- **Stimando:** la quantità che una stima o un test intende approssimare. Due pesi sono confrontabili solo se stimano la stessa grandezza.
- **Master:** frame di calibrazione combinato, con stato dichiarato, non identificato dal nome del file.
- **Maschera di invalidità:** insieme dei pixel che non devono contribuire al risultato. Non è alpha.
- **Coverage:** frazione geometrica di area di un pixel di output coperta da campioni validi di input.
- **Alpha:** canale di composizione o trasparenza per display ed editing. Non è una maschera scientifica.
- **Varianza:** incertezza del segnale per pixel. Non è una immagine da interpolare come se fosse segnale.
- **Covarianza:** relazione tra incertezze di pixel diversi o tra parametri stimati. Può essere approssimata, ma l'approssimazione va dichiarata.
- **DAG:** grafo aciclico diretto delle operazioni, immutabile nelle sue revisioni.
- **Template di strategia:** modello tipizzato e versionato di una sequenza parziale di operatori, con precondizioni, postcondizioni, capability prodotte e revocate.
- **Ruleset:** insieme dichiarativo di regole di composizione e preferenze, trattato come dato non fidato e limitato a identificativi in allowlist.
- **Effect ledger:** registro degli effetti già applicati, usato per evitare doppie calibrazioni e ripetizioni illegittime, non per vietare filtri legittimi ripetibili.
- **Provenance:** insieme verificabile di input, parametri, versioni, backend, maschere e trasformazioni che permette di ricostruire un risultato.

### 0.8 Protocollo operativo futuro degli agenti

Il mandato autorizza Linux M0..M9, senza conferme ricorrenti per ADR, spike, sotto-milestone o milestone. Le scelte tecniche sono autonome solo dopo le evidenze richieste; il consenso tra agenti non sostituisce prove. Sono riservati M10, distribuzione commerciale, licenza del codice con conseguenze irreversibili, nuovi costi a pagamento, credenziali mancanti, distruzione fuori progetto e permessi reali della piattaforma. Un blocco riservato isola il ramo interessato; le attività indipendenti continuano.

1. Il coordinatore registra obiettivo autorizzato, input, vincoli di filesystem, sola lettura o scrittura ammessa, deliverable e criterio di uscita. Una richiesta di piano non autorizza codice, installazioni o benchmark.
2. Esegue una ricognizione delle capacità attraverso interfacce autorizzate. Distingue catalogo, accesso riuscito, modello dichiarato dal runtime e disponibilità non verificata. Usa CodeGraph o Graphify solo se esiste già l'indice pertinente; non lo crea senza consenso.
3. Assegna la proposta primaria allo specialista competente, passando contesto sufficiente, requisiti e limiti. Non forza un modello diverso da quello configurato senza richiesta esplicita. Se lo specialista manca o fallisce, dichiara il limite prima di proporre un'alternativa.
4. Parallelizza solo attività indipendenti. Un file ha un solo autore attivo; nessun agente aggira permessi o delega nuovamente al coordinatore lo stesso incarico.
5. Richiede una critica indipendente del testo o del diff effettivo, non del solo riassunto dell'autore. Una review parziale dichiara l'intervallo letto e non vale come review completa.
6. Per scienza, risorse, sicurezza, licenze e UX coinvolge gli specialisti pertinenti. Un modello non costituisce attestazione professionale; fonti non lette e test non eseguiti restano espliciti.
7. Il coordinatore verifica i finding, risolve gli errori dimostrabili e registra disaccordi non risolti, alternative, evidenze e condizioni che cambierebbero la raccomandazione. Non accetta una formula o una valutazione per autorità del modello.
8. Dopo le correzioni, richiede verifica mirata dei rilievi e controlli di regressione adeguati al deliverable. Per un documento controlla ID, dipendenze, perimetro, formule e integrità degli input; non li presenta come test del software.
9. Verifica automaticamente i gate scientifici, numerici, di regressione, sicurezza, licenze, prestazioni, compatibilità e DoD; conserva prove e review. Se falliscono, indaga, corregge e ripete autonomamente senza degradare criteri, eliminare fixture o disabilitare funzioni. NC-21 arresta il ramo del prodotto, non il lavoro di sviluppo. Dopo il gate realmente superato aggiorna lo stato e prosegue; interpella il proprietario solo sulle questioni riservate sopra.
10. Registra agente, sessione, modello/provider dichiarati, materiali letti, risultato, stato e limiti della review. Costi e token si riportano solo se esposti, senza stime inventate. Ogni milestone conserva questo verbale nel proprio ExecPlan autorizzato.

### 0.9 Repository indicato dal proprietario

Il proprietario ha indicato come repository ufficiale del progetto:

`https://github.com/Luporosso76/astroProcessing.git`

Alla redazione della 2.2 l'URL era soltanto dichiarato e il workspace non era un repository Git. Questa delega 2.2.1 non esegue `git init`, `add`, `commit`, `remote` o `push`, non crea README, LICENSE o `.gitignore` e non accede alla rete. Non attribuisce a sé verifiche svolte in parallelo da altri incarichi; lo stato operativo corrente è registrato separatamente.

Il mandato autorizza l'ispezione autonoma della copia locale e del repository ufficiale nei permessi reali. Non creare storia alternativa se il repository ufficiale contiene storia utile; niente force-push, riscrittura della storia, push su branch protetti, cancellazione di branch altrui o pubblicazione di segreti. Commit locali e push verso il solo `origin` ufficiale sono autorizzati, con credenziali già disponibili e permessi effettivi, dopo test, review e controllo sicurezza/privacy. L'ultima istruzione del proprietario prevale sui precedenti divieti di push di `ISTRUZIONI_OPERATIVE.md`, che resta intatto. Questa delega documentale non esegue Git né attesta verifiche remote proprie; lo stato operativo e le evidenze correnti sono in `docs/progress.md` e `docs/evidence/`.

Le istruzioni persistenti sono in `AGENTS.md`; `PLANS.md` definisce gli ExecPlan in `plans/`, `docs/decisions/` conserva gli ADR e `docs/test-matrix.md` distingue prove pianificate ed eseguite. Il coordinatore aggiorna questi file in incarichi separati. Prima di interrompere una sessione registra concluso, in corso, fallito, causa e prossimo comando verificabile in `docs/progress.md` e nell'ExecPlan pertinente. Se l'utente interrompe, arresta in sicurezza senza avviare altri job. Non promette esecuzione dopo la fine della sessione senza un runtime persistente realmente disponibile e autorizzato.

---

## 1. Sintesi esecutiva

### 1.1 Visione

Il prodotto è una applicazione desktop nativa per il post-processing astrofotografico, pensata per accompagnare l'utente dall'immagine già acquisita al risultato finale senza obbligarlo a passare stabilmente tra strumenti separati. Il nucleo scientifico e l'orchestrazione sono del progetto. Non è un wrapper di un altro programma astronomico.

L'ambizione è un prodotto competitivo e accessibile, non un prototipo dimostrativo. Accessibile significa che un principiante arriva a un primo risultato sensato con pochi passaggi, che un esperto trova parametri, maschere, diagnostica e automazione, e che entrambi usano lo stesso motore. Competitivo significa che il prodotto viene misurato per asse e per dataset contro alternative reali, non che viene dichiarato vincitore in assoluto.

La prima release è Linux. La matrice di supporto è progressiva e dichiara per ogni combinazione se è verificata, compatibile non certificata o non supportata.

### 1.2 Obiettivi del prodotto

- Coprire il ciclo deep sky end-to-end: import, calibrazione, analisi, registrazione, stacking, post-processing, export e riapertura del progetto.
- Rendere il percorso non distruttivo, riproducibile e ispezionabile.
- Gestire RAW di più marche e FITS mono, CFA e RGB, senza legare il prodotto a una singola fotocamera o a un singolo telescopio.
- Offrire tre modalità di uso, Automatica, Guidata ed Esperta, sopra un solo motore.
- Funzionare su CPU per tutte le funzioni fondamentali, con GPU facoltativa su AMD, Intel e NVIDIA.
- Mantenere il nucleo offline. Nessun account e nessun cloud sono richiesti per usare il prodotto.
- Analizzare l'immagine e assistere il post-processing con un motore diagnostico tradizionale, deterministico e offline, senza AI obbligatoria.
- Ottimizzare anche la strategia di pipeline, non solo i parametri di un singolo operatore, entro regole di composizione dichiarate e un budget bounded.
- Gestire la calibrazione con qualunque sottoinsieme di dark, flat e bias, dichiarando ciò che è valido e ciò che non lo è.
- Espandere il prodotto a narrowband, LRGB, multi-sessione, mosaici, planetario, Luna, Sole, comete, analisi scientifica e AI opzionale, con gate separati.

### 1.3 Non obiettivi iniziali

- Acquisizione o controllo di mount, camera, focuser, guida, INDI o ASCOM. Il prodotto importa metadati, non pilota l'osservatorio.
- Cloud obbligatorio, account obbligatorio, telemetria occulta o upload di immagini.
- Rilascio Windows o macOS nella prima fase.
- Addestramento di grandi reti neurali nel primo prototipo.
- Promesse di qualità scientifica per immagini alterate da AI non validata.
- Supporto universale di ogni distribuzione Linux, ogni desktop, ogni GPU o ogni formato RAW storico.
- Promessa di ricostruire informazione astronomica non acquisita, per esempio stelle mancanti, nuclei saturi o nebulosità assenti.

### 1.4 Modifiche principali rispetto alla versione 2.1

Questa revisione non riduce il perimetro. Conserva le 14 capacità P0 precedenti e aggiunge un post-processing fotografico sostanziale già nella prima release.

1. **MVP fotografico.** Maschere, denoise, asinh, sharpening multiscala, riduzione stellare e separazione limitata entrano nel percorso P0 come slice dei requisiti esistenti. Deconvoluzione a PSF fissa, colore relativo e solver assistito sono APPROVATI P0 da D-19 e D-20. Le etichette `.a`, `.b`, `.c` non creano nuovi requisiti padre.
2. **M5 bloccata.** La prima release Linux non è dichiarata finché tutti i nuovi P0 fotografici, le sequenze Advisor e i 14 P0 precedenti non sono soddisfatti e testati. Non si compensa con una nota di marketing.
3. **Advisor di strategia.** L'Advisor non ottimizza solo parametri. Costruisce, valida e confronta sequenze parziali di operatori usando un template tipizzato, un registro interno fidato e un ruleset dichiarativo. La versione P0 funziona su un insieme dichiarato di operatori e non attende M6.
4. **Dipendenze corrette.** F-ANA-01.a non dipende da F-POST-01. Diagnosi, confronto, report e planner, anche su immagini non lineari prodotte dagli operatori headless, sono M3. M4 aggiunge soltanto visualizzazione e interazione. Le slice operative sono definite individualmente nell'appendice D.
5. **Registro delle dipendenze.** L'appendice D separa i nodi (82 F, 24 NFR, 17 ADR, 19 contratti, 21 decisioni, deliverable, milestone) dagli archi tipizzati `requires_all`, `requires_when`, `contains`, `decided_by` e `verified_by`. `requires_all` non contiene milestone o ADR. Ogni ID citato è definito, unico e senza cicli. La matrice 11.11 verifica l'attivazione delle dipendenze condizionali nelle configurazioni P0.
6. **Maschere di prima classe.** Maschera float 0..1, distinta da invalidità, coverage e alpha, con brush, poligono, morfologia, feather, invert, combine, opacity e riproducibilità nel DAG.
7. **Asset utente autorevoli.** Maschere dipinte o importate non sono cache eliminabile. Sono asset versionati con rasterizzazione o eventi durevoli, e il commit nel database avviene dopo la persistenza.
8. **Denoise dichiarato.** À trous non decimato con soft shrinkage, rumore per scala, modelli distinti per dati lineari e non lineari, correlazione ed eteroschedasticità dichiarate, f64 nei passaggi critici.
9. **Stretch corretto.** Asinh analitico `f(k, u) = asinh(k u) / asinh(k)`, con limite di identità per `k` che tende a zero, gestione separata di shift, scala, negativi, fuori intervallo e `k` nullo. Il P0 cromatico usa il fattore comune `C' = g(I) C`, `g(I) = f(k, I)/I`, `I = w_R C_R + w_G C_G + w_B C_B`, con `g(0) = k / asinh(k)`: i rapporti RGB si conservano su tutto il dominio valido pre-gamut, rispetto al riferimento dichiarato dopo offset e bilanciamenti. Il gamut può alterarli dopo. GHS resta P1 con formula propria validata.
10. **Deconvoluzione definita.** P0 APPROVATO con PSF fissa, `b` fissato e stimato a monte, minimi quadrati regolarizzati su dato radiometricamente lineare, funzione obiettivo esplicita sul sottospazio dei pixel validi, `W` precisione PSD che approssima `C_V^{-1}`, condizioni di esistenza e unicità, distinzione tra unicità e stabilità, maschere, negativi trattati senza offset arbitrario, sensibilità a PSF, fondo, `lambda` e rumore, baseline no-op. Un residuo piccolo non è una prova della PSF.
11. **Colore relativo P0 APPROVATO.** Calibrazione cromatica relativa broadband con pacchetto offline, stelle non sature, WCS valido, apertura e unità compatibili. Non è SPCC completa con risposta spettrale e non garantisce un colore fisico con risposta sconosciuta.
12. **Plate solving assistito P0 APPROVATO.** Solver offline in dominio bounded, con posizione e scala approssimate fornite dall'utente o da metadati verificati, match robusto, WCS rifittato e validazione su stelle tenute fuori dal fit.
13. **Sequenze e metriche.** Cinque stati distinti per l'esito, deduplicazione esatta, pruning dichiarato, frontiera esplorata e non spazio esaustivo, confronto solo tra endpoint compatibili, holdout scelto prima della taratura.
14. **Test ampliati.** Nuove famiglie di contratto C-11 a C-19, identificativi T-IO, T-PRJ, T-CAL, T-REG, T-STK, T-POST, T-AST-ASSIST01, T-ANA, T-VAL-INDEP e T-FBK-01, protocollo anti-leakage e corpus di accettazione indipendente. Sono piani, non risultati.

### 1.5 Stato delle decisioni

| Tema | Stato | Conseguenza pratica |
|---|---|---|
| Indipendenza da Siril | Deciso e rafforzato | Zero codice copiato, tradotto o adattato. Zero runtime, CLI, API, engine o plugin Siril. Documentazione Siril non usata come specifica. Solo confronto esterno documentato. |
| Piattaforma iniziale | Deciso | Linux soltanto nella prima release. |
| Deep sky come primo percorso | Deciso | Il primo prodotto verticale è deep sky. |
| Tre modalità UX | Deciso | Automatica, Guidata ed Esperta sullo stesso motore. |
| AI facoltativa | Deciso | Nessun requisito AI per calibrazione, registrazione, stacking e funzioni fondamentali. |
| Advisor tradizionale | Vincolante P0 | Analisi, raccomandazioni, confronto di strategie e rollback senza AI. Restano proposti il default di assistenza e le soluzioni tecniche. |
| Calibrazione flessibile | Vincolante P0 | Otto combinazioni, dark-flat e master tipizzati. Restano proposti il default automatico e le soluzioni tecniche. |
| Post-processing fotografico P0 | Vincolante per perimetro, tecniche da validare | Maschere, denoise, asinh, sharpening, stelle, deconvoluzione, colore relativo e solver assistito sono obbligatori. D-19 e D-20 APPROVATE; i gate restano da eseguire. |
| Linguaggio e stack | Non deciso | Raccomandazione provvisoria C++20 con Qt Widgets; Rust con GTK4 secondo. Nessuna scelta prima di spike autorizzati. |
| GUI definitiva | Non decisa | Qt Widgets, QML, GTK4/libadwaita e alternative competitive da confrontare. |
| Persistenza | Non approvata | SQLite come autorità è una proposta. JSON e modello ibrido restano alternative. |
| Licenza del prodotto | Aperta | Accessibile non significa gratuito deciso. Il modello di costo e licenza resta del proprietario. |
| Packaging Linux | Non deciso | AppImage, Flatpak e pacchetti nativi vanno confrontati con prove. |
| Matrice Linux | Proposta | Tre configurazioni minime concrete da verificare in M0 e M5. |
| Dataset e soglie | Aperti | Dataset pubblici con licenza e hash. Soglie motivate per algoritmo prima dell'implementazione. |
| Covarianza | Proposta approssimata | Diagonale per pixel più correzioni locali validate. Nessuna matrice globale densa richiesta. |

Advisor, calibrazione flessibile e tutte le promozioni fotografiche, comprese D-19 e D-20, sono P0 vincolanti. Tecniche e default si decidono autonomamente dopo le prove. Difficoltà o gate falliti non riaprono il perimetro.

---

## 2. Eredità, analisi e limiti

### 2.1 Cosa conserviamo dalla pianificazione precedente

La pianificazione precedente non ha eseguito codice, spike o benchmark e non ha approvato stack, licenza o persistenza. Ha prodotto una matrice di giudizio, una preferenza di ricerca, critiche e un insieme di vincoli scientifici e architetturali. Questi risultati si conservano come input dichiarati, non come verità misurate.

Conserviamo in particolare:

- motore headless, GUI come client, applicazione locale e offline;
- immagine tipizzata con unità, CFA, geometria, maschere, coverage, varianza e provenance;
- DAG non distruttivo, invalidazione selettiva, cache ricostruibile e protezione dai risultati obsoleti;
- originali intatti, recovery, scritture robuste, report e auditabilità;
- CPU di riferimento, SIMD con dispatch nel preflight, GPU facoltativa;
- separazione tra dato lineare, elaborazione fotografica, display e AI dichiarata;
- confronto con concorrenti su dataset e metriche, senza falsi gap;
- licenze, packaging Linux, accessibilità, affidabilità e sicurezza;
- milestone con gate, senza implementare prima delle decisioni.

### 2.2 Valutazione dello stack senza falsa precisione

La matrice storica assegnava punteggi pesati e produceva, con i pesi base, totali come 4,28 per C++ con Qt e 4,15 per Rust con GTK. Quei totali non sono misure e non vengono usati qui come criterio decisionale. La differenza tra 4,28 e 4,15 non è abbastanza solida per scegliere, e presentarla come tale darebbe una apparenza di precisione che i dati non hanno. La matrice resta come traccia dei criteri, non come verdetto.

La valutazione è qualitativa e separa tre domande distinte: linguaggio del core, toolkit GUI, integrazione tra i due. Una tabella unica che mescola le tre domande nasconde il costo dell'integrazione e favorisce la combinazione più familiare. Le etichette sono giudizi motivati, non punteggi.

#### 2.2.1 Core

| Candidato | Punti forti | Costi e rischi | Evidenza disponibile | Posizione provvisoria |
|---|---|---|---|---|
| C++20 o 23 | Controllo di allocazioni, layout e lifetime; librerie scientifiche C e C++ mature; tooling; prestazioni prevedibili | Sicurezza della memoria affidata alla disciplina; costo di refactoring; errori di lifetime e di aliasing | Analisi di architetto e ricerca, nessuno spike eseguito | Candidato principale per il core |
| Rust | Sicurezza della memoria, concorrenza, confini espliciti, refactoring più sicuro | Ecosistema scientifico meno uniforme, FFI con librerie C, tempi di compilazione, competenze da formare | Preferenza della ricerca precedente, nessuno spike eseguito | Alternativa forte, prima se la sicurezza della memoria diventa criterio dominante |
| Python con core nativo | Prototipazione rapida, ecosistema scientifico, orchestrazione e test | GIL e confini di chiamata, packaging dell'interprete, due mondi da mantenere, controllo memoria indiretto | Conoscenza generale, nessuno spike | Livello di orchestrazione, non sostituto del core nativo |
| C# con .NET e Avalonia | Produttività, tooling, GUI cross-platform, gestione memoria | Ecosistema numerico e driver su Linux da dimostrare, runtime da distribuire, interoperabilità con librerie C | Analisi teorica | Non escluso, non prioritario |

#### 2.2.2 GUI

| Toolkit | Viewer e immagini grandi | Accessibilità, colore e IME | Licenza e deployment | Rischio principale | Posizione provvisoria |
|---|---|---|---|---|---|
| Qt Widgets | Widget nativi e scene graph adatti a tile e piramidi; prestazioni note, ma il rendering delle tile va progettato | Copertura ampia, da verificare su Wayland e X11; gestione colore dipendente dal percorso scelto | LGPL, GPL o commerciale per modulo; link statico o dinamico cambia gli obblighi; packaging consolidato | Peso delle dipendenze e licenza per modulo | Candidato principale |
| QML | Accelerazione grafica e animazioni, ma controllo meno diretto del rendering scientifico | Dipende dai componenti usati; da verificare | Come Qt | Mescolare logica di vista e motore; test headless più difficili | Variante di Qt da valutare, non default |
| GTK4 e libadwaita | Buone primitive, ma il rendering di immagini grandi richiede lavoro; widget nativi GNOME | Buona su GNOME, da verificare su KDE e su X11 nativo | LGPL, runtime GTK, Flatpak naturale | Integrazione su desktop non GNOME e maturità di alcune API | Candidato principale se il core è Rust |
| Alternative rilevanti, per esempio Slint, egui, Avalonia, Tauri | Dipende dal progetto; alcune sono immature per immagini grandi e tile | Spesso il punto debole: accessibilità, IME, focus | Licenze e deployment vari | Ecosistema piccolo, accessibilità incompleta, dipendenze inattese | Solo se un requisito specifico lo giustifica |

#### 2.2.3 Integrazione

| Combinazione | Ownership del confine FFI | Ownership dei buffer | Deployment | Criterio di inversione | Posizione provvisoria |
|---|---|---|---|---|---|
| C++20 con Qt Widgets | Nessun confine di linguaggio; API C++ diretta; il core non usa tipi Qt | Ownership del core; Qt riceve viste o copie per il display | Binario nativo, AppImage, Flatpak o pacchetto; dipendenze Qt | Se la sicurezza della memoria o il costo degli errori di lifetime supera il vantaggio di integrazione | Raccomandazione provvisoria |
| Rust con GTK4 | Rust chiama C per LibRaw, CFITSIO e WCSLIB; GTK tramite binding; confine stretto e tipizzato | Ownership Rust; GTK riceve slice con lifetime o copie | Flatpak o AppImage, runtime GTK, binario nativo | Se il viewer GTK o l'accessibilità non soddisfano i requisiti, o se il costo FFI domina | Seconda scelta, prima se la sicurezza è dominante |
| Rust con Qt | Shim C++ e binding; due modelli di memoria e lifetime | Ownership divisa; rischio di copie e di use-after-free al confine | Qt più Rust; più parti mobili da distribuire | Se la manutenzione del binding supera il beneficio misurato | Da misurare, non default |
| Ibrido Python con core nativo | pybind11 o simile; GIL sui callback | Condivisione zero-copy possibile ma da verificare; rischio di copie | Interprete incluso; dimensione e aggiornamenti | Se packaging o latenza non sono accettabili | Orchestrazione e scripting, non core |
| C# con Avalonia e core nativo | P/Invoke; marshalling e pinning | Buffer gestiti dal GC con pinning; rischio di copie | Runtime .NET o distribuzione self-contained | Se prestazioni numeriche o librerie scientifiche non bastano | Non prioritario |

La raccomandazione provvisoria è C++20 per il core con Qt Widgets per la GUI, perché riduce il costo di integrazione e usa un ecosistema maturo per viewer, stampa, accessibilità e packaging. La seconda scelta è Rust con GTK4 se la sicurezza della memoria e la robustezza dei confini diventano il criterio dominante. Il disaccordo tra architetto e ricerca resta registrato: l'architetto preferiva C++ con Qt, la ricerca preferiva Rust. Non viene appianato per comodità.

Le tre tabelle non sostituiscono gli spike. La scelta finale richiede misure su kernel rappresentativi, viewer e integrazione, con dataset, metriche, criteri di uscita e un criterio di inversione dichiarato per ogni combinazione. Gli spike sono pianificati in M1 e non eseguiti ora.

### 2.3 Sensitività alla sicurezza

La ricetta storica è registrata e verificabile. Usa nove criteri, indicati con le sigle P, S, L, U, M, G, T, A, D. I pesi base sono, nello stesso ordine, 20, 15, 15, 12, 12, 10, 6, 5, 5. Lo scenario di sicurezza usa 15, 30, 12, 10, 12, 6, 5, 5, 5.

I punteggi registrati dall'analisi dell'architetto sono:

| Candidato | P | S | L | U | M | G | T | A | D |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| C++ con Qt | 5 | 2 | 5 | 5 | 4 | 5 | 5 | 4 | 3 |
| Rust con GTK4 | 5 | 5 | 3 | 4 | 4 | 4 | 4 | 3 | 4 |

Con i pesi dello scenario di sicurezza il calcolo dà 3,83 per C++ con Qt e 4,28 per Rust con GTK4. Con i pesi base lo stesso calcolo riproduce 4,28 per C++ con Qt e 4,15 per Rust con GTK4, coerenti con i totali storici. Il risultato cambia la graduatoria rispetto ai pesi base, ma non è una misura: è un giudizio ponderato.

Questi numeri sono giudizi storici, non misure. Non hanno unità, incertezza o origine sperimentale. La scelta tecnica autonoma richiede spike e dati, non una preferenza personale o una nuova conferma del proprietario.

La lezione resta valida. Se la sicurezza della memoria diventa il criterio dominante, il vantaggio di C++ si riduce o si inverte. Se prevalgono ecosistema GUI, librerie e tempo di sviluppo, C++ con Qt resta forte. I pesi vanno rivalutati al gate, non congelati.

### 2.4 Errori corretti rispetto alla 2.0

- **Significato di `A` nella propagazione.** La formula `Sigma_out = A Sigma_in A^T` è la propagazione generale di una covarianza sotto una trasformazione lineare, e vale anche per la varianza fotometrica quando `A` è la matrice di resampling. L'errore della 2.0 non era la formula: era il significato attribuito ad `A`, trattata come trasformazione di coordinate o come identità, senza dichiarare che la matrice dei pesi è un oggetto distinto.
- **Varianza interpolata come segnale.** Trasferire una mappa di varianza con lo stesso kernel del segnale non produce la varianza dell'output. La varianza di una combinazione lineare dipende dai quadrati dei pesi e dalle covarianze.
- **WCS trattato come mappa di pesi.** La mappatura `WCS_out(p) = WCS_in(T(p))` descrive le coordinate, non i pesi.
- **Copertura confusa con somma dei pesi o con il conteggio dei campioni.** Coverage è frazione geometrica di area; conteggio, somma dei pesi firmati e alpha sono grandezze distinte.
- **Flusso e brillanza trattati come equivalenti.** Conservare il flusso integrato o la brillanza superficiale dipende dallo stimando, dal kernel e dall'area del pixel.
- **Deconvoluzione descritta come lineare.** Richardson-Lucy è iterativa e non lineare anche con input lineare. Wiener è lineare solo per un filtro fissato.
- **Calibrazione con master obbligatori.** La realtà è una matrice di otto combinazioni, con dark-flat e master precalibrati.
- **Flat non calibrato usato comunque.** Senza offset del light noto, la divisione per un flat non corretto non garantisce una calibrazione.
- **`F - B` usato come sostituto universale del dark-flat.** È valido solo quando il termico è trascurabile e la scelta è registrata.
- **Pesi e qualità sommati senza controllo.** Un fattore di qualità che contiene già il rumore conta due volte la stessa degradazione.
- **Soglie presentate come universali.** `RMS <= 0.1 sigma_noise`, jitter PSF e soglie MAD restano candidati con dominio limitato.
- **M2 descritto come fondazione astratta.** M2 deve produrre una prima slice scientifica reale.
- **Advisor assente.** Ora ci sono sei F-ANA P0, con estensione alla strategia.

### 2.5 Errori e contraddizioni corretti rispetto alla 2.1

- **F-ANA-01 dipendeva da F-POST-01 in M4 mentre le primitive erano in M3.** La dipendenza era impropria. La diagnostica numerica headless non richiede viewer né STF. La dipendenza è rimossa e sostituita da slice `.a` headless e `.b` GUI.
- **M5 poteva sembrare una release completa con post-processing fotografico in M6.** Ora M5 è bloccata finché i nuovi P0 fotografici non sono soddisfatti.
- **Maschere ridotte a ROI.** Ora esiste un motore maschere P0 con editing e riproducibilità; la segmentazione avanzata resta P1.
- **Denoise generico.** Ora il metodo P0 è definito, con limiti e test di oversmoothing.
- **Stretch avanzato ambiguo.** Asinh è P0 con formula e casi limite; GHS è P1.
- **Stelle e deconvoluzione senza contratto sufficiente.** Ora hanno contratti espliciti e limiti di identificabilità.
- **Colore e solver con decisione di perimetro.** D-20 li approva entrambi P0 con i limiti dichiarati; la validazione resta futura.
- **Advisor limitato ai parametri.** Ora ottimizza sequenze parziali, con template, registro, ruleset, ricerca bounded e metriche compatibili.
- **Dipendenze dei requisiti non tracciate per foglia.** Ora l'appendice D è il registro canonico, con slice e sub-milestone.

### 2.6 Limiti residui

La matrice Linux non è verificata con prove. Le versioni dei concorrenti sono state registrate alla data della pianificazione precedente e non riconfermate. Le licenze candidate non hanno avuto audit legale. I dataset pubblici non sono scelti. Le soglie numeriche non sono validate. Le formule sono controllate per coerenza interna, non con esperimenti. La scelta dello stack, del packaging, della persistenza e della licenza resta aperta. Non esiste una certificazione astrofisica indipendente. Le prestazioni dei nuovi operatori e dello spazio di ricerca Advisor non sono misurate.

### 2.7 Esito della critica indipendente e adjudication

La prima critica indipendente delle proposte ha coperto la richiesta 2.2 integrale e le sezioni dichiarate della 2.1, non il piano integrale e non il nuovo documento. Il coordinatore ha verificato ogni finding. La tabella seguente registra decisione, motivo e limite di quel passaggio preliminare. La successiva review sul testo effettivo è registrata separatamente in 24.6.

| Finding | Decisione | Motivo e limite |
|---|---|---|
| Mismatch tra milestone core e UI di F-ANA | Accettato | F-ANA-01..06 sono spezzati in `.a` headless e `.b` GUI. La diagnosi numerica non dipende dal viewer. |
| Nuovi contratti C-11 e seguenti indispensabili | Accettato | Maschere, denoise, sharpening, stelle, deconvoluzione, colore relativo e strategia non possono entrare in P0 senza contratto di undici campi. Aggiunto anche C-18 per il solver assistito. |
| DoD deve coprire tutti i P0 | Accettato | La DoD elenca ogni capacità nuova e le 14 precedenti, con evidenze e rollback. |
| Distinzione tra leakage dell'holdout e budget | Accettato | L'holdout è scelto prima della taratura; il budget di ricerca è separato dal costo della validazione finale. |
| Perimetro e licenza del pack cataloghi | Accettato | Catalogo, indice, passband e dati sono oggetti distinti; licenza e hash sono prerequisiti, non dettagli. |
| Prerequisiti del solver espliciti | Accettato | Posizione e scala approssimate o metadati verificati, dominio bounded, validazione su stelle escluse dal fit, fallimento esplicito fuori dominio. |
| Denoise lineare obbligatorio e unico P0 | Respinto e rettificato | Il rumore su dati non lineari non è impossibile da stimare né non falsificabile. Si possono usare simulazioni con trasformazione nota e repliche. Il P0 mantiene modelli distinti per lineare e non lineare, senza copiare la varianza. |
| Riduzione stelle obbligata a usare wavelet | Respinto e rettificato | La riduzione P0 usa il fit PSF delle stelle, non à trous. L'infrastruttura multiscala può essere condivisa con denoise e sharpening quando validata, ma i contratti hanno scopi diversi. Non si impongono tre reimplementazioni indipendenti. |
| Valutare gli artefatti solo prima del primo stretch | Respinto e rettificato | Un endpoint solo pre-stretch non basta per gli artefatti fotografici. Si conserva un endpoint fotografico comune e un antenato lineare per la diagnostica. Non si invertono tutte le metriche per fingere un dominio lineare. |
| Un solo holdout e una sola valutazione finale per sempre | Respinto e rettificato | Si usa un batch di candidati congelato e dichiarato prima della valutazione finale, con molteplicità pianificata. L'holdout non si riusa in modo adattivo. Non è obbligatorio testare un solo candidato in eterno. |
| Ogni operatore idempotente o commutativo | Respinto | È falso in generale. Le regole algebriche valgono solo se dichiarate e provate. Non si inventa un grado di chiusura. |
| PSF derivabile dalla sola FWHM | Respinto e rettificato | Una FWHM sola non identifica la PSF. Servono profili stellari, centri, normalizzazione, incertezza e modello di fit. Il padding riflesso ripetuto non garantisce l'assenza di aloni. |
| `R_L + alpha S_L' = I_L` per ogni stella separata | Respinto e rettificato | Vale solo con la stessa `S_L` consistente usata per costruire `R_L = I_L - S_L`. Se `S_L'` è una versione editata diversa, l'identità non regge. La riduzione `I_L - alpha S_L` è una alterazione fotografica legittima con `alpha = 0` identità, non una conservazione universale di media e varianza. |

Le rettifiche sono incorporate nei contratti C-09, C-12, C-13, C-14, C-15, C-16 e C-17, nei test e nella strategia di confronto. Questo verbale preliminare non sostituisce la review del file effettivo, conclusa nei limiti di 24.6.

---

## 3. Decisioni vincolanti conservate, DEC-01 a DEC-12

Le decisioni seguenti sono conservate dalla specifica originale. Dove una istruzione nuova è più stringente, il vincolo nuovo prevale e resta registrato nella sezione 4. La colonna di verifica indica come si controlla che la decisione non sia stata aggirata.

| ID | Decisione | Stato | Verifica e gate |
|---|---|---|---|
| DEC-01 | Motore elaborativo indipendente da Siril | Conservata e rafforzata: nessuna dipendenza strutturale, runtime, API, plugin, CLI o engine. Nessun codice copiato, tradotto o adattato. | Revisione delle dipendenze e del codice, SBOM, test di build senza Siril. Gate M0, M2, M5. |
| DEC-02 | Gli algoritmi astronomici fondamentali sono sviluppati o controllati dal progetto | Conservata. Librerie general purpose ammesse se licenza e comportamento sono verificati. | Contratti scientifici della sezione 10, revisione indipendente, test su ground truth. Gate M3. |
| DEC-03 | Prima piattaforma: Linux soltanto | Conservata. Windows e macOS solo in roadmap potenziale e con autorizzazione separata. | Matrice Linux e packaging. Nessun lavoro su altre piattaforme. Gate M5, M10. |
| DEC-04 | Linguaggio non deciso | Conservata. La comparazione resta obbligatoria. C++ con Qt e Rust con GTK sono finalisti, non vincitori. | Spike M1 con metriche, rivalutazione dei pesi, decisione tecnica autonoma documentata. Gate M1. |
| DEC-05 | RAM configurabile | Conservata. Modalità Auto e Manuale, nessun valore personale o universale imposto. | Test con più budget, preflight, spill, rifiuto motivato. Gate M2, M4. |
| DEC-06 | Deep sky come primo percorso applicativo | Conservata. Planetario, Luna, Sole e altri scenari sono successivi ma previsti nell'architettura. | Confini nella sezione 7.11, sotto-milestone M9 separate. Gate M9. |
| DEC-07 | Precisione e risultato verificabile prima dell'effetto visivo | Conservata. Nessuna alterazione non dichiarata e nessun algoritmo non testato. | Contratti, metriche, provenance, test di regressione. Gate M3 e M4. |
| DEC-08 | Pipeline modificabile, non distruttiva, riproducibile | Conservata. Invalidazione cache e tracciabilità sono requisiti. | Test del DAG, recovery, roundtrip, hash. Gate M2. |
| DEC-09 | Automatica, Guidata, Esperta | Conservata. Le tre modalità usano lo stesso motore, non tre implementazioni separate. | Test di uguaglianza del risultato a parità di parametri. Gate M4. |
| DEC-10 | AI facoltativa e dichiarata | Conservata. Output scientifico, fotografico e generativo restano separati. | Provenance AI, opt-in, test di artefatti. Gate M8. |
| DEC-11 | Codex sul PC dell'utente dopo la pianificazione | Mandato Linux M0..M9 autorizzato; M10 esclusa. | ExecPlan, prerequisiti ed evidenze prima di ogni fase; nessuna nuova conferma rituale. Gate M0. |
| DEC-12 | Usare agenti specializzati e modelli alternativi effettivamente disponibili | Conservata. Delega e revisione incrociata sono obbligatorie nei limiti reali, senza inventare disponibilità. | Protocollo agenti, registro delle deleghe, nessuna attestazione inventata. Gate M0 e M1. |

---

## 4. Nuovi vincoli, NC-01 a NC-21

I vincoli nuovi sono criteri di conformità. Non sono raccomandazioni e non hanno un default implicito che li indebolisca.

| ID | Vincolo | Conseguenza verificabile | Gate o milestone |
|---|---|---|---|
| NC-01 | Workspace protetto e accesso tecnico minimo | Scritture, build, fixture, cache configurabili e temporanei in `/opt/workspace/astroProcessor`. Sola lettura esterna ammessa esclusivamente per metadati tecnici non segreti strettamente necessari a tool, agenti e compilatori già disponibili. Vietati dati personali, segreti e modifiche di sistema; un permesso realmente mancante si registra e non si aggira. Governa gli agenti, non l'app finale, che segue NC-17. | Tutte le fasi |
| NC-02 | Zero codice Siril copiato, tradotto o adattato | Il codice non contiene derivazioni da Siril. Nessuna chiamata CLI, API, plugin o engine. Siril è solo confronto esterno documentato. | M0, M2, M5 |
| NC-03 | Nessuna personalizzazione su attrezzatura o hardware proprietario | I requisiti non dipendono da una marca di fotocamera, un telescopio, una distribuzione o una macchina personale. | M0, M3 |
| NC-04 | RAW multivendor e FITS mono, CFA e RGB | Validazione con campioni pubblici licenziati e fixture sintetiche. Le foto private non sono materiale di prova. | M3 |
| NC-05 | Linux ampio dal primo rilascio, con livelli di supporto | Matrice minima con tre configurazioni concrete, estensione progressiva, nessuna promessa su ogni Linux storico. | M0, M5 |
| NC-06 | Minimi di ABI, kernel e CPU misurati, non arbitrari | La soglia minima si determina con prove. x86_64 è il candidato iniziale. ARM64 è uno studio, non una promessa. | M0, M5 |
| NC-07 | GPU AMD, Intel e NVIDIA facoltativa | Viewer software e tutte le funzioni fondamentali funzionano su CPU. Il core è offline. La GPU non è promessa su ogni combinazione. Il percorso CPU è primario e scelto a priori, indipendente dalla presenza o dal guasto della GPU. | M2, M7 |
| NC-08 | SIMD con dispatch nel preflight | Nessun requisito universale di AVX2. Il binario seleziona nel preflight il percorso SIMD disponibile oppure il percorso scalare, entrambi primari e dichiarati. Non esiste sostituzione automatica di percorso a runtime. | M2 |
| NC-09 | Packaging comparato, non promesso | AppImage, Flatpak e pacchetti nativi vanno confrontati con prove di installazione, aggiornamento, primo avvio offline e rimozione. | M5 |
| NC-10 | Nessun cloud o account richiesto | Le funzioni online sono opzionali, dichiarate e disattivabili. Il prodotto funziona offline. | M2, M5 |
| NC-11 | Nome, logo, prezzo e licenza non bloccano la scienza | Nome e logo non sono prerequisiti della progettazione scientifica. Il prezzo resta aperto. Le licenze delle dipendenze si valutano presto per preservare sia la distribuzione open source sia quella proprietaria futura. | M0 |
| NC-12 | Competitività misurata per asse e dataset | Nessuno slogan di vittoria assoluta. Ogni vantaggio ha una metrica, un dataset e un confronto. | M5 |
| NC-13 | MVP è una tappa, non il prodotto finale | L'MVP copre un percorso deep sky dichiarato e ricco, ora inclusivo del post-processing fotografico P0. Il prodotto finale non è limitato a una singola sessione. | M4, M5 |
| NC-14 | Nessun lavoro inventato | Deleghe, ricerche e misure sono dichiarate per ciò che sono. I metadati non attestati restano tali. Il mandato autorizza spike M1 in `tmp/` locale e fasi Linux M0..M9, senza consenso dedicato ricorrente. Un piano di prova non è una prova eseguita. | Tutte le fasi |
| NC-15 | Formule da letteratura e standard indipendenti | Le formule non derivano da codice o documentazione implementativa di concorrenti. La bibliografia va completata e verificata prima dell'implementazione. | M3 |
| NC-16 | Nessuna richiesta di foto private | I test usano dataset pubblici licenziati, sintetici o fixture create dal progetto. | M3 |
| NC-17 | Scratch utente configurabile | L'applicazione finale non impone il workspace dello sviluppatore. Cache e temporanei degli agenti restano locali al progetto; lo scratch applicativo è scelto dall'utente. | M2, M4 |
| NC-18 | Persistenza e cache con integrità verificabile | Scrittura atomica, verifica, fsync, rename sullo stesso filesystem, fsync della directory, poi commit nel database. Recovery e orfani gestiti. | M2 |
| NC-19 | Sicurezza dei parser, dei percorsi e dei ruleset | Limiti di dimensione e decompressione, niente eval di progetti, ruleset solo dati con allowlist di identificativi, protezione da symlink e hardlink in export, cleanup confinato, fuzz e sanitizer. | M2, M3 |
| NC-20 | Nessuna telemetria occulta e nessun upload | Le immagini non lasciano la macchina senza azione esplicita. I download sono opt-in. | M2, M5 |
| NC-21 | Divieto totale di fallback e di degradazione sostitutiva | Nessun percorso, algoritmo, backend, formato, risolutore, display o calibrazione sostituisce un altro in automatico. Un'indisponibilità, un errore o un gate fallito produce stop controllato, errore non nullo e diagnosi. Il resume prosegue solo con la stessa ricetta, lo stesso metodo, lo stesso backend e gli stessi parametri del checkpoint valido; ogni cambiamento apre una nuova ricetta e non è una prosecuzione. L'autorizzazione dell'utente non abilita fallback, nemmeno dichiarati. | Tutte le fasi; M2 core, M3 operatori, M4 UI, M5 release |

NC-21 riguarda il prodotto e i suoi dati: STOP del ramo selezionato, mai CPU per GPU o altro fallback, anche dichiarato. Non ferma lo sviluppatore: errori di build e test richiedono diagnosi, correzione autonoma e nuove prove secondo 21.13. Una ricetta primaria selezionata e dichiarata nel preflight, comprese le otto combinazioni DFB, resta legittima; non si cambia ricetta dopo il guasto.

### 4.1 Nota su NC-01 e sugli strumenti

NC-01 non impedisce compilatori, sandbox o agenti già configurati. Consente la ricognizione tecnica minima non segreta descritta in 0.1, senza richiesta rituale. Riconfigurare dentro il workspace gli output configurabili. Se servono permessi ulteriori non disponibili, registrare il limite reale e proseguire con le attività consentite; non modificare configurazioni di sistema per comodità.

NC-01 disciplina gli agenti di sviluppo, non l'app finale. L'app finale non eredita il divieto di uscire dal workspace: legge i percorsi che l'utente sceglie, applica NC-17 e tratta lo scratch come preferenza dell'utente. Le due regole non si espandono a vicenda.

---

## 5. Priorità, dipendenze e perimetro

### 5.1 Classi di priorità

- **P0:** necessario per il primo percorso deep sky realmente utilizzabile e per l'MVP. Include fondamenta, sicurezza dei dati, recovery, provenance, accessibilità, calibrazione flessibile, Advisor tradizionale, post-processing fotografico P0 e strategia bounded.
- **P1:** deep sky avanzato, qualità professionale, narrowband, LRGB, multi-sessione, SPCC completa, plate solving generale, maschere avanzate, denoise e deconvoluzione avanzati, Drizzle.
- **P2:** mosaici avanzati, planetario, Luna, Sole, comete, fotometria estesa, AI sofisticata e altre modalità, ciascuna con dataset, UX e gate propri.
- **R:** ricerca o decisione ancora aperta. Un elemento R non è approvato e non entra in implementazione senza esito documentato.

La priorità non è una promessa di calendario. È un ordine di dipendenza e di rischio.

### 5.2 Regole di priorità

1. Un requisito non sale di priorità perché è facile, né scende perché è difficile. Sale se serve a un percorso P0 dichiarato o se la sua assenza rende non verificabile un requisito P0.
2. Un requisito P1 può avere un sottoinsieme P0 quando serve a rendere utilizzabile o verificabile l'MVP. Il sottoinsieme va dichiarato come slice, non come promozione silenziosa dell'intero requisito.
3. Un requisito P1 non viene promosso automaticamente. La matrice 5.9 elenca slice, benefici, costi, dipendenze e rischi. Le promozioni D-19 e D-20 sono APPROVATE P0 e restano obbligatorie.
4. Nessun taglio di perimetro è giustificato da un calendario. Non si usano stime di tempo per ridurre test, accessibilità, recovery, precisione o configurazione risorse.
5. Un gate di qualità blocca le funzioni che dipendono da un algoritmo non validato. Non si costruisce una funzione avanzata su una formula che non ha superato il proprio contratto.
6. Una slice P0 non è una versione dimostrativa. Se non raggiunge il criterio dichiarato, la release non passa. Non si abbassa il criterio per chiudere M5.

### 5.3 Riprioritarizzazione rispetto alla specifica originale

| Requisito | Da | A | Motivo |
|---|---|---|---|
| F-IO-08 | P1 | P0 | File mancanti o spostati non devono corrompere il progetto già nel primo percorso. |
| F-IO-10 | P2 | P0 | L'MVP deve accettare input prestacked per la slice iniziale e per utenti con dati già calibrati. |
| F-PRJ-06 | P1 | P0 | Recovery dopo crash è parte del contratto non distruttivo. |
| F-PRJ-07 | P1 | P0 | Provenance e manifest sono necessari per export e riproducibilità dell'MVP. |
| F-CAL-06 | P1 | P0 | Pesi e varianza dalla calibrazione allo stack sono necessari per lo stacking pesato e per la qualità low N. |
| F-CAL-08 | P1 | P0 | Il debayer Bayer dopo calibrazione CFA è nel percorso MVP dichiarato. |
| F-UX-05 | P1 | P0 | Le tre modalità e l'onboarding sono richieste già nell'MVP. |
| F-UX-07 | P1 | P0 | L'accessibilità è P0 per decisione nuova. |
| F-POST-05 | P1 | P0 APPROVATO per `.a`, P1 per `.b` | Colore relativo approvato da D-20; SPCC completa con risposta spettrale resta P1, senza garanzia automatica di calibrazione fotometrica assoluta. |
| F-POST-06 | P1 | P0 per `.a`, P1 per `.b` | Asinh è uno stretch fotografico verificabile; GHS resta P1. |
| F-POST-08 | P1 | P0 per `.a`, P1 per `.b` | Le maschere sono infrastruttura del post-processing, non un extra. |
| F-POST-09 | P1 | P0 per `.a`, P1 per `.b` | Un denoise solido è necessario per la resa; le varianti avanzate restano P1. |
| F-POST-10 | P1 | P0 APPROVATO per `.a`, P1 per `.b` | La deconvoluzione a PSF fissa è approvata da D-19 per l'MVP; la versione generale resta P1. |
| F-POST-11 | P1 | P0 per `.a`, P1 per `.b` | Sharpening e contrasto locale fondamentali servono alla resa; HDR avanzato resta P1. |
| F-POST-12 | P1 | P0 per `.a` e `.b`, P1 per `.c` | Riduzione e separazione limitata sono valutate per l'MVP; la separazione generale e ML restano P1/M8. |
| F-REG-04 | P1 | P0 APPROVATO per `.a`, P1 per `.b` | Il solver assistito sblocca il colore relativo senza software esterno; il blind solving resta P1. |

Le altre priorità restano quelle originali, salvo i sottoinsiemi dichiarati nella sezione 5.5. Drizzle, SPCC completa, plate solving generale, maschere avanzate, denoise e deconvoluzione avanzati restano P1.

### 5.4 Che cosa contiene l'MVP

L'MVP è una tappa verticale, non il prodotto finale. Deve elaborare un percorso deep sky dichiarato fino all'export senza richiedere altri software. Il proprietario ha chiesto un MVP complesso e fotograficamente convincente. Non si riduce per pubblicare prima.

Contenuto obbligatorio, i dodici elementi della 2.0:

1. Import RAW multivendor e FITS mono, CFA e RGB, con metadati conservati. Il supporto è dichiarato per un insieme di campioni pubblici licenziati, non per ogni camera esistente.
2. Creazione di master bias, dark e flat, con dark-flat dove il dataset lo richiede.
3. Maschere di pixel invalidi, saturazione, bordi, copertura e bad pixel.
4. Debayer Bayer dopo calibrazione CFA, con pattern riconosciuto e mai applicato in modo silenziosamente sbagliato.
5. Metriche stelle, detection, centroidi, FWHM o HFD, eccentricità, SNR e scoring frame.
6. Registrazione geometrica con stelle corrispondenti, outlier rejection, trasformazione e residui.
7. Stacking pesato e robusto per N basso, con coverage, pixel invalidi e report di frame integrati e scartati.
8. Bilanciamento del fondo additivo, bilanciamento canali, stretch e curve.
9. Export FITS e TIFF con provenance.
10. Progetto salvabile, undo/redo, cache invalidabile e recovery dopo crash.
11. Tre modalità, Automatica, Guidata ed Esperta, sullo stesso motore.
12. Report per ogni fase critica, leggibili da un utente e sufficienti per la diagnosi.

L'addendum aggiunge due capacità P0 vincolanti dentro l'MVP:

13. Analisi intelligente tradizionale e assistenza attiva, con diagnosi, raccomandazioni motivate, confronto verificabile e ottimizzazione controllata, senza AI obbligatoria.
14. Calibrazione flessibile con tutte le otto combinazioni di dark, flat e bias, dark-flat come quarto asse e master precalibrati tipizzati.

Questa revisione aggiunge il post-processing fotografico P0, come slice dei requisiti P1 esistenti:

15. Motore maschere con luminanza, stelle, sfondo, oggetto, brush, poligono, morfologia, feather, invert, combine, opacity, mascheratura float e riproducibilità nel DAG.
16. Denoise à trous non decimato con soft shrinkage, rumore per scala, modelli distinti per lineare e non lineare, protezione del dettaglio e rollback.
17. Asinh analitico e curve, con casi limite separati e conservazione dei rapporti prima del gamut.
18. Sharpening multiscala bounded e contrasto locale, con guardie su rumore, aloni, ringing, stelle, saturazione e clipping.
19. Riduzione stellare controllata e separazione limitata con reintegrazione numerica dichiarata, senza promettere verità semantica.
20. Deconvoluzione a PSF fissa APPROVATA P0, su dato lineare, con regolarizzazione, maschere, negativi legittimi e limiti espliciti.
21. Colore relativo broadband APPROVATO P0, con pacchetto offline, stelle non sature, WCS valido e apertura compatibile.
22. Plate solving assistito APPROVATO P0, con dominio bounded, hint di posizione e scala, WCS rifittato e validazione su stelle escluse.
23. Advisor di strategia P0, con template tipizzati, registro operatori, ricerca bounded, confronto tra endpoint compatibili, holdout congelato e rollback.

I punti 15..23 non sono nuovi requisiti padre. Sono slice P0 obbligatorie dei requisiti esistenti o estensioni delle F-ANA. I punti 20, 21 e 22 sono approvati da D-19/D-20: un gate fallito blocca la funzione e M5, non provoca un declassamento a P1.

L'MVP non include drizzle, SPCC completa, plate solving generale, maschere avanzate, denoise e deconvoluzione avanzati, LRGB, narrowband, multi-sessione, mosaici, planetario, Luna, Sole, comete e AI. Queste funzioni restano nel prodotto e hanno milestone proprie.

### 5.5 Sottoinsiemi P0 e slice di requisiti formalmente P1

Questi sottoinsiemi non cambiano gli 82 ID genitori e non promuovono l'intero requisito. Ogni slice ha un ID di pianificazione definito in appendice D. Per gli aggregati P0 che comprendono core e UI, `.1` indica il core M3 e `.2` la UI M4. Le dipendenze complete e autoritative sono esclusivamente in appendice D; questa tabella ne riassume lo scopo.

| Requisito e slice | Priorità | Milestone | Dipendenze | Perché serve | Cosa resta P1 | Decisione richiesta |
|---|---|---|---|---|---|---|
| F-PRJ-05.a.1, branch reversibili core | P0 | M3.r | Appendice D, foglia F-PRJ-05.a.1 | L'Advisor propone varianti senza mutare il progetto. | F-PRJ-05.b, preset e batch | Nessuna. |
| F-PRJ-05.a.2, controlli branch | P0 | M4.b | Appendice D, foglia F-PRJ-05.a.2 | L'utente vede e annulla una variante. | F-PRJ-05.b | Nessuna. |
| F-POST-14.a, confronto prima e dopo | P0 | M4 | F-POST-01, F-ANA-03.a | Senza confronto verificabile l'Advisor non è controllabile. | F-POST-14.b, anteprime multiple avanzate | Nessuna. |
| F-UX-06.a, ROI e confronto | P0 | M4 | F-UX-01, F-UX-02, F-POST-01, F-POST-14.a | Serve a mostrare una regione e una variante senza editing distruttivo. Non dipende da F-POST-08. | F-UX-06.b, editing locale avanzato | Nessuna. |
| F-POST-01, primitiva display e STF | P0 | M4.a | F-PRJ-03, non F-UX-02 | F-UX-02 consuma questa primitiva; nessun ciclo e nessuna dipendenza della diagnostica headless. | - | Nessuna. |
| F-POST-02.a, curve e livelli core | P0 | M3 | F-POST-01 non richiesto per headless | Servono all'export e allo stretch. La GUI è M4. | F-POST-02.b GUI | Nessuna. |
| F-POST-03.a, background core | P0 | M3.i | F-POST-02.a, F-POST-08.a.1 | Il fondo richiede maschere core, non editing GUI. | F-POST-03.b GUI P0 M4 | Nessuna. |
| F-POST-04.a, bilanciamento canali core | P0 | M3 | F-POST-02.a | Il colore relativo e l'export richiedono una rappresentazione coerente. | F-POST-04.b GUI e profili avanzati | Nessuna. |
| F-POST-05.a, colore relativo | P0 APPROVATO | `.a.1` M3.q, `.a.2` M4.f | Appendice D, foglie F-POST-05.a.1 e F-POST-05.a.2 | Colore broadband offline; solver condizionale alla ricetta, WCS importato validato come percorso primario alternativo a priori. | F-POST-05.b, SPCC completa | D-20 APPROVATA. |
| F-POST-06.a, asinh | P0 | `.a.1` M3.k, `.a.2` M4.f | Appendice D, foglie F-POST-06.a.1 e F-POST-06.a.2 | Stretch verificabile e reversibile. | F-POST-06.b, GHS | Nessuna. |
| F-POST-08.a, maschere | P0 | `.a.1` M3.j, `.a.2` M4.e | Appendice D, foglie F-POST-08.a.1 e F-POST-08.a.2 | Generatori ed edit serializzati nel core; brush, poligono, morfologia e feather nella UI. | F-POST-08.b, segmentazione avanzata | Nessuna. |
| F-POST-09.a, denoise | P0 | `.a.1` M3.l, `.a.2` M4.f | Appendice D, foglie F-POST-09.a.1 e F-POST-09.a.2 | Modelli L/NL distinti e maschere core. | F-POST-09.b | Nessuna. |
| F-POST-10.a, deconvoluzione | P0 APPROVATO | `.a.1` M3.o, `.a.2` M4.f | Appendice D, foglie F-POST-10.a.1 e F-POST-10.a.2 | PSF fissa e prerequisiti scientifici prima del calcolo. | F-POST-10.b | D-19 APPROVATA. |
| F-POST-11.a, sharpening | P0 | `.a.1` M3.m, `.a.2` M4.f | Appendice D, foglie F-POST-11.a.1 e F-POST-11.a.2 | Contrasto con guardie di artefatto. | F-POST-11.b | Nessuna. |
| F-POST-12.a, riduzione stelle | P0 | `.a.1` M3.n, `.a.2` M4.f | Appendice D, foglie F-POST-12.a.1 e F-POST-12.a.2 | Fit sull'antenato lineare. | F-POST-12.c non AI; F-POST-15 ML | Nessuna. |
| F-POST-12.b, separazione limitata | P0 | `.b.1` M3.n, `.b.2` M4.f | Appendice D, foglie F-POST-12.b.1 e F-POST-12.b.2 | Reintegrazione numerica non prova correttezza astronomica. | F-POST-12.c | Nessuna. |
| F-REG-04.a, solver assistito | P0 APPROVATO | M3 core, M4 UI | F-REG-02, F-REG-01, ADR-08, ADR-10 | Sblocca WCS e colore senza software esterno. | F-REG-04.b, blind solving | D-20 APPROVATA. |
| F-ANA-01.a, diagnostica headless | P0 | M3.r | Appendice D, foglia F-ANA-01.a | Analisi RAW, light, stack e immagini non lineari senza viewer. | F-ANA-01.b GUI P0 M4 | Nessuna. |
| F-ANA-02.a, schede evidenza core | P0 | M3 | F-ANA-01.a, F-PRJ-07 | Le raccomandazioni devono essere oggetti del progetto. | F-ANA-02.b, presentazione GUI | Nessuna. |
| F-ANA-03.a, confronto core | P0 | M3.r | F-ANA-02.a, F-PRJ-05.a.1 | Il confronto tra varianti non richiede ROI UI. | F-ANA-03.b GUI P0 M4 | Nessuna. |
| F-ANA-04.a, planner strategia | P0 | M3.r | Appendice D, foglia F-ANA-04.a | Strategia P0 senza GUI; branch F-PRJ-05.a.1. | F-ANA-04.b GUI P0 M4; catalogo esteso M6 | Nessuna. |
| F-ANA-05.a, guardrail | P0 | M3 | F-ANA-01.a, NFR-TEST-01.a, NFR-TEST-02 | I test anti-regressione sono prerequisito, non conseguenza. | F-ANA-05.b, gate utente | Nessuna. |
| F-ANA-06, integrazione UX | P0 | M4 | F-ANA-02.b, F-ANA-03.b, F-ANA-04.b, F-UX-05 | Le tre modalità devono esporre la strategia. | - | Nessuna. |
| F-RES-05.a, budget separati | P0 | M2 | F-RES-01, F-RES-02 | RAM, cache, scratch e VRAM devono essere distinti già nel primo percorso. | F-RES-05.b | Nessuna. |
| F-RES-06.a, pressione e checkpoint | P0 | M2 | F-RES-03 | Il sistema deve fermare nuove ammissioni, creare checkpoint, fare spill o cancel sicuro senza ridurre risoluzione o qualità. | F-RES-06.b | Nessuna. |
| F-RES-07.a, ammissione e disco pieno | P0 | M2 | F-RES-01, F-RES-03 | Un job impossibile va rifiutato prima; un disco pieno non deve produrre output completo. | F-RES-07.b | Nessuna. |
| F-RES-08.a, worker e quota scratch | P0 | M2 | F-RES-02, F-RES-03 | Senza worker, quota e cleanup base l'MVP non è governabile. | F-RES-08.b | Nessuna. |

Regola: un requisito P1 non diventa P0 per intero perché una sua slice è P0. La slice ha criterio di accettazione proprio e non autorizza le funzioni avanzate.

### 5.6 Candidati P1 rivalutati

| Candidato | Decisione v2.2 | Motivo |
|---|---|---|
| Denoise classico | P0 per `.a` | La resa fotografica senza controllo del rumore non è credibile. Il metodo P0 è definito, non generico. |
| Maschere | P0 per `.a` | Sono prerequisito di denoise, sharpening, stelle, colore e Advisor. |
| Stretch asinh | P0 per `.a` | Formula analitica, reversibile e testabile. GHS resta P1. |
| Sharpening multiscala | P0 per `.a` | La versione bounded è controllabile; HDR avanzato resta P1. |
| Riduzione e separazione stelle | P0 per `.a` e `.b` | La riduzione PSF e la separazione limitata sono verificabili. La separazione generale resta P1. |
| Deconvoluzione | P0 APPROVATO per `.a` | PSF fissa, dominio lineare e gate scientifico obbligatorio secondo D-19. |
| Colore relativo | P0 APPROVATO per `.a` | Il contratto relativo è distinto dalla SPCC completa e non certifica un colore fisico senza risposta nota. |
| Solver assistito | P0 APPROVATO per `.a` | Senza WCS il colore relativo e l'astrometria richiedono software esterno. Il blind solving resta P1. |
| LRGB, narrowband, multi-sessione | P1 | Le fondamenta multicanale sono progettate ora, ma la prima release non le richiede. |
| GHS | P1 | Formula e comportamento cromatico richiedono contratto e test propri. |

### 5.7 Che cosa contiene il prodotto finale

Il prodotto finale copre deep sky broadband e narrowband, LRGB e composizioni multicanale, sessioni multiple, mosaici con sovrapposizione e distorsione, planetario, Luna, Sole, comete e asteroidi, workflow fotografico e workflow di misura separati, automazione, batch e API locale. Non è limitato a una singola sessione e non è limitato a un solo tipo di sensore.

Il prodotto finale può includere AI facoltativa, ma solo con modelli licenziati, provenance, test di artefatti e separazione dal percorso di misura. L'AI non è necessaria per le funzioni fondamentali.

### 5.8 Fuori perimetro

- Controllo hardware e acquisizione.
- Cloud obbligatorio, account obbligatorio, telemetria occulta.
- Pubblicazione Windows o macOS prima dell'autorizzazione.
- Addestramento di grandi modelli nel primo prototipo.
- Garanzia di supporto per ogni distribuzione Linux, ogni desktop, ogni GPU o ogni RAW storico.
- Promesse di accuratezza scientifica per output AI non validati.
- Promesse di ricostruire stelle, nuclei o nebulosità non presenti nei dati.

### 5.9 Matrice delle decisioni MVP, funzione per funzione

La matrice è il riferimento per i gate. "P0 esatto" indica cosa deve funzionare nella prima release. "P1" indica cosa resta fuori. "Impatto" copre GUI, DAG, RAM e Advisor. D-19 e D-20 sono APPROVATE P0.

| Funzione | P0 esatto | P1 o fuori MVP | Motivazione e vincoli | Criterio di accettazione | Impatto GUI, DAG, RAM, Advisor |
|---|---|---|---|---|---|
| Import FITS e RAW | FITS mono/CFA/RGB; RAW multivendor da campioni pubblici; metadati, pattern, black e white level dichiarati | XISF, TIFF 32, formati esotici | Il percorso non deve dipendere da una marca. Il decoder dichiara offset e crop. | Roundtrip e import senza debayer prematuro; mismatch non silenzioso | GUI import M4; nodi decoder nel DAG; streaming e preview; Advisor usa metadati e stato |
| Calibrazione | 8 combinazioni, dark-flat, master tipizzati, omissione motivata | Dark scaling avanzato e master library P1 | La calibrazione parziale è meglio di una calibrazione falsa. | Test E2E delle 8 combinazioni; nessuna doppia sottrazione | GUI matrice M4; DAG per correzione; RAM per master; Advisor diagnostica master |
| Debayer | Bayer dopo calibrazione, pattern riconosciuto, passthrough dichiarato | X-Trans completo | Un pattern sbagliato è peggio di un rifiuto. | Test aliasing, colore, pattern | GUI M4; nodo DAG; RAM streaming; Advisor segnala pattern ignoto |
| Stelle e qualità | Detection, centroide, FWHM/HFD, eccentricità, SNR, scoring | Deblending avanzato e modelli PSF complessi | La detection non richiede tutti i master; le metriche radiometriche dichiarano i limiti. | Ground truth sintetico e frame pubblici | GUI M4; nodo metriche; RAM ROI; Advisor usa evidenze e confidenza |
| Registrazione e resampling | Match, outlier, trasformazione, `R`, varianza, coverage, WCS | Drizzle e modelli distorti avanzati | La varianza non si interpola; il WCS non è una mappa di pesi. | Trasformazioni note e propagazione di covarianza | GUI M4; nodo warp; RAM tile e halo; Advisor confronta residui |
| Stacking | Pesi, N basso, rejection solo se N basta, coverage, Neff | Drizzle, multi-sessione, mosaic blending | N=1 e N=2 non permettono rejection statistica. | SNR, bias, coverage, casi limite | GUI report M4; DAG stack; RAM streaming; Advisor usa Neff e scarti |
| Background e WB | Background additivo/divisivo, protezione nebulosa, astensione; bilanciamento canali core | Modelli multiscala avanzati | Fondo e segnale diffuso possono essere non separabili. | Sintetici con gradiente e nebulosa | GUI M4; nodo bkg; RAM fit su campioni; Advisor astensione |
| Curve e asinh | Curve, livelli, asinh analitico, casi limite, reversibilità | GHS, curve artistiche avanzate | Lo stretch è display o fotografia, non misura. | Monotonicità, clipping, colore, roundtrip | GUI M4; nodo LUT; RAM bassa; Advisor confronta endpoint |
| Maschere | Luminanza, stelle, sfondo, oggetto; brush, poligono, morfologia, feather, invert, combine, opacity; float 0..1; DAG | Segmentazione avanzata, colore selettivo, ML | La maschera non è alpha né invalidità. L'asset utente è autorevole. | Editing riproducibile, invalidazione, salvataggio, test bordi | GUI editor M4; DAG con revisione; RAM raster; Advisor usa maschere per ROI |
| Denoise | À trous non decimato, soft shrinkage, rumore per scala, modelli lineare e non lineare distinti | Metodi avanzati, ML | Non si copia la varianza; il rumore non lineare è stimabile con modello e simulazioni. | Riduzione NPS, bias, dettaglio, stelle, rollback | GUI M4; DAG per scala; RAM piramidale; Advisor valuta oversmooth |
| Sharpening | Multiscala bounded, contrasto locale, guardie clipping/aloni/ringing/stelle | HDR avanzato e deconvolution sharpening | Il dettaglio falso è un artefatto, non un miglioramento. | Metriche di alone, ringing, clipping, confronto | GUI M4; DAG multiscala; RAM buffer per scala; Advisor confronta |
| Riduzione stelle | Riduzione PSF frazionaria, solo sorgenti identificabili; fit locale di fondo e PSF | Riduzione general, aloni complessi | Non si inventano stelle; la riduzione è fotografica, non misura. | Ricostruzione numerica, artefatti, astensione crowding/saturazione | GUI M4; DAG maschera stelle; RAM fit locale; Advisor spiega limiti |
| Separazione limitata | Componente modello `S_L` sull'antenato lineare, `R_L = I_L - S_L`, reintegrazione neutra `R_L + S_L = I_L` con la stessa `S_L` | Separazione generale, starless, ML | L'identità vale solo con la stessa componente, non con una `S_L'` editata. | Test di coerenza, reintegrazione, artefatti | GUI M4; DAG due rami; RAM doppio buffer; Advisor dichiara non verità |
| Deconvoluzione | PSF fissa, minimi quadrati regolarizzati, dato lineare, maschere, negativi legittimi | Blind, PSF variabile, RL avanzato | La semplicità non è una scusa per un clamp silenzioso. | Esistenza, unicità condizionata, ringing, rumore, PSF errata | GUI M4; DAG con FFT; RAM preflight; Advisor stima dominio |
| Colore relativo | Broadband, pack offline, stelle non sature, WCS, apertura, risposta dichiarata | SPCC completa, colore narrowband | Il colore relativo non è una calibrazione fisica assoluta. | Holdout, outlier, moto proprio, bande compatibili | GUI M4; DAG colore; RAM pack; Advisor mostra residui |
| Solver assistito | Dominio bounded, hint posizione/scala, match robusto, WCS rifittato, validazione | Blind solving, modelli completi | Senza hint il problema è troppo ampio per un P0. | Stelle tenute fuori, ambiguità, out of domain | GUI M4; DAG solve; RAM indice; Advisor segnala fallimento |
| Advisor diagnostica | 9 famiglie, evidenze, confidenza, astensione, no-op | Apprendimento e modelli generativi | La diagnosi non è una correzione certa. | FP/FN, copertura, calibrazione confidenza | GUI schede M4; DAG report; RAM bounded; è il motore stesso |
| Advisor strategia | Template, registro, ruleset, ricerca bounded, 5 stati, confronto, rollback | Estensioni a più operatori e P1 | La strategia non è una permutazione libera. | Test di illegalità, budget, holdout, provenance | GUI M4; branch DAG; RAM preflight; è il motore stesso |
| Export | FITS/TIFF con provenance, manifest, colore dichiarato, nessun STF del monitor | Formati avanzati | Ciò che il formato non rappresenta va nel sidecar. | Roundtrip, header, manifest, hash | GUI M4; nodo writer; RAM streaming; Advisor verifica limiti |
| Progetto e recovery | Salvataggio, riapertura, undo, redo, cache, crash recovery, asset utente durevole | Preset condivisi, batch | La cache non è la storia; l'asset utente non è cache. | Crash test, roundtrip, invalidazione | GUI M4; DAG e DB; RAM cache; Advisor usa revisioni |
| Tre modalità | Automatica, Guidata, Esperta sullo stesso motore | Personalizzazioni avanzate | Cambia l'esposizione, non il risultato. | Uguaglianza a parità di parametri | GUI M4; nessun ramo di codice separato; Advisor con 3 livelli |
| Accessibilità | Tastiera, focus, temi, HiDPI, colore non unico, testi alternativi | Localizzazione estesa | È P0, non un rifinitura. | Checklist e test manuali | GUI M4; nessun impatto DAG; Advisor accessibile |
| Risorse | Auto/Manuale, preflight, spill, checkpoint, worker, quota | Tuning avanzato e VRAM | Il pool gestito non è l'RSS. | Test multi-budget, disco pieno, cancel | GUI M4; scheduler DAG; RAM/scratch; Advisor rispetta budget |
| Linux e packaging | Matrice L1-L3, una modalità packaging, CPU-only, offline | Altre famiglie e packaging | Nessuna promessa senza prova. | Installazione, avvio offline, X11/Wayland | GUI M5; nessun impatto DAG; Advisor offline |
| AI | Nessun requisito | M8, opt-in | L'AI non è necessaria e non è misura. | Provenance, artefatti, separazione dalla misura | GUI M8; DAG separato; RAM/VRAM; Advisor non la richiede |

Deconvoluzione, colore relativo e solver assistito sono APPROVATI P0. Se un gate fallisce, M5 resta bloccata; lo sviluppatore corregge e ripete le prove senza ridurre il perimetro o i criteri. Prosegue intanto sulle attività indipendenti.

---

## 6. Mercato, concorrenza e posizionamento misurabile

### 6.1 Matrice dei concorrenti

La matrice è una base di confronto, non una fotografia esaustiva. Molte capacità dipendono da versioni, moduli, plugin o installazioni aggiuntive. Le versioni sono quelle dichiarate nella 2.0 e non sono state riconfermate in questa revisione. Dove il dato non è certo, la cella lo dichiara. Non si trasforma questa tabella in pubblicità comparativa e non si attribuiscono mancanze non verificate.

| Prodotto | Versione e data dichiarate | Piattaforma e licenza | Area di forza da studiare | Verifica aperta | Fonte dichiarata |
|---|---|---|---|---|---|
| Siril | 1.4.4, pagina di download consultata il 25 ottobre 2026 | Linux, Windows, macOS; open source | Preprocessing e post-processing deep sky, stacking, astrometria, SPCC, Drizzle, mosaici, script | Le funzioni citate vanno legate alla versione esatta. Solo confronto esterno, mai fonte implementativa. | https://siril.org/download/ |
| PixInsight | 1.9.4, testo della pagina requisiti; build non verificata | Linux, Windows, macOS; commerciale | Processi modulari, multiscala, maschere, storia, ecosistema di script e plugin | Requisiti e build vanno verificati sul sistema di prova. | https://pixinsight.com/sysreq/ |
| Astro Pixel Processor | Licenza owner, beta 48 dichiarata | Linux, Windows, macOS; commerciale | Mosaici, normalizzazione locale, integrazione multi-sessione, blending multibanda | La beta e le funzioni esatte vanno verificate. | https://www.astropixelprocessor.com/product/astro-pixel-processor-owner-license/ |
| DeepSkyStacker | 6.2.2, release Linux dichiarata | Linux, Windows; gratuito | Preprocessing e integrazione deep sky orientati all'accessibilità | La release Linux va provata. | https://github.com/deepskystacker/DSS/releases/tag/6.2.2 |
| GraXpert | 3.0.2 | Linux, Windows, macOS; open source | Rimozione gradienti, metodi tradizionali e AI | Modelli e licenze vanno verificati. | https://github.com/Steffenhir/GraXpert/releases/tag/3.0.2 |
| StarNet | CLI 2.6.2, DeepSNR 1.3.2 | Multipiattaforma; licenza da verificare | Separazione stelle e sfondo | CLI e pesi vanno verificati. | https://starnetastro.com/ |
| RC Astro tools | CLI, build non verificata | Multipiattaforma; commerciale | Operazioni specializzate basate su ML | Build, licenza e modelli non verificati. | https://www.rc-astro.com/stand-alone-rc-astro-tools/ |
| AstroImageJ, Fiji, ImageJ | DA VERIFICARE | Multipiattaforma; licenze varie | Analisi quantitativa, fotometria, misurazioni | Scope e versioni da verificare. | https://www.astro.louisville.edu/software/astroimagej/ |
| AutoStakkert! | DA VERIFICARE | Windows; licenza da verificare | Lucky imaging Luna, Sole, pianeti | Piattaforma e versione da verificare. | https://www.autostakkert.com/ |
| RegiStax, WaveSharp | DA VERIFICARE | Windows; licenza da verificare | Sharpening multiscala e wavelet | Versioni e supporto da verificare. | Fonti ufficiali da completare |
| AstroSurface | DA VERIFICARE | Windows; licenza da verificare | Operazioni locali, deconvoluzione, wavelet | Versioni e compatibilità da verificare. | https://astrosurface.com/pageuk.html |
| PIPP | DA VERIFICARE | Windows; licenza da verificare | Preprocessing video planetario | Versione e codec da verificare. | Fonte ufficiale da completare |
| ImPPG | DA VERIFICARE | Multipiattaforma; licenza da verificare | Sharpening e allineamento solare e lunare | Versione e risultati da verificare. | Fonte ufficiale da completare |
| GIMP | 3.2.6, annuncio del 10 settembre 2026 | Linux, Windows, macOS; GPL | Rifinitura fotografica, maschere, curve, colore | Non è un tool astronomico. Il confronto riguarda la rifinitura finale. | https://www.gimp.org/news/2026/09/10/gimp-3-2-6-released/ |
| Affinity Photo, RawTherapee | DA VERIFICARE | Multipiattaforma; licenze varie | Rifinitura, maschere, curve, gestione colore | Versioni e licenze da verificare. | https://www.rawtherapee.com/ |

La tabella non dimostra che un concorrente manchi di una funzione. Serve a evitare falsi gap. La nota originale su Siril 1.4.0 citava SPCC, Drizzle, correzione distorsione SIP e WCS, mosaici, maschere, blending, ICC e scripting Python. Sono capacità da riconoscere, non da negare.

### 6.2 Assi di competitività

| Asse | Cosa si misura | Dataset o prova | Baseline esterna |
|---|---|---|---|
| Ampiezza import | Formati e metadati accettati senza perdita | RAW multivendor, FITS mono, CFA, RGB, prestacked | Siril, PixInsight, APP, DSS |
| Correttezza calibrazione | Bias residuo, rumore, artefatti, casi limite, otto combinazioni | Sintetici con ground truth e campioni pubblici | Siril, PixInsight, DSS |
| Registrazione | Errore residuo, outlier, stabilità su campo debole | Trasformazioni sintetiche e campi reali pubblici | Siril, PixInsight, APP |
| Stacking low N | SNR, copertura, rifiuto, assenza di bordi neri, varianza | N piccolo, frame con nuvole, saturazione, cosmici | DSS, Siril, APP |
| Post-processing fotografico | Riduzione rumore, dettaglio, aloni, colore, stelle, artefatti | Sintetici con ground truth, immagini pubbliche, endpoint dichiarati | PixInsight, Siril, APP, tool di rifinitura |
| Strategia Advisor | Legalità delle sequenze, qualità del confronto, overfitting, costo | Batch di candidati, holdout congelato, casi illegali | Nessuna baseline unica, protocollo del prodotto |
| Memoria e I/O | Picco RSS, spill, tempo, comportamento oltre RAM | Dataset più grande del budget | Nessuna baseline unica, misura del prodotto |
| Viewer | Latenza pan e zoom, memoria, precisione display | Immagini grandi, tile, HiDPI, X11 e Wayland | PixInsight, Siril, APP |
| Colore e provenance | Coerenza lineare, ICC, tracciabilità export | Immagini pubbliche e sintetiche | PixInsight, Siril |
| Analisi e assistenza | Accuratezza diagnostica, falsi positivi e negativi, utilità del confronto | Sintetici con difetti noti, campioni pubblici, out of distribution | Nessuna baseline unica, protocollo del prodotto |
| UX | Tempo al primo risultato, errori, passaggi | Test con principianti ed esperti | Concorrenti selezionati, stesso dataset |
| Linux | Installazione, avvio offline, desktop, packaging | Matrice M5 | Distribuzioni, non un solo competitor |
| Offline e privacy | Funzionamento senza rete, nessun upload | Prova in rete isolata | Requisito del prodotto |

Un asse può essere migliore senza che il prodotto sia migliore in tutto. La comunicazione deve seguire questa regola.

### 6.3 Ipotesi di gap da testare

- Un workflow end-to-end che non obbliga a esportare verso strumenti esterni per il percorso dichiarato.
- Un progetto unico e una storia non distruttiva dal RAW alla finalizzazione, con invalidazione selettiva.
- Risorse configurabili e monitorabili, con comportamento prevedibile oltre la RAM disponibile.
- Tre modalità coerenti sopra lo stesso motore, non tre prodotti diversi.
- Post-processing fotografico P0 con artefatti misurati, non solo strumenti presenti.
- Una strategia Advisor spiegabile e bounded, non una ricerca combinatoria mascherata da automazione.
- Qualità scientifica spiegata e misurata, non solo dichiarata.
- Uso AI esplicito, opzionale e separato dalla misura.
- Pipeline riprendibile e confrontabile, con report leggibili.
- Analisi diagnostica che dichiara l'incertezza e sa astenersi.
- Calibrazione che funziona con i master realmente disponibili e dichiara i limiti.
- Integrazione Linux curata, non un porting minimo.

### 6.4 Protocollo di confronto

Ogni confronto esterno usa gli stessi input, la stessa configurazione dichiarata, le stesse risorse e la stessa precisione di output, per quanto la controparte lo consenta. Si confrontano metriche oggettive e, quando utile, una valutazione umana del flusso. Non si legge il codice sorgente del concorrente per dedurre l'implementazione. Non si dichiara un confronto se non è legalmente e tecnicamente possibile. Versioni, parametri, hash e condizioni vanno registrati.

Le verifiche web della 2.0 restano etichettate come storiche. Non sono state rifatte in questa revisione e non diventano misure attuali.

---

## 7. Architettura

### 7.1 Principi

Il core scientifico è headless e indipendente dalla GUI. La GUI è un client del core, non il luogo in cui vivono gli algoritmi. Un test headless deve poter eseguire la stessa pipeline senza aprire finestre. Questo vale anche per batch, automazione e diagnostica.

Il core non contiene codice, chiamate o derivazioni da Siril. Le formule sono modelli indipendenti di fisica del sensore, statistica e geometria. Le librerie general purpose sono ammesse per I/O, algebra, FFT, compressione, GUI e ML, con licenze verificate.

L'architettura deve rendere difficile perdere dati, pubblicare un risultato stale o superare il budget di memoria senza accorgersene. La sicurezza dei dati è un requisito di progetto, non un controllo aggiunto alla fine.

Il core non usa tipi Qt o tipi di un toolkit GUI nelle proprie interfacce. UI e CLI chiamano gli stessi application service. Non esiste un quarto motore per l'Advisor, per la strategia, per il confronto o per l'anteprima.

### 7.2 Moduli e responsabilità

```text
[GUI Linux: viewer, inspector, project editor, monitor, advisor, strategy]
[CLI e headless: batch, test, report, diagnostica]
                 |
        API tipizzata / command bus
                 |
[Application services: sessioni, job, preferenze, report, advisor, planner]
                 |
[Pipeline: DAG immutabile, registry operatori, scheduler, cache, provenance]
        /          |             |            \
[I/O RAW/FITS] [operatori] [metadati/WCS] [qualità, stelle, analisi]
        \          |             |            /
[Immagine tipizzata, maschere, pesi, varianza, coverage, asset utente]
                 |
[Backend CPU: scalare, SIMD dispatch] [Backend GPU opzionale]
                 |
[Cache disco derivata] [SQLite autorità proposta] [Export]
```

GUI e CLI headless sono due client dello stesso core e degli stessi application service. La figura descrive responsabilità, non obbliga a microservizi, plugin o a un linguaggio specifico. Preferisce un'applicazione locale semplice, con confini di modulo chiari e API stabili.

### 7.3 Immagine tipizzata

L'immagine non è un array con qualche stringa nei metadati. È un contratto tipizzato che dichiara almeno:

- unità fisiche o ADU, con fattore di conversione quando noto;
- dominio CFA, mono, RGB o multicanale;
- pattern Bayer o X-Trans, con offset e rotazione;
- geometria, origine, orientamento, pixel scale e WCS quando presente;
- precisione numerica, cioè f32 o f64, e politica di promozione;
- linearità, cioè lineare, non lineare o sconosciuta;
- maschere di pixel invalidi, saturazione, bordi e bad pixel;
- coverage, cioè la frazione geometrica di area del pixel di output coperta da campioni validi di input, distinta dal conteggio dei campioni, dalla somma dei pesi firmati e dal canale alpha;
- varianza o peso, con provenienza e approssimazione dichiarata;
- capability, cioè cosa l'immagine può supportare: misura, display, CFA, calibrazione, stacking, analisi;
- provenance, cioè nodo, input, parametri, versione algoritmo, backend e data.

Quattro grandezze non vanno confuse. Il conteggio dice quanti campioni contribuiscono. La coverage dice quanta area geometrica è coperta. La somma dei pesi firmati misura il guadagno del kernel e può valere uno con coverage inferiore a uno. Alpha è un canale di composizione o trasparenza per il display, non una maschera scientifica. Un pixel senza campioni ha coverage zero ed è invalido, non zero. `Neff`, definito nella sezione 8.8, misura l'efficienza dei pesi e non è una coverage.

Le maschere di editing sono un tipo separato. Una maschera di editing ha valori float in `[0, 1]`, semantica dichiarata, feather e combinazione. Può essere usata per pesare un effetto fotografico, non per dichiarare un pixel scientificamente valido. Una maschera di invalidità è booleana o a copertura, e la sua perdita non si compensa con alpha.

Non esiste un booleano `scientific_valid`. La validità scientifica è una proprietà contestuale, fatta di unità, calibrazione, incertezza, maschere e provenance. Un flag booleano nasconderebbe proprio le informazioni che servono. La capability `measure_ready` è ammessa solo se accompagnata dai motivi e dalle limitazioni.

La precisione di lavoro è f32 come candidato per il volume dei pixel. f64 va usato negli operatori che amplificano il condizionamento, per esempio normalizzazioni, riduzioni, accumuli lunghi, fitting, deconvoluzione, propagazione di covarianza e stime di rumore. La scelta va motivata per operatore, non imposta globalmente. f32 non è automaticamente sufficiente solo perché il dato sorgente è a 16 bit: la sottrazione di due valori grandi può perdere precisione prima che il risultato venga memorizzato.

### 7.4 DAG immutabile, hash e invalidazione

La pipeline è un DAG immutabile. Ogni nodo ha input, output, dipendenze, parametri canonici, versione algoritmo, decoder, precisione, backend numerico, seed e provenance. La chiave di cache include:

- hash del contenuto degli input;
- metadati rilevanti, per esempio CFA, geometria, unità, WCS e maschere;
- parametri canonici, con ordinamento e serializzazione stabile;
- versione dell'algoritmo;
- versione del decoder, quando il nodo legge un RAW;
- precisione numerica;
- backend numerico effettivamente eseguito e seed, quando il risultato può variare;
- versione dello schema dei dati e versione della serializzazione dell'operatore; il solo numero di schema del database non invalida ogni operatore, perché non cambia il contratto numerico.

Un cambio di parametro, metadato, maschera, versione algoritmo, decoder, precisione o backend effettivo invalida il nodo e tutti i suoi discendenti. Un nodo a monte indipendente e non toccato conserva la cache valida. Un cambio di solo display, per esempio lo stretch di anteprima, non deve rifare master, calibrazione o stacking.

Un cambio del solo schema del database non invalida un operatore se il contratto numerico e la serializzazione rilevante non cambiano. La regola è: invalida ciò che può cambiare il risultato, non ogni versione del contenitore. Per lo stesso motivo la chiave registra il backend effettivamente eseguito, non quello richiesto. Se la GPU fallisce, il job si ferma con errore non nullo e diagnosi. Non riparte su CPU e non crea un nuovo job CPU come sostituzione automatica. Un job CPU è una ricetta nuova, scelta dall'utente prima dell'avvio.

Le maschere derivate hanno una dipendenza esplicita dal nodo sorgente e dalla revisione. Una maschera dinamica non può dipendere da se stessa, né da un nodo che la consuma. Il grafo rifiuta il ciclo alla costruzione e alla pubblicazione. Una maschera dipinta o importata è un asset autorevole: non è una cache, non si elimina per eviction e non si ricostruisce da una ricetta se il raster non esiste.

L'undo e le revisioni non sono una cache. Una revisione ripristina uno stato logico del progetto; la cache può essere ricostruita o eliminata senza perdere la storia. Confondere le due cose produce progetti che sembrano reversibili ma non lo sono.

Ogni esecuzione porta un token di generazione. Un risultato prodotto con un token vecchio non può essere pubblicato come corrente. Questo impedisce che un job lento sovrascriva un risultato più recente. La pubblicazione è atomica e verificata.

L'identità di un input è il digest completo del contenuto, più la dimensione. Percorso e mtime sono indizi, non identità. Un file modificato con la stessa dimensione e lo stesso mtime deve essere rilevato dal digest. Non ci si fida di un `stat` modificato.

Il digest completo si calcola all'import e si persiste nella stessa transazione che registra l'input e la dimensione. Alla riapertura, un input non ancora verificato in quella sessione è marcato `NON VERIFICATO` prima del primo uso numerico. Il digest viene ricalcolato prima dell'esecuzione o della pubblicazione, oppure si usa uno snapshot immutabile già verificato. Un digest parziale non è una prova: può servire alla prima esplorazione, non alla pubblicazione.

Se il file cambia durante l'hash o durante il decode, il job si ferma. Il digest non si lega a byte diversi da quelli usati. La policy di lettura è dichiarata: snapshot verificato oppure lettura consistente con rilevamento della modifica. Un file spostato si ricollega solo dopo verifica del digest. Un file mancante blocca il ramo che ne dipende. Una cache corrotta viene invalidata, non riparata in silenzio.

### 7.5 Persistenza, SQLite, asset utente e cache

La proposta è usare SQLite come sola autorità per progetto, nodi, parametri, storia, riferimenti ai file, stato dei job, asset utente e provenance. JSON può essere usato per export, interoperabilità o formati di scambio, non come seconda autorità concorrente. La proposta non è approvata: ADR-04 deve confrontare SQLite, JSON e un modello ibrido.

Il protocollo di scrittura dei file derivati è questo:

1. Scrivere su un file temporaneo esclusivo nella stessa directory o sullo stesso filesystem dell'output.
2. Verificare la scrittura e i byte o l'hash attesi.
3. Eseguire fsync sul file.
4. Rinominare atomicamente sullo stesso filesystem.
5. Eseguire fsync sulla directory.
6. Solo dopo, confermare il record nel database con una transazione durevole.

Se il processo muore prima del commit, il file temporaneo o l'output non confermato viene trattato come orfano. La recovery lo rileva, lo ignora o lo elimina secondo una politica dichiarata. Un output parziale non è mai marcato completato. Un errore di fsync non viene ignorato: il job fallisce e lascia lo stato coerente.

Gli asset utente, cioè maschere dipinte, maschere importate, sorgenti o riferimenti necessari a ricostruire un intervento, seguono una regola diversa dalla cache. Sono autorevoli. Si salvano con un raster versionato oppure con un event log durevole che permette di ricostruire il raster. Il record nel database viene confermato solo dopo che il dato durevole è stato scritto e verificato. Non si elimina un asset utente per fare spazio alla cache. Se l'asset manca, il nodo dipendente è invalido e il progetto lo dichiara.

La garbage collection non pinna per sempre ogni artefatto di ogni revisione. I metadati, i parametri e le ricette si conservano: sono piccoli e servono alla storia. La cache derivata è eliminabile, altrimenti la quota non è efficace e il disco cresce senza limite. Restano pinnati solo gli oggetti con un lease attivo: job in corso, lettori, checkpoint dichiarati. Un asset di input o un asset utente non ricostruibile non è cache e non entra nella politica di eviction. Un oggetto referenziato da una revisione si ricostruisce dalla ricetta se la cache è stata eliminata: la ricostruzione è il criterio, non il pin permanente. Cancellare la cache non deve perdere la ricostruibilità né la storia.

Le migrazioni di schema hanno una copia di sicurezza prima della modifica. Una migrazione fallita lascia il progetto apribile nella versione precedente o in uno stato di sola lettura con messaggio chiaro. Uno schema sconosciuto più recente si apre in sola lettura e si rifiuta di sovrascriverlo. Le versioni future non devono rompere progetti vecchi senza una procedura di upgrade.

L'export di più file non è una singola rename atomica. Si scrive un manifest con hash di ogni output e un marker di commit, in una transazione o in una sequenza verificabile. Se il manifest manca, il bundle è incompleto. Non si dichiara atomico ciò che non lo è.

Un bundle importato con un manifest di hash entra in un nuovo progetto oppure in un fork esplicito. Non si importa un manifest dentro un progetto esistente come seconda autorità: il manifest descrive il bundle, non sostituisce il database del progetto. Se il bundle è incompleto, manca il marker o un hash non corrisponde, l'import si ferma e non pubblica un progetto parziale.

### 7.6 Scheduler, memoria e spill

Lo scheduler lavora con prenotazioni di memoria. Prima di avviare un job stima il working set, chiede una prenotazione e attende se il budget non è disponibile. Le code sono bounded. La backpressure è esplicita: chi produce troppo velocemente aspetta, non accumula senza limite.

Le politiche includono eviction, spill su disco con quota, priorità, controllo del parallelismo annidato e prevenzione dei deadlock. La prenotazione può essere atomica per l'insieme RAM, scratch e VRAM, oppure seguire un ordine stabile ed esplicito. Un job non deve poter prenotare memoria mentre aspetta una risorsa detenuta da un altro job che aspetta la sua. Le prenotazioni hanno timeout e diagnostica, non attese infinite. Nessuna attesa di risorsa avviene mentre è aperta una transazione di scrittura SQLite o è detenuto un lock della garbage collection. I worker vengono creati solo dopo che le risorse sono state ammesse, non prima. Se una stima del working set minimo è incerta, il sistema lo dichiara; se il job è infeasible, lo rifiuta prima di avviarlo.

Il limite hard riguarda il pool gestito dal motore, non l'RSS totale del processo. L'RSS è best effort e va misurato, perché allocator, mmap, librerie terze e driver GPU possono allocare fuori dal pool. Il prodotto non deve dichiarare un limite hard dove non può mantenerlo. Deve mostrare la differenza tra memoria gestita, RSS, cache, scratch e VRAM.

La modalità Automatica usa segnali di pressione disponibili su Linux, per esempio PSI e cgroup quando presenti, oltre alla memoria disponibile e alla capacità dei thread. Non usa margini fissi del 70 per cento, timeout arbitrari o costanti universali. Un margine si misura e si motiva. Se il segnale di pressione non è disponibile, la stima è conservativa e dichiarata.

La modalità Manuale accetta MiB, GiB o percentuale. La percentuale ha un denominatore esplicito: capacità fisica o capacità effettiva, con snapshot al momento della scelta. La percentuale si congela in byte, non in un valore di RAM libera che cambia sotto i piedi. I valori si mostrano anche in byte. Le ambiguità di conversione sono zero. Se la capacità effettiva del cgroup cambia, vale la politica di riduzione descritta sotto: stop delle nuove ammissioni, checkpoint, spill o cancel sicuro, mai revoca di un buffer attivo.

Il viewer ha un budget separato e bounded: piramide con numero di livelli dichiarato, tile visibili e job di costruzione dell'istogramma contabilizzati. Il decoder può aver bisogno di un full frame fuori dal pool gestito. In quel caso lo scheduler fa un preflight e dichiara il minimo fattibile. Se il budget è sotto il minimo, il job viene rifiutato con un messaggio chiaro, non avviato per poi fallire a metà. Limiti a processi esterni o a un decoder tramite cgroup sono possibili solo se la piattaforma lo consente e la policy lo autorizza; non sono una promessa universale.

Quando il budget si riduce, il sistema smette di ammettere nuovi job, crea checkpoint, fa spill o cancella in modo sicuro. Non riduce risoluzione, candidati o qualità per rientrare nel budget. Non revoca un buffer attivo a un job in corso. Il disco pieno ferma il job in modo controllato, i file parziali vengono rimossi o marcati, e il progetto resta apribile. Un output incompleto non viene pubblicato come completo.

### 7.7 I/O, scratch e filesystem

L'import non carica obbligatoriamente tutti i pixel. Previews, inventario e metadati usano letture mirate e cache derivate. I file originali non vengono sovrascritti. L'export è esplicito e protetto da path che tentano di uscire dalla destinazione, da symlink e da hardlink non voluti.

I percorsi con spazi, caratteri non ASCII, permessi variabili e dischi rimovibili fanno parte dei test. Un file mancante o spostato non corrompe il progetto: viene marcato, può essere ricollegato, e i nodi dipendenti restano bloccati finché la sorgente non torna. I file prestacked sono accettati con la fase di processo dichiarata.

Lo scratch ha quota, pulizia e diagnostica. Su disco pieno il job si ferma in modo controllato. Il cleanup è confinato alle directory dichiarate con le protezioni contro race e symlink della sezione 19.2; verificare soltanto un path risolto non basta. Non si cancellano file fuori dallo scratch o dalla cache del progetto. Lo scratch dell'applicazione è scelto dall'utente; il workspace dello sviluppatore non è un default.

### 7.8 GUI e viewer

Il viewer è un componente critico, non un dettaglio del toolkit. Deve gestire immagini grandi con tile visibili e piramidi bounded, cioè con un numero di livelli e una memoria massima dichiarati. Il pan e lo zoom devono restare reattivi senza caricare l'intera immagine in RAM.

Il rendering usa un percorso software sulla CPU per tutte le funzioni fondamentali. La GPU può accelerare il display, ma non è necessaria. L'istogramma è asincrono e non blocca la UI. La precisione del display è separata dalla precisione dei dati: la trasformazione ICC o sRGB serve a mostrare, non a modificare i pixel scientifici. La differenza tra preview e dato reale è sempre visibile.

La UI non contiene logica scientifica. Comunica con il core tramite API tipizzata o command bus. Le operazioni lunghe sono cancellabili o mettibili in pausa dove è tecnicamente sicuro, con feedback di fase e motivi di errore azionabili. Un annullamento non lascia il progetto in uno stato ambiguo.

Le tre modalità e l'Advisor non creano pipeline separate. Le viste indipendenti previste per l'Advisor, cioè elenco delle raccomandazioni, confronto, spiegazione e scheda di strategia, leggono lo stesso DAG e gli stessi report. Non esiste una quarta pipeline.

### 7.9 CPU, SIMD e GPU

La CPU è il backend di riferimento. Le funzioni fondamentali devono produrre risultati corretti senza GPU, incluso il viewer software e la STF delle tile visibili. Il parallelismo è configurabile e controllato, con attenzione al parallelismo annidato tra decoder, operatori e scheduler.

SIMD usa dispatch nel preflight. Il binario seleziona il percorso SIMD disponibile oppure il percorso scalare: entrambi sono primari, dichiarati e misurati. Nessun percorso sostituisce l'altro a runtime. Non si impone AVX2 come requisito universale. Le differenze numeriche tra percorsi SIMD e scalare sono coperte da tolleranze e test.

La GPU è opzionale e supporta AMD, Intel e NVIDIA quando il backend scelto e i driver lo consentono. I backend candidati sono Vulkan Compute, OpenCL, CUDA e HIP o ROCm. La scelta non è fatta in anticipo. Si confrontano portabilità, driver, effort, qualità numerica e licenze.

La ricerca GPU è pianificata presto, in M1, su operatori rappresentativi: pixel-wise, convoluzione, warp e FFT. Si misura il tempo totale, incluse copie, upload e download, allocazioni, VRAM, batch, sincronizzazione e cold start contro warm start. Non è stato eseguito alcun benchmark GPU in questa revisione e non si dichiara un risultato.

L'implementazione produttiva GPU resta facoltativa in M7. Layout, strides, halo, batch, precisione e lifetime dei buffer decisi ora determinano se un backend futuro sarà efficiente. Per questo le API degli operatori separano il contratto matematico dal backend, ma senza overengineering. Il controllo CPU, l'I/O, i piccoli passaggi globali e alcune operazioni sequenziali restano probabilmente su CPU finché un beneficio misurato non giustifica il contrario.

CPU e GPU non devono essere bit-identical. Devono rispettare un error budget dichiarato, con seed, configurazione e confronti registrati. Se un backend GPU non è disponibile o fallisce, il job non passa in automatico alla CPU: si ferma con errore tipizzato non nullo e diagnosi. La CPU si usa solo quando la ricetta è stata configurata su CPU nel preflight.

### 7.10 Errori, report e provenance

Ogni fase produce un report: input usati, parametri, versioni, backend, tempi, memoria, frame integrati e scartati, copertura, avvisi e motivi di errore. Il report è leggibile dall'utente e sufficiente per ricostruire il risultato. I log tecnici sono separati dai messaggi utente.

La provenance accompagna il progetto e l'export. Include hash degli input, versione algoritmo, parametri, backend, maschere, trasformazioni WCS, correzioni di calibrazione applicate o omesse e dichiarazione di eventuale AI. Per la strategia include template, ruleset, preferenze, budget, candidati, metriche, ROI, holdout, costi, fallimenti e decisione umana. Un output senza provenance non è un risultato completo del prodotto.

Gli errori sono tipizzati: input non valido, budget insufficiente, disco pieno, file mancante, master incompatibile, trasformazione non valida, calibrazione non possibile, diagnosi non identificabile, sequenza illegale, out of domain, candidato non valutabile, cancellazione utente. Ogni errore ha un'azione suggerita e non espone dati sensibili quando il log viene condiviso.

### 7.11 Confini tra deep sky, planetario e scientifico

Il core condiviso fornisce immagini, maschere, geometria, cache, scheduler e provenance. I workflow deep sky, planetario e scientifico hanno operatori, dataset, UX e criteri di accettazione separati. Condividere il core non significa forzare lo stesso percorso.

Il planetario richiede sequenze video, scoring per frame, allineamento locale, lucky imaging e sharpening multiscala. Il scientifico richiede unità, incertezze, fotometria, astrometria e separazione dall'editing estetico. Il deep sky richiede calibrazione, registrazione, stacking, background, colore e post-processing. Le astrazioni vanno progettate ora, ma le funzioni entrano nelle fasi proprie.

### 7.12 Invarianti da testare prima di aggiungere operatori

1. Un nodo invalidato invalida tutti e soli i discendenti, non gli antenati indipendenti.
2. Un cambio di metadato o maschera invalida come un cambio di parametro.
3. Un risultato con token vecchio non viene pubblicato.
4. Un crash in ogni punto del protocollo di scrittura non produce un output marcato completo.
5. Un progetto riaperto ricostruisce lo stesso stato logico e gli stessi riferimenti.
6. Un input spostato si ricollega solo con hash verificato; uno mancante blocca il ramo.
7. Una cache corrotta viene invalidata senza corrompere il progetto.
8. L'undo ripristina una revisione, non solo un file di cache.
9. Il budget gestito non viene superato dalle prenotazioni; l'RSS è misurato e dichiarato come best effort.
10. Un job rifiutato per working set minimo non lascia file parziali pubblicati.
11. La stessa pipeline produce lo stesso risultato con gli stessi parametri, backend e seed, entro tolleranza dichiarata.
12. Il viewer resta bounded con immagini più grandi della RAM.
13. Una maschera dinamica non può dipendere da se stessa o da un consumatore; il grafo rifiuta il ciclo.
14. Un asset utente non viene eliminato dalla garbage collection della cache.
15. Una sequenza illegale viene rifiutata prima dell'esecuzione e di nuovo alla pubblicazione.
16. Un candidato non valutabile non viene presentato come migliore.

### 7.13 Registro operatori e contratti di composizione

Il core espone un registro interno fidato di operatori. Ogni voce dichiara:

- identificativo stabile e versione;
- template di input e output, unità, geometria, dominio CFA o RGB, stato di linearità e stato di calibrazione richiesto;
- precondizioni e postcondizioni;
- capability prodotte e revocate, per esempio `linear`, `wcs_valid`, `measure_ready`, `display_ready`;
- effetti collaterali dichiarati, inclusi cambio di unità, cambio di linearità, cambio di geometria e applicazione di un offset fisico;
- requisiti di risorse, halo, dipendenza globale o locale, checkpoint e backend;
- relazioni di composizione ammesse e vietate;
- politica di idempotenza o ripetibilità, solo se dimostrata.

Lo stato di linearità sconosciuto è `UNKNOWN`, distinto da `LINEAR` e `NONLINEAR`. Un operatore che richiede linearità non accetta `UNKNOWN` senza una conversione o una dichiarazione esplicita. Il registro include anche l'effetto del decoder: un offset o un crop già applicato non è invisibile.

Il registro non è un ruleset. È codice interno fidato, versionato e testato. Il ruleset è un dato dichiarativo del progetto, trattato come non fidato, limitato a identificativi in allowlist. Non contiene espressioni eval, callback, import dinamici o comandi di shell. Un ruleset che cita un ID sconosciuto viene rifiutato.

La composizione usa un grafo di template con dipendenze tipizzate e un ordine parziale. I template sono versionati e possono aggregare sotto-template. Un template aggregato non è eseguibile: contiene nodi. La validazione avviene alla costruzione, prima dell'esecuzione e di nuovo alla pubblicazione, con il token di revisione. La stessa validazione è usata dalla pipeline manuale, dalla CLI, dalla modalità Esperta e dall'Advisor. Non esiste un quarto motore.

Le regole algebriche di riordino valgono solo se dichiarate e provate dal contratto. Due operatori non commutano per default. Un filtro legittimo può essere riapplicato se il contratto lo consente; l'effect ledger serve a impedire doppie calibrazioni e ripetizioni illegittime, non a vietare ogni ripetizione. Si distinguono identità numerica, equivalenza provata e similarità empirica. Solo le prime due possono autorizzare una deduplicazione o un riordino.

### 7.14 Asset di maschera e persistenza

Una maschera può essere:

- dipinta dall'utente con brush o poligono;
- importata da un file;
- derivata da luminanza, stelle, sfondo o oggetto;
- combinata da altre maschere.

Le prime due sono asset utente autorevoli. Le seconde sono derivate e hanno una dipendenza esplicita dal nodo sorgente e dalla revisione. Una maschera derivata può usare la detection stelle senza richiedere il background. Una maschera di luminanza non richiede lo stretch. Una maschera di oggetto può richiedere una stima di fondo, ma il contratto lo dichiara.

Il salvataggio di una maschera dipinta usa un raster versionato o un event log durevole con rasterizzazione deterministica. Il database conferma il record solo dopo la persistenza. Il progetto conserva la revisione della maschera e la versione del rasterizzatore. Se il rasterizzatore cambia, la maschera vecchia resta leggibile nella sua versione o viene marcata per ricostruzione dichiarata.

### 7.15 Planner di strategia

Il planner è un servizio del core, non un motore separato. Riceve stato dei dati, capability, obiettivi, template, ruleset, preferenze e budget. Produce un insieme di candidati legali, non un'unica risposta obbligata. Usa lo stesso DAG, lo stesso scheduler e la stessa cache.

Il planner distingue:

- template e regole: struttura e legalità;
- stato dei dati: cosa è già stato applicato e con quale revisione;
- preferenze: obiettivi e pesi scelti dall'utente;
- budget: risorse e numero di candidati;
- valutazione: metriche, ROI, holdout e criteri di regressione.

Le chiavi di cache sono separate in tre famiglie: artefatto numerico, valutazione e ricerca. Cambiare una preferenza non invalida i pixel. Cambiare una metrica non invalida l'artefatto. Cambiare il budget di ricerca non invalida il risultato già pubblicato.

---

## 8. Scienza, formule e contratti numerici

### 8.1 Regola sulle fonti

Le formule di questa sezione sono modelli indipendenti di fisica del sensore, statistica, geometria e teoria dei segnali. Non sono copiate, tradotte o adattate da Siril o da un altro programma. La bibliografia di metodi è ancora da completare e verificare prima dell'implementazione. Le fonti già consultate nella pianificazione precedente sono elencate nella sezione 25. Il solo abstract di un articolo non basta per implementare: la lettura integrale è un gate.

Le formule sono state controllate per coerenza interna. Non sono state validate con esperimenti, dataset o benchmark. Una formula corretta sul piano algebrico può essere inadeguata a un sensore reale: il contratto dell'operatore deve dichiarare il dominio e i test.

### 8.2 Modello del segnale e notazione

Un frame light può essere descritto, in prima approssimazione, come:

```text
L = B + t_L d + q S + eps_L
D = B + t_D d + eps_D
F = B + t_F d + q_F A + eps_F
```

Dove:

- `L` è il valore misurato del light;
- `B` è l'offset additivo o bias;
- `t_L` è il tempo di esposizione del light;
- `d` è il tasso di corrente di buio;
- `q` è la risposta relativa per pixel e componente del sensore;
- `S` è il segnale integrato;
- `eps_L` raccoglie rumore di lettura, shot noise e altre componenti;
- `D` è un dark con lo stesso bias `B`, se il dark è matched e include il bias;
- `F` è un flat, con `A` che rappresenta l'illuminazione ricevuta attraverso il percorso ottico;
- `q_F` è la risposta relativa nella banda e configurazione del flat. La divisione corregge `q` solo nella misura in cui il modello di risposta flat/light è compatibile.

La formula è un modello di lavoro, non una verità universale per ogni sensore. Sensori con offset variabile, amplificatori diversi, dark non lineari, shutter, amp glow o elettronica dipendente dalla temperatura richiedono modelli e test propri.

La distinzione centrale è questa:

- un dark matched non privato dell'offset contiene anche quel bias residuo;
- un dark con bias rimosso non lo contiene;
- un dark scalato non è universale;
- un flat non calibrato non è un flat corretto;
- un master identificato dal nome del file non è un master tipizzato.

Il segnale integrato `S` è espresso in unità di fotoni o elettroni integrati, oppure in ADU con fattore di conversione dichiarato. La sola linearità non basta a dichiarare una misura.

### 8.3 Casi di calibrazione

| Caso | Formula di lavoro | Condizione | Avviso |
|---|---|---|---|
| Dark matched, con bias contenuto nel dark | `Lc = L - D` | Dark compatibile per esposizione, gain, temperatura, offset e pattern. | Non sottrarre anche `B`: sarebbe una doppia sottrazione. |
| Dark con bias rimosso separatamente | `Lc = L - B - D'` | `D'` è il dark senza bias e la sua provenienza è dichiarata. | Il bias non va sottratto due volte. |
| Dark scalato | `Lc = L - B - alpha (D - B)` | Scaling validato, pattern termico stabile, gain e offset compatibili, temperatura e tempo dichiarati, amp glow valutato. | `alpha` non è universale. Lo scaling non si applica solo perché è disponibile. |
| Solo bias | `Lc = L - B` | Il termico resta non corretto. | Il report deve dichiararlo e la misura può non essere idonea. |
| Nessuna calibrazione | `Lc = L` | Passaggio dichiarato, con limiti. | Non è una calibrazione. Registrazione e stacking possono restare possibili. |

La formula `Lc = L - D` è valida solo quando il dark contiene davvero il bias e la compatibilità è verificata. Se il dark è già bias-removed, usare `L - B - D'`. Se si usa un dark con bias rimosso senza sottrarre il bias del light, resta un bias parziale: il risultato può essere pubblicato come parzialmente corretto, con avviso, non come calibrazione completa. `L - D'` non è un errore da abort globale se il limite è dichiarato.

La calibrazione non è un ordine unico per ogni camera. La sezione 9 presenta la matrice completa delle otto combinazioni di dark, flat e bias, il quarto asse del dark-flat e gli stati dei master precalibrati. Questa sezione definisce la matematica; la sezione 9 definisce la decisione.

### 8.4 Flat, CFA e debayer

Per un flat con dark-flat matched:

```text
Fc = F - DF
Q = Fc / N(Fc)
C = Lc / Q
```

Dove `F` è il flat, `DF` è il dark-flat matched, `Fc` è il flat corretto, `N` è una normalizzazione dichiarata, `Q` è il flat normalizzato adimensionale e `C` è il light calibrato. `DF` contiene il bias quando è matched; non si sottrae un `B` aggiuntivo. `F - B` è un'alternativa solo quando il termico è trascurabile e la scelta è registrata. Non si applica `F - B` per comodità se il dark-flat è disponibile e compatibile.

`Fc` ha le unità del flat. `Q` è adimensionale. Un `Q` positivo e valido può essere maggiore di 1: la normalizzazione non impone un massimo. Un `Q` nullo, negativo o non valido non si sostituisce con epsilon. Il pixel si maschera e la maschera si propaga.

La normalizzazione del flat può essere globale o per componente CFA. È una scelta del modello di sensore. Deve registrare la scala e i fattori cromatici, e dichiarare se i canali sono trattati separatamente. Una normalizzazione globale conserva rapporti di risposta diversi da una normalizzazione per componente; nessuna delle due è universalmente corretta. La scelta dipende dal sensore, dallo spettro dell'illuminazione e dal successivo modello colore. Non esiste un requisito universale di quattro canali, e non si presume che `G1` e `G2` siano uguali.

Il debayer avviene dopo la calibrazione CFA, non prima. Un debayer prematuro rende ambigua la sottrazione di bias e dark e introduce interpolazione su pixel non calibrati. Il debayer Bayer riconosce pattern, offset e rotazione; se il pattern non è noto, rifiuta invece di indovinare. Il debayer X-Trans può essere rinviato con dichiarazione esplicita.

Il decoder può applicare offset, black level, scaling e crop. Queste operazioni non sono garantite innocue e non sono garantite assenti. Il contratto del decoder deve dire esattamente cosa fa e quale stato lascia al dato. Se il decoder sottrae un offset e la calibrazione lo sottrae di nuovo, il risultato è sbagliato. La provenienza deve registrare l'offset già applicato dal decoder e impedire il doppio trattamento. Lo stesso vale per un crop che cambia la geometria CFA e per uno scaling che cambia le unità.

La compatibilità non è la presenza. Camera, sensore, gain o ISO, offset, esposizione, temperatura quando applicabile, CFA, orientamento, dimensioni, binning, filtro ed epoca sono confrontati nel preflight. Un master incompatibile non è utilizzabile senza un algoritmo di compatibilità provato e approvato. L'omissione è una scelta precedente alla ricetta eseguita; un prerequisito perso dopo la selezione arresta il ramo senza cambiare correzione. Gli altri rami del progetto restano apribili.

Se il flat non è correggibile e l'offset del light non è noto, la divisione per un `Q` pur valido lascia un termine `B_res/Q` nel risultato. Non è una correzione fisica completa. Il default è omettere il flat, oppure offrire un percorso parziale esplicitamente dichiarato, non presentare una calibrazione completa.

### 8.5 Incertezza e propagazione dalla calibrazione

Per un pixel `X` corretto con un flat normalizzato `Q`, una approssimazione al primo ordine è:

```text
Var(X/Q) ~= Var(X)/Q^2 + X^2 Var(Q)/Q^4 - 2 X Cov(X,Q)/Q^3
```

La formula mostra che la sola varianza del flat non basta se `X` e `Q` sono correlati. I master condivisi e la normalizzazione possono correlare pixel e frame. La propagazione deve dichiarare se usa la covarianza, una sua approssimazione o un modello conservativo. Non si assume indipendenza senza motivo.

La formula è al primo ordine. Con `Q` vicino a zero, con errori grandi o con distribuzioni molto asimmetriche, l'approssimazione non è affidabile e il pixel va mascherato o il risultato dichiarato inaffidabile. La varianza e il peso sono dati di prima classe. Lo stacking pesato usa i pesi derivati dalla calibrazione e dalla qualità, non un peso arbitrario per frame. La propagazione dalla calibrazione allo stack è P0.

Il calcolo critico può usare f64 per ridurre il condizionamento, mentre il volume dei pixel resta f32 quando è sicuro. f32 non è sempre sufficiente: la sottrazione di due valori grandi può perdere precisione prima che il risultato venga memorizzato. La scelta va fatta per operatore, non per slogan.

### 8.6 Resampling: segnale, varianza, coordinate e flusso

Questa è la correzione scientifica principale rispetto alla 2.0. Tre oggetti vanno tenuti separati:

1. la matrice `R` che descrive come i campioni di input contribuiscono a ogni pixel di output;
2. il Jacobiano `J` della trasformazione di coordinate, che descrive come cambia l'area e come si mappano le posizioni;
3. la covarianza dei parametri geometrici stimati, che descrive l'incertezza della trasformazione stessa.

Per un resampling lineare con trasformazione fissata:

```text
y = R x
C_out = R C_in R^T
```

Se i campioni di input sono indipendenti:

```text
Var(y_p) = sum_i r_pi^2 Var(x_i)
Cov(y_p, y_q) = sum_i r_pi r_qi Var(x_i)
```

Se i campioni sono correlati, la forma completa è `C_out = R C_in R^T`, con `C_in` che include le covarianze. Non si interpola una mappa di varianza come se fosse un segnale. La varianza dell'output non è `R` applicata alla varianza dell'input. Applicare il kernel alla varianza ignora i quadrati dei pesi e le covarianze.

Esempio bilineare a due pixel. Se `y = (x_1 + x_2)/2` e i due campioni sono indipendenti con varianza `v`, allora:

```text
Var(y) = (v + v)/4 = v/2
```

Non `v`. Due output vicini che condividono gli stessi campioni sono correlati, anche se i campioni di input erano indipendenti. La correlazione tra output non è un difetto: è una conseguenza del kernel e va propagata o dichiarata.

La direzione della trasformazione deve essere esplicita. Per il WCS:

```text
WCS_out(p) = WCS_in(T_out_to_in(p))
```

Dove `T_out_to_in` porta un pixel dell'output al corrispondente input. Questa formula descrive le coordinate. Non è una mappa di pesi. Il trasferimento di una maschera usa la stessa mappatura di coordinate, ma resta un trasferimento di maschera, non una propagazione di varianza. La mappatura inversa usata per errore sposta maschere, pesi e metadati nel posto sbagliato.

Se la trasformazione geometrica non è rappresentabile dal WCS, o se il rifit dei residui supera la tolleranza, il WCS viene marcato invalido. Non si conserva un WCS vecchio su una geometria nuova. Le funzioni che richiedono WCS si bloccano o chiedono un rifit. Un residuo di rifit oltre tolleranza non si nasconde con un warning cosmetico.

#### 8.6.1 Famiglie di interpolatori

| Interpolatore | Segnale | Varianza | Invalidità e coverage | Correlazioni | Pesi firmati | Bordi |
|---|---|---|---|---|---|---|
| Nearest | Selezione del campione più vicino, piecewise costante | Varianza del campione selezionato | La validità del campione è binaria; la coverage geometrica del pixel può essere parziale e si calcola dal footprint, non dalla scelta nearest | Output che scelgono lo stesso campione sono perfettamente correlati | No | Nessuna extrapolazione; i pixel fuori supporto sono invalidi |
| Bilineare | Media pesata di 4 campioni, pesi positivi | Somma dei quadrati dei pesi per le varianze, più covarianze | Coverage parziale ai bordi; un campione invalido richiede rinomormalizzazione o invalidazione | Output adiacenti correlati; la larghezza della correlazione è locale | No | La rinomormalizzazione ai bordi va dichiarata e cambia la varianza |
| Bicubica | Kernel a 16 tap, può andare in overshoot | Somma dei quadrati dei pesi; i pesi negativi non rendono negativa la varianza se le varianze sono positive | Un input invalido con peso negativo non si esclude banalmente: serve rinomormalizzazione o maschera di output | Correlazioni più larghe, anche tra pixel non adiacenti | Sì | Bordi e ringing da dichiarare; nessuna rinomormalizzazione universale a somma 1 |
| Lanczos | Kernel largo con lobi negativi, ringing possibile | Somma dei quadrati dei pesi per input indipendenti; altrimenti forma completa con covarianze tra input | Coverage e maschera richiedono una politica esplicita; il ringing non è un errore di calcolo | Covarianze possono essere negative tra output vicini | Sì | Supporto troncato ai bordi; la somma dei pesi non è una misura di coverage |
| Drizzle | Deposito dei campioni nel footprint di output, non semplice interpolazione | Pesi e conteggi dei campioni contribuenti; la varianza dipende dal numero e dai pesi | Coverage dal footprint e dal pixfrac; pixel senza campioni restano invalidi | Correlazioni da campioni condivisi tra pixel di output | No, se il droplet è positivo | Bordi con copertura parziale; non si inventa copertura piena |

La tabella non elegge un interpolatore universale. Ogni famiglia ha un dominio, un costo e un artefatto tipico. La scelta va registrata nel nodo e testata sullo stimando dichiarato. Il nearest può avere un campione valido e una coverage geometrica parziale: le due cose non coincidono.

#### 8.6.2 Coverage, maschere, alpha e flusso

Coverage è la frazione geometrica di area di un pixel di output coperta da campioni validi di input. Non è la somma dei pesi firmati. Un kernel con lobi negativi può avere somma dei pesi uguale a uno e copertura inferiore a uno. Un kernel positivo può avere somma dei pesi diversa da uno se i bordi sono troncati.

La maschera di invalidità non è un canale alpha. Alpha descrive la composizione o la trasparenza per il display e per l'editing. La maschera descrive quali pixel non contribuiscono a una stima. Trattare l'alpha come maschera scientifica, o viceversa, produce risultati plausibili e sbagliati. Una maschera di editing float 0..1 non è una maschera di invalidità e non è una coverage.

La conservazione del flusso integrato e quella della brillanza superficiale sono diverse. Il flusso integrato si conserva quando la somma dei contributi tiene conto dell'area e del Jacobiano. La brillanza superficiale si conserva quando il valore medio per unità di area resta lo stesso. Un operatore può conservare l'una e non l'altra; il contratto deve dire quale. Il test di flusso va fatto sull'unità dichiarata e sullo stimando dichiarato, non su un'immagine qualsiasi. Conservare il flusso può richiedere un riscalamento che non conserva la brillanza, e viceversa.

Non esiste una rinomormalizzazione universale a zero o a uno ai bordi. La scelta di rinomormalizzare, lasciare il bordo invalido o accettare una perdita di flusso è parte del contratto. La scelta deve essere visibile nella provenance e nei test.

### 8.7 Covarianza pratica e approssimazioni

Non si richiede una matrice globale densa per ogni immagine. Una rappresentazione pratica è:

- varianza diagonale per pixel;
- correlazioni locali introdotte dal kernel di resampling, dal NPS o dalla PSF;
- un fattore di apertura per le misure fotometriche;
- covarianze esplicite per i master condivisi e per le trasformazioni stimate, quando il modello le include.

Le approssimazioni accettabili devono dichiarare cosa omettono. Per esempio, `R C_in R^T` con `R` fissata non include l'incertezza della geometria stimata. Se la trasformazione è stimata dai dati, la sua covarianza va propagata con il Jacobiano dei parametri, oppure l'omissione va dichiarata e testata. Un risultato che ignora l'incertezza della geometria non è automaticamente sbagliato, ma non può dichiararsi completo.

Le correlazioni tra pixel non sono tutte uguali. Resampling, filtri digitali e accoppiamenti del sensore possono correlare il rumore tra pixel vicini. Una PSF ottica larga descrive la risposta media a una sorgente: da sola non implica rumore correlato, per esempio nel modello di conteggi Poisson indipendenti condizionato alla scena. Un'incertezza strutturata del flat può correlare pixel lontani; un master condiviso correla frame diversi. La rappresentazione scelta deve coprire i casi che il test usa. Se un test richiede una covarianza che il modello non rappresenta, il test non è superato.

La covarianza dei parametri geometrici e la covarianza fotometrica restano separate anche quando si combinano. La prima descrive l'incertezza di una posizione o di una trasformazione. La seconda descrive l'incertezza del segnale. Sommarle o confonderle produce un errore di interpretazione, non solo un errore numerico.

### 8.8 Stacking, N basso e minimum variance

Lo stacking combina frame allineati con pesi. Per una media pesata:

```text
S_hat = sum_i w_i y_i / sum_i w_i
Var(S_hat) = w^T C_y w / (sum_i w_i)^2
Var(S_hat) = sum_i w_i^2 v_i / (sum_i w_i)^2    (campioni indipendenti, v_i = Var(y_i))
Var(S_hat) = 1 / sum_i w_i                       (solo se w_i = 1/v_i, nelle condizioni precedenti)
```

Le formule di propagazione sopra condizionano a pesi fissati e a uno stesso stimando. `C_y` è la covarianza tra i campioni combinati. Un riscalamento comune dei pesi non cambia né lo stack né la sua varianza; il test deve verificarlo. L'ultima uguaglianza richiede pesi esattamente inversi delle varianze, senza fattori qualità o riscalamenti ulteriori. Con pesi diversi si usa la formula generale.

Per osservazioni indipendenti dello stesso stimando, i pesi minimum variance sono proporzionali all'inverso della varianza, con normalizzazione e regolarizzazione dichiarate. La stima della varianza può essere approssimata; il report deve dire come. Pesi adattivi, rejection e pesi stimati dagli stessi dati richiedono un modello ulteriore e test del bias. PSF diverse possono corrispondere a stimandi pixel diversi: il contratto deve specificare se stima intensità sulla griglia, brillanza o flusso con risposta PSF dichiarata. Non si deduce ottimalità dalla sola inversa varianza marginale.

Peso statistico e qualità del frame non devono contare due volte la stessa degradazione. Se il fattore di qualità contiene già il rumore, moltiplicarlo per l'inverso della varianza conta due volte lo stesso rumore. Un fattore di qualità non rumoroso, per esempio FWHM, trailing o copertura, può essere combinato se il modello dichiara l'indipendenza o l'ortogonalizzazione. Non esiste una formula universale per tutti i fattori di qualità.

Per `N = 1` e `N = 2` non si applica una rejection statistica. Non ci sono abbastanza campioni per distinguere un outlier da una variazione reale. Si usa una maschera di pixel invalidi, saturazione, bordi e copertura. Il risultato può essere pubblicato con un avviso sul numero effettivo di campioni. Per `N = 3` una mediana è robusta, ma non è una prova di identificazione dell'outlier. Non si dichiara che il frame anomalo è stato identificato se il test non lo dimostra.

Il numero effettivo locale `Neff` può variare per pixel e per canale:

```text
Neff = (sum_i w_i)^2 / sum_i w_i^2
```

Coverage e `Neff` sono più informativi di un conteggio globale di frame. Il report deve mostrare coverage, pixel senza copertura, frame scartati e motivo. Un pixel senza copertura non è zero: è invalido.

La soglia `RMS_num <= 0.1 sigma_noise` è solo una proposta da testare. Non è una verità scientifica e non va implementata come criterio universale. Richiede di esaminare bias, code della distribuzione del flusso, limiti noiseless e arrotondamenti. Senza questi test resta una ipotesi. Non esiste una soglia MAD universale per l'azzeramento sotto soglia: la soglia dipende da N, dal rumore e dal dataset, e va dichiarata per algoritmo. Una soglia MAD stimata dai dati può assorbire segnale astrofisico e rumore correlato. Va validata l'intera procedura adattiva, non solo il numero, su realizzazioni di rumore indipendenti. Gli errori dello stimatore di rumore, dello shrinkage e della selezione vanno misurati separatamente. Non esiste un debias universale.

La tolleranza di jitter per asse:

```text
jitter_axis <= sqrt(1.01^2 - 1) * sigma_PSF = 0.142 sigma_PSF
```

Vale solo per PSF gaussiana, isotropa e indipendente, e non per blur o resample. Non è una tolleranza generale per ogni trasformazione. Se la PSF non è gaussiana o è correlata, la formula non si applica. Il valore è un target di verifica, non una promessa di qualità.

### 8.9 Deconvoluzione e ordine lineare

La deconvoluzione non è un operatore lineare in generale. Richardson-Lucy è un procedimento iterativo non lineare anche quando l'input è lineare. Wiener è lineare solo per un filtro fissato e un modello di rumore fissato. La linearità dell'input non rende lineare l'algoritmo e non rende automaticamente valido il risultato.

La deconvoluzione va applicata a dati lineari quando l'algoritmo lo richiede. Se il dato è non lineare, l'algoritmo va scelto o motivato di conseguenza, oppure la deconvoluzione va eseguita prima dello stretch. L'ordine è parte del contratto del nodo, non una preferenza estetica.

La non negatività di Richardson-Lucy non autorizza un clamp silenzioso dei negativi. Un clamp nasconde un modello sbagliato, una PSF sbagliata o un dato fuori dominio. Il contratto deve dichiarare la verosimiglianza usata, l'alternativa sicura e il rifiuto del dominio quando il risultato non è affidabile. Le stelle sature, il ringing, il numero di iterazioni e la regolarizzazione sono parametri con limiti e test. Il confronto tra Wiener, Richardson-Lucy e varianti va fatto solo dove l'algoritmo è adatto e con metriche di artefatto, non solo con impressione visiva.

Il P0 APPROVATO usa un modello lineare a PSF fissa, non Richardson-Lucy e non una PSF variabile. Lo stimatore ai minimi quadrati regolarizzati è lineare o affine in `y` solo quando `H`, `W`, `L`, `lambda` e `b` sono tutti fissi. Se la PSF, `lambda` o il modello di rumore vengono stimati dai dati, la pipeline complessiva diventa adattiva e non lineare, anche se il problema interno resta quadratico. La formula e i limiti sono nella sezione 10.16. Richardson-Lucy resta un operatore P1 non lineare, descritto correttamente e non cancellato.

### 8.10 Linearità, misura e output

Un'immagine lineare non è automaticamente una misura. Per la misura servono unità, calibrazione, incertezza, maschere, geometria e provenance. Un'immagine stirata o alterata da AI non è equivalente a un RAW per fotometria o astrometria.

Il prodotto deve separare:

- dato lineare calibrato, adatto a misure dichiarate;
- dato lineare elaborato, con trasformazioni tracciate;
- dato non lineare per display o export fotografico;
- output AI, dichiarato e separato dalla misura.

La linearità è una condizione necessaria per alcuni modelli, non una garanzia di validità scientifica. La validità è una proprietà per misura, con capability, incertezza e motivi. Un'immagine può essere lineare e comunque inadatta alla misura richiesta perché manca una calibrazione o una maschera necessaria. Il WCS è richiesto solo dalle misure che usano coordinate celesti o associazioni a catalogo, non da ogni fotometria in coordinate pixel.

### 8.11 Numerica, precisione e tolleranze

La precisione di lavoro è f32 come candidato per il volume dei pixel. f64 va usato negli operatori che amplificano il condizionamento: normalizzazioni, riduzioni, accumuli lunghi, fitting, deconvoluzione, stime di rumore e propagazione di covarianza. La scelta va motivata per operatore.

Il criterio `RMS <= 0.1 sigma_noise` è solo un candidato di budget di errore. Non copre da solo bias, conservazione del flusso, code della distribuzione, ringing, artefatti di bordo o arrotondamento noiseless. Un test che misura solo l'RMS può passare con un risultato scientificamente inaccettabile. Il budget deve includere tutti i contributi dichiarati.

Per un broadening FWHM proposto dell'1 per cento, il rapporto jitter per asse su `sigma_PSF` è `sqrt(1.01^2 - 1)`, circa 0,142. Vale soltanto con PSF gaussiana e jitter gaussiano isotropo indipendente; non include il blur del resampling. Non è una tolleranza universale verificata. È un target da testare.

Le differenze tra CPU scalare, SIMD e GPU non devono essere bit-identical. Devono rispettare un error budget dichiarato, con seed, configurazione e confronti registrati. Un risultato GPU che supera il budget non è accettabile solo perché è più veloce.

### 8.12 Catena di propagazione

La catena minima è questa:

1. **Calibrazione:** produce `Lc`, maschere, varianza o pesi, unità e provenienza. La varianza include il contributo del flat e, quando modellata, la covarianza tra light e flat.
2. **Debayer:** produce canali RGB o multicanale con maschere e incertezza. Il debayer non è una misura dei canali mancanti: l'incertezza dei canali interpolati non è la stessa di un campione misurato.
3. **Registrazione:** stima la trasformazione e la sua incertezza. La trasformazione applicata al resampling ha una covarianza di parametri distinta dalla covarianza fotometrica.
4. **Resampling:** applica `R` e propaga `C_out = R C_in R^T`, con le approssimazioni dichiarate. Coverage e maschere si trasferiscono con la mappatura di coordinate.
5. **Stacking:** combina i campioni con pesi coerenti con lo stimando. La varianza del risultato dipende dai pesi e dalle correlazioni residue. `Neff` e coverage sono report obbligatori.
6. **Drizzle:** tratta il deposito dei campioni come caso proprio, non come un interpolatore bilineare. Coverage, pixfrac, peso e varianza hanno un contratto separato.
7. **Post-processing:** denoise, sharpening, stelle e deconvoluzione dichiarano il dominio. La varianza non si copia attraverso una trasformazione non lineare.

In ogni passaggio, la varianza non è un'immagine da trattare con lo stesso kernel del segnale. La maschera non è alpha. La copertura non è la somma dei pesi. Il flusso non è la brillanza. Queste quattro confusioni sono i difetti che i test devono intercettare.

### 8.13 Stato delle formule

| Formula o regola | Stato | Cosa manca |
|---|---|---|
| `L = B + t d + q S + eps` | Modello di lavoro | Verifica su sensori diversi e condizioni non ideali |
| `Lc = L - D` con dark matched | Valida sotto compatibilità | Test di doppia sottrazione e mismatch |
| `Lc = L - B - D'` | Valida sotto stato dichiarato | Test su master precalibrati |
| `Lc = L - B - alpha (D - B)` | Condizionata | Validazione per temperatura, gain, tempo e amp glow |
| `Q = Fc / N(Fc)` | Valida sotto normalizzazione dichiarata | Scelta globale o per CFA, fattori cromatici |
| `Var(X/Q)` al primo ordine | Approssimazione | Test con covarianza e Q piccolo |
| `y = R x`, `C_out = R C_in R^T` | Valida per R lineare fissata | Test su kernel, bordi e covarianze |
| `Var(y_p) = sum r_pi^2 Var(x_i)` | Valida per input indipendenti | Test con rumore correlato |
| `WCS_out(p) = WCS_in(T(p))` | Convenzione | Test di direzione e rifit |
| `Var(S_hat) = w^T C_y w / (sum w)^2` | Valida per pesi fissati e covarianza dichiarata; `1/sum w` solo con indipendenza e `w_i = 1/v_i` | Test di riscalamento dei pesi, correlazioni, pesi stimati e stesso stimando |
| `Neff = (sum w)^2 / sum w^2` | Definizione operativa | Test locale per pixel e canale |
| `jitter <= 0.142 sigma_PSF` | Target condizionato | Solo PSF gaussiana isotropa indipendente |
| `RMS <= 0.1 sigma_noise` | Candidato di budget | Test su bias, flusso, code e noiseless |
| RL non lineare, Wiener lineare a filtro fissato | Corretto concettualmente | Contratti, test di artefatto, dominio |
| `Cz ~= Jf Cx Jf^T` per trasformazione non lineare | Approssimazione locale | Differenziabilità, curvatura, clipping, modello di rumore |
| Soglia MAD adattiva | Procedura da validare interamente | Assorbimento di segnale, rumore correlato, bias di stima, selezione e shrinkage |

### 8.14 Rumore su dati non lineari e denoise falsificabile

Un'immagine non lineare non rende impossibile la stima del rumore. Se la trasformazione `z = f(x)` è nota e differenziabile, una approssimazione locale è:

```text
Cz ~= Jf Cx Jf^T
```

dove `Jf` è il Jacobiano di `f`. L'approssimazione richiede variazioni piccole rispetto alla curvatura di `f`. Con clipping o saturazione l'informazione è persa e l'inversione del Jacobiano non la recupera. Il contratto deve dichiarare il dominio, il clipping e il trattamento dei pixel non invertibili.

Il denoise non lineare è falsificabile quando si conoscono la scena, la trasformazione e il modello di rumore. Si possono generare realizzazioni indipendenti di rumore, applicare `f` nota, eseguire il denoise e confrontare con il ground truth. In questo modo si misurano bias, varianza residua, perdita di dettaglio e artefatti. Non serve un ground truth fotografico universale.

Se `f` è sconosciuta, si può usare un modello empirico delimitato, per esempio una LUT o una famiglia parametrica stimata dai dati. La confidenza si riduce e il sistema può astenersi. Non si riusa la varianza lineare come se fosse valida dopo lo stretch. La varianza lineare può essere un input del modello, non la varianza dell'output.

La stima del rumore non lineare non è una licenza a inventare. Il report dichiara modello, parametri, dominio e incertezza. Un test che riduce il rumore ma peggiora la struttura debole o le stelle non è un successo.

### 8.15 Stima della PSF

La FWHM da sola non identifica una PSF. Una PSF gaussiana e una Moffat possono avere la stessa FWHM e ali diverse. Per il fit servono profili stellari, centri, maschera delle stelle sature, stima locale del fondo, normalizzazione, modello di rumore e incertezza dei parametri.

Una PSF gaussiana bidimensionale ellittica richiede centro, scale principali e orientamento; ampiezza e fondo appartengono al modello dell'immagine. Una Moffat aggiunge un parametro di forma delle ali. Il fit deve dichiarare il modello, i parametri liberi, i limiti e la bontà del fit. Una PSF derivata da una sola FWHM non è una PSF verificata.

Il padding e le condizioni al bordo sono parte del contratto. Il padding riflesso ripetuto non garantisce l'assenza di aloni e non è una verità fisica. Il supporto della PSF, la normalizzazione e l'errore di modello vanno dichiarati. Un supporto troncato troppo piccolo produce ringing; uno troppo grande aumenta il costo. Il preflight delle risorse include il padding e i buffer FFT.

---

## 9. Calibrazione flessibile e master

### 9.1 Principio

L'utente può possedere light e qualunque sottoinsieme di dark, flat e bias. Il software non deve imporre tutti e tre per avviare la lavorazione, ma non deve inventare una calibrazione scientificamente impossibile. La presenza di un file non è compatibilità. La compatibilità non è qualità. La qualità non è garanzia di risultato. Il requisito è P0 vincolante; questa sezione propone la politica di default e i dettagli tecnici.

La calibrazione è un albero di decisioni prima dell'esecuzione, non una formula unica. Analisi e preflight stabiliscono quali rami sono ammissibili; la ricetta selezionata dichiara correzioni e omissioni. Dopo la selezione, la perdita di un prerequisito o un'incompatibilità produce STOP, non un'alternativa eseguita al suo posto. Questa distinzione governa tutte le omissioni, le correzioni parziali e i passthrough delle sezioni 8–10 e dei requisiti F-CAL.

### 9.2 Stati tipizzati dei master

Il tipo del master è un metadato, non il nome del file. Il contratto separa ruolo (`bias`, `dark`, `dark_flat`, `flat`), componenti additivi inclusi o rimossi, scaling, normalizzazione e provenienza. Le voci seguenti sono attributi e ruoli combinabili, non un unico enum mutuamente esclusivo:

| Stato | Significato | Uso |
|---|---|---|
| `raw` | Master combinato ma non calibrato, con bias e termico secondo il tipo. | Richiede la correzione appropriata prima dell'uso. |
| `bias_included` | Dark che contiene il bias. | `L - D` se matched. Non sottrarre `B`. |
| `bias_subtracted` | Dark o master da cui il bias è già stato rimosso. | `L - B - D'`. Non sottrarre `B` due volte. |
| `calibrated` | Flat già corretto e normalizzato, con normalizzazione dichiarata. | `Lc / Q`, senza ricalibrare il flat. |
| `scaled` | Dark scalato con `alpha` e condizioni dichiarate. | Solo se lo scaling è validato. |
| `dark_flat` | Ruolo additivo con relazione esplicita al flat. | `Fc = F - DF` se matched con bias incluso; se il bias è già rimosso valgono le condizioni della sezione 9.4. |
| `bias_master` | Master bias dedicato. | `L - B`, `F - B`, secondo il caso. |
| `unknown` | Stato non dichiarato. | Non si usa per una correzione critica senza verifica. |

Uno stato `unknown` non si tratta come `raw` per comodità. Se il master non dichiara lo stato, il sistema chiede una verifica o lo esclude dalla correzione. Un master `calibrated` usato come raw produce una doppia normalizzazione. Un master `bias_subtracted` usato con `L - D` lascia il bias nel light.

Il decoder dichiara l'offset o black level già applicato. Se il decoder ha sottratto un offset e la calibrazione lo sottrae di nuovo, il risultato è sbagliato. La provenienza impedisce il doppio trattamento.

### 9.3 Matrice delle otto combinazioni

L'ordine delle righe è quello richiesto: dark, flat, bias. `1` significa presente e utilizzabile, `0` significa assente o non utilizzabile.

| Dark | Flat | Bias | Percorso valido | Condizione | Cosa non è possibile o va dichiarato |
|---|---|---|---|---|---|
| 1 | 1 | 1 | `Lc = L - D` con dark matched che include il bias, oppure `L - B - D'`; flat corretto con dark-flat matched o con bias, poi normalizzato. | Compatibilità completa; `F - B` solo se il contributo termico del flat è trascurabile rispetto al budget dichiarato; nessuna doppia sottrazione. | Se il dark include il bias, `B` non si sottrae di nuovo. Se il flat non è correggibile, si omette quella correzione. |
| 1 | 1 | 0 | `Lc = L - D` se il dark include il bias; il flat richiede dark-flat matched con offset incluso, flat già calibrato o una strategia di offset dimostrata. | Compatibilità e stato del dark noti. Se è disponibile solo `D'`, `L - D'` è parziale e lascia l'offset del light. | Nessun uso automatico del flat senza un numeratore nel dominio additivo richiesto. In assenza di prova si omette il flat, conservando le correzioni ammesse. |
| 1 | 0 | 1 | `Lc = L - D`; il bias si usa solo se la rappresentazione del dark lo richiede. | Nessuna doppia sottrazione. | Niente flat-field: vignettatura e polvere restano. Avviso esplicito. |
| 1 | 0 | 0 | `Lc = L - D` se il dark matched contiene il bias; con solo `D'`, sottrazione parziale `L - D'`. | Stato e compatibilità del dark verificati. | Nessuna correzione flat. Nel percorso parziale resta il bias; nel percorso matched non si presume un offset residuo aggiuntivo. Restano le altre incertezze dichiarate. |
| 0 | 1 | 1 | `Lc = L - B`; flat corretto con dark-flat matched, oppure con bias, poi normalizzato. | `F - B` solo con termico del flat trascurabile rispetto al budget; altrimenti dark-flat o flat precalibrato. | La corrente di buio del light non è compensata. Il report lo dichiara; non è calibrazione termica completa. |
| 0 | 1 | 0 | Flat utilizzabile come `Q` solo se già calibrato oppure correggibile con dark-flat o procedura validata; il numeratore richiede separatamente un offset del light noto, già rimosso o ricostruito. | Un dark-flat compatibile può correggere il flat, ma non dimostra che l'offset del light sia stato rimosso. | Anche dividendo per un `Q` corretto, un offset residuo del light diventa `B_res/Q`: non è una correzione fisica completa. Default: omettere il flat quando quel prerequisito manca; eventuale percorso parziale esplicito, con limite propagato. |
| 0 | 0 | 1 | `Lc = L - B`. | Bias valido. | Termico e difetti di campo restano. Avviso. |
| 0 | 0 | 0 | Passaggio senza master. | Nessuna correzione. | Qualità e limiti dichiarati. Registrazione, stacking e post-processing restano possibili se il dato lo consente. |

La tabella non promuove un percorso a calibrazione completa. Ogni riga ha un esito dichiarato. La modalità Automatica propone il percorso adatto ai dati presenti; la modalità Esperta consente di scegliere un ramo motivato senza aggirare i controlli numerici.

### 9.4 Dark-flat come quarto asse

Il dark-flat è un input opzionale con una relazione esplicita ai flat. Non è intercambiabile con il dark dei light. Gli stati del dark-flat sono:

| Dark-flat | Relazione con il flat | Uso | Limite |
|---|---|---|---|
| Assente | Nessuna | Il flat richiede un altro percorso di calibrazione. | Non si inventa un dark-flat. |
| Presente e matched | Tempo, gain o ISO, temperatura, offset e pattern compatibili. | `Fc = F - DF`. | Se `DF` include il bias, non si sottrae `B` in aggiunta. |
| Presente ma incompatibile | Parametri diversi o pattern non compatibile. | Non si usa per la correzione. | Si tratta come assente, con avviso. |
| Presente con bias rimosso | Dark-flat additivo `DF'` con stato dichiarato. | `Fc = F - B - DF'`, se il bias residuo del flat è noto e compatibile; oppure dominio già privo di bias verificato. | `DF'` non è un flat normalizzato e non si usa come `Q`. Senza offset noto non si dichiara il flat calibrato. |

Lo stato del flat è un asse separato: raw, corretto ma non normalizzato, normalizzato con fattori dichiarati, oppure sconosciuto. Un flat già calibrato e normalizzato si usa direttamente come `Q`; non è uno stato del dark-flat e non richiede un nuovo dark-flat.

La matrice completa nasce dall'incrocio tra le otto combinazioni, lo stato del dark-flat e lo stato del flat. Non tutte le combinazioni ammettono ogni correzione fisica. Un dark-flat presente non rende valido un flat se la relazione non è compatibile. Prima dell'esecuzione è possibile scegliere una ricetta che ometta la correzione non giustificata, con provenienza e limiti dichiarati. Una ricetta già selezionata non cambia in seguito a un errore.

### 9.5 Compatibilità e presenza

La compatibilità si verifica su:

- camera e sensore;
- gain o ISO e offset;
- esposizione, per dark e dark-flat;
- temperatura, quando il sensore la espone e il modello la usa;
- pattern CFA, offset e orientamento;
- dimensioni, binning e crop;
- filtro, per flat e light;
- epoca di acquisizione, quando il comportamento del sensore cambia.

Un master presente ma incompatibile è escluso durante l'analisi precedente alla selezione della ricetta. Il light è un input obbligatorio, non un master. Se manca un input richiesto dalla ricetta selezionata, si arresta il ramo dipendente; gli altri rami e il progetto restano apribili. Non si omette a runtime una correzione selezionata.

La capacità di calibrazione è un prerequisito del motore; la presenza di ogni master non lo è. F-CAL-08 accetta le correzioni o il passthrough scelti prima dell'esecuzione e registrati nel ledger. Dopo un errore non passa al passthrough né a un'altra calibrazione.

### 9.6 Doppio offset e contratto del decoder

Il decoder può aver già applicato black level, offset, scaling o crop. La calibrazione non deve ripetere una sottrazione già fatta. Il contratto del decoder dichiara:

- offset o black level applicato;
- scala applicata;
- crop e geometria CFA risultante;
- metadati conservati o eliminati;
- stato del dato: raw, parzialmente corretto o calibrato.

La provenienza registra queste operazioni. Se il decoder non dichiara lo stato, il sistema non assume che il dato sia intatto. Un test con un offset noto verifica che la catena non lo sottragga due volte. Lo stesso test verifica che un crop non abbia cambiato il pattern CFA in modo non dichiarato.

### 9.7 Alternative quando mancano i master

Quando l'analisi iniziale rileva un master mancante, prima di selezionare ed eseguire la ricetta il prodotto può proporre:

- una ricetta senza quella correzione, con limiti dichiarati;
- usare una correzione cosmetica per hot e dead pixel, se il contratto lo consente;
- usare rejection e dithering nello stacking per ridurre il contributo di outlier;
- stimare un pattern o un gradiente con un metodo statistico, dichiarando che non è un flat fisico;
- rifiutare una correzione non giustificata.

Un algoritmo di gradient removal non equivale a un flat. Non recupera polvere, risposta pixel-to-pixel o vignettatura in campi con nebulosità diffusa. Una tecnica cosmetica non ricostruisce con certezza il dark o il bias non acquisito. Le stime possono essere offerte con confidenza e limiti, non come correzioni equivalenti.

La scelta manuale "esegui senza questo master" e la proposta dell'Advisor avvengono prima dell'esecuzione. Dopo uno STOP l'utente può creare manualmente una nuova ricetta: non è un fallback autorizzato né un resume. Il rollback esplicito ripristina una revisione precedente, non sostituisce l'operazione fallita.

### 9.8 Propagazione di varianza, maschere e provenienza

La calibrazione parziale propaga:

- varianza o pesi per ogni correzione applicata;
- maschere di invalidità, saturazione, bordi e bad pixel;
- coverage dei campioni validi;
- provenienza delle correzioni applicate e di quelle omesse;
- stato dei master e motivo dell'omissione.

Una correzione omessa non è un errore nascosto. È una voce del report. Una correzione applicata con un master di qualità dubbia è una voce con confidenza. Il risultato può essere pubblicato, ma la sua capability di misura riflette i limiti.

### 9.9 Test della calibrazione

I test minimi sono:

1. otto combinazioni end-to-end su dati sintetici con ground truth;
2. dark-flat presente con bias incluso, presente con bias rimosso, assente o incompatibile; separatamente flat precalibrato e normalizzato;
3. master `raw`, `bias_included`, `bias_subtracted`, `calibrated`, `scaled` e `unknown`;
4. nessuna doppia sottrazione di bias in nessuna permutazione valida;
5. flat non correggibile senza offset noto: il sistema offre l'omissione, non una divisione apparentemente plausibile;
6. pixel di flat nulli o negativi: maschera, non epsilon;
7. decoder con offset noto: nessun doppio trattamento;
8. master incompatibile prima della scelta: ricetta con omissione motivata; incompatibilità sopravvenuta dopo la scelta: STOP del ramo, nessuna sostituzione;
9. varianza e maschere propagate fino allo stack;
10. dataset reali pubblici quando disponibili, con licenza e hash.

I test devono produrre metriche: bias residuo, rumore, flat residuo, perdita di segnale, copertura, artefatti. Le soglie si derivano dal ground truth e dalla precisione del dato, non da un numero inventato.

---

## 10. Contratti scientifici

### 10.1 Template unico di contratto

Ogni famiglia di operatori usa lo stesso template di undici campi. Il template è l'autorità unica: non si ripetono undici paragrafi generici per ogni funzione. Ogni famiglia li istanzia con contenuto proprio. Un operatore P1 o P2 che non ha un contratto approvato non entra in implementazione.

| Campo | Domanda a cui rispondere |
|---|---|
| 1. Problema matematico | Che grandezza si stima o si trasforma, con quale modello e quale notazione. |
| 2. Dominio, unità e prerequisiti | Quali input sono ammessi, in quali unità, con quali metadati e compatibilità. |
| 3. Ipotesi e limiti | Quali assunzioni reggono il metodo e quando il metodo non è affidabile. |
| 4. Alternative e scelta | Quali metodi sono stati confrontati e perché si sceglie quello proposto. |
| 5. Riferimenti indipendenti e stato | Quali standard o articoli sostengono il metodo e quanto sono stati verificati. |
| 6. Numerica, stabilità e varianza | Precisione, condizionamento, propagazione dell'errore e tolleranze. |
| 7. Maschere, metadati e bordi | Come si trattano invalidità, saturazione, bad pixel, coverage, WCS e bordi. |
| 8. Determinismo | Cosa rende il risultato ripetibile e cosa può variare tra backend. |
| 9. Fixture, metriche, soglia e gate | Quali dati sintetici o pubblici, quali metriche, quale soglia approvata e quale gate. |
| 10. Failure mode, warning e provenance | Come si comporta in errore, cosa avvisa e cosa registra. |
| 11. Ottimizzazione | Quali ottimizzazioni sono ammesse senza cambiare il contratto. |

I contratti C-01..C-10 sono conservati e aggiornati. I contratti C-11..C-19 sono nuovi e coprono le funzioni P0 fotografiche, la strategia, il solver assistito e la validazione. Il numero non implica una priorità.

### 10.2 C-01 Calibrazione del light

| Campo | Contenuto |
|---|---|
| 1. Problema matematico | Stimare `S` da `L` con `L = B + t_L d + q S + eps`, scegliendo il ramo valido tra `L - D`, `L - B - D'`, `L - B - alpha (D - B)` e `L - B`. |
| 2. Dominio, unità e prerequisiti | Dati lineari in ADU o elettroni, mono, CFA o RGB. Compatibilità di camera, gain, offset, esposizione, temperatura, CFA, binning, filtro e geometria. Stato del decoder dichiarato. |
| 3. Ipotesi e limiti | Compatibilità del termine additivo nel percorso matched, flat moltiplicativo e sensore stabile. La proporzionalità del dark nel tempo è richiesta solo quando si applica scaling. Amp glow non modellato, offset variabile, nonlinearità, shutter o mismatch possono invalidare la correzione. |
| 4. Alternative e scelta | Correzione completa, solo dark, solo bias, dark scalato, nessuna correzione: ricette primarie selezionate a priori secondo presenza, compatibilità, stato e validazione. Un master incompatibile è escluso prima della selezione, non dopo il fallimento della ricetta. |
| 5. Riferimenti indipendenti e stato | Letteratura di calibrazione del sensore, standard FITS, documentazione LibRaw. Candidati non letti integralmente in questa revisione. Nessuna derivazione da Siril. |
| 6. Numerica, stabilità e varianza | f32 per il volume, f64 per offset e accumuli critici. Propagazione della varianza del flat con la formula del primo ordine. Nessun epsilon al posto di un pixel invalido. |
| 7. Maschere, metadati e bordi | Maschere di invalidità, saturazione, bordi e bad pixel propagate. Coverage conservata. Provenienza delle correzioni applicate e omesse. Unità conservate. |
| 8. Determinismo | Ordine canonico dei master, stessi parametri e stesso backend producono lo stesso risultato scalare. SIMD entro tolleranza. Nessun seed se non c'è rumore simulato. |
| 9. Fixture, metriche, soglia e gate | Sintetici con `B`, `d`, `q`, `S` noti, otto combinazioni, dark-flat e stati dei master. Metriche: bias residuo, rumore, flat residuo, perdita di segnale. Soglia dal ground truth. Gate M3. |
| 10. Failure mode, warning e provenance | Prima della scelta, master incompatibile o offset ignoto escludono le correzioni non giustificate; omissione e percorso parziale sono ricette distinte. Dopo la selezione, prerequisito perso o errore producono STOP senza sostituzione. Warning e provenance obbligatori. |
| 11. Ottimizzazione | Streaming, tile, cache dei master, SIMD. La GPU può accelerare pixel-wise e accumuli, senza cambiare il contratto. |

### 10.3 C-02 Generazione dei master

| Campo | Contenuto |
|---|---|
| 1. Problema matematico | Combinare `N` frame per stimare `B`, `D(t, T)`, `F`, `DF` o `Q`, con rejection e normalizzazione dichiarate. La stima è una riduzione statistica, non una misura priva di incertezza. |
| 2. Dominio, unità e prerequisiti | Frame dello stesso gruppo per esposizione, gain, offset, temperatura, CFA, binning, filtro e geometria. Unità coerenti. Stato raw o calibrato dichiarato. |
| 3. Ipotesi e limiti | Stazionarietà nel gruppo, outlier sparsi, illuminazione del flat stabile. Non affidabile con nuvole in movimento, deriva termica, flat saturi o gruppi disomogenei. |
| 4. Alternative e scelta | Media, mediana, media pesata, sigma clipping, winsorized. La scelta dipende da `N`, dal modello di outlier e dal costo. Per `N` piccolo la mediana non è una prova di identificazione. |
| 5. Riferimenti indipendenti e stato | Statistica robusta, stima di rumore, standard FITS. Candidati non letti integralmente. |
| 6. Numerica, stabilità e varianza | Accumulo in f64, output f32 quando sicuro. L'incertezza del master non è sempre `sigma/sqrt(N)`: la correlazione tra frame e la normalizzazione vanno dichiarate. |
| 7. Maschere, metadati e bordi | Maschere per frame, mappe di outlier, saturazione, gradienti di illuminazione, bordi. Il master conserva la maschera e la provenienza dei frame usati. |
| 8. Determinismo | Ordine dei frame canonico, tie-breaking dichiarato, seed per metodi stochastici. |
| 9. Fixture, metriche, soglia e gate | Sintetici con bias, dark, flat e outlier iniettati. Metriche: residuo, rumore, struttura spaziale, stabilità. Soglia dal ground truth. Gate M3. |
| 10. Failure mode, warning e provenance | Frame insufficienti, gruppo incompatibile, flat saturo, bassa illuminazione: rifiuto o warning. Provenienza di ogni frame e di ogni scarto. |
| 11. Ottimizzazione | Accumulatori in streaming, nessuna pila completa in RAM, cache del master. |

### 10.4 C-03 Debayer

| Campo | Contenuto |
|---|---|
| 1. Problema matematico | Ricostruire i canali RGB da un mosaico CFA. È una interpolazione, non una misura dei canali mancanti. Il risultato ha incertezza diversa tra canale misurato e canale interpolato. |
| 2. Dominio, unità e prerequisiti | CFA con pattern noto, offset e rotazione dichiarati, dopo le correzioni disponibili o un passthrough esplicito senza master. Dati lineari e limiti radiometrici dichiarati. Mono e RGB non passano da questo operatore. |
| 3. Ipotesi e limiti | Immagine band-limited, pattern corretto, aliasing contenuto. Non affidabile con pattern ignoto, bordi forti, campionamento insufficiente o pixel invalidi estesi. |
| 4. Alternative e scelta | Nearest, bilineare, edge-aware, metodi frequenziali. La scelta si basa su errore colore, aliasing e risoluzione, non su preferenza estetica. |
| 5. Riferimenti indipendenti e stato | Letteratura demosaicing, per esempio Malvar, He e Cutler 2004. Candidato non letto integralmente. |
| 6. Numerica, stabilità e varianza | f32, nessun clipping dei negativi, guadagni di canale separati. La varianza dei canali interpolati non è quella di un campione misurato. |
| 7. Maschere, metadati e bordi | Pixel CFA invalidi, saturazione, bordi e coverage. Il pattern e l'offset restano nella provenance. |
| 8. Determinismo | Versione algoritmo e parametri fissi. Nessuna scelta casuale del pattern. |
| 9. Fixture, metriche, soglia e gate | CFA sintetico da RGB noto, con pattern e offset. Metriche: errore colore, aliasing, risoluzione, artefatti. Gate M3. |
| 10. Failure mode, warning e provenance | Pattern ignoto: rifiuto, mai scelta silenziosa. X-Trans rinviato con dichiarazione. Provenienza di pattern, offset e algoritmo. |
| 11. Ottimizzazione | Implementazioni separabili, SIMD, GPU in M7. |

Nota: il debayer non è un deblend di sorgenti e non è un modello di PSF. La detection stelle può usare canali o luminanza, ma non deduce i canali mancanti.

### 10.5 C-04 Rilevamento stelle e metriche PSF

| Campo | Contenuto |
|---|---|
| 1. Problema matematico | Rilevare sorgenti puntiformi e stimare centroide, FWHM o HFD, eccentricità, SNR e qualità locale della PSF. La detection non è un modello di deblend e non risolve sorgenti sovrapposte. |
| 2. Dominio, unità e prerequisiti | Immagine lineare calibrata, parzialmente calibrata o senza master, non stirata; CFA trattato con il percorso dichiarato. Pixel scale opzionale. Maschere e stato di calibrazione disponibili. La detection per registrazione non richiede tutti i master; SNR e metriche radiometriche si pubblicano solo con modello di fondo e rumore adeguato. |
| 3. Ipotesi e limiti | Stelle non risolte, PSF localmente approssimabile, fondo liscio. Non affidabile in campi affollati, su nebulosità estesa, stelle sature, cosmici o PSF fortemente variabile. Il deblend di sorgenti sovrapposte è un modello ulteriore, non un effetto della detection. |
| 4. Alternative e scelta | Soglia su componenti connesse, filtro matched, deblending, fit locale. La scelta dipende da crowding e SNR. |
| 5. Riferimenti indipendenti e stato | Letteratura su PSF e fotometria. Candidati non letti integralmente. |
| 6. Numerica, stabilità e varianza | Fit in f64, centroide subpixel, incertezza dai residui o dalla covarianza del fit. La PSF non si deriva dalla sola FWHM. |
| 7. Maschere, metadati e bordi | Saturazione esclusa dal fit, margine ai bordi, cosmici mascherati. La maschera non si perde nel passaggio alla registrazione. |
| 8. Determinismo | Ordine deterministico delle stelle, soglia fissata, tie-breaking dichiarato. |
| 9. Fixture, metriche, soglia e gate | Stelle gaussiane e Moffat con centro, FWHM, ali ed ellitticità noti; campi affollati, sature e con nebulosità. Metriche: completezza, falsi positivi, errore centroide, FWHM e parametri PSF. Gate M3. |
| 10. Failure mode, warning e provenance | Troppe poche stelle: nessuna soluzione, warning. La provenienza registra soglia, maschera, modello e versione. |
| 11. Ottimizzazione | ROI, downsampling per detection, refine a piena risoluzione. |

### 10.6 C-05 Registrazione geometrica

| Campo | Contenuto |
|---|---|
| 1. Problema matematico | Stimare una trasformazione `T` che minimizza il residuo robusto tra stelle corrispondenti, con modello geometrico e outlier. |
| 2. Dominio, unità e prerequisiti | Liste di stelle in coordinate pixel, geometria dell'immagine, WCS opzionale. Le stelle devono provenire da frame compatibili per campo e filtro. |
| 3. Ipotesi e limiti | Campo comune, numero sufficiente di match, modello adeguato alla distorsione. Non affidabile con campo sparso, nuvole, trailing, rotazione estrema o distorsione non modellata. |
| 4. Alternative e scelta | Matching per triangoli, RANSAC, correlazione di fase. La scelta del modello va motivata con i residui, non con la complessità. |
| 5. Riferimenti indipendenti e stato | Letteratura su registrazione e astrometria. Candidati non letti integralmente. |
| 6. Numerica, stabilità e varianza | Fit in f64, residuo RMS e percentile, condizionamento del modello. La covarianza dei parametri geometrici è distinta dalla covarianza fotometrica. |
| 7. Maschere, metadati e bordi | Bordi invalidi, nessuna extrapolazione oltre il supporto, maschere trasferite con la mappatura di coordinate. |
| 8. Determinismo | Seed RANSAC fisso, tie-breaking, ordine dei match. |
| 9. Fixture, metriche, soglia e gate | Trasformazioni sintetiche note con outlier, rumore e distorsione. Metriche: residuo in pixel, percentile, outlier rate, recupero dei parametri. Gate M3. |
| 10. Failure mode, warning e provenance | Nessuna soluzione, ambiguità o pochi match: blocco del ramo dipendente, non pubblicazione di un allineamento dubbio. Provenienza di modello, residui e stelle usate. |
| 11. Ottimizzazione | Riuso delle liste stelle, coarse-to-fine, cache della trasformazione. |

Il plate solving assistito è un caso separato, C-18. Non è una variante nascosta di C-05 e non richiede un catalogo per la registrazione relativa tra frame.

### 10.7 C-06 Resampling e warp

| Campo | Contenuto |
|---|---|
| 1. Problema matematico | Calcolare `y = R x` su una griglia di output, con `C_out = R C_in R^T`, Jacobiano delle coordinate, copertura e flusso dichiarati. |
| 2. Dominio, unità e prerequisiti | Unità coerenti, area del pixel e Jacobiano disponibili, griglia di output definita. Trasformazione lineare locale o non lineare con Jacobiano locale. |
| 3. Ipotesi e limiti | Supporto compatto del kernel, linearità locale, campionamento sufficiente. Non affidabile con forte undersampling, distorsione non rappresentabile o kernel troncato senza dichiarazione. |
| 4. Alternative e scelta | Nearest, bilineare, bicubica, Lanczos, Drizzle. La tabella 8.6.1 è il riferimento. La scelta dipende da stimando, artefatti e costo. |
| 5. Riferimenti indipendenti e stato | Interpolazione numerica, Fruchter e Hook 2002 per Drizzle, standard WCS. Candidati non letti integralmente. |
| 6. Numerica, stabilità e varianza | f32 per il volume, f64 per accumuli e covarianze. Pesi firmati ammessi con politica esplicita. La varianza usa i quadrati dei pesi. |
| 7. Maschere, metadati e bordi | Input invalidi esclusi o output invalidato. Coverage geometrica distinta dalla somma dei pesi. Nessuna rinomormalizzazione universale ai bordi. WCS rifittato o invalido. |
| 8. Determinismo | Tabella del kernel fissa, ordine dei tap fisso, versione algoritmo nella chiave. |
| 9. Fixture, metriche, soglia e gate | Delta, rumore indipendente e correlato, gradiente, shift, rotazione, scala, bordi. Metriche: varianza, covarianza tra output, flusso integrato, brillanza, ringing, copertura. Gate M3. |
| 10. Failure mode, warning e provenance | Trasformazione non valida o WCS non rappresentabile: blocco o rifit. Provenienza di kernel, direzione, coverage e approssimazione di covarianza. |
| 11. Ottimizzazione | Kernel separabili, tile con halo, batch, GPU in M7. |

### 10.8 C-07 Stacking e integrazione

| Campo | Contenuto |
|---|---|
| 1. Problema matematico | Combinare frame allineati con pesi e rejection per stimare `S_hat`, con varianza, coverage e `Neff`. |
| 2. Dominio, unità e prerequisiti | Frame allineati, stesse unità e stesso stimando. Maschere e coverage disponibili. La rejection statistica richiede `N` sufficiente. |
| 3. Ipotesi e limiti | Rumore indipendente o covarianza nota, stesso stimando, trasformazioni già applicate. Non affidabile con `N` basso, correlazioni ignorate o pesi che duplicano il rumore. |
| 4. Alternative e scelta | Media, media pesata, mediana, sigma clipping, winsorized. Per `N = 1` e `N = 2` non si applica rejection statistica. Per `N = 3` la mediana è robusta ma non prova l'outlier. |
| 5. Riferimenti indipendenti e stato | Statistica robusta, per esempio Huber 1981 e Rousseeuw e Leroy 1987. Candidati non letti integralmente. |
| 6. Numerica, stabilità e varianza | Accumuli in f64, varianza generale `w^T C_y w / (sum w)^2` per pesi fissati. `1/sum w` solo con indipendenza e pesi esattamente `1/v_i`. Test di invarianza al riscalamento comune dei pesi, correlazioni, rejection e pesi stimati. |
| 7. Maschere, metadati e bordi | Invalidità, saturazione, bordi, coverage e `Neff` per pixel e canale. Nessun bordo nero nei pixel validi. |
| 8. Determinismo | Ordine canonico dei frame, seed per metodi stochastici, tie-breaking. |
| 9. Fixture, metriche, soglia e gate | `N = 1, 2, 3`, nuvole, cosmici, saturazione, rumore correlato. Metriche: SNR, bias, coverage, perdita di segnale, artefatti. Gate M3. |
| 10. Failure mode, warning e provenance | Frame insufficienti, pixel tutti invalidi, peso zero: warning o rifiuto. Provenienza di frame, pesi, rejection e motivo di scarto. |
| 11. Ottimizzazione | Streaming per tile, cache, parallelismo controllato. |

### 10.9 C-08 Background modeling

| Campo | Contenuto |
|---|---|
| 1. Problema matematico | Stimare un fondo additivo `b(x)` o moltiplicativo `g(x)` e sottrarlo o dividerlo, con selezione dei campioni e protezione del segnale. |
| 2. Dominio, unità e prerequisiti | Dati lineari in ADU o elettroni. Campioni di fondo selezionati. Maschera degli oggetti e bordi disponibili. |
| 3. Ipotesi e limiti | Fondo liscio, campioni liberi da segnale. Non affidabile con nebulosità diffusa che occupa il campo, gradienti ottici complessi o vignettatura scambiata per cielo. |
| 4. Alternative e scelta | Polinomio, spline, RBF, griglia di mediane, multiscala. La scelta dipende dal residuo e dalla protezione del segnale. |
| 5. Riferimenti indipendenti e stato | Letteratura su stima del fondo e statistica robusta. Candidati non letti integralmente. |
| 6. Numerica, stabilità e varianza | Fit in f64, regolarizzazione, evitare overfitting. La varianza del fondo stimato va dichiarata o approssimata. |
| 7. Maschere, metadati e bordi | Maschera oggetto, maschera campioni, bordi. La modalità additiva o divisiva è esplicita. |
| 8. Determinismo | Selezione campioni deterministica, seed se stocastico. |
| 9. Fixture, metriche, soglia e gate | Gradiente sintetico più nebulosa, vignettatura, polvere. Metriche: residuo di fondo, perdita di segnale diffuso, artefatti. Gate M3. |
| 10. Failure mode, warning e provenance | Non identificabile: astensione, non correzione. Non si finge un flat. Provenienza di modello, campioni e modalità. |
| 11. Ottimizzazione | Fit su downsampling, raffinamento locale. |

### 10.10 C-09 Stretch e curve, con asinh P0

| Campo | Contenuto |
|---|---|
| 1. Problema matematico | Applicare una trasformazione monotona dal dato lineare al display o all'export fotografico. Per asinh: `f(k, u) = asinh(k u) / asinh(k)` per `k > 0`; il caso analitico ammesso a priori è `f(0, u) = u`. Il P0 cromatico applica un fattore comune: `I = w_R C_R + w_G C_G + w_B C_B`, con pesi dichiarati e somma uno, e `C' = g(I) C`. Per `k > 0`, `g(I) = f(k, I)/I` se `I != 0` e `g(0) = k/asinh(k)`; per `k = 0`, `g(I) = 1` per ogni `I`. I rapporti RGB con denominatore non nullo si conservano globalmente nel dominio valido pre-gamut. Curve, livelli e STF hanno formule proprie. |
| 2. Dominio, unità e prerequisiti | Dato lineare o non lineare dichiarato. La trasformazione di display non modifica il dato se non è esplicitamente cotta. Asinh richiede input normalizzato con shift e scala dichiarati, un intervallo definito e una politica per i negativi. Il rapporto tra canali ha un contratto separato, nella sezione 10.10.1. |
| 3. Ipotesi e limiti | La trasformazione è display o fotografica, non una misura. Il clipping e la perdita di informazione vanno dichiarati. Asinh per canale non conserva i rapporti RGB; il P0 cromatico usa il fattore comune `g(I)` e conserva i rapporti solo sul dominio valido pre-gamut, rispetto al riferimento dichiarato dopo offset e bilanciamento. Asinh non garantisce la costanza della tinta percepita. GHS non è una promessa cromatica automatica. |
| 4. Alternative e scelta | STF, asinh, GHS, curve, livelli. Asinh è P0 perché è analitico, monotono e reversibile nel dominio dichiarato. GHS resta P1 con formula e test propri. |
| 5. Riferimenti indipendenti e stato | Scienza del colore, standard di display, pratica di stretching astronomico. Candidati non letti integralmente. La formula asinh è elementare e non richiede una derivazione da un concorrente. |
| 6. Numerica, stabilità e varianza | LUT con precisione dichiarata, monotonicità verificata, nessun banding introdotto. `k` nullo, negativo, molto piccolo o molto grande sono casi separati. Shift e scala sono separati da `k`. I negativi e i valori fuori intervallo sono gestiti o rifiutati, non schiacciati in silenzio. Il fattore comune `g(I) = f(k, I)/I` ha limite finito `k / asinh(k)` per `I` che tende a zero; in `I = 0` si usa il limite registrato e non si valuta `0/0`. Se `I` cambia segno o è nullo su un vettore non nullo, il contratto dichiara la politica prima dell'applicazione. La varianza non si propaga con la sola LUT; per il denoise non lineare vale 8.14. |
| 7. Maschere, metadati e bordi | Indicatori di clipping, protezione stelle, maschere di luminanza quando disponibili. Il rapporto tra canali RGB è applicato prima del gamut, non dopo una trasformazione percettiva. Il gamut di display può alterare i rapporti: la trasformazione di visualizzazione è registrata e non viene presentata come neutra. |
| 8. Determinismo | LUT deterministica, parametri canonici, versione algoritmo. |
| 9. Fixture, metriche, soglia e gate | Cuneo di gradiente, stelle, carta colore, casi `k = 0`, `k` negativo, input negativo, fuori range, RGB con canali a zero e clipping di gamut. Metriche: monotonicità, clipping, shift di tinta, rapporto tra canali, banding, errore rispetto al limite analitico. Gate M3 core, M4 UI. |
| 10. Failure mode, warning e provenance | `k = 0` è il caso analitico legale identità, non una risposta a un errore. `k < 0` o non finito produce STOP. Per `k` piccolo un'approssimazione dell'identità è ammessa soltanto con algoritmo versionato e limite d'errore dimostrato sul dominio dichiarato, mai come switch arbitrario o sostituzione dopo errore. Provenienza di trasformazione, clipping e politica dei negativi. |
| 11. Ottimizzazione | LUT, GPU per display in M7. |

La formula asinh del P0 non è GHS e non è una curva generica. GHS richiede una formula validata, un contratto e test propri in M6.

#### 10.10.1 Contratto colore per componenti RGB e reference

Un'affermazione del tipo "il colore è preservato" non è ammessa senza dire in quale stadio e rispetto a quale riferimento. La tabella seguente separa gli stadi.

| Stadio | Effetto sui rapporti RGB | Cosa si può dichiarare |
|---|---|---|
| Offset o black subtraction, anche comune ai canali | `(I_c - b)/(I_d - b)` non è `I_c/I_d`. Un offset comune cambia i rapporti originali. | Il cambiamento è dichiarato. Non si presenta il risultato come rapporto fisico originale. |
| Bilanciamento canali esplicito | Moltiplica ogni canale per un fattore noto. Cambia i rapporti in modo voluto e registrato. | Il fattore è nella provenance e il riferimento è dichiarato. |
| Fattore comune applicato fuori dalla nonlinearità | Un divisore o moltiplicatore comune si cancella nel rapporto. | Il rapporto tra canali trasformati resta quello dei valori trasformati, non quello degli input. |
| Asinh per canale con lo stesso `k` | La nonlinearità comprime ogni canale in modo dipendente dal suo livello. Il rapporto degli output non è il rapporto degli input, salvo il regime lineare `k I` piccolo. | Non si dichiara conservazione dei rapporti RGB. È un percorso di visualizzazione dichiarato, non il P0 cromatico. |
| Asinh cromatico con fattore comune `g(I) = f(k, I)/I`, `I = w_R C_R + w_G C_G + w_B C_B` | `C' = g(I) C`: il fattore è identico sui tre canali, quindi i rapporti si conservano su tutto il dominio valido pre-gamut. | Si conservano i rapporti rispetto al riferimento dichiarato dopo offset e bilanciamenti espliciti. Il gamut di display può alterarli dopo questo stadio. |
| Asinh con `k` diverso per canale | Cambia la tinta in modo non lineare, non riconducibile a un fattore comune. | Non ammesso senza una motivazione e un test di tinta. |
| Mapping di gamut o profilo di display | Può alterare i rapporti dopo la trasformazione. | È una trasformazione di visualizzazione, dichiarata e distinta dal dato. |
| Export fotografico | Il file contiene ciò che è stato cotto, non il dato lineare. | La provenance dice quale stadio è stato cotto e con quali parametri. |

Il riferimento è il vettore RGB dopo offset e bilanciamento, prima del mapping comune e del gamut. La conservazione matematica dei rapporti con denominatore non nullo vale su tutto il dominio valido, non soltanto sulle stelle usate nei test. Non implica conservazione della tinta percepita né dei rapporti precedenti a offset e bilanciamento.

La gestione dei negativi è esplicita. `asinh` è una funzione dispari e accetta valori negativi nel dominio reale, ma un'immagine calibrata può avere negativi legittimi da rumore. Il contratto sceglie una delle tre politiche e la registra:

1. **Dominio reale completo:** i negativi passano nella formula, la LUT è definita su un intervallo simmetrico dichiarato, e il display può mostrare un valore negativo.
2. **Shift dichiarato:** si somma un offset noto prima della trasformazione, e l'offset entra nella provenance e nel calcolo dei rapporti. Non è una neutralità.
3. **Rifiuto del dominio:** i pixel negativi sono mascherati o il job si ferma, con un messaggio.

Il clipping silenzioso a zero non è una quarta politica. Un canale a zero non si usa come denominatore di un rapporto: il rapporto è indefinito e va dichiarato.

Il limite per `k` che tende a zero è analitico: `f(k, u)` tende a `u` per ogni `u` reale, con la normalizzazione dichiarata. In pratica si sceglie un `k` minimo e si usa la formula esatta o l'identità, non una divisione per zero. Per `I` che tende a zero, `f(k, I)/I` tende a `k / asinh(k)`, che è finito; il valore in `I = 0` è definito per continuità come quel limite, se il contratto lo ammette.

Le fixture verificano: rapporto degli input, rapporto dopo offset comune, rapporto dopo bilanciamento, rapporto dopo asinh per canale, rapporto dopo asinh cromatico con fattore comune `g(I)`, caso `k` piccolo, caso `k` grande, negativo, zero, canale nullo, mapping di gamut. Per l'asinh cromatico il test verifica la conservazione su tutto il dominio valido pre-gamut, rispetto al riferimento dichiarato dopo offset e bilanciamenti. Per l'asinh per canale il test verifica che il contratto dichiari la non conservazione. Nessuna trasformazione altera i rapporti in silenzio.

### 10.11 C-10 Export

| Campo | Contenuto |
|---|---|
| 1. Problema matematico | Serializzare pixel, metadati, maschere e provenance senza trasformazioni nascoste. Lo stretch è opzionale ed esplicito. |
| 2. Dominio, unità e prerequisiti | FITS con `BITPIX` float con segno e `BUNIT`; TIFF con profondità e profilo colore dichiarati. Il dato di partenza e la trasformazione applicata sono noti. |
| 3. Ipotesi e limiti | Il writer è corretto, l'endianness è dichiarata, la capacità di metadati è diversa tra formati. FITS e TIFF non hanno le stesse capacità. |
| 4. Alternative e scelta | FITS primary più estensioni; TIFF più sidecar; manifest esterno. La scelta dipende da ciò che il formato può rappresentare. |
| 5. Riferimenti indipendenti e stato | FITS Standard 4.0, specifica TIFF 6.0, ICC.1:2010. Candidati non letti integralmente. |
| 6. Numerica, stabilità e varianza | Nessun passaggio automatico a 8 bit. Roundtrip verificato. NaN e infiniti gestiti o rifiutati. |
| 7. Maschere, metadati e bordi | Maschere e varianza in estensioni opzionali o sidecar. Se il formato non le rappresenta, il manifest lo dichiara. |
| 8. Determinismo | Ordine degli header fisso, timestamp dichiarato, versione writer. |
| 9. Fixture, metriche, soglia e gate | Roundtrip, lettura con strumento esterno, validazione header, file grandi. Gate M3 core, M5 release. |
| 10. Failure mode, warning e provenance | Metadato non rappresentabile: sidecar e warning. Disco pieno: nessun file completo. Provenienza con hash degli output. |
| 11. Ottimizzazione | Scrittura in streaming, nessuna copia inutile. |

### 10.12 C-11 Maschere e composizione

| Campo | Contenuto |
|---|---|
| 1. Problema matematico | Costruire e combinare maschere `m` a valori float in `[0, 1]` per pesare un effetto fotografico. La composizione è una media pesata o una combinazione dichiarata, non una validità scientifica. |
| 2. Dominio, unità e prerequisiti | Immagine di riferimento con geometria, maschera di invalidità, coverage, sorgente della maschera e revisione. Maschere di luminanza, stelle, sfondo e oggetto hanno prerequisiti propri. La maschera di luminanza non richiede lo stretch. La maschera stelle richiede detection, non background. La maschera oggetto può richiedere una stima di fondo. |
| 3. Ipotesi e limiti | La maschera è una preferenza o una segmentazione, non una verità semantica. Una maschera sbagliata altera il risultato. Feather, morfologia e soglie cambiano i bordi. La maschera non è alpha, non è invalidità e non è coverage. |
| 4. Alternative e scelta | Brush, poligono, luminanza, detection stelle, stima sfondo, segmentazione oggetto, combinazione. La versione P0 include brush, poligono, morfologia, feather, invert, combine, opacity e salvataggio. La segmentazione avanzata e il colore selettivo sono P1. |
| 5. Riferimenti indipendenti e stato | Morfologia matematica, compositing, segmentazione. Candidati non letti integralmente. Nessuna dipendenza da un formato di maschera di un concorrente. |
| 6. Numerica, stabilità e varianza | Maschera f32 in `[0, 1]`, accumuli e morfologia in f64 quando serve. Il feather non deve creare valori fuori intervallo. La composizione con opacity è `m_out = clip((1 - alpha) m_a + alpha m_b, 0, 1)` con `alpha` dichiarato. L'invert è `1 - m`. Il combine è dichiarato, non implicito. |
| 7. Maschere, metadati e bordi | La maschera di invalidità resta separata e prevale sull'effetto. I pixel invalidi non diventano validi perché la maschera di editing è uno. I bordi del brush e del poligono sono definiti. La maschera conserva la revisione della sorgente. |
| 8. Determinismo | Rasterizzatore versionato, eventi ordinati, seed se stochastico, nessuna dipendenza dall'ordine di UI non registrato. La stessa maschera e la stessa revisione producono lo stesso raster. |
| 9. Fixture, metriche, soglia e gate | Gradiente, stelle, bordo di oggetto, feather, invert, combine, opacity, maschera vuota, maschera tutta uno, ciclo di dipendenza, asset mancante, riapertura del progetto. Metriche: errore di bordo, copertura della maschera, riproducibilità. Gate M3 core, M4 editing. |
| 10. Failure mode, warning e provenance | Maschera vuota o degenere: warning. Ciclo: rifiuto. Asset mancante: nodo invalido. Provenienza di sorgente, revisione, rasterizzatore ed eventi. |
| 11. Ottimizzazione | Tile, preview a bassa risoluzione, cache del raster, nessuna perdita della maschera piena. |

La maschera di editing non sostituisce la maschera di invalidità. Un pixel con maschera di editing alta e invalidità vera resta invalido.

### 10.13 C-12 Denoise multiscala

| Campo | Contenuto |
|---|---|
| 1. Problema matematico | Stimare una versione denoised `x_hat` di un'immagine `x` con rumore noto o stimato, usando una trasformazione à trous non decimata e soft shrinkage dei coefficienti. Il modello è `x = s + n`, con `n` per scala e correlazioni dichiarate. |
| 2. Dominio, unità e prerequisiti | Dati lineari o non lineari dichiarati. Per il modello lineare servono rumore per scala, gain, offset e maschere. Per il modello non lineare servono la trasformazione `f`, il suo Jacobiano o un modello empirico, e il clipping. CFA, RGB e multicanale sono dichiarati. |
| 3. Ipotesi e limiti | Il rumore è stazionario o eteroschedastico secondo il modello. La correlazione tra pixel e tra scale è dichiarata. Il soft shrinkage riduce rumore e può ridurre segnale debole. Non esiste una garanzia universale di preservare ogni nebulosa. Il denoise non è una misura e non inventa dettaglio. |
| 4. Alternative e scelta | À trous con soft shrinkage, metodi spaziali, metodi multiscala alternativi. Il P0 sceglie à trous non decimato con soft shrinkage perché è separabile, controllabile e testabile. Metodi avanzati e ML restano P1/M8. |
| 5. Riferimenti indipendenti e stato | Letteratura su wavelet à trous, shrinkage e stima del rumore. Candidati non letti integralmente. Nessuna formula derivata da un concorrente. |
| 6. Numerica, stabilità e varianza | Coefficienti e soglie in f64 nei passaggi critici; output f32 quando sicuro. La varianza lineare non si copia sul dato non lineare. Per `z = f(x)` si usa `Cz ~= Jf Cx Jf^T` con i limiti di 8.14. La soglia per scala e il rumore per scala sono registrati. Gli errori di shrinkage e selezione sono misurati separatamente. |
| 7. Maschere, metadati e bordi | Maschere di invalidità e saturazione escluse. I bordi usano una politica dichiarata, non un padding che finge dati. Il rumore ai bordi non si inventa. Coverage e alpha restano separati. |
| 8. Determinismo | Trasformazione deterministica, ordine delle scale fisso, seed se il modello di rumore è simulato, versione algoritmo. |
| 9. Fixture, metriche, soglia e gate | Rumore bianco, colorato, correlato ed eteroschedastico; segnale noto; stelle; nebulosa debole; dati lineari e non lineari con `f` nota; repliche indipendenti. Metriche: riduzione NPS, bias, perdita di struttura debole, distorsione stelle, artefatti, varianza residua. Il test non richiede di ridurre il NPS a un solo valore obiettivo. Gate M3 core, M4 UI. |
| 10. Failure mode, warning e provenance | Rumore non stimabile, trasformazione ignota, clipping esteso: astensione o confidenza ridotta. Provenienza di modello, scale, soglie, rumore e backend. |
| 11. Ottimizzazione | Piramide à trous con buffer riusati, tile con halo, f64 solo nei passaggi critici, GPU in M7. |

Il denoise P0 non è "lineare soltanto". Il modello lineare e quello non lineare sono distinti. Per il non lineare non si copia la varianza e non si dichiara impossibile la stima.

### 10.14 C-13 Sharpening e contrasto locale

| Campo | Contenuto |
|---|---|
| 1. Problema matematico | Aumentare il contrasto di bande spaziali selezionate con un guadagno bounded, e applicare un contrasto locale con una maschera di protezione. La piramide non decimata si definisce con `c_0 = x`, `c_j = smooth_j(c_{j-1})`, `w_j = c_{j-1} - c_j` per `j = 1..J`, e l'output è `x_hat = x + sum_j g_j w_j`, con `g_j` limitato e dichiarato. `smooth_j` è il filtro della scala `j`, non un filtro implicito. La banda residua `w_j` è la differenza tra due livelli consecutivi della stessa piramide: non esiste una banda `x_b` separata. |
| 2. Dominio, unità e prerequisiti | Dati lineari o non lineari dichiarati, maschere di invalidità e clipping, stima del rumore, stelle e PSF quando disponibili. Il sharpening non richiede deconvoluzione. |
| 3. Ipotesi e limiti | Il dettaglio è nella banda scelta. Un guadagno alto amplifica rumore e ringing. Un contrasto locale può creare aloni. Non si promette recupero di dettaglio non campionato. HDR avanzato non è P0. |
| 4. Alternative e scelta | Bande à trous, unsharp mask, contrasto locale, HDR multiscala. Il P0 usa bande bounded e contrasto locale con guardie. HDR avanzato resta P1. L'infrastruttura multiscala può essere condivisa con C-12 quando validata, ma il contratto e le metriche restano distinti. |
| 5. Riferimenti indipendenti e stato | Letteratura su unsharp masking, wavelet e contrasto locale. Candidati non letti integralmente. |
| 6. Numerica, stabilità e varianza | f64 per il calcolo delle bande e dei guadagni, output f32 se sicuro. Il guadagno è limitato e dichiarato. Il rumore amplificato è misurato. Il clipping è rilevato prima e dopo. |
| 7. Maschere, metadati e bordi | Maschere stelle, sfondo, luminanza e oggetto. Protezione delle stelle e delle transizioni. I bordi non si inventano. La maschera di invalidità prevale. |
| 8. Determinismo | Bande, ordine e guadagni fissi, versione algoritmo, nessun parametro nascosto dalla UI. |
| 9. Fixture, metriche, soglia e gate | Gradiente, stelle, rumore, nebulosa debole, bordi, clipping. Metriche: alone, ringing, clipping, rumore amplificato, distorsione stelle, dettaglio falso. Confronto prima e dopo. Gate M3 core, M4 UI. |
| 10. Failure mode, warning e provenance | Guadagno fuori limite, clipping eccessivo, rumore amplificato: warning o rifiuto. Provenienza di bande, guadagni, maschere e clipping. |
| 11. Ottimizzazione | Bande separabili, tile con halo, cache delle bande, GPU in M7. |

Il sharpening non è una licenza a inventare dettaglio. Un aumento di contrasto che peggiora aloni, ringing o stelle non è un miglioramento.

### 10.15 C-14 Riduzione e separazione delle stelle

| Campo | Contenuto |
|---|---|
| 1. Problema matematico | Stimare una componente stellare `S_L` da una PSF locale e da un fondo, esclusivamente sull'antenato lineare `I_L`. La riduzione lineare è `I_L - alpha S_L` con `alpha` in `[0, 1]`; la separazione lineare è `R_L = I_L - S_L` con `R_L + S_L = I_L`. Per una visualizzazione non lineare `T` si definiscono `R_T = T(I_L - S_L)` e `S_T = T(I_L) - T(I_L - S_L)`, quindi `R_T + S_T = T(I_L)` in modo esatto. Il fit e la sottrazione non si eseguono mai sul dato non lineare. Se l'antenato lineare manca, non si inventa un fit: il job si ferma con errore esplicito. |
| 2. Dominio, unità e prerequisiti | Antenato lineare `I_L` disponibile e dichiarato, stelle non sature o mascherate, PSF locale, fondo locale, maschere. La riduzione P0 richiede sorgenti identificabili. La separazione limitata richiede un modello di componente e un test di ricostruzione. Un'immagine solo non lineare non è un dominio valido per il fit. |
| 3. Ipotesi e limiti | La PSF è locale, il fondo è stimabile e il fit avviene sull'antenato lineare. In campi affollati, stelle sature, PSF sbagliata o nebulosa compatta il modello non è identificabile. La separazione non è una verità semantica e non ricostruisce stelle mancanti, nuclei o nebulosità. La riduzione altera il flusso in modo fotografico legittimo, non conserva media e varianza in modo universale. La neutralità di `R_T + S_T = T(I_L)` è contabile, non radiometrica. |
| 4. Alternative e scelta | Riduzione PSF frazionaria, sottrazione del modello, separazione componente, metodi ML. Il P0 usa fit PSF e riduzione controllata; la separazione limitata è inclusa solo dove il modello è validato. Il general starless e i metodi ML restano P1/M8. |
| 5. Riferimenti indipendenti e stato | Letteratura su PSF, fitting stellare, separazione di componenti. Candidati non letti integralmente. Nessuna formula derivata da un concorrente. |
| 6. Numerica, stabilità e varianza | Fit in f64 sull'antenato lineare, PSF normalizzata, centro e supporto dichiarati. La PSF non si deriva dalla sola FWHM. La varianza della componente stimata è distinta dalla varianza dell'immagine. Il clipping e i negativi sono gestiti. `alpha = 0` è identità; `alpha = 1` è rimozione piena secondo il modello. Per la visualizzazione non lineare, `T` è applicata una volta a `I_L` e una volta a `I_L - S_L`, con la stessa versione; la somma `R_T + S_T` è verificata numericamente contro `T(I_L)`. |
| 7. Maschere, metadati e bordi | Maschere di saturazione, invalidità, crowding e bordi. Le stelle sature non si ricostruiscono. La maschera di editing non rende valido un pixel invalido. |
| 8. Determinismo | Ordine delle stelle, seed del fit, versione del modello, tie-breaking. |
| 9. Fixture, metriche, soglia e gate | PSF gaussiana e Moffat note, ali, ellitticità, crowding, saturazione, nebulosa compatta, fondo variabile, stelle sintetiche, coppia lineare e non lineare con `T` nota, assenza dell'antenato lineare. Metriche: errore di ricostruzione, residuo, artefatti, distorsione del fondo, perdita di segnale diffuso, fedeltà delle stelle non rimosse, scarto `R_T + S_T - T(I_L)`. Gate indipendenti per riduzione e separazione, M3 core, M4 UI. |
| 10. Failure mode, warning e provenance | Antenato lineare assente, crowding, saturazione, PSF incerta, componente non identificabile: errore o astensione. Provenienza di PSF, fondo, maschere, alpha, modello, trasformazione `T` e antenato lineare. |
| 11. Ottimizzazione | Fit locale su ROI, PSF condivisa per gruppo, tile con halo, cache del modello. |

La formula `R_L + S_L = I_L` non vale se `S_L` viene modificata in un secondo momento. Se l'utente edita `S_L` in `S_L'`, la reintegrazione non è neutra. Il contratto conserva la stessa `S_L` o dichiara la differenza. Per il display, la neutralità esatta è `R_T + S_T = T(I_L)` e non autorizza un fit sul dato trasformato.

Il preflight di F-IO-10 e dell'Advisor deve spiegare che un import soltanto non lineare, senza antenato radiometricamente lineare verificabile, non ammette C-14. Non si ricostruisce un antenato con un'inversione inventata dello stretch e non si usa un algoritmo sostitutivo P0. Il test M3.n copre import NL isolato, NL con antenato valido e linearità sconosciuta; verifica il rifiuto prima del fit e la diagnostica visibile in M4. Si valuta separatamente un eventuale operatore fotografico nativo NL come proposta P1, con contratto, fonti e validazione propri: non è promesso né sostituisce C-14.

#### 10.15.1 Gate della riduzione stellare

La riduzione è un effetto fotografico controllato. Il gate è indipendente da quello della separazione e non condivide il criterio di reintegrazione.

| Aspetto | Regola |
|---|---|
| Oggetto | `I_L - alpha S_L`, con `alpha` in `[0, 1]` e `S_L` componente stimata sull'antenato lineare. Per il display si applica `T` una volta al risultato lineare. |
| Limiti di frazione | `alpha` è limitato per stella o per regione. Una frazione elevata su stelle in crowding, sature o vicine a transizioni produce un danno misurabile e richiede astensione. |
| Danno | Si misura la perdita di flusso non stellare, il cambio di colore, il ringing attorno alle stelle, l'alterazione delle transizioni e la deformazione delle stelle non rimosse. |
| Oggettività | Il confronto usa metriche prima e dopo e una baseline no-op. Non usa solo la preferenza visiva. |
| Identità | `alpha = 0` è identità. La riduzione non promette che il risultato sia identico all'originale per `alpha > 0`. |
| Reintegrazione | La riduzione non rivendica `R_L + S_L = I_L`. Quella identità appartiene alla separazione con la stessa `S_L` non editata. |
| Astensione | Crowding non risolto, saturazione, PSF incerta, transizione non modellabile: la riduzione non viene proposta come sicura. |

La riduzione può migliorare la leggibilità della nebulosa e allo stesso tempo alterare il colore delle stelle residue. Il gate accetta il trade-off solo se il danno è misurato e mostrato.

#### 10.15.2 Gate della separazione limitata

La separazione produce due componenti e una relazione numerica verificabile.

| Aspetto | Regola |
|---|---|
| Oggetto | `R_L = I_L - S_L` sull'antenato lineare, con `S_L` componente stellare del modello; per il display `R_T = T(I_L - S_L)` e `S_T = T(I_L) - T(I_L - S_L)`. |
| Identità neutra | `R_L + S_L = I_L` vale solo con la stessa `S_L`, non editata, e con la stessa geometria. `R_T + S_T = T(I_L)` vale per costruzione, con la stessa `T`. |
| Reintegrazione | La reintegrazione usa i due rami originali. Se uno è editato, il contratto conserva l'originale e dichiara la differenza; non presenta la somma come identità. |
| Fedeltà | Si misura l'errore di ricostruzione, il residuo non stellare, la distorsione del fondo, la perdita di segnale diffuso e la fedeltà delle stelle non separate. |
| Limiti | La separazione è limitata alle sorgenti identificabili. Non è una verità semantica e non ricostruisce stelle mancanti, nuclei saturi o nebulosità compatta. |
| Gate indipendente | Il superamento del gate di separazione non implica il superamento del gate di riduzione, e viceversa. |
| Astensione | Componente non identificabile, saturazione o crowding: astensione, non separazione arbitraria. |

La separazione è più esigente della riduzione perché promette una relazione numerica tra due output. Per questo ha un gate proprio e un test di reintegrazione separato.

### 10.16 C-15 Deconvoluzione a PSF fissa

| Campo | Contenuto |
|---|---|
| 1. Problema matematico | Stimare `x` da osservazioni lineari `y = H x + b + epsilon`, con `H` convoluzione per una PSF fissa, `b` fondo noto o separato, e `epsilon` con covarianza modellata. Si opera sul sottospazio dei pixel validi: `y_V = P y`, `H_V = P H`, `b_V = P b`, `C_V = P C_y P^T`, con `P` proiezione sui pixel validi. Il P0 minimizza la norma al quadrato pesata `norm(W^{1/2} (H_V x + b_V - y_V))^2 + lambda * norm(L x)^2`, con `W` matrice dei pesi PSD finita che approssima `C_V^{-1}` e `lambda >= 0`. |
| 2. Dominio, unità e prerequisiti | Dato radiometricamente lineare, unità dichiarate, PSF normalizzata e centrata con supporto e errore noti, maschere di invalidità e saturazione, fondo `b` noto o modello separato, pesi validi. La PSF non si deriva dalla sola FWHM. |
| 3. Ipotesi e limiti | Il modello di convoluzione è locale e shift-invariant nel supporto dichiarato. La PSF è nota, non stimata insieme a `x`. Il fondo non è ambiguo rispetto al segnale. Il rumore è descritto da `W` o da una sua approssimazione dichiarata. Il metodo non è blind e non è una PSF variabile. |
| 4. Alternative e scelta | Minimi quadrati regolarizzati a PSF fissa, Wiener a filtro fissato, Richardson-Lucy non lineare. Il P0 sceglie i minimi quadrati regolarizzati per trasparenza e perché il problema è lineare a PSF fissa. Con `H`, `W`, `L`, `lambda` e `b` tutti fissi, lo stimatore è lineare o affine in `y`; se la PSF, `lambda` o il modello di rumore sono stimati dai dati, l'intera pipeline diventa adattiva e non lineare. RL resta P1 e non viene descritto come lineare. |
| 5. Riferimenti indipendenti e stato | Letteratura su deconvoluzione regolarizzata, Richardson 1972, Lucy 1974, Wiener 1949. Candidati non letti integralmente. Nessuna derivazione da un concorrente. |
| 6. Numerica, stabilità e varianza | `W` è una precisione PSD che approssima `C_V^{-1}` sul sottospazio dei pixel validi. Per `W` PSD finita e `lambda >= 0` esiste almeno un minimizzatore. Per `lambda > 0` l'unicità vale se `ker(W^{1/2} H_V) intersecato con ker(L) = {0}`. `lambda` da solo non basta se `L` ha un nullspace che interseca il kernel. L'unicità non implica stabilità: una soluzione unica può amplificare rumore e artefatti, e il condizionamento va misurato. `W` diagonale è una approssimazione dichiarata, non `C_V^{-1}` in presenza di rumore correlato. `b_V` è fissato e stimato a monte, con la sua incertezza dichiarata e propagata; il fit congiunto di `b` e `x` è P1. Se `b` e `x` sono entrambi liberi e non vincolati, il problema è degenere. |
| 7. Maschere, metadati e bordi | Maschera di invalidità e saturazione. La maschera di osservazione non è il blending fotografico `(1 - m) y + m x_hat`; il blending è un raffinamento separato con un gate proprio. `H` e il suo aggiunto sono definiti, il padding e le condizioni al bordo sono dichiarati, la periodicità FFT è esplicita. L'inversa non è locale e il supporto finito non garantisce l'assenza di aloni. |
| 8. Determinismo | PSF, supporto, normalizzazione, `W`, `L`, `lambda`, criterio di stop e versione algoritmo nella chiave. Il criterio di scelta di `lambda` e l'arresto sono preregistrati. Nessuna garanzia empirica universale. |
| 9. Fixture, metriche, soglia e gate | PSF gaussiana e Moffat note, PSF errata, rumore bianco e correlato, maschere, saturazione, fondo noto e incerto, negativi, bordi, periodicità. Metriche: residuo, ringing, overshoot, bias, varianza, perdita di dettaglio, stabilità, condizionamento. Gate M3 core, M4 UI. |
| 10. Failure mode, warning e provenance | PSF invalida, `W` non PSD, `lambda` non valido, sistema degenere, maschera incompatibile, memoria insufficiente: rifiuto motivato. I negativi legittimi non si correggono con un offset arbitrario e non si eliminano con un clamp silenzioso. Provenienza completa. |
| 11. Ottimizzazione | Matrice-free o fattorizzazione validata, piano FFT con padding e buffer complessi, preflight della working memory, iterazioni bounded. Il tiling automatico non è una verità: se il problema è globale, il contratto lo dichiara. GPU in M7. |

La covarianza dell'output non è quella dell'input riusata. La mappa di incertezza dichiara se è una approssimazione o se è sconosciuta. La capability di misura riflette il modello.

Il protocollo di sensibilità è preregistrato e usa una baseline no-op. Si perturba la PSF entro la sua incertezza dichiarata, si perturba il fondo `b`, si varia `lambda` su una griglia registrata e si usano realizzazioni di rumore coerenti con `W`. Per ogni punto si misurano residuo ricondotto, ringing, overshoot, negativi, perdita di dettaglio e stabilità. Un residuo piccolo dopo la riconvoluzione non dimostra che la PSF sia vera: la riconvoluzione con la stessa PSF usata nella soluzione può nascondere un errore di modello. Il criterio di stop e la scelta di `lambda` non si ottimizzano sul risultato osservato senza registrare la procedura.

Se la PSF varia sul campo oltre il dominio dichiarato, la deconvoluzione non viene eseguita su tutta l'immagine come se fosse shift-invariant. Il contratto può permettere una PSF locale su ROI dichiarate, oppure il sistema si astiene. Una PSF mediocre non si compensa con più iterazioni. La deconvoluzione ordinaria a PSF fissa non è una garanzia di recupero del dettaglio: è un'ipotesi di modello con un dominio e un errore.

### 10.17 C-16 Colore relativo broadband

| Campo | Contenuto |
|---|---|
| 1. Problema matematico | Stimare fattori di scala cromatici relativi tra canali broadband usando stelle di riferimento e un catalogo di colore, con risposta strumentale dichiarata e trasformazioni di banda. Non si stima una SPD assoluta né una calibrazione fisica universale. |
| 2. Dominio, unità e prerequisiti | Immagine lineare, canali broadband dichiarati, WCS valido, stelle non sature, fondo locale stimato, apertura e unità compatibili, catalogo offline con versione, licenza e autenticazione. La risposta del filtro e del sensore è nota dove si dichiara una trasformazione. |
| 3. Ipotesi e limiti | Le stelle di calibrazione sono comuni, non sature e non variabili in modo non modellato. Il colore del catalogo e la banda sono compatibili. Il moto proprio e l'epoca sono considerati. Outlier e stelle binarie sono gestiti. Il risultato è relativo, non assoluto. Una risposta sconosciuta riduce la confidenza o impone l'astensione. |
| 4. Alternative e scelta | Colore relativo con catalogo offline, SPCC completa con risposta spettrale, bilanciamento manuale. Il P0 APPROVATO usa il colore relativo perché è verificabile offline. SPCC completa resta P1 e non garantisce automaticamente calibrazione fotometrica assoluta. Il bilanciamento manuale è un percorso primario scelto a priori, non una calibrazione fisica né una sostituzione dopo errore. |
| 5. Riferimenti indipendenti e stato | Cataloghi stellari, sistemi fotometrici, trasformazioni di banda, letteratura su calibrazione del colore. Candidati non letti integralmente. Licenze e disponibilità dei cataloghi da verificare. |
| 6. Numerica, stabilità e varianza | Fit in f64, pesi da incertezza, apertura matched, correzione del fondo, outlier robusti. Il rapporto tra canali è applicato prima del gamut. Non si garantisce la costanza della tinta percepita. La varianza dei fattori è propagata o dichiarata. |
| 7. Maschere, metadati e bordi | Stelle sature, bordi, cosmici e pixel invalidi esclusi. Holdout di stelle non usate nel fit. Moto proprio ed epoca. Il WCS deve essere valido; un WCS invalido blocca o richiede rifit. |
| 8. Determinismo | Catalogo, versione, selezione, seed, tie-breaking, ordine delle stelle e versione algoritmo. |
| 9. Fixture, metriche, soglia e gate | Stelle sintetiche con colore noto, catalogo simulato, outlier, saturazione, moto proprio, errore di banda, WCS errato, pack mancante. Metriche: residui sul fit e sull'holdout, dispersione, bias di colore, stabilità tra sessioni. Gate M3 core, M4 UI. |
| 10. Failure mode, warning e provenance | Poche stelle, saturazione, WCS invalido, catalogo incompatibile, risposta sconosciuta: astensione o errore esplicito. Provenienza di catalogo, versione, bande, risposta, holdout e residui. |
| 11. Ottimizzazione | Indice locale, cache del catalogo, ROI, nessun download automatico. |

Il colore relativo non è una garanzia di colore fisico. Se la risposta è sconosciuta, il risultato è una stima relativa con confidenza ridotta, non una calibrazione assoluta.

La SPCC completa è un'altra cosa: stima la risposta spettrale e richiede curve, catalogo e modello di banda, e resta P1 in M6.b. La calibrazione broadband relativa di C-16 non si presenta come SPCC completa, non usa la stessa formulazione e non ne eredita il gate.

Un WCS risolto non rende valida la calibrazione del colore. Il solver fornisce coordinate; il colore richiede una trasformazione ammessa, un riferimento, una risposta strumentale supportata e bande compatibili. La tabella seguente separa i residui e i limiti.

| Elemento | Cosa misura | Cosa non dimostra |
|---|---|---|
| Residuo di matching astrometrico | Scarto tra posizioni rilevate e catalogo dopo la trasformazione. | Non misura il colore e non valida la risposta del filtro. |
| Residuo del fit fotometrico | Scarto tra colori misurati e colori di riferimento sulle stelle usate. | Non è una stima dell'errore sistematico e non copre la nebulosa. |
| Copertura del campione stellare | Distribuzione dei colori e delle magnitudini delle stelle usate. | Un fit buono su stelle poco colorate non copre l'intervallo della nebulosa. |
| Limite sistematico di colore | Estinzione, banda, risposta del sensore, trasformazione, epoca e moto proprio. | Non si riduce azzerando il residuo del fit. |
| Colore della nebulosa | Emissione o riflessione con spettro diverso dalle stelle di riferimento. | Un buon fit stellare non certifica il colore della nebulosa. |

Le soglie non si inventano: si identificano dai dati, dal catalogo e dalla risposta dichiarata, oppure restano aperte. Un intervallo di bande non supportato produce astensione. Il report mostra residui di matching, residui fotometrici, copertura del campione e limiti sistematici separati. Il colore relativo è una stima verificabile nel dominio dichiarato, non un certificato di fedeltà cromatica dell'intera immagine.

### 10.18 C-17 Strategia e composizione di operatori

| Campo | Contenuto |
|---|---|
| 1. Problema matematico | Cercare un insieme di sequenze parziali legali di operatori che migliorino un vettore di obiettivi sotto vincoli tipizzati e un budget. Non è una permutazione libera e non massimizza un punteggio universale di bellezza. |
| 2. Dominio, unità e prerequisiti | Stato dei dati con capability, unità, geometria, CFA, linearità, calibrazione, maschere e revisione. Registro operatori fidato. Template versionati. Ruleset dichiarativo con allowlist. Preferenze e budget. |
| 3. Ipotesi e limiti | L'ordine è parziale. Gli operatori non commutano per default. Alcune sequenze sono illegali, non solo peggiori. L'holdout non è indipendente se viene riusato dopo la taratura. La ricerca non è esaustiva. La qualità estetica non ha un ground truth unico. |
| 4. Alternative e scelta | Template fissi, ricerca greedy, beam search, enumerazione bounded, ricerca con pruning. Il P0 usa template e ricerca bounded con deduplicazione esatta e pruning dichiarato. Non promette l'ottimo globale. |
| 5. Riferimenti indipendenti e stato | Pianificazione, ricerca su grafi, vincoli, statistica della selezione, teoria delle decisioni multi-obiettivo. Candidati non letti integralmente. Nessuna dipendenza da un pianificatore esterno. |
| 6. Numerica, stabilità e varianza | Ogni candidato ha metriche con unità, dominio e incertezza. La deduplicazione è esatta solo per identità numerica o equivalenza provata. La similarità empirica non fonde candidati. Le metriche di domini diversi non si sommano in un numero privo di significato. La varianza delle metriche e la molteplicità sono dichiarate. |
| 7. Maschere, metadati e bordi | ROI stratificate, holdout scelto prima della taratura, maschere di invalidità, coverage e bordi. Le ROI non sono l'intera immagine. L'holdout non si riusa in modo adattivo. Il full resolution è streamed solo se il contratto globale è equivalente. |
| 8. Determinismo | Template, ruleset, preferenze, budget, seed, ordine di esplorazione, cache e checkpoint. Il budget logico è riproducibile; il timeout hard è una sicurezza best effort, non una garanzia di cancel istantaneo. |
| 9. Fixture, metriche, soglia e gate | Sequenze legali e illegali, doppia calibrazione, riordino non permesso, maschera ciclica, budget esaurito, cache crash, candidato non valutabile, holdout leakage, pack mancante. Metriche: legalità, regressione, artefatti, costo, copertura, stabilità della scelta. Gate M3 core, M4 UI. |
| 10. Failure mode, warning e provenance | Cinque stati distinti: no-op, astensione, alternativa provata inadatta, operatore non disponibile, budget o ricerca incompleta. Un no-op è un esito legittimo solo quando il protocollo lo prevede; dopo un errore non è una sostituzione e non si promuove un nuovo candidato al posto di quello fallito. Una sequenza illegale viene rifiutata alla costruzione e alla pubblicazione. Provenienza di ogni candidato, metrica, ROI, holdout, costo, fallimento e decisione umana. |
| 11. Ottimizzazione | Cache separate per artefatto, valutazione e ricerca. Pruning per illegalità, costo minimo ed equivalenza esatta. Il pruning euristico è dichiarato e non promette l'ottimo. La frontiera riportata è quella esplorata, non tutto lo spazio possibile. |

Il confronto tra strategie usa un endpoint antecedente lineare immutabile per le misure ammesse e un endpoint fotografico con geometria, maschera spaziale, coverage, unità, scala, profilo RGB e rendering dichiarati. Non si confronta un SNR lineare con un risultato non lineare come se fosse lo stesso stimando. Un confronto incompatibile è `NON CONFRONTABILE`, non zero. Non si eliminano i pixel difficili per migliorare il punteggio.

L'Advisor può proporre una strategia, ma non applicarla senza conferma se la modifica è sostanziale. Il branch è reversibile. L'utente vede perché, in quale ordine, con quali prerequisiti, benefici, rischi e costi.

#### 10.18.1 Template P0 ammessi e confronto obbligatorio

I template sono sequenze tipizzate, versionate e legali per costruzione. Non sono una permutazione libera e non si generano a runtime. La tabella fissa il set P0; ogni variante dichiara gli operatori, l'ordine e l'endpoint.

| Template | Sequenza | Uscita | Uso |
|---|---|---|---|
| **T0 baseline no-op** | Antenato lineare immutabile; nessuna modifica numerica | Rendering fotografico dell'antenato con gli stessi asinh, profilo e parametri di rendering congelati per A/B | Baseline numerica distinta dalla sua vista fotografica; mai sostituzione dopo errore |
| **T-A** | Background comune → stesso bilanciamento → asinh cromatico → denoise non lineare | Endpoint fotografico non lineare | Modello di rumore non lineare valido richiesto |
| **T-B** | Background comune → stesso bilanciamento → denoise lineare → stesso asinh cromatico | Endpoint fotografico non lineare | Cambia soltanto la posizione del denoise e il relativo contratto di dominio |
| **T-C** | Background → separazione stellare lineare su `I_L` → riduzione o separazione → reintegrazione → asinh cromatico | Endpoint fotografico non lineare | Percorso P0 con stelle |
| **T-C1 starless lineare** | Background → separazione su `I_L` → export di `R_L = I_L - S_L` | Endpoint lineare starless | Variante dichiarata, non un risultato fotografico completo |
| **T-C2 riduzione lineare** | Background → fit `S_L` su `I_L` → `I_L - alpha S_L` → asinh | Endpoint fotografico non lineare | Variante con riduzione controllata |
| **T-C3 separazione lineare** | Background → fit `S_L` su `I_L` → `R_L`, `S_L` → reintegrazione con la stessa `S_L` | Due rami lineari più ricomposizione | Variante con identità `R_L + S_L = I_L` |

Il gate P0 richiede casi del corpus nei quali A e B sono entrambi ammissibili e vengono confrontati con T0. Non richiede entrambi su ogni immagine: se uno è illegale, il report dichiara `NON CONFRONTABILE`, senza eseguirlo o sostituirlo dopo un errore. Background e bilanciamento comuni sono congelati a monte; asinh, profilo, geometria, supporto spaziale e rendering sono identici, senza STF automatica indipendente. Non si confrontano sigma o SNR lineari con quelli non lineari. Le metriche lineari usano gli antenati compatibili; quelle fotografiche usano l'endpoint comune. La ricerca dei parametri del denoise rispetta i due contratti e registra il costo. Le varianti stellari partono dall'antenato lineare. D-19 è APPROVATA P0: un template aggiuntivo con deconvoluzione richiede dominio valido e gate scientifici superati; non cambia la coppia controllata A/B. Le varianti starless lineari non sono equivalenti agli endpoint fotografici.

### 10.19 C-18 Plate solving assistito

| Campo | Contenuto |
|---|---|
| 1. Problema matematico | Stimare una soluzione WCS da pattern stellari, dati un intervallo di posizione e scala e un catalogo offline. La soluzione include centro, scala, rotazione, eventuale modello di distorsione limitato e residui. |
| 2. Dominio, unità e prerequisiti | Immagine con stelle non sature, detection, FOV, intervallo di posizione e scala fornito dall'utente o da metadati verificati, catalogo e indice con versione, licenza e autenticazione. Un WCS precedente può essere un hint, non una verità. |
| 3. Ipotesi e limiti | Il campo è nel dominio bounded, il hint è abbastanza vicino, il catalogo copre la regione, la distorsione è limitata. Fuori dominio il solver fallisce esplicitamente. Non è un blind solver generale e non garantisce una soluzione con poche stelle o hint sbagliati. |
| 4. Alternative e scelta | Matching per triangoli, hash geometrico, correlazione, solver blind. Il P0 usa un solver assistito con hint e validazione. Il blind solving e i modelli completi restano P1. |
| 5. Riferimenti indipendenti e stato | Standard WCS, cataloghi astrometrici, letteratura su plate solving. Candidati non letti integralmente. Nessuna dipendenza da un servizio esterno o da Siril. |
| 6. Numerica, stabilità e varianza | Fit robusto in f64, RANSAC con seed, residui, covarianza dei parametri, ambiguità. La soluzione è validata su stelle tenute fuori dal fit. La scala e la posizione sono intervalli, non certezze. |
| 7. Maschere, metadati e bordi | Saturazione, bordi, cosmici, crowding, catalog bounds, epoca e moto proprio. Un catalogo che non copre il campo produce un errore, non una soluzione inventata. |
| 8. Determinismo | Versione di catalogo e indice, seed, tie-breaking, ordine dei match, versione algoritmo. |
| 9. Fixture, metriche, soglia e gate | Hint corretti, hint errati, scala ambigua, WCS sbagliato, pack sbagliato o mancante, campo fuori catalogo, poche stelle, distorsione. Metriche: tasso di soluzione, residuo, errore di centro e scala, falsa soluzione, copertura. Test T-AST-ASSIST01. Gate M3 core, M4 UI. |
| 10. Failure mode, warning e provenance | Fuori dominio, ambiguità, pochi match, catalogo incompatibile: fallimento esplicito. Provenienza di hint, catalogo, indice, residui e stelle di validazione. |
| 11. Ottimizzazione | Indice locale, cache, ROI, nessun download automatico. |

Il solver assistito non è una calibrazione del colore. Fornisce il WCS che C-16 può usare. Un WCS sbagliato non si nasconde con un residuo cosmetico.

---

### 10.20 C-19 Validazione, indipendenza e accettazione

| Campo | Contenuto |
|---|---|
| 1. Problema matematico | Stabilire se una conclusione del motore è un controllo di regressione o una validazione statistica indipendente, e misurare l'utilità del motore senza premiare l'astensione sistematica. Non è una formula unica: è un protocollo con dati, dichiarazioni e misure. |
| 2. Dominio, unità e prerequisiti | Registro di derivazione dei dati con pixel, frame, acquisizioni, master condivisi, trasformazioni globali e hash. Dataset di sviluppo separati dal corpus di accettazione. Endpoint di confronto dichiarati. |
| 3. Ipotesi e limiti | Il controllo di regressione spaziale non rende indipendenti i campioni. La PSF, il rumore di fondo, le maschere, i master e un fit globale possono correlare regioni e frame. Un corpus di accettazione indipendente riduce il leakage noto, non dimostra la generalizzazione a ogni campo. |
| 4. Alternative e scelta | Solo holdout spaziale; holdout più buffer; acquisizioni indipendenti; simulazioni con generatori indipendenti; corpus di accettazione separato. Il P0 richiede la distinzione esplicita e, per le conclusioni di validazione, dati indipendenti o simulazioni dichiarate. Il buffer o la molteplicità da soli non sono una correzione. |
| 5. Riferimenti indipendenti e stato | Statistica della selezione, validazione, leakage, molteplicità. Candidati non letti integralmente. Nessuna dipendenza da un servizio esterno. |
| 6. Numerica, stabilità e varianza | Intervalli e incertezze dichiarati. La molteplicità dei confronti è registrata. Le stime di beneficio e danno hanno unità e dominio. Un intervallo che include il no-op non autorizza a dichiarare un miglioramento. |
| 7. Maschere, metadati e bordi | Le ROI di taratura, il controllo di regressione e il corpus di accettazione sono registrati con hash. I pixel invalidi non si eliminano per migliorare il punteggio. Il lineage dice quali dati ha visto quale fase. |
| 8. Determinismo | Dataset, split, hash, seed, versione del motore, versione delle metriche e ordine dei confronti sono nella chiave. Nessun riuso adattivo non registrato. |
| 9. Fixture, metriche, soglia e gate | Controllo di regressione etichettato come tale; validazione indipendente con acquisizioni o simulazioni separate; corpus di accettazione con PSF e rumore non identici a quelli di taratura; casi identificabili e non identificabili. Metriche: copertura utile, falsi positivi, falsi negativi, danno, beneficio, calibrazione della confidenza, tasso di astensione corretto. Gate M3 per il protocollo, M4 per l'uso nella GUI. |
| 10. Failure mode, warning e provenance | Lineage mancante: il test non è una validazione indipendente. Corpus contaminato: il risultato non è accettabile. Astensione sistematica: l'accettazione fallisce anche se non ci sono diagnosi sbagliate. Provenienza di split, hash, metriche e decisioni. |
| 11. Ottimizzazione | Nessuna ottimizzazione può ridurre i dati di accettazione, il lineage o il budget dell'intero batch. La cache dei risultati non cambia lo split. |

Il contratto non introduce un nuovo ADR. Le decisioni tecniche restano in ADR-05 per la provenienza, ADR-06 per il budget e ADR-15 per l'Advisor.

---

## 11. Requisiti funzionali

### 11.1 Conteggio, notazione e regole

Il documento conserva 76 requisiti F della versione 2.0, uno per riga, con identificativo, priorità, milestone, dipendenze e criterio di accettazione verificabile. Aggiunge sei requisiti F-ANA P0 per l'analisi intelligente e l'assistenza. Il totale è 82 requisiti F: 76 conservati e 6 nuovi.

Le slice `.a`, `.b`, `.c` sono di pianificazione. Non creano identificativi di requisito, non promuovono l'intero requisito e non sostituiscono il criterio padre. Una slice ha un criterio proprio e un gate proprio; il requisito padre resta quello che è.

La colonna delle dipendenze usa cinque tipi, gli stessi dell'appendice D:

| Tipo | Significato | Cosa non significa |
|---|---|---|
| `requires_all` | Tutti i prerequisiti elencati devono essere soddisfatti prima del gate. | Non è un ordine di esecuzione delle fasi interne. |
| `requires_when` | Il prerequisito si attiva solo nella configurazione dichiarata. | Non è una dipendenza facoltativa in ogni caso. |
| `contains` | Il nodo padre contiene figli e non è eseguibile da solo. | Non è una dipendenza tra pari. |
| `decided_by` | La scelta di parametro o politica è una decisione registrata, non un valore inventato. | Non è un prerequisito tecnico. |
| `verified_by` | Il completamento è verificato dal test o dal gate citato. | Non è una dipendenza di implementazione. |

Un grafo aciclico non è un grafo completo. Un DAG canonico dimostra solo che non esistono cicli tra le dipendenze dichiarate. Non dimostra che ogni configurazione P0 attivi i propri prerequisiti condizionali. Per questo la sezione 11.11 elenca le configurazioni P0 dichiarate e le condizioni che devono essere attive in ciascuna. Il controllo è di attivazione, non di esistenza: si verifica che ogni arco `requires_when` della configurazione sia presente, che l'ordine temporale sia rispettato (nessun nodo richiede un output di una milestone successiva) e che ogni padre abbia foglie eseguibili, con l'ADR richiesto approvato prima del codice.

La verifica non pretende di provare tutti gli scenari possibili. Copre le configurazioni dichiarate, cioè quelle che il piano promette di supportare in P0. Un caso non dichiarato resta fuori dal contratto e non si presenta come coperto.

### 11.2 Import, formati e dataset, F-IO

| ID | Priorità | Milestone | Dipendenze | Criterio di accettazione |
|---|---|---|---|---|
| **F-IO-01** | P0 | M2 | Appendice D, F-IO-01; approvazioni ADR-03.a e ADR-08.a | Importa FITS mono, CFA, RGB e multi-plane; rileva profondità bit, dimensioni, unità e header; i metadati non interpretabili sono conservati e segnalati; fixture sintetiche e campioni pubblici licenziati passano. |
| **F-IO-02** | P0 | M3 | Appendice D, F-IO-02; approvazioni ADR-03.b e ADR-08.b | Importa RAW di più marche e modelli da campioni pubblici licenziati come CFA, senza debayer prematuro; pattern, black level e white level sono dichiarati; nessun requisito dipende da una marca o da un modello specifico. |
| **F-IO-03** | P0 | M3.a | Appendice D, F-IO-03 | Import batch di light, dark, flat, dark-flat e bias con classificazione modificabile; controlla dimensioni, CFA, esposizione, gain, binning, filtro, temperatura e offset. Prima della scelta propone solo ricette compatibili e omissioni motivate. Dopo la scelta, un mismatch arresta il ramo senza sostituzione; il progetto resta apribile. |
| **F-IO-04** | P0 | M3 | `requires_all` F-IO-01 | I metadati originali restano immutabili e disponibili: esposizione, ora, camera, sensore, scala pixel, orientamento e WCS quando presente; l'export di provenance include l'hash della sorgente e l'offset eventualmente già applicato dal decoder. |
| **F-IO-05** | P0 | M2 | `requires_all` F-IO-01 | Previews e analisi inventario non caricano obbligatoriamente tutti i pixel; la memoria resta dentro il budget; le thumbnails sono cache derivata e ricostruibile. |
| **F-IO-06** | P1 | M6 | `requires_all` F-IO-01 | Importa FITS complessi, TIFF 16 e 32 bit, XISF dopo revisione specifica, PNG e JPEG come input non scientifico; le funzionalità non supportate sono rifiutate, mai coercite in silenzio. |
| **F-IO-07** | P1 | M6 | `requires_all` F-IO-03 | Cataloghi e filtri personalizzati; associazione sessione, camera o ottica, filtro e target; import ed export dei set senza perdita. |
| **F-IO-08** | P0 | M3 | `requires_all` F-PRJ-01, F-IO-03 | File mancanti o spostati sono rilevati e ricollegabili senza corrompere il progetto; il ricollegamento verifica l'hash; i nodi dipendenti restano bloccati e il motivo è visibile; il salvataggio non perde i riferimenti risolti. |
| **F-IO-09** | P2 | M9 | `requires_all` F-IO-01 | Importa SER, AVI e codec selezionati per lucky imaging con timestamp, ROI e metadati frame quando disponibili; un codec non supportato viene rifiutato con messaggio chiaro. |
| **F-IO-10** | P0 | M2 | `requires_all` F-IO-01, F-PRJ-07 | Importa immagini calibrate o prestacked con la fase di processo dichiarata, senza pretendere i RAW; la provenance distingue prestacked, calibrato e raw e blocca le operazioni non compatibili. Il preflight distingue linearità nota, NL e sconosciuta: C-14 richiede un antenato radiometricamente lineare verificabile. Su input solo NL mostra la limitazione, non inverte uno stretch inventato e non sostituisce l'operatore. |

### 11.3 Progetto, workspace e provenienza, F-PRJ

| ID | Priorità | Milestone | Dipendenze | Criterio di accettazione |
|---|---|---|---|---|
| **F-PRJ-01** | P0 | M2 | Appendice D, F-PRJ-01; approvazione ADR-04.a | Il progetto si salva e si riapre con riferimenti ai file, parametri, versioni algoritmo, risultati e storia; un test di roundtrip ricostruisce lo stesso stato logico. |
| **F-PRJ-02** | P0 | M2 | `requires_all` F-PRJ-01 | Gli originali non vengono mai sovrascritti implicitamente; l'export è esplicito; un test verifica che l'hash della sorgente resti invariato dopo l'intero workflow. |
| **F-PRJ-03** | P0 | M2.b | Appendice D, F-PRJ-03 e ADR-05.a | Pipeline a DAG con nodi, input, output, dipendenze e cache; un cambio di parametro, metadato o maschera invalida solo il nodo e i discendenti, con test di invalidazione selettiva. Il modello DAG precede la persistenza M2.c; l'integrazione durevole è verificata lì, senza prerequisito retrogrado. |
| **F-PRJ-04** | P0 | M2 | `requires_all` F-PRJ-03 | Modifica di un parametro a monte, undo, redo e snapshot ripristinano esattamente lo stato logico. I risultati dipendenti si recuperano dalla cache o si ricostruiscono entro il contratto numerico e con le versioni dichiarate; l'eviction non perde la storia. Nessun nodo indipendente viene ricalcolato senza motivo; l'undo ripristina una revisione, non solo una cache. |
| **F-PRJ-05** | P1, `.a` P0 | `.a.1` M3.r, `.a.2` M4.b, `.b` M7 | Appendice D, foglie F-PRJ-05.a.1, F-PRJ-05.a.2 e F-PRJ-05.b | `.a` aggrega core e controlli UI reversibili senza mutazioni nascoste. `.b` aggiunge preset condivisibili, duplicazione progetto e batch. Un test verifica che il progetto originale non cambi. |
| **F-PRJ-06** | P0 | M2 | Appendice D, F-PRJ-06; stadi minimi ADR-04.a e ADR-05.a | Dopo crash, kill o perdita di energia il progetto si riapre; i risultati temporanei incompleti sono eliminati o marcati; le fasi sicure riprendono; un output parziale non è mai marcato completato. |
| **F-PRJ-07** | P0 | M2 | Appendice D, F-PRJ-07; stadio minimo ADR-05.a | Log semantici, provenance e manifest esportabile contengono versioni algoritmo, dataset, impostazioni, hash, correzioni applicate o omesse e condizioni di riproducibilità; un altro agente può ricostruire il risultato. |
| **F-PRJ-08** | P2 | M9 | `requires_all` F-PRJ-03; `decided_by` ADR-11 | Script e plugin hanno API versionata, sandbox, compatibilità dichiarata e limiti; il progetto non esegue eval di contenuto arbitrario e un plugin difettoso non corrompe il progetto. |

### 11.4 Calibrazione, master, maschere e debayer, F-CAL

| ID | Priorità | Milestone | Dipendenze | Criterio di accettazione |
|---|---|---|---|---|
| **F-CAL-01** | P0 | `.a` M2.e, `.b` M3.b | Appendice D, F-CAL-01.a e F-CAL-01.b | Crea master bias, dark, flat e dark-flat con combinazione, statistiche e rejection configurabili; gestisce set parziali e gruppi incompatibili; il report mostra frame usati, scartati, rumore, artefatti e stato del master; i master sono riproducibili da progetto. La `.a` è la slice sintetica minima, la `.b` completa il requisito. |
| **F-CAL-02** | P0 | `.a` M2.e, `.b` M3.c | Appendice D, F-CAL-02.a e F-CAL-02.b | Corregge il light secondo tutte le otto combinazioni, dark-flat e master tipizzati; evita doppia sottrazione. Omissioni e correzioni parziali sono scelte prima dell'esecuzione; un prerequisito perso dopo la scelta arresta il ramo senza sostituzioni. T-CAL-08 copre otto combinazioni e stati dark-flat. La `.a` limita il contratto al caso matched sintetico, non certifica anticipatamente la `.b`. |
| **F-CAL-03** | P0 | `.a` M2.e, `.b` M3.c | Appendice D, F-CAL-03.a e F-CAL-03.b | Prepara e normalizza flat mono, CFA e per filtro; applicazione solo nel dominio additivo ammesso del light. Pixel quasi nulli o invalidi sono mascherati, mai divisi per epsilon. Omissione o percorso parziale si scelgono prima dell'esecuzione; dopo errore STOP. Fattori di normalizzazione conservati. La preparazione non dipende dalla calibrazione del light. `.a` copre Q sintetico, `.b` i casi completi. |
| **F-CAL-04** | P0 | M3.c | Appendice D, F-CAL-04 | Rileva hot pixel, dead pixel e pixel anomali; usa bad-pixel map; il comportamento ai bordi è definito; la maschera si propaga a stacking e post-processing. |
| **F-CAL-05** | P1 | M6.f | Appendice D, F-CAL-05 | Applica dark scaling solo quando è validato, incluso il comportamento su amp glow; la master library abbina temperatura, gain ed esposizione; un mismatch produce diagnostica e arresta il percorso selezionato. |
| **F-CAL-06** | P0 | `.a` M2.e, `.b` M3.c | Appendice D, F-CAL-06.a e F-CAL-06.b | Propaga rumore, varianza, pesi, maschere e coverage dalla calibrazione allo stack; documenta covarianza light/flat e master condivisi. Test sintetici verificano le tolleranze. `.a` applica il modello minimo approvato M2.entry; `.b` completa i modelli dichiarati M3, senza sostituzione di un modello fallito. |
| **F-CAL-07** | P1 | M6.f | Appendice D, F-CAL-07 | Cosmetica, amp glow, pattern noise, banding e artefatti del sensore sono trattati con metodi fisicamente giustificati; metriche prima e dopo dimostrano effetto e limiti. |
| **F-CAL-08** | P0 | M3.d | Appendice D, F-CAL-08 | Debayer Bayer dopo le correzioni CFA o il passthrough scelti a priori, senza master universalmente obbligatori. Nessun passthrough dopo errore. Test di aliasing, colore, risoluzione e pattern; riconosce Bayer e X-Trans senza applicare pattern errati silenziosamente. X-Trans può essere rinviato con dichiarazione esplicita. |

### 11.5 Qualità, registrazione e astrometria, F-REG

| ID | Priorità | Milestone | Dipendenze | Criterio di accettazione |
|---|---|---|---|---|
| **F-REG-01** | P0 | M3 | Appendice D, F-REG-01 e C-04; stadio algoritmico ADR-09.b | Rileva stelle, centroidi, FWHM o HFD, eccentricità e SNR; esclude le stelle sature dal fit; produce uno scoring per frame; test su ground truth sintetico rispettano la tolleranza dichiarata. |
| **F-REG-02** | P0 | M3 | `requires_all` F-REG-01; `verified_by` T-REG-02 | Registra con stelle corrispondenti, outlier rejection, fit della trasformazione ed errori residui; trasformazioni sintetiche note vengono recuperate entro tolleranza; il report mostra match e residui; una soluzione ambigua non viene pubblicata come valida. |
| **F-REG-03** | P0 | M3 | Appendice D, F-REG-03 e C-06; ADR-10 e stadio ADR-17.b | Il resampling è selezionabile; bordi, pixel assenti, maschere e coverage sono gestiti; la varianza è propagata con la matrice di resampling e non interpolando una mappa di varianza; le correlazioni introdotte sono dichiarate; un test di conservazione del flusso e uno di brillanza sull'unità dichiarata passano. |
| **F-REG-04** | P1, `.a` P0 APPROVATO | `.a` M3.p, `.b` M6.a | Appendice D, F-REG-04.a e F-REG-04.b | Plate solving con WCS, identificazione e distorsioni dichiarate. La `.a` richiede pack locale selezionato, hint bounded e validazione su stelle escluse; nessun catalogo opzionale quando richiesto dalla ricetta. La `.b` è il blind solving. Report di residui e confidenza; gestione UI in M4.f, non un secondo solver. |
| **F-REG-05** | P1 | M6.a | Appendice D, F-REG-05 | Supporta modelli geometrici multipli e correzioni di distorsione, rotazione, prospettiva e setup diversi; la scelta del modello è motivata e validata con residui. |
| **F-REG-06** | P1 | M6.f | Appendice D, F-REG-06 | Gestisce sessioni multiple con qualità atmosferica variabile, filtri e inquadrature diverse; ogni sessione ha diagnostica e il merge non nasconde un mismatch. |
| **F-REG-07** | P2 | M9 | `requires_all` F-REG-02 | Registra su nucleo cometario o asteroidale e combina separatamente stelle e soggetto; un target sintetico in movimento valida la separazione e i residui. |
| **F-REG-08** | P2 | M9.f | Appendice D, F-REG-08 | Astrometria quantitativa con validazione WCS e strumenti su coordinate; i residui rispetto a catalogo sono riportati con unità e limiti. |

### 11.6 Stacking e integrazione, F-STK

| ID | Priorità | Milestone | Dipendenze | Criterio di accettazione |
|---|---|---|---|---|
| **F-STK-01** | P0 | M3.h | Appendice D, F-STK-01 | Implementa media, media pesata e almeno un metodo robusto per rejection; i prerequisiti statistici sono espliciti. Per `N = 1` e `N = 2` si seleziona a priori un percorso senza rejection statistica; per `N = 3` non si dichiara provata l'identificazione dell'outlier. Una perdita di campioni dopo la selezione non autorizza il cambio di metodo. |
| **F-STK-02** | P0 | M3 | `requires_all` F-REG-03 | Gestisce coverage, pixel invalidi, maschere e footprint; nessun bordo nero contamina i pixel validi; produce una coverage map e il conteggio dei campioni per pixel. |
| **F-STK-03** | P0 | M3 | `requires_all` F-STK-01, F-STK-02; `verified_by` T-STK-03 | Usa un peso per frame basato su qualità e rumore, con lo stesso stimando e senza doppio conteggio del rumore; il report mostra frame integrati, scartati, esposizione effettiva, `Neff` e motivazione; lo stesso input produce lo stesso risultato con gli stessi parametri. |
| **F-STK-04** | P1 | M6 | `requires_all` F-STK-01, F-STK-03 | Metodi robusti avanzati, per esempio sigma clipping e winsorized, sono confrontati su dataset sintetici; parametri e limiti per N basso sono documentati con metriche. |
| **F-STK-05** | P1 | M6 | `requires_all` F-STK-01 | Normalizza tra frame, gestisce pedestal e offset quando richiesto, e si difende da gradienti variabili; il report mostra la normalizzazione applicata. |
| **F-STK-06** | P1 | M6 | `requires_all` F-REG-03, F-STK-01 | Drizzle gestisce campionamento, coverage, pixfrac e memoria; non promette dettaglio nuovo quando il dataset non lo supporta; test su PSF e campionamento noti. |
| **F-STK-07** | P1 | M6 | `requires_all` F-STK-01, F-STK-05, F-REG-06 | Multi-sessione, stack per filtro, controllo cromatico e combinazione selettiva di subframe funzionano con report per sessione e per filtro. |
| **F-STK-08** | P2 | M9 | `requires_all` F-STK-05, F-STK-07, F-REG-05 | Mosaic registration, local normalization e seam blending con misura oggettiva di uniformità; il report mostra residui di seam e differenze di fondo. |
| **F-STK-09** | P2 | M9 | `requires_all` F-REG-07, F-STK-02 | Combina un bersaglio mobile con doppio allineamento stelle e soggetto quando applicabile; un test sintetico distingue il moto del soggetto dal moto del campo. |

### 11.7 Post-processing, F-POST

| ID | Priorità | Milestone | Dipendenze | Criterio di accettazione |
|---|---|---|---|---|
| **F-POST-01** | P0 | M4 | `requires_all` F-PRJ-03 | Visualizzazione lineare con screen transfer reversibile e stretching esplicito; preview e pixel reali sono distinti; nessuna trasformazione di display modifica il dato. |
| **F-POST-02** | P0, `.a` P0 | M3 per `.a`, M4 per `.b` | `.a`: `requires_all` F-PRJ-03 | Curve, livelli e istogram stretch con black point e white point; il clipping è segnalato; undo e redo ripristinano il dato e la vista. La `.a` è headless, la `.b` è GUI. |
| **F-POST-03** | P0 | `.a` M3.i, `.b` M4.f | Appendice D, F-POST-03.a e F-POST-03.b | Background modeling e gradient removal con selezione campioni, anteprima del modello sottratto o diviso e protezione delle nebulosità diffuse; la modalità additiva o divisiva è esplicita e testata; se il fondo non è identificabile, il sistema si astiene senza sostituire una correzione fallita. |
| **F-POST-04** | P0, `.a` P0 | M3 per `.a`, M4 per `.b` | `.a`: `requires_all` F-POST-02.a | Bilanciamento canali di base; gestione coerente dei profili colore di visualizzazione ed export; il percorso dati resta lineare e distinto dalla trasformazione ICC. Una trasformazione cromatica è ammessa solo se dichiarata, con riferimento e bande supportate. |
| **F-POST-05** | P1, `.a` P0 APPROVATO | `.a.1` M3.q, `.a.2` M4.f, `.b` M6.b | Appendice D, foglie del requisito | `.a` è colore relativo broadband con catalogo offline, stelle non sature, WCS valido e apertura compatibile; WCS importato e solver sono scelte a priori. `.b` è SPCC completa con risposta spettrale, non garanzia di calibrazione fotometrica assoluta. Un buon fit stellare non certifica il colore della nebulosa: limiti sistematici nel report. |
| **F-POST-06** | P1, `.a` P0 | `.a.1` M3.k, `.a.2` M4.f, `.b` M6.i | Appendice D, foglie del requisito | Stretch evoluto, protezione delle stelle, test cromatici e artefatti. `.a` è asinh cromatico `C' = g(I) C`, con rapporti conservati nel dominio dichiarato; `.b` è GHS con contratto proprio. |
| **F-POST-07** | P1 | M6.c | Appendice D, F-POST-07 | LRGB, HOO, SHO e miscelazioni tramite PixelMath controllato; provenance di ogni canale e contributi del blend tracciati. |
| **F-POST-08** | P1, `.a` P0 | `.a.1` M3.j, `.a.2` M4.e, `.b` M6.d | Appendice D, foglie del requisito | Maschere stella, sfondo, oggetto, luminanza e colore; editing locale, ROI, transizioni, salvataggio e riproducibilità. `.a` include brush, poligono, morfologia, feather, invert, combine, opacity e persistenza 7.14; core serializzato M3, interazione M4. `.b` è segmentazione avanzata. |
| **F-POST-09** | P1, `.a` P0 | `.a.1` M3.l, `.a.2` M4.f, `.b` M6.g | Appendice D, foglie del requisito | Denoise classico spaziale e multiscala con controlli di artefatto e domini espliciti. `.a` è à trous con modelli distinti L/NL; T-POST-09 falsifica la procedura, non solo la soglia. |
| **F-POST-10** | P1, `.a` P0 APPROVATO | `.a.1` M3.o, `.a.2` M4.f, `.b` M6.h | Appendice D, foglie del requisito | Deconvoluzione non AI con PSF, regolarizzazione, saturazione e ringing; RL non lineare distinta da Wiener a filtro fissato; nessun clamp silenzioso. `.a` usa `b` fisso stimato a monte e `W` PSD che approssima `C_V^{-1}` sul sottospazio valido; sensibilità a `lambda`, PSF e fondo perturbati e baseline no-op. Il gate fallito blocca la funzione e M5, senza declassamento a P1. |
| **F-POST-11** | P1, `.a` P0 | `.a.1` M3.m, `.a.2` M4.f, `.b` M6.i | Appendice D, foglie del requisito | Sharpening e contrasto locale controllati, HDR/multiscala avanzati nella `.b`, riduzione alone; test di artefatti e confronto prima/dopo. |
| **F-POST-12** | P1, `.a` e `.b` P0 | `.a.1` e `.b.1` M3.n, `.a.2` e `.b.2` M4.f, `.c` M6.i | Appendice D, foglie del requisito | Fit e sottrazione solo sull'antenato lineare `I_L`. Per display `R_T = T(I_L - S_L)` e `S_T = T(I_L) - T(I_L - S_L)`, con `R_T + S_T = T(I_L)` verificata. Se `I_L` manca, STOP. Riduzione e separazione hanno gate indipendenti di accuratezza del modello e danno non stellare; la ricomposizione non prova correttezza astronomica. |
| **F-POST-13** | P1 | M6.i | Appendice D, F-POST-13 | Saturazione e tono selettivi, dominanti e rifinitura; layer e maschere con provenance, dipendenti da F-POST-08.b. |
| **F-POST-14** | P1, `.a` P0 | M4 per `.a`, M6 per `.b` | `.a`: `requires_all` F-POST-01, F-ANA-03.a | La `.a` offre confronto prima e dopo, ispezione 1:1 su dettaglio e stelle, nessun dettaglio inventato presentato come misura. La `.b` aggiunge anteprime multiple avanzate. |
| **F-POST-15** | P2 | M8 | Appendice D, F-POST-15 | Starless ML facoltativo, modelli licenziati, provenance, riconciliazione stellare e test di artefatti; percorso non AI primario e indipendente, non riserva dopo un guasto ML. |
| **F-POST-16** | P2 | M9.a | Appendice D, F-POST-16 | Strumenti per nebulosità diffuse, aree molto luminose e mosaici narrowband avanzati; dataset e gate propri, senza riuso acritico del deep sky. |

**Conteggio requisiti F conservati:** 10 F-IO + 8 F-PRJ + 8 F-CAL + 8 F-REG + 9 F-STK + 16 F-POST + 9 F-UX + 8 F-RES = 76.

### 11.8 Esperienza utente e interfaccia, F-UX

| ID | Priorità | Milestone | Dipendenze | Criterio di accettazione |
|---|---|---|---|---|
| **F-UX-01** | P0 | M4 | `requires_all` F-IO-03, F-PRJ-01 | Import e workspace chiari: browser file e dataset, vista immagine, pipeline, inspector parametri e monitor esecuzione; un utente completa il primo import senza documentazione esterna. |
| **F-UX-02** | P0 | M4 | `requires_all` F-POST-01 | Zoom 1:1, pan e preview veloce con rendering progressivo per immagini grandi; istogramma e indicatori di clipping; la memoria del viewer resta bounded. |
| **F-UX-03** | P0 | M4 | `requires_all` F-PRJ-03, F-RES-03 | Cancel e pausa dove sono tecnicamente sicuri, senza corruzione; feedback delle fasi e motivi di errore azionabili; un job cancellato lascia il progetto coerente. |
| **F-UX-04** | P0 | M4 | `requires_all` F-RES-01, F-RES-02, F-RES-03 | Preferenze CPU, RAM, cache e anteprima configurabili e persistenti; i valori sono validati e il default è spiegato; un budget troppo basso produce un avviso, non un crash. |
| **F-UX-05** | P0 | M4 | `requires_all` F-UX-01, F-UX-02 | Guide contestuali, onboarding, tooltip tecnici, preset comprensibili e recovery automatico; le tre modalità usano lo stesso motore; un principiante completa il percorso dichiarato con aiuto contestuale. |
| **F-UX-06** | P1, `.a` P0 | `.a` M4.d, `.b` M6.d | Appendice D, F-UX-06.a e F-UX-06.b | `.a` offre ROI e confronto pixel-level sul viewer e DAG, senza dipendere dall'editing avanzato. `.b` comprende solo editing sofisticato e varianti avanzate annullabili. L'editing base delle maschere è già P0 in F-POST-08.a.2 M4.e e non è rinviato a questa `.b`. |
| **F-UX-07** | P0 | M4 | `requires_all` F-UX-01 | Accessibilità: tastiera, focus visibile, tooltip, tema chiaro e scuro, HiDPI, colori non come unico segnale; audit con checklist e test manuali; nessun controllo essenziale richiede solo il mouse. |
| **F-UX-08** | P1 | M6 | `requires_all` F-UX-01, F-UX-05 | Interfaccia localizzabile, con italiano e inglese proposti come lingue iniziali; il coordinatore decide il livello di traduzione dopo verifiche UX; stringhe esterne al codice. |
| **F-UX-09** | P2 | M9.f | Appendice D, F-UX-09 | Personalizzazione workspace, script editor e pipeline visuale avanzata solo se test UX giustificano il costo; nessuna dashboard decorativa senza funzione elaborativa. |

### 11.9 Risorse, RAM, cache e thread, F-RES

| ID | Priorità | Milestone | Dipendenze | Criterio di accettazione |
|---|---|---|---|---|
| **F-RES-01** | P0 | M2 | Appendice D, F-RES-01; approvazione ADR-06.a | Modalità Automatica: stima il budget dalla memoria disponibile e dalla pressione di OS e altri processi, usando PSI o cgroup quando presenti; evita di consumare tutta la RAM; testata con più budget e carichi concorrenti. |
| **F-RES-02** | P0 | M2 | Appendice D, F-RES-02; approvazione ADR-06.a | Modalità Manuale: limite in MiB o GiB oppure percentuale; il denominatore è esplicito e la capacità è uno snapshot; conversioni e comportamento sono ambigui zero; il valore è persistente e validato. |
| **F-RES-03** | P0 | M2 | `requires_all` F-RES-01, F-RES-02 | Scheduler e pipeline adattano frame, tile, cache, thread e buffer al budget; usano blocchi, streaming e spill quando serve. Un dataset oltre RAM viene elaborato o rifiutato con preflight; i test verificano le prenotazioni del pool e i fallimenti controllati. Non si garantisce assenza universale di OOM o kill da OS per allocazioni esterne o pressione concorrente. |
| **F-RES-04** | P0 | M2 | `requires_all` F-RES-01, F-RES-03 | Monitora memoria gestita, picco stimato, cache residente e limite scelto; segnala quando il budget è troppo restrittivo; distingue pool gestito, RSS, scratch e VRAM. |
| **F-RES-05** | P1, `.a` P0 | M2 per `.a`, M6 per `.b` | `.a`: `requires_all` F-RES-01, F-RES-02 | La `.a` separa limite del pool RAM gestito, obiettivo RSS best effort, cache su disco, scratch e VRAM quando disponibile. La `.b` aggiunge coordinamento avanzato e politiche dipendenti dal carico. |
| **F-RES-06** | P1, `.a` P0 | M2 per `.a`, M6 per `.b` | `.a`: `requires_all` F-RES-03 | La `.a` gestisce pressione e checkpoint, ferma nuove ammissioni, fa spill o cancel sicuro. La `.b` aggiunge cambio interattivo a caldo senza riavvio. Il confine tra `.a` e `.b` richiede una decisione esplicita. |
| **F-RES-07** | P1, `.a` P0 | M2 per `.a`, M6 per `.b` | `.a`: `requires_all` F-RES-01, F-RES-03 | La `.a` evita OOM e swap eccessivo con preflight, backpressure e rifiuto motivato; non promette un limite hard per driver o allocazioni esterne; un job impossibile viene rifiutato con il minimo fattibile dichiarato. La `.b` aggiunge politiche avanzate. |
| **F-RES-08** | P1, `.a` P0 | `.a` M2.d, `.b` M6.i | Appendice D, F-RES-08.a e F-RES-08.b | `.a` configura worker, CPU, scratch, quota, cache e cleanup confinato. `.b` aggiunge tuning CPU avanzato. Il tuning VRAM della GPU produttiva appartiene a NFR-GPU-01 in M7, non anticipa né ritarda la foglia `.b`. |

### 11.10 Nuovi requisiti di analisi e assistenza, F-ANA

I sei requisiti sono P0 vincolanti. Non richiedono AI, rete, account o cloud. Sono deterministici e offline. L'Advisor non è un quarto motore: usa lo stesso DAG, gli stessi operatori e gli stessi report.

#### F-ANA-01, Analisi diagnostica multi-fase

- **Priorità:** P0.
- **Milestone:** M3 per `.a`, M4 per `.b`.
- **Dipendenze:** appendice D, foglie F-ANA-01.a e F-ANA-01.b; nessuna dipendenza del core dal viewer.
- **Criterio di accettazione:** il motore analizza almeno le nove famiglie della sezione 13 su RAW, light calibrato, stack lineare e immagine non lineare, dichiarando per ogni metrica definizione, unità, dominio di validità, incertezza e procedura di test. Su dati sintetici con difetto noto, la metrica risponde entro la tolleranza dichiarata. Su un caso non identificabile, il motore si astiene e non produce una diagnosi. La `.a` comprende già in M3 diagnosi e metriche non lineari senza viewer; la `.b` aggiunge soltanto la presentazione GUI. Il costo è bounded dal budget di risorse.

#### F-ANA-02, Schede di evidenza e spiegazione

- **Priorità:** P0.
- **Milestone:** M3 per `.a`, M4 per `.b`.
- **Dipendenze:** `.a`: `requires_all` F-ANA-01.a, F-PRJ-07.
- **Criterio di accettazione:** ogni raccomandazione produce una scheda con evidenza e localizzazione, metrica e confidenza, ipotesi e alternative, beneficio atteso come ipotesi, effetti collaterali, fase ottimale e prerequisiti, anteprima confrontabile, accettazione o rifiuto. Una scheda senza evidenza non viene mostrata. L'utente può rifiutare senza modificare il progetto. La scheda è riproducibile, conservata nella provenance e non attribuisce a una preferenza soggettiva lo statuto di risultato scientifico.

#### F-ANA-03, Confronto e preview multi-variante

- **Priorità:** P0.
- **Milestone:** M3 per `.a`, M4 per `.b`.
- **Dipendenze:** appendice D, foglie F-ANA-03.a e F-ANA-03.b; il core usa F-PRJ-05.a.1, non l'aggregato comprensivo della UI.
- **Criterio di accettazione:** l'utente confronta prima e dopo e almeno due varianti sullo stesso DAG, con ispezione 1:1 su dettaglio e stelle. Le viste indipendenti leggono lo stesso motore. Non esiste una pipeline separata per l'Advisor. Un confronto non presenta dettaglio inventato come misura. Il rollback è disponibile. Il confronto dichiara se i candidati sono confrontabili: se uno è stato valutato su un endpoint diverso, l'esito è `NON CONFRONTABILE`.

#### F-ANA-04, Ottimizzazione assistita multi-obiettivo

- **Priorità:** P0.
- **Milestone:** M3 per `.a`, M4 per `.b`, M6 per l'estensione.
- **Dipendenze:** appendice D, foglie F-ANA-04.a e F-ANA-04.b, compresi guardrail, operatori P0 e capacità condizionali; verifica T-ANA-04.
- **Criterio di accettazione:** l'ottimizzazione distingue integrità scientifica, qualità tecnica e resa fotografica, con preferenze esplicite. Non produce un punteggio universale di bellezza. Seleziona algoritmi e intervalli di parametri ammessi dal contratto, esplora su ROI rappresentative, poi verifica a piena risoluzione in streaming con gate globale e controllo di regressione dichiarato. Rileva clipping, perdita di nebulosità, oversmooth, ringing e distorsione stellare. Può proporre rollback. Registra versione algoritmo, parametri, seed, metriche e costo CPU, RAM e scratch. Rispetta i budget e non esegue un costo illimitato. La distinzione tra controllo di regressione e validazione statistica indipendente è obbligatoria (sezione 13.14).

#### F-ANA-05, Guardrail scientifici e anti-regressione

- **Priorità:** P0.
- **Milestone:** M3 per `.a`, M4 per `.b`.
- **Dipendenze:** `.a`: `requires_all` F-ANA-01.a, NFR-TEST-01.a, NFR-TEST-02.
- **Criterio di accettazione:** la suite copre casi puliti, segnale debole, nebulosità diffusa, campo affollato e stelle sature, più out of distribution e astensione. Riporta falsi positivi, falsi negativi e copertura delle diagnosi. Un no-op è un esito valido. "Nessun difetto rilevato" non significa "incerto" e non significa "l'alternativa è provata cattiva". Una strategia automatica non peggiora in silenzio le metriche di qualità; i trade-off approvati sono visibili. L'astensione sistematica non è una strategia vincente: il corpus di accettazione della sezione 13.16 misura anche l'utilità nei casi identificabili.

#### F-ANA-06, Integrazione UX e modalità

- **Priorità:** P0.
- **Milestone:** M4.
- **Dipendenze:** `requires_all` F-ANA-02.b, F-ANA-03.b, F-ANA-04.b, F-UX-05.
- **Criterio di accettazione:** esistono tre livelli, solo suggerimenti, anteprima e confronto, ottimizzazione automatica con conferma. Sono collegati alle modalità Automatica, Guidata ed Esperta senza creare un quarto motore. L'Advisor esegue solo operatori disponibili nel registro e nei domini validati; un operatore assente non viene simulato né presentato come capacità P0. La modalità Automatica propone parametri motivati, mostra confidenza e anomalie e consente l'intervento manuale.

**Conteggio requisiti F totali:** 76 conservati + 6 F-ANA = 82.

### 11.11 Matrice di attivazione delle configurazioni P0

La matrice collega le configurazioni ai predicati canonici dell'appendice D. Le descrizioni seguenti non definiscono archi aggiuntivi. Il controllo documentale del grafo è distinto dalla futura verifica runtime dell'attivazione; nessuno dei due prova completezza scientifica di ogni combinazione immaginabile.

| Configurazione dichiarata | Archi `requires_when` attivi | Controllo di ordine | Cosa resta fuori |
|---|---|---|---|
| Calibrazione, otto combinazioni | P-DFB000, P-DFB001, P-DFB010, P-DFB011, P-DFB100, P-DFB101, P-DFB110, P-DFB111; assi P-DF-ABS, P-DF-INC, P-DF-SUB, P-DF-BAD e P-FLAT-RAW, P-FLAT-Q, P-FLAT-UNKNOWN; P-OFFSET | Analisi prima della scelta, flat preparato prima dell'uso; P-LOST arresta la ricetta selezionata, non attiva omissioni | Stati non dichiarati: uso critico vietato |
| Solver con pack presente, hint valido | `requires_when` WCS richiesto → F-REG-04.a; `requires_when` colore relativo → WCS valido | Il solver precede il colore; il pack precede il solver | Campi fuori dal dominio bounded |
| Solver con pack mancante | `requires_when` pack mancante → errore esplicito; il WCS importato non è una sostituzione automatica e si usa solo se la ricetta lo prevede a priori | L'errore precede il colore e l'astrometria | Soluzione senza catalogo |
| WCS importato, solver non usato | `requires_when` WCS importato → validazione; `requires_when` WCS invalido → blocco del colore | La validazione precede F-POST-05.a | Correzione di un WCS sbagliato senza dati |
| Rumore lineare nel denoise | `requires_when` modello lineare → rumore per scala e gain | La stima del rumore precede lo shrinkage | Dati non lineari senza Jacobiano o modello empirico |
| Rumore non lineare nel denoise | `requires_when` trasformazione nota → `Jf`; `requires_when` trasformazione ignota → modello empirico e confidenza ridotta | La trasformazione dichiarata precede la stima | Garanzia di varianza esatta |
| Deconvoluzione selezionata | `requires_when` F-POST-10.a attivo → PSF, `W`, `lambda`, maschere, budget | PSF e fondo precedono la soluzione; il budget precede l'esecuzione | PSF variabile sul campo |
| Deconvoluzione non selezionata | Nessun arco di F-POST-10.a; la strategia deve comunque produrre un esito valido | Il planner non richiede un operatore assente | Il risultato non può dipendere da F-POST-10.a |
| Maschera derivata dopo un riordino | `requires_when` riordino ammesso → politica di trasformazione della selezione; `requires_when` asset congelato → selezione conservata | La policy precede la pubblicazione del candidato | Riordino senza policy dichiarata |

Il controllo si esegue in M2 sui contratti minimi e si ripete in M3 e M4 con le configurazioni effettivamente implementate. Un arco attivo che punta a una milestone successiva è un errore di piano, non un dettaglio di scheduling.

---

## 12. Requisiti non funzionali, 24 requisiti

I requisiti non funzionali non sostituiscono gli F. Li rendono verificabili. Ogni NFR ha una milestone e un criterio. I 23 identificativi della 2.0 sono conservati; alcuni criteri sono estesi per coprire Advisor, calibrazione flessibile, resampling, manifest, corpus di accettazione e verifica delle configurazioni. NFR-FBK-01 è l'unico ID nuovo e rende verificabile NC-21.

| ID | Priorità | Milestone | Requisito e criterio di accettazione |
|---|---|---|---|
| **NFR-UX-01** | P0 | M4 | Le tre modalità Automatica, Guidata ed Esperta usano lo stesso motore, gli stessi nodi e le stesse viste dell'Advisor. Un test dimostra che il risultato è lo stesso a parità di parametri e che nessuna modalità richiede un percorso di codice separato. |
| **NFR-EXP-01** | P0 | `.a` M2.a, `.b` M3.s, `.c` M4.g, `.d` M5 | FITS minimo M2; FITS/TIFF completi M3 con provenance, unità, versioni, parametri, hash, correzioni e AI dichiarata. Riapertura conserva metadati; manifest o sidecar per ciò che il formato non rappresenta. M4 verifica UI, M5 gli artefatti distribuiti. |
| **NFR-REP-01** | P0 | M2 base, M3 headless, M4 UI, M5 release | Ogni fase critica produce un report quando introdotta: import, master, calibrazione, debayer, registrazione, resampling, stacking, background, stretch, export, recovery e Advisor. M3 include report, confronti e planner anche non lineari; M4 aggiunge soltanto la presentazione interattiva. Gli stadi sono definiti in appendice D. |
| **NFR-LINUX-01** | P0 | M0, M5 | Matrice Linux con livelli verificato, compatibile non certificato, non supportato. La matrice minima proposta è di tre configurazioni concrete. Nessuna promessa su ogni distribuzione storica. |
| **NFR-LINUX-02** | P0 | M5 | X11 nativo e Wayland sono provati con il toolkit scelto. HiDPI, temi, tastiera e focus funzionano nei casi supportati. XWayland non è X11 nativo. Un ambiente non verificato è dichiarato compatibile non certificato o non supportato. |
| **NFR-LINUX-03** | P0 | M0 piano, M1 prove preliminari, M5 verifica | I minimi di ABI, kernel e CPU sono misurati con prove, non fissati per intuizione. M0 definisce il protocollo, non minimi già misurati. x86_64 è il candidato iniziale. ARM64 è uno studio, senza impegno di release. |
| **NFR-CPU-01** | P0 | `.a` M2.e, `.b` M3.s, `.c` M4.g, `.d` M5 | Tutte le funzioni fondamentali, incluso viewer software e STF, funzionano CPU-only. CPU e renderer software sono percorsi primari scelti nel preflight; non sostituiscono GPU fallite. Le verifiche coprono progressivamente slice, headless completo, GUI e release. |
| **NFR-GPU-01** | P1 | M7 | GPU AMD, Intel e NVIDIA sono facoltative. Il backend non è deciso in anticipo. Il percorso CPU è primario, dichiarato e testato; non è una sostituzione automatica di una GPU assente o fallita. La GPU non è necessaria per nessun requisito P0. |
| **NFR-SIMD-01** | P0 | M2 | SIMD usa dispatch nel preflight. AVX2 non è un requisito universale. Il percorso SIMD e il percorso scalare sono entrambi primari e dichiarati, con tolleranze numeriche proprie. Un percorso non sostituisce l'altro a runtime. |
| **NFR-FBK-01** | P0 | M2 core, M3 operatori, M4 UI, M5 release | Nessun fallback e nessuna degradazione sostitutiva in nessuno stadio. Un errore, una risorsa indisponibile o un gate fallito produce stop controllato, codice di uscita non nullo, messaggio diagnostico e provenance dell'arresto. Il resume riprende solo con ricetta, metodo, backend e parametri identici a quelli del checkpoint valido; qualunque differenza apre una nuova ricetta e non è una prosecuzione. L'Advisor può confrontare candidati già pianificati se il protocollo preregistrato lo consente; dopo un errore non produce un no-op sostitutivo, non promuove un nuovo candidato al posto di quello fallito e non dichiara completa una validazione eseguita su un insieme incompleto. Il budget non autorizza il taglio automatico di risoluzione, candidati o qualità; streaming, spill e checkpoint sono ammessi solo se previsti dalla ricetta. Un'autorizzazione dell'utente non abilita un fallback, nemmeno dichiarato. |
| **NFR-OFF-01** | P0 | `.a` M2.e, `.b` M3.s, `.c` M4.g, `.d` M5 | Core e Advisor offline, senza account o cloud. M2 verifica solo la slice disponibile; M3 l'Advisor completo, M4 la GUI, M5 il primo avvio. Cataloghi/download opzionali sono opt-in e disattivabili. Colore relativo e solver richiedono il pack locale selezionato; assenza o invalidità producono STOP, non rete implicita o cambio ricetta. |
| **NFR-PKG-01** | P0 | M5 | AppImage, Flatpak e pacchetti nativi sono confrontati con criteri: installazione, aggiornamento, primo avvio offline, integrazione desktop, sandbox, dimensione, dipendenze, riproducibilità e rimozione. La scelta tecnica autonoma segue le prove. La distribuzione dei catalog pack segue D-21, preferenza inclusa APPROVATA con verifiche legali e tecniche ancora da eseguire secondo 17.4. |
| **NFR-PKG-02** | P0 | M5 | La modalità scelta viene provata su installazione pulita, aggiornamento, avvio senza rete, CPU-only, GPU disponibile, HiDPI, X11, Wayland e rimozione. Un fallimento blocca il gate di release. |
| **NFR-COMP-01** | P0 | M5 | La competitività è misurata per asse e dataset, con baseline e condizioni dichiarate. Nessuna affermazione di vittoria assoluta. Un asse non misurato resta aperto. |
| **NFR-SEC-01** | P0 | M2 | Parser e decoder hanno limiti di dimensione, profondità, numero di frame e decompressione. Input malformati falliscono in modo controllato, senza overflow o consumo incontrollato. |
| **NFR-SEC-02** | P0 | M2 | I progetti non eseguono eval, script arbitrari o comandi di sistema. I plugin, quando esistono, sono sandboxati e con API dichiarata. L'Advisor non esegue codice generato. |
| **NFR-SEC-03** | P0 | M2 | Export e cleanup proteggono gli originali da symlink, hardlink, path traversal e sovrascrittura accidentale. Il cleanup è confinato alle directory dichiarate. |
| **NFR-SEC-04** | P0 | M3 | Parser e codec critici hanno fuzz e sanitizer nei limiti delle risorse. I risultati dei test sono registrati. Nessun download di immagini o upload è automatico. |
| **NFR-LIC-01** | P0 | M0 politica e candidati, M1 verifica prima dell'adozione, M5 SBOM distribuito | Inventario con versione, hash, moduli e dipendenze transitive prima di adottare ogni libreria; SBOM progressivo fino agli artefatti effettivamente distribuiti. M0 approva il piano, non inventa una distinta completa. Un audit non è dichiarato finito finché non è stato fatto. Un catalog pack è un artefatto distribuito: la sua licenza, la sua provenienza e la sua versione entrano nell'SBOM. |
| **NFR-TEST-01** | P0 | `.a` M2.f, `.b` M3.s, `.c` M4.g, `.d` M5 | `.a` consegna infrastruttura locale/CI e fixture prima dei kernel; test unitari, property/metamorphic e ground truth crescono con le funzioni. M2 verifica gli invarianti e la varianza della slice senza certificare operatori assenti. `.b` chiude scienza/integrazione headless, `.c` UI, `.d` release. T-FBK-01 cresce a ogni stadio senza essere prerequisito della propria implementazione. |
| **NFR-TEST-02** | P0 | M3 | I dataset pubblici sono scelti con licenza compatibile e hash. Le fixture sintetiche coprono CFA, PSF, rumore, gradienti, hot pixel, rotazioni, traslazioni subpixel, cosmici, bordi, difetti Advisor, assenza di master e out of distribution. Non si usano foto private dell'utente. Il corpus di accettazione dell'Advisor è separato dai dati di sviluppo. |
| **NFR-TEST-03** | P0 | M3 | I benchmark misurano wall time, CPU time, throughput, peak RSS, I/O, disco temporaneo, VRAM e qualità. Il budget è relativo al working set e alle risorse dichiarate, non alla macchina personale di un utente. |
| **NFR-PERF-01** | P0 | M2 | Nessun valore universale di RAM, tempo o margine viene imposto. I limiti sono configurabili o misurati. Un margine del 70 per cento o un timeout arbitrario non sono criteri di progetto. |
| **NFR-DATA-01** | P0 | M2 | Gli originali sono identificati con hash e non vengono modificati. Export e cache hanno integrità verificabile. Un output parziale non è pubblicato come completo. Un bundle multi-file è completo solo con manifest valido, hash degli output verificati e marker di commit presente e coerente con quel manifest. |

NFR-FBK-01 verifica il comportamento del prodotto, non vieta correzioni del codice. Lo sviluppatore deve correggere e ripetere le prove secondo 21.13; un errore non impone la sospensione di tutto il progetto. La fault injection di T-FBK-01 deve accertare il percorso effettivo e l'assenza di chiamate sostitutive, oltre al messaggio e al codice di uscita.

**Conteggio NFR:** 24.

---

## 13. Analisi intelligente e Advisor

### 13.1 Principi

L'Advisor è un motore di analisi e assistenza deterministico e offline. Non è una rete neurale, non richiede account, non invia immagini e non è un quarto motore di elaborazione. Usa gli stessi operatori, lo stesso DAG e gli stessi report del prodotto.

L'Advisor non decide al posto dell'utente. Propone, spiega, confronta e, se l'utente conferma, esegue una variante reversibile. Non nasconde un difetto con una correzione plausibile. Quando non può identificare la causa, si astiene.

La capacità è P0 vincolante, ma il suo uso non è obbligatorio per ogni workflow. Un utente può usare la pipeline senza aprire l'analisi. L'analisi non blocca il percorso e non introduce una dipendenza da servizi esterni.

L'assistenza eseguibile è limitata al registro operatori effettivamente disponibile e ai domini validati in quel momento. Una diagnosi su un dominio non validato produce un avviso o un'astensione, non un'esecuzione. Un operatore futuro non viene simulato.

### 13.2 Nove famiglie diagnostiche

| Famiglia | Cosa misura | Metrica e unità | Dominio e incertezza | Confusione tipica e astensione | Test sintetici |
|---|---|---|---|---|---|
| 1. Fondo, gradiente, vignettatura, dominante | Livello additivo, andamento spaziale, profilo radiale, offset di canale | Livello in ADU o elettroni, ampiezza del gradiente, coefficienti radiali, offset per canale | Dati lineari; incertezza cresce con nebulosità diffusa e campo incompleto | Nebulosità diffusa scambiata per fondo; vignettatura moltiplicativa scambiata per gradiente additivo; dominante scambiata per colore intrinseco. Se non separabile, astensione | Gradiente noto più nebulosa, vignettatura sintetica, flat residuo, dominante nota |
| 2. Rumore, banding, hot e dead pixel | Deviazione del rumore, spettro spaziale, picchi di banda, conteggio pixel anomali | Sigma in ADU o elettroni, potenza per banda, conteggio e frazione | Dipende dallo stato di calibrazione; il rumore di lettura e il fixed pattern possono confondersi | Hot pixel scambiato per cosmico; banding scambiato per struttura; rumore correlato scambiato per indipendente. Astensione se manca il modello | Rumore bianco e correlato, banding noto, mappa hot e dead |
| 3. Stelle, PSF, FWHM, HFD, ellitticità | Qualità della PSF, variazione sul campo, completezza della detection | Pixel o arcosecondo per FWHM e HFD, ellitticità adimensionale, conteggio stelle | Dato lineare anche parzialmente calibrato o senza master, con limiti per metrica; stelle non sature; campi affollati, pattern residui e nebulosità riducono la confidenza | Trailing, coma, fuocheggiatura e seeing possono produrre ellitticità simile; la causa ottica non è certa. Astensione sulla causa e sulle metriche non stimabili | PSF gaussiana e Moffat, PSF variabile, trailing, coma, campo affollato, assenza di master |
| 4. Saturazione, aloni, clipping, gamma dinamica | Frazione satura, clipping, alone, margine recuperabile | Percentuale di pixel saturi, conteggio clip, contrasto dell'alone, intervallo dinamico in stop | Dipende dal RAW o dal dato lineare; il recupero oltre saturazione non è garantito | Alone ottico, blooming, alone di processing e diffusione possono confondersi. Non si promette recupero di informazione non acquisita | Stelle sature, blooming, cuneo di clipping, alone noto |
| 5. SNR, segnale debole, struttura diffusa, aliasing | Rapporto segnale rumore, visibilità di struttura, contenuto ad alta frequenza | SNR per apertura, sigma locale, contrasto struttura, indicatore di aliasing | Richiede una stima del rumore e una maschera; il segnale diffuso può essere scambiato per fondo | Nebulosità debole scambiata per rumore; dettaglio reale scambiato per aliasing; struttura a bassa frequenza scambiata per gradiente. Astensione se il segnale è sotto la soglia di identificabilità | Strutture deboli note, rumore a diversi livelli, campionamento noto |
| 6. Artefatti di deconvoluzione e denoise | Ringing, overshoot, perdita di dettaglio, stelle deformate, rumore residuo | Contrasto di ringing, errore rispetto a riferimento, variazione di FWHM, sigma residuo | Solo dopo l'operatore, con un riferimento o un modello; la valutazione estetica non è una misura | Dettaglio reale scambiato per artefatto; lisciatura scambiata per riduzione rumore. Astensione senza riferimento | PSF nota, rumore noto, stelle, immagine sintetica con dettaglio controllato |
| 7. Qualità dei frame e inclusione | Nitidezza, fondo, nuvole, trailing, satellite, SNR, numero stelle | FWHM, HFD, SNR, conteggio stelle, copertura, flag di evento | Per frame, non causale; un frame scartato non è necessariamente difettoso in modo irreparabile | Nuvola scambiata per gradiente; satellite scambiato per cosmico; trailing lieve scambiato per seeing. Astensione sull'inclusione automatica se il beneficio è ambiguo | Frame con nuvole, trailing, satellite, seeing variabile |
| 8. Qualità e compatibilità dei master | Rumore del master, struttura residua, saturazione, illuminazione, compatibilità di gruppo | Sigma, residuo spaziale, frazione satura, uniformità, differenze di parametri | Per gruppo di calibrazione; la compatibilità non è la qualità | Master rumoroso ma compatibile può peggiorare il risultato; master incompatibile ma simile può ingannare. Astensione se manca lo stato | Master sintetici buoni, rumorosi, incompatibili, saturi |
| 9. Affidabilità per dominio | Validità delle altre metriche nel dominio corrente | Flag di validità, confidenza, copertura, motivi | Trasversale; dipende da RAW, light calibrato, stack lineare o immagine non lineare | Applicare una metrica di stack a un RAW o una metrica estetica a una misura. Astensione quando il dominio non è dichiarato | Matrice di casi per dominio, con metriche valide e invalide |

### 13.3 Affidabilità per dominio

| Dominio | Metriche relativamente affidabili | Metriche non affidabili o vietate | Confidenza e limite |
|---|---|---|---|
| RAW | Pattern CFA, saturazione con soglia nota, anomalie candidate, qualità del frame, compatibilità dei master se i metadati sono completi | Fondo finale, colore finale, SNR di stack; identificazione certa hot/dead da un solo frame senza modello o riscontri | Confidenza dipendente da metadati e modello; cause ottiche e persistenza dei difetti possono non essere identificabili |
| Light calibrato | Fondo e gradiente, rumore, stelle e PSF, SNR di singolo frame, residui di flat | SNR di stack, dettaglio finale, colore calibrabile senza catalogo | Media, dipende dalle correzioni applicate e omesse |
| Stack lineare | Fondo, rumore, PSF, struttura debole, copertura, confronto tra varianti | Fotometria assoluta senza calibrazione; colore assoluto senza riferimento e modello di risposta; dettaglio oltre il campionamento | Coverage e pesi noti aiutano, ma non certificano la diagnosi o la causa ottica. SPCC non è l'unico metodo possibile di calibrazione colore |
| Immagine non lineare | Clipping, aloni, artefatti, resa, confronto estetico, dettaglio percepito | Non riusare SNR, flusso o fit PSF del dominio lineare senza un contratto che modelli la trasformazione | Nel percorso fotografico P0 non si certificano misure dal solo stato non lineare. Eventuali capability quantitative richiedono validazione specifica; la non linearità da sola non dimostra invalidità di ogni astrometria |

Una metrica può essere valida nel dominio e comunque avere una causa non identificabile. La tabella separa la validità della metrica dalla certezza della diagnosi.

### 13.4 Dalla diagnosi all'operatore eseguibile

Una diagnosi non implica che esista una correzione P0. Questa matrice collega le nove famiglie agli operatori del registro e dichiara il tipo di risposta ammessa. "Eseguibile P0" significa che l'operatore esiste, è validato nel dominio e può essere proposto come variante. "Solo avviso" significa che il difetto è rilevabile ma non correggibile con il registro attuale. "Non correggibile" significa che l'informazione non è nei dati. "Futuro" significa che l'operatore è pianificato in una milestone successiva.

| Famiglia | Operatore P0 eseguibile | Solo avviso o astensione | Non correggibile dai dati | Futuro |
|---|---|---|---|---|
| 1. Fondo e gradiente | F-POST-03.a con modello e maschera; bilanciamento canali F-POST-04.a | Fondo non separabile dalla nebulosa; dominante senza riferimento | Flat mancante: la risposta pixel-to-pixel non si ricostruisce da un gradiente | Modelli multiscala avanzati |
| 2. Rumore e pixel anomali | F-POST-09.a; maschera bad pixel F-CAL-04; dark e bias disponibili | Banding rilevato ma non corretto in P0 | Un pixel saturo o mancante non si recupera | Correzione banding e pattern specializzata in F-CAL-07, P1; metodi di denoise avanzati in F-POST-09.b |
| 3. Stelle e PSF | F-REG-01 per le metriche; F-POST-14.a per il confronto | Causa ottica non identificabile; PSF variabile sul campo fuori contratto | Dettaglio oltre il campionamento non si inventa | Modelli PSF complessi, deblending avanzato |
| 4. Saturazione e clipping | Maschera e avviso; F-POST-03.a per il fondo | Recupero oltre saturazione non garantito | Il clipping ha perso l'informazione: non si ricostruisce | Ricostruzione dichiarata solo con modello e dati aggiuntivi, non P0 |
| 5. Segnale debole e aliasing | F-POST-09.a con misura di danno; F-POST-11.a bounded | Aliasing reale non distinguibile senza ground truth | Nessuna verità di tracking o di struttura non campionata | Metodi avanzati e ML |
| 6. Artefatti di deconvoluzione e denoise | F-POST-10.a con baseline no-op; F-POST-09.a con rollback | Artefatto senza riferimento: astensione | Un dettaglio assente non si recupera con la deconvoluzione ordinaria | Richardson-Lucy, PSF variabile, deconvoluzione appresa |
| 7. Qualità dei frame | Scoring F-REG-01 e inclusione F-STK-03 | Beneficio d'inclusione ambiguo | Un frame mancante non si ricostruisce | Selezione avanzata per lucky imaging |
| 8. Master | Omissione motivata; F-CAL-01 per rigenerare | Master rumoroso accettato con limite dichiarato | Un master non acquisito non si ricostruisce con certezza | Dark scaling e master library P1 |
| 9. Affidabilità per dominio | Astensione e capability | Ogni metrica fuori dominio | Una misura non dichiarata non diventa valida per ripetizione | Nessuna |

Due conseguenze sono vincolanti. La presenza del denoise P0 non significa che il banding, il pattern noise o altre correzioni specializzate siano disponibili: restano F-CAL-07, P1, e una scheda che li proponga deve dichiararli non disponibili. Il clipping da saturazione è perdita di informazione: l'Advisor può mascherare, avvisare o proporre un modello dichiarato, non certificare un recupero. Allo stesso modo non esiste una verità di aliasing o di tracking inventata, e la deconvoluzione a PSF fissa non garantisce il recupero del dettaglio oltre il limite del dato.

### 13.5 Scheda di evidenza

Ogni raccomandazione produce una scheda con questi campi:

| Campo | Contenuto |
|---|---|
| Identificativo | ID della scheda, famiglia, versione motore e versione metrica. |
| Evidenza | Valore osservato, unità, ROI o heatmap, copertura e numero di campioni. |
| Confidenza | Livello e motivo, non un numero privo di calibrazione. |
| Ipotesi | Cause candidate, ordinate, con evidenza a favore e contro. |
| Alternative | Trattamenti alternativi, incluso il no-op. |
| Beneficio atteso | Ipotesi da verificare, non promessa. |
| Effetti collaterali | Perdita di segnale, aloni, clipping, smoothing, ringing, distorsione stellare. |
| Fase e prerequisiti | Dove intervenire nella pipeline e cosa deve essere già fatto. |
| Disponibilità | Operatore presente e validato, solo avviso, non correggibile o futuro. |
| Anteprima | Confronto prima e dopo, con possibilità di rifiutare o modificare. |
| Provenienza | Parametri, seed, versione, costo e collegamento al nodo DAG. |
| Astensione | Se la diagnosi non è identificabile, la scheda lo dichiara e non propone una correzione come certa. |

La scheda non è un messaggio pubblicitario. È un oggetto del progetto, ispezionabile e riproducibile.

### 13.6 Non identificabilità e astensione

Alcuni casi non sono identificabili dai soli dati disponibili:

- **Fondo additivo contro nebulosità estesa.** Se la nebulosa occupa il campo e non c'è una regione di cielo libero, il fondo e il segnale diffuso non sono separabili senza un prior. L'Advisor si astiene o propone solo un esperimento con confronto, non una sottrazione certa.
- **Flat contro gradiente.** Senza un flat, un gradiente non recupera la risposta pixel-to-pixel, la polvere o la vignettatura. Non si finge un flat.
- **Vignettatura moltiplicativa contro gradiente additivo.** Un fit può spiegare entrambi. Se il modello non è distinguibile, si mostra l'ambiguità e si chiede una scelta.
- **Dominante contro colore intrinseco.** Senza catalogo, SPCC o una referencia, non si dichiara che il colore è sbagliato.
- **Hot pixel contro cosmico.** Su un singolo frame l'ambiguità resta. Su più frame, la persistenza aiuta ma non è una prova assoluta.
- **Trailing contro coma o seeing.** L'ellitticità da sola non identifica la causa ottica.
- **Recupero oltre saturazione.** Non esiste informazione oltre il fondo scala. Si può mascherare, ricostruire con un modello dichiarato o rifiutare, non inventare.

L'astensione è un risultato di qualità. Non è un fallimento dell'Advisor. Non è nemmeno una strategia sufficiente: la sezione 13.16 richiede che il motore sia utile nei casi identificabili.

### 13.7 Tre obiettivi e multi-obiettivo

L'Advisor distingue tre obiettivi:

| Obiettivo | Cosa protegge | Esempi di metrica | Vincolo |
|---|---|---|---|
| Integrità scientifica | Originali, unità, correzioni dichiarate, incertezza, maschere, non invenzione | Bias residuo, flusso e validità per la misura richiesta | Vincolo forte sulle dichiarazioni e sul ramo di misura. Il ramo fotografico può trasformare il flusso, ma deve conservarne la provenienza e aggiornare le capability |
| Qualità tecnica | Rumore, PSF, SNR, artefatti, copertura | Sigma, FWHM, ringing, clipping, Neff | Soglie dichiarate per operatore |
| Resa fotografica | Contrasto, colore, composizione, leggibilità | Metriche di clipping, alone, dettaglio percepito, preferenza utente | Preferenza esplicita, non universale |

Non esiste un punteggio universale di bellezza. L'ottimizzazione è multi-obiettivo e mostra un vettore di metriche con i trade-off. Se l'utente sceglie un compromesso, la scelta è registrata. L'integrità scientifica non è sacrificabile per un guadagno estetico non dichiarato.

### 13.8 Ciclo di ottimizzazione

1. **Diagnosi.** L'Advisor identifica un problema con evidenza e confidenza. Se non è identificabile, si astiene.
2. **Candidati.** Seleziona algoritmi e intervalli di parametri ammessi dal contratto dell'operatore. Non esplora valori fuori dominio e non propone operatori assenti dal registro.
3. **ROI.** Esplora su regioni rappresentative: fondo, nebulosa, stelle, transizione, area satura. Le ROI non sono l'intera immagine.
4. **Gate globale.** Verifica a piena risoluzione in streaming, senza caricare tutto in RAM. Un miglioramento locale non basta.
5. **Controllo di regressione.** Usa regioni non usate per la taratura. Questo controllo riduce l'overfit locale, ma non è una validazione statistica indipendente (sezione 13.14).
6. **Regressione.** Controlla clipping, perdita di nebulosità, oversmooth, ringing e distorsione stellare. Se una metrica peggiora oltre la soglia, la variante non è proposta come miglioramento.
7. **Confronto.** Mostra il vettore di metriche e il confronto visivo. Non nasconde i trade-off.
8. **Conferma.** L'utente accetta, modifica o rifiuta. La variante è un branch reversibile.
9. **Provenienza.** Registra versione algoritmo, parametri, seed, metriche, costo CPU, RAM e scratch, e il collegamento al nodo DAG.
10. **Rollback.** In ogni momento si torna alla versione precedente senza perdere il progetto.

Il costo è bounded. L'utente imposta un limite di tempo, memoria o numero di varianti. L'Advisor stima il costo prima e si ferma se il budget non basta. Non esegue un ciclo illimitato a piena risoluzione.

### 13.9 Livelli di assistenza e viste

| Livello | Comportamento | Modifica il progetto | Modalità |
|---|---|---|---|
| Solo suggerimenti | Mostra schede e priorità, non esegue | No | Automatica, Guidata, Esperta |
| Anteprima e confronto | Crea una variante temporanea reversibile, mostra prima e dopo | Solo come branch non pubblicato | Automatica, Guidata, Esperta |
| Ottimizzazione con conferma | Propone un piano, esegue dopo conferma, con rollback | Sì, dopo conferma | Automatica, Guidata, Esperta |

Le viste indipendenti sono tre:

1. **Elenco delle raccomandazioni:** priorità, confidenza, fase, costo, disponibilità.
2. **Confronto:** prima e dopo, varianti, ispezione 1:1.
3. **Spiegazione:** evidenza, ipotesi, alternative, effetti collaterali.

Le tre viste leggono lo stesso motore e lo stesso DAG. Non esiste una quarta pipeline. Le viste non duplicano la logica di analisi. Livello di assistenza e modalità UI sono scelte indipendenti; cambiano esposizione e default, non le capacità del motore.

### 13.10 Semantica degli esiti

Cinque esiti sono distinti e non intercambiabili:

- **No-op:** le metriche sono nel dominio atteso. Non serve una correzione.
- **Astensione:** il dato non basta per decidere. L'Advisor spiega cosa manca.
- **Alternativa provata inadatta:** un test o un modello mostra che la correzione peggiora le metriche.
- **Operatore non disponibile:** la correzione esiste solo in una milestone futura. La scheda lo dichiara e non simula l'esecuzione.
- **Budget o ricerca incompleta:** l'esplorazione si è fermata prima di una conclusione. Il risultato parziale non è un candidato Pareto eleggibile e non viene presentato come vincitore.

"Nessun difetto rilevato" non significa "incerto". "Incerto" non significa "l'alternativa è provata cattiva". Confondere i casi produce raccomandazioni false o silenzio ingiustificato.

### 13.11 Confine di capacità

L'Advisor esegue solo operatori disponibili nel registro e validati nel dominio corrente. Può raccomandare un denoise P1 futuro, ma la scheda deve dire che l'operatore non è disponibile in P0 e non deve fingere una capacità inesistente. Può spiegare un beneficio atteso, non simularlo come se fosse già eseguito.

Questa regola evita due errori: promettere una funzione che non c'è e nascondere una funzione disponibile per non implementarla. La disponibilità è una proprietà del motore e del dominio validato, non della scheda.

Per import prestacked solo NL, il preflight e le schede Advisor dichiarano il limite C-14 di 10.15: senza antenato lineare verificabile non propongono fit, riduzione o separazione P0. Escludere un template illegale prima dell'avvio non è fallback; sostituirlo dopo un guasto è vietato. Un futuro metodo fotografico NL separato resta una proposta P1, non una capability disponibile.

Fuori dai domini validati l'unica risposta ammessa è l'astensione motivata. Un avviso può descrivere il difetto e la sua localizzazione, ma non può proporre l'esecuzione di un operatore che non è validato in quel dominio. La stessa regola vale quando il registro contiene l'operatore ma la fixture o il test di dominio mancano: l'operatore resta non disponibile per quel caso.

### 13.12 Costo, risorse e budget dell'intero batch

L'Advisor rispetta RAM, CPU, scratch e VRAM come il resto del motore. Non usa un budget separato non dichiarato. Il costo di una analisi o di una ottimizzazione è misurato e riportato. Se il budget non basta, l'Advisor si ferma e propone una nuova ricetta preregistrata con un batch più piccolo, oppure si astiene. Non riduce in automatico risoluzione, candidati o qualità e non avvia un job che manda il processo in OOM.

La prenotazione del budget copre l'intero batch congelato di candidati, non il singolo vincitore. Il preventivo include la baseline, i candidati globali, i checkpoint intermedi e la verifica finale. Se il batch non entra nel budget, il sistema lo rifiuta prima di partire. Non riduce in automatico il numero di candidati, la risoluzione o la qualità, e non avvia un numero di candidati che non può concludere.

La stima è incerta per definizione. Il prodotto non promette che ogni ricerca terminerà entro il budget. Promette che non supererà il budget gestito, che fermerà le nuove ammissioni sotto pressione, che creerà checkpoint e che un candidato parziale non sarà eleggibile. La cancellazione di una FFT o di un solver iterativo avviene su un confine di checkpoint: i buffer attivi non vengono revocati a metà calcolo, e i file scratch stale del job cancellato vengono rimossi o marcati.

### 13.13 Limiti aperti

- La calibrazione della confidenza richiede dataset con ground truth e non è stata eseguita.
- La separazione tra fondo e nebulosità diffusa resta non identificabile in alcuni campi.
- La valutazione estetica non ha un ground truth oggettivo e resta multi-obiettivo.
- Il costo computazionale dell'ottimizzazione su immagini grandi non è misurato.
- La qualità delle diagnosi su RAW di marche diverse dipende dai metadati disponibili.
- Non esiste ancora una revisione astrofisica indipendente del motore Advisor.

### 13.14 Controllo di regressione e indipendenza statistica

Due cose diverse vengono spesso confuse sotto la parola holdout.

Il **controllo di regressione spaziale** usa regioni tenute fuori dalla taratura dei parametri. Serve a scoprire un peggioramento locale che la ROI di taratura non mostra. Non rende indipendenti i campioni. La PSF, il rumore di fondo, le maschere e i master sono condivisi tra le regioni; un fit globale o una FFT possono mescolare informazioni di tutta l'immagine. Aggiungere buffer o ripetere il confronto non elimina questa dipendenza.

La **validazione statistica indipendente** richiede dati che non hanno contribuito alla taratura nemmeno indirettamente: acquisizioni separate, simulazioni con generatori indipendenti, e la dichiarazione delle componenti comuni come i master condivisi. La correlazione dovuta a un master comune va modellata o dichiarata. Se non lo è, l'incertezza misurata è ottimistica.

Il registro di derivazione dei dati, o data lineage, dichiara per ogni fit quali pixel, frame e acquisizioni hanno contribuito, quali master sono condivisi e quali trasformazioni sono globali. Il registro è la base per sapere se un test è un controllo di regressione o una validazione indipendente. Un test che non può dichiararlo non viene presentato come prova di indipendenza.

L'holdout congelato del planner serve a limitare l'overfitting della selezione. Non è una validazione scientifica e non va raccontato come tale. La distinzione è visibile nella scheda, nella provenance e nel report finale.

### 13.15 Protocollo anti-leakage della selezione

Il protocollo vale per l'Advisor e per ogni confronto tra varianti.

1. Le ROI di taratura, il controllo di regressione e il corpus di accettazione sono definiti prima dell'esplorazione e registrati con hash.
2. Il controllo di regressione non viene riusato in modo adattivo dopo che un candidato è stato scartato in base ad esso.
3. Il numero di confronti è registrato. La molteplicità è dichiarata e, quando il numero di candidati è elevato, il criterio di selezione è conservativo.
4. Il batch di candidati è congelato prima della valutazione finale. Un candidato aggiunto dopo la valutazione non entra nello stesso confronto.
5. Il budget dell'intero batch è riservato prima dell'esecuzione (sezione 13.12).
6. I dati di accettazione non sono usati per scegliere iperparametri, soglie o template.
7. Un candidato valutato su un endpoint incompatibile è `NON CONFRONTABILE`, non zero e non "peggiore".
8. La provenienza registra quale dataset ha visto cosa, in quale fase e con quale versione.

Il protocollo riduce il leakage noto. Non dimostra che la scelta generalizzi a ogni campo del cielo.

### 13.16 Corpus di accettazione indipendente e divieto di astensione sistematica

Un motore che si astiene sempre non sbaglia mai una diagnosi e non è utile. Per questo l'accettazione usa un corpus indipendente dai dati di sviluppo, con generatori sintetici che non si limitano agli stessi modelli di PSF usati per tarare il motore. Il corpus include PSF diverse, rumore con caratteristiche diverse, errori di verità tra rumore e PSF simulate, e casi in cui la diagnosi è identificabile.

Prima dell'esecuzione si dichiarano due famiglie di misure:

- **Copertura utile:** quota dei casi identificabili in cui il motore produce una diagnosi corretta o un no-op corretto.
- **Danno e beneficio:** per ogni caso identificabile, effetto di una raccomandazione accettata rispetto al no-op, con metriche di segnale, artefatti e costo. Un beneficio medio non compensa un danno grave non dichiarato.

L'accettazione richiede che il motore sia utile nei casi identificabili. L'astensione è valutata come esito corretto solo dove l'identificabilità è davvero assente. Una preferenza soggettiva dell'utente resta una preferenza: non diventa una metrica scientifica e non si presenta come tale.

### 13.17 Test di falsi positivi, falsi negativi e astensione

La suite Advisor include:

- **Caso pulito:** nessun difetto introdotto. L'Advisor non deve raccomandare una correzione dannosa. Un no-op è accettato.
- **Segnale debole:** struttura a basso SNR. Si misura la detection e la confidenza.
- **Nebulosità diffusa:** il fondo non è separabile. Si misura l'astensione e la protezione del segnale.
- **Campo affollato:** stelle e PSF si confondono. Si misura la stabilità delle metriche.
- **Stelle sature:** aloni e clipping. Si misura il rifiuto di recupero inventato.
- **Out of distribution:** dato con proprietà non viste, per esempio pattern CFA inatteso, rumore anomalo o immagine non lineare. L'Advisor deve abbassare la confidenza o astenersi.
- **Copertura:** ogni report dichiara quali famiglie e quali domini sono stati analizzati e quali no.

Le metriche di accettazione sono falsi positivi, falsi negativi, tasso di astensione corretto, copertura utile, danno e beneficio nei casi identificabili e calibrazione della confidenza. Non si dichiara una sensibilità universale. Le soglie si derivano dai dataset e dal ground truth.

---

## 14. Modalità utente, flussi e accessibilità

### 14.1 Le tre modalità

| Modalità | Obiettivo | Comportamento richiesto | Limite |
|---|---|---|---|
| **Automatica** | Risultato valido con pochi passaggi | Riconosce il dataset, propone parametri motivati, esegue la pipeline, mostra punteggi e avvisi, consente override. Usa l'Advisor per diagnosi e confronto, con conferma prima di una modifica sostanziale. Non inventa risultati e non nasconde dati inadeguati. | Non decide al posto dell'utente su dati ambigui. |
| **Guidata** | Imparare elaborando | Wizard con spiegazioni brevi, anteprima delle trasformazioni, suggerimenti motivati, schede di evidenza e accesso al dettaglio avanzato. | Non nasconde i parametri critici. |
| **Esperta** | Pieno controllo | Tutti gli algoritmi e parametri, nodi modificabili, confronto output, maschere, log diagnostici, preset e batch. L'Advisor è consultabile e disattivabile. | Non aggira i controlli di integrità. |

Le tre modalità non sono tre prodotti. Sono tre livelli di esposizione dello stesso DAG, degli stessi operatori, degli stessi report e delle stesse viste Advisor. Un cambio di modalità non deve cambiare il risultato a parità di parametri.

### 14.2 Flussi critici

**Primo import.** L'utente sceglie una cartella, il prodotto inventaria i file, propone la classificazione, segnala file mancanti o formati non supportati e mostra una preview senza caricare tutto in RAM. Il primo risultato utile è visibile prima di qualsiasi calibrazione.

**Calibrazione parziale.** L'utente vede i master disponibili, lo stato di ciascuno, la compatibilità con i light e le conseguenze di ogni omissione. Un master mancante non blocca tutto. Un master incompatibile non viene usato in silenzio. La schermata mostra la matrice delle correzioni applicate e non applicate.

**Registrazione e stack.** Il prodotto mostra stelle rilevate, match, residui, frame scartati, coverage e tempo stimato. Con N basso spiega perché non applica rejection statistica. Mostra `Neff` e pixel senza copertura.

**Analisi e Advisor.** L'utente vede le schede di evidenza, la confidenza, le alternative, il costo, la disponibilità e la fase consigliata. Può accettare, modificare, confrontare o rifiutare. Un no-op è mostrato come esito valido. Una diagnosi incerta è dichiarata. Un operatore futuro è etichettato come non disponibile.

**Post-processing.** Il viewer distingue preview e dato. Background, bilanciamento, stretch, curve e maschere sono annullabili. L'utente può tornare a un nodo a monte e vedere cosa viene invalidato.

**Export.** L'utente sceglie FITS o TIFF, profondità, colore e destinazione. Il file porta la provenance. Ciò che il formato non rappresenta va nel manifest o nel sidecar. Un export non sovrascrive l'originale.

**Recovery.** Dopo un crash, il prodotto propone il ripristino delle fasi sicure, elenca i risultati incompleti e non pubblica un output parziale.

**Risorse.** L'utente vede budget, cache, scratch, thread e VRAM. Può cambiare il budget e capire perché un job è stato rifiutato.

### 14.3 Viewer e schermate critiche

Il viewer gestisce tile visibili e piramidi bounded. Mostra zoom 1:1, pan, istogramma asincrono e indicatori di clipping. La precisione del display è separata dalla precisione dei dati. Il profilo ICC o la trasformazione sRGB serve a mostrare, non a riscrivere i pixel.

Le schermate critiche da progettare e testare sono: workspace di import, inspector di calibrazione, pannello stelle e registrazione, report di stacking, editor di post-processing, pannello Advisor, confronto, export con provenance, preferenze risorse, recovery. Non servono dashboard decorative. Ogni schermata deve avere un compito elaborativo.

L'editor delle maschere mostra sempre la politica della maschera: asset congelato o generatore derivato, sorgente, revisione, trasformazioni di coordinate applicate e stato di validità dopo un riordino. Se la politica manca, il candidato non è valido.

### 14.4 Accessibilità e localizzazione

L'accessibilità è P0. Il prodotto deve funzionare da tastiera per i controlli essenziali, mostrare il focus, avere tooltip e nomi accessibili, supportare HiDPI e tema chiaro e scuro, e non usare il colore come unico segnale. Un controllo non deve richiedere solo il mouse. Le maschere, il viewer e le heatmap Advisor devono avere alternative testuali o indicatori non cromatici.

La localizzazione italiana e inglese è proposta. L'architettura deve tenere le stringhe fuori dal codice e permettere altre lingue. Il coordinatore decide il livello di traduzione iniziale dopo verifiche UX. Non si promette una traduzione completa prima di averla verificata.

### 14.5 Test UX proposti

Questi numeri sono proposte di test, non risultati ottenuti. Non autorizzano inferenze sulla popolazione generale.

- Sei principianti: almeno cinque completano il percorso dichiarato con l'aiuto contestuale.
- Dodici utenti: almeno undici distinguono correttamente preview STF e dato reale.
- Cinque esperti: cinque completano un compito di modifica cache e mostrano di capire cosa viene invalidato.
- Sei utenti: almeno cinque capiscono una scheda Advisor e sanno rifiutare una proposta senza rompere il progetto.
- Sei utenti: almeno cinque distinguono "nessun difetto rilevato" da "diagnosi incerta".
- Sei utenti: almeno cinque distinguono un operatore disponibile da uno dichiarato futuro.

I test vanno registrati con dataset, compito, tempo, errori e osservazioni. Un campione piccolo non dimostra che tutta la popolazione capirà il prodotto. Serve a trovare attriti e regressioni.

---

## 15. Testing, dataset e benchmark

### 15.1 Livelli di test

1. **Unitari deterministici.** Operazioni pixel-level, CFA, normalizzazione flat, operatori numerici, metadati, hash, invalidazione, matrici di resampling.
2. **Property-based o metamorphic.** Linearità dove richiesta, conservazione del flusso e della brillanza per lo stimando dichiarato, invarianza a offset e addizione, frame identici che producono la stessa immagine, ordine dei canali che non cambia il risultato dove non dovrebbe.
3. **Sintetici con ground truth.** Stelle gaussiane con PSF nota, gradienti controllati, shot noise e read noise, hot pixel, rotazioni, deformazioni, traslazioni subpixel, cosmici, saturazione e bordi.
4. **Reali pubblici.** RAW di più marche, FITS mono, CFA e RGB, multi-sessione, narrowband, mosaici, frame falliti e dataset più grandi della RAM. Ogni dataset ha licenza, provenienza e hash.
5. **Confronto esterno.** Siril, PixInsight e APP sono usati solo come benchmark esterno, se legalmente e tecnicamente disponibili. Non c'è dipendenza del prodotto. Versioni, input e parametri sono annotati. Siril non è fonte implementativa.
6. **Scientifici.** Bias residuo, stima del rumore e della varianza, SNR, FWHM, eccentricità, accuratezza del matching, conservazione del flusso, errore di calibrazione cromatica, residui WCS, covarianza tra output.
7. **Calibrazione flessibile.** Otto combinazioni, dark-flat, master precalibrati, incompatibilità e omissioni, senza doppia sottrazione.
8. **Advisor.** Nove famiglie, falsi positivi e negativi, astensione, out of distribution, no-op, disponibilità degli operatori, corpus di accettazione indipendente, danno e beneficio.
9. **Robustezza.** OOM, budget basso, disco pieno, permessi, interruzione, riavvio, file corrotti, pressione CPU o GPU, timeout di kernel.
10. **UI.** Flusso guidato, configurazione risorse, feedback errori, cancellazione, non blocco della UI, accessibilità, HiDPI, X11 e Wayland.
11. **Benchmark e regressione.** Dataset standard, baseline versionate, ripetizioni, cache calda e fredda, stesse condizioni dichiarate.
12. **Packaging Linux.** Installazione, aggiornamento, primo avvio offline, CPU-only, GPU disponibile, rimozione e pulizia.
13. **Configurazioni P0.** Matrice di attivazione della sezione 11.11, con controllo di ordine temporale e chiusura padre-foglia.
14. **Documentali.** Conteggi degli identificativi, riferimenti incrociati, coerenza delle formule, registro delle sessioni, hash degli input e assenza di contraddizioni. Una suite documentale ripetibile e automatica è futura.
15. **Non-sostituzione e resume.** Un errore, una risorsa assente o un gate fallito produce stop, codice non nullo e diagnosi; nessun backend, algoritmo o percorso alternativo parte in automatico. Il resume con ricetta, metodo, backend o parametri diversi è rifiutato come prosecuzione e apre una nuova ricetta. Un candidato fallito non viene sostituito né promosso; una validazione su insieme incompleto non è dichiarata completa.

### 15.2 Identificativi di test usati nel documento

I riferimenti `verified_by` puntano a questi test. La tabella fissa il nome; il contenuto dettagliato cresce con l'implementazione.

| Test | Oggetto | Criterio minimo |
|---|---|---|
| **T-IO-01** | Import FITS e metadati | Roundtrip, unità, header non interpretabili conservati |
| **T-PRJ-01** | Roundtrip di progetto | Stesso stato logico dopo riapertura |
| **T-CAL-08** | Otto combinazioni e dark-flat | Nessuna doppia sottrazione; omissione motivata; otto esiti dichiarati |
| **T-REG-02** | Trasformazioni sintetiche | Parametri recuperati entro tolleranza; ambiguità rifiutata |
| **T-STK-03** | Pesi e varianza | Invarianza al riscalamento comune; `Neff`; copertura |
| **T-POST-09** | Denoise falsificabile | Scena nota, trasformazione nota, realizzazioni indipendenti, danno misurato |
| **T-POST-10** | Deconvoluzione PSF fissa | Baseline no-op, sensibilità a PSF, fondo, `lambda` e rumore, negativi legittimi |
| **T-POST-12** | Riduzione e separazione | Due gate separati; reintegrazione neutra solo con la stessa `S_L`; `R_T + S_T = T(I_L)` verificata |
| **T-AST-ASSIST01** | Solver assistito | Hint corretti ed errati, pack mancante, fuori catalogo, falsa soluzione |
| **T-FBK-01** | Non-sostituzione e resume | Backend o risorsa assente → errore non nullo e nessuna continuazione alternativa; resume identico ammesso, resume difforme rifiutato; candidato fallito non sostituito; validazione incompleta non dichiarata completa |
| **T-ANA-01** | Diagnostica | Nove famiglie, unità, dominio, astensione |
| **T-ANA-04** | Planner | Legalità, budget dell'intero batch, stati di esito, nessun candidato parziale eleggibile |
| **T-VAL-INDEP** | Indipendenza | Dichiarazione di lineage; nessun riuso adattivo del controllo di regressione |

### 15.3 Dataset

I dataset devono essere pubblici e licenziati, oppure sintetici e generati dal progetto. Non si chiedono foto private all'utente e non si usano immagini personali come fixture obbligatoria.

Criteri di selezione:

- licenza compatibile con l'uso di test e con la distribuzione del progetto;
- provenienza e hash registrati;
- copertura di più marche RAW, non una sola;
- FITS mono, CFA e RGB;
- casi di N basso, frame falliti, saturazione, nuvole, gradienti e amp glow;
- dataset più grande del budget di memoria per i test di spill;
- assenza di dati personali o metadati sensibili;
- difetti sintetici noti per l'Advisor: gradiente, vignettatura, dominante, banding, hot pixel, seeing variabile, trailing, saturazione, nebulosità diffusa, segnale debole;
- casi di assenza o incompatibilità dei master;
- casi out of distribution per l'astensione;
- corpus di accettazione separato dai dati usati per sviluppare e tarare il motore.

La selezione è un lavoro di M0 e M3. Non è stata fatta in questa revisione. Un dataset senza licenza chiara non entra nella suite.

### 15.4 Metriche, soglie e protocollo

Le soglie non sono inventate prima del ground truth. Per ogni operatore si dichiarano:

- lo stimando e l'unità;
- la metrica e il suo dominio di validità;
- la tolleranza derivata dal ground truth o dalla precisione del dato;
- il test che la verifica;
- il comportamento in astensione;
- il costo massimo ammesso.

Le metriche minime per famiglia sono nella sezione 8.13 e nei contratti della sezione 10. Il documento non aggiunge soglie universali. Un numero senza derivazione resta una proposta da validare.

Il protocollo anti-leakage della sezione 13.15 si applica a ogni confronto tra varianti, non solo all'Advisor. I test di regressione spaziale e le validazioni indipendenti sono etichettati in modo distinto nel report.

### 15.5 Confronto esterno e limite su Siril

Il confronto con Siril, PixInsight e APP è esterno e non è una dipendenza. Non si legge il codice del concorrente per dedurre l'implementazione. Non si dichiara un confronto se non è legalmente e tecnicamente possibile. Versioni, parametri, hash e condizioni vanno registrati.

### 15.6 Stato di validazione

Nessun test di questa sezione è stato eseguito. Il documento definisce cosa va provato, con quali metriche e con quali gate. La distinzione tra controllo di regressione e validazione indipendente è una regola di metodo, non un risultato sperimentale.

---

## 16. Linux, compatibilità e packaging

### 16.1 Livelli di supporto

| Livello | Significato | Impegno |
|---|---|---|
| Verificato | Installazione, avvio, percorso P0 e test dichiarati passano su quella configurazione. | Supporto e regressione bloccante. |
| Compatibile non certificato | Funziona in prova esplorativa, ma non ha la matrice completa. | Nessuna garanzia di regressione. |
| Non supportato | Non testato o con problemi noti. | Nessun impegno. |

La matrice non copre ogni distribuzione storica. Un ambiente non provato non diventa "verificato" perché è simile a uno della lista.

### 16.2 Matrice minima proposta

La matrice minima è una proposta di M0, non una misura. Le configurazioni concrete si scelgono con criteri di stabilità, disponibilità del toolkit, driver e rappresentatività. La matrice deve contenere almeno una configurazione X11 e una Wayland, una CPU-only e una con GPU disponibile.

| Livello | Configurazione proposta | Sessione | Accelerazione | Perché è nella matrice |
|---|---|---|---|---|
| **L1** | Ubuntu 24.04 LTS, x86_64, kernel della distribuzione | Wayland e X11 nativo | CPU-only | Base più diffusa, verifica il percorso P0 completo e il packaging principale |
| **L2** | Debian 12, x86_64, glibc più vecchia della L1 | X11 nativo | CPU-only | Verifica compatibilità ABI e dipendenze conservative |
| **L3** | Fedora 42, x86_64, Mesa recente | Wayland | GPU AMD o Intel facoltativa | Verifica Wayland recente, driver Mesa e percorso GPU opzionale |

Le tre configurazioni sono proposte, non verifiche. In M0 il coordinatore definisce autonomamente matrice minima e protocollo su evidenze; M5 dichiara verificato solo ciò che ha passato i test. Il fallimento di una configurazione obbligatoria blocca M5. Correggere e ripetere: non rimuovere una configurazione per aggirare il gate o ridurre la copertura promessa.

### 16.3 Copertura progressiva

M0 definisce il protocollo. M1 fa prove preliminari su viewer, toolkit e packaging. M5 verifica tutta la matrice minima approvata. Gli ambienti aggiuntivi non provati restano fuori dalla copertura dichiarata; non sostituiscono una configurazione obbligatoria che ha fallito.

### 16.4 Minimi misurati

I minimi di ABI, kernel e CPU si misurano. M0 non li inventa. Un minimo senza prova resta una proposta. x86_64 è il candidato iniziale; ARM64 è uno studio senza impegno di release.

### 16.5 Desktop e display

X11 nativo e Wayland sono provati separatamente. HiDPI, tema chiaro e scuro, tastiera, focus, IME e color management sono parte dei test. XWayland non è X11 nativo e non si presenta come tale.

### 16.6 Packaging

AppImage, Flatpak e pacchetti nativi si confrontano su installazione, aggiornamento, primo avvio offline, integrazione desktop, sandbox, dimensione, dipendenze, riproducibilità e rimozione. La scelta è documentata in ADR-12 e verificata in M5.

Il catalog pack per colore relativo e solver assistito è un artefatto versionato distinto, incluso nella distribuzione secondo la preferenza D-21 APPROVATA e subordinato alle verifiche di 17.4. Distinto non significa facoltativo per una ricetta che lo richiede.

### 16.7 Criteri di release Linux

La release M5 richiede: almeno una modalità di packaging verificata sulla matrice minima; primo avvio offline; X11 nativo e Wayland provati; nessuna regressione P0; SBOM aggiornato; documentazione di installazione e rimozione; nessun download implicito di cataloghi o modelli.

---

## 17. Licenze, dipendenze e SBOM

### 17.1 Candidati e stato

| Dipendenza o famiglia | Uso previsto | Stato licenza | Nota |
|---|---|---|---|
| CFITSIO | I/O FITS | Da verificare prima dell'adozione | Candidata, non approvata. |
| LibRaw | Decodifica RAW | Da verificare, incluse le implicazioni della licenza | Candidata, non approvata; il decoder deve dichiarare offset e crop. |
| Libreria FFT | Convoluzione, deconvoluzione, multiscala | Da scegliere | Confronto su licenza, precisione e supporto. |
| Toolkit GUI | Viewer e shell | Da scegliere con ADR-02 | Nessuna decisione anticipata. |
| SQLite | Persistenza proposta | Da verificare nella forma distribuita | ADR-04. |
| Librerie di compressione | Formati e cache | Da verificare | Nessuna adozione senza inventario. |
| Cataloghi stellari | Colore relativo e solver | Licenza e ridistribuzione da verificare per ogni pack | Un pack senza diritti chiari non si distribuisce. |
| Modelli AI facoltativi | M8 | Licenza, pesi e condizioni d'uso da verificare | Non necessari per il P0. |

L'elenco è un inventario di candidati, non un SBOM. Un candidato non è adottato finché licenza, versione, hash e dipendenze transitive non sono registrati.

### 17.2 Processo di adozione

1. Identificare la necessità e l'alternativa interna.
2. Verificare licenza, versione, hash, manutenzione e vulnerabilità note.
3. Registrare la decisione e l'uso previsto.
4. Aggiungere al SBOM prima della distribuzione.
5. Riesaminare quando cambia versione o licenza.

Nessuna libreria entra perché è comoda. Nessuna licenza si presume compatibile senza verifica.

Prima dell'adozione, verificare anche NC-21: percorsi effettivi, errori e automatismi devono essere osservabili e controllabili. Una dipendenza che impone fallback non osservabili o non disattivabili è NON CONFORME e non viene adottata. Il controllo riguarda anche dipendenze transitive, decoder, driver, toolkit, gestione colore, librerie numeriche e writer. Non si accetta una dichiarazione del wrapper se la libreria può sostituire internamente il percorso scelto.

Sono vietati, dopo un errore: CPU al posto di GPU, scalare al posto di SIMD, sRGB al posto di ICC, un altro decoder, output a 8 bit al posto del formato selezionato, varianza diagonale al posto del modello scelto e no-op al posto di un candidato fallito. Nessuna autorizzazione dell'utente rende lecito un fallback. Una nuova ricetta manuale è una nuova esecuzione, non un resume; il rollback esplicito resta ammesso. Le scelte primarie nel preflight, incluse calibrazione parziale fra otto combinazioni e trattamento low N, non sono risposte sostitutive a un fallimento.

### 17.3 Licenza del prodotto

La licenza del prodotto resta una decisione del proprietario. Il piano non la sceglie e non la deduce dalle dipendenze. Le incompatibilità tra licenze si affrontano prima dell'adozione, non dopo la release.

### 17.4 Distribuzione dei catalog pack, D-21 approvata e verifiche pendenti

Il colore relativo e il solver assistito richiedono un pack locale con catalogo, indice, versione e licenza. D-21 approva la preferenza per l'inclusione nella distribuzione. Il confronto seguente conserva motivi e alternativa in caso di blocco documentato:

| Opzione | Vantaggio | Costo e rischio |
|---|---|---|
| Pack incluso e distribuito con il prodotto | Percorso offline completo e riproducibile per l'utente. | Dimensione del pacchetto, condizioni di ridistribuzione, aggiornamenti e SBOM. |
| Pack separato, scaricato o installato manualmente | Pacchetto applicativo più piccolo e licenze separate. | Primo avvio non completo senza un passaggio esterno; serve una verifica di integrità e versione. |

D-21 è APPROVATA come preferenza per pack inclusi. La verifica legale e tecnica non è stata eseguita: licenza e diritti di ridistribuzione, provenienza, integrità, dimensione, versione, aggiornamento, copertura celeste e bande supportate vanno accertati autonomamente prima dell'adozione. Non si inventano dimensioni o audit riusciti. G-PACK e NFR-LIC-01.d restano gate reali, non soddisfatti dall'approvazione della preferenza.

La distribuzione Linux offline completa deve contenere i pack leciti necessari ai domini dichiarati, manifest con hash e versioni, licenze e indici compatibili. M5 prova installazione pulita e primo avvio senza rete, inclusi colore e solver nei domini dichiarati. Pack richiesto assente, incompatibile o con hash errato: STOP del ramo prima dell'operazione, senza ricerca online, download implicito o cambio del pack selezionato.

Se inclusione, licenza o dimensioni non sono sostenibili, non distribuire il pack e non simularne la presenza. Registrare causa e dipendenze bloccate, verificare un canale separato di installazione dichiarato e preparare una proposta alternativa motivata. La proposta non modifica a runtime la ricetta e non chiude M5 senza prove; il lavoro indipendente continua.

---

## 18. Export, colore, provenance e riproducibilità

### 18.1 FITS

Il writer FITS dichiara `BITPIX`, `BUNIT`, endianness, estensione e metadati. Non applica uno stretch implicito. Un dato lineare esportato resta lineare salvo richiesta esplicita. Maschere, varianza e coverage vanno in estensioni o sidecar quando il formato lo consente.

### 18.2 TIFF

Il TIFF dichiara profondità, campionamento, profilo colore e orientamento. Un TIFF a 8 bit è un output di visualizzazione, non una misura. Il passaggio a 8 bit è esplicito e non automatico.

### 18.3 Manifest e sidecar

Il manifest contiene hash degli output, versione writer, parametri, trasformazioni applicate, elenco dei file e marker di commit. Ciò che il formato non rappresenta va nel sidecar. Un bundle senza manifest valido è incompleto.

### 18.4 Colore e display

Il percorso dati è lineare e distinto dal percorso di visualizzazione. La trasformazione ICC o sRGB mostra, non riscrive i pixel. Un export fotografico dichiara lo spazio colore e la trasformazione cotta. Il mapping di gamut è registrato perché può alterare i rapporti tra canali (sezione 10.10.1).

### 18.5 Riproducibilità nel tempo

Un risultato è riproducibile quando input, hash, parametri, versioni, backend, seed, maschere e trasformazioni sono registrati. Un cambio di versione algoritmo può cambiare il risultato: la provenance lo dichiara. La cache non è la storia, ma la ricetta deve permettere la ricostruzione.

### 18.6 Input spostati, mancanti o corrotti

Un input spostato si ricollega solo con hash verificato. Uno mancante blocca il ramo dipendente. Uno corrotto non si ripara in silenzio: si segnala e si chiede una sorgente valida.

### 18.7 Output AI

Un output AI è separato dal percorso di misura, dichiarato nella provenance e non presentato come dato osservato. Il percorso non AI è primario, completo e indipendente; nessun guasto di un modello AI attiva una sostituzione automatica. Un modello senza licenza verificata non entra nella release.

---

## 19. Sicurezza, privacy e affidabilità

### 19.1 Modello di minaccia

I rischi principali sono: input malformati, path traversal, symlink e hardlink, scrittura fuori dallo scratch, sovrascrittura degli originali, esecuzione di contenuto non fidato, esaurimento di memoria o disco, corruzione del progetto, fuga di metadati personali nei log, dipendenze con licenza o vulnerabilità non verificate.

Il prodotto è locale e offline per il percorso P0. Non richiede account e non invia immagini. Le funzioni online future sono opt-in.

### 19.2 Controlli richiesti

- Limiti di dimensione, profondità, numero di frame e decompressione nei parser.
- Nessun eval, nessuno script arbitrario, nessun comando di sistema dal progetto.
- Export e cleanup confinati alla destinazione dichiarata, con protezione da symlink, hardlink e path traversal.
- Scrittura atomica, fsync e commit del database dopo la verifica.
- Quote di scratch e cache, cleanup verificato, nessuna cancellazione fuori dalle directory dichiarate.
- Log senza percorsi personali non necessari e senza metadati sensibili.
- Fuzz e sanitizer sui parser critici.
- SBOM e verifica delle licenze prima della distribuzione.

Il cleanup non si limita a controllare un path risolto: protegge da race e symlink durante l'operazione.

---

## 20. ADR proposti, 17 decisioni

Gli ADR tecnici restano aperti finché mancano le evidenze richieste. Il coordinatore li decide autonomamente dopo prove e review, prima del codice dipendente, senza autorizzazioni rituali. Restano riservate solo le questioni di 0.8, in particolare licenza irreversibile del codice, costi nuovi e distribuzione commerciale. La numerazione 01 a 17 e il contenuto tecnico sono conservati; questa revisione non aggiunge ADR.

| ID | Tema | Opzioni | Proposta e stato | Evidenze e limiti | Gate | Responsabilità decisionale |
|---|---|---|---|---|---|---|
| **ADR-01** | Linguaggio e componenti | C++20 o 23, Rust, Python con core nativo, C#/.NET, ibrido | Core C++20 con Qt Widgets come raccomandazione provvisoria; Rust con GTK4 secondo; Rust con Qt e ibrido Python da misurare. Core headless. Proposto, non deciso. | Analisi di architetto e ricerca, nessuno spike. Disaccordo conservato. | M1 | Coordinatore dopo spike misurati e review |
| **ADR-02** | GUI e comunicazione con il core | Qt Widgets o QML, GTK4 e libadwaita, egui, Slint, Tauri, Avalonia | Scegliere dopo confronto su viewer, licenza, accessibilità, colore, IME, docking e packaging. API tipizzata o command bus. Nessuna logica scientifica nella GUI. Proposto. | Criteri noti, prove assenti. | M1 | Coordinatore dopo prove e review UX |
| **ADR-03** | Modello pixel, precisione e maschere | Array contiguo, tile, piramide, f32, f64, variance e weights | Immagine tipizzata con unità, CFA, geometria, linearità, maschere, coverage, varianza e capability. f32 candidato, f64 per condizionamento. Maschera di editing distinta da invalidità e alpha. Proposto. | Coerenza con i requisiti, nessuna misura di memoria. | M2 | Coordinatore e architettura dopo evidenze |
| **ADR-04** | Persistenza del progetto | SQLite, JSON, ibrido, file binari | SQLite come sola autorità proposta, cache esterna derivata, protocollo atomico, migrazioni con backup, schema sconosciuto in sola lettura. Non approvato. | Transazioni e query a favore, migrazione e formato a sfavore. | M2 | Coordinatore dopo prove di recovery e migrazione |
| **ADR-05** | DAG, cache e invalidazione | DAG immutabile, grafo mutabile, cache per hash | DAG immutabile con hash di contenuto, metadati, parametri canonici, versione algoritmo, decoder, precisione e backend. Token di generazione contro risultati stale. Il grafo è aciclico ma la completezza si verifica per configurazione (sezione 11.11). La provenienza distingue controllo di regressione e validazione indipendente. Proposto. | Invarianti chiari, costo hash da misurare. | M2 | Coordinatore e architettura dopo evidenze |
| **ADR-06** | RAM, scheduler e spill | Limite hard RSS, pool gestito, ibrido | Pool gestito con prenotazioni, code bounded, backpressure, eviction, spill quota, checkpoint e prevenzione deadlock. RSS best effort e misurato. La prenotazione copre l'intero batch di candidati, non il singolo vincitore. Se il budget non basta, il job si ferma con errore non nullo: non riduce risoluzione, candidati o qualità e non passa a un percorso alternativo (NC-21, NFR-FBK-01). Proposto. | Distinzione corretta, nessuna misura su dataset grandi. | M2 | Coordinatore e architettura dopo misure |
| **ADR-07** | CPU, SIMD e GPU | CPU-only, Vulkan, OpenCL, CUDA, HIP o ROCm | CPU di riferimento con dispatch SIMD nel preflight; il percorso CPU e quello scalare sono primari. GPU facoltativa, backend da confrontare, nessuna sostituzione automatica della GPU con la CPU. Ricerca GPU in M1, produzione in M7. Proposto. | Nessun benchmark eseguito. | M1 e M7 | Coordinatore dopo misure |
| **ADR-08** | Import RAW, FITS e XISF | CFITSIO, LibRaw, XISF, implementazioni proprie | Usare librerie mature per I/O, con licenze verificate, e mantenere il core indipendente da Siril. Contratto del decoder esplicito. Proposto. | Librerie candidate, licenze da verificare. | M2 e M3 | Coordinatore dopo audit e prove |
| **ADR-09** | Calibrazione, registrazione e stacking | Formule e ordini alternativi | Applicare i casi delle sezioni 8 e 9, con ground truth, maschere, varianza e report. Nessuna formula universale. Per la deconvoluzione, una sola formulazione P0: `b` fisso stimato a monte, `W` PSD che approssima `C_V^{-1}` sul sottospazio valido, sensibilità a PSF, fondo, `lambda` e rumore, baseline no-op. Proposto. | Matematica controllata documentalmente, validazione assente. | M3 | Coordinatore dopo fonti primarie, prove e review numerica |
| **ADR-10** | Colore, WCS e maschere | ICC, sRGB, linear workspace, WCS refit | Separare dati lineari e display, conservare o invalidare il WCS dopo warp, propagare maschere e covarianza, non confondere alpha e maschera. Il WCS risolto non valida il colore: servono trasformazione ammessa, riferimento, risposta e bande. I rapporti RGB si dichiarano per componenti di riferimento e per stadio (sezione 10.10.1). Proposto. | Standard candidati, test assenti. | M3 e M4 | Coordinatore dopo prove e review tecnica/UX |
| **ADR-11** | Plugin e API di scripting | Nessun plugin, API locale, sandbox, processo separato | Rinviare a P2 e M9. Se introdotto, API versionata, sandbox e nessun eval. Proposto. | Nessuna necessità P0. | M9 | Coordinatore dopo evidenze e review sicurezza |
| **ADR-12** | Packaging, release e licenza | AppImage, Flatpak, nativo, misto | Politica licenze prima delle adozioni strutturali; confronto del packaging in anticipo e scelta prima di chiudere M5, che verifica la modalità scelta. Licenza irreversibile del codice e distribuzione commerciale restano riservate. D-21 approva la preferenza pack inclusi secondo 17.4. | Trade-off noti, prove assenti. | M0 e M5 | Coordinatore per packaging e audit; proprietario solo per questioni riservate |
| **ADR-13** | Offline, cataloghi e AI | Offline puro, online opzionale, modelli locali | Core e Advisor offline. Funzioni online opt-in. AI facoltativa con modelli, licenze e provenance dichiarate. I catalog pack sono locali, versionati e richiedono licenza verificata. Proposto. | Requisito chiaro, modelli non scelti. | M2 e M8 | Coordinatore dopo audit e prove; nessun costo nuovo autorizzato implicitamente |
| **ADR-14** | Confini deep sky, planetario e scientifico | Core unico, moduli separati, prodotti separati | Core condiviso, workflow e test separati. Nessun percorso planetario forzato nel deep sky. Proposto. | Coerenza con roadmap, algoritmi separati da definire. | M0 e M9 | Coordinatore e architettura dopo evidenze nel perimetro autorizzato |
| **ADR-15** | Motore di analisi e Advisor | Nessun Advisor (escluso dal mandato), regole deterministiche, statistico, ML | Advisor deterministico, offline, con nove famiglie, schede di evidenza, confronto, astensione, tre livelli di assistenza e assistenza eseguibile solo nel registro validato. P0 vincolante per requisito; default e soluzione tecnica proposti. Dopo un errore non produce un no-op sostitutivo, non promuove un candidato nuovo al posto di quello fallito e non dichiara completa una validazione incompleta. | Requisiti F-ANA, nessun test. | M3 e M4 | Coordinatore dopo prove di utilità, costo e UX |
| **ADR-16** | Calibrazione incompleta | Richiedere tutti i master (escluso dal mandato), permettere ogni sottoinsieme con matrice, ibrido | Matrice delle otto combinazioni, dark-flat come quarto asse, stati tipizzati dei master, omissione delle correzioni opzionali prima della ricetta, blocco del solo ramo obbligatorio. P0 vincolante per requisito; matrice e default da validare. | Matematica e casi definiti, test assenti. | M3 | Coordinatore dopo prove e review scientifica |
| **ADR-17** | Covarianza opzionale | Nessuna covarianza, diagonale, NPS locale, densa | Diagonale per pixel più NPS locale, kernel e fattore di apertura, con correlazioni esplicite dei master e incertezza della geometria quando modellata. Nessuna matrice globale densa richiesta. Proposto. | Approssimazione dichiarata, validazione assente. | M3 e M6 | Coordinatore dopo prove e review numerica |

### 20.1 Note sugli ADR più critici

**ADR-01.** La matrice storica non è una misura. C++ con Qt ha una raccomandazione provvisoria per costo di integrazione ed ecosistema. Rust con GTK vince se la sicurezza della memoria diventa il criterio dominante. La scelta va fatta con spike su kernel rappresentativi, viewer e integrazione, con rivalutazione dei pesi. Non si sceglie per preferenza personale.

**ADR-04.** SQLite è una proposta perché offre transazioni e query, ma introduce un formato e una migrazione. JSON è semplice ma rischia corruzione e concorrenza. Un ibrido può essere peggiore di entrambi se crea due autorità. La decisione deve includere recovery, backup, crash test e schema sconosciuto in sola lettura.

**ADR-05.** Il DAG immutabile non è un dettaglio implementativo. È il modo per garantire invalidazione selettiva, cache corretta e provenienza. Se il DAG è mutabile senza hash, un risultato stale può essere pubblicato. La generazione impedisce la corsa tra job vecchi e nuovi. L'undo è una revisione, non una cache. Un grafo aciclico non è una prova di completezza: la matrice delle configurazioni P0 è un controllo separato.

**ADR-06.** Il limite hard del pool gestito non è il limite RSS. Il prodotto deve mostrare la differenza e non promettere ciò che driver e allocator esterni possono violare. Il preflight e il rifiuto chiaro sono preferibili a un OOM a metà. Il viewer ha un budget separato. La prenotazione dell'intero batch evita di scoprire a metà che i candidati non entrano.

**ADR-07.** La GPU non è necessaria per nessun P0. Introdurla troppo presto crea lock-in e debito. Introdurla troppo tardi può limitare la competitività. Il gate M7 confronta backend e operatori con misure, incluse copie, VRAM e sincronizzazione. Layout e lifetime dei buffer si decidono ora per non precludere il futuro.

**ADR-09.** La deconvoluzione P0 non è una promessa di recupero. La formulazione è una sola, con ipotesi dichiarate. Il residuo dopo riconvoluzione non è una prova della PSF. La sensibilità a PSF, fondo, `lambda` e rumore è parte del gate.

**ADR-10.** Il colore non è preservato per magia. Offset, bilanciamento, nonlinearità per canale, gamut di display ed export cambiano i rapporti in modi diversi. Il contratto dice quale stadio è stato cotto e quale riferimento è stato usato. Un WCS valido è una condizione di coordinate, non un certificato di colore.

**ADR-12.** AppImage, Flatpak e nativo hanno costi diversi. Un pacchetto nativo su una famiglia non copre le altre. Flatpak e AppImage possono ridurre il carico di dipendenze ma aggiungono sandbox e integrazione. La scelta autonoma richiede prove. I catalog pack seguono la preferenza inclusa D-21 e i gate di 17.4, ancora da verificare.

**ADR-15.** L'Advisor è P0 vincolante nella forma deterministica e offline. Non è una scusa per introdurre AI obbligatoria. Le tre viste e i tre livelli non creano pipeline separate. L'assistenza eseguibile si ferma al registro validato; fuori da lì c'è avviso o astensione. Default, invasività e costo computazionale si decidono autonomamente dopo prove UX e misure, senza riaprire l'inclusione P0.

**ADR-16.** La calibrazione flessibile non è una promessa di calibrazione impossibile. La matrice dichiara cosa è valido, condizionato o non possibile. La presenza di un master non basta. Il requisito è vincolante; matrice e politica sono la proposta tecnica. Default automatico e livello di avviso si decidono autonomamente dopo prove, senza sostituzioni successive al guasto.

**ADR-17.** La covarianza piena non è richiesta. La proposta è una rappresentazione pratica con limiti dichiarati. Per una misura che dichiara incertezza, il modello deve coprire i contributi dominanti; per un'immagine fotografica, una approssimazione conservativa può bastare. La decisione riguarda quali dichiarazioni di misura sono ammesse con quale covarianza.

---

---

## 21. Roadmap, milestone e gate

### 21.1 Sequenza

| Milestone | Scopo | Gate di uscita | Dipendenze | Fase originale |
|---|---|---|---|---|
| **M0** | Documento consolidato, review, decisioni aperte, piano di verifica Linux, licenze, dataset e spike | Verifica autonoma della conformità al mandato, review registrata e piani riproducibili. Nessun codice di prodotto. Minimi e SBOM provvisori non si dichiarano misurati. | Questa revisione e mandato vigente | Fase 0 |
| **M1** | Spike già autorizzati dal mandato, isolati in `tmp/`, con domanda, metrica e uscita dichiarate | Risultati riproducibili, codice usa e getta, nessuna integrazione nel prodotto, decisione tecnica autonoma registrata dopo le prove. | Gate M0 superato con evidenze | Fase 0 e preparazione Fase 1 |
| **M2** | Fondamenta, contratti minimi e prima slice scientifica: immagine tipizzata, FITS, hash, DAG, persistenza, scheduler, calibrazione `L - D`, flat `Q`, maschere, varianza, report | Slice FITS e sintetica con calibrazione vera, maschere, incertezza e report; DAG durevole; recovery, cancel e worker prima di carichi grandi; test degli invarianti; contratti minimi ADR-03, ADR-04, ADR-05, ADR-06, ADR-16 e ADR-17 applicati prima delle slice scientifiche. | M1 | Fase 1 |
| **M3** | Deep sky headless in 19 slice ortogonali, dal RAW all'export, con Advisor e planner | Dataset pubblico e sintetico elaborati end-to-end; otto combinazioni; metriche di calibrazione, registrazione, stacking, varianza e memoria; configurazioni P0 verificate; nessun software esterno nel percorso dichiarato. | M2 | Fase 2 |
| **M4** | GUI completa in 7 slice: viewer, inspector, monitor, tre modalità, Advisor, editing maschere, accessibilità | Percorso completo con GUI; tre modalità; Advisor con tre viste e tre livelli; accessibilità P0; nessuna regressione headless. | M3 | Fase 3 |
| **M5** | Release Linux: matrice minima, packaging, offline, CPU-only, documentazione | Almeno una modalità di packaging verificata sulla matrice minima; primo avvio offline; X11 nativo e Wayland; nessuna regressione P0; SBOM aggiornato. | M4 | Fase 3 e release |
| **M6** | Deep sky avanzato: plate solving, SPCC, LRGB, narrowband, maschere avanzate, denoise, deconvoluzione, Drizzle, multi-sessione | Ogni funzione ha contratto, dataset, benchmark e regressione; la qualità misurata non peggiora i P0. Gate separati per sotto-milestone. | M5 | Fase 4 e parte Fase 5 |
| **M7** | GPU facoltativa e automazione: backend, batch, preset | Benchmark GPU contro CPU con error budget; CPU resta completa; nessun P0 dipende da GPU; batch riproducibile. Lo spike GPU è in M1, non dopo M2. | M5 | Fase 5 |
| **M8** | AI opzionale: modelli, provenance, artefatti, separazione dalla misura | Modelli licenziati, opt-in, test di artefatti, separazione dalla misura; il percorso non AI resta primario e completo, senza sostituzione automatica. | M6 | Fase 6 |
| **M9** | Altre modalità: mosaici, planetario, Luna, Sole, comete e asteroidi, scienza | Ogni modalità ha dataset, UX, test e gate propri. Nessun riuso acritico del percorso deep sky. | M6 | Fase 7 |
| **M10** | Futuro Windows e macOS | Solo con autorizzazione esplicita. Core portabile verificato, installer e firma solo allora. | M5 e autorizzazione | Fase 8 |

M5 è bloccata finché tutti i P0 non sono completati e testati, inclusi i 14 P0 ereditati, post-processing fotografico e Advisor. D-19 e D-20 approvano deconvoluzione, colore relativo e solver assistito: tutti sono obbligatori. Si correggono gli errori e si ripetono le prove, senza tagliare perimetro o criteri. Dopo ogni gate superato si aggiorna lo stato e si prosegue autonomamente fino a M9 secondo le dipendenze; M10 resta esclusa.

### 21.2 M0, consolidamento e prove

M0 non è un lavoro di codice. Produce:

- questo piano revisionato e il verbale della review indipendente sul testo effettivo, ancora PENDING per la 2.2.1;
- la mappa delle decisioni tecniche aperte e di quelle riservate, con D-19/D-20 approvate P0 e D-21 approvata come preferenza;
- il piano di verifica della matrice Linux minima;
- il piano di verifica delle licenze e dell'SBOM;
- il piano di selezione dei dataset e del corpus di accettazione;
- il pacchetto degli spike M1 già autorizzati, con domanda, metrica, criterio di uscita e permessi effettivamente necessari;
- la dichiarazione di ciò che è provvisorio.

Il gate M0 verifica il recepimento del perimetro già approvato e la completezza del piano di verifica. Non certifica un minimo misurato, un SBOM completo o un dataset non ancora scelto. Il coordinatore registra le prove, corregge i rilievi e prosegue senza chiedere conferme rituali; nessuna decisione tecnica anticipa le evidenze richieste.

### 21.3 M1, spike pianificati

Gli spike sono pianificati, non eseguiti da questa revisione. Sono autorizzati dal mandato M0..M9, usano `tmp/` locale e producono codice usa e getta, non integrato. Prima di adottare una dipendenza si verificano licenze e permessi reali. Le scelte tecniche si chiudono autonomamente dopo le misure.

Ogni spike pertinente misura anche costo del preflight e dell'osservabilità: tempo, memoria e I/O per backend, decoder e modelli, separando avvio freddo e caldo. Confrontare percorso preregistrato ed effettivamente eseguito tramite tracce verificabili. Iniettare guasti prima e durante l'esecuzione, verificare arresto, output non pubblicati e assenza di invocazioni alternative: il solo messaggio di errore non prova NC-21. Librerie con fallback nascosti non disattivabili sono inammissibili, incluse dipendenze transitive. Queste misure ampliano gli spike esistenti, non aggiungono sotto-milestone.

| Spike | Domanda | Metrica | Uscita |
|---|---|---|---|
| M1.a Core e kernel | C++20, Rust e ibrido Python sono adatti a pixel-wise, convoluzione, warp e FFT? | Tempo, memoria, qualità numerica, costo di sviluppo, sicurezza | Raccomandazione motivata, non vincolante |
| M1.b Viewer e GUI | Qt Widgets, QML e GTK4 gestiscono immagini grandi, HiDPI, X11 e Wayland? | Latenza pan e zoom, memoria bounded, accessibilità, colore, IME | Toolkit candidato e rischi |
| M1.c Persistenza e crash | SQLite e JSON sopravvivono a crash, migrazioni e concorrenza? | Integrità, recovery, tempo di commit, complessità | Modello di persistenza preferito |
| M1.d Memoria e scheduler | Pool gestito, prenotazioni e spill governano dataset oltre RAM? | Picco RSS, spill, throughput, deadlock, rifiuto | Politica di memoria e limiti |
| M1.e GPU | Quale backend riduce il tempo totale, non solo il kernel? | Copie, VRAM, sincronizzazione, cold e warm start, error budget | Backend candidato o rinvio |
| M1.f Hash e provenance | Il digest degli input è un collo di bottiglia? | Tempo di hash, streaming, cache, costo su dataset grandi | Strategia di hash e misura |

Lo spike GPU è M1.e e si esegue all'inizio di M1, non dopo M2. Serve a decidere se la GPU entra in M7 con un beneficio misurato, non a sostituire la CPU.

### 21.4 M2, fondamenta e contratti minimi

M2 non è una costruzione astratta. Deve produrre una piccola pipeline scientifica reale e i contratti minimi che le slice successive useranno.

Anche qui le lettere identificano deliverable, non una sequenza temporale. Un ordine topologico del registro è M2.g, M2.f, M2.a, M2.b, M2.c, M2.d, M2.e: contratti e infrastruttura delle fixture precedono l'implementazione che verificano. Le evidenze di superamento dei test si producono dopo l'operatore; `verified_by` non impone un test già superato prima del codice.

| Sotto-milestone | Contenuto | Uscita verificabile |
|---|---|---|
| M2.a Immagine e FITS | Immagine tipizzata, FITS mono e CFA, metadati, maschere, unità | Import e roundtrip FITS con metadati conservati |
| M2.b Hash e DAG | Content hash, nodo, dipendenze, invalidazione, token di generazione | Test di invalidazione selettiva e risultato stale |
| M2.c Persistenza e recovery | SQLite proposto, scrittura atomica, commit, orfani, migrazione, schema sconosciuto | Crash test e riapertura |
| M2.d Scheduler base | Budget Auto e Manuale, prenotazioni, worker, cancel, checkpoint | Limite del pool rispettato, job impossibile rifiutato, fallimenti iniettati gestiti senza pubblicare output parziali; nessuna garanzia hard sull'RSS |
| M2.e Calibrazione vera | `L - D` con dark matched, `Q = Fc/N(Fc)`, maschere, varianza, report | Sintetico con ground truth e report leggibile |
| M2.f Fixture e CI | FITS sintetici, dark e flat, maschere, test unitari e property-based | Suite eseguibile e invarianti |
| M2.g Contratti minimi | ADR-03 modello pixel e maschere, ADR-04 persistenza, ADR-05 provenienza e lineage, ADR-06 budget dell'intero batch, ADR-16 stati dei master, ADR-17 rappresentazione della covarianza | Contratti scritti e testati nelle parti che M2 usa; nessuna slice scientifica dipende da un contratto non deciso |

Il gate M2 richiede la slice FITS e sintetica con calibrazione `L - D`, flat `Q`, maschere, metadati, incertezza e report. Richiede inoltre DAG durevole, recovery, cancel e worker prima di un carico grande. Una fondazione che non elabora un dato astronomico reale non supera il gate.

### 21.5 M3, deep sky headless in 19 slice

Le lettere delle slice sono identificativi, non un ordine temporale. Un ordine topologico dei deliverable M3 è: a, b, c, d, e, f, g, h, k, j, i, l, m, n, o, p, q, r, s. Gli archi espliciti dell'appendice D sono l'autorità: i e l non precedono j/k solo perché la loro lettera lo suggerisce. M3.p è sempre il solver; M3.q è sempre il colore. Le decisioni `.entry` precedono la costruzione, non aggiungono sotto-milestone alle 54 dichiarate.

| Slice | Contenuto | Requisiti coperti | Uscita verificabile |
|---|---|---|---|
| **M3.a** | RAW e FITS, decoder contract, prestacked | F-IO-01, F-IO-02, F-IO-03, F-IO-04, F-IO-05, F-IO-08, F-IO-10 | Campioni pubblici e sintetici importati senza debayer prematuro; offset del decoder dichiarato |
| **M3.b** | Master bias, dark, flat, dark-flat, stati tipizzati | F-CAL-01.b | Master riproducibili con report di frame usati e scartati |
| **M3.c** | Calibrazione flessibile, otto combinazioni, omissioni pianificate | F-CAL-02.b, F-CAL-03.b, F-CAL-04, F-CAL-06.b | T-CAL-08; nessuna doppia sottrazione; varianza propagata; STOP se un prerequisito della ricetta selezionata viene meno |
| **M3.d** | Debayer Bayer dopo calibrazione CFA | F-CAL-08 | Test di aliasing, colore e pattern; rifiuto del pattern ignoto |
| **M3.e** | Stelle, centroidi, FWHM/HFD, eccentricità, SNR, scoring | F-REG-01 | Ground truth sintetico e frame reali pubblici |
| **M3.f** | Registrazione, trasformazione, residui, ambiguità | F-REG-02 | T-REG-02; soluzione ambigua non pubblicata |
| **M3.g** | Resampling, varianza, coverage, WCS, flusso e brillanza | F-REG-03 | Trasformazioni note, covarianza tra output, conservazione dichiarata |
| **M3.h** | Stacking pesato, N basso, rejection condizionata, `Neff` | F-STK-01, F-STK-02, F-STK-03 | SNR, bias, coverage, casi N = 1, 2, 3 |
| **M3.i** | Background additivo e divisivo, astensione | F-POST-03.a | Sintetici con gradiente e nebulosa; astensione dichiarata |
| **M3.j** | Motore maschere, persistenza, policy di riordino | F-POST-08.a.1 | Edit serializzati riproducibili, asset congelato, generatore derivato, riapertura; nessuna UI richiesta |
| **M3.k** | Curve, bilanciamento, asinh e casi limite | F-POST-02.a, F-POST-04.a, F-POST-06.a.1 | Monotonicità, caso analitico `k = 0`, negativi, gamut, rapporti globali nel dominio dichiarato |
| **M3.l** | Denoise à trous, modelli lineare e non lineare, rollback | F-POST-09.a.1 | T-POST-09; bias, NPS, danno su struttura debole e stelle |
| **M3.m** | Sharpening multiscala bounded, contrasto locale | F-POST-11.a.1 | Aloni, ringing, clipping, rumore amplificato, confronto |
| **M3.n** | Riduzione stelle e separazione limitata, gate separati | F-POST-12.a.1, F-POST-12.b.1 | T-POST-12; accuratezza modello e danno non stellare separati dall'identità di reintegrazione `R_T + S_T = T(I_L)`; import solo NL senza antenato lineare rifiutato nel preflight C-14, senza inversione inventata o sostituto |
| **M3.o** | Deconvoluzione P0 a PSF fissa, APPROVATA D-19 | F-POST-10.a.1 | T-POST-10; sensibilità a PSF, fondo, `lambda`, rumore; baseline no-op |
| **M3.p** | Solver assistito, hint, validazione, pack mancante | F-REG-04.a | T-AST-ASSIST01; falsa soluzione rifiutata |
| **M3.q** | Colore relativo broadband, pack locale, residui e limiti | F-POST-05.a.1 | Residui di matching e fotometrici separati, copertura, limiti sistematici |
| **M3.r** | Advisor headless, schede, confronto, planner, corpus di accettazione | F-ANA-01.a, F-ANA-02.a, F-ANA-03.a, F-ANA-04.a, F-ANA-05.a | T-ANA-01, T-ANA-04, T-VAL-INDEP; cinque stati di esito; budget dell'intero batch |
| **M3.s** | Export FITS e TIFF, provenance, manifest, E2E e harness | F-PRJ-07, NFR-EXP-01.b, NFR-DATA-01.b, NFR-TEST-01.b | Export riaperto con metadati; manifest valido; percorso headless completo |

Il gate M3 richiede che il percorso headless elabori un dataset pubblico e uno sintetico, con metriche di calibrazione, registrazione, stacking, varianza e memoria. Le metriche degli operatori non lineari — stretch, denoise, sharpening, riduzione e separazione stellare, deconvoluzione — sono calcolate headless in M3, sui risultati dell'operatore e non su una preview. Richiede inoltre che la matrice di attivazione della sezione 11.11 sia verificata sulle configurazioni dichiarate: otto combinazioni di calibrazione, pack presente e mancante, WCS importato e solver assistito, rumore lineare e non lineare, deconvoluzione selezionata e non selezionata. Le sei F-ANA devono avere almeno le primitive e i test di astensione. Nessun software esterno è richiesto nel percorso dichiarato.

### 21.6 M4, GUI completa in 7 slice

| Sotto-milestone | Contenuto | Uscita verificabile |
|---|---|---|
| M4.a Workspace e viewer | Import, browser, viewer bounded, tile, istogramma, clipping | Immagine grande senza caricare tutto in RAM |
| M4.b Pipeline e progetto | Inspector, DAG, undo, redo, recovery, cache | Modifica a monte e invalidazione visibile |
| M4.c Tre modalità | Automatica, Guidata, Esperta sullo stesso motore | Stesso risultato a parità di parametri |
| M4.d Advisor UI | Tre viste, tre livelli, schede, confronto, conferma, rollback | Utente che accetta o rifiuta senza rompere il progetto |
| M4.e Editor maschere | Brush, poligono, feather, combine, policy, revisione | Maschera riaperta con la stessa semantica |
| M4.f Post-processing UI | Background, colore, asinh, denoise, sharpening, stelle, deconvolution | Preview distinta dal dato; rollback |
| M4.g Accessibilità, risorse ed export | Tastiera, focus, temi, HiDPI, monitor risorse, export con provenance | Checklist di accessibilità e export verificato |

Il gate M4 richiede il percorso completo con GUI, le tre modalità equivalenti, l'Advisor con tre viste e tre livelli, l'accessibilità P0 e nessuna regressione headless. M4 è limitata a visualizzazione e interazione: non introduce operatori, metriche o calcoli scientifici nuovi, che restano in M3 headless.

Ordine topologico M4: a, b, e, f, d, c, g. M4.e realizza F-POST-08.a.2; M4.f realizza F-POST-02.b, F-POST-03.b, F-POST-04.b e le foglie UI `.a.2` dei nuovi operatori, più F-POST-12.b.2. Colore, solver assistito e deconvoluzione entrano secondo le decisioni D-19/D-20. M4.d realizza F-ANA-01.b fino a F-ANA-05.b e confronto/ROI; M4.c integra F-ANA-06. Nessun aggregato core+UI viene usato come prerequisito M3.

### 21.7 M5, release Linux

M5 verifica matrice Linux, packaging, offline, CPU-only e documentazione. Non introduce funzioni nuove. Un fallimento di gate blocca la release.

### 21.8 M6, deep sky avanzato in 9 sotto-milestone

| Sotto-milestone | Contenuto | Gate |
|---|---|---|
| **M6.a** Plate solving e WCS | Identificazione del campo, distorsione, cataloghi locali, rifit | Residui e confidenza su dataset noti |
| **M6.b** SPCC e calibrazione colore | Cataloghi, curve spettrali, indicatori di affidabilità. Distinta dalla calibrazione broadband relativa di M3.q | Residui e limiti; nessuna calibrazione assoluta senza catalogo e risposta |
| **M6.c** LRGB e narrowband | Composizioni, PixelMath, provenienza dei canali | Colore e artefatti, canali tracciati |
| **M6.d** Maschere e processing locale | Maschere stella, fondo, oggetto, luminanza e colore; editing reversibile | Transizioni, conservazione delle maschere e riproducibilità; prerequisito dei metodi che le richiedono |
| **M6.e** Drizzle | Pixfrac, coverage, campionamento, memoria | PSF e campionamento noti, nessun dettaglio inventato |
| **M6.f** Multi-sessione | Sessioni, filtri, normalizzazione, merge | Report per sessione, nessun mismatch nascosto |
| **M6.g** Denoise classico avanzato | Spaziale e multiscala, domini lineare e non lineare, protezione del dettaglio, oltre il P0 di M3.l | Riduzione del rumore, bias, perdita di struttura debole e distorsione stellare misurati separatamente; rollback |
| **M6.h** Deconvoluzione avanzata non AI | PSF, Wiener a filtro fissato, RL e varianti nei domini ammessi, oltre il P0 di M3.o | Contratto della verosimiglianza, residui, ringing, saturazione, arresto e stabilità; nessun clamp silenzioso |
| **M6.i** Sharpening e rifinitura | Contrasto locale, HDR/multiscala, aloni e controllo cromatico | Artefatti e clipping dichiarati, confronto prima/dopo e reversibilità; non eredita il gate del denoise |

Ogni sotto-milestone ha un gate indipendente. Una funzione avanzata non eredita la validità di un'altra.

Le lettere M6 non sono cronologiche: un ordine valido è a, f, b, c, d, e, g, h, i. M6.f precede M6.c per stack per filtro e multi-sessione. M6.a include F-IO-06; M6.f include F-IO-07, F-CAL-05, F-CAL-07 e F-STK-04/05/07. M6.i comprende GHS, riduzione/separazione non AI avanzata, layer, rifinitura cromatica, anteprime avanzate, localizzazione e slice risorse `.b`. Queste assegnazioni non promuovono P1 a P0 e non rinviano capacità P0. Gli archi puntuali sono in appendice D.

### 21.9 M7, M8 e M10

- **M7:** GPU facoltativa e automazione. Benchmark contro CPU, error budget, nessuna sostituzione automatica. Batch riproducibile. Nessun P0 dipende dalla GPU.
- **M8:** AI opzionale. Modelli licenziati, opt-in, provenance, test di artefatti, separazione dalla misura. Il percorso non AI resta primario e indipendente.
- **M10:** Windows e macOS solo con autorizzazione esplicita. Nessun lavoro prima.

### 21.10 M9, altre modalità in 6 sotto-milestone

| Sotto-milestone | Contenuto | Perché è distinta |
|---|---|---|
| **M9.a** Mosaici | Registrazione tra campi, normalizzazione locale, seam blending, distorsione | Richiede geometria e photometric matching tra pannelli |
| **M9.b** Planetario | Video, scoring frame, lucky imaging, allineamento locale, sharpening | Il segnale e il rumore sono diversi dal deep sky |
| **M9.c** Luna | Allineamento, terminatore, sharpening, mosaico lunare | Superficie estesa e dettaglio ad alta frequenza |
| **M9.d** Sole | Filtri, dinamica estrema, allineamento, feature solari | Sicurezza, saturazione e dinamica proprie |
| **M9.e** Comete e asteroidi | Doppio allineamento, moto del soggetto, stacking separato | Il soggetto si muove rispetto alle stelle |
| **M9.f** Misure scientifiche | Fotometria, astrometria, incertezza, unità, report | La misura non è editing estetico |

Condividere il core non significa condividere gli algoritmi in modo scorretto. Ogni modalità ha proprietà e test propri.

### 21.11 Complessità relativa, rischio e incertezza

La tabella è qualitativa. Non contiene date, settimane o stime di calendario. Il totale è di 54 sotto-milestone: 6 in M1, 7 in M2, 19 in M3, 7 in M4, 9 in M6 e 6 in M9.

| Sotto-milestone | Complessità | Rischio tecnico | Incertezza | Dipendenza principale |
|---|---|---|---|---|
| M1.a Core e kernel | Alta | Alto | Alta | Linguaggio e spike |
| M1.b Viewer | Alta | Medio-alto | Alta | Toolkit |
| M1.c Persistenza | Media | Alto | Media | Modello dati |
| M1.d Memoria | Alta | Alto | Alta | Scheduler |
| M1.e GPU | Alta | Alto | Molto alta | Backend e driver |
| M1.f Hash e provenance | Media | Medio | Bassa | Strategia di hash |
| M2.a Immagine e FITS | Media | Medio | Bassa | ADR-03, ADR-08 |
| M2.b Hash e DAG | Alta | Alto | Media | ADR-05 |
| M2.c Recovery | Media | Alto | Media | ADR-04 |
| M2.d Scheduler | Alta | Alto | Alta | ADR-06 |
| M2.e Calibrazione | Media | Medio | Media | ADR-09 |
| M2.f Fixture e CI | Media | Medio | Bassa | NFR-TEST-01 |
| M2.g Contratti minimi | Media | Alto | Media | ADR-03, ADR-04, ADR-05, ADR-06, ADR-16, ADR-17 |
| M3.a RAW e FITS | Alta | Alto | Alta | Decoder |
| M3.b Master tipizzati | Alta | Medio | Media | ADR-16 |
| M3.c Otto combinazioni | Alta | Alto | Media | ADR-16 |
| M3.d Debayer | Media | Medio | Media | Pattern |
| M3.e Stelle | Media | Medio | Media | PSF |
| M3.f Registrazione | Alta | Alto | Alta | Match e WCS |
| M3.g Resampling | Alta | Alto | Alta | Covarianza |
| M3.h Stacking | Alta | Alto | Media | Pesi e N basso |
| M3.i Background | Media | Medio | Alta | Nebulosità |
| M3.j Maschere | Media-alta | Alto | Media | Modello maschere |
| M3.k Stretch e asinh | Media | Medio | Media | Colore e reference |
| M3.l Denoise P0 | Alta | Alto | Alta | Rumore per scala |
| M3.m Sharpening P0 | Media-alta | Alto | Media | Aloni e clipping |
| M3.n Stelle | Alta | Alto | Alta | PSF e antenato lineare |
| M3.o Deconvoluzione P0 | Alta | Alto | Alta | PSF, dominio e artefatti |
| M3.p Solver assistito | Alta | Medio | Media | Cataloghi e hint |
| M3.q Colore relativo | Alta | Alto | Alta | WCS e cataloghi |
| M3.r Advisor headless | Alta | Alto | Molto alta | Metriche e astensione |
| M3.s Export e E2E | Media | Medio | Media | Manifest e provenance |
| M4.a Workspace e viewer | Alta | Medio | Media | Toolkit |
| M4.b Pipeline e progetto | Media | Medio | Media | DAG e persistenza |
| M4.c Tre modalità | Media | Medio | Bassa | Stesso motore |
| M4.d Advisor UI | Media | Medio | Alta | F-ANA |
| M4.e Editor maschere | Media-alta | Medio | Media | Modello maschere |
| M4.f Post-processing UI | Media | Medio | Media | Operatori M3 |
| M4.g Accessibilità, risorse ed export | Media | Medio | Media | Checklist e packaging |
| M6.a Plate solving | Alta | Medio | Media | Cataloghi |
| M6.b SPCC | Alta | Alto | Alta | Cataloghi e colore |
| M6.c LRGB e narrowband | Media | Medio | Media | Colore |
| M6.d Maschere | Media-alta | Alto | Media | Modello maschere ed editing locale |
| M6.e Drizzle | Alta | Alto | Media | Campionamento |
| M6.f Multi-sessione | Alta | Medio | Media | Normalizzazione |
| M6.g Denoise avanzato | Alta | Alto | Alta | M6.d quando richiesto, segnale debole e rumore |
| M6.h Deconvoluzione avanzata | Alta | Alto | Alta | PSF, dominio e artefatti |
| M6.i Sharpening e rifinitura | Media-alta | Alto | Media | Clipping, aloni e colore |
| M9.a Mosaici | Molto alta | Alto | Alta | Geometria e blending |
| M9.b Planetario | Alta | Medio | Media | Video |
| M9.c Luna | Media | Medio | Media | Allineamento |
| M9.d Sole | Alta | Alto | Alta | Dinamica |
| M9.e Comete | Alta | Alto | Alta | Doppio allineamento |
| M9.f Scienza | Alta | Alto | Alta | Unità e incertezza |

### 21.12 Regole di dipendenza

1. Una milestone non si chiude se un gate P0 è aperto.
2. Una funzione avanzata non si costruisce su un contratto non validato.
3. Una dipendenza condizionale non attiva è un errore di configurazione, non un'opzione.
4. Un ADR richiesto da una slice va approvato prima del codice della slice.
5. Una slice parziale non è un candidato Pareto e non si presenta come risultato.
6. M4 non sostituisce M3: consuma un core già testato e non ricalcola la scienza nella GUI.

### 21.13 Stop-and-fix e regressione

Un difetto che rompe un invariante blocca il gate della slice, non il lavoro dello sviluppatore. Indagare, correggere il codice, rieseguire test e review e registrare gli esiti. Non eliminare fixture, disabilitare funzioni, degradare algoritmi o allentare criteri per ottenere un PASS. NC-21 impone STOP al ramo del prodotto senza sostituzioni; non vieta bugfix e nuove prove. Se emerge un'impossibilità scientifica, legale o di permesso, dichiarare l'assunzione fallita e continuare sulle attività indipendenti. Alla chiusura verificata del gate passare alla prossima attività senza attendere il proprietario.

---

## 22. Rischi e mitigazioni

| Rischio | Probabilità | Impatto | Mitigazione | Segnale precoce |
|---|---|---|---|---|
| Complessità dell'MVP | Alta | Alto | Slice ortogonali, gate espliciti, nessun taglio silenzioso | Slice che non chiude il proprio criterio |
| Contratti scientifici non validati | Alta | Alto | Ground truth, test, astensione, nessuna soglia inventata | Un operatore senza fixture |
| Overfitting della strategia | Media | Alto | Controllo di regressione e validazione indipendente distinti; corpus separato | Metriche ottime solo sulle ROI di taratura |
| Astensione sistematica | Media | Alto | Corpus di accettazione con copertura utile e danno/beneficio | Nessuna raccomandazione nei casi identificabili |
| Budget del batch sottostimato | Media | Medio | Prenotazione dell'intero batch, preflight e rifiuto del batch che non entra; nessun taglio automatico di risoluzione, candidati o qualità | Job fermi a metà con scratch pieno |
| Semantica delle maschere dopo riordino | Media | Alto | Policy esplicita per asset e generatore; test di riapertura e crop | Maschera spostata o selezione cambiata in silenzio |
| Colore sovradichiarato | Alta | Medio | Contratto per stadio, componenti reference, limiti sistematici | "Colore preservato" senza stadio né riferimento |
| Deconvoluzione sovradichiarata | Alta | Medio | Una formulazione, sensibilità, baseline no-op, astensione su PSF variabile | Residuo piccolo presentato come verità della PSF |
| Prestazioni GPU deludenti | Media | Medio | GPU facoltativa, misura end-to-end, percorso CPU primario dichiarato nel preflight; nessuna sostituzione automatica | Speedup solo sul kernel |
| Packaging Linux fragile | Media | Medio | Matrice minima, prove M5, nessuna promessa universale | Installazione pulita che fallisce |
| Licenze dei catalog pack | Media | Alto | Verifica autonoma per pack e SBOM secondo D-21; blocco e proposta motivata se inclusione non lecita o sostenibile | Pack senza diritti chiari |
| Corruzione del progetto | Bassa | Alto | Scrittura atomica, recovery, migrazioni con backup | Progetto non apribile dopo crash |
| Memoria oltre il pool | Media | Alto | Prenotazioni, spill, preflight, RSS misurato | Picco RSS non spiegato |
| Dipendenza da un formato o una marca | Media | Medio | Campioni multivendor, decoder contract, nessuna marca obbligatoria | Un solo RAW supportato davvero |

---

## 23. Decision log prioritizzato

### 23.1 Decisioni D-01 a D-21

Le decisioni tecniche aperte non hanno un default implicito: il coordinatore le chiude autonomamente dopo evidenze e review secondo 0.8. D-01..D-18 conservano ID e oggetto; D-18 resta risolta. D-19 e D-20 sono APPROVATE P0. D-21 è APPROVATA come preferenza per pack inclusi, non come verifica legale o tecnica già riuscita.

| ID | Decisione | Opzioni | Proposta o raccomandazione | Stato | Chi decide | Gate |
|---|---|---|---|---|---|---|
| **D-01** | Nome, logo e identità | Nome provvisorio, nome definitivo, identità visiva | Non bloccano la scienza; usare identità provvisoria e verificare prima della release | APERTA | Coordinatore; distribuzione commerciale riservata | M5 |
| **D-02** | Licenza delle dipendenze e politica per il prodotto commerciale | Solo open source, doppia licenza, componente proprietario | Preservare opzioni open source e commerciali; verificare licenze prima dell'adozione. pocketfft è candidata, non sostituzione runtime di FFTW | APERTA: audit autonomo; licenza irreversibile e distribuzione commerciale riservate | Coordinatore per audit e dipendenze; proprietario per le sole scelte riservate | M0, prima di adottare librerie |
| **D-03** | Linguaggio e architettura del core | C++20 o 23, Rust, ibrido Python | C++20 provvisorio; Rust e ibrido da misurare in M1 | APERTA tecnica | Coordinatore dopo spike e review | M1 |
| **D-04** | Toolkit GUI e comunicazione con il core | Qt Widgets, QML, GTK4, egui, Slint, Tauri, Avalonia | Qt Widgets provvisorio; confronto in M1 su viewer, accessibilità, colore e packaging | APERTA tecnica | Coordinatore dopo spike e review | M1 |
| **D-05** | Persistenza: SQLite, JSON o ibrido | SQLite, JSON, ibrido | SQLite come sola autorità, backup e schema sconosciuto in sola lettura | APERTA tecnica | Coordinatore dopo prove ADR-04 | M2 |
| **D-06** | Packaging: AppImage, Flatpak, nativo o misto | AppImage, Flatpak, pacchetti nativi, misto | Confronto con prove; la scelta non è promessa in anticipo | APERTA tecnica | Coordinatore dopo prove | M5 |
| **D-07** | Matrice Linux minima e livelli di supporto | Tre configurazioni L1..L3, più famiglie; solo L1 non basta alla matrice minima | Tre configurazioni L1, L2, L3; il resto compatibile non certificato | APERTA tecnica | Coordinatore dopo evidenze; nessuna rimozione per aggirare un gate | M0 piano, verifica M5 |
| **D-08** | Dataset pubblici, licenze e hash | Dataset pubblici, sintetici, mix | Selezionare prima di M3, con licenza, provenienza e hash | APERTA tecnica | Coordinatore e QA dopo verifica licenze | M3 |
| **D-09** | Soglie numeriche per algoritmo | Soglia unica, soglia per algoritmo, nessuna soglia | Definire per algoritmo, con ground truth e dataset, prima dell'implementazione | APERTA tecnica | Coordinatore dopo prove e review numerica indipendente | Prima di ogni funzione |
| **D-10** | Backend GPU e priorità degli operatori | CPU-only, Vulkan, OpenCL, CUDA, HIP o ROCm | Benchmark M1 e M7 con copie, VRAM, sincronizzazione ed error budget; GPU facoltativa | APERTA tecnica | Coordinatore e architettura dopo misure | M1 e M7 |
| **D-11** | Modelli AI, licenze, pesi e provenienza | Nessun modello, modelli locali, servizio opzionale | AI facoltativa, opt-in, con licenza e provenance verificate; il percorso non AI resta primario | APERTA tecnica | Coordinatore dopo audit e QA; costi nuovi riservati | M8 |
| **D-12** | Momento e forma dell'API plugin | Nessun plugin, API locale, sandbox, processo separato | Rinviare a P2 e M9; se introdotta, API versionata e sandbox | APERTA tecnica | Coordinatore dopo evidenze e review sicurezza | M9 |
| **D-13** | Ambito scientifico P2: fotometria, astrometria, spettroscopia | Incluso in M9 o ambito separato secondo contratto; nessuna eliminazione silenziosa | Dipende da M9 e da dataset; non si promette una misura prima dei test | APERTA tecnica | Coordinatore e review scientifica nel perimetro autorizzato | M9 |
| **D-14** | Livello di traduzione iniziale italiano e inglese | Solo italiano, italiano e inglese, più lingue | Italiano e inglese; nessun blocco del motore | APERTA tecnica | Coordinatore dopo verifica UX | M4 o M6 |
| **D-15** | Default dell'Advisor e livello di invasività | Solo suggerimenti, anteprima, ottimizzazione con conferma | Il perimetro P0 è vincolante; il default resta da scegliere | APERTA tecnica | Coordinatore dopo prove UX e costo | M4 |
| **D-16** | Default della calibrazione incompleta | Matrice di ricette compatibili e omissioni pianificate | Prima dell'esecuzione scegliere le correzioni compatibili e registrare le omissioni. Dopo la scelta, un prerequisito perso arresta il ramo senza sostituzione. | APERTA sul default, vincolata da NC-21 | Coordinatore dopo prove ADR-16 | M3.entry |
| **D-17** | Covarianza ammessa per dichiarare una misura | Nessuna, diagonale, diagonale più NPS locale, densa | Diagonale più NPS locale e correlazioni dei master; incertezza della geometria quando modellata | APERTA tecnica | Coordinatore dopo prove e review numerica | M3 e M6 |
| **D-18** | Promozione di denoise, maschere o GHS nell'MVP | Decisione del mandato corrente | P0 denoise classico, maschere utilizzabili, asinh, sharpening/contrasto e riduzione stellare; GHS P1. I metodi e i gate restano da validare, il perimetro non richiede nuova approvazione. | RISOLTA dal mandato | Proprietario | M0 |
| **D-19** | Deconvoluzione nell'MVP | P0 vincolante; P1 non è un'alternativa al gate fallito | PSF fissa, `b` fisso, `W` che approssima `C_V^{-1}` sul sottospazio valido, sensibilità e baseline no-op. Gate fallito: funzione e M5 bloccate, correzione e nuove prove senza declassamento | APPROVATA P0 dal mandato | Proprietario ha deciso; coordinatore esegue e verifica | M3 gate futuro, M5 release |
| **D-20** | Colore e solver nell'MVP | Colore relativo e solver assistito entrambi P0 vincolanti | Pack locale; WCS importato validato è un percorso primario scelto prima della ricetta, non sostituzione del solver fallito e non rimozione del solver dall'MVP | APPROVATA P0 per entrambi dal mandato | Proprietario ha deciso; coordinatore esegue e verifica | M3 gate futuro, M5 release |
| **D-21** | Distribuzione dei catalog pack | Preferenza pack inclusi; canale separato solo come proposta motivata se l'inclusione è bloccata | Verificare autonomamente licenza, ridistribuzione, versione, hash, dimensioni, copertura e aggiornamento secondo 17.4; nessun download implicito o cambio runtime | PREFERENZA APPROVATA; verifiche legali e tecniche NON ESEGUITE | Proprietario ha deciso la preferenza; coordinatore verifica | M0 politica registrata, M3 adozione e M5 distribuzione da verificare |

Le decisioni D-01..D-18 mantengono l'oggetto storico, con responsabilità tecniche delegate. D-18 resta vincolante; D-19 e D-20 sono APPROVATE P0. D-21 approva la preferenza, non i singoli pack: dimensioni, sostenibilità tecnica e diritti non sono ancora verificati. Le approvazioni non equivalgono a gate scientifici o software superati.

### 23.2 Bloccanti per il prossimo gate

- Verifiche legali e tecniche dei pack secondo D-21; D-19/D-20 già approvate, senza nuove decisioni di inclusione.
- Scelta del toolkit e del linguaggio dopo gli spike M1 (D-03, D-04).
- Verifica delle licenze delle dipendenze candidate (D-02).
- Definizione del corpus di accettazione e del lineage.
- Definizione tecnica autonoma della matrice Linux minima e del protocollo, poi prove reali.

### 23.3 Differibili in sicurezza

- Scelta del backend GPU.
- Formato dei preset condivisi.
- Localizzazione oltre italiano e inglese.
- Plugin e scripting.
- Modelli AI.

### 23.4 Dipendenti da evidenze future

- Sensitivity reale della deconvoluzione su dati pubblici.
- Copertura del campione stellare per il colore relativo.
- Tasso di soluzione del solver su campi difficili.
- Costo del batch di candidati su immagini grandi.
- Beneficio misurabile del denoise e dello sharpening.

### 23.5 Regole per le decisioni

Una decisione registra opzioni, evidenza, stato, autore e data. Il coordinatore decide le questioni tecniche solo dopo le prove richieste, senza attendere conferme ordinarie. Una preferenza non è una misura; l'approvazione di D-21 non verifica un pack. Questioni riservate e blocchi reali seguono 0.8. Nessun fallimento autorizza declassamenti silenziosi.

---

## 24. Deliverable, Definition of Done e stato onesto

### 24.1 Deliverable

| Deliverable | Contenuto | Stato |
|---|---|---|
| Piano consolidato v2.2.1 | Questo documento, autosufficiente | Review indipendente PENDING; mandato M0..M9 già conferito |
| Registro requisiti | 82 F, 24 NFR, 21 D, 21 NC | Presente |
| Contratti scientifici | C-01..C-19 | Presenti come proposte |
| ADR | 17, con gate ed evidenze | Scelte tecniche autonome dopo le prove; questioni riservate in 0.8 |
| Roadmap | M0..M10; M1 6, M2 7, M3 19, M4 7, M6 9, M9 6 sotto-milestone; 54 totali | Presente |
| Test plan | Livelli, identificativi, protocollo anti-leakage | Presente come piano, non eseguito |
| Registro dipendenze | Appendice D, cinque tipi | Presente |
| Review indipendente | Revisore distinto dall'autore sul testo o diff effettivo | PENDING per 2.2.1; il PASS storico 2.2 non vale per questa revisione |

### 24.2 Definition of Done dell'MVP complesso

L'MVP è done quando:

1. Il percorso deep sky dichiarato funziona end-to-end senza software esterno.
2. Import, calibrazione con otto combinazioni, debayer, registrazione, resampling, stacking, background, maschere, stretch, denoise, sharpening, stelle, export e recovery superano i propri gate.
3. I P0 fotografici e l'Advisor sono testati, non solo presenti.
4. Le configurazioni P0 della sezione 11.11 sono verificate.
5. La matrice Linux minima e il packaging scelto passano M5.
6. Nessun output parziale è pubblicato come completo.
7. La provenance permette di ricostruire il risultato.
8. I limiti e le astensioni sono visibili all'utente.
9. Il corpus di accettazione indipendente è separato e misurato.
10. D-19 e D-20, già APPROVATE P0, superano i gate di deconvoluzione, colore relativo e solver assistito. D-21, preferenza APPROVATA, richiede verifiche legali e tecniche dei pack, integrità e primo avvio offline dimostrati. Nessuna di queste prove è implicita nell'approvazione.
11. NFR-FBK-01 è verificato: nessun fallback o sostituzione automatica, stop con errore non nullo su indisponibilità o gate fallito, resume ammesso solo con ricetta identica al checkpoint valido.

### 24.3 Checklist di accettazione del piano

- [x] 82 requisiti F presenti con criterio di accettazione proposto.
- [x] 24 NFR presenti, incluso NFR-FBK-01.
- [x] 21 D e 21 NC presenti.
- [x] 17 ADR presenti con gate.
- [x] C-01..C-19 presenti, ciascuno con undici campi.
- [x] M3 in 19 slice, M4 in 7 slice, M6 in 9, M9 in 6; 54 sotto-milestone totali.
- [x] Matrice di attivazione P0 presente.
- [x] Distinzione tra controllo di regressione e validazione indipendente.
- [x] Corpus di accettazione e divieto di astensione sistematica specificati, non ancora realizzati.
- [x] Budget dell'intero batch specificato.
- [x] Semantica delle maschere dopo riordino specificata.
- [x] Gate separati per riduzione e separazione stellare.
- [x] Contratto colore per stadio e componenti reference.
- [x] Deconvoluzione con sensibilità e baseline no-op.
- [x] D-19/D-20 registrate APPROVATE P0; D-21 preferenza pack inclusi APPROVATA, audit non eseguito.
- [ ] Review indipendente della 2.2.1: PENDING, da eseguire sul testo o diff effettivo dopo consegna stabile.
- [x] Mandato operativo M0..M9 recepito; nessuna conferma rituale, M10 non autorizzata.
- [ ] Validazioni empiriche, selezione dei dataset, soglie, misure e audit delle dipendenze previsti dai gate futuri.

### 24.4 Change log 2.1 verso 2.2

| Area | Cambiamento | Impatto per il lettore |
|---|---|---|
| Struttura | M3 divisa in 19 slice ortogonali, M4 in 7 | Si vede dove si innesta ogni funzione e quale gate la blocca |
| Dipendenze | Cinque tipi e matrice di attivazione P0 | Un grafo aciclico non basta più: si controllano le configurazioni dichiarate |
| Validazione | Controllo di regressione e validazione indipendente separati | Le conclusioni non possono più essere presentate come indipendenti quando non lo sono |
| Advisor | Corpus di accettazione, copertura utile, danno e beneficio | L'astensione sistematica non passa l'accettazione |
| Budget | Prenotazione dell'intero batch di candidati | Le stime non garantiscono il completamento; risorse insufficienti arrestano il percorso senza promuovere risultati parziali |
| Maschere | Policy per asset congelato e generatore derivato | Un riordino non cambia la maschera in silenzio |
| Stelle | Gate indipendenti per riduzione e separazione | La reintegrazione neutra non viene confusa con la riduzione |
| Deconvoluzione | Una formulazione P0, `b` fisso, sensibilità, no-op | Non si promette recupero oltre il modello |
| Colore | Contratto per stadio e componenti reference | Non si dichiara "colore preservato" in ogni stadio |
| Diagnostica | Matrice famiglia, operatore eseguibile, avviso, non correggibile, futuro | Il denoise non implica correzioni specializzate |
| Roadmap | M5 bloccata finché tutti i P0 non sono testati | Nessun taglio per calendario |
| Decisioni | Nella 2.2 tre scelte del proprietario erano esplicite; risolte dal mandato 2.2.1 | Storico del perimetro, non nuova attesa di approvazione |

### 24.4.1 Change log mirato 2.2 verso 2.2.1

- Registrate D-19/D-20 APPROVATE P0 e D-21 preferenza inclusa APPROVATA, separando politica da verifiche future.
- Delegate le decisioni tecniche dopo evidenze; eliminati consensi ricorrenti per ADR, spike, milestone e verifica repository. M0..M9 autorizzate; questioni riservate in 0.8.
- Distinti STOP del prodotto senza fallback e correzione autonoma dello sviluppo; criteri, fixture e perimetro restano vincolanti.
- Precisati NC-01, limite C-14 per input solo NL, installazione offline dei pack e misure no-fallback negli spike esistenti.
- Conservati sezioni, contratti, identificativi, 54 sotto-milestone e archi canonici. Predicati di inclusione D-19/D-20 veri per il mandato, non per test già superati.
- Review 2.2.1 PENDING; PASS 2.2 conservato solo come storico. Nessuna implementazione applicativa o validazione empirica eseguita in questa delega.

### 24.5 Punti scientifici aperti

- Validazione della deconvoluzione su PSF reali e sensibilità a PSF errata.
- Copertura e sistematica del colore relativo su campi poveri di stelle.
- Tasso di soluzione del solver assistito con hint imprecisi.
- Stima del rumore non lineare con trasformazione ignota.
- Calibrazione della confidenza dell'Advisor.
- Beneficio reale del denoise e dello sharpening su dati pubblici.
- Rappresentazione della covarianza nei casi correlati.
- Separazione fondo e nebulosa diffusa.

### 24.6 Stato di validazione del documento e verbale storico

**Esito della review indipendente 2.2.1: PENDING.** Nessun PASS della versione precedente vale come review di questo file. L'autore incaricato è `architect`, modello/provider dichiarati dal runtime `openai/gpt-6-astra`, senza attestazione indipendente; ID di sessione non esposto nel mandato corrente, costi e token non attestati. Materiali letti integralmente: richiesta di 122 righe e piano 2.2 di 4452 righe, in blocchi consecutivi, con ripresa dalla riga 1736. Le istruzioni operative successive e i percorsi canonici sono recepiti dalle comunicazioni del coordinatore e dagli aggiornamenti AGENTS esposti dal runtime; non si dichiara lettura integrale di `ISTRUZIONI_OPERATIVE.md`. Nessuna subdelega in questo incarico.

Il verbale seguente riguarda esclusivamente la **2.2 storica**. Esito storico: PASS documentale, non validazione sperimentale. Revisore indipendente dagli autori di allora: agente `architect`, sessione `ses_e8cd7a18effet7SEK9Mx757TMg`, modello `openai/gpt-6-astra` autodichiarato dal runtime, senza attestazione indipendente.

Copertura cumulativa: richiesta di 139 righe letta integralmente; prima versione effettiva v2.2 di 3136 righe letta integralmente; successive riletture mirate delle correzioni, inclusa tutta l'appendice D aggiornata, allora alle righe 3099–4433. La conferma finale ha verificato anche rettifiche a PSF, matrice Linux obbligatoria, ordine M2 e limite delle stime di budget. Non si dichiara una nuova lettura integrale dell'ultima versione corretta. I contatori, i riferimenti e gli hash sono stati verificati separatamente dal coordinatore, non attestati dal revisore per delega.

| Rilievo sul testo | Correzione verificata | Stato |
|---|---|---|
| Mapping asinh incompatibile con i rapporti RGB promessi | Fattore comune nel dominio reference; `k=0` analitico; parametro invalido provoca STOP | Chiuso |
| Formula di sharpening ambigua | Decomposizione telescopica e guadagni sulle bande definite | Chiuso |
| PSF stellare additiva applicata a dati non lineari | Fit sull'antenato lineare e componente differenziale trasformata; gate semantico distinto dalla ricostruzione | Chiuso |
| Dipendenze mancanti, autocontenimenti e slice non definite | Nodi individuali, archi separati, stadi ADR/NFR, riferimenti risolti e ordine topologico esplicito | Chiuso |
| Diagnosi non lineare rinviata alla GUI | Analisi numerica e strategie headless in M3; interazione in M4 | Chiuso |
| Declassamenti o sostituzioni dopo errore | D-18 risolta; P0 approvato vincola M5; NC-21/NFR-FBK-01 vietano tutti i fallback | Chiuso |
| Strategie A/B non confrontabili | Coppia denoise lineare/post-stretch con endpoint comune e baseline non sostitutiva | Chiuso |

Controlli documentali del coordinatore: 395 nodi, 847 archi e 27 predicati; nessun duplicato, estremo irrisolto, autociclo o ciclo nel contenimento e nell'unione di prerequisiti, condizioni e decisioni. Nessun riferimento testuale F/NFR/ADR/C/D irrisolto. Tutte le 133 foglie F/NFR P0 dichiarate entro M5, comprese quelle proposte quando incluse, sono raggiungibili transitivamente dal gate M5 nell'unione degli archi condizionali. Verificati conservazione degli ID storici, 54 sotto-milestone, 19 contratti con undici campi e integrità dei sei input storici. L'analisi dell'unione copre strutturalmente i suoi sottografi attivati; non dimostra correttezza dei predicati runtime, completezza degli scenari fisici o funzionamento del futuro scheduler.

Alla chiusura della 2.2 le decisioni D-19/D-20/D-21 erano aperte. Nella 2.2.1 il mandato le risolve nei termini di 23.1; i gate scientifici, bibliografici, legali ed empirici restano da eseguire. Questa delega non esegue test applicativi, benchmark, spike o prove su immagini. Il mandato autorizza l'esecuzione successiva, ma nessun controllo documentale dimostra funzionamento del prodotto.

### 24.7 Conclusione onesta

Il piano è ampio e difendibile nel metodo. Non è ancora dimostrato nei risultati. La parte più solida è la separazione tra dato, misura, display e fotografia. La parte più fragile è la catena di validazione, che dipende da dataset, corpus e test non ancora costruiti.

---

---

## 25. Riferimenti

### 25.1 Fonti di prodotto consultate nella pianificazione precedente

Le fonti seguenti provengono dalla pianificazione 2.0 e non sono state ricontrollate in questa revisione. Restano etichettate come storiche. Le date e le versioni sono quelle dichiarate allora.

- Siril, pagina di download, https://siril.org/download/
- PixInsight, requisiti di sistema, https://pixinsight.com/sysreq/
- Astro Pixel Processor, licenza, https://www.astropixelprocessor.com/product/astro-pixel-processor-owner-license/
- DeepSkyStacker, release, https://github.com/deepskystacker/DSS/releases/tag/6.2.2
- GraXpert, release, https://github.com/Steffenhir/GraXpert/releases/tag/3.0.2
- StarNet, https://starnetastro.com/
- RC Astro tools, https://www.rc-astro.com/stand-alone-rc-astro-tools/
- AstroImageJ, https://www.astro.louisville.edu/software/astroimagej/
- AutoStakkert!, https://www.autostakkert.com/
- AstroSurface, https://astrosurface.com/pageuk.html
- GIMP, annuncio 3.2.6, https://www.gimp.org/news/2026/09/10/gimp-3-2-6-released/
- RawTherapee, https://www.rawtherapee.com/

### 25.2 Standard e letteratura candidata

I riferimenti seguenti sono candidati. Non sono stati letti integralmente in questa revisione e non sostengono da soli un'implementazione.

- FITS Standard 4.0.
- Specifica TIFF 6.0.
- ICC.1:2010.
- Richardson, 1972; Lucy, 1974; Wiener, 1949, per la deconvoluzione.
- Fruchter e Hook, 2002, per Drizzle.
- Huber, 1981; Rousseeuw e Leroy, 1987, per la statistica robusta.
- Malvar, He e Cutler, 2004, per il demosaicing.
- Letteratura su wavelet à trous, shrinkage e stima del rumore.
- Letteratura su PSF, fitting stellare e separazione di componenti.
- Cataloghi stellari e sistemi fotometrici, da verificare per licenza e copertura.

### 25.3 Riferimenti di processo

- Specifica originale e piano 2.1, conservati come storia.
- Richiesta di revisione v2.2, con il mandato e i vincoli.
- Review dell'architetto e adjudication del coordinatore, registrate come storico e non come certificazione di questa versione.

---

## 26. Appendici

### Appendice A. Conteggi, integrità e stato dei file

| Voce | Valore |
|---|---|
| Requisiti F totali | 82: 76 conservati più 6 F-ANA |
| Requisiti NFR | 24, incluso NFR-FBK-01 |
| Decisioni DEC | 12 |
| Decisioni D | 21, da D-01 a D-21 |
| Nuovi vincoli NC | 21, incluso NC-21 |
| ADR | 17 |
| Contratti scientifici | 19, da C-01 a C-19 |
| Slice M3 | 19, da M3.a a M3.s |
| Slice M4 | 7, da M4.a a M4.g |
| Sotto-milestone M6 | 9, da M6.a a M6.i |
| Sotto-milestone M9 | 6, da M9.a a M9.f |
| Sotto-milestone totali | 54: M1 6, M2 7, M3 19, M4 7, M6 9, M9 6 |
| Milestone | M0..M10 |
| Test identificati | 13 nella tabella 15.2, più i test citati nei contratti |
| Review indipendente v2.2 storica | PASS documentale con copertura e riserve di 24.6, non valido per 2.2.1 |
| Review indipendente v2.2.1 | PENDING |
| Repository indicato dal proprietario | `https://github.com/Luporosso76/astroProcessing.git`; nessuna verifica Git in questa delega, stato corrente in `docs/progress.md` |

Integrità dei file di input usati per questa stesura:

| File | Righe | SHA-256 |
|---|---|---|
| `ASTRO_POSTPROCESSING_PLAN_v2.1.md` | 2705 | `384857061fc7cc79c271dbc114a4a4dccecbe9b792dbe3ad6790bee4b59ce5a3` |
| `RICHIESTA_REVISIONE_ASTRO_POSTPROCESSING_PLAN_v2.2_CODEX.md` | 139 | `5e1cabc46695d162b5a7d73dcc7c47ed0791732e49e62134a5ec26883da03ebb` |

Il file v2.2.1 non contiene il proprio hash. L'input storico `ASTRO_POSTPROCESSING_PLAN_v2.2.md` ha 4452 righe e SHA-256 `8fa0f881bd932c1e468449dea74037bad8cd88bdc8937c7077d1f233aa1f29c1`; resta immutato. Gli hash della tabella precedente sono dati storici della redazione 2.2, non attestazioni di lettura integrale compiuta dall'autore 2.2.1.

### Appendice B. Tracciabilità delle sezioni

| Sezione | Contenuto | Fonte o motivo |
|---|---|---|
| 0..4 | Perimetro, sintesi, eredità, DEC, NC | Conservati e aggiornati |
| 5 | Priorità, MVP, slice, matrice funzioni | Post-processing P0 conservato; deconvoluzione, colore e solver APPROVATI |
| 6 | Mercato e confronto | Conservato, fonti storiche |
| 7 | Architettura, DAG, persistenza, maschere, planner | Esteso con semantica delle maschere e lineage |
| 8 | Scienza e formule | Esteso con rumore non lineare, PSF, asinh e colore |
| 9 | Calibrazione flessibile | Conservato e precisato |
| 10 | Contratti C-01..C-19 | C-11..C-19 nuovi, C-19 validazione |
| 11 | Requisiti F e attivazione P0 | 82 requisiti, cinque tipi di dipendenza |
| 12 | NFR | 23 conservati e NFR-FBK-01 aggiunto; 24 totali |
| 13 | Advisor | Esteso con disponibilità, indipendenza, corpus, batch |
| 14 | Modalità, flussi, accessibilità | Conservato, aggiunta policy maschere |
| 15 | Test, dataset, benchmark | Esteso con identificativi e anti-leakage |
| 16..19 | Linux, licenze, export, sicurezza | Conservati e aggiornati |
| 20 | ADR | 17, senza nuovi ID |
| 21 | Roadmap e gate | M3 in 19 slice, M4 in 7, M5 bloccata sui P0 |
| 22..24 | Rischi, decisioni, DoD, stato | Decisioni del mandato recepite; review 2.2.1 PENDING, evidenze future distinte |
| 25..27 | Riferimenti, appendici, sintesi | Chiusura |

### Appendice C. Come usare questo documento

1. Leggere la sezione 5 per il perimetro e le tre decisioni strategiche D-19, D-20 e D-21.
2. Leggere la sezione 11.1 e la sezione 11.11 prima di parlare di dipendenze.
3. Leggere la sezione 13.14 e la sezione 13.16 prima di dichiarare una validazione o un'accettazione.
4. Usare l'appendice D come registro canonico delle dipendenze.
5. Non citare una formula o un test come eseguito se non lo è.
6. Non presentare questa versione come review indipendente.

### Appendice D. Registro canonico delle dipendenze

Questa appendice è l'unica autorità per gli archi di dipendenza. Le sezioni 5, 11, 12, 20 e 21 descrivono capacità e criteri; i loro riepiloghi non aggiungono archi. Ogni riga NODI definisce un ID; ogni riga ARCHI definisce un solo arco. Gli aggregati non sono prerequisiti operativi: si usano le foglie. `contains` descrive appartenenza, `verified_by` collega evidenze senza imporre test conclusi prima dell'operatore. `requires_all` è un prerequisito di costruzione/consegna; `requires_when` attiva una capacità o un controllo runtime secondo un predicato dichiarato. `decided_by` richiede approvazione prima della costruzione, non il solo superamento del gate finale.

Lo stadio indica la prima consegna richiesta della foglia o il completamento dell'aggregato. `.entry` è il gate antecedente alla fase, non una sotto-milestone aggiuntiva. Nello stesso stadio vale l'ordine topologico degli archi. Le decisioni tecniche `decided_by` si chiudono autonomamente dopo evidenze; non sono voti rituali del proprietario. D-19 e D-20 sono già APPROVATE P0; D-21 è già APPROVATA come preferenza, mentre NFR-LIC-01.d e G-PACK richiedono prove future. Registrare uno stadio non dichiara un gate superato. Nessun grafo prova da solo completezza scientifica o fattibilità.

#### D.1 NODI funzionali

| ID | Classe | Priorità | Stadio | Descrizione |
|---|---|---|---|---|
| F-IO-01 | F | P0 | M2.a | FITS |
| F-IO-02 | F | P0 | M3.a | RAW |
| F-IO-03 | F | P0 | M3.a | Classificazione batch |
| F-IO-04 | F | P0 | M3.a | Metadati decoder |
| F-IO-05 | F | P0 | M2.a | Inventario bounded |
| F-IO-06 | F | P1 | M6.a | Formati estesi |
| F-IO-07 | F | P1 | M6.f | Cataloghi dataset |
| F-IO-08 | F | P0 | M3.a | Ricollegamento |
| F-IO-09 | F | P2 | M9.b | Video |
| F-IO-10 | F | P0 | M2.c | Prestacked tipizzato |
| F-PRJ-01 | F | P0 | M2.c | Persistenza |
| F-PRJ-02 | F | P0 | M2.c | Originali |
| F-PRJ-03 | F | P0 | M2.b | DAG |
| F-PRJ-04 | F | P0 | M2.c | Revisioni |
| F-PRJ-05 | F-aggregato | P1 | M7 | Branch e automazione |
| F-PRJ-05.a | F-aggregato | P0 | M4.b | Branch reversibili |
| F-PRJ-05.a.1 | F-slice | P0 | M3.r | Branch core |
| F-PRJ-05.a.2 | F-slice | P0 | M4.b | Controlli branch |
| F-PRJ-05.b | F-slice | P1 | M7 | Preset e batch |
| F-PRJ-06 | F | P0 | M2.c | Recovery |
| F-PRJ-07 | F | P0 | M2.c | Provenance base estensibile |
| F-PRJ-08 | F | P2 | M9.f | Plugin e scripting |
| F-CAL-01 | F-aggregato | P0 | M3.b | Master |
| F-CAL-01.a | F-slice | P0 | M2.e | Master sintetici minimi |
| F-CAL-01.b | F-slice | P0 | M3.b | Master completi |
| F-CAL-02 | F-aggregato | P0 | M3.c | Calibrazione |
| F-CAL-02.a | F-slice | P0 | M2.e | Light matched minimal |
| F-CAL-02.b | F-slice | P0 | M3.c | Otto combinazioni |
| F-CAL-03 | F-aggregato | P0 | M3.c | Flat |
| F-CAL-03.a | F-slice | P0 | M2.e | Q sintetico |
| F-CAL-03.b | F-slice | P0 | M3.c | Flat e dark-flat completi |
| F-CAL-04 | F | P0 | M3.c | Bad pixel |
| F-CAL-05 | F | P1 | M6.f | Scaling dark |
| F-CAL-06 | F-aggregato | P0 | M3.c | Incertezza |
| F-CAL-06.a | F-slice | P0 | M2.e | Varianza minima |
| F-CAL-06.b | F-slice | P0 | M3.c | Propagazione completa dichiarata |
| F-CAL-07 | F | P1 | M6.f | Cosmetica avanzata |
| F-CAL-08 | F | P0 | M3.d | Debayer |
| F-REG-01 | F | P0 | M3.e | Stelle e rumore |
| F-REG-02 | F | P0 | M3.f | Registrazione |
| F-REG-03 | F | P0 | M3.g | Warp |
| F-REG-04 | F-aggregato | P1 | M6.a | Astrometria |
| F-REG-04.a | F-slice | P0 | M3.p | Solver assistito |
| F-REG-04.b | F-slice | P1 | M6.a | Blind generale |
| F-REG-05 | F | P1 | M6.a | Distorsione |
| F-REG-06 | F | P1 | M6.f | Sessioni |
| F-REG-07 | F | P2 | M9.e | Bersaglio mobile |
| F-REG-08 | F | P2 | M9.f | Astrometria quantitativa |
| F-STK-01 | F | P0 | M3.h | Integrazione |
| F-STK-02 | F | P0 | M3.h | Coverage |
| F-STK-03 | F | P0 | M3.h | Pesi e report |
| F-STK-04 | F | P1 | M6.f | Rejection avanzata |
| F-STK-05 | F | P1 | M6.f | Normalizzazione |
| F-STK-06 | F | P1 | M6.e | Drizzle |
| F-STK-07 | F | P1 | M6.f | Stack per filtro |
| F-STK-08 | F | P2 | M9.a | Mosaici |
| F-STK-09 | F | P2 | M9.e | Stack mobile |
| F-POST-01 | F | P0 | M4.a | STF e primitiva display senza dipendenza UX-02 |
| F-POST-02 | F-aggregato | P0 | M4.f | Curve |
| F-POST-02.a | F-slice | P0 | M3.k | Curve core |
| F-POST-02.b | F-slice | P0 | M4.f | Curve UI |
| F-POST-03 | F-aggregato | P0 | M4.f | Background |
| F-POST-03.a | F-slice | P0 | M3.i | Background core |
| F-POST-03.b | F-slice | P0 | M4.f | Background UI |
| F-POST-04 | F-aggregato | P0 | M4.f | Bilanciamento |
| F-POST-04.a | F-slice | P0 | M3.k | Bilanciamento core |
| F-POST-04.b | F-slice | P0 | M4.f | Bilanciamento UI |
| F-POST-05 | F-aggregato | P1 | M6.b | Colore catalogo |
| F-POST-05.a | F-aggregato | P0 | M4.f | Colore relativo |
| F-POST-05.a.1 | F-slice | P0 | M3.q | Colore core |
| F-POST-05.a.2 | F-slice | P0 | M4.f | Colore UI |
| F-POST-05.b | F-slice | P1 | M6.b | SPCC completa |
| F-POST-06 | F-aggregato | P1 | M6.i | Stretch evoluto |
| F-POST-06.a | F-aggregato | P0 | M4.f | Asinh |
| F-POST-06.a.1 | F-slice | P0 | M3.k | Asinh core |
| F-POST-06.a.2 | F-slice | P0 | M4.f | Asinh UI |
| F-POST-06.b | F-slice | P1 | M6.i | GHS |
| F-POST-07 | F | P1 | M6.c | LRGB e narrowband |
| F-POST-08 | F-aggregato | P1 | M6.d | Maschere |
| F-POST-08.a | F-aggregato | P0 | M4.e | Maschere utilizzabili |
| F-POST-08.a.1 | F-slice | P0 | M3.j | Generatori ed edit serializzati |
| F-POST-08.a.2 | F-slice | P0 | M4.e | Brush poligoni morfologia feather UI |
| F-POST-08.b | F-slice | P1 | M6.d | Maschere sofisticate |
| F-POST-09 | F-aggregato | P1 | M6.g | Denoise |
| F-POST-09.a | F-aggregato | P0 | M4.f | Denoise fondamentale |
| F-POST-09.a.1 | F-slice | P0 | M3.l | Denoise L e NL core |
| F-POST-09.a.2 | F-slice | P0 | M4.f | Denoise UI |
| F-POST-09.b | F-slice | P1 | M6.g | Denoise avanzato |
| F-POST-10 | F-aggregato | P1 | M6.h | Deconvoluzione |
| F-POST-10.a | F-aggregato | P0 | M4.f | PSF fissa |
| F-POST-10.a.1 | F-slice | P0 | M3.o | Solver regolarizzato core |
| F-POST-10.a.2 | F-slice | P0 | M4.f | Deconvoluzione UI |
| F-POST-10.b | F-slice | P1 | M6.h | RL e blind |
| F-POST-11 | F-aggregato | P1 | M6.i | Contrasto |
| F-POST-11.a | F-aggregato | P0 | M4.f | Sharpening fondamentale |
| F-POST-11.a.1 | F-slice | P0 | M3.m | Multiscala core |
| F-POST-11.a.2 | F-slice | P0 | M4.f | Contrasto UI |
| F-POST-11.b | F-slice | P1 | M6.i | HDR avanzato |
| F-POST-12 | F-aggregato | P1 | M6.i | Stelle |
| F-POST-12.a | F-aggregato | P0 | M4.f | Riduzione |
| F-POST-12.a.1 | F-slice | P0 | M3.n | Riduzione core |
| F-POST-12.a.2 | F-slice | P0 | M4.f | Riduzione UI |
| F-POST-12.b | F-aggregato | P0 | M4.f | Separazione limitata |
| F-POST-12.b.1 | F-slice | P0 | M3.n | Separazione core |
| F-POST-12.b.2 | F-slice | P0 | M4.f | Separazione UI |
| F-POST-12.c | F-slice | P1 | M6.i | Separazione avanzata non AI |
| F-POST-13 | F | P1 | M6.i | Colore selettivo e layer |
| F-POST-14 | F-aggregato | P1 | M6.i | Anteprime |
| F-POST-14.a | F-slice | P0 | M4.d | Comparatore |
| F-POST-14.b | F-slice | P1 | M6.i | Anteprime avanzate |
| F-POST-15 | F | P2 | M8 | Starless ML |
| F-POST-16 | F | P2 | M9.a | Mosaici narrowband |
| F-UX-01 | F | P0 | M4.a | Workspace |
| F-UX-02 | F | P0 | M4.a | Pan zoom istogramma |
| F-UX-03 | F | P0 | M4.b | Cancel |
| F-UX-04 | F | P0 | M4.g | Preferenze risorse |
| F-UX-05 | F | P0 | M4.c | Modalità |
| F-UX-06 | F-aggregato | P1 | M6.d | ROI e editing |
| F-UX-06.a | F-slice | P0 | M4.d | ROI e confronto |
| F-UX-06.b | F-slice | P1 | M6.d | Solo editing sofisticato |
| F-UX-07 | F | P0 | M4.g | Accessibilità |
| F-UX-08 | F | P1 | M6.i | Localizzazione |
| F-UX-09 | F | P2 | M9.f | Workspace avanzato |
| F-RES-01 | F | P0 | M2.d | Auto |
| F-RES-02 | F | P0 | M2.d | Manuale |
| F-RES-03 | F | P0 | M2.d | Scheduler |
| F-RES-04 | F | P0 | M2.d | Monitor |
| F-RES-05 | F-aggregato | P1 | M6.i | Budget |
| F-RES-05.a | F-slice | P0 | M2.d | Pool separati |
| F-RES-05.b | F-slice | P1 | M6.i | Coordinamento avanzato |
| F-RES-06 | F-aggregato | P1 | M6.i | Pressione |
| F-RES-06.a | F-slice | P0 | M2.d | Checkpoint |
| F-RES-06.b | F-slice | P1 | M6.i | Modifica interattiva |
| F-RES-07 | F-aggregato | P1 | M6.i | Admission |
| F-RES-07.a | F-slice | P0 | M2.d | Minimo fattibile |
| F-RES-07.b | F-slice | P1 | M6.i | Politiche avanzate |
| F-RES-08 | F-aggregato | P1 | M6.i | Worker |
| F-RES-08.a | F-slice | P0 | M2.d | Quota e cleanup |
| F-RES-08.b | F-slice | P1 | M6.i | Tuning CPU; GPU separata M7 |
| F-ANA-01 | F-aggregato | P0 | M4.d | Diagnosi |
| F-ANA-01.a | F-slice | P0 | M3.r | Diagnosi L e NL headless |
| F-ANA-01.b | F-slice | P0 | M4.d | Diagnosi UI |
| F-ANA-02 | F-aggregato | P0 | M4.d | Evidenze |
| F-ANA-02.a | F-slice | P0 | M3.r | Schede core |
| F-ANA-02.b | F-slice | P0 | M4.d | Schede UI |
| F-ANA-03 | F-aggregato | P0 | M4.d | Confronto |
| F-ANA-03.a | F-slice | P0 | M3.r | Confronto L e NL core |
| F-ANA-03.b | F-slice | P0 | M4.d | Confronto UI |
| F-ANA-04 | F-aggregato | P0 | M4.d | Strategia |
| F-ANA-04.a | F-slice | P0 | M3.r | Planner core |
| F-ANA-04.b | F-slice | P0 | M4.d | Planner UI |
| F-ANA-05 | F-aggregato | P0 | M4.d | Guardrail |
| F-ANA-05.a | F-slice | P0 | M3.r | Guardrail L e NL core |
| F-ANA-05.b | F-slice | P0 | M4.d | Guardrail UI |
| F-ANA-06 | F | P0 | M4.c | Integrazione modalità |

#### D.2 NODI non funzionali

Gli stadi NFR qualificano solo le funzioni già introdotte. La verifica release non è anticipata a M2.

| ID | Classe | Priorità | Stadio | Descrizione |
|---|---|---|---|---|
| NFR-UX-01 | NFR | P0 | M4.g | Modalità coerenti |
| NFR-EXP-01 | NFR-aggregato | P0 | M5 | Export |
| NFR-EXP-01.a | NFR-slice | P0 | M2.a | FITS minimo |
| NFR-EXP-01.b | NFR-slice | P0 | M3.s | FITS TIFF headless |
| NFR-EXP-01.c | NFR-slice | P0 | M4.g | Export UI |
| NFR-EXP-01.d | NFR-slice | P0 | M5 | Export release |
| NFR-REP-01 | NFR-aggregato | P0 | M5 | Report |
| NFR-REP-01.a | NFR-slice | P0 | M2.e | Report slice |
| NFR-REP-01.b | NFR-slice | P0 | M3.s | Report L e NL |
| NFR-REP-01.c | NFR-slice | P0 | M4.g | Report UI |
| NFR-REP-01.d | NFR-slice | P0 | M5 | Report release |
| NFR-LINUX-01 | NFR-aggregato | P0 | M5 | Matrice |
| NFR-LINUX-01.a | NFR-slice | P0 | M0 | Protocollo matrice |
| NFR-LINUX-01.b | NFR-slice | P0 | M5 | Matrice verificata |
| NFR-LINUX-02 | NFR | P0 | M5 | X11 e Wayland |
| NFR-LINUX-03 | NFR-aggregato | P0 | M5 | Minimi |
| NFR-LINUX-03.a | NFR-slice | P0 | M0 | Piano minimi |
| NFR-LINUX-03.b | NFR-slice | P0 | M1.b | Prove preliminari |
| NFR-LINUX-03.c | NFR-slice | P0 | M5 | Minimi release |
| NFR-CPU-01 | NFR-aggregato | P0 | M5 | CPU completa |
| NFR-CPU-01.a | NFR-slice | P0 | M2.e | CPU slice |
| NFR-CPU-01.b | NFR-slice | P0 | M3.s | CPU headless |
| NFR-CPU-01.c | NFR-slice | P0 | M4.g | Viewer software |
| NFR-CPU-01.d | NFR-slice | P0 | M5 | CPU release |
| NFR-GPU-01 | NFR | P1 | M7 | GPU opzionale |
| NFR-SIMD-01 | NFR | P0 | M2.e | Dispatch preflight |
| NFR-OFF-01 | NFR-aggregato | P0 | M5 | Offline |
| NFR-OFF-01.a | NFR-slice | P0 | M2.e | Offline slice |
| NFR-OFF-01.b | NFR-slice | P0 | M3.s | Advisor e pack offline |
| NFR-OFF-01.c | NFR-slice | P0 | M4.g | GUI offline |
| NFR-OFF-01.d | NFR-slice | P0 | M5 | Primo avvio offline |
| NFR-PKG-01 | NFR | P0 | M5 | Scelta packaging |
| NFR-PKG-02 | NFR | P0 | M5 | Prove packaging |
| NFR-COMP-01 | NFR | P0 | M5 | Confronti per asse |
| NFR-SEC-01 | NFR | P0 | M2.a | Limiti parser; estesi a ogni decoder |
| NFR-SEC-02 | NFR | P0 | M2.b | Nessun eval; esteso al ruleset |
| NFR-SEC-03 | NFR | P0 | M2.c | Filesystem sicuro |
| NFR-SEC-04 | NFR | P0 | M3.s | Fuzz e sanitizer |
| NFR-LIC-01 | NFR-aggregato | P0 | M5 | Licenze |
| NFR-LIC-01.a | NFR-slice | P0 | M0 | Politica |
| NFR-LIC-01.b | NFR-slice | P0 | M1.entry | Prima delle adozioni spike |
| NFR-LIC-01.c | NFR-slice | P0 | M2.entry | Prima delle adozioni core |
| NFR-LIC-01.d | NFR-slice | P0 | M3.entry | Prima delle adozioni e pack |
| NFR-LIC-01.e | NFR-slice | P0 | M5 | SBOM artefatti distribuiti |
| NFR-TEST-01 | NFR-aggregato | P0 | M5 | Suite |
| NFR-TEST-01.a | NFR-slice | P0 | M2.f | Infrastruttura |
| NFR-TEST-01.b | NFR-slice | P0 | M3.s | Scienza |
| NFR-TEST-01.c | NFR-slice | P0 | M4.g | GUI |
| NFR-TEST-01.d | NFR-slice | P0 | M5 | Release |
| NFR-TEST-02 | NFR | P0 | M3.entry | Fixture e dataset autorizzati |
| NFR-TEST-03 | NFR | P0 | M3.s | Performance misurata |
| NFR-PERF-01 | NFR | P0 | M2.d | Budget senza costanti universali |
| NFR-DATA-01 | NFR-aggregato | P0 | M5 | Integrità |
| NFR-DATA-01.a | NFR-slice | P0 | M2.c | Hash e recovery |
| NFR-DATA-01.b | NFR-slice | P0 | M3.s | Bundle headless |
| NFR-DATA-01.c | NFR-slice | P0 | M4.g | Pubblicazione UI |
| NFR-DATA-01.d | NFR-slice | P0 | M5 | Integrità release |
| NFR-FBK-01 | NFR-aggregato | P0 | M5 | Nessun fallback |
| NFR-FBK-01.a | NFR-slice | P0 | M2.d | Stop e resume core |
| NFR-FBK-01.b | NFR-slice | P0 | M3.s | Stop operatori |
| NFR-FBK-01.c | NFR-slice | P0 | M4.g | Stop UI |
| NFR-FBK-01.d | NFR-slice | P0 | M5 | Stop release |

#### D.3 NODI decisioni e contratti

| ID | Classe | Priorità | Stadio | Descrizione |
|---|---|---|---|---|
| ADR-01 | ADR | R | M2.entry | Linguaggio dopo spike |
| ADR-02 | ADR | R | M2.entry | GUI dopo spike |
| ADR-03 | ADR-aggregato | R | M3.entry | Tipi |
| ADR-03.a | ADR-stadio | R | M2.entry | Tipi minimi |
| ADR-03.b | ADR-stadio | R | M3.entry | Tipi estesi |
| ADR-04 | ADR-aggregato | R | M3.entry | Persistenza |
| ADR-04.a | ADR-stadio | R | M2.entry | Autorità e recovery |
| ADR-04.b | ADR-stadio | R | M3.entry | Branch e migrazioni |
| ADR-05 | ADR-aggregato | R | M3.entry | DAG |
| ADR-05.a | ADR-stadio | R | M2.entry | DAG minimo |
| ADR-05.b | ADR-stadio | R | M3.entry | Ruleset e cache ricerca |
| ADR-06 | ADR-aggregato | R | M3.entry | Risorse |
| ADR-06.a | ADR-stadio | R | M2.entry | Scheduler |
| ADR-06.b | ADR-stadio | R | M3.entry | Batch di ricerca |
| ADR-07 | ADR-aggregato | R | M7.entry | Backend |
| ADR-07.a | ADR-stadio | R | M2.entry | CPU dopo ricerca M1 |
| ADR-07.b | ADR-stadio | R | M7.entry | GPU produzione |
| ADR-08 | ADR-aggregato | R | M3.entry | Decoder e pack |
| ADR-08.a | ADR-stadio | R | M2.entry | FITS minimo |
| ADR-08.b | ADR-stadio | R | M3.entry | RAW e cataloghi |
| ADR-09 | ADR-aggregato | R | M3.entry | Algoritmi |
| ADR-09.a | ADR-stadio | R | M2.entry | Calibrazione minima |
| ADR-09.b | ADR-stadio | R | M3.entry | Algoritmi P0 |
| ADR-10 | ADR | R | M3.entry | Colore e WCS |
| ADR-11 | ADR | R | M9.entry | Plugin |
| ADR-12 | ADR-aggregato | R | M5.entry | Licenze e distribuzione |
| ADR-12.a | ADR-stadio | R | M0 | Politica |
| ADR-12.b | ADR-stadio | R | M5.entry | Packaging |
| ADR-13 | ADR-aggregato | R | M8.entry | Offline e AI |
| ADR-13.a | ADR-stadio | R | M2.entry | Offline |
| ADR-13.b | ADR-stadio | R | M8.entry | AI |
| ADR-14 | ADR-aggregato | R | M9.entry | Confini workflow |
| ADR-14.a | ADR-stadio | R | M0 | Confini iniziali |
| ADR-14.b | ADR-stadio | R | M9.entry | Modalità estese |
| ADR-15 | ADR | R | M3.entry | Advisor prima del planner |
| ADR-16 | ADR-aggregato | R | M3.entry | Calibrazione incompleta |
| ADR-16.a | ADR-stadio | R | M2.entry | Ledger minimo |
| ADR-16.b | ADR-stadio | R | M3.entry | Matrice completa |
| ADR-17 | ADR-aggregato | R | M3.entry | Covarianza |
| ADR-17.a | ADR-stadio | R | M2.entry | Modello minimo |
| ADR-17.b | ADR-stadio | R | M3.entry | Modelli P0 completi |
| C-01 | C-aggregato | P0 | M3.entry | Light |
| C-01.a | C-stadio | P0 | M2.entry | Light minimo |
| C-01.b | C-stadio | P0 | M3.entry | Light completo |
| C-02 | C-aggregato | P0 | M3.entry | Master |
| C-02.a | C-stadio | P0 | M2.entry | Master minimo |
| C-02.b | C-stadio | P0 | M3.entry | Master completo |
| C-03 | C | P0 | M3.entry | Debayer |
| C-04 | C | P0 | M3.entry | Stelle |
| C-05 | C | P0 | M3.entry | Registrazione |
| C-06 | C | P0 | M3.entry | Resampling |
| C-07 | C | P0 | M3.entry | Stack |
| C-08 | C | P0 | M3.entry | Background |
| C-09 | C | P0 | M3.entry | Curve e asinh |
| C-10 | C-aggregato | P0 | M3.entry | Export |
| C-10.a | C-stadio | P0 | M2.entry | FITS minimal |
| C-10.b | C-stadio | P0 | M3.entry | Export completo |
| C-11 | C | P0 | M3.entry | Maschere |
| C-12 | C | P0 | M3.entry | Denoise |
| C-13 | C | P0 | M3.entry | Sharpening |
| C-14 | C | P0 | M3.entry | Stelle separate |
| C-15 | C | P0 | M3.entry | Deconvoluzione |
| C-16 | C | P0 | M3.entry | Colore relativo |
| C-17 | C | P0 | M3.entry | Planner |
| C-18 | C | P0 | M3.entry | Solver |
| C-19 | C | P0 | M3.entry | Validazione indipendente |
| D-01 | Decisione | R | M5.entry | Identità |
| D-02 | Decisione | R | M0 | Licenze |
| D-03 | Decisione | R | M2.entry | Core |
| D-04 | Decisione | R | M2.entry | GUI |
| D-05 | Decisione | R | M2.entry | Persistenza |
| D-06 | Decisione | R | M5.entry | Packaging |
| D-07 | Decisione | R | M0 | Matrice Linux |
| D-08 | Decisione | R | M3.entry | Dataset |
| D-09 | Decisione-aggregato | R | M3.entry | Soglie per algoritmo |
| D-09.a | Decisione | R | M2.entry | Soglie minime |
| D-09.b | Decisione | R | M3.entry | Soglie operatori P0 |
| D-10 | Decisione | R | M7.entry | Backend produttivo |
| D-11 | Decisione | R | M8.entry | AI |
| D-12 | Decisione | R | M9.entry | Plugin |
| D-13 | Decisione | R | M9.entry | Misure |
| D-14 | Decisione | R | M4.entry | Lingue iniziali |
| D-15 | Decisione | R | M4.entry | Default Advisor |
| D-16 | Decisione | R | M3.entry | Default calibrazione |
| D-17 | Decisione-aggregato | R | M3.entry | Covarianza ammessa |
| D-17.a | Decisione | R | M2.entry | Covarianza minima |
| D-17.b | Decisione | R | M3.entry | Covarianza P0 |
| D-18 | Decisione | P0 | M0 | Risolta dal mandato |
| D-19 | Decisione | P0 | M3.entry | APPROVATA inclusione deconvoluzione; evidenze del gate future |
| D-20 | Decisione | P0 | M3.entry | APPROVATA inclusione colore e solver; evidenze del gate future |
| D-21 | Decisione | P0 | M0 | APPROVATA preferenza pack inclusi; verifiche legali e tecniche pendenti |

#### D.4 NODI milestone, verifiche e controlli runtime

| ID | Classe | Priorità | Stadio | Descrizione |
|---|---|---|---|---|
| M0 | Milestone | P0 | M0 | Consolidamento, review e gate documentale autonomo |
| M1 | Milestone | R | M1 | Spike autorizzati |
| M1.a | Delivery | R | M1.a | Core |
| M1.b | Delivery | R | M1.b | Viewer |
| M1.c | Delivery | R | M1.c | Persistenza |
| M1.d | Delivery | R | M1.d | Memoria |
| M1.e | Delivery | R | M1.e | GPU |
| M1.f | Delivery | R | M1.f | Hash |
| M2 | Milestone | P0 | M2 | Fondamenta |
| M2.a | Delivery | P0 | M2.a | FITS |
| M2.b | Delivery | P0 | M2.b | DAG |
| M2.c | Delivery | P0 | M2.c | Recovery |
| M2.d | Delivery | P0 | M2.d | Scheduler |
| M2.e | Delivery | P0 | M2.e | Slice scientifica |
| M2.f | Delivery | P0 | M2.f | Infrastruttura fixture |
| M2.g | Delivery | P0 | M2.g | Applicazione contratti minimi già approvati |
| M3 | Milestone | P0 | M3 | Headless |
| M3.a | Delivery | P0 | M3.a | Import |
| M3.b | Delivery | P0 | M3.b | Master |
| M3.c | Delivery | P0 | M3.c | Calibrazione |
| M3.d | Delivery | P0 | M3.d | Debayer |
| M3.e | Delivery | P0 | M3.e | Stelle |
| M3.f | Delivery | P0 | M3.f | Registrazione |
| M3.g | Delivery | P0 | M3.g | Warp |
| M3.h | Delivery | P0 | M3.h | Stack |
| M3.i | Delivery | P0 | M3.i | Background |
| M3.j | Delivery | P0 | M3.j | Maschere |
| M3.k | Delivery | P0 | M3.k | Curve colore base asinh |
| M3.l | Delivery | P0 | M3.l | Denoise |
| M3.m | Delivery | P0 | M3.m | Sharpening |
| M3.n | Delivery | P0 | M3.n | Stelle separate |
| M3.o | Delivery | P0 | M3.o | Deconvoluzione |
| M3.p | Delivery | P0 | M3.p | Solver assistito |
| M3.q | Delivery | P0 | M3.q | Colore relativo |
| M3.r | Delivery | P0 | M3.r | Advisor |
| M3.s | Delivery | P0 | M3.s | Export E2E |
| M4 | Milestone | P0 | M4 | GUI |
| M4.a | Delivery | P0 | M4.a | Viewer |
| M4.b | Delivery | P0 | M4.b | Progetto |
| M4.c | Delivery | P0 | M4.c | Modalità |
| M4.d | Delivery | P0 | M4.d | Advisor UI |
| M4.e | Delivery | P0 | M4.e | Editor maschere |
| M4.f | Delivery | P0 | M4.f | Processing UI |
| M4.g | Delivery | P0 | M4.g | Accessibilità risorse export |
| M5 | Milestone | P0 | M5 | Release |
| M6 | Milestone | P1 | M6 | Estensioni |
| M6.a | Delivery | P1 | M6.a | WCS e import estesi |
| M6.b | Delivery | P1 | M6.b | SPCC |
| M6.c | Delivery | P1 | M6.c | LRGB |
| M6.d | Delivery | P1 | M6.d | Maschere avanzate |
| M6.e | Delivery | P1 | M6.e | Drizzle |
| M6.f | Delivery | P1 | M6.f | Sessioni e calibrazione avanzata |
| M6.g | Delivery | P1 | M6.g | Denoise avanzato |
| M6.h | Delivery | P1 | M6.h | Deconvoluzione avanzata |
| M6.i | Delivery | P1 | M6.i | Rifinitura GHS stelle UI risorse |
| M7 | Milestone | P1 | M7 | GPU automazione |
| M8 | Milestone | P2 | M8 | AI |
| M9 | Milestone | P2 | M9 | Altre modalità |
| M9.a | Delivery | P2 | M9.a | Mosaici |
| M9.b | Delivery | P2 | M9.b | Planetario |
| M9.c | Delivery | P2 | M9.c | Luna |
| M9.d | Delivery | P2 | M9.d | Sole |
| M9.e | Delivery | P2 | M9.e | Comete |
| M9.f | Delivery | P2 | M9.f | Misure e scripting |
| M10 | Milestone | P2 | M10 | Portabilità previa autorizzazione |
| T-IO-01 | Test | P0 | M3.s | Import completo; incrementale da M2 |
| T-PRJ-01 | Test | P0 | M3.s | Progetto completo; incrementale da M2 |
| T-CAL-08 | Test | P0 | M3.s | Matrice calibrazione |
| T-REG-02 | Test | P0 | M3.s | Registrazione |
| T-STK-03 | Test | P0 | M3.s | Stack |
| T-POST-09 | Test | P0 | M3.s | Denoise |
| T-POST-10 | Test | P0 | M3.s | Deconvoluzione |
| T-POST-12 | Test | P0 | M3.s | Riduzione e separazione |
| T-AST-ASSIST01 | Test | P0 | M3.s | Solver |
| T-FBK-01 | Test | P0 | M5 | Stop; incrementale M2–M5 |
| T-ANA-01 | Test | P0 | M3.s | Diagnosi |
| T-ANA-04 | Test | P0 | M3.s | Planner |
| T-VAL-INDEP | Test | P0 | M3.s | Lineage e holdout |
| G-CAL | Controllo | P0 | M3.a | Stato master e compatibilità prima della scelta |
| G-OFFSET | Controllo | P0 | M3.a | Offset light noto e ledger |
| G-PACK | Controllo | P0 | M3.a | Pack licenza versione hash copertura |
| G-WCS | Controllo | P0 | M3.g | Validazione WCS importato o risolto |
| G-NOISE-L | Controllo | P0 | M3.e | Rumore lineare |
| G-NOISE-NL | Controllo | P0 | M3.k | Rumore NL con metodo scelto prima dell'esecuzione |
| G-PSF | Controllo | P0 | M3.e | PSF lineare e fondo fissato |
| G-STOP | Controllo | P0 | M2.d | Stop senza sostituzione |
| G-MASK | Controllo | P0 | M3.j | Trasporto maschere e lineage |

#### D.5 PREDICATI

Un predicato non è un'istruzione di fallback. Le scelte sono congelate nella ricetta; una condizione richiesta non soddisfatta attiva G-STOP. DFB usa disponibilità e compatibilità accertate prima della scelta; nessuna combinazione implica da sola calibrazione completa. Gli stati DF e flat sono assi separati.

| ID | Definizione |
|---|---|
| P-DFB000 | Nessun dark, flat o bias utilizzabile nel preflight |
| P-DFB001 | Solo bias utilizzabile nel preflight |
| P-DFB010 | Solo flat utilizzabile nel preflight |
| P-DFB011 | Flat e bias utilizzabili nel preflight |
| P-DFB100 | Solo dark utilizzabile nel preflight |
| P-DFB101 | Dark e bias utilizzabili nel preflight |
| P-DFB110 | Dark e flat utilizzabili nel preflight |
| P-DFB111 | Dark flat bias utilizzabili nel preflight |
| P-DF-ABS | Dark-flat assente prima della scelta |
| P-DF-INC | Dark-flat matched con bias incluso |
| P-DF-SUB | Dark-flat matched con bias rimosso |
| P-DF-BAD | Dark-flat incompatibile o sconosciuto prima della scelta |
| P-FLAT-RAW | Flat raw selezionato |
| P-FLAT-Q | Flat già normalizzato selezionato |
| P-FLAT-UNKNOWN | Stato flat sconosciuto: uso critico vietato |
| P-OFFSET | Ricetta seleziona divisione flat: offset light richiesto |
| P-SOLVE | Ricetta seleziona solver assistito; D-20.approved = true per il mandato |
| P-IMPORT | Ricetta seleziona WCS importato, non solver |
| P-PACK-MISSING | Pack richiesto dalla ricetta assente o invalido |
| P-NOISE-L | Denoise selezionato su dato lineare |
| P-NOISE-NL | Denoise selezionato su dato non lineare |
| P-DECONV | Ricetta seleziona deconvoluzione; D-19.approved = true per il mandato |
| P-INCLUDE-DECONV | true: D-19.approved = true, inclusione P0 APPROVATA; gate M3/M5 obbligatori, ancora da eseguire |
| P-INCLUDE-COLOR | true: D-20.approved = true, colore e solver entrambi P0 APPROVATI; gate M3/M5 obbligatori, ancora da eseguire |
| P-STAR-MASK | Generatore maschera stellare selezionato |
| P-REORDER | Riordino con maschera dipendente da geometria o dati |
| P-LOST | Prerequisito perso dopo selezione della ricetta |

#### D.6 ARCHI

| Da | Tipo | A | Predicato |
|---|---|---|---|
| F-PRJ-05 | contains | F-PRJ-05.a | - |
| F-PRJ-05 | contains | F-PRJ-05.b | - |
| F-PRJ-05.a | contains | F-PRJ-05.a.1 | - |
| F-PRJ-05.a | contains | F-PRJ-05.a.2 | - |
| F-CAL-01 | contains | F-CAL-01.a | - |
| F-CAL-01 | contains | F-CAL-01.b | - |
| F-CAL-02 | contains | F-CAL-02.a | - |
| F-CAL-02 | contains | F-CAL-02.b | - |
| F-CAL-03 | contains | F-CAL-03.a | - |
| F-CAL-03 | contains | F-CAL-03.b | - |
| F-CAL-06 | contains | F-CAL-06.a | - |
| F-CAL-06 | contains | F-CAL-06.b | - |
| F-REG-04 | contains | F-REG-04.a | - |
| F-REG-04 | contains | F-REG-04.b | - |
| F-POST-02 | contains | F-POST-02.a | - |
| F-POST-02 | contains | F-POST-02.b | - |
| F-POST-03 | contains | F-POST-03.a | - |
| F-POST-03 | contains | F-POST-03.b | - |
| F-POST-04 | contains | F-POST-04.a | - |
| F-POST-04 | contains | F-POST-04.b | - |
| F-POST-05 | contains | F-POST-05.a | - |
| F-POST-05 | contains | F-POST-05.b | - |
| F-POST-05.a | contains | F-POST-05.a.1 | - |
| F-POST-05.a | contains | F-POST-05.a.2 | - |
| F-POST-06 | contains | F-POST-06.a | - |
| F-POST-06 | contains | F-POST-06.b | - |
| F-POST-06.a | contains | F-POST-06.a.1 | - |
| F-POST-06.a | contains | F-POST-06.a.2 | - |
| F-POST-08 | contains | F-POST-08.a | - |
| F-POST-08 | contains | F-POST-08.b | - |
| F-POST-08.a | contains | F-POST-08.a.1 | - |
| F-POST-08.a | contains | F-POST-08.a.2 | - |
| F-POST-09 | contains | F-POST-09.a | - |
| F-POST-09 | contains | F-POST-09.b | - |
| F-POST-09.a | contains | F-POST-09.a.1 | - |
| F-POST-09.a | contains | F-POST-09.a.2 | - |
| F-POST-10 | contains | F-POST-10.a | - |
| F-POST-10 | contains | F-POST-10.b | - |
| F-POST-10.a | contains | F-POST-10.a.1 | - |
| F-POST-10.a | contains | F-POST-10.a.2 | - |
| F-POST-11 | contains | F-POST-11.a | - |
| F-POST-11 | contains | F-POST-11.b | - |
| F-POST-11.a | contains | F-POST-11.a.1 | - |
| F-POST-11.a | contains | F-POST-11.a.2 | - |
| F-POST-12 | contains | F-POST-12.a | - |
| F-POST-12 | contains | F-POST-12.b | - |
| F-POST-12 | contains | F-POST-12.c | - |
| F-POST-12.a | contains | F-POST-12.a.1 | - |
| F-POST-12.a | contains | F-POST-12.a.2 | - |
| F-POST-12.b | contains | F-POST-12.b.1 | - |
| F-POST-12.b | contains | F-POST-12.b.2 | - |
| F-POST-14 | contains | F-POST-14.a | - |
| F-POST-14 | contains | F-POST-14.b | - |
| F-UX-06 | contains | F-UX-06.a | - |
| F-UX-06 | contains | F-UX-06.b | - |
| F-RES-05 | contains | F-RES-05.a | - |
| F-RES-05 | contains | F-RES-05.b | - |
| F-RES-06 | contains | F-RES-06.a | - |
| F-RES-06 | contains | F-RES-06.b | - |
| F-RES-07 | contains | F-RES-07.a | - |
| F-RES-07 | contains | F-RES-07.b | - |
| F-RES-08 | contains | F-RES-08.a | - |
| F-RES-08 | contains | F-RES-08.b | - |
| F-ANA-01 | contains | F-ANA-01.a | - |
| F-ANA-01 | contains | F-ANA-01.b | - |
| F-ANA-02 | contains | F-ANA-02.a | - |
| F-ANA-02 | contains | F-ANA-02.b | - |
| F-ANA-03 | contains | F-ANA-03.a | - |
| F-ANA-03 | contains | F-ANA-03.b | - |
| F-ANA-04 | contains | F-ANA-04.a | - |
| F-ANA-04 | contains | F-ANA-04.b | - |
| F-ANA-05 | contains | F-ANA-05.a | - |
| F-ANA-05 | contains | F-ANA-05.b | - |
| F-IO-01 | decided_by | ADR-08.a | - |
| F-IO-01 | decided_by | ADR-03.a | - |
| F-IO-01 | requires_all | C-10.a | - |
| F-IO-01 | requires_all | NFR-SEC-01 | - |
| F-IO-02 | requires_all | F-IO-01 | - |
| F-IO-02 | decided_by | ADR-08.b | - |
| F-IO-02 | decided_by | ADR-03.b | - |
| F-IO-03 | requires_all | F-IO-02 | - |
| F-IO-03 | requires_all | G-CAL | - |
| F-IO-04 | requires_all | F-IO-01 | - |
| F-IO-05 | requires_all | F-IO-01 | - |
| F-IO-06 | requires_all | F-IO-01 | - |
| F-IO-07 | requires_all | F-IO-03 | - |
| F-IO-08 | requires_all | F-PRJ-01 | - |
| F-IO-08 | requires_all | F-IO-03 | - |
| F-IO-09 | requires_all | F-IO-01 | - |
| F-IO-10 | requires_all | F-IO-01 | - |
| F-IO-10 | requires_all | F-PRJ-07 | - |
| F-IO-02 | verified_by | T-IO-01 | - |
| F-PRJ-01 | decided_by | ADR-04.a | - |
| F-PRJ-02 | requires_all | F-PRJ-01 | - |
| F-PRJ-03 | decided_by | ADR-05.a | - |
| F-PRJ-04 | requires_all | F-PRJ-03 | - |
| F-PRJ-04 | requires_all | F-PRJ-01 | - |
| F-PRJ-05.a.1 | requires_all | F-PRJ-04 | - |
| F-PRJ-05.a.1 | decided_by | ADR-04.b | - |
| F-PRJ-05.a.2 | requires_all | F-PRJ-05.a.1 | - |
| F-PRJ-05.a.2 | requires_all | F-UX-01 | - |
| F-PRJ-05.b | requires_all | F-PRJ-05.a.1 | - |
| F-PRJ-06 | requires_all | F-PRJ-01 | - |
| F-PRJ-06 | requires_all | F-PRJ-03 | - |
| F-PRJ-07 | requires_all | F-PRJ-01 | - |
| F-PRJ-07 | requires_all | F-PRJ-03 | - |
| F-PRJ-08 | requires_all | F-PRJ-03 | - |
| F-PRJ-08 | decided_by | ADR-11 | - |
| F-PRJ-06 | verified_by | T-PRJ-01 | - |
| F-CAL-01.a | requires_all | F-IO-01 | - |
| F-CAL-01.a | requires_all | C-02.a | - |
| F-CAL-01.b | requires_all | F-CAL-01.a | - |
| F-CAL-01.b | requires_all | F-IO-03 | - |
| F-CAL-01.b | requires_all | C-02.b | - |
| F-CAL-03.a | requires_all | F-CAL-01.a | - |
| F-CAL-02.a | requires_all | F-CAL-03.a | - |
| F-CAL-02.a | requires_all | C-01.a | - |
| F-CAL-06.a | requires_all | F-CAL-02.a | - |
| F-CAL-06.a | decided_by | ADR-17.a | - |
| F-CAL-03.b | requires_all | F-CAL-03.a | - |
| F-CAL-03.b | requires_all | F-CAL-01.b | - |
| F-CAL-02.b | requires_all | F-CAL-02.a | - |
| F-CAL-02.b | requires_all | F-CAL-03.b | - |
| F-CAL-02.b | requires_all | C-01.b | - |
| F-CAL-04 | requires_all | F-CAL-02.b | - |
| F-CAL-06.b | requires_all | F-CAL-06.a | - |
| F-CAL-06.b | requires_all | F-CAL-04 | - |
| F-CAL-06.b | decided_by | ADR-17.b | - |
| F-CAL-05 | requires_all | F-CAL-06.b | - |
| F-CAL-07 | requires_all | F-CAL-06.b | - |
| F-CAL-08 | requires_all | F-CAL-06.b | - |
| F-CAL-08 | requires_all | C-03 | - |
| F-CAL-02.b | verified_by | T-CAL-08 | - |
| F-REG-01 | requires_all | F-IO-04 | - |
| F-REG-01 | requires_all | F-CAL-06.b | - |
| F-REG-01 | requires_all | C-04 | - |
| F-REG-02 | requires_all | F-REG-01 | - |
| F-REG-02 | requires_all | C-05 | - |
| F-REG-03 | requires_all | F-REG-02 | - |
| F-REG-03 | requires_all | C-06 | - |
| F-REG-03 | decided_by | ADR-10 | - |
| F-REG-04.a | requires_all | F-REG-02 | - |
| F-REG-04.a | requires_all | C-18 | - |
| F-REG-04.a | decided_by | D-20 | - |
| F-REG-04.b | requires_all | F-REG-02 | - |
| F-REG-05 | requires_all | F-REG-04.b | - |
| F-REG-06 | requires_all | F-REG-05 | - |
| F-REG-07 | requires_all | F-REG-02 | - |
| F-REG-08 | requires_all | F-REG-05 | - |
| F-REG-02 | verified_by | T-REG-02 | - |
| F-REG-04.a | verified_by | T-AST-ASSIST01 | - |
| F-STK-01 | requires_all | F-REG-03 | - |
| F-STK-01 | requires_all | F-CAL-06.b | - |
| F-STK-01 | requires_all | C-07 | - |
| F-STK-02 | requires_all | F-REG-03 | - |
| F-STK-03 | requires_all | F-STK-01 | - |
| F-STK-03 | requires_all | F-STK-02 | - |
| F-STK-03 | requires_all | F-REG-01 | - |
| F-STK-04 | requires_all | F-STK-03 | - |
| F-STK-05 | requires_all | F-STK-01 | - |
| F-STK-06 | requires_all | F-REG-03 | - |
| F-STK-06 | requires_all | F-STK-01 | - |
| F-STK-07 | requires_all | F-STK-05 | - |
| F-STK-07 | requires_all | F-REG-06 | - |
| F-STK-08 | requires_all | F-STK-07 | - |
| F-STK-08 | requires_all | F-REG-05 | - |
| F-STK-09 | requires_all | F-REG-07 | - |
| F-STK-09 | requires_all | F-STK-02 | - |
| F-STK-03 | verified_by | T-STK-03 | - |
| F-POST-01 | requires_all | F-PRJ-03 | - |
| F-POST-02.a | requires_all | F-PRJ-03 | - |
| F-POST-02.a | requires_all | C-09 | - |
| F-POST-02.b | requires_all | F-POST-02.a | - |
| F-POST-02.b | requires_all | F-UX-02 | - |
| F-POST-03.a | requires_all | F-POST-02.a | - |
| F-POST-03.a | requires_all | F-POST-08.a.1 | - |
| F-POST-03.a | requires_all | C-08 | - |
| F-POST-03.b | requires_all | F-POST-03.a | - |
| F-POST-03.b | requires_all | F-POST-08.a.2 | - |
| F-POST-04.a | requires_all | F-POST-02.a | - |
| F-POST-04.a | decided_by | ADR-10 | - |
| F-POST-04.b | requires_all | F-POST-04.a | - |
| F-POST-04.b | requires_all | F-UX-02 | - |
| F-POST-05.a.1 | requires_all | F-POST-03.a | - |
| F-POST-05.a.1 | requires_all | F-POST-04.a | - |
| F-POST-05.a.1 | requires_all | C-16 | - |
| F-POST-05.a.1 | decided_by | D-20 | - |
| F-POST-05.a.2 | requires_all | F-POST-05.a.1 | - |
| F-POST-05.a.2 | requires_all | F-UX-02 | - |
| F-POST-05.b | requires_all | F-REG-04.b | - |
| F-POST-05.b | requires_all | F-POST-04.a | - |
| F-POST-06.a.1 | requires_all | F-POST-04.a | - |
| F-POST-06.a.1 | requires_all | C-09 | - |
| F-POST-06.a.2 | requires_all | F-POST-06.a.1 | - |
| F-POST-06.a.2 | requires_all | F-UX-02 | - |
| F-POST-06.b | requires_all | F-POST-02.a | - |
| F-POST-07 | requires_all | F-POST-04.a | - |
| F-POST-07 | requires_all | F-STK-07 | - |
| F-POST-08.a.1 | requires_all | F-POST-02.a | - |
| F-POST-08.a.1 | requires_all | C-11 | - |
| F-POST-08.a.2 | requires_all | F-POST-08.a.1 | - |
| F-POST-08.a.2 | requires_all | F-UX-02 | - |
| F-POST-08.b | requires_all | F-POST-08.a.2 | - |
| F-POST-09.a.1 | requires_all | F-POST-08.a.1 | - |
| F-POST-09.a.1 | requires_all | F-CAL-06.b | - |
| F-POST-09.a.1 | requires_all | C-12 | - |
| F-POST-09.a.2 | requires_all | F-POST-09.a.1 | - |
| F-POST-09.a.2 | requires_all | F-POST-08.a.2 | - |
| F-POST-09.b | requires_all | F-POST-08.b | - |
| F-POST-10.a.1 | requires_all | F-POST-08.a.1 | - |
| F-POST-10.a.1 | requires_all | F-REG-01 | - |
| F-POST-10.a.1 | requires_all | F-CAL-06.b | - |
| F-POST-10.a.1 | requires_all | C-15 | - |
| F-POST-10.a.1 | decided_by | D-19 | - |
| F-POST-10.a.2 | requires_all | F-POST-10.a.1 | - |
| F-POST-10.a.2 | requires_all | F-POST-08.a.2 | - |
| F-POST-10.b | requires_all | F-POST-08.b | - |
| F-POST-10.b | requires_all | F-REG-01 | - |
| F-POST-11.a.1 | requires_all | F-POST-08.a.1 | - |
| F-POST-11.a.1 | requires_all | C-13 | - |
| F-POST-11.a.2 | requires_all | F-POST-11.a.1 | - |
| F-POST-11.a.2 | requires_all | F-POST-08.a.2 | - |
| F-POST-11.b | requires_all | F-POST-11.a.1 | - |
| F-POST-12.a.1 | requires_all | F-POST-08.a.1 | - |
| F-POST-12.a.1 | requires_all | F-REG-01 | - |
| F-POST-12.a.1 | requires_all | C-14 | - |
| F-POST-12.a.2 | requires_all | F-POST-12.a.1 | - |
| F-POST-12.a.2 | requires_all | F-POST-08.a.2 | - |
| F-POST-12.b.1 | requires_all | F-POST-12.a.1 | - |
| F-POST-12.b.2 | requires_all | F-POST-12.b.1 | - |
| F-POST-12.b.2 | requires_all | F-POST-08.a.2 | - |
| F-POST-12.c | requires_all | F-POST-08.b | - |
| F-POST-12.c | requires_all | F-POST-12.b.1 | - |
| F-POST-13 | requires_all | F-POST-04.a | - |
| F-POST-13 | requires_all | F-POST-08.b | - |
| F-POST-14.a | requires_all | F-POST-01 | - |
| F-POST-14.a | requires_all | F-ANA-03.a | - |
| F-POST-14.b | requires_all | F-POST-14.a | - |
| F-POST-15 | requires_all | F-POST-12.c | - |
| F-POST-16 | requires_all | F-STK-08 | - |
| F-POST-16 | requires_all | F-POST-06.b | - |
| F-POST-16 | requires_all | F-POST-07 | - |
| F-POST-09.a.1 | verified_by | T-POST-09 | - |
| F-POST-10.a.1 | verified_by | T-POST-10 | - |
| F-POST-12.a.1 | verified_by | T-POST-12 | - |
| F-POST-12.b.1 | verified_by | T-POST-12 | - |
| F-UX-01 | requires_all | F-IO-03 | - |
| F-UX-01 | requires_all | F-PRJ-01 | - |
| F-UX-02 | requires_all | F-POST-01 | - |
| F-UX-03 | requires_all | F-PRJ-03 | - |
| F-UX-03 | requires_all | F-RES-03 | - |
| F-UX-04 | requires_all | F-RES-03 | - |
| F-UX-05 | requires_all | F-UX-01 | - |
| F-UX-05 | requires_all | F-UX-02 | - |
| F-UX-06.a | requires_all | F-UX-01 | - |
| F-UX-06.a | requires_all | F-UX-02 | - |
| F-UX-06.a | requires_all | F-POST-14.a | - |
| F-UX-06.b | requires_all | F-POST-08.b | - |
| F-UX-07 | requires_all | F-UX-01 | - |
| F-UX-08 | requires_all | F-UX-05 | - |
| F-UX-09 | requires_all | F-UX-06.b | - |
| F-UX-09 | requires_all | F-PRJ-08 | - |
| F-RES-01 | decided_by | ADR-06.a | - |
| F-RES-02 | decided_by | ADR-06.a | - |
| F-RES-03 | requires_all | F-RES-01 | - |
| F-RES-03 | requires_all | F-RES-02 | - |
| F-RES-04 | requires_all | F-RES-03 | - |
| F-RES-05.a | requires_all | F-RES-01 | - |
| F-RES-05.a | requires_all | F-RES-02 | - |
| F-RES-05.b | requires_all | F-RES-05.a | - |
| F-RES-06.a | requires_all | F-RES-03 | - |
| F-RES-06.b | requires_all | F-RES-06.a | - |
| F-RES-07.a | requires_all | F-RES-03 | - |
| F-RES-07.b | requires_all | F-RES-07.a | - |
| F-RES-08.a | requires_all | F-RES-03 | - |
| F-RES-08.b | requires_all | F-RES-08.a | - |
| F-ANA-01.a | requires_all | F-IO-04 | - |
| F-ANA-01.a | requires_all | F-CAL-06.b | - |
| F-ANA-01.a | requires_all | F-REG-01 | - |
| F-ANA-01.a | requires_all | F-STK-02 | - |
| F-ANA-01.a | requires_all | F-POST-06.a.1 | - |
| F-ANA-01.b | requires_all | F-ANA-01.a | - |
| F-ANA-01.b | requires_all | F-POST-01 | - |
| F-ANA-02.a | requires_all | F-ANA-01.a | - |
| F-ANA-02.a | requires_all | F-PRJ-07 | - |
| F-ANA-02.b | requires_all | F-ANA-02.a | - |
| F-ANA-02.b | requires_all | F-UX-01 | - |
| F-ANA-03.a | requires_all | F-ANA-02.a | - |
| F-ANA-03.a | requires_all | F-PRJ-05.a.1 | - |
| F-ANA-03.b | requires_all | F-ANA-03.a | - |
| F-ANA-03.b | requires_all | F-POST-14.a | - |
| F-ANA-03.b | requires_all | F-UX-06.a | - |
| F-ANA-04.a | requires_all | F-ANA-03.a | - |
| F-ANA-04.a | requires_all | F-ANA-05.a | - |
| F-ANA-04.a | requires_all | F-RES-05.a | - |
| F-ANA-04.a | requires_all | F-RES-06.a | - |
| F-ANA-04.a | requires_all | F-RES-07.a | - |
| F-ANA-04.a | requires_all | F-RES-08.a | - |
| F-ANA-04.a | requires_all | F-POST-03.a | - |
| F-ANA-04.a | requires_all | F-POST-09.a.1 | - |
| F-ANA-04.a | requires_all | F-POST-11.a.1 | - |
| F-ANA-04.a | requires_all | F-POST-12.b.1 | - |
| F-ANA-04.a | requires_all | C-17 | - |
| F-ANA-04.a | decided_by | ADR-15 | - |
| F-ANA-04.a | decided_by | ADR-06.b | - |
| F-ANA-04.a | decided_by | ADR-05.b | - |
| F-ANA-04.b | requires_all | F-ANA-04.a | - |
| F-ANA-04.b | requires_all | F-ANA-03.b | - |
| F-ANA-05.a | requires_all | F-ANA-01.a | - |
| F-ANA-05.a | requires_all | NFR-TEST-01.a | - |
| F-ANA-05.a | requires_all | NFR-TEST-02 | - |
| F-ANA-05.a | requires_all | C-19 | - |
| F-ANA-05.b | requires_all | F-ANA-05.a | - |
| F-ANA-05.b | requires_all | F-ANA-03.b | - |
| F-ANA-06 | requires_all | F-ANA-02.b | - |
| F-ANA-06 | requires_all | F-ANA-04.b | - |
| F-ANA-06 | requires_all | F-UX-05 | - |
| F-ANA-01.a | verified_by | T-ANA-01 | - |
| F-ANA-04.a | verified_by | T-ANA-04 | - |
| F-ANA-05.a | verified_by | T-VAL-INDEP | - |

| NFR-EXP-01 | contains | NFR-EXP-01.a | - |
| NFR-EXP-01 | contains | NFR-EXP-01.b | - |
| NFR-EXP-01 | contains | NFR-EXP-01.c | - |
| NFR-EXP-01 | contains | NFR-EXP-01.d | - |
| NFR-REP-01 | contains | NFR-REP-01.a | - |
| NFR-REP-01 | contains | NFR-REP-01.b | - |
| NFR-REP-01 | contains | NFR-REP-01.c | - |
| NFR-REP-01 | contains | NFR-REP-01.d | - |
| NFR-LINUX-01 | contains | NFR-LINUX-01.a | - |
| NFR-LINUX-01 | contains | NFR-LINUX-01.b | - |
| NFR-LINUX-03 | contains | NFR-LINUX-03.a | - |
| NFR-LINUX-03 | contains | NFR-LINUX-03.b | - |
| NFR-LINUX-03 | contains | NFR-LINUX-03.c | - |
| NFR-CPU-01 | contains | NFR-CPU-01.a | - |
| NFR-CPU-01 | contains | NFR-CPU-01.b | - |
| NFR-CPU-01 | contains | NFR-CPU-01.c | - |
| NFR-CPU-01 | contains | NFR-CPU-01.d | - |
| NFR-OFF-01 | contains | NFR-OFF-01.a | - |
| NFR-OFF-01 | contains | NFR-OFF-01.b | - |
| NFR-OFF-01 | contains | NFR-OFF-01.c | - |
| NFR-OFF-01 | contains | NFR-OFF-01.d | - |
| NFR-LIC-01 | contains | NFR-LIC-01.a | - |
| NFR-LIC-01 | contains | NFR-LIC-01.b | - |
| NFR-LIC-01 | contains | NFR-LIC-01.c | - |
| NFR-LIC-01 | contains | NFR-LIC-01.d | - |
| NFR-LIC-01 | contains | NFR-LIC-01.e | - |
| NFR-TEST-01 | contains | NFR-TEST-01.a | - |
| NFR-TEST-01 | contains | NFR-TEST-01.b | - |
| NFR-TEST-01 | contains | NFR-TEST-01.c | - |
| NFR-TEST-01 | contains | NFR-TEST-01.d | - |
| NFR-DATA-01 | contains | NFR-DATA-01.a | - |
| NFR-DATA-01 | contains | NFR-DATA-01.b | - |
| NFR-DATA-01 | contains | NFR-DATA-01.c | - |
| NFR-DATA-01 | contains | NFR-DATA-01.d | - |
| NFR-FBK-01 | contains | NFR-FBK-01.a | - |
| NFR-FBK-01 | contains | NFR-FBK-01.b | - |
| NFR-FBK-01 | contains | NFR-FBK-01.c | - |
| NFR-FBK-01 | contains | NFR-FBK-01.d | - |
| NFR-UX-01 | requires_all | F-ANA-06 | - |
| NFR-EXP-01.a | requires_all | F-IO-01 | - |
| NFR-EXP-01.b | requires_all | NFR-EXP-01.a | - |
| NFR-EXP-01.b | requires_all | C-10.b | - |
| NFR-EXP-01.b | requires_all | F-PRJ-07 | - |
| NFR-EXP-01.c | requires_all | NFR-EXP-01.b | - |
| NFR-EXP-01.c | requires_all | F-UX-01 | - |
| NFR-EXP-01.d | requires_all | NFR-EXP-01.c | - |
| NFR-REP-01.a | requires_all | F-CAL-06.a | - |
| NFR-REP-01.a | requires_all | F-PRJ-07 | - |
| NFR-REP-01.b | requires_all | F-ANA-04.a | - |
| NFR-REP-01.b | requires_all | NFR-REP-01.a | - |
| NFR-REP-01.c | requires_all | NFR-REP-01.b | - |
| NFR-REP-01.c | requires_all | F-ANA-06 | - |
| NFR-REP-01.d | requires_all | NFR-REP-01.c | - |
| NFR-LINUX-01.a | decided_by | D-07 | - |
| NFR-LINUX-01.b | requires_all | NFR-LINUX-01.a | - |
| NFR-LINUX-01.b | requires_all | NFR-PKG-02 | - |
| NFR-LINUX-02 | requires_all | M4.g | - |
| NFR-LINUX-03.a | decided_by | D-07 | - |
| NFR-LINUX-03.b | requires_all | NFR-LINUX-03.a | - |
| NFR-LINUX-03.c | requires_all | NFR-LINUX-03.b | - |
| NFR-LINUX-03.c | requires_all | M4.g | - |
| NFR-CPU-01.a | requires_all | F-CAL-06.a | - |
| NFR-CPU-01.a | decided_by | ADR-07.a | - |
| NFR-CPU-01.b | requires_all | NFR-CPU-01.a | - |
| NFR-CPU-01.b | requires_all | M3.r | - |
| NFR-CPU-01.c | requires_all | NFR-CPU-01.b | - |
| NFR-CPU-01.c | requires_all | F-UX-02 | - |
| NFR-CPU-01.d | requires_all | NFR-CPU-01.c | - |
| NFR-GPU-01 | decided_by | ADR-07.b | - |
| NFR-GPU-01 | requires_all | NFR-CPU-01.d | - |
| NFR-SIMD-01 | decided_by | ADR-07.a | - |
| NFR-SIMD-01 | requires_all | F-CAL-06.a | - |
| NFR-OFF-01.a | decided_by | ADR-13.a | - |
| NFR-OFF-01.a | requires_all | M2.e | - |
| NFR-OFF-01.b | requires_all | NFR-OFF-01.a | - |
| NFR-OFF-01.b | requires_all | M3.r | - |
| NFR-OFF-01.c | requires_all | NFR-OFF-01.b | - |
| NFR-OFF-01.c | requires_all | F-ANA-06 | - |
| NFR-OFF-01.d | requires_all | NFR-OFF-01.c | - |
| NFR-PKG-01 | decided_by | ADR-12.b | - |
| NFR-PKG-02 | requires_all | NFR-PKG-01 | - |
| NFR-PKG-02 | requires_all | M4.g | - |
| NFR-COMP-01 | requires_all | NFR-TEST-03 | - |
| NFR-SEC-01 | decided_by | ADR-08.a | - |
| NFR-SEC-02 | decided_by | ADR-05.a | - |
| NFR-SEC-03 | decided_by | ADR-04.a | - |
| NFR-SEC-04 | requires_all | F-IO-02 | - |
| NFR-LIC-01.a | decided_by | D-02 | - |
| NFR-LIC-01.b | requires_all | NFR-LIC-01.a | - |
| NFR-LIC-01.c | requires_all | NFR-LIC-01.b | - |
| NFR-LIC-01.d | requires_all | NFR-LIC-01.c | - |
| NFR-LIC-01.d | decided_by | D-21 | - |
| NFR-LIC-01.e | requires_all | NFR-LIC-01.d | - |
| NFR-LIC-01.e | requires_all | M4.g | - |
| NFR-TEST-01.a | decided_by | ADR-01 | - |
| NFR-TEST-01.b | requires_all | NFR-TEST-01.a | - |
| NFR-TEST-01.b | requires_all | M3.r | - |
| NFR-TEST-01.c | requires_all | NFR-TEST-01.b | - |
| NFR-TEST-01.c | requires_all | F-ANA-06 | - |
| NFR-TEST-01.d | requires_all | NFR-TEST-01.c | - |
| NFR-TEST-02 | decided_by | D-08 | - |
| NFR-TEST-03 | requires_all | M3.r | - |
| NFR-PERF-01 | decided_by | ADR-06.a | - |
| NFR-DATA-01.a | requires_all | F-PRJ-06 | - |
| NFR-DATA-01.b | requires_all | NFR-DATA-01.a | - |
| NFR-DATA-01.b | requires_all | NFR-EXP-01.b | - |
| NFR-DATA-01.c | requires_all | NFR-DATA-01.b | - |
| NFR-DATA-01.c | requires_all | F-PRJ-05.a.2 | - |
| NFR-DATA-01.d | requires_all | NFR-DATA-01.c | - |
| NFR-FBK-01.a | requires_all | F-RES-03 | - |
| NFR-FBK-01.b | requires_all | NFR-FBK-01.a | - |
| NFR-FBK-01.b | requires_all | M3.r | - |
| NFR-FBK-01.c | requires_all | NFR-FBK-01.b | - |
| NFR-FBK-01.c | requires_all | F-ANA-06 | - |
| NFR-FBK-01.d | requires_all | NFR-FBK-01.c | - |
| NFR-FBK-01.d | verified_by | T-FBK-01 | - |
| ADR-03 | contains | ADR-03.a | - |
| ADR-03 | contains | ADR-03.b | - |
| ADR-04 | contains | ADR-04.a | - |
| ADR-04 | contains | ADR-04.b | - |
| ADR-05 | contains | ADR-05.a | - |
| ADR-05 | contains | ADR-05.b | - |
| ADR-06 | contains | ADR-06.a | - |
| ADR-06 | contains | ADR-06.b | - |
| ADR-07 | contains | ADR-07.a | - |
| ADR-07 | contains | ADR-07.b | - |
| ADR-08 | contains | ADR-08.a | - |
| ADR-08 | contains | ADR-08.b | - |
| ADR-09 | contains | ADR-09.a | - |
| ADR-09 | contains | ADR-09.b | - |
| ADR-12 | contains | ADR-12.a | - |
| ADR-12 | contains | ADR-12.b | - |
| ADR-13 | contains | ADR-13.a | - |
| ADR-13 | contains | ADR-13.b | - |
| ADR-14 | contains | ADR-14.a | - |
| ADR-14 | contains | ADR-14.b | - |
| ADR-16 | contains | ADR-16.a | - |
| ADR-16 | contains | ADR-16.b | - |
| ADR-17 | contains | ADR-17.a | - |
| ADR-17 | contains | ADR-17.b | - |
| C-01 | contains | C-01.a | - |
| C-01 | contains | C-01.b | - |
| C-02 | contains | C-02.a | - |
| C-02 | contains | C-02.b | - |
| C-10 | contains | C-10.a | - |
| C-10 | contains | C-10.b | - |
| D-09 | contains | D-09.a | - |
| D-09 | contains | D-09.b | - |
| D-17 | contains | D-17.a | - |
| D-17 | contains | D-17.b | - |
| ADR-01 | decided_by | D-03 | - |
| ADR-02 | decided_by | D-04 | - |
| ADR-03.a | requires_all | NFR-LIC-01.c | - |
| ADR-03.b | requires_all | ADR-03.a | - |
| ADR-04.a | decided_by | D-05 | - |
| ADR-04.b | requires_all | ADR-04.a | - |
| ADR-05.b | requires_all | ADR-05.a | - |
| ADR-06.b | requires_all | ADR-06.a | - |
| ADR-07.b | decided_by | D-10 | - |
| ADR-08.a | requires_all | NFR-LIC-01.c | - |
| ADR-08.b | requires_all | ADR-08.a | - |
| ADR-08.b | requires_all | NFR-LIC-01.d | - |
| ADR-09.a | decided_by | D-09.a | - |
| ADR-09.b | requires_all | ADR-09.a | - |
| ADR-09.b | decided_by | D-09.b | - |
| ADR-11 | decided_by | D-12 | - |
| ADR-12.a | decided_by | D-02 | - |
| ADR-12.b | requires_all | ADR-12.a | - |
| ADR-12.b | decided_by | D-06 | - |
| ADR-13.b | requires_all | ADR-13.a | - |
| ADR-13.b | decided_by | D-11 | - |
| ADR-14.b | requires_all | ADR-14.a | - |
| ADR-14.b | decided_by | D-13 | - |
| ADR-15 | requires_all | ADR-05.b | - |
| ADR-15 | requires_all | ADR-06.b | - |
| ADR-16.b | requires_all | ADR-16.a | - |
| ADR-16.b | decided_by | D-16 | - |
| ADR-17.a | decided_by | D-17.a | - |
| ADR-17.b | requires_all | ADR-17.a | - |
| ADR-17.b | decided_by | D-17.b | - |
| C-01.a | decided_by | ADR-09.a | - |
| C-01.a | decided_by | ADR-16.a | - |
| C-01.b | requires_all | C-01.a | - |
| C-01.b | decided_by | ADR-16.b | - |
| C-01.b | decided_by | ADR-09.b | - |
| C-02.a | decided_by | ADR-09.a | - |
| C-02.b | requires_all | C-02.a | - |
| C-02.b | decided_by | ADR-09.b | - |
| C-03 | decided_by | ADR-09.b | - |
| C-04 | decided_by | ADR-09.b | - |
| C-05 | decided_by | ADR-09.b | - |
| C-06 | decided_by | ADR-17.b | - |
| C-07 | decided_by | ADR-09.b | - |
| C-08 | decided_by | ADR-09.b | - |
| C-09 | decided_by | ADR-10 | - |
| C-10.a | decided_by | ADR-08.a | - |
| C-10.b | requires_all | C-10.a | - |
| C-10.b | decided_by | ADR-08.b | - |
| C-11 | decided_by | ADR-03.b | - |
| C-12 | decided_by | ADR-09.b | - |
| C-13 | decided_by | ADR-09.b | - |
| C-14 | decided_by | ADR-09.b | - |
| C-15 | decided_by | ADR-09.b | - |
| C-15 | decided_by | D-19 | - |
| C-16 | decided_by | ADR-10 | - |
| C-16 | decided_by | D-20 | - |
| C-17 | decided_by | ADR-15 | - |
| C-18 | decided_by | ADR-08.b | - |
| C-18 | decided_by | D-20 | - |
| C-19 | decided_by | ADR-15 | - |
| M1 | contains | M1.a | - |
| M1 | contains | M1.b | - |
| M1 | contains | M1.c | - |
| M1 | contains | M1.d | - |
| M1 | contains | M1.e | - |
| M1 | contains | M1.f | - |
| M2 | contains | M2.a | - |
| M2 | contains | M2.b | - |
| M2 | contains | M2.c | - |
| M2 | contains | M2.d | - |
| M2 | contains | M2.e | - |
| M2 | contains | M2.f | - |
| M2 | contains | M2.g | - |
| M3 | contains | M3.a | - |
| M3 | contains | M3.b | - |
| M3 | contains | M3.c | - |
| M3 | contains | M3.d | - |
| M3 | contains | M3.e | - |
| M3 | contains | M3.f | - |
| M3 | contains | M3.g | - |
| M3 | contains | M3.h | - |
| M3 | contains | M3.i | - |
| M3 | contains | M3.j | - |
| M3 | contains | M3.k | - |
| M3 | contains | M3.l | - |
| M3 | contains | M3.m | - |
| M3 | contains | M3.n | - |
| M3 | contains | M3.o | - |
| M3 | contains | M3.p | - |
| M3 | contains | M3.q | - |
| M3 | contains | M3.r | - |
| M3 | contains | M3.s | - |
| M4 | contains | M4.a | - |
| M4 | contains | M4.b | - |
| M4 | contains | M4.c | - |
| M4 | contains | M4.d | - |
| M4 | contains | M4.e | - |
| M4 | contains | M4.f | - |
| M4 | contains | M4.g | - |
| M6 | contains | M6.a | - |
| M6 | contains | M6.b | - |
| M6 | contains | M6.c | - |
| M6 | contains | M6.d | - |
| M6 | contains | M6.e | - |
| M6 | contains | M6.f | - |
| M6 | contains | M6.g | - |
| M6 | contains | M6.h | - |
| M6 | contains | M6.i | - |
| M9 | contains | M9.a | - |
| M9 | contains | M9.b | - |
| M9 | contains | M9.c | - |
| M9 | contains | M9.d | - |
| M9 | contains | M9.e | - |
| M9 | contains | M9.f | - |
| M1.a | requires_all | M0 | - |
| M1.b | requires_all | M0 | - |
| M1.c | requires_all | M0 | - |
| M1.d | requires_all | M0 | - |
| M1.e | requires_all | M0 | - |
| M1.f | requires_all | M0 | - |
| M1.a | requires_all | NFR-LIC-01.b | - |
| M1.b | requires_all | NFR-LIC-01.b | - |
| M1.c | requires_all | NFR-LIC-01.b | - |
| M1.d | requires_all | NFR-LIC-01.b | - |
| M1.e | requires_all | NFR-LIC-01.b | - |
| M1.f | requires_all | NFR-LIC-01.b | - |
| M1 | requires_all | M1.a | - |
| M1 | requires_all | M1.b | - |
| M1 | requires_all | M1.c | - |
| M1 | requires_all | M1.d | - |
| M1 | requires_all | M1.e | - |
| M1 | requires_all | M1.f | - |
| M2.g | requires_all | M1 | - |
| M2.g | decided_by | ADR-03.a | - |
| M2.g | decided_by | ADR-04.a | - |
| M2.g | decided_by | ADR-05.a | - |
| M2.g | decided_by | ADR-06.a | - |
| M2.g | decided_by | ADR-08.a | - |
| M2.g | decided_by | ADR-09.a | - |
| M2.g | decided_by | ADR-16.a | - |
| M2.g | decided_by | ADR-17.a | - |
| M2.f | requires_all | M2.g | - |
| M2.f | requires_all | NFR-TEST-01.a | - |
| M2.a | requires_all | M2.f | - |
| M2.a | requires_all | F-IO-01 | - |
| M2.b | requires_all | M2.a | - |
| M2.b | requires_all | F-PRJ-03 | - |
| M2.c | requires_all | M2.b | - |
| M2.c | requires_all | F-PRJ-06 | - |
| M2.d | requires_all | M2.c | - |
| M2.d | requires_all | F-RES-08.a | - |
| M2.e | requires_all | M2.d | - |
| M2.e | requires_all | F-CAL-06.a | - |
| M2 | requires_all | M2.e | - |
| M2 | requires_all | NFR-DATA-01.a | - |
| M2 | requires_all | NFR-FBK-01.a | - |
| M2 | requires_all | NFR-OFF-01.a | - |
| M3.a | requires_all | M2 | - |
| M3.a | requires_all | F-IO-03 | - |
| M3.b | requires_all | M3.a | - |
| M3.b | requires_all | F-CAL-01.b | - |
| M3.c | requires_all | M3.b | - |
| M3.c | requires_all | F-CAL-06.b | - |
| M3.d | requires_all | M3.c | - |
| M3.d | requires_all | F-CAL-08 | - |
| M3.e | requires_all | M3.d | - |
| M3.e | requires_all | F-REG-01 | - |
| M3.f | requires_all | M3.e | - |
| M3.f | requires_all | F-REG-02 | - |
| M3.g | requires_all | M3.f | - |
| M3.g | requires_all | F-REG-03 | - |
| M3.h | requires_all | M3.g | - |
| M3.h | requires_all | F-STK-03 | - |
| M3.k | requires_all | M3.h | - |
| M3.k | requires_all | F-POST-06.a.1 | - |
| M3.j | requires_all | M3.k | - |
| M3.j | requires_all | F-POST-08.a.1 | - |
| M3.i | requires_all | M3.j | - |
| M3.i | requires_all | F-POST-03.a | - |
| M3.l | requires_all | M3.i | - |
| M3.l | requires_all | F-POST-09.a.1 | - |
| M3.m | requires_all | M3.i | - |
| M3.m | requires_all | F-POST-11.a.1 | - |
| M3.n | requires_all | M3.i | - |
| M3.n | requires_all | F-POST-12.b.1 | - |
| M3.o | requires_all | M3.i | - |
| M3.o | requires_all | F-POST-10.a.1 | - |
| M3.p | requires_all | M3.g | - |
| M3.p | requires_all | F-REG-04.a | - |
| M3.q | requires_all | M3.i | - |
| M3.q | requires_all | F-POST-05.a.1 | - |
| M3.q | requires_when | M3.p | P-SOLVE |
| M3.r | requires_all | M3.l | - |
| M3.r | requires_all | M3.m | - |
| M3.r | requires_all | M3.n | - |
| M3.r | requires_all | F-ANA-04.a | - |
| M3.r | requires_when | M3.o | P-INCLUDE-DECONV |
| M3.r | requires_when | M3.p | P-INCLUDE-COLOR |
| M3.r | requires_when | M3.q | P-INCLUDE-COLOR |
| M3.s | requires_all | M3.r | - |
| M3.s | requires_all | NFR-EXP-01.b | - |
| M3.s | requires_all | NFR-REP-01.b | - |
| M3.s | requires_all | NFR-TEST-01.b | - |
| M3.s | requires_all | NFR-CPU-01.b | - |
| M3.s | requires_all | NFR-OFF-01.b | - |
| M3.s | requires_all | NFR-FBK-01.b | - |
| M3 | requires_all | M3.s | - |
| M4.a | requires_all | M3 | - |
| M4.a | requires_all | F-UX-02 | - |
| M4.a | decided_by | ADR-02 | - |
| M4.b | requires_all | M4.a | - |
| M4.b | requires_all | F-PRJ-05.a.2 | - |
| M4.e | requires_all | M4.b | - |
| M4.e | requires_all | F-POST-08.a.2 | - |
| M4.f | requires_all | M4.e | - |
| M4.f | requires_all | F-POST-02.b | - |
| M4.f | requires_all | F-POST-03.b | - |
| M4.f | requires_all | F-POST-04.b | - |
| M4.f | requires_all | F-POST-06.a.2 | - |
| M4.f | requires_all | F-POST-09.a.2 | - |
| M4.f | requires_all | F-POST-11.a.2 | - |
| M4.f | requires_all | F-POST-12.a.2 | - |
| M4.f | requires_all | F-POST-12.b.2 | - |
| M4.f | requires_when | F-POST-10.a.2 | P-INCLUDE-DECONV |
| M4.f | requires_when | F-POST-05.a.2 | P-INCLUDE-COLOR |
| M4.d | requires_all | M4.f | - |
| M4.d | requires_all | F-ANA-04.b | - |
| M4.d | requires_all | F-ANA-05.b | - |
| M4.c | requires_all | M4.d | - |
| M4.c | requires_all | F-ANA-06 | - |
| M4.c | decided_by | D-15 | - |
| M4.g | requires_all | M4.c | - |
| M4.g | requires_all | F-UX-07 | - |
| M4.g | requires_all | F-UX-04 | - |
| M4.g | requires_all | NFR-TEST-01.c | - |
| M4.g | requires_all | NFR-EXP-01.c | - |
| M4.g | requires_all | NFR-DATA-01.c | - |
| M4.g | requires_all | NFR-OFF-01.c | - |
| M4.g | requires_all | NFR-CPU-01.c | - |
| M4.g | requires_all | NFR-FBK-01.c | - |
| M4 | requires_all | M4.g | - |
| M5 | requires_all | M4.g | - |
| M5 | requires_all | NFR-EXP-01.d | - |
| M5 | requires_all | NFR-REP-01.d | - |
| M5 | requires_all | NFR-CPU-01.d | - |
| M5 | requires_all | NFR-OFF-01.d | - |
| M5 | requires_all | NFR-DATA-01.d | - |
| M5 | requires_all | NFR-FBK-01.d | - |
| M5 | requires_all | NFR-TEST-01.d | - |
| M5 | requires_all | NFR-LIC-01.e | - |
| M5 | requires_all | NFR-LINUX-01.b | - |
| M5 | requires_all | NFR-LINUX-02 | - |
| M5 | requires_all | NFR-LINUX-03.c | - |
| M5 | requires_all | NFR-COMP-01 | - |
| M5 | requires_all | NFR-SEC-04 | - |
| M5 | requires_all | NFR-PKG-02 | - |
| M5 | requires_when | F-POST-10.a.2 | P-INCLUDE-DECONV |
| M5 | requires_when | F-POST-05.a.2 | P-INCLUDE-COLOR |
| M6.a | requires_all | M5 | - |
| M6.b | requires_all | M6.a | - |
| M6.c | requires_all | M6.f | - |
| M6.d | requires_all | M5 | - |
| M6.e | requires_all | M5 | - |
| M6.f | requires_all | M6.a | - |
| M6.g | requires_all | M6.d | - |
| M6.h | requires_all | M6.d | - |
| M6.i | requires_all | M6.d | - |
| M6 | requires_all | M6.b | - |
| M6 | requires_all | M6.c | - |
| M6 | requires_all | M6.e | - |
| M6 | requires_all | M6.g | - |
| M6 | requires_all | M6.h | - |
| M6 | requires_all | M6.i | - |
| M7 | requires_all | M5 | - |
| M7 | requires_all | F-PRJ-05.b | - |
| M7 | requires_all | NFR-GPU-01 | - |
| M8 | requires_all | M6 | - |
| M8 | requires_all | F-POST-15 | - |
| M8 | decided_by | ADR-13.b | - |
| M9.a | requires_all | M6 | - |
| M9.b | requires_all | M6 | - |
| M9.c | requires_all | M6 | - |
| M9.d | requires_all | M6 | - |
| M9.e | requires_all | M6 | - |
| M9.f | requires_all | M6 | - |
| M9 | requires_all | M9.a | - |
| M9 | requires_all | M9.b | - |
| M9 | requires_all | M9.c | - |
| M9 | requires_all | M9.d | - |
| M9 | requires_all | M9.e | - |
| M9 | requires_all | M9.f | - |
| M10 | requires_all | M5 | - |
| G-CAL | requires_all | F-IO-04 | - |
| G-CAL | decided_by | ADR-16.b | - |
| G-OFFSET | requires_all | F-IO-04 | - |
| G-PACK | requires_all | NFR-LIC-01.d | - |
| G-WCS | requires_all | F-REG-03 | - |
| G-NOISE-L | requires_all | F-REG-01 | - |
| G-NOISE-NL | requires_all | F-POST-06.a.1 | - |
| G-NOISE-NL | requires_all | G-NOISE-L | - |
| G-PSF | requires_all | F-REG-01 | - |
| G-STOP | requires_all | NFR-FBK-01.a | - |
| G-MASK | requires_all | F-POST-08.a.1 | - |
| F-CAL-02.b | requires_when | G-CAL | P-DFB000 |
| F-CAL-02.b | requires_when | G-CAL | P-DFB001 |
| F-CAL-02.b | requires_when | G-CAL | P-DFB010 |
| F-CAL-02.b | requires_when | G-CAL | P-DFB011 |
| F-CAL-02.b | requires_when | G-CAL | P-DFB100 |
| F-CAL-02.b | requires_when | G-CAL | P-DFB101 |
| F-CAL-02.b | requires_when | G-CAL | P-DFB110 |
| F-CAL-02.b | requires_when | G-CAL | P-DFB111 |
| F-CAL-03.b | requires_when | G-CAL | P-DF-ABS |
| F-CAL-03.b | requires_when | G-CAL | P-DF-INC |
| F-CAL-03.b | requires_when | G-CAL | P-DF-SUB |
| F-CAL-03.b | requires_when | G-CAL | P-DF-BAD |
| F-CAL-03.b | requires_when | G-CAL | P-FLAT-RAW |
| F-CAL-03.b | requires_when | G-CAL | P-FLAT-Q |
| F-CAL-03.b | requires_when | G-STOP | P-FLAT-UNKNOWN |
| F-CAL-02.b | requires_when | G-OFFSET | P-OFFSET |
| F-CAL-02.b | requires_when | G-STOP | P-LOST |
| F-REG-04.a | requires_when | G-PACK | P-SOLVE |
| F-REG-04.a | requires_when | G-STOP | P-PACK-MISSING |
| F-POST-05.a.1 | requires_all | G-PACK | - |
| F-POST-05.a.1 | requires_when | F-REG-04.a | P-SOLVE |
| F-POST-05.a.1 | requires_when | G-WCS | P-IMPORT |
| F-POST-05.a.1 | requires_when | G-STOP | P-PACK-MISSING |
| F-POST-09.a.1 | requires_when | G-NOISE-L | P-NOISE-L |
| F-POST-09.a.1 | requires_when | G-NOISE-NL | P-NOISE-NL |
| F-POST-10.a.1 | requires_when | G-PSF | P-DECONV |
| F-POST-10.a.1 | requires_when | G-NOISE-L | P-DECONV |
| F-POST-10.a.1 | requires_when | F-RES-07.a | P-DECONV |
| F-POST-08.a.1 | requires_when | F-REG-01 | P-STAR-MASK |
| F-ANA-04.a | requires_when | G-MASK | P-REORDER |
| F-ANA-04.a | requires_when | G-STOP | P-LOST |
| F-ANA-04.a | requires_when | F-POST-10.a.1 | P-INCLUDE-DECONV |
| F-ANA-04.a | requires_when | F-POST-05.a.1 | P-INCLUDE-COLOR |
| M2.a | requires_all | F-IO-05 | - |
| M2.c | requires_all | F-IO-10 | - |
| M2.c | requires_all | F-PRJ-02 | - |
| M2.b | requires_all | NFR-SEC-02 | - |
| M2.c | requires_all | NFR-SEC-03 | - |
| M2.d | requires_all | F-RES-04 | - |
| M2.d | requires_all | NFR-PERF-01 | - |
| M2 | requires_all | NFR-SIMD-01 | - |
| M3.a | requires_all | F-IO-08 | - |
| M4.b | requires_all | F-UX-03 | - |
| M4.d | requires_all | F-ANA-01.b | - |
| M4.g | requires_all | NFR-UX-01 | - |
| M6.a | requires_all | F-IO-06 | - |
| M6.a | requires_all | F-REG-05 | - |
| M6.b | requires_all | F-POST-05.b | - |
| M6.c | requires_all | F-POST-07 | - |
| M6.d | requires_all | F-UX-06.b | - |
| M6.e | requires_all | F-STK-06 | - |
| M6.f | requires_all | F-IO-07 | - |
| M6.f | requires_all | F-CAL-05 | - |
| M6.f | requires_all | F-CAL-07 | - |
| M6.f | requires_all | F-STK-04 | - |
| M6.f | requires_all | F-STK-07 | - |
| M6.g | requires_all | F-POST-09.b | - |
| M6.h | requires_all | F-POST-10.b | - |
| M6.i | requires_all | F-POST-06.b | - |
| M6.i | requires_all | F-POST-11.b | - |
| M6.i | requires_all | F-POST-12.c | - |
| M6.i | requires_all | F-POST-13 | - |
| M6.i | requires_all | F-POST-14.b | - |
| M6.i | requires_all | F-UX-08 | - |
| M6.i | requires_all | F-RES-05.b | - |
| M6.i | requires_all | F-RES-06.b | - |
| M6.i | requires_all | F-RES-07.b | - |
| M6.i | requires_all | F-RES-08.b | - |
| F-UX-08 | decided_by | D-14 | - |
| M9.a | requires_all | F-POST-16 | - |
| M9.b | requires_all | F-IO-09 | - |
| M9.e | requires_all | F-STK-09 | - |
| M9.f | requires_all | F-REG-08 | - |
| M9.f | requires_all | F-UX-09 | - |

#### D.7 Stato della verifica del registro

Il controllo documentale Python in sola lettura della 2.2.1 ha contato 395 nodi, 847 archi e 27 predicati. ID dei nodi e dei predicati, tutti gli archi e le 54 sotto-milestone sono conservati. Nessun duplicato, estremo o predicato irrisolto; nessun ciclo nel contenimento, nell'unione dei prerequisiti e decisioni, né nella loro unione con il contenimento. Tutte le 133 foglie F/NFR P0 entro M5 sono raggiungibili dal gate M5 nell'unione degli archi di prerequisito, condizione e decisione. Questo controllo non esegue i predicati runtime e non prova la completezza fisica degli scenari.

Il verbale storico del PASS 2.2 è in 24.6; la review indipendente del delta 2.2.1 è PENDING. L'attivazione P-INCLUDE-DECONV/P-INCLUDE-COLOR è ora obbligatoria per il mandato, mentre selezione della ricetta e controlli runtime restano condizionali. Formule e stadi non modificati non ricevono una nuova attestazione scientifica. Le verifiche documentali non dimostrano funzionamento dell'applicazione.

---

## 27. Sintesi finale del contratto

Il piano definisce un percorso deep sky che parte dai dati disponibili, dichiara ciò che non può correggere e separa fotografia e misura. L'MVP conserva i 14 P0 ereditati e tutte le promozioni fotografiche, compresi deconvoluzione, colore relativo e solver assistito APPROVATI da D-19/D-20. D-21 approva la preferenza per pack inclusi; diritti e fattibilità restano da verificare.

Le regole che tengono insieme il documento sono poche:

- un dato assente non si inventa;
- un master presente non è un master compatibile;
- una maschera di editing non è una maschera di invalidità;
- una coverage non è una somma di pesi;
- un residuo piccolo non è una prova della PSF;
- un WCS risolto non è un colore calibrato;
- un controllo di regressione non è una validazione indipendente;
- un'astensione sistematica non è un successo;
- un candidato parziale non è un vincitore;
- un grafo aciclico non è una prova di completezza;
- un test non eseguito non è un risultato.

La review indipendente della 2.2.1 è PENDING; il PASS storico 2.2 non la sostituisce. Il mandato autorizza Linux M0..M9, spike e implementazione secondo dipendenze e gate reali, senza conferme rituali; M10 resta esclusa. Nessun fallback è ammesso nel prodotto: un percorso selezionato che fallisce si arresta senza sostituzioni. Lo sviluppatore corregge, ripete prove e review e prosegue autonomamente senza falsificare evidenze o ridurre criteri. Questa revisione documentale non dimostra un'app implementata né gate scientifici superati.
