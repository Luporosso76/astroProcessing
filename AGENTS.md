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
- Interfaccia grafica (proprietario, 29 ottobre 2026): prima di qualsiasi lavoro sulla GUI (shell Qt, viewer, codice o scelte di design dell'interfaccia) ci si ferma e la si progetta col proprietario. Core, CLI e test proseguono in autonomia.
- Decisioni tecniche delegate: linguaggio, GUI, formati interni, SQLite/JSON, strumenti, algoritmi, dataset leciti, metodi di benchmark, packaging Linux, budget, organizzazione del codice. Si decidono dopo le evidenze richieste; nessuna preferenza sostituisce una misura. Nessuna funzione P0 mancante chiude un gate; una P1 non si promuove né si elimina in silenzio.
- Componenti (proprietario, 30 ottobre 2026: applicazione professionale): per ogni componente, cioè libreria, dataset, catalogo, algoritmo, metodo o strumento, si sceglie il migliore disponibile, con licenza certa letta alla fonte primaria e sicuro. Mai un ripiego scelto perché più comodo, più piccolo o con licenza incerta. Se il migliore è escluso da licenza, costo o autorizzazione, la scelta va al proprietario in chat, subito, con le opzioni e una raccomandazione; scriverla solo nei documenti non basta.
- I gate automatici (scienza, regressione, sicurezza, licenze, correttezza numerica, prestazioni, compatibilità, DoD) si verificano senza chiedere pareri. Se falliscono, si corregge e si ripete.
- Si blocca solo il ramo colpito da: M10, release commerciale, licenza del codice con effetto irreversibile, costo a pagamento non autorizzato, credenziale mancante, modifica distruttiva fuori progetto, permesso reale della piattaforma assente. Causa ed effetto vanno registrati; i rami indipendenti proseguono.
- Licenza (D-02, proprietario, 28 ottobre 2026): il prodotto può essere GPL. Dipendenze GPL/LGPL compatibili (FFTW, Qt) sono ammesse. L'emissione del file `LICENSE` si registra in ADR-12.

## Ritmo di lavoro

- Un record di evidenza per unità o spike, aggiornato fino alla chiusura; non un record per ogni tentativo, mock o ricevuta.
- Un errore di sviluppo si indaga e si corregge nello stesso turno, poi si riesegue. Nessun turno di ammissione, preflight cerimoniale, ricevuta di PID o autenticazione di hash per i tentativi intermedi.
- Gli harness degli spike restano minimi: lanciano, misurano, confrontano con l'oracle. Un guard dell'harness che impedisce la misura è un difetto dell'harness, non un esito della prova.
- Restano invariati: verità di riferimento indipendente, tolleranze derivate prima della misura, iniezione di guasti sui percorsi del prodotto, review indipendente sul diff.

### Ciclo di un'unità (proprietario, 29 ottobre 2026: niente tempo perso)

1. Perimetro dal registro dell'appendice D (`tools/trace.py ids --stage <Mx.y>`), mai dai riepiloghi del §21. Piano corto: il dettaglio sta in contratti e test. Unità piccole: uno o pochi ID coerenti per unità e per review, mai pacchetti di contratti, codice e strumenti insieme. Un contratto afferma solo ciò che è dimostrato o misurato; il resto lo scrive come ipotesi, stima o limite dichiarato.
2. Contratto e test prima del codice: i test nuovi falliscono finché manca l'implementazione. Statistica e numerica: autore Opus fin dall'inizio.
3. L'autore implementa ed esegue `tools/check.sh` (build, test, lint delle classi di difetto) e `tools/mutate.sh` per le prove di mutazione. Non consegna con controlli rossi. Prima della review rilegge il proprio diff con la checklist di `tools/review-template.md` e corregge da sé: la review è un controllo finale, non il modo di trovare i difetti.
4. L'autore chiede la review con `tools/review.sh` (prompt e checklist fissi, elenco completo al primo giro, nessun tetto di righe), corregge e ripete sul solo delta fino al PASS. Non scrive il prompt del revisore: passa solo fatti (ID, file, log). Al massimo 2 giri in tutto per unità (proprietario, 30 ottobre 2026; slug `-roundN`): il giro 1 rivede contratto, test e codice, il giro 2 le correzioni sul delta; dal secondo giro il revisore valuta solo correzioni e delta, e un difetto nuovo in codice non cambiato conta solo se BLOCCANTE (gli altri vanno in "Fuori delta", lavoro per un'unità successiva). Al terzo giro `review.sh` esce con 3: l'autore si ferma e il coordinatore arbitra (ridurre il perimetro, dichiarare un limite o correggere). Fra un giro e l'altro si corregge soltanto: modifiche minime, limitate ai rilievi, niente riscritture né cambi di impostazione. Se un rilievo richiede di riprogettare, il ciclo si chiude e la nuova impostazione riparte dal punto 2 come unità nuova, dichiarata al proprietario; un rilievo ordinario non apre un'unità nuova. Il tetto vale anche per il gate. A ogni giro `review.sh` salva un'istantanea dei file modificati e nuovi (`tmp/review-snap/`) e al giro 2 calcola da sé il delta (`tmp/review-delta-<unità>-round2.diff`) e lo passa al revisore; il giro 1 deve quindi passare dallo strumento.
5. Il coordinatore verifica (build pulita dev+release, `tools/trace.py check`), poi privacy, commit e push. Stato, conteggi e tabella ID → test si generano con `tools/trace.py` e `tools/status.py`, non si scrivono a mano.
6. Unità indipendenti nel grafo girano in parallelo in worktree separati, dividendo il tetto del 70% delle CPU. Un harness di misura passa in review prima di misurare.
7. Attese su un processo: per pid o `pgrep -x <nome>`, sempre con un tempo massimo. Mai `pgrep -f "<testo>"` se il testo compare nel comando che aspetta: trova sé stesso e l'attesa non finisce. Un comando lanciato in background notifica da solo la fine: niente cicli `until … sleep` sul suo file di output.

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
- Nessun report di crash del sistema (proprietario, 29 ottobre 2026: arrivano come notifiche sul desktop). Gli script Python gestiscono ogni errore e terminano con `sys.exit("<diagnosi>")`, mai con un'eccezione non gestita, e si lanciano con `python3 -S` (niente hook di apport). Un abort voluto in un death test passa da `exit_instead_of_core_dump()` (`tests/unit/no_crash_report.hpp`).
- Build, cache, fixture e temporanei dentro il progetto (`tmp/` alla radice). Nessun nuovo temporaneo esterno: niente `/tmp`, niente `mktemp` senza directory; i test si lanciano coi preset di ctest, che portano `TEST_TMPDIR` e `TMPDIR` in `build/<preset>/test_tmp/sys` (gtest altrimenti scrive in `/tmp`). Gli spike M1 usano codice isolato in `tmp/`, ammesso dopo M0 e mai integrato. Il codice di prodotto si scrive da M2, dopo i gate M1.
- CodeGraph/Graphify (proprietario, 29 ottobre 2026): gli indici esistono (`.codegraph/`, `graphify-out/`, fuori da Git) e si usano. `codegraph sync` a inizio sessione e dopo modifiche ampie; `codegraph impact <simbolo>` prima di cambiare una firma o un simbolo condiviso; `graphify update .` dopo modifiche ampie al codice. Ricostruire la parte documentale di graphify consuma token: la si rifà solo quando cambiano piano, ADR o contratti.

