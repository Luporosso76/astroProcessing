# M2.e: calibrazione dark_matched_flat_dark_flat

Stato: **DONE**, 29 ottobre 2026. Autore: agente executor su Claude Sonnet 5.5. Review indipendente: Codex `gpt-6-astra` (reasoning high, sola lettura), giri 1-3 FAIL con correzioni; esito per giro nella sezione dedicata. Contratti: `docs/contracts/ADR-16-stati-master.md`, `ADR-17-varianza.md`, `ADR-03-pixel-maschere.md` (sezioni "Aggiunte di M2.e"); piano: `plans/M2.md` (M2.e, Gate M2).

## Comando ed esito

- Dev (ASan + UBSan): `rm -rf build && tools/check.sh > tmp/check-m2e.log 2>&1`. Esito: 313/313 (`tmp/check-m2e.log`, riga "100% tests passed, 0 tests failed out of 313"; tempo reale 83,86 s). Il preset `dev` filtra `-L fast`: i tre test `slow` (T-M2E-13, T-M2E-21/26, T-M2D-13) non girano in dev.
- Release: `cmake --preset release && cmake --build --preset release && ctest --preset release > tmp/check-m2e-rel.log 2>&1`. Esito: 315/315 (`tmp/check-m2e-rel.log`, 312 `fast` + 3 `slow`; tempo reale 166,10 s, di cui 112,23 s per `T-M2D-13.m1d_protocol`, 28,38 s per `T-M2E-13.real_palomar_lfc`, 5,49 s per `T-M2E-21-26.monte_carlo`). `ctest --preset release` non filtra per etichetta: esegue anche gli `slow`.
- Rispetto a M2.d (268 dev, 268 release): +45 in dev e +47 in release. In release entrano anche i due `slow` di M2.e e `T-M2F-02.m2e_network`.
- Nuovi file di test: `test_calibration.cpp` (32 casi), `test_calibration_job.cpp` (12), `test_calibration_stats.cpp` (2 casi in un solo test CTest, `slow`), `test_calibration_real.cpp` (1, `slow`), `m2e_workflow.cpp` (eseguibile sotto strace, T-M2F-02).

## Revisione e ambiente

- Revisione git di partenza: `16ed5bd0dc660e104e07e3adea5912844bd1fde7` (M2.d). I test sono stati eseguiti sul working tree poi entrato nel commit di chiusura di M2.e, che aggiunge `src/core/calibration{,_internal,_job,_report}`, `include/astro/calibration.hpp`, `tests/fixtures/exact_rational.hpp`, `tests/unit/{calib_support.hpp,m2e_workflow.cpp,test_calibration*.cpp}`, `tools/check_m2f02_network.sh`, e modifica `fits_scan.cpp`, `fits.hpp` (`fits_plan` con larghezza, altezza e piani), `job.{hpp,cpp}` (`run_job_files`), `node.{hpp,cpp}`, `tests/fixtures/syn_fits.hpp` e gli ADR 03, 16, 17.
- Linux 7.0.0-38-generic x86_64, 16 CPU, 31,6 GiB di RAM, GCC 15.2.0, SQLite 3.46.1, OpenSSL 3.5.5. Preset `dev`: `-fsanitize=address,undefined -fno-sanitize-recover=all`.

## Consegna

- `calibration` (`calibration.cpp`, `calibration_internal.hpp`): `Lc = L - D` con dark matched (bias incluso), `Fc = F - DF`, `Q = Fc/N(Fc)` con N mediana globale o per canale CFA, `C = Lc/Q`; varianza al primo ordine; bit di invalidità per pixel (`source_nonfinite`, `source_flagged`, `saturated`, `master_invalid`, `q_nonpositive`, `q_nonfinite`, `q_below_min`, `q_unreliable`, `result_nonfinite`). Nessun epsilon: un `Q` non valido è mascherato.
- `calibration_job` (`calibration_job.cpp`): preflight di compatibilità e digest, prenotazione dal piano dei file, lettura a tile con worker, pubblicazione di immagine, report e testo coerenti con l'output; ricetta e nodo dello slot devono coincidere.
- `calibration_report` (`calibration_report.cpp`): report JSON con normalizzazione (N, `n_valid`, `normalization_rel_var_estimate`, `iid_assumption`, `fc_nonpositive_in_N`), registro delle correzioni, avvisi e `image_fields` con valore e origine (`header`/`declared`) di binning, unità e linearità.
- Decisioni del coordinatore sulle sette ambiguità: (1) i campi d'immagine dichiarati nella ricetta sono `declared_image_fields`; (2) la maschera di uscita è l'unione dei bit di input più `master_invalid`; (3) un NaN in input accende solo il bit di input, mai `q_nonfinite`; (4) il controllo morto sull'offset è rimosso; (5) la geometria di estensione ha la precedenza (`fits.extension_geometry`); (6) `calib.master_state_unknown` per il descrittore assente sul master e `calib.role_slot` per il descrittore presente sul light; (7) T-M2E-04 si prova per overflow di `Fc`.
- Interfaccia e codici: sezioni "Aggiunte di M2.e" di ADR-16, ADR-17 e ADR-03.

