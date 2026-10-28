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
- `M1-e-vulkan-review.md`: review statica del candidato compute, con cleanup e ammissione da correggere prima dell'esecuzione.
- `M1-e-vulkan-followup.md`: correzioni successive compilate e tre shader validati; nessuna esecuzione engine, GPU o regressione. Review aggiornata distinta dalla preparazione.
- `M1-e-vulkan-followup-review.md`: due MED e un LOW chiusi staticamente; rilievo sul test mismatch e ammissione della prima suite supervisor, non del confronto GPU.
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
- `M1-a-error-validation.md`: prima build ed esecuzione delle regressioni d'errore; verifica indipendente runtime ancora distinta.
- `M1-a-error-validation-review.md`: tre rilievi chiusi sui risultati reali e integrità verificata; ambito limitato al dispatcher e ai figli controllati.
- `M1-c-measurement.md`: prima serie temporale SQLite/JSON, campioni e bootstrap ricalcolati dal coordinatore; nessuna decisione di persistenza.
- `M1-c-measurement-review.md`: ricalcolo indipendente di conteggi, mediane e bootstrap della serie reale; limiti di confronto dichiarati.
- `M1-b-preparation.md`: sorgenti del viewer e preregistrazione predisposti; controlli AST e sintattici, nessun avvio GUI o benchmark.
- `M1-b-static-review.md`: review indipendente dei quattordici sorgenti; cinque correzioni richieste prima dei test del viewer.
- `M1-catalog-rights.md`: lettura delle linee guida APASS e dei termini del canale ESA Tycho2; grant mancanti e limiti commerciali, nessun catalogo acquisito.
- `M1-e-supervisor-validation.md`, `M1-e-supervisor-review.md`: 18 regressioni del supervisor eseguite e confermate indipendentemente; primo tentativo non qualificato conservato, nessuna esecuzione GPU.
- `M1-f-static-review.md`: quattro rilievi prima dell'esecuzione degli hash; nessuna fixture o misura ancora eseguita.
- `M1-f-freeze-003-review.md`: quattro guardie chiuse staticamente e due correzioni residue; nessuna approval o esecuzione hash anticipata.
- `M1-d-lifecycle-review.md`: review del massimo simultaneo e dell'ammissione; runtime ancora bloccato, nessun PASS dinamico.
- `M1-d-admission-review-v4.md`: partizioni strutturali esaminate, quattro invarianti residue e necessità del ceiling fisico prima della preallocazione.
- `M1-e-preparation-refresh.md`: nuova PREP 004 compilata e validata staticamente dopo la suite supervisor; non è un'esecuzione compute.
- `M1-e-cpu-pilot.md`: primo pilot CPU nativo effettivo e lock consegnato per review; un caso, nessun confronto GPU o vincitore CPU generale.
- `M1-e-cpu-pilot-review.md`: lock e oracle CPU confermati indipendentemente; ammissione limitata alla validazione GPU del caso congelato.
- `M1-e-gpu-validation.md`: affine CPU/GPU e copy-control realmente eseguiti sul dispositivo AMD fissato; esito primario da verificare, nessun benchmark comparativo.
- `M1-e-gpu-validation-review.md`: binding, hardware, tracce e copy-control confermati indipendentemente; soli FAULTS candidabili al prossimo turno, limiti di osservabilità espliciti.
- `M1-e-faults.md`: nove guasti preregistrati con arresto controllato effettivamente eseguiti; risultati primari da verificare prima di CONFIRM.
- `M1-f-freeze-004.md`: due correzioni predisposte e nuovo freeze; nessuna esecuzione hash o approval anticipata.
- `M1-f-freeze-004-review.md`: delta verificato e approval separata limitata alle otto regressioni; turno ed esiti dinamici distinti.
- `M1-f-regressions.md`, `M1-f-delivery-hardening.md`: primo turno fermo al preflight, zero test; soli permessi corretti e verificati prima di una nuova ammissione.
- `M1-f-regressions-002.md`: otto regressioni riuscite nel secondo turno; non esecuzione dei producer, dei digest di contenuto o delle 38 ricette.
- `M1-f-regressions-002-review.md`: otto metodi e relativi artefatti confermati indipendentemente; limite discriminante dei probe annotato, nessuna approval automatica ai producer.
- `M1-e-faults-review.md`: nove fault confermati indipendentemente; perdita device soltanto iniettata prima della submit, nessuna prova di perdita hardware reale.
- `M1-e-confirm-affine.md`: 21 coppie confermative CPU/Vulkan effettive sul solo affine 17x19, in attesa di review; nessun vincitore generale dedotto.
- `M1-e-confirm-affine-review.md`: mediane, bootstrap e integrità confermati indipendentemente; precisato che il confronto GT è separato da total_ns.
- `M1-e-batch-preparation.md`: 66 tuple preparate senza dati; integrazione, cleanup e budget ancora da qualificare.
- `M1-d-admission-v5.md`, `M1-d-admission-v5-review.md`: quattro bug chiusi staticamente, dodici test predisposti; modello filesystem non provato, nessun runtime ammesso.
- `M1-d-ext4-sources.md`: ricerca circoscritta su extent ext4 e conversioni unwritten; fonti non equivalenti a prova del kernel host.
- `M1-d-unit-scope-decision.md`: rettificato un prerequisito locale più forte del protocollo; preparazione di dodici unit separate, senza sostituire i gate bulk.
- `M1-b-static-followup.md`: cinque correzioni e oracle esteso predisposti; nessuna build, GUI o regressione eseguita nella preparazione.
- `M1-b-static-followup-review.md`: cinque rilievi chiusi staticamente; ammissibili prima build e oracle in un turno distinto, non preview o benchmark.
- `M1-f-freeze-005.md`, `M1-f-freeze-005-review.md`: probe di pin corretti staticamente e approval separata; nuove regressioni 005 e digest di contenuto ancora non eseguiti.
- `M1-e-publication-001.md`: review della precisa allowlist di diciannove documenti, staging verificato e push effettivo delle prime prove Vulkan; non chiusura M1.
- `M1-b-build-oracle.md`, `M1-b-build-followup.md`: prima build fallita e conservata; correzione circoscritta004 predisposta senza eseguire oracle o nuova build.
- `M1-b-build-followup-review.md`: delta004, diciannove binding e conservazione003 verificati indipendentemente; nuova build/oracle restano prove distinte.
- `M1-b-build-oracle-004.md`, `M1-b-build-oracle-004-review.md`: build reale e oracle004 confermati indipendentemente; nessuna finestra o prestazione GUI provata.
- `M1-e-batch-followup.md`, `M1-e-batch-review-002.md`: namespace e parte del cleanup corretti staticamente; tre rilievi residui prima della prima suite mock, nessun dato batch.
- `M1-e-batch-cleanup-followup.md`, `M1-e-batch-review-003.md`: cleanup003 verificato staticamente; una directory symlink può sfuggire al guard sidecar, correzione richiesta prima dei dati.
- `M1-d-unit-preparation.md`, `M1-d-unit-static-review.md`: dodici unit lifecycle primarie predisposte e review statica conclusa; ammissione e test effettivi separati dai gate bulk.
- `M1-f-regressions-005.md`: otto regressioni effettivamente riuscite con probe di pin specifici; non esecuzione di producer o digest di contenuto, review runtime distinta.
- `M1-f-regressions-005-review.md`: otto regressioni e probe specifici confermati indipendentemente; soli tiny empty/abc candidabili a un turno successivo, non avviati dalla review.
- `M1-f-tiny-validation.md`: due producer, quattro clean chunk1 e due agreement reali completati; esiti primari in review, nessun benchmark.
- `M1-fft-licence-candidates.md`: fonte pocketfft BSD versionata e notices verificate; candidato non adottato, nessuna compilazione o FFT in questa ricerca.
- `M1-a-pocketfft-preparation.md`, `M1-a-pocketfft-static-review.md`: qualifica numerica predisposta, due rilievi cleanup/durabilità bloccano il primo BUILD; nessun test FFT eseguito.
- `M1-a-pocketfft-followup.md`: due correzioni e venti preregressioni predisposte, in review; nessun nuovo test o BUILD eseguito.
- `M1-publication-001.md`: review della precisa allowlist e push delle prime evidenze M1; non approvazione complessiva degli spike.
- `M1-reproducibility-001.md`, `M1-reproducibility-review-001.md`: archivio verificato di sorgenti e risultati storici, con copertura privacy indipendente e limiti della riproducibilità espliciti.

