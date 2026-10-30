# Contratto ADR-09: algoritmi P0

Lo stadio `.a` (calibrazione minima, registro riga 3406) sta in [ADR-16](ADR-16-stati-master.md), «Ricetta M2: `dark_matched_flat_dark_flat`», con la decisione nella riga 9 del verbale [M2-entry](../decisions/M2-entry.md). Questo file non lo ripete e contiene l'estensione `.b`.

## Estensione ADR-09.b: algoritmi P0 di M3

Stadio: ADR-09.b, «Algoritmi P0» (registro riga 3407; aggregato ADR-09 riga 3405; testo righe 2570 e 2592). Decisione collegata: [D-09.b](../decisions/D-09-soglie.md) (riga 3461; aggregato D-09 riga 3459; testo riga 2875). Archi: ADR-09.b `requires_all` ADR-09.a e `decided_by` D-09.b (righe 4091, 4092). Sono `decided_by` ADR-09.b i contratti C-01.b, C-02.b, C-03, C-04, C-05, C-07, C-08, C-12, C-13, C-14 e C-15 (righe 4112, 4115-4118, 4120, 4121, 4127-4130). Piano: §8.11 (righe 1129-1137), §8.13 (1153-1172), §10.1 (1335-1353), campi 4 e 9 di §10.2-§10.16, §13.14-§13.15 (2203-2228), §15.4 (2376-2389). Prima slice che lo usa: M3.b.

Ogni clausola è una **regola** (vincolo verificato dal test o dalla review indicata), un **fatto** (con derivazione o fonte) o un'**ipotesi** (dichiarata, con il limite che la riguarda).

Fuori da ADR-09.b c'è la formulazione delle famiglie decise da altri ADR: C-06 (ADR-17.b), C-09 e C-16 (ADR-10; C-16 anche D-20), C-10.b e C-18 (ADR-08.b; C-18 anche D-20), C-11 (ADR-03.b), C-17 e C-19 (ADR-15). La regola delle soglie di §b.3 (D-09.b) vale invece per il campo 9 di ogni contratto C-NN di M3. D-09 chiede infatti soglie «per algoritmo», da fissare «prima di ogni funzione» (riga 2875).

### b.1 Una sola formulazione P0 per famiglia

Regola. La formulazione di una famiglia comprende:

1. lo stimando o la trasformazione, con unità e modello del dato;
2. i metodi P0, ciascuno definito per intero nel contratto della famiglia e con soglie proprie nel campo 9;
3. la regola che fissa il metodo per ogni ingresso prima dell'esecuzione. Può essere la scelta esperta dichiarata nella ricetta o la proposta a priori scritta nel contratto (come C-02.b §b.4 e D-16).

Ogni famiglia ha una sola formulazione P0. Più metodi stanno nella stessa formulazione solo se stimano la stessa grandezza con lo stesso modello e il campo 4 del piano li elenca (C-02 e C-07). Un metodo con un altro stimando o un altro modello è un'altra formulazione: non è P0 e senza un contratto approvato non entra in implementazione (§10.1, riga 1337). Dopo la selezione un guasto non cambia metodo, parametri o formulazione: il ramo si ferma (NC-21).

Regola per la scelta fra candidati. In alcune famiglie il campo 4 del piano fa dipendere la scelta da una misura: C-03 («errore colore, aliasing e risoluzione»), C-04 («crowding e SNR»), C-08 («residuo e protezione del segnale»). Lì il contratto della famiglia:

- fissa prima del confronto metriche, soglie (§b.3), fixture e criterio di scelta;
- confronta i candidati solo su fixture di sviluppo, con generatori o seed disgiunti da quelli del gate (§13.15, punti 1 e 6);
- registra il confronto e nomina un solo metodo P0. Gli altri candidati restano fuori da P0.

### b.2 Famiglie di M3 e formulazione P0

