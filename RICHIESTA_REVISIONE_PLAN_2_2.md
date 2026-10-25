# AstroProcessing — Revisione finale del Plan 2.2 e mandato operativo autonomo

**Documento di istruzioni impartite da me a Codex**  
**Riferimento:** `ASTRO_POSTPROCESSING_PLAN_v2.2.md` — 25 ottobre 2026  
**Ambito:** revisione mirata del piano, consolidamento delle decisioni e, una volta completato, esecuzione continua del progetto Linux  
**Priorità:** massima qualità scientifica, completezza dell'MVP, trasparenza degli errori, lavoro autonomo

## 1. Obiettivo e modalità di lavoro

Ho esaminato il Piano 2.2. Mantengo tutte le scelte fondamentali già fissate: motore indipendente da Siril, Linux come unica piattaforma iniziale, valutazione sperimentale del linguaggio e della GUI, RAM configurabile, pipeline non distruttiva, test scientifici, calibrazione con tutte le combinazioni dei master e Advisor capace di analizzare e confrontare strategie complete.

**Non voglio ricominciare la pianificazione da zero e non voglio ridurre l'MVP.** Il piano è molto articolato: apporta solo le modifiche necessarie e conserva come storico il documento originale. Non modificare criteri scientifici per aggirare gate falliti.

**Voglio anche un modo di lavorare preciso: dopo il consolidamento del piano devi procedere con il lavoro in autonomia, senza chiedermi approvazioni a ogni ADR, spike, test, sotto-milestone o milestone.** Non voglio essere il project manager operativo, il tester manuale o la persona incaricata di riavviare il lavoro dopo ogni checkpoint. Prendi decisioni tecniche motivate, esegui le prove, correggi gli errori, verifica il risultato e prosegui.

Questa istruzione, quando te la invio, costituisce il **mandato operativo per le fasi Linux M0–M9**, soggetto ai permessi effettivi degli strumenti e alle restrizioni di sicurezza del sistema. **Non autorizza M10 (Windows/macOS)**, né la pubblicazione commerciale, né la diffusione di segreti, né operazioni distruttive fuori dal progetto. Se la sessione o lo strumento termina, salva uno stato riprendibile; non dichiarare di poter lavorare in background quando non è possibile.

## 2. Decisioni D-19, D-20 e D-21: sciogliere il blocco

Nella sezione 23 scrivi ancora che restano tre decisioni da approvare e che nessuna implementazione è stata avviata. Non lasciare queste voci sospese dopo aver recepito le presenti istruzioni.

**D-19 — Deconvoluzione nell'MVP: APPROVO P0.** Voglio la deconvoluzione a PSF fissa descritta nel piano, con test su dati sintetici e reali, sensibilità a PSF/fondo/rumore/parametro di regolarizzazione, gestione corretta dei negativi, diagnosi e astensione dove la correzione non è giustificabile. Un gate fallito blocca quella funzione e il rilascio che la richiede: non comporta un declassamento automatico a P1.

**D-20 — Colore relativo e solver assistito nell'MVP: APPROVO P0 PER ENTRAMBI.** Voglio la calibrazione cromatica relativa broadband e il plate solving assistito offline nei domini dichiarati. I loro limiti devono essere visibili; non presentare il colore relativo come SPCC assoluta. Un WCS importato scelto prima della ricetta è un percorso primario valido, non una sostituzione automatica del solver fallito.

**D-21 — Catalog pack: APPROVO COME PREFERENZA PACK INCLUSI NELLA DISTRIBUZIONE**, purché la ridistribuzione sia legalmente consentita e integrità, dimensioni, versione, copertura e licenze siano verificate. Verifica tutto autonomamente. Se l'inclusione non è lecita o tecnicamente sostenibile, non distribuire materiale senza diritti, non simulare la presenza dei cataloghi e non scaricarli di nascosto: documenta il blocco, analizza un canale di installazione separato dichiarato e prepara una proposta alternativa motivata **senza sostituire a runtime il pack selezionato**. Prosegui sulle attività indipendenti. La preferenza per l'inclusione non autorizza violazioni delle licenze.

Aggiorna **tutte** le occorrenze di D-19, D-20, D-21 (stato, matrice P0, gate, dipendenze, roadmap, DoD, riepilogo, appendice D se interessata). Non lasciare sezioni che dicano contemporaneamente “approvato” e “in attesa del proprietario”.

## 3. Correzione principale: autonomia nello sviluppo, non richiesta ricorrente di permessi

