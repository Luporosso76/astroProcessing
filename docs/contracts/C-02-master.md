# Contratto C-02.a: generazione dei master, slice minima

Stadio: C-02.a (registro riga 3430, testo C-02 righe 1371-1385 del piano). Unità: R2 di `plans/M2-residui.md`, nodi DAG dei master. Perimetro `.a`: set parziali e gruppi misti appartengono a F-CAL-01.b (M3.b). Stati e descrittore dei master in [ADR-16](ADR-16-stati-master.md); varianza in [ADR-17](ADR-17-varianza.md), con l'estensione richiesta in fondo a questo contratto; maschere e tipi in [ADR-03](ADR-03-pixel-maschere.md); errori e nodo in [ADR-05](ADR-05-dag-provenance.md#modello-derrore-comune); blocchi di righe in [ADR-06](ADR-06-budget-scheduler.md). Decisioni D5, D6, D23 del piano.

## 1. Problema matematico

Dato un gruppo di `n` frame raw dello stesso ruolo, stimare per ogni pixel il master `M` come media dei campioni `x_f`. Ruoli: `bias`, `dark`, `dark_flat`, `flat`. Il master è una stima con incertezza, non una misura priva di errore. L'incertezza è un piano `VARIANCE` stimato dai frame stessi (varianza empirica, §6), senza modello di rumore: il piano chiede di dichiarare l'incertezza del master (riga 1380, campo 6), e la stima empirica non dipende da guadagno, rumore di lettura o offset dichiarati.

Parametri, tutti dichiarati nella ricetta e senza default:

| Parametro | Valori | Note |
|---|---|---|
| `combine` | `mean` | Unico valore in `.a` |
| `rejection` | `none` | Unico valore in `.a` |
| `flat_normalization` | `none` | Unico valore in `.a`; solo ruolo `flat` |
| `flat_level_rel_tol` | in (0, 1) | Solo ruolo `flat`: scarto relativo massimo dei livelli dei frame (§2, §6.2) |
| `saturation_adu` | numero o assente | Assente: avviso `saturation_not_evaluated` |
| `saturated_fraction_max` | in [0, 1] | Obbligatorio se `saturation_adu` è dichiarato |
| `flat_min_signal_adu` | > 0 | Solo ruolo `flat` |
| `offset_adu` | numero | Per il livello del flat e la compatibilità |

`combine = median`, `rejection = sigma_clip` e `flat_normalization = median_ratio` si fermano nel preflight con `unsupported`, `master.variance_domain`, e appartengono a F-CAL-01.b (M3.b; motivi in «Fuori da `.a`»). D6 (`plans/M2-residui.md` riga 70) vuole per ogni metodo uno stimatore con dominio derivato e ferma ciò che ne è fuori. D5 (riga 69) delimita la slice con la sola media senza normalizzazione. La riga 1840 del piano chiede che la `.a` sia la slice sintetica minima e che la `.b` completi il requisito.

Per pixel, `M = x̄`, la media in f64 dei `x_f` nell'ordine canonico dei frame (§8). Il master flat include il dark-flat; la sottrazione del dark-flat resta della calibrazione (ADR-16, stato `raw`).

Livello del flat, solo per il controllo del §2: `m_f` è la mediana di `(x_f − offset_adu)` sui pixel senza bit di invalidità in `x_f`, e `m̄` la media degli `m_f`. Con un numero dispari di pixel la mediana è l'elemento centrale; con un numero pari è `(z_k + z_(k+1)) / 2` in f64, somma e divisione correttamente arrotondate in quest'ordine. `m_f` è una statistica deterministica del frame; non entra nel master né nel piano.

## 2. Dominio, unità e prerequisiti

Frame mono o CFA, unità `adu`, fase di processo `raw`, `linear` (altrimenti `unsupported`, `master.input_state`). `rgb` e `multi_plane`: `unsupported`, `calib.plane_layout`. `pedestal_adu` ≠ 0: `unsupported`, `calib.pedestal`.

Il gruppo è compatibile solo se tutti i frame hanno uguali: larghezza, altezza, `binning`, CFA (pattern e offset), `camera`, `gain`, `offset_adu`, `roi_x`, `roi_y`, `readout_mode`, `row_order` (se presenti), `filter` per il ruolo `flat`. `exposure_s` entro `exposure_rel_tol` e `temperature_c` entro `temperature_abs_tol_c` (stessa regola e stessa `temperature_policy` di ADR-16) sono confrontati a coppie con il frame di riferimento canonico (il primo). `IMAGETYP` in conflitto col ruolo: `incompatible`, `calib.imagetyp_conflict`.

Il preflight verifica tutto prima di ogni calcolo e non esclude mai un frame in automatico.

| Condizione | Categoria | Codice |
|---|---|---|
| Campo di compatibilità diverso | `incompatible` | `master.mismatch.<campo>`, con valori e tolleranza nel contesto |
| Campo richiesto assente | `metadata_missing` | `master.metadata.<campo>` |
| `n` sotto il minimo | `invalid_input` | `master.frames_insufficient` |
| Due frame con lo stesso digest | `unsupported` | `calib.shared_input` |
| Parametro assente, non finito o di tipo errato | `invalid_input` | `master.param.<nome>` |
| Metodo o valore fuori dal dominio della varianza (§1, §6) | `unsupported` | `master.variance_domain` |
| Frame saturo oltre `saturated_fraction_max` | `invalid_input` | `master.flat_saturated` (ruolo `flat`), `master.saturated` (altri) |
| `m_f` sotto `flat_min_signal_adu`, nessun pixel valido o `m_f` non finito | `invalid_input` | `master.flat_low_signal` |
| Un frame con `|m_f / m̄ − 1| > flat_level_rel_tol` | `unsupported` | `master.variance_domain`, con `m_f`, `m̄` e la tolleranza nel contesto |

Minimi e domini: `n ≥ 2`. Per il flat, i livelli `m_f` si calcolano in un primo passaggio, prima della combinazione, e il loro controllo è parte del preflight del master.

## 3. Ipotesi e limiti

Stazionarietà nel gruppo, outlier sparsi, illuminazione del flat stabile. Il metodo non è affidabile con nuvole in movimento, deriva termica, flat saturi o gruppi disomogenei; il report lo dichiara come ipotesi e non lo verifica. Frame indipendenti fra loro. Per bias, dark e dark-flat, stessa media per pixel in tutti i frame (`stationary_mean`); per il flat, le ipotesi del §6.2.

Un outlier non viene scartato: sposta il master e, se lontano dagli altri campioni, aumenta `s²` su quel pixel. Il piano ne riflette la dispersione, ma non è una garanzia sull'errore del master in quel pixel.

## 4. Alternative e scelta

Fuori da `.a`, in F-CAL-01.b (M3.b): mediana, media pesata, winsorized, `sigma_clip`, normalizzazione dei flat. La varianza da modello di rumore (`noise_model` di ADR-17) non serve ai master del prodotto, perché la stima empirica non ne dipende. Resta per i master esterni dichiarati con `combine_method = mean`. Il prodotto non sceglie e non cambia metodo dopo un fallimento.

## 5. Riferimenti indipendenti

- Varianza campionaria a due passaggi e sua stabilità: Chan, Golub e LeVeque, «Algorithms for computing the sample variance: analysis and recommendations», *The American Statistician* 37(3), 1983.
- Distribuzione di `s²` per campioni gaussiani (`χ²` con `n − 1` gradi, non centrale con medie diverse). La funzione di ripartizione del `χ²` è la gamma incompleta regolarizzata `P(k/2, x/2)`: serie e frazione continua di Abramowitz e Stegun, *Handbook of Mathematical Functions*, 1964, 6.5.29 e 6.5.31.
- Aritmetica in virgola mobile: Higham, *Accuracy and Stability of Numerical Algorithms*, 2ª ed., SIAM 2002, cap. 3 e 4 (come in ADR-07).

Nessuna derivazione da software esistente. I testi non sono stati riletti integralmente, come dichiarato nel record di R0.

## 6. Numerica, stabilità e varianza

Accumulo in f64, un solo arrotondamento al tipo di uscita (`sample_type` del master, f32 di default). Il master include il piano `VARIANCE`, `variance_source = plane` per i consumatori (ADR-17). Per pixel, sui `x_f` di tutti gli `n` frame:

```
s² = Σ_f (x_f − x̄)² / (n − 1)
piano VARIANCE = s² / n
```

`s²` è calcolata a due passaggi nell'ordine canonico: prima `x̄` in f64, poi la somma dei quadrati degli scarti in f64 (Chan, Golub e LeVeque, 1983).

Tre proprietà distinte, da non confondere:

1. **Valore atteso.** Rispetto alla varianza vera di `M`, il piano è non distorto per bias, dark e dark-flat sotto `stationary_mean` (§6.1). Per il flat sovrastima, in valore atteso, di un termine `B ≥ 0`, limitato sotto ipotesi dichiarate (§6.2).
2. **Variabilità.** Il piano di un singolo pixel e di una singola realizzazione è una stima casuale. Può risultare sopra o sotto la varianza vera, anche dove il valore atteso è corretto o in eccesso.
3. **Variabilità nel caso gaussiano omoschedastico.** Per frame gaussiani con la stessa varianza `σ²` e la stessa media, `(n − 1) s² / σ² ~ χ²_(n−1)` e il coefficiente di variazione di `s²` è `sqrt(2 / (n − 1))`: 0.50 con `n = 9`, 0.20 con `n = 51`. Fuori da quel caso (eteroschedastico, non gaussiano, medie diverse) la formula non vale. Il report lo registra come `variance_cv_gaussian_homoscedastic`, con l'ipotesi nel nome, e non come proprietà del dato.

### 6.1 Bias, dark, dark-flat

Frame indipendenti con la stessa media `μ` per pixel (`stationary_mean`) e varianze `σ_f²`, anche diverse fra frame. Si ha `Σ (x_f − x̄)² = Σ (x_f − μ)² − n (x̄ − μ)²`, quindi `E[Σ (x_f − x̄)²] = Σ σ_f² − Σ σ_f² / n = (n − 1) Σ σ_f² / n`. Ne segue `E[s² / n] = Σ σ_f² / n² = Var(M)`. Il valore atteso è corretto senza ipotesi di gaussianità né di omoschedasticità, e comprende ogni sorgente di rumore dei frame, quantizzazione inclusa. Una deriva della media fra i frame, che viola `stationary_mean`, aggiunge in valore atteso `Var_f(μ) / n`, con `Var_f(μ) = Σ (μ_f − μ̄)² / (n − 1)`.

### 6.2 Flat

