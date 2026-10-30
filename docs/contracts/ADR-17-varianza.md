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

## Estensione ADR-17.b: modelli di covarianza P0 (M3)

Stadio: ADR-17.b, modelli P0 completi (registro riga 3425; aggregato ADR-17 riga 3423, testo righe 2578 e 2602). Decisione collegata: [D-17.b](../decisions/D-17-covarianza.md) (riga 3471, testo riga 2883). Archi: ADR-17.b `requires_all` ADR-17.a e `decided_by` D-17.b (righe 4106, 4107); F-CAL-06.b e C-06 `decided_by` ADR-17.b (righe 3734, 4119). Piano: §8.5 (righe 976-988), §8.6 (990-1052), §8.7 (1054-1067), §8.8 (1069-1104), §8.12-§8.14 (1139-1188), §10.7 C-06 campi 1, 6, 9, 10 (righe 1443-1452), §10.10 C-09 campo 6 (riga 1496), §10.13 C-12 campo 6 (riga 1577), F-CAL-06 (riga 1845), F-REG-03 (riga 1855). Dipendenze fra contratti: [ADR-16.b](ADR-16-stati-master.md#estensione-adr-16b-matrice-completa-della-calibrazione-m3) §b.3, §b.5, §b.10, §b.12; [ADR-03.b](ADR-03-pixel-maschere.md) §b.3, §b.4, §b.13; [C-02.a](C-02-master.md) §6 e §9. Unità che lo implementano: M3.c (§b.2, §b.3, §b.9), M3.d (blocco fra canali, §b.4), M3.g (§b.5), M3.h (§b.6), M3.k (§b.7).

Ogni clausola è una **regola** (vincolo del prodotto, verificato dal test indicato), un **fatto** (con derivazione o fonte) o un'**ipotesi** (dichiarata, con il test o il limite che la riguarda). La parte `.a` resta valida.

Fuori da ADR-17.b: incertezza della geometria stimata dentro la covarianza fotometrica (§b.5, limite); pesi, rejection e loro scelta (C-07: qui solo la varianza a pesi fissati); drizzle e scelta del kernel (C-06); varianza dei master combinati con mediana (C-02.b); covarianza fra master con frame sorgente comuni (esclusa, §b.3 e §b.6); dark scalato (F-CAL-05, P1).

### b.1 Sorgenti, sensibilità e righe

**Sorgenti.** Regola: le variabili casuali del modello sono i campioni dei light e dei master che entrano nella calibrazione, con la varianza della loro `variance_source` (ADR-17.a). Un'immagine letta da file con `variance_scope = independent` e senza calibrazione nel progetto è una sorgente essa stessa.

Ipotesi H-IND: esposizioni distinte sono indipendenti; i pixel di una sorgente sono indipendenti fra loro; le immagini importate sono indipendenti fra loro e dalle sorgenti del progetto (`imported_independence_not_verified`). Il rumore correlato del sensore (bande di righe o colonne, diafonia) la viola e non è modellato. Per i master del prodotto la disgiunzione dei frame è verificata (§b.3).

**Sensibilità.** Fatto: un campione `y` prodotto dalla catena è funzione delle sorgenti. Al primo ordine `y ≈ y* + Σ_s ρ_{y,s} δs`, con `ρ_{y,s} = ∂y/∂s`, e

```
Cov(y, y') = Σ_s ρ_{y,s} · ρ_{y',s} · v_s
```

È la legge di propagazione per grandezze d'ingresso indipendenti (JCGM 100:2008 §5.1.2 eq. (10)) nella forma matriciale `C_y = J diag(v) Jᵀ` dei modelli multivariati (JCGM 102:2011, sezione 6); riferimenti non riletti in questa unità. È esatta quando `y` è lineare nelle sorgenti con pesi indipendenti dai dati; è un'approssimazione al primo ordine altrimenti. Le sensibilità si valutano sui valori osservati (plug-in), come in ADR-17.a.

**Righe.** Regola: un operatore che §b.8 ammette su ingressi correlati espone, per ogni campione d'uscita, la sua **riga**: le coppie (campione d'ingresso, `∂uscita/∂ingresso`), in ordine canonico, come funzione pura degli ingressi e dei parametri del nodo.

- Pesi fissati (debayer `linear_weights`, resampling con `R` fissata, media a pesi fissati): riga esatta.
- Pesi che dipendono dai dati (debayer edge-aware, rejection, pesi stimati): riga a pesi congelati, approssimazione dichiarata col codice `variance.fixed_weight_approximation` (ADR-03.b §b.13).
- Passo non lineare: Jacobiano (§b.7).

La riga di una catena è il prodotto delle righe dei suoi stadi (regola della catena). **Pullback** di un funzionale lineare `φ = Σ_p a_p y_p`: coefficienti `b_s = Σ_p a_p ρ_{p,s}`, propagati all'indietro stadio per stadio fino alle sorgenti; `Var(φ) = Σ_s b_s² v_s`.

### b.2 Calibrazione: covarianza light/flat e bias in due catene

Notazione di ADR-16.b §b.5: `C = Lc/Q`; `Q = Fc/N` per le catene su flat `raw` e per `flat.normalize` (dove `Fc` è `F`); `Q` dal flat per `flat.given_q`; `C = Lc` per `flat.none`. Regola: `N` è trattata come costante (ADR-17.a). Sensibilità di `C` sulle sorgenti dello stesso pixel:

| Sorgente | `ρ = ∂C/∂s` |
|---|---|
| `L` | `1/Q` |
| master solo nella catena del light (`B`, `D`, `D'`) | `−1/Q` |
| `F` (flat `raw` o `corrected_unnormalized`) | `−C/Fc` |
| master solo nella catena del flat (`B`, `DF`, `DF'`) | `+C/Fc` |
| `B` in entrambe le catene | `(C − N)/(N·Q)` |
| flat `calibrated` (`flat.given_q`) | `−C/Q` |

