# Matrice dei test

Stato: **seed pianificato. Le righe `PIANIFICATO` non sono state eseguite; i test M2 eseguiti sono elencati nella sezione "M2: ID di lavoro dai contratti".** Una riga `PIANIFICATO` non è una prova e non chiude un gate. Solo un record in `docs/evidence/` con comando, hash, versioni, ambiente, esito e limiti può sostenere un esito.

## Regole

- **Tolleranze da derivare.** Nessun valore numerico è inventato in questa matrice. Ogni tolleranza si ricava dal contratto scientifico, dal ground truth o dal protocollo preregistrato prima dell'esecuzione, e si annota nel record.
- **Dataset.** Prima il sintetico autogenerato, mai foto private. Un dataset pubblico si usa solo dopo verifica di licenza, provenienza e hash. Gli ID qui sotto sono etichette di lavoro, non artefatti già disponibili.
- **Metodi.** Gli ID `T-*` e `F-*` sono quelli del piano 2.2.1. Nessun ID nuovo viene presentato come approvato.
- **Nessuna sostituzione.** T-FBK-01 non verifica solo il messaggio: inietta il guasto e accerta che il percorso alternativo non venga eseguito.
- **Costi.** Solo se esposti dal runtime.

## Seed

| Test | Requisito o gate | Dataset / fixture | Metodo | Metrica preregistrata | Tolleranza | Evidenza prevista | Stato |
|---|---|---|---|---|---|---|---|
| T-FBK-01 | NFR-FBK-01, NC-21; M2 core, M3, M4, M5 | `FIX-FAULT-01` guasti sintetici: backend assente, decoder che fallisce, GPU che si stacca | Iniezione del guasto; traccia delle chiamate ai backend; exit code; provenance dell'arresto | Zero invocazioni del percorso sostitutivo; nessun output pubblicato; errore non nullo | Binaria: sostituzione assente o presente | `docs/evidence/` | PARZIALE: parte M2 ESEGUITA (decoder che fallisce, budget insufficiente, store corrotto o guasto in commit: `CalibJob.T_FBK_01_*`, `docs/evidence/M2-e.md`); M3, M4, M5 PIANIFICATO |
| M2 slice calibrazione | Gate M2; F-CAL-01/02 | `FIX-SYN-FITS-01` FITS sintetico con dark e flat; ground truth; PUB-LFC-01 reale | Calibrazione `L - D`, flat `Q`, maschere, varianza, report | Errore numerico, varianza propagata, doppia sottrazione assente | Derivate prima della misura (zero per i dyadic, `gamma_6 |C|`, `gamma_36 Var(C)`, Wilson-Hilferty; vedi `docs/evidence/M2-e.md`) | `docs/evidence/M2-e.md` | ESEGUITO, PASS (T-M2E-01…32; T-M2E-26 e il flat vignettato registrati come limite) |
| T-CAL-08 | M3.c; F-CAL-02.b, F-CAL-03.b, F-CAL-04, F-CAL-06.b | `FIX-SYN-CAL-08`: otto combinazioni di master, incluso dark-flat e casi mancanti | Matrice delle ricette, omissioni pianificate, prerequisito perso | Correzioni applicate, omissioni dichiarate, stop sul prerequisito mancante | Da derivare per combinazione | `docs/evidence/` | PIANIFICATO — non eseguito |
| T-REG-02 | M3.f; F-REG-02 | `FIX-SYN-WARP-01` trasformazioni note; `PUB-REG-01` pubblico licenziato | Registrazione, residui, ambiguità | Errore di trasformazione, residuo, casi ambigui non pubblicati | Da derivare dalla trasformazione nota | `docs/evidence/` | PIANIFICATO — non eseguito |
| T-POST-09 | M3.l; F-POST-09.a.1 | `FIX-SYN-NOISE-01` rumore noto, struttura debole, stelle | Denoise lineare e non lineare, rollback | Bias, NPS, danno su struttura debole e stelle | Da derivare dal modello di rumore | `docs/evidence/` | PIANIFICATO — non eseguito |
| T-POST-10 | M3.o; F-POST-10.a.1 | `FIX-SYN-PSF-01` PSF nota; `PUB-DEEP-01` pubblico licenziato | Deconvoluzione P0, baseline no-op, sensibilità a PSF, fondo, lambda, rumore | Recupero, residuo, ringing, stabilità | Da derivare; baseline no-op obbligatoria | `docs/evidence/` | PIANIFICATO — non eseguito |
| T-POST-12 | M3.n; F-POST-12.a.1, F-POST-12.b.1 | `FIX-SYN-STARS-01` campo sintetico con stelle e segnale diffuso | Riduzione e separazione con gate separati; reintegrazione | Accuratezza del modello, danno non stellare, identità `R_T + S_T = T(I_L)` | Da derivare; identità entro tolleranza dichiarata | `docs/evidence/` | PIANIFICATO — non eseguito |
| T-AST-ASSIST01 | M3.p; F-REG-04.a | `FIX-SYN-FIELD-01` campi noti; pack assente e pack corrotto | Solver assistito, hint, validazione, falsa soluzione | Tasso di soluzione, falsa soluzione rifiutata, stop senza pack | Da derivare dal dominio dichiarato | `docs/evidence/` | PIANIFICATO — non eseguito |
| T-ANA-01, T-ANA-04 | M3.r; F-ANA-01.a…F-ANA-05.a | `FIX-ANA-CORPUS` sintetico; `PUB-CORPUS-01` accettazione separato | Advisor headless, schede, confronto, astensione | Copertura utile, astensione non sistematica, budget dell'intero batch | Da derivare dal corpus; astensione sempre non ammessa | `docs/evidence/` | PIANIFICATO — non eseguito |
| T-VAL-INDEP | M3.r; §24 | `PUB-CORPUS-01` indipendente e separato dallo sviluppo | Validazione su corpus non usato per tarare | Prestazione sul corpus indipendente | Da derivare prima dell'uso | `docs/evidence/` | PIANIFICATO — non eseguito |
| Linux L1 | M1 prove preliminari, M5; NFR-LINUX-01/02 | Ubuntu LTS supportata x86_64, kernel di distribuzione; sessione Wayland e X11 nativo; CPU-only | Installazione offline, avvio, pan/zoom, HiDPI, GNOME e KDE, recovery dopo crash, color management | Esito per combinazione esplicita; errore ICC deve fermare, nessuna sostituzione sRGB | Nessuna soglia empirica ancora fissata | `docs/evidence/` | PIANIFICATO — non eseguito |
| Linux L2 | M1 prove preliminari, M5; NFR-LINUX-01/02 | Debian stabile supportata x86_64, glibc di riferimento registrata; X11 nativo; CPU-only | Installazione offline, avvio, dipendenze conservative, recovery | Esito per configurazione | Nessuna soglia empirica ancora fissata | `docs/evidence/` | PIANIFICATO — non eseguito |
| Linux L3 | M1 prove preliminari, M5; NFR-LINUX-01/02, NFR-GPU-01 | Fedora supportata x86_64, Mesa registrata; Wayland; GPU AMD o Intel facoltativa | Installazione offline, avvio, percorso GPU opzionale, HiDPI | Esito per configurazione; CPU resta completa senza GPU | Nessuna soglia empirica ancora fissata | `docs/evidence/` | PIANIFICATO — non eseguito |
| Color management | M1/M5; §16.5 | Display profilato e profilo ICC guasto | Caricamento ICC, errore, resa colore | Errore ICC produce stop; nessun fallback sRGB automatico | Binaria: fallback assente o presente | `docs/evidence/` | PIANIFICATO — non eseguito |

## Versioni e licenze

Le tre famiglie Linux sono candidate da §16.2, non versioni fissate né misure. Prima della prova si congelano release supportata, kernel, glibc, desktop, sessione nativa, renderer e driver; ogni combinazione ha un esito distinto. La macchina di sviluppo non dimostra il supporto delle altre configurazioni. Ogni dipendenza e ogni dataset pubblico entra nell'inventario con versione, hash e licenza prima dell'adozione; nessun `LICENSE` di prodotto è emesso. Il SBOM si costruisce sui componenti realmente usati, non su un elenco previsto.

## M2: ID di lavoro dai contratti

Definiti in `docs/contracts/`; stato per unità, non per singolo test. Un ID si dichiara eseguito solo nel record di evidenza dell'unità.

