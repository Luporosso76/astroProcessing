# Contratto F-IO-03: classificazione batch (M3.a)

Stadio: F-IO-03, P0, M3.a (registro riga 3158; testo riga 1814 del piano). Archi: F-IO-03 `requires_all` F-IO-02 e G-CAL (righe 3687, 3688); M3.a `requires_all` F-IO-03 (riga 4236); F-CAL-01.b `requires_all` F-IO-03 (riga 3719); F-IO-07 e F-IO-08 `requires_all` F-IO-03 (righe 3692, 3694); F-UX-01 `requires_all` F-IO-03 (riga 3853). Lacune chiuse: L2 e L5 di `plans/M3a.md`. Dipendenze fra contratti: [ADR-16](ADR-16-stati-master.md) (`.a` «Stati tipizzati» e §2; `.b` §b.1–b.9, §b.14), [C-02.b](C-02-master.md#estensione-c-02b-master-completi-m3) §b.1, §b.2, [C-01.b](C-01-calibrazione-light.md) §2, [ADR-03](ADR-03-pixel-maschere.md) (metadati dichiarati, §b.2, §b.14), [ADR-08.b](ADR-08-decoder-pack.md) §2.6, §2.9, [ADR-04](ADR-04-persistenza.md) (schema, snapshot), [BASE](BASE-confini.md) (OB-11, §1, §3.1). Errori: [modello comune](ADR-05-dag-provenance.md#modello-derrore-comune).

Accettazione (riga 1814): «Import batch di light, dark, flat, dark-flat e bias con classificazione modificabile; controlla dimensioni, CFA, esposizione, gain, binning, filtro, temperatura e offset. Prima della scelta propone solo ricette compatibili e omissioni motivate. Dopo la scelta, un mismatch arresta il ramo senza sostituzione; il progetto resta apribile.»

Regole di lettura. Ogni clausola è una **regola** (vincolo del prodotto, con il test che la verifica), un **fatto** (con la fonte) o un'**ipotesi** (dichiarata, con il test che la prova o il limite). Ogni obbligo `CL-NN` ha un solo **proprietario**, la funzione che lo implementa; i nomi di funzione e di file nuovi sono proposte, il proprietario resta unico. Chi ha bisogno dell'obbligo chiama il proprietario e non lo riscrive. Le regole di compatibilità, gli stati, le catene e il default restano di ADR-16.b e C-01.b: questo contratto li usa e non li ripete.

Fuori da F-IO-03:

- formazione dei gruppi di frame di un ruolo e proposta dei master da quei gruppi (C-02.b §b.2, M3.b): F-IO-03 consegna l'insieme candidato classificato (CL-05);
- scelta automatica fra più candidati compatibili dello stesso ruolo per punteggio o evidenza (Advisor, ADR-15): qui la scelta, quando serve, è dell'utente (CL-08);
- esecuzione della ricetta selezionata, P-LOST nel preflight del job `.b`, effetti di §b.8 e resume (M3.c, ADR-16.b §b.8, T-M3A-CAL-05, 09, 10);
- ricollegamento di file spostati (F-IO-08, lacuna L3), cataloghi e filtri personalizzati, sessioni e target (F-IO-07, P1);
- interfaccia grafica della classificazione (F-UX-01, M4): si progetta col proprietario prima di ogni lavoro sulla GUI (AGENTS.md, «Autonomia»). Qui solo core e funzioni chiamabili da CLI e test.

## 1. Flusso

Regola. Tre fasi, come in ADR-16.b §b.1:

1. **Analisi del batch, prima della scelta.** Registrazione degli input e proposta di classificazione (CL-01, CL-04), classificazione effettiva (CL-03), insiemi per ruolo e candidati (CL-05, CL-06), gruppi di light (CL-07), candidato per ruolo (CL-08), analisi G-CAL per gruppo e proposta (CL-09). La registrazione (CL-01, CL-04) scrive input, job d'import e revisioni; il resto è una funzione pura degli ingressi persistiti e delle scelte fornite (CL-12), che non scrive nello store.
2. **Scelta.** L'utente sceglie una ricetta fra quelle proposte per un gruppo. La selezione è quella di ADR-16.b §b.1 punto 2 (ricetta esplicita, stato e identità del decoder osservati nell'analisi), con in più il light e il candidato di ogni ruolo. Una ricetta fuori dall'insieme ammissibile non diventa una selezione (G-STOP, ADR-16.b §b.9).
3. **Dopo la scelta.** Ogni differenza fra la selezione e ciò che si osserva di nuovo ferma il ramo, senza sostituzione (§9).

## 2. Classificazione proposta

**CL-01 — Proposta da regole dichiarate, mai indovinata.**

- Ruoli: `light`, `bias`, `dark`, `dark_flat`, `flat`, più lo stato esplicito `unclassified` («da classificare»), che porta sempre un motivo.
- Regola `imagetyp.v1` (input FITS importato). La proposta è il ruolo restituito da `imagetyp_role` (`calibration_internal.hpp:235`) sul valore di `IMAGETYP` del report di import. È la stessa funzione che applica il controllo `imagetyp_conflict` di ADR-16.a, quindi l'elenco dei valori riconosciuti ha una sola casa e non si copia. Fatto, dal codice: confronto esatto dopo il passaggio a minuscole ASCII, nessuna rimozione di spazi, undici valori (`light`, `light frame`, `dark`, `dark frame`, `flat`, `flat field`, `flat frame`, `bias`, `bias frame`, `dark flat`, `flat dark`). Esiti:
  - valore riconosciuto: il suo ruolo; `dark` e `dark frame` danno `dark`, mai `dark_flat`;
  - valore non riconosciuto: `unclassified`, motivo `imagetyp_unrecognized`, con il valore nell'evidenza;
  - keyword assente: `unclassified`, motivo `imagetyp_absent`.