- `M1-f-tiny-validation-review.md`: quattro clean e due agreement confermati indipendentemente; sola progressione al4097 candidabile, non tutte le38 ricette.
- `M1-b-preview-preparation.md`, `M1-b-preview-static-review.md`: modo Widgets con paint/export/exit predisposto e verificato staticamente; BUILD005 distinta dalla futura ammissione GUI.
- `M1-d-unit-validation.md`: dodici unit lifecycle effettive riuscite, raw in review; nessun PASS dei gate bulk/spill o della M1.d complessiva.
- `M1-e-batch-sidecar-followup.md`: correzione del controllo directory symlink, suite mock non ancora eseguita.
- `M1-fixture-store-preparation.md`, `M1-fixture-store-static-review.md`: chiusura66 tuple e168 fixture verificata staticamente; contesto dei gate operativi da correggere, nessun archivio o retire.
- `M1-a-pocketfft-preregression-review.md`: due rilievi originari coerenti staticamente; fixture FD Mock blocca il primo turno, nessuna FFT eseguita.
- `M1-b-gtk-renderer-policy.md`: ricerca bounded sul selettore Cairo e warning fatali; candidato runtime ancora non qualificato, nessun lancio GTK.

- `M1-b-build-005.md`: nuovo host compilato, DSO identica004; nessun oracle005 o successo GUI dedotto dalla build.
- `M1-d-unit-validation-review.md`: dodici lifecycle unit confermate indipendentemente; non sostituiscono pool/bulk/spill o prestazioni.
- `M1-f-publication-001.md`: allowlist di venti record revisionati,905 righe, staging e push verificati; non pubblicazione dei raw o del codice F corrente.
- `M1-b-preview-005.md`: primo lancio xcb fallito con SIGABRT prima del renderer; zero frame o export, raw e processi raccolti senza retry.
- `M1-a-pocketfft-preregressions-002.md`: venti preregressioni mock effettive, exit0 e fonti invariati; review degli esiti distinta, nessun compilatore o worker FFT avviato.
- `M1-a-pocketfft-preregressions-002-review.md`: venti esiti mock e binding confermati indipendentemente da `tests`; build e232 casi FFT restano futuri.
- `M1-b-session-metadata-006.md`: variabile XAUTHORITY disponibile ma omessa dal figlio005; soli metadati verificati, nessun cookie letto, fix o nuovo lancio eseguiti.
- `M1-fixture-store-v2-review.md`: bypass degli helper corretto staticamente; copertura lifecycle del contesto ancora incompleta, nessuna operazione archivio ammessa.
- `M1-f-pattern4097-validation.md`, `M1-f-pattern4097-validation-review.md`: due clean chunk4096 e un agreement confermati; tutti i4097 byte verificati, attribuzione PID rettificata senza modificare i raw. Totali6/38 e3/11, non chiusura M1.f.
- `M1-fixture-context-preparation.md`:18 test lifecycle aggiunti ai24 precedenti, suite combinata42 predisposta con firstfail. AST e hash verificati dall'autore; nessun test o fase store eseguiti, review distinta in corso.
- `M1-fixture-context-static-review.md`: prima suite42 non pronta: closure sintetica materialize incompleta e cleanup con attese illimitate. Fix del solo delta in preparazione, nessun test eseguito.
- `M1-fixture-context-followup.md`: closure e cleanup corretti dall'autore; cinque test supervisore aggiunti,47 totali predisposti. AST/hash soltanto, delta in review indipendente prima dell'ammissione runtime.
- `M1-fixture-context-followup-review.md`: due rilievi chiusi staticamente;47 test pronti per distinta ammissione. Coordinatore:20 file hash/AST, sei pin e cinque snapshot verificati; zero test eseguiti.
- `M1-b-session-followup-006.md`: cinque righe per l'inoltro opaco di XAUTHORITY al figlio Qt+xcb; FIRSTFAIL005 preservato, nessuna nuova build o GUI. Delta006 in review, non prova di autenticazione riuscita.
- `M1-b-session-followup-006-review.md`: READY statico per futura BUILD006;22 file verificati separatamente dal coordinatore,19 pin correnti conformi. Nessun lancio GUI o lettura di cookie.
- `M1-f-pattern4097-completion.md`, `M1-f-pattern4097-completion-review.md`:24 casi e quattro agreement confermati indipendentemente, incluse16 diagnosi negative attese senza output. Totali30/38 ricette e7/11 agreement; nessun benchmark o chiusura M1.f.