| Slice | Operatori | Famiglia, deciso da | Formulazione P0 |
|---|---|---|---|
| a | Decoder RAW, classificazione, ledger dell'offset, ricollegamento, crop | ADR-08.b, ADR-03.b; C-01.b per G-OFFSET (ADR-16.b, ADR-09.b) | Nessuno stimatore numerico: decodifica del mosaico grezzo senza sottrazione del nero (ADR-08.b), stato dell'offset di C-01.b §1. Soglie di classe B ed E |
| b | Master | C-02.b; ADR-09.b | Stima per pixel della grandezza comune `μ` di un gruppo compatibile, `x_f = μ + e_f`; metodi di C-02.b §b.1 con la proposta a priori di §b.4. Flat: `single` e `mean`, più l'unità di §b.6 |
| c | Calibrazione del light | C-01.b; ADR-09.b, ADR-16.b | Modello `L = B + t d + q S + ε`; catene e ricette di ADR-16.b §b.5-§b.6, scelte prima dell'esecuzione (D-16); varianza di ADR-17.b. Il dark scalato non è P0 (F-CAL-05, P1) |
| d | Debayer | C-03; ADR-09.b | Interpolazione lineare a pesi fissi per fase CFA: ogni campione d'uscita è `Σ w x`, con pesi che non dipendono dai dati. Un solo kernel P0, scelto in C-03 secondo §b.1. Motivo: con righe fisse varianza e blocco fra canali sono esatti (ADR-17.b §b.1, §b.4); con pesi che dipendono dai dati (metodi edge-aware) sarebbero solo a pesi congelati, con errore non misurato (ADR-17.b §b.13). Solo Bayer; X-Trans rinviato con dichiarazione (F-CAL-08) |
| e | Stelle e metriche PSF | C-04; ADR-09.b | Sorgenti puntiformi isolate su fondo liscio, con il rumore dal piano di varianza. Detection: soglia di significatività sull'immagine filtrata con un kernel lineare fisso. Misura: fit ai minimi quadrati pesati, in f64, di un profilo di una famiglia dichiarata, sui pixel non saturi, con incertezze dalla covarianza del fit. Kernel e famiglia del profilo li fissa C-04 secondo §b.1. Il deblending non è P0 (§10.5, campi 1 e 3). Motivo: la base della tolleranza di M3.e è il limite di Cramér–Rao (`plans/M3.md`, slice e), riferito a uno stimatore del modello. Con rumore gaussiano indipendente di varianza nota, i minimi quadrati pesati con l'inversa della varianza sono la massima verosimiglianza (fatto) |
| f | Registrazione | C-05; ADR-09.b | Corrispondenze per triangoli e stima robusta con seed fisso, poi rifit ai minimi quadrati in f64 sugli inlier, con covarianza dei parametri `σ² (AᵀA)⁻¹`. Il modello si sceglie da una lista annidata dichiarata, con un criterio sui residui fissato in C-05. Una soluzione ambigua non si pubblica |
| g | Resampling | C-06; ADR-17.b, ADR-10 | `y = R x` con interpolatore selezionabile (tabella 8.6.1), `C_out = R C Rᵀ` (ADR-17.b §b.5): formulazione di C-06, non di questo ADR |
| h | Stacking | C-07; ADR-09.b | `Ŝ = Σ w x / Σ w`, con pesi fissati prima della combinazione e rejection condizionata a `N` (nessuna con `N` = 1, 2). Metodi e proposta a priori secondo `N` come C-02.b. Varianza di ADR-17.b §b.6; `Neff = (Σ w)² / Σ w²`. Con pesi stimati dai dati la varianza è a pesi congelati, con `variance.fixed_weight_approximation` (ADR-17.b §b.1) |
| k | Curve, bilanciamento, asinh | C-09; ADR-10 | Asinh cromatico `C' = g(I) C` (§10.10): formulazione del piano, contratto deciso da ADR-10 |
| j | Maschere | C-11; ADR-03.b | Nessuno stimatore numerico: rasterizzazione versionata, bit-esatta |
| i | Fondo | C-08; ADR-09.b | Modo additivo o divisivo dichiarato. Modello lineare nei parametri su una base fissa, con fit ai minimi quadrati pesati sui campioni scelti in modo deterministico fuori dalle maschere di protezione. Astensione quando il problema non è identificabile (rango, condizionamento o numero di campioni sotto il criterio di C-08). La base la sceglie C-08 secondo §b.1. Motivo: con campioni, maschere e pesi che non dipendono dal rumore della stessa immagine, lo stimatore è lineare nei dati. La covarianza del fondo stimato è allora `(AᵀWA)⁻¹ AᵀW C W A (AᵀWA)⁻¹`, che con `W = C⁻¹` si riduce a `(AᵀWA)⁻¹` (fatto). Se selezione o pesi dipendono dai dati, la stessa formula è un'approssimazione a selezione e pesi congelati, con `variance.fixed_weight_approximation` (ADR-17.b §b.1). L'identificabilità con campioni fissati è una condizione di rango, verificabile prima del fit |
| l | Denoise | C-12; ADR-09.b | À trous non decimata con soft shrinkage dei coefficienti (§10.13). Soglia per scala e per pixel `t_j = k σ_j`, con `k` dichiarato nella ricetta e `σ_j` dal modello di rumore. Nel modello lineare `σ_j` viene dalle righe della trasformata e dal piano di varianza; nel non lineare da `Jf` o da un modello empirico con confidenza ridotta (§8.14). La soglia MAD stimata dagli stessi dati non è P0 (§8.13: «procedura da validare interamente») |
| m | Sharpening e contrasto locale | C-13; ADR-09.b | `x̂ = x + Σ_j g_j w_j` sulla piramide non decimata di §10.14, con `g_j` limitati e dichiarati; contrasto locale con maschera di protezione |
| n | Riduzione e separazione stellare | C-14; ADR-09.b | Fit della PSF solo sull'antenato lineare `I_L` (detection e fit di C-04). Riduzione `I_L − α S_L` con `α ∈ [0, 1]`; separazione `R_L = I_L − S_L`, con `R_T + S_T = T(I_L)`. Gate separati (§10.15.1, §10.15.2) |
| o | Deconvoluzione | C-15; ADR-09.b, D-19 | Minimi quadrati regolarizzati a PSF fissa sul sottospazio valido: `W` PSD, `b` fisso stimato a monte, `λ` e arresto preregistrati, baseline no-op (§10.16, riga 2570). Richardson–Lucy e Wiener non sono P0 |
| p | Solver assistito | C-18; ADR-08.b, D-20 | Formulazione di C-18 |
| q | Colore relativo | C-16; ADR-10, D-20 | Formulazione di C-16 |
| r | Advisor | C-17, C-19; ADR-15 | Formulazione di C-17 e C-19 |
| s | Export | C-10.b; ADR-08.b | Nessuno stimatore numerico: roundtrip bit-esatto |