- Regola `raw_no_type.v1` (input RAW). LibRaw non fornisce un tipo di immagine: ADR-08.b §2.6 elenca i metadati del decoder e nessuno è un tipo. Esito: sempre `unclassified`, motivo `raw_no_type_metadata`.
- Regola `import_stopped.v1`. Import fermato (per esempio `raw.xtrans`): `unclassified`, motivo `import_stopped`, con categoria e codice dello stop nell'evidenza.
- Mai fonte di classificazione: nome del file o della directory, estensione, esposizione, temperatura, gain, dimensioni, ordine di import. In particolare un dark all'esposizione del flat non diventa `dark_flat` (ADR-16.a, «Stati tipizzati»).
- Ipotesi H-IT1: il lettore FITS di ADR-03 restituisce le stringhe senza gli spazi finali, che nello standard FITS 4.0 (§4.2.1.1) non sono significativi. La prova T-M3A-05a usa `'Dark Frame   '`. Se l'ipotesi è smentita, il valore risulta `imagetyp_unrecognized`, quindi nessun ruolo inventato; la correzione spetta al lettore di ADR-03, non a questa regola.
- Coerenza con ADR-16.a («il prodotto non deduce il ruolo da `IMAGETYP`»). La proposta precede la selezione e porta l'origine (`rule`, con identificativo della regola ed evidenza). Il ruolo entra in una ricetta solo con la selezione esplicita dell'utente (§1, punto 2). Il controllo `imagetyp_conflict` resta invariato.
- Proprietario: `propose_classification` (nuovo `src/core/batch_classification.cpp`), funzione pura del report di import o dello stop d'import.
- Prova: T-M3A-05a.

## 3. Persistenza e correzione dell'utente

**CL-02 — Revisioni append-only nel progetto.**

- Dove: tabella `input_classifications` nello schema 1 di ADR-04, estesa prima di ogni rilascio come in ADR-04 «Estensione R4» (nessun progetto rilasciato da migrare). Colonne:
  - `input_id`: riferimento a `inputs.id`;
  - `revision`: intero ≥ 1;
  - `role`: uno dei sei valori di CL-01;
  - `source` ∈ {`rule`, `user`};
  - `rule_id`: per esempio `imagetyp.v1`, nullo se `source = user`;
  - `evidence_canonical`: JSON canonico, con il valore di `IMAGETYP` o il codice dello stop;
  - `declarations_canonical`: JSON canonico `astro.input_declarations.v1`, con i campi dichiarabili di ADR-03 (`declared_image_fields`: `binning_x`, `binning_y`, `unit`, `linearity`; campi di `frame_metadata`) e, solo per un master esterno, il `master_descriptor` di ADR-16.a;
  - `record_version` = 1.
- Chiave primaria (`input_id`, `revision`). Righe mai aggiornate né cancellate, con trigger come `snapshots`.
- Scrittura. Un elenco di revisioni (anche di più input) entra in una sola transazione: ognuna prende `revision` = massimo corrente + 1, e un errore non lascia nulla scritto. Ogni revisione è completa, cioè ripete ruolo e dichiarazioni per intero e non porta un delta.
- Errori:
  - `input_id` assente: `invalid_input`, `classify.unknown_input`;
  - ruolo fuori elenco, `rule_id` presente con `source = user` o assente con `source = rule`: `invalid_input`, `classify.role`. Un `unclassified` scritto dall'utente porta il motivo `user`;
  - dichiarazioni non conformi ai tipi di ADR-03 (tipo della card, nessuna coercizione) o descrittore non valido per ADR-16.a: `invalid_input`, `classify.declaration`.
- Proprietario: `sqlite_dag_store::append_classifications` (`sqlite_dag_store.cpp`; DDL e trigger in `schema_v1` di `sqlite_project.cpp`).
- Prova: T-M3A-05b.

**CL-03 — La correzione dell'utente prevale e si registra.**

- La classificazione effettiva di un input è la sua revisione di numero massimo.
- Le regole scrivono solo la revisione 1, alla registrazione (CL-04), e mai più per quell'input. Così una revisione `user` non viene mai coperta da una proposta.
- Una regola nuova (`rule_id` con versione nuova) vale per gli input registrati dopo; gli input esistenti non si riclassificano da soli.
- La storia completa resta leggibile, con origine e regola di ogni revisione.
- Undo e redo di F-PRJ-04 (ADR-04, «Snapshot, undo e redo») non toccano le revisioni. Decisione: la classificazione non è una mutazione della ricetta, e la selezione congela i ruoli (§9). Tornare a un ruolo precedente è una nuova revisione `user`.
- Il ricollegamento (F-IO-08) conserva `input_id`, quindi la classificazione.
- Una correzione in conflitto con un `IMAGETYP` riconosciuto si registra comunque. Nell'analisi vale ADR-16.a: il candidato è `excluded` con `imagetyp_conflict`, e un light finisce in un gruppo `light_blocked` con `calib.imagetyp_conflict` (§7). Il prodotto non scavalca il controllo (limite, §14).
- `inputs.role` non è una fonte di classificazione. Gli input registrati da CL-04 vi scrivono la costante `batch`, e nessun codice del prodotto la legge come ruolo di calibrazione.
- Proprietario: `sqlite_dag_store::classifications` (lettura della revisione corrente e della storia).
- Prova: T-M3A-05b, 05c.