Fatto: `C = N·Lc/Fc` con `Lc` e `Fc` affini nelle sorgenti a coefficienti ±1, quindi `∂C/∂Lc = 1/Q` e `∂C/∂Fc = −C/Fc`. Per `B` in entrambe le catene la somma `−1/Q + C/Fc` vale `(C − N)/(N·Q)` perché `Fc = N·Q`. Nessun altro master può stare in entrambe le catene (ADR-16.b §b.5).

Regola, varianza del pixel come somma di termini non negativi, in f64, in quest'ordine:

```
S_L = Var(L) + Σ Var(M)     M solo nella catena del light, nell'ordine della formula della catena
S_F = Var(F) + Σ Var(M)     M solo nella catena del flat, idem
a = S_L / (Q·Q)
b = S_F · ((C/Fc)·(C/Fc))   con flat.given_q: b = Var(Q) · ((C/Q)·(C/Q))
c = Var(B) · (r·r),  r = (C − N)/(N·Q)    solo con B in entrambe le catene; altrimenti c = 0
Var(C) = (a + b) + c        con flat.none: Var(C) = S_L
```

Fatti:

- Espandendo `c` si ottiene `Var(Lc)/Q² + C²·Var(Q)/Q² − 2·C·Cov(Lc, Q)/Q²`, con `Var(Lc) = S_L + Var(B)`, `Var(Q) = (S_F + Var(B))/N²` e `Cov(Lc, Q) = Var(B)/N`. È la formula di ADR-17.a (piano §8.5) con la covarianza al posto dello zero. La forma a termini non negativi dà `Var(C) ≥ 0` senza cancellazione.
- Il termine di `B` si annulla per `C = N`: il rumore del bias sottratto da numeratore e denominatore si compensa. Trattare le due sottrazioni come indipendenti darebbe `(1/Q² + C²/Fc²)·Var(B)` al posto di `r²·Var(B)`.

Regole:

- Le ricette con `B` in entrambe le catene hanno questo modello e sono ammesse (T-M3C-VAR-01, 02).
- Per i bit di `Q` (ADR-16 §7: `q_unreliable`) `Var(Q) = Var(Fc)/N²`, con `Var(Fc) = S_F + Var(B)` solo quando `B` è in entrambe le catene; con `B` nella sola catena del flat `Var(B)` è già in `S_F` e `Var(Fc) = S_F`.
- L'uscita di ogni catena ha `variance_scope = independent`. Fatto: con `N` costante due pixel distinti non hanno sorgenti comuni.

**`N` costante.** Il report conserva la stima di ADR-17.a. Con `B` in entrambe le catene `N` dipende anche da `B(x)`, attraverso `Fc(x)`. Stima asintotica sotto `iid_gaussian_fc`: `Cov(Lc, Q)` cambia del fattore `1 − Q/n_valid`. La stima viene dalla rappresentazione di Bahadur della mediana (Bahadur 1966, *Ann. Math. Statist.* 37) e dal lemma di Stein per variabili gaussiane (Stein 1981, *Ann. Statist.* 9), non riletti. È omessa dal piano: è una stima, non un bound.

**Errore del primo ordine.** Fatto, identità algebrica per `C = N·Lc/Fc` con `N` fissata:

```
C − C_lin = −(δFc/Fc) · (C_lin − C*)
```

dove `C*` è il valore senza rumore, `C_lin = C* + Σ_s ρ_s(s*) δs` e `Fc = Fc* + δFc` il valore realizzato. Derivazione: si sostituisce `C_lin = N(Lc* + δLc)/Fc* − N·Lc*·δFc/Fc*²` nella differenza e si raccoglie `δFc`. Per `flat.given_q` vale la stessa identità con `Q` al posto di `Fc`. L'errore del primo ordine su un campione è quindi la frazione `|δFc/Fc|` della sua deviazione lineare. Con `sqrt(Var(Fc))/Fc = σ_r`, che la soglia `q_rel_sigma_max` di ADR-16 limita, e `δFc` gaussiano: `|δFc/Fc| ≤ 7σ_r/(1 − 7σ_r)` con probabilità ≥ 1 − 2.6·10⁻¹². È una stima, perché `σ_r` è plug-in.

### b.3 Master condivisi fra light

Fatto: due light `i` e `j` calibrati con almeno un master in comune sono correlati sullo stesso pixel:

```
Cov(C_i(x), C_j(x))  = Σ_{M comuni} ρ_{i,M}(x) · ρ_{j,M}(x) · Var(M(x))
Cov(C_i(x), C_j(x')) = 0   per x ≠ x'
```

Le sensibilità sono quelle di §b.2 per ciascun light, e differiscono fra light quando dipendono da `C_i`. Esempio esatto (`light.dark` + `flat.none`): la media di `K` light con lo stesso dark ha varianza `Σ_i Var(L_i)/K² + Var(D)`; il termine del dark non decresce con `K`. La scala comune di `N` correla tutti i light e i pixel che usano lo stesso flat come un fattore globale: resta la stima per canale di ADR-17.a, che non entra nei piani.

**Rappresentazione.** Regola: nessun piano e nessuna matrice in più. Il report e la provenance di ogni light calibrato portano `covariance_sources`: per ogni master usato, digest dell'artefatto, slot e catene (`light`, `flat`, `both`). Il consumatore che combina più light (§b.6) ricostruisce le sensibilità dai light calibrati, dai master e dalle ricette, che sono artefatti del DAG.