Il Plan 2.2 contiene ancora molte formulazioni che mi richiedono di autorizzare separatamente ogni passaggio. Non è il flusso che voglio. Esempi concreti da correggere:

- **§0.8 punto 9 e §0.9:** approvazioni del proprietario e autorizzazione separata per la verifica del repository.
- **§4 NC-14 e §21.3:** consenso dedicato per ogni spike.
- **§20 e §23:** scelta personale del proprietario per quasi tutti gli ADR tecnici e le decisioni ordinarie.
- **§21.1–21.2 e §24:** ogni milestone può essere letta come obbligo di attendere un'altra mia conferma.
- **Ultimo paragrafo §27:** non autorizza implementazione o spike, pur avendo io dato adesso un mandato operativo esplicito.

Riscrivi le regole operative distinguendo rigorosamente:

1. **Decisioni tecniche delegabili:** linguaggio, GUI, formati interni, SQLite/JSON, strumenti, algoritmi, dataset di test leciti, metodi di benchmark, packaging Linux, gestione dei budget, organizzazione del codice e scelte di implementazione. Le prendi autonomamente *dopo* aver raccolto le evidenze richieste dagli ADR. Nessuna preferenza personale sostituisce benchmark e test.
2. **Gate automatici:** test scientifici, regressione, sicurezza, licenze, correttezza numerica, prestazioni, compatibilità e DoD. Li verifichi senza chiedere il mio parere. Se falliscono, correggi il problema e ripeti. Non dichiararli superati finché mancano le prove.
3. **Decisioni strategiche già vincolanti:** perimetro complesso dell'MVP, D-19 e D-20 P0, preferenza D-21, Linux soltanto, motore indipendente, nessun fallback, risorse configurabili. Non riaprire queste scelte semplicemente perché difficili.
4. **Questioni riservate o realmente bloccanti:** cambio di piattaforma a Windows/macOS, distribuzione commerciale, scelta di licenza del codice con conseguenze irreversibili, costi a pagamento non autorizzati, uso di credenziali non disponibili, modifica distruttiva di dati fuori progetto o permessi richiesti dalla piattaforma. Non aggirare i controlli reali. Se una di queste impedisce un ramo, isola il blocco, registra causa ed effetti e continua i rami indipendenti.

**Una milestone chiusa è un evento di avanzamento, non una domanda rivolta a me.** Dopo gate superato, aggiorna lo stato ed esegui la prossima attività dipendente. Non fermarti per fare un semplice riepilogo, non aspettare che ti dica ogni volta «continua».

## 4. Distinzione essenziale: stop dell'applicazione ≠ stop del lavoro di Codex

Ho chiesto un divieto rigoroso di fallback e lo confermo. Il requisito **NC-21 / NFR-FBK-01** è centrale:

- Se l'utente ha selezionato GPU e la GPU fallisce, l'app si ferma con errore esplicito; non usa CPU al suo posto.
- Se un algoritmo, decoder, formato, ICC, catalogo o master selezionato fallisce, nessuna alternativa è eseguita al suo posto.
- Non abbassare silenziosamente risoluzione, qualità, precisione o numero di candidati per rientrare nel budget.
- Un resume è valido solo con la ricetta e il backend esatti del checkpoint; un cambio genera una nuova ricetta, non una prosecuzione.
- La calibrazione senza alcuni master è una **ricetta primaria determinata prima dell'esecuzione**, non un fallback. La modalità automatica può pianificare ed esplicitare una ricetta corretta prima dell'avvio, senza sostituzioni dopo il guasto.

**Questo vincolo riguarda il comportamento del prodotto e i suoi dati, non la capacità di Codex di correggere bug nel proprio codice.** Se un test fallisce, devi indagare, modificare il codice, rieseguire il test e procedere. Non “risolvere” il test degradando un algoritmo, eliminando una fixture, disabilitando una funzione o cambiando il contratto scientifico. Se si tratta di una vera impossibilità scientifica o legale, segnala precisamente quale assunzione è fallita.

Aggiungi esplicitamente questa distinzione a §0.8, NC-21, NFR-FBK-01, roadmap e istruzioni operative, così che gli agenti non interpretino ogni errore di sviluppo come un obbligo di sospendere l'intero progetto.

## 5. Accesso agli strumenti, filesystem e agenti

Mantieni la protezione dei file personali e degli originali. Tuttavia **NC-01 non deve impedire artificiosamente l'impiego degli agenti specializzati e dei compilatori già disponibili**.

