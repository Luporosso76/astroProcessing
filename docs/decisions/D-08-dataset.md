# Verbale D-08: dataset pubblici, licenze e hash; uso nei test (NFR-TEST-02)

Stato: **APPROVATO (review `d08-round2` PASS, `.omc/artifacts/ask/codex-d08-round2-20261030-0448.md`)**, 30 ottobre 2026. Approvazione autonoma secondo `AGENTS.md` (decisioni tecniche delegate): `APPROVATO` quando la review indipendente passa; `VERIFICATO` al gate M3, con i test `T-M3A-DAT-*` eseguiti. Record: `docs/evidence/M3-entry-D08.md`.

Piano attivo: `ASTRO_POSTPROCESSING_PLAN_v2.2.1.md`, SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`. Registro: D-08 riga 3458, testo riga 2874 («Selezionare prima di M3, con licenza, provenienza e hash»; «Coordinatore e QA dopo verifica licenze»); NFR-TEST-02 riga 3367, testo riga 2020; arco NFR-TEST-02 `decided_by` D-08 (riga 4028); F-ANA-05.a `requires_all` NFR-TEST-02 (riga 3917); NC-16 riga 379; NFR-LIC-01.d riga 3360, testo di NFR-LIC-01 riga 2018. Candidati e scelte del coordinatore: `docs/datasets/M3-candidates.md`. Blocchi per slice: `plans/M3.md`, «Dati pubblici e blocchi».

## 1. Decisione

| Campo | Contenuto |
|---|---|
| Domanda | Quali dataset pubblici usa M3, con quale licenza, fonte, rilascio e hash; come li usano i test |
| Opzioni del piano | Dataset pubblici, sintetici, misti (riga 2874) |
| Decisione | Misto. Il sintetico è la verità di riferimento di ogni slice; il dato pubblico serve dove §21.5, la matrice test o l'appendice D lo chiedono (tabella di §2). Ogni dato pubblico entra con un record `docs/datasets/PUB-*.md` e una licenza della lista chiusa di §3. I test usano il dato secondo §4 |
| Alternative scartate | Solo sintetico: non soddisfa le uscite di M3.a («campioni pubblici»), M3.e («frame reali pubblici») e il Gate M3 punto 1. Dataset con licenza NC, ND o SA (archivio LCO, Gaia DR3, Tycho-2, file CC BY-NC-SA di raw.pixls.us): fuori dalla lista chiusa. Dataset senza licenza dichiarata alla fonte primaria (NOIRLab, Pan-STARRS, ZTF, APASS, UCAC4): non adottabili (`M3-candidates.md`) |
| Evidenza | Licenze lette alla fonte primaria il 29 ottobre 2026 (`M3-candidates.md`) e, per ESO, SDSS, MAST e Zenodo 3989223, rilette il 30 ottobre 2026 (§2, record); per raw.pixls.us licenze per file dai metadati del 29 ottobre (`PUB-RAW-01.md`). Nessun dato nuovo scaricato |
| Test che la esercitano | T-M3A-DAT-01…06 (§6); T-M3A-RAW-09, T-M3A-LIC-01 (ADR-08.b §6); T-REG-02, T-POST-10, T-AST-ASSIST01, T-ANA-01, T-ANA-04, T-VAL-INDEP sul dato reale |

## 2. Dato per slice

Stato: `ACQUISITO` = file presenti in `data/public/` con SHA-256 nel record; `DA ACQUISIRE` = scelta fatta, file assenti; licenza «aperta» = la fonte primaria non dà ancora una licenza della lista chiusa per quel prodotto. Un dato non acquisito, o con licenza aperta, tiene aperto il gate della slice e dei dipendenti secondo `plans/M3.md`; non blocca l'ingresso.

| Slice | Dato | Record | Fonte | Licenza (SPDX) | Rilascio | Hash | Stato |
|---|---|---|---|---|---|---|---|
| a | RAW di sette marche più un X-Trans | `PUB-RAW-01` | raw.pixls.us | `CC0-1.0` per file; esclusi i CC BY-NC-SA | Nessuna versione; identità = `id` del file | SHA-256 pubblicato dal repository, già nel record | ACQUISITO (30 ottobre 2026) |
| b, c, e, f (coppia g→i) | Palomar LFC, chip 0 | `PUB-LFC-01` | Zenodo 3254683 | `CC-BY-4.0` | Record Zenodo, DOI 10.5281/zenodo.3254683 | SHA-256 nel record; MD5 dell'archivio verificato | ACQUISITO |
| d | Sub CFA Seestar S30 | `PUB-CFA-01` | Zenodo 17843622 | `CC-BY-4.0` | Record Zenodo, DOI 10.5281/zenodo.17843622 | SHA-256 nel record; MD5 dell'archivio intero non verificabile (estrazione in streaming) | ACQUISITO |
| e, f, g, h; s (E2E, B1); Gate M3 punto 1 | EFOSC2, NGC 2808, R#642, una notte: 15 light, 21 bias, 12 flat | `PUB-REG-01` | ESO Science Archive, prog. 092.D-0449(A) | `CC-BY-4.0`, credito ESO, header conservati | Nessuna versione; identità = `dp_id` | Nessun hash pubblicato: SHA-256 all'acquisizione, `CHECKSUM`/`DATASUM` FITS se presenti | DA ACQUISIRE |
| f (riserva, non adottata) | WASP-74b, ASI183MM | — | Zenodo 3989223 | `CC-BY-4.0` (record letto il 30 ottobre 2026) | v1, DOI 10.5281/zenodo.3989223 | MD5 pubblicato dell'archivio `7d201a0be5648a8b3ab5e5af1a8a3239` (15,1 GB) | Non adottata: si adotta solo con un record e una review, se PUB-REG-01 non si può acquisire |
| o (i facoltativo, l facoltativo) | Immagine profonda: cutout HST ACS drz/drc di un campo nebulare | `PUB-DEEP-01` (alla scelta, in M3.i) | MAST | Aperta: la fonte primaria dice «science data become available for public use without restriction» (https://archive.stsci.edu/hst/proprietary_rights.html, letta il 30 ottobre 2026) e «Most data hosted at MAST are in the public domain» (https://archive.stsci.edu/data_use.html, letta il 29 ottobre 2026, `M3-candidates.md`): nessuna delle due è una dichiarazione di pubblico dominio per il prodotto scelto | Prodotto e versione di calibrazione da fissare in M3.i | SHA-256 all'acquisizione | DA ACQUISIRE, licenza aperta |
| p | Estratto 2MASS PSC (astrometria) | componente di pack, record alla scelta | IRSA, 2MASS All-Sky Release | Aperta: la fonte primaria dice «Most data served by IRSA is public, with no usage restrictions» (https://irsa.ipac.caltech.edu/data_use_terms.html, letta il 29 ottobre 2026, `M3-candidates.md`); il «public domain» si legge solo in una risposta dell'helpdesk del 2015 citata nel copyright Debian (fonte secondaria) | All-Sky Release | SHA-256 all'acquisizione, nel manifest del pack (ADR-08.b §3) | DA ACQUISIRE, licenza aperta |
| p (hint reali) | Puntamento negli header | `PUB-LFC-01`, `PUB-REG-01` | come sopra | come sopra | — | come sopra | LFC ACQUISITO; REG DA ACQUISIRE |
| q (footprint SDSS) | Frame SDSS g, r, i con WCS e catalogo SDSS | record alla scelta dei campi, in M3.q | SDSS | `LicenseRef-public-domain`: «All SDSS data released in our public data releases are considered in the public domain» (https://www.sdss.org/collaboration/image-use-policy/, 30 ottobre 2026); credito «Sloan Digital Sky Survey» | Data release da fissare nel record | SHA-256 all'acquisizione | DA ACQUISIRE |
| q (tutto il cielo) | Catalogo broadband ridistribuibile | — | — | Nessun candidato nella lista chiusa | — | — | BLOCKED: decide il proprietario (`M3-candidates.md`) |
| r | Corpus di accettazione: ESO MAXI-J0556-332 (prog. 092.D-0588(A)), Zenodo 3989223, sub Seestar di oggetti diversi da (4) Vesta, campi SDSS disgiunti da quelli di q | `PUB-CORPUS-01` (alla scelta, in M3.r) | ESO, Zenodo, SDSS | `CC-BY-4.0`; `LicenseRef-public-domain` per SDSS | Per componente | SHA-256 all'acquisizione; per Zenodo 3989223 estrazione per membro con richieste HTTP Range, MD5 dell'archivio non verificabile | DA ACQUISIRE |
| j, k, m, n | Nessun dato pubblico | — | — | — | — | — | Non necessario (`plans/M3.md`) |

Scelta di PUB-DEEP-01 in M3.i: un candidato entra solo con licenza della lista chiusa letta alla fonte primaria per il prodotto scelto. Candidati: il cutout HST, se la fonte primaria lo dichiara di pubblico dominio; lo stack Seestar di Zenodo 17843622 (`CC-BY-4.0`, archivio `.tgz` da 35 GB non indicizzabile). Lo stesso vale per 2MASS: una dichiarazione scritta della fonte primaria (IRSA) registrata nel record chiude la licenza; altrimenti p resta con il gate aperto, come dice il rischio di `plans/M3.md`.

## 3. Regola di licenza per i dati di test

Ogni dataset pubblico usato da un test ha una licenza della lista chiusa di ADR-08.b §4.3: `LicenseRef-public-domain` (dichiarazione alla fonte primaria), `CC0-1.0`, `CC-BY-4.0` con attribuzione non vuota, `GPL-2.0-or-later`, `GPL-3.0-or-later`. La regola di ADR-08.b vale per i componenti di pack distribuiti; qui si applica anche ai dati che restano fuori dal repository, perché i record e i risultati derivati (report, metriche) entrano nel repository e nei record di evidenza. Ogni altra licenza richiede una revisione di questo verbale con review. Un file con un identificativo del dispositivo o un dato personale nell'header (per esempio `TELESCOP` dei sub Seestar) non si copia nel repository, nemmeno come estratto.

## 4. NFR-TEST-02: come i test usano il dato pubblico

Regole; il test che le verifica è indicato fra parentesi.

1. **Posizione.** I file stanno in `data/public/<dir>/`, dove `<dir>` è dichiarato nel record. `/data/` è escluso da git (`.gitignore`, riga 7). Nessun file di dataset pubblico in `tests/fixtures/` né in altri percorsi versionati (T-M3A-DAT-01).
2. **Record prima del test.** Il record esiste, con licenza SPDX della lista di §3, attribuzione per `CC-BY-4.0`, `Stato` e, se `ACQUISITO`, il blocco degli SHA-256 nel formato `<sha256>  <percorso relativo a data/public/<dir>/>`, prima che un test legga il file (T-M3A-DAT-01).
3. **Una sola casa dell'hash.** L'hash atteso sta nel record. Un test lo legge dal record (come `documented_sha256` in `tests/unit/test_fits.cpp`); una copia nel sorgente del test è ammessa solo se uguale alla riga del record. Fatto: `tests/unit/test_import_real.cpp` e `tests/unit/test_calibration_real.cpp` hanno copie letterali degli SHA-256 di PUB-LFC-01 e PUB-CFA-01 (T-M3A-DAT-02).
4. **Verifica prima dell'uso.** Ogni lettura di `data/public/` nei test passa dal supporto di test, che legge il file una volta, ne calcola lo SHA-256 e lo confronta con il record con un'asserzione fatale (`ASSERT_*` o eccezione); al codice in prova arrivano quegli stessi byte. Un valore diverso fa fallire il test con atteso, ottenuto e record, e il codice in prova non viene chiamato. Se il codice in prova riceve un percorso e non dei byte (per esempio `run_import_job`), il test gli passa anche l'hash del record e il codice in prova lo verifica sui byte che legge: fatto, `open_verified` in `src/core/import_job.cpp` (riga 164) si ferma con `input.digest_mismatch`. Fatti da correggere prima che T-M3A-DAT-03 passi: `tests/unit/test_fits.cpp` (riga 1360) confronta il digest con `EXPECT_EQ`, non fatale, e poi rilegge il file per percorso; `tests/unit/test_calibration_real.cpp` e `test_import_real.cpp` non usano un supporto comune (T-M3A-DAT-03).
5. **Dato assente: il test fallisce.** Un file mancante fa fallire il test (`ASSERT_*` o eccezione) con il messaggio `PREREQUISITO MANCANTE: <record> <percorso>`. Vietati `GTEST_SKIP`, il prefisso `DISABLED_`, l'uscita anticipata con successo, la registrazione del test in CMake condizionata all'esistenza del file. Ogni `add_test` scritto a mano che lancia un eseguibile gtest su dato pubblico porta `FAIL_REGULAR_EXPRESSION "\\] 0 tests from 0 test suites ran"`, come T-M2E-13 e T-M2R2-09, perché un eseguibile che non esegue alcun test esce con 0; i test registrati con `gtest_discover_tests` sono già uno per caso. Fatto: `T-M2R1-09.public_samples` (`tests/unit/CMakeLists.txt`, righe 84-85) non ha questa guardia (NC-21; T-M3A-DAT-03, T-M3A-DAT-04). Un test su un dato `DA ACQUISIRE` fallisce finché il dato manca: è il segnale del gate aperto della slice.
6. **Sola lettura.** I test aprono `data/public/` in sola lettura e scrivono solo in `tmp/` o in `build/<preset>/test_tmp/`. Dopo l'esecuzione SHA-256 e `mtime` dei file letti sono invariati (T-M3A-DAT-03).
7. **Solo file registrati.** Un test legge da `data/public/` solo percorsi elencati in un record; un percorso non elencato fa fallire il test. Nessuna foto privata dell'utente (NC-16) può entrare per questa via (T-M3A-DAT-03).
8. **Registrazione.** Un test su dato pubblico è sempre registrato in ctest; l'etichetta `fast` o `slow` dipende dalla durata, non dalla presenza del dato. Fatto: `Fits.REAL_DATA_PUB_LFC_01_*` (`tests/unit/test_fits.cpp`) è `fast` tramite `gtest_discover_tests`; T-M2E-13, T-M2R2-09 e T-M2R1-09 sono `slow`.
9. **Corpus separato.** I file di `PUB-CORPUS-01` hanno SHA-256 disgiunti da ogni altro record e campi disgiunti (oggetto, programma, `run`/`camcol`/`field` SDSS) da quelli usati dai test di sviluppo; li leggono solo T-ANA-01, T-ANA-04 e T-VAL-INDEP. Il corpus si sceglie e si registra con gli hash prima che l'Advisor lo veda; nessuna taratura su di esso (T-M3A-DAT-05).
10. **Worktree.** Un worktree non ha `data/` (fatto: assente nel worktree di questa unità): i test su dato pubblico lì falliscono per la regola 5. Si eseguono nella copia principale.

## 5. Fixture sintetiche richieste da NFR-TEST-02

Ogni voce del requisito (riga 2020) ha la sua fixture nella slice che la usa (`plans/M3.md`, «Test, verità di riferimento e dati»). La copertura si verifica al gate M3 (T-M3A-DAT-06).

| Voce di NFR-TEST-02 | Fixture | Slice |
|---|---|---|
| CFA | DNG sintetici scritti con libtiff; CFA campionato da RGB noto | a, d |
| PSF | `FIX-SYN-PSF-01` | o |
| Rumore | `FIX-SYN-NOISE-01` | l |
| Gradienti | Gradiente noto più nebulosa e vignettatura | i |
| Hot pixel | Outlier iniettati in posizioni note in `FIX-SYN-CAL-08`; `FIX-ANA-CORPUS` | b, r |
| Rotazioni, traslazioni subpixel | `FIX-SYN-WARP-01` | f, g |
| Cosmici | Repliche con cosmici | h |
| Bordi | Bordi e pixel invalidi nel resampling | g |
| Difetti Advisor | `FIX-ANA-CORPUS` | r |
| Assenza di master | `FIX-SYN-CAL-08`, otto combinazioni | c |
| Out of distribution | `FIX-ANA-CORPUS`, casi fuori distribuzione | r |

## 6. Test pianificati

Etichette di lavoro, riportate in `docs/test-matrix.md`. Criteri fissati qui, prima di ogni esecuzione. Gli strumenti di 01, 02, 04, 05 sono script `python3 -S` del progetto, non codice di prodotto.

| ID | Verifica | Criterio | Unità |
|---|---|---|---|
| T-M3A-DAT-01 | Controllo dei record `docs/datasets/PUB-*.md`: titolo, fonte con URL, riga `Stato` con `ACQUISITO` o `DA ACQUISIRE`, `Licenza SPDX` della lista di §3, attribuzione non vuota per `CC-BY-4.0`, blocco SHA-256 non vuoto e ben formato se `ACQUISITO`, nessun percorso duplicato; nessun file versionato (`git ls-files`, in qualunque percorso) ha uno SHA-256 uguale a una riga di hash di un record, né sta sotto `data/`. Controlli negativi su record e repository sintetici in `tmp/`: licenza `CC-BY-NC-4.0`, attribuzione assente, `ACQUISITO` senza hash, hash di 63 caratteri, copia versionata di un file registrato in `tests/fixtures/` | Binaria: record reali accettati, ogni negativo rifiutato con il campo nominato | M3.a |
| T-M3A-DAT-02 | Ogni stringa di 64 cifre esadecimali in un sorgente di `tests/` che nomina `data/public` è associata, nello stesso inizializzatore o nella stessa voce di tabella, a un percorso; la coppia (percorso, hash), risolta nella directory del record, coincide con una riga del record. Controlli negativi: una cifra alterata; due digest validi scambiati fra due file; un hash senza percorso associato | Binaria | M3.a |
| T-M3A-DAT-03 | Supporto di test per il dato pubblico, con record e file sintetici in `tmp/`: file assente → test FAILED con `PREREQUISITO MANCANTE` (non SKIPPED, verificato sul junit); un byte cambiato → FAILED prima del parsing (contatore delle chiamate al codice in prova = 0); percorso non elencato → FAILED; SHA-256 e `mtime` del file invariati dopo l'esecuzione. Adozione: lint su `tests/` che fallisce se un sorgente apre un percorso di `data/public` (`std::ifstream`, `open`, `fopen`, `read_file`, `open_fits_input`, costruzione di percorsi con `data/public`) fuori dal supporto, o passa a un'operazione per percorso senza l'hash del record; controllo negativo con un sorgente sintetico per ciascun caso. Integrazione: con una copia in `tmp/` alterata di un byte e un record sintetico, `run_import_job` si ferma con `input.digest_mismatch` | Binaria per ogni caso | M3.a |
| T-M3A-DAT-04 | Lint su `tests/`: nessun `GTEST_SKIP`, nessun `DISABLED_`, nessun `if(EXISTS` su `data/public` nei `CMakeLists.txt`; ogni `add_test` scritto a mano di un eseguibile che legge `data/public` ha `FAIL_REGULAR_EXPRESSION` sul caso «0 tests» (oggi manca a `T-M2R1-09.public_samples`: il lint fallisce finché non si aggiunge). Controlli negativi per ogni regola | Binaria | M3.a |
| T-M3A-DAT-05 | Corpus: SHA-256 di `PUB-CORPUS-01` disgiunti da ogni altro record; campi dichiarati disgiunti; solo i sorgenti di T-ANA-01, T-ANA-04, T-VAL-INDEP nominano la directory del corpus. Controlli negativi: un hash in comune, un campo in comune, un riferimento da un altro test | Binaria | M3.r |
| T-M3A-DAT-06 | Copertura di §5: ogni voce ha almeno un test eseguito nella matrice, con la fixture indicata | Binaria per voce | Gate M3 (M3.s) |

## 7. Acquisizione

Passo separato, deciso dal coordinatore; questo verbale non scarica nulla. Per ogni record `DA ACQUISIRE`:

1. si rilegge la licenza alla fonte primaria e si annota la data; una licenza cambiata o fuori dalla lista ferma l'acquisizione;
2. si scarica in `data/public/<dir>/`; dove la fonte pubblica un hash (raw.pixls.us SHA-256, Zenodo MD5) lo si confronta; un valore diverso ferma l'acquisizione di quel file;
3. si scrivono nel record gli SHA-256 dei file e si porta `Stato` ad `ACQUISITO`; il diff del record passa da una review;
4. nessun file sostitutivo scelto in automatico: un file non acquisibile riapre la scelta con review.

## 8. Limiti

- Licenze aperte per PUB-DEEP-01 (HST) e per l'estratto 2MASS: i gate di o e di p restano aperti finché la fonte primaria non dà una licenza della lista chiusa o non si sceglie un altro candidato con review.
- Colore tutto-cielo BLOCKED: decisione del proprietario.
- PUB-RAW-01 è acquisito (30 ottobre 2026, `docs/datasets/PUB-RAW-01.md`). PUB-REG-01 è scelto ma non acquisito: il gate di f (e a cascata g, h, k, j, i, l…s e Gate M3) resta aperto fino all'acquisizione.
- Per ESO e SDSS non esiste un hash pubblicato dalla fonte: la catena d'integrità parte dal download del progetto. `CHECKSUM`/`DATASUM` FITS, se presenti, sono l'unico controllo indipendente.
- Gli elenchi dei `dp_id` di PUB-REG-01 sono stati letti attraverso uno strumento che riassume la risposta del TAP: si rileggono all'acquisizione (limite nel record).
- La verifica delle licenze non è un parere legale.
- Nessun test `T-M3A-DAT-*` esiste ancora: le regole di §4 sono piani, salvo i fatti citati (`.gitignore`, test M2 esistenti, worktree senza `data/`).