**Disgiunzione dei frame (H-IND).** Regola per i master del prodotto (descrittore derivato dal nodo, ADR-16 R2). L'analisi G-CAL confronta gli insiemi dei digest dei frame sorgente di slot diversi, e il digest del light con ognuno di essi. Un'intersezione non vuota fra due ingressi toglie dall'insieme ammissibile (ADR-16.b §b.5) le ricette che li usano entrambi, con il motivo `shared_input` (ADR-16.b §b.3). Nel preflight è uno stop `unsupported`, `calib.shared_input`, con i digest comuni nel contesto. `B` usato in due catene è un solo slot, non un'intersezione.

Per i master esterni gli insiemi non sono noti: il report dichiara `master_frames_disjoint_not_verified:<slot>`. Motivo dell'esclusione: la covarianza fra due master con frame comuni richiede la varianza dei singoli frame, che il piano del master (`s²/n`, C-02.a §6) non conserva.

### b.4 Rappresentazione P0

| Componente | Rappresentazione | Dove |
|---|---|---|
| Varianza del campione | piano `VARIANCE` per canale, marginale, esatta al primo ordine | memorizzata (ADR-17.a) |
| Covarianza fra i canali dello stesso pixel, immagini `rgb` | tre piani `channel_covariance` | memorizzata (sotto) |
| Covarianza fra pixel diversi della stessa immagine | righe (§b.1) fino alle sorgenti | per riferimento |
| Covarianza fra immagini diverse (master comuni) | sensibilità di §b.2 e `covariance_sources` | per riferimento |
| Fattore d'apertura, funzione di covarianza locale | pullback | calcolati dal consumatore |
| Matrice densa | — | mai |

**Per riferimento.** Regola: una covarianza per riferimento non si memorizza. Il consumatore la calcola con le righe degli operatori a monte e con i piani di varianza delle sorgenti, artefatti del DAG che ADR-05 ricostruisce in modo deterministico. Il riferimento è disponibile se l'immagine è prodotta nel progetto e ogni operatore fra lei e le sorgenti espone le righe. Un'immagine letta da file con `variance_scope = marginal` non ha riferimento.

**Blocco fra canali.** Regola, tipo esteso dell'immagine (ADR-03.b §b.4 delega qui la rappresentazione delle covarianze):

- `image.channel_covariance`: tre piani `Cov(y_c(p), y_d(p))` nell'ordine (0,1), (0,2), (1,2); stessa geometria e `sample_type` dell'immagine; ammesso solo con `layout = rgb` e `variance` presente. Valore NaN se il pixel è invalido nel piano `c` o nel piano `d`, finito altrimenti. Violazioni: `invalid_input`, `image.channel_covariance` (`validate_image`).
- Semidefinitezza, controllata da `validate_image` per ogni pixel, in f64 sui valori memorizzati, con `ε = 2⁻²¹`, sulla sottomatrice dei canali validi in quel pixel. Sia `A` l'insieme dei canali validi con `v_c > 0`:
  - un canale valido con `v_c = 0` ha `c_cd = 0` esatto con ogni canale valido `d`;
  - per ogni coppia in `A`, `ρ_cd = (c_cd / sqrt(v_c)) / sqrt(v_d)`, in quest'ordine, soddisfa `|ρ_cd| ≤ 1 + ε`;
  - se `A` ha tre canali, `det R = 1 + 2 ρ_01 ρ_02 ρ_12 − ρ_01² − ρ_02² − ρ_12² ≥ −14ε`; con due canali o meno la condizione sulle coppie basta, perché una matrice 2×2 con diagonale unitaria è semidefinita se e solo se `|ρ| ≤ 1`.

  La forma `(c / sqrt(v_c)) / sqrt(v_d)` non trabocca su un blocco ammissibile: `sqrt` di un valore f32 o f64 positivo, anche subnormale, è normale, e `|c / sqrt(v_c)| ≤ (1 + ε)·sqrt(v_d)`; un quoziente che trabocca viene da un blocco non ammissibile e fallisce il controllo. Il primo quoziente può invece andare in underflow, fino a zero, quando `|ρ|·sqrt(v_d) < 2⁻¹⁰²²` (per esempio `v_c = 2⁶⁰⁰`, `v_d = 2⁻⁶⁰⁰`, `c = 2⁻¹⁰⁰⁰`). Fatto: con underflow graduale il suo errore assoluto è al più `2⁻¹⁰⁷⁵`, e `sqrt(v_d) ≥ 2⁻⁵³⁷`, quindi `ρ` cambia al più di `2⁻⁵³⁸` in assoluto. Insieme ai `3u` relativi delle tre operazioni, l'errore resta molto sotto il margine `ε` e sposta `det R` di meno di `12·2⁻⁵³⁸`: l'esito del controllo non cambia.

  Fatto (derivazione): sui valori esatti `R` è una matrice di correlazione, quindi `|ρ| ≤ 1` e `det R ≥ 0`; se `c` e `v` vengono da somme f64 con `γ_n ≤ 2⁻²⁶` e da un arrotondamento f32 (`u_32 = 2⁻²⁴`), `|ρ̂ − ρ| ≤ (1 + γ_n)(1 + u_32)/((1 − γ_n)(1 − u_32)) − 1 ≤ ε`, e le derivate di `det R` rispetto a ogni `ρ` sono in modulo ≤ 4, quindi `|det R̂ − det R| ≤ 13ε` al primo ordine; un `ε` di margine copre il calcolo f64 del determinante e i termini di secondo ordine. Con `v_c = 0` esatto la somma che dà `c_cd` ha tutti i termini nulli. Un operatore con righe di più di `2²⁷` termini dichiara il proprio `ε` nel suo contratto.