## Tolleranze derivate

Fissate prima dell'esecuzione, nelle intestazioni di `test_calibration.cpp`, `test_calibration_stats.cpp`, `test_calibration_real.cpp` e in `calib_support.hpp`. L'oracolo è razionale esatto (`tests/fixtures/exact_rational.hpp`), indipendente dal prodotto.

| Grandezza | Tolleranza | Derivazione |
|---|---|---|
| Casi dyadic | zero | Valori e operazioni esatti in binario. |
| `C`, uscita f64 | `gamma_6 |C|` | Conteggio: `Lc` 1, `Fc` 1, N pari 2 (Fc centrale più somma; il dimezzamento è esatto), `Q = Fc/N` 1, `C = Lc/Q` 1. |
| `C`, uscita f32 | mezzo ulp f32 più `gamma_6 |C|` | Mezzo ulp = `2^max(e-24, -150)` per valori non nulli (nei subnormali lo spacing è `2^-149`, T-M2E-28). |
| `Var(C)` | `gamma_36 Var(C)` | Tutti i termini sono non negativi. Ramo Poisson: sub, div, add, div (4); ramo lettura: mul, add, div (3); `Var(Lc)`, `Var(Fc)`: più una somma (5). `Q`: 4; `q2`: 9; `q4`: 19; `vlc/q2`: 15; `Lc*Lc`: 3; `vq`: 11; `(Lc*Lc)*vq`: 15; `/q4`: 35; somma dei due termini: 36. |
| T-M2E-21, scarto di `Var(C)` | Wilson-Hilferty, K = 2000, `nu = K - 1`, 8 pixel, Bonferroni alfa complessivo 1e-3, due code, coda per pixel 6,25e-5 (z = 3,83) | Statistica `(K-1) s^2 / Var_propagata`, con `Var_propagata` dalla formula del primo ordine sui valori veri (`Var(Q)/Q^2 <= 1e-4`). |
| T-M2E-26, stima della varianza relativa di N | Wilson-Hilferty, K = 3000, `nu = K - 1`, alfa 1e-3 due code (coda 5e-4, z = 3,2905), intervallo del rapporto allargato di più o meno 2 % per l'approssimazione asintotica | Rapporto fra varianza empirica relativa di N e stima del prodotto. |
| T-M2E-13, campioni validi | mezzo ulp f32 più 2 `gamma_6 |C|` | Il prodotto e il test differiscono ciascuno al più di `gamma_6 |C|` dal valore esatto. |
| T-M2E-13, varianza | 1e-4 relativo | Formula del primo ordine ricalcolata dal test in double. |

## Esito per test

Tutti PASS, release; dev per tutti tranne T-M2E-13 e T-M2E-21/26 (`slow`). Per gli ID senza nome di test proprio il nome è nel file indicato, con prefisso `CalibM2E.` (`test_calibration.cpp`), `CalibJob.` (`test_calibration_job.cpp`), `CalibStats.` (`test_calibration_stats.cpp`), `CalibReal.` (`test_calibration_real.cpp`).