### b.3 Regola delle soglie (D-09.b)

Regola. Ogni soglia di un campo 9 appartiene a una delle classi seguenti e si fissa prima della prima misura che la usa, con la derivazione scritta.

**B, binaria.** L'esito atteso è definito senza tolleranza: codici, stop con 0 calcoli, appartenenza a un insieme, bit-identità, metadati. La soglia è l'uguaglianza.

**E, oracle esatto.** Il valore atteso si calcola esattamente dagli ingressi della fixture: l'oracle riesegue la sequenza di operazioni fissata dal contratto. Tolleranza:

1. 0, dove l'oracle verifica che ogni operazione è esatta;
2. altrimenti la regola I di ADR-17.b §b.10, estesa dalle varianze a ogni grandezza: intervalli su razionali e fattore `[1 − u, 1 + u]` per ogni operazione con arrotondamento corretto IEEE 754 (`+`, `−`, `×`, `÷`, `sqrt`, `fma`, con estremi razionali che racchiudono il risultato esatto). Per le statistiche d'ordine si prendono gli estremi come in ADR-17.b. Si aggiunge mezzo ulp del tipo d'uscita se è f32;
3. in alternativa al punto 2, un bound analitico derivato nel contratto con la sua fonte e dimostrato contenere l'errore, per esempio `γ_m` di Higham (ADR-16.b §b.12) o il bound di una FFT o di un risolutore.

