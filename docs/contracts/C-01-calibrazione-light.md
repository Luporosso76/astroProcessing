# Contratto C-01.b: calibrazione del light, forma completa

Stadio: C-01.b, light completo (registro riga 3428; aggregato C-01 riga 3426; testo §10.2 righe 1355-1369 del piano). Archi: C-01.b `requires_all` C-01.a e `decided_by` ADR-16.b e ADR-09.b (righe 4110-4112); F-CAL-02.b `requires_all` C-01.b (riga 3730) e `requires_when` G-OFFSET con P-OFFSET (riga 4396); G-OFFSET `requires_all` F-IO-04 (riga 4372). G-OFFSET: «Offset light noto e ledger», stadio M3.a (riga 3560); P-OFFSET: «Ricetta seleziona divisione flat: offset light richiesto» (riga 3590). Piano: §9.3 (righe 1227-1242), §9.6 (1276-1286), §9.8 (1302-1312), §9.9 (1314-1331), §10.1 (1335-1351), F-IO-04 (riga 1815).

C-01.a è la forma minima nella parte `.a` di [ADR-16](ADR-16-stati-master.md) (ricetta `dark_matched_flat_dark_flat`) e resta valida; questo file non la ripete. Dipendenze fra contratti: [ADR-16.b](ADR-16-stati-master.md#estensione-adr-16b-matrice-completa-della-calibrazione-m3) (analisi G-CAL, catene, default, P-LOST, ledger, numerica) e il verbale [D-16](../decisions/D-16-calibrazione-incompleta.md); [ADR-03.b](ADR-03-pixel-maschere.md) §b.2 (`decoder_state`), §b.5 (coverage), §b.13 (capability), §b.14 (tabella delle operazioni); [ADR-08.b](ADR-08-decoder-pack.md) §2.4, §2.6, §2.9 (nero dichiarato, ipotesi H3, record del decoder); [ADR-17.a](ADR-17-varianza.md). ADR-09.b, l'altro `decided_by`, non ha ancora un contratto: le catene sono quelle di ADR-16.b, e un ADR-09.b che le cambi riapre questo contratto. Unità che lo implementano: M3.a (G-OFFSET nell'analisi e nel ledger, dominio dell'ingresso, coverage nell'analisi), M3.c (esecuzione, residuo del percorso parziale, varianza, metriche).

Ogni clausola è una **regola** (vincolo del prodotto, verificato dal test indicato), un **fatto** (con la fonte) o un'**ipotesi** (dichiarata, con il test o l'unità che la prova). Una regola ha una sola casa: dove ADR-16.b fissa già il contenuto, qui c'è il rimando.

Fuori da C-01.b: dark scalato `L − B − α(D − B)` (F-CAL-05, P1, M6.f); bad pixel map (F-CAL-04, M3.c); regione di normalizzazione del flat (F-CAL-03.b); covarianze e master usato in due catene (ADR-17.b); scelta fra più master per ruolo (F-IO-03, ADR-15); light RGB o in elettroni (§3).

## 1. Problema matematico

Regola. Stimare `S` da `L = B + t_L d + q S + eps` con una delle ricette di ADR-16.b §b.5 (catena del light × catena del flat), scelta prima dell'esecuzione secondo §b.1 e §b.6. Notazione di ADR-16.b §b.5. Per un light RAW `b_L(r, c)` è il nero dichiarato dal decoder, valutato con l'ipotesi H3 di ADR-08.b §2.4 (`b_L = black + black_per_color[FC(r, c)] + P(r, c)`, intero esatto).

**Uso del nero del decoder (G-OFFSET).** Regola: il prodotto non sottrae mai `b_L` dai campioni. Nessuna catena lo usa: campioni, varianza e maschera di uscita dipendono dai campioni, dai piani di varianza, dalle maschere, da `saturation_adu` e dai parametri della catena selezionata, mai dai campi del nero di `decoder_state` (`black`, `black_per_color`, `black_pattern`). `b_L` entra in tre punti soltanto:

1. compatibilità fra ingressi RAW, come sentinella dell'offset (ADR-16.b §b.7);
2. voce `light_offset` del ledger e residuo del percorso parziale nel report (§10);
3. codici di capability `calib.offset.known_not_removed` e `calib.assumption.decoder_black` (ADR-03.b §b.13).