- `M1-fixture-context-validation.md`: primo turno fermato al controllo directory,0/47 e nessun launcher; ammissione e sorgenti invariati.
- `M1-fixture-context-delivery-hardening.md`, `M1-fixture-context-hardening-review.md`: sola directory0775 portata a0700 dal coordinatore, dodici pin e identità dei file invariati. Review documentale coerente, non un ricalcolo indipendente: l'esecutore deve ricontrollare stat/hash e spazio prima del nuovo turno.
- `M1-skymapper-rights.md`: due pagine primarie consultate da `research`, DOI non verificato; redistribuzione commerciale del catalogo non dimostrata, nessun pack adottato.

- `M1-b-build-006.md`: FIRSTFAIL durante supervisione configure, ESRCH; nessun viewer006 compilato, nessuna nuova GUI o oracle.
- `M1-a-pocketfft-build-firstfail.md`: primo compilatore fermato con spazio virtuale esaurito; nessun worker FFT prodotto o qualifica numerica.

- `M1-fixture-context-validation-002.md`: STOP prima del launcher per writer concorrente; nessun test eseguito.
- `M1-fixture-context-validation-003.md`, `M1-fixture-context-validation-003-review.md`: 47/47 sintetici confermati dalla review; 22 file verificati dal coordinatore. HOME worker assente dichiarata, senza attestazione di isolamento completo; nessuna fase sulle 168 fixture scientifiche.
- `M1-b-supervisor-regressions-008.md`: 41/41 mock confermati dalla review di sessione `ses_e88559e99ffe4gqqjD6FUr0dmR`; 20 pin correnti verificati dal coordinatore. Nessuna build008 o GUI eseguita; FIRSTFAIL006 conservato.
- `M1-a-pocketfft-preregressions-003.md`: 36/36 mock confermati dalla review di sessione `ses_e88478cf0ffeZkrXH3wReaTRxL`: 13 Cleanup, 7 Barriers e 16 Limits. Non è una qualifica numerica FFT.
- `M1-a-pocketfft-build-003.md`: esito autore PASS_BUILD_ONLY, supervisore PID 917720 exit 0 e raccolto, GCC/readelf exit 0. ELF di 180200 byte prodotto ma mai eseguito; AS build 1 GiB e validate 256 MiB immutato. FIRSTFAIL storico conservato; inventario runtime/licenze e 232 casi FFT non completati da questa build.
- `M1-a-pocketfft-build-003-review.md`: review documentale indipendente PASS_BUILD_ONLY, senza rilievi e senza ricalcolo degli hash dal revisore. Il coordinatore ha autenticato separatamente byte/hash/stat di cinque file con fd NOFOLLOW: report, handoff, manifest degli artefatti, manifest003 ed ELF di 180200 byte. Non ha riverificato tutti i 97 percorsi storici. Nessuna traccia PID indipendente GCC/readelf; NEEDED non dimostra la risoluzione delle librerie, confronto completo delle versioni simbolo non concluso e nessun PASS licenze. Nessun worker o caso FFT eseguito dalla review.