- Coerenza con la portata: con `variance_scope = independent` ogni valore finito del blocco è 0.
- Ogni immagine `rgb` con varianza prodotta da un operatore lo porta, anche con valori nulli: è un invariante di `operator_output_invariants_hold`. Fa eccezione l'uscita di un operatore che non mescola i canali (curva per canale, combinazione di frame) quando un ingresso `rgb` con `variance_scope` diverso da `independent` non ha il blocco: la covarianza fra canali resta sconosciuta e l'uscita resta senza blocco, mai con zeri. In un'immagine letta da file il blocco può mancare: la covarianza fra canali è allora sconosciuta, salvo `variance_scope = independent`, che la dichiara nulla.
- FITS: HDU `CHANCOV`, NAXIS3 = 3, stesso BITPIX float dei campioni, scala identità, scritto quando presente e letto verbatim. Senza HDU `VARIANCE`: `invalid_input`, `fits.channel_covariance_without_variance`. Layout diverso da `rgb`: `invalid_input`, `fits.channel_covariance_layout`. Codifica e geometria: `fits.extension_encoding`, `fits.extension_geometry`.
- `equivalent_content` lo confronta.

Motivo: luminanza, asinh cromatico e trasformazioni di colore mescolano i canali di ogni pixel. Senza piani memorizzati, il blocco richiederebbe un pullback per pixel, che su una pila costa quanto ricalcolarla. In memoria costa tre piani per immagine `rgb`, quanti i piani di varianza.

**Derivati.** Fattore d'apertura di `φ = Σ_p a_p y_p`: `f_A = Var(φ) / Σ_p a_p² Var(y_p)`, con `Var(φ)` per pullback; esatto al primo ordine. Con denominatore nullo `f_A` è assente con motivo `variance_zero`, mai 0 o 1. Funzione di covarianza locale, cioè la NPS locale nel dominio spaziale: `Cov(y_p, y_{p+Δ})` per `Δ` entro il supporto delle righe, calcolata per pullback. Un consumatore che la sostituisce con una forma stazionaria per tile:

- dichiara l'approssimazione con `variance.fixed_weight_approximation`;
- ne misura l'errore nel proprio contratto, con soglia fissata prima della misura (C-12).

**Costo per riferimento** (stime, non misure):

- il pullback di un funzionale costa la somma delle lunghezze delle righe attraversate;
- un'apertura di `A` pixel su una pila di `K` frame con righe composte di lunghezza `ℓ` costa `O(A·K·ℓ)`;
- la varianza della pila pixel per pixel (§b.6) costa `O(K·ℓ)` per pixel, con un accumulatore di `O(K·ℓ)` valori per worker.

La misura spetta ai benchmark di M3.h (NFR-TEST-03, carico B1). Un costo inaccettabile riapre questo contratto: il modello non cambia in silenzio. Un budget insufficiente è uno stop (ADR-06).

### b.5 Resampling: `C_out = R C_in Rᵀ`

- Regola: `y = R x`, con `R` fissata da trasformazione stimata, kernel, politica dei bordi e maschera d'ingresso. La rinormalizzazione ai bordi o attorno ai pixel invalidi fa parte di `R`. Varianze e covarianze sono condizionate a `R`, quindi alla geometria e alla maschera.
- Ingresso `independent`. Fatto, caso indipendente del piano §8.6, esatto per `R` fissata e valido anche con pesi negativi:
  - `Var(y_p) = Σ_x r_{px}² v_x`;
  - `Cov(y_p, y_q) = Σ_x r_{px} r_{qx} v_x`, che può essere negativa.
- Ingresso `marginal` con riferimento. Righe di `R` composte con quelle degli operatori a monte fino alle sorgenti; per un RGB da debayer `ρ = R·D` fino alla CFA calibrata:
  - `Var(y_{p,c}) = Σ_x ρ_{pc,x}² v_x`;
  - blocco fra canali `Σ_x ρ_{pc,x} ρ_{pd,x} v_x`.

  Applicare `Σ r² v` ai soli piani marginali ignora le covarianze dell'ingresso ed è vietato.
- Vietato interpolare il piano di varianza con il kernel del segnale (`Σ r v`).
- Uscita: `variance_scope = marginal` (ADR-03.b §b.4), blocco fra canali per `rgb`, covarianze fra pixel per riferimento.
- **Geometria.** I piani non contengono l'incertezza della trasformazione stimata (piano §8.7). Il report del resampling dichiara `geometry_uncertainty = not_propagated`, e la covarianza dei parametri resta separata, in C-05. È un limite, quantificato da T-M3C-VAR-10.

### b.6 Combinazione di più frame

Regola per C-07, a pesi fissati (scelta dei pesi e rejection sono di C-07). Per `S(p) = Σ_i w_i y_i(p) / Σ_i w_i` la varianza è il pullback fino alle sorgenti, attraverso le righe di ogni frame e le sensibilità di calibrazione:

```
Var(S(p)) · (Σ_i w_i)² = Σ_i Σ_x b_{i,x}² · Var(L_i(x)) / Q_i(x)²
                       + Σ_M Σ_x ( Σ_{i che usa M} b_{i,x} · ρ_{i,M}(x) )² · Var(M(x))
```

`b_{i,x}` è il coefficiente di `C_i(x)` nel pullback di `w_i y_i(p)` lungo la riga composta del frame `i` (debayer, resampling); `ρ_{i,M}` viene da §b.2.

Fatti:

- È §b.1 applicata al funzionale, esatta al primo ordine.
- Con frame non registrati e senza debayer (`b_{i,x} = w_i` per `x = p`, 0 altrove) si riduce a `Σ_i w_i² Var(C_i(p)) + Σ_{i≠j} w_i w_j Cov(C_i(p), C_j(p))`, con le covarianze di §b.3.

Regole:

- Il termine dei master è parte della varianza di ogni pila che dichiara una varianza.
- Disgiunzione fra i master di tutti i frame combinati. La formula tratta master con digest diversi come indipendenti. Prima di ogni calcolo l'operatore confronta gli insiemi dei frame sorgente di tutti i master del prodotto usati dai frame combinati, e i digest dei light con quegli insiemi. Due master con digest diversi e frame comuni, o un light fra i frame di un master: `unsupported`, `var.master_frames_overlap`, con i digest nel contesto. Per i master esterni con digest diversi l'indipendenza è dichiarata (`master_frames_disjoint_not_verified:<digest>`).
- Rejection e pesi stimati dai dati usano righe a pesi congelati, con il codice `variance.fixed_weight_approximation`.
- Uscita: `marginal` se un ingresso è `marginal`, altrimenti `independent`. Un ingresso `independent` non registrato non correla pixel diversi della pila (§b.3).

### b.7 Passo non lineare

- Regola: per `z = f(y)` applicata pixel per pixel, `Cov(z) ≈ J_f Cov(y) J_fᵀ` (piano §8.14), con `J_f` sui valori osservati. Copiare sull'uscita la varianza dell'ingresso è vietato (§8.14, C-09 campo 6).
- **Curva di un solo canale.** `Var(z_p) = f'(y_p)² Var(y_p)`. La riga di `z_p` è `f'(y_p)` volte la riga di `y_p`; il blocco fra canali si moltiplica per `f'_c · f'_d`. Dove `f` non è derivabile si usa la derivata unilaterale di modulo maggiore. Dove `f` è costante per clipping `f' = 0`, e la varianza al primo ordine è 0.
- **Mescolamento dei canali dello stesso pixel.** `Σ'_p = J_p Σ_p J_pᵀ`, con `Σ_p` dal piano di varianza e dal blocco fra canali. Asinh cromatico `C' = g(I)·C`, `I = Σ_d w_d C_d` (§10.10 C-09): `J_p[c, d] = g(I)·δ_cd + C_c·g'(I)·w_d`.
- Uscita della curva per canale: lo stesso `variance_scope` dell'ingresso. Fatto: `Cov(z_p, z_q) = J_p Cov(y_p, y_q) J_qᵀ` e `J_p` è diagonale, quindi non nascono covarianze fra pixel né fra canali.
- Uscita del mescolamento dei canali: `variance_scope = marginal`, perché un `J_p` non diagonale crea covarianze fra canali anche da canali indipendenti; il blocco fra canali le porta.
- Limite: vicino a un punto di non derivabilità o di curvatura forte, entro poche deviazioni standard, il primo ordine non ha un bound dimostrato qui. C-09 lo misura con soglia fissata prima della misura.

### b.8 Ingressi `marginal` e blocco fra canali assente

ADR-03.b §b.4 lascia ad ADR-17.b questa decisione.

| Operatore | Ingresso `marginal` | Come |
|---|---|---|
| Calibrazione (ADR-16.b) | no, `var.marginal_input` | assume sorgenti indipendenti |
| Debayer (C-03) | no, `var.marginal_input` | assume la CFA `independent` (ADR-03.b §b.3) |
| Resampling (C-06) | sì, con riferimento | righe composte (§b.5) |
| Combinazione di frame (C-07) | sì; con riferimento se ci sono master comuni | §b.6; frame importati indipendenti fra loro per H-IND |
| Curva per canale | sì | Jacobiano per pixel (§b.7) |
| Mescolamento dei canali (luminanza, asinh cromatico, colore) | sì, con blocco fra canali | §b.7 |
| Somma o media su più pixel con incertezza dichiarata (apertura, fondo, rumore per scala) | sì, con riferimento | pullback (§b.4) |

Regole, controllate prima di ogni calcolo:

- Un operatore che richiede il riferimento e non lo ha: `unsupported`, `var.marginal_input`.
- Mescolamento dei canali di un `rgb` con varianza, senza blocco fra canali e con `variance_scope` diverso da `independent`: `unsupported`, `var.channel_covariance_absent`.

### b.9 Offset del modello di rumore per un light RAW

Chiude la questione dell'offset scalare di `noise_model` rispetto al nero `b_L` per posizione.

- Regola: per un ingresso con `decoder_state` e `variance_source = noise_model`, `offset_adu` si dichiara nella ricetta, uno scalare per ingresso. Se manca: `metadata_missing`, `var.source.offset_adu`. Non si legge dall'header e non si ricava da `b`, perché la varianza non dipende dal nero dichiarato (C-01.b §1).
- Fatto: se l'offset atteso vero del pixel è `o(p)`, l'errore del termine di Poisson è al più `|o(p) − offset_adu| / (g·n)` ADU². Motivo: `t ↦ max(t, 0)` è 1-lipschitziana.
- Diagnostica nel report, fuori dal calcolo, per ogni ingresso RAW, sul periodo di H3:
  - `noise_offset_black_spread_adu = max b − min b`;
  - `noise_offset_var_error_estimate = max_p |b(p) − offset_adu| / (g·n)`.

  Sotto H-BLK (C-01.b §3) stima l'errore del termine di Poisson; non è un bound.
- Limite: un offset che varia per colore o per posizione non è rappresentato.

### b.10 Numerica

- Regola: ogni termine in f64, nell'ordine di §b.2, con un solo arrotondamento al tipo di uscita. `C`, `Q`, `Fc` e `N` sono gli intermedi f64 della catena (ADR-16.b §b.12), presi prima dell'arrotondamento dell'uscita.
- Rappresentabilità: valgono le regole di ADR-17.a («Aggiunte di M2.e»). Danno `result_nonfinite`:
  - prodotti e quozienti con operandi non nulli e risultato zero o subnormale;
  - somme di termini non negativi, non tutti nulli, con quel risultato;
  - risultati non finiti.

  `C − N` è una differenza con segno e non si controlla; `r = 0` dà `c = 0`, valido.