Una funzione senza arrotondamento corretto garantito (per esempio `exp`, `log` o `asinh` di libreria) allarga l'intervallo di `[1 − 2ku, 1 + 2ku]`, con `k` il bound in ulp dichiarato nel contratto con la sua fonte. Fatto: per un risultato normale, 1 ulp è al più `2u|x|`. Senza una fonte per `k` il punto 2 non si applica, e il contratto deriva un altro bound prima della misura. Le condizioni della regola I (nessun underflow, nessun denominatore il cui intervallo contiene 0) le verifica l'oracle. Un caso che non le soddisfa esce dalla fixture prima della misura ed è contato nel record. Ogni percorso di ADR-07.a ha il bound della propria sequenza di operazioni; un percorso non eredita il bound dell'altro allargato a posteriori.

**D, discretizzazione con verità analitica.** La verità è una grandezza analitica della scena: integrale sull'area del pixel, risposta all'impulso, scena band-limited. Tolleranza: bound analitico dell'errore del metodo sulla classe della fixture, con fonte o derivazione, più la parte E. Le due parti sono dichiarate separate (`plans/M3.md`, slice g). L'errore di discretizzazione è zero solo dove il metodo è esatto per costruzione.

**S, statistica.** Monte Carlo con verità nota.

- Seed, `K`, `P` e parametri sono fissati prima della misura e scritti nel record.
- `α = 10⁻³` per test, con Bonferroni sulle `m` statistiche del test, come C-02.a §9, C-02.b §b.9 e ADR-17.b §b.12.
- Il criterio usa la distribuzione esatta della statistica oppure una maggiorazione che non presume la forma della distribuzione:
  - distribuzioni esatte: χ² per varianze campionarie gaussiane, binomiale con intervallo di Clopper–Pearson per le proporzioni;
  - maggiorazioni: Chebyshev, con varianza esatta o maggiorata a priori con derivazione; Hoeffding per variabili limitate.

  Le approssimazioni asintotiche (normale, Wilson–Hilferty) non fissano un criterio `.b`. I quantili si calcolano con errore dichiarato, trascurabile rispetto ad `α/m` (C-02.a §9).
- L'errore deterministico del prodotto (classe E) entra nel criterio come margine, come `Δ` di MC-lin. Un errore del prodotto non allarga l'intervallo.
- Se la varianza della statistica non è né nota né maggiorabile, il criterio si scrive su una proporzione: un evento con probabilità nota sotto il modello, con intervallo binomiale esatto.
- **Potenza.** Un criterio S che è l'unica prova di una proprietà P0 dichiara l'alternativa da rifiutare (un modello `V_0` o uno scarto minimo `δ`). `K` e `P` si scelgono prima della misura perché la probabilità di non rilevarla sia ≤ `β`. `β = 10⁻³`, salvo un valore più piccolo fissato dal contratto (10⁻⁶ in ADR-17.b). La probabilità si calcola con la stessa distribuzione o maggiorazione del criterio. Un criterio S senza potenza dichiarata è un **controllo di coerenza**: accompagna una prova di classe E o un fatto derivato e non chiude da solo un gate.
- MC-lin (ADR-17.b §b.12) è il protocollo S per la varianza degli operatori lineari.

**P, requisito di prestazione.** Vale dove non esiste un bound dimostrato: accuratezza di un fit rispetto al limite di Cramér–Rao, completezza e falsi positivi della detection, aliasing, aloni, ringing, danno rispetto al no-op. La soglia è un riferimento moltiplicato per un margine dichiarato:

