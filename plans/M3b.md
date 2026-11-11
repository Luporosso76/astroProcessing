# ExecPlan M3.b: master completi (F-CAL-01.b)

Stato: **PLANNED**. Convenzione: `PLANS.md`. Piano madre: `plans/M3.md` (riga della slice M3.b, «Base comune», «Questioni aperte», «Dati pubblici e blocchi»). Modello di formato: `plans/M3a.md`.

## 1. Ambito

Perimetro dal registro (`python3 -S tools/trace.py ids --stage M3.b`): F-CAL-01, F-CAL-01.b e la consegna M3.b. Testo di accettazione: F-CAL-01 riga 1840 del piano attivo («combinazione, statistiche e rejection configurabili; gestisce set parziali e gruppi incompatibili; il report mostra frame usati, scartati, rumore, artefatti e stato del master; i master sono riproducibili da progetto»). Uscita §21.5 riga 2684.

Dentro, tutto in `docs/contracts/C-02-master.md` (sezioni «Estensione C-02.b» §b.1–§b.15 e «Estensione C-02.b-flat» §f.1–§f.15), con ADR-09.b (soglie), ADR-16.b §b.2 e §b.7 (stati e stato del decoder nei master):

- metodi oltre `mean`/`none` di C-02.a: `single`, `weighted_mean`, `median`, `winsorized`, `mean` + `sigma_clip` (§b.1);
- proposta a priori secondo `n` e ruolo, con N = 1 e N = 2 senza rejection (§b.4);
- analisi del set candidato: esclusioni per frame, gruppi incompatibili separati per esposizione, gain, offset, temperatura, CFA, binning, filtro, geometria, anche da RAW; sottoinsiemi scelti dall'utente (§b.2);
- master CFA per canale (§b.2 «Canali CFA», §b.6 «Flat», §b.13 punto 4);
- stati tipizzati: descrittore del master del prodotto, `frame_decoder_states` per i master da RAW (§b.7; ADR-16.b §b.7, §b.11);
- report di frame usati e scartati con motivo, rumore, artefatti, stato, ipotesi, avvisi (§b.7);
- flat: normalizzazione `median_ratio` e metodi robusti (C-02.b-flat §f.1–§f.14).

Fuori (C-02.b, «Fuori da C-02.b»; §b.14; §f.4):

- `Q`, flat `corrected_unnormalized` o `calibrated`, master `bias_subtracted` (`D'`, `DF'`): non prodotti dal master;
- pesi stimati dai dati, dark scalato e libreria di master (F-CAL-05, M6.f), mappa degli outlier come artefatto;
- scelta fra gruppi e fra master compatibili (F-IO-03, ADR-15);
- normalizzazione con dark-flat sottratto per frame;
- otto combinazioni, P-LOST nel job di calibrazione, regione di normalizzazione, bad pixel map, covarianza light/flat (M3.c);
- debayer (M3.d), GUI (M4), GPU (M7).

## 2. Prerequisiti e dipendenze reali

- **Gate M3.a DONE**, commit `a2fdc62` (`STATO.md`). M3.a consegna `analyze_batch`, `frame_pending_master`, selezione congelata, `verify_selection`, `master_descriptor` con `frame_decoder_states`, decoder RAW e DNG, G-OFFSET, G-CAL.
- **D.6** (appendice D, righe 3718-3720, 4114-4115, 4238): `M3.b requires_all M3.a` e `F-CAL-01.b`; `F-CAL-01.b requires_all F-CAL-01.a, F-IO-03, C-02.b`; `C-02.b requires_all C-02.a`, `decided_by ADR-09.b`. `tools/trace.py prereqs F-CAL-01.b` aggiunge in chiusura F-IO-01…04, ADR-03.b, ADR-08.b, ADR-16.b, D-09.b, D-16, G-CAL: tutti chiusi in M3.a o all'ingresso.
- **Contratti letti**: C-02.b e C-02.b-flat (in `C-02-master.md`), ADR-09.b §b.3, §b.5, §b.6; ADR-16.b §b.2, §b.7, §b.11; ADR-17.b §b.10 (regola I); F-IO-03 CL-05, CL-11; ADR-06 «Estensione R2.r».
- **Codice esistente da estendere**: `src/core/master.cpp` (`make_plan`, `validate_master_recipe`, `require_phase`, `require_linear`, `combine_master`, `master_descriptor_from_node`), `src/core/include/astro/master.hpp` (`master_recipe`, `flat_normalization`), `src/core/include/astro/calibration.hpp` (`combine_method`, `master_rejection`, `variance_estimator`, `master_descriptor`), `src/core/batch_classification.cpp` (`analyze_batch`, `frame_pending_master`), `src/core/job_types.cpp` (`make_master`).

`codegraph impact` (snapshot del checkout principale): `combine_master` 18 chiamanti, `master_descriptor_from_node` 8 chiamanti (`batch_classification.cpp`, `calibration_job.cpp`, `master.cpp`), `require_phase` 2. Test da rilanciare, tra quelli elencati: `test_master`, `test_master_stats`, `test_simd`, `test_calibration_real`, `test_batch_analysis`, `test_verify_selection`. Ogni unità rilancia `codegraph impact` sui propri simboli prima del codice.

## 3. Deliverable