| ID | Contratto | Unità | Stato |
|---|---|---|---|
| T-M2A-01…13 | ADR-03 | M2.a | ESEGUITO, PASS (`docs/evidence/M2-a.md`) |
| T-M2B-01…08, 10, 13 | ADR-05 | M2.b | ESEGUITO (T-M2B-10: record e provenance del runner; persistenza atomica e rilettura dopo riapertura in `CalibJob.T_M2B_10_provenance_is_persisted_for_success_stop_and_cancel_and_survives_reopen`, solo per `run_job_files`) |
| T-M2B-09 | ADR-05 | M2.b, M2.d | ESEGUITO: errore dell'input e confine del job (M2.b); blocco dei soli dipendenti con `run_branches` (`Branches.T_M2B_09_*`, M2.d, `docs/evidence/M2-d.md`) |
| T-M2B-11 | ADR-05 | M2.b, M2.c | ESEGUITO, PASS: chiave e forma canonica invariate dopo una migrazione dello schema SQLite (`Store.T_M2B_11`, `docs/evidence/M2-c.md`) |
| T-M2B-12 | ADR-05 | M2.b, M2.d | ESEGUITO: exit 2/3 e job marcato (M2.b); prenotazioni rilasciate su successo, stop e cancel (`Scheduler.T_M2B_12_*`, M2.d, `docs/evidence/M2-d.md`); arresti prima del lavoro registrati e propagati, registrazione fallita con `registrazione_arresto`, avvio fallito senza job (`Job.T_M2B_12_*`, `Store.T_M2B_12_*`, R5.d, `docs/evidence/M2-R5d.md`) |
| `io input.short_read` (ADR-05) | ADR-05 | M2.a | ESEGUITO, PASS: `Fits.T_M2A_short_read_with_unchanged_size_stops_as_io` (punto `input.pread` del `fault_injector`, 0 byte con dimensione invariata; che il test fallisca senza il ramo è dedotto da lettura del codice, non provato da una mutazione eseguita) |
| T-M2C-01…25 | ADR-04 | M2.c | ESEGUITO, PASS (T-M2C-11 esteso all'intera calibrazione: `CalibJob.T_M2C_11_the_four_inputs_are_untouched_by_the_whole_calibration_workflow`: digest, permessi, inode e mtime dei quattro input) (`docs/evidence/M2-c.md`); durabilità contro perdita di corrente e processo malevolo stesso UID fuori dal modello; `readdir` coperto da `Export.T_M2A_readdir_failure_while_verifying_stops_as_io_and_names_errno` (punto `dir.readdir`; che il test fallisca senza il ramo è dedotto da lettura del codice, non provato da una mutazione eseguita) |
| T-M2D-01…15 | ADR-06 | M2.d | ESEGUITO, PASS (`docs/evidence/M2-d.md`; test `Budget.T_M2D_01…03`, `Pool*.T_M2D_04/12`, `Scheduler.T_M2D_05…08/10/11/14/15`, `Resume.T_M2D_09`, `Spill.T_M2D_10`; T-M2D-13 è `test_m1d_protocol`, label `slow`). Soglie F-RES-01 dal gate M2 con carichi reali (`docs/evidence/M2-gate.md`): frazione normale 0.375 CONFERMATA (perimetro: il PC di sviluppo); pressione 0.25 e PSI 10.0 confermate (S4 quinquies); page cache dello spill fuori dal budget; TSan non nei preset |
| T-M2E-01…32 | ADR-16, ADR-17, ADR-03 | M2.e | ESEGUITO, PASS (`docs/evidence/M2-e.md`; test `T_M2E_*` nelle suite `CalibM2E`, `CalibJob`, `CalibStats`, `CalibReal`; T-M2E-20 dentro `T_M2E_02`; 28-32 nati dalle review; T-M2E-13 su PUB-LFC-01 e T-M2E-21/26 sono `slow`). Limiti: checkpoint/resume non collegato, master non combinati dal prodotto, overscan non ritagliato, stima di N non valida su flat vignettato (rapporto 8,33, nessun gate) |
| T-FBK-01 (parte M2) | NFR-FBK-01, NC-21 | M2.e | ESEGUITO, PASS (`CalibJob.T_FBK_01_*`, `docs/evidence/M2-e.md`) |
| T-M2F-01 | ADR-13 | M2.f | ESEGUITO, PASS (`docs/evidence/M2-f.md`) |
| T-M2F-02 | ADR-13 | M2.e | ESEGUITO, PASS (`tools/check_m2f02_network.sh`, ctest `T-M2F-02.m2e_network`: canarino visto da strace, 0 righe di rete sul workflow; `docs/evidence/M2-e.md`) |
| T-M2R0-01 | NFR-LIC-01.c | R0 | ESEGUITO, PASS in dev e release: `T-M2R0-01.licenses` (`tools/check_licenses.py`) e `T-M2R0-01.licenses_negative` (riga mancante, hash alterato, licenza vuota rilevati); `docs/evidence/M2-R0.md` |
| T-M2R0-02 | NFR-SEC-02 | R0 | ESEGUITO, PASS in dev e release: `T-M2R0-02.noexec` (`tools/check_noexec.sh` su `libastrocore`) e `.noexec_negative` (dieci chiamate vietate rilevate su oggetti di prova); `M2-R0.md` |
| T-M2R0-03 | C-10.a campo 10 | R0.b | ESEGUITO, PASS in dev e release: `Export.T_M2R0_03_enospc_at_every_export_write_point_*` (ENOSPC a ognuno dei punti di scrittura dell'export: stop `io` con errno 28, nessun `COMMIT`, nessun temporaneo, store invariato, export successivo riuscito); `docs/evidence/M2-R0b.md`. Limite: iniezione, non filesystem pieno |
| T-M2R0-04 | ADR-02, D-04 | R0 | ESEGUITO, PASS in dev e release: `T-M2R0-04.headless` (`tools/check_core_headless.sh`: nessun simbolo, `NEEDED` né riferimento di build Qt in `libastrocore` e `test_build_info`) e `.headless_negative`; `M2-R0.md` |
| T-M2R1-01 | F-IO-05 | R1 | ESEGUITO: `Inventory.T_M2R1_01_*`, byte letti contati dall'osservatore di `pread` del test, EIO a ogni offset di pixel; `docs/evidence/M2-R1.md` |
| T-M2R1-02 | F-IO-05 | R1 | ESEGUITO: `Inventory.T_M2R1_02_*`, prenotazione uguale alla formula, minimo nel contesto dello stop |
| T-M2R1-03 | F-IO-05 | R1 | ESEGUITO: `Inventory.T_M2R1_03_*`, anteprima contro l'oracle in long double (tolleranza derivata, zero sul dyadic) |
| T-M2R1-04 | F-IO-05, NFR-DATA-01.a | R1 | ESEGUITO: `Thumbnail.T_M2R1_04_T_M2R1_11_*`, voce alterata: `cache.corrupt`, file invariato, nessun `cache.rebuild` |
| T-M2R1-05 | F-IO-10 | R1 | ESEGUITO: `Phase.T_M2R1_05_*`, `PhaseCalib.T_M2R1_05_*`, `Import.T_M2R1_05_*` (matrice 3x3, dichiarazioni, calibrazione, import) |
| T-M2R1-06 | F-IO-10 | R1 | ESEGUITO: `Import.T_M2R1_06_*`, calibrato e prestacked senza RAW inventariati, anteprima, export, limite C-14 |
| T-M2R1-07 | F-IO-01 | R1 | ESEGUITO: `Import.T_M2R1_07_*`, `PhaseCalib.T_M2R1_07_*`, keyword nei report e nella provenance |
| T-M2R1-08 | NFR-REP-01.a | R1 | ESEGUITO: `Import.T_M2R1_08_*`, report riletto dopo la riapertura |
| T-M2R1-09 | F-IO-01 | R1 | ESEGUITO: ctest `T-M2R1-09.public_samples` (slow), PUB-LFC-01 e PUB-CFA-01 |
| T-M2R1-10 | F-IO-05, NFR-SEC-01 | R1 | ESEGUITO: `InventoryFuzz.T_M2R1_10_*` (limiti, tile compressi, fuzz di 800 file, lettura oltre la fine) |
| T-M2R1-11 | F-IO-05, NFR-DATA-01.a | R1 | ESEGUITO: `Thumbnail.T_M2R1_04_T_M2R1_11_*`, job `cache.rebuild` |
| T-M2R1-12 | F-IO-05, NFR-DATA-01.a | R1 | ESEGUITO: `Thumbnail.T_M2R1_12_*`, `computed`/`cache_hit`, quota, symlink, hardlink, cleanup confinato |
| T-M2R2-01 | F-CAL-01.a, C-02.a | R2 | ESEGUITO: `Master.T_M2R2_01_*`, dyadic con scarto 0 su media e piano (n = 2 e 4, f32 e f64), unione dei bit, saturazione, underflow e overflow `result_nonfinite`; `docs/evidence/M2-R2.md` |
| T-M2R2-02 | F-CAL-01.a, C-02.a | R2 | ESEGUITO: `Master.T_M2R2_02_*`, non dyadic con forte cancellazione, n = 5 e 6, entro i bound di C-02.a §6.3 |
| T-M2R2-03 | F-CAL-01.a, C-02.a | R2 | ESEGUITO: `Master.T_M2R2_03_*`, outlier non scartati, zero frame scartati nel report |
| T-M2R2-04 | F-CAL-01.a, C-02.a, ADR-17.a | R2 | ESEGUITO: ctest `T-M2R2-04.monte_carlo` (slow), chi-quadro esatto e Chebyshev con Bonferroni su 68 statistiche |
| T-M2R2-05 | F-CAL-01.a, C-02.a | R2 | ESEGUITO: `Master.T_M2R2_05_*`, `MasterJob.T_M2R2_05_*`, ogni campo di compatibilità, parametri, domini, stop senza master |
| T-M2R2-06 | F-CAL-01.a, C-02.a, NFR-REP-01.a | R2 | ESEGUITO: `Master.T_M2R2_06_*`, `MasterJob.T_M2R2_06_*`, nodo canonico, riapertura e ricostruzione con stessi digest |
| T-M2R2-07 | F-CAL-01.a, C-02.a | R2 | ESEGUITO: `MasterJob.T_M2R2_07_*`, pila oltre il budget a blocchi bit-identica, ordine canonico, minimo dichiarato, file cambiato |
| T-M2R2-08 | F-CAL-01.a, C-02.a, NFR-REP-01.a | R2 | ESEGUITO: `MasterJob.T_M2R2_08_*`, report, testo, metadati, statistiche per canale, provenance |
| T-M2R2-09 | F-CAL-01.a, C-02.a, F-CAL-02.a, F-CAL-03.a, F-CAL-06.a | R2 | ESEGUITO: ctest `T-M2R2-09.real_palomar_product_masters` (slow), master da PUB-LFC-01 e T-M2E-13 su quei master |
| T-M2R2R-01 | F-RES-03 | R2.r | ESEGUITO (dev e release): `Resources.T_M2R2R_01_*`, modello del master sui casi p-z di ADR-06 (estensione R2.r), piano non rappresentabile, ingressi non usati rifiutati, `compute_plan_bounds` coerente; `docs/evidence/M2-R2r.md` |
| T-M2R2R-02 | F-CAL-01.a, F-RES-03, C-02.a | R2.r | ESEGUITO (dev e release): `MasterJob.T_M2R2R_02_*`, worker e tile automatici dal pianificatore (budget ampio, budget pari al minimo con tile 1, `cpu_set` di una CPU con l'affinità letta in ogni task), master bit-identico al calcolo in memoria |
| T-M2R2R-03 | F-CAL-01.a, F-RES-03, C-02.a | R2.r | ESEGUITO (dev e release): `MasterJob.T_M2R2R_03_*`, coda dei task configurata (7) e margine nel `job_plan` e nella provenance |
| T-M2R2R-04 | F-CAL-01.a, F-RES-03, C-02.a | R2.r | ESEGUITO (dev e release): `MasterJob.T_M2R2R_04_*`, minimo − 1, worker espliciti oltre il budget, tile esplicito: stop prima di ogni prenotazione, nessun master |
| T-M2R2R-05 | F-CAL-01.a, C-02.a | R2.r | ESEGUITO (dev e release): `MasterJob.T_M2R2R_05_*`, configurazione con le origini e piano scelto nella provenance con lo snapshot; senza snapshot nessun `budget` |
| T-M2R5-01 | NFR-SIMD-01, ADR-07.a, NFR-CPU-01.a | R5 | ESEGUITO (dev e release): suite `CalibM2E`, `CalibJob`, `Master`, `MasterJob`, `MasterPipeline` scoperte con `scalar` e rieseguite con `ASTRO_TEST_SIMD=avx2` (ctest `T-M2R5-01.avx2.{test_calibration,test_calibration_job,test_master,test_calibration_r32}`), con dentro T-M2E-01, 02, 20, T-M2R2-01, 02 e la residenza a blocchi; `docs/evidence/M2-R5.md` |
| T-M2R5-02 | NFR-SIMD-01, ADR-07.a | R5 | ESEGUITO (dev e release): `Simd.T_M2R5_02_*`, 0 ulp su valori, varianza e bit di calibrazione (228 configurazioni) e master (320), `pixel_sum` coi due schemi dichiarati entro il bound congiunto in aritmetica esatta |
| T-M2R5-03 | NFR-SIMD-01, ADR-07.a | R5 | ESEGUITO (dev e release): `Simd.T_M2R5_03_*`, tabella del dispatch, capacità iniettate, `simd` assente, `avx2` senza AVX2 (`cpu.avx2_unavailable`, zero chiamate), `auto` senza AVX2 su `cpu_scalar`, `auto` risolto diversamente (`*.node_mismatch`) |
| T-M2R5-04 | NFR-SIMD-01, ADR-07.a | R5 | ESEGUITO (dev e release): `Simd.T_M2R5_04_*`, percorso in backend, chiave, parametri e `details.cpu` dei job di calibrazione e master; resume con percorso diverso `resume.recipe` senza esecuzioni |
| T-M2R5-05 | NFR-SIMD-01, ADR-07.a | R5 | ESEGUITO (dev e release): ctest `T-M2R5-05.kernel_instructions` (`tools/check_simd_binary.sh objects`) e self-test negativo |
| T-M2R5-06 | NFR-SIMD-01, ADR-07.a | R5 | ESEGUITO (dev e release): `Simd.T_M2R5_06_*`, ingressi f32 subnormali validi, quozienti e medie subnormali `result_nonfinite`, somma con segno nulla valida, conversioni attorno a `2^-150`, divisione dopo le riduzioni entro il bound |
| T-M2R5-07 | NFR-SIMD-01, ADR-07.a; NFR-FBK-01.a, G-STOP (R5.c, R5.d) | R5, R5.c, R5.d | ESEGUITO (dev e release): `Simd.T_M2R5_07_*` (FZ, DAZ, RC prima del preflight, nei job e nei worker via `fault_injector`; maschere di eccezione spente prima del preflight e all'avvio del task; contesto `mxcsr`; errno iniettato) e ctest `T-M2R5-07.no_crtfastmath` con self-test negativo. R5.c: con DM spenta `run_job`, `run_job_files`, `run_branches` e i job di calibrazione, master e import si fermano prima di leggere il nodo; ogni task nei worker si ferma prima di leggere il blocco (residenza a blocchi, master); processo figlio con gestore di SIGFPE; `docs/evidence/M2-R5c.md`. R5.d: l'arresto al confine è registrato (job `failed` numeric `cpu.fp_mode` con la chiave del nodo, sullo store durevole la sola provenance dell'arresto senza nodo), anche con FZ e con tutte le eccezioni smascherate, senza segnale; `docs/evidence/M2-R5d.md` |

## M3.entry: ID di lavoro di ADR-08.b

Definiti in `docs/contracts/ADR-08-decoder-pack.md` §6, con tolleranze e criteri fissati prima di ogni misura. Nessuno è eseguito: le righe sono piani, non prove.

| ID | Requisito o gate | Contratto | Unità | Stato |
|---|---|---|---|---|
| T-M3A-RAW-01 | F-IO-02; ADR-08.b §2.3 (H2) | ADR-08.b | M3.a | verde (U2) |
| T-M3A-RAW-02 | F-IO-02, F-IO-04; ADR-08.b §2.2, §2.4 (H1, H3) | ADR-08.b | M3.a | verde (U2) |
| T-M3A-RAW-03 | F-IO-02; ADR-08.b §2.3 (stop) | ADR-08.b | M3.a | verde (U2) |
| T-M3A-RAW-04 | F-IO-04; ADR-08.b §2.4 (H4) | ADR-08.b | M3.a | verde (U2) |
| T-M3A-RAW-05 | NFR-SEC-01, NC-21; ADR-08.b §2.1, §2.5 | ADR-08.b | M3.a | verde (U3) |
| T-M3A-RAW-06 | ADR-06; ADR-08.b §2.8 | ADR-08.b | M3.a | verde (U3) |
| T-M3A-RAW-07 | ADR-13, NFR-SEC-04; ADR-08.b §2.1, §5 | ADR-08.b | M3.a | PIANIFICATO — non eseguito |
| T-M3A-RAW-08 | ADR-06; ADR-08.b §2.7 (H6, creazioni di thread con `pthread_create` interposto) | ADR-08.b | M3.a | verde (U3) |
| T-M3A-RAW-09 | F-IO-02 (più marche); ADR-08.b §2.3, §2.6 | ADR-08.b | M3.a | PIANIFICATO — non eseguito; richiede `PUB-RAW-01` acquisito (record scritto, stato DA ACQUISIRE) |
| T-M3A-RAW-10 | NFR-SEC-04; ADR-08.b §2.5 (H5) | ADR-08.b | M3.a | verde (U3) |
| T-M3A-RAW-11 | F-IO-04, §9.6; ADR-08.b §2.9 | ADR-08.b | M3.a | PIANIFICATO — non eseguito |
| T-M3A-RAW-12 | ADR-05 (chiave del nodo, determinismo); ADR-08.b §2.9 | ADR-08.b | M3.a | PIANIFICATO — non eseguito |
| T-M3A-PACK-01 | G-PACK, P-PACK-MISSING, G-STOP; ADR-08.b §3 | ADR-08.b | M3.a (pack sintetico) | verde (U10) |
| T-M3A-PACK-02 | G-PACK, P-PACK-MISSING, G-STOP; ADR-08.b §3 | ADR-08.b | M3.a (pack sintetico) | verde (U10) |
| T-M3A-PACK-03 | G-PACK, P-PACK-MISSING, G-STOP; ADR-08.b §3 | ADR-08.b | M3.a (pack sintetico) | verde (U10) |
| T-M3A-PACK-04 | G-PACK, P-PACK-MISSING, G-STOP; ADR-08.b §3 | ADR-08.b | M3.a (pack sintetico) | verde (U10) |
| T-M3A-PACK-05 | G-PACK, P-PACK-MISSING, G-STOP; ADR-08.b §3 | ADR-08.b | M3.a (pack sintetico) | verde (U10) |
| T-M3A-PACK-06 | G-PACK, P-PACK-MISSING, G-STOP; ADR-08.b §3 | ADR-08.b | M3.a (pack sintetico) | verde (U11) |
| T-M3A-PACK-07 | G-PACK, P-PACK-MISSING, G-STOP; ADR-08.b §3 | ADR-08.b | M3.a (pack sintetico) | verde (U10) |
| T-M3A-PACK-08 | G-PACK, P-PACK-MISSING, G-STOP; ADR-08.b §3 | ADR-08.b | M3.a (pack sintetico) | verde (U11) |
| T-M3A-PACK-09 | G-PACK, P-PACK-MISSING, G-STOP; ADR-08.b §3 | ADR-08.b | M3.a (pack sintetico) | verde (U10, U11) |
| T-M3A-LIC-01 | NFR-LIC-01.d; ADR-08.b §4 | ADR-08.b | M3.a | verde (U10) |

## M3.entry: ID di lavoro di ADR-03.b

Definiti in `docs/contracts/ADR-03-pixel-maschere.md` §b.10 (01–12) e §b.17 (13–18), con criteri fissati prima di ogni misura. Nessuno è eseguito: le righe sono piani, non prove.

| ID | Requisito o gate | Contratto | Unità | Stato |
|---|---|---|---|---|
| T-M3A-TYP-01 | F-IO-02; ADR-03.b §b.1 (origine `decoder`) | ADR-03.b | M3.a | verde (U1) |
| T-M3A-TYP-02 | F-IO-02, §9.6; ADR-03.b §b.2 (`decoder_state`) | ADR-03.b | M3.a | verde (U1) |
| T-M3A-TYP-03 | F-IO-02; ADR-03.b §b.1 (`capture_time_libraw`) | ADR-03.b | M3.a | verde (U1) |
| T-M3A-TYP-04 | F-CAL-08, C-03; ADR-03.b §b.3 (`demosaic`, `sample_kind`) | ADR-03.b | M3.d | PIANIFICATO — non eseguito |
| T-M3A-TYP-05 | F-CAL-08; ADR-03.b §b.3 (keyword `ASTRD*`) | ADR-03.b | M3.d | PIANIFICATO — non eseguito |
| T-M3A-TYP-06 | ADR-17; ADR-03.b §b.4 (`variance_scope`) | ADR-03.b | M3.d | PIANIFICATO — non eseguito |
| T-M3A-TYP-07 | F-REG-03, §8.6.2; ADR-03.b §b.5 (coverage) | ADR-03.b | M3.g | PIANIFICATO — non eseguito |
| T-M3A-TYP-08 | ADR-03.b §b.6 (bit `support_incomplete`) | ADR-03.b | M3.d | PIANIFICATO — non eseguito |
| T-M3A-TYP-09 | C-11, F-POST-08.a.1; ADR-03.b §b.7 (tipi distinti) | ADR-03.b | M3.j | PIANIFICATO — non eseguito |
| T-M3A-TYP-10 | C-11, §7.14; ADR-03.b §b.7 (`mask_info`) | ADR-03.b | M3.j | PIANIFICATO — non eseguito |
| T-M3A-TYP-11 | NFR-SEC-01; ADR-03.b §b.8 (limiti con `COVERAGE`) | ADR-03.b | M3.g | PIANIFICATO — non eseguito |
| T-M3A-TYP-12 | ADR-03.b §b.8 (`equivalent_content` sui campi nuovi; coppia «solo provenienza» di §b.15) | ADR-03.b | M3.a | verde (U1) |
| T-M3A-TYP-13 | §8.6.2, C-10.b; ADR-03.b §b.12 (`alpha_plane`, tipi distinti) | ADR-03.b | M3.s | PIANIFICATO — non eseguito |
| T-M3A-TYP-14 | NFR-EXP-01.b, C-10.b; ADR-03.b §b.12 (`derive_alpha`) | ADR-03.b | M3.s | PIANIFICATO — non eseguito |
| T-M3A-TYP-15 | §9.8; ADR-03.b §b.13 (`measure`, keyword `ASTRMCAP`/`ASTRMLnn`) | ADR-03.b | M3.a | verde (U1b; parte M3.d di TYP-16 aperta) |
| T-M3A-TYP-16 | §7.3, §9.8; ADR-03.b §b.13 (limitazioni, unione, idoneità per misura) | ADR-03.b | M3.a | verde (U1b; parte M3.d di TYP-16 aperta) |
| T-M3A-TYP-17 | §9.8, F-CAL-02.b, F-CAL-03.b; ADR-03.b §b.13 (codici della calibrazione `.b`) | ADR-03.b, ADR-16.b | M3.c | PIANIFICATO — non eseguito |
| T-M3A-TYP-18 | G-CAL; ADR-03.b §b.14 (tabella delle operazioni, ADR-16.b §b.2) | ADR-03.b, ADR-16.b | M3.a | PIANIFICATO — non eseguito |

## M3.entry: ID di lavoro di ADR-16.b e D-16

Definiti in `docs/contracts/ADR-16-stati-master.md` §b.13, con tolleranze e criteri fissati prima di ogni misura. Nessuno è eseguito: le righe sono piani, non prove.

| ID | Requisito o gate | Contratto | Unità | Stato |
|---|---|---|---|---|
| T-M3A-CAL-01 | G-CAL, P-DFB000…111, P-DF-*, P-FLAT-*; ADR-16.b §b.1–b.6 | ADR-16.b, D-16 | M3.a | PIANIFICATO — non eseguito |
| T-M3A-CAL-02 | D-16; ADR-16.b §b.5 (controllo termico), §b.6 | ADR-16.b, D-16 | M3.a | PIANIFICATO — non eseguito |
| T-M3A-CAL-03 | F-CAL-02.b, F-CAL-03.b; ADR-16.b §b.5 (doppia sottrazione), §b.12 | ADR-16.b | M3.c | PIANIFICATO — non eseguito |
| T-M3A-CAL-04 | F-CAL-02.b, D-16; ADR-16.b §b.10 | ADR-16.b, D-16 | M3.c | PIANIFICATO — non eseguito |
| T-M3A-CAL-05 | G-STOP, P-LOST, NC-21; ADR-16.b §b.8 | ADR-16.b | M3.c | PIANIFICATO — non eseguito |
| T-M3A-CAL-06 | G-STOP, P-FLAT-UNKNOWN, G-OFFSET, P-OFFSET; ADR-16.b §b.9 | ADR-16.b | M3.c | PIANIFICATO — non eseguito |
| T-M3A-CAL-07 | G-CAL, F-IO-04; ADR-16.b §b.7 (master da RAW) | ADR-16.b | M3.b | PIANIFICATO — non eseguito |
| T-M3A-CAL-08 | G-CAL, G-OFFSET; ADR-16.b §b.7 (compatibilità del decoder) | ADR-16.b | M3.a | PIANIFICATO — non eseguito |
| T-M3A-CAL-09 | P-LOST; ADR-16.b §b.7, §b.8 | ADR-16.b | M3.a (light), M3.b (master) | PIANIFICATO — non eseguito |
| T-M3A-CAL-10 | G-STOP, resume; ADR-16.b §b.8 | ADR-16.b | M3.c | PIANIFICATO — non eseguito |

## M3.entry: ID di lavoro di C-01.b

Definiti in `docs/contracts/C-01-calibrazione-light.md` §9, con soglie e criteri fissati prima di ogni misura. Nessuno è eseguito: le righe sono piani, non prove.

| ID | Requisito o gate | Contratto | Unità | Stato |
|---|---|---|---|---|
| T-M3A-C01-01 | G-OFFSET, F-IO-04; C-01.b §1 (stato dell'offset del light) | C-01.b | M3.a | PIANIFICATO — non eseguito |
| T-M3A-C01-02 | G-OFFSET, P-OFFSET; C-01.b §1, ADR-16.b §b.5 | C-01.b | M3.a | PIANIFICATO — non eseguito |
| T-M3A-C01-03 | G-OFFSET, F-IO-04; C-01.b §10 (voce `light_offset`) | C-01.b | M3.a | PIANIFICATO — non eseguito |
| T-M3A-C01-04 | G-OFFSET, F-CAL-02.b, T-CAL-08; C-01.b §1, §9 (bias residuo, nero non sottratto) | C-01.b | M3.c | PIANIFICATO — non eseguito |
| T-M3A-C01-05 | G-OFFSET, P-OFFSET; C-01.b §6 (residuo del percorso parziale) | C-01.b | M3.c | PIANIFICATO — non eseguito |
| T-M3A-C01-06 | G-CAL; C-01.b §2 (dominio del light), ADR-16.b §b.2 | C-01.b | M3.a | PIANIFICATO — non eseguito |
| T-M3A-C01-07 | F-CAL-06.b, §9.8; C-01.b §7 (coverage) | C-01.b | M3.a (analisi), M3.c (esecuzione) | PIANIFICATO — non eseguito |
| T-M3A-C01-08 | F-CAL-06.b; C-01.b §6 (varianza delle catene) | C-01.b | M3.c | PIANIFICATO — non eseguito |
| T-M3A-C01-09 | F-CAL-02.b, F-CAL-03.b; C-01.b §7, §9 (perdita di segnale) | C-01.b | M3.c | PIANIFICATO — non eseguito |
| T-M3A-C01-10 | F-CAL-02.b, T-CAL-08; C-01.b §9 (otto combinazioni, flat residuo) | C-01.b | M3.c | PIANIFICATO — non eseguito |
| T-M3A-C01-11 | F-CAL-02.b; C-01.b §8 (determinismo) | C-01.b | M3.c | PIANIFICATO — non eseguito |

## M3.entry: ID di lavoro di D-08 e NFR-TEST-02

Definiti in `docs/decisions/D-08-dataset.md` §6, con criteri fissati prima di ogni esecuzione. Nessuno è eseguito: le righe sono piani, non prove.

| ID | Requisito o gate | Contratto | Unità | Stato |
|---|---|---|---|---|
| T-M3A-DAT-01 | D-08, NFR-TEST-02, NFR-LIC-01.d; D-08 §3, §4 regole 1-2 (record dei dataset) | D-08 | M3.a | PIANIFICATO — non eseguito |
| T-M3A-DAT-02 | NFR-TEST-02; D-08 §4 regola 3 (una sola casa dell'hash) | D-08 | M3.a | PIANIFICATO — non eseguito |
| T-M3A-DAT-03 | NFR-TEST-02, NC-21, NC-16; D-08 §4 regole 4-7 (verifica prima dell'uso, dato assente, sola lettura, solo file registrati) | D-08 | M3.a | PIANIFICATO — non eseguito |
| T-M3A-DAT-04 | NFR-TEST-02, NC-21; D-08 §4 regola 5 (nessuno skip, guardia «0 tests») | D-08 | M3.a | PIANIFICATO — non eseguito |
| T-M3A-DAT-05 | NFR-TEST-02, F-ANA-05.a; D-08 §4 regola 9 (corpus separato) | D-08 | M3.r | PIANIFICATO — non eseguito |
| T-M3A-DAT-06 | NFR-TEST-02; D-08 §5 (copertura delle fixture sintetiche) | D-08 | Gate M3 (M3.s) | PIANIFICATO — non eseguito |

## M3.entry: ID di lavoro di ADR-17.b e D-17.b

Definiti in `docs/contracts/ADR-17-varianza.md` §b.12, con tolleranze, soglie Monte Carlo (protocollo MC-lin) e potenza fissate prima di ogni misura. Nessuno è eseguito: le righe sono piani, non prove.

| ID | Requisito o gate | Contratto | Unità | Stato |
|---|---|---|---|---|
| T-M3C-VAR-01 | F-CAL-06.b; ADR-17.b §b.2, §b.10 (regola I) | ADR-17.b | M3.c | PIANIFICATO — non eseguito |
| T-M3C-VAR-02 | F-CAL-06.b; ADR-17.b §b.2 (bias in due catene), §b.12 (MC-lin) | ADR-17.b | M3.c | PIANIFICATO — non eseguito |
| T-M3C-VAR-03 | F-CAL-06.b; ADR-17.b §b.3 (master comuni fra light) | ADR-17.b | M3.c | PIANIFICATO — non eseguito |
| T-M3C-VAR-04 | F-CAL-06.b, G-CAL; ADR-17.b §b.3, §b.9, §b.11 | ADR-17.b | M3.c | PIANIFICATO — non eseguito |
| T-M3C-VAR-05 | F-CAL-08; ADR-17.b §b.4 (blocco fra canali) | ADR-17.b, D-17.b | M3.d | PIANIFICATO — non eseguito |
| T-M3C-VAR-06 | F-REG-03; ADR-17.b §b.1, §b.4, §b.5 (pullback) | ADR-17.b, D-17.b | M3.g | PIANIFICATO — non eseguito |
| T-M3C-VAR-07 | F-REG-03; ADR-17.b §b.5 (`R C Rᵀ`) | ADR-17.b | M3.g | PIANIFICATO — non eseguito |
| T-M3C-VAR-08 | F-STK-01, F-CAL-06.b; ADR-17.b §b.6 | ADR-17.b, D-17.b | M3.h | PIANIFICATO — non eseguito |
| T-M3C-VAR-09 | F-POST-06.a.1, G-NOISE-NL; ADR-17.b §b.7 | ADR-17.b | M3.k | PIANIFICATO — non eseguito |
| T-M3C-VAR-10 | F-REG-03; ADR-17.b §b.5 (geometria non propagata) | ADR-17.b, D-17.b | M3.g | PIANIFICATO — non eseguito |
| T-M3C-VAR-11 | NC-21; ADR-17.b §b.8 (ammissione degli ingressi `marginal`) | ADR-17.b, D-17.b | M3.g, M3.h, M3.k | PIANIFICATO — non eseguito |

## M3.entry: ID di lavoro di C-02.b

Definiti in `docs/contracts/C-02-master.md` §b.12, con tolleranze, livelli statistici e criteri fissati prima di ogni misura. T-M3B-C02-06 e 07 sono caratterizzazioni senza soglia, a sostegno dei limiti dichiarati. Nessuno è eseguito: le righe sono piani, non prove.

| ID | Requisito o gate | Contratto | Unità | Stato |
|---|---|---|---|---|
| T-M3B-C02-01 | F-CAL-01.b; C-02.b §b.1, §b.6, regola I di ADR-17.b §b.10 (valori dei metodi) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-C02-02 | F-CAL-01.b; C-02.b §b.1 (`sigma_clip`, appartenenza all'insieme rifiutato, Samuelson) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-C02-03 | F-CAL-01.b; C-02.b §b.6 (piano della media pesata) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-C02-04 | F-CAL-01.b; C-02.b §b.6 (piano `gaussian_reference` di mediana e winsorized) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-C02-05 | F-CAL-01.b; C-02.b §b.6 (accuratezza di `κ`) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-C02-06 | F-CAL-01.b; C-02.b §b.6, ADR-17.b §b.1 (selezione congelata, caratterizzazione) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-C02-07 | F-CAL-01.b; C-02.b §b.3 (quantizzazione della mediana, caratterizzazione) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-C02-08 | F-CAL-01.b, F-IO-03; C-02.b §b.2 (analisi, gruppi, frame incompatibile escluso prima della scelta) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-C02-09 | F-CAL-01.b; C-02.b §b.2, §b.4 (proposta secondo `n`, domini) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-C02-10 | F-CAL-01.b; C-02.b §b.8 (master riproducibili bit-esatti dal progetto) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-C02-11 | F-CAL-01.b; C-02.b §b.7 (report di frame usati e scartati, rumore, artefatti, stato) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-C02-12 | F-CAL-01.b; C-02.b §b.13 punto 1, ADR-03 R1 (codici di fase e linearità) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-C02-13 | F-CAL-01.b; C-02.b §b.6 (`single`, modello di rumore) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-C02-14 | F-CAL-01.b; C-02.b §b.6 (flat CFA per canale) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-C02-15 | T-FBK-01, NC-21; C-02.b §b.2, §b.10 (guasti dopo la selezione) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-C02-16 | F-CAL-01.b, NFR-TEST-02; C-02.b §b.9 (PUB-LFC-01) | C-02.b | M3.b | PIANIFICATO — non eseguito |

## M3.entry: ID di lavoro di C-02.b-flat

Definiti in `docs/contracts/C-02-master.md` §f.12, con classi, livelli statistici e criteri fissati prima di ogni misura. T-M3B-FLAT-05 è una caratterizzazione senza soglia, a sostegno di un limite dichiarato. Nessuno è eseguito: le righe sono piani, non prove.

| ID | Requisito o gate | Contratto | Unità | Stato |
|---|---|---|---|---|
| T-M3B-FLAT-01 | F-CAL-01.b; C-02.b-flat §f.1, §f.6, regola I di ADR-17.b §b.10 (valori con `median_ratio`) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-FLAT-02 | F-CAL-01.b; C-02.b-flat §f.1 (fattore globale senza rumore, ADR-09.b §b.6) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-FLAT-03 | F-CAL-01.b; C-02.b-flat §f.6 (piano e valori di `mean` + `median_ratio`, controllo di coerenza) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-FLAT-04 | F-CAL-01.b; C-02.b-flat §f.1 (valore atteso dei metodi robusti normalizzati, controllo di coerenza) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-FLAT-05 | F-CAL-01.b; C-02.b-flat §f.6, ADR-17.b §b.1 (piano robusto eteroschedastico, caratterizzazione) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-FLAT-06 | F-CAL-01.b, NC-21; C-02.b-flat §f.2, §f.10 (domini e stop) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-FLAT-07 | F-CAL-01.b; C-02.b-flat §f.7 (descrittore, capability, report) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-FLAT-08 | F-CAL-01.b; C-02.b-flat §f.8 (determinismo) | C-02.b | M3.b | PIANIFICATO — non eseguito |
| T-M3B-FLAT-09 | F-CAL-01.b, NFR-TEST-02; C-02.b-flat §f.9 (PUB-LFC-01) | C-02.b | M3.b | PIANIFICATO — non eseguito |

## M3.entry: ID di lavoro di ADR-09.b e D-09.b

Definito in `docs/contracts/ADR-09-algoritmi.md` §b.7. Le soglie dei singoli operatori stanno nel campo 9 dei loro contratti, con le classi di §b.3. Nessuno è eseguito: la riga è un piano, non una prova.

| ID | Requisito o gate | Contratto | Unità | Stato |
|---|---|---|---|---|
| T-M3-ADR09-01 | D-09.b; ADR-09.b §b.3, §b.4 (classe, derivazione e precedenza di ogni soglia sulla prima misura) | ADR-09.b, D-09.b | Gate M3 | PIANIFICATO — non eseguito |

## M3 base: suite di conformità di BASE-confini

Definita in `docs/contracts/BASE-confini.md` §3; eseguita una volta sul preset `dev` (`docs/evidence/M3-base-suite.md`, log `tmp/base-suite-red.log`). Ogni caso gira per ogni voce del registro dei tipi di job (`registered_job_types`). «Rosso atteso» è un test registrato in ctest che fallisce finché l'unità di codice non paga il debito del §4: non è una regressione.

| ID | Obbligo | Test | Stato |
|---|---|---|---|
| T-M3BASE-01 | OB-01 | `Types/Conformance.T_M3BASE_01_*` | verde |
| T-M3BASE-02 | OB-02 | `Types/Conformance.T_M3BASE_02_*` | verde |
| T-M3BASE-03 | OB-03 | `ConformanceWorkflow.T_M3BASE_03_*` | verde (I4) |
| T-M3BASE-04 | OB-04 | `Types/Conformance.T_M3BASE_04_*` | verde (I1) |
| T-M3BASE-05 | OB-05 | `Types/ConformanceScheduler.T_M3BASE_05_*` | verde (I3; dall'I3 anche import, miniatura, ricostruzione) |
| T-M3BASE-06 | OB-06 | `Types/Conformance.T_M3BASE_06_*` | verde |
| T-M3BASE-07 | OB-07 | `Types/Conformance.T_M3BASE_07_*` | verde (I2) |
| T-M3BASE-08 | OB-08 | `Types/Conformance.T_M3BASE_08_*` | verde (I2) |
| T-M3BASE-09 | OB-09 | `Types/Conformance.T_M3BASE_09_*` | verde (I2) |
| T-M3BASE-10 | OB-10 | `Types/Conformance.T_M3BASE_10_*` | verde (I1) |
| T-M3BASE-11 | OB-11 | `Types/Conformance.T_M3BASE_11_*` | verde (I2 per i tipi di I/O, I3 per calibrazione e master) |
| T-M3BASE-12 | OB-12 | `Types/Conformance.T_M3BASE_12_*` e verifica (e) di ogni caso | verde |
| T-M3BASE-13 | OB-13 | `Types/Conformance.T_M3BASE_13_*` | verde |
| T-M3BASE-14 | OB-14 | `Types/Conformance.T_M3BASE_14_*` | verde |
| T-M3BASE-15 | OB-15 | `Types/ConformanceCheckpoint.T_M3BASE_15_*` | verde |
| T-M3BASE-16 | OB-16 | `ConformanceWorkflow.T_M3BASE_16_*` | verde |
| T-M3BASE-17 | OB-17 | `Types/Conformance.T_M3BASE_17_*`, `ConformanceSource.T_M3BASE_17_*` | verde |
| T-M3BASE-18 | OB-18 | `T-M3BASE-18.network` | verde |
| T-M3BASE-19 | OB-19 | `Types/ConformanceCheckpoint.T_M3BASE_19_*` | verde (debito OB-19 non osservabile al confine, vedi il record) |
| T-M3BASE-20 | OB-11, OB-12 (L1) | `ConformanceFormat.T_M3BASE_20_*` | verde (U0) |
| T-M3BASE-21 | OB-11 (L1) | `ConformanceFormat.T_M3BASE_21_*` | verde (U0) |
| T-M3BASE-22 | OB-11, OB-12 (L1) | `ConformanceFormatRole.T_M3BASE_22_*` | verde (U0) |
| Registro | §3.1 punto 3 | `T-M3BASE.job_types_registry`, `T-M3BASE.job_types_registry_negative` | verde |

## ID del piano → test (registro dell'appendice D, stadi M2.* e M2.entry)

Fonte: `ASTRO_POSTPROCESSING_PLAN_v2.2.1.md`, appendice D. Piano di lavoro: `plans/M2-residui.md` (testo di accettazione esatto di ogni ID nella sua appendice A). Stato iniziale del 29 ottobre 2026, dall'audit del registro e dalla verifica del codice. Stati durante il lavoro: `APPROVATO` (decisione con verbale e contratto passati dalla review: abilita i dipendenti); `DA RIESEGUIRE` (test eseguito prima che i prerequisiti fossero abilitati, oppure su un workflow che poi si estende); `PARZIALE`, `PIANIFICATO`; `APERTO` per le consegne. Stato finale, deciso solo al gate da un'unica esecuzione finale completa registrata in `docs/evidence/M2-gate.md`, a cui ogni riga rimanda: `CHIUSO` per ID funzionali, NFR, G e consegne; `VERIFICATO` per gli ID ADR, C e D. In entrambi i casi il test nominato deve passare nell'esecuzione finale e i prerequisiti devono essere nello stesso stato. La colonna «Unità» indica la fase di lavoro e di abilitazione (`plans/M2-residui.md`, sezione 5): le fasi `.b` sono riesecuzioni operative, prima dei dipendenti. Nessuna riga si chiude prima del gate. Stati finali assegnati il 30 ottobre 2026 dall'esecuzione finale del giro 8 di `M2-gate.md` (`tools/check_m2_trace.py`, uscita 0); vanno riconfermati dalla riesecuzione sul commit del gate. «Reg.» è la riga del registro, «Acc.» la riga di accettazione nel piano.

| ID | Stadio | Reg. | Acc. | Test eseguiti | Test pianificati | Record | Stato | Unità |
|---|---|---|---|---|---|---|---|---|
| F-IO-01 | M2.a | 3156 | 1812 | T-M2A-02, 03, 07, 13; T-M2R1-07, 09 | — | `M2-a.md`, `M2-R1.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`). Limite: RGB e multi-plane solo su fixture sintetiche (T-M2A-13): nessun FITS RGB pubblico trovato (D4) | R1 |
| F-IO-05 | M2.a | 3160 | 1816 | T-M2R1-01, 02, 03, 04, 10, 11, 12 | — | `M2-R1.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`). Limite: HDU compressi rifiutati dal lettore (`unsupported`), clausola D2 vuota in M2 | R1 |
| F-IO-10 | M2.c | 3165 | 1821, 1622 | T-M2R1-05, 06 | — | `M2-R1.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`). Perimetro: limite C-14; master del prodotto in R2 | R1 |
| F-PRJ-01 | M2.c | 3166 | 1827 | T-M2C-01 | — | `M2-c.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R0.b |
| F-PRJ-02 | M2.c | 3167 | 1828 | T-M2C-11 (`CalibJob.T_M2C_11_*`, `MasterPipeline.T_M2C_11_*`) | — | `M2-c.md`, `M2-gate.md`, `M2-R2.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R0.b; R2 |
| F-PRJ-03 | M2.b | 3168 | 1829 | T-M2B-03, 04, 05, 09 | — | `M2-b.md`, `M2-d.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R0.b |
| F-PRJ-04 | M2.c | 3169 | 1830 | T-M2C-13…18 | — | `M2-c.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R0.b |
| F-PRJ-06 | M2.c | 3175 | 1832, 2283 | T-M2C-02, 03, 10, 12; T-M2R4-01…04, anche su un workflow di passi di import di R1 più calibrazioni (T-M2R4-01, 04) | — | `M2-c.md`, `M2-R4.md`, `M2-R4b.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`). T-M2R4-01…04 sono la parte M2 del test di registro T-PRJ-01 (stadio M3.s, «incrementale da M2», arco 3715), che non ha un test ctest con quel nome | R4, R4.b |
| F-PRJ-07 | M2.c | 3176 | 1833 | T-M2B-10, T-M2C-19…21 | — | `M2-b.md`, `M2-c.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R0.b |
| F-CAL-01.a | M2.e | 3179 | 1840 | T-M2R2-01…09; T-M2R2R-02…05 (piano del job) | — | `M2-R2.md`, `M2-R2r.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`). Perimetro: `median`, `sigma_clip`, `median_ratio` fermati (F-CAL-01.b) | R2, R2.r |
| F-CAL-02.a | M2.e | 3182 | 1841 | T-M2E-01…03, 08…11, 18, 19; `MasterPipeline.T_M2C_11_*`, T-M2R2-09 | — | `M2-e.md`, `M2-R2.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R2 |
| F-CAL-03.a | M2.e | 3185 | 1842 | T-M2E-04, 06, 07; `CalibM2E.T_M2E_04_05_q_bits_separate_and_together`; `CalibM2E.T_M2E_04_var_q_overflow_*`; `MasterPipeline.T_M2C_11_*`, T-M2R2-09 | — | `M2-e.md`, `M2-R2.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R2 |
| F-CAL-06.a | M2.e | 3190 | 1845 | T-M2E-20…26; `MasterPipeline.T_M2E_20_*`; `T-M2E-21.product_masters_monte_carlo`; T-M2R2-09; `CalibM2E.T_M2E_02_nondyadic_cancellation_even_and_odd_n` | — | `M2-e.md`, `M2-R2.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R2 |
| F-RES-01 | M2.d | 3279 | 1917 | T-M2D-03; misura F-RES-01 del gate | — | `M2-d.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R0.b |
| F-RES-02 | M2.d | 3280 | 1918 | T-M2D-01, 02 | — | `M2-d.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R0.b |
| F-RES-03 | M2.d | 3281 | 1919 | T-M2D-04, 05, 06, 11, 13; T-M2R3-02 (pianificatore e job reale); T-M2R3-01 (blocchi di righe, working set ≥ 8 volte il budget, uscita uguale byte per byte), T-M2R3-06 (checkpoint e resume nel job); T-M2R2R-01…04 (modello e job del master) | — | `M2-d.md`, `M2-R3.md`, `M2-R32.md`, `M2-R2r.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R3, R2.r |
| F-RES-04 | M2.d | 3282 | 1920 | T-M2R3-03 | — | `M2-R3.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R3 |
| F-RES-05.a | M2.d | 3284 | 1921 | T-M2D-04, 10, 12; T-M2R3-03, 09 | — | `M2-d.md`, `M2-R3.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R3 |
| F-RES-06.a | M2.d | 3287 | 1922 | T-M2D-09, 10; T-M2R3-06 (checkpoint, uccisione e resume nel job), T-M2R3-07 (pressione a ogni ammissione, attesa senza timeout, stop con checkpoint) | — | `M2-d.md`, `M2-R32.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`). Perimetro: confine `.a`/`.b` di D11 nel contratto (estensione R3.2) | R3 |
| F-RES-07.a | M2.d | 3290 | 1923 | T-M2D-05; T-M2R3-04; T-M2R3-01 (minimo fattibile dei blocchi), T-M2R3-05 (ENOSPC su scratch, checkpoint, artefatti) | — | `M2-d.md`, `M2-R3.md`, `M2-R32.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`). Limite: ENOSPC sulla cache delle miniature non provato (limite in `M2-R32.md`) | R3 |
| F-RES-08.a | M2.d | 3293 | 1924 | T-M2D-10, 15; T-M2R3-09 | — | `M2-d.md`, `M2-R3.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R3 |
| NFR-EXP-01.a | M2.a | 3320 | 2001 | T-M2A-02, 09, 10, T-M2C-19 | — | `M2-a.md`, `M2-c.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R1.b |
| NFR-REP-01.a | M2.e | 3325 | 2002 | T-M2E-12, 26 (calibrazione); T-M2R1-08 (import); T-M2R4-03 (recovery), T-M2R4-06 (export); T-M2R2-06, 08 (master) | — | `M2-e.md`, `M2-R1.md`, `M2-R4.md`, `M2-R2.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R1, R4, R2; chiusura in R2.b |
| NFR-CPU-01.a | M2.e | 3338 | 2006 | T-M2E-01…04, 06…32 (`CalibM2E.T_M2E_04_05_q_bits_separate_and_together` prova il membro 05) su backend `cpu_scalar`; T-M2F-01; T-M2R5-01 (suite di calibrazione e master su `cpu_avx2`); `MasterPipeline.T_M2C_11_*`, T-M2R2-09 (master del prodotto) | — | `M2-e.md`, `M2-f.md`, `M2-R5.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R5 |
| NFR-SIMD-01 | M2.e | 3343 | 2008 | T-M2R5-01…07 | — | `M2-R5.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R5 |
| NFR-OFF-01.a | M2.e | 3345 | 2010 | T-M2F-01, T-M2F-02 | riesecuzione sul workflow finale | `M2-f.md`, `M2-e.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R2.b |
| NFR-SEC-01 | M2.a | 3352 | 2014 | T-M2A-05, 06, 11, 12; T-M2R1-10 | — | `M2-a.md`, `M2-R1.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R0.b |
| NFR-SEC-02 | M2.b | 3353 | 2015 | T-M2R0-02 (`T-M2R0-02.noexec`, con `.noexec_negative`) | — | `M2-R0.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R0 |
| NFR-SEC-03 | M2.c | 3354 | 2016 | T-M2C-03, 12, 22, 24; T-M2D-10 (symlink, traversal, sovrascrittura, confinamento); T-M2R4-05 (hardlink, anche nella cache delle thumbnails di R1) | — | `M2-c.md`, `M2-d.md`, `M2-R4.md`, `M2-R4b.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`). Limite: la cache delle thumbnails non confronta gli inode degli originali (ADR-04, integrazione con R1) | R4, R4.b |
| NFR-LIC-01.c | M2.entry | 3359 | 2018 | T-M2R0-01 (`T-M2R0-01.licenses`, con `.licenses_negative`) | — | `docs/licenses.md`, `M2-R0.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R0 |
| NFR-TEST-01.a | M2.f | 3363 | 2019 | T-M2F-01 | — | `M2-f.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R0.b |
| NFR-PERF-01 | M2.d | 3369 | 2022 | misura delle frazioni F-RES-01; T-M2R3-08 | — | `M2-gate.md`, `M2-R3.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`). Limite: misura delle frazioni F-RES-01 su un solo PC (sezione «Misura F-RES-01» di `M2-gate.md`) | R3 |
| NFR-DATA-01.a | M2.c | 3371 | 2023 | T-M2B-07, 08, T-M2C-02, 20, 21; T-M2R1-04, 11, 12 (cache) | — | `M2-b.md`, `M2-c.md`, `M2-R1.md`, `M2-R4b.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`) | R1 (estensione), R4.b |
| NFR-FBK-01.a | M2.d | 3376 | 2009 | T-FBK-01 (parte M2), T-M2D-09; `Simd.T_M2R5_07_job_with_ftz_*`, `Simd.T_M2R5_07_denormal_mask_off_*` (R5.c: stop `cpu.fp_mode` ai confini dei job e all'avvio dei task, senza segnale); `Simd.T_M2R5_07_every_exception_unmasked_*`, `Job.T_M2B_12_*`, `Store.T_M2B_12_*`, `Thumbnail.T_M2R1_04_T_M2R1_11_*` (R5.d: arresti prima del lavoro registrati con la provenance dell'arresto) | — | `M2-d.md`, `M2-e.md`, `M2-R32.md`, `M2-R5c.md`, `M2-R5d.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); i test di R5.c e R5.d entrano nella prossima esecuzione finale | R3.b, R5.c, R5.d |
| ADR-01 | M2.entry | 3385 | 2562 | spike M1.a; build C++20 in T-M2F-01 | verbale (scritto) | `ADR-M1-stack.md`, `M1-a.md`, `decisions/M2-entry.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`) | R0 (`APPROVATO`); gate |
| ADR-02 | M2.entry | 3386 | 2563 | viewer M1.b; T-M2R0-04 (`T-M2R0-04.headless`, con `.headless_negative`) | verbale (scritto) | `ADR-M1-stack.md`, `M1-closure-001.md`, `decisions/M2-entry.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`). Prova M1.b nel record `M1-closure-001.md` | R0 (`APPROVATO`); gate |
| ADR-03.a | M2.entry | 3388 | 2564 | T-M2A-01…13 | verbale (scritto) dopo NFR-LIC-01.c (arco 4080) | `contracts/ADR-03-pixel-maschere.md`, `M2-a.md`, `decisions/M2-entry.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`) | R0 (`APPROVATO`); gate |
| ADR-04.a | M2.entry | 3391 | 2565 | T-M2C-01…25 | verbale (scritto) | `ADR-M1-stack.md`, `contracts/ADR-04-persistenza.md`, `M2-c.md`, `decisions/M2-entry.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`) | R0 (`APPROVATO`); gate |
| ADR-05.a | M2.entry | 3394 | 2566 | T-M2B-01…13 | verbale (scritto) | `contracts/ADR-05-dag-provenance.md`, `M2-b.md`, `decisions/M2-entry.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`) | R0 (`APPROVATO`); gate |
| ADR-06.a | M2.entry | 3397 | 2567 | T-M2D-01…15 | verbale (scritto); estensione in R3 | `contracts/ADR-06-budget-scheduler.md`, `M2-d.md`, `decisions/M2-entry.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`) | R0 (`APPROVATO`); gate |
| ADR-07.a | M2.entry | 3400 | 2568 | M1.e; percorso CPU in T-M2E; T-M2R5-01…07 (R5, R5.c e R5.d, dev e release) | — | `ADR-M1-stack.md`, `decisions/M2-entry.md`, `contracts/ADR-07-cpu-simd.md` (aggiunte di R5, R5.c e R5.d), `M2-R5.md`, `M2-R5c.md`, `M2-R5d.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`) | R0 (`APPROVATO`); gate |
| ADR-08.a | M2.entry | 3403 | 2569 | T-M2A-01…13 (lettore FITS in ADR-03) | verbale (scritto) dopo NFR-LIC-01.c (arco 4087) | `M2-a.md`, `decisions/M2-entry.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`) | R0 (`APPROVATO`); gate |
| ADR-09.a | M2.entry | 3406 | 2570 | T-M2E-01…04, 06…32 (`CalibM2E.T_M2E_04_05_q_bits_separate_and_together` prova il membro 05; ricetta in ADR-16) | verbale (scritto) | `contracts/ADR-16-stati-master.md`, `M2-e.md`, `decisions/M2-entry.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`) | R0 (`APPROVATO`); gate |
| ADR-13.a | M2.entry | 3414 | 2574 | T-M2F-01, 02 | verbale (scritto) | `contracts/ADR-13-offline.md`, `M2-f.md`, `decisions/M2-entry.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`) | R0 (`APPROVATO`); gate |
| ADR-16.a | M2.entry | 3421 | 2577 | T-M2E-08…12, 18; `CalibM2E.T_M2E_04_var_q_overflow_*`, `Master.T_M2R2_06_node_is_canonical_and_descriptor_derives_from_it` | verbale (scritto); estensione in R2 | `contracts/ADR-16-stati-master.md`, `M2-e.md`, `decisions/M2-entry.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`) | R0 (`APPROVATO`); gate |
| ADR-17.a | M2.entry | 3424 | 2578 | T-M2E-20…26; `CalibM2E.T_M2E_02_nondyadic_cancellation_even_and_odd_n`, T-M2R2-04 | verbale (scritto); estensione in R2 | `contracts/ADR-17-varianza.md`, `M2-e.md`, `decisions/M2-entry.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`) | R0 (`APPROVATO`); gate |
| C-01.a | M2.entry | 3427 | 1355-1369 | T-M2E-01…04, 06…32 (`CalibM2E.T_M2E_04_05_q_bits_separate_and_together` prova il membro 05; forma minima in ADR-16); `MasterPipeline.T_M2C_11_*`, T-M2R2-09 (master del prodotto) | riesecuzione con F-CAL-02.a | `contracts/ADR-16-stati-master.md`, `M2-e.md`, `decisions/M2-entry.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`) | R0 (`APPROVATO`); gate |
| C-02.a | M2.entry | 3430 | 1371-1385 | T-M2R2-01…09, T-M2R2R-02…05 | — | `contracts/C-02-master.md`, `decisions/M2-entry.md`, `M2-R2.md`, `M2-R2r.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`) | R0 (`APPROVATO`); gate |
| C-10.a | M2.entry | 3440 | 1534-1548 | T-M2A-02, 09, 10, T-M2C-19…22; T-M2R0-03; T-M2R3-05 (disco pieno su scratch e checkpoint, R3.2) | — | `M2-a.md`, `M2-c.md`, `M2-R0b.md`, `decisions/M2-entry.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`) | R0 (`APPROVATO`); gate |
| D-03 | M2.entry | 3453 | 2869 | T-M2F-01 (build C++20 del core headless, come ADR-01, arco 4078) | verbale (scritto) | `ADR-M1-stack.md`, `decisions/M2-entry.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`) | R0 (`APPROVATO`); gate |
| D-04 | M2.entry | 3454 | 2870 | come ADR-02 (arco 4079); T-M2R0-04 (`T-M2R0-04.headless`, con `.headless_negative`) | verbale (scritto) | `ADR-M1-stack.md`, `decisions/M2-entry.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`). Prova M1.b nel record `M1-closure-001.md` | R0 (`APPROVATO`); gate |
| D-05 | M2.entry | 3455 | 2871 | come ADR-04.a (arco 4082); T-M2C-05, 06 | verbale (scritto) | `ADR-M1-stack.md`, `M2-c.md`, `decisions/M2-entry.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`) | R0 (`APPROVATO`); gate |
| D-09.a | M2.entry | 3460 | 2875 | T-M2E-01, `CalibM2E.T_M2E_02_nondyadic_cancellation_even_and_odd_n` (anche la cancellazione `L ~ D`), T-M2E-21, 26 (tolleranze di `M2-e.md`); T-M2R1-03 (R1); T-M2R2-01…04, 09 (C-02 §9); T-M2R5-01, 02, 06 (ADR-07) | verbale (scritto) | `M2-e.md`, `M2-R1.md`, `M2-R2.md`, `M2-R5.md`, `decisions/M2-entry.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`) | R0 (`APPROVATO`); gate |
| D-17.a | M2.entry | 3470 | 2883 | T-M2E-20…26 (`diagonal_independent`); `CalibM2E.T_M2E_02_nondyadic_cancellation_even_and_odd_n` | verbale (scritto) | `contracts/ADR-17-varianza.md`, `decisions/M2-entry.md`, `M2-gate.md` | VERIFICATO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); APPROVATO in R0 (review PASS, `.omc/artifacts/ask/codex-r0-review9-20261029-1916.md`, verbale `docs/decisions/M2-entry.md`) | R0 (`APPROVATO`); gate |
| M2 | M2 | 3489 | 2675 | aggregato | — | `M2-gate.md` | CHIUSO al gate: tutti i componenti M2 del registro passano nell'esecuzione finale (`M2-gate.md`) | gate |
| M2.a | M2.a | 3490 | 2667 | aggregato: M2.f, F-IO-01, F-IO-05 (archi 4221, 4222, 4414) | — | `M2-gate.md` | CHIUSO al gate: tutti i componenti M2 del registro passano nell'esecuzione finale (`M2-gate.md`) | abilitata in R1.b, chiusa al gate |
| M2.b | M2.b | 3491 | 2668 | aggregato: M2.a, F-PRJ-03, NFR-SEC-02 (archi 4223, 4224, 4417) | — | `M2-gate.md` | CHIUSO al gate: tutti i componenti M2 del registro passano nell'esecuzione finale (`M2-gate.md`) | abilitata in R1.b, chiusa al gate |
| M2.c | M2.c | 3492 | 2669 | aggregato: M2.b, F-PRJ-06, F-IO-10, F-PRJ-02, NFR-SEC-03 (archi 4225, 4226, 4415, 4416, 4418) | — | `M2-gate.md` | CHIUSO al gate: tutti i componenti M2 del registro passano nell'esecuzione finale (`M2-gate.md`) | abilitata in R4.b, chiusa al gate |
| M2.d | M2.d | 3493 | 2670 | aggregato: M2.c, F-RES-08.a, F-RES-04, NFR-PERF-01 (archi 4227, 4228, 4419, 4420) | — | `M2-gate.md` | CHIUSO al gate: tutti i componenti M2 del registro passano nell'esecuzione finale (`M2-gate.md`) | abilitata in R3.b, chiusa al gate |
| M2.e | M2.e | 3494 | 2671 | aggregato: M2.d, F-CAL-06.a (archi 4229, 4230) | — | `M2-gate.md` | CHIUSO al gate: tutti i componenti M2 del registro passano nell'esecuzione finale (`M2-gate.md`) | abilitata in R2.b, chiusa al gate |
| M2.f | M2.f | 3495 | 2672 | aggregato: M2.g, NFR-TEST-01.a (archi 4219, 4220) | — | `M2-f.md`, `M2-gate.md` | CHIUSO al gate: tutti i componenti M2 del registro passano nell'esecuzione finale (`M2-gate.md`) | abilitata in R0.b, chiusa al gate |
| M2.g | M2.g | 3496 | 2673 | aggregato: M1, `decided_by` ADR-03.a…ADR-17.a (archi 4210-4218) | — | `M2-g.md`, `M2-gate.md` | CHIUSO al gate: tutti i componenti M2 del registro passano nell'esecuzione finale (`M2-gate.md`) | abilitata in R0.b, chiusa al gate |
| G-STOP | M2.d | 3566 | 3571 | T-FBK-01 (parte M2); `Simd.T_M2R5_07_job_with_ftz_*`, `Simd.T_M2R5_07_denormal_mask_off_*` (R5.c); `Simd.T_M2R5_07_every_exception_unmasked_*`, `Job.T_M2B_12_*`, `Store.T_M2B_12_*`, `Thumbnail.T_M2R1_04_T_M2R1_11_*` (R5.d) | — | `M2-e.md`, `M2-R32.md`, `M2-R5c.md`, `M2-R5d.md`, `M2-gate.md` | CHIUSO al gate: test nominati passati nell'esecuzione finale dev e release (`M2-gate.md`); i test di R5.c e R5.d entrano nella prossima esecuzione finale | R3.b, R5.c, R5.d |

