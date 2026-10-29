# Matrice dei test

Stato: **seed pianificato. Nessun test qui è stato eseguito.** Una riga `PIANIFICATO` non è una prova e non chiude un gate. Solo un record in `docs/evidence/` con comando, hash, versioni, ambiente, esito e limiti può sostenere un esito.

## Regole

- **Tolleranze da derivare.** Nessun valore numerico è inventato in questa matrice. Ogni tolleranza si ricava dal contratto scientifico, dal ground truth o dal protocollo preregistrato prima dell'esecuzione, e si annota nel record.
- **Dataset.** Prima il sintetico autogenerato, mai foto private. Un dataset pubblico si usa solo dopo verifica di licenza, provenienza e hash. Gli ID qui sotto sono etichette di lavoro, non artefatti già disponibili.
- **Metodi.** Gli ID `T-*` e `F-*` sono quelli del piano 2.2.1. Nessun ID nuovo viene presentato come approvato.
- **Nessuna sostituzione.** T-FBK-01 non verifica solo il messaggio: inietta il guasto e accerta che il percorso alternativo non venga eseguito.
- **Costi.** Solo se esposti dal runtime.

## Seed

| Test | Requisito o gate | Dataset / fixture | Metodo | Metrica preregistrata | Tolleranza | Evidenza prevista | Stato |
|---|---|---|---|---|---|---|---|
| T-FBK-01 | NFR-FBK-01, NC-21; M2 core, M3, M4, M5 | `FIX-FAULT-01` guasti sintetici: backend assente, decoder che fallisce, GPU che si stacca | Iniezione del guasto; traccia delle chiamate ai backend; exit code; provenance dell'arresto | Zero invocazioni del percorso sostitutivo; nessun output pubblicato; errore non nullo | Binaria: sostituzione assente o presente | `docs/evidence/` | PIANIFICATO — non eseguito |
| M2 slice calibrazione | Gate M2; F-CAL-01/02 | `FIX-SYN-FITS-01` FITS sintetico con dark e flat; ground truth | Calibrazione `L - D`, flat `Q`, maschere, varianza, report | Errore numerico, varianza propagata, doppia sottrazione assente | Da derivare dal contratto e dal ground truth | `docs/evidence/` | PIANIFICATO — non eseguito |
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
| T-M2B-01…08, 10, 13 | ADR-05 | M2.b | ESEGUITO (T-M2B-10: record e provenance del runner; ledger ADR-16 e varianza ADR-17 con M2.e, scrittura atomica con M2.c) |
| T-M2B-09 | ADR-05 | M2.b | PARZIALE: errore dell'input, confine del job e `descendants`; il blocco dei soli dipendenti è dello scheduler (M2.d) |
| T-M2B-11 | ADR-05 | M2.b, M2.c | ESEGUITO, PASS: chiave e forma canonica invariate dopo una migrazione dello schema SQLite (`Store.T_M2B_11`, `docs/evidence/M2-c.md`) |
| T-M2B-12 | ADR-05 | M2.b | PARZIALE: exit 2/3 e job marcato eseguiti; il rilascio delle prenotazioni è di M2.d |
| `io input.short_read` (ADR-05) | ADR-05 | M2.a | NON ESEGUITO: richiede un lettore iniettabile (pread che restituisce meno byte con dimensione invariata); il ramo esiste in `fits.cpp` (`unexpected_eof`) ma non ha test |
| T-M2C-01…25 | ADR-04 | M2.c | ESEGUITO, PASS (`docs/evidence/M2-c.md`); durabilità contro perdita di corrente e processo malevolo stesso UID fuori dal modello; `readdir` senza test di iniezione |
| T-M2D-01…15 | ADR-06 | M2.d | PIANIFICATO |
| T-M2E-01…27 | ADR-16, ADR-17 | M2.e | PIANIFICATO |
| T-M2F-01 | ADR-13 | M2.f | ESEGUITO, PASS (`docs/evidence/M2-f.md`) |
| T-M2F-02 | ADR-13 | M2.e | PIANIFICATO |