- Codice: metodi, κ, normalizzazione dei flat, analisi, gruppi, proposta, master da RAW, job generale. File nuovi proposti (nomi indicativi, la scelta è dell'autore): `src/core/master_methods.cpp`, `src/core/robust_kappa.cpp`, `src/core/master_flat_norm.cpp`, `src/core/master_analysis.cpp`.
- Contratti: estensione ADR-06 R2.r (modello di memoria, B0); decisioni sulle lacune L1…L6 e L8…L11 in `docs/decisions/` (L7 chiusa, §8).
- Test: `tests/unit/test_master_*.cpp` nuovi, fixture `tests/fixtures/syn_cal.hpp`, protocollo preregistrato `docs/evidence/M3b-protocollo.md`.
- Evidenza: `docs/evidence/M3b-B<n>.md` per unità, `docs/evidence/M3-b.md` per il gate; `docs/test-matrix.md` e `STATO.md` aggiornati.

## 4. Metodo per ogni unità

Contratto letto; test scritti da un agente diverso dall'autore del codice e visti fallire; `codegraph impact`; codice; `tools/check.sh` e `tools/mutate.sh`; review con `tools/review.sh B<n>-roundN` (tetto 2 giri); record. Le soglie non si allentano dopo una misura. Il modello di ogni delega si legge da `tools/model-routing.tsv`. Test statistici pesanti in esecuzione seriale, al 70% di CPU e RAM, temporanei in `tmp/`.

## 5. Unità (ordine topologico)

Ordine: **B0 → B1 → {B2, B3, B4} → B3b → B5 → B6 → B7 → B8 → B9 → B10 → B11**, ramo calcolo. Ramo analisi dopo B3 e B7: **B12 → B13**; **B15a** dopo B3 (nodo e preflight RAW); **B14** dopo B13, B4, B5 e B15a. Giunzione: **B15 → B16 → B17 → gate**; B16 dopo B10, B14, B15 e B15a. Al massimo quattro unità attive insieme, purché non tocchino gli stessi file sorgente (utente, 10/11/2026).

### Modelli per unità (si legge prima di ogni delega)

| Unità | Test (agente diverso dal codice) | Codice | Motivo |
|---|---|---|---|
| B0, B1 | n/d | n/d | solo documenti; nessuna riga in `tools/model-routing.tsv` |
| B2 | Opus | Opus | fixture con verità analitica in aritmetica razionale esatta, oracolo di B5–B9 |
| B3 | Sonnet | Sonnet | tipi, domini, codici |
| B3b | Sonnet | Sonnet | pianificatore di memoria e suo test |
| B4 | Opus | Opus | κ: quadratura, oracolo indipendente |
| B5, B6 | Opus | Opus | stimatori, piano di varianza, aritmetica esatta |
| B7 | Opus | Opus | livelli e bias per canale |
| B8, B9 | Opus | Opus | normalizzazione, regola I, fattore globale |
| B10 | Sonnet | Sonnet | blocchi, worker, modello di memoria |
| B11 | Opus | Opus | diagnostiche con ordine di somma fisso |
| B12, B14 | Sonnet | Sonnet | analisi e proposta senza statistica |
| B13 | Opus | Opus | nero effettivo in interi a 64 bit sul periodo `lcm(2, rows) × lcm(2, cols)` (ADR-16.b §b.7; in M3.a U6 era Opus), mediane `m_(f,c)` |
| B15a, B15 | Opus | Opus | stato del decoder e nero effettivo; mediane per canale CFA |
| B16, B17 | Sonnet | Sonnet | job, dato reale |

Righe proposte per `tools/model-routing.tsv` (non modificato da questo piano; colonne unità, test, codice; B0 e B1 sono solo documenti e non hanno riga):

```
B2	opus	opus
B3	sonnet	sonnet
B3b	sonnet	sonnet
B4	opus	opus
B5	opus	opus
B6	opus	opus
B7	opus	opus
B8	opus	opus
B9	opus	opus
B10	sonnet	sonnet
B11	opus	opus
B12	sonnet	sonnet
B13	opus	opus
B14	sonnet	sonnet
B15a	opus	opus
B15	opus	opus
B16	sonnet	sonnet
B17	sonnet	sonnet
```

Revisione: `tools/review.sh` (Argon, poi `--reviewer claude`). Ricognizione e passi meccanici: Haiku.

### B0. Emendamenti prima del codice — solo documenti

Stato: **CHIUSA** il 9 novembre 2026 (review `M3b-B0-round1` FAIL, `M3b-B0-round2` PASS). Decisioni ed emendamenti applicati: `docs/decisions/M3b-lacune.md`.

ID: nessuno nuovo. (1) Estensione ADR-06 R2.r, solo testo: i buffer di ordinamento per pixel, gli indicatori di `sigma_clip`, gli accumulatori del report per riga e le costanti delle scale entrano nel modello di memoria del master (C-02.b §b.11, §b.13 punto 5; C-02.b-flat §f.13 punto 11). Il pianificatore e il suo test sono in B3b, prima del codice di B5. (2) Decisioni del coordinatore sulle lacune L1…L6 e L8…L11 che bloccano il codice (§8). Nessun test, nessun codice.

### B1. Protocollo preregistrato — solo documento

Stato: **CHIUSA** il 9 novembre 2026 (review `M3b-B1-round1` PASS, 1 LOW corretto). Protocollo congelato: `docs/evidence/M3b-protocollo.md`, versione 1 SHA-256 `83e30030…`, versione 2 (9/11, emendamento di τ_k, «Decisioni successive (B5)») SHA-256 `4b06acb6…`, versione 3 (9/11, nota sull'implementazione della regola I nell'oracle, criteri invariati) SHA-256 `e21760c7f68590d71bccee78088f1577ea0a1bd7e337bc758c3a4aa7f3e1ffc7`; emendamento di C-02 §b.12 in `docs/decisions/M3b-lacune.md`, «Decisioni successive (B1)».

ID: C-02.b §b.9; C-02.b-flat §f.9; ADR-09.b §b.4 e T-M3-ADR09-01. File `docs/evidence/M3b-protocollo.md`: per ogni test statistico `K`, `P`, seed, parametri, `α = 10⁻³` con Bonferroni sulle `m` statistiche, margini deterministici `τ_k`. Regola di derivazione di K e P per T-M3B-C02-04(b): semiampiezza relativa di Chebyshev < metà dello scarto relativo fra `κ` esatto e `π/(2n)` per ogni `n`. Committato **prima** del primo record di misura; non si modifica dopo.

### B2. Fixture `FIX-SYN-CAL-08` — ~400 righe, solo test

Stato: **CHIUSA** il 9 novembre 2026 (review `M3b-B2-round1` PASS, 1 LOW corretto). `tests/fixtures/syn_cal.hpp`, `tests/unit/test_syn_cal.cpp` (13 test).

Generatore in `tests/fixtures/syn_cal.hpp`, scritto da chi non scrive l'operatore (`tests/fixtures/exact_rational.hpp` come aritmetica esatta). B, D, F, DF noti; rumore gaussiano di varianza nota, anche eteroschedastico (`σ_f² = c_f g / G + R²`); outlier in posizioni note; flat a livelli diversi per canale con regione di livello (più della metà dei pixel del canale con `g = 1`, `β = 0`, nessun rumore) e regione di misura (C-02.b-flat §f.9); frame RAW sintetici da `FIX-SYN-DNG-01`. Senza codice di prodotto.

### B3. Tipi, ricetta, domini, fase e linearità — ~350 righe

Stato: **CHIUSA** il 9 novembre 2026 (review `M3b-B3-round1` FAIL, `M3b-B3-round2` PASS; test e codice da agenti distinti; mutazioni 12/12 rilevate). Suite fast 1236 test verde in 3 shard sull'albero con B2.

ID: C-02.b §b.1 (tabella dei metodi), §b.2 (tabella dei domini), §b.10 (parametri e codici), §b.13 punti 1 e 3; C-02.b-flat §f.13 punti 1-2.

Fa:
- aggiunge a `combine_method` `single`, `weighted_mean`, `winsorized` (resta `other` per i master esterni, ADR-16.a), a `variance_estimator` `noise_model`, `empirical_weighted`, `gaussian_reference`, `frozen_selection`;
- campi di ricetta: pesi (campo `weight` della voce del frame, L5), parametro `analysis` (esito canonico dell'analisi, `null` per un nodo non proposto, L8), `winsor_g`, `clip_kappa_low`, `clip_kappa_high`, `clip_max_iter`, `clip_min_kept`, `offset_adu`, `conversion_gain_e_per_adu`, `read_noise_adu`;
- validazione strutturale: coppia (`combine`, `rejection`) fuori tabella, `n` fuori dominio, ruolo non ammesso → `unsupported`, `master.variance_domain`; parametri assenti o di un metodo non scelto → `invalid_input`, `master.param.<nome>`;
- `require_phase` e `require_linear` chiamano `require_operation_allowed(master_create, …)`: `op.phase_blocked` poi `op.linearity_blocked`, ruolo nel contesto; assenze e unità invariate;
- nodo e chiave: parametri canonici, `master_algorithm_version` aumentata, `master_descriptor_from_node` accetta i metodi di §b.1 e scrive `combine_method`, `rejection`, `variance_estimator` e il campo `frame_normalization` (`none` o `median_ratio` per il flat, `null` per bias, dark e dark-flat; C-02.b §b.7 e §b.13 punto 3, C-02.b-flat §f.7 e §f.13 punto 7);
- valore di `flat_normalization` fuori da {`none`, `median_ratio`}: `invalid_input`, `master.param.flat_normalization` (decisione di L11: si segue il testo, nessun codice nuovo, `master.not_master_node` alla lettura di un nodo); le attese di `test_master.cpp` e T-M2R2-05 sul caso `median_ratio` → `master.variance_domain` cambiano in B8, dove `median_ratio` diventa ammesso (C-02.b-flat §f.13 punto 1).

Resta fuori: confronti in aritmetica esatta su pesi e κ di sigma (B5, B6), calcolo dei metodi, flat `median_ratio` (B8).
Test: T-M3B-C02-12; T-M3B-C02-09 (domini e stop); T-M3B-C02-03(c); T-M3B-DSC-01 (con `frame_normalization` e `analysis`: la chiave cambia se cambia `analysis`); T-M3B-FLAT-06 (parte parametri: assenti, presenti dove vietati, fuori dominio, coppie vietate della tabella di §f.2, `sigma_clip` con soglie diverse); T-M3B-C02-15 (solo `master.param.<nome>`, controllo del contatore del `fault_injector`); riesecuzione T-M2R2-01…09 e `test_master.cpp` con attese cambiate di categoria e codice sui casi `phase` e `nonlinear`.
Rischio: `master_algorithm_version` nuova rende `stale` i master `.a` esistenti. Lo si dichiara nel record e T-M2R2 si riesegue.

### B4. κ di mediana e winsorized — ~350 righe

Stato: **CHIUSA** il 9 novembre 2026 (review `M3b-B4-round1` PASS senza rilievi; test e codice da agenti distinti; mutazioni 5/5).

ID: C-02.b §b.6 «Metodi robusti» (regola sul calcolo di `κ`), §b.5; ADR-09.b classe P.

Fa: `κ_T(n, g) = Var(T(Z_1…Z_n))` per mediana e winsorized, `n ≤ 1024`, errore relativo ≤ 10⁻⁹; stima d'errore del proprio calcolo; stop `numeric`, `master.kappa_accuracy` prima di ogni lettura di campioni. Funzione pura calcolata una volta nel preflight.
Resta fuori: uso di κ nel piano (B5).
Test: T-M3B-C02-05 (ancore `κ_med(2) = 1/2`, `κ_med(3) = 1 − √3/π`, winsorized `n ∈ {2g+1, 2g+2}` = mediana; oracolo in `long double` con quadratura diversa, errore dell'oracolo stimato ≤ 10⁻¹¹ prima della misura); T-M3B-C02-15 (solo `master.kappa_accuracy`, controllo del contatore del `fault_injector`).

### B3b. Pianificatore di memoria del master — ~150 righe

Stato: **CHIUSA** il 9 novembre 2026 (review `M3b-B3b-round1` PASS, 2 LOW corretti; test e codice da agenti distinti; mutazioni 5/5). Suite fast 1256 test verde in 3 shard sull'albero con B4.

ID: C-02.b §b.11, §b.13 punto 5; C-02.b-flat §f.13 punto 11; estensione ADR-06 R2.r di B0.

Fa: il pianificatore del master conta i buffer nuovi (ordinamento per pixel, indicatori di `sigma_clip`, accumulatori del report per riga, costanti delle scale) per ogni metodo. Test scritto prima e visto fallire; test e codice nello stesso commit, nessun test rosso committato fra unità.
Dopo B3 e B0; prima di B5. Per i frame RAW (L10, opzione C) conta la quota di spill `Z = n·S·6`, che non è memoria del pool, e si ferma prima di ogni lettura se lo spill non è dichiarato o la quota non basta (ADR-06 R2.r, «Frame RAW tramite spill»).
Test: T-M3B-MEM-01 (piano non rappresentabile, minimo − 1 → stop prima di ogni prenotazione), T-M2R2R-01…05 rieseguiti.

### B5. `single`, `weighted_mean`, `median`, `winsorized` — ~400 righe

Stato: **CHIUSA** il 9 novembre 2026 (review `M3b-B5-round1` FAIL con 1 BLOCCANTE, `round2` PASS, revisore Opus; test e codice da agenti distinti; protocollo emendato v2 e v3). Albero con B15a: suite fast 1293 verde in 3 shard, 10/10 slow statistici.

ID: C-02.b §b.1, §b.6 (ordine delle operazioni, rappresentabilità), §b.7 (maschere, capability), §b.8 (ordine totale per pixel).

Fa: i quattro metodi con piano `V` per metodo (`noise_model`, `empirical_weighted`, `gaussian_reference` con `κ` di B4); costanti dei pesi `W`, `a_f`, `T`, `c_f` nel preflight; dominio `2 max w < W` in aritmetica esatta sui f64; ordinamento per pixel su (valore, indice canonico); accumulo f64 e un solo arrotondamento al `sample_type`; `result_nonfinite` su zero o subnormale; `measure` come `merge_measure` senza codici propri.
Resta fuori: `sigma_clip` (B6), flat (B8, B9), report (B11), blocchi e worker (B10).
Test: T-M3B-C02-01 (casi dyadic, pareggi `−0`/`+0`), 03, 04, 07 (caratterizzazione, nessuna soglia), 13; T-M3B-C02-15 (solo `master.result_nonfinite` e `master.param.weights`, controllo del contatore del `fault_injector`).

### B6. `mean` + `sigma_clip` — ~300 righe

Stato: **CHIUSA** il 10 novembre 2026 (review `M3b-B6-round1` FAIL con 1 BLOCCANTE, `round2` PASS, revisore Opus; test e codice da agenti distinti; mutazioni 7/7). Porta nel codice il bit 9 `support_incomplete` di ADR-03.b §b.6 e T-M3A-TYP-08 (anticipato da M3.d); decisioni in `docs/decisions/M3b-lacune.md`, «Decisioni successive (B6)». Albero `b10be73`: suite fast 1322 verde in 3 shard, slow 39/39, release compilata.

ID: C-02.b §b.1 («`sigma_clip`», Samuelson), §b.6 (selezione congelata), §b.7.

Fa: procedura per pixel con soglie strette, `clip_min_kept` → bit `support_incomplete`, campione e varianza NaN; rifiuto non acceso come invalidità; `master.param.sigma_clip_inert` con confronto esatto sui f64 di `(n − 1)²` contro `κ² n`; piano `frozen_selection`; capability `variance.fixed_weight_approximation`; avviso `frozen_selection_approximation`.
Test: T-M3B-C02-01 (parte sigma), 02 (appartenenza esatta, casi inerti `n = 10, κ = 3` e `n = 4, κ = 1.5`, `n = 11, κ = 3` ammesso; casi con intervalli sovrapposti tolti prima della misura e contati), 06 (caratterizzazione); T-M3B-C02-15 (pixel `support_incomplete` senza cambio di metodo, a livello di combinazione).

### B7. Livelli e statistiche del flat per canale — ~250 righe

Stato: **CHIUSA** il 10 novembre 2026 (review `M3b-B7-round1` PASS con 6 LOW corretti, `round2` PASS con 2 LOW residui in `docs/decisions/M3b-lacune.md`; revisore Opus; test e codice da agenti distinti; mutazioni 11/11). Corregge anche `master_row_channels` del job (difetto di B3b). Albero `4431098`: suite fast 1337 verde in 3 shard, release 1374/1374 (fast e slow).

ID: C-02.b §b.2 («Canali CFA»), §b.6 «Flat», §b.13 punto 4; T-M3B-C02-14.

Fa: `m_(f,c)` per canale (quattro posizioni del motivo, G1 e G2 separati), tolleranza `flat_level_rel_tol` per canale (con `flat_normalization = none`), `level_bias_estimate` per canale come mediana e massimo sui pixel validi del canale (statistiche d'ordine dai piani, come oggi in `src/core/master.cpp`, `channel_report`: nessuna somma per riga nel flat); mono invariato; versione dell'algoritmo del nodo.
Resta fuori: normalizzazione (B8), analisi (B13).
Test: T-M3B-C02-14 (livello fuori tolleranza in un solo canale con livello globale entro tolleranza; mono identico a `.a`).

### B8. `median_ratio`, parte 1: parametri, costanti, preflight — ~350 righe

Stato: **CHIUSA** l'11 novembre 2026 (review `M3b-B8-round1` FAIL con 2 MED di prova, `round2` PASS; revisore Opus; test e codice da agenti distinti; mutazioni 15/15). La chiave del nodo cambia per ogni master (parametri nuovi nei parametri del nodo). Albero `7e34936`: suite fast 1452 verde in 4 shard, release 1491/1491 (fast e slow).

ID: C-02.b-flat §f.1 (regola), §f.2 (tabella dei metodi, parametri, controlli 1-4), §f.10, §f.13 punti 1-2, 4-5, 8.

Fa: `m_H,c`, `r_(f,c)` nell'ordine canonico; parametri `flat_additive_bound_adu`, `flat_norm_rel_budget`, `flat_level_rel_tol` (presenza e divieti per metodo); controlli: `master.flat_low_signal`, `master.flat_scale_nonfinite`, `max m ≤ 4 min m` esatto per mediana, winsorized, sigma_clip (`master.variance_domain`), `A · ρ_c ≤ budget · m_H,c` esatto (`master.flat_norm_budget`); `sigma_clip` sul flat solo con soglie uguali. `median_ratio` diventa ammesso in `validate_master_recipe` (oggi fuori dominio con `master.variance_domain`, C-02.b-flat §f.13 punto 1): le attese di `test_master.cpp` e T-M2R2-05 su questo caso si aggiornano qui. Con `median_ratio` `flat_level_rel_tol` è vietato (C-02.b-flat §f.2 e §f.13 punto 2) e il controllo di `finish_preflight` sul livello (`master.variance_domain` con `none`) non si applica; nessun parametro opzionale e nessun avviso dedicato oltre `flat_norm_scales_estimated`, per decisione di L11 (`docs/decisions/M3b-lacune.md`).
Test: T-M3B-FLAT-06 (parte stop: caselle della tabella di §f.2 con `median_ratio`, successore f64 di `4 min m` escluso, budget uguale al predecessore f64 escluso, `flat_min_signal_adu` e livello subnormali; nessun calcolo e nessun nodo negli stop; le casistiche ammesse al confine sono in B9), T-M3B-FLAT-01 (costanti dyadic: livelli `(1, 3)` → `m_H = 3/2`, `r = (3/2, 1/2)`).

### B9. `median_ratio`, parte 2: valori e piano — ~300 righe

Stato: **CHIUSA** l'11 novembre 2026 (review `M3b-B9-round1` FAIL con 1 MED di prova, `round2` PASS; revisore Opus; test e codice da agenti distinti; mutazioni 11/11). Albero `db98d12` (con B13): suite fast 1514 verde in 4 shard, release 1561/1561 (fast e slow).

ID: C-02.b-flat §f.1 (fatti 1-4), §f.6, §f.7 (maschere).

Fa: `y_f = offset_adu + r_f (x_f − offset_adu)` fuso nel ciclo per pixel; metodi di B5 e B6 sugli `y_f`; piano che stima `Var(M | r)`; invalidità e saturazione sugli `x_f`; capability `variance.fixed_weight_approximation` per ogni metodo con `median_ratio`.
Test: T-M3B-FLAT-06 (parte confini ammessi: rapporto uguale a 4 e `A · ρ_c = budget · m_H,c` con la combinazione eseguita; prodotto `r_f (x_f − offset_adu)` subnormale → `master.result_nonfinite`), T-M3B-FLAT-01, 02 (fattore globale senza rumore: (a) `mean`, β qualunque, rapporto fino a 16; (b) ogni metodo con β ≡ 0, rapporto fino a 4), 03, 04, 05 (caratterizzazione, nessuna soglia).

### B10. Blocchi, worker, determinismo, memoria — ~250 righe

Stato: **CHIUSA** l'11 novembre 2026 (review `M3b-B10-round1` PASS con LOW corretti, `round2` PASS; solo test, prodotto invariato; mutazioni 5/5). Albero `d75552f`: suite fast 1613 verde in 4 shard, slow 38/38 (esclusi i fuzz del decoder, non toccato).

ID: C-02.b §b.8, §b.11; C-02.b-flat §f.8, §f.11; estensione ADR-06 R2.r di B0.

Fa: invarianza bit a bit a `tile_rows` ∈ {1, 7, 64}, worker ∈ {1, 4}, blocchi e memoria, per ogni metodo; usa il pianificatore di B3b.
Test: T-M3B-C02-10 (parte metodi: master, piano e maschera), T-M3B-FLAT-08 (parte valori: master, piano e maschera), T-M3B-MEM-01 e T-M2R2R-01…05 rieseguiti. Le parti residue: report in B11; candidati permutati in B14; riapertura del progetto e ricostruzione del nodo in B16.

### B11. Report del master — ~400 righe

Stato: **CHIUSA** l'11 novembre 2026 (review `M3b-B11-round1` FAIL con 6 MED, `round2` PASS, LOW residui chiusi in B15; revisore Opus; test e codice da agenti Opus distinti; mutazioni 8/8 + 4/4). Albero `13325e1`: suite fast 1650 verde in 5 shard, slow 38/38 (esclusi i fuzz del decoder), build release pulita.

ID: C-02.b §b.7 («Report del master»: rumore, artefatti, stato, ipotesi, statistiche di `.a` per metodo), §b.10 (avvisi); C-02.b-flat §f.7.

Fa: `variance_plane_median`; `residual_mean_adu` e `residual_rms_adu` per frame e canale (somma per riga in ordine di colonna, poi righe in ordine crescente); conteggi per frame e per bit; costanti `κ`, `a_f`, `c_f`, `m_(f,c)`, `r_(f,c)`, `ρ_c`, `norm_level_bias_bound`; `variance_cv_gaussian_homoscedastic` o `null` con motivo; `level_bias_estimate` `null` con motivo `normalized`; campo `frame_normalization` nel report (il campo del descrittore è in B3); ipotesi non verificate e avvisi. `norm_level_bias_bound` è una costante per canale, con mediana e massimo del suo rapporto col piano sui pixel validi; la riduzione a due livelli vale solo per `residual_mean_adu` e `residual_rms_adu`; con `median_ratio` `flat_level_rel_tol` assente e l'esito dei controlli come in B8.
La sezione «Analisi» del report (C-02.b §b.7: chiave del gruppo, candidati usati ed esclusi, proposta e se il metodo scelto coincide) si costruisce qui dal parametro `analysis` del nodo, che B14 produce.
Test: T-M3B-C02-11 (parte master, e parte analisi: sezione «Analisi» del report dal parametro `analysis`), T-M3B-FLAT-07 (descrittore, `measure` e report per ogni metodo con `median_ratio` e con `none`; la parte analisi è in B14), T-M3B-C02-10 e T-M3B-FLAT-08 (parte report: 0 differenze bit a bit), T-M3B-C02-15 (solo `master.report_nonfinite`, controllo del contatore del `fault_injector`).

### B12. Analisi: esclusioni per frame — ~350 righe

Stato: **CHIUSA** il 10 novembre 2026 (review `M3b-B12-round1` FAIL con 7 MED, `round2` PASS, LOW residui in `docs/decisions/M3b-lacune.md`; revisore Opus; test e codice da agenti Sonnet distinti; mutazioni 15/15). Emenda C-02.b §b.2 punto 1 (`declared_conflict`). Albero `68e7793`: suite fast 1427 verde in 4 shard, release compilata.

ID: C-02.b §b.2 punto 1 (lista chiusa di motivi), `master.no_usable_frames`.

Fa: funzione pura degli ingressi: `layout`, `metadata_missing.*`, `phase`, `linearity`, `unit`, `pedestal`, `input_variance`, `imagetyp_conflict`, `duplicate_digest`, `decode_failed`, `saturation_conflict`, `saturated_fraction`, `flat_low_signal`, ciascuno con i valori nel report. Nessuna correzione, nessuna sostituzione. `offset_adu` dei RAW come parametro d'analisi `master.offset_adu` (L9). Estende `read_analysis_params` con il membro `master` di `batch.analysis_params` e l'esito `params_missing` per ruolo, con lo stop `master.param.saturation_adu` (L4). Lettore di statistiche per frame (L2) per i FITS: lettura verificata OB-11, pool e cancel; per ogni frame esito di decodifica, saturi, totale dei pixel e `m_(f,c)` (assenti senza pixel validi); la parte RAW del lettore è in B15.
Test: T-M3B-C02-08 (ogni motivo da solo, compresi FITS e RAW come ingressi puri: `decode_failed`, `saturation_conflict` con FITS e con RAW, `flat_low_signal`; la parte su gruppi è in B13, il passaggio del preflight dei nodi proposti in B14), T-M3B-C02-12 (parte analisi), T-M3B-C02-15 (solo `master.no_usable_frames`, controllo del contatore del `fault_injector`); prove del lettore FITS (`input.digest_mismatch`, `input.missing`, cancel) e parametri assenti uno alla volta (L4), come test aggiunti di B12.

### B13. Analisi: gruppi e livelli del flat — ~400 righe

Stato: **CHIUSA** l'11 novembre 2026 (review `M3b-B13-round1` FAIL con 1 MED di prova, `round2` PASS; revisore Opus; test e codice da agenti distinti; mutazioni 12/12). Albero `db98d12`: suite fast 1514 verde in 4 shard, release 1561/1561.

ID: C-02.b §b.2 punti 2-3; ADR-16.b §b.7 (campi strutturali, nero effettivo).

Fa: chiave esatta (larghezza, altezza, binning, CFA con offset, camera, gain, filtro solo flat, `roi_x`, `roi_y`, `readout_mode`, `row_order`, presenza dello stato del decoder, `offset_adu` per FITS, campi strutturali per RAW); gruppi per tolleranza con riferimento a digest minimo (esposizione, temperatura, nero effettivo in interi a 64 bit); stato `flat_level` per gruppo con `m_(f,c)` nel report; ogni gruppo è un nodo distinto.
Test: T-M3B-C02-08 (un campo alla volta; RAW con nero alla tolleranza e a tolleranza + 1; RAW e FITS insieme; permutazioni dell'elenco; gruppi disgiunti).

### B14. Proposta a priori, nodi proposti, aggancio a `analyze_batch` — ~300 righe

Stato: **CHIUSA** l'11 novembre 2026 (review `M3b-B14-round1` FAIL con 8 MED, `round2` FAIL con 1 MED sul piano: arbitraggio del coordinatore, piano corretto; test e codice da agenti Sonnet distinti; mutazioni 14/14). Tocca `astro.batch_proposal.v1` con una chiave aggiuntiva solo in attesa (deroga motivata). Albero `d75552f`: suite fast 1613 verde in 4 shard, slow 38/38 (esclusi i fuzz del decoder).

Dipende da B13, da B4 e B5 (κ e piano nel preflight di `median`, `single`) e da B15a (nodo e preflight dei gruppi RAW).

ID: C-02.b §b.2 («Selezione ed esecuzione»), §b.4, §b.7 (sorgente della sezione «Analisi»: parametro `analysis` del nodo), §b.10 (avviso `proposal_not_followed`); F-IO-03 CL-05.

Fa: tabella di §b.4 (n = 1 con `single` solo se `conversion_gain_e_per_adu`, `read_noise_adu` e `offset_adu` sono dichiarati; n = 2 `mean`; n = 3…1024 `median`; n > 1024 `mean`; flat con `flat_level` senza proposta e con `median_ratio` indicata come alternativa); nodo `master.combine` per gruppo con i frame in ordine canonico; esito dell'analisi nel parametro `analysis` del nodo (L8); una funzione propria `propose_masters` (L3) legge i frame elencati in `frame_pending_master` di `analyze_batch` (invariato, `astro.batch_proposal.v1` invariato salvo la chiave `flat_thermal_pending_master`, presente solo con esito in attesa) e produce gruppi e nodi proposti nel documento `astro.master_proposal.v1`; i nodi `master.combine` proposti entrano in `analyze_batch` come `slot_candidate` di tipo (c) di F-IO-03 CL-06, con il descrittore da `master_descriptor_from_node`, l'osservazione costruita dal frame di riferimento del gruppo, senza `load_samples` e con la verifica termica `flat.bias` dichiarata `thermal_check_pending_master`; `verify_selection` si aggiorna per accettarli e per controllare che i frame del nodo siano quelli scelti, senza escludere nulla (C-02.b §b.2 «Selezione ed esecuzione»; F-IO-03 CL-06, CL-11), e i test di M3.a (`test_batch_analysis`, `test_verify_selection`) restano validi perché `batch_proposal.v1` non cambia; si aggiunge la verifica di `select_recipe` e `verify_selection` su un candidato (c) e la nuova verifica dopo l'esecuzione del master (L3).
Test: T-M3B-C02-09 (proposta), T-M3B-C02-08 (resto: il preflight di ogni nodo proposto passa, anche per i gruppi RAW; frame incompatibile mai negli ingressi di un altro gruppo; `master.no_usable_frames`), T-M3B-C02-11 (sorgente della sezione «Analisi»: parametro `analysis`), T-M3B-FLAT-07 (parte analisi: gruppo `flat_level` senza proposta e alternativa `median_ratio` indicata), T-M3B-C02-10 e T-M3B-FLAT-08 (parte candidati permutati: stessa chiave del nodo e stesso `analysis`; digest dell'artefatto in B16), T-M3B-ANA-01, `test_batch_analysis` e `test_verify_selection` estesi al candidato (c).

### B15a. Master da RAW, parte 1: stato del decoder, nodo e preflight — ~250 righe

Stato: **CHIUSA** il 9 novembre 2026 (review `M3b-B15a-round1` FAIL, `round2` PASS, revisore Opus; test e codice da agenti distinti; mutazioni 16/16). Suite fast 1274 test verde in 3 shard sull'albero con B3b.

Dopo B3; prima di B14.

ID: ADR-16.b §b.7 (master), §b.11; C-02.b §b.2 («Selezione ed esecuzione», «RAW»), §b.7 (descrittore), §b.10 (righe RAW).

Fa: `frame_decoder_states` nei parametri del nodo (ordine canonico), invarianti del descrittore (`master.decoder_state_param`), `master.decoder_state_mixed`, preflight su campi strutturali e nero (`master.mismatch.<campo>`, `master.mismatch.decoder_black`), `master.metadata.offset_adu`. Il nero non si sottrae (C-01.b §1). Aggiunge al decoder il modo con prenotazione del chiamante e la funzione `raw_decode_needed_bytes` (ADR-08.b §2.8). Non legge frame RAW nel job: la lettura nel job (decodifica in `prepare`, uno alla volta, nello spill dichiarato, L10 opzione C) è in B16.
Test: T-M3A-CAL-07, T-M3B-DEC-01 (modo del chiamante, ADR-08.b §2.8), T-M3B-C02-08 (RAW: nodo con un campo strutturale alla volta e nero alla tolleranza e a tolleranza + 1, a livello di preflight del nodo).

### B15. Master da RAW, parte 2: CFA per canale — ~200 righe

Dopo B7 e B15a; prima di B16.

ID: C-02.b §b.2 («Canali CFA»), §b.6 «Flat», ADR-16.b §b.7.

Fa: master CFA per canale da DNG sintetici (G1 e G2 separati, livelli `m_(f,c)` per canale, nero non sottratto) tramite `combine_master`, con campioni già decodificati passati dal chiamante; contiene la parte RAW del lettore di statistiche di L2 (lettura verificata OB-11, decodifica, cancel, esito di decodifica per frame).
Test: T-M3B-CFA-01.

### B16. Job `master.combine` generale, anche da RAW — ~350 righe

Dopo B10, B14, B15 e B15a.

ID: C-02.b §b.2 («Selezione ed esecuzione»), §b.10; ADR-16.b §b.7 (P-LOST del master); plans/M3.md «Base comune» punto 3.

Fa: registro dei tipi di job: seconda voce `master.combine` per i frame RAW e varianti per ruolo e metodo con tre frame (oggi `make_master` ha tre ingressi `frame0…frame2`, ruolo `dark`, solo FITS: `src/core/job_types.cpp`), secondo la decisione di L1; estensione di `run_master_job` ai RAW (lettura verificata OB-11, decodifica in `prepare` uno alla volta, frame scritto nello spill dichiarato del job, fasce lette dallo spill, riverifica in `finalize`; senza spill stop prima di ogni lettura; decisione di L10, opzione C), con `master.decoder_state_changed` alla ridecodifica; suite di conformità applicata; guasti sul percorso del prodotto: frame modificato dopo la selezione, stato del decoder diverso, pixel `support_incomplete` senza cambio di metodo. Controllo termico `flat.bias` ricalcolato col master eseguito prima che giri una catena scelta con `flat_thermal_pending_master` (CL-06 (c), decisione di B14); digest dell'artefatto uguale per candidati permutati (T-M3B-C02-10, FLAT-08).
Test: T-M3B-JOB-01 (conformità), T-M3A-CAL-09 (parte master), T-M3B-C02-15 (frame modificato, decoder, `support_incomplete` sul percorso del job), T-M3B-FLAT-06 (nessun nodo né calcolo negli stop, contatore del `fault_injector`), T-M3B-C02-10 e T-M3B-FLAT-08 (parte riapertura del progetto e ricostruzione del nodo nello store).

### B17. Dato reale e riesecuzioni — ~200 righe

ID: C-02.b §b.9 (R, PUB-LFC-01); NFR-TEST-02.
Test: T-M3B-C02-16, T-M3B-FLAT-09; riesecuzione T-M2R2-01…09, T-M3A-CAL-07, T-M2F-02 (nessuna syscall di rete sul percorso).

## 6. Contratto scientifico per unità

Valido per B4, B5, B6, B7, B8, B9, B11. Il testo normativo è in C-02.b e C-02.b-flat; qui solo cosa l'unità deve rendere verificabile.

| Aspetto | Regola da C-02.b / C-02.b-flat |
|---|---|
| Modello | Frame indipendenti con stessa media per pixel (`stationary_mean`). `median`, `winsorized`: `iid_gaussian` e `continuous_samples` (§b.3). Flat con `median_ratio`: `b + c_(f,c) g + ν_f`, `scale_fixed`, `same_illumination_shape`, `additive_common` (§f.1, §f.3) |
| Piano `V` | `mean` `s²/n`; `weighted_mean` `Σ c_f (x_f − M)²` (non distorto con pesi dichiarati); `median`/`winsorized` `κ s²` (non distorto solo sotto le ipotesi); `sigma_clip` selezione congelata (approssimazione, errore misurato non derivato); `single` modello di rumore (§b.6) |
| N piccoli | N = 1 `single` o nessuna proposta; N = 2 `mean`; `weighted_mean` richiede `n ≥ 3` e `2 max w < W`; `median` 3…1024; `winsorized` `2g + 1`…1024; `sigma_clip` `n ≥ 3`, non inerte |
| Tutti i pixel rigettati | `sigma_clip` con meno di `clip_min_kept` tenuti: bit `support_incomplete`, campione e varianza NaN; nessun cambio di metodo (NC-21) |
| Varianza nulla | `s = 0`: nessun rifiuto; rapporti del report `null` con motivo `variance_zero`; `sqrt(0) = 0` valido |
| NaN e non finiti | Campione invalido → pixel invalido; nessun NaN nell'ordinamento; prodotto o quoziente con operandi non nulli che dà zero o subnormale → `result_nonfinite`; costanti dei pesi o delle scale non normali → `master.param.weights`, `master.flat_scale_nonfinite` |
| Errore | Stop tipizzati con valori nel contesto (§b.10, §f.10); nessun nodo, nessun percorso alternativo, nessun artefatto; un controllo fallito non passa a `none` né a un altro metodo |
| Riproducibilità | Ordine canonico dei frame (digest crescente), ordine totale (valore, indice) per pixel, accumulo f64, un solo arrotondamento; bit-identico con qualsiasi `tile_rows`, worker, memoria o blocchi, e dopo riapertura del progetto; nessun seed. Fra backend diversi (ADR-07) l'identità di `sigma_clip` non è richiesta (§b.8) |

## 7. Test e misure

I test `T-M3B-C02-01…16` e `T-M3B-FLAT-01…09` sono già in `docs/test-matrix.md` (righe 243-274, «PIANIFICATO — non eseguito») con criteri fissati in `C-02-master.md` §b.12 e §f.12. Questo piano non li ridefinisce. **Da aggiungere alla matrice**, dove il contratto non dà un ID:

| ID | Verifica | Unità |
|---|---|---|
| T-M3B-DSC-01 | Descrittore, enum e chiave del nodo per ogni metodo: valori di §b.7, `master_descriptor_from_node` per ogni combinazione ammessa, stop per il resto, versione dell'algoritmo | B3 |
| T-M3B-MEM-01 | Modello di memoria del master con i buffer dell'emendamento di B0: piano non rappresentabile, minimo − 1 → stop prima di ogni prenotazione | B3b |
| T-M3B-ANA-01 | Funzione di proposta (L3) sui frame di `frame_pending_master`: gruppi e nodi proposti uguali all'oracolo scritto a mano, indipendenti dall'ordine dei candidati | B14 |
| T-M3B-CFA-01 | Master CFA per canale da DNG sintetici: `frame_decoder_states`, livelli per canale, G1 e G2 separati, nero non sottratto | B15 |
| T-M3B-JOB-01 | Job `master.combine`, istanze minime con tre frame per ruolo e metodo scelti (voci FITS e RAW) nella suite di conformità (L1): ambiente FP, cancel, budget, input corrotto, nessun artefatto su errore | B16 |

- **Fixture**: `FIX-SYN-CAL-08` (B2), esteso con flat a livelli per canale; `FIX-SYN-DNG-01` per i RAW. Verità analitica scritta nel test, non col codice del prodotto.
- **Metriche preregistrate** (classi ADR-09.b §b.3, assegnazioni in C-02.b §b.9 e C-02-flat §f.9): E (zero) per T-C02-01, 13, FLAT-01, 02; E con intervalli per C02-02; S di coerenza per C02-03, 04, FLAT-03, 04; P per C02-05; C per C02-06, 07, FLAT-05; B per i test binari; R per C02-16, FLAT-09.
- **Derivazione delle tolleranze**: zero dove l'oracolo verifica esatta ogni operazione (casi dyadic); altrove regola I (ADR-17.b §b.10: l'oracolo riesegue la sequenza in aritmetica degli intervalli) più mezzo ulp f32 se l'uscita è f32; statistica con `α = 10⁻³` per test, Bonferroni sulle `m` statistiche, quantili χ² esatti (errore relativo ≤ 10⁻¹⁰), limite di Chebyshev `sqrt(m Var(X)/(α K P))`, binomiale esatta per FLAT-04. `K`, `P`, seed e parametri in `M3b-protocollo.md` (B1) prima della misura.
- **Dati reali** (in `data/public` del checkout principale, sola lettura):
  - `zenodo-3254683/` = PUB-LFC-01, mono: bias `ccd.001–006` più `darks/ccd.002`, dark `darks/ccd.013–015`, dark-flat `darks/ccd.017–019` e `darks/ccd.023–025`, flat g `ccd.014–016`, flat i `ccd.017–019`. Usato da C02-16 e FLAT-09. Limite: N = 3 per ruolo (nessun test su n grande), nessun CFA, un solo filtro per flat.
  - `raw-pixls-us/` = PUB-RAW-01, otto RAW CC0 di modelli diversi: solo coerenza della chiave dei gruppi (modelli diversi → gruppi diversi) e stop di X-Trans. Nessuna verità sui valori; nessun set di calibrazione.
  - `zenodo-17843622/first_sub.fits` (PUB-CFA-01): un solo sub, nessun frame di calibrazione: non serve a M3.b.

## 8. Lacune dei contratti

Il contratto non le decide; il piano non le decide. Ognuna è chiusa dalla decisione in `docs/decisions/M3b-lacune.md` (9 novembre 2026).

- **L1** — decisa: `docs/decisions/M3b-lacune.md`. Origine (prima di B16). `master.combine` è registrato con tre ingressi fissi `frame0…frame2`, ruolo `dark`, solo FITS (`src/core/job_types.cpp`). C-02.b non dice come il registro rappresenta `n` variabile, ruolo, metodo, parametri e ingressi RAW. Domanda: un solo tipo di job con ingressi variabili, o una variante per ruolo o per metodo? Come si dichiara la cardinalità nella suite di conformità?
- **L2** — decisa: `docs/decisions/M3b-lacune.md`. Origine (prima di B12). C-02.b §b.2 dice che l'analisi «legge i campioni solo per la frazione satura e per i livelli `m_(f,c)` del flat». `analyze_batch` (F-IO-03 CL-09) non legge campioni salvo il controllo termico di `flat.bias`. Domanda: chi legge i campioni dell'analisi dei master, con quale confine di lettura verificata (OB-11), budget del pool e cancel? L'analisi dei master resta una funzione pura con campioni passati da fuori?
- **L3** — decisa: `docs/decisions/M3b-lacune.md`. Origine (prima di B14). Il contratto F-IO-03 rimanda a C-02.b per «i gruppi e i master proposti», ma non dice dove vive l'API. Domanda: funzione propria (per esempio nel file nuovo di analisi) o estensione di `analyze_batch`? Il JSON `astro.batch_proposal.v1` cambia schema o versione? Come si espone un gruppo di master accanto ai gruppi di light?
- **L4** — decisa: `docs/decisions/M3b-lacune.md`. Origine (prima di B12). I parametri d'analisi (`exposure_rel_tol`, `temperature_abs_tol_c`, `temperature_policy`, `saturation_adu`, `saturated_fraction_max`, `flat_min_signal_adu`, `flat_level_rel_tol`, `decoder_black_abs_tol_adu`) non hanno default. Domanda: stanno nella stessa chiave `batch.analysis_params` dei light (`astro.batch_analysis_params.v1`) o in una propria? Cosa fa l'analisi se manca uno dei parametri (stop o gruppo `params_missing`, come per i light)?
- **L5** — decisa: `docs/decisions/M3b-lacune.md`. Origine (prima di B3). I pesi di `weighted_mean` sono «dichiarati per frame». Domanda: a quale frame si legano (digest, posizione nell'ordine canonico, `input_id`)? Come entrano nella chiave del nodo e come si trattano due frame con lo stesso digest?
- **L6** — decisa: `docs/decisions/M3b-lacune.md`. Origine (prima di B14). Il registro chiede di «gestire set parziali»; C-02.b non definisce il termine. Domanda: si intende (a) un sottoinsieme di un gruppo scelto dall'utente, (b) un gruppo con meno frame del minimo del metodo (stop `master.variance_domain`), (c) altro? Chi dichiara quanti frame mancano?
- **L7** — chiusa, non è una lacuna (conferma letterale in `docs/decisions/M3b-lacune.md`): C-02.b §b.1 (tabella dei metodi) e §b.2 (tabella dei domini) non danno nessun `n` massimo a `sigma_clip` e `weighted_mean` (`n ≤ 1024` solo per `median` e `winsorized`, §b.1 e §b.2; §b.4 riporta solo la proposta `mean` per `n > 1024`). B0 registra la conferma letterale; nessun tetto nuovo.
- **L8** — decisa: `docs/decisions/M3b-lacune.md`. Origine (prima di B14, B16). C-02.b §b.10 dice che la provenance del nodo contiene l'esito dell'analisi (esclusioni e gruppi) e le costanti del preflight. Il job non esegue l'analisi, e l'esito entrerebbe nei parametri solo cambiando la chiave. Domanda: l'esito dell'analisi sta nei parametri del nodo (chiave), in un record separato collegato al nodo, o solo nella provenance al momento dell'esecuzione? Quale dei tre garantisce «ricostruito dopo la riapertura, stessa chiave e stesso artefatto» (§b.8)?
- **L9** — decisa: `docs/decisions/M3b-lacune.md`. Origine (prima di B12). `offset_adu` dei RAW è «dichiarato una volta per l'insieme» e serve solo ai livelli del flat e a `single`. Domanda: lo dichiara l'analisi, la selezione o la ricetta del nodo? Come si lega al ledger `light_offset` di G-OFFSET?

- **L10** — decisa: `docs/decisions/M3b-lacune.md`. Origine (prima di B3b e B16). `run_master_job` (`src/core/master.cpp`) legge e riverifica oggi solo file FITS (lettura per fasce di righe, `fits_input_format()`); i frame RAW non hanno lettura per fasce di righe. C-02.b non dice come il job del master legge i frame RAW, come li riverifica contro digest e dimensione (OB-11) e come il modello di memoria ADR-06 R2.r conta i loro buffer. Domanda: decodifica intera del frame e poi fasce? Quale confine di lettura verificata e quale budget del pool? Come entra nel pianificatore di B3b?
- **L11** — decisa: `docs/decisions/M3b-lacune.md`. Origine (prima di B3 e B8). Due punti dove il contratto e la review del piano non coincidono. (a) Il codice di stop per `flat_normalization` fuori da {`none`, `median_ratio`}: C-02.b-flat §f.10 e C-02.b §b.10 non ne elencano uno; `master.flat_normalization_unsupported` non è nel contratto. (b) Con `median_ratio` C-02.b-flat §f.2 e §f.13 punto 2 vietano `flat_level_rel_tol` e non definiscono avvisi per la dispersione dei livelli; il contratto non contiene `flat_norm_wide_scale_spread` né `flat_level_spread_not_checked`. Domanda: si emenda il contratto (codice nuovo, parametro opzionale, avvisi) o si segue il testo attuale?

## 9. Gate M3.b

Criterio di uscita (`plans/M3.md`, riga di M3.b; C-02.b §b.9, C-02.b-flat §f.9): **master riproducibili bit-esatti dal progetto in ordine canonico; report completo; frame incompatibile escluso prima della scelta**. Verificabile così:

1. Un solo run completo, sullo stesso commit, di `ctest --preset dev-full` (sanitizer, test slow compresi) e di `release`, ogni test del preset passato. Rilancio del singolo test = diagnosi, non PASS.
2. Test del gate: T-M3B-C02-10 e T-M3B-FLAT-08 (bit-esattezza e riapertura); T-M3B-C02-11 e T-M3B-FLAT-07 (report); T-M3B-C02-08 (esclusione prima della scelta).
3. Tutti gli altri `T-M3B-C02-01…16`, `T-M3B-FLAT-01…09` e i cinque nuovi di §7 passati o, per le caratterizzazioni (C02-06, 07, FLAT-05), registrati con le stime nel record, senza soglia inventata.
4. T-M3A-CAL-07 e la parte master di T-M3A-CAL-09 verdi; T-M2R2-01…09, T-M2R2R-01…05 e T-M2F-02 rieseguiti.
5. T-FBK-01 parte M3.b (C02-15, FLAT-06): ogni guasto si inietta sul percorso del prodotto e nessun percorso alternativo parte.
6. Dato reale PUB-LFC-01 usato (C02-16, FLAT-09); senza, il gate resta aperto.
7. `python3 -S tools/trace.py check` pulito per F-CAL-01, F-CAL-01.b e M3.b; protocollo `M3b-protocollo.md` committato prima del primo record di misura (T-M3-ADR09-01).
8. Review PASS per ogni unità (tetto 2 giri); build pulita dev e release.

Ogni dichiarazione di chiusura cita comando, commit, passati/totali, elenco dei non passati.

## 10. Evidenza

`docs/evidence/M3b-B<n>.md` per unità e `docs/evidence/M3-b.md` per il gate: comando, commit, hash dei dati (SHA-256 di PUB-LFC-01 dal suo record), versioni, ambiente (CPU, RAM, compilatore, preset, worker), esito, limiti. `K`, `P` e seed dei test statistici dal protocollo preregistrato. Blocchi evidenza rigenerati con `tools/evidence.py`.

## 11. Rollback e rischi

- Rollback: ogni unità è un commit coerente; la ricetta `.a` (`mean` + `none`, `flat_normalization = none`) resta valida e rieseguita da T-M2R2 a ogni unità che tocca `master.cpp`.
- **Chiavi dei nodi**: nuova `master_algorithm_version` (B3, B7) rende `stale` i master `.a` già prodotti. Va dichiarato nei record.
- **κ a n grande** (B4): costo e accuratezza di 10⁻⁹ fino a `n = 1024` con winsorized; l'errore dell'oracolo è stimato, non dimostrato (C-02.b §b.14). Se la stima supera 10⁻¹¹ il test non parte.
- **Test statistici lunghi** (B5, B6, B9): `K · P` realizzazioni; esecuzione seriale, mai alzare il timeout per far tornare il verde.
- **`sigma_clip` vicino alle soglie**: i casi con intervalli sovrapposti escono dalla fixture prima della misura; il loro numero va nel record.
- **Flat a livelli molto diversi**: oltre `max m = 4 min m` mediana, winsorized e sigma_clip sono fuori dominio (stop); niente metodo alternativo.
- **Lacune L1…L6 e L8…L11**: L1, L2, L3, L8, L10 sono sul cammino critico del ramo analisi e del job; B12…B16 non partono senza decisione; L11 blocca B3 e B8. Tutte decise (§8).
- **Limiti dichiarati dai contratti** (C-02.b §b.14, C-02.b-flat §f.14): piano dei metodi robusti non distorto solo sotto `iid_gaussian` e campioni continui; selezione congelata senza garanzia; `scale_fixed` non derivata; il nero del decoder è sentinella dell'offset. Il report li dichiara come ipotesi non verificate.
- Una funzione P0 mancante blocca la milestone: nessun metodo, nessuna normalizzazione e nessuna parte del report si declassa a P1.

## 12. Divergenze registro e riepiloghi

Il registro (appendice D) vince.

- **C-02.b-flat**: `plans/M3.md` (riga 76) lo cita come contratto prima del codice. Non è un nodo del registro (nessuna occorrenza nell'appendice D; `trace.py ids --stage M3.b` dà F-CAL-01, F-CAL-01.b, M3.b). Esiste come unità di contratto assegnata da ADR-09.b §b.6 sotto C-02.b. Seguito il registro: è parte di F-CAL-01.b.
- **Stati tipizzati ADR-16.b**: §21.5 riga 2684 e `plans/M3.md` li citano per M3.b. Nel registro F-CAL-01.b non ha un arco diretto verso ADR-16.b; vi arriva in chiusura via F-IO-03 → G-CAL. Seguito il registro: gli stati entrano come descrittore del master (C-02.b §b.7) e come stato del decoder (ADR-16.b §b.7).
- **«Bit-esatti in ordine canonico»**: F-CAL-01 (riga 1840) dice «riproducibili da progetto»; §21.5 riga 2684 «master riproducibili». Il criterio bit-esatto è di `plans/M3.md` e di C-02.b §b.8-§b.9. Seguito il contratto per il gate.
- **«Set parziali»**: F-CAL-01 lo richiede, C-02.b non lo definisce (L6).
- **Matrice test**: i T-M3B-C02 e T-M3B-FLAT sono già in `docs/test-matrix.md`; il piano aggiunge solo i cinque di §7.
- **ADR-16.a, tabella dei tipi**: elenca `combine_method` ∈ {`mean`, `median`, `other`}; ADR-16.b §b.11 e C-02.b §b.7 li sostituiscono per i master del prodotto. Seguito ADR-16.b; `other` resta per i master esterni.

## 13. Chiusura

Stato del gate: aperto. Alternative scartate: nessuna in questo piano (le scelte di metodo sono nei contratti, C-02.b §b.4 e C-02.b-flat §f.4).

## 14. Prossimo comando

`python3 -S tools/trace.py ids --stage M3.b`; poi `git status --short --branch` e `codegraph sync`. Primo passo: B0 (in corso: decisioni su L1…L6 e L8…L11 e estensione ADR-06 R2.r, solo testo, in `docs/decisions/M3b-lacune.md`) e B1 (protocollo), in parallelo con i test rossi di B3 scritti da un agente diverso dall'autore del codice. Primo test rosso: T-M3B-C02-12, frame `calibrated` in un master → `incompatible`, `op.phase_blocked` al posto di `master.input_state`.