## Delega e review

- Proposta primaria allo specialista competente. Review indipendente sul diff o sul testo effettivo, mai sull'auto-riassunto dell'autore. Nessuna auto-approvazione.
- Non si sceglie il modello più costoso per default. Costi e token si riportano solo se esposti dal runtime.
- Modelli per competenza (proprietario, 28 ottobre 2026): Haiku 5.5 per ricognizione e compiti meccanici, Sonnet 5.5 per codice, test e documentazione ordinari, Opus per coordinamento, architettura e decisioni, Fable 5 solo per casi estremamente complessi. Il modello si passa esplicitamente alla delega.
- Review indipendenti di codice e contratti: Codex `gpt-6-astra` (reasoning high, `-s read-only`, via `codex exec`) finché ha token (proprietario, 28 ottobre 2026); a quota esaurita si torna ai revisori Claude. Codex non scrive codice. Gemini via `agy` solo per testi e lingue, mai sul codice: `agy -p "<prompt>" --model gemini-3.8-flash-high`. Gemini 4 Argon non è disponibile e non si usa. Ricerche complesse al miglior modello Claude (Opus; Fable 5 per i casi estremi). La review indipendente la fa un agente Claude diverso dall'autore.
- Di norma tenere al massimo due deleghe contemporanee. Passare soltanto percorsi, delta e gate necessari; riusare le sessioni pertinenti senza rileggere piani già verificati, così da contenere contesto e consumo.
- Dopo un errore terminale di quota, verificare gli artefatti e registrare se il lavoro è partito prima di riprenderlo. Non ripetere alla cieca la chiamata né dichiarare attiva una delega già terminata. Non correggere manualmente la configurazione dei modelli per aggirare l'errore.
- Metadati agente: modello dichiarato dal runtime, percorsi e conteggi di righe letti. Nessuna attestazione inventata.

## Documenti

- `docs/progress.md` è l'unica fonte dello stato. Non duplica il piano né le 54 unità.
- `PLANS.md` definisce la convenzione degli ExecPlan. Ogni unità attiva ha un piano in `plans/` con prerequisiti, gate e prossimo comando verificabile.
- `docs/test-matrix.md` separa esplicitamente test pianificati e test eseguiti. `docs/evidence/` contiene solo prove reali con comando, hash, versioni, ambiente, esito e limiti. Un test pianificato non è un test eseguito.
- Gli ADR canonici stanno in `docs/decisions/`. Una regola ha una sola casa: qui i vincoli operativi, non la scienza.