## 4. Registrazione del batch

**CL-04 — Un import per file, rami indipendenti, revisione 1.**

- Per ogni percorso:
  - `add_input`, con `role = batch`;
  - un job d'import del tipo già registrato: `io.import` se il file comincia con la card `SIMPLE  =` (FITS 4.0 §4.4.1.1, prima keyword obbligatoria), altrimenti `io.raw_import` (U4). Decide solo il contenuto: l'estensione non conta.
- I job si compongono con `run_branches`, un ramo per file. Un import fermato chiude solo il suo ramo (BASE §1: `run_branches` è una composizione, non un tipo di job). Poi le revisioni 1 di tutti gli input (CL-01) entrano in una sola chiamata di CL-02.
- Nessun tipo di job nuovo: F-IO-03 non entra nel registro di BASE §3.1.
- Proprietario: `import_batch` (`batch_classification.cpp`).
- Prova: T-M3A-05a (esito per file), T-M3A-06b (progetto riaperto).

## 5. Insiemi per ruolo e input esclusi dall'analisi

**CL-05 — Ogni input è analizzato o escluso con motivo.**

- Motivi di esclusione dall'analisi (lista chiusa, nella proposta con i valori):
  - `unclassified.<motivo>`, con il motivo di CL-01 o `user`;
  - `import_stopped`, con categoria e codice;
  - `classification_missing`: input senza revisione, per esempio con la registrazione interrotta;
  - `declared_conflict`: un valore dichiarato diverso da quello dell'header o del decoder, come `metadata.declared_conflict` di ADR-03 e ADR-03.b §b.1;
  - `duplicate_digest_conflict`: input con lo stesso SHA-256 registrato e classificazione effettiva (ruolo e dichiarazioni) diversa; tutte le copie sono escluse. Con classificazione uguale le copie sono un solo input analizzato, identificato dal digest, che riporta l'elenco dei loro `input_id`. È un raggruppamento su digest già registrati, non una verifica di byte (OB-11).
- Gli altri input formano un insieme per ruolo.
- I frame di un ruolo di calibrazione senza `master_descriptor` dichiarato sono l'«insieme candidato di frame di un ruolo» di C-02.b §b.1. La proposta li elenca per ruolo, con l'indicazione `frame_pending_master`; i gruppi e i master proposti li calcola C-02.b (M3.b).
- Proprietario: `analyze_batch` (`batch_classification.cpp`).
- Prova: T-M3A-05a, 05c.

**CL-06 — Candidati per ruolo.**

- Un candidato di un ruolo di calibrazione è uno fra:
  - (a) un input classificato in quel ruolo con `master_descriptor` dichiarato (master esterno);
  - (b) uno slot `current` il cui nodo è `master.combine` di quel ruolo, con il descrittore da `master_descriptor_from_node` (R2);
  - (c) da M3.b, un nodo `master.combine` proposto dall'analisi di C-02.b, con lo stesso descrittore derivato.
- Le tre fonti consegnano la stessa forma: descrittore, metadati di compatibilità con origine, stato del decoder o `frame_decoder_states`, e digest o chiave del nodo. Per questo (c) entra senza cambiare il contratto.
- In M3.a esistono solo (a) e (b). Fatto, da ADR-16.b §b.7 e §b.14: un light RAW con un master esterno o da FITS dà `decoder_state_mixed`. Quindi in M3.a un light RAW non ha candidati `usable`, e la sua proposta è `light.none` + `flat.none` con le esclusioni motivate.
- Proprietario: `analyze_batch`.
- Prova: T-M3A-04a, 04d.

## 6. Osservazione di un input

- Regola. L'analisi lavora su un'osservazione per input, con questi campi e le loro origini:
  - geometria e binning;
  - layout e CFA;
  - unità, fase e linearità;
  - `frame_metadata` interpretati;
  - piedistallo e coverage;
  - per i RAW, `decoder_state` (ADR-03.b §b.2) e identità del decoder (ADR-08.b §2.9).
- Le dichiarazioni si applicano con le regole di ADR-03 e sono un argomento esplicito: l'analisi passa quelle della revisione corrente, `verify_selection` e il preflight (M3.c) quelle congelate nella selezione (§9). Un costruttore unico, `make_observation` (`batch_classification.cpp`), riceve i campi dalle due sorgenti: il report di import (analisi) e l'immagine letta nel preflight. Nessuna seconda costruzione.
- Prova: T-M3A-04b (dichiarazioni applicate), T-M3A-06b (osservazione dal preflight del job del test).

## 7. Raggruppamento dei light

**CL-07 — Gruppi esatti, nessuna tolleranza nuova.**