Ti autorizzo, nei limiti dei permessi reali, a consultare in sola lettura le informazioni *strettamente necessarie* per individuare e invocare strumenti e agenti configurati (metadati di capacità e configurazioni tecniche non segrete). Non leggere, esporre o copiare credenziali, token, chiavi private o dati personali per questo scopo. Mantieni build, fixture, cache configurabili, temporanei e file generati all'interno del workspace del progetto quando è tecnicamente possibile. Non modificare file o impostazioni di sistema per comodità. Se un'operazione necessita davvero di permessi ulteriori non disponibili, registrala come limite reale e prosegui nelle attività consentite.

Utilizza gli **agenti specializzati realmente configurati**, non semplicemente nominati nel piano. Identifica i loro strumenti e i modelli effettivamente accessibili. Assegna compiti secondo competenza: architettura, numerica, astronomia, prestazioni, UX, test, sicurezza, licenze, revisione indipendente. Se manca uno specialista astrofisico, compensa con fonti scientifiche primarie e review numerica indipendente, dichiarando il limite.

Utilizza anche **altri modelli disponibili quando sono adatti**. Fai produrre proposte e critiche separate per decisioni ad alto impatto; usa un revisore che legga il diff effettivo. Non scegliere il modello più costoso per ogni compito e non consumare token inutilmente: massimizza qualità ed evidenze per token impiegato, parallelizzando soltanto attività indipendenti e riutilizzando risultati verificati.

## 6. Repository, continuità e lavoro senza supervisione

Il repository indicato nel piano è `https://github.com/Luporosso76/astroProcessing.git`. Verifica autonomamente se la copia locale è un clone valido e quali permessi effettivi sono disponibili. Non inizializzare un repository alternativo se quello ufficiale contiene già storia utile; non sovrascrivere né forzare la storia remota.

Lavora su branch comprensibili, con commit piccoli, verificati e descrittivi. Esegui test e review prima di integrare. Puoi sincronizzare il lavoro remoto **solo se le credenziali e i permessi esistono già e l'operazione non richiede di forzare o alterare la storia protetta**. Mai force-push, cancellazione di branch altrui o pubblicazione di file sensibili. Mantieni tracciabilità delle decisioni e riproducibilità delle build.

Predisponi e aggiorna i documenti operativi del progetto, compresi `AGENTS.md`, ExecPlan per unità di lavoro, registro ADR, stato milestone, problemi noti, evidenze dei test, benchmark e istruzioni di ripresa. `AGENTS.md` deve essere coerente con il piano: nessun blocco artificiale per conferme ricorrenti, nessuna eccezione al divieto di fallback del prodotto. Usa nomi e percorsi coerenti con le convenzioni effettivamente supportate dal tuo ambiente.

Se una sessione termina o uno strumento non può proseguire, salva nel repository o nei file di stato del progetto **cosa è concluso, cosa è in corso, cosa ha fallito, perché e qual è il prossimo comando/compito verificabile**. Non dichiarare di poter continuare in modo autonomo dopo la fine della sessione, a meno che esista effettivamente una modalità di esecuzione persistente e autorizzata.

## 7. Verifiche tecniche aggiuntive da fare sul Plan 2.2

Non considero le 4.452 righe del piano una prova della sua correttezza. Il PASS documentale è un buon segnale ma non è validazione scientifica o software. Prima di implementare i singoli algoritmi, verifica con evidenze i rispettivi contratti.

**7.1. Coerenza integrale.** Il file ora include tutte le sezioni fino alla 27 e l'appendice D; conserva questa completezza. Verifica che dopo la chiusura delle decisioni e la revisione delle autorizzazioni non rimangano riferimenti irrisolti, stadi impossibili, dipendenze cicliche o gate senza evidenza. Una tabella non deve contraddire la sua sezione normativa. Non affermare che una suite è stata eseguita se ne hai soltanto definito il piano.

**7.2. Portata reale dell'MVP.** L'MVP rimane end-to-end e ricco: RAW/FITS, master e calibrazione flessibile, debayer, stelle, registrazione, stacking, background, colore, stretch, maschere, denoise, sharpening, riduzione/separazione stelle, deconvoluzione, solver assistito, Advisor completo, export e recovery. Non spostare al P1 le funzionalità P0 appena approvate per velocizzare il rilascio.

**7.3. Immagini prestacked e non lineari.** Verifica che `F-IO-10` e i percorsi di elaborazione non presentino come universalmente disponibili operazioni che richiedono obbligatoriamente un antenato radiometricamente lineare. Il contratto `C-14` richiede l'antenato lineare per il fit stellare: è una limitazione legittima del metodo P0, ma deve essere spiegata all'utente già nel preflight, senza conversioni inverse inventate né algoritmi sostitutivi. Valuta esplicitamente se esiste un futuro operatore fotografico dedicato per immagini solamente non lineari; mantienilo separato e non prometterlo nel P0 senza validazione.

