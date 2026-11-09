# Protocollo preregistrato M3.b: test statistici dei master

**Preregistrato: si committa prima del primo record di misura e non si modifica dopo; una modifica è un protocollo nuovo con SHA-256 nuovo.**

Versione 2 (9 novembre 2026): `τ_k` come ampiezza dell'intervallo e criterio di inclusione; versione 1 SHA-256 `83e300300952752e049536db3c3d792db83860ebddfafe8a851f24ae6e86fe00`; motivo in `docs/decisions/M3b-lacune.md`, «Decisioni successive (B5)». Nessun record di misura è stato registrato con la versione 1.

Versione 3 (9 novembre 2026): nota sull'implementazione della regola I nell'oracle (§1, long double arrotondato verso l'esterno), criteri e valori invariati; versione 2 SHA-256 `4b06acb685f79ce9599037efb5b70e82b11fc5a6cf563686fff118c67238e8a9`.

Unità B1 di `plans/M3b.md`. Fonti: `docs/contracts/C-02-master.md` §b.9, §b.12, §b.14, §f.9, §f.12; `docs/contracts/ADR-09-algoritmi.md` §b.3, §b.4, T-M3-ADR09-01; `docs/contracts/ADR-17-varianza.md` §b.12 (MC-lin, modello del margine); C-02.a §9 (quantili, Chebyshev). Il record di ogni test cita il commit e lo SHA-256 di questo file. Le soglie stanno nel contratto; qui ci sono solo `K`, `P`, seed, parametri, `m` e margini.

## 1. Regole comuni

**Livello.** `α = 10⁻³` per test, Bonferroni sulle `m` statistiche del test: ogni statistica al livello `α/m`. Un intervallo bilaterale mette `α/(2m)` in ogni coda. I test S di questo protocollo sono quattro (T-M3B-C02-03, C02-04, FLAT-03, FLAT-04): per la disuguaglianza di Boole la probabilità che almeno uno fallisca con prodotto e modello corretti è ≤ 4 · 10⁻³ (ADR-09.b §b.3). Un fallimento si indaga; non si ripete con altri seed.

**Realizzazioni.** Nelle formule del contratto `K` è il numero di realizzazioni indipendenti della statistica. Una **classe** è un insieme di pixel con la stessa verità (stessi parametri di rumore, stessa forma, stesso `β`). La media `μ_p` può cambiare dentro una classe solo dove la statistica non dipende da essa (piani, `(M − μ_p)²`, scarti dalla media del pixel). Pixel diversi sono indipendenti per costruzione: un solo flusso di estrazioni, nessun termine condiviso fra pixel o fra realizzazioni. Dove il protocollo usa `K` immagini da `P_c` pixel di una classe, la statistica usa `K · P_c` realizzazioni al posto di `K`, con la stessa distribuzione. Le varianze campionarie dei valori del master sono raggruppate per classe: `Σ_p Σ_k (M̂_(p,k) − media_k(M̂_p))²`, con `ν = P_c (K − 1)` gradi di libertà, perché la media del pixel può cambiare da pixel a pixel.

**Generatore.** Un `std::mt19937_64` per flusso, costruito con il seed della tabella (algoritmo e inizializzazione fissati dallo standard C++). La trasformazione gaussiana è scritta nella fixture `tests/fixtures/syn_cal.hpp` (B2), non `std::normal_distribution`, il cui algoritmo lo standard non fissa:

- `U = (w >> 11) · 2⁻⁵³`, con `w` la parola a 64 bit successiva;
- metodo polare di Marsaglia: `v₁ = 2U₁ − 1`, `v₂ = 2U₂ − 1`, `s = v₁² + v₂²`; si scarta la coppia se `s ≥ 1` o `s = 0`; altrimenti `z₁ = v₁ · sqrt(−2 ln s / s)`, `z₂ = v₂ · sqrt(−2 ln s / s)`, usati in quest'ordine;
- le `z` si consumano in sequenza su tutto il flusso; una `z₂` avanzata passa al campione successivo, anche oltre la fine di una realizzazione.

Ordine del flusso: realizzazione `k` crescente; dentro, frame per indice di generazione della fixture (non l'ordine canonico, che dipende dai digest); dentro, pixel in ordine di riga. Solo i pixel con rumore consumano estrazioni. La riproducibilità vale con la toolchain del record (`ln` di libreria).

**Campioni.** Frame e master in f64. Campione `x = μ + σ · z`: prima `σ · z`, poi `+ μ`, ciascuno arrotondato in f64. La verità dei test è quella della distribuzione continua con i parametri dichiarati (ADR-09.b §b.3, «riferimento: la verità della simulazione»).

**Margini deterministici `τ_k`.** Per ogni realizzazione `k` l'oracle riesegue la sequenza del contratto (C-02.b §b.6, §f.6) in aritmetica degli intervalli secondo la regola I (ADR-17.b §b.10): ingressi razionali esatti (i campioni f64 realizzati, i livelli esatti della regione di livello, `κ̂` riportato dal prodotto come ingresso puntuale per `gaussian_reference`), fattore `[1 − u, 1 + u]`, `u = 2⁻⁵³`, per ogni operazione arrotondata, estremi per le statistiche d'ordine. `τ_k` è l'ampiezza dell'intervallo, `hi − lo`, massima sui valori confrontati della realizzazione: dipende solo dai campioni e dalla sequenza, mai dal valore del prodotto. Criterio binario, parte di ogni test S: ogni valore f64 del prodotto confrontato sta nel suo intervallo (nessun valore fuori). Fatto: l'intervallo contiene il valore esatto e, se il prodotto segue la sequenza, quello calcolato (ADR-17.b §b.10), quindi con l'inclusione `|x̂ − x| ≤ τ_k`. Implementazione dell'oracle (scelta che non cambia criteri né valori): gli estremi si calcolano in long double (64 bit di mantissa) allargando ogni operazione di `(u + 2⁻⁶⁰)|·|` invece di `u|·|`; il termine `2⁻⁶⁰` copre gli arrotondamenti in long double del passo, quindi l'intervallo calcolato contiene quello razionale della regola I ed è più largo al più di `2⁻⁶⁰/u = 1/128` per operazione. Origine del margine:

- aritmetica: la sola fonte inclusa, come chiede C-02.b §b.9;
- arrotondamento d'uscita: nessuno, perché i master sono f64 (niente mezzo ulp f32);
- modello: nessuno. Due scarti noti restano fuori dal margine e sono dichiarati come limiti, con il valore nel record: l'arrotondamento f64 dei campioni (`|x̂ − (μ + σz)| ≤ u(|σz| + |x̂|)`, al più circa 10⁻¹² ADU per i valori di queste fixture, tutti ≤ 7000 ADU), e l'errore dell'oracle di quadratura su `κ` e `μ_4` (stimato ≤ 10⁻¹¹ relativo, C-02.b §b.14). Il record riporta il loro valore accanto alla semiampiezza del criterio.

Una realizzazione che non soddisfa le condizioni della regola I (underflow, denominatore che contiene 0), o in cui per `sigma_clip` gli intervalli di `x_f − μ` e della soglia si sovrappongono, esce prima della misura ed è contata nel record (C-02.b §b.6, ADR-09.b §b.3). La statistica usa il numero di realizzazioni rimaste.

**Quantili.** χ²: gamma incompleta regolarizzata invertita per bisezione, come C-02.a §9, con lo strumento `tests/unit/chi2_exact.hpp` (errore relativo ≤ 10⁻¹⁰ sul quantile). Il suo ciclo ha un tetto di 10⁵ iterazioni. Con `x ≈ a` la serie converge in circa `8√a` termini, quindi lo strumento copre `ν` fino a circa 3 · 10⁸ (stima). Tutti i `ν` di questo protocollo sono ≤ 1.05 · 10⁸. Normale: bisezione su `std::erfc`, errore relativo ≤ 10⁻¹⁰ sulla coda. Binomiale: somma esatta dei termini in `long double`, errore relativo ≤ 10⁻¹⁰ sulla coda (C-02.b §f.9).

## 2. Riepilogo

| Test | Classe | Unità | `m` | Livello per statistica | `K` | `P` | Seed |
|---|---|---|---|---|---|---|---|
| T-M3B-C02-03 | S, controllo di coerenza | B5 | 4 | `2.5 · 10⁻⁴` | 2000 | 4096 | 2026110901, 2026110902 |
| T-M3B-C02-04 | S, controllo di coerenza; (b) con regola su `K · P` | B5 | 30 | `3.333 · 10⁻⁵` | per `n`, §3.2 | per `n`, §3.2 | 2026110911…2026110915 |
| T-M3B-C02-06 | C | B6 | — | — | 2000 | 4096 | 2026110921…2026110923 |
| T-M3B-FLAT-03 | S, controllo di coerenza | B9 | 18 | `5.556 · 10⁻⁵` | 4000 | 1365 per classe | 2026110931 |
| T-M3B-FLAT-04 | S, controllo di coerenza | B9 | 8 | `1.25 · 10⁻⁴` | 2000 | 682 o 1364 | 2026110941…2026110944 |
| T-M3B-FLAT-05 | C | B9 | — | — | 256 | 1023 per classe | formula, §3.6 |
| T-M3B-C02-05 | P, deterministico | B4 | — | — | — | — | nessuno |
| T-M3B-C02-07 | C, deterministico | B5 | — | — | — | — | nessuno |

## 3. Test

### 3.1 T-M3B-C02-03: `weighted_mean`

Statistiche: (a) piano, `X = V`, `E = Σ a_f² σ_f²`, `Var(X) = 2 tr((Aᵀ C A Σ)²)`, `A = I − 1aᵀ`, `C = diag(c)`, `Σ = diag(σ_f²)`, tutto in razionali esatti. (b) valori, `(K − 1) var_K(M)/V_vera ~ χ²_(K−1)`, raggruppata per classe (§1). (c) domini: binaria, codici di §b.10, fuori dal conto statistico. Ipotesi nulla: il prodotto calcola `M` e `V` secondo §b.6 e i frame seguono la fixture; allora `E[V] = Var(M)` (fatto di §b.6).

Fixture: immagine 64 × 64, `P = 4096`, una classe. `μ_p = 1000 + (p mod 37)` ADU, `p` indice in ordine di riga. Due configurazioni, ognuna con il proprio flusso:

| Config. | `n` | `w` | `W` | `a` | `σ_f` (ADU) | `c_f` | `E` (ADU²) | `Var(X)` (ADU⁴) | Seed |
|---|---|---|---|---|---|---|---|---|---|
| W1 | 5 | 1, 2, 3, 4, 6 | 16 | 1/16, 1/8, 3/16, 1/4, 3/8 | 1, 2, 3, 5, 8 | 15/5944, 35/2972, 189/5944, 105/1486, 945/2972 | 1401/128 ≈ 10.945 | ≈ 198.807 | 2026110901 |
| W2 | 4 | 7, 3, 3, 3 | 16 | 7/16, 3/16, 3/16, 3/16 | 2, 1, 3, 4 | 245/432, 1/48, 1/48, 1/48 | 215/128 ≈ 1.680 | ≈ 3.840 | 2026110902 |

W2 ha `max a = 7/16`, vicino al confine del dominio. L'identità `Σ_f c_f (1 − 2a_f) σ_f² + S Σ a_g² σ_g² = E` è verificata in razionali per entrambe.

`K = 2000`. `m = 4`: (a) e (b) per ciascuna configurazione.

Criteri e margini (C-02.b §b.9, §b.12):

- (a) `|media_(K·P)(V̂) − E| ≤ sqrt(m Var(X)/(α K P)) + ρ_c E + max_k τ_k`, con `τ_k` l'ampiezza dell'intervallo di `V̂` (§1);
- (b) `sd_pool(M̂) ∈ [sqrt(V_vera q_lo/ν) − Δ, sqrt(V_vera q_hi/ν) + Δ]`, `ν = P (K − 1) = 8 187 904`, `q_lo`, `q_hi` quantili di `χ²_ν` alle code `α/(2m)`, `Δ = sqrt(K/(K − 1)) max_k τ_k` con `τ_k` l'ampiezza dell'intervallo di `M̂` (§1). Fatto: `|‖c(M̂)‖ − ‖c(M)‖| ≤ ‖M̂ − M‖ ≤ sqrt(K P) max τ`, con `c(·)` gli scarti dalla media di pixel, da cui il fattore `sqrt(K/(K − 1))` anche sulla deviazione raggruppata.

Inclusione (§1): ogni `V̂` e ogni `M̂` nel proprio intervallo della regola I.

`ρ_c` è l'ampiezza relativa massima degli intervalli dei `c_f` calcolati dai `w_f` dichiarati con la regola I: deterministico, dipende solo dai pesi, calcolato dall'oracle prima della misura (§5).

Semiampiezza attesa, senza margini: (a) 0.312 ADU² (2.85 % di `E`) per W1, 0.0433 ADU² (2.58 %) per W2; (b) circa 0.09 % sulla deviazione standard. Nessuna potenza dichiarata: il contratto lo classifica come controllo di coerenza.

### 3.2 T-M3B-C02-04: `median` e `winsorized`

Statistiche, per ogni cella (metodo, `n`, `g`): (a) piano, `K(n − 1) · media_K(V)/(κ σ²) ~ χ²_(K(n−1))`, esatta perché `V = κ s²`; (b) valori, `X = (M − μ)²`, `E = κσ²`, `Var(X) = (μ_4 − κ²) σ⁴`, `μ_4 = E[T(Z)⁴]` dall'oracle per quadratura. Ipotesi nulla: `iid_gaussian`, `continuous_samples`, prodotto conforme a §b.6.

Celle: mediana con `n ∈ {3, 4, 9, 10, 101}`; winsorized con ogni `g ∈ {1, 2, ⌊(n − 1)/2⌋}` ammesso (`n ≥ 2g + 1`): `n = 3`: {1}; `n = 4`: {1}; `n = 9`: {1, 2, 4}; `n = 10`: {1, 2, 4}; `n = 101`: {1, 2, 50}. Sono 5 + 11 = 16 celle. Le celle winsorized con `n ∈ {2g + 1, 2g + 2}` coincidono con la mediana in aritmetica esatta ma passano per un'altra sequenza f64, quindi restano statistiche distinte. (a) vale per tutte le 16 celle. (b) vale per 14: la mediana con `n = 101` e winsorized (101, 50) ne sono fuori per decisione del coordinatore (`docs/decisions/M3b-lacune.md`, «Decisioni successive (B1)»; C-02.b §b.12). Le loro proprietà le provano T-M3B-C02-01 (valori, esatti) e T-M3B-C02-05 (`κ`), che le comprendono. `m = 16 + 14 = 30`: livello per statistica `α/m = 3.333 · 10⁻⁵`, `α/(2m) = 1.667 · 10⁻⁵` per coda.

Fixture: `σ = 4` ADU (`σ² = 16`), `μ_p = 1000 + (p mod 37)` ADU, immagine 64 × 64 (`P = 4096`), una classe. Per ogni `n` un solo flusso: mediana e tutti i winsorized di quel `n` si calcolano sulle stesse realizzazioni. Statistiche dipendenti sono ammesse, perché Bonferroni non chiede indipendenza.

(a) usa le prime `K_a = 256` realizzazioni del flusso, con `K_a · P = 2²⁰`: `ν_a = 2²⁰ (n − 1)`, al più 1.05 · 10⁸, dentro il campo dello strumento dei quantili. Criterio: `media(V̂) ∈ [κσ² q_lo/ν_a − max_k τ_k, κσ² q_hi/ν_a + max_k τ_k]`, con `τ_k` l'ampiezza dell'intervallo di `V̂` (§1), e ogni `V̂` di queste realizzazioni nel proprio intervallo. Semiampiezza relativa attesa `z sqrt(2/ν_a)`, con `z = z_(1 − α/(2m)) ≈ 4.149`: 0.41 % per `n = 3`, 0.057 % per `n = 101`. Basta: `κ` è verificato a 10⁻⁹ da T-M3B-C02-05.

(b) usa tutte le `K · P` realizzazioni. Criterio: `|media_(K·P)(X̂) − κσ²| ≤ sqrt(m Var(X)/(α K P)) + max_k τ_k (2|M̂_k − μ| + τ_k)`, con `τ_k` l'ampiezza dell'intervallo di `M̂` (§1), e ogni `M̂` nel proprio intervallo.

**Regola di derivazione di `K · P` per (b)** (C-02.b §b.12). La semiampiezza relativa è `h = sqrt(m (μ_4/κ² − 1)/(α K P))`. Lo scarto relativo è `δ = |κ − π/(2n)|/κ`. La regola chiede `h < δ/2` per ogni cella di (b), quindi

```
K · P > 4 m (μ_4/κ² − 1) / (α δ²),   m = 30, α = 10⁻³.
```

Valori di `κ` e `μ_4` (per `σ = 1`) da quadratura di Gauss–Legendre composta, 20 nodi per pannello, pannelli di 0.25 e di 0.125 su `[−9, 9]`: le due griglie coincidono entro 10⁻¹². Mediana a `n` dispari dalla densità della statistica d'ordine centrale. Gli altri casi da `T = ((g + 1)(a + b) + S)/n`, con `a = Z_(g+1)`, `b = Z_(n−g)` e `S` la somma degli `n − 2g − 2` interni, gaussiani troncati ad `(a, b)` (C-02.b §b.6). Ancore riprodotte: `κ_med(3) = 0.4486711045782` contro `1 − √3/π = 0.44867110457821`; media di 2 e di 5 (`g = 0`) con `κ = 1/n` e `μ_4 = 3/n²`. Conto: `python3 -S tmp/b1/kappa_kp.py`. È un dimensionamento, non l'oracle del test.

| Metodo | `n` | `g` | `κ` | `μ_4` | `μ_4/κ² − 1` | `π/(2n)` | `δ` | `K·P` minimo | `h` col `K·P` scelto | `δ/2` | `h/(δ/2)` |
|---|---|---|---|---|---|---|---|---|---|---|---|
| median | 3 | — | 0.4486711046 | 6.109081e−01 | 2.034727 | 0.5235987756 | 1.669991e−01 | 8.7551e+06 | 8.1385e−02 | 8.3500e−02 | 0.9747 |
| winsorized | 3 | 1 | 0.4486711046 | 6.109081e−01 | 2.034727 | 0.5235987756 | 1.669991e−01 | 8.7551e+06 | 8.1385e−02 | 8.3500e−02 | 0.9747 |
| median | 4 | — | 0.2981996184 | 2.684711e−01 | 2.019141 | 0.3926990817 | 3.169000e−01 | 2.4127e+06 | 8.5990e−02 | 1.5845e−01 | 0.5427 |
| winsorized | 4 | 1 | 0.2981996184 | 2.684711e−01 | 2.019141 | 0.3926990817 | 3.169000e−01 | 2.4127e+06 | 8.5990e−02 | 1.5845e−01 | 0.5427 |
| median | 9 | — | 0.1661012814 | 8.341076e−02 | 2.023264 | 0.1745329252 | 5.076206e−02 | 9.4223e+07 | 2.4695e−02 | 2.5381e−02 | 0.9730 |
| winsorized | 9 | 1 | 0.1167739703 | 4.094309e−02 | 2.002538 | 0.1745329252 | 4.946218e−01 | 9.8224e+05 | 2.4568e−02 | 2.4731e−01 | 0.0993 |
| winsorized | 9 | 2 | 0.1261252037 | 4.783829e−02 | 2.007266 | 0.1745329252 | 3.838069e−01 | 1.6352e+06 | 2.4597e−02 | 1.9190e−01 | 0.1282 |
| winsorized | 9 | 4 | 0.1661012814 | 8.341076e−02 | 2.023264 | 0.1745329252 | 5.076206e−02 | 9.4223e+07 | 2.4695e−02 | 2.5381e−02 | 0.9730 |
| median | 10 | — | 0.1383264358 | 5.775045e−02 | 2.018179 | 0.1570796327 | 1.355720e−01 | 1.3177e+07 | 6.5936e−02 | 6.7786e−02 | 0.9727 |
| winsorized | 10 | 1 | 0.1043666191 | 3.269841e−02 | 2.001949 | 0.1570796327 | 5.050754e−01 | 9.4172e+05 | 6.5670e−02 | 2.5254e−01 | 0.2600 |
| winsorized | 10 | 2 | 0.1113319936 | 3.725248e−02 | 2.005489 | 0.1570796327 | 4.109119e−01 | 1.4253e+06 | 6.5728e−02 | 2.0546e−01 | 0.3199 |
| winsorized | 10 | 4 | 0.1383264358 | 5.775045e−02 | 2.018179 | 0.1570796327 | 1.355720e−01 | 1.3177e+07 | 6.5936e−02 | 6.7786e−02 | 0.9727 |
| median | 101 | — | 0.0154862319 | 7.201261e−04 | 2.002735 | 0.0155524389 | 4.275214e−03 | 1.3149e+10 | fuori da (b) | 2.1376e−03 | — |
| winsorized | 101 | 1 | 0.0099222430 | 2.953535e−04 | 2.000007 | 0.0155524389 | 5.674318e−01 | 7.4539e+05 | 2.3921e−01 | 2.8372e−01 | 0.8431 |
| winsorized | 101 | 2 | 0.0099482584 | 2.969053e−04 | 2.000018 | 0.0155524389 | 5.633328e−01 | 7.5628e+05 | 2.3921e−01 | 2.8167e−01 | 0.8493 |
| winsorized | 101 | 50 | 0.0154862319 | 7.201261e−04 | 2.002735 | 0.0155524389 | 4.275214e−03 | 1.3149e+10 | fuori da (b) | 2.1376e−03 | — |

Le due righe «fuori da (b)» mostrano il conto che ha motivato la decisione; entrano solo in (a). La regola vale per `n`, quindi `K · P` si fissa per `n` dalla cella più esigente fra quelle di (b), con almeno il 5 % di margine sul minimo. Così uno scarto di 10⁻¹⁰ fra questi valori e quelli dell'oracle non rovescia la verifica che il record ripete. Per `n = 101` il minimo è sotto `2²⁰`, quindi `K = K_a = 256`.

| `n` | `P` (immagine) | `K` | `K · P` | `K·P` minimo | Margine | `K_a` per (a) | Seed | Estrazioni gaussiane `n · K · P` |
|---|---|---|---|---|---|---|---|---|
| 3 | 4096 (64 × 64) | 2 250 | 9 216 000 | 8 755 062 | 5.3 % | 256 | 2026110911 | 2.77 · 10⁷ |
| 4 | 4096 (64 × 64) | 2 000 | 8 192 000 | 2 412 700 | 240 % | 256 | 2026110912 | 3.28 · 10⁷ |
| 9 | 4096 (64 × 64) | 24 300 | 99 532 800 | 94 222 650 | 5.6 % | 256 | 2026110913 | 8.96 · 10⁸ |
| 10 | 4096 (64 × 64) | 3 400 | 13 926 400 | 13 176 520 | 5.7 % | 256 | 2026110914 | 1.39 · 10⁸ |
| 101 | 4096 (64 × 64) | 256 | 1 048 576 | 756 284 | 38.6 % | 256 | 2026110915 | 1.06 · 10⁸ |

Fatto (potenza implicita della regola). Se i valori del master avessero valore atteso di `X` pari a `π/(2n) σ²` e la stessa `Var(X)`, la media si concentrerebbe a distanza `δ E > 2h E` da `E`. L'accettazione richiederebbe allora uno scarto di almeno `(δ − h) E > h E` dal proprio valore atteso, con probabilità ≤ `α/m` per Chebyshev. Con varianza diversa la probabilità scala col rapporto delle varianze.

Il record ricalcola `κ` e `μ_4` con l'oracle del test (B4, B5) e verifica `h < δ/2` per ogni cella di (b) con i `K`, `P` di questa tabella, prima della misura. Se la verifica fallisce non si cambia `K` dopo una misura: si scrive un protocollo nuovo prima di misurare.

### 3.3 T-M3B-C02-06: selezione congelata, caratterizzazione

Stima: `R = media_(K·P)(V̂) / s²_pool(M̂)`, con `s²_pool` la varianza dei master raggruppata per pixel (§1). Nessuna soglia, nessun intervallo di copertura (C-02.b §b.12).

Celle: `n ∈ {11, 21, 51}`, `clip_kappa_low = clip_kappa_high = 3`, `clip_max_iter ∈ {1, 5}`, `clip_min_kept = 2`. Tutte non inerti (`(n − 1)² > 9n`). Fixture: `σ = 4`, `μ_p = 1000 + (p mod 37)`, 64 × 64, `P = 4096`, `K = 2000`. Un flusso per `n`, condiviso dalle due `clip_max_iter`: seed 2026110921 (`n = 11`), 2026110922 (`n = 21`), 2026110923 (`n = 51`).

Errore Monte Carlo nel record: 20 lotti di 100 realizzazioni consecutive, errore standard della media dei 20 rapporti di lotto. I pixel `support_incomplete` (attesi 0) sono contati e tolti dalla stima. Errore relativo atteso della varianza dei master circa `sqrt(2/(K P)) ≈ 5 · 10⁻⁴`.

### 3.4 T-M3B-FLAT-03: `mean` + `median_ratio`

Statistiche per classe: (a) piano, `X = V`, `E = Σ r_f² σ_f²/n² + Var_f(μ)/n`, `Var_f(μ) = Σ (μ_f − μ̄)²/(n − 1)` (C-02.a §6.2), `Var(X) = [2 tr((HΣ_y)²) + 4 μᵀ H Σ_y H μ]/((n − 1)² n²)`, `H = I − 11ᵀ/n`, `Σ_y = diag(r_f² σ_f²)`; (b) valori, `ν · s²_pool(M)/Var(M | r) ~ χ²_ν`; (c) centro, `media(M) − (b + λ g)` normale di varianza `Var(M | r)/(K P_c)`. Ipotesi nulla: modello di §f.1 con `r` fissato dalla regione di livello, prodotto conforme a §f.6.

Fixture (B2), mono, immagine 128 × 128: regione di livello di 8194 pixel (più della metà) con `g = 1`, `β = 0`, nessun rumore; regione di misura di 8190 pixel in 6 classi da `P_c = 1365`. Parametri:

- `n = 4`, livelli `c = (1000, 1500, 2000, 3000)` ADU, quindi `m_(f) = c_f` esatto, `m_H = 1600`, `r = (8/5, 16/15, 4/5, 8/15)`, `ρ = 3/5`, `λ = (1/n) Σ r_f c_f = 1600`;
- `offset_adu = 512`, `b = offset_adu + β`, `flat_additive_bound_adu = A = 32`, `flat_min_signal_adu = 100`; `flat_norm_rel_budget` assente (vietato per `mean`);
- rumore `σ_f² = c_f g/G + R²`, `G = 1` e⁻/ADU, `R = 5` ADU;
- classi `g ∈ {1/2, 3/2}` × `β ∈ {0, +A, −A}`.

`K = 4000`, seed 2026110931. `m = 18`: (a), (b), (c) per 6 classi. `z_(1 − α/(2m)) ≈ 4.0309`.

| `g` | `β` | `E` (ADU²) | termine dei livelli | `Var(M | r)` | `h_a/E` | `h_c` (ADU) |
|---|---|---|---|---|---|---|
| 1/2 | 0 | 207.222 | 0 | 207.222 | 4.94 % | 0.0248 |
| 1/2 | ±32 | 260.319 | 53.096 | 207.222 | 4.89 % | 0.0248 |
| 3/2 | 0 | 607.222 | 0 | 607.222 | 4.93 % | 0.0425 |
| 3/2 | ±32 | 660.319 | 53.096 | 607.222 | 4.94 % | 0.0425 |

`A = 32` rende il termine dei livelli l'8-20 % di `E`, sopra la semiampiezza di (a). Per (b), `ν = P_c (K − 1) = 5 458 635`, semiampiezza relativa circa 0.24 % sulla varianza.

Criteri e margini (C-02.b §f.9, §f.12; margine `max_k τ_k` del valore confrontato):

- (a) `|media(V̂) − E| ≤ sqrt(m Var(X)/(α K P_c)) + max_k τ_k`, con `τ_k` il bound di `V̂`;
- (b) `sd_pool(M̂) ∈ [sqrt(Var(M | r) q_lo/ν) − Δ, sqrt(Var(M | r) q_hi/ν) + Δ]`, `Δ = sqrt(K/(K − 1)) max_k τ_k`, come in §3.1;
- (c) `|media(M̂) − (b + λ g)| ≤ z_(1 − α/(2m)) sqrt(Var(M | r)/(K P_c)) + max_k τ_k`, con `τ_k` il bound di `M̂`.

### 3.5 T-M3B-FLAT-04: metodi robusti normalizzati

Statistica: conteggi `U` e `D` di §f.12 su tutte le realizzazioni-pixel della cella. Ipotesi nulla: ogni evento ha probabilità ≤ 1/2 (§f.1), quindi ogni conteggio è dominato stocasticamente da `Bin(N, 1/2)`, anche mescolando pixel di classi diverse. Criterio: `U` e `D` ≤ il quantile superiore esatto di `Bin(N, 1/2)` alla coda `α/(2m)`. `τ_k` entra nelle soglie di ogni realizzazione, come scritto in §f.12.

Celle: (a) `β ≡ 0`: mediana `n = 3`, mediana `n = 9`, winsorized `n = 9`, `g = 1`, `sigma_clip` `n = 11`, `sigma_clip` `n = 31`; (b) `|β| ≤ A`: mediana `n = 3`, mediana `n = 9`, winsorized `n = 9`, `g = 1`. `m = 8`; `U` e `D` sono le due code della stessa statistica.

Fixture (B2), mono, 64 × 64: regione di livello di 2050 pixel; regione di misura di 2046 pixel in 6 classi da 341, `g ∈ {1/2, 3/2}` × `β ∈ {0, +A, −A}`. (a) usa le classi con `β = 0` (682 pixel), (b) quelle con `β = ±A` (1364 pixel). Parametri:

- livelli: `n = 3`: `(1000, 2000, 4000)`; `n ∈ {9, 11, 31}`: `c_f = 1000 + ⌊3000 f/(n − 1)⌋`, `f = 0 … n − 1`. Rapporto massimo 4, ammesso con l'uguaglianza (§f.2 punto 3);
- `offset_adu = 512`, `A = 32`, `flat_norm_rel_budget = 0.02`, `flat_min_signal_adu = 100`; rumore `σ_f² = c_f g/G + R²`, `G = 1`, `R = 5`;
- `sigma_clip`: `clip_kappa_low = clip_kappa_high = 3`, `clip_max_iter = 5`, `clip_min_kept = 2`.

Controllo del budget (§f.2 punto 4), in razionali esatti:

| `n` | `m_H` | `ρ` | `A ρ` | `budget · m_H` |
|---|---|---|---|---|
| 3 | 1714.2857 | 0.714286 | 22.857 | 34.286 |
| 9 | 2068.6469 | 1.068647 | 34.197 | 41.373 |
| 11 | 2087.6021 | 1.087602 | 34.803 | 41.752 |
| 31 | 2138.5582 | 1.138558 | 36.434 | 42.771 |

`K = 2000`. `N = 1 364 000` per le celle (a), `2 728 000` per le (b), meno le realizzazioni escluse per la regola I o per intervalli sovrapposti di `sigma_clip`. Seed per `n`, condivisi da (a) e (b) e dai metodi dello stesso `n`: 2026110941 (`n = 3`), 2026110942 (`n = 9`), 2026110943 (`n = 11`), 2026110944 (`n = 31`).

Soglia indicativa, calcolata in doppia precisione: il test la ricalcola esatta in `long double` sul `N` effettivo. Per `N = 1 364 000`, `q = 684 240` (`N/2 + 2240`, 0.16 %); per `N = 2 728 000`, `q = 1 367 168` (`N/2 + 3168`, 0.12 %).

### 3.6 T-M3B-FLAT-05: piano robusto eteroschedastico, caratterizzazione

Stima: `R = media(V̂)/s²_pool(M̂)` per classe, stima di `E[V]/Var(M | r)`. Nessuna soglia (§f.12).

Celle: `n ∈ {3, 9, 31}` × rapporto `ϱ ∈ {1, 1.5, 2, 3, 4}` × regime di rumore × metodo. Metodi: mediana; winsorized con `g ∈ {1, ⌊(n − 1)/2⌋}` (per `n = 3` solo `g = 1`); `sigma_clip` con soglie uguali `κ_3 = 1`, `κ_9 = 2.5`, `κ_31 = 3`, scelte perché siano non inerti (`(n − 1)² > κ² n`), `clip_max_iter = 5`, `clip_min_kept = 2`.

Fixture (B2), mono, 64 × 64: regione di livello di 2050 pixel; regione di misura di 2046 pixel in 2 classi da 1023, `g ∈ {1/2, 3/2}`, `β = 0`. Livelli `c_f = 1000 + ⌊(ϱ − 1) · 1000 · f/(n − 1)⌋`. `offset_adu = 512`, `A = 32`, `flat_norm_rel_budget = 0.02`, `flat_min_signal_adu = 100`. Il controllo del budget passa in ogni cella (caso peggiore `n = 31`, `ϱ = 4`: `A ρ = 36.43 ≤ 42.77`). Regimi: fotonico `G = 1`, `R = 2`; di lettura `G = 16`, `R = 50`, con `c g/G ≤ 375 < R² = 2500`.

`K = 256`. Un flusso per (`n`, `ϱ`, regime), condiviso dai metodi: seed `2026110950000 + 1000 n + 10 · (10 ϱ) + i`, con `i = 0` fotonico e `i = 1` lettura. Esempio: `n = 9`, `ϱ = 1.5`, lettura → 2026110959151. I 30 seed sono distinti, perché `10 · (10 ϱ) ≤ 400 < 1000`.

Errore Monte Carlo nel record: 16 lotti di 16 realizzazioni, errore standard della media dei rapporti di lotto.

### 3.7 Test deterministici della stessa famiglia

**T-M3B-C02-05** (B4, classe P). Celle del contratto: mediana per `n` in 2…64; winsorized per ogni `g` con `n ≤ 32`; `n ∈ {101, 255, 1024}` con `g ∈ {1, ⌊n/10⌋, ⌊(n − 1)/2⌋}`; mediana con `n = 101` e winsorized (101, 2); ancore. Nessun seed, nessun `K`. Prima della misura l'oracle stima il proprio errore con il dimezzamento del passo e il confronto di due regole di quadratura; la stima deve essere ≤ 10⁻¹¹ in ogni cella (§b.12), altrimenti non si misura.

**T-M3B-C02-07** (B5, classe C). Celle: `σ ∈ {0.5, 1, 1.5, 2, 3, 5}` ADU, `n ∈ {3, 9, 101}`, `μ = 1000 + φ` con `φ ∈ {0, 0.25, 0.5}`. Campione intero `j` con probabilità `Φ((j + 1/2 − μ)/σ) − Φ((j − 1/2 − μ)/σ)`. Mediana a `n` dispari dalla distribuzione esatta della statistica d'ordine discreta. `E[V] = κ · Var(campione intero)`, perché `s²` è non distorta per campioni iid, con `κ` dell'oracle di C02-05. Supporto `|j − μ| ≤ ⌈40σ⌉ + 1`, calcolo in `long double`. L'errore si stima ripetendo con supporto `⌈60σ⌉ + 1` e in `double`, e deve essere ≤ 10⁻¹² relativo prima della registrazione. Nessun seed. La parte binaria (`continuous_samples` fra le ipotesi non verificate) usa una fixture a campioni interi senza estrazioni.

## 4. Fixture con estrazioni nei test E e B

Questi test non hanno soglie statistiche, ma le loro fixture possono estrarre valori. Cambiare il seed o i casi esclusi dopo un esito è un allentamento (ADR-09.b §b.3), quindi i seed si fissano qui. Dimensioni, posizioni degli outlier e parametri si fissano nel codice della fixture (B2), committato prima del primo record del test. Il record cita quel commit. Un test che non estrae nulla non usa il seed, e il record lo dichiara.

| Test | Uso | Seed |
|---|---|---|
| T-M3B-C02-01 | casi non dyadic | 2026110961 |
| T-M3B-C02-02 | rumore di `FIX-SYN-CAL-08` con outlier, `n ∈ {11, 15, 31}`; casi con intervalli sovrapposti esclusi e contati | 2026110962 |
| T-M3B-C02-10 | frame dei casi di determinismo | 2026110963 |
| T-M3B-C02-11 | frame dei casi di report | 2026110964 |
| T-M3B-FLAT-01 | casi non diadici | 2026110965 |
| T-M3B-FLAT-02 | `g`, `β`, livelli dei casi non diadici (`ν = 0`) | 2026110966 |
| T-M3B-FLAT-07 | frame dei casi di report | 2026110967 |
| T-M3B-FLAT-08 | frame dei casi di determinismo | 2026110968 |

Fuori dal protocollo, senza componente stocastica: T-M3B-C02-08, 09, 12, 13, 14, 15, 16; T-M3B-FLAT-06, 09.

## 5. Costo dichiarato e punti da derivare

Estrazioni gaussiane: T-M3B-C02-04 circa 1.2 · 10⁹, quasi tutte per `n = 9`; gli altri test insieme circa 1.3 · 10⁹; in tutto circa 2.5 · 10⁹. Il tempo non ha soglia (ADR-09.b §b.4 punto 7). Con la mediana a `n = 101` e winsorized (101, 50) dentro (b), la regola avrebbe chiesto (con `m = 32`, cioè le due celle incluse) `K · P` ≥ 1.40 · 10¹⁰ e circa 1.42 · 10¹² estrazioni: per questo il coordinatore le ha tolte da (b) (`docs/decisions/M3b-lacune.md`, «Decisioni successive (B1)»).

Da derivare, con la regola, prima della prima misura del test:

| Punto | Unità | Regola |
|---|---|---|
| `ρ_c` di T-M3B-C02-03 | B5 | ampiezza relativa massima degli intervalli dei `c_f` di W1 e W2, regola I dai `w_f` dichiarati; scritto nel record prima della misura |
| `κ` e `μ_4` dell'oracle di T-M3B-C02-04 e verifica `h < δ/2` | B4, B5 | §3.2; con i `K`, `P` di questa tabella; se fallisce, protocollo nuovo prima di misurare |
| errore dell'oracle di quadratura di T-M3B-C02-05 | B4 | ≤ 10⁻¹¹ per cella, dimezzamento del passo e due regole |
| errore della distribuzione discreta di T-M3B-C02-07 | B5 | ≤ 10⁻¹² relativo, §3.7 |
| dimensioni e posizioni delle fixture di §4 | B2 | nel codice della fixture, committato prima del primo record |

I `τ_k` non sono numeri da fissare prima: sono funzioni dei campioni realizzati, con la formula fissata in §1 e per test in §3.
