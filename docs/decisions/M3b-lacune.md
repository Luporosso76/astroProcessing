# M3.b, unità B0: decisioni sulle lacune L1…L6, L8…L11 e estensione di ADR-06 R2.r

Stato: **DECISO dal coordinatore** il 9 novembre 2026 (decisione tecnica delegata, `AGENTS.md`). Proposte dell'agente approvate per L1…L6, L8, L9, L11 e per l'estensione di R2.r; L10 decisa diversamente (opzione C, spill: vedi L10). Gli emendamenti ai contratti si applicano in B0 insieme a questo documento.

Fonti lette per il documento: `plans/M3b.md` (§5 B0, §8 e le unità citate), `docs/contracts/C-02-master.md` (C-02.b §b.1…§b.15, C-02.b-flat §f.2, §f.10, §f.13), `docs/contracts/F-IO-03-classificazione.md` (CL-05, CL-06, CL-11, CL-12), `docs/contracts/ADR-16-stati-master.md` §b.7, `docs/contracts/ADR-06-budget-scheduler.md` (estensione R2.r), `docs/contracts/BASE-confini.md` §3.1-§3.3. Codice letto nel worktree: `src/core/job_types.cpp`, `src/core/master.cpp`, `src/core/resources.cpp`, `src/core/batch_classification.cpp`, `src/core/raw_import_job.cpp` e le intestazioni `job_types.hpp`, `master.hpp`, `resources.hpp`, `batch_classification.hpp`, `calibration_analysis.hpp`, `raw_decode.hpp`. I risultati di `codegraph explore` vengono dallo snapshot del checkout principale; ogni fatto di codice qui sotto è stato riletto nel worktree.

Criteri di scelta, in quest'ordine: (1) nessun fallback, stop tipizzati con provenance (`AGENTS.md`); (2) base comune riusata, non rifatta (`plans/M3.md`, `CLAUDE.md` §2.1); (3) riproducibilità bit-esatta e chiave del nodo stabile dopo la riapertura (C-02.b §b.8); (4) minima superficie nuova.

Riepilogo:

| Lacuna | Proposta in una frase | Emendamento di contratto |
|---|---|---|
| L1 | Una seconda voce del registro per i frame RAW, varianti per ruolo e metodo con tre frame; la cardinalità variabile non entra nel registro | sì (BASE §3.1, una frase) |
| L2 | Un lettore di statistiche per frame (lettura verificata) separato dall'analisi, che resta una funzione pura | sì (C-02.b §b.2) |
| L3 | Funzione propria in `master_analysis.cpp`; `batch_proposal.v1` invariato (salvo `flat_thermal_pending_master`, decisione di B14); i nodi proposti entrano in `analyze_batch` come candidati | sì (C-02.b §b.2, F-IO-03 CL-06) |
| L4 | Stessa chiave `batch.analysis_params`, nuovo membro opzionale `master`; parametro assente = esito `params_missing` per ruolo | sì (F-IO-03, C-02.b §b.2) |
| L5 | Il peso si lega al digest del frame, scritto nella voce del frame del nodo | no |
| L6 | «Set parziale» = sottoinsieme scelto dall'utente dopo le esclusioni per frame; nessun conteggio di frame mancanti | sì (una frase di chiarimento in C-02.b §b.2) |
| L7 | Chiusa: conferma letterale | no |
| L8 | L'esito dell'analisi sta nei parametri del nodo (parametro `analysis`) | sì (C-02.b §b.2, §b.8, §b.10) |
| L9 | `offset_adu` dei RAW è dichiarato nella ricetta del nodo; l'analisi lo riceve come parametro; nessun legame col ledger `light_offset` | no |
| L10 | Decisione C: decodifica in `prepare`, un frame alla volta, nello spill dichiarato del job; quota `n·S·6` nel pianificatore, non memoria del pool; senza spill stop prima di ogni lettura | sì (ADR-06 R2.r, «Frame RAW tramite spill»; C-02.b §b.2 «RAW») |
| L11 | Si segue il testo attuale: nessun codice nuovo, nessun parametro opzionale, nessun avviso nuovo | no |

---

## L1. Registro dei tipi di job per `master.combine`

**Domanda (dal piano).** Un solo tipo di job con ingressi variabili, o una variante per ruolo o per metodo? Come si dichiara la cardinalità nella suite di conformità?

**Fatti.**
- Codice: `registered_job_types()` in `src/core/job_types.cpp` registra `master_operator_id` con `input_roles` `{"frame0", "frame1", "frame2"}`, `fits_roles(...)` e una variante `{"unica", false, true}`. `make_master` costruisce una `master_recipe` con ruolo `dark`, `mean`, `none`, tre frame.
- Codice: `job_type_entry` ha un solo `input_roles` e una sola mappa `role_formats` per voce (`job_types.hpp`). La variante (`job_type_variant`) porta solo nome, `checkpoint`, `scratch`.
- Codice: `run_master_job` accetta una `master_recipe` con un numero qualunque di frame (`recipe.frames`, `p.n`): il job reale non ha cardinalità fissa. La fissa è solo quella del costruttore minimo.
- Contratto: BASE §3.1 punto 2: «Ogni voce porta un costruttore del job minimo. Dagli input sintetici della suite [...] costruisce il nodo e una chiamata del job». BASE §3.2: «Ogni caso gira per ogni voce del registro e per ogni variante». T-M3BASE-11 gira «per ogni ruolo di input»; T-M3BASE-22 verifica il formato dichiarato per ogni voce e ruolo.
- Codice: `tools/check_job_types.py` confronta insiemi di `operator_id` (regex `ENTRY` su `{x_operator_id,`): due voci con lo stesso operatore non lo rompono. Il nome del test è `operator_id` + `_` + nome della variante (`test_conformance.cpp`, riga 246): i nomi delle varianti devono essere distinti fra voci dello stesso operatore.
- Codice: i frame RAW hanno un formato diverso (`raw_input_format()`), dichiarato per ruolo nella voce. Con una sola voce non si possono avere ruoli FITS e ruoli RAW dello stesso operatore.

**Opzioni.**
- A. Una voce, ingressi variabili: richiede che `input_roles` dipenda dalla variante. Cambia la struttura del registro e la suite (`write_inputs`, T-M3BASE-11 e 22). Superficie nuova grande.
- B. Una voce per formato dei frame (FITS, RAW), stesso operatore, con `frame0…frame2` e varianti per ruolo e metodo ammessi con tre frame. La cardinalità resta quella dei ruoli dichiarati (3). La variabilità di `n`, ruolo e metodo è coperta dai test del prodotto (T-M3B-C02-xx, T-M3B-MEM-01), che chiamano lo stesso `run_master_job`.
- C. Una voce per ruolo o per metodo: moltiplica le voci e i test di conformità senza aggiungere ostacoli nuovi (le obbligazioni OB-01…OB-20 non dipendono dal metodo).

**Proposta: B.** Motivo: il registro dichiara il job *minimo valido*, non lo spazio dei parametri (BASE §3.1). La suite controlla le obbligazioni del confine; `n`, ruolo e metodo non le cambiano. L'unica differenza reale per il confine è il formato dei ruoli (FITS contro RAW), e una seconda voce la esprime senza toccare la struttura. Le varianti proposte, con tre frame: `unica` (oggi: `dark`, `mean`), `dark_mediana`, `flat_media` per la voce FITS; `raw_dark_mediana` per la voce RAW (ruoli `frame0…frame2` con `raw_input_format()`). Non è una scelta di metodo del prodotto: sono istanze di prova.

**Emendamento proposto.** BASE-confini §3.1, punto 1, aggiungere in fondo la frase: «Un operatore può avere più voci quando i formati dei suoi ruoli differiscono; le varianti hanno nomi distinti fra le voci dello stesso operatore, perché il nome di un caso è l'operatore più la variante. La voce dichiara un job minimo valido: la variabilità dei parametri del job è coperta dai test dell'operatore, non dal registro.»

Applicato in `docs/contracts/BASE-confini.md` §3.1, punto 1 (in fondo).

**Conseguenze.**
- B16: due voci in `job_types.cpp`, costruttore minimo per la voce RAW; nessun cambio di `job_type_entry`.
- T-M3B-JOB-01: la dicitura del piano «N variabile e ruolo variabile nella suite» diventa «istanze minime con tre frame per ruolo e metodo scelti, FITS e RAW»; la cardinalità variabile è provata da T-M3B-C02-10 e T-M3B-MEM-01.
- T-M3BASE-11, 22 e la suite: nessun cambio di codice, due voci in più. T-M3A-CAL-09 (parte master) usa la voce RAW.

---

## L2. Chi legge i campioni dell'analisi dei master

**Domanda.** C-02.b §b.2 dice che l'analisi «legge i campioni solo per la frazione satura e per i livelli `m_(f,c)` del flat». `analyze_batch` non legge campioni salvo `flat.bias`. Chi legge, con quale confine di lettura verificata (OB-11), budget e cancel? L'analisi resta pura?

**Fatti.**
- Contratto: F-IO-03 CL-12: `analyze_batch` è «una funzione pura»; «Nessuna lettura di campioni, tranne quella del controllo termico di `flat.bias`».
- Codice: `batch_input::load_samples` (`batch_classification.hpp`) è l'unico canale di lettura; per i FITS è `load_full_image(path, digest, pool, cancel)` (`batch_classification.cpp`), che usa `open_fits_input`, `plan_fits`, `read_verified_input` (OB-11) e `decode_fits`, e controlla `cancel`. È impostato solo per ruoli `flat` e `bias`.
- Codice: per i RAW `load_samples` non è impostato: l'osservazione viene dal record del decoder del nodo d'import (`batch_classification.cpp`, ramo dopo `rec.format == fits_input_format().prefix`). Nessun RAW ha un lettore di campioni nell'analisi.
- Contratto: C-02.b §b.2: «L'uscita è una funzione pura degli ingressi»; C-02.b §b.8: «Analisi: funzione pura degli ingressi, indipendente dall'ordine in cui i candidati arrivano».

**Opzioni.**
- A. L'analisi dei master chiama `load_samples` (estende `analyze_batch` e rompe CL-12 per ogni ruolo).
- B. Un lettore separato produce per frame una piccola struttura di statistiche (campioni saturi e totale; `m_(f,c)` per canale); l'analisi pura le riceve come ingresso, come riceve le osservazioni.
- C. Le statistiche si calcolano al primo uso e si persistono come artefatto d'analisi (cache). Superficie nuova: nuovo tipo di artefatto e sua invalidazione.

**Proposta: B.** Motivo: lascia `analyze_batch` e CL-12 intatti; l'analisi resta funzione pura (§b.8); il lettore è un confine di lettura con le garanzie esistenti. Il lettore (nome indicativo `read_master_frame_stats`) legge un frame alla volta: FITS con lo stesso percorso di `load_full_image`; RAW con `open_input(path, raw_input_format())`, `read_verified_input` e `decode_raw_buffer` con `raw_decode_context{&pool, &cancel, ...}` (come `raw_import_job.cpp`). Digest o dimensione diversi: stop `integrity` dei codici di OB-11, nessuna statistica parziale. Cancel controllato prima di ogni frame. Il lettore legge solo se serve: `saturation_adu` dichiarato (frazione satura) oppure ruolo `flat` (livelli). Le statistiche sono funzioni deterministiche dei byte verificati: la proposta non cambia se i candidati sono permutati.

