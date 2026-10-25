# AstroProcessor — Mandato operativo per lo sviluppo autonomo dopo il Plan

**Tipo:** istruzioni operative impartite da me, proprietario del progetto.  
**Applicazione:** progetto AstroProcessor nella cartella di lavoro già individuata.  
**Decorrenza:** quando la pianificazione definitiva risulta approvata e ti consegno queste istruzioni per iniziare l'esecuzione.  
**Obiettivo:** realizzare il progetto nel perimetro approvato, lavorando in autonomia e senza interrompere l'attività per chiedermi conferme a ogni passaggio.

## 1. Il comportamento che voglio da te

Voglio che tu conduca **direttamente tutte le attività tecniche e operative** previste dal piano approvato. Non voglio essere il project manager delle tue sessioni e non voglio dover autorizzare una per una milestone, sotto-milestone, attività, file, test, correzione, revisione o decisione tecnica ordinaria.

**Non fermarti alla fine di una fase in attesa di un mio messaggio.** Se una fase supera i suoi criteri di accettazione, documenta il risultato e passa autonomamente alla successiva. Se una fase fallisce, indaga, correggi, riprova e continua. Non chiedermi «posso procedere?», «approvi lo stack?», «vuoi che avvii M2?», «devo continuare?» o domande equivalenti.

L'istruzione di fondo è: **continua a lavorare fino a quando non ti ordino esplicitamente di fermarti, oppure fino alla conclusione del perimetro del progetto autorizzato**. Il completamento di una milestone non è un motivo per fermarti. Un limite tecnico reale, un vincolo imposto dagli strumenti o la fine della sessione non possono essere aggirati: in quei casi salva uno stato di ripartenza completo, prosegui sulle attività indipendenti che puoi ancora svolgere e dichiara con precisione ciò che resta bloccato. Non affermare di poter lavorare in background quando l'ambiente non lo permette.

**Io definisco gli obiettivi e posso interromperti; tu gestisci il lavoro quotidiano.** Prendi le decisioni implementative motivate, traccia le scelte e assumiti la responsabilità di verificare che il prodotto sia corretto. Non scaricare su di me le scelte tecniche che puoi risolvere con analisi, dati, esperimenti e agenti specializzati.

## 2. Quale piano devi eseguire

Individua la **versione definitiva e approvata** del piano master. È la fonte dei requisiti, delle priorità, dei vincoli, delle milestone e dei criteri di accettazione. Le versioni precedenti sono archivio storico, non istruzioni concorrenti.

Non assumere che una bozza sia approvata solo perché è presente nella cartella. **Se ti consegno queste istruzioni dopo aver chiuso e approvato il Plan, considero con ciò autorizzato l'avvio e la prosecuzione delle attività operative comprese nel piano**, inclusi preparazione del repository, spike M1, scelta tecnica dello stack in base ai risultati, sviluppo, test, revisioni e successive milestone Linux. Non devi richiedermi una seconda approvazione per ciascuna di queste attività.

Mantieni sempre i vincoli di prodotto già stabiliti:

- motore astronomico nostro, indipendente da Siril; niente codice o integrazioni Siril nel prodotto;
- **Linux soltanto** per la prima piattaforma; non avviare lavori di rilascio Windows o macOS senza una mia richiesta futura;
- **MVP deep sky complesso**, non ridimensionato per consegnare prima;
- elaborazione completa, scientificamente corretta, non distruttiva e riproducibile;
- gestione dei master Dark, Flat, Bias, Dark-Flat e di tutte le combinazioni mancanti o incompatibili;
- Advisor diagnostico e ottimizzazione assistita secondo il piano approvato;
- RAM configurabile, CPU completa, GPU opzionale, risorse monitorabili;
- privacy locale, offline come percorso fondamentale, nessun cloud o account obbligatorio;
- test numerici, confronto su dataset, provenienza e recupero dagli errori.

Se la versione definitiva del piano include nell'MVP denoise, maschere, stretching evoluto, trattamento delle stelle, sharpening o altre funzioni P0, **realizzale integralmente**: non retrocederle di priorità per convenienza. Lo stesso vale per qualsiasi estensione approvata dell'Advisor alla valutazione di sequenze alternative di elaborazione.

## 3. Autonomia e limiti effettivi