- il riferimento è un fatto derivato dal modello della fixture: limite di Cramér–Rao, probabilità di coda del rumore, risposta analitica, metrica della baseline no-op;
- il margine è una scelta del contratto, motivata e scritta prima della misura, non un fatto.

La verifica usa la classe S, o la E se la metrica è deterministica. Un budget P non si riduce a un RMS. Copre bias, conservazione del flusso, code, ringing, bordi e arrotondamento senza rumore, oppure dichiara quale contributo non si applica (§8.11, riga 1133). Per gli operatori fotografici il confronto con il no-op è obbligatorio (§10.15.1, §10.16).

**R, dati pubblici.**

- Su un dato pubblico sono criteri di gate le classi B ed E su grandezze che non richiedono verità: bit-identità, determinismo, invarianti come `R_T + S_T = T(I_L)`, oracle ricavato dagli header come in T-M3B-C02-16.
- L'accuratezza su un dato pubblico è un criterio di gate solo con un riferimento indipendente dichiarato, per esempio un catalogo (C-16, C-18). L'incertezza del riferimento entra nel criterio S e i suoi limiti sistematici nel record. Senza riferimento la grandezza è di classe C.
- I dati del gate non scelgono metodi, parametri o soglie (§13.15, punto 6). Un parametro misurato su dati pubblici entra nel modello di una fixture solo se è misurato su dati disgiunti da quelli che verificano la stessa grandezza.

**C, caratterizzazione.** Misura senza soglia: il record riporta il valore con il metodo e la sua incertezza, oppure come stima descrittiva. È ammessa solo per una grandezza dichiarata come limite nel contratto e non richiesta da un gate del piano. Una soglia introdotta in seguito su quella grandezza si verifica su realizzazioni nuove (seed o dati diversi), non su quelle già misurate.

**Regole comuni.**

- Una soglia sta nel contratto approvato prima della prima misura che la usa. Seed, `K` e `P` stanno nel contratto o in un protocollo preregistrato committato prima della misura (§b.4). Il record del test cita il commit del contratto e del protocollo.
- Nessuna soglia si allenta dopo una misura. Cambiare seed, `K`, `P`, fixture o casi esclusi dopo un esito è un allentamento. Una soglia irraggiungibile per un errore della derivazione è un'assunzione fallita: il gate della funzione resta FAIL e decide il coordinatore (AGENTS.md, «Fallback»), senza declassare la funzione P0.
- Una soglia si stringe solo con una revisione del contratto, e la versione più stretta si verifica su realizzazioni nuove.
- Fixture e oracle sono scritti da chi non scrive l'operatore, con modello analitico e senza codice del prodotto (`docs/test-matrix.md`, «Regole»).
- Fatto (disuguaglianza di Boole): su `T` test S la probabilità che almeno uno fallisca con prodotto e modello corretti è ≤ `T · 10⁻³`, anche con test dipendenti. Un fallimento si indaga e non si ripete con altri seed. Se l'indagine non trova un difetto, il record lo dichiara e decide il coordinatore.

### b.4 Contenuto del campo 9

Regola. Per ogni soglia il campo 9 di un contratto C-NN di M3 riporta (§10.1 campo 9, §15.4):

1. stimando e unità; metrica e dominio di validità;
2. classe (§b.3), con derivazione (fatto) e margine (scelta) separati;
3. valore o formula della soglia;
4. fixture, generatore indipendente e verità. Per la classe S anche `α`, `m`, distribuzione o maggiorazione, margine deterministico, e l'alternativa con la potenza oppure la dicitura «controllo di coerenza». Seed, `K` e `P` stanno nel campo 9 o in un protocollo preregistrato che il campo 9 richiama: un file versionato, con commit anteriore alla prima misura (T-M3-ADR09-01);
5. test e gate;
6. comportamento in astensione e negli stop (classe B);
7. costo: prenotazioni del pool entro il budget (ADR-06). Il tempo non ha soglia: è una baseline del benchmark (`plans/M3.md`, «Benchmark»).