Stato dell'offset del light dopo la catena: tabella di ADR-16.b §b.5. Qui si precisa che cosa significa «noto»:

- `removed`: un master (`B` o `D`) ha tolto l'offset misurato sui suoi frame;
- `known_not_removed`: il light ha `decoder_state` (ADR-03.b §b.2) e il record del decoder dichiara `black_applied_by_decoder = none` (ADR-08.b §2.4, §2.9). «Noto» vuol dire dichiarato dal decoder e non sottratto da nessuno, non misurato;
- `unknown`: ogni altro light. Un FITS con `offset_adu` (keyword `OFFSET`, ADR-03.a) resta `unknown`: il valore è un'impostazione della camera, e né l'header né lo standard FITS ne dichiarano la relazione con il livello in ADU dei campioni.

Fatto, per costruzione: nessuno stadio prima della calibrazione sottrae il nero dichiarato. Il decoder non lo sottrae (ADR-08.b §2.2, ipotesi H1, T-M3A-RAW-02). Un master da RAW `bias_included` è combinato dai campioni dei frame (C-02.a), che contengono lo stesso nero del light; un `D'` o un `DF'` ha l'offset tolto da un bias, non da `b`. Quindi `L − D` e `L − B` tolgono l'offset una volta sola, e `b_L` non si può sottrarre in aggiunta senza contarlo due volte.

Residuo del percorso parziale. Con offset `known_not_removed` e catena del flat che divide (`flat_division_offset = known_not_removed`, ADR-16.b §b.5), il report dichiara `r(r, c) = b_L(r, c) / Q(r, c)`. È il residuo dell'offset **dichiarato**: il residuo fisico è `(B + t_L d)/Q` con `light.none` e `B/Q` con `light.dark_sub_partial`, e il prodotto non lo misura (§3, H-BLK). Senza divisione il residuo dichiarato è `b_L`.

## 2. Dominio, unità e prerequisiti