- **Regola I, tolleranza degli oracle in aritmetica esatta.** Vale per ogni test che confronta varianze o covarianze del prodotto con un oracle esatto. L'oracle riesegue la sequenza di operazioni fissata dal contratto dell'operatore in aritmetica degli intervalli su razionali:
  - parte dagli ingressi esatti, cioè intervalli puntuali;
  - allarga ogni operazione in virgola mobile del fattore `[1 − u, 1 + u]`, con `u = 2⁻⁵³`;
  - per una mediana prende la mediana degli estremi inferiori e quella degli estremi superiori.

  La tolleranza è l'ampiezza dell'intervallo finale, più mezzo ulp del tipo di uscita se è f32. Fatto, dal teorema fondamentale dell'aritmetica degli intervalli (inclusione isotona; Moore, Kearfott e Cloud, *Introduction to Interval Analysis*, SIAM 2009, non riletto): l'intervallo contiene sia il valore esatto sia quello calcolato. Servono due condizioni: nessuna operazione in underflow, esclusa dalla regola di rappresentabilità, e nessun denominatore il cui intervallo contenga zero, che l'oracle verifica. Un caso che non le soddisfa esce dalla fixture prima della misura. Dove l'oracle verifica che ogni operazione è esatta, la tolleranza è 0. La regola I dà il bound della varianza di ogni catena di ADR-16.b §b.5 su valori non dyadic.

### b.11 Report e provenance

Il `calibration_report` aggiunge:

- `covariance_sources` (§b.3);
- `light_flat_covariance` ∈ {`bias_in_both_chains`, `none`};
- le ipotesi `master_frames_disjoint_not_verified:<slot>` (§b.3);
- la correzione omessa `1 − Q/n_valid` (§b.2), marcata `iid_gaussian_fc`;
- per gli ingressi RAW con `noise_model`, `offset_adu` con sorgente `declared` e le diagnostiche di §b.9.

Il resampling riporta `geometry_uncertainty` (§b.5). Ogni operatore con righe riporta `variance_propagation` (`linear_weights` o `fixed_weight_approximation`, ADR-03.b §b.3).

### b.12 Test pianificati

Tolleranze, soglie e potenza sono fissate qui, prima di ogni misura. Oracle e fixture sono scritti nel test, indipendenti dall'implementazione.

**Protocollo MC-lin** (test Monte Carlo).

Fixture:

- `K = 4000` realizzazioni indipendenti con seed;
- sorgenti con rumore gaussiano continuo di varianza nota, indipendente fra sorgenti, pixel e realizzazioni;
- varianze vere date come piani (`variance_source = plane`);
- `N` deterministica per costruzione: in ogni canale di normalizzazione più di metà dei pixel validi per `N` ha `Fc` senza rumore e uguale a `N*`, quindi i due centrali valgono `N*`;
- sui pixel delle statistiche `sqrt(Var(Fc))/Fc ≤ 10⁻³`;
- deviazioni standard dyadic, varianze uguali ai loro quadrati esatti.

Per ogni statistica `φ`, un funzionale lineare fissato delle uscite del prodotto, il test calcola in aritmetica esatta:

- `φ_lin,k = φ* + Σ_s (∂φ/∂s)(s*)·ε_{s,k}` e `V = Σ_s (∂φ/∂s)(s*)²·v_s`;
- `φ_ex,k`, il valore esatto di `φ` sugli ingressi realizzati.

`ε_{s,k} = σ_s·z_{s,k}`, con `z` dal generatore normale e prodotto esatto, non lo scarto dell'ingresso arrotondato: l'arrotondamento degli ingressi entra in `φ_ex − φ_lin`, quindi in `Δ`. Per il passo non lineare `φ_ex` è in `long double`, con il bound di C-09.

Criteri:

- **(A)** Per ogni `k`, `|φ_k − φ_ex,k| ≤ τ_k`. `τ_k` è la tolleranza numerica del contratto dell'operatore: ADR-16.b §b.12 per la calibrazione, C-06, C-07 e C-09 per gli altri.
- **(B)** `sd_K(φ)` sta in `[sqrt(V·q_lo/(K−1)) − Δ, sqrt(V·q_hi/(K−1)) + Δ]`.
  - `q_lo` e `q_hi` sono i quantili bilaterali di `χ²_(K−1)` al livello `α/m`, con `α = 10⁻³` per test e Bonferroni sulle `m` statistiche del test; si calcolano in modo esatto come in C-02.a §9.
  - `Δ = sd_K(φ_ex − φ_lin) + sqrt(K/(K−1))·max_k τ_k`.
- **(C)** Su una realizzazione aggiuntiva senza rumore (`ε = 0`), la varianza che il prodotto riporta o calcola per riferimento per `φ` (piano, blocco fra canali, valutatore di §b.4) è uguale a `V` entro la regola I. Senza rumore le sensibilità plug-in coincidono con quelle nei valori veri, quindi un prodotto che riporta `V_0` fallisce (C) quando `V_0 ≠ V`.

Fatti:

- `(K−1)·var_K(φ_lin)/V ~ χ²_(K−1)`, esattamente.
- Per la disuguaglianza triangolare sulla norma degli scarti dalla media, `|sd_K(φ) − sd_K(φ_lin)| ≤ sd_K(φ − φ_lin) ≤ Δ` quando vale (A). Con prodotto e modello corretti, (B) fallisce quindi con probabilità ≤ `α/m`.
- `Δ` dipende dal prodotto solo attraverso `τ`, che (A) verifica: un errore del prodotto non allarga l'intervallo.

Potenza, verificata dall'oracle prima dell'esecuzione, solo sulle statistiche discriminanti (i controlli senza covarianza hanno `V_0 = V` e usano soltanto (A), (B), (C)):