Regola per i contratti ancora da scrivere, compresi quelli decisi da altri ADR. Il campo 9 contiene almeno questi criteri, nelle classi indicate; i numeri si fissano nel contratto.

| Famiglia | Criteri e classi |
|---|---|
| C-03 | Implementazione E: l'oracle applica il kernel in aritmetica razionale, con 0 sui casi esatti. Errore colore, aliasing e risoluzione P, calcolati in modo deterministico dalla risposta del kernel sulla fixture. Varianza e blocco fra canali con T-M3C-VAR-05. Pattern ignoto e crop incoerente B. PUB-CFA-01: R senza verità |
| C-04 | Completezza e falsi positivi S su proporzioni (Clopper–Pearson), con il riferimento P dal modello di rumore della fixture. Centroide, FWHM e parametri del profilo P rispetto al limite di Cramér–Rao calcolato dall'oracle. Stelle sature escluse, ordine deterministico, errore in assenza di stelle B. PUB-LFC-01 e PUB-REG-01: R senza verità |
| C-05 | Recupero dei parametri S, con la covarianza `σ² (AᵀA)⁻¹` propagata dai centroidi della fixture. Insieme degli inlier B dove gli outlier sono separabili per costruzione. Ambiguità e falsa soluzione S su proporzioni. PUB-REG-01: R senza verità |
| C-07 | Valori dei metodi E, come C-02.b. Varianza S (MC-lin, T-M3C-VAR-08). `Neff` e invarianza al riscalamento comune dei pesi E. Casi `N` = 1, 2, 3 e frame perso B. Rejection S, con potenza o come controllo di coerenza. PUB-REG-01: R |
| C-08 | Fondo nello spazio della base, senza rumore: E, con il bound del risolutore. Con rumore: S, con la covarianza esatta se selezione e pesi non dipendono dal rumore; altrimenti S sull'approssimazione a selezione congelata, con il suo errore misurato o dichiarato come limite. Perdita di segnale nelle zone protette P rispetto al segnale iniettato. Casi non identificabili B (astensione, nessuna correzione) |
| C-12 | Trasformata e ricostruzione senza shrinkage E. Bias, varianza residua e NPS S, con scena e rumore noti e repliche indipendenti. Perdita di struttura debole e distorsione delle stelle P rispetto al no-op. Rumore non stimabile B (astensione) |
| C-13 | Con `g_j` fissi e senza maschera l'operatore è lineare: risposta all'impulso e al gradino e guadagno del rumore E. Aloni, ringing, clipping e contrasto locale P rispetto al no-op. Guadagno fuori limite B |
| C-14 | `R_T + S_T = T(I_L)` E. Flusso del fit P rispetto al limite di Cramér–Rao, verificato in S. Danno non stellare, cambio di colore e deformazione delle stelle residue P rispetto al no-op. Antenato lineare assente B. Gate di riduzione e di separazione separati |
| C-15 | Soluzione contro l'oracle denso di `plans/M3.md` (slice o): E, con il bound analitico dal residuo e dal condizionamento. Sensibilità a PSF, `b`, `λ` e rumore P rispetto al no-op, secondo il protocollo preregistrato (§10.16). PSF invalida, `W` non PSD e budget insufficiente B. PUB-DEEP-01: R senza verità |
| C-06 | Segnale, flusso e brillanza D più E, con le due parti separate (`plans/M3.md`, slice g). Varianze e covarianze E (T-M3C-VAR-06) e S (MC-lin, T-M3C-VAR-07). Coverage, bordi, WCS non rappresentabile B |
| C-09 | Valori E contro il riferimento in `long double` scritto dal test, con bound in ulp. Monotonicità, `k = 0` identità, rapporti RGB nel dominio pre-gamut, negativi e parametri fuori dominio B o E. Banding e clipping P, se il contratto li usa come criterio |
| C-10.b | Roundtrip FITS e TIFF bit-esatto B, letto da un lettore diverso dal writer. Writer in errore e disco pieno B |
| C-11 | Raster bit-esatto dopo riapertura B. Profilo di feather E contro il profilo analitico. Ciclo e asset mancante B |
| C-16 | Fattori di colore S contro il catalogo simulato, con l'errore fotometrico propagato. Su dato reale R, con il catalogo come riferimento indipendente e la sua incertezza nel criterio; residui su holdout. Pack mancante, WCS invalido, campo fuori footprint B |
| C-17, C-19 | Copertura utile, falsi positivi, falsi negativi e astensione S su proporzioni nel corpus, con corpus di accettazione disgiunto (R, §13.15-§13.16). Danno e beneficio P rispetto al no-op. Budget esaurito, candidato non valutabile, holdout riusato B |
| C-18 | Errore di centro e scala S dalla covarianza dei match. Falsa soluzione S su proporzioni (Clopper–Pearson). Pack mancante, corrotto o fuori copertura B. Estratto 2MASS: R con il catalogo come riferimento |