Modello: `x_f(p) = b(p) + c_f · g(p) + ν_f(p)`. `b` è il termine additivo comune (bias e corrente di buio all'esposizione del flat). `g` è la forma d'illuminazione, comune ai frame (`same_illumination_shape`). `c_f` è il livello del frame, `ν_f` sono rumori indipendenti di media nulla e varianza `σ_f²(p)`. Con medie `μ_f = b + c_f g` diverse fra frame, la stessa identità del §6.1 dà:

```
E[Σ (x_f − x̄)²] = (n − 1) Σ σ_f² / n + Σ (μ_f − μ̄)²
E[s² / n]       = Var(M) + g(p)² · Var_f(c) / n,    Var_f(c) = Σ (c_f − c̄)² / (n − 1)
```

con `Var(M) = Σ σ_f² / n²`. In valore atteso, il piano sovrastima `Var(M)` del termine `B(p) = g(p)² · Var_f(c) / n ≥ 0`, dovuto ai livelli diversi e non al rumore.

Limite del termine. Ipotesi `level_proportional`: `m_f = κ · c_f`, cioè la mediana di `(x_f − offset_adu)` è proporzionale al livello. È un'ipotesi dichiarata e non verificata, ragionevole quando `b − offset_adu` e il rumore sono trascurabili nella mediana su molti pixel. Sotto questa ipotesi `|c_f / c̄ − 1| = |m_f / m̄ − 1| ≤ ε`, con `ε = flat_level_rel_tol`, quindi `Σ (c_f − c̄)² ≤ n ε² c̄²` e:

```
B(p) ≤ ε² · (c̄ g(p))² / (n − 1)
```

Due ipotesi ulteriori, entrambe necessarie per confrontare i quadrati. `additive_nonnegative`: `b(p) ≥ offset_adu`. `nonnegative_signal`: `c̄ g(p) ≥ 0`, cioè il segnale d'illuminazione atteso non è negativo. Senza la seconda il confronto fallisce: con `b − offset_adu = 2`, `c̄g = −2` e livelli variabili, `B > 0` ma `E[M] − offset_adu = 0`. Con entrambe, `0 ≤ c̄ g(p) ≤ E[M(p)] − offset_adu` e `B(p) ≤ B_max(p) = ε² · (E[M(p)] − offset_adu)² / (n − 1)`. `E[M]` non è noto: il report registra la stima plug-in

```
level_bias_estimate(p) = ε² · max(M(p) − offset_adu, 0)² / (n − 1)
```

con `M` osservato. È una stima, non un limite. In una singola realizzazione può stare sotto `B(p)`: per esempio con `n = 2`, `b = offset_adu = 0`, `g = 1`, `c = (1 − ε, 1 + ε)` e rumore medio negativo, `B = ε²` ma la stima vale `ε² M² < ε²`. In valore atteso invece `E[level_bias_estimate] ≥ B_max ≥ B` sotto le ipotesi `level_proportional`, `additive_nonnegative` e `nonnegative_signal`, per la disuguaglianza di Jensen, perché `t ↦ max(t, 0)²` è convessa. Con `M − offset_adu > 0`, `E[level_bias_estimate] = ε² ((E[M] − offset_adu)² + Var(M)) / (n − 1)`. Il report registra per canale, sui pixel validi, mediana e massimo della stima e del suo rapporto col piano. Un piano nullo, caso valido (§6.3), rende il rapporto `null` con motivo `variance_zero`: il pixel resta valido, è escluso dagli aggregati del rapporto e contato a parte (`level_bias_ratio_null`). Se la forma d'illuminazione cambia fra i frame, `s²` contiene anche quella dispersione, che `ε` non limita. Il report dichiara `same_illumination_shape`, `level_proportional`, `additive_nonnegative` e `nonnegative_signal` come ipotesi non verificate. `nonnegative_signal` non è verificabile per pixel con le statistiche disponibili, perché `M − offset_adu` include `b − offset_adu`. Il report conta però per canale i pixel validi con `M − offset_adu ≤ 0` (`pixels_nonpositive_signal`), un indizio di violazione, non una verifica.

Correlazioni. Il master flat non contiene il dark-flat e non usa scale stimate. Il dark-flat entra solo in calibrazione (`Fc = F − DF`), come esposizione distinta: `Cov(F, DF) = 0`, come in ADR-17.a.

### 6.3 Aritmetica

Nessun `epsilon` al posto di un valore invalido. Regola di rappresentabilità, la stessa della calibrazione (`calibration.cpp`: `mul_ok`, `div_ok`, `add_ok`, `convert_finite`; D23, ADR-07 per FTZ, DAZ e MXCSR):

- **Prodotti e quozienti** (`(x_f − x̄)²`, `/ n`, `/ (n − 1)`): con operandi non nulli, un risultato zero o subnormale è un underflow e rende il pixel `result_nonfinite`.
- **Somme di termini non negativi** (la somma dei quadrati): con termini non tutti nulli, un risultato zero o subnormale rende il pixel `result_nonfinite`.
- **Somme e differenze di valori con segno** (`Σ x_f`, `x_f − x̄`): uno zero o un subnormale è il risultato esatto e resta valido. Con underflow graduale e arrotondamento al più vicino, `x − y = 0` solo se `x = y` (Higham, cap. 2, lemma di Sterbenz), e una somma il cui valore esatto cade fra i subnormali è rappresentata esattamente (Hauser, «Handling floating-point exceptions in numeric programs», *ACM TOPLAS* 18(2), 1996). L'esattezza vale per la singola operazione su operandi già arrotondati, non per una riduzione. `Σ x_f` accumula errori: in f64 `(2⁵⁴ + 1) − 2⁵⁴` dà 0, perché `2⁵⁴ + 1` è già arrotondato a `2⁵⁴`, mentre il valore esatto è 1. Uno zero o un subnormale prodotto dalla riduzione resta un valore valido, non un underflow, ma con l'errore cumulativo del bound di T-M2R2-02 (§6.3), non esatto. Così una media nulla da campioni `−1, +1` è un valore valido. La calibrazione segue già questa regola: non controlla `l − d`.
- **Ogni risultato non finito** rende il pixel `result_nonfinite`.
- **Conversione al tipo di uscita** (`convert_finite`): un valore f64 non nullo che diventa zero o subnormale rende il pixel `result_nonfinite`; uno zero esatto resta valido.

Un piano con `s² = 0` (tutti i campioni uguali: scarti nulli, quadrati di operandi nulli, somma di zeri) è un valore valido, non un errore. I subnormali in ingresso sono valori finiti validi, letti come sono.

Errore di arrotondamento del piano, per T-M2R2-02. Modello standard `fl(a ∘ b) = (a ∘ b)(1 + δ)` con `|δ| ≤ u` e `(1 + γ_a)(1 + γ_b) ≤ 1 + γ_(a+b)` (Higham, cap. 3).

- Media: la somma ricorsiva e la divisione danno `|x̄̂ − x̄| ≤ Δ = γ_n Σ|x_f| / n`.
- Identità esatta: `Σ (x_f − x̄̂)² = Σ (x_f − x̄)² + n (x̄̂ − x̄)²`.
- Ogni termine si calcola come `d̂_f = (x_f − x̄̂)(1 + δ_1)` e `fl(d̂_f · d̂_f) = (x_f − x̄̂)² (1 + δ_1)² (1 + δ_2)`, con tre fattori e quindi `1 + γ_3`. La sottrazione non si assume esatta: nel dominio non è garantita la condizione di Sterbenz.
- La somma ricorsiva degli `n` quadrati attraversa al più `n − 1` addizioni: `1 + γ_(n−1)`. Ogni termine porta in tutto `1 + γ_(n+2)`.
- Le divisioni per `n − 1` e per `n` aggiungono `γ_2`.

Con `V = s²/n` esatto:

```
|V̂ − V| ≤ γ_(n+4) · V + (1 + γ_(n+4)) · Δ² / (n − 1)
```

più mezzo ulp del tipo di uscita. Tutti i termini sommati sono non negativi, quindi non c'è cancellazione nella somma dei quadrati.

## 7. Maschere, metadati e bordi

- Un campione non finito o con bit di invalidità in un frame lascia il frame nel gruppo e rende invalido il pixel del master, con i bit di input propagati per unione (`source_nonfinite`, `source_flagged`); campione e varianza NaN. Nessuna esclusione automatica del campione: un pixel valido usa sempre tutti gli `n` frame.
- Un campione ≥ `saturation_adu` accende `saturated` sul pixel del master (sotto la soglia `saturated_fraction_max` per frame), con campione e varianza NaN.
- Il master conserva geometria, CFA e metadati comuni del gruppo. Il descrittore derivato dal nodo (ADR-16) contiene `combine_count = n`, `combine_method = mean`, `rejection = none`, `variance_estimator = empirical`, ruolo e digest.
- Il report contiene, per frame, digest e ruolo; per il flat anche `m_f` e `m_f / m̄ − 1`. Contiene inoltre i totali per bit di invalidità, `variance_estimator = empirical`, `variance_cv_gaussian_homoscedastic`, e per il flat `level_bias_estimate` per canale (mediana e massimo, assoluti e relativi al piano, con `level_bias_ratio_null`). Riporta infine `pixels_nonpositive_signal` per il flat, le ipotesi (`stationary_mean`, oppure `same_illumination_shape`, `level_proportional`, `additive_nonnegative`, `nonnegative_signal`) e gli avvisi. Nessun frame è scartato, e il report lo dichiara (`rejection = none`).

## 8. Determinismo

Ordine canonico dei frame: digest SHA-256 crescente, confrontato byte per byte. Con stessa ricetta, stessi input e stesso backend, il master è bit-identico con qualunque `tile_rows` e numero di worker, a blocchi o in memoria (T-M2R2-07). Media, `s²` e i loro accumuli per pixel seguono l'ordine canonico. `m_f` si calcola sull'intera immagine prima di dividere in blocchi. Con un numero dispari di pixel è una selezione, esatta; con un numero pari è la media f64 dei due centrali (§1), non esatta in senso matematico ma bit-identica fra percorsi, perché usa le stesse due operazioni IEEE nello stesso ordine. I master non usano mediane per pixel. Nessun seed: il prodotto non ha componenti stocastiche.

## 9. Fixture, metriche, soglia e gate

Fixture sintetiche con `B`, `d`, `q`, `S` e rumore noti, e outlier iniettati; dato reale PUB-LFC-01. Le tolleranze sono derivate qui, prima della misura. Nei test statistici il riferimento è la varianza vera della simulazione, nota all'oracle, non un'altra stima.

| Test | Oracle | Tolleranza |
|---|---|---|
| T-M2R2-01 | Aritmetica razionale, valori dyadic | 0, per master e piano `VARIANCE`. Include un pixel con media nulla da campioni di segno opposto (`−1, +1, …`) e un pixel con campioni tutti uguali (`s² = 0`): entrambi validi |
| T-M2R2-02 | Aritmetica razionale, valori non dyadic | Master entro `γ_n Σ|x_i| / n` (f64, con `γ_k = ku/(1 − ku)`, `u = 2⁻⁵³`), più mezzo ulp f32 se l'uscita è f32. Piano entro il bound del §6.3 |
| T-M2R2-03 | Outlier iniettati in posizioni note | Nessuna esclusione: master e piano uguali all'oracle razionale su tutti i campioni, outlier compresi (tolleranze di T-M2R2-02), e il report conta zero scarti |
| T-M2R2-04 | Varianza vera della simulazione | Vedi sotto |
| T-M2R2-05 | — | Binaria: stop prima della combinazione, nessun master |
| T-M2R2-06 | — | `node_key` e digest identici dopo riapertura e ricostruzione |
| T-M2R2-07 | Caso in memoria | 0 ulp, bit-identico |
| T-M2R2-08 | Output del master | Report e provenance coerenti con output e input |
| T-M2R2-09 | Media costruita dal test su PUB-LFC-01 | Master con `sample_type = f64` nella ricetta della fixture: `γ_n Σ|x_i| / n`, senza termine di uscita. È la scelta più severa; il percorso f32 è coperto da T-M2R2-02 con il mezzo ulp |

T-M2R2-04. Per pixel si fanno `K` realizzazioni indipendenti con seed, con campioni gaussiani continui. `K`, il numero di pixel `P`, i seed e i parametri sono fissati prima dell'esecuzione. Pixel e realizzazioni sono indipendenti per costruzione della fixture (seed distinti, nessun termine condiviso). Livello complessivo α = 10⁻³, diviso con Bonferroni sulle `m` statistiche verificate. Scale esplicite: il piano è `s²/n`, quindi `E[piano] = E[s²]/n` e `Var(piano) = Var(s²)/n²`.

Casi esatti, chi-quadro:

- **Piano, stessa media e stessa varianza `σ²`.** `(n − 1) s²/σ² ~ χ²_(n−1)`, quindi `K(n − 1) · media_K(piano)/(σ²/n) ~ χ²_(K(n−1))` per pixel. Il rapporto `media_K(piano)/(σ²/n)` deve stare nell'intervallo bilaterale di quella distribuzione al livello `α/m`. La distribuzione è esatta. I quantili si calcolano anch'essi in modo esatto, entro l'errore numerico dichiarato:
  - il test inverte `P(k/2, x/2)` per bisezione, con la sola libreria standard C++ (`std::lgamma`, `std::exp`, `std::log`);
  - per `x/2 < k/2 + 1` usa la serie 6.5.29, altrimenti la frazione continua 6.5.31;
  - si ferma con errore relativo ≤ 10⁻¹² sulla coda calcolata e ≤ 10⁻¹⁰ sul quantile. La coda è `P` per il quantile inferiore e `Q = 1 − P` per quello superiore, quest'ultima presa direttamente dalla frazione continua, senza sottrazione da 1.

  Questo errore sposta la probabilità di copertura di meno di `10⁻⁶ · α/m`, trascurabile. Non si usa l'approssimazione di Wilson-Hilferty.
- **Valori del master.** `M` è gaussiano con varianza `V = Σσ_f²/n²` anche con varianze o livelli diversi, quindi `(K − 1) · var_K(M)/V ~ χ²_(K−1)`, con lo stesso intervallo.

Casi senza distribuzione esatta della media (Chebyshev, senza ipotesi sulla forma della distribuzione oltre alla varianza esatta). Per una statistica `X` con valore atteso `E` e varianza `Var(X)` esatti, mediata su `K · P` valori indipendenti, si richiede

```
|media_(K·P)(X) − E| ≤ sqrt(m · Var(X) / (α · K · P))
```

che per la disuguaglianza di Chebyshev fallisce con probabilità ≤ `α/m`.

- **Stessa media, varianze `σ_f²` diverse.** `X = piano`, `E = Σσ_f²/n²`, `Var(X) = 2 tr((AΣ)²)/((n−1)² n²)`, con `A = I − 11ᵀ/n` e `Σ = diag(σ_f²)`. È la varianza esatta della forma quadratica gaussiana.
- **Flat con livelli diversi** (`μ_f = b + c_f g`, `|c_f/c̄ − 1| ≤ ε`). `X = piano`, `E = Σσ_f²/n² + g² Var_f(c)/n`, `Var(X) = [2 tr((AΣ)²) + 4 μᵀ A Σ A μ]/((n−1)² n²)`, varianza esatta della forma quadratica gaussiana non centrale.
- **Stima del termine dei livelli** (stessa fixture, `E[M] − offset_adu ≥ 8 sd(M)`, così `P(M ≤ offset_adu) < 10⁻¹⁵` e la troncatura `max(·, 0)` è trascurabile). `X = level_bias_estimate`, `E = ε² (a² + V)/(n − 1)` e `Var(X) = ε⁴ (4 a² V + 2 V²)/(n − 1)²`, con `a = E[M] − offset_adu` e `V = Var(M)`. Il test verifica la stima in valore atteso, non una maggiorazione per pixel. Verifica inoltre, sui parametri della fixture, che `E ≥ g² Var_f(c)/n`.
- **Fixture discriminante, fuori dalle ipotesi.** Una regione con `b − offset_adu = 2 ADU`, `c̄ g = −2 ADU`, livelli agli estremi `c̄(1 ± ε)` e `Var(M) ≤ 1 ADU²`. L'oracle calcola `B` e `E[level_bias_estimate]` in forma chiusa con la troncatura, `E[max(Y, 0)²] = (a² + V) Φ(a/√V) + a √V φ(a/√V)` con `Y ~ N(a, V)`, e verifica `B > E[level_bias_estimate]`: fuori da `nonnegative_signal` la stima non maggiora `B`. Il test verifica che il report elenchi `nonnegative_signal` fra le ipotesi non verificate e che `pixels_nonpositive_signal` sia maggiore di zero sulla regione. Verifica anche la media della stima su `K · P` valori contro il suo valore atteso esatto, con il limite di Chebyshev e `Var(X)` dal secondo e dal quarto momento di `max(Y, 0)`, calcolati dall'oracle in forma chiusa.

Gate: M2 (`plans/M2-residui.md`, sezione 6). La `.b` completa fixture e metodi in M3.b.

## 10. Failure mode, warning e provenance

Ogni condizione del §2 è uno stop controllato con codice non nullo, diagnosi e provenance dell'arresto, senza ricetta alternativa (NFR-FBK-01, NC-21). Un prerequisito perso dopo il preflight (digest cambiato, file assente) è `integrity` o `io`, con stop del ramo (ADR-16 §10). Avvisi: `saturation_not_evaluated`, `temperature_not_compared` (policy dichiarata), ipotesi non verificate. Il report del master contiene la provenienza di ogni frame usato.

## 11. Ottimizzazione

Accumulatori a blocchi di righe: per ogni blocco si leggono le righe corrispondenti degli `n` frame e si riducono media e `s²` del blocco, senza mai tenere in RAM la pila completa. Per il flat, un primo passaggio a blocchi calcola gli `m_f`, che sono mediane globali per frame. Dimensione del blocco e residenza dei frame sono scelte dal pianificatore nel preflight (ADR-06, D7) e non cambiano durante l'esecuzione. Un budget insufficiente è uno stop con il minimo fattibile (D8). Il master è un artefatto del DAG con digest; la cache è il riuso del nodo di ADR-05, con esito nella provenance.

## Test

I test sono T-M2R2-01…09 di `plans/M2-residui.md`, sezione R2, con le tolleranze del §9. T-M2R2-05 copre anche `master.flat_low_signal` e `master.variance_domain`: `combine = median`, `rejection = sigma_clip`, `flat_normalization = median_ratio`, un flat con livello oltre `flat_level_rel_tol`. Riesecuzioni: T-M2E-01…32 sui master del prodotto dove pertinente, T-M2C-11, T-M2F-02.

## Estensione richiesta ad ADR-17

Da applicare ad `ADR-17-varianza.md` in R2, prima del codice, con la sua review.

1. **Sorgente.** Ogni master del prodotto ha `variance_source = plane`, con `combine_method = mean` e `variance_estimator = empirical` nel descrittore. `noise_model` resta per i master esterni con `combine_method = mean`, come in ADR-17.a.
2. **Propagazione.** Invariata: `Var(Fc) = Var(F) + Var(DF)` con `Cov(F, DF) = 0`, perché il master flat non contiene il dark-flat (§6.2), e `Var(Lc) = Var(L) + Var(D)`.
3. **Errori ereditati.** Il piano di un master empirico è una stima casuale (§6, proprietà 2). Quello del flat ha in valore atteso la sovrastima `B(p)`, limitata da `B_max(p)` sotto le ipotesi del §6.2 (compresa `nonnegative_signal`) e stimata da `level_bias_estimate`. Il report di calibrazione li riporta dal descrittore e dal report del master, con le ipotesi, e non li presenta come garanzie per il singolo pixel.
4. **Test.** T-M2E-20 e T-M2E-21 si rieseguono con master del prodotto (`variance_source = plane`). In T-M2E-21 il riferimento della varianza di `C` usa la varianza vera dei master della simulazione. La soglia tiene conto della variabilità del piano empirico, derivata dalla sua distribuzione nota (§9) prima della misura.

Il campo `variance_estimator` del descrittore entra nell'estensione di ADR-16 già prevista in R2 (descrittore derivato dal nodo).

## Aggiunte di R2: punti che il contratto lascia aperti

Decisioni di applicazione, scritte prima del codice. Non cambiano metodi, dominio né tolleranze dei paragrafi precedenti.

- **Sorgenti dei valori.** `offset_adu` e i campi di compatibilità del §2 sono metadati dei frame, letti dall'header o dichiarati nella ricetta frame per frame (ADR-03, origine `declared`). `saturation_adu` è il parametro della ricetta: un frame con `saturation_adu` nell'header diverso dal parametro dà `incompatible`, `metadata.declared_conflict`; presente nell'header e assente dalla ricetta dà `invalid_input`, `master.param.saturation_adu`, perché la soglia del frame non si ignora. Fase assente: `metadata_missing`, `master.metadata.phase`; linearità o unità assenti: `master.metadata.linearity`, `master.metadata.unit`; fase non `raw`, non lineare o unità diversa da `adu`: `unsupported`, `master.input_state`.
- **Piano `VARIANCE` in un frame.** `unsupported`, `master.input_variance`: la varianza del master è empirica e un piano in ingresso resterebbe senza uso.
- **Parametri dei ruoli.** `flat_normalization`, `flat_level_rel_tol` e `flat_min_signal_adu` sono obbligatori per il flat e vietati per gli altri ruoli (`invalid_input`, `master.param.<nome>`). `combine`, `rejection`, `exposure_rel_tol`, `temperature_abs_tol_c` e `temperature_policy` sono obbligatori per tutti. `output_type` (`sample_type` del master) vale f32 se non dichiarato ed entra in nodo e report.
- **Frazione satura.** Per frame: campioni senza bit di invalidità e `≥ saturation_adu`, diviso il numero di pixel; stop se `> saturated_fraction_max`.
- **Metadati del master.** I campi di uguaglianza esatta del §2 sono comuni per costruzione. `exposure_s` e `temperature_c`, uguali entro tolleranza, sono quelli del frame di riferimento canonico, con origine `reference_frame` nel report. Gli altri metadati restano solo se uguali in tutti i frame, altrimenti si omettono e il report li elenca in `metadata_not_common`. `saturation_adu` del master è il parametro della ricetta. Fase `raw`, `linear`, unità `adu`.
- **Statistiche del report.** Per canale: mediana del piano `VARIANCE` sui pixel validi (`variance_plane_median`, il rumore del master); per il flat quelle del §6.2 e del §7. Una statistica non finita (per esempio `level_bias_estimate` in overflow) ferma il job con `numeric`, `master.report_nonfinite`; un canale senza pixel validi ha statistiche `null`. La stima per pixel resta disponibile nel risultato in memoria per il test statistico (T-M2R2-04).
- **Lettura e blocchi (§11).** Ogni frame si legge prima per intero, uno alla volta: digest dell'intero file sullo stesso descrittore, decodifica e validazione completa (ADR-03), frazione satura e `m_f`. Poi i blocchi di righe si leggono con `pread` sullo stesso descrittore, con la stessa conversione dei campioni e dei bit di `decode_fits`. Picco: un frame decodificato più i buffer dei blocchi e l'uscita, mai la pila. Prima della pubblicazione l'identità `fstat` deve essere invariata (`integrity`, `input.size_changed` o `input.modified`) e il digest dell'intero file, ricalcolato in streaming, uguale all'atteso (`integrity`, `input.digest_mismatch`). Limite dichiarato: i byte dei blocchi non hanno un digest proprio; li lega al file verificato il descrittore aperto con `ctime` invariato, e una modifica che riscrive `ctime` (privilegi di root) è fuori modello.
- **Piano del job (R2.r).** Worker, tile e capacità della coda dei task li sceglie `plan_resources` dalla `resource_config` del job (`master_job_options.resources`) con `planner_input.frame_count` = numero di frame del gruppo e il budget del pool: modello, formule e tabella in ADR-06, estensione R2.r. I `worker_count` e `tile_rows` espliciti della configurazione restano intatti. Se il piano non entra nel budget: `budget`, `sched.job_exceeds_budget`, con `minimum_feasible_bytes` (un worker, una riga se il tile è automatico), `picco`, `budget_bytes`, `shared_bytes`, `per_task_bytes`, `working_set_bytes` e `residenza` = `master` nel contesto, prima di ogni prenotazione; nessun ridimensionamento oltre a quello del piano, che avviene nel preflight. Una memoria del piano non rappresentabile in 64 bit (prodotti e somme controllati) dà lo stesso stop, con il valore `non rappresentabile` nel contesto: è un piano oltre ogni budget, come il picco non rappresentabile dello scheduler (ADR-06). Il piano scelto e, con lo snapshot del budget, la configurazione con le origini entrano nella provenance. Test: T-M2R2R-01…05 (ADR-06, estensione R2.r).
- **Statistiche del report non rappresentabili.** `level_bias_estimate`, il suo rapporto col piano e le loro mediane seguono la regola del §6.3: un prodotto o un quoziente con operandi non nulli che dà zero, un sottonormale o un non finito, o una mediana non finita, ferma il job con `numeric`, `master.report_nonfinite`.
- **Campi opzionali.** `roi_x`, `roi_y`, `readout_mode` e `row_order` presenti si confrontano con il primo frame canonico che li porta, anche quando il frame di riferimento non li ha; ogni frame che non li ha è elencato fra le ipotesi.

## Fuori da `.a`

In F-CAL-01.b (M3.b), con stop `unsupported`, `master.variance_domain` in `.a`:

- **`sigma_clip`** e ogni rejection. L'insieme dei frame tenuti dipende dai dati per pixel, e la varianza dei pixel validi dipende da quella selezione.
- **`median`.** La mediana di campioni interi resta su un reticolo. La sua varianza supera quella della mediana di gaussiane continue, `c_med(n) · Var(campione) / n`, di circa `1/(12 σ_med²)`, con `σ_med` la sua deviazione in ADU: +3.7 % a 1.5 ADU, +2.08 % a 2 ADU, +0.92 % a 3 ADU, e cresce rapidamente sotto 1.5 ADU. Sono valori dal calcolo esatto della distribuzione discreta, con `n` fino a 1001; lo strumento e la sua uscita sono conservati per M3.b. Per stare sotto il 2.08 % servirebbe un rumore di lettura di almeno 4.9 ADU con 9 frame e di almeno 16 ADU con 101, fuori dalla portata di quasi tutti i bias reali.
- **`median_ratio`.** Le scale sono stimate dagli stessi dati e sono variabili casuali. La varianza del master normalizzato contiene il termine `Var(E[M | scale])` e le covarianze fra scale, campioni e dark-flat, che `.a` non deriva.
- Media pesata, winsorized, set parziali, gruppi misti.

In M3.c: bias come master calibrante. In ADR-17.b: correlazioni dei master nello stacking.

## Estensione C-02.b: master completi (M3)

Stadio: C-02.b (registro riga 3431; aggregato C-02 riga 3429; testo §10.3 righe 1371-1385). Requisito: F-CAL-01.b (riga 3180; testo F-CAL-01 riga 1840; §21.5 riga 2684). Archi: F-CAL-01.b `requires_all` F-CAL-01.a, F-IO-03, C-02.b (righe 3718-3720); C-02.b `requires_all` C-02.a e `decided_by` ADR-09.b (righe 4114, 4115); M3.b `requires_all` F-CAL-01.b (riga 4238); F-CAL-03.b `requires_all` F-CAL-01.b (riga 3727). Dipendenze fra contratti: [ADR-16.b](ADR-16-stati-master.md#estensione-adr-16b-matrice-completa-della-calibrazione-m3) §b.2, §b.3, §b.7, §b.11; [ADR-17.b](ADR-17-varianza.md#estensione-adr-17b-modelli-di-covarianza-p0-m3) §b.1, §b.3, §b.6, §b.10 (regola I); [ADR-03.b](ADR-03-pixel-maschere.md) §b.6, §b.13, §b.14; [C-01.b](C-01-calibrazione-light.md) §2. Unità che lo implementa: M3.b.

Ogni clausola è una **regola** (vincolo del prodotto, verificato dal test indicato), un **fatto** (con derivazione o fonte) o un'**ipotesi** (dichiarata, con il test o il limite che la riguarda). La parte `.a` resta valida; i punti che cambiano sono in §b.13. C-02.a ammette solo `mean` con `none` (§1): mediana e `sigma_clip`, fermati in `.a`, sono definiti qui.

Fuori da C-02.b, ognuno con stop o motivo dichiarato:

- **Normalizzazione dei flat** (`median_ratio`) e, sul ruolo `flat`, ogni metodo diverso da `single` e `mean`: stop `unsupported`, `master.variance_domain`. Con livelli diversi i frame del flat non sono identicamente distribuiti, quindi il modello di §b.6 per i metodi robusti non vale. Con scale stimate dai dati, inoltre, la varianza del master contiene il termine delle scale, che qui non è derivato (C-02.a, «Fuori da `.a`»). Assegnati a F-CAL-01.b, unità `C-02.b-flat` (ADR-09.b §b.6).
- **Produzione di `Q`, di flat `corrected_unnormalized` o `calibrated`**: il master flat del prodotto è `raw`; `Q` si calcola nella calibrazione (ADR-16.b §b.5).
- **Master `bias_subtracted`** (`D'`, `DF'`). Il prodotto non li genera. Un `D'` ottenuto dal bias `B` del prodotto darebbe `(L − B) − (D − B) = L − D`, cioè la catena `light.dark`. Avrebbe inoltre i frame di `B` fra i propri, e ADR-17.b §b.3 toglierebbe la ricetta con `shared_input`. `D'` e `DF'` restano master esterni (ADR-16.b §b.2). La riga `master_create` della tabella delle operazioni resta quella di R1 (ADR-03.b §b.14).
- **Pesi stimati dai dati**: solo pesi dichiarati (§b.1).
- **Dark scalato e libreria di master**: F-CAL-05, P1, M6.f.
- **Mappa degli outlier come artefatto**: i conteggi stanno nel report (§b.7), la mappa per pixel solo nel risultato in memoria.
- **Scelta fra gruppi e fra master compatibili**: F-IO-03, ADR-15.
- **Formulazione unica delle soglie per famiglia**: ADR-09.b e D-09.b. Le soglie di §b.9 ne sono un ingresso.

### b.1 Problema matematico

Dato un insieme candidato di frame di un ruolo (classificato da F-IO-03), l'**analisi** lo divide in gruppi compatibili ed esclude, prima di ogni scelta, i frame non utilizzabili (§b.2). Per ogni gruppo propone un metodo scelto a priori secondo `n` e il ruolo (§b.4). Il master di un gruppo stima per pixel la grandezza comune dei frame, con il piano `VARIANCE` di §b.6.

Metodi, con `x_f` i campioni del pixel nell'ordine canonico dei frame (C-02.a §8) e `z_1 ≤ … ≤ z_n` gli stessi ordinati secondo l'ordine totale di §b.8:

| `combine` | `rejection` | Master `M` | `n` |
|---|---|---|---|
| `single` | `none` | `M = x_1` | 1 |
| `mean` | `none` | C-02.a §1 | ≥ 2 |
| `weighted_mean` | `none` | `M = (Σ_f w_f x_f) / W`, `W = Σ_f w_f`, pesi `w_f > 0` dichiarati per frame | ≥ 3 e `2 max_f w_f < W` |
| `median` | `none` | `n` dispari: `z_((n+1)/2)`; `n` pari: `(z_(n/2) + z_(n/2+1)) / 2` | 3 … 1024 |
| `winsorized` | `none` | `M = (Σ_i v_i) / n`, con `v_i = z_(g+1)` per `i ≤ g`, `v_i = z_(n−g)` per `i > n − g`, `v_i = z_i` altrimenti; `g = winsor_g` ≥ 1 dichiarato | `2g + 1` … 1024 |
| `mean` | `sigma_clip` | media dei campioni tenuti dalla procedura sotto | ≥ 3, non inerte |

`a_f = w_f / W` è il peso normalizzato. La condizione `2 max_f w_f < W` equivale a `max_f a_f < 1/2`; con `n = 2` e pesi positivi è impossibile, quindi `weighted_mean` richiede `n ≥ 3`. `winsorized` con `n ∈ {2g + 1, 2g + 2}` coincide con la mediana in aritmetica esatta (fatto, dalla definizione); in f64 i due calcoli di §b.6 possono differire per arrotondamento.

**`sigma_clip`.** Parametri dichiarati, senza default: `clip_kappa_low` > 0, `clip_kappa_high` > 0, `clip_max_iter` ≥ 1 intero, `clip_min_kept` intero in [2, n]. Per pixel, con `S` l'insieme dei frame tenuti (all'inizio tutti gli `n`), per `t = 1 … clip_max_iter`:

1. `m = |S|`, `μ = (Σ_{f∈S} x_f) / m`, `s = sqrt(Σ_{f∈S} (x_f − μ)² / (m − 1))`, nell'ordine canonico (§b.6);
2. `R` = i frame di `S` con `x_f − μ > clip_kappa_high · s` oppure `μ − x_f > clip_kappa_low · s` (disuguaglianze strette);
3. se `R` è vuoto la procedura termina; altrimenti `S ← S \ R`, e se `|S| < clip_min_kept` il pixel è `support_incomplete` (§b.7) e la procedura termina.

Il master è `μ` dell'insieme `K` finale, con `m = |K|`. Con `s = 0` nessun campione è rifiutato.

Fatto (disuguaglianza di Samuelson, §b.5; verificata in aritmetica razionale, `tmp/c02b_algebra.py`): per `m` campioni `|x_f − μ| ≤ s (m − 1)/√m`, con uguaglianza quando `m − 1` campioni sono uguali. Quindi alla prima iterazione un campione è rifiutabile verso l'alto solo se `(n − 1)² > clip_kappa_high² · n`, e verso il basso solo se `(n − 1)² > clip_kappa_low² · n`. Regola: se nessuna delle due disuguaglianze vale, confrontate in aritmetica esatta sui valori f64 dichiarati, la procedura non può rifiutare nulla, e la ricetta si ferma con `invalid_input`, `master.param.sigma_clip_inert` (`n`, i due `κ` e il limite nel contesto). Esempio: con `κ = 3` il primo `n` ammesso è 11.

Il prodotto non cambia metodo, parametri o insieme dei frame dopo un esito: un pixel `support_incomplete` resta invalido, non passa a un altro metodo (NC-21).

### b.2 Dominio, unità e prerequisiti

Restano i prerequisiti di C-02.a §2 e delle «Aggiunte di R2», con le modifiche di §b.13 (codici di fase e linearità). Domini per metodo e ruolo:

| Metodo | `bias`, `dark`, `dark_flat` | `flat` |
|---|---|---|
| `single`, `mean` + `none` | ammesso | ammesso |
| `weighted_mean`, `median`, `winsorized`, `mean` + `sigma_clip` | ammesso nel dominio di §b.1 | `unsupported`, `master.variance_domain` |

Una coppia (`combine`, `rejection`) assente dalla tabella di §b.1, un `n` fuori dal dominio del metodo o un ruolo non ammesso: `unsupported`, `master.variance_domain`, con metodo, `n` e ruolo nel contesto. Il limite `n ≤ 1024` dei metodi robusti è il dominio entro cui `κ` è verificato (§b.6, T-M3B-C02-05).

**Analisi del set candidato (prima della scelta).** Ingressi: i frame candidati di un ruolo e i parametri d'analisi dichiarati. Parametri d'analisi: `exposure_rel_tol`, `temperature_abs_tol_c`, `temperature_policy` (ADR-16.a §2), `saturation_adu` e `saturated_fraction_max` (C-02.a §2), per il flat `flat_min_signal_adu` e `flat_level_rel_tol`. Per i frame RAW servono anche `decoder_black_abs_tol_adu` (ADR-16.b §b.7) e, per il flat e per `single`, `offset_adu`, dichiarato una volta per l'insieme. L'analisi legge header, metadati e stato del decoder, e legge i campioni solo per la frazione satura e per i livelli `m_(f,c)` del flat. L'uscita è una funzione pura degli ingressi. Comprende:

1. **Esclusioni per frame**, lista chiusa, ognuna con i valori nel report: `layout`, `metadata_missing.phase`, `phase`, `metadata_missing.linearity`, `linearity`, `metadata_missing.unit`, `unit`, `pedestal`, `input_variance`, `metadata_missing.<campo>` (un campo di compatibilità di C-02.a §2), `imagetyp_conflict`, `duplicate_digest` (escluse tutte le copie tranne la prima nell'ordine canonico), `decode_failed` (con il codice di ADR-08.b §2.5), `saturation_conflict`, `saturated_fraction`, `flat_low_signal`. `saturation_conflict`: la soglia di saturazione del frame, dall'header o dal decoder (ADR-08.b §2.6), differisce da `saturation_adu` dell'analisi; nel preflight è `incompatible`, `metadata.declared_conflict`, come in `.a` («Aggiunte di R2»). Una soglia del frame presente con `saturation_adu` assente ferma l'analisi con `invalid_input`, `master.param.saturation_adu`, come in `.a`. Così tutti i frame di un gruppo hanno la soglia del parametro. Le condizioni sono quelle che in C-02.a §2 fermano il job. Nell'analisi escludono il frame; nel preflight del nodo fermano come in `.a`.
2. **Gruppi**. Due frame stanno nello stesso gruppo solo se hanno uguale chiave esatta. La chiave contiene: larghezza, altezza, `binning`, CFA (pattern e offset), `camera`, `gain`, `filter` (solo flat), e `roi_x`, `roi_y`, `readout_mode`, `row_order`, con «assente» come valore. Contiene poi la presenza dello stato del decoder (frame RAW e FITS non stanno mai nello stesso gruppo). Per i FITS contiene `offset_adu`; per i RAW, i campi strutturali di ADR-16.b §b.7 (`raw_width`, `raw_height`, `window`, `cfa_color_index`, `black_pattern_rows`, `black_pattern_cols`, `linearization`, `curve_sha256`). Dentro una classe di chiave, i gruppi si formano per tolleranza in modo deterministico. Il riferimento è il frame restante con digest minimo. Il gruppo comprende i frame restanti che, rispetto al riferimento, hanno `exposure_s` entro `exposure_rel_tol`, `temperature_c` entro `temperature_abs_tol_c` (se `temperature_policy = compare`) e, per i RAW, nero effettivo `b(r, c)` entro `decoder_black_abs_tol_adu` (calcolato come in ADR-16.b §b.7). I frame del gruppo escono dai restanti, e si ripete. Fatto, per costruzione: il riferimento è il primo frame canonico del suo gruppo, quindi il gruppo supera i confronti a coppie col riferimento canonico del preflight (C-02.a §2) con gli stessi parametri. La chiave dell'analisi è più stretta del preflight: un campo opzionale assente separa i gruppi.
3. **Livelli del flat**, per gruppo: `m_(f,c)` per canale (§b.6). Se un frame viola la tolleranza in un canale, il gruppo ha stato `flat_level`, con gli `m_(f,c)` nel report. Nessun frame è escluso per livello, perché `m̄_c` dipende dall'insieme scelto: un sottoinsieme lo sceglie l'utente.
4. **Proposta** per ogni gruppo secondo §b.4.

Un candidato senza alcun frame utilizzabile ferma il job con `invalid_input`, `master.no_usable_frames` e le esclusioni nel contesto. Ogni gruppo è un master distinto: «gruppi incompatibili separati» significa un nodo per gruppo, mai frame di gruppi diversi nello stesso nodo. Fatto, per costruzione: i gruppi di un'analisi sono disgiunti, quindi i master proposti da un'analisi non hanno frame comuni (ADR-17.b §b.3, §b.6).

**Selezione ed esecuzione.** Il nodo `master.combine` contiene esattamente i frame scelti (un gruppo o un suo sottoinsieme, scelto dall'utente), in ordine canonico, con metodo, parametri, pesi e, per i RAW, `frame_decoder_states` (ADR-16.b §b.7). Il preflight riesegue i controlli di C-02.a §2 e di questo paragrafo sui frame del nodo e non esclude nulla. Per i RAW confronta i campi strutturali per uguaglianza e il nero effettivo entro `decoder_black_abs_tol_adu` col riferimento canonico: `incompatible`, `master.mismatch.<campo>` e `master.mismatch.decoder_black`. Un frame cambiato dopo la selezione è `integrity` o `io` (C-02.a §10); uno stato del decoder diverso è `master.decoder_state_changed` (ADR-16.b §b.7).

**RAW.** `offset_adu` è il valore dichiarato nella ricetta, unico per il nodo: non si confronta fra frame, perché il confronto dell'offset è quello del nero (ADR-16.b §b.7). Serve solo a `m_(f,c)` del flat e al modello di rumore di `single`. Il nero del decoder non si sottrae (C-01.b §1).

**Canali CFA.** Per un frame CFA i canali sono le quattro posizioni del motivo, con G1 e G2 separati come in ADR-16.a §6. Livelli del flat, statistiche e report sono per canale.

### b.3 Ipotesi e limiti

- `mean`, `weighted_mean`: frame indipendenti, stessa media per pixel (`stationary_mean`), varianze anche diverse; nessuna ipotesi sulla forma della distribuzione (§b.6).
- `median`, `winsorized`: ipotesi `iid_gaussian` (frame indipendenti con la stessa distribuzione gaussiana per pixel) e `continuous_samples` (campioni non quantizzati). Il piano è non distorto solo sotto entrambe (§b.6). Su campioni interi la varianza della mediana si scosta da quella del caso continuo (C-02.a, «Fuori da `.a`»), di un termine che dipende da rumore, numerosità e parte frazionaria della media e può avere entrambi i segni. Il piano non lo contiene: limite misurato da T-M3B-C02-07, non corretto. Con outlier, `s²` cresce e il piano con lui: è un comportamento atteso, non un limite dimostrato.
- `mean` + `sigma_clip`: piano a selezione congelata, approssimazione (§b.6). Errore non derivato, misurato da T-M3B-C02-06 sotto `iid_gaussian`.
- Flat (`mean`): ipotesi di C-02.a §6.2 per canale, con `c_(f,c)` il livello del frame nel canale (`level_proportional` per canale).
- `single`: il piano viene dal modello di rumore dichiarato (ADR-17.a), con lo stimatore plug-in `observed_clamped`. È un'ipotesi dell'utente.
- Restano le ipotesi di C-02.a §3. Con `n` piccolo la mediana non identifica gli outlier: ne limita l'effetto entro il punto di rottura `⌊(n − 1)/2⌋`.

### b.4 Alternative e scelta

Confrontati: media, media pesata, mediana, winsorized, sigma clipping (§10.3 campo 4). Ciascuno resta disponibile come scelta esperta nel suo dominio. **Proposta a priori**, cioè una regola su `n` e ruolo applicata prima di guardare i campioni:

| `n` del gruppo | `bias`, `dark`, `dark_flat` | `flat` |
|---|---|---|
| 1 | `single`, se `conversion_gain_e_per_adu`, `read_noise_adu` e `offset_adu` sono già dichiarati nel progetto; altrimenti nessuna proposta (`noise_model_not_declared`) | come a sinistra |
| 2 | `mean` + `none` | `mean` + `none`; con stato `flat_level` nessuna proposta (`flat_level`) |
| 3 … 1024 | `median` + `none` | `mean` + `none`; con stato `flat_level` nessuna proposta (`flat_level`) |
| > 1024 | `mean` + `none` | `mean` + `none` (o nessuna, con `flat_level`) |

Motivi. La proposta non inventa parametri, come il default di D-16: `weighted_mean`, `winsorized` e `sigma_clip` ne richiedono e non sono mai proposti. Con `n = 1` e `n = 2` non c'è rejection. Con `n ≥ 3` la mediana resiste a `⌊(n − 1)/2⌋` campioni anomali per pixel (raggi cosmici nei dark) senza stimare una scala. Il costo dichiarato è l'efficienza: sotto `iid_gaussian` la sua varianza è `κ_med(n) σ²` contro `σ²/n`. Con `n = 3`, `3 κ_med(3) = 3 − 3√3/π ≈ 1.346` (fatto, §b.6). Per `n → ∞` il rapporto tende a `π/2` (risultato asintotico classico, §b.5, non usato come valore). Il flat resta alla media, perché i metodi robusti sui flat sono fuori dominio (§b.2). Scartati come proposta: `sigma_clip`, che richiede `κ` e per `κ = 3` non rifiuta nulla sotto `n = 11` (§b.1); la media per i dark, che non resiste ai raggi cosmici. La proposta è una regola, non una misura di qualità. La scelta fra proposta e alternative esperte si registra nel report.

### b.5 Riferimenti indipendenti e stato

Nessuno dei testi è stato riletto in questa unità. Il contratto non dipende da un loro valore numerico. Le derivazioni usate sono in §b.6, verificate in aritmetica razionale o per quadratura in `tmp/c02b_algebra.py`. Le stesse quantità sono verificate dai test di §b.9.

- Stima robusta di posizione: P. J. Huber, «Robust estimation of a location parameter», *Annals of Mathematical Statistics* 35(1), 1964; F. R. Hampel, E. M. Ronchetti, P. J. Rousseeuw, W. A. Stahel, *Robust Statistics: The Approach Based on Influence Functions*, Wiley 1986; P. J. Huber, E. M. Ronchetti, *Robust Statistics*, 2ª ed., Wiley 2009 (punto di rottura, efficienza della mediana).
- Media winsorized: W. J. Dixon, «Simplified estimation from censored normal samples», *Annals of Mathematical Statistics* 31(2), 1960; J. W. Tukey, D. H. McLaughlin, «Less vulnerable confidence and significance procedures for location based on a single sample: trimming/winsorization 1», *Sankhyā A* 25(3), 1963.
- Statistiche d'ordine: H. A. David, H. N. Nagaraja, *Order Statistics*, 3ª ed., Wiley 2003 (densità congiunta, distribuzione condizionata degli interni, momenti delle statistiche d'ordine normali).
- Disuguaglianza di Samuelson: P. A. Samuelson, «How deviant can you be?», *Journal of the American Statistical Association* 63(324), 1968.
- Rumore e combinazione dei frame CCD: S. B. Howell, *Handbook of CCD Astronomy*, 2ª ed., Cambridge University Press 2006; M. V. Newberry, «Signal-to-noise considerations for sky-subtracted CCD data», *PASP* 103, 1991.
- Quantizzazione: W. F. Sheppard, «On the calculation of the most probable values of frequency-constants, for data arranged according to equidistant division of a scale», *Proceedings of the London Mathematical Society* 29, 1898; usato solo per il limite di §b.3.
- Varianza campionaria, aritmetica, χ²: come C-02.a §5. Aritmetica degli intervalli: ADR-17.b §b.10 (regola I).

Nessuna derivazione da software esistente (DEC-02, NC-15).

### b.6 Numerica, stabilità e varianza

Accumulo in f64, un solo arrotondamento al `sample_type` del master. Il piano `VARIANCE` dipende dal metodo; `variance_estimator` lo nomina nel descrittore:

| Metodo | `variance_estimator` | Piano `V` |
|---|---|---|
| `single` | `noise_model` | `read_noise_adu² + max(x_1 − offset_adu, 0) / conversion_gain_e_per_adu` (ADR-17.a con `n = 1`) |
| `mean` | `empirical` | `s²/n` (C-02.a §6) |
| `weighted_mean` | `empirical_weighted` | `Σ_f c_f (x_f − M)²` |
| `median`, `winsorized` | `gaussian_reference` | `κ · s²`, con `s²` di C-02.a §6 su tutti gli `n` campioni |
| `mean` + `sigma_clip` | `frozen_selection` | `(Σ_{f∈K} (x_f − M)² / (m − 1)) / m` |

**Media pesata.** `T = Σ_g a_g² / (1 − 2a_g)` e `c_f = a_f² / ((1 − 2a_f)(1 + T))`. Fatto: con pesi dichiarati (quindi indipendenti dai campioni), per frame indipendenti con la stessa media e varianze `σ_f²` qualunque, `E[V] = Var(M) = Σ_f a_f² σ_f²` in aritmetica esatta. Derivazione: con `x_f = μ + e_f` e `r_f = x_f − M = e_f − Σ_g a_g e_g`, si ha `E[r_f²] = σ_f² (1 − 2a_f) + Σ_g a_g² σ_g²`. Quindi `E[Σ_f c_f r_f²] = Σ_g σ_g² [c_g (1 − 2a_g) + S a_g²]` con `S = Σ_f c_f`, e il valore cercato richiede `c_g (1 − 2a_g) + S a_g² = a_g²` per ogni `g`. La soluzione è `c_g = a_g² (1 − S)/(1 − 2a_g)` con `S = T/(1 + T)`, cioè la formula sopra. Con `max a_f < 1/2` si ha `T > 0` e ogni `c_f > 0`, quindi `V ≥ 0`. Con un `a_f > 1/2` alcuni `c_f` sono negativi: con pesi `(3, 1, 1)`, `c_2 = c_3 = −1/10`, e il piano può essere negativo; con `a_f = 1/2` la formula non è definita. Di qui il dominio di §b.1. Con pesi uguali `c_f = 1/(n(n − 1))`, cioè `s²/n` di `.a`. Verifica: identità e segni su 3000 casi razionali con seed, `tmp/c02b_algebra.py`.

**Metodi robusti.** `κ = κ_T(n, g) = Var(T(Z_1, …, Z_n))` con `Z` iid `N(0, 1)` e `T` lo stimatore (mediana o winsorized). Fatto: `T` è equivariante per posizione e scala, `T(μ + σZ) = μ + σ T(Z)`, quindi sotto `iid_gaussian` `Var(M) = κ σ²`. Poiché `E[s²] = σ²` per campioni iid, `E[V] = Var(M)`. Valori noti: `κ_med(2) = 1/2`; `κ_med(3) = 1 − √3/π ≈ 0.448671`, ricavata dalla densità della statistica d'ordine centrale e confermata per quadratura con errore < 10⁻¹⁰. Fuori da `iid_gaussian` non c'è garanzia (§b.3).

Regola sul calcolo di `κ`: il metodo è libero, ma l'errore relativo deve essere ≤ 10⁻⁹ per ogni (metodo, `n`, `g`) del dominio (T-M3B-C02-05). Il valore si calcola una volta nel preflight ed entra nel report. Se l'implementazione non raggiunge l'accuratezza secondo il proprio controllo d'errore: `numeric`, `master.kappa_accuracy`, prima di ogni lettura di campioni. Fatto utilizzabile per il calcolo (David e Nagaraja, distribuzione condizionata): dati `Z_(g+1) = a` e `Z_(n−g) = b`, i `n − 2g − 2` campioni interni sono iid gaussiani troncati ad `(a, b)`. Quindi `κ_win` è un integrale doppio di momenti troncati in forma chiusa, e la mediana a conteggio pari è il caso `n = 2g + 2`. `tmp/c02b_algebra.py` lo confronta con un Monte Carlo per `(n, g)` = (5, 1), (7, 2), (9, 1).

**Selezione congelata.** Il piano tratta `K` come fissato: è la riga a pesi congelati di ADR-17.b §b.1 (pesi `1/m` su `K`), con il codice `variance.fixed_weight_approximation` (§b.7). Non è uno stimatore non distorto: la selezione dipende dai campioni, e i tenuti hanno dispersione minore della popolazione. L'errore non è derivato. T-M3B-C02-06 lo misura sotto `iid_gaussian` e il report lo dichiara come approssimazione, senza garanzia.

**Flat.** Livelli per canale: `m_(f,c)` è la mediana di `(x_f − offset_adu)` sui pixel del canale `c` senza bit di invalidità (stessa regola di C-02.a §1), `m̄_c` la media sugli `n` frame. La tolleranza `|m_(f,c)/m̄_c − 1| ≤ flat_level_rel_tol` vale per ogni canale. Il limite e la stima `level_bias_estimate` di C-02.a §6.2 valgono canale per canale, con `c̄` e `ε` del canale: la derivazione è la stessa, ristretta ai pixel del canale. Per un flat mono c'è un solo canale, come in `.a`.

**Ordine delle operazioni**, fissato per la regola I (ADR-17.b §b.10) e per il determinismo:

- `weighted_mean`. Costanti del nodo, calcolate nel preflight: `W` (somma nell'ordine canonico), `a_f = w_f / W`, `T` (somma nell'ordine canonico di `(a_f · a_f) / (1 − 2 · a_f)`), `c_f = (a_f · a_f) / ((1 − 2 · a_f) · (1 + T))`. Per pixel: `M = (Σ_f w_f · x_f) / W`, poi `V = Σ_f c_f · ((x_f − M) · (x_f − M))`, entrambe nell'ordine canonico. Il dominio `2 max w < W` si confronta in aritmetica esatta sui `w_f` f64.
- `median`, `winsorized`: ordinamento secondo §b.8. Mediana a conteggio pari: `(z_k + z_(k+1)) / 2`, somma e divisione in quest'ordine. Winsorized: `Σ_i v_i` per `i` crescente, poi `/ n`. Piano: `s²` come C-02.a §6, poi `κ · s²`.
- `sigma_clip`: §b.1. `μ`, `Σ (x − μ)²`, divisione per `m − 1`, `sqrt` (IEEE, arrotondamento corretto), soglie `κ · s`, confronti fra `x_f − μ` (e `μ − x_f`) e la soglia. Piano: `Σ_{f∈K} (x_f − M)²`, `/ (m − 1)`, `/ m`.
- `single`: `x_1 − offset_adu`, `max(·, 0)`, `/ conversion_gain_e_per_adu`, `read_noise_adu²`, somma.

Rappresentabilità: la regola di C-02.a §6.3, applicata a ogni nuova operazione. Prodotti e quozienti con operandi non nulli (`w_f · x_f`, `/ W`, `c_f · q`, `κ · s²`, `κ · s`, `/ m`) che danno zero o un subnormale rendono il pixel `result_nonfinite`. Lo stesso vale per somme di termini non negativi non tutti nulli con quel risultato. Somme e differenze con segno (`Σ w_f x_f`, `x_f − M`, `Σ v_i`, `x_f − μ`) sono esatte o valide come in `.a`. `sqrt(0) = 0` è valido. Nelle costanti del nodo, `a_f`, `T` o `c_f` non normali o non finiti fermano con `invalid_input`, `master.param.weights` e il valore nel contesto.

Errore d'arrotondamento: per `mean` resta il bound di C-02.a §6.3. Per gli altri metodi la tolleranza dei test è quella della regola I. Nel confronto dei piani `gaussian_reference` l'oracle prende come ingresso puntuale il `κ` riportato dal prodotto: l'aritmetica si verifica qui, l'accuratezza di `κ` in T-M3B-C02-05. Vale la regola I di ADR-17.b §b.10, con l'oracle che riesegue la sequenza di questo paragrafo in aritmetica degli intervalli. Per una mediana prende la mediana degli estremi, che vale anche per la selezione di `winsorized` perché ogni `v_i` è una statistica d'ordine, monotona nei campioni. Per `sigma_clip` l'oracle calcola l'intervallo di ogni confronto. Un caso in cui l'intervallo di `x_f − μ` e quello della soglia si sovrappongono esce dalla fixture prima della misura: l'esito del confronto non è determinato dal bound (§b.9).

### b.7 Maschere, metadati e bordi

- Campioni invalidi e saturi: come C-02.a §7 per ogni metodo. Un campione invalido rende invalido il pixel del master; un pixel valido usa tutti gli `n` frame prima della rejection. Un campione rifiutato da `sigma_clip` non è un'invalidità e non accende bit.
- `sigma_clip` con meno di `clip_min_kept` tenuti: bit `support_incomplete` (ADR-03.b §b.6), campione e varianza NaN.
- Capability (ADR-03.b §b.13): il master è un'uscita d'operatore, con `measure` uguale a `merge_measure` dei frame più i codici propri. Con `sigma_clip` aggiunge `variance.fixed_weight_approximation`; gli altri metodi non aggiungono codici. Per `gaussian_reference` e `noise_model`, che non hanno un codice, l'ipotesi sta in `variance_estimator` del descrittore e nel report (limite in §b.14).
- Descrittore derivato dal nodo (ADR-16 «Aggiunte di R2», ADR-16.b §b.7, §b.11). Ruolo e stati come in R2 (`dark` e `dark_flat` `bias_included`, `unscaled`; `flat` `raw`). Poi `combine_count = n` e `combine_method` ∈ {`single`, `mean`, `weighted_mean`, `median`, `winsorized`}. Poi `rejection` ∈ {`none`, `sigma_clip`}, `variance_estimator` come in §b.6 e `frame_decoder_states` per i master da RAW. Gli ingressi frame del nodo sono l'insieme dei frame sorgente, che ADR-17.b §b.3 e §b.6 confrontano per la disgiunzione: i master del prodotto entrano nelle covarianze per riferimento (`covariance_sources`), senza piani aggiuntivi.
- Metadati del master: C-02.a, «Aggiunte di R2». Per i RAW, `offset_adu` è il valore dichiarato, con origine `declared`.
- **Report del master.** Contiene i campi di C-02.a §7 e, in più:
  - **Analisi**: frame candidati, ciascuno `used` (con il gruppo) o `excluded` (con il motivo di §b.2 e i valori); gruppi con chiave, riferimento, membri e stato; proposta per gruppo e motivo dell'eventuale assenza; se il metodo scelto coincide con la proposta.
  - **Rumore**: per canale, `variance_plane_median` (C-02.a). Per frame e canale, sui pixel validi del master, `residual_mean_adu = Σ r / P` e `residual_rms_adu = sqrt(Σ r² / P)`, con `r = x_f − M`: somma per riga in ordine di colonna, poi somma delle righe in ordine crescente, così il risultato non dipende dai blocchi. Sono diagnostiche deterministiche, non stime di `σ_f`.
  - **Artefatti**: per frame, `rejected_samples`, `saturated_samples`, `invalid_samples`; per canale, i conteggi per bit (compreso `support_incomplete`) e `pixels_with_rejection`.
  - **Stato**: descrittore, `κ` (metodi robusti), costanti `a_f` e `c_f` (media pesata), parametri del metodo.
  - **Ipotesi**: quelle di §b.3 per il metodo, dichiarate non verificate.
  - **Statistiche di `.a` per metodo.** `variance_cv_gaussian_homoscedastic` vale `sqrt(2/(n − 1))` per `mean`, `median` e `winsorized`: per questi ultimi `V = κ s²`, con lo stesso coefficiente di variazione di `s²` sotto `iid_gaussian`. È `null` con motivo per `single` (`single_frame`), `weighted_mean` (`not_derived`) e `sigma_clip` (`not_derived`). `level_bias_estimate`, il suo rapporto col piano e `level_bias_ratio_null` valgono per il flat `mean` come in C-02.a §6.2, per canale; per il flat `single` sono `null` con motivo `single_frame`, senza divisione per `n − 1` e senza `master.report_nonfinite`. `pixels_nonpositive_signal` vale per ogni flat.

### b.8 Determinismo

- Ordine canonico dei frame: C-02.a §8. Ogni somma per pixel segue quell'ordine.
- Ordinamento per pixel: ordine totale sulle coppie (valore, indice canonico del frame). Fatto: il valore di una statistica d'ordine non dipende da come si rompono i pareggi fra valori uguali, salvo il segno di uno zero (`−0 == +0`). L'ordine totale fissa anche quello: a parità di valore viene prima il frame canonico minore. Nessun NaN entra nell'ordinamento (§b.7).
- `sigma_clip`: decisioni e master dipendono solo dalle operazioni IEEE di §b.6, nello stesso ordine. Con stessa ricetta, stessi input e stesso backend il master, il piano, la maschera e il report sono bit-identici con qualunque `tile_rows` e numero di worker, a blocchi o in memoria. Il calcolo è per pixel e non attraversa i blocchi. Fra backend diversi (ADR-07) l'identità non è richiesta: una soglia vicina può cambiare un'appartenenza.
- Analisi: funzione pura degli ingressi, indipendente dall'ordine in cui i candidati arrivano (gruppi e riferimenti per digest).
- Riproducibilità dal progetto: il nodo contiene frame, metodo, parametri, pesi e stati del decoder. Ricostruito dopo la riapertura dello store, dà la stessa chiave e lo stesso artefatto (ADR-05). `κ` è calcolato dallo stesso algoritmo. Nessun seed: nessun componente stocastico.

### b.9 Fixture, metriche, soglia e gate

Fixture: `FIX-SYN-CAL-08` (bias, dark, dark-flat e flat con `B`, `d`, `F` e rumore noti, outlier iniettati in posizioni note), con frame RAW sintetici di M3.a (DNG scritti con libtiff) per i gruppi da RAW; dato reale PUB-LFC-01 (bias 6+1, dark e flat con `N = 3`, mono). Oracle scritti nel test, indipendenti dall'implementazione. Soglie fissate qui, prima di ogni misura. Statistica come C-02.a §9: `α = 10⁻³` per test, Bonferroni sulle `m` statistiche del test, quantili χ² esatti (gamma incompleta regolarizzata, bisezione, errore relativo ≤ 10⁻¹⁰), limite di Chebyshev `|media_(K·P)(X) − E| ≤ sqrt(m · Var(X) / (α · K · P))`. Il riferimento è la varianza vera della simulazione. `K`, `P`, seed e parametri sono fissati prima dell'esecuzione in un protocollo preregistrato, committato prima della misura e citato dal record del test (ADR-09.b §b.4).

Classi delle soglie ([ADR-09.b](ADR-09-algoritmi.md#estensione-adr-09b-algoritmi-p0-di-m3) §b.3): E per T-M3B-C02-01, 13 e per l'oracle di 11; E con intervalli per 02; S come controlli di coerenza per 03 e 04 (le proprietà che verificano sono fatti derivati in §b.6, e l'aritmetica la verificano esattamente 01 e 05), con la varianza di 04(b) che dipende da `μ_4` stimato (§b.14); P per 05 (accuratezza richiesta 10⁻⁹, errore dell'oracle di quadratura stimato, §b.14); C per 06 e 07; B per 08, 09, 10, 12, 14, 15; R con oracle dagli header per 16.

Margine deterministico dei criteri S di 03 e 04 (ADR-09.b §b.3). Per ogni realizzazione `k` l'oracle calcola con la regola I il bound `τ_k` del valore del prodotto confrontato, piano o master. L'intervallo di accettazione si allarga di:

- `max_k τ_k` per le medie dei piani di 03(a) e 04(a), oltre a `ρ_c · E` di 03(a); fatto: una media dista dalla media dei valori esatti al più del massimo scarto;
- `sqrt(K/(K − 1)) · max_k τ_k` per la deviazione standard campionaria dei master di 03(b), confrontata come nel criterio (B) di ADR-17.b §b.12; fatto: `|sd_K(x̂) − sd_K(x)| ≤ sd_K(x̂ − x) ≤ sqrt(K/(K − 1)) · max_k |x̂_k − x_k|`;
- `max_k τ_k (2|M̂_k − μ| + τ_k)` per la media di `(M − μ)²` di 04(b); fatto: `|(M̂ − μ)² − (M − μ)²| = |M̂ − M| · |M̂ + M − 2μ|`.

Gate della slice M3.b: master riproducibili bit-esatti dal progetto in ordine canonico (T-M3B-C02-10), report completo (T-M3B-C02-11), frame incompatibile escluso prima della scelta (T-M3B-C02-08). I test sono in §b.12.

### b.10 Failure mode, warning e provenance

Restano gli stop di C-02.a §2 e §10, con i codici di fase e linearità di §b.13. Aggiunti:

| Condizione | Categoria | Codice |
|---|---|---|
| Coppia (`combine`, `rejection`) fuori tabella, `n` fuori dominio, metodo non ammesso per il ruolo, `max a_f ≥ 1/2` | `unsupported` | `master.variance_domain` |
| Pesi assenti, in numero diverso dai frame, non finiti o ≤ 0; costanti dei pesi non rappresentabili; pesi con un metodo diverso da `weighted_mean` | `invalid_input` | `master.param.weights` |
| `winsor_g` assente, non intero o < 1; parametri di `sigma_clip` assenti o fuori dominio; parametri di un metodo non scelto presenti | `invalid_input` | `master.param.<nome>` |
| `sigma_clip` inerte (§b.1) | `invalid_input` | `master.param.sigma_clip_inert` |
| `single` senza parametri del modello di rumore | `metadata_missing` | `var.source.<campo>` (ADR-17.a) |
| `κ` non calcolabile con l'accuratezza richiesta | `numeric` | `master.kappa_accuracy` |
| Nessun frame utilizzabile nell'analisi | `invalid_input` | `master.no_usable_frames` |
| Frame RAW con campo strutturale o nero diverso dal riferimento (preflight) | `incompatible` | `master.mismatch.<campo>`, `master.mismatch.decoder_black` |
| `offset_adu` assente con frame RAW nel flat o in `single` | `metadata_missing` | `master.metadata.offset_adu` |

Avvisi: `gaussian_reference_not_verified` (metodi robusti), `frozen_selection_approximation` (`sigma_clip`), `noise_model_variance` (`single`), `proposal_not_followed` (metodo diverso dalla proposta), più quelli di `.a`. La provenance del nodo contiene l'esito dell'analisi (esclusioni e gruppi) e le costanti del preflight (`κ`, `a_f`, `c_f`). Ogni stop porta i valori che lo motivano nel contesto e nessun percorso alternativo parte (NFR-FBK-01, NC-21).

### b.11 Ottimizzazione

Restano i blocchi di righe di C-02.a §11. Per pixel l'ordinamento lavora su `n` valori in un buffer del task; la selezione di `sigma_clip` usa `n` indicatori; `κ` e le costanti dei pesi si calcolano una volta nel preflight. Gli accumulatori del report per frame e canale (`Σ r`, `Σ r²`, conteggi) tengono un parziale per riga, sommato alla fine nell'ordine delle righe (§b.7). Il modello di memoria di ADR-06 (estensione R2.r) deve contare questi buffer: l'estensione si aggiorna in M3.b, con il suo test, prima del codice (§b.13). Nessuna ottimizzazione cambia l'ordine delle operazioni di §b.6.

### b.12 Test pianificati

Criteri fissati qui, prima di ogni misura.

| ID | Verifica | Criterio |
|---|---|---|
| T-M3B-C02-01 | Valori del master e del piano per ogni metodo: casi dyadic scelti perché ogni operazione sia esatta (pesi con `W` potenza di due, `n` e `m` potenze di due dove dividono, mediana a conteggio dispari e pari, winsorized con `g` = 1 e `⌊(n−1)/2⌋`); casi non dyadic; pareggi con `−0` e `+0` | 0 sui casi in cui l'oracle verifica esatta ogni operazione della sequenza di §b.6 (per i piani `gaussian_reference` col `κ` riportato come ingresso); negli altri, regola I (ADR-17.b §b.10) più mezzo ulp f32 se l'uscita è f32. Pareggi: bit dell'elemento atteso secondo l'ordine totale di §b.8 |
| T-M3B-C02-02 | `sigma_clip` su `FIX-SYN-CAL-08` con outlier in posizioni note, `n` ∈ {11, 15, 31}, una e più iterazioni, soglie asimmetriche; `clip_min_kept` raggiunto; casi inerti `n = 10`, `κ = 3` e `n = 4`, `κ = 1.5` (uguaglianza di Samuelson); `n = 11`, `κ = 3` ammesso | Appartenenza esatta all'insieme rifiutato per pixel e conteggi per frame uguali all'oracle, che calcola i confronti in aritmetica degli intervalli; i casi con intervalli sovrapposti sono rimossi prima della misura e contati nel record. `support_incomplete` esattamente sui pixel dell'oracle; `master.param.sigma_clip_inert` nei casi inerti |
| T-M3B-C02-03 | `weighted_mean`, frame gaussiani con varianze `σ_f²` diverse e pesi non uniformi. (a) Piano: `X = V`, `E = Σ a_f² σ_f²`, `Var(X) = 2 tr((Aᵀ C A Σ)²)` con `A = I − 1aᵀ`, `C = diag(c)`, `Σ = diag(σ_f²)`, a pesi razionali esatti. (b) Valori: `(K − 1) var_K(M)/V_vera ~ χ²_(K−1)`. (c) Domini: `n = 2`; `2 max w = W`; `2 max w > W`; pesi assenti, nulli, negativi, non finiti | (a) Chebyshev più `ρ_c · E`, con `ρ_c` l'ampiezza relativa massima degli intervalli dei `c_f` secondo la regola I. (b) Intervallo bilaterale esatto al livello `α/m`. (c) Codici di §b.10 |
| T-M3B-C02-04 | `median` e `winsorized` sotto `iid_gaussian`, `n` ∈ {3, 4, 9, 10, 101}, `g` ∈ {1, 2, ⌊(n−1)/2⌋}. (a) Piano: `K(n − 1) · media_K(V)/(κ σ²) ~ χ²_(K(n−1))`, esatto perché `V = κ s²`. (b) Valori: `X = (M − μ)²`, `E = κ σ²`, `Var(X) = (μ_4 − κ²) σ⁴`, con `μ_4 = E[T(Z)⁴]` calcolato dall'oracle per quadratura | (a) Intervallo esatto al livello `α/m`. (b) Chebyshev. Prima della misura, `K` e `P` si scelgono in modo che la semiampiezza relativa di (b) sia minore di metà dello scarto relativo fra `κ` esatto e `π/(2n)` per ogni `n` del test; il record lo verifica |
| T-M3B-C02-05 | `κ` del prodotto contro l'oracle del test (quadratura diversa, in `long double`): mediana per ogni `n` in 2…64; winsorized per ogni `g` con `n` ≤ 32; `n` ∈ {101, 255, 1024} con `g` ∈ {1, ⌊n/10⌋, ⌊(n−1)/2⌋}; ancore `κ_med(2) = 1/2`, `κ_med(3) = 1 − √3/π`, winsorized con `n ∈ {2g+1, 2g+2}` uguale alla mediana | Errore relativo ≤ 10⁻⁹ contro le ancore esatte. Contro l'oracle di quadratura: scarto relativo ≤ 10⁻⁹; l'errore dell'oracle è stimato (dimezzamento del passo e confronto di due regole di quadratura), non dimostrato, e la stima deve essere ≤ 10⁻¹¹ prima della misura |
| T-M3B-C02-06 | Caratterizzazione della selezione congelata: `iid_gaussian`, `n` ∈ {11, 21, 51}, `κ = 3`, `clip_max_iter` ∈ {1, 5}; rapporto `E[V]/Var(M)` stimato su `K · P` realizzazioni | Nessuna soglia sul rapporto: stima puntuale (media dei piani diviso la varianza campionaria dei master sulle stesse realizzazioni) nel record, descrittiva, senza un intervallo di copertura, come limite dichiarato. Binario: `variance.fixed_weight_approximation` in `measure`, `variance_estimator = frozen_selection`, avviso presente |
| T-M3B-C02-07 | Caratterizzazione della quantizzazione: distribuzione discreta esatta, calcolata dall'oracle, della mediana di gaussiane arrotondate all'intero; `σ` ∈ {0.5, 1, 1.5, 2, 3, 5} ADU, `n` ∈ {3, 9, 101}, parte frazionaria della media ∈ {0, 0.25, 0.5}; rapporto `Var(M)/E[V]` | Nessuna soglia: valori nel record come limite di §b.3. Binario: `continuous_samples` fra le ipotesi non verificate del report |
| T-M3B-C02-08 | Analisi: candidati che differiscono dal resto per un campo alla volta (esposizione entro e oltre `exposure_rel_tol`, `gain`, `offset_adu`, temperatura, pattern e offset CFA, binning, `filter` per il flat, larghezza, altezza, `roi_x`, `roi_y`, `readout_mode`, `row_order`); RAW con un campo strutturale alla volta e nero alla tolleranza e alla tolleranza + 1; RAW e FITS insieme; ogni motivo di esclusione di §b.2 da solo, compreso `saturation_conflict` con FITS e con RAW; copie con lo stesso digest; permutazioni dell'elenco dei candidati; nessun frame utilizzabile | Esatta contro l'oracle scritto a mano: gruppi (membri, riferimento, stato), esclusioni e motivi, proposte. Il frame incompatibile non compare mai negli ingressi del nodo di un altro gruppo; il preflight di ogni nodo proposto passa; `master.no_usable_frames` |
| T-M3B-C02-09 | Proposta e domini: `n` ∈ {1, 2, 3, 1024, 1025} per ruolo; `n = 1` con e senza parametri del modello di rumore; flat con stato `flat_level` con `n = 2` e `n = 3`; ogni stop di dominio di §b.2 e §b.10 | Proposte della tabella di §b.4; codici di §b.10; nessun nodo negli stop |
| T-M3B-C02-10 | Determinismo e riproducibilità per ogni metodo: `tile_rows` ∈ {1, 7, 64}, worker ∈ {1, 4}, a blocchi e in memoria; riapertura del progetto e ricostruzione del nodo; candidati permutati | 0 differenze bit a bit in master, piano, maschera e report; stessa chiave del nodo e stesso digest dell'artefatto |
| T-M3B-C02-11 | Report per ogni metodo, campo per campo: analisi, rumore (`variance_plane_median`, `residual_mean_adu`, `residual_rms_adu`), artefatti, stato, ipotesi, avvisi; statistiche di `.a` con valore o `null` e motivo secondo il metodo (§b.7), compreso il flat `single` | Esatta contro l'oracle (diagnostiche con la regola I) |
| T-M3B-C02-12 | Codici di fase e linearità (§b.13): frame `calibrated`, `prestacked`, `nonlinear`, fase e linearità entrambe non ammesse; fase o linearità assenti; unità diversa da `adu`; nell'analisi gli stessi casi | `incompatible`, `op.phase_blocked` (prima) e `op.linearity_blocked`; assenze `metadata_missing` come in `.a`; unità `unsupported`, `master.input_state`; analisi: motivi `phase`, `linearity`, `metadata_missing.*`, `unit` |
| T-M3B-C02-13 | `single`: piano contro l'oracle del modello di rumore, con pixel sotto e sopra `offset_adu`; parametri assenti uno alla volta; descrittore | Regola I; `var.source.<campo>`; `combine_method = single`, `variance_estimator = noise_model` |
| T-M3B-C02-14 | Flat CFA: livello fuori tolleranza in un solo canale con livello globale entro tolleranza; `level_bias_estimate` per canale; flat mono invariato | Stop `master.variance_domain` nell'esecuzione e stato `flat_level` nell'analisi; statistiche per canale contro l'oracle; mono identico a `.a` |
| T-M3B-C02-15 | Guasti sul percorso del prodotto (parte M3.b di T-FBK-01): frame modificato dopo la selezione; stato del decoder diverso alla nuova decodifica; pixel `support_incomplete` | `integrity` o `master.decoder_state_changed`, nessun altro nodo né metodo eseguito (contatore del `fault_injector`); pixel invalido senza cambio di metodo |
| T-M3B-C02-16 | PUB-LFC-01: analisi dei bias, dei dark e dei flat con i parametri del record del dato; master con la proposta di §b.4 e `sample_type = f64` | Gruppi ed esclusioni contro l'oracle scritto dagli header; master entro la regola I contro l'oracle del test, come T-M2R2-09 |

Riesecuzioni: T-M2R2-01…09 sulla ricetta `.a`, e T-M3A-CAL-07 (stato del decoder nei master da RAW).

### b.13 Modifiche ad altri contratti e al codice

1. **Codice di stop per un ingresso in fase o linearità non ammessa** (questione aperta di `plans/M3.md`). Regola: vale la tabella delle operazioni di ADR-03 («Aggiunte di R1»), unica casa della regola operazione × fase × linearità, già usata dalla calibrazione (`require_operation_allowed`). Fase diversa da `raw`: `incompatible`, `op.phase_blocked`. Linearità `nonlinear`: `incompatible`, `op.linearity_blocked`. La fase si controlla prima. Restano invariati fase o linearità assenti (`metadata_missing`, `master.metadata.phase` e `master.metadata.linearity`) e l'unità diversa da `adu` (`unsupported`, `master.input_state`). Sostituisce, in C-02.a §2 e nelle «Aggiunte di R2», i casi «fase non `raw`» e «non lineare» del codice `master.input_state`. Codice da cambiare in M3.b, prima del resto: `require_phase` e `require_linear` di `src/core/master.cpp` chiamano `require_operation_allowed(operation_kind::master_create, …)`, e le attese di `tests/unit/test_master.cpp` sui casi `phase` e `nonlinear` cambiano categoria e codice (T-M3B-C02-12).
2. **ADR-16.b §b.3**: aggiunto il motivo di esclusione `pedestal` (master con `pedestal_adu` presente e diverso da zero; nel preflight `unsupported`, `calib.pedestal` come in `.a`), e il caso in T-M3A-CAL-01. Il light resta uno stop (C-01.b §2).
3. **Descrittore** (ADR-16.a tipi, ADR-16 «Aggiunte di R2», ADR-16.b §b.11). I valori di `combine_method`, `rejection` e `variance_estimator` dei master del prodotto sono quelli di §b.7. `master_descriptor_from_node` accetta i metodi di §b.1, invece di fermare ogni combinazione diversa da `mean` + `none`. Per un master esterno restano i valori dichiarati di ADR-16.a. La regola R2 «`mean`, `none`, `empirical`» resta per la ricetta `.a`, come stabilisce ADR-16.b §b.11.
4. **C-02.a, flat CFA**: `m_f` e la tolleranza dei livelli diventano per canale (§b.6) per ogni master flat CFA; per il mono nulla cambia. Il cambio entra nella versione dell'algoritmo del nodo.
5. **ADR-06, estensione R2.r**: il modello di memoria del master conta i buffer di §b.11. Si aggiorna in M3.b, con la sua review, prima del codice.

### b.14 Limiti dichiarati

- Il piano dei metodi robusti è non distorto solo sotto `iid_gaussian` e `continuous_samples`. Su campioni interi lo scostamento della varianza della mediana dal caso continuo non è corretto, solo misurato (T-M3B-C02-07). Con outlier il piano cresce, senza un bound.
- La selezione congelata di `sigma_clip` ha un errore non derivato, misurato solo sotto `iid_gaussian` (T-M3B-C02-06).
- `gaussian_reference` e `noise_model` non hanno un codice di capability: l'ipotesi è nel descrittore e nel report, e una misura che la deve conoscere la legge da lì. Un codice nuovo sarebbe un'estensione di ADR-03.b.
- `κ` è verificato ai punti di T-M3B-C02-05 e per `n ≤ 1024`; oltre, i metodi robusti sono fuori dominio. L'errore dell'oracle di quadratura è stimato, non dimostrato. Lo stesso vale per `μ_4` in T-M3B-C02-04(b): la copertura `α/m` del suo limite di Chebyshev vale sotto l'ipotesi che la stima sia corretta.
- La caratterizzazione della selezione congelata (T-M3B-C02-06) è descrittiva, senza copertura probabilistica.
- Flat: solo `single` e `mean`, senza normalizzazione. I flat a livelli molto diversi (crepuscolo) richiedono un sottoinsieme scelto dall'utente o la normalizzazione, che è dell'unità `C-02.b-flat` (ADR-09.b §b.6).
- Pesi solo dichiarati; nessuna proposta li usa.
- Il nero del decoder è una sentinella dell'offset (ADR-16.b §b.14); `offset_adu` dei RAW è un valore dichiarato.
- Fra backend diversi `sigma_clip` può dare appartenenze diverse vicino alle soglie.
- La proposta di §b.4 è una regola, non una misura di qualità: nessun dato confronta ancora i metodi sui master reali.
- Nessun test è eseguito: le righe di §b.12 sono piani.

### b.15 Questioni

Chiuse da questo contratto (rimosse da `plans/M3.md`):

- codice di stop per un ingresso del master in stato non ammesso: §b.13 punto 1;
- master con `pedestal_adu ≠ 0` nell'analisi `.b`: motivo `pedestal` in ADR-16.b §b.3 (§b.13 punto 2).

Chiuse da ADR-09.b:

- normalizzazione dei flat (`median_ratio`) e metodi robusti sui flat, con la varianza delle scale stimate: assegnati a F-CAL-01.b, unità `C-02.b-flat`, prima del codice di M3.b che li usa (ADR-09.b §b.6);
- formulazione P0 e classi delle soglie (archi `decided_by`): ADR-09.b §b.2 e §b.5, classi in §b.9.
