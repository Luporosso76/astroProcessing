# M3.entry, C-02.b-flat: normalizzazione e metodi robusti dei flat

Stato: **APPROVATO** (review `c02bflat-round1` PASS). Unità: M3.entry, estensione C-02.b-flat per la slice M3.b, assegnata da ADR-09.b §b.6. Solo testo: nessun codice di prodotto, nessun test eseguito.

| Campo | Valore |
|---|---|
| Unità | M3.entry / C-02.b-flat |
| Scopo | Fissare prima del codice la normalizzazione `median_ratio` dei flat e i metodi di C-02.b §b.1 sui flat normalizzati, con il piano di varianza, le scale stimate, i domini, gli stop, i test e le soglie; dimostrare il fattore globale o dichiarare l'ipotesi che lo regge |
| Comando | `python3 -S tmp/c02bflat_algebra.py`; `tools/review.sh c02bflat-round1 tmp/c02bflat-facts-round1.md`; `python3 -S tools/evidence.py docs/evidence/M3-entry-C02bflat.md` |
| Input | Piano attivo (SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`): §10.1 (righe 1335-1352), §10.3 C-02 (1371-1385, campo 1 riga 1375), F-CAL-01 (1840), appendice D righe 3180 e 3431. Contratti C-02.a, C-02.b, ADR-09.b (§b.3, §b.4, §b.6), ADR-16.a/b (§6, §b.2, §b.5), ADR-17.b (§b.1, §b.6, §b.10), C-01.b; D-09.b |
| Versioni | Worktree da `05ab379` |
| Ambiente | Linux 7.0.0-38-generic x86_64, macchina di sviluppo |
| Risultato | Sezione «Estensione C-02.b-flat» in `docs/contracts/C-02-master.md` (§f.1-§f.15); 9 ID pianificati `T-M3B-FLAT-01…09` in `docs/test-matrix.md`; riga M3.b di `plans/M3.md` e riga C-02 di `docs/contracts/README.md` aggiornate |
| Output | File elencati nel blocco generato sotto; artefatto di review in `.omc/artifacts/ask/` |
| Ambito | Contratto. Fuori: codice (M3.b), normalizzazione con dark-flat sottratto per frame (altro stato del master), proposta a priori con normalizzazione |
| Esito | Review PASS al giro 1. Vedi «Review» |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Decisioni

- `median_ratio` per canale: livelli `m_(f,c)` di C-02.b §b.6, riferimento armonico `m_H,c`, scale `r_(f,c) = m_H,c / m_(f,c)`, normalizzazione attorno a `offset_adu`. Con il riferimento armonico `Σ r = n`.
- Fattore globale: per `mean` dimostrato condizionato alle scale, senza ipotesi sui livelli né sul termine additivo (`E[M | r] = b + λ_c g`). Per mediana, winsorized e media pesata, sotto `level_proportional` e `additive_bounded` (`|b − offset_adu| ≤ A`, ipotesi su struttura del bias e termico del flat), lo scarto è al più `ρ_c A`, controllato da un budget dichiarato (`master.flat_norm_budget`). Per `sigma_clip` (soglie uguali obbligatorie sul flat) lo stesso limite vale nell'approssimazione della selezione congelata.
- Metodi robusti sul flat solo con `median_ratio`: senza normalizzazione il coefficiente di `g` dipende dal rapporto segnale/rumore del pixel (controesempio, fatto 3).
- Piano: `Var(M | r)` con gli stimatori di C-02.b sugli `y_f`; termine dei livelli limitato (`norm_level_bias_bound`); termine delle scale omesso e dichiarato con `variance.fixed_weight_approximation`. Per `mean` è una fluttuazione del fattore globale, che in `Q` entra al secondo ordine. Piano di mediana, winsorized e `sigma_clip` approssimato su campioni eteroschedastici, con dominio `max m ≤ 4 min m` e caratterizzazione T-M3B-FLAT-05.
- CFA: un fattore per canale. Con `global_median` in calibrazione un solo fattore richiede `same_illumination_color`.
- Proposta a priori invariata: `median_ratio` è solo una scelta esperta, perché richiede `flat_additive_bound_adu`.

## Questioni aperte

Chiusa: normalizzazione dei flat e metodi robusti sui flat (ADR-09.b §b.6, C-02.b §b.15). In `plans/M3.md` non era più nell'elenco delle questioni; la riga M3.b ora rimanda all'estensione. Nessuna nuova questione.

## Verifiche dell'autore

- `python3 -S tmp/c02bflat_algebra.py` (SHA-256 `e613f23a2adec3bd7037e5d5d3d2d674fddd1327620705ed19b86f1428309749`), uscita 0. In aritmetica razionale controlla: `Σ r = n`; `(M − b)/g` costante per la media normalizzata con `β` qualunque; il controesempio del riferimento aritmetico; campioni normalizzati uguali con `β = 0`; limite di spostamento di mediana e winsorized; non monotonia di `sigma_clip`; mediana su un plateau maggioritario; influenza di un pixel sulle statistiche d'ordine; disuguaglianza di Popoviciu; rapporti fra canali sotto colore costante e controesempio. Con Monte Carlo a seed fisso controlla la simmetria (`P(T > 0) = 1/2`) con varianze diverse e il controesempio senza normalizzazione: coefficiente 2.0 a SNR alto, 4.396 a SNR basso, atteso 13/3. Stima, non prova: `E[κ s²]/Var(M | r)` per la mediana con `n = 9` e rumore fotonico vale 1.003, 1.031, 1.139, 1.357 con rapporti fra i livelli 1, 2, 4, 8. È una verifica delle derivazioni, non un test del prodotto.

## Limiti

- Fattore globale dimostrato solo per `mean`; per gli altri metodi sotto ipotesi dichiarate e non verificate dal prodotto.
- Dipendenza delle scale dal rumore del singolo pixel non derivata (`scale_fixed`).
- Piano dei metodi robusti su flat normalizzati approssimato, con errore non derivato; solo stimato dall'autore.
- Nessun test eseguito.

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`. Perimetro chiesto dal coordinatore: solo difetti bloccanti, ciascuno con un controesempio.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-c02bflat-round1-20261030-0816.md` | `tmp/c02bflat-round1.diff` | PASS | Nessun rilievo |

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/C-02-master.md` | `c34b078781862e905ddd53734909d34113f893367a92ad8bf9f37a514336876d` |
| `docs/contracts/README.md` | `353e714178f05198679290fc445edac14ca3aa881f4f1d4e91c44d7c8df00b7c` |
| `docs/test-matrix.md` | `e4b5c8d30854c61d040e84dc2f61a5961bbf3092572780d8782ec4bcb1393d39` |
| `plans/M3.md` | `e091bb75a8655635046cffda62dcd3b54005c69dfdbe65f9d99f7f03090f6757` |

<!-- evidence:end -->
