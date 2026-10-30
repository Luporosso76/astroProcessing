# Contratto ADR-16.a: stati dei master e calibrazione minima

Stadio: ADR-16.a, ledger minimo (registro riga 3421). Contiene anche la ricetta M2 che il registro assegna ad ADR-09.a (riga 3406) e la forma minima di C-01 (§10.2). Piano §8.3, §8.4, §9.2, §9.4. Unità: M2.e. Varianza in [ADR-17](ADR-17-varianza.md). Tipi immagine in [ADR-03](ADR-03-pixel-maschere.md). Errori: [modello comune](ADR-05-dag-provenance.md#modello-derrore-comune).

## Stati tipizzati

Attributi combinabili, non un enum unico (§9.2). Lo stato si dichiara nella ricetta (`state_source = declared`); il nome del file non conta mai.

| Tipo | Valori |
|---|---|
| `master_role` | `bias`, `dark`, `dark_flat`, `flat` |
| `additive_state` | `bias_included`, `bias_subtracted`, `unknown` |
| `scaling_state` | `unscaled`, `scaled`, `unknown` |
| `flat_state` | `raw`, `corrected_unnormalized`, `calibrated`, `unknown` |
| `combine_method` | `mean`, `median`, `other` |

`master_descriptor`: `role`, `additive` (`std::optional`, solo dark e dark_flat), `scaling` (idem), `flat` (`std::optional`, solo flat), `combine_count` e `combine_method` (`std::optional`), `digest`, `metadata`.

Stati accettati in M2:

| Ruolo | Stato richiesto |
|---|---|
| `dark` | `bias_included`, `unscaled` |
| `dark_flat` | `bias_included`, `unscaled` |
| `flat` | `raw` |

- Uno stato `unknown`: `metadata_missing`, `calib.master_state_unknown`. Mai trattato come `raw` (T-M2E-10).
- Un altro stato noto: `unsupported`, `calib.master_state_m2`; appartiene alla matrice di M3.c.
- Master nello slot di un altro ruolo, o un `bias` passato a questa ricetta: `invalid_input`, `calib.role_slot`.
- Un dark ripreso all'esposizione del flat si usa come `dark_flat` solo se la ricetta gli assegna quel ruolo. Il prodotto non lo deduce mai da esposizione, `IMAGETYP` o nome; la compatibilità F–DF si verifica come per ogni dark-flat (T-M2E-18).
- `IMAGETYP` riconosciuto (senza distinzione di maiuscole: `light`, `light frame`, `dark`, `dark frame`, `flat`, `flat field`, `flat frame`, `bias`, `bias frame`, `dark flat`, `flat dark`) che indica un altro ruolo (un `IMAGETYP` di dark è compatibile col ruolo `dark_flat` dichiarato): `incompatible`, `calib.imagetyp_conflict`. Valore non riconosciuto: registrato con avviso.

## Ricetta M2: `dark_matched_flat_dark_flat`

### 1. Problema

Modello `L = B + t_L d + q S + eps`. Dark matched `D = B + t_L d`. Flat `F = B_F + t_F d + q K`, dark-flat matched `DF = B_F + t_F d`. Stima di `S`:

```
Lc = L − D
Fc = F − DF
Q  = Fc / N(Fc)
C  = Lc / Q
```

`C` stima `S` a meno della scala globale `N/K`. Nessun `B` si sottrae: è già in `D` e in `DF`.

### 2. Dominio e prerequisiti

Un piano, `plane_layout` `mono` o `cfa` (altrimenti `unsupported`, `calib.plane_layout`), `linear`, unità `adu` per tutti gli input. Compatibilità verificata nel preflight, prima di ogni calcolo:

| Coppia | Uguaglianza esatta | Tolleranza dichiarata |
|---|---|---|
| L–D, F–DF | larghezza, altezza, binning, CFA (pattern e offset), `camera`, `gain`, `offset_adu` | `exposure_s` entro `exposure_rel_tol`; `temperature_c` entro `temperature_abs_tol_c` |
| L–F | larghezza, altezza, binning, CFA, `camera`, `filter` | — |
| L–D, F–DF, L–F | `roi_x`, `roi_y`, `readout_mode`, `row_order`, se presenti in entrambi | — |

- Campo richiesto assente in un input: `metadata_missing`, `calib.metadata.<campo>`. Si può dichiarare nella ricetta (ADR-03, `metadata_source = declared`).
- Differenza oltre la regola: `incompatible`, `calib.mismatch.<campo>`, con valori e tolleranza nel contesto.
- `roi_x`, `roi_y`, `readout_mode`, `row_order` assenti in uno o in entrambi gli input della coppia: non verificati; il report li elenca fra le ipotesi, con l'input che li omette (T-M2E-19).
- `temperature_policy` ∈ {`compare`, `not_applicable_declared`}; il secondo salta solo il confronto di temperatura e genera un avviso nel ledger.
- Un'incompatibilità non apre un'altra ricetta: M2 ha questa sola (T-M2E-08).
- Due input con lo stesso digest: `unsupported`, `calib.shared_input` (ADR-17).
- `pedestal_adu` presente e diverso da zero: `unsupported`, `calib.pedestal`.

Parametri obbligatori, senza default: `normalization` ∈ {`global_median`, `cfa_channel_median`}, `q_min` > 0, `q_rel_sigma_max` > 0, `exposure_rel_tol` ≥ 0, `temperature_abs_tol_c` ≥ 0, `temperature_policy`. Valore assente o non finito: `invalid_input`, `calib.param.<nome>`.

### 3. Ipotesi e limiti

Termine additivo stabile fra light e dark; flat moltiplicativo; nessuno scaling del dark; amp glow non modellato. Il report le dichiara come ipotesi, non come verifiche.

### 6. Numerica

Ogni pixel in f64 a partire dai campioni; un solo arrotondamento finale al `sample_type` del light. Tolleranza contro un oracle indipendente in aritmetica razionale, derivata prima della misura (T-M2E-01, T-M2E-02):

- casi dyadic: zero;
- fondamento: l'oracle parte dai campioni in memoria, che sono valori esatti, e ogni operazione IEEE è arrotondata correttamente rispetto al proprio risultato esatto, quindi la cancellazione in `L − D` e `F − DF` non amplifica errori. Sul percorso `Lc`, `Fc`, `N` (media dei centrali), `Q`, `C` vale `|C_f64 − C| ≤ γ_6 · |C|`, con `γ_m = m·u/(1 − m·u)` e `u = 2^-53`;
- uscita f64: `γ_6 · |C|`, con `C` dell'oracle: sei arrotondamenti relativi con `N` a conteggio pari (`Lc`, `Fc`, i due `Fc` centrali, la somma della media, `Q`, `C`), cinque con conteggio dispari; si usa sempre `γ_6`;
- uscita f32: mezzo ulp f32 dell'arrotondamento finale più `γ_6 · |C|`, quindi al più 1 ulp f32.

Normalizzazione `N`: mediana di `Fc` sui pixel validi per `N`, cioè quelli senza bit di input in `F` e `DF` (`source_*`, `master_invalid`, `saturated`) e con `Fc` finito. I bit di `Q` (4–7) dipendono da `N` e non entrano in questa selezione; i valori `Fc ≤ 0` sì. `n_valid` è il loro numero. Globale, oppure per ciascuno dei quattro canali CFA, con G1 e G2 separati. Conteggio pari: media in f64 dei due centrali. `cfa_channel_median` su un'immagine mono: `invalid_input`, `calib.normalization_requires_cfa`. Nessun pixel valido nel canale, `N` ≤ 0 o non finita: `numeric`, `calib.normalization_invalid`.

### 7. Maschere

Pixel di uscita invalido, con il bit di ADR-03, se:

- un input è invalido sullo stesso pixel (`source_*`, `master_invalid`);
- `L` o `F` ≥ `saturation_adu` del rispettivo input, se dichiarato (`saturated`); se non dichiarato, avviso `saturation_not_evaluated` nel ledger;
- `Q` non finito: solo `q_nonfinite`; altrimenti `Q ≤ 0`: solo `q_nonpositive`. Per questi due casi `q_unreliable` non si valuta, perché l'incertezza relativa non è definita;
- con `Q > 0` finito, indipendenti: `Q < q_min` accende `q_below_min`; `sqrt(Var(Q))/Q > q_rel_sigma_max` accende `q_unreliable`. Una `Var(Q)` non finita, o non rappresentabile in modo normale, calcolata da input finiti è un overflow o un underflow numerico e dà `result_nonfinite` (`calibration.cpp` riga 913: `add_ok`, `div_ok`), non `q_unreliable`;
- `C` non finito con input validi (`result_nonfinite`).

Un `Q` valido maggiore di 1 resta valido (T-M2E-05). Mai epsilon.

### 8. Determinismo

`N` si calcola su tutta l'immagine prima di dividere in blocchi. Output bit-identico con 1, 2 e 4 worker e con ogni `tile_rows` (T-M2E-16).

### 10. Failure mode e provenance

Prerequisito perso dopo il preflight (digest cambiato, file assente): stop del ramo, `integrity` o `io` (ADR-05), nessuna correzione diversa (T-M2E-11).

`correction_ledger`: una voce per `dark_subtraction`, `dark_flat_subtraction`, `flat_division`, `bias_subtraction`, con `status` ∈ {`applied`, `omitted`, `not_applicable`}, digest del master e motivo. In questa ricetta i primi tre sono `applied`; `bias_subtraction` è `not_applicable`, motivo `bias_in_matched_dark`.

`calibration_report`: JSON canonico più `report.txt` generato dagli stessi dati. Contiene ricetta e parametri, input con digest e stati, ogni confronto di compatibilità con valori e tolleranze, ledger, `N` per canale con conteggio dei pixel validi, conteggi per bit di invalidità, modello di varianza, ipotesi, avvisi, digest degli output. Si scrive col protocollo atomico di ADR-04.

## Test

Fixture `FIX-SYN-FITS-01` con `B`, `d`, `q`, `S`, `K` noti.

| ID | Clausola |
|---|---|
| T-M2E-01 | Valori dyadic senza rumore: `L`, `D`, `F`, `DF` interi, `Fc` e `N` potenze di due, quindi `Q` potenza di due e anche le divisioni sono esatte: `C = S·N/K` esatto. |
| T-M2E-02 | Valori non dyadic, con forte cancellazione `L ≈ D`, con `N` a conteggio pari e dispari: entro `γ_6 · |C|` (f64) o 1 ulp (f32) dall'oracle razionale. |
| T-M2E-03 | Bias nel dark: nessuna doppia sottrazione; bias residuo nullo. |
| T-M2E-04 | `Q` nullo, negativo, NaN: solo il bit relativo, mai `q_unreliable`; sotto `q_min` e inaffidabile, separati e insieme: bit corretti; campione e varianza NaN. |
| T-M2E-05 | `Q > 1` valido invariato. |
| T-M2E-06 | Normalizzazione globale e per canale; G1 ≠ G2 nella fixture. |
| T-M2E-07 | `N` non valida: stop `numeric`; `cfa_channel_median` su mono: stop `calib.normalization_requires_cfa`; pixel con bit di input esclusi da `N`, `Fc ≤ 0` inclusi; il report dichiara per canale quanti `Fc ≤ 0` sono entrati in `N`. |
| T-M2E-08 | Ogni campo della tabella di compatibilità fuori regola: stop prima del calcolo, nessun output. |
| T-M2E-09 | Metadato assente: stop; dichiarato in ricetta: accettato e registrato; in conflitto con l'header: stop. `temperature_policy = not_applicable_declared`: confronto saltato, avviso nel ledger. |
| T-M2E-10 | Stati `unknown` e non M2: stop; ruolo nello slot sbagliato; `IMAGETYP` in conflitto. |
| T-M2E-11 | Master modificato dopo il preflight: stop del ramo. |
| T-M2E-12 | Report e ledger completi e coerenti con l'output. |
| T-M2E-13 | Dato pubblico reale (light, dark, flat, dark-flat) con licenza verificata: esecuzione e report. |
| T-M2E-14 | Maschere degli input propagate per unione. |
| T-M2E-15 | Saturazione dichiarata mascherata; non dichiarata segnalata. |
| T-M2E-16 | Determinismo rispetto a worker e `tile_rows`. |
| T-M2E-17 | Input `rgb` o `multi_plane`: stop `unsupported` prima del calcolo. |
| T-M2E-18 | Dark all'esposizione del flat: accettato come `dark_flat` solo se dichiarato; non dichiarato non riempie lo slot `dark_flat`. |
| T-M2E-19 | `roi_x`, `roi_y`, `readout_mode`, `row_order` diversi: stop; assenti: elencati fra le ipotesi del report. |
| T-M2E-27 | Parametri assenti, non finiti o fuori dominio: stop `calib.param.<nome>`; `pedestal_adu` ≠ 0: stop `calib.pedestal`. |

## Aggiunte di M2.e

- Campione NaN o ±Inf in un input: il pixel è invalido per input (`source_nonfinite`, più `master_invalid` se l'input è un master). `q_nonfinite` è riservato al `Q` calcolato da input validi (per esempio overflow di `Fc`); `Q` non si valuta su un pixel con input invalido.
- Descrittore di master assente: `metadata_missing`, `calib.master_state_unknown`. Descrittore presente sul light: `invalid_input`, `calib.role_slot`.
- Maschera di uscita: unione dei bit degli input più `master_invalid` (vedi ADR-03, T-M2E-14).
- File riletto con dimensione o geometria diverse da quelle del piano di prenotazione: `integrity`, `input.size_changed` / `input.geometry_changed`.
- Intermedi di varianza non finiti: `N²` nullo o non finito, o stima di `N` non finita, ferma con `numeric`, `calib.variance_overflow`; a livello di pixel un intermedio non finito o azzerato per underflow (`Q²`, `Q⁴`, `Lc²`, termini) accende `result_nonfinite`. Nessuna riscalatura.
- Il report riporta valore e origine (`header`/`declared`) di `binning_x`, `binning_y`, `unit`, `linearity` per ogni input (`image_fields`).
- Rappresentabilità, nel calcolo di `C`, `Q` e `Var`: la stessa regola di C-02 §6.3 e di `calibration.cpp` (`mul_ok`, `div_ok`, `add_ok`, `convert_finite`).
  - Un prodotto o un quoziente con operandi non nulli e risultato zero o subnormale (`fpclassify`) dà il pixel `result_nonfinite`. Lo stesso vale per una somma di termini non negativi non tutti nulli con quel risultato.
  - Le differenze con segno (`L − D`, `F − DF`, `x − offset_adu`) non si controllano: uno zero o un subnormale è il loro risultato esatto (lemma di Sterbenz, underflow graduale).
  - Ogni risultato non finito dà `result_nonfinite`, salvo `Q`: un `Q` non finito calcolato da input validi dà `q_nonfinite`, non `result_nonfinite` (ADR-16 §7, `calibration.cpp` riga 904).
  - Nella conversione al tipo di uscita, un valore non nullo che diventa zero o subnormale dà `result_nonfinite`; uno zero esatto resta valido.
  - Un `rn²` con `rn` non nullo e risultato zero, subnormale o non finito è `invalid_input`, `var.param.read_noise_adu`; con `rn = 0` il quadrato nullo è valido. Una stima di `N` non normale è `numeric`, `calib.variance_overflow`.

## Aggiunte di R2: descrittore derivato dal nodo del master

Unità R2 di `plans/M2-residui.md`. Il master del prodotto è il nodo `master.combine` di [C-02.a](C-02-master.md).

- **Descrittore derivato.** `master_descriptor_from_node` ricava il descrittore dal solo nodo, senza leggere file: ruolo dai parametri; `dark` e `dark_flat` `bias_included`, `unscaled` (frame raw, nessuna sottrazione, nessuna scala); `flat` `raw`; `combine_count` = numero di ingressi frame del nodo; `combine_method = mean`, `rejection = none`, `variance_estimator = empirical`. Un nodo con un altro operatore: `invalid_input`, `master.not_master_node`. Il descrittore dichiarato resta per i master esterni; i campi `rejection` e `variance_estimator` sono opzionali ed entrano nei parametri del nodo di calibrazione solo se presenti.
- **Ingresso della calibrazione.** `product_master_input` costruisce l'ingresso dallo slot del master: slot non `current` o senza artefatto: `invalid_input`, `calib.product_master_not_current`, con lo stato nel contesto. Percorso e digest sono quelli dell'artefatto `primary` pubblicato, `variance_source = plane`, e l'ingresso del nodo di calibrazione è il nodo del master (`from_node`), non il file: la calibrazione è discendente del master nel DAG (ADR-05, `descendants`). Un master con un nodo diverso lascia lo slot del master senza artefatto corrente: l'ingresso si ricostruisce, con un nuovo nodo di calibrazione.
- **Ricetta.** Un ingresso con nodo del master deve avere `variance_source = plane` e un descrittore con `combine_method = mean`, `rejection = none`, `variance_estimator = empirical`; altrimenti `invalid_input`, `calib.product_master`, con il campo nel contesto.
- **Job.** Prima di leggere i file, per ogni ingresso con nodo del master: nodo assente dallo store: `graph`, `dag.missing_node`; descrittore della ricetta diverso da quello derivato dal nodo: `invalid_input`, `calib.descriptor_mismatch`; file dell'ingresso diverso (SHA-256 e dimensione) da ogni artefatto presente di ruolo `primary` del nodo: `integrity`, `calib.master_artifact_mismatch`.
- **Report.** `master_state` riporta anche `rejection`, `variance_estimator` e `master_node_key`, quando presenti; le ipotesi sui piani empirici sono in [ADR-17](ADR-17-varianza.md#aggiunte-di-r2-varianza-dei-master-del-prodotto).
- **§7, `Var(Q)` non rappresentabile.** Il calcolo non ha un ramo `q_unreliable` per una `Var(Q)` non finita: `add_ok` e `div_ok` la marcano prima `result_nonfinite`, e un ramo successivo sarebbe irraggiungibile. Lo verifica `CalibM2E.T_M2E_04_var_q_overflow_is_result_nonfinite_not_q_unreliable`, con overflow del quoziente `Var(Fc)/N²` su un solo pixel e somma `Var(F) + Var(DF)` subnormale. L'overflow della somma su un pixel valido per `N` non arriva al pixel: lo ferma prima il preflight della stima di `N` (`calib.variance_overflow`, T-M2E-29).
- **Riesecuzioni sui master del prodotto.** Sono pertinenti i test che dipendono dall'origine del master: T-M2E-12 e T-M2E-16 (report e determinismo sulla pipeline frame → master → light calibrato), T-M2E-11 (artefatto del master diverso dal file letto), T-M2E-13 (T-M2R2-09), T-M2E-20 e T-M2E-21 ([ADR-17](ADR-17-varianza.md#aggiunte-di-r2-varianza-dei-master-del-prodotto)). Gli altri T-M2E verificano la calibrazione a master dato e non dipendono dalla sua origine: si rieseguono invariati.

## Fuori da M2

Matrice delle otto combinazioni, omissioni pianificate, `L − B − D'`, dark scalato, solo bias, passthrough, `F − B`, dark-flat `bias_subtracted`, flat `calibrated` o `corrected_unnormalized` (ADR-16.b, M3.c); debayer; RAW e offset del decoder (M3.a); stato dichiarato da header dedicati.

## Estensione ADR-16.b: matrice completa della calibrazione (M3)

Stadio: ADR-16.b, matrice completa (registro riga 3422; aggregato ADR-16 riga 3420, testo righe 2577 e 2600). Decisione collegata: [D-16](../decisions/D-16-calibrazione-incompleta.md) (riga 3468, testo riga 2882). Archi: ADR-16.b `requires_all` ADR-16.a e `decided_by` D-16 (righe 4103, 4104); C-01.b e G-CAL `decided_by` ADR-16.b (righe 4111, 4371); G-CAL `requires_all` F-IO-04 (riga 4370); F-CAL-02.b e F-CAL-03.b `requires_when` G-CAL, G-OFFSET, G-STOP con P-DFB000…111, P-DF-*, P-FLAT-*, P-OFFSET, P-LOST (righe 4381-4397); F-ANA-04.a `requires_when` G-STOP con P-LOST (riga 4411). Predicati: righe 3571-3601. Piano: §9.1–§9.9 (righe 1200-1326), §10.2 C-01 campi 2, 4, 6, 10 (righe 1355-1369), F-CAL-02 e F-CAL-03 (righe 1841, 1842), §11.11 riga 1980, T-CAL-08 (righe 2344, 3548). Dipendenze fra contratti: [ADR-03.b](ADR-03-pixel-maschere.md) §b.2 (`decoder_state`), [ADR-08.b](ADR-08-decoder-pack.md) §2.4 e §2.9 (nero dichiarato, ipotesi H3, record del decoder), [ADR-17](ADR-17-varianza.md) (varianza), [C-02](C-02-master.md) (master). Unità che lo implementano: M3.a (G-CAL nella classificazione F-IO-03, compatibilità del decoder), M3.b (descrittore dei master `.b` con lo stato del decoder dei frame), M3.c (ricette, P-LOST, T-CAL-08).

Ogni clausola è una **regola** (vincolo del prodotto, verificato dal test indicato), un **fatto** (con la fonte) o un'**ipotesi** (dichiarata, con il test o l'unità che la prova). La parte `.a` sopra resta valida; i punti che cambiano sono in §b.11.

Fuori da ADR-16.b: dark scalato `L − B − α(D − B)` (F-CAL-05, P1, M6.f); scelta fra più master candidati dello stesso ruolo (F-IO-03, ADR-15); sottrazione del nero del decoder come sorgente d'offset del light (C-01.b, G-OFFSET); regione di normalizzazione del flat (F-CAL-03.b); bad pixel map (F-CAL-04); modelli di covarianza, compreso il master usato in due catene (ADR-17.b); generazione dei master `bias_subtracted` (C-02.b).

### b.1 Analisi, selezione, esecuzione

Regola (§9.1 riga 1206, §9.5, D.5 riga 3571). La calibrazione ha tre fasi distinte, e solo la prima può escludere un master.

1. **Analisi (G-CAL).** Ingressi: il light, al più un candidato per ruolo (`bias`, `dark`, `dark_flat`, `flat`), le dichiarazioni della ricetta. L'analisi legge header, descrittori e stato del decoder; legge i campioni solo per il controllo termico del flat (§b.5). Uscita, funzione pura degli ingressi: per ogni candidato e per ogni catena di §b.5 che lo usa, `usable` oppure `excluded` con un motivo della lista di §b.3 (un candidato è `usable` se lo è in almeno una catena); i predicati di §b.4; l'insieme delle ricette ammissibili (§b.5); la ricetta di default (D-16, §b.6). Un master escluso non produce uno stop: l'esclusione è un esito dell'analisi.
2. **Selezione.** La ricetta scelta (default o scelta esperta) diventa il nodo di calibrazione: catene, ingressi, parametri, dichiarazioni e stato del decoder osservato nell'analisi entrano nei parametri e quindi nella chiave (ADR-05). Una ricetta fuori dall'insieme ammissibile non diventa un nodo: stop prima del calcolo (G-STOP, §b.9).
3. **Esecuzione.** Il preflight riesegue sul nodo gli stessi controlli dell'analisi. Ogni differenza è P-LOST (§b.8): stop del ramo, nessuna correzione diversa, nessuna omissione attivata.

La ricetta è sempre esplicita: il default è una proposta dell'analisi, scritta per intero nel nodo come una scelta esperta. Il report registra se la ricetta scelta coincide con il default. Le ricette proposte dall'Advisor (F-ANA-04.a) seguono le stesse tre fasi.

### b.2 Stati accettati in M3

| Ruolo | Stato `usable` | Stato `excluded` (motivo) |
|---|---|---|
| `dark` | `bias_included`, `unscaled` (`D`); `bias_subtracted`, `unscaled` (`D'`) | `scaled` (`state_scaled`, F-CAL-05); `unknown` (`state_unknown`) |
| `dark_flat` | `bias_included`, `unscaled` (P-DF-INC); `bias_subtracted`, `unscaled` (P-DF-SUB) | `scaled`, `unknown`: P-DF-BAD (`state_scaled`, `state_unknown`) |
| `flat` | `raw` (P-FLAT-RAW); `corrected_unnormalized`; `calibrated` con `flat_normalization_declared` (P-FLAT-Q) | `unknown` (P-FLAT-UNKNOWN, `state_unknown`); `calibrated` senza normalizzazione dichiarata (`state_incomplete`) |
| `bias` | descrittore con `role = bias` e `additive`, `scaling`, `flat` assenti | descrittore con uno di quei campi (`state_incomplete`) |

- `flat_normalization_declared` ∈ {`global_median`, `cfa_channel_median`} è un campo nuovo e opzionale del descrittore, ammesso solo per `flat` `calibrated`: la normalizzazione con cui il flat è stato prodotto. Il prodotto la registra, calcola `N(Q)` con quella modalità e la riporta, senza confrontarla con 1: una scala globale di `Q` cambia solo la scala di `C` (§1).
- `corrected_unnormalized` non ha un predicato nel registro. Regola: flat con termine additivo già rimosso secondo il descrittore; il prodotto lo normalizza (`Q = F / N(F)`) senza correzioni additive. Come P-FLAT-Q, rende il dark-flat non applicabile.
- Unità richiesta: `adu` per light, bias, dark, dark-flat e flat `raw` o `corrected_unnormalized`; `dimensionless` per il flat `calibrated`. Master di unità diversa: `excluded` (`unit`).
- Fase di processo (ADR-03, «Aggiunte di R1»), sempre con linearità `linear`: `raw` per light, bias, `D`, `DF` e flat `raw`; `raw` o `calibrated` per `D'`, `DF'` e per i flat `corrected_unnormalized` e `calibrated`, che sono master già preparati. Master `prestacked`, o con una fase fuori da questa regola: `excluded` (`phase`). Questa regola sostituisce, per la calibrazione `.b`, il requisito «quattro ingressi `raw`» di ADR-03 (riga della tabella `calibrate`).
- Il light non è un candidato e non si esclude: fase, linearità, unità o layout fuori da queste regole fermano l'analisi, con i codici di [C-01.b](C-01-calibrazione-light.md) §2 (`op.phase_blocked` e `op.linearity_blocked` di ADR-03.b §b.14, `calib.unit`, `calib.plane_layout`).
- Ipotesi H-D': un `D'` o un `DF'` contiene solo il termine termico. Il residuo di bias, se `D'` è stato prodotto con un bias diverso da quello usato nella ricetta, non è verificato. Il report la dichiara su ogni ricetta che usa `D'` o `DF'`.

### b.3 Compatibilità prima della scelta

Le coppie si confrontano con le regole di §2 (`.a`), estese:

| Coppia | Regola |
|---|---|
| L–D, L–D', F–DF, F–DF' | come L–D e F–DF della parte `.a` (esposizione entro `exposure_rel_tol`, temperatura secondo `temperature_policy`) |
| L–B, F–B | come L–D senza il confronto di esposizione |
| L–F | come la parte `.a` |
| ogni coppia con almeno un ingresso RAW | §b.7, che sostituisce il confronto di `offset_adu` |

Motivi di esclusione (lista chiusa, nel report con valori e tolleranze): `state_unknown`, `state_scaled`, `state_incomplete`, `unit`, `phase`, `layout`, `linearity`, `metadata_missing.<campo>`, `incompatible.<campo>`, `imagetyp_conflict`, `shared_input`, `decoder_state_mixed`, `decoder_state_mismatch.<campo>`, `decoder_black`, `flat_thermal_budget`, `coverage` (coverage presente, [C-01.b](C-01-calibrazione-light.md) §7). La compatibilità è per coppia, quindi l'esclusione è per catena: un bias incompatibile con F ma compatibile con L resta `usable` nelle catene del light (`light.bias`, `light.bias_dark_sub`) ed è `excluded` in quelle del flat; `flat_thermal_budget` esclude la sola catena `flat.bias`, non il flat. Una condizione che nella parte `.a` ferma (`metadata_missing`, `incompatible`) nell'analisi esclude il candidato dalle catene che usano quella coppia; nel preflight del nodo ferma l'esecuzione come in `.a`. Il dark-flat si confronta col flat candidato, anche se escluso; senza flat candidato P-DF si calcola dal solo stato del dark-flat.

### b.4 Predicati

| Predicato | Regola di calcolo nell'analisi |
|---|---|
| P-DFBxyz | `x` = 1 se il dark (`D` o `D'`) è `usable`; `y` = 1 se il flat è `usable`; `z` = 1 se il bias è `usable` (in almeno una catena). Esattamente uno degli otto è vero |
| P-DF-ABS / INC / SUB / BAD | dark-flat assente / `usable` con `bias_included` / `usable` con `bias_subtracted` / presente ed `excluded` |
| P-FLAT-RAW, P-FLAT-Q | predicati della ricetta selezionata, non dell'analisi (D.5 riga 3587): veri se la ricetta contiene una catena del flat su flat `raw` (`flat.dark_flat`, `flat.bias_dark_flat_sub`, `flat.bias`) o `flat.given_q`. Con `flat.none` sono falsi anche se il flat è `usable`. L'analisi riporta a parte lo stato del flat disponibile |
| P-FLAT-UNKNOWN | flat di stato `unknown`: `excluded` nell'analisi; selezionato in una ricetta, G-STOP (§b.9) |
| P-OFFSET | la ricetta contiene una divisione per `Q` (catena flat diversa da `flat.none`): G-OFFSET richiesto (§b.5) |
| P-LOST | prerequisito della ricetta selezionata assente o cambiato nell'esecuzione (§b.8) |

### b.5 Catene e ricette ammissibili

Una ricetta è una coppia (catena del light, catena del flat). Modello di §1: `L = B + t_L d + q S + eps`, `D = B + t_L d`, `D' = t_L d`, `F = B_F + t_F d + q K`, `DF = B_F + t_F d`, `DF' = t_F d`.

Catene del light:

| Catena | Formula | Ingressi | Offset del light dopo la catena |
|---|---|---|---|
| `light.none` | `Lc = L` | — | `known_not_removed` se il light ha `decoder_state`, altrimenti `unknown` |
| `light.bias` | `Lc = L − B` | B | `removed` |
| `light.dark` | `Lc = L − D` | D | `removed` |
| `light.bias_dark_sub` | `Lc = (L − B) − D'` | B, D' | `removed` |
| `light.dark_sub_partial` | `Lc = L − D'` | D' | come `light.none` |

Catene del flat:

| Catena | Formula | Ingressi e condizioni |
|---|---|---|
| `flat.none` | nessuna divisione | — |
| `flat.dark_flat` | `Fc = F − DF`, `Q = Fc / N(Fc)` | flat `raw`, P-DF-INC |
| `flat.bias_dark_flat_sub` | `Fc = (F − B) − DF'`, `Q = Fc / N(Fc)` | flat `raw`, P-DF-SUB, bias `usable` |
| `flat.bias` | `Fc = F − B`, `Q = Fc / N(Fc)` | flat `raw`, bias `usable`, controllo termico dichiarato superato |
| `flat.normalize` | `Q = F / N(F)` | flat `corrected_unnormalized` |
| `flat.given_q` | `Q = F` | flat `calibrated` |

In ogni catena con divisione `C = Lc / Q`, con le maschere di `.a` §7 su `Q`.

Regole:

- **Nessuna doppia sottrazione.** Ogni catena sottrae l'offset del proprio ingresso al più una volta: `B` non si combina con `D` né con `DF` (`bias_included`), e le catene `L − B − D` e `F − B − DF` non esistono. Fatto, per costruzione delle tabelle: il coefficiente di `B` (o `B_F`) nel risultato è 0 per le catene con offset `removed` e 1 per le altre (T-M3A-CAL-03).
- **Controllo termico di `flat.bias`.** `F − B` lascia `t_F d` nel flat. La ricetta dichiara `flat_thermal_bound_adu` ≥ 0 (limite superiore di `t_F d` per pixel, ipotesi dell'utente) e `flat_thermal_rel_budget` in (0, 1). Per ogni canale di normalizzazione `c`, con `N_c` (valore f64) da `Fc = F − B`, il prodotto confronta in aritmetica esatta sui valori f64 (prodotto senza arrotondamento, per esempio con una trasformazione esente da errore) `flat_thermal_bound_adu > flat_thermal_rel_budget · q_min · N_c`: vero, `flat.bias` `excluded` (`flat_thermal_budget`). Sia `β_c = flat_thermal_bound_adu / (q_min · N_c)`, razionale esatto, e `β⁺_c = β_c (1 + u)²`; anche `β⁺_c ≥ 1`, confrontato in aritmetica esatta, esclude `flat.bias` (`flat_thermal_budget`), perché il limite che segue vale solo per `β⁺_c < 1`. Fatto: un pixel valido ha `fl(Fc/N_c) ≥ q_min` (altrimenti `q_below_min`), quindi `Fc ≥ q_min · N_c / (1 + u)`, e `Fc = fl(F − B)` dista al più `u` relativo da `F − B`; sotto l'ipotesi `0 ≤ t_F d ≤ bound` la perturbazione relativa di `F − B` è al più `β⁺_c` e quella di `Q` al più `max(q_min · β⁺_c, β⁺_c / (1 − β⁺_c))`, perché la mediana è crescente nei valori e si sposta al più di `bound`. Esempio di uguaglianza: `Fc = [3, 3, 3]`, `q_min = 1`, bound 1, termico `[1, 0, 0]`: perturbazione di `Q` sul primo pixel `1/2 = β/(1 − β)` con `β = 1/3`. Senza le due dichiarazioni `flat.bias` non è ammissibile. Il report riporta `β_c`, `β⁺_c` e il limite su `Q` come ipotesi dichiarata.
- **G-OFFSET (P-OFFSET).** Una catena flat diversa da `flat.none` è ammissibile solo se l'offset del light dopo la catena è `removed`, oppure `known_not_removed` con la dichiarazione esplicita `flat_division_offset = known_not_removed` nella ricetta (percorso parziale, §9.3 riga 1238). Nel percorso parziale il report propaga il residuo `b_L(r, c) / Q` (nero del decoder secondo H3 di ADR-08.b §2.4) come limite e marca l'uscita come non fotometrica. Con offset `unknown` la divisione non è ammissibile. `flat_division_offset` è obbligatorio quando l'offset è `known_not_removed` e la catena flat divide, vietato negli altri casi (`calib.param_not_applicable.flat_division_offset`).
- **Insieme ammissibile.** Ogni coppia di catene i cui ingressi sono `usable` e che rispetta le regole sopra. Comprende le ricette che omettono una correzione disponibile (omissione scelta, motivo `user_omitted`).
- Parametri di una catena non selezionata (per esempio `normalization`, `q_min`, `q_rel_sigma_max` con `flat.none`; `exposure_rel_tol` senza dark né dark-flat) devono essere assenti: `invalid_input`, `calib.param_not_applicable.<nome>`. Così due ricette uguali hanno la stessa chiave.
- Fase dell'uscita: `calibrated` per ogni ricetta, compresa `light.none` + `flat.none`. La fase dice che l'immagine è uscita dal nodo di calibrazione; che cosa è stato corretto lo dice il ledger (§b.10).

### b.6 Matrice delle otto combinazioni e default

Regola del default (D-16): la ricetta di default contiene, per ciascuna delle due catene, la correzione di priorità più alta fra quelle ammissibili con i soli fatti verificati dall'analisi e le dichiarazioni già presenti nel progetto. Non inventa una dichiarazione e non sceglie mai il percorso parziale della divisione. Priorità del light: `light.dark` > `light.bias_dark_sub` > `light.dark_sub_partial` > `light.bias` > `light.none` (`L − D'` toglie il termico e lascia l'offset come `light.none`: nessun termine peggiora). Priorità del flat, solo se l'offset del light è `removed`: `flat.given_q` o `flat.normalize` (secondo lo stato) > `flat.dark_flat` > `flat.bias_dark_flat_sub` > `flat.bias` > `flat.none`. Ogni correzione non applicata è una voce `omitted` del ledger con motivo.

Nella tabella il bias conta come presente in una catena solo se è `usable` in quella catena (§b.3): se è `usable` solo nelle catene del light, le celle del flat si leggono come senza bias; se è `usable` solo in quelle del flat, le celle del light si leggono come senza bias. Esempi: 111 con `D'` e bias `usable` solo col flat dà `light.dark_sub_partial` e, con offset non rimosso, `flat.none`; 011 con bias `usable` solo col light dà `light.bias` e per il flat solo le catene senza bias (`flat.given_q`, `flat.normalize`, `flat.dark_flat`; altrimenti `flat.none`); 011 con bias `usable` solo col flat dà `light.none` e `flat.none` (`light_offset_not_removed`). La tabella è la regola del default applicata all'insieme ammissibile, non una regola separata.

| DFB | Light di default | Flat di default (per P-DF e stato del flat) | Alternative esperte notevoli | Avvisi del default |
|---|---|---|---|---|
| 111 | `D`: `light.dark`, bias `not_applicable` (`bias_in_matched_dark`); `D'`: `light.bias_dark_sub` | `calibrated`: `given_q`; `corrected_unnormalized`: `normalize`; `raw` con INC: `dark_flat`; con SUB: `bias_dark_flat_sub`; con ABS o BAD: `flat.bias` se dichiarato e nel budget, altrimenti `flat.none` (`flat_thermal_not_declared` o `flat_thermal_budget`) | `light.bias` (dark omesso); `flat.bias` invece di `dark_flat` | omissioni del flat |
| 110 | `D`: `light.dark`; `D'`: `light.dark_sub_partial` | con `D`: `given_q`, `normalize`, `dark_flat` come in 111; `raw` con SUB, ABS o BAD: `flat.none` (`dark_flat_sub_requires_bias`, `flat_offset_not_removable`). Con `D'`: `flat.none` (`light_offset_not_removed`) | con `D'` e light RAW: divisione con `flat_division_offset = known_not_removed` | offset residuo con `D'`; omissioni del flat |
| 101 | come 111 | `flat.none` (`master_absent` o `master_excluded`) | `light.bias` | nessuna correzione flat: vignettatura e polvere restano |
| 100 | `D`: `light.dark`; `D'`: `light.dark_sub_partial` | `flat.none` | `light.none` | nessuna correzione flat; offset residuo con `D'` |
| 011 | `light.bias` | come 111 con bias presente | `light.none` | termico del light non compensato |
| 010 | `light.none` | `flat.none` (`light_offset_not_removed`) | light RAW: catena flat ammissibile con `known_not_removed` (`raw` solo con INC) | nessuna correzione additiva; flat omesso |
| 001 | `light.bias` | `flat.none` | `light.none` | termico e difetti di campo restano |
| 000 | `light.none` (passaggio) | `flat.none` | — | nessuna correzione; qualità e limiti dichiarati |

Livelli (D-16): `info` per le voci `not_applicable`; `warning` per ogni voce `omitted`, per ogni percorso parziale e per ogni ipotesi dichiarata (H-D', limite termico); gli errori sono stop, mai avvisi. Un flat `unknown`, un dark `scaled` o un master incompatibile compaiono nell'analisi come `excluded` con motivo, e la riga DFB è quella calcolata senza di loro.

### b.7 Stato del decoder nei master e compatibilità con i light RAW

Decisione (vincolo di ADR-03.b §b.2: `decoder_state` non si scrive in FITS):

- **Dove sta.** Un master costruito da frame RAW porta lo stato del decoder dei suoi frame nei parametri del nodo `master.combine`: `frame_decoder_states`, un elemento per ingresso frame, nello stesso ordine canonico degli ingressi, con i campi di `decoder_state` di ADR-03.b §b.2 tranne `source`. Il valore viene dalla decodifica dell'analisi, quindi entra nella chiave del nodo del master e, attraverso l'ingresso `from_node`, in quella della calibrazione. L'immagine del master non porta `decoder_state` (non è un'uscita del decoder) e si scrive in FITS come in C-02.a.
- **Descrittore.** `master_descriptor_from_node` aggiunge al descrittore `frame_decoder_states`, letti dai parametri del nodo, senza leggere file. Invarianti: presente per tutti i frame o assente; lunghezza uguale al numero di ingressi frame; ogni elemento rispetta gli invarianti di ADR-03.b §b.2 che non dipendono dall'immagine. Violazione: `invalid_input`, `master.decoder_state_param`. Il descrittore dichiarato di un master esterno non ha `frame_decoder_states`.
- **Job del master.** Frame RAW e frame FITS nello stesso master: `incompatible`, `master.decoder_state_mixed`, prima del calcolo. Stato ottenuto decodificando di nuovo un frame diverso da quello nei parametri del nodo: `integrity`, `master.decoder_state_changed` (P-LOST del job del master).
- **Compatibilità con il light.** Ingressi della ricetta tutti con stato del decoder (light RAW, master da RAW) o tutti senza. Un light RAW con un master esterno o da FITS, o un light FITS con un master da RAW: `decoder_state_mixed` (escluso; nel preflight `incompatible`, `calib.decoder_state_mixed`). Per ogni coppia di §b.3 e per ogni elemento di `frame_decoder_states` del master:
  - campi strutturali uguali: `raw_width`, `raw_height`, `window`, `cfa_color_index`, `black_pattern_rows`, `black_pattern_cols`, `linearization`, `curve_sha256`. Differenza: `decoder_state_mismatch.<campo>` (nel preflight `incompatible`, `calib.decoder_state_mismatch.<campo>`);
  - nero effettivo, solo per le coppie additive (L–D, L–D', L–B, F–DF, F–DF', F–B): `b(r, c) = black + black_per_color[FC(r, c)] + P(r, c)` secondo H3, in aritmetica intera a 64 bit (esatta: somma di tre `uint32_t`), su un periodo `lcm(2, rows) × lcm(2, cols)` limitato all'area visibile (righe e colonne del motivo nulle valgono 1). `max |b_X − b_Y| > decoder_black_abs_tol_adu`: `decoder_black` (nel preflight `incompatible`, `calib.decoder_black`, con differenza massima, posizione e tolleranza nel contesto);
  - `white`, `data_maximum`, `linear_max` non si confrontano: ogni ingresso usa il proprio `saturation_adu` (ADR-08.b §2.6).
- **Parametro.** `decoder_black_abs_tol_adu` ≥ 0, intero, senza default: obbligatorio se un ingresso ha stato del decoder, vietato altrimenti (`calib.param.decoder_black_abs_tol_adu`, `calib.param_not_applicable.decoder_black_abs_tol_adu`). Con stato del decoder il confronto di `offset_adu` di `.a` non si applica: il valore non è un metadato del decoder (ADR-08.b §2.6).
- **Mai una correzione automatica.** Un'incompatibilità del decoder esclude il master nell'analisi o ferma il preflight; il prodotto non riallinea finestre, non converte indici di colore, non compensa la differenza di nero.
- **Identità del decoder.** Ogni elemento di `frame_decoder_states`, e lo stato del light nel nodo di calibrazione, porta anche `decoder_identity`: versione di LibRaw (`LibRaw::version()`) e SHA-256 di `libraw_r.so` caricata, i campi «Libreria» del record di ADR-08.b §2.9. L'identità non si confronta fra light e master (versioni diverse con stato uguale sono compatibili); si confronta solo fra analisi ed esecuzione dello stesso ingresso.
- **Light.** Il nodo di calibrazione porta nei parametri stato e identità del decoder del light osservati nell'analisi; nel preflight una decodifica con stato o identità diversi dà `integrity`, `calib.decoder_state_changed`, con il campo diverso nel contesto (P-LOST). Lo stesso vale per i frame nel job del master (`master.decoder_state_changed`). Il light calibrato non porta `decoder_state` (ADR-03.b §b.2); lo stato consumato e il nero effettivo del light vanno nella provenance e nella voce `light_offset` del ledger.
- **Ipotesi.** Che una differenza del nero dichiarato indichi una differenza dell'offset fisico, e che nero uguale indichi offset uguale, è un'ipotesi: LibRaw non distingue nero letto dal file e nero da tabella (ADR-08.b §2.4). La regola usa il nero dichiarato come sentinella, non come misura.

### b.8 P-LOST

Regola (§9.1 riga 1206, §9.5, D.5 riga 3601, archi 4397 e 4411). Dopo la selezione, ogni prerequisito della ricetta assente o diverso da quello del nodo arresta il ramo:

| Prerequisito | Stop |
|---|---|
| File di un ingresso assente o illeggibile | `io` (ADR-05) |
| Digest o dimensione diversi | `integrity` (ADR-05, `input.size_changed`, `input.geometry_changed`) |
| Master del prodotto: slot non `current`, nodo assente, artefatto diverso | `calib.product_master_not_current`, `dag.missing_node`, `calib.master_artifact_mismatch` (R2) |
| Descrittore derivato diverso da quello della ricetta | `calib.descriptor_mismatch` (R2) |
| Stato o identità del decoder diversi alla nuova decodifica | `calib.decoder_state_changed`, `master.decoder_state_changed` |
| Decodifica RAW fallita | codice di ADR-08.b §2.5 |

Ogni stop di questa tabella porta nel contesto `predicate = P-LOST` e `prerequisite = <ingresso>.<oggetto>`. Effetti, verificati da T-M3A-CAL-05: nessuna uscita; nessun altro nodo creato o eseguito; nessuna voce del ledger convertita in `omitted`; il report d'errore riporta intatta la ricetta selezionata. Una nuova ricetta dopo lo stop è un nuovo nodo creato dall'utente, non un resume (§9.7 riga 1300). Resume: quello di ADR-06 e ADR-04 (R3, R4), con ricetta identica; difforme, `resume_mismatch`. Uno stop numerico o di budget è G-STOP ma non P-LOST.

### b.9 Selezione non ammissibile (G-STOP)

Una ricetta fuori dall'insieme di §b.5 si ferma prima del calcolo, senza nodo:

| Caso | Categoria | Codice |
|---|---|---|
| Flat `unknown` selezionato (P-FLAT-UNKNOWN), o un altro master `unknown` | `metadata_missing` | `calib.master_state_unknown` |
| Dark o dark-flat `scaled` selezionato | `unsupported` | `calib.master_state_scaled` |
| Catena che richiede un master `excluded` o assente, catena incoerente con lo stato, doppia sottrazione, divisione con offset `unknown`, `flat.bias` senza dichiarazioni o fuori budget | `invalid_input` | `calib.recipe_not_admissible`, con il motivo di §b.3 o di §b.10 nel contesto |

### b.10 Ledger, report e varianza

- Voci del ledger: `dark_subtraction`, `bias_subtraction`, `dark_flat_subtraction`, `flat_bias_subtraction`, `flat_normalization`, `flat_division`, con `status` ∈ {`applied`, `omitted`, `not_applicable`}, digest del master e motivo. Motivi (lista chiusa): `master_absent`, `master_excluded` (con il motivo di §b.3), `user_omitted`, `light_offset_not_removed`, `dark_flat_sub_requires_bias`, `flat_offset_not_removable`, `flat_thermal_not_declared`, `flat_thermal_budget`, `bias_in_matched_dark`, `flat_already_corrected`, `double_offset`.
- Voce `light_offset`: stato (`removed`, `known_not_removed`, `unknown`), master che lo rimuove, nero effettivo del light per posizione del periodo se il light è RAW.
- Report: esito dell'analisi (candidati con stato e motivo, predicati, insieme ammissibile, default), ricetta scelta e se coincide con il default, ledger, ipotesi (H-D', limite termico con `β_c`, nero come sentinella, H3), avvisi con livello, residuo del percorso parziale. Il resto come `.a` §10.
- Varianza: ogni termine sottratto aggiunge la sua varianza a `Var(Lc)` o a `Var(Fc)` (ADR-17.a, ingressi distinti); `flat.given_q` prende `Var(Q)` dal piano `VARIANCE` del flat (`variance_source = noise_model` non si applica a un'immagine adimensionale: `invalid_input`, `var.q_noise_model`). Un master usato in due catene (`B` in `light.bias*` e `flat.bias*`) e la covarianza light/flat sono di ADR-17.b: le ricette con `B` in entrambe le catene non hanno un modello di varianza finché ADR-17.b non lo fissa, e il codice di M3.c che le esegue parte dopo ADR-17.b.

### b.11 Modifiche ad ADR-16.a

- Slot `bias` aggiunto; gli slot non usati dalla ricetta restano vuoti invece di essere obbligatori.
- «Un altro stato noto: `calib.master_state_m2`» è sostituito dalla tabella di §b.2 e dalle ricette di §b.5.
- Compatibilità estesa da §b.3 e §b.7; `offset_adu` non si confronta fra ingressi con stato del decoder.
- Fase degli ingressi secondo §b.2 al posto di «tutti `raw`» (ADR-03, tabella delle operazioni, riga `calibrate`).
- `combine_method`, `rejection`, `variance_estimator` del master del prodotto: valori di C-02.b; la regola R2 «`mean`, `none`, `empirical`» resta per la ricetta `.a`.

### b.12 Numerica

Regola: ogni catena in f64 dai campioni, da sinistra a destra nell'ordine scritto in §b.5, un solo arrotondamento finale al `sample_type` del light. Tolleranze contro un oracle razionale, fissate qui prima di ogni misura. Notazione e lemmi: Higham, *Accuracy and Stability of Numerical Algorithms*, 2ª ed., SIAM 2002, cap. 2-3 (`γ_m = m·u/(1 − m·u)`, `u = 2^-53`, `(1 + θ_j)(1 + θ_k)^{±1} = 1 + θ_{j+k}`).

- **Casi esatti**: zero per ogni catena quando ogni intermedio è esatto in f64: campioni interi, ogni differenza parziale (`L − B`, `(L − B) − D'`, `F − B`, …) intera di modulo minore di `2^53`, `Fc`, `N` e `Q` potenze di due. Con due sottrazioni l'esattezza della differenza parziale è una condizione della fixture, non una proprietà della catena (`fl(2^54 − 1) = 2^54`); fuori da queste condizioni vale il bound generale.
- **Catene con al più una sottrazione** in `Lc` e in `Fc`. Come `.a` §6 (i centrali di `N` sono arrotondamenti corretti dei centrali esatti, perché `fl` è monotona): `|C_f64 − C| ≤ γ_m |C|`, con `k_L` ∈ {0, 1} le sottrazioni del light e `m` = `k_L` (`flat.none`), `k_L + 1` (`flat.given_q`), `k_L + 3` (`flat.normalize`), `k_L + 5` (`flat.dark_flat`, `flat.bias`; `.a` è il caso `k_L = 1`). Ipotesi, come in `.a`: con `N` a conteggio pari i due centrali hanno lo stesso segno; altrimenti vale il bound generale.
- **Bound generale** (obbligatorio con `light.bias_dark_sub` o `flat.bias_dark_flat_sub`). Per una catena `x = (a − b) − c` valutata come scritto, `|x̂ − x| ≤ E = u|x| + u(1 + u)|a − b|` (due arrotondamenti); con una sottrazione `E = u|x|`, senza sottrazioni `E = 0`. Con `E_x` del light, `E_F,i` del flat sul pixel, `E_N` = massimo di `E_F,j` sui pixel validi per `N` (le statistiche d'ordine sono 1-lipschitziane in norma infinito), e `E_F,i < |Fc_i|`, `E_N < N`:
  `ρ_i = (1 + E_N / N)(1 + γ_3)/(1 − E_F,i / |Fc_i|) − 1`, `|C_f64 − C| ≤ ρ_i |C| + (1 + ρ_i) E_x / |Q|`.
  `γ_3` copre la somma dei centrali, `Q` e `C`. Con `flat.given_q`: `ρ = γ_1`, `E_F = E_N = 0`; con `flat.none`: `|C_f64 − C| ≤ E_x`. Derivazione: `C_f64 = (x + ε_x)(N/Fc_i)·R` con `|R − 1| ≤ ρ_i`, quindi `C_f64 − C = C(R − 1) + ε_x R / Q`.
- **Uscita f32**: mezzo ulp f32 del valore arrotondato più il bound f64.

### b.13 Test pianificati

Tolleranze e criteri fissati qui. Oracle scritti nel test, indipendenti dall'implementazione: le tabelle di §b.5 e §b.6 sono ricodificate a mano nel test, e la ricodifica è controllata da una tabella scritta a mano con almeno una configurazione per riga di §b.6 e per catena di §b.5.

| ID | Verifica | Criterio |
|---|---|---|
| T-M3A-CAL-01 | Analisi su tutte le configurazioni: dark ∈ {assente, `D`, `D'`, `scaled`, `unknown`, incompatibile}, bias ∈ {assente, valido, incompatibile con L e con F, compatibile solo con L, compatibile solo con F}, flat ∈ {assente, `raw`, `corrected_unnormalized`, `calibrated`, `calibrated` senza normalizzazione, `unknown`, incompatibile}, dark-flat ∈ {assente, INC, SUB, incompatibile, `unknown`}, light ∈ {FITS, RAW}, dichiarazioni termiche ∈ {assenti, nel budget, fuori budget}, con i casi «solo L» e «solo F» del bias incrociati con ogni riga di §b.6; inoltre, per ogni ruolo, ogni fase (`raw`, `calibrated`, `prestacked`) | Esatta: per ogni configurazione stato e motivo di ogni candidato per catena, P-DFB, P-DF, stato del flat disponibile, insieme ammissibile e default uguali all'oracle; P-FLAT-RAW e P-FLAT-Q valutati sulla ricetta di default e su una con `flat.none` e flat `usable` (falsi) |
| T-M3A-CAL-02 | Default: per ogni configurazione di 01, default ∈ insieme ammissibile; nessun default con `flat_division_offset`; `flat.bias` di default solo con dichiarazioni nel budget; confine del controllo termico con valori scelti perché `β_c` sia rappresentabile in f64: `flat_thermal_rel_budget = β_c` (ammesso) e uguale al predecessore f64 di `β_c` (escluso); un caso con `β_c` non rappresentabile (per esempio `1/3`) e budget uguale a `fl(β_c)` arrotondato per difetto (escluso); `β_c ≤ budget` con `β⁺_c ≥ 1` (per esempio `Fc = [1, 1, 1]`, `q_min = 1`, bound e budget `1 − u`: escluso). Perturbazione relativa di `Q` con termico iniettato (`t ∈ [0, bound]`, anche al massimo su un solo pixel) contro un oracle razionale, con `β⁺_c` vicino a 1 da sotto | Esatta; perturbazione dell'oracle ≤ `max(q_min · β⁺_c, β⁺_c / (1 − β⁺_c))` su ogni pixel valido |
| T-M3A-CAL-03 | Ogni catena di §b.5 su sintetici con `B`, `d`, `q`, `S`, `K` noti: casi esatti di §b.12; non dyadic con cancellazione forte; per le catene con due sottrazioni, differenza parziale non rappresentabile (`L = 2^54`, `B = 1`); `N` a conteggio pari e dispari; centrali di segno opposto | Casi esatti 0; altri entro il bound di §b.12; bias residuo esattamente 0 (offset `removed`) o `B/Q` (percorso parziale) nei casi esatti |
| T-M3A-CAL-04 | Ledger e report per una ricetta di default e una esperta di ogni riga di §b.6: voci, stati, motivi, `light_offset`, ipotesi, livelli | Binaria, campo per campo contro l'oracle |
| T-M3A-CAL-05 | P-LOST: ogni riga di §b.8 iniettata dopo la selezione, una alla volta, su una ricetta con flat e una con omissione | Stop con categoria e codice della tabella, `predicate = P-LOST`; 0 uscite, 0 nodi nuovi, 0 calcoli (contatore del `fault_injector`), nessuna voce convertita in `omitted` |
| T-M3A-CAL-06 | Ogni caso di §b.9: flat `unknown`, dark `scaled`, `L − B − D`, `F − B − DF`, divisione con offset `unknown`, `flat.bias` senza dichiarazioni, master escluso; parametri non applicabili presenti | Codici di §b.9 e `calib.param_not_applicable.<nome>`; nessun nodo |
| T-M3A-CAL-07 | Master da RAW: `frame_decoder_states` nei parametri e nel descrittore derivato, senza letture di file (contatore); un campo di uno stato cambiato alla volta: chiavi del master e della calibrazione diverse; frame RAW e FITS insieme; invarianti violati | Esatta; `master.decoder_state_mixed`; `master.decoder_state_param` |
| T-M3A-CAL-08 | Compatibilità del decoder: ogni campo strutturale diverso uno alla volta; nero effettivo con differenza uguale a `decoder_black_abs_tol_adu` (passa) e tolleranza + 1 (stop), con nero scalare, per colore e con motivo; light RAW con master esterno; light FITS con master da RAW; `white` diverso (passa) | `calib.decoder_state_mismatch.<campo>`, `calib.decoder_black`, `calib.decoder_state_mixed`; negli stop nessuna uscita e 0 calcoli |
| T-M3A-CAL-09 | Alla decodifica dell'esecuzione, per il light e per un frame del master, iniettati uno alla volta: un campo di `decoder_state` diverso (nero); `decoder_identity` diversa con stato uguale (versione, SHA-256 della libreria). Controllo: light e master con versioni diverse e stato uguale nell'analisi (compatibili) | `calib.decoder_state_changed`, `master.decoder_state_changed`, `predicate = P-LOST`, campo diverso nel contesto; controllo: nessuno stop |
| T-M3A-CAL-10 | Resume di una ricetta `.b` con checkpoint: identica; con una catena, un parametro o uno stato del decoder diverso | Identica: uscita bit-identica all'esecuzione senza interruzione; diversa: `resume_mismatch`, nessun task eseguito |

Test del piano che questi alimentano: T-CAL-08 (otto combinazioni e dark-flat, M3.s), con i dati pubblici reali quando acquisiti (§9.9 punto 10).

### b.14 Limiti dichiarati

- Il nero dichiarato dal decoder è una sentinella dell'offset, non una misura; la tolleranza la sceglie l'utente.
- Il limite termico di `flat.bias` e H-D' sono ipotesi dell'utente, non verificate dal prodotto.
- Master esterni e light RAW non si combinano: la calibrazione di un light RAW richiede master costruiti dal prodotto da frame RAW.
- Un candidato per ruolo: la scelta fra più master compatibili non è di questo contratto.
- `corrected_unnormalized` non ha un predicato nel registro (D.5); è trattato come flat preparato, con dark-flat non applicabile.
- Le ricette con `B` in due catene attendono il modello di ADR-17.b.
- Il bound generale di §b.12 è più largo di `γ_m` anche dove la catena del flat ha una sola sottrazione; non si stringe dopo la misura.
- Dimensione di `frame_decoder_states`: fino a 4098 valori del motivo per frame, stimata in qualche decina di kB per frame nel caso peggiore, non misurata.