- `M1-a-pocketfft-runtime-inventory-003.md`, `M1-a-pocketfft-runtime-inventory-003-review.md`: inventario statico e review indipendente conclusa; coerenza manuale di cinque runtime, sei ELF e 13 NEEDED, sette definizioni di versione del loader distinte dalle sonde STAP. Restano MED sulla ricostruzione indipendente del verificatore e negativi per omissioni, LOW sulla diagnosi001 trascritta. Sei record autenticati successivamente dal coordinatore, non runtime host o tutta la storia. Nessuna prova relocation/live binding, qualifica numerica o conformità distributiva; 003 immutabile.

- `M1-a-pocketfft-runtime-followup-004-preparation.md`: verificatore indipendente e 21 test predisposti in namespace separato; sola preparazione AST/hash, senza PASS dei test o checksum host. MED e gap LOW non chiusi dalla consegna.
- `M1-a-pocketfft-runtime-followup-004-tests.md`: unico turno di 21 test, 12 PASS, 8 failure, 1 errore, 0 skip; STOP_TEST_FAILURE, exit 1 e PID timeout 955456 raccolto. Diagnosi `VerificationError: version header grammar` in `verify.py:151`; MED003 aperto. L'autore riporta 23 record autenticati e hash pre/post identici, non una verifica dei runtime host. Nessun retry, FFT, binding live o PASS licenze.