| ID | Test | Cosa verifica |
|---|---|---|
| T-M2E-01 | `CalibM2E.T_M2E_01` | Caso dyadic: scarto zero su `C` e `Var(C)`. |
| T-M2E-02 | `CalibM2E.T_M2E_02` | Valori non dyadic con forte cancellazione `L ~ D`, N pari e dispari, f32 e f64, entro le tolleranze derivate. |
| T-M2E-03 | `CalibM2E.T_M2E_03` | Bias nel dark: nessuna doppia sottrazione. |
| T-M2E-04 | `CalibM2E.T_M2E_04_05`, `T_M2E_04_nan_flat_sample...` | `q_nonfinite` provato per overflow di `Fc` (decisione 7); un campione NaN nel flat è invalido per input, non `q_nonfinite`. |
| T-M2E-05 | `CalibM2E.T_M2E_04_05` | Bit `q_nonpositive`, `q_below_min`, `q_unreliable` separati e insieme. |
| T-M2E-06 | `CalibM2E.T_M2E_06` | Normalizzazione globale e per canale CFA, con G1 diverso da G2. |
| T-M2E-07 | `CalibM2E.T_M2E_07` | Errori di normalizzazione ed esclusioni. |
| T-M2E-08 | `CalibM2E.T_M2E_08_*` (2) | Ogni campo di compatibilità fuori regola ferma; geometria diversa ferma. |
| T-M2E-09 | `CalibM2E.T_M2E_09` | Metadati mancanti, dichiarati, in conflitto. |
| T-M2E-10 | `CalibM2E.T_M2E_10` | Stati e ruoli dei master, IMAGETYP. |
| T-M2E-11 | `CalibJob.T_M2E_11` | Master modificato o rimosso dopo il preflight: il ramo si ferma. |
| T-M2E-12 | `CalibJob.T_M2E_12` | Immagine, report e testo pubblicati e coerenti con l'output. |
| T-M2E-13 | `CalibReal.T_M2E_13` (`T-M2E-13.real_palomar_lfc`, `slow`) | Dato reale PUB-LFC-01, bande g e i (vedi sotto). Fallisce se i file mancano o hanno SHA-256 diverso. |
| T-M2E-14 | `CalibM2E.T_M2E_14` | Unione delle maschere di input. |
| T-M2E-15 | `CalibM2E.T_M2E_15` | Saturazione dichiarata mascherata; non dichiarata, avviso. |
| T-M2E-16 | `CalibJob.T_M2E_16` | Uscita bit a bit identica per numero di worker e righe di tile. |
| T-M2E-17 | `CalibM2E.T_M2E_17` | RGB e multi-piano: stop prima del calcolo. |
| T-M2E-18 | `CalibM2E.T_M2E_18` | Dark all'esposizione del flat solo se dichiarato `dark_flat`. |
| T-M2E-19 | `CalibM2E.T_M2E_19` | Campi opzionali assenti elencati come ipotesi. |
| T-M2E-20 | dentro `CalibM2E.T_M2E_02` | Cancellazione `L ~ D` senza sottrazione doppia (nessun test proprio). |
| T-M2E-21 | `CalibStats.T_M2E_21` (`T-M2E-21-26.monte_carlo`, `slow`) | Monte Carlo seed fisso: varianza di `C` contro propagazione del primo ordine. |
| T-M2E-22 | `CalibM2E.T_M2E_22` | Digest condiviso fra input: stop. |
| T-M2E-23 | `CalibM2E.T_M2E_23` | Sorgenti e parametri della varianza. |
| T-M2E-24 | `CalibM2E.T_M2E_24` | Invarianti della varianza su pixel validi e invalidi. |
| T-M2E-25 | `CalibM2E.T_M2E_25` | Valore sotto l'offset: resta il solo rumore di lettura. |
| T-M2E-26 | `CalibStats.T_M2E_26` (`slow`), `CalibJob.T_M2E_26` | Stima della varianza relativa di N, flat uniforme e vignettato; il report segue la soglia del CV (`iid_assumption`, con `fc_cv:null` e `not_verified` se il CV non è finito). |
| T-M2E-27 | `CalibM2E.T_M2E_27` | Parametri e piedistallo. |
| T-M2E-28 | `CalibM2E.T_M2E_28` | Mezzo ulp f32 ai confini subnormali (nato dal giro 1). |
| T-M2E-29 | `CalibM2E.T_M2E_29_*` (3) | Overflow e underflow di `N^2` (anche subnormale) ed intermedio di pixel non finito: `calib.variance_overflow` o `result_nonfinite` (giri 1 e 3). |
| T-M2E-30 | `CalibM2E.T_M2E_30` | Il report registra valore e origine di binning, unità e linearità (giro 1). |
| T-M2E-31 | `CalibJob.T_M2E_31` | Geometria diversa alla rilettura rispetto al piano: stop `integrity` (giro 1). |
| T-M2E-32 | `CalibM2E.T_M2E_32_*` (4) | Underflow di varianza, quoziente, risultato f32 subnormale da f64 normale, passo del modello: `result_nonfinite` (giro 2). |
| T-FBK-01 (parte M2) | `CalibJob.T_FBK_01_*` (3) | Decoder che fallisce, budget insufficiente senza ridurre tile o worker, errore dello store in commit e store corrotto: stop, nessun percorso alternativo, nessun output. |
| T-M2F-02 | `T-M2F-02.m2e_network` (`tools/check_m2f02_network.sh`) | Vedi sotto. |