### b.5 Verifica di C-01.b e C-02.b

- **C-01.b §9: conforme**, con le modifiche minime che seguono. Le soglie sono di classe B (T-M3A-C01-01, 02, 03, 06, 07, 09, 11) ed E (04, 05, 08, 10). Il flat residuo delle ricette con omissione dichiarata è di classe C. Mancavano due cose: la soglia della varianza sulla variante non dyadic, che è T-M3C-VAR-01 con la regola I, e la classe del dato reale. Modifiche:
  - riga «Rumore» della tabella delle metriche;
  - riga delle classi in §9;
  - §6, voce «Varianza», e «Limiti dichiarati» allineati ad ADR-17.b;
  - tolta la voce «ADR-09.b non è scritto».
- **C-02.b §b.9: conforme**, con le modifiche minime che seguono e un limite dichiarato. Classi:
  - E: T-M3B-C02-01, 13 e l'oracle di 11; E con intervalli: 02;
  - S come controllo di coerenza: 03 e 04, perché le proprietà che verificano sono fatti derivati in §b.6, e l'aritmetica la verificano esattamente 01 e 05;
  - P: 05, con accuratezza richiesta 10⁻⁹ e oracle di quadratura con errore stimato (limite di §b.14);
  - C: 06 e 07;
  - B: 08, 09, 10, 12, 14, 15;
  - R con oracle dagli header: 16.

  Mancavano il margine deterministico nei criteri S di 03 e 04 e la sede preregistrata di seed, `K` e `P`. Modifiche: riga delle classi, margini e protocollo preregistrato in §b.9. Limite: la varianza di Chebyshev di 04(b) usa `μ_4` calcolato per quadratura, con errore stimato e non dimostrato; la copertura `α/m` di 04(b) vale sotto l'ipotesi che la stima sia corretta (C-02.b §b.14). 04 è un controllo di coerenza: nessuna proprietà P0 dipende solo da esso.

  Modifiche: riga delle classi in §b.9; questione dei flat e voce su ADR-09.b in §b.15, nell'elenco «Fuori da C-02.b» e in §b.14, aggiornate secondo §b.6.

### b.6 Normalizzazione e metodi robusti dei flat

Decisione. Appartengono a F-CAL-01.b (slice M3.b), in un'unità di contratto `C-02.b-flat` dello stadio M3.entry. Non sono un limite del P0.