- Per ogni statistica discriminante è dichiarato il modello ingenuo `V_0`, quello senza la covarianza, e la fixture ha `|V_0/V − 1| ≥ 0.25`.
- La probabilità di mancata discriminazione, cioè che `sd_K(φ_lin)` cada nell'intervallo costruito con `V_0` allargato di `2·Δ_max`, deve essere ≤ 10⁻⁶ (potenza ≥ 1 − 10⁻⁶); si calcola dalla distribuzione di `χ²_(K−1)`.
- `Δ_max` maggiora `Δ` con probabilità ≥ 1 − 10⁻⁶ su `|δFc| ≤ 7` deviazioni standard. Per la divisione viene dall'identità di §b.2; per il passo non lineare dal resto di Lagrange sullo stesso dominio.

Una fixture senza questa potenza si cambia prima della misura.

| ID | Unità | Verifica | Criterio |
|---|---|---|---|
| T-M3C-VAR-01 | M3.c | Varianza di ogni catena di ADR-16.b §b.5 × light {FITS, RAW con `noise_model`}. Con `B` in entrambe le catene: `C/N` ∈ {0, 1/2, 1, 2} e un caso con `|C − N|` piccolo rispetto a `N`. Variante dyadic con ogni operazione esatta; variante non dyadic. Fuori dal caso `C = N`, dove `c = 0` è il valore atteso, ogni termine `a`, `b`, `c` presente vale almeno `10⁻³` di `Var(C)` (lo verifica l'oracle), quindi un termine omesso o con coefficiente sbagliato esce dalla tolleranza. Bit `q_unreliable` al confine, con valori dyadic per cui il confronto è esatto: `sqrt(Var(Q))/Q` uguale a `q_rel_sigma_max` e appena sopra, per `light.dark` + `flat.bias` e `light.dark` + `flat.bias_dark_flat_sub` (`B` solo nel flat) e per `light.bias` + `flat.bias` (`B` in entrambe); `Var(B)` scelta perché contarla due volte in `Var(Fc)` porti il pixel al confine sopra la soglia | Dyadic: 0. Altrimenti regola I, più mezzo ulp per uscita f32. `q_unreliable`: acceso solo sopra la soglia, esatto |
| T-M3C-VAR-02 | M3.c | MC-lin con `B` in entrambe le catene: {`light.bias`, `light.bias_dark_sub`} × {`flat.bias`, `flat.bias_dark_flat_sub`}, pixel con `C/N` ∈ {1/2, 1, 2}. `V_0` tratta le due sottrazioni di `B` come indipendenti. Controlli senza bias comune: `light.dark` + `flat.bias`, `light.bias` + `flat.dark_flat` | (A), (B), (C); mancata discriminazione ≤ 10⁻⁶ sulle statistiche discriminanti |
| T-M3C-VAR-03 | M3.c | MC-lin su due light calibrati con gli stessi master. Statistiche `C_1(x) + C_2(x)` e `C_1(x) − C_2(x)` per `light.dark` + `flat.none` (lineare) e per `light.bias_dark_sub` + `flat.bias_dark_flat_sub`; `V_0` con light indipendenti. Controlli: due light con dark diversi; `C_1(x) + C_2(x')` con `x ≠ x'`. Per (C) la covarianza è quella del valutatore per riferimento di §b.3 | (A), (B), (C); mancata discriminazione ≤ 10⁻⁶ sulle statistiche discriminanti |
| T-M3C-VAR-04 | M3.c | `covariance_sources` e `light_flat_covariance` per ogni catena. Master del prodotto con un frame comune fra due slot; light fra i frame di un master. `B` in due catene. Master esterni. Light RAW con `noise_model` senza `offset_adu`. Nero `b → b + 16` su tutti gli ingressi RAW | Binaria, campo per campo: `shared_input` nell'analisi; `calib.shared_input` nel preflight con i digest comuni; nessuna esclusione per `B` in due catene; `master_frames_disjoint_not_verified:<slot>`; `var.source.offset_adu`. Con `b + 16`: campioni, varianza e maschera bit-identici, diagnostiche di §b.9 uguali all'oracle |
| T-M3C-VAR-05 | M3.d | Blocco fra canali del debayer di C-03 su CFA con varianze distinte per pixel, quattro pattern, contro `Σ_x w_c w_d v_x` dell'oracle; righe sintetiche con campioni condivisi fra canali (blocco non nullo, indipendente dal metodo). Invarianti: NaN se il pixel è invalido in uno dei due piani, finito altrimenti, presenza su ogni `rgb` prodotto; semidefinitezza con blocchi al confine (`|ρ| = 1`, `det R = 0` esatti), appena fuori (`|ρ| = 1 + 2ε`, `det R = −28ε`), `v_c = 0` con `c_cd ≠ 0`; una, due e tre varianze nulle con covarianze nulle; ognuno dei tre canali invalido con la coppia restante fuori confine (`v = 1`, `c = 2`); varianze f64 `2⁶⁰⁰` e `2⁻⁶⁰⁰` con `|ρ|` al confine; `v_c = 2⁶⁰⁰`, `v_d = 2⁻⁶⁰⁰`, `c = 2⁻¹⁰⁰⁰` (underflow del primo quoziente, blocco semidefinito); `independent` con blocco non nullo; in memoria e da FITS. `CHANCOV` scritto e riletto; HDU senza `VARIANCE`, layout non `rgb`, BITPIX o geometria diversi. `equivalent_content` con un solo valore diverso | Dyadic 0, altrimenti regola I; rilettura bit-esatta; confine, varianze nulle coerenti e varianze estreme accettati; fuori confine, coppia valida fuori confine con un canale invalido e incoerenze `image.channel_covariance`; altri codici di §b.4; `false` |
| T-M3C-VAR-06 | M3.g | Pullback su griglie fino a 16×16 contro l'oracle denso `J diag(v) Jᵀ` in aritmetica esatta. Casi: resampling di un ingresso `independent` (shift subpixel, rotazione, scala con Jacobiano ≠ 1); RGB da debayer (righe composte); bordi rinormalizzati e invalidati; kernel con pesi negativi. Grandezze: varianze, covarianze fra uscite entro il supporto, blocco fra canali, somme d'apertura, `f_A`, anche con denominatore nullo. Righe: il test le ricava in modo indipendente dalla definizione del kernel (C-06) e dai pesi del debayer (C-03), le confronta con quelle del prodotto e verifica che il segnale del prodotto sia `Σ ρ x` con le sue righe; mutazione: righe alterate a segnale invariato | Covarianze: dyadic 0, altrimenti regola I, con le righe del prodotto convertite in razionali senza errore. Righe e segnale: tolleranza di C-06. `f_A` con denominatore nullo: assente, `variance_zero`. La mutazione fa fallire il test |
| T-M3C-VAR-07 | M3.g | MC-lin del resampling (lineare) per ingresso `independent` e per RGB da debayer. Statistiche: `y_p`, `y_p ± y_q` su coppie vicine, somme d'apertura 3×3. Modelli `V_0`: varianza interpolata `Σ r v`; `Σ r² v` sui soli piani marginali; covarianza nulla (`f_A = 1`) | (A) con la tolleranza di C-06; (B); (C); mancata discriminazione ≤ 10⁻⁶ contro ogni `V_0` applicabile |
| T-M3C-VAR-08 | M3.h | Pila di 3 e di 8 frame con master comuni, senza registrazione e con spostamenti subpixel e interi diversi fra frame. Oracle esatto della formula di §b.6 contro il prodotto. MC-lin con `flat.none` e con `B` in entrambe le catene. Modelli `V_0`: frame indipendenti; limite di Cauchy–Schwarz `(Σ |w_i| sd_i)²` sulla configurazione con spostamenti. Frame calibrati in nodi diversi con master di digest diversi e un frame sorgente comune; un light fra i frame del master di un altro light | Oracle: regola I. MC: (A) con la tolleranza di C-07; (B); (C); mancata discriminazione ≤ 10⁻⁶. Master con frame comuni: `var.master_frames_overlap` e 0 calcoli |
| T-M3C-VAR-09 | M3.k | Jacobiano di una curva per canale e dell'asinh cromatico, su un ingresso con blocco fra canali non nullo. Oracle `J Σ Jᵀ` in `long double`. MC-lin nel regime `Δ_max ≤ 0.01·sqrt(V)`. Modelli `V_0`: varianza lineare copiata; blocco fra canali ignorato. `variance_scope` dell'uscita per ingresso `independent` e `marginal`, anche dopo export e rilettura | Oracle: bound di C-09. MC: (A), (B), (C); mancata discriminazione ≤ 10⁻⁶. Portata: invariata per la curva per canale, `marginal` per il mescolamento |
| T-M3C-VAR-10 | M3.g | Report del resampling. MC su una scena con gradiente forte, con i parametri della trasformazione estratti dalla covarianza di C-05 | Report: `geometry_uncertainty = not_propagated`, binaria. Il rapporto fra varianza empirica e varianza del piano si misura e si registra come limite, senza gate |
| T-M3C-VAR-11 | M3.g, M3.h, M3.k | Ammissione di §b.8, per ogni operatore della tabella che richiede il riferimento: un ingresso `marginal` letto da file e lo stesso prodotto nel progetto. `rgb` letto senza `CHANCOV`, con `variance_scope` `marginal` e `independent`, verso un operatore che mescola i canali e verso curva per canale e combinazione di frame | Senza riferimento: `var.marginal_input` e 0 calcoli (contatore del `fault_injector`). Con riferimento: nessuno stop. Blocco assente: `var.channel_covariance_absent` con `marginal`, nessuno stop con `independent`; curva per canale e combinazione di frame: con ingresso `marginal` senza blocco uscita senza blocco e nessuno zero scritto, con ingresso `independent` blocco presente e nullo |

Calibrazione e debayer su un ingresso `marginal` sono già coperti da T-M3A-TYP-06. Test del piano che questi alimentano: T-CAL-08 (varianza propagata, M3.s), il gate di M3.g («covarianza tra output», §8.6.2) e il punto 1 del Gate M3 (metriche di varianza).

### b.13 Limiti dichiarati

- H-IND: il rumore correlato del sensore non è modellato. Per i master esterni la disgiunzione dei frame non è verificata. Due master del prodotto con frame comuni non si combinano.
- `N` costante: il fattore di scala comune ha solo la stima di ADR-17.a; la correzione `1 − Q/n_valid` è una stima omessa.
- Primo ordine con plug-in: per il rapporto, l'errore è limitato dall'identità di §b.2. Per il passo non lineare non c'è un bound vicino ai punti non derivabili.
- Righe a pesi congelati (rejection, pesi stimati, debayer edge-aware): errore non misurato qui.
- Geometria: varianze condizionate alla trasformazione stimata; l'incertezza dei parametri non è propagata.
- L'offset di `noise_model` per un light RAW è uno scalare dichiarato.
- Il costo delle covarianze per riferimento è stimato, non misurato.
- Un'immagine esportata conserva solo piano di varianza e blocco fra canali; letta di nuovo, non ha riferimento.
- `multi_plane`: nessun blocco fra canali. Un operatore che ne mescola i canali con varianza non è P0.
- I piani empirici dei master del prodotto restano stime casuali (Aggiunte di R2).

### b.14 Modifiche ad altri contratti

- ADR-16.b §b.10, voce «Varianza», e §b.14: le ricette con `B` in entrambe le catene hanno il modello di §b.2.
- ADR-03.b §b.11: il limite sulle covarianze del RGB da debayer rinvia a §b.4 e §b.8.