Test aggiuntivi senza ID di matrice: `CalibJob.input_changed_before_the_first_read_stops_without_decoding`, `incompatible_inputs_stop_before_any_output`, `recipe_must_match_the_slot_node`, `cancel_requested_publishes_nothing`; `CalibM2E.unit_and_linearity_must_be_declared`, `report_text_matches_report_content`.

## T-M2E-26: misure

- Flat uniforme: rapporto fra varianza empirica relativa di N e stima del prodotto 1,035, dentro l'intervallo derivato.
- Flat vignettato (più o meno 5 %): rapporto 8,33. È registrato come **limite**, non come gate: la stima assume pixel indipendenti e distribuiti in modo gaussiano e identico (ADR-17, ipotesi `iid_gaussian_fc`); con una vignettatura la stima sottovaluta la varianza di N. Il report dichiara l'ipotesi con `iid_assumption` (`not_verified` quando non si può verificare).

## T-M2E-13: dato reale PUB-LFC-01

Palomar LFC (Zenodo 10.5281/zenodo.3254683, CC BY 4.0, `docs/datasets/PUB-LFC-01.md`), immagini 2080x4128 (regione dati 2048 colonne, overscan a destra). I 19 file usati sono verificati per SHA-256 prima dell'uso; `data/public` non è versionato e il test fallisce, senza skip, se un file manca o ha un hash diverso. I master (dark, flat, dark-flat) sono medie di tre frame costruite dal test: il prodotto non combina master. Rumore di lettura e offset sono stimati dai bias `ccd.001` e `ccd.002`: `read_noise` 5,24178 ADU (sigma robusta della differenza diviso radice di 2), offset 1137 ADU (mediana). Misure da `tmp/m2e-real/report/real-{g,i}.txt`:

| Grandezza | Banda g | Banda i |
|---|---|---|
| N (mediana di Fc) | 20 120 | 21 006 |
| `n_valid` (pixel con flat non saturo) | 8 586 239 | 8 586 239 |
| Pixel validi | 8 384 636 (97,652 %) | 8 384 665 (97,6523 %) |
| Pixel invalidi | 201 604 | 201 575 |
| Bit `q_below_min` | 8669 | 9773 |
| Bit `q_nonpositive` | 188 662 | 188 556 |
| Bit `q_unreliable` | 12 941 | 13 018 |
| Bit `saturated` | 1 | 1 |
| Varianza media sui validi (ADU^2) | 782,729 | 3880,92 |
| Mediana di C, regione dati (ADU) | 701,134 | 3816,21 |
| CV delle mediane dei blocchi 64x64, prima di Q | 0,0474247 | 0,0325735 |
| CV delle mediane dei blocchi 64x64, dopo Q | 0,036437 | 0,011841 |
| Mediana di `L - D` nell'overscan (ADU) | -73,3334 | -70,6666 |
| `normalization_rel_var_estimate` di N | 2,77845e-12 | 2,65677e-12 |
| `iid_assumption` | `not_verified` | `not_verified` |
| `Fc <= 0` entrati in N | 188 662 | 188 556 |

- Il test verifica per ogni pixel maschera, N, campione e varianza contro il ricalcolo indipendente in double sui campioni dei master; l'unica asserzione fisica è la diminuzione del CV a grande scala dopo il flat (vera in entrambe le bande).
- Lo scarto di `L - D` nell'overscan (circa -73 ADU) è registrato **senza soglia**: l'overscan non è ritagliato dal prodotto.
- Avvisi nel report: `imagetyp_unrecognized:light:OBJECT`, `temperature_comparison_skipped:light-dark`, `temperature_comparison_skipped:flat-dark_flat`.
- Il light grezzo non porta offset, guadagno e saturazione: la ricetta li dichiara (offset 1137 ADU, guadagno 1,1 e- per ADU, saturazione 65 535 ADU, come da documentazione del dataset).