- Motivo. F-CAL-01 (riga 1840) chiede combinazione, statistiche e rejection configurabili per bias, dark, flat e dark-flat. §10.3, campo 1 (riga 1375), chiede di combinare i frame «con rejection e normalizzazione dichiarate». Un flat limitato a `single` e `mean` lascia fuori una parte di un requisito P0, e una funzione P0 mancante non chiude un gate (AGENTS.md, «Autonomia»). F-CAL-03.b (M3.c) prepara `Q` dal master e non combina frame, quindi non è la casa giusta.
- Perimetro dell'unità:
  - normalizzazione dei frame del flat a un livello comune per canale, con scale stimate dai dati (`median_ratio`);
  - metodi di C-02.b §b.1 sui flat normalizzati;
  - varianza del master con il termine delle scale stimate, derivato o maggiorato;
  - soglie secondo §b.3.
- Formulazione: la stessa di C-02. Lo stimando del master flat è la risposta per pixel a meno di un fattore di scala globale. Fatto: `Q = Fc / N(Fc)` non cambia se `Fc` si moltiplica per una costante positiva, perché la mediana è equivariante per scala. La normalizzazione dei frame resta un passo del metodo, e non un'altra formulazione, se il master normalizzato differisce da quello a livelli uguali solo per un fattore globale. L'unità lo dimostra, oppure dichiara l'ipotesi che lo regge (per esempio sul termico dei frame del flat).
- Ordine: prima del codice di M3.b che tocca flat normalizzati o con metodi robusti; il resto di M3.b procede. F-CAL-01.b non si chiude senza l'unità.

### b.7 Test pianificati

| ID | Verifica | Criterio |
|---|---|---|
| T-M3-ADR09-01 | Al gate M3, per ogni contratto C-NN di M3 e per ogni soglia del campo 9: classe di §b.3, derivazione o margine, valore. Dalla storia di git: commit del contratto che fissa la soglia e del protocollo preregistrato, primo record di misura del test, modifiche successive alla soglia o al protocollo | Binario per soglia: campi presenti; commit della soglia e del protocollo anteriori al primo record; nessuna modifica che li allenti dopo quel record |

### b.8 Riferimenti indipendenti e stato

Nessuno dei testi è stato riletto in questa unità. La regola non dipende da un loro valore numerico.

- Aritmetica degli intervalli: come ADR-17.b §b.10 (Moore, Kearfott e Cloud, SIAM 2009). Errore d'arrotondamento: Higham, *Accuracy and Stability of Numerical Algorithms*, 2ª ed., SIAM 2002. Arrotondamento corretto: IEEE 754-2019.
- Intervallo binomiale esatto: C. J. Clopper, E. S. Pearson, «The use of confidence or fiducial limits illustrated in the case of the binomial», *Biometrika* 26(4), 1934.
- Variabili limitate: W. Hoeffding, «Probability inequalities for sums of bounded random variables», *Journal of the American Statistical Association* 58(301), 1963.
- Limite di Cramér–Rao ed efficienza della massima verosimiglianza: C. R. Rao, «Information and the accuracy attainable in the estimation of statistical parameters», *Bulletin of the Calcutta Mathematical Society* 37, 1945; H. Cramér, *Mathematical Methods of Statistics*, Princeton University Press 1946.
- Selezione e leakage: piano §13.14-§13.15.

Nessuna derivazione da software esistente (DEC-02, NC-15).

### b.9 Limiti dichiarati

- Il margine di una soglia P è una scelta, non un fatto: superarla mostra che il prodotto soddisfa il requisito, non che il requisito sia il migliore possibile.
- La probabilità di falso fallimento è controllata per test, non per gate (§b.3, disuguaglianza di Boole).
- I test `.a` di M2 restano con le soglie approvate da D-09.a, comprese le approssimazioni dichiarate allora (Wilson–Hilferty in M2.e). Rieseguiti in M3, le conservano.
- La scelta del metodo in C-03, C-04 e C-08 è rinviata ai loro contratti; qui c'è solo la regola.
- Per le funzioni di libreria senza arrotondamento corretto il bound in ulp e la sua fonte li sceglie il contratto che le usa: oggi nessuna fonte è scelta.
- Nessun test è eseguito.