## M3.a: test pianificati

Contiene solo gli ID di M3.a non elencati nelle sezioni M3.entry (queste ultime restano la casa degli altri ID). Definiti nei contratti di M3.a e nel piano `plans/M3a.md`, unità per unità. Le righe `PIANIFICATO` non sono eseguite; `TEST SCRITTI, codice in corso` indica test rossi scritti prima del codice e non ancora verdi.

| ID | Requisito o gate | Contratto | Unità | Stato |
|---|---|---|---|---|
| T-M3A-RAW-13 | F-IO-04 | ADR-08.b §2.6 (emendamento L4) | U4 | PIANIFICATO |
| T-M3A-04 | F-IO-03 | F-IO-03 §12 | U8 | PIANIFICATO |
| T-M3A-05 | F-IO-03 | F-IO-03 §12 | U8 | PIANIFICATO |
| T-M3A-06 | F-IO-03 | F-IO-03 §12 | U8 | PIANIFICATO |
| T-M3A-07 | F-IO-08 | F-IO-08 §10 | U9 | PIANIFICATO |
| T-M3A-08 | F-IO-08 | F-IO-08 §10 | U9 | PIANIFICATO |
| T-M3A-09 | F-IO-08 | F-IO-08 §10 | U9 | PIANIFICATO |
| T-M3A-02 | F-IO-04 | nessun contratto: plans/M3a.md U4 | U4 | PIANIFICATO |