Stati, ruoli, unità, fasi e compatibilità dei master: ADR-16.b §b.2, §b.3, §b.7. Il light ha un dominio proprio. Regola (chiude la questione dell'ingresso fuori dalla propria riga):

- **Il light non è un candidato.** Fase, linearità, unità e layout del light fuori dalla regola fermano l'analisi, perché senza light non esiste alcuna ricetta. Si controllano in quest'ordine e il primo violato dà lo stop:
  - fase diversa da `raw`: `op.phase_blocked`, valutata prima della linearità;
  - linearità diversa da `linear`: `op.linearity_blocked`. Questi due codici e la loro precedenza sono quelli di ADR-03.b §b.14, riga `calibrate_light`;
  - unità diversa da `adu`: `unsupported`, `calib.unit`;
  - layout diverso da `mono` o `cfa`: `unsupported`, `calib.plane_layout`. Unità e layout seguono ADR-16.a §2.
- **Un master fuori dominio** è `excluded` con il motivo `phase`, `linearity`, `unit` o `layout` (ADR-16.b §b.3), il primo violato nello stesso ordine, dopo lo stato (ADR-03.b §b.14), e l'analisi prosegue.
- ADR-16.b §b.2 è allineato a questa regola.

Prerequisiti aggiunti da questo contratto:

- coverage assente su ogni ingresso (§7, regola C7.3);
- `pedestal_adu` del light presente e diverso da zero: stop come in ADR-16.a §2 (`calib.pedestal`). Il piedistallo non è un offset noto. Un master con piedistallo è `excluded` nell'analisi con motivo `pedestal` (ADR-16.b §b.3).

Verifica: T-M3A-C01-06 (dominio), T-M3A-C01-07 (coverage).

## 3. Ipotesi e limiti

- Le ipotesi di ADR-16.a §3 e di ADR-16.b §b.2 e §b.5 restano: H-D' e il limite termico di `flat.bias`.
- **H-BLK, ipotesi non verificata.** Che `b_L` approssimi l'offset fisico `B` del light. Il prodotto non la verifica, e per questo non sottrae `b_L` (§1). Fatti che la rendono non verificabile in P0:
  - l'API di LibRaw non distingue il nero letto dal file da quello delle tabelle per modello (ADR-08.b §2.4);
  - le operazioni del firmware non sono osservabili (`camera_processing = unknown`, ADR-08.b §2.9);
  - `BlackLevelDeltaH`/`V` non sono coperti;
  - i pixel mascherati del nero ottico non si importano (ADR-08.b §2.3).

  Nel percorso parziale il residuo dichiarato vale come limite proprio sotto H-BLK. La componente termica `t_L d / Q` non è stimata.
- **Linearità dell'uscita.** Fatto: ogni catena ha la forma `C = (L − X)/Q` oppure `Lc = L − X`, con `X` e `Q` che non dipendono da `L`, quindi l'uscita è affine in `L`. Con un light `linear` resta lineare nel segnale a meno di un termine additivo. La linearità si riferisce al segnale al netto del livello di buio (EMVA 1288, riferimento di §5): il residuo additivo è una limitazione di capability (`calib.offset.*`), non di linearità. L'uscita ha quindi `lin = linear` per ogni catena. Che una misura accetti il residuo lo decide il suo contratto, con `assess_measure` di ADR-03.b §b.13. C-14 (antenato lineare) e C-16 (fotometria) non sono ancora scritti: devono dichiarare il proprio insieme accettato rispetto a `calib.offset.known_not_removed`.
- **Curva di linearizzazione.** Ipotesi: con `linearization = decoder_curve` nero e campioni stanno nello stesso dominio dopo la curva. Secondo la specifica DNG, la tabella si applica prima della sottrazione del nero; la specifica non è riletta in questa unità. Le coppie RAW hanno `curve_sha256` uguale (ADR-16.b §b.7).
- **Limite: unità `electron`.** Non ammessa, light stop e master `excluded`. Il modello di varianza di ADR-17.a è in ADU.
- **Limite: light `rgb`.** Non ammesso. Il decoder produce solo CFA (ADR-08.b §2.3, `raw.not_cfa`), e un light RGB con fase `raw` non ha una catena definita in ADR-16.b. Il §10.2 del piano elenca ADU, elettroni e RGB; C-01.b copre ADU, mono e CFA.

## 4. Alternative e scelta

Catene e default: ADR-16.b §b.5 e §b.6, D-16. Per l'offset del light si sono confrontate tre alternative:

| Alternativa | Esito | Motivo |
|---|---|---|
| A. Sottrarre `b_L` in automatico quando manca un master dell'offset | Scartata | Trasforma una sentinella non verificata (H-BLK) in una correzione. Con un master presente sottrarrebbe l'offset due volte (§1) |
| B. Catena esperta `L − b_L` con limite dichiarato `|B − b_L| ≤ bound` | Scartata per P0 | Richiede un nuovo stato dell'offset, nuove voci del ledger, nuovi codici di capability e una riga nuova nelle priorità di D-16, cioè una revisione di ADR-16.b, D-16 e ADR-03.b. Il bisogno, dividere per `Q` senza master dell'offset, è già coperto dal percorso parziale esplicito con residuo dichiarato |
| C. Nessuna sottrazione; `b_L` come sentinella, voce del ledger e residuo dichiarato | **Scelta** | Afferma solo ciò che il prodotto sa: nero dichiarato e non sottratto. L'offset si toglie solo con un master |

Condizioni perché un'unità futura possa sottrarre `b_L`:

- una misura che confronti `b_L` con l'offset dello stesso light, con una tolleranza fissata prima: per esempio i pixel di nero ottico importati come geometria dichiarata, che oggi ADR-08.b §2.3 esclude;
- la revisione di ADR-16.b, D-16 e ADR-03.b descritta in B.

Un bias compatibile non basta: rende `light.bias` disponibile, e quella catena ha la priorità.

## 5. Riferimenti indipendenti e stato

Nessuna derivazione da software esistente (DEC-02, NC-15). I testi seguenti non sono stati riletti in questa unità: il contratto non dipende da un loro valore numerico, li usa per modello, definizioni e ordine delle operazioni.

- Modello del segnale con livello di buio e corrente di buio proporzionale all'esposizione; linearità definita sul segnale al netto del buio; misura del buio da immagini al buio: EMVA Standard 1288, *Standard for Characterization of Image Sensors and Cameras*, Release 4.0, European Machine Vision Association, 2021.
- Bias, dark, flat e ordine delle correzioni nella riduzione di immagini CCD; livello di bias dall'overscan e sua variazione: S. B. Howell, *Handbook of CCD Astronomy*, 2ª ed., Cambridge University Press, 2006, cap. 4.
- Offset elettronico e trasferimento dei fotoni: J. R. Janesick, *Scientific Charge-Coupled Devices*, SPIE Press, 2001.
- Tag `BlackLevel`, `BlackLevelRepeatDim`, `BlackLevelDeltaH`/`V`, `LinearizationTable` e ordine linearizzazione → sottrazione del nero → scala: Adobe, *Digital Negative (DNG) Specification*, versione 1.6.0.0, 2020 (la stessa citata in ADR-08.b §6).
- Propagazione della varianza: JCGM 100:2008 (GUM), §5.1.2 eq. (10).
- Aritmetica in virgola mobile: N. J. Higham, *Accuracy and Stability of Numerical Algorithms*, 2ª ed., SIAM, 2002, cap. 2-3 (come ADR-16.b §b.12).
- Header FITS e unità: W. D. Pence et al., «Definition of the Flexible Image Transport System (FITS), version 3.0», *A&A* 524, A42, 2010.
- Campi del nero in LibRaw: fatti letti negli header installati (ADR-03.b §b.2, ADR-08.b §2.4).

## 6. Numerica, stabilità e varianza

- **Catene.** Regola: numerica e tolleranze di ADR-16.b §b.12 (f64 dai campioni, ordine scritto, un solo arrotondamento finale).
- **Nero dichiarato.** Regola: `b_L(r, c)` si calcola in interi a 64 bit, in modo esatto (ADR-16.b §b.7), e si converte in f64 senza errore. Fatto: la somma di tre `uint32_t` è minore di `2^34 < 2^53`.
- **Residuo del percorso parziale.** Regola: si calcola come una catena con `Lc = b_L` esatto, cioè `k_L = 0` ed `E_x = 0` in ADR-16.b §b.12, con lo stesso `Q` della catena del flat selezionata. Bound per pixel:
  - `|r̂ − r| ≤ γ_m |r|`, con `m` = 1 (`flat.given_q`), 3 (`flat.normalize`), 5 (`flat.dark_flat`, `flat.bias`), nelle stesse ipotesi di §b.12;
  - `|r̂ − r| ≤ ρ_i |r|` con il bound generale (`flat.bias_dark_flat_sub`).

  Il report riporta, per ciascuno dei quattro canali CFA, `G1` e `G2` separati, il minimo e il massimo di `r̂` sui pixel validi dell'uscita. Fatto: minimo e massimo sono 1-lipschitziani in norma infinito, quindi `|min r̂ − min r|` e `|max r̂ − max r|` sono al più `max_i |r̂_i − r_i|`, preso sui pixel del canale. Valori in f64, senza arrotondamento al tipo del light. Un canale senza pixel validi nell'uscita (per esempio tutti i suoi light saturi) ha minimo e massimo assenti, dichiarati con motivo `no_valid_pixels` e conteggio 0: non è uno stop, perché l'uscita resta valida, e nessun valore sostituisce quelli assenti.
- **Varianza.** Regola di ADR-16.b §b.10: ogni termine sottratto aggiunge la sua varianza. Per catena, con i master usati una volta:

  | Catena | Varianza |
  |---|---|
  | light | `Var(Lc) = Var(L) + Σ Var(termini sottratti)` |
  | `flat.dark_flat`, `flat.bias`, `flat.bias_dark_flat_sub` | `Var(Fc)` per somma analoga, `Var(Q) = Var(Fc)/N²` |
  | `flat.normalize` | `Var(Q) = Var(F)/N²` |
  | `flat.given_q` | `Var(Q)` dal piano del flat |
  | ogni catena che divide | `Var(C)` con la formula di ADR-17.a e `Cov(Lc, Q) = 0` |

  Fatto: per una combinazione lineare di ingressi indipendenti la somma delle varianze è esatta (GUM eq. 10). La divisione per `Q` è al primo ordine, come in ADR-17.a, già verificata nel regime di T-M2E-21. La tabella vale con i master usati una volta; con `B` in entrambe le catene vale il modello di ADR-17.b §b.2. T-M3A-C01-08 verifica la tabella sui casi dyadic; il bound non dyadic è la regola I di ADR-17.b §b.10, verificata in T-M3C-VAR-01.
- Nessun epsilon. `Q` non valido si maschera (ADR-16.a §7).

## 7. Maschere, metadati e bordi

Maschere: ADR-16.a §7 per ogni catena. Saturazione: il `saturation_adu` di ciascun ingresso (ADR-08.b §2.6). Coverage, regole che chiudono la questione aperta di §9.8:

- **C7.1, griglia del sensore.** Regola: la calibrazione è un'operazione per pixel sulla griglia del sensore. L'uscita ha la geometria del light, e il pixel `(x, y)` di uscita viene dal pixel `(x, y)` di ogni ingresso (compatibilità di dimensioni e `roi` in ADR-16.a §2). Non ha un kernel: non accende `support_incomplete` e non produce coverage.
- **C7.2, ingressi senza coverage.** Regola: l'uscita non ha coverage. La validità di ogni campione è nei bit di invalidità, unione di quelli degli ingressi più i bit propri (ADR-16.a §7). Da quei bit la coverage nasce negli operatori geometrici, come frazione dell'area coperta da campioni validi (ADR-03.b §b.5; C-06, M3.g). Lo stack la usa secondo C-07. La calibrazione propaga così la «coverage dei campioni validi» di §9.8 attraverso la validità.
- **C7.3, ingresso con coverage.** Regola: un ingresso con coverage presente è fuori dominio.
  - Fatto: la coverage di ADR-03.b §b.5 è la frazione dell'area di un pixel coperta da campioni validi. La calibrazione combina per aritmetica valori che sono medie su aree diverse, e la frazione coperta del risultato non è funzione delle frazioni degli ingressi. Con due ingressi di coverage `c1` e `c2` l'area coperta da entrambi sta fra `max(0, c1 + c2 − 1)` e `min(c1, c2)`, ed entrambi gli estremi si raggiungono: con `c1 = c2 = 1/2`, metà sinistra e metà destra coperte danno 0, la stessa metà dà 1/2.
  - Light con coverage: stop dell'analisi, `unsupported`, `calib.coverage_input`.
  - Master con coverage: `excluded` con motivo `coverage`, aggiunto alla lista chiusa di ADR-16.b §b.3. Nel preflight del nodo: `unsupported`, `calib.coverage_input`, con l'ingresso nel contesto.
  - La regola vale anche per la ricetta `.a` di ADR-16.a: in M2 non riceve coverage, perché il lettore la produce da M3.a (ADR-03.b §b.5); da M3.a in poi lo stop esclude che la coverage si perda in silenzio. Un ingresso senza coverage non cambia nessun esito di `.a`.
- **Metadati dell'uscita.** Regola: `phase = calibrated` (ADR-16.b §b.5), `lin = linear` (§3), unità `adu`, `measure` secondo ADR-03.b §b.13 (T-M3A-TYP-17), `variance_scope = independent` quando c'è varianza (ADR-03.b §b.4). `decoder_state` non si copia (ADR-03.b §b.2): lo stato consumato va nella provenance e nella voce `light_offset`.
- **Bordi.** Nessuno: senza kernel non c'è bordo. Le righe e le colonne dell'overscan non entrano nell'immagine (ADR-08.b §2.3).

## 8. Determinismo

Regola: come ADR-16.a §8, esteso a ogni catena di ADR-16.b §b.5. `N` si calcola su tutta l'immagine prima dei blocchi. Uscita, varianza, maschera e report sono bit-identici con 1, 2 e 4 worker e con ogni `tile_rows`. Minimo e massimo del residuo non dipendono dall'ordine di visita. Ordine canonico degli ingressi del nodo (ADR-05). Nessun seed. SIMD entro le tolleranze di ADR-07.a. T-M3A-C01-11.

## 9. Fixture, metriche, soglia e gate

Fixture, scritte da chi non scrive l'operatore, con modello analitico e senza codice del prodotto (`docs/test-matrix.md`, regole):

- **`FIX-SYN-CAL-08`** (M3.c). Light, bias, dark (`D` e `D'`), dark-flat (`DF` e `DF'`) e flat generati da `B`, `d`, `q`, `S`, `K` noti.
  - `B` per canale CFA e con motivo spaziale; offset fisico `B = b + 7`, diverso dal nero dichiarato `b` degli stati del decoder.
  - `q` con vignettatura (minimo 0,6 al bordo) e una macchia di polvere a 0,3.
  - Una variante senza rumore e dyadic, con le condizioni dei casi esatti di ADR-16.b §b.12; una non dyadic.
  - Stati del decoder costruiti in memoria con nero scalare, per colore e con motivo 2×3, per ingressi RAW.
- **DNG sintetici di T-M3A-RAW-02** (M3.a). Nero scalare e per canale 2×2, origine dell'area visibile pari e dispari, scritti con libtiff.

Metriche del §10.2 e §9.9 del piano, con soglia fissata qui:

| Metrica | Definizione | Soglia | Test |
|---|---|---|---|
| Bias residuo | Sensibilità all'offset fisico: `s = Q · (C(Δ) − C(0)) / Δ` (`Q = 1` senza divisione), con `Δ = 64` aggiunto ai frame che contengono l'offset: light, bias, `D`, `DF`, flat `raw`; non a `D'`, `DF'` e ai flat preparati | `s = 0` esatto per offset `removed`, `s = 1` esatto per `known_not_removed` e `unknown`, sulla variante dyadic | T-M3A-C01-04 |
| Doppio offset | Uscita al variare del nero dichiarato `b → b + 16` su tutti gli ingressi RAW, campioni invariati | Campioni, varianza e maschera bit-identici; estremi per canale del residuo dichiarato uguali a `min_i (b_i + 16)/Q_i` e `max_i (b_i + 16)/Q_i` dell'oracle, esatti (dyadic) | T-M3A-C01-04, 05 |
| Rumore | Piano di varianza per catena contro la formula di §6 in aritmetica razionale | 0 sulla variante dyadic; variante non dyadic: regola I (ADR-17.b §b.10) | T-M3A-C01-08; T-M3C-VAR-01 |
| Flat residuo | `m_i = Ĉ_i·K/(N(Fc)·S_i) − 1` sui pixel validi, con `Ĉ_i` il campione di uscita e `K`, `N(Fc)`, `S_i` esatti dell'oracle, calcolata dal test in aritmetica razionale (nessun arrotondamento nella metrica); la fixture ha `S ≥ 1` ovunque, quindi `S = 0` non compare. Ricette con offset e termico rimossi e flat applicato | `|m_i| ≤ B_C,i · K/(N(Fc)·S_i)` pixel per pixel, con `B_C,i` il bound assoluto di ADR-16.b §b.12 per `C` (uscita f32 compresa). Fatto: in aritmetica esatta queste ricette danno `C = S·N(Fc)/K`, quindi `m_i = (Ĉ_i − C_i)·K/(N(Fc)·S_i)`. Per le ricette con un'omissione dichiarata si riporta senza soglia: l'omissione è un esito dichiarato | T-M3A-C01-10 |
| Perdita di segnale | Insieme dei pixel invalidi dell'uscita e conteggi per bit | Insieme uguale a quello dell'oracle, esatto | T-M3A-C01-09 |
| Residuo dichiarato | Minimo e massimo per canale di `b_L/Q` | 0 sulla variante dyadic; altrimenti entro `max_i` del bound di §6 sui pixel del canale; canale senza pixel validi: valori assenti con `no_valid_pixels` | T-M3A-C01-05 |

Test pianificati. Oracle scritti a mano nel test, indipendenti dall'implementazione:

| ID | Slice | Verifica | Criterio |
|---|---|---|---|
| T-M3A-C01-01 | a | Stato dell'offset per ogni catena del light × light {RAW con `decoder_state`, FITS senza `OFFSET`, FITS con `OFFSET` da header, FITS con `offset_adu` dichiarato, FITS con `pedestal_adu ≠ 0`} | Stati della tabella di ADR-16.b §b.5 con la definizione di §1, esatti: `removed` dopo `light.bias`, `light.dark`, `light.bias_dark_sub` per ogni light; dopo `light.none` e `light.dark_sub_partial` `known_not_removed` per il RAW e `unknown` per i FITS, con o senza `OFFSET`; piedistallo: `calib.pedestal` |
| T-M3A-C01-02 | a | P-OFFSET: ogni catena del flat × stato dell'offset × `flat_division_offset` presente o assente; default di ogni configurazione | `flat.none`: ammissibile con ogni stato dell'offset e dichiarazione assente; con la dichiarazione, `calib.param_not_applicable.flat_division_offset`. Catene che dividono: ammissibili solo con offset `removed` e dichiarazione assente, o `known_not_removed` con dichiarazione; altrimenti `calib.recipe_not_admissible` (offset `unknown`, o `known_not_removed` senza dichiarazione) o `calib.param_not_applicable.flat_division_offset` (dichiarazione con offset `removed`). Nessun default con divisione su offset non `removed` |
| T-M3A-C01-03 | a | Voce `light_offset` dai DNG di T-M3A-RAW-02 decodificati e da stati in memoria con motivo 2×3, 1×4 e 0×0: nero per posizione del periodo, periodo `lcm(2, rows) × lcm(2, cols)` (dimensione nulla del motivo = 1), `black_applied_by_decoder = none`, identità del decoder | Esatta contro la tabella scritta a mano; dai DNG il nero è uguale al valore scritto nel file |
| T-M3A-C01-04 | c | Su `FIX-SYN-CAL-08` dyadic, per ogni catena di ADR-16.b §b.5: sensibilità all'offset `s` con `Δ = 64`; nero dichiarato `b → b + 16` su tutti gli ingressi RAW | `s` esatto come in tabella; campioni, varianza e maschera bit-identici al variare di `b`. Discrimina la sottrazione di `b_L` in aggiunta al master (campioni che cambiano con `b`), al posto del master (`s = 1` invece di 0) e una doppia sottrazione fra master (`s = −1`) |
| T-M3A-C01-05 | c | Percorso parziale (`light.none` e `light.dark_sub_partial` × ogni catena del flat con `flat_division_offset`): minimo e massimo di `b_L/Q` per canale; `light.none` + `flat.none`: residuo `b_L`. Codici `calib.offset.known_not_removed` e `calib.assumption.decoder_black` nell'uscita. Un canale con tutti i light saturi e flat valido | Dyadic: 0; non dyadic: entro il bound di §6; `b → b + 16`: estremi uguali a `min_i (b_i + 16)/Q_i` e `max_i (b_i + 16)/Q_i` dell'oracle, esatti (dyadic), anche con `b` e `Q` diversi fra pixel dello stesso canale; codici presenti; canale saturo: estremi assenti con `no_valid_pixels` e conteggio 0, gli altri canali riportati |
| T-M3A-C01-06 | a | Light fuori dominio, uno alla volta: fase `calibrated` e `prestacked`; linearità `nonlinear` e assente; unità `electron` e `dimensionless`; layout `rgb` e `multi_plane`; fase e linearità entrambe fuori. Master fuori dominio: bias e `D` con fase `calibrated`; `D'` e flat `calibrated` con fase `prestacked`; `D` con linearità `nonlinear`; `D` con unità `dimensionless`, flat `raw` con unità `electron`, flat `calibrated` con unità `adu`; `D` con layout `rgb`. Controlli positivi: `D'` con fase `calibrated`, flat `calibrated` con unità `dimensionless` | Light: stop dell'analisi con `op.phase_blocked` (anche con entrambe fuori), `op.linearity_blocked`, `calib.unit`, `calib.plane_layout`, nessun esito dell'analisi. Master: `excluded` con `phase`, `linearity`, `unit`, `layout`, analisi completata. Controlli positivi: `usable` |
| T-M3A-C01-07 | a, c | Coverage presente sul light, su un master per ruolo, su un ingresso della ricetta `.a`; coverage tutta a 1; nessun ingresso con coverage | Light: `calib.coverage_input` nell'analisi; master: `excluded` (`coverage`), e nel preflight `calib.coverage_input`; `.a`: `calib.coverage_input`. Zero calcoli negli stop (contatore del `fault_injector`). Senza coverage: uscita senza coverage; il bit `support_incomplete` dell'uscita è uguale all'unione di quello degli ingressi (la calibrazione non lo genera), verificato con un ingresso che lo porta e uno che non lo porta |
| T-M3A-C01-08 | c | Piano di varianza di ogni catena con master usati una volta, variante dyadic con varianze dyadic distinte e non nulle per ogni ingresso | 0 contro la formula di §6 in aritmetica razionale. Un termine omesso cambia il valore, quindi il test lo rileva |
| T-M3A-C01-09 | c | Per ogni catena: pixel invalidi da ingresso, saturazione e risultato non finito, in posizioni note; per le catene che dividono, anche i quattro bit di `Q`; `lin`, `phase`, unità, `variance_scope` dell'uscita | Insieme dei pixel invalidi e bit uguali all'oracle; conteggi per bit nel report uguali; metadati come in §7 |
| T-M3A-C01-10 | c | Otto combinazioni dall'analisi all'esecuzione: per ogni riga di ADR-16.b §b.6 (con `D` e con `D'`, flat `raw`, `corrected_unnormalized`, `calibrated`) la ricetta di default su `FIX-SYN-CAL-08` dyadic e non dyadic | Uscita entro il bound di ADR-16.b §b.12 dall'oracle razionale della ricetta; flat residuo come in tabella delle metriche; metriche nel record |
| T-M3A-C01-11 | c | Ogni catena con 1, 2 e 4 worker e `tile_rows` ∈ {1, 7, altezza} | Uscita, varianza, maschera e report bit-identici |

Gate: G-OFFSET in M3.a con T-M3A-C01-01, 02, 03, 06 e la parte di analisi di 07, insieme a T-M3A-CAL-08 e T-M3A-RAW-02 (nero non sottratto dal decoder). T-CAL-08 al gate M3 con T-M3A-C01-04, 05, la parte di esecuzione di 07, e 08…11. Dato reale: PUB-LFC-01 per le combinazioni ottenibili per omissione dichiarata (`plans/M3.md`, slice c). Senza verità indipendente vale solo per coerenza e report.

Classi delle soglie ([ADR-09.b](ADR-09-algoritmi.md#estensione-adr-09b-algoritmi-p0-di-m3) §b.3): B per T-M3A-C01-01, 02, 03, 06, 07, 09, 11; E per 04, 05, 08, 10; C per il flat residuo delle ricette con un'omissione dichiarata. PUB-LFC-01 è di classe R senza verità: sono criteri di gate solo quelli di classe B o E che non richiedono verità, e le metriche di accuratezza sono di classe C.

## 10. Failure mode, warning e provenance

- Stop, nessun ripiego (NC-21):
  - analisi: §2 e §7;
  - selezione: ADR-16.b §b.9;
  - esecuzione: P-LOST di ADR-16.b §b.8.

  Un light fuori dominio ferma l'analisi; un master fuori dominio è escluso. Nessuno dei due attiva una ricetta diversa.
- **Voce `light_offset`.** Contenuto (ADR-16.b §b.10, completato qui):
  - stato;
  - master che rimuove l'offset (ruolo e digest), assente se nessuno;
  - per un light RAW: nero per posizione del periodo, `black_applied_by_decoder`, identità del decoder (ADR-16.b §b.7);
  - per il percorso parziale: minimo e massimo per canale del residuo dichiarato (§6), assenti con `no_valid_pixels` per un canale senza pixel validi, con l'ipotesi H-BLK.
- **Report.** Riporta le ipotesi H-BLK e «nero come sentinella» per ogni ricetta con ingressi RAW. Livelli di D-16: `warning` per il percorso parziale.
- **Provenance.** Lo stato del decoder consumato e il nero effettivo del light (ADR-03.b §b.2). F-IO-04: l'export della provenance porta l'hash dell'originale e l'offset applicato dal decoder (`none`).

## 11. Ottimizzazione

Ammesse senza cambiare il contratto: blocchi di righe e streaming (ADR-06), percorso SIMD entro ADR-07.a, master in cache per digest, tabella di `b_L` sul periodo calcolata una volta per light. Nessuna ottimizzazione cambia ordine delle operazioni in f64, arrotondamento finale o insieme dei pixel validi. GPU fuori da P0 (ADR-07).

## Limiti dichiarati

- Il prodotto non sottrae il nero del decoder. Un light RAW senza master dell'offset si divide per `Q` solo nel percorso parziale, con residuo dichiarato e non misurato (H-BLK).
- Il residuo dichiarato non comprende la componente termica.
- Unità `electron` e light RGB fuori dal dominio.
- Un ingresso con coverage, anche tutta a 1, è fuori dominio.
- La tabella della varianza di §6 vale con i master usati una volta; le ricette con `B` in due catene hanno il modello di ADR-17.b §b.2.
- L'offset del modello di rumore (`noise_model.offset_adu`, ADR-17.a) resta una dichiarazione della ricetta. Il prodotto non lo ricava da `b_L`.
- I riferimenti di §5 non sono riletti in questa unità.
- C-14 e C-16 devono dichiarare l'accettazione di `calib.offset.known_not_removed`.
