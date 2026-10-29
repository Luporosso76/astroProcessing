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

## Fuori da M2

Matrice delle otto combinazioni, omissioni pianificate, `L − B − D'`, dark scalato, solo bias, passthrough, `F − B`, dark-flat `bias_subtracted`, flat `calibrated` o `corrected_unnormalized` (ADR-16.b, M3.c); debayer; RAW e offset del decoder (M3.a); stato dichiarato da header dedicati.