- `M1-a-pocketfft-runtime-followup-005-tests.md`: 43 test storici riusciti, 5 positivi e 38 negativi; nessuna esecuzione FFT.
- `M1-a-pocketfft-runtime-followup-005-verification.md`: STOP_PREFLIGHT del tentativo001, verificatore non avviato; prova fallita conservata.
- `M1-a-pocketfft-runtime-followup-005-verification-002.md`, `M1-a-pocketfft-runtime-followup-005-verification-002-review.md`: verifica e review concluse; MED003 chiuso nel solo ambito statico. Ricostruzione esatta dai raw, binding dei 43 test e 48 input pre/post secondo receipt; quattro record autenticati separatamente dal coordinatore. LOW stderr001 e tre negativi opzionali mancanti conservati. Nessuna prova di live binding, licenze, 232 casi FFT o PASS M1.

- `M1-a-pocketfft-validation-004.md`, `M1-a-pocketfft-validation-004-review.md`: 232/232 casi confermati dalla review indipendente; autenticazione separata di 16 file completata dal coordinatore. Unico turno, supervisor PID 1119979 exit 0 e raccolto secondo receipt; nessuna nuova osservazione OS. Mappa di sei file nel solo caso 088, non binding dei simboli. PASS limitato alla qualifica numerica e agli errori attesi, senza prestazioni, licenze, adozione o chiusura M1; report primario e storici immutati.

