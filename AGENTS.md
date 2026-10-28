# AGENTS.md

Regole operative per agenti che lavorano in `/opt/workspace/astroProcessor`. Il piano scientifico non si ripete qui: le fonti sono linkate.

## Fonti

- Mandato: `RICHIESTA_REVISIONE_PLAN_2_2.md` (122 righe). Definisce autonomia M0–M9, D-19/D-20 P0, D-21 come preferenza.
- Istruzioni: `ISTRUZIONI_OPERATIVE.md` (164 righe), valide salvo l'override Git sotto.
- Piano attivo: `ASTRO_POSTPROCESSING_PLAN_v2.2.1.md`, SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`. La review indipendente è registrata in `docs/evidence/M0-plan-review.md`: supera il PENDING dello snapshot di consegna senza alterarlo. Gli storici sono in `doc_old/`; non serve un voto del proprietario per avanzare M0–M9.
- Revisione in scrittura: se un autore sta scrivendo attivamente, nessun altro la legge o la modifica. Alla consegna stabile, un revisore indipendente la legge e registra la review; solo dopo diventa fonte.
- Stato: `docs/progress.md`. Convenzione ExecPlan: `PLANS.md`. Piani: `plans/`. Matrice test: `docs/test-matrix.md`. Decisioni: `docs/decisions/`. Prove: `docs/evidence/`.

## Autonomia

- M0–M9 sono autorizzate. Una milestone chiusa è un evento di avanzamento, non una domanda al proprietario. Nessuna approvazione rituale per ADR, spike, test o sotto-milestone. D-19 e D-20 sono già approvate P0 e non si riaprono.
- Decisioni tecniche delegate: linguaggio, GUI, formati interni, SQLite/JSON, strumenti, algoritmi, dataset leciti, metodi di benchmark, packaging Linux, budget, organizzazione del codice. Si decidono dopo le evidenze richieste; nessuna preferenza sostituisce una misura. Nessuna funzione P0 mancante chiude un gate; una P1 non si promuove né si elimina in silenzio.
- I gate automatici (scienza, regressione, sicurezza, licenze, correttezza numerica, prestazioni, compatibilità, DoD) si verificano senza chiedere pareri. Se falliscono, si corregge e si ripete.
- Si blocca solo il ramo colpito da: M10, release commerciale, licenza del codice con effetto irreversibile, costo a pagamento non autorizzato, credenziale mancante, modifica distruttiva fuori progetto, permesso reale della piattaforma assente. Causa ed effetto vanno registrati; i rami indipendenti proseguono.
- Licenza (D-02, proprietario, 28 ottobre 2026): il prodotto può essere GPL. Dipendenze GPL/LGPL compatibili (FFTW, Qt) sono ammesse. L'emissione del file `LICENSE` si registra in ADR-12.

## Ritmo di lavoro

- Un record di evidenza per unità o spike, aggiornato fino alla chiusura; non un record per ogni tentativo, mock o ricevuta.
- Un errore di sviluppo si indaga e si corregge nello stesso turno, poi si riesegue. Nessun turno di ammissione, preflight cerimoniale, ricevuta di PID o autenticazione di hash per i tentativi intermedi.
- Gli harness degli spike restano minimi: lanciano, misurano, confrontano con l'oracle. Un guard dell'harness che impedisce la misura è un difetto dell'harness, non un esito della prova.
- Restano invariati: verità di riferimento indipendente, tolleranze derivate prima della misura, iniezione di guasti sui percorsi del prodotto, review indipendente sul diff.

## Git

- Repository ufficiale: `https://github.com/Luporosso76/astroProcessing.git`. Nessun repository alternativo. Branch di lavoro: `work/m0-m1`.
- Commit locali e push sono consentiti solo verso `origin` ufficiale, con credenziali già disponibili.
- Prima della pubblicazione: review, test, controllo di sicurezza e privacy. Nessun force push, nessuna riscrittura della storia, nessun push su branch protetto, nessun segreto, nessun token o servizio a pagamento nuovo.
- L'ultima istruzione del proprietario in chat prevale sui divieti di push delle righe 42 e 121 di `ISTRUZIONI_OPERATIVE.md` (sezioni 3 e 9), che resta invariato come documento storico.
- Niente installazioni globali, niente modifiche a servizi o configurazioni di sistema, niente lettura di dati personali o segreti.

