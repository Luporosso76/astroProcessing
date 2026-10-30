# Contratto ADR-07.a: percorsi CPU, dispatch e ambiente in virgola mobile

Stadio: ADR-07.a (registro riga 3400, testo riga 2568; NFR-SIMD-01, riga 2008). Unità: R5 di `plans/M2-residui.md`. Decisioni D19, D20, D23. Stack in [ADR-M1-stack](../decisions/ADR-M1-stack.md) (nessun backend GPU nel prodotto fino a M7). Errori: [modello comune](ADR-05-dag-provenance.md#modello-derrore-comune). Calibrazione in [ADR-16](ADR-16-stati-master.md) e [ADR-17](ADR-17-varianza.md); master in [C-02](C-02-master.md).

## Percorsi

Due percorsi primari, entrambi dichiarati e con tolleranze proprie. Nessuno sostituisce l'altro a runtime.

| Backend | Architettura | Codice |
|---|---|---|
| `cpu_scalar` | qualunque | Compilato senza vettorizzazione |
| `cpu_avx2` | x86-64 con AVX2 | Unità di traduzione con `__attribute__((target("avx2")))`, 4 corsie f64 |

La compilazione usa `-fno-fast-math -ffp-contract=off`; non si usano `-mfma`, `-ffast-math`, `crtfastmath.o`, né istruzioni approssimate (`vrcpps`, `vrsqrtps`) o fuse (`vfmadd*`, `vfmsub*`, `vfnmadd*`, `vfnmsub*`). Nessuna libreria nuova: solo i builtin di GCC (`__builtin_cpu_supports`).

## Dispatch nel preflight

La ricetta contiene `simd` ∈ {`auto`, `scalar`, `avx2`}, senza default implicito. Il preflight legge le capacità della CPU (`__builtin_cpu_supports("avx2")`, che include il supporto del sistema operativo allo stato esteso) e risolve in un percorso concreto:

| Richiesta | Capacità | Esito |
|---|---|---|
| `scalar` | qualunque | `cpu_scalar` |
| `avx2` | AVX2 presente | `cpu_avx2` |
| `avx2` | AVX2 assente | stop `unsupported`, `cpu.avx2_unavailable`; il percorso scalare non viene eseguito |
| `auto` | AVX2 presente | `cpu_avx2` |
| `auto` | AVX2 assente | `cpu_scalar` |

`auto` è una regola dichiarata della ricetta, risolta una volta nel preflight; il risultato è registrato. Il backend risolto compare nel campo `backend` del nodo, entra nella chiave del nodo (ADR-05) e nella provenance, insieme alle capacità lette. Il percorso non cambia durante l'esecuzione. Un resume con percorso diverso da quello del checkpoint dà `resume_mismatch` (NFR-FBK-01).

## Ambiente in virgola mobile (D23)

Stesso per i due percorsi: arrotondamento al più vicino, FTZ e DAZ spenti. Il preflight e ogni worker, all'avvio del task, leggono MXCSR (`_mm_getcsr`: FZ bit 15, DAZ bit 6, RC bit 13-14). Un valore diverso porta a stop `numeric`, `cpu.fp_mode`, senza esecuzione e senza ripristino automatico del registro.

Subnormali in ingresso: valori finiti validi, letti come sono (ADR-03 marca `source_nonfinite` solo sui non finiti). In uscita, regola di rappresentabilità (ADR-16 §Aggiunte di M2.e, ADR-17, `calibration.cpp`: `mul_ok`, `div_ok`, `add_ok`, `convert_finite`). Un prodotto o un quoziente con operandi non nulli e risultato zero o subnormale è un underflow e dà il pixel `result_nonfinite`; lo stesso vale per una somma di termini non negativi non tutti nulli. Una somma o una differenza di valori con segno che dà zero o un subnormale è esatta, per il lemma di Sterbenz e l'underflow graduale, e resta valida: per esempio `l − d`, o la media di `−1, +1`. Ogni risultato non finito dà `result_nonfinite`, salvo `Q`: un `Q` non finito calcolato da input validi dà `q_nonfinite`, non `result_nonfinite` (ADR-16 §7, `calibration.cpp` riga 904). Nella conversione al tipo di uscita, un valore non nullo che diventa zero o subnormale dà `result_nonfinite`, mentre uno zero esatto resta valido. C-02 applica la stessa regola ai master.

## Tolleranze (D20), derivate prima della misura

Simboli: `u = 2⁻⁵³` (f64); `γ_k = k·u/(1 − k·u)`; `η_p` minimo subnormale del formato `p` (f32: `2⁻¹⁴⁹`; f64: `2⁻¹⁰⁷⁴`). Se l'implementazione usa uno schema diverso da quelli dichiarati qui, il bound si ricava di nuovo prima di misurare.

**(a) Kernel elemento per elemento e accumulazioni per pixel lungo i frame dei master: 0 ulp.** Sono vettorizzati fra pixel: stesso ordine per pixel, stessa precisione, arrotondamento al più vicino, nessuna FMA, nessuna istruzione approssimata. Ogni operazione IEEE dà lo stesso risultato correttamente arrotondato. La mediana di un numero dispari di valori è una selezione: esatta e bit-identica fra i percorsi. Con un numero pari è `(z_k + z_(k+1)) / 2` in f64, somma e divisione nello stesso ordine: non esatta in senso matematico, ma bit-identica fra i percorsi perché usa le stesse operazioni IEEE correttamente arrotondate. Riguarda le mediane della calibrazione (`N(Fc)`) e il livello `m_f` dei flat; i master di C-02.a non usano mediane per pixel.

**(b) Riduzioni su pixel** (somme per statistiche globali). Per Higham, *Accuracy and Stability of Numerical Algorithms*, 2ª ed., SIAM 2002, cap. 4, §4.2: `|ŝ − s| ≤ Σ_i γ_(h_i) |x_i|`, con `h_i` il numero di addizioni attraversate da `x_i`.

- Scalare: somma ricorsiva nell'ordine degli indici, accumulatore inizializzato col primo termine: `h_i ≤ n − 1`, `|ŝ_sc − s| ≤ γ_(n−1) Σ|x_i|`.
- AVX2: 4 corsie f64, termine `i` nella corsia `i mod 4`; ogni corsia è ricorsiva sui suoi al più `⌈n/4⌉` termini, inizializzata col primo; riduzione finale a coppie `(c0 + c1) + (c2 + c3)`, profondità 2. Quindi `h_i ≤ ⌈n/4⌉ + 1` e `|ŝ_simd − s| ≤ γ_(⌈n/4⌉+1) Σ|x_i|`.
- Underflow graduale. Ogni arrotondamento al formato `p` segue `fl(x) = x(1 + δ) + ε`, `|δ| ≤ u_p`, `|ε| ≤ η_p/2` (Higham, §2.1). Applicato alle somme aggiunge al più `h · (1 + γ_h) · η_64 / 2` per percorso, con `h` il massimo numero di addizioni attraversate da un termine. È una stima conservativa: un'addizione che va in underflow è esatta, ma il termine resta nel bound.
- Bound congiunto per disuguaglianza triangolare:

```
|ŝ_sc − ŝ_simd| ≤ (γ_(n−1) + γ_(⌈n/4⌉+1)) · Σ|x_i|
                 + ((n−1)(1 + γ_(n−1)) + (⌈n/4⌉+1)(1 + γ_(⌈n/4⌉+1))) · η_64 / 2
```

Il test maggiora `Σ|x_i|` con `fl(Σ|x_i|) / (1 − γ_(n−1))`.

- Ogni operazione successiva alla riduzione (divisione per `N`, conversione a f32) aggiunge `|fl(a) − fl(b)| ≤ |a − b| + u_p(|a| + |b|) + η_p`. Quindi lo scarto ammesso fra i percorsi è almeno un ulp subnormale `η_p` per ogni conversione. Esempio: `a = 2⁻¹⁵⁰ − 2⁻²⁰¹` e `b = 2⁻¹⁵⁰ + 2⁻²⁰¹` danno in f32 `0` e `2⁻¹⁴⁹`, entro `2⁻²⁰⁰ + u_32(|a| + |b|) + 2⁻¹⁴⁹`.

**Classificazione.** Dove lo scarto è 0 ulp, valore e classe (normale, subnormale, zero) coincidono, e con essi il bit della maschera. Sulle riduzioni la classe può differire solo se il valore esatto dista dalla soglia meno del bound congiunto; in quella banda ciascun percorso applica la regola al proprio risultato.

**(c) FMA e istruzioni approssimate vietate** in entrambi i percorsi (T-M2R5-05).

**(d) Contro l'oracle.** Il percorso scalare ha le tolleranze di T-M2E-02, T-M2E-20 e T-M2R2-02. Il percorso AVX2 ha le stesse su (a), e `γ_(⌈n/4⌉+1)` al posto di `γ_(n−1)` su (b).

## Test

| ID | Clausola |
|---|---|
| T-M2R5-01 | Ciascun percorso contro l'oracle (T-M2E-01, 02 e 20; T-M2R2-01 e 02), una volta per percorso. |
| T-M2R5-02 | Scarto fra percorsi: 0 ulp su kernel elemento per elemento, accumulazioni per pixel e mediana; sulle riduzioni entro il bound congiunto di (b). Casi: `n` multiplo e non multiplo di 4, `n < 4`, forte cancellazione, subnormali. |
| T-M2R5-03 | Capacità iniettate senza AVX2 e richiesta `avx2`: stop `unsupported`, zero chiamate al percorso scalare (T-FBK-01). |
| T-M2R5-04 | Il percorso compare in backend, chiave e provenance; resume con percorso diverso: `resume_mismatch`. |
| T-M2R5-05 | `objdump` non trova `vfmadd*`, `vfmsub*`, `vfnmadd*`, `vfnmsub*`, `vrcp*`, `vrsqrt*` nei kernel dei due percorsi. |
| T-M2R5-06 | Subnormali: ingressi f32 `2⁻¹⁴⁹` e `2⁻¹²⁶ − 2⁻¹⁴⁹` presi come validi; prodotti e quozienti con operandi non nulli e risultato subnormale o zero marcati `result_nonfinite`; differenze con segno che danno zero o subnormale (esatte) lasciate valide con bit identici nei due percorsi; conversioni attorno a `2⁻¹⁵⁰` entro il bound con `η_32`; riduzioni con termini vicini alla soglia di underflow f64. Esatta su valori e maschere dei kernel (0 ulp), bound di (b) sulle riduzioni. |
| T-M2R5-07 | FZ, DAZ o RC diversi dal «più vicino» impostati con `_mm_setcsr` prima del preflight, o in un worker tramite il `fault_injector`: stop `numeric`, `cpu.fp_mode`, zero chiamate ai kernel, nessun output, registro non corretto. Con DM spenta e parametri o campioni subnormali (R5.c), lo stesso stop senza segnale ai confini dei job, prima di leggere il nodo, e all'avvio di ogni task, prima di leggere il blocco. Al confine (R5.d) l'arresto è registrato: un job `failed` numeric `cpu.fp_mode` con la chiave del nodo e, sullo store durevole, la sola provenance dell'arresto senza nodo; nessun altro artefatto, nessun segnale nemmeno con tutte le eccezioni smascherate. Nessun `crtfastmath.o` nei simboli e nelle opzioni di link. |

Entrambi i percorsi girano sotto ASan e UBSan. Il throughput è registrato come informazione, senza soglia.

## Aggiunte di R5

Precisano come il prodotto applica il contratto; non cambiano percorsi, tabella del dispatch né tolleranze.

**Ricetta e nodo.** `calibration_recipe` e `master_recipe` hanno il campo `simd`; assente, si ha `invalid_input` con codice `calib.param.simd` o `master.param.simd`. La richiesta entra nei parametri del nodo (`simd`: `auto`, `scalar` o `avx2`), il percorso risolto nel campo `backend` (`cpu_scalar` o `cpu_avx2`). La risoluzione avviene quando si costruisce il nodo (`make_calibration_node`, `make_master_node`) e di nuovo nel preflight del calcolo (`prepare_calibration`, preflight del master). Il job risolve di nuovo la ricetta prima di leggere i file e confronta la chiave ricostruita con quella dello slot. Una richiesta `avx2` su una CPU senza AVX2 dà `unsupported`, `cpu.avx2_unavailable`. Con `auto`, se le capacità cambiano fra la creazione dello slot e il job, cambiano il percorso risolto e la chiave: stop `invalid_input`, `calib.node_mismatch` o `master.node_mismatch`. In entrambi i casi non si esegue alcun percorso e il percorso non cambia.

**Provenance.** I job di calibrazione e del master scrivono `details.cpu` = `{"simd": <richiesta>, "backend": <percorso>, "capabilities": {"avx2": <bool>}}`.

**Kernel dei due percorsi.** Valgono per le due residenze della calibrazione (intera e a blocchi di righe, ADR-06, R3.2), che chiamano lo stesso `calibrate_band`. Sono le righe della calibrazione (`Fc`, `Q`, `Var(Q)`, `Lc`, `C`, `Var(C)` e bit della maschera) e le bande del master (somma, media, quadrati degli scarti, `s²/n`, e per il flat `level_bias_estimate` e il suo rapporto col piano). Rientrano in (a): 0 ulp su valori e bit. Le regole di rappresentabilità (`mul_ok`, `div_ok`, `add_ok`, `convert_finite`) si valutano per corsia con le stesse soglie (`DBL_MIN`, `DBL_MAX`, `FLT_MIN`, `FLT_MAX`). I campioni f32 diventano f64 con una conversione esatta; l'uscita f32 si arrotonda una sola volta, al più vicino. La coda di una riga di larghezza non multipla di 4 passa per lo stesso kernel vettoriale, con le corsie in eccesso scartate. Il kernel scalare sta in un'unità di traduzione compilata con `-fno-tree-vectorize -fno-tree-slp-vectorize`.

**Riduzione su pixel.** `pixel_sum` realizza lo schema (b) in ciascun percorso; T-M2R5-02 e T-M2R5-06 ne misurano il bound su ingressi identici. Il prodotto non la usa: le statistiche di `Fc` per `fc_cv` (ADR-17, `iid_assumption`) restano l'accumulo di Welford in ordine raster comune alle due residenze (ADR-06, estensione R3.2), che devono dare report identici; è lo stesso codice nei due percorsi, quindi `fc_cv` è bit-identico fra i percorsi. **Limite:** nessuna riduzione su pixel del prodotto è vettorizzata. Anche le mediane (`N(Fc)` e `m_f`), la media dei livelli `m_f` e la varianza di `N` usano lo stesso codice nei due percorsi e sono bit-identiche.

**Ambiente FP.** MXCSR si controlla in due punti, sempre prima di qualunque kernel. Il primo è l'inizio del preflight (`prepare_calibration`, preflight del master in `combine_master` e nel job); il secondo è l'inizio di ogni chiamata a `calibrate_rows` e di ogni banda del master, nel thread del worker. Oltre a FZ, DAZ e RC, il controllo richiede attive le sei maschere delle eccezioni (bit 7-12): con un'eccezione non mascherata anche un'operazione su una corsia scartata, per esempio `0/0`, genererebbe un segnale. Lo stop è lo stesso. Il contesto dello stop riporta `mxcsr` in esadecimale. Lo stesso controllo precede anche ogni lettura dei dati (R5.c), in altri due punti. Il primo è il confine dei job (`run_job`, `run_job_files`, quindi `run_branches` e ogni job di import, calibrazione e master), prima di leggere il nodo del job e, nei job di calibrazione e master, prima di serializzare le risorse nel budget. Lo stop è un arresto prima del lavoro (R5.d, contratto in `job.hpp`): si registra senza analizzare né serializzare numeri in virgola mobile e si propaga al chiamante con la stessa categoria, lo stesso codice e lo stesso contesto. La registrazione consiste in un job `failed` numeric `cpu.fp_mode` e, sullo store durevole, nella provenance dell'arresto senza nodo ([ADR-05](ADR-05-dag-provenance.md#provenance)). Il secondo è il worker dello scheduler, all'inizio di ogni task e prima che il task legga o decodifichi il proprio blocco.

**Seam di test (`astro::cpu_test`), solo per i test.** Le capacità iniettate sostituiscono `__builtin_cpu_supports`. Il `fault_injector` del task è chiamato nel thread del worker al punto `cpu.task_start`, prima del controllo di MXCSR; un valore non nullo è l'errno del passo e dà lo stop `io`, `cpu.task_start`, con `errno` nel contesto. I contatori contano le chiamate ai kernel di ciascun percorso, cioè righe, bande e `pixel_sum`. Il seam si imposta prima del job e non cambia durante l'esecuzione.

**Esecuzione dei test.** T-M2R5-01: le suite di calibrazione, del job di calibrazione (residenza intera e a blocchi) e del master (con dentro T-M2E-01, 02 e 20 e T-M2R2-01 e 02) girano una volta per percorso. I test scoperti usano `scalar`; le voci ctest `T-M2R5-01.avx2.*` le rieseguono con `ASTRO_TEST_SIMD=avx2`, e sono anche la regressione di NFR-CPU-01.a sul secondo percorso. T-M2R5-05 e la parte binaria di T-M2R5-07 le esegue `tools/check_simd_binary.sh`, che controlla tre cose: le istruzioni vietate negli oggetti dei kernel (`objdump -d`), il simbolo `set_fast_math` di `crtfastmath.o` nei binari dei test (`nm`) e le opzioni `-ffast-math`, `-Ofast` e `crtfastmath` nelle righe di link.

## Fuori da questo contratto

Backend GPU (M7, ADR-07.b); AVX-512 e NEON; dispatch dinamico a runtime; riduzioni con schemi diversi da quelli dichiarati sopra (richiedono un nuovo bound prima della misura).