**7.4. Cataloghi e riproducibilità.** Verifica licenza, integrità, dimensioni, aggiornamento e copertura reale dei pack. Specifica come funziona un'installazione Linux offline completa quando D-21 richiede pack incluso. Se il pack richiesto manca o non supera l'hash, l'operazione che lo richiede si ferma senza ricerca online implicita.

**7.5. Costi del no-fallback.** Durante gli spike misura anche l'impatto del preflight dei backend, dei decoder e dei modelli: serve evidenza che il percorso scelto prima dell'avvio è realmente quello eseguito. Una libreria che esegue fallback nascosti e non disattivabili non è utilizzabile sotto NC-21. I test devono iniettare guasti ed accertare l'assenza di sostituzioni, non solo la presenza di un messaggio.

**7.6. Gate scientifici.** Per resampling/covarianza, stacking low-N, denoise lineare e non lineare, deconvoluzione, separazione stellare e Advisor richiedo ground truth, test di stabilità e misure del danno, non soltanto risultati visivamente piacevoli. L'Advisor deve saper astenersi ma non può superare i test astenendosi sempre.

## 8. Sequenza di esecuzione che ti incarico di seguire

1. Verifica il Plan 2.2 effettivamente presente e applica **solo le correzioni mirate** di questo mandato. Registra una revisione 2.2.1 o un changelog equivalente, senza cancellare la 2.2.
2. Chiudi D-19 e D-20 come approvate P0 e registra la politica D-21 con verifica legale ancora da eseguire. Elimina i blocchi documentali che richiedono il mio intervento per decisioni tecniche già delegate.
3. Crea o aggiorna le istruzioni operative persistenti e gli ExecPlan, con tracciabilità rispetto alle 54 sotto-milestone e alle dipendenze canoniche. Pianifica la ripresa da nuove sessioni.
4. Completa autonomamente M0 con le verifiche possibili; esegui gli spike M1, confronta linguaggi, GUI, persistenza, scheduler, GPU e hash, scegli lo stack sulla base delle prove e registra le alternative scartate.
5. Implementa M2 e M3 secondo ordine topologico, con fixture e test prima o insieme agli operatori, non dopo l'intero progetto. Poi M4 e M5: la prima release Linux deve essere completa nei suoi P0 e testata, senza riduzioni del perimetro.
6. Prosegui autonomamente con M6, M7, M8 e M9 nei limiti delle dipendenze e dei permessi. **Non iniziare M10** senza una mia successiva richiesta esplicita.
7. Per ogni sotto-milestone: implementa, testa, misura, raccogli review indipendente, correggi i rilievi, ripeti i test, registra i risultati e passa alla successiva. Non aspettare una mia risposta quando il gate è verificabilmente superato.
8. Se qualcosa fallisce, non applicare fallback al prodotto e non “far passare” artificialmente il gate. Correggi il difetto o isola un'impossibilità reale; continua sulle attività indipendenti. Rendi visibili i blocchi tecnici, scientifici, legali o di permesso.
9. Se ti interrompo esplicitamente, arresta il lavoro in sicurezza, conserva lo stato e non avviare altri job. Le altre pause devono dipendere solo da limiti effettivi dell'ambiente, non da richieste rituali di approvazione.

## 9. Cosa considero un risultato conforme

Non voglio un nuovo enorme documento che cambi nomi e sposti tabelle senza aggiungere sostanza. Voglio:

- piano consolidato coerente e decisioni D-19/20 chiuse, D-21 governata con evidenze;
- istruzioni persistenti che consentano esecuzione autonoma e ripresa dopo nuove sessioni;
- analisi critica e benchmark reali prima delle decisioni tecnologiche irreversibili;
- codice Linux verificato per fase, con report di test e revisioni indipendenti;
- nessun fallback nascosto o dichiarato nel prodotto, nessun gate inventato, nessuna modifica silenziosa dei dati;
- avanzamento continuo su tutte le attività autorizzate, senza richiedere mie approvazioni di routine.

**Non chiedermi di scegliere un linguaggio, eseguire comandi, avviare un test o autorizzare ogni milestone: è il lavoro che sto delegando a te.** Se manca un permesso imposto dalla piattaforma, non fingere di possederlo; documenta il blocco e continua dove possibile. Il mio obiettivo resta un'applicazione Linux completa e scientificamente difendibile, non una demo e non una collezione di piani.