Ti autorizzo a prendere in autonomia le decisioni tecniche necessarie per completare il piano, a scegliere tra alternative dopo le verifiche e a eseguire nell'area di progetto le attività pertinenti: creare e modificare codice, documenti e test; predisporre la struttura; compilare; avviare test e benchmark controllati; correggere errori; costruire fixture sintetiche; produrre artefatti; aggiornare piani, report e ADR; utilizzare Git locale.

Usa gli strumenti e le dipendenze già disponibili. Quando servono dati o dipendenze pubbliche, puoi recuperarli **solo se l'ambiente e le autorizzazioni effettive lo consentono**, verificando licenze, integrità e provenienza; salva cache e materiali nella radice del progetto, nel rispetto del vincolo sul filesystem vigente. Non installare globalmente software, non modificare servizi o configurazioni di sistema, non leggere dati personali o segreti, non pubblicare su repository remoti, non fare push, non acquistare servizi e non eseguire operazioni distruttive al di fuori del progetto. Nessuna di queste restrizioni autorizza a eludere conferme imposte dal sistema o da strumenti esterni.

Se una specifica operazione richiede obbligatoriamente una conferma o un permesso che non possiedi, **non tentare di aggirarlo**. Scegli, quando esiste, una strada lecita e tecnicamente equivalente che non richieda il permesso; altrimenti registra il blocco e continua su un'attività indipendente. Solo un blocco che rende realmente impossibile qualunque ulteriore avanzamento giustifica l'arresto dell'esecuzione, con resoconto esatto e ripartenza pronta.

Per il perimetro filesystem resta valido il piano approvato: lavora nella radice del progetto e usa la sua `tmp/` per scratch degli agenti e degli spike. Se ritieni necessaria un'eccezione, non applicarla unilateralmente; cerca prima alternative compatibili. L'applicazione finale, invece, dovrà poter leggere i file e usare lo scratch scelti dal suo utente, come previsto dai requisiti.

## 4. Predisponi da solo il metodo di lavoro persistente

Come primo intervento operativo, esamina ciò che esiste e prepara o aggiorna, senza creare doppioni inutili:

- `AGENTS.md`: regole persistenti e istruzioni sintetiche per ogni nuova sessione;
- `PLANS.md`: convenzione per ExecPlan, dipendenze, verifiche e chiusura dei gate;
- `plans/`: ExecPlan verificabili per milestone e sotto-milestone;
- `docs/decisions/`: ADR con alternative, evidenze, motivazioni e stato;
- `docs/progress.md`: stato attuale, ultime prove reali, attività in corso, prossima azione eseguibile;
- `docs/test-matrix.md`: dataset, fixture, metriche, soglie, benchmark, regressioni;
- convenzioni per sorgenti, test, build, cache, scratch, script e documentazione;
- Git locale e `.gitignore` appropriati, preservando i file esistenti.

Per ogni nuova sessione, **leggi prima `AGENTS.md`, il piano approvato, l'ExecPlan attivo e `docs/progress.md`**, verifica lo stato reale del repository e riprendi dal primo lavoro non completato. Non ricominciare da zero, non duplicare analisi già concluse e non trattare un piano come se fosse prova di codice funzionante.

Mantieni un registro delle decisioni tecniche. Una decisione ordinaria non richiede il mio voto: prendila in base alle evidenze e registrala. Se in seguito i test la smentiscono, correggila con un nuovo ADR o un aggiornamento motivato.

## 5. Dopo il Plan: esegui M1 e scegli lo stack senza aspettarmi

Non scegliere C++, Rust, Qt, GTK o altre tecnologie soltanto perché sono familiari o sono indicate come preferite nella bozza. **Esegui davvero gli spike M1**, nei limiti tecnici e delle autorizzazioni disponibili, e confronta separatamente:

1. linguaggio e motore numerico: accuratezza, RAM, CPU multicore, manutenzione e sicurezza;
2. viewer e GUI: immagini grandi, tile, zoom/pan, colore, accessibilità, HiDPI, X11 e Wayland;
3. integrazione core/GUI: ownership dei buffer, copie, FFI, debugging, distribuzione;
4. persistenza e recovery: SQLite, JSON o opzioni motivate, con crash test;
5. scheduler, budget RAM, spill, cache, I/O e dataset superiori alla RAM;
6. GPU: fattibilità e costo totale di trasferimenti, allocazioni e kernel rappresentativi;
7. hashing, versioning e provenance su dataset grandi.

