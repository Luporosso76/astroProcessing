# Contratto ADR-17.a: varianza minima della calibrazione

Stadio: ADR-17.a, modello minimo (registro riga 3424). Piano §8.5 (righe 973-980), C-01 campo 6. Unità: M2.e, ricetta di [ADR-16](ADR-16-stati-master.md). Piano di varianza e bit di invalidità in [ADR-03](ADR-03-pixel-maschere.md). Errori: [modello comune](ADR-05-dag-provenance.md#modello-derrore-comune).

## Modello

`astro::covariance_model`: in M2 un solo valore, `diagonal_independent`. Una varianza per pixel, in unità del campione al quadrato (ADU² per `C`, adimensionale per `Q`). Nessuna covarianza fra pixel rappresentata.

`variance_plane`: stessa geometria e `sample_type` dell'immagine. Invarianti: pixel valido con varianza finita e ≥ 0; pixel invalido con varianza NaN (T-M2E-24).

## Sorgente della varianza degli input

Ogni input dichiara `variance_source` nella ricetta. Nessun default.

| `variance_source` | Contenuto | Condizione |
|---|---|---|
| `plane` | HDU `VARIANCE` del file | Geometria uguale all'immagine; altrimenti `invalid_input`, `var.plane_geometry`. |
| `noise_model` | `conversion_gain_e_per_adu` > 0, `read_noise_adu` ≥ 0, `offset_adu` | Per un master anche `combine_count` ≥ 1 e `combine_method = mean`. |

`conversion_gain_e_per_adu` è un dato distinto da `gain` di ADR-03, che è l'impostazione della camera: non si deriva mai da quello. Si dichiara nella ricetta o si legge da `EGAIN`, con la sorgente registrata.

Varianza da `noise_model`, in f64:

```
Var(X) = (read_noise_adu² + max(X − offset_adu, 0) / conversion_gain_e_per_adu) / n
```

con `n = combine_count` per un master e `n = 1` per un light. Lo stimatore del termine di Poisson usa il valore osservato; sotto l'offset il conteggio atteso è zero e resta il solo rumore di lettura (T-M2E-25). Il report lo registra come `shot_estimator = observed_clamped`.

| Condizione | Categoria | Codice |
|---|---|---|
| `variance_source` assente, o parametro del modello assente | `metadata_missing` | `var.source.<campo>` |
| Master con `combine_method` diverso da `mean` e `noise_model` | `unsupported` | `var.combine_method` |
| Parametro non finito o fuori dominio | `invalid_input` | `var.param.<nome>` |

## Propagazione

Ogni termine in f64, un solo arrotondamento finale.

```
Var(Lc) = Var(L) + Var(D)
Var(Fc) = Var(F) + Var(DF)
Var(Q)  = Var(Fc) / N²
Var(C)  = Var(Lc) / Q² + Lc² · Var(Q) / Q⁴ − 2 · Lc · Cov(Lc, Q) / Q³
```

con `N` del canale del pixel (ADR-16) e `Cov(Lc, Q) = 0`.

Motivo dell'indipendenza: `L`, `D`, `F`, `DF` sono esposizioni distinte. Il prodotto lo verifica solo sul digest: due input uguali producono `unsupported`, `calib.shared_input`, perché la covarianza non nulla non è modellata in M2 (T-M2E-22). Che i master non condividano frame sorgente è un'ipotesi dichiarata nel report, non una verifica.

`N` è trattata come costante. Il report quantifica l'approssimazione con una stima per canale, non un bound:

```
normalization_rel_var_estimate = (π/2) · median(Var(Fc)) / (n_valid · N²)
```

dove `n_valid` è il numero di pixel validi per `N` definito in ADR-16 §6. La formula è la varianza asintotica della mediana di `n_valid` valori i.i.d. gaussiani. Con vignettatura o gradienti i valori di `Fc` non sono identicamente distribuiti e la stima non è garantita: il report la marca con l'ipotesi `iid_gaussian_fc` (T-M2E-26).

Validità al primo ordine: con `Q > 0` finito, `sqrt(Var(Q))/Q > q_rel_sigma_max` rende il pixel invalido con bit `q_unreliable` (ADR-16, §7). `Var(C)` non finita su un pixel altrimenti valido accende `result_nonfinite`.

## Provenance

Il `calibration_report` contiene: `covariance_model`, `variance_source` e parametri per ogni input con la loro sorgente, `shot_estimator`, ipotesi di indipendenza, `n_valid` e stima per canale con la sua ipotesi, digest dei master condivisi fra più light (`shared_master_digests`), che correlano i light calibrati fra loro.

## Test

| ID | Clausola |
|---|---|
| T-M2E-20 | Propagazione su input fissati contro un oracle razionale. Tutti i termini sono non negativi, quindi senza cancellazione: tolleranza `γ_m · Var(C)`, con `m` contato prima della misura sommando le operazioni di ogni termine e gli esponenti delle grandezze calcolate (`Lc`, `Q`) che vi compaiono; per uscita f32 si aggiunge mezzo ulp f32. |
| T-M2E-21 | Monte Carlo con seed, rumore gaussiano di varianza nota, regime `Var(Q)/Q² ≤ 10⁻⁴`: varianza empirica di `C` coerente con quella propagata. Soglia dal chi-quadro con `K − 1` gradi di libertà, α complessivo 10⁻³ con Bonferroni sui pixel, fissata prima dell'esecuzione. |
| T-M2E-22 | Due input con lo stesso digest: stop. |
| T-M2E-23 | Sorgente o parametri assenti: stop `var.source.<campo>`; parametri non finiti o fuori dominio: stop `var.param.<nome>`; piano `VARIANCE` di geometria diversa: stop `var.plane_geometry`; `median` con `noise_model`: stop. |
| T-M2E-24 | Invarianti del piano di varianza. |
| T-M2E-25 | Valore sotto l'offset: varianza uguale al rumore di lettura su `n`. |
| T-M2E-26 | Report: modello, sorgenti, `n_valid`, stima su `N` con ipotesi, master condivisi. Monte Carlo con seed su flat uniforme, `n_valid ≥ 10⁴`, `K` realizzazioni: rapporto fra varianza empirica di `N` e stima dentro l'intervallo chi-quadro a `K − 1` gradi di libertà con α = 10⁻³, allargato di ±2 % per l'approssimazione asintotica; soglie fissate prima della misura. Su flat vignettato il rapporto si misura e si registra come limite, senza gate.; il report marca `iid_assumption = not_verified` quando il coefficiente di variazione spaziale di `Fc` sui pixel validi supera 0,01, soglia dichiarata prima della misura. |

## Aggiunte di M2.e

- `offset_adu` è richiesto dalla compatibilità di tutti gli input: la sua assenza si ferma prima con `metadata_missing`, `calib.metadata.offset_adu`, mai con `var.source.offset_adu`.
- Un HDU `VARIANCE` di geometria diversa letto da file è respinto dal lettore con `fits.extension_geometry`; `var.plane_geometry` vale per immagini in memoria il cui piano ha dimensione diversa, controllata prima di `validate_image`.
- Un campione NaN in un input non genera `q_nonfinite` (ADR-16).
- Rappresentabilità, nel calcolo di `C`, `Q` e `Var`: la stessa regola di C-02 §6.3 e di `calibration.cpp` (`mul_ok`, `div_ok`, `add_ok`, `convert_finite`).
  - Un prodotto o un quoziente con operandi non nulli e risultato zero o subnormale (`fpclassify`) dà il pixel `result_nonfinite`. Lo stesso vale per una somma di termini non negativi non tutti nulli con quel risultato.
  - Le differenze con segno (`L − D`, `F − DF`, `x − offset_adu`) non si controllano: uno zero o un subnormale è il loro risultato esatto (lemma di Sterbenz, underflow graduale).
  - Ogni risultato non finito dà `result_nonfinite`, salvo `Q`: un `Q` non finito calcolato da input validi dà `q_nonfinite`, non `result_nonfinite` (ADR-16 §7, `calibration.cpp` riga 904).
  - Nella conversione al tipo di uscita, un valore non nullo che diventa zero o subnormale dà `result_nonfinite`; uno zero esatto resta valido.
  - Un `rn²` con `rn` non nullo e risultato zero, subnormale o non finito è `invalid_input`, `var.param.read_noise_adu`; con `rn = 0` il quadrato nullo è valido. Una stima di `N` non normale è `numeric`, `calib.variance_overflow`.

## Aggiunte di R2: varianza dei master del prodotto

Applicazione della «Estensione richiesta ad ADR-17» di [C-02.a](C-02-master.md#estensione-richiesta-ad-adr-17).

1. **Sorgente.** Un master del prodotto ha `variance_source = plane` con `combine_method = mean` e `variance_estimator = empirical` nel descrittore derivato dal nodo ([ADR-16](ADR-16-stati-master.md#aggiunte-di-r2-descrittore-derivato-dal-nodo-del-master)). `noise_model` resta per i master esterni con `combine_method = mean`.
2. **Propagazione.** Invariata: `Var(Fc) = Var(F) + Var(DF)` con `Cov(F, DF) = 0`, `Var(Lc) = Var(L) + Var(D)`. Il master flat non contiene il dark-flat (C-02.a §6.2).
3. **Errori ereditati.** Per ogni ingresso con `variance_estimator = empirical` il report di calibrazione aggiunge alle ipotesi `empirical_master_variance_is_estimate:<slot>`: il piano è una stima casuale, non una garanzia per pixel. Per il flat aggiunge `master_flat_variance_level_bias:flat` (sovrastima in valore atteso `B(p)`, limitata da `B_max(p)` sotto le ipotesi di C-02.a §6.2) e `master_flat_hypotheses_not_verified:same_illumination_shape,level_proportional,additive_nonnegative,nonnegative_signal`. I valori di `level_bias_estimate` restano nel report del master, citato dal digest del suo artefatto.
4. **Test.** Soglie fissate prima della misura.
   - **T-M2E-20 sui master del prodotto**: master dark, flat e dark-flat combinati dal prodotto da frame non dyadic; oracle razionale della propagazione sui piani `VARIANCE` dei master decodificati e sui loro campioni. Stessa tolleranza di T-M2E-20 (`γ_m · Var(C)` con `m` contato sulle operazioni, più mezzo ulp f32).
   - **T-M2E-21 sui master del prodotto**, due parti, `α = 10⁻³` complessivo con Bonferroni sulle `m` statistiche delle due parti, quantili chi-quadro esatti come C-02.a §9 (gamma incompleta regolarizzata, bisezione, errore relativo ≤ 10⁻¹⁰ sul quantile):
     - (a) valori di `C`: `K` realizzazioni indipendenti di light e frame di tutti i master, rumore gaussiano noto, regime `Var(Q)/Q² ≤ 10⁻⁴`. Riferimento: la varianza vera di `C` al primo ordine, calcolata dalle varianze vere dei master della simulazione (`σ²/n`), non dai piani empirici. `(K − 1) var_K(C)/Var(C)` nell'intervallo bilaterale di `χ²_(K−1)` al livello `α/m`. I valori di `C` dipendono solo dalle medie dei master: la variabilità dei piani empirici non entra in questa statistica.
     - (b) varianza riportata di `C`: fixture con flat e dark-flat senza rumore e `Fc` costante, quindi `Q = 1` esatto e `Var(Q) = 0`, light con piano nullo. Allora `Var(C)` riportata è il piano del master dark senza arrotondamenti, `(n − 1) n P_D/σ_D² ~ χ²_(n−1)` per realizzazione e `K(n − 1) · media_K(Var(C))/(σ_D²/n) ~ χ²_(K(n−1))`: la soglia è l'intervallo esatto della distribuzione del piano empirico, nessuna approssimazione.
   - La media della `Var(C)` riportata con piani empirici rumorosi anche nel flat non ha un gate: la sua distribuzione è quella di un rapporto di variabili casuali, non derivata in `.a`. È un limite dichiarato; la parte (b) e T-M2R2-04 coprono il piano e la sua propagazione.

## Fuori da M2

NPS locale, kernel di correlazione e fattore di apertura (ADR-17.b, M3); correlazioni dei master usate nello stacking; incertezza della geometria; covarianza fra canali dopo debayer; pesi dello stacking; varianza di master combinati con mediana senza piano dichiarato.