## T-M2F-02: nessuna rete

`tools/check_m2f02_network.sh` (ctest `T-M2F-02.m2e_network`, label `fast`) esegue due passi:

1. Canarino: `strace -f -e trace=network` su un tentativo di connessione su loopback, porta 1. Il test fallisce se il canarino non produce righe `socket` o `connect`, perché un controllo cieco non proverebbe nulla.
2. Il workflow completo `m2e_workflow` (store, job di calibrazione, pubblicazione) sotto lo stesso `strace`: passa solo se lo store è prodotto e la traccia ha **0** righe di rete.

Sotto ptrace LeakSanitizer non funziona, quindi il workflow gira con `detect_leaks=0`; la rilevazione dei leak resta nei test non tracciati dello stesso workflow. Esito: PASS in dev e in release (0,14 s in release).

## Review indipendente (Codex gpt-6-astra, sola lettura)

Artefatti in `.omc/artifacts/ask/codex-m2e-review*.md`. Revisione statica del codice: i giri non hanno compilato né eseguito i test.

- **Giro 1: FAIL.** 2 bloccanti, 1 MED, 1 LOW: geometria riletta non confrontata col piano (righe aggiunte pubblicate come valide); overflow di `N^2` che azzerava `Var(Q)` e aggirava `q_unreliable`; report senza `image_fields`; mezzo ulp errato nei subnormali f32 nel test. Corretti (T-M2E-28, 29, 30, 31).
- **Giro 2: FAIL.** 1 bloccante, 1 LOW: underflow silenzioso nei termini (`Var(C) = 0` pubblicato su pixel valido, quozienti sfuggiti ai controlli); il test del CV non finito non asseriva `fc_cv:null` e `iid_assumption:not_verified`. Corretti (T-M2E-32, T-M2E-26).
- **Giro 3: FAIL.** 1 bloccante: `N^2` subnormale positivo superava il controllo. Corretto con regressione dedicata (T-M2E-29).
- **Giro 4:** PASS, nessun rilievo residuo nei controlli per canale e nel kernel, nessuna invalidazione spuria (`.omc/artifacts/ask/codex-m2e-review4-20261029-0913.md`).

## Limiti dichiarati

- **Checkpoint e resume** non sono collegati alla calibrazione (`checkpoint_every` 0): un job interrotto riparte da zero solo per decisione esplicita, non in automatico.
- **Master non combinati dal prodotto**: dark, flat e dark-flat sono master già pronti; le medie sul dato reale le costruisce il test.
- **Overscan non ritagliato**: il prodotto non toglie l'overscan; sul dato reale i suoi pixel finiscono in maschera tramite `Q` e `L - D` vi vale circa -73 ADU (registrato senza soglia).
- **Input letti due volte**: dopo il preflight il digest di ogni input viene riverificato alla lettura (costo doppio di I/O).
- **Guardie protette solo insieme**: alcune guardie non sono isolabili per mutazione, perché il test T_M2E_29 le protegge solo congiuntamente e il quoziente f64 non è isolabile; una mutazione che rimuove una sola guardia può non essere rilevata.
- **Stima di N sotto vignettatura**: l'ipotesi `iid_gaussian_fc` non regge su un flat vignettato (rapporto 8,33 contro 1,035 su flat uniforme); nessun gate, solo dichiarazione nel report.
- **Soglie sul dato reale**: `q_min` 0,05 e `q_rel_sigma_max` 0,01 della ricetta sono valori di prova; nessuna è tarata sull'uscita.
- **Solo mono e CFA**: RGB e multi-piano si fermano (T-M2E-17). Matrice delle otto combinazioni, dark scalato, solo bias e passthrough restano a M3.c (ADR-16.b).
- **`ctest --preset release` esegue anche gli `slow`** (circa 146 s di CPU); il preset `dev` no, quindi T-M2E-13 e T-M2E-21/26 non sono stati eseguiti sotto ASan/UBSan.