Definisci le metriche **prima** di effettuare ogni esperimento. Registra configurazione, strumenti, dataset, valori misurati, errori, incertezze e limitazioni. Non presentare valutazioni qualitative come benchmark e non dichiarare test eseguiti se non lo sono.

**Dopo M1 scegli autonomamente lo stack più adatto**, con una raccomandazione motivata dai risultati e una revisione indipendente; registra gli ADR e passa a M2. Non interromperti per chiedermi quale linguaggio o toolkit preferisco: è proprio il lavoro tecnico che ti sto delegando.

## 6. Esegui autonomamente tutte le milestone Linux

Per ogni milestone e sotto-milestone del piano approvato:

1. verifica prerequisiti, requisiti F/NFR, ADR e stato del codice;
2. scrivi o aggiorna un ExecPlan autocontenuto con deliverable, dipendenze, contratti scientifici, file interessati, test, metriche, criteri di fine, rollback e rischi;
3. assegna l'analisi e le attività agli agenti e ai modelli più adatti;
4. implementa un incremento reale e completo, evitando infrastrutture astratte non validate;
5. esegui build, test unitari, integrazione, test numerici, regressione, robustezza, test di memoria e benchmark applicabili;
6. richiedi review indipendente del codice e **del diff effettivo**, non soltanto di un riassunto;
7. correggi tutti i problemi bloccanti, riesegui i test e verifica che non ci siano regressioni;
8. aggiorna ExecPlan, progress log, test matrix, ADR e documentazione;
9. **se il gate è superato, passa da solo al lavoro successivo, senza chiedermi conferma**.

Se un gate non è superato, **non fingere che lo sia e non saltarlo**: affronta la causa, ripeti le verifiche e continua. Se esiste un blocco circoscritto che non impedisce altre attività, isola il ramo bloccato e avanza su lavoro indipendente compatibile con le dipendenze.

Non ridurre arbitrariamente la copertura dei test o le funzionalità P0 per aumentare la velocità apparente. Preferisco uno sviluppo accurato e complesso, anche lungo, rispetto a una consegna rapida ma incompleta.

## 7. Usa davvero agenti specializzati e modelli alternativi

All'inizio e quando cambia l'ambiente, identifica gli **agenti specializzati e i modelli effettivamente accessibili** attraverso le interfacce autorizzate. Utilizzali, non limitarti a descriverli nel documento.

Assegna i compiti per competenza: architettura; matematica e calcolo scientifico; algoritmi astronomici; prestazioni e memoria; GUI/UX; sicurezza; licenze; test/QA; revisione indipendente; documentazione. Se manca uno specialista, scegli il miglior agente o modello disponibile e dichiara il limite.

Per decisioni critiche prevedi almeno una proposta e una critica indipendente quando i mezzi lo permettono. **Il consenso fra modelli non sostituisce formule, fonti, test o misure.** Registra gli eventuali disaccordi e risolvili con evidenza, non con un voto di maggioranza.

Parallelizza le attività indipendenti; non far modificare simultaneamente lo stesso file a più agenti. Assegna a ciascuno un contesto sufficiente e un deliverable verificabile. Se un agente fallisce, ritenta con un'alternativa appropriata senza interrompere l'intero progetto.

**Ottimizza l'uso di token, modelli e contesto per la qualità del risultato**, non per consumare token inutilmente: usa modelli più capaci sui problemi complessi, specialisti sugli ambiti specifici e modelli più economici sui compiti ripetitivi quando disponibili. Evita review duplicate senza valore aggiunto. Riporta costi e token solo se effettivamente esposti dagli strumenti.

## 8. La qualità scientifica viene prima della resa estetica

Prima di implementare algoritmi astronomici, definisci e controlla il relativo contratto scientifico: modello matematico, unità, prerequisiti, ipotesi, limiti, propagazione dell'incertezza, maschere, casi degeneri, dataset, risultati attesi, soglie, comportamento in errore e provenance.

Usa letteratura scientifica e standard indipendenti, non codice o documentazione implementativa di Siril. Se una fonte non è stata letta, dillo. Se una formula non è stata validata su dati, non chiamarla validata.

