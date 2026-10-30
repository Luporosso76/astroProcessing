# ExecPlan M3.a: import RAW, classificazione, offset, ricollegamento, G-PACK

Stato: **PLANNED**. Base: commit `311fb7a` (base comune I1–I4 chiusa, suite di conformità `725bdac`). Convenzione: `PLANS.md`. Piano madre: `plans/M3.md` (riga della slice M3.a, «Base comune», «Questioni aperte», «Dati pubblici»).

## Ambito

Perimetro dal registro (`python3 -S tools/trace.py ids --stage M3.a`): F-IO-02, F-IO-03, F-IO-04, F-IO-08, G-CAL, G-OFFSET, G-PACK e la consegna M3.a. Testi di accettazione: F-IO-02 riga 1813, F-IO-03 riga 1814, F-IO-04 riga 1815, F-IO-08 riga 1819 del piano attivo; G-CAL, G-OFFSET, G-PACK righe 3559-3561; uscita §21.5 riga 2683; contratto del decoder §9.6 riga 1276.

Fuori: F-IO-01, F-IO-05 e F-IO-10 (consegnati da M2, qui solo usati: `plans/M3.md` riga 18); esecuzione delle ricette `.b`, P-LOST nel job di calibrazione, T-M3A-CAL-03/04/05/10, la parte M3.c di T-M3A-CAL-09 (preflight all'esecuzione) e T-M3A-C01-04/05/08…11 (M3.c); master `.b` e T-M3A-CAL-07 (M3.b); debayer, coverage, maschere, alpha, `measure` (T-M3A-TYP-04…10, 13, 14, 17: M3.d, g, j, c, s; T-M3A-TYP-15, 16 e 18 restano in M3.a secondo `docs/test-matrix.md`: U1b e U6); estratti 2MASS e SDSS (M3.p, q).

## Dipendenze (D.6)

F-IO-02 `requires_all` F-IO-01, `decided_by` ADR-08.b e ADR-03.b (righe 3684-3686). F-IO-04 → F-IO-01 (3689). G-OFFSET → F-IO-04 (4372). G-CAL → F-IO-04, `decided_by` ADR-16.b (4370-4371). F-IO-03 → F-IO-02, G-CAL (3687-3688). F-IO-08 → F-PRJ-01, F-IO-03 (3693-3694). G-PACK → NFR-LIC-01.d (4373). M3.a → M2, F-IO-03, F-IO-08 (4235-4236, 4422).

Ordine: U0 → U1 → U2 → U3 → U4 (F-IO-02, F-IO-04) → U5 (G-OFFSET) → U6 → U7 (G-CAL) → U8 (F-IO-03) → U9 (F-IO-08) → gate. Ramo indipendente: U10 → U11 (G-PACK), in parallelo dopo U1, al massimo due unità attive.

## Metodo per ogni unità

Ciclo di `AGENTS.md` («Ciclo di un'unità»): test scritti da un agente diverso dall'autore del codice, prima del codice, rossi finché manca l'implementazione; al più ~400 righe di prodotto per review; `tools/check.sh` e `tools/mutate.sh` verdi; `tools/review.sh`, al più 2 giri. Ogni obbligo ha un solo proprietario; i nomi di file e funzione nuovi sono proposte dell'autore, il proprietario resta unico. Gli obblighi trasversali (stop con provenance, FP, cancel, budget, verifica degli input, nessuna pubblicazione su errore) sono di `docs/contracts/BASE-confini.md`: nessuna unità li riscrive, un tipo di job nuovo entra solo dal registro `src/core/job_types.cpp` e la suite T-M3BASE-01…19 lo copre da sola. Gli ID di test dei contratti (`T-M3A-RAW-`, `-PACK-`, `-LIC-`, `-TYP-`, `-CAL-`, `-C01-`) restano quelli; i test nuovi di questo piano sono `T-M3A-nn`. Tolleranze: tutte zero o binarie (interi, tabelle, codici); nessuno stimatore numerico in M3.a (ADR-09.b riga 33 di `ADR-09-algoritmi.md`).

Fixture: `FIX-SYN-DNG-01`, DNG non compressi a 16 bit scritti con libtiff (`TIFFClientOpen` in memoria) dall'autore dei test, con pattern, `ActiveArea`, `BlackLevel`, `BlackLevelRepeatDim`, `WhiteLevel`, `LinearizationTable` noti: verità indipendente dal parser di LibRaw (ADR-08.b §6). Versione della specifica DNG di Adobe registrata nel record. Dati reali: `PUB-RAW-01` (8 file CC0 con SHA-256, `/opt/workspace/astroProcessor/data/public/raw-pixls-us/`, sola lettura), solo coerenza, nessuna verità sui campioni.

## Unità

### Modelli per unità (AGENTS.md, «Modelli per competenza»; si legge prima di ogni delega)

| Unità | Test (agente diverso dal codice) | Codice | Review |
|---|---|---|---|
| U0 | Sonnet | Opus (confine della base) | Codex `gpt-6-astra` |
| U1 | Sonnet | Sonnet (eseguito con Opus: deviazione dichiarata) | Codex |
| U1b | Sonnet (6 casi aggiunti dall'autore del codice per mutazioni sopravvissute, deviazione dichiarata) | Sonnet | Codex |
| U2, U2b, U3 | Sonnet (U2: 3 casi aggiunti dall'autore del codice su rilievi della review, deviazione dichiarata) | Opus (numerica del decoder, fuzz) | Codex |
| U4, U5 | Sonnet | Sonnet | Codex |
| U6, U7 | Opus (predicati e compatibilità) | Opus | Codex |
| U8a, U8b, U9 | Sonnet | Sonnet | Codex |
| U10 | Sonnet (5 casi aggiunti dall'autore del codice: deviazione dichiarata) | Sonnet (eseguito con Opus: deviazione dichiarata) | Codex |
| U11 | Opus (oracolo HEALPix) | Sonnet | Codex |
| U12 (librerie migliori) | Sonnet (Opus per rifare le sonde dei fatti di ADR-08 su LibRaw 0.22) | Sonnet | Codex |

Ricognizione, allineamento di piani e record, controlli di stato: Haiku. Testi e lingue: Gemini via `agy`. Il coordinatore copia il modello da questa tabella nella chiamata di delega. Una mutazione sopravvissuta o un caso mancante trovato dall'autore del codice non diventa un test suo: lo riporta, e i casi li scrive l'agente dei test (regola ripetuta nei prompt da U3 in poi).

### U0. Lettura degli input RAW nel confine (base, non slice) — ~120 righe

Lacuna L1. Correzione della base secondo `plans/M3.md` «Base comune» punto 3, con emendamento di BASE §2.5 e review prima del codice. ID: prerequisito di F-IO-02.
1. Apertura in sola lettura con limite e codice del formato (`raw.max_file_bytes`, ADR-08.b §2.1) prima di ogni allocazione — proprietario: lettura degli input (`open_fits_input` generalizzata con la dichiarazione del formato, BASE §2.5 emendamento L1; una seconda funzione che ricopia il protocollo è vietata), OB-12.
2. Lettura intera verificata senza `fits_plan` (piano = dimensione da `fstat`), digest prima di `open_buffer`, riverifica prima della pubblicazione — proprietario: `read_verified_input` / `reverify_input` (`hash.cpp`), OB-11.
Test: T-M3BASE-20, T-M3BASE-21, T-M3BASE-22 (BASE §3.2, emendamento L1). Le varianti RAW di T-M3BASE-11/12 passano a U4, che registra il primo tipo di job con un ruolo RAW. Uscita: suite di conformità verde, nessun digest confrontato fuori da `hash.cpp`.

### U1. Tipi del decoder (ADR-03.b §b.1, §b.2, §b.9 righe 1-2) — ~250 righe

ID: F-IO-02 (`decided_by` ADR-03.b), F-IO-04.
1. Origine `decoder` in `metadata_origin`, conflitto `declared`/`decoder` → `metadata.declared_conflict` (§b.1) — `image.hpp`/`image.cpp`.
2. `decoder_state` e i suoi invarianti in `validate_image` → `image.decoder_state` (§b.2) — `image.cpp`.
3. Scrittura FITS di un'immagine con `decoder_state` → `fits.decoder_state`, nessun buffer (§b.2) — scrittore FITS (`fits.cpp`).
4. `capture_time_libraw` e keyword `ASTRTSTP` (§b.1) — `fits_header.cpp`.
Test: T-M3A-TYP-01, 02, 03, 12 (solo le coppie `decoder_state`). Uscita: test verdi, mutazioni sugli invarianti uccise.

### U1b. Capacità di misura `measure` (ADR-03.b §b.13) — ~200 righe

ID: tipi di §9.8 usati da G-CAL (stadio M3.a in `docs/test-matrix.md`). Proprietari: `image.hpp`/`image.cpp` (`measure`, `history_limits`, `measure_limits`, `merge_measure`), scrittore e lettore FITS (`ASTRMCAP`, `ASTRMLnn`).
Test: T-M3A-TYP-15; T-M3A-TYP-16 per `history_limits`, `measure_limits` senza i tipi di M3.d, `merge_measure`, `assess_measure`, `require_measure_ready`. Uscita: test verdi, mutazioni rilevate.
Spostati dall'arbitraggio del 30 ottobre: i casi di T-M3A-TYP-16 con `variance_scope` e `demosaic` vanno a M3.d (elenco in fondo a questo piano, da copiare nel piano di M3.d); `measure` obbligatoria in `operator_output_invariants_hold` è un obbligo trasversale su tutti gli operatori e va nella base comune come intervento I6 con un proprietario unico (OB-20 da scrivere in `docs/contracts/BASE-confini.md`; casi attesi in fondo a questo piano), prima che un operatore nuovo pubblichi immagini.

### U2. Decoder puro: geometria, pattern, livelli (ADR-08.b §2.2–2.4, §2.6) — ~380 righe

ID: F-IO-02. Proprietario unico: `decode_raw_buffer` (nuovo `src/core/raw_decode.cpp`).
1. Parametri utente iniziali verificati, nessuna funzione che modifica i campioni (§2.2, H1).
2. Area visibile e crop `{x0, y0, w, h}` dichiarati come geometria, overscan non importato, `raw_inset_crops` e `flip` solo registrati (§2.3).
3. Pattern da `idata.filters` con `FC` di LibRaw, parità dei margini, `cfa_color_index` (§2.3, H2).
4. Stop della tabella di §2.3 (`raw.xtrans`, `raw.not_cfa`, `raw.float_data` con `CONVERTFLOAT_TO_INT` tolto prima di `open_buffer`, `raw.cfa_pattern`, `raw.multi_image`, `raw.geometry`) e `raw.black_pattern_anchor` di §2.4 (emendamento dopo i test di U2, `docs/evidence/M3a-ADR08-U2.md`).
5. Nero e bianco dichiarati e non sottratti, linearizzazione e digest della curva (§2.4, H3, H4).
6. Campioni f32 = `uint16`, layout `cfa`, fase `raw`, metadati `std::optional` con origine `decoder`, nessun valore di comodo (§2.6).
Test: T-M3A-RAW-01, 02, 03, 04 (esatti: 0). Dati: `FIX-SYN-DNG-01`. Uscita: H1–H4 provate o, se smentite, clausola bloccata e contratto riaperto. Stato al 30 ottobre: H3 smentita per motivi di nero non uniformi con margine non nullo → stop `raw.black_pattern_anchor`; H4 provata; finestra con origine dispari secondo la regola di §2.3.

### U2b. Ora di acquisizione e avviso Fujifilm (ADR-08.b §2.5, §2.6, emendamento del 30 ottobre, `docs/evidence/M3a-ADR08-U4.md`) — ~80 righe

ID: F-IO-02, F-IO-04. Proprietario: `decode_raw_buffer` (verifica di `TZ=UTC0` e di `mktime` prima di `open_buffer`, `capture_time_libraw` come `timegm` dei campi); un `main` di test condiviso che imposta `TZ=UTC0` prima dei thread per ogni eseguibile che chiama il decoder.
Test: T-M3A-RAW-01 (ora esatta 1793363696, ora inesistente 1774751400, ora doppia 1792895400, death test con `TZ` diversa); T-M3A-RAW-05 (`PARSEFUJI_PROCESSED` ammesso e registrato).

### U3. Decoder robusto: limiti, errori, thread, memoria (ADR-08.b §2.1, §2.5, §2.7, §2.8) — ~300 righe

ID: F-IO-02. Proprietario: `raw_decode.cpp` (stesso file, funzioni distinte).
1. `max_pixels` dopo `open_buffer`, prima di `unpack` (§2.1).
2. Gestore degli errori di dati: stop `raw.data_error` anche con `unpack` riuscito; progress handler sul token di cancel (§2.5).
3. Avvisi ammessi e non ammessi; tabella dei codici di ritorno con funzione, valore e `libraw_strerror` nel contesto (§2.5).
4. `omp_set_num_threads(1)` nel worker prima di `open_buffer` (§2.7, H6).
5. Prenotazione nel pool prima di `unpack`, `max_raw_memory_mb` (§2.8).
Test: T-M3A-RAW-05, 06, 08, 10 (fuzz: zero crash e zero abort, H5). Uscita: fuzz pulito sotto ASan/UBSan entro il 70% delle CPU.

### U4. Job di import RAW, record del decoder, export (ADR-08.b §2.9, §5) — ~300 righe

ID: F-IO-02, F-IO-04.
1. Tipo di job `io.raw_import` registrato in `job_types.cpp` con costruttore minimo (BASE §3.1) — `raw_import_job.cpp`.
2. Record del decoder di §2.9 in provenance e nella chiave del nodo, identità della libreria (`LibRaw::version()`, SHA-256 di `libraw_r.so` caricata) — `raw_decode.cpp`.
3. Export della provenance con hash dell'originale e offset applicato dal decoder (`none`) (F-IO-04; C-01.b §10) — `provenance.cpp` (solo campi; scrittura nel runner).
4. Simboli vietati assenti (§5) — `tools/check_offline.sh`.
Decisi il 30 ottobre (ADR-08.b §2.5, §2.6, §2.9): `PARSEFUJI_PROCESSED` ammesso e registrato (X-A1 si decodifica); ora di acquisizione col processo a `TZ=UTC0`, nel record e nella chiave. Test: T-M3BASE-11 e T-M3BASE-12 sulla variante RAW (da U0); T-M3A-RAW-07, 09, 11, 12, 13 (chiavi `null` di ADR-08.b §2.6, emendamento L4); T-M3A-02 (export F-IO-04: hash della sorgente, `black_applied_by_decoder = none`, campi assenti restano assenti); T-M2F-02 e T-M2R0-01 rieseguiti con i binari che collegano LibRaw e libtiff. Dati: `PUB-RAW-01` (7 Bayer, X-E1 → `raw.xtrans`). Uscita: suite di conformità verde sul nuovo tipo.

### U5. G-OFFSET: stato dell'offset e voce `light_offset` (C-01.b §1, §10; ADR-16.b §b.5 colonna offset, §b.10) — ~200 righe

ID: G-OFFSET. Proprietario: nuovo `src/core/calibration_analysis.cpp`.
1. Stato `removed` / `known_not_removed` / `unknown` per catena del light; `OFFSET` FITS resta `unknown`; piedistallo → `calib.pedestal` (C-01.b §1, §2).
2. Voce `light_offset`: nero per posizione del periodo `lcm(2, rows) × lcm(2, cols)` con H3, `black_applied_by_decoder`, identità del decoder; mai sottratto (C-01.b §1, §10).
Test: T-M3A-C01-01, 03. Uscita: G-OFFSET chiuso con U6 (T-M3A-CAL-08, C01-06, 07 parte di analisi) e U7 (C01-02), più T-M3A-RAW-02.

### U6. G-CAL, parte 1: stati accettati e compatibilità (ADR-16.b §b.2, §b.3, §b.7) — ~380 righe

ID: G-CAL. Proprietario: `calibration_analysis.cpp`, funzione di compatibilità per coppia.
1. Stati `usable`/`excluded` per ruolo, unità, fase, linearità, layout, piedistallo, coverage (§b.2; C-01.b §2, §7).
2. Compatibilità per coppia e per catena, lista chiusa dei motivi (§b.3).
3. Compatibilità del decoder: campi strutturali, nero effettivo in interi a 64 bit contro `decoder_black_abs_tol_adu` obbligatorio senza default, `decoder_state_mixed` (§b.7).
Test: T-M3A-CAL-08; T-M3A-TYP-18 (tabella delle operazioni di ADR-03.b §b.14); T-M3A-C01-06; T-M3A-C01-07 (parte di analisi). Uscita: zero calcoli negli stop (contatore del `fault_injector`).

### U7. G-CAL, parte 2: predicati, ammissibili, default, selezione (ADR-16.b §b.1 punti 1-2, §b.4–b.6, §b.9) — ~400 righe

ID: G-CAL, G-OFFSET (P-OFFSET). Proprietario: `analyze_calibration` (`calibration_analysis.cpp`), funzione pura degli ingressi.
1. Predicati P-DFB, P-DF, stato del flat disponibile (§b.4).
2. Insieme ammissibile, nessuna doppia sottrazione, P-OFFSET con `flat_division_offset` (§b.5).
3. Controllo termico di `flat.bias` in aritmetica esatta sui valori f64 (§b.5).
4. Default D-16, mai il percorso parziale (§b.6).
5. Ricetta fuori dall'insieme → G-STOP senza nodo; parametri non applicabili (§b.9, §b.5).
Test: T-M3A-CAL-01, 02, 06; T-M3A-C01-02. Oracle: tabelle di §b.5 e §b.6 ricodificate a mano dall'autore dei test.

### U8. F-IO-03: classificazione batch — stima ~550 righe, divisa in U8a (proposta, persistenza, import batch, `add_input` solo con percorsi assoluti) e U8b (analisi, gruppi, candidati, ricette) secondo `docs/contracts/F-IO-03-classificazione.md`

ID: F-IO-03. Bloccata finché il contratto non copre L2 e L5.
1. Classificazione modificabile di light, dark, flat, dark-flat, bias; ruolo mai dedotto da esposizione, `IMAGETYP` o nome (ADR-16.a, «Stati tipizzati»); `IMAGETYP` in conflitto → `imagetyp_conflict`.
2. Controlli di dimensioni, CFA, esposizione, gain, binning, filtro, temperatura, offset prima della scelta: delegati a U6/U7, nessuna copia.
3. Proposta di sole ricette ammissibili e omissioni motivate (uscita di U7); input prestacked e RGB: nessuna ricetta di calibrazione, motivo `op.phase_blocked` (ADR-03.b §b.14).
4. Dopo la scelta, mismatch → stop del ramo senza sostituzione, progetto riapribile (L5).
Test: T-M3A-CAL-09 parte M3.a (luce: `verify_selection` e stop del ramo, U8b; F-IO-03 tabella di L5); T-M3A-04 (batch con un campo incompatibile alla volta: motivo e proposta uguali all'oracle), T-M3A-05 (prestacked e RGB nel batch; ruolo cambiato dall'utente → nuova analisi), T-M3A-06 (file cambiato e stato del decoder cambiato dopo la scelta: stop del solo ramo con `run_branches`, altri rami completati, riapertura del progetto con stato logico identico).

### U9. F-IO-08: ricollegamento — stima ~300 righe, dopo la Lacuna L3

ID: F-IO-08. Proprietario: store del progetto (`sqlite_project.cpp`), unica funzione di ricollegamento.
1. File mancante o spostato rilevato (`io` `input.missing`, identità `dev`/`ino` di ADR-04 R4) senza corrompere il progetto.
2. Ricollegamento accettato solo con dimensione e SHA-256 registrati.
3. Nodi dipendenti bloccati con motivo visibile; salvataggio e riapertura conservano i riferimenti risolti.
Test: T-M3A-07 (spostamento e cancellazione: rilevati, progetto riapribile, nessun artefatto toccato), T-M3A-08 (ricollegamento a file uguale: accettato; dimensione o digest diversi: `integrity`, record invariato), T-M3A-09 (dipendenti bloccati con motivo; dopo il ricollegamento salvataggio, riapertura, undo e redo coerenti). Verità: digest scritto dal generatore della fixture.

### U10. G-PACK, parte 1: manifest, file, licenze (ADR-08.b §3.1, §3.2, §3.3 punti 1-4 e 6, §3.4, §4) — ~380 righe

ID: G-PACK. Proprietario: `verify_pack` (nuovo `src/core/pack.cpp`), chiamata nel preflight.
1. Manifest canonico, schema, campi obbligatori, colonne e bit (§3.1, §3.3.1).
2. Digest, `pack_id`, `pack_version` uguali alla ricetta; `pack_digest` in chiave e provenance (§3.2, §3.3.2).
3. File elencati, dimensione e hash completi a ogni preflight, nessun file non elencato (§3.3.3).
4. Licenze ammesse, `license_file`, `license_evidence` con record esistente e licenza uguale (§3.3.4, §4).
5. Scopo e bande (§3.3.6); stop del ramo, nessun altro pack, nessun download (§3.4).
Test: T-M3A-PACK-01, 02 (vedi L6), 03, 04, 05, 07, 09; T-M3A-LIC-01. Pack sintetico generato dal test (seed fisso, `CC0-1.0`).

### U11. G-PACK, parte 2: conteggi, indice, righe, copertura HEALPix (§3.1 file dati e indice, §3.3 punti 5 e 7) — ~350 righe

ID: G-PACK. Proprietario: `pack.cpp` (conteggi, indice, righe), `healpix.cpp` (celle NESTED, Górski et al. 2005).
1. Conteggi, indice contiguo e ordinato, righe coerenti con `null_bit` e `NaN`.
2. Copertura conservativa: regione allargata del raggio massimo di una cella; mai un falso accetto.
Test: T-M3A-PACK-06, 08; T-M3A-PACK-09 sulla parte dell'indice (in U10 il fuzz dell'indice prova solo i file elencati); casi aperti di ADR-08.b §3.6 (directory relativa, `evidence_root` vuoto, `flag_bits: []`). Oracle di PACK-06, fissato qui prima della misura: l'autore dei test scrive un proprio `ang2pix` NESTED dalle formule di Górski et al. 2005, lo controlla sui centri analitici delle 12 celle di base e sulla regola di annidamento dei 4 figli, e campiona fittamente ogni regione (calotta) a ordine 1–2; regione esterna o a cavallo = almeno un campione in una cella fuori `coverage`; regione interna = calotta allargata di due raggi massimi tutta in `coverage`. Criterio binario.

## Gate M3.a

1. Tutti i test elencati verdi su `dev` (ASan/UBSan) e `release`; `python3 -S tools/trace.py check` pulito; suite di conformità verde su `io.raw_import`.
2. Uscita di `plans/M3.md`: pattern, nero, bianco, crop e stato dichiarati; offset noto non sottratto due volte (T-M3A-RAW-02, C01-01/03, CAL-08); pattern ignoto o X-Trans → stop; mismatch dopo la scelta → stop del ramo, progetto riapribile; fuzz pulito; G-PACK su pack sintetico.
3. T-FBK-01 parte M3.a (decoder che fallisce, pack mancante: nessun percorso alternativo), T-M2F-02 e T-M2R0-01 rieseguiti.
4. Dato pubblico `PUB-RAW-01` usato (T-M3A-RAW-09); senza, il gate resta aperto (`plans/M3.md`, «Dati pubblici e blocchi»).
5. Record `docs/evidence/M3-a.md` con review indipendente PASS per ogni unità.

## Lacune dei contratti

- **L1** Il confine di lettura degli input è tipizzato FITS: `open_fits_input` applica `fits_limits::max_file_bytes` con codice `fits.max_file_bytes`; `read_verified_input` richiede un `fits_plan`. ADR-08.b §2.1 chiede limite e codice RAW e lettura dello stesso buffer. Si corregge nella base (U0) con emendamento di BASE §2.5, non nella slice. Stato: emendato in `docs/contracts/BASE-confini.md` §2.5 («Emendamento L1»: OB-11, OB-12), §3.1 punto 1, §3.2 (T-M3BASE-20…22), §4; record `docs/evidence/M3a-emendamenti.md`.
- **L2** F-IO-03 non ha contratto: fonte della classificazione proposta per i RAW (senza `IMAGETYP`), persistenza della classificazione modificabile, raggruppamento dei light, scelta fra più candidati dello stesso ruolo (rinviata a «F-IO-03, ADR-15» da ADR-16.b e C-02.b, ma non scritta), controlli di binning e filtro (presenti nei gruppi di C-02.b, non nelle coppie di ADR-16.b §b.3). Chiusa in `docs/contracts/F-IO-03-classificazione.md` (review Codex 2 giri, arbitraggio del coordinatore).
- **L3** F-IO-08 non ha contratto: ADR-04 registra `path`, `dev`, `ino` degli input ma nessuna regola di aggiornamento del percorso, di interazione con snapshot, undo e recovery, né lo stato «bloccato» dei nodi dipendenti con motivo. Serve un'estensione di ADR-04 prima di U9. Stato: contratto in `docs/contracts/F-IO-08-ricollegamento.md` (`RL-01`…`RL-08`, estensione di ADR-04; test T-M3A-07…09 a casi), rinvio in ADR-04; record `docs/evidence/M3a-F-IO-08-contratto.md`.
- **L4** F-IO-04 chiede scala pixel, sensore e WCS «quando presente»; ADR-08.b §2.6 non li nomina per i RAW. Va dichiarato nel contratto che restano assenti (LibRaw non li fornisce), non dedotti. Stato: emendato in `docs/contracts/ADR-08-decoder-pack.md` §2.6 (chiavi `null` con `absent_reason`), §2.9, §7; test T-M3A-RAW-13; record `docs/evidence/M3a-emendamenti.md`.
- **L5** «Mismatch dopo la scelta» è nell'accettazione di F-IO-03 (M3.a), ma ADR-16.b assegna P-LOST e T-M3A-CAL-05/09 a M3.c. Va scritto quale parte prova M3.a (proposta: rianalisi pura sul nodo selezionato e stop del ramo sul cambio di input) e quale M3.c. Chiusa in `docs/contracts/F-IO-03-classificazione.md` (ripartizione del mismatch dopo la scelta fra base, M3.a, M3.b e M3.c).
- **L6** G-PACK non ha un consumatore di prodotto prima di M3.p: la parte «stop del solo ramo dipendente» di T-M3A-PACK-02 in M3.a gira con un job del test composto da `run_branches`; la conferma su un operatore del prodotto spetta a M3.p. Un tipo di job solo di test non entra nel registro del prodotto (BASE §3.1 punto 3). Stato: emendato in `docs/contracts/ADR-08-decoder-pack.md` §3.4 («Prova in M3.a») e §6 (T-M3A-PACK-02); record `docs/evidence/M3a-emendamenti.md`.

## Rischi

- Ipotesi H1–H6 di ADR-08.b non provate: una smentita blocca la clausola e riapre il contratto; un crash di LibRaw nel fuzz (H5) blocca il gate e apre un'unità nuova (decodifica in processo separato), mai un ripiego.
- DNG sintetici: LibRaw può rifiutare un DNG minimo o applicare tabelle per modello; build senza zlib, quindi solo DNG non compressi.
- `PUB-RAW-01`: CFA per modello è ipotesi (X-A1 Bayer, X-E1 X-Trans); un avviso fuori lista su un formato reale dà `raw.warning`, che si registra e torna in review del contratto, non si ammette in silenzio.
- U7 può superare 400 righe (controllo termico e default): si divide prima della review, non dopo.
- L2, L3 e L1 sono sul cammino critico: U8, U9 e U0 non partono senza contratto rivisto.
- Fuzz (5 000 mutazioni per seme, semi da ~20 MB, ASan) lungo al 70% delle CPU.

## Prossimo comando

`python3 -S tools/trace.py ids --stage M3.a`; poi, in parallelo: emendamento di BASE §2.5 per L1 (U0) e test rossi di U1 (T-M3A-TYP-01, 02, 03) scritti da un agente diverso dall'autore del codice. Contratti per L2, L3, L4, L5 prima di U8 e U9.

## Casi spostati da U1b (arbitraggio del 30 ottobre)

### A M3.d (`variance_scope`, `demosaic`)

### Parti di T-M3A-TYP-16 passate a M3.d (richiedono `image::variance_scope` e `image::demosaic`, tipi di §b.3/§b.4)

Immagine di base completa del contratto: `lin = linear`, `variance` presente, `variance_scope = independent`, `measure` vuota (limitazioni attese: nessuna). In U1b la base è senza varianza (limitazioni attese: `variance.absent`).

measure_limits (attesi scritti dal contratto):
1. Base completa: `measure_limits` = {}.
2. `variance` presente e `variance_scope` assente da sola: {`variance.scope_unknown`}.
3. `variance_scope = marginal` da solo: {`variance.marginal`}.
4. `variance` assente da sola sulla base completa (con `variance_scope` reimpostato): {`variance.absent`} (in U1b coperto dalla base senza varianza).
5. `lin = nonlinear` da solo sulla base completa: {`linearity.nonlinear`}; `lin` assente da solo: {`linearity.unknown`} (in U1b coperti, con in più `variance.absent`).
6. Combinazione {`nonlinear`, `variance` senza `variance_scope`}: {`linearity.nonlinear`, `variance.scope_unknown`}.
7. Combinazione {`nonlinear`, `variance_scope = marginal`, `measure` con `samples.interpolated`}: {`linearity.nonlinear`, `variance.marginal`, `samples.interpolated`} nell'ordine canonico.
8. Unione memorizzati e derivati: `measure` {`history.unknown`, `calib.assumption.h_dprime`, `variance.fixed_weight_approximation`} + `nonlinear` + `marginal`: {history.unknown, h_dprime, linearity.nonlinear, variance.marginal, variance.fixed_weight_approximation}.
La combinazione {`lin` assente, `variance` assente} → {`linearity.unknown`, `variance.absent`} resta in U1b.

merge_measure: nel caso «mai un codice perso, nessun derivato aggiunto» l'ingresso aveva anche `variance_scope = marginal` (resta in U1b senza quella riga: stessa asserzione).

assess_measure / require_measure_ready: l'immagine di prova aveva `variance_scope = marginal` (limitazioni {h_dprime, nonlinear, variance.marginal, samples.interpolated}); in U1b usa `variance.absent` al posto di `variance.marginal`. Variante con `marginal` e il caso `require_measure_ready` su immagine di base senza limitazioni (base completa, accettazione vuota: nessuno stop) passano a M3.d.

### Alla base comune, intervento I6 (`measure` obbligatoria)

### Casi di `operator_output_invariants_hold` tolti da U1b (obbligo trasversale: `measure` presente in ogni uscita di operatore)

Intervento a sé nella base comune, proprietario unico. Test già scritti (tmp/test_measure.before, sezione «operator_output_invariants_hold»). Asserzioni attese, uscite altrimenti valide (`validate_image` passa):
1. Uscita mono con `measure` vuota: `operator_output_invariants_hold` = true.
2. Stessa uscita con `measure` assente: false.
3. Uscita RGB con `demosaic` (variance_scope marginal, varianza presente) con `measure` priva di `samples.interpolated` (vuota, oppure solo `history.unknown`): false.
4. `demosaic` con `variance_propagation = fixed_weight_approximation`, `measure` = {`samples.interpolated`} senza `variance.fixed_weight_approximation`: false.
5. Stessa uscita con `measure` = {`variance.fixed_weight_approximation`, `samples.interpolated`}: true.
6. `demosaic` con `variance_propagation = linear_weights`, `measure` = {`samples.interpolated`}: true.
(Casi 3-6 dipendono anche dai tipi `demosaic` e `variance_scope` di M3.d.)
Conseguenza: i test esistenti che si aspettano true per immagini senza `measure` (`test_image.cpp:108,118`, `test_fits.cpp:823`, test_calibration*, test_master, test_import*, m2e_workflow) vanno aggiornati nello stesso intervento.

## Correzioni dall'audit dei componenti (30 ottobre 2026, regola «Componenti» di AGENTS.md)

### U12, in M3.a dopo U2b+U3 e U11, prima di U4: librerie migliori compilate nel progetto
- LibRaw 0.22.x corrente con zlib (DNG deflate), sorgente in `third_party/libraw/` a versione fissa con SHA-256, LICENSE e PROVENANCE. Adobe DNG SDK (DNG JPEG-XL) solo dopo la lettura della sua licenza alla fonte primaria e la verifica di compatibilità con GPL-3.0-or-later.
- CFITSIO corrente compilata senza curl: nessuna libreria di rete nel processo (ADR-13).
- WCSLIB corrente nel prodotto (M3.g, M3.p), non solo come oracolo dei test.
- Le sonde dei fatti LibRaw di ADR-08 (H3, origine dispari, float, curva, `cblack`, memoria M, thread, TZ, Fuji) si rifanno sulla nuova versione prima del codice; ogni differenza è un emendamento del contratto.
- `docs/licenses.md`: licenza di ogni dipendenza diretta letta dal LICENSE upstream, non dal `copyright` Debian.

### Emendamenti alle slice successive (prima del loro codice)
- M3.d: debayer edge-aware (RCD) come default più CFA/Bayer drizzle per lo stacking; il bilineare a pesi fissi resta per la varianza esatta (ADR-09).
- M3.q: colore del prodotto da Gaia DR3 anche nel footprint SDSS; SDSS solo come riferimento indipendente nei test.
- PUB-DEEP-01: stack Seestar di Zenodo 17843622 (CC BY 4.0) al posto del cutout HST (licenza non dichiarata alla fonte).
- M3.p: licenza di Hipparcos e Tycho-2 letta alla fonte prima di escluderli come complemento di Gaia per le stelle brillanti.
- Chiuso: RawSpeed assente da LibRaw di sistema (`libraw_capabilities()` = 0x80, nessun dato `cameras.xml` nel binario).