- Due light stanno nello stesso gruppo se e solo se:
  1. hanno uguale chiave: presenza dello stato del decoder, larghezza, altezza, `binning_x`, `binning_y`, CFA (pattern e offset), `camera`, `gain`, `filter`, `roi_x`, `roi_y`, `readout_mode`, `row_order`, con «assente» come valore;
  2. hanno lo stesso esito del dominio del light (C-01.b §2: nessuno stop, oppure lo stesso codice);
  3. per ogni candidato di ogni ruolo, la coppia light–candidato di ADR-16.b §b.3 e §b.7 (L–D, L–D', L–B, L–F) ha lo stesso esito: `usable`, oppure `excluded` con lo stesso codice di motivo. I valori del motivo possono differire.
- Fatto, per costruzione. L'analisi di ADR-16.b §b.1, con un candidato fissato per ruolo, dipende dal light solo attraverso:
  - le coppie L–X;
  - il dominio;
  - la presenza dello stato del decoder (stato dell'offset, §b.5).

  Le coppie F–DF e F–B e il controllo termico non dipendono dal light. Quindi i light di un gruppo hanno la stessa analisi: stati per catena, predicati, insieme ammissibile e default. La proposta si calcola sul light di digest minimo e vale per ogni membro.
- Nessuna tolleranza nuova: esposizione e temperatura separano i light solo attraverso gli esiti delle coppie, con le tolleranze dichiarate di ADR-16.a §2.
- Esito di un gruppo:
  - `ready`: con proposta (CL-09);
  - `candidate_choice_required` (CL-08);
  - `light_blocked`: con categoria e codice dello stop del dominio o del light (`op.phase_blocked`, `op.linearity_blocked`, `calib.unit`, `calib.plane_layout`, `calib.pedestal`, `calib.coverage_input`, `calib.imagetyp_conflict`), senza ricette;
  - `params_missing`: con i nomi dei parametri d'analisi assenti, senza ricette e senza default (ADR-16.a §2: parametri obbligatori senza default).
- I parametri d'analisi stanno in `settings`, chiave `batch.analysis_params`, JSON canonico `astro.batch_analysis_params.v1`. Sono quelli di ADR-16.a §2, ADR-16.b §b.5 e §b.7 (per esempio `exposure_rel_tol`, `temperature_abs_tol_c`, `temperature_policy`, `decoder_black_abs_tol_adu`, le dichiarazioni termiche), e la loro validità è di U6/U7.
- Input prestacked e RGB (F-IO-10, ADR-03.b §b.14) classificati `light`:
  - un prestacked dà `light_blocked` con `op.phase_blocked`;
  - un RGB in fase `raw` dà `light_blocked` con `calib.plane_layout`;
  - come candidati sono `excluded` con `phase` o `layout` (C-01.b §2).
- Identità e ordine di un gruppo: il digest minimo dei membri. Membri in ordine di digest, a parità di digest in ordine di `input_id`.
- Proprietario: `group_lights` (`batch_classification.cpp`).
- Prova: T-M3A-04c, T-M3A-05d.

## 8. Scelta fra più candidati e proposta

**CL-08 — Un candidato per ruolo: automatico solo se unico.** Chiude il rinvio «F-IO-03, ADR-15» di ADR-16.b (introduzione e §b.14) e di C-02.b («Fuori da C-02.b»).

- Candidati idonei di un ruolo per un gruppo, `E(ruolo)`. Il candidato non deve essere escluso per stato, dominio, piedistallo o coverage (ADR-16.b §b.2, §b.3; C-01.b §2, §7). In più:
  - `dark` (`D` o `D'`): coppia L–D o L–D' compatibile;
  - `flat`: coppia L–F compatibile;
  - `bias`: coppia L–B compatibile, oppure F–B compatibile con almeno un flat idoneo;
  - `dark_flat`: coppia F–DF (o F–DF') compatibile con almeno un flat idoneo; se il gruppo ha flat candidati ma nessuno idoneo, la coppia si confronta coi flat candidati esclusi («il dark-flat si confronta col flat candidato, anche se escluso», ADR-16.b §b.3) e il dark-flat è idoneo solo se compatibile con almeno uno di essi; solo senza alcun flat candidato basta lo stato («senza flat candidato P-DF si calcola dal solo stato del dark-flat»).
- Esito per ruolo:
  - `|E| = 0` senza candidati del ruolo: ruolo assente nell'analisi (per esempio P-DF-ABS, ledger `master_absent`);
  - `|E| = 0` con almeno un candidato: il ruolo entra nell'analisi come presente ed escluso, con il primo candidato in ordine canonico (digest minimo, poi chiave del nodo) e il suo motivo (per esempio P-DF-BAD, ledger `master_excluded`, ADR-16.b §b.4, §b.10); gli altri candidati compaiono `excluded` col loro motivo;
  - `|E| = 1`: il candidato è scelto e la proposta registra `choice = only_compatible`;
  - `|E| ≥ 2`: il gruppo è `candidate_choice_required` e la proposta elenca `E` con gli esiti delle coppie. Finché l'utente non sceglie, il gruppo non ha ricette proposte né default.
- Scelta dell'utente: un candidato per ogni ruolo ambiguo, preso fra i candidati del ruolo (anche non idonei: l'analisi lo mostra `excluded`).
  - Candidato inesistente: `invalid_input`, `classify.choice_unknown_candidate`.
  - Selezione chiesta con un ruolo ambiguo senza scelta: `invalid_input`, `classify.choice_required`.
  - «Nessun candidato» per un ruolo ambiguo non è ammesso. Altrimenti il ledger direbbe `master_absent` con master presenti; l'omissione si ottiene scegliendo una ricetta che omette (`user_omitted`, ADR-16.b §b.5).
- La scelta dei candidati è un ingresso dell'analisi e non si persiste in M3.a: dopo una riapertura un gruppo ambiguo torna `candidate_choice_required` finché la scelta non è fornita di nuovo. La persiste la selezione (§9), che in M3.c diventa il nodo.
- Nessun ordinamento per vicinanza di temperatura, esposizione o data, e nessun criterio di qualità: sono materia dell'Advisor (ADR-15).
- Proprietario: `choose_candidates` (`batch_classification.cpp`).
- Prova: T-M3A-04d.

**CL-09 — Solo ricette ammissibili, omissioni motivate.**

- Per un gruppo `ready`, la proposta è l'uscita di `analyze_calibration` (U7, ADR-16.b §b.1 punto 1) sul light di riferimento con il candidato scelto per ruolo: stato e motivo di ogni candidato per catena, predicati, insieme ammissibile e default (D-16).
- La proposta aggiunge:
  - gli input esclusi (CL-05) e i candidati non idonei (CL-08), con motivo;
  - per il default, le voci `omitted` del ledger con motivo (ADR-16.b §b.10).
- Regola: nessuna ricetta fuori dall'insieme ammissibile compare nella proposta. Ogni ricetta proposta, selezionata, supera §b.9 senza G-STOP.
- Uscita: JSON canonico `astro.batch_proposal.v1`. Non si persiste: è derivata dagli ingressi persistiti (CL-02, `settings`, store), e la riapertura la riproduce identica a parità di scelte fornite (CL-12).
- Proprietario: `analyze_batch`, che chiama `analyze_calibration` e `pair_compatibility` senza copiarli.
- Prova: T-M3A-04a, 04e.

**CL-10 — Controlli dell'accettazione; estensione dichiarata per binning e filtro.**

Ogni controllo dell'accettazione corrisponde a una clausola esistente, con un solo proprietario: la funzione di compatibilità per coppia `pair_compatibility` (U6, `calibration_analysis.cpp`).

| Accettazione | Coppie | Regola | Motivo nell'analisi |
|---|---|---|---|
| dimensioni | tutte | uguaglianza di larghezza e altezza (ADR-16.a §2) | `incompatible.width`, `incompatible.height` |
| CFA | tutte | pattern e offset uguali; RAW: `cfa_color_index`, `window` e campi strutturali (ADR-16.b §b.7) | `incompatible.cfa`, `decoder_state_mismatch.<campo>` |
| esposizione | L–D, L–D', F–DF, F–DF' | entro `exposure_rel_tol` | `incompatible.exposure_s` |
| gain | L–D, L–D', F–DF, F–DF', L–B, F–B | uguaglianza | `incompatible.gain` |
| binning | tutte | uguaglianza di `binning_x`, `binning_y` | `incompatible.binning_x`, `incompatible.binning_y` |
| filtro | L–F | uguaglianza di `filter` | `incompatible.filter` |
| temperatura | L–D, L–D', F–DF, F–DF', L–B, F–B | entro `temperature_abs_tol_c` secondo `temperature_policy` | `incompatible.temperature_c` |
| offset | coppie additive | FITS: `offset_adu` uguale; RAW: nero effettivo entro `decoder_black_abs_tol_adu` (§b.7) | `incompatible.offset_adu`, `decoder_black` |

- Un campo richiesto assente dà `metadata_missing.<campo>` (ADR-16.a §2).
- Fatto: binning e filtro sono già nelle coppie di ADR-16.b §b.3, che rinvia alla parte `.a`:
  - binning in L–D, F–DF e L–F (`.a` §2), quindi in L–D', F–DF', L–B, F–B;
  - filtro in L–F.

  Il codice M2 li controlla (`calibration.cpp:285-289` binning, `:326-328` filtro, con `calib.metadata.<campo>` se assenti). La lacuna L2 nasce dalla tabella di §b.3, che non li nomina.
- Estensione dichiarata qui, che U6 implementa senza toccare il resto di §b.3:
  1. la tabella sopra rende espliciti, per tutte le coppie di §b.3, i campi e i motivi di binning e filtro;
  2. per gli input RAW binning e filtro vengono solo dalle dichiarazioni della revisione corrente (CL-02), con origine `declared`, perché il decoder non li produce (ADR-08.b §2.6). Il prodotto non assume mai `1×1` né «nessun filtro»: senza dichiarazione la coppia è `metadata_missing.binning_x` (o `.binning_y`, `.filter`);
  3. binning e filtro entrano nella chiave dei gruppi di light (CL-07, punto 1). Due light con binning o filtro diversi non stanno mai nello stesso gruppo, anche se nessun flat li separa.
- Prova: T-M3A-04a, 04b, 04c.

## 9. Dopo la scelta: mismatch (lacuna L5)

**CL-11 — Nessuna sostituzione; ripartizione fra M3.a, M3.b e M3.c.**

- La selezione è congelata. Contiene:
  - ricetta e parametri;
  - light e candidato per ruolo (input con digest e dimensione, oppure chiave del nodo del master);
  - dichiarazioni;
  - stato e identità del decoder di ogni ingresso RAW osservati nell'analisi.
- Una revisione di classificazione o di dichiarazioni successiva alla selezione non la modifica e non la ferma. Produce una nuova analisi, che propone nuove selezioni (decisione: la ricetta è esplicita, ADR-16.b §b.1).
- **Regola di M3.a**: `verify_selection(selezione, osservazioni nuove)` è una funzione pura. Per ogni ingresso RAW della selezione confronta stato e identità del decoder congelati con quelli della nuova decodifica. Alla prima differenza ferma con `integrity`, `calib.decoder_state_changed`, con il campo, `predicate = P-LOST` e `prerequisite = <ingresso>.decoder_state` nel contesto (codici di ADR-16.b §b.7 e §b.8). Uguali: nessun effetto.
- `verify_selection` non restituisce mai un'altra ricetta, un altro candidato o una selezione modificata: l'unica uscita è «nessun effetto» oppure lo stop.
- I byte degli ingressi non li confronta `verify_selection`: le osservazioni nuove vengono da letture verificate dal confine di BASE OB-11 (`hash.cpp`) contro digest e dimensione della selezione.
- Fatto: a byte verificati uguali e a stato e identità del decoder uguali, l'analisi pura (CL-09) dà lo stesso esito. Quindi la ricetta selezionata resta ammissibile senza rieseguirla.
- Proprietario: `verify_selection` (`calibration_analysis.cpp`). Il preflight del job `.b` (M3.c) la chiama e non la riscrive.

| Parte del mismatch dopo la scelta | Proprietario | Slice | Prova |
|---|---|---|---|
| File assente, digest o dimensione diversi | lettura degli input (OB-11) | base | T-M3BASE-11; sul ramo in T-M3A-06b |
| Stato o identità del decoder di un ingresso RAW diversi alla nuova decodifica | `verify_selection` | M3.a (funzione e stop del ramo); M3.c (chiamata nel preflight all'esecuzione) | T-M3A-06a, 06b; T-M3A-CAL-09 |
| Stato del decoder di un frame del master | job del master (`master.decoder_state_changed`, ADR-16.b §b.7) | M3.b | T-M3A-CAL-07, 09 |
| Master del prodotto: slot non `current`, nodo assente, descrittore o artefatto diversi | job di calibrazione (R2, ADR-16 «Aggiunte di R2») | M2, riusato in M3.c | T-M2E-11; T-M3A-CAL-05 |
| Effetti: 0 uscite, 0 nodi, ledger non convertito, report con ricetta intatta; resume | job `.b` | M3.c | T-M3A-CAL-05, 10 |
| Solo il ramo colpito si ferma, gli altri completano, il progetto si riapre con lo stesso stato logico | `run_branches` (M2) con `verify_selection` | M3.a | T-M3A-06b |

- Limite: in M3.a la selezione non diventa un nodo del prodotto, perché l'operatore `.b` lo registra M3.c (BASE §3.1). T-M3A-06b usa un job del test composto da `run_branches`, che legge gli ingressi dal confine e chiama `verify_selection`, come la lacuna L6 per G-PACK. Un tipo di job solo di test non entra nel registro del prodotto.

## 10. Determinismo e purezza

**CL-12 — Stessi ingressi persistiti, stessa proposta.**

- `analyze_batch` è una funzione pura di: input registrati, revisioni correnti, report di import, slot dei master, parametri d'analisi, scelte dei candidati.
- Con le stesse scelte dei candidati fornite, permutare l'ordine di registrazione degli input, o riaprire il progetto, dà la stessa proposta, byte per byte in JSON canonico. Nella forma canonica gli input sono identificati dal digest; gli `input_id`, identità locale che dipende dall'ordine di registrazione, stanno in un elenco a parte escluso dal confronto.
- Nessuna scrittura nello store.
- Nessuna lettura di campioni, tranne quella del controllo termico di `flat.bias` (ADR-16.b §b.1, punto 1).
- Proprietario: `analyze_batch`.
- Prova: T-M3A-04c, 04f.

## 11. Errori aggiunti

| Condizione | Categoria | Codice |
|---|---|---|
| `input_id` sconosciuto in una revisione | `invalid_input` | `classify.unknown_input` |
| Ruolo fuori elenco; `rule_id` incoerente con `source` | `invalid_input` | `classify.role` |
| Dichiarazioni o descrittore non conformi ad ADR-03 o ADR-16.a | `invalid_input` | `classify.declaration` |
| Scelta di un candidato che non esiste per quel ruolo | `invalid_input` | `classify.choice_unknown_candidate` |
| Selezione con un ruolo ambiguo senza scelta | `invalid_input` | `classify.choice_required` |

Gli stop di analisi, selezione ed esecuzione restano quelli di ADR-16.b, C-01.b e BASE. Gli esiti di CL-05, CL-07 e CL-08 non sono errori: sono esiti della proposta.

## 12. Test pianificati

Verità di riferimento indipendenti dall'implementazione:

- tabelle scritte a mano dall'autore dei test, cioè l'elenco dei valori di `IMAGETYP` copiato da ADR-16.a e le catene e il default ricodificati da ADR-16.b §b.5 e §b.6, con la stessa tabella di controllo di §b.13;
- file FITS con header noti e DNG `FIX-SYN-DNG-01` scritti dal generatore delle fixture;
- digest scritti dal generatore;
- lettura delle righe di `input_classifications` con SQL diretto, non con l'API.

Criteri tutti esatti o binari: nessuna tolleranza nuova. Le lettere distinguono i casi di uno stesso ID.

| ID | Caso | Verifica | Criterio |
|---|---|---|---|
| T-M3A-04 | a | Batch FITS con un light e un master per ruolo; ogni riga della tabella di CL-10 violata da un master alla volta (esposizione e temperatura alla tolleranza e oltre; nero RAW alla tolleranza e tolleranza + 1, su osservazioni in memoria di un master con `frame_decoder_states`) | Motivo uguale alla tabella; insieme ammissibile e default uguali all'oracle; nessuna ricetta proposta usa il candidato escluso |
| T-M3A-04 | b | Light e master RAW (osservazioni in memoria) senza dichiarazioni di binning o filtro, poi con dichiarazioni uguali e diverse; FITS con `XBINNING` presente e dichiarazione diversa | Senza: `metadata_missing.binning_x`, `.binning_y`, `.filter`; uguali: `usable`; diverse: `incompatible.<campo>`; conflitto con l'header: input escluso `declared_conflict` |
| T-M3A-04 | c | Batch di light progettato a mano: differenze solo di filtro, solo di binning, esposizione dentro e fuori `exposure_rel_tol` del dark, temperatura idem, dominio diverso (un light `calibrated`); copie con lo stesso digest e classificazione uguale, con ruoli diversi, e con lo stesso ruolo e dichiarazioni diverse; permutazioni dell'ordine di registrazione | Partizione, riferimento ed esito di ogni gruppo uguali all'oracle; light diversi per filtro o binning in gruppi distinti anche senza flat; copie concordi: un solo input con l'elenco degli `input_id`; discordi per ruolo o per dichiarazioni: tutte `duplicate_digest_conflict`, invariato fra le permutazioni; forma canonica della proposta byte-identica fra le permutazioni; per ogni gruppo, l'analisi di ogni membro uguale a quella del riferimento |
| T-M3A-04 | d | Per ruolo 0, 1, 2 candidati idonei; bias idoneo solo per F–B; due dark-flat di cui uno solo compatibile col flat; flat candidato escluso (per esempio `state_unknown`) con un dark-flat incompatibile con esso e un dark-flat compatibile, e senza alcun flat candidato; scelta dell'utente valida, inesistente, mancante; riapertura dopo una scelta su un gruppo ambiguo, senza e con la stessa scelta fornita di nuovo; master esterno e slot `current` come candidati; light RAW con soli master esterni | 0 senza candidati: ruolo assente (P-DF-ABS, `master_absent`); 0 con candidati: presente ed escluso (P-DF-BAD, `master_excluded` col motivo del primo canonico); 1: `only_compatible`; 2: `candidate_choice_required` e nessuna ricetta; scelta valida: proposta uguale all'oracle; `classify.choice_unknown_candidate`; `classify.choice_required`; riapertura: senza scelta `candidate_choice_required`, con la stessa scelta proposta byte-identica; light RAW: `decoder_state_mixed` e proposta `light.none` + `flat.none`; flat escluso: dark-flat incompatibile `excluded` `incompatible.<campo>`, compatibile idoneo; senza flat candidato: idoneità dal solo stato |
| T-M3A-04 | e | Per ogni configurazione di 04a–04d, ogni ricetta proposta passata alla selezione | Nessun G-STOP (§b.9); nessuna ricetta fuori dall'insieme ammissibile; omissioni del default con motivo uguali all'oracle |
| T-M3A-04 | f | Purezza: contatore delle letture di campioni; `analyze_batch` su uno store aperto in sola lettura (`open_intent::read_only`: ogni scrittura ferma con `store.read_only`) e `sqlite3_total_changes` della connessione prima e dopo; mutazione che inserisce una scrittura in `analyze_batch`; parametri d'analisi assenti uno alla volta | Letture di campioni solo per il controllo termico; analisi completata in sola lettura e `total_changes` invariato; la mutazione fa fallire il caso; `params_missing` con il nome, nessuna ricetta e nessun default |
| T-M3A-05 | a | `import_batch` su: FITS con ognuno degli undici `IMAGETYP` in minuscolo, maiuscolo e misto; `'Master Dark'`, `'darkflat'`, `'Dark_Frame'`, `' light'`, stringa vuota; keyword assente; `'Dark Frame   '` (H-IT1); file chiamato `dark_001.fits` con `IMAGETYP = 'Light'`; dark all'esposizione di un flat; DNG `FIX-SYN-DNG-01` e due file Bayer di `PUB-RAW-01`; il file X-Trans di `PUB-RAW-01`; un FITS con estensione `.cr2` e un RAW con estensione `.fits` | Ruolo, motivo ed evidenza uguali alla tabella scritta a mano; nome ed estensione senza effetto (il FITS va a `io.import`, il RAW a `io.raw_import`); dark mai `dark_flat`; RAW `unclassified` `raw_no_type_metadata`; X-Trans `import_stopped` con `raw.xtrans`, gli altri rami completati |
| T-M3A-05 | b | Correzione dell'utente su tre input in una chiamata; guasto dello store (`fault_injector`) sulla seconda riga; `UPDATE` e `DELETE` in SQL diretto; chiusura e riapertura; revisioni con input sconosciuto, ruolo non valido, dichiarazioni malformate; undo e redo dopo una correzione | Revisioni 2 `user` con la storia intatta; col guasto nessuna riga scritta; trigger: abort; dopo la riapertura storia identica; `classify.unknown_input`, `classify.role`, `classify.declaration`; undo e redo lasciano invariate le revisioni |
| T-M3A-05 | c | Ruolo cambiato dall'utente (dark → flat; light → `unclassified`); correzione in conflitto con `IMAGETYP` riconosciuto (master e light); `IMAGETYP` di dark con ruolo `dark_flat` | Nuova analisi uguale all'oracle; input spostato d'insieme; conflitto: revisione registrata, candidato `excluded` `imagetyp_conflict`, light in gruppo `light_blocked` `calib.imagetyp_conflict`; `dark_flat` accettato |
| T-M3A-05 | d | Input prestacked (mono e RGB) e RGB in fase `raw` classificati `light`, poi come master | `light_blocked` con `op.phase_blocked` (prestacked) e `calib.plane_layout` (RGB `raw`), nessuna ricetta; come master: `excluded` con `phase` o `layout` |
| T-M3A-06 | a | `verify_selection` su una selezione con light RAW: ogni campo di `decoder_state` diverso uno alla volta, `decoder_identity` diversa (versione, SHA-256) con stato uguale; controllo con tutto uguale; revisione di classificazione e di dichiarazioni (binning diverso, valore in conflitto con l'header) dopo la selezione, con l'osservazione costruita da `make_observation` sulle dichiarazioni congelate | `integrity` `calib.decoder_state_changed` con campo, `predicate = P-LOST`, `prerequisite`; controllo e revisioni: osservazione uguale a quella della selezione, nessun effetto; nessuna uscita diversa da stop o nessun effetto (firma) |
| T-M3A-06 | b | Tre rami con `run_branches` e un job del test che legge gli ingressi dal confine e chiama `verify_selection`: ramo 1, un byte del light cambiato dopo la selezione; ramo 2, stato del decoder cambiato con il `fault_injector` alla nuova decodifica; ramo 3, invariato. Poi chiusura e riapertura del progetto | Ramo 1 `integrity` `input.digest_mismatch`; ramo 2 `calib.decoder_state_changed`; ramo 3 completato; nessun nodo o ramo sostitutivo. Durante l'esecuzione cambiano solo: record di job e provenance dei tre rami, generazione degli slot dei rami avviati, artefatti e slot pubblicati del ramo 3; input, revisioni, nodi, archi e snapshot invariati. Stato logico subito prima della chiusura uguale a quello dopo la riapertura; proposta ricalcolata con le stesse scelte identica a quella prima della selezione |

Gate di F-IO-03 in M3.a: T-M3A-04, 05, 06, con U6 e U7 chiusi (G-CAL). Le parti di §9 assegnate a M3.b e M3.c si chiudono con i loro test e non riaprono F-IO-03.

## 13. Unità di codice

Stima, non misurata: circa 550 righe di prodotto, oltre il tetto di ~400 per review di `plans/M3a.md`. Due unità in sequenza, con contratto unico:

- **U8a**: CL-01…CL-04 e CL-03 lato store (`propose_classification`, `import_batch`, tabella e metodi dello store);
- **U8b**: CL-05…CL-12 (`make_observation`, `analyze_batch`, `group_lights`, `choose_candidates`, estensione di `pair_compatibility`, `verify_selection`).

U8b richiede U6 e U7 chiusi; U8a richiede U4 (`io.raw_import`).

## 14. Ipotesi e limiti dichiarati

- H-IT1 (spazi finali di `IMAGETYP` rimossi dal lettore): ipotesi, provata da T-M3A-05a. Se smentita, nessun ruolo è inventato.
- I RAW non hanno una proposta di ruolo: l'utente li classifica tutti. Una regola nuova, per esempio un tag DNG, richiede una revisione di questo contratto e un `rule_id` nuovo.
- Una correzione dell'utente non scavalca un `IMAGETYP` riconosciuto in conflitto (ADR-16.a): un file con `IMAGETYP` sbagliato non è utilizzabile in quel ruolo. Un'eccezione dichiarata richiede l'emendamento di ADR-16.a.
- Un input ha un solo ruolo per revisione. Lo stesso file non è candidato in due ruoli; due registrazioni dello stesso file sono un solo input se concordi, altrimenti `duplicate_digest_conflict` (CL-05), coerente con `shared_input` di ADR-16.a.
- Undo e redo non coprono la classificazione (§3).
- La scelta dei candidati non si persiste prima della selezione: se il gruppo cambia, per esempio con un light aggiunto e un riferimento diverso, la scelta si ripete.
- In M3.a i candidati sono solo master esterni e slot `current`. Per un light RAW nessun master è utilizzabile fino a M3.b (master da RAW con `frame_decoder_states`).
- `verify_selection` confronta solo stato e identità del decoder. Metadati FITS diversi a byte verificati uguali, cioè con il lettore cambiato fra selezione ed esecuzione, non sono rilevati da questa funzione: la chiave del nodo porta le versioni dell'operatore (ADR-05), non quella del lettore.
- L'export (F-PRJ-07) non porta la tabella `input_classifications`: i ruoli usati stanno nei parametri dei nodi esportati.
- Stato del codice letto al commit `311fb7a` con `grep` e lettura di `calibration_internal.hpp`, `calibration.cpp`, `sqlite_project.cpp`, `sqlite_dag_store.cpp` e `scheduler.hpp`: nessuna build, nessun test eseguito in questa unità.
