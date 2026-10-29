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
| T-M2R5-07 | FZ, DAZ o RC diversi dal «più vicino» impostati con `_mm_setcsr` prima del preflight, o in un worker tramite il `fault_injector`: stop `numeric`, `cpu.fp_mode`, zero chiamate ai kernel, nessun output, registro non corretto. Nessun `crtfastmath.o` nei simboli e nelle opzioni di link. |

Entrambi i percorsi girano sotto ASan e UBSan. Il throughput è registrato come informazione, senza soglia.

## Fuori da questo contratto

Backend GPU (M7, ADR-07.b); AVX-512 e NEON; dispatch dinamico a runtime; riduzioni con schemi diversi da quelli dichiarati sopra (richiedono un nuovo bound prima della misura).