## Fallback: prodotto contro sviluppo

- Nel prodotto il fallback è vietato, sempre (NC-21, NFR-FBK-01). Errore, risorsa assente o gate fallito producono stop controllato, codice di uscita non nullo e diagnosi. Nessun cambio automatico di percorso, algoritmo, backend, formato, solver, display o calibrazione. Nessuna riduzione silenziosa di risoluzione, qualità, precisione o candidati. Il resume vale solo con ricetta, metodo, backend e parametri identici al checkpoint valido. L'autorizzazione dell'utente non abilita eccezioni.
- La calibrazione parziale è una ricetta primaria scelta e dichiarata prima dell'esecuzione, non un ripiego dopo il guasto.
- Nel codice il bug si corregge: indagare, modificare, rieseguire. Mai far passare un test degradando l'algoritmo, cancellando una fixture, disabilitando una funzione o allentando il gate. Se l'impossibilità è scientifica o legale, si dichiara l'assunzione fallita.

## Perimetro

- Core CPU, offline, Linux. Nessun codice, runtime, CLI o API Siril. Solo letteratura e standard pubblici, implementazione indipendente.
- Originali immutabili, in sola lettura, persistenti. I metadati di strumenti e filesystem si leggono in sola lettura, solo se strettamente necessari, mai segreti, senza modificare il sistema.
- Build, cache, fixture e temporanei dentro il progetto (`tmp/` alla radice). Nessun nuovo temporaneo esterno. Gli spike M1 usano codice isolato in `tmp/`, ammesso dopo M0 e mai integrato. Il codice di prodotto si scrive da M2, dopo i gate M1.
- CodeGraph/Graphify: nessun indice creato dagli agenti. Si usa solo un indice già esistente, se il proprietario lo ha creato. Se l'indice manca, si prosegue: non è un prerequisito degli spike M1.

## Delega e review

- Proposta primaria allo specialista competente. Review indipendente sul diff o sul testo effettivo, mai sull'auto-riassunto dell'autore. Nessuna auto-approvazione.
- Non si sceglie il modello più costoso per default. Costi e token si riportano solo se esposti dal runtime.
- Modelli per competenza (proprietario, 28 ottobre 2026): Haiku 5.5 per ricognizione e compiti meccanici, Sonnet 5.5 per codice, test e documentazione ordinari, Opus per coordinamento, architettura e decisioni, Fable 5 solo per casi estremamente complessi. Il modello si passa esplicitamente alla delega.
- Codex non si usa. Gemini via `agy` solo per testi e lingue, mai sul codice: `agy -p "<prompt>" --model gemini-3.8-flash-high`. Gemini 4 Argon non è disponibile e non si usa. Ricerche complesse al miglior modello Claude (Opus; Fable 5 per i casi estremi). La review indipendente la fa un agente Claude diverso dall'autore.
- Di norma tenere al massimo due deleghe contemporanee. Passare soltanto percorsi, delta e gate necessari; riusare le sessioni pertinenti senza rileggere piani già verificati, così da contenere contesto e consumo.
- Dopo un errore terminale di quota, verificare gli artefatti e registrare se il lavoro è partito prima di riprenderlo. Non ripetere alla cieca la chiamata né dichiarare attiva una delega già terminata. Non correggere manualmente la configurazione dei modelli per aggirare l'errore.
- Metadati agente: modello dichiarato dal runtime, percorsi e conteggi di righe letti. Nessuna attestazione inventata.

## Documenti

- `docs/progress.md` è l'unica fonte dello stato. Non duplica il piano né le 54 unità.
- `PLANS.md` definisce la convenzione degli ExecPlan. Ogni unità attiva ha un piano in `plans/` con prerequisiti, gate e prossimo comando verificabile.
- `docs/test-matrix.md` separa esplicitamente test pianificati e test eseguiti. `docs/evidence/` contiene solo prove reali con comando, hash, versioni, ambiente, esito e limiti. Un test pianificato non è un test eseguito.
- Gli ADR canonici stanno in `docs/decisions/`. Una regola ha una sola casa: qui i vincoli operativi, non la scienza.
