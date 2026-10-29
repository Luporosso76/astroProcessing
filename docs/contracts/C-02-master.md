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

## Fuori da `.a`

In F-CAL-01.b (M3.b), con stop `unsupported`, `master.variance_domain` in `.a`:

- **`sigma_clip`** e ogni rejection. L'insieme dei frame tenuti dipende dai dati per pixel, e la varianza dei pixel validi dipende da quella selezione.
- **`median`.** La mediana di campioni interi resta su un reticolo. La sua varianza supera quella della mediana di gaussiane continue, `c_med(n) · Var(campione) / n`, di circa `1/(12 σ_med²)`, con `σ_med` la sua deviazione in ADU: +3.7 % a 1.5 ADU, +2.08 % a 2 ADU, +0.92 % a 3 ADU, e cresce rapidamente sotto 1.5 ADU. Sono valori dal calcolo esatto della distribuzione discreta, con `n` fino a 1001; lo strumento e la sua uscita sono conservati per M3.b. Per stare sotto il 2.08 % servirebbe un rumore di lettura di almeno 4.9 ADU con 9 frame e di almeno 16 ADU con 101, fuori dalla portata di quasi tutti i bias reali.
- **`median_ratio`.** Le scale sono stimate dagli stessi dati e sono variabili casuali. La varianza del master normalizzato contiene il termine `Var(E[M | scale])` e le covarianze fra scale, campioni e dark-flat, che `.a` non deriva.
- Media pesata, winsorized, set parziali, gruppi misti.

In M3.c: bias come master calibrante. In ADR-17.b: correlazioni dei master nello stacking.