- `M1-a-pocketfft-validation-003.md`: FIRSTFAIL dell'outer, supervisor raccolto dopo segnale 9; tutti i 232 casi NOT_RUN, nessun fallimento numerico dedotto. Storico conservato, distinto dalla successiva validation004.
- `M1-a-pocketfft-runtime-observation-tests-004-validation.md`: due target budget PASS, poi suite37 PASS_MOCK37; solo mock, non 232 FFT né qualifica OS di ownership/mappe/loader. Nessuna ammissione runtime reale derivata dal record.
- `M1-a-pocketfft-benchmark-prep-followup.md`: 38 mock/unitari reali dell'autore, 22 ProtocolTests e 16 LifecycleTests, exit 0; nessuna build, worker o benchmark nativi. Il mandato era ambiguo sull'esecuzione mock: la dichiarazione iniziale di autorizzazione del proprietario non è attestata da questa indicizzazione. Esiti e hash precedono la patch del MED residuo; non ne provano la correttezza. Rettifiche in `M1-a-pocketfft-coordination-audit-001.md`.
- `M1-a-pocketfft-coordination-audit-001.md`: disposizioni su otto finding di fedeltà, due HIGH del checkpoint e limiti della ricostruzione storica; checkpoint invalido come ammissione. Conserva il PASS232 limitato e il PASS37 mock; distingue autenticazioni successive e 38 mock dalla patch ancora da verificare. Non certifica tutta la cronologia né chiude il gate scientifico.
- `M1-a-pocketfft-benchmark-primary-error-tests-001.md`: 47 mock PASS sul relativo SHA, senza riproduzione RED/GREEN; non chiudeva il successivo MED annidato. Storico immutato.
- `M1-a-pocketfft-benchmark-primary-error-chain-red-001.md`: FIRSTFAIL_UNEXPECTED della fixture, una sola write anziché due; nessuna perdita diagnostica dimostrata dal turno.
- `M1-a-pocketfft-benchmark-primary-error-chain-red-002.md`: REPRODUCED_KNOWN_MED con BufferedWriter e tre write; fallimento atteso del contratto diagnostico, non PASS del test. RED001 conservato.
- `M1-a-pocketfft-benchmark-primary-error-chain-green-001.md`: otto target poi suite55 PASS, exit0 senza skip o retry; 63 esecuzioni di 55 metodi unici. Registri originali e due wrapper esatti verificati; nessun nativo o benchmark.
- `M1-a-pocketfft-benchmark-primary-error-chain-review-001.md`: review indipendente PASS_SCOPED del codice e delle prove mock, MED annidato chiuso nel perimetro; autenticazione separata di 22 file dal coordinatore. Non ammette build, misure, licenze o chiusura M1.

- `M1-a-series001-rust-retire-mocks-001.md`: unico turno naturale di otto metodi e17 scenari mock PASS, nessun target Rust aperto o unlink reale; l'outer nuovo non è qualificato dai percorsi non esercitati.
- `M1-a-series001-rust-retire-review-001.md`: review indipendente PASS_SCOPEDCODE_MOCKS e autenticazioni separate; blocco operativo EACCES conservato. Non è READY_UNLINK né prova di durabilità o ammissione futura.

- `M1-f-pattern16m-contract-tests-002.md`: otto mock launcher e quattro unit del contratto JSON PASS, una sola esecuzione per fase; STOP001 conservato, zero producer RUN. Errata additive nel verbale seguente, senza modificare il primario.
- `M1-f-pattern16m-contract-review-002.md`: PASS_SCOPED indipendente e autenticazione separata di63 file unici; distingue contratto JSON da enforcement e qualifica generale. Corregge dimensioni stderr e descrizione della PREP producer, senza ammetterne l'esecuzione.

- `M1-f-pattern16m-producer-setup-green-001.md`, `M1-f-pattern16m-producer-setup-green-review-001.md`: unico regression test GREEN confermato PASS_SCOPED, byte-identico al RED; visibilità della causa, non attributi diagnostici esatti o enforcement. Baseline e test avversi non eseguiti, producer0 RUN.
- `M1-a-pocketfft-source-license-001.md`: PASS_SCOPED_SOURCE_LICENSE sui testi BSD-3-Clause locali; obbligo di conservare header completo e LICENSE. Non qualifica licenze runtime/ELF/packaging né sceglie la licenza del progetto.

I record distinguono fatti alla data della prova ed eventuali errata successive. Lo stato aggregato e la prossima azione restano in `docs/progress.md`.