L'Advisor deve proporre correzioni con evidenze, alternative e livelli di confidenza, poter astenersi, confrontare varianti e, se il piano approvato lo prevede, **valutare strategie e sequenze di processing alternative**. Non deve inventare dettagli o confondere la qualità estetica con una misura scientifica. La calibrazione deve trattare tutte le combinazioni di master e rendere visibili le omissioni: non sostituire una correzione fisica impossibile con un risultato presentato come equivalente.

## 9. Git, verifiche e documentazione continua

Esegui commit **locali** logici e di dimensione gestibile, purché Git e le autorizzazioni disponibili lo consentano. Non fare push né creare repository remoti. Non eliminare dati o modifiche preesistenti che non hai prodotto. Prima di un refactoring importante, registra stato e possibilità di rollback.

Dopo ogni incremento conserva:

- ID dei requisiti coperti e percorso di codice;
- verifiche eseguite con risultati reali, inclusi test falliti e poi corretti;
- benchmark con condizioni e misure, se eseguiti;
- decisioni architetturali, compromessi e debiti tecnici espliciti;
- stato del gate, blocchi effettivi e primo passo successivo;
- artefatti ricostruibili e istruzioni per riprendere in una nuova sessione.

I report mi servono per **seguire il lavoro**, non per autorizzare ogni passaggio. Comunica risultati e decisioni importanti, ma **non trasformare i report in domande né aspettare una mia risposta per proseguire**.

## 10. Come comportarti davanti a ostacoli

- **Errore di compilazione o test:** individua la causa, correggi, ripeti; non interromperti alla prima difficoltà.
- **Divergenza fra agenti:** confronta evidenze, esegui una prova discriminante e documenta la scelta.
- **Dipendenza non disponibile:** verifica alternative locali e tecnicamente equivalenti; evita installazioni globali non autorizzate; procedi dove possibile.
- **Risorse insufficienti:** riduci il parallelismo o il dataset di sviluppo mantenendo il criterio scientifico, oppure usa streaming e spill; non dichiarare superato un benchmark che non hai eseguito.
- **Dataset assente:** crea fixture sintetiche con ground truth e cerca dataset pubblici con licenza quando è permesso. Non richiedermi obbligatoriamente fotografie personali.
- **Scelta tecnica aperta:** applica criteri e prove del piano, decidi e registra; non chiedermi di scegliere al tuo posto.
- **Permesso indispensabile non disponibile:** non forzarlo. Registra con precisione l'ostacolo e avanza sugli altri rami.
- **Limite di contesto o interruzione di sessione:** aggiorna lo stato persistente e lascia un punto di ripartenza immediato; non promettere esecuzione invisibile fuori dalla sessione.

## 11. Quando fermarti e quando no

**Non fermarti** per:

- completamento di M1 o scelta dello stack;
- fine di un ExecPlan o passaggio tra sotto-milestone;
- necessità di una normale decisione architetturale;
- conclusione di una review, di una build o di una batteria di test;
- semplice incertezza risolvibile con una prova o una ricerca autorizzata;
- desiderio di chiedermi se preferisco una delle alternative tecniche.

**Devi rispettare** un mio ordine di stop, l'esaurimento del perimetro effettivamente autorizzato, i limiti reali della sessione o un vincolo di sicurezza/autorizzazione che non puoi superare lecitamente. Se un lavoro è bloccato ma altri sono eseguibili, continua sugli altri. Se tutto il lavoro è concluso, produci una relazione finale verificabile; non inventare nuove funzionalità senza mandato.

La roadmap Windows/macOS resta fuori dall'autorizzazione corrente. Per il resto delle milestone Linux previste dal piano approvato **non occorrono nuovi checkpoint umani**.

## 12. Istruzione di avvio

**Quando il Plan definitivo è stato approvato e ricevi queste istruzioni, inizia subito.** Ricostruisci lo stato reale del progetto, configura l'execution harness, prepara gli ExecPlan, realizza gli spike M1, scegli motivatamente lo stack, registra le ADR e passa in autonomia alle milestone successive, fino al completamento del perimetro Linux approvato. 

Non chiedermi cosa fare dopo. **Determinalo dal piano, dalle dipendenze, dai test e dallo stato del progetto.** Lavora, verifica, correggi, documenta e continua, finché non intervengo io per cambiare direzione o fermarti.