**Emendamento proposto.** C-02.b §b.2, paragrafo «Analisi del set candidato», sostituire «L'analisi legge header, metadati e stato del decoder, e legge i campioni solo per la frazione satura e per i livelli `m_(f,c)` del flat.» con: «L'analisi legge header, metadati e stato del decoder. I campioni, solo per la frazione satura e per i livelli `m_(f,c)` del flat, li legge un'operazione separata del confine di lettura verificata (OB-11, con pool e cancel), che restituisce per ogni frame l'esito di decodifica (ok, oppure `decode_failed` con il dettaglio e il codice di ADR-08.b §2.5; gli stop di integrità OB-11 restano stop e non diventano un'esclusione), il numero di campioni saturi, il numero totale di pixel (`width·height`, denominatore di `saturated_fraction` di C-02.a) e i livelli `m_(f,c)` per canale, assenti per un canale senza pixel validi; l'analisi riceve queste statistiche come ingresso. `analyze_batch` (F-IO-03 CL-12) non cambia.»

Applicato in `docs/contracts/C-02-master.md` §b.2, paragrafo «Analisi del set candidato (prima della scelta)».

**Conseguenze.** B12 (esclusione `saturated_fraction`, `flat_low_signal`) e B13 (livelli del flat) ricevono le statistiche in ingresso; i loro test (T-M3B-C02-08) usano statistiche scritte a mano. Un'unità di lettura nuova (non prevista dal piano) sta dentro B12 per FITS e dentro B15a/B15 per RAW; la riverifica OB-11 dei frame è coperta da T-M3BASE-11 solo se il lettore è raggiunto dal registro: non lo è (non è un job), quindi serve una prova propria dentro B12 (stop `input.digest_mismatch` e `input.missing`, cancel). Da dichiarare nel record di B12 come test aggiunto, non come ID nuovo di contratto.

---

## L3. Dove vive l'API dei gruppi e dei master proposti

**Domanda.** Funzione propria o estensione di `analyze_batch`? `astro.batch_proposal.v1` cambia schema o versione? Come si espone un gruppo di master accanto ai gruppi di light?

**Fatti.**
- Contratto: F-IO-03 CL-05: i frame di un ruolo senza `master_descriptor` sono elencati «con l'indicazione `frame_pending_master`; i gruppi e i master proposti li calcola C-02.b (M3.b)». F-IO-03 CL-06 (c): «da M3.b, un nodo `master.combine` proposto dall'analisi di C-02.b, con lo stesso descrittore derivato». «Le tre fonti consegnano la stessa forma [...] Per questo (c) entra senza cambiare il contratto.»
- Codice: `batch_proposal` ha `groups` (light), `excluded`, `frame_pending_master` (ruolo → digest); `canonical()` scrive `astro.batch_proposal.v1` con `frame_pending_master` (`batch_classification.cpp`, funzione `batch_proposal::canonical`). `analyze_batch(inputs, slots, request, fault)` accetta `slots` di tipo `slot_candidate` (`node_key`, ruolo, descrittore, osservazione, `load_samples`).
- Codice: `analyze_batch` oggi mette in `frame_pending_master` ogni frame di ruolo di calibrazione senza descrittore e non lo valuta.
- Non letto: come `select_recipe` e `verify_selection` trattano una `node_key` che non è uno slot `current`. B14 lo verifica prima di scrivere il codice.

**Opzioni.**
- A. Estendere `analyze_batch` e `batch_proposal`: serve ricevere le statistiche di L2 (nuovo parametro) e cambiare lo schema (v2) con `master_groups`. Rompe i test di M3.a sullo schema v1.
- B. Funzione propria in `src/core/master_analysis.cpp` (nome indicativo `propose_masters`), funzione pura di: osservazioni dei frame di `frame_pending_master`, statistiche di L2, parametri d'analisi (L4), scelte dell'utente. Uscita: `master_proposal` con JSON canonico `astro.master_proposal.v1` (un documento nuovo, versione 1). I nodi proposti si passano ad `analyze_batch` come `slot_candidate` nel parametro `slots` già esistente.
- C. Come B, ma con un nuovo tipo di candidato in `analyze_batch`. Superficie nuova senza vantaggio rispetto al riuso di `slot_candidate`.

**Proposta: B.** Motivo: `frame_pending_master` è già il contratto di consegna (CL-05); `analyze_batch` e `astro.batch_proposal.v1` non cambiano; i gruppi di master stanno in un documento a parte, collegato ai light solo tramite il candidato (CL-06 (c)). Un gruppo di master accanto ai gruppi di light: il chiamante (UI/CLI) legge i due documenti; le ricette dei light vedono i master proposti come candidati dello stesso ruolo.

**Emendamento proposto.**
- C-02.b §b.2, in coda al paragrafo «Selezione ed esecuzione», aggiungere: «I gruppi e i nodi proposti li calcola una funzione propria, pura, che riceve i frame elencati in `frame_pending_master`, le loro statistiche, i parametri d'analisi e le scelte dell'utente, e restituisce il documento canonico `astro.master_proposal.v1`. I nodi proposti entrano in `analyze_batch` come candidati di tipo (c) di F-IO-03 CL-06, con la stessa forma di un candidato da slot. L'osservazione (`slot_candidate::observation`) del candidato (c) si costruisce dal frame di riferimento del gruppo (primo in ordine canonico) con le regole dei metadati comuni di C-02.a §2; il candidato (c) non ha `load_samples`. La verifica termica `flat.bias` (ADR-16.b §b.5) con F o B candidato (c) non si esegue alla proposta: l'esito è dichiarato, `thermal_check_pending_master`, mai omesso in silenzio; dopo l'esecuzione del master la selezione si verifica di nuovo con `verify_selection`.»
- F-IO-03 CL-06 (c), aggiungere: «Il candidato (c) è passato a `analyze_batch` con la stessa struttura del candidato (b); la sua chiave è la chiave del nodo proposto. L'osservazione (`slot_candidate::observation`) del candidato (c) si costruisce dal frame di riferimento del gruppo (primo in ordine canonico) con le regole dei metadati comuni di C-02.a §2; il candidato (c) non ha `load_samples`. La verifica termica `flat.bias` (ADR-16.b §b.5) con F o B candidato (c) non si esegue alla proposta: l'esito è dichiarato, `thermal_check_pending_master`, mai omesso in silenzio; dopo l'esecuzione del master la selezione si verifica di nuovo con `verify_selection`.»

Applicato in `docs/contracts/C-02-master.md` §b.2, paragrafo «Selezione ed esecuzione» (in coda), e in `docs/contracts/F-IO-03-classificazione.md` CL-06 (c) (anche per l'osservazione e la verifica termica del candidato (c)).

**Conseguenze.** B14: la funzione e il documento; `test_batch_analysis` e `test_verify_selection` cambiano solo dove usano `frame_pending_master` come uscita finale (F-IO-03 CL-11): lo schema `batch_proposal.v1` non cambia, quindi i loro confronti canonici restano. T-M3B-ANA-01 verifica l'uguaglianza con l'oracolo a mano e l'indipendenza dall'ordine dei candidati. Nuova verifica da aggiungere a B14 (non è un ID di contratto): `verify_selection` e `select_recipe` su un candidato (c).

---

## L4. Parametri d'analisi dei master

**Domanda.** Stanno nella stessa chiave `batch.analysis_params` dei light o in una propria? Cosa fa l'analisi se ne manca uno (stop o gruppo `params_missing`)?

**Fatti.**
- Codice: `read_analysis_params` (`batch_classification.cpp`) legge `settings` `batch.analysis_params`, formato `astro.batch_analysis_params.v1`; chiavi note: `exposure_rel_tol`, `temperature_abs_tol_c`, `temperature_policy`, `decoder_black_abs_tol_adu`, `normalization`, `q_min`, `q_rel_sigma_max`, `flat_thermal_bound_adu`, `flat_thermal_rel_budget`. Una chiave sconosciuta è `invalid_input` `batch.analysis_params` («chiave sconosciuta»). Assente = nessuna dichiarazione.
- Codice: `calibration_declarations` (`calibration_analysis.hpp`) contiene già `exposure_rel_tol`, `temperature_abs_tol_c`, `temp_policy`, `decoder_black_abs_tol_adu`: quattro dei parametri dei master sono gli stessi dei light.
- Contratto: F-IO-03 CL-07/CL-09: `params_missing` è un esito della proposta di un gruppo di light, con i nomi dei parametri assenti, «senza ricette e senza default». «Gli esiti [...] non sono errori: sono esiti della proposta.»
- Contratto: C-02.b §b.2: i parametri d'analisi sono `exposure_rel_tol`, `temperature_abs_tol_c`, `temperature_policy`, `saturation_adu`, `saturated_fraction_max`, per il flat `flat_min_signal_adu` e `flat_level_rel_tol`, per i RAW `decoder_black_abs_tol_adu`, per il flat RAW e per `single` `offset_adu`. «Una soglia del frame presente con `saturation_adu` assente ferma l'analisi con `invalid_input`, `master.param.saturation_adu`, come in `.a`.»
- Codice: `master_recipe` (`master.hpp`): `saturation_adu` assente dà l'avviso `saturation_not_evaluated`; `saturated_fraction_max` è obbligatorio con `saturation_adu`.

**Opzioni.**
- A. Chiave propria `batch.master_analysis_params`. Dichiara due volte le quattro tolleranze comuni: rischio di valori diversi per lo stesso batch.
- B. Stessa chiave, un membro opzionale `master` (oggetto con i parametri propri dei master); le quattro tolleranze comuni restano al livello superiore, una sola dichiarazione. Il formato resta `astro.batch_analysis_params.v1`: nessuna chiave esistente cambia significato. Il lettore continua a fermarsi su chiavi sconosciute, dentro e fuori da `master`.
- C. Come B con formato `v2`. Rompe le fixture e le impostazioni di M3.a senza guadagno.

Assenza di un parametro: (i) stop per l'intera analisi; (ii) esito `params_missing` per ruolo con l'elenco dei nomi, senza proposta per quel ruolo; (iii) default. (iii) è vietato.

**Proposta: B con (ii).** Un ruolo con parametri richiesti mancanti dà l'esito `params_missing` con i nomi, nessun gruppo proposto per quel ruolo; gli altri ruoli procedono (uno stop bloccherebbe bias e dark per un flat senza `flat_level_rel_tol`). Eccezione dettata dal contratto: frame con soglia propria e `saturation_adu` assente ferma con `invalid_input`, `master.param.saturation_adu`. Parametri richiesti per ruolo, dal contratto: tutti i ruoli `exposure_rel_tol`, `temperature_abs_tol_c` e `temperature_policy` (come ADR-16.a §2 per i light); RAW `decoder_black_abs_tol_adu`; `saturated_fraction_max` solo se `saturation_adu` presente; flat `flat_min_signal_adu` e `flat_level_rel_tol`; flat RAW e `single` da RAW `offset_adu`.

**Emendamento proposto.**
- F-IO-03, voce «I parametri d'analisi stanno in `settings`, chiave `batch.analysis_params`», aggiungere: «Il documento può contenere un membro `master` con i parametri dell'analisi dei master di C-02.b §b.2 che non sono dei light (`saturation_adu`, `saturated_fraction_max`, `flat_min_signal_adu`, `flat_level_rel_tol`, `offset_adu`); le tolleranze comuni stanno al livello superiore e valgono per light e master.»
- C-02.b §b.2, dopo l'elenco dei parametri d'analisi: «Un parametro richiesto per un ruolo e assente dà, per quel ruolo, l'esito `params_missing` con i nomi, senza proposta e senza default; fa eccezione `saturation_adu` (stop `master.param.saturation_adu`, sopra).»

Applicato in `docs/contracts/F-IO-03-classificazione.md` §7, voce «I parametri d'analisi stanno in `settings`», e in `docs/contracts/C-02-master.md` §b.2, paragrafo «Analisi del set candidato (prima della scelta)».

**Conseguenze.** B12 estende `read_analysis_params` e la `calibration_declarations` solo per il lato dei master (struttura propria, non nelle dichiarazioni congelate di CL-11: i campi dei master non sono «applicabili alla ricetta» del light). T-M3B-C02-08/09 aggiungono un caso per parametro assente (uno alla volta) e per chiave sconosciuta in `master`.

---

## L5. Pesi di `weighted_mean`

**Domanda.** A quale frame si legano i pesi (digest, posizione nell'ordine canonico, `input_id`)? Come entrano nella chiave del nodo? Due frame con lo stesso digest?

**Fatti.**
- Codice: `canonical_order` (`master.cpp`) ordina i frame per digest SHA-256 crescente (esadecimale minuscolo). Il controllo di unicità (`master.cpp`, dopo `validate_params`) ferma due frame con lo stesso digest: `unsupported`, `calib.shared_input`.
- Codice: `make_master_node` scrive i frame in ordine canonico sotto `params["frames"]`, una voce per frame (`declared_metadata`, `declared_fields`, `fits_read_options`). I numeri f64 del JSON canonico usano `std::to_chars` in forma scientifica (`json.cpp`).
- Contratto: C-02.b §b.10: «Pesi assenti, in numero diverso dai frame, non finiti o ≤ 0; [...] pesi con un metodo diverso da `weighted_mean`» → `invalid_input`, `master.param.weights`. C-02.b §b.2: «Il nodo contiene esattamente i frame scelti [...] con metodo, parametri, pesi».
- Contratto: F-IO-03 CL-12: gli `input_id` sono identità locali che dipendono dall'ordine di registrazione, esclusi dal confronto canonico.

**Opzioni.** (A) `input_id`: non stabile, escluso. (B) Posizione nell'ordine canonico: stabile solo se l'insieme non cambia, e il chiamante dovrebbe prevedere l'ordine dei digest. (C) Digest del frame: l'identità che già determina l'ordine e la chiave.

**Proposta: C.** Il peso è un campo `weight` della voce del frame nel nodo (`frames[k].weight`, `null` per i metodi senza pesi); nella ricetta è un campo opzionale di `master_frame_spec`. La posizione deriva dal digest, quindi i due legami coincidono; permutare i candidati non cambia la chiave. «In numero diverso dai frame» si legge come: frame con peso diverso da tutti i frame; il contesto dello stop riporta i due conteggi. Due frame con lo stesso digest: lo stop esistente `calib.shared_input` arriva prima di ogni controllo dei pesi; nell'analisi la copia è esclusa come `duplicate_digest` e nessun peso è proposto (i pesi non si propongono mai, C-02.b §b.4).

**Emendamento di contratto: no.** C-02.b §b.2 e §b.10 sono rispettati alla lettera.

**Conseguenze.** B3: campo `weight` in `master_frame_spec`, serializzazione nella voce del frame, `master_algorithm_version` aumentata (già prevista). T-M3B-C02-03(c): pesi assenti su un solo frame, in tutti, presenti con `mean`; T-M3B-DSC-01: chiave invariata per permutazione dei frame.

---

## L6. «Set parziali»

**Domanda.** Che cosa vuol dire «gestire set parziali»: (a) sottoinsieme scelto dall'utente, (b) gruppo con meno frame del minimo del metodo, (c) altro? Chi dichiara quanti frame mancano?

**Fatti.**
- Piano, F-CAL-01 riga 1840: «gestisce set parziali e gruppi incompatibili». C-02.a, intestazione: «set parziali e gruppi misti appartengono a F-CAL-01.b (M3.b)». C-02.a «Fuori da `.a`»: «Media pesata, winsorized, set parziali, gruppi misti.»
- Contratto: C-02.b §b.2: «un gruppo o un suo sottoinsieme, scelto dall'utente»; «Nessun frame è escluso per livello [...] un sottoinsieme lo sceglie l'utente»; il report dell'analisi elenca ogni frame `used` o `excluded` con il motivo (§b.7).
- Contratto: C-02.b §b.4: la proposta non propone un metodo fuori dominio (`n = 1`: `single` o nessuna proposta; `n = 2`: `mean`); §b.10: `n` fuori dominio → `master.variance_domain`.
- Nessun testo definisce un numero atteso di frame né un parametro che lo dichiari.

**Opzioni.** (a) e (b) sono già coperti dal contratto, in punti diversi. (c) un conteggio atteso dichiarato dall'utente: parametro nuovo, non richiesto.

**Proposta.** Per questo prodotto «set parziale» è l'insieme dei frame che resta dopo le esclusioni per frame (§b.2 punto 1) e, in più, ogni sottoinsieme di un gruppo scelto dall'utente; il report dichiara candidati, usati ed esclusi con motivo. Un gruppo troppo piccolo per un metodo non si «completa»: la proposta non lo propone (§b.4) e l'esecuzione si ferma (§b.10). Nessuno dichiara «frame mancanti»: il prodotto non conosce un numero atteso e non lo inventa.

**Emendamento proposto.** C-02.b §b.2, in coda al paragrafo «Selezione ed esecuzione»: «Per «set parziali» (F-CAL-01) questo contratto intende: l'insieme dei frame rimasti dopo le esclusioni per frame e ogni sottoinsieme di un gruppo scelto dall'utente. Il prodotto non conosce né dichiara un numero atteso di frame; un gruppo con meno frame del minimo del metodo non ha proposta per quel metodo e, se scelto, si ferma con `master.variance_domain`.»

Applicato in `docs/contracts/C-02-master.md` §b.2, paragrafo «Selezione ed esecuzione» (in coda).

**Conseguenze.** B14: nessun campo nuovo. T-M3B-C02-09 già copre `n ∈ {1, 2, 3, 1024, 1025}` e gli stop di dominio; T-M3B-C02-08 la selezione di un sottoinsieme (preflight che passa, nodo con esattamente quei frame).

---

## L7. Chiusa: conferma letterale

C-02.b §b.1, tabella dei metodi, riga `weighted_mean`: colonna `n`: «≥ 3 e `2 max_f w_f < W`». Riga `mean` + `sigma_clip`: colonna `n`: «≥ 3, non inerte». Nessuna delle due riporta un massimo. C-02.b §b.2 (tabella dei domini): nessun `n` massimo. L'unico tetto è per `median` e `winsorized`: §b.1, colonna `n`: «3 … 1024» e «`2g + 1` … 1024»; §b.2: «Il limite `n ≤ 1024` dei metodi robusti è il dominio entro cui `κ` è verificato». Nota: il piano cita §b.4 per il limite 1024; §b.4 (tabella della proposta) riporta solo «> 1024 | `mean` + `none`». Nessun tetto nuovo per `sigma_clip` e `weighted_mean`.

---

## L8. L'esito dell'analisi: chiave, record separato o provenance

**Domanda.** L'esito dell'analisi sta nei parametri del nodo (chiave), in un record separato collegato, o solo nella provenance al momento dell'esecuzione? Quale garantisce «ricostruito dopo la riapertura, stessa chiave e stesso artefatto» (§b.8)?

**Fatti.**
- Contratto: C-02.b §b.2: «Il nodo `master.combine` contiene esattamente i frame scelti [...] con metodo, parametri, pesi e, per i RAW, `frame_decoder_states`». §b.8: «il nodo contiene frame, metodo, parametri, pesi e stati del decoder. Ricostruito dopo la riapertura dello store, dà la stessa chiave e lo stesso artefatto». §b.10: «La provenance del nodo contiene l'esito dell'analisi (esclusioni e gruppi) e le costanti del preflight».
- Contratto: C-02.b §b.7, report: la sezione «Analisi» elenca i frame candidati `used` o `excluded` con motivo e valori, i gruppi, la proposta. L'artefatto `report` è parte dell'output del job (`master.cpp`: `files` con `master_report_role`).
- Codice: `make_master_node` costruisce la chiave dai parametri canonici e dagli ingressi; il job non esegue l'analisi. `observed_inputs` entra solo nei `details` del job (`master.cpp`, fine di `run_master_job`).
- T-M3B-C02-10: candidati permutati e riapertura → «stessa chiave del nodo e stesso digest dell'artefatto».

**Ragionamento.** Il report è un artefatto e contiene frame esclusi, che non appartengono al nodo. Se l'esito non fosse nella chiave, due esecuzioni con la stessa chiave e analisi diverse darebbero report diversi: viola §b.8. Un record separato ha lo stesso problema, salvo che il suo digest entri nella chiave: sarebbe l'opzione A con un'indirezione in più. La sola provenance non determina il report.

**Opzioni.**
- A. Parametro `analysis` del nodo: il JSON canonico dell'esito (candidati con stato e motivi, gruppo del nodo, riferimento, stato del gruppo, proposta, adesione alla proposta). Entra nella chiave.
- B. Record separato, riferito dal nodo con il suo digest: stessa dipendenza di A, un artefatto in più.
- C. Solo nei `details` del job: non determina il report; viola §b.8.

**Proposta: A.** Costo accettato: due selezioni degli stessi frame da insiemi di candidati diversi sono nodi diversi (stesso pixel, provenienza diversa): è coerente con il fatto che il report le descrive in modo diverso. Un nodo non nato da una proposta ha `analysis: null` esplicito e il report dice «analisi non eseguita». Le costanti del preflight (`κ`, `a_f`, `c_f`, `m_(f,c)`, `r_(f,c)`, `m_H,c`, `ρ_c`) restano derivate da parametri e campioni: stanno nei `details` del job (e nel report), non nella chiave. Il parametro `analysis` è un sottoinsieme di `astro.master_proposal.v1` (L3): la funzione di proposta lo produce; il job lo legge dalla ricetta senza rieseguire l'analisi.

**Emendamento proposto.**
- C-02.b §b.2, «Selezione ed esecuzione»: dopo «con metodo, parametri, pesi e, per i RAW, `frame_decoder_states` (ADR-16.b §b.7)» aggiungere «e l'esito canonico dell'analisi da cui il nodo nasce (parametro `analysis`; `null` per un nodo non proposto)».
- C-02.b §b.8, «Riproducibilità dal progetto»: «il nodo contiene frame, metodo, parametri, pesi, stati del decoder e l'esito dell'analisi».
- C-02.b §b.10, ultima frase del paragrafo degli avvisi: sostituire «La provenance del nodo contiene l'esito dell'analisi (esclusioni e gruppi) e le costanti del preflight (`κ`, `a_f`, `c_f`).» con «L'esito dell'analisi (esclusioni e gruppi) sta nei parametri del nodo e quindi nella sua provenance; le costanti del preflight (`κ`, `a_f`, `c_f`) stanno nei `details` del job.»

Applicato in `docs/contracts/C-02-master.md` §b.2 «Selezione ed esecuzione», §b.8 «Riproducibilità dal progetto» e §b.10 (paragrafo degli avvisi).

**Conseguenze.** B3: parametro `analysis` nel nodo e nella ricetta (opaco: oggetto JSON canonico, validato solo come oggetto o `null`). B14: produzione del parametro. B16: il job lo legge dal nodo. T-M3B-C02-10: candidati permutati → stesso `analysis`, stessa chiave; T-M3B-DSC-01: la chiave cambia se cambia `analysis`. La dimensione del parametro cresce con il numero dei candidati: nessun limite nuovo.

---

## L9. `offset_adu` dei RAW

**Domanda.** Lo dichiara l'analisi, la selezione o la ricetta del nodo? Come si lega al ledger `light_offset` di G-OFFSET?

**Fatti.**
- Contratto: C-02.b §b.2, paragrafo «RAW»: «`offset_adu` è il valore dichiarato nella ricetta, unico per il nodo: non si confronta fra frame [...] Serve solo a `m_(f,c)` del flat e al modello di rumore di `single`. Il nero del decoder non si sottrae». §b.7: «Per i RAW, `offset_adu` è il valore dichiarato, con origine `declared`». §b.2 parametri d'analisi: per il flat e per `single` «`offset_adu`, dichiarato una volta per l'insieme».
- Codice: per i FITS `offset_adu` è metadato per frame (`frame_metadata::offset_adu`, `image.hpp`) e `compare_optional_fields` lo confronta fra frame (`master.cpp`); `p.offset` è quello del frame di riferimento. Per i RAW non esiste.
- Contratto: C-01.b §10: la voce `light_offset` riporta lo stato dell'offset del *light*, il master che lo rimuove e il nero effettivo del light RAW. ADR-16.b §b.7: il confronto di `offset_adu` non si applica con stato del decoder.

**Opzioni.** (a) analisi: parametro del membro `master` (L4), copiato nella ricetta; (b) selezione: stesso valore ma congelato altrove; (c) ricetta: il valore sta nei parametri del nodo (e quindi nella chiave).

**Proposta: seguire il contratto.** Lo dichiara l'utente come parametro d'analisi `master.offset_adu` (L4), l'analisi lo riporta nel gruppo e nell'esito, il nodo lo porta come parametro della ricetta (origine `declared`) e quindi nella chiave. Nessun legame col ledger `light_offset`: il ledger descrive il light e il nero che lo riguarda; `offset_adu` del master serve ai livelli del flat e al rumore di `single`. Il contratto risponde già: nessun emendamento.

**Conseguenze.** B12: parametro d'analisi; B15a: campo di ricetta e stop `master.metadata.offset_adu` (`metadata_missing`) se assente con RAW nel flat o in `single`; T-M3B-C02-08, T-M3B-CFA-01.

---

## L10. Lettura dei frame RAW nel job del master

**Domanda.** Decodifica intera del frame e poi fasce? Quale confine di lettura verificata e budget del pool? Come entra nel pianificatore di B3b?

**Fatti.**
- Codice: `run_master_job` legge i FITS a fasce di righe con `read_fits_rows` nei task (`pread`) e in `prepare` decodifica ogni frame per intero, uno alla volta, per il preflight; in `finalize` riverifica con `reverify_input`. La pila non è mai residente (ADR-06 R2.r, D8).
- Codice: i RAW non hanno lettura per fasce: `decode_raw_buffer(bytes, hooks, ctx)` decodifica un file intero, in un solo thread (ADR-08.b §2.7), con `raw_decode_context{pool, cancel, max_pixels, held_bytes}`; con `pool` il decoder si ferma con `sched.job_exceeds_budget` se `held_bytes` più la prenotazione superano il budget.
- Contratto: ADR-16.b §b.7: stato del decoder dalla decodifica dell'analisi; stato diverso alla nuova decodifica: `integrity`, `master.decoder_state_changed`; RAW e FITS insieme: `incompatible`, `master.decoder_state_mixed`.
- Codice: `decode_raw_buffer` produce un'immagine `f32` con maschera `uint16`: 4 + 2 byte per pixel.

**Opzioni.**
- A. Frame decodificati residenti per tutta l'esecuzione (`n·S·6` byte nel pool).
- B. Ridecodificare per ogni task: costo inaccettabile.
- C. Decodificare in `prepare` e scrivere ogni frame nello spill dichiarato del job, riletto a fasce.
- D. Usare gli artefatti FITS dei nodi `io.raw_import`: contraddice ADR-16.b §b.7.

**Proposta dell'agente: A. Decisione del coordinatore: C.** Motivo: con A la memoria vale `n·S·6` byte (50 dark da 24 MP: circa 7 GB), quindi i master da RAW di dimensione normale si fermerebbero per budget; D8 (pila mai residente) è un vincolo dell'ADR-06, non un'ottimizzazione. C non è superficie nuova: lo spill dichiarato esiste già nella base (`astro/spill.hpp`: `spill_area`, `spill_file` anonimo `O_TMPFILE` nella directory di scratch, quota dei byte logici, `spill.quota_exceeded`, `spill.short_read`, cancellazione alla chiusura).
Regole: in `prepare` ogni frame RAW si legge verificato (`read_verified_input` con `raw_input_format()`, OB-11) e si decodifica per intero, uno alla volta, nella memoria già prenotata dal job in `jc.shared` (decoder nel modo con prenotazione del chiamante, ADR-08.b §2.8); lo stato del decoder si confronta col parametro del nodo (`master.decoder_state_changed`); l'immagine (`f32` più maschera `uint16`, righe in ordine) si scrive in un `spill_file` e la memoria del frame si libera prima del frame successivo. I task leggono le fasce di righe dallo spill con le stesse regole delle fasce FITS (`frame_residency = blocks` per entrambi). `finalize` riverifica i file RAW (`reverify_input`, OB-11). Un master con frame RAW richiede lo spill dichiarato nelle risorse del job: senza, stop tipizzato prima di ogni lettura (nessun percorso residente alternativo); quota insufficiente: `spill.quota_exceeded` dalla base. Quota necessaria: `n·S·6` byte, calcolata nel pianificatore e controllata prima della prima decodifica. Il picco di decodifica (file più `needed` del decoder) entra in `P` e quindi in `shared'` e in `minimum_feasible_bytes`: il piano si chiude prima di ogni prenotazione.

**Emendamento.** ADR-06, estensione R2.r: sezione finale di questo documento («Frame RAW tramite spill»). C-02.b §b.2, paragrafo «RAW»: aggiungere «Il job decodifica ogni frame RAW per intero nel `prepare`, uno alla volta, dopo la lettura verificata (OB-11), e lo scrive nello spill dichiarato del job; i task leggono le fasce di righe dallo spill; la riverifica dei file RAW avviene in `finalize`. Senza spill dichiarato un master con frame RAW si ferma prima di ogni lettura (`invalid_input`, `master.raw_requires_spill`).»

Applicato in `docs/contracts/ADR-06-budget-scheduler.md`, estensione R2.r, «Estensione M3.b del piano del master» → «Frame RAW tramite spill»; in `docs/contracts/ADR-08-decoder-pack.md` §2.8 (modo con prenotazione del chiamante); in `docs/contracts/C-02-master.md` §b.2, paragrafo «RAW», e §b.10 (tabella degli errori: `master.raw_requires_spill`, `spill.quota_exceeded`); C-02.b §b.11 (parziali per riga solo per le somme in virgola mobile).

**Conseguenze.** B3b: `P` dei RAW con il picco di decodifica, quota di spill `n·S·6` nel pianificatore e stop senza spill dichiarato o con quota insufficiente; B15a/B16: decodifica in `prepare`, confronto dello stato, riverifica; T-M3A-CAL-09 (parte master) e T-M3B-C02-15 (stato del decoder diverso) valgono sul percorso del job; T-M3B-MEM-01 comprende i casi RAW.

---

## L11. Stop per `flat_normalization` non valida; avvisi e parametri con `median_ratio`

**Domanda.** (a) Codice di stop per `flat_normalization` fuori da {`none`, `median_ratio`}. (b) Con `median_ratio`: `flat_level_rel_tol` vietato, nessun avviso per la dispersione dei livelli. Si emenda il contratto o si segue il testo?

**Fatti.**
- Codice: `flat_normalization` è un `enum class { none, median_ratio }` (`master.hpp`): una ricetta in memoria non può contenere un terzo valore. Assente su un flat: `bad_param("flat_normalization", "assente")` → `invalid_input`, `master.param.flat_normalization` (`master.cpp`, `validate_params`). Presente su un ruolo diverso da `flat`: stesso codice. Oggi `median_ratio` → `master.variance_domain` (`outside_variance_domain`).
- Codice: `master_descriptor_from_node` ferma un nodo non riconosciuto con `master.not_master_node` (`bad(...)`).
- Contratto: C-02.b-flat §f.2: «`flat_normalization` ∈ {`none`, `median_ratio`}, solo per il ruolo `flat`»; «Parametri con `median_ratio` (regole; assenti, non finiti o fuori dominio: `invalid_input`, `master.param.<nome>`)»; «`flat_level_rel_tol`: vietato». §f.10: «Parametri di §f.2 assenti, non finiti, fuori dominio o presenti dove vietati → `invalid_input`, `master.param.<nome>`». §f.10, avvisi: `flat_norm_scales_estimated` e quelli di C-02.b. Provenance: `m_(f,c)`, `r_(f,c)`, `m_H,c` e `ρ_c`.
- Ricerca nel repository (`grep`): `master.flat_normalization_unsupported`, `flat_norm_wide_scale_spread` e `flat_level_spread_not_checked` compaiono solo nel piano `plans/M3b.md`, non nei contratti né nel codice.

**Opzioni.** (1) Seguire il testo. (2) Emendare con un codice nuovo, un parametro opzionale e due avvisi.

**Proposta: (1).** (a) Un testo fuori dall'insieme, letto da un parametro serializzato o da un'interfaccia che lo riceve come stringa, è un parametro «fuori dominio» di §f.2: `invalid_input`, `master.param.flat_normalization` (la famiglia esistente), con il valore nel contesto. Un nodo con il valore sconosciuto letto da `master_descriptor_from_node` si ferma con `master.not_master_node`. Nessun codice nuovo. (b) Nessun parametro opzionale: ogni parametro nuovo è una scelta nascosta (criterio 4), e una soglia per «dispersione larga» non ha una derivazione nei contratti; per i metodi robusti il limite `max m ≤ 4 min m` (§f.2 controllo 3) e il budget (§f.2 controllo 4) sono già stop tipizzati, per `mean` il fattore globale è dimostrato per qualunque rapporto (T-M3B-FLAT-02 (a)). La dispersione dei livelli è visibile nel report e nella provenance (`m_(f,c)`, `r_(f,c)`, `ρ_c`, §f.10) e non richiede un avviso. L'avviso `flat_norm_scales_estimated` resta l'unico di `median_ratio`.

**Emendamento di contratto: no.**

**Conseguenze.** B3: validazione del valore con `master.param.flat_normalization`, nessun codice nuovo. B8: `median_ratio` ammesso; `flat_level_rel_tol` presente con `median_ratio` → `master.param.flat_level_rel_tol` (vietato dove non ammesso); T-M3B-FLAT-06 (parte parametri). 

---

## Estensione proposta di ADR-06, «Estensione R2.r: piano del job del master»

Dove sta: `docs/contracts/ADR-06-budget-scheduler.md`, sezione «Estensione R2.r: piano del job del master» (righe 293-362 nel worktree), sottosezioni «Ingresso», «Modello», «Stop», «Test R2.r». Il testo sotto è la decisione, applicato in `docs/contracts/ADR-06-budget-scheduler.md`, estensione R2.r, sottosezione «Estensione M3.b del piano del master» («Ingresso (M3.b)», «Modello (M3.b)», «Stop (M3.b)», «Frame RAW tramite spill») e riga T-M3B-MEM-01 della tabella «Test R2.r».

### Testo da aggiungere alla sezione «Ingresso»

Nuovi campi di `planner_input`, validi con `frame_count` > 0 (nomi indicativi): `master_row_channels` ∈ {1, 2} (canali presenti in una riga del frame: 1 per un master mono, 2 per un master CFA, perché una riga CFA contiene due delle quattro posizioni del motivo) e `master_raw_frames` (booleano: i frame del gruppo sono RAW). Un valore fuori da {1, 2}: `invalid_input`, `resources.planner_input`. Il metodo di combinazione non è un ingresso: i buffer dei metodi si contano tutti, per la quota massima di ogni metodo (limite dichiarato sotto). Con `master_raw_frames` il pianificatore legge `scratch` e la sua quota dalla `resource_config`.

### Testo da aggiungere alla sezione «Modello»

Simboli nuovi: `Cr` = `master_row_channels`; `q(n) = 8·⌈n/8⌉`.

```
buffer per task (indipendenti dal tile, uno per task, un pixel alla volta):
  puntatori           16·n                        (già in R2.r)
  ordinamento         16·n    coppie (valore f64, indice canonico) di median e winsorized (C-02.b §b.8)
  indicatori          q(n)    uno per frame, per sigma_clip (C-02.b §b.1, §b.11)
  contatori           24·n + 544    parziali del task, sommati in finalize nei totali persistenti Cn
                              rifiutati, saturi e invalidi per frame (3 · 8 · n) più, per canale
                              (bound a 4): conteggi per bit (16 · 8 · 4 = 512) e pixel con rejection (4 · 8 = 32)
  F(n) = 16·n + 16·n + q(n) + 24·n + 544 = 56·n + q(n) + 544

task(t)   = F(n) + t·W·(10·n + 8)                 (sostituisce 16·n + t·W·(10·n + 8))

termini condivisi nuovi:
  righe     R = 16·Cr·H·n
              accumulatori in virgola mobile del report per riga: Σr e Σr² (2 · 8 byte) per frame, per canale di
              riga e per riga (C-02.b §b.7, §b.11); il flat non ha somme di riga (mediana e massimo si calcolano
              sui piani contati in K·S)
  costanti  CS = 120·n + 168
              pesi w, a, c (24·n); W e T (16); per frame e canale m, 1/m, r (3 · 8 · 4 · n = 96·n); κ (8);
              per canale Σ(1/m), m_H e ρ (3 · 8 · 4 = 96); A (8) e budget (8); norm_level_bias_bound per canale
              (4 · 8 = 32) (C-02.b §b.6, C-02.b-flat §f.2, §f.6, §f.13 punto 11)
  contatori persistenti  Cn = 24·n + 544
              totali interi (rifiutati, saturi, invalidi per frame; conteggi per bit e pixel con rejection per
              canale), somma dei parziali dei task in finalize

shared'   = shared + R + CS + Cn
spill     Z = n·S·6 se master_raw_frames, altrimenti 0: quota di spill richiesta, non memoria del pool
            (vedi «Frame RAW tramite spill»)
task(t)   = F(n) + t·W·(10·n + 8)
minimum_feasible_bytes = shared' + task(t_min)
tile      = tile esplicito, oppure min(t0, floor((floor((B − shared') / k) − F(n)) / (W·(10·n + 8))))
peak      = shared' + workers·task(tile)
working_set = shared' + min(C, ceil(H / t0))·task(t0)
```

Byte per task in funzione di `n`, `w` (cioè `W`) e `tile_rows` (`t`): `task(t) = 56·n + 8·⌈n/8⌉ + 544 + t·W·(10·n + 8)`. Il confronto con R2.r attuale: `master_task_bytes(n, t, W)` in `master.cpp` calcola `16·n + t·W·(10·n + 8)` (puntatori `n·(8 + 8)`, `t·W` per frame `n·(8 + 2)`, appoggio `8·t·W`); la proposta aggiunge solo `40·n + q(n) + 544` per task, indipendente da `t`. I termini `P`, `8·S`, `out`, `K·S`, `(out + HB)`, `D`, `M` di `shared` non cambiano. `K` = 32 resta valido finché i dati per pixel del report sono quelli attuali: `lbe` (8 byte), `ratio` (8), `nonpositive` (1), copia del piano della varianza per la mediana (8) = 25 byte (`master_plan` e `channel_report` in `master.cpp`). Con `median_ratio` `norm_level_bias_bound` è una costante per canale (C-02.b-flat §f.6, §f.7), contata in `CS`; l'unico dato per pixel è il suo rapporto col piano, già contato in `ratio` (8 byte); `level_bias_estimate` è `null` (C-02.b-flat §f.13 punto 3). Se B11 aggiunge un array per pixel oltre questi quattro, `K` cresce di 8 e il test lo mostra.

Limite dichiarato: il modello riserva i buffer di ogni metodo anche se il metodo scelto non li usa. L'eccesso massimo per task è `40·n + q(n) + 544` byte (ordinamento, indicatori, contatori già allocati comunque) e `CS` non dipende dal metodo; per `n` = 1024 sono circa 42 KB per task. Un kernel che ordina più pixel insieme (corsie SIMD) moltiplicherebbe il termine di ordinamento per il numero di corsie: oggi l'ordinamento è per pixel (C-02.b §b.11) e il modello non lo prevede.

### Testo da aggiungere alla sezione «Stop»

Invariato nella forma. Con `shared'` e `task` nuovi: `B < minimum_feasible_bytes` (anche in automatico); worker esplicito e tile automatico senza alcun `t ≥ 1` che entri, con `picco = shared' + k·task(1)`; worker esplicito con `peak > B`. Ogni grandezza di memoria nuova (`R`, `CS`, `Cn`, `F`, e `P` per i RAW) si calcola con prodotti e somme controllati, come `shared` e `task`; oltre 2^64 − 1 dà lo stesso stop con `non rappresentabile`. `Z` non è memoria del pool: i suoi stop sono quelli di «Frame RAW tramite spill». `Cr` fuori da {1, 2}: `invalid_input`, `resources.planner_input`.

#### Frame RAW tramite spill

Per `master_raw_frames` il job decodifica ogni frame per intero in `prepare` (uno alla volta, dopo la lettura verificata), lo scrive nello spill dichiarato del job (`f32` più maschera `uint16`, righe in ordine) e libera la memoria del frame prima del successivo. D8 resta valido anche per i RAW: la pila non è mai residente; i task leggono le fasce dallo spill (`frame_residency = blocks`).

Memoria. `P` (`input_peak_bytes`) è il massimo fra il picco FITS e, per ogni frame RAW, la dimensione del file più `needed` del decoder (`raw_image_bytes + 6·W·H`; per il decoder DNG `6·W·H + seg.buffer_bytes + zlib_cap`), con `S·6` contato una volta sola. Entra in `shared'` e in `minimum_feasible_bytes`. In `prepare` la decodifica usa la memoria già prenotata dal job in `jc.shared`, come `decode_fits` nel job FITS, senza una seconda `pool->reserve` (ADR-08.b §2.8, modo con prenotazione del chiamante). `needed` si calcola prima del piano, dal solo header e senza campioni, con una funzione pura del decoder (nome indicativo `raw_decode_needed_bytes`, B15a). Il piano si chiude prima di ogni prenotazione: OB-08 vale. La prenotazione interna del decoder non è esclusa dal pianificatore: è dentro `P`.

Spill. Quota richiesta `Z = n·S·6`, con prodotto controllato. Con `master_raw_frames` `resource_plan.spill` è valorizzato da `config.scratch`. Stop, nessun percorso residente alternativo:

- `config.scratch` assente: `invalid_input`, `master.raw_requires_spill`, contesto `frame` (il primo frame RAW in ordine canonico) e `n`; è del job, prima del pianificatore e di ogni lettura.
- `Z` oltre `scratch.quota_bytes` o oltre 2^64 − 1: `budget`, `spill.quota_exceeded`, contesto `richiesti` (o `non rappresentabile`) e `quota`; è del pianificatore, prima di ogni prenotazione.

### Testo da aggiungere alla sezione «Test R2.r»

| ID | Clausola |
|---|---|
| T-M3B-MEM-01 | Pianificatore con i termini nuovi: caso di riferimento di R2.r ricalcolato a mano con `Cr` = 1 e `Cr` = 2 e con `n` = 10 (`F(10)` = 560 + 16 + 544 = 1120; `R` = 16·Cr·1000·10 = 160 000·Cr; `CS` = 1368; `Cn` = 784) e con `n` = 3 (dove `q(n)` = 8); caso con `master_raw_frames`: `P` = file + `needed`, `resource_plan.spill` valorizzato, `Z` = n·S·6; stop `master.raw_requires_spill` senza `scratch`, stop `spill.quota_exceeded` con quota minore di `Z` e con `Z` non rappresentabile, entrambi prima di ogni lettura e prenotazione; piano non rappresentabile con `n` o `H` enormi; `minimum_feasible_bytes` − 1, anche per un master RAW, → stop prima di ogni prenotazione (picco del pool 0); tile automatico a confine (`tile` = 1 contro 0); `Cr` fuori da {1, 2} → `resources.planner_input`; T-M2R2R-01…05 rieseguiti con i valori ricalcolati. |

Ricalcolo dei valori di R2.r (tabella p-z) con i termini nuovi: da eseguire a mano nell'unità B3b prima del codice, come nel testo originale; qui si fissano solo le formule.

### Conseguenze sul codice e sui test

- `master_task_bytes` (`master.cpp`, dichiarata in `master.hpp`) e `make_master_model`/`plan_master` (`resources.cpp`) cambiano `fixed = 16·n` in `F(n)` e aggiungono `R`, `CS`, `Cn` a `shared`; `P` dei RAW include il picco di decodifica; `Z` è la quota di spill richiesta, non memoria del pool. Il pianificatore ha 2 chiamanti nel job (`master.cpp`) e il test `test_resources`/`test_master` che lo copre; `codegraph impact` va eseguito in B3b su `plan_resources`, `compute_plan_bounds`, `master_task_bytes` e `planner_input` prima di toccarli, e si rilanciano i test elencati (T-M2R2R-01…05, `test_master`, `test_master_stats`).
- `master_job_options::on_job_plan` e `details.resource_plan` portano i valori nuovi.
- I valori di `K`, `D`, `M` non cambiano.

## Decisioni successive (B1)

### T-M3B-C02-04(b) senza le celle della mediana a `n = 101`

**Decisione del coordinatore**, 9 novembre 2026, durante B1 (protocollo preregistrato, `docs/evidence/M3b-protocollo.md`). La mediana con `n = 101` e winsorized (101, 50), che coincide con la mediana in aritmetica esatta, escono da T-M3B-C02-04(b). Restano in T-M3B-C02-04(a). Winsorized (101, 1) e (101, 2) restano in (b). La mediana con `n = 101` e winsorized (101, 2) diventano punti espliciti di T-M3B-C02-05. T-M3B-C02-01 riceve un caso con `n = 101`: mediana e winsorized con `g` ∈ {1, 2, 50}.

**Conto.** La regola di C-02.b §b.12 chiede `K · P > 4 m (μ_4/κ² − 1)/(α δ²)`, con `δ = |κ − π/(2n)|/κ`. Per la mediana con `n = 101`: `κ = 0.0154862319`, `π/(2n) = 0.0155524389`, `δ = 0.43 %`, `μ_4/κ² − 1 = 2.0027`. Ne segue `K · P` minimo di circa 1.4 · 10¹⁰: con 101 campioni per realizzazione, circa 1.5 · 10¹² estrazioni gaussiane, ore di CPU in un solo test del gate. Le celle winsorized (101, 1) e (101, 2) chiedono circa 7.5 · 10⁵.

**Motivo.** (b) è un controllo di coerenza senza potenza (C-02.b §b.9): confronta la dispersione dei valori del master con `κσ²`, con il `κ` dell'oracle. Non è l'unica prova di nessuna proprietà. L'aritmetica dei valori della mediana e di winsorized la prova esattamente T-M3B-C02-01 (classe E), l'accuratezza di `κ` del prodotto la prova T-M3B-C02-05 (errore relativo ≤ 10⁻⁹). Con le aggiunte sopra, entrambi comprendono le due celle tolte. Il valore atteso di `(M − μ)²` è `κσ²` per il fatto di §b.6 (equivarianza). Togliere le due celle da (b) non toglie quindi una prova; toglie solo il costo.

**Conseguenza sul protocollo.** In T-M3B-C02-04 le statistiche diventano 16 per (a) e 14 per (b), quindi `m = 30`, livello per statistica `α/m = 3.333 · 10⁻⁵`, `α/(2m) = 1.667 · 10⁻⁵` per coda. `K` e `P` sono in `docs/evidence/M3b-protocollo.md` §3.2.

Applicato in C-02 §b.12 (C02-01, C02-04(b), C02-05).

---

## Decisioni successive (B3)

**`n` sotto il minimo del metodo.** `mean` con `n = 1` (e in generale `n` sotto il minimo del metodo) è `unsupported`, `master.variance_domain` (C-02.b §b.10; la riga della tabella dei domini di §b.2 estende C-02.a). Motivo: `.b` estende `.a` e la sua tabella dei domini copre il caso; un solo codice per lo stesso caso. Emendata la riga «`n` sotto il minimo» della tabella di C-02.a §2. `master.frames_insufficient` non è più prodotto da `validate_master_recipe`.

**Scelte di test convalidate.**
- Ordine dei controlli: presenza di `combine` e `rejection`, coppia, `n` e ruolo prima dei parametri; i domini che dipendono da un parametro (`n ≥ 2g + 1`, `max a_f < 1/2`) dopo la validazione del parametro.
- `winsor_g`, `clip_max_iter` e `clip_min_kept` sono letti come numeri e validati come interi: se non interi, `master.param.<nome>`.
- Nel nodo i parametri hanno il nome dei campi di ricetta (`null` se assenti); il peso sta nella voce `frames[k]`, legato al digest.
- `role` nel contesto di `op.phase_blocked` e `op.linearity_blocked` è il nome del ruolo del master.

**Scelte del codice di B3.**
- Parametri di `single`: assenti → `metadata_missing`, `var.source.<campo>`; non finiti o fuori dominio → `invalid_input`, `var.param.<campo>` (ADR-17.a). `offset_adu`, `conversion_gain_e_per_adu` e `read_noise_adu` sono vietati con un metodo diverso da `single` (`master.param.<nome>`).
- Un metodo ammesso dai domini ma senza calcolo (tutto tranne `mean` senza rejection, fino a B5 e B6) ferma `combine_master` e il job con `unsupported`, `master.method_not_computable`: mai calcolato come media. Il nodo e il descrittore sono invece costruiti.
- `master_algorithm_version` = 2: i master `.a` diventano `stale`.
- `sigma_clip_inert` e i confronti esatti di κ non sono in B3 (B6).

---

## Decisioni successive (B15a)

**Decisioni del coordinatore**, 9 novembre 2026, durante B15a (master da RAW, parte 1). Già nei test dell'unità.

- **`offset_adu` nella ricetta.** Ammesso solo con `single` o con il flat da RAW (L9); altrove `invalid_input`, `master.param.offset_adu`. Assente con `single` da RAW o col flat da RAW: `metadata_missing`, `master.metadata.offset_adu` (riga più specifica di C-02.b §b.10). Con `single` da FITS resta `var.source.offset_adu` (ADR-17.a).
- **`decoder_black_abs_tol_adu` del master.** Assente con frame RAW o presente con soli frame FITS: `invalid_input`, `master.param.decoder_black_abs_tol_adu`. C-02.b §b.10 non elenca il codice: si segue `master.param.<nome>` e ADR-16.b §b.7 «obbligatorio [...] vietato altrimenti».
- **Modi del decoder.** `raw_decode_context` con `pool` e `caller_reserved_bytes` insieme: `invalid_input`, `raw.decode_context`, prima di ogni lettura del file (ADR-08.b §2.8: un solo modo per chiamata, scelto a priori).
- **ID.** `T-M3B-DEC-01` per il modo con prenotazione del chiamante e per `raw_decode_needed_bytes`.
- **Rinvii.** La chiave della calibrazione con `frame_decoder_states` (ingresso `from_node`) è di M3.c, con la ricetta `.b` della calibrazione: la ricetta `.a` rifiuta un master con `frame_decoder_states`. Il controllo sui campioni del nero non sottratto è di B15.

**Scelte del codice di B15a.**
- Parametri del nodo: `frame_decoder_states` è `null` senza frame RAW, altrimenti un array con un elemento per ingresso frame in ordine canonico; `decoder_black_abs_tol_adu` è l'intero dichiarato o `null`. Le chiavi dei master da FITS cambiano (due parametri `null` in più): i nodi già pubblicati diventano `stale`.
- Elemento di `frame_decoder_states`: le chiavi del record del decoder (ADR-08.b §2.9) per i membri di contenuto di `decoder_state` (`decoder_state_record`, inverso `decoder_state_of_record`) più le quattro chiavi dell'identità. Il descrittore lo rilegge e lo accetta solo se coincide con la propria forma canonica; un errore di lettura diventa `master.decoder_state_param` con la chiave e il codice d'origine nel contesto.
- Invarianti indipendenti dall'immagine (`decoder_state_violation`, condivisa con `validate_image`): `dng` presente se e solo se `astro_dng`, finestra non vuota dentro il buffer senza overflow, motivo del nero, curva, e `cfa_color_index` con un sito 0, un sito 2 e due siti in {1, 3}. Si controllano nel preflight prima del nero effettivo e di nuovo nel descrittore.
- Ordine del preflight: frame RAW e FITS insieme (`master.decoder_state_mixed`) prima dei parametri, perché le regole di `offset_adu` e della tolleranza dipendono dal tipo dei frame; poi parametri, digest duplicati, invarianti, campi strutturali e nero col riferimento canonico. Il confronto è quello dell'analisi della calibrazione (`decoder_state_difference`, sempre con il nero); lo stop del nero porta differenza massima, posizione nel periodo e tolleranza.
- Decoder: la prenotazione dei due decoder passa da un solo punto (`reserve_decode`) che precede ogni allocazione della decodifica; nel modo del chiamante non chiama il pool. `raw_decode_needed_bytes` rifà gli stop del decoder fino a quel punto: per LibRaw `open_buffer` senza `unpack`, per il decoder del progetto la lettura dei tag e dei segmenti.
- Nodi `master.combine` pubblicati prima di B15a, senza `frame_decoder_states`: il descrittore non li legge. Su uno slot ancora `current` lo stop è esplicito, `invalid_input`, `master.decoder_state_param` con motivo «parametro assente dal nodo», mai un descrittore senza stato; il nodo diventa `stale` dopo che lo slot è stato ripuntato su un master ricostruito.
- Contesto degli stop RAW del preflight: `master.mismatch.<campo>` porta `coppia`, `campo`, `sinistra` (riferimento canonico) e `destra` (frame) nella forma JSON del record del decoder, e `tolleranza` = `esatta`, come lo stop dei FITS; `master.mismatch.decoder_black` porta `differenza_massima_adu`, `riga_periodo`, `colonna_periodo` e `tolleranza_adu`. Il preflight rilegge ogni stato dalla forma scritta nel nodo, così ricetta e descrittore accettano lo stesso insieme di stati.
- `raw_decode_context` con `held_bytes` senza `pool`, anche nel modo del chiamante: `invalid_input`, `raw.decode_context`, prima di ogni lettura (`held_bytes` vale solo per il modo che prenota dal pool).
- Limite della prova «prima di ogni allocazione» del modo del chiamante: il contatore delle allocazioni (`allocated_sample_bytes()`) vede l'immagine di `make_image`, non il buffer del segmento del decoder del progetto (`std::vector`, U13 §5), né le allocazioni interne di LibRaw. Che il controllo preceda quel buffer lo mostrano il codice (un solo punto, `reserve_decode`, prima del buffer) e la mutazione che sposta il controllo dopo il tetto di zlib, rilevata dai ganci.

## Decisioni successive (B5)

### `τ_k` dei criteri S di T-M3B-C02-03 e 04: ampiezza dell'intervallo e inclusione

**Difetto.** La versione 1 del protocollo (§1) definiva `τ_k` come la massima distanza fra l'intervallo della regola I e il valore f64 del prodotto, e C-02.b §b.9 come «il bound del valore del prodotto». Con questa definizione un errore `D` del prodotto allarga `τ_k` di circa `D`, quindi anche il margine: C02-03 e C02-04 non possono fallire per un errore del prodotto. Il test lo seguiva alla lettera; il conteggio dei valori fuori dall'intervallo era solo stampato.

**Misura** (autore del codice di B5, worktree `m3b-b5`, nessun record registrato). Con κ tolto dal piano della mediana, T-M3B-C02-04.n3 passa: media di `V̂` 16.0 contro `E` 7.18, valori fuori dall'intervallo 1048576/1048576. Con i `c_f` moltiplicati per 1.25, T-M3B-C02-03 W2 passa.

**Decisione del coordinatore**, 9 novembre 2026: emendamento del protocollo preregistrato (versione 2). `τ_k` è l'ampiezza dell'intervallo della regola I (`hi − lo`), che non dipende dal prodotto. Ogni test S ha in più un criterio binario: ogni valore del prodotto confrontato sta nel suo intervallo. Il criterio è strettamente più severo della versione 1 e nessun record di misura era stato registrato, quindi l'emendamento non è un allentamento. La definizione di §1 vale per tutti i test S del protocollo, compresi quelli dei flat (B9).

Applicato in: `docs/evidence/M3b-protocollo.md` §1, §3.1, §3.2 (versione 2; la versione 3 aggiunge in §1 la nota sull'implementazione della regola I nell'oracle, criteri e valori invariati); `docs/contracts/C-02-master.md` §b.9 e §f.9.

### Kernel dei due percorsi

**Decisione del coordinatore**, 9 novembre 2026, durante B5. `single`, `weighted_mean`, `median` e `winsorized` usano lo stesso codice scalare per pixel in entrambi i percorsi di ADR-07 (`method_band` in `src/core/master.cpp`), senza un kernel AVX2. Il percorso risolto resta quello del preflight e sta nel campo `backend` del nodo. Ogni banda chiama `begin_kernel_task()`, quindi il controllo dell'ambiente FP all'avvio del task vale come per `mean`. I contatori `cpu_test::kernel_calls` non contano queste bande: contano solo i kernel dei due percorsi (righe della calibrazione, bande di `mean`, `pixel_sum`).

**Verifica su [ADR-07](../contracts/ADR-07-cpu-simd.md).** ADR-07 non chiede un kernel AVX2 per ogni operatore.
- «Kernel dei due percorsi» elenca quali sono i kernel: le righe della calibrazione e le bande del master di C-02.a (somma, media, quadrati degli scarti, `s²/n`, statistiche del flat). I metodi di C-02.b non sono nell'elenco.
- «Riduzione su pixel», **Limite**, ammette già codice comune ai due percorsi per le mediane `N(Fc)` e `m_f`, la media dei livelli `m_f` e la varianza di `N`, bit-identiche fra i percorsi perché è lo stesso codice.
- «Tolleranze» (a): 0 ulp per le accumulazioni per pixel lungo i frame dei master; la mediana dispari è una selezione e la pari è `(z_k + z_(k+1)) / 2` con le stesse operazioni IEEE. Con un solo codice l'identità fra i percorsi vale per costruzione.
- Compilazione: `master.cpp` usa `-fno-fast-math -ffp-contract=off` (`ASTRO_FP_FLAGS`), senza FMA né istruzioni approssimate (§«Percorsi», (c)). Non ha `-fno-tree-vectorize`, che ADR-07 chiede solo all'unità del kernel scalare (`kernels_scalar.cpp`). Una vettorizzazione automatica non cambia i risultati: senza riassociazione ogni operazione IEEE resta la stessa, nello stesso ordine per pixel.

**Limite dichiarato.** Sul percorso `cpu_avx2` i quattro metodi non sono vettorizzati fra pixel, a differenza di quanto (a) descrive per le bande di `mean`. La verifica dei due percorsi li copre: T-M2R5-01 riesegue con `ASTRO_TEST_SIMD=avx2` anche i test fast di `test_master_b5` (`T-M2R5-01.avx2.test_master_b5`, escluse le statistiche lente `*Slow.*`), con le stesse attese esatte e gli stessi oracle del percorso scalare. Un kernel AVX2 per questi metodi, se servirà per le prestazioni, è un'unità separata con il proprio confronto fra percorsi (T-M2R5-02).

### Segno delle costanti dei pesi

**Decisione del coordinatore**, 9 novembre 2026, review di B5 (giro 1). Il dominio `2 max w < W` si confronta in aritmetica esatta, ma `W` in f64 è arrotondato: un `a_f = w_f / W` può valere 1/2 o più anche con il dominio esatto valido. Esempio dalla review: pesi `(2⁵³, 2⁵² + 1, 2⁵¹ + 1, 2⁵¹ − 1.5)`, con `T` negativo e `c_f` negativi ma normali. Regola: nel preflight un `1 − 2 · a_f` ≤ 0, `T` ≤ 0 o un `c_f` ≤ 0 in f64 fermano con `invalid_input`, `master.param.weights`, con il valore nel contesto, prima di ogni lettura. Così ogni `c_f` è > 0, il piano è una somma di termini non negativi e la regola delle somme di §6.3 vale. Applicata in C-02 §b.6, «Rappresentabilità».

### Campione subnormale d'ingresso con uscita f64

**Decisione del coordinatore**, 9 novembre 2026, review di B5 (giro 1). Con uscita f64 la conversione al tipo di uscita è un'identità: un campione d'ingresso subnormale scelto senza operazione (`single`, mediana a conteggio dispari) non vi diventa subnormale e resta valido (C-02.a §6.3: «un valore f64 non nullo che diventa zero o subnormale»; i subnormali in ingresso sono valori finiti validi). Con uscita f32 vale la regola: un valore non nullo che diventa zero o subnormale rende il pixel `result_nonfinite`. Nei metodi di B5 ogni prodotto, quoziente e somma di termini non negativi è controllato prima della conversione, quindi un subnormale f64 prodotto da un'operazione non arriva all'uscita come valido. La funzione condivisa `convert_finite` della calibrazione e del kernel di `mean` non cambia in B5. Lì la stessa classe resta aperta con uscita f64: `convert_finite<double>` marca un subnormale che passa per una differenza esatta (`src/core/calibration_internal.hpp:298`). Va decisa con il confine comune, fuori da B5.

### Rinvii a B11

- `κ` dei metodi robusti e le costanti `a_f`, `c_f` di `weighted_mean`: oltre che nel report (§b.7, «Stato»), vanno nei `details` del job (§b.10). In B5 sono calcolati nel preflight ma non registrati né nel report né nei `details`.
- Valori del campo `plane` nel blocco `variance` del report: `read_noise2_plus_signal_over_gain` (`single`), `weighted_residuals` (`weighted_mean`), `kappa_s2` (`median`, `winsorized`), `kept_s2_over_m` (`mean` + `sigma_clip`), accanto a `s2_over_n` di `mean`. Sono nomi provvisori, da fissare in C-02.b §b.7 in B11.

## Decisioni successive (B6)

Decisioni del coordinatore, 10 novembre 2026, sui punti sollevati dall'autore dei test di B6.

### Bit `support_incomplete` nel codice

**Fatto.** [ADR-03.b](../contracts/ADR-03-pixel-maschere.md) §b.6 definisce il bit 9 `support_incomplete` e `reserved_mask = 0xFC00`; il codice ha ancora `0xFE00` (`src/core/include/astro/image.hpp`), quindi FITS rifiuta un piano con il bit 9 e un master `sigma_clip` con pixel `support_incomplete` non si potrebbe pubblicare. T-M3A-TYP-08 era assegnato a M3.d.

**Decisione.** B6 porta nel codice ADR-03.b §b.6 così com'è scritto: costante del bit 9, `reserved_mask = 0xFC00`, lettura FITS del bit 9, bit 10–15 ancora `invalid_input`, `fits.invalid_reserved_bits`. Non è un confine rifatto: la regola è già normativa e tutti i controlli usano la stessa costante (`fits.cpp`, `fits_blocks.cpp`, `calibration_blocks.cpp`). T-M3A-TYP-08 si anticipa a M3.b (unità B6) in `docs/test-matrix.md`. Debayer e resampling, che in M3.d accenderanno lo stesso bit, non cambiano.

### `rejected_samples` dei pixel `support_incomplete`

C-02.b §b.1 punto 3 toglie `R` da `S` prima del controllo su `clip_min_kept`. Quindi i campioni di ogni `R` applicato, compreso l'ultimo che porta `|S|` sotto `clip_min_kept`, si contano in `rejected_samples` del loro frame. Il pixel conta anche fra i `support_incomplete` del canale; `pixels_with_rejection` lo comprende.

### Contesto di `master.param.sigma_clip_inert`

Chiavi: `n`, `clip_kappa_low`, `clip_kappa_high` (valori f64 dichiarati, in forma che rilegge lo stesso f64) e `limite` = `(n − 1)²`, intero esatto.

### Appartenenza per pixel in T-M3B-C02-02

Il prodotto espone per pixel solo `M`, `V` e i bit, e per frame i conteggi (§b.7, report). T-M3B-C02-02 verifica l'appartenenza attraverso `M` e `V` entro la regola I, i conteggi per frame e le rotazioni dei frame: un campione in più o in meno nell'insieme tenuto sposta `M` di un ordine di `σ/m`, molto sopra l'ampiezza dell'intervallo. **Limite dichiarato:** l'insieme rifiutato per pixel non si osserva direttamente; il record lo dice. Nessun canale di diagnosi nuovo nel prodotto.

### Parametri di `sigma_clip` di C02-01 e C02-02

`M3b-protocollo.md` §4 li vuole fissati prima della misura. Sono fissati nel codice del test (`c02_01_clip`, `c02_02_configs` in `tests/unit/test_master_b6.cpp`) e il record cita il commit che li introduce. Unico cambio prima di ogni misura del prodotto: `clip_min_kept` della configurazione E da `n − 3` a `⌈2n/3⌉`.

### Stop `sigma_clip_inert` nella ricetta

Lo stop sta in `validate_master_recipe`, quindi arriva prima del confronto della ricetta col nodo e prima di ogni lettura, anche nel job.

### Conteggi del report per `sigma_clip`

`rejected_samples` per frame è di B6. Il conteggio per bit comprende il bit 9 appena la costante esiste, perché i conteggi per bit si fanno sui bit dichiarati. `pixels_with_rejection` per canale va a B11 (report), con gli altri rinvii di «Rinvii a B11».

### Correzioni della review di B6 (giro 1)

- **`clip_max_iter` senza tetto.** Il contratto non fissa un massimo. Il codice usa `min(clip_max_iter, n)`, calcolato in f64 prima di convertirlo in intero. In aritmetica esatta è la stessa procedura: ogni iterazione che non chiude toglie almeno un campione, quindi dopo `n` iterazioni la procedura è finita. Un valore dichiarato grande (`1e20`, `2^64`) resta ammesso e dà lo stesso risultato di `n`. La stessa verifica vale per ogni parametro reale della ricetta convertito in intero.
- **`rejected_samples` e `result_nonfinite`.** Gli `R` di un pixel si contano solo se la procedura dà un esito: valore valido oppure `support_incomplete`. Un pixel che diventa `result_nonfinite` non contribuisce, per una qualsiasi operazione di §b.6 compresa la conversione al tipo di uscita, perché il suo insieme `K` non è definito come risultato. Il conteggio si fa quindi dopo l'esito finale del pixel.
- **Punto di somma dei conteggi.** C-02.b §b.11 somma i conteggi per frame in `finalize`. Il codice somma i parziali di ogni task con interi atomici. La somma di interi non dipende dall'ordine, quindi il risultato è identico. Il commento del codice lo dice; il contratto non cambia.
- **Tabella dei nomi dei bit.** È una sola, accanto alle costanti di `invalid_reason`. La usano master e calibrazione (in memoria e a blocchi), quindi anche il report di calibrazione conta `support_incomplete`.
- **Record di evidenza** di T-M3B-C02-01, 02 e 06 (commit dei parametri, stime di C02-06): con quelli di B5, nel record del gate di M3.b, dal run completo.

## Decisioni successive (B7)

Decisioni del coordinatore, 10 novembre 2026, sui punti sollevati dall'autore dei test di B7.

- **Report per canale (nomi provvisori, da fissare in C-02.b §b.7 in B11 con gli altri di «Rinvii a B11»).** Frame CFA: `flat_levels` = elenco di `{label, level, rel_dev}` nell'ordine di `channels`, al posto di `flat_level` e `flat_level_rel_dev`. Per canale: `channels[c].flat_level_mean`, al posto di `flat_level_mean` globale. Il mono resta quello di `.a`, senza campi nuovi.
- **Stop per canale.** La tolleranza fuori limite in un canale dà `unsupported`, `master.variance_domain`, come in C-02.a (tabella degli stop), con il contesto di `.a` più `canale` (etichetta R, G1, G2, B), come `master.flat_norm_budget`.
- **Segnale minimo e pixel validi per canale.** C-02.b §b.13 punto 4 rende per canale `m_f` e i suoi controlli. Un canale con livello sotto `flat_min_signal_adu`, o senza pixel validi in un frame, dà `master.flat_low_signal` con `canale` nel contesto. Un canale senza pixel validi nel master resta il caso di C-02.a: statistiche `null`.
- **T-M2R2-08.** Il test che toglieva tutti i pixel R al frame 0 ricadrebbe nello stop sopra. I NaN sono spostati in modo che nessun frame resti senza pixel R validi, mentre il master resta senza pixel R validi: le attese sono invariate e il caso verificato è lo stesso.
- **Versione dell'algoritmo del nodo: 3.** Cambia anche la chiave dei nodi mono, che diventano `stale` (`plans/M3b.md` §11). Per il mono «invariato» vale per master e report, non per la chiave.
- **Ordine del controllo** (prima frame o prima canale): il contratto non lo fissa. I test hanno una sola coppia (frame, canale) in violazione. Il codice scorre i frame nell'ordine canonico e per ogni frame i canali nell'ordine di `channels`, e riporta la prima violazione.
- **`master_row_channels` nel job.** Il job calcolava `master_row_channels = per_channel ? 2 : 1` prima che il preflight impostasse `per_channel`. Quindi valeva sempre 1, e un master CFA prenotava gli accumulatori per riga di un solo canale: `R = 16·Cr·H·n` sottostimato di `16·H·n` byte (ADR-06 R2.r, B3b). È un difetto di B3b, che B7 corregge perché tocca proprio il momento in cui `per_channel` si decide: il valore si calcola dopo il preflight, e il test lo verifica sul minimo di memoria di un job CFA.

### Correzioni della review di B7 (giro 1)

PASS con sei LOW (`.omc/artifacts/ask/review-M3b-B7-round1-claude-20261110-1949.md`), corretti in un solo giro.

- **Precedenza del CFA sui livelli.** Un frame k > 0 con CFA diverso dal frame 0, o mono, si ferma con `incompatible`, `master.mismatch.cfa` prima che si calcolino i suoi livelli per canale. Senza questa precedenza i livelli si calcolavano col motivo del frame 0 e lo stop poteva essere `master.flat_low_signal` con un `canale` senza senso. Nei flat il CFA si confronta così anche prima di larghezza, altezza e binning: un frame che differisce in geometria e CFA si ferma su `cfa` (il contratto non fissa l'ordine fra campi incompatibili). Test: le due varianti (in memoria e job) di `T_M3B_C02_14_*_stops_on_cfa_mismatch_before_its_channel_levels`.
- **Coerenza fra peek dell'header e decode nel job.** Resta `ASTRO_ASSERT`: è un invariante, non una condizione di input. Peek e decode leggono gli stessi byte con le stesse `read_options` e la stessa interpretazione dell'header, e una modifica del file fra i due la ferma già `read_verified_input` con `integrity`, `input.modified` (`check_unchanged` per la riscrittura sul posto, `check_path_names` per la sostituzione). L'autore dei test l'ha verificato in entrambi i modi con un test diagnostico, poi tolto: uno stop tipizzato al posto dell'assert sarebbe un ramo irraggiungibile.
- **Test.** Frame dello stop del segnale minimo fissato a `frame_000`; insieme esatto delle chiavi del contesto negli stop CFA della tolleranza; l'intestazione del test dichiara che lo stato `flat_level` nell'analisi (parte di T-M3B-C02-14) resta a B13; il commento del target cita T-M3B-MEM-01.
- **Review giro 2: PASS** (`.omc/artifacts/ask/review-M3b-B7-round2-claude-20261110-2022.md`). Corretti dal coordinatore due commenti e questa decisione. Restano due LOW, da riprendere in un'unità successiva che tocca questi test o questo confronto: insieme esatto delle chiavi del contesto negli stop `master.flat_low_signal` CFA e in `expect_cfa_mismatch` (con `sinistra`, `destra`, `tolleranza == "esatta"`); helper unico per il confronto del CFA, oggi in `inspect_frame` e in `compare_with_reference`.

## Decisioni successive (B12)

- **API pubblica dell'analisi dei master** (L3, opzione B): intestazione nuova `astro/master_analysis.hpp`, implementazione in `src/core/master_analysis.cpp`. Contiene `master_analysis_params` e `read_master_analysis_params` (membro `master` di `batch.analysis_params`, L4), il lettore `read_master_frame_stats` (L2, solo FITS; la parte RAW è di B15) e la funzione pura `screen_master_frames` (esclusioni per frame ed esito `params_missing` per ruolo). Il membro `master` si legge con lo stesso parser di `read_analysis_params`, non con un secondo parser.
- **Nomi dei campi `values` delle esclusioni**: provvisori, con chiavi italiane come i contesti degli stop (per esempio `saturated_fraction` {`saturi`, `totale`, `frazione`, `massimo`}, `flat_low_signal` {`canale` solo CFA, `livello` null se assente, `minimo`}). Il contratto dice solo «con i valori nel report»: i nomi definitivi si fissano col report in B11.
- **Un motivo per frame**: il primo della lista chiusa di C-02.b §b.2 punto 1, nel suo ordine. Copie con lo stesso digest ordinate per (digest, `input_id`); resta la prima.
- **Contesti degli stop.** `master.no_usable_frames`: {`ruolo`, `esclusioni`}, con `esclusioni` array JSON canonico di {`digest`, `input_id`, `motivo`, `valori`}. `master.param.saturation_adu`: {`frame` = digest esadecimale, `valore`}.
- **Punto del `fault_injector`**: `master.analysis.groups`, chiamato una volta dopo le esclusioni e mai dopo uno stop (T-M3B-C02-15).
- **`params_missing` per ruolo**: tutti i ruoli `exposure_rel_tol` e `temperature_policy`, e `temperature_abs_tol_c` solo con `temperature_policy = compare`; flat anche `flat_min_signal_adu` e `flat_level_rel_tol`; frame RAW anche `decoder_black_abs_tol_adu`; flat RAW anche `offset_adu`; `saturated_fraction_max` solo con `saturation_adu` dichiarato. Con `params_missing` il ruolo non ha esclusioni né frame utilizzabili e non c'è stop; `params_missing` precede `master.no_usable_frames`. `temperature_abs_tol_c` è richiesto con `temperature_policy = compare` e con politica assente (che è essa stessa `params_missing`).
- **Statistiche del lettore, uguali al preflight del master** (`inspect_frame`): saturi contati fra i campioni validi con `>= saturation_adu`, totale = tutti i pixel; frazione satura confrontata con `>` stretto; `m_(f,c)` = mediana dei campioni validi meno l'offset della richiesta (header o dichiarato per i FITS, `params.offset_adu` per i RAW), canali nell'ordine dei siti (0,0), (0,1), (1,0), (1,1); livello assente se il canale non ha pixel validi; livello sotto `flat_min_signal_adu` = segnale basso, uguale ammesso. La regola è scritta due volte (preflight e lettore) perché B8 sta cambiando `master.cpp` in parallelo: da riunire in un solo helper quando B14 aggancia il preflight dei nodi proposti.
- **Test aggiunti di B12 senza ID proprio** (L2, L4): portano il prefisso `T_M3B_C02_08_` e li prendono le righe esistenti del trace.

### Correzioni della review di B12 (giro 1)

FAIL con sette MED (`.omc/artifacts/ask/review-M3b-B12-round1-claude-20261110-2250.md`). Decisioni:

- **Offset dei livelli.** `master_stats_request` porta l'offset da sottrarre: per i FITS quello dell'osservazione (header o `offset_adu` dichiarato, come `inspect_frame` dopo `apply_declared`), per i RAW `params.offset_adu`. Un flat FITS senza `OFFSET` nell'header e con offset dichiarato ha i suoi livelli.
- **Livello non finito.** `flat_low_signal` se il livello è assente, non finito o sotto `flat_min_signal_adu`, come il preflight (C-02.a §2). `valori`: {`canale` solo CFA, `livello` (null se assente o non finito), `minimo`, `motivo` ∈ {`assente`, `non_finito`, `sotto_minimo`}}.
- **Domini dei parametri d'analisi.** Dopo `params_missing`, gli stessi domini della ricetta del master (`saturation_adu`, `saturated_fraction_max` in [0, 1], `flat_min_signal_adu` > 0, `flat_level_rel_tol` in (0, 1), tolleranze ≥ 0 e finite, `offset_adu`, come in `master.cpp`): fuori dominio = `invalid_input`, `master.param.<nome>`, contesto come lo stop della ricetta. Un errore sul parametro non deve diventare `master.no_usable_frames`.
- **Conflitto di dichiarazioni: motivo `declared_conflict`**, primo della lista chiusa, valori {`campo`}. Emendamento applicato in C-02.b §b.2 punto 1: nel preflight il conflitto ferma il job (`incompatible`, `metadata.declared_conflict`), quindi nell'analisi esclude il frame, come per i light (CL-05); l'osservazione non è affidabile, quindi precede ogni altro motivo.
- **Esito di decodifica per codice, lista chiusa.** Diventano `decode_failure` gli stop di `plan_fits` e `decode_fits` con codice `fits.*` o `image.*`, compreso `fits.checksum` (proprietà del contenuto del file, il cui digest coincide: il frame è inutilizzabile, non alterato). Restano stop `input.*` (OB-11), I/O, budget (`sched.*`) e cancel. La regola per codice vale anche per la parte RAW di B15 (`raw.*`, compreso `raw.data_error` che ha categoria `integrity`).
- **Digest sul ramo d'errore di `plan_fits`**: prima di dichiarare `decode_failure` il lettore confronta il digest del file; diverso = `integrity`, `input.digest_mismatch`. Ratificato, con test.
- **Guasto iniettato**: `fault_injector` con errno ≠ 0 → `io`, `master.analysis.fault`, contesto {`point`, `errno`} (modello `calib.analysis.fault`). Ratificato, con test.
- **Stop `master.param.saturation_adu`**: il contratto non pone condizioni, quindi si controlla su ogni frame con soglia propria, escluso o no, prima di `params_missing` e delle esclusioni (uno stop dell'analisi precede gli esiti).
- **`duplicate_digest`**: resta la prima copia nell'ordine canonico (lettera del contratto), anche se esclusa per un altro motivo; `primo` può quindi indicare un frame escluso.
- **Forma dei `valori`**: {`campo`} anche per `metadata_missing.phase`, `.linearity`, `.unit`.
- **Campioni non finiti senza bit**: il lettore si affida a `decode_fits`, che li marca (vincolo scritto nel commento).
- **Review giro 2: PASS** (`.omc/artifacts/ask/review-M3b-B12-round2-claude-20261110-2330.md`). Corretti dal coordinatore testi della decisione, del contratto e un'intestazione dei test. Restano LOW per un'unità successiva: `saturated_fraction_max` senza `saturation_adu` accettato nell'analisi e fermo nella ricetta (si chiude in B14, che aggancia il preflight dei nodi proposti, con test); test del ramo `image.*` di `is_decode_error`, della precedenza di `params_missing` sullo stop di dominio e di quattro coppie adiacenti dell'ordine dei motivi; sul ramo d'errore di `plan_fits` manca `check_unchanged` dopo il digest (da aggiungere con la parte RAW del lettore in B15).

## Decisioni successive (B8)

- **Campi della ricetta**: `master_recipe::flat_additive_bound_adu` (A) e `master_recipe::flat_norm_rel_budget`, entrambi `std::optional<double>`; presenza e divieti per metodo e ruolo come C-02.b-flat §f.2.
- **Contesti degli stop.** `master.flat_norm_budget`: {`A`, `rho`, `m_H`, `budget`} in `%.17g`, più `canale` solo per un CFA (come gli stop per canale di B7). Controllo 3 (`max m ≤ 4 min m`): contesto esistente di `master.variance_domain` più `canale` per un CFA. `master.flat_scale_nonfinite` su `1/m_f` o `r_f`: {`frame`, `m_f`}, più `canale` per un CFA.
- **Ordine dei controlli**: la numerazione 1-4 di §f.2; i test hanno una sola violazione per caso.
- **T-M3B-FLAT-01 in B8**: con i nomi di report e provenance non ancora fissati (B11), le costanti si leggono dal contesto dello stop del budget; livelli (1, 3, 3/2, 3/2), con `m_H = 3/2` esatto in ogni ordine canonico.
- **`median_ratio` ammesso ma non ancora calcolato**: dopo validazione e preflight, prima dei blocchi, del nodo e dell'artefatto, stop `unsupported`, `master.method_not_computable`, contesto {`flat_normalization`: `median_ratio`}. Combinare senza normalizzare darebbe un master sbagliato senza segnalarlo. B9 sostituisce lo stop col calcolo.
- **Chiave del nodo.** `flat_additive_bound_adu` e `flat_norm_rel_budget` entrano nei parametri del nodo (`recipe_report`), quindi la chiave cambia per ogni master, anche mono e senza `median_ratio` (due chiavi `null` in più), come in B5 e B6; `master_algorithm_version` resta 3. Effetto di `plans/M3b.md` §11: i nodi master già prodotti non si ritrovano più e diventano `stale`.
- **Contesti non fissati dal contratto**: stop non finiti sulla somma `Σ 1/m_f` e su `m_H`: {`costante`, `valore`} (+ `canale` per un CFA). Nel contesto di `master.flat_norm_budget` `rho` è il valore f64 arrotondato; il confronto resta esatto.
- **Confronti.** Controllo 3 in f64 (`hi ≤ 4·lo`): moltiplicare per una potenza di 2 è esatto e un overflow a `+inf` non cambia l'esito. Controllo 4 in aritmetica diadica esatta (`dyadic` su `wide_uint`), senza sottrazioni. `A = -0.0` è ammesso (uguale a 0).

### Correzioni della review di B8 (giro 1)

FAIL con due MED, tutti di prova (`.omc/artifacts/ask/review-M3b-B8-round1-claude-20261110-2307.md`). Decisioni:

- **Contesto di `master.flat_scale_nonfinite`, uniforme**: chiave `costante` ∈ {`1/m_f`, `r_f`, `somma`, `m_H`} in ogni stop; per `1/m_f` e `r_f` anche {`frame`, `m_f`}, per `somma` e `m_H` anche {`valore`}; `canale` per un CFA. Sostituisce il contesto {`frame`, `m_f`} senza `costante` deciso sopra.
- **`m̄_c` solo con `none`**: con `median_ratio` non si calcola, quindi un livello enorme ma ammesso non dà `master.report_nonfinite`. `m̄_c` serve alla tolleranza dei livelli e ai campi del report sui livelli (`rel_dev` per frame CFA, `flat_level_rel_dev` mono, `channels[c].flat_level_mean`, `flat_level_mean` mono): con `median_ratio` B9 o B11 decidono se calcolarli o dichiararli `null`; senza decisione `rel_dev` sarebbe infinito.
- **`master.method_not_computable` è temporaneo**: non sta nei cataloghi di §f.10 e §b.10; B9 lo toglie insieme al test `T_M3B_FLAT_06_admitted_median_ratio_stops_with_method_not_computable_until_b9`.
- Test: controllo 3 non applicato a `mean` e `weighted_mean` (rapporto oltre 4 ammesso); un test per costante del controllo 2; `A = -0.0` ammesso; commenti corretti.
- **Review giro 2: PASS** (`.omc/artifacts/ask/review-M3b-B8-round2-claude-20261110-2355.md`). I tre LOW (commenti su `m̄_c` e sul contesto di `master.flat_low_signal`, decisione sopra) corretti dal coordinatore.

## Decisioni successive (B9)

- **Campi dei livelli nel report con `median_ratio`**: `flat_level_rel_dev` (mono), `flat_levels[].rel_dev` (CFA), `flat_level_mean` (mono) e `channels[c].flat_level_mean` (CFA) valgono JSON `null`, coerente con B8 (`m̄_c` non si calcola con `median_ratio`); `flat_level` e `flat_levels[].level` restano `m_(f,c)`. Nomi provvisori fino a B11.
- **FLAT-05**: B9 verifica la capability `variance.fixed_weight_approximation` per ogni metodo con `median_ratio`; l'ipotesi `gaussian_reference_heteroscedastic` e il suo avviso nel report sono di B11 (FLAT-07).
- **Oracle di B9**: per FLAT-02(b) con `sigma_clip` si usa il limite su ogni sottoinsieme tenuto (la selezione dipende solo dall'arrotondamento); nei casi diadici la tolleranza 0 presuppone costanti f64 uguali ai razionali esatti, e il test lo verifica prima di confrontare.
- **Stop temporaneo tolto**: `master.method_not_computable` sparisce; il test `T_M3B_FLAT_06_admitted_median_ratio_stops_with_method_not_computable_until_b9` è sostituito da `T_M3B_FLAT_06_admitted_median_ratio_is_combined_in_memory_and_in_the_job`.
- **`level_bias_estimate` con `median_ratio`**: `null` con motivo `normalized` (C-02.b-flat §f.13 punto 3); `pixels_nonpositive_signal` resta. Nomi del report provvisori fino a B11.
- **Valori normalizzati**: `y_f = offset + r_f (x_f − offset)` nel buffer per pixel già esistente del task (`sort_slot`), quindi il modello di memoria non cambia; `mean` con `median_ratio` passa dal percorso per pixel (`method_band`), `mean` con `none` resta sui kernel scalar/avx2. Pixel `result_nonfinite` se `x − offset` non è finito, se il prodotto è zero, subnormale o non finito con `x ≠ offset`, o se `y` non è finito.

### Correzioni della review di B9 (giro 1)

FAIL con un MED di prova (`.omc/artifacts/ask/review-M3b-B9-round1-claude-20261111-0133.md`). Decisioni:

- **`pixels_nonpositive_signal` con `median_ratio`**: si verifica il valore esatto per canale, non solo il tipo.
- **Copertura di FLAT-01 non dyadic**: conteggio per regione, sempre stampato; nella regione di misura nessun pixel saltato (o un numero dichiarato nel test con la ragione). La premessa dei casi dyadic (costanti f64 uguali ai razionali esatti) si asserisce per canale.
- **`sigma_clip` con `y_f` uguali in aritmetica esatta**: l'oracle ammette anche `support_incomplete` come esito previsto, perché una selezione decisa solo dall'arrotondamento può scendere sotto `clip_min_kept`.
- **Uscita f32 con `median_ratio`**: almeno un caso dyadic e uno non dyadico, tolleranza della regola I più mezzo ulp f32. Ramo «prodotto zero con `x ≠ offset`» testato se un campione subnormale è ammesso in ingresso, altrimenti il test dice perché non si costruisce.
- **Registrazione delle stime di FLAT-03/04/05** (stime, errore Monte Carlo, limiti del protocollo §1): nei record di evidenza del gate di M3.b, dai run slow del gate; i test le stampano sullo stdout di ctest.
- **Versione dell'algoritmo**: B11 completa il report di `median_ratio` (avviso `flat_norm_scales_estimated`, ipotesi, provenance delle costanti, C-02.b-flat §f.7 e §f.10) e porta `master_algorithm_version` a 4, così gli artefatti `median_ratio` prodotti fra B9 e B11 diventano `stale`.
- Commenti: vincoli al posto delle questioni aperte e della storia delle unità; documentazione di `level_bias_estimate` con il caso `median_ratio` (corretta dal coordinatore).
- **Review giro 2: PASS** (`.omc/artifacts/ask/review-M3b-B9-round2-claude-20261111-0159.md`). Corretto dal coordinatore un commento storico. Restano LOW di prova per B10, che riesegue questi test: esiti `support_incomplete` ammessi da contare a parte e stampare; `EXPECT_GT(checked_in[1], 0)` nei due test non dyadic; controllo tautologico della somma dei contatori da sostituire; con `sigma_clip` indeciso il limite inferiore di `pixels_nonpositive_signal` può valere 0.

## Decisioni successive (B13)

- **Dove si formano i gruppi**: dentro `screen_master_frames`, dopo il punto `master.analysis.groups` del `fault_injector`, solo sui frame `usable`. `params_missing` e gli stop non producono gruppi.
- **API** (nomi provvisori, da fissare col report in B11): `master_group_status { ok, flat_level }`; `master_group { reference, members, status, flat_levels }`, con `members` gli `input_id` in ordine canonico e `members[0] == reference`, `flat_levels` = `m_(f,c)` per membro e canale, solo per il flat e per ogni gruppo flat; `master_screening::groups` ordinato per posizione canonica del riferimento.
- **Regole di tolleranza uguali al preflight**, perché il contratto («Fatto, per costruzione») chiede che ogni gruppo superi i confronti a coppie col riferimento canonico del preflight con gli stessi parametri: esposizione `|x − y| ≤ tol · max(|x|, |y|)` come in `master.cpp`; temperatura con `compare` entro `temperature_abs_tol_c`; nero effettivo RAW in interi a 64 bit entro `decoder_black_abs_tol_adu`.
- **Stato `flat_level`**: come la tolleranza dei livelli di `finish_preflight` (`m̄_c` media dei membri nell'ordine canonico, violazione `|m/m̄ − 1| > tol`, uguale ammesso). Non esclude frame: segna il gruppo. Nel report i livelli di ogni gruppo flat; `m̄_c` e la tolleranza no.
- **Regole riscritte o riusate.** Chiave esatta e tolleranze di esposizione e temperatura sono riscritte identiche a `compare_with_reference`, e la tolleranza dei livelli a `finish_preflight`, perché B8 cambiava `master.cpp` in parallelo: da riunire in un helper quando B14 aggancia il preflight dei nodi proposti. Per i RAW si riusano `decoder_state_difference` e `decoder_black_period_of` (`calibration_analysis.hpp`), come `validate_raw_frames`: campi strutturali e nero per sito in interi a 64 bit; uno stato del decoder incoerente ferma l'analisi con `calib.decoder_state`.
- **Media dei livelli non finita** in un gruppo (livelli vicino a `DBL_MAX`): `numeric`, `master.report_nonfinite`, come il preflight; senza questo stop il gruppo prenderebbe `flat_level` senza segnalarlo.
- **Chiave del gruppo nel report**: il contratto chiede «gruppi con chiave», l'API di B13 non la porta; si fissa col report in B11.

### Correzioni della review di B13 (giro 1)

FAIL con un MED di prova (`.omc/artifacts/ask/review-M3b-B13-round1-claude-20261111-0023.md`). Decisioni: `canale` nel contesto di `master.report_nonfinite` dell'analisi come `flat_context` del preflight (etichetta CFA, chiave assente nel mono). Test aggiunti: RAW riferimento con FITS candidato e RAW con `offset_adu` dichiarato uguale al FITS (separati solo dalla presenza dello stato del decoder); due RAW con `offset_adu` dichiarati diversi nello stesso gruppo; guasto iniettato che precede gli stop della formazione dei gruppi; permutazione di un flat con `flat_level`; gruppo flat RAW.
- **Review giro 2: PASS** (`.omc/artifacts/ask/review-M3b-B13-round2-claude-20261111-0046.md`). I tre LOW (due commenti, ordine di un include) corretti dal coordinatore.

## Decisioni successive (B14)

- **API** (provvisoria fino a B11, firme nel commento in testa a `tests/unit/test_master_b14.cpp`): `propose_masters(master_proposal_request, fault_injector)` → `master_proposal` con `canonical()` (`astro.master_proposal.v1`); `proposed_slot_candidates(master_proposal)`; `slot_candidate::proposed_frames` e `selected_candidate::proposed_frames` (`std::optional<std::vector<content_digest>>`, inizializzati a `std::nullopt`: la presenza marca il candidato (c) di CL-06); `calibration_analysis::flat_thermal_pending_master`; `verify_selection(sel, observed, observed_proposed_node)`.
- **Scelte dell'utente** (sottoinsieme, metodo esperto) e avviso `proposal_not_followed`: fuori da `propose_masters`, nel report di B11 e nel job generale di B16.
- **Forma provvisoria** di `astro.master_proposal.v1`: `{format, role, params_missing, candidates, groups}`; gruppo `{reference, members, status, proposal|null, proposal_absent_reason, alternatives, node_key}`; parametro `analysis` del nodo (L8) `{role, candidates, group}`; frame identificati dal digest, gruppo dal digest del riferimento; motivi ed `values` delle esclusioni come in B12. La chiave del gruppo nel report resta a B11.
- **Stato del decoder dei RAW proposti** dall'osservazione del frame (stessa fonte dell'analisi), non dal chiamante.
- **Verifica termica `flat.bias` con master in attesa**: campo `flat_thermal_pending_master` di `calibration_analysis`, catena ancora ammissibile; falso se flat e bias sono esterni; nel JSON `astro.batch_proposal.v1` entra solo con esito in attesa (sostituito dalle correzioni del giro 1, sotto).
- **`verify_selection` sul candidato (c)**: frame del nodo diversi da quelli scelti, o nodo non osservato → `integrity`, `calib.proposed_node_changed`, contesto {`node_key`, `field` ∈ {`frames`, `observation`}}, dopo il controllo del decoder; nessun frame escluso.
- **`output_type` della ricetta proposta**: il default (f32); un'altra scelta è dell'utente (B16).
- **Helper unico per le regole riscritte** (esposizione, temperatura, livelli; residuo di B12 e B13): non in B14, perché B10 lavora su `master.cpp` in parallelo; si riunisce quando nessuna unità tocca `master.cpp` (al più tardi in B11). Il preflight dei nodi proposti si verifica chiamando il preflight vero del master (`combine_master`, ricetta e nodo), quindi una divergenza delle regole riscritte fallisce i test di B14.
- **Digest dell'artefatto per candidati permutati** (T-M3B-C02-10, FLAT-08): richiede il job, quindi B16; B14 verifica documento, chiavi dei nodi e `analysis`.
- **`duplicate_digest` nel documento della proposta e in `analysis`**: senza `values.primo`, che è un `input_id` e romperebbe l'indipendenza dagli `input_id` (T-M3B-C02-10); nel risultato di `screen_master_frames` resta. B11 decide la forma del report.
- **Stop di coerenza della richiesta di `propose_masters`**: `invalid_input`, `master.proposal.frame_digest` (digest della specifica diverso da quello dell'analisi) e `master.proposal.input_id` (`input_id` ripetuto); codici nuovi, interni all'API, con test. Un candidato con `proposed_frames` e senza `node_key` in `verify_selection` è un errore di programma (`ASTRO_ASSERT`).

### Correzioni della review di B14 (giro 1)

FAIL con otto MED (`.omc/artifacts/ask/review-M3b-B14-round1-claude-20261111-0330.md`). Decisioni:

- **`temperature_abs_tol_c` sempre richiesto**: la ricetta del master lo vuole per ogni politica (`validate_master_recipe`), quindi l'analisi torna a L4 (tutti i ruoli `exposure_rel_tol`, `temperature_abs_tol_c` e `temperature_policy`) e la sua assenza è `params_missing` anche con `not_applicable_declared`. Sostituisce la regola di «Decisioni successive (B12)» (tolleranza richiesta solo con `compare`); i test di B12 che la attendevano cambiano. Così ogni nodo proposto supera il preflight anche con `not_applicable_declared`.
- **`thermal_check_pending_master` dichiarato nel documento**: chiave `flat_thermal_pending_master: true` nell'oggetto `analysis` del gruppo di `astro.batch_proposal.v1`, accanto a `flat_thermal_within_budget`, presente solo quando l'esito è in attesa. Deroga motivata allo schema invariato: chiave aggiuntiva e assente in ogni altro caso, quindi i documenti esistenti restano byte per byte uguali; CL-06 (c) vieta di ometterla in silenzio. Il ricalcolo del controllo termico col master eseguito, prima che la catena giri, è di B16 (`plans/M3b.md`).
- **Sezione «Analisi» del report**: la costruisce B11 dal parametro `analysis` del nodo (piano aggiornato). B14 copre la sorgente (`analysis`), non il report: la copertura di T-M3B-C02-11 in B14 si chiama «sorgente della sezione Analisi».
- **Candidati con la stessa `node_key` nello stesso ruolo** (per esempio il nodo proposto e lo stesso nodo già eseguito): `invalid_input`, `classify.duplicate_candidate`, contesto {`ruolo`, `node_key`}, con test; la scelta non deve dipendere dall'ordinamento.
- **`spec.decoder` del chiamante**: se presente e diverso dallo stato dell'osservazione, `invalid_input`, `master.proposal.decoder_state`; le dichiarazioni della specifica non si confrontano con quelle dell'analisi (vincolo scritto nel commento dell'API: la richiesta porta le stesse dichiarazioni usate per l'osservazione).
- **Dominio di `conversion_gain_e_per_adu` e `read_noise_adu`**: si valida solo quando serve un gruppo `single`; uno stop ferma l'intera proposta (scritto nel commento dell'API).
- **Prove aggiunte**: due copie dello stesso digest con `input_id` scambiati (documento, `analysis` e chiave uguali, `values` vuoto); `select_recipe` su `flat.bias` con bias proposto; fixture con `input_id` in ordine inverso ai digest; confine vero dell'esposizione con valori esatti in binario e gruppo RAW con nero a tolleranza esatta col preflight del nodo; valori attesi dell'esclusione scritti a mano; `alternatives` e `proposal_absent_reason`; permutazione dell'ingresso di `propose_masters` in ANA-01.

### Arbitraggio dopo la review di B14 (giro 2)

FAIL con un MED (`.omc/artifacts/ask/review-M3b-B14-round2-claude-20261111-0407.md`): testo del piano, sezione B14, che assegnava ancora a B14 la sezione «Analisi» del report e il digest dei permutati. Tetto di due giri raggiunto: arbitra il coordinatore. Il MED è un difetto del piano, non del codice: corretto dal coordinatore (B14 copre la sorgente `analysis`, B11 la sezione del report, B16 il digest). LOW corretti: testi su `astro.batch_proposal.v1`, posizione della chiave (oggetto `analysis` del gruppo) e test che la legge lì; controllo di `spec.decoder` spostato nel ciclo iniziale di `propose_masters`, per ogni frame, con test dell'identità diversa a stato uguale. B14 si chiude con questi interventi e la verifica completa dell'albero.

## Decisioni successive (B10)

- **B10 non cambia il prodotto**: i 15 test di invarianza (`tile_rows` ∈ {1, 7, 64}, worker ∈ {1, 4}, blocchi e memoria; master, piano, maschera e digest dell'artefatto) sono verdi sul codice di B9. La prova che discriminano sono le mutazioni (`tmp/b10-mut/`).
- **Mutazioni** (release, regex `MasterB10\.`): (1) `channel_of(c, y0+r)` → `channel_of(c, r)` nella banda `median_ratio`: falliscono i cinque FLAT-08 e MEM-01; (2) ordine dei frame invertito nei task dispari: 14/15; (3) `sort_slot` condiviso fra task: 13/15 falliti, 12 per asserzione e 1 in abort (`T_M2R2R_02`), e `tools/mutate.sh` esce con 2 con la diagnosi «INFRASTRUTTURA: … fallito senza marcatore CHECK-RESULT né fallimento gtest», perché l'abort non lascia il marcatore: la mutazione è rilevata, l'uscita 2 è il limite dello strumento sugli abort; (4) maschera che ignora l'ultimo frame con blocchi > 32 righe: 14/15; (5) indicatori `kept` condivisi fra task: 5/15, i due `sigma_clip`, i due test f32 e T-M2R2R-02.
- **Etichetta `fast`** per `MasterB10` (47 s a macchina libera): gli `slow` restano i Monte Carlo da minuti.
- **Uguaglianza fra backend scalare e avx2**: non richiesta da §b.8; la variante `T-M2R5-01.avx2.test_master_b10` riesegue la suite sul secondo percorso.
- **T-M3B-MEM-01 esteso ai metodi**: il minimo del job coincide col modello scritto a mano (formula di B7) per ogni metodo e per `median_ratio`; con minimo − 1 stop `sched.job_exceeds_budget` senza letture.
- `docs/test-matrix.md`: le righe di M3.b restano «PIANIFICATO» fino al gate, come per le unità precedenti.

### Correzioni della review di B10 (giro 1)

PASS con LOW (`.omc/artifacts/ask/review-M3b-B10-round1-claude-20261111-0311.md`), corretti in un giro: testo delle mutazioni (sopra); test di B10 (piano del job confrontato con `plan_resources`, `single` in MEM-01 e nel test f32, soglia dei pixel marcati derivata, derivazione del budget del flat, `expected_minimum` condivisa in `master_support.hpp`, include, prova di «nessuna lettura» col picco del pool); i quattro LOW di prova lasciati da B9 (esiti `support_incomplete` contati a parte e stampati, `EXPECT_GT(checked_in[1], 0)`, controllo tautologico tolto (resta `EXPECT_GT(checked_in[1], 0)`), limite inferiore di `pixels_nonpositive_signal` con `sigma_clip` dichiarato).
- **Review giro 2: PASS** (`.omc/artifacts/ask/review-M3b-B10-round2-claude-20261111-0340.md`). Corretti dal coordinatore: uguaglianze tautologiche tolte, commento sul limite inferiore, testo della mutazione (5), riga lunga. Restano LOW: `expected_plan()` di B10 e `master_job_input` di `test_master.cpp` ricostruiscono lo stesso `planner_input` (helper da condividere in `master_support.hpp` alla prossima unità che li tocca); `ASSERT_GE` ridondante nel test dei pixel marcati.

## Decisioni successive (B11)

- **Nomi definitivi del report**: restano quelli provvisori già in uso (`plane` con `s2_over_n`, `weighted_residuals`, `kappa_s2`, `kept_s2_over_m`, `read_noise2_plus_signal_over_gain`; `flat_levels{label, level, rel_dev}`; `channels[c].flat_level_mean`; campi dei livelli `null` con `median_ratio`; `null_reason`, `variance_cv_null_reason`; chiavi italiane dei `values` delle esclusioni). Campi nuovi: `frames[k].residuals[c]{label, residual_mean_adu, residual_rms_adu}`, `frames[k].invalid_samples`, `channels[c].invalid_counts`, `channels[c].pixels_with_rejection` (0 senza scarti: è un conteggio, non un dato assente), `channels[c].flat_normalization{level_ratio, additive_bound_adu, rel_budget, check_value, norm_level_bias_bound, bound_ratio_median, bound_ratio_max, bound_ratio_null, bound_ratio_null_reason}`, `constants{kappa, a_f[], c_f[], flat_scales[{label, m[], r[], m_H, rho}]}` copiato tale e quale in `details.master_constants`, sezione `analysis{role, candidates, group{…, key}, chosen, proposal_followed}`.
- **`master_algorithm_version` = 4** (deciso in B9): i nodi master precedenti diventano `stale`.
- **Residui con `median_ratio`**: `r = y_f − M` sui campioni normalizzati, quelli che il metodo combina.
- **Chiave del gruppo** in `analysis.group.key`, prodotta da `propose_masters` (cambia la chiave dei nodi proposti, coperto dalla versione 4); campi {width, height, binning_x, binning_y, cfa, camera, gain, filter, roi_x, roi_y, readout_mode, row_order, decoder_state, offset_adu}.
- **`check_value` e ipotesi `level_proportional`, `additive_bounded`**: solo per i metodi diversi da `mean` con `median_ratio` (con `mean` il budget è vietato e FLAT-02(a) vale per ogni β).
- **Avviso con `median_ratio` per i metodi robusti**: `gaussian_reference_not_verified`, con `gaussian_reference_heteroscedastic` fra le ipotesi (FLAT-05, rinviato da B9).
- **`proposal_not_followed`**: confronta `combine`, `rejection` e `flat_normalization` della ricetta con la proposta.
- **Coerenza del parametro `analysis`**: se i membri del gruppo in `analysis` non sono i frame del master, `invalid_input`, `master.param.analysis`, con test.
- `single`: nessuna ipotesi oltre quelle di §b.3; `duplicate_digest` nel report senza `primo`, come in `analysis`.
- **Due punti di controllo per `analysis`**: `validate_master_recipe` confronta i membri solo se `group.members` esiste (DSC-01 di B3 ammette un oggetto qualunque nel nodo); `make_plan` controlla la forma completa prodotta da `propose_masters` prima di ogni lettura, con lo stesso codice `master.param.analysis`. Un nodo con `analysis` di forma diversa si crea ma si ferma all'esecuzione, prima di leggere.
- **Gruppo senza proposta** (`proposal` null) con `analysis` presente: `proposal_followed = false` e avviso `proposal_not_followed`.
- **Normalizzazione proposta**: il documento della proposta non porta `flat_normalization`; il confronto assume `none`, l'unica proposta da §f.2.
- **Ipotesi con `median_ratio`**: tolte quelle dei livelli di `.a` (`illumination_stable`, `level_proportional` sempre, `additive_nonnegative`, `nonnegative_signal`), sostituite da quelle di §f.3; per i metodi robusti restano `iid_gaussian` e `continuous_samples` di §b.3.
- **Chiave del gruppo dei RAW**: `offset_adu` null; i campi strutturali RAW stanno in `frame_decoder_states` del nodo, non ripetuti nella chiave del report.
- **Nuovi avvisi**: `gaussian_reference_not_verified`, `noise_model_variance`, `flat_norm_scales_estimated`, `proposal_not_followed`.

### Correzioni della review di B11 (giro 1)

FAIL con sei MED (`.omc/artifacts/ask/review-M3b-B11-round1-claude-20261111-0644.md`). Decisioni del coordinatore:
- **Forma completa di `analysis` all'esecuzione**: `make_plan` richiede, col loro tipo, anche `group.key`, `group.reference`, `group.status`, `group.proposal_absent_reason` e `group.alternatives`, come li scrive `propose_masters`; stesso stop `invalid_input`, `master.param.analysis`, prima di ogni lettura. Test con nodo creato da `make_master_node` e `analysis` malformata in più modi (ruolo diverso, membri assenti, proposta assente o di tipo sbagliato, campo nuovo assente): `combine_master` e job fermi, `frames.read` non raggiunto, nessun artefatto.
- **Statistiche diagnostiche della normalizzazione** (`norm_level_bias_bound`, `check_value`, `level_ratio`, `q = bound/v`, `bound_ratio_*`): regola §6.3 di C-02.a come in `.a` (`mul_ok`/`div_ok`): prodotto o quoziente di operandi non nulli che dà zero o subnormale, o risultato non finito, → `master.report_nonfinite` coi valori nel contesto. `A = 0` (operando nullo) resta un risultato 0 valido. Test: almeno un caso flat `median_ratio` che arriva allo stop attraverso il bound o il rapporto.
- **Ipotesi esatte**: i test confrontano l'insieme intero dei nomi per ogni metodo, dark e flat, mono e CFA, `single` compreso.
- **`proposal_followed`**: test con `group.proposal` null e con `rejection` diversa.
- LOW: commenti dei test allineati alle decisioni, definizione di `invalid_samples`, valore di `cfa` nella chiave del mono, test dei `details` dichiarato di identità, commento sulla varianza attorno a 1 (§f.6).
- **Review giro 2: PASS** (`.omc/artifacts/ask/review-M3b-B11-round2-claude-20261111-0724.md`). Corretto dal coordinatore: commento dei rapporti col piano («normali o nulli»). Restano LOW: contesto dello stop del rapporto senza `x`, `y` del pixel; ipotesi esatte del flat `none` provate solo per `mean` e `single`; casi di forma già controllati al giro 1 senza test (`role` assente o non testo, `candidates` assente, `group` non oggetto, `members` non elenco, `proposal.combine`/`rejection` assenti); `master.report_nonfinite` via `level_ratio` e `check_value` senza test. Da chiudere alla prossima unità che tocca `master.cpp` (B15).

## Decisioni successive (B15)

- **Lettore RAW delle statistiche**: sovraccarico di `read_master_frame_stats` con `const input_format&` dopo il digest; il lettore FITS a cinque argomenti resta invariato. Esiti di decodifica `raw.*` (compresi `raw.not_cfa`, `raw.data_error`) restituiti come `decode_failure`, non come stop; `input.*`, cancel e `sched.job_exceeds_budget` restano stop. (Classificazione degli esiti del decoder superata dalle correzioni del giro 1, sotto.)
- **`master.decoder_state_changed` anche in memoria**: `combine_master` confronta lo stato decodificato di ogni immagine con `frame_decoder_states` della ricetta prima di combinare; il contesto porta il nome canonico del frame (`frame`) e il nome del campo diverso (`campo`), come `master.mismatch.<campo>`.
- **`single` da RAW in memoria** entra in B15 (piano sull'`offset_adu` della ricetta).
- **A B16** restano la lettura RAW nel job (decodifica in `prepare`, spill, riverifica in finalize, L10 opzione C) e l'aggancio del lettore RAW all'analisi.
- **Residuo di B12** (`check_unchanged` sul ramo d'errore di `plan_fits`): chiuso in B15 con un test deterministico (file cambiato e ripristinato al primo `input.read`, mtime spostato: atteso `input.modified`).
- Il test di B15 usa `main_utc0.cpp`: il decoder RAW richiede TZ=UTC0.
- **Scelte dell'autore del codice di B15**, confermate dal coordinatore: immagine senza `decoder_state` per un frame RAW della ricetta (o con stato per un frame FITS) → `master.decoder_state_changed` con `campo` = `decoder_state`; in `combine_master` si confronta lo stato, non l'identità del decoder (le immagini non la portano), coi nomi dei membri di `decoder_state`; `raw.cancelled` e `raw.decode_context` restano stop, ogni altro `raw.*` (compreso `raw.too_big`) è esito di decodifica; il lettore RAW legge il file entro il budget del pool e prenota file + `raw_decode_needed_bytes` per tutta la decodifica, con `sched.job_exceeds_budget` prima della prenotazione; formato diverso da FITS e RAW nel sovraccarico: asserzione (errore di programmazione, i formati sono chiusi). (Classificazione degli esiti del decoder superata dalle correzioni del giro 1, sotto.)
- **`offset_adu` dei RAW solo dalla ricetta** (L9): un frame RAW con `offset_adu` nei metadati (anche dichiarato) → `invalid_input`, `master.param.offset_adu`; con frame RAW `offset_adu` non entra nel confronto col riferimento; nel master da RAW `offset_adu` ha origine `declared` e nel report `source` = `"declared"`, assente se la ricetta non lo porta.
- **Test aggiunti dopo il codice** (dall'autore dei test, prima della review): `offset_adu` nei metadati di un RAW (in immagine e dichiarato), `offset_adu` del master da RAW con origine `declared` e assente senza ricetta, immagine senza `decoder_state`; LOW di B11: contesto `x`, `y` del rapporto, casi di forma di `analysis`, ipotesi del flat `none` su tutti i metodi, `master.report_nonfinite` via `check_value`. `analysis` con `group.members` non elenco: lo stop `master.param.analysis` arriva già in `make_master_node` (`validate_master_recipe` confronta i membri quando il campo esiste), stesso codice, prima di ogni lettura: lettura confermata della decisione di B11. Flat `none`: solo `single` e `mean` ammessi (§b.2, §f.2), gli altri si fermano con `master.variance_domain`, nessuna ipotesi da confrontare. `level_ratio` non arriva a `master.report_nonfinite` coi metodi robusti (il controllo 3, max m ≤ 4 min m, lo limita a 4); con `mean` e `weighted_mean` il controllo 3 non c'è e l'overflow è raggiungibile (test nel giro di correzioni).

### Correzioni della review di B15 (giro 1)

FAIL con due MED (`.omc/artifacts/ask/review-M3b-B15-round1-claude-20261111-0846.md`). Decisioni del coordinatore:
- **Esiti di decodifica RAW solo per i limiti propri del file**: `LIBRAW_TOO_BIG` (dimensioni dichiarate dal file), `raw.max_pixels`, tetto di zlib e gli altri difetti del contenuto restano `decode_failure`; la memoria negata dal sistema (`LIBRAW_UNSUFFICIENT_MEMORY`, `LIBRAW_MEMPOOL_OVERFLOW`, `Z_MEM_ERROR`) e `raw.libraw_error` (rc sconosciuti) sono stop: un esito dell'analisi non dipende dalla macchina (NC-21, regola di B12). La distinzione usa il codice o il contesto (`rc`, `funzione`) che il decoder già emette, senza cambiare i codici del decoder.
- **Cancel durante la decodifica**: test deterministico se i ganci lo permettono; altrimenti argomento scritto qui e mutazione documentata.
- **`p.offset` dei RAW**: nessun valore di default per un dato assente; asserzione che il flat lo abbia, o `optional` letto solo dal flat.
- **Limite dichiarato**: i byte del file letto (FITS e RAW) stanno fuori dal pool fra la lettura verificata e la prenotazione; la lettura è entro `pool.budget_bytes()`. Classe della base comune, non di B15: si chiude con la lettura verificata prenotata nel pool, in un'unità che tocca quel confine.
- LOW di test: immagine FITS con `decoder_state`; `campo` per ogni membro (test parametrico); chiavi di contesto del budget; file RAW più grande del budget; i due controlli di cancel distinti; contesto `frame` e `valore` dello stop `master.param.offset_adu` per frame; `level_ratio` in overflow con `mean` + `median_ratio`; commenti di testa allineati; riga di trace di T-M3B-CFA-01. LOW di codice: commento di `stats_of`; mutazioni per le funzioni coperte dai test aggiunti dopo il codice (stop per frame, origine `declared`, `x`/`y`); il salto di `offset_adu` in `compare_with_reference` protetto da asserzione esplicita invece che dal solo invariante.
- **Prova dei casi non riproducibili dal lettore** (memoria negata dal sistema, `raw.libraw_error`, `raw.cancelled` durante la decodifica: il lettore chiama il decoder senza ganci, LibRaw alloca con `malloc`, il cancel durante la decodifica dipende dai tempi fra thread): la classificazione degli errori del decoder (esito del file contro stop) sta in una funzione pura dichiarata in un header interno, provata direttamente con `stop_error` sintetici che portano codice, categoria e contesto (`rc`, `funzione`) come li emette il decoder. Il lettore la usa senza altra logica. `LIBRAW_TOO_BIG` non si produce dal file (il decoder fissa `max_raw_memory_mb` dalle dimensioni del file, T-M3A-RAW-06): il lato «limite del file» si prova con `raw.max_pixels` dal lettore.
- **Classificazione degli errori del decoder** (`master_detail::raw_decode_error_is_file_outcome`, `src/core/master_analysis_internal.hpp`): esito solo per i `raw.*` attribuiti in positivo al file; stop per categoria `cancelled` e `io`, codici fuori da `raw.*`, `raw.decode_context`, `raw.libraw_error`; `raw.too_big` esito solo con contesto `rifiuto` = `tetto` (zlib) o `valore` = `LIBRAW_TOO_BIG`, stop con `rifiuto` = `sistema`, con gli rc di memoria di LibRaw o con contesto non riconosciuto. `master_plan::offset` è `optional`, letto solo dal flat. L'asserzione in `compare_with_reference` ferma anche da sola un frame RAW con `offset_adu` (mutazione dello stop per frame misurata togliendo stop e asserzione insieme).
- **Mutazioni del giro di correzioni** (`tmp/b15-mut-r1/`, release, `^(MasterB11|MasterB15)\.`): origine `declared` → `header` e `source` → `common`: RILEVATE da `T_M3B_CFA_01_raw_flat_master_carries_the_recipe_offset_adu_with_declared_origin_in_metadata_and_report`; `x`/`y` tolti dal contesto del rapporto: RILEVATA da `T_M3B_C02_15_subnormal_normalization_diagnostics_stop_with_report_nonfinite_and_no_extra_store_work`; stop per frame e asserzione tolti insieme: RILEVATA dai due test `offset_adu` per frame; classificazione (zlib `sistema`, `valore` = UNSUFFICIENT_MEMORY, categoria `cancelled`, `raw.libraw_error` come esito): 4/4 RILEVATE da `T_M3B_C02_08_L2_raw_decoder_errors_are_file_outcomes_only_when_attributed_to_the_file`. Limite dichiarato: dal lettore si produce solo `raw.max_pixels`, quindi un lettore che tornasse a un predicato proprio non sarebbe rilevato; il gancio di prova sul lettore va in B16 (lettura RAW nel job).
- **Review giro 2: PASS** (`.omc/artifacts/ask/review-M3b-B15-round2-claude-20261111-1002.md`). Corretti dal coordinatore: voci superate marcate, mutazioni elencate, commento del limite anche nel lettore FITS, riga vuota e commento con storia nel test di B11.

## Decisioni successive (B17)

- **Parametri d'analisi e della ricetta su PUB-LFC-01**: stanno solo nel record `docs/datasets/PUB-LFC-01.md`, «Parametri d'uso in M3.b», con l'origine (T-M2E-13, T-M2R2-09); il test li cita da lì.
- **`A` di T-M3B-FLAT-09** (corretta al giro 1 della review: la mediana era un livello tipico, non il limite di C-02 §f.1): `A` = massimo su ogni pixel della regione dati (colonne 0…2047, la stessa dell'offset) di |media dei dark-flat della stessa esposizione − offset|, sui campioni non saturi, per banda, valore esatto. Se pochi pixel caldi lo rendono enorme si dichiara anche il quantile 99,9 % e la frazione di pixel sopra di esso, ma `A` resta il massimo (l'ipotesi `additive_bounded` vale per ogni pixel). Script di misura versionato, con letture allineate alle card FITS. Con `mean` `A` non cambia valori, costanti né piano: entra solo nelle diagnostiche del report.
- **Frame**: i set del record e del piano (bias `ccd.001–006` + `darks/ccd.002`, dark `darks/ccd.013–015`, dark-flat `darks/ccd.017–019` e `ccd.023–025`, flat g `ccd.014–016`, flat i `ccd.017–019`); dark-flat come ruolo `dark_flat` dichiarato.
- **Etichetta**: `slow`, seriale, come T-M2R2-09.
- **Controllo di non vacuità dell'oracle corretto dopo la prima misura**: la prima normalizzazione per |M| era sbagliata (cancellazione di `offset + r(x − offset)` con M piccolo rispetto ai termini); ora è rispetto alla somma dei moduli dei termini, con la derivazione nell'intestazione del test. Il criterio di accettazione (M e piano dentro l'intervallo della regola I) è quello fissato prima ed è invariato; la correzione riguarda solo il controllo che l'intervallo non sia vuoto di significato.
- **Non coperto dal dato reale** (N = 3 per ruolo, mono FITS): n grande, CFA e RAW (coperti dai test sintetici di B10, B15, B16), controllo termico di `flat.bias` (B16, sintetico), confronto fra backend (ADR-07, fuori da M3.b).

### Correzioni della review di B17 (giro 1)

FAIL con due MED (`.omc/artifacts/ask/review-M3b-B17-round1-claude-20261111-0916.md`): `A` (voce sopra) e byte non verificati in `propose()` (si passano i byte già verificati di `pub_frame`, D-08 §4 regola 4). LOW corretti: intestazione e costanti col rimando al record; commento senza storia; soglia di non vacuità al limite derivato 40ε (2·(2(n+3)+4+n+1) ε S con n = 3) (o margine motivato); ampiezza dell'intervallo di V riportata come diagnostica; `node_key` descritto come coerenza interna; esclusioni attese zero asserite, prova delle esclusioni nei test di B12; chiave di gruppo dell'oracle e motivo `absence` dichiarati come limiti; messaggio `PREREQUISITO MANCANTE: PUB-LFC-01 <percorso>`; BZERO/BSCALE letti e controllati, apice di chiusura controllato; `ASTRO_TEST_TMP` per preset sotto `tmp/`. La rilettura di T-M2R2-09 e T-M2R2-04 si cita alla chiusura dal run della verifica finale di B11.
- **Review giro 2: PASS** (`.omc/artifacts/ask/review-M3b-B17-round2-claude-20261111-0936.md`). Corretti prima della chiusura: aritmetica della soglia di non vacuità (40ε), `A²` con l'aritmetica a intervalli, ampiezza di V normalizzata sulla somma dei quadrati dei termini, record con `A` come stima conservativa da 3 frame e pixel saturo escluso invalido nei master del flat, attribuzione ai pixel caldi dichiarata come ipotesi, script con SHA-256 confrontato col record, diagnosi `sys.exit`, larghezza 2080 controllata (valori di `A` invariati). La rilettura di T-M2R2-09 e T-M2R2-04 è il run slow della verifica finale di B11 (`13325e1`, 38/38): B17 non cambia il prodotto.
