# Gate M3.b: master completi (F-CAL-01.b)

Stato: **DA INSERIRE DAL RUN DEL GATE** (scheletro preparato prima del run; nessun esito è stato misurato). Clausola: `plans/M3b.md`, sezione 9 «Gate M3.b» (8 punti). Albero delle unità B0…B17: `bd31fc4`.

Criterio di uscita (`plans/M3.md`, riga di M3.b; C-02.b §b.9, C-02.b-flat §f.9): **master riproducibili bit-esatti dal progetto in ordine canonico; report completo; frame incompatibile escluso prima della scelta**.

| Campo | Valore |
|---|---|
| Unità | Gate della slice M3.b |
| Scopo | Verificare gli 8 punti del gate M3.b sul lotto finale (unità B0–B17 chiuse) |
| Comando | DA INSERIRE DAL RUN DEL GATE (vedi «Esecuzione») |
| Commit del run | DA INSERIRE DAL RUN DEL GATE |
| Dati | PUB-LFC-01: SHA-256 dell'archivio `example-cryo-LFC.tar.bz2` `373cfadcbfa4172516c146c8d07b0485e77f73a7af02712d71fc775e33fce659` e SHA-256 dei singoli file FITS nel record `docs/datasets/PUB-LFC-01.md`; PUB-RAW-01 nel suo record |
| Versioni | DA INSERIRE DAL RUN DEL GATE (compilatore, CMake e CTest, LibRaw e libtiff da `third_party/`, SHA-256 del piano attivo) |
| Ambiente | DA INSERIRE DAL RUN DEL GATE (kernel, CPU, RAM, preset, job di build e di test, worker) |
| Esito | DA INSERIRE DAL RUN DEL GATE |
| Autore e data | DA INSERIRE DAL RUN DEL GATE |

## Esecuzione

DA INSERIRE DAL RUN DEL GATE: comandi, log, directory di build, junit prodotti. Il PASS è un solo run completo sullo stesso commit in cui ogni test del preset è passato (`AGENTS.md`, `CLAUDE.md` §3): un test in timeout, saltato o non eseguito non è PASS; il rilancio del singolo test è diagnosi.

Comandi attesi (da confermare col run):

1. Build pulite dev e release; `tools/check.sh` (a passi se un comando supera 600 s).
2. `ctest --preset dev-full` (sanitizer, slow compresi) con `--output-junit tmp/dev-full-junit.xml`.
3. `ctest --preset release` con `--output-junit tmp/release-junit.xml`.
4. `python3 -S tools/trace.py check --stage M3.b --map tests/trace/M3b.tsv --build-dir build/dev --ctest-junit tmp/dev-full-junit.xml` e lo stesso su `build/release` col junit release.
5. `python3 -S tools/evidence.py docs/evidence/M3-b.md` (genera il blocco in fondo).

## Punto 1: run completo

Prova attesa: `ctest --preset dev-full` e `ctest --preset release` sullo stesso commit, ogni test passato.

| Preset | Comando | Commit | Passati/totali | Non passati |
|---|---|---|---|---|
| dev-full | DA INSERIRE DAL RUN DEL GATE | DA INSERIRE DAL RUN DEL GATE | DA INSERIRE DAL RUN DEL GATE | DA INSERIRE DAL RUN DEL GATE |
| release | DA INSERIRE DAL RUN DEL GATE | DA INSERIRE DAL RUN DEL GATE | DA INSERIRE DAL RUN DEL GATE | DA INSERIRE DAL RUN DEL GATE |

## Punto 2: test del gate

| Verifica | Test | Record | Esito |
|---|---|---|---|
| Bit-esattezza e riapertura, metodi | T-M3B-C02-10: `MasterB10.*` (metodi), `MasterB11.*` (report), `MasterB14.*` (candidati permutati), `MasterB16.*` (riapertura del progetto, nodo nello store) | M3b-B10.md, M3b-B11.md, M3b-B14.md, M3b-B16.md | DA INSERIRE DAL RUN DEL GATE |
| Bit-esattezza e riapertura, flat | T-M3B-FLAT-08: stessi suite, parte valori e job | M3b-B10.md, M3b-B11.md, M3b-B14.md, M3b-B16.md | DA INSERIRE DAL RUN DEL GATE |
| Report del master | T-M3B-C02-11: `MasterB11.*`, `MasterB14.*` (sezione «Analisi») | M3b-B11.md, M3b-B14.md | DA INSERIRE DAL RUN DEL GATE |
| Report del flat | T-M3B-FLAT-07: `MasterB11.*`, `MasterB14.*` | M3b-B11.md, M3b-B14.md | DA INSERIRE DAL RUN DEL GATE |
| Esclusione prima della scelta | T-M3B-C02-08: `MasterB12.*`, `MasterB13.*`, `MasterB14.*`, `MasterB15.*`, `MasterB15a.*`, `MasterB16.*` | M3b-B12.md, M3b-B13.md, M3b-B14.md, M3b-B15a.md, M3b-B15.md, M3b-B16.md | DA INSERIRE DAL RUN DEL GATE |

## Punto 3: tutti gli altri test di M3.b

`T-M3B-C02-01…16`, `T-M3B-FLAT-01…09` e i cinque nuovi di `plans/M3b.md` §7 (T-M3B-DSC-01, T-M3B-MEM-01, T-M3B-ANA-01, T-M3B-CFA-01, T-M3B-JOB-01), più T-M3B-DEC-01 e T-M3A-TYP-08. Ogni riga con il suo ctest e il suo record è in `docs/test-matrix.md` (sezioni «M3.entry: ID di lavoro di C-02.b» e «C-02.b-flat»). Esito per test: DA INSERIRE DAL RUN DEL GATE.

Caratterizzazioni senza soglia, da registrare qui con le stime e senza soglia inventata (le fonti delle unità non riportano le stime; i test le stampano sullo stdout di ctest, decisione di B9 giro 1):

| Test | Stime e parametri (`K`, `P`, seed dal protocollo `M3b-protocollo.md`) | Fonte |
|---|---|---|
| T-M3B-C02-06 (`sigma_clip`, selezione congelata; numero dei casi con intervalli sovrapposti tolti prima della misura) | DA INSERIRE DAL RUN DEL GATE | ctest `T-M3B-C02-06.<cella>` |
| T-M3B-C02-07 (quantizzazione della mediana) | DA INSERIRE DAL RUN DEL GATE | `MasterB5.T_M3B_C02_07_*` |
| T-M3B-FLAT-05 (piano robusto eteroschedastico) | DA INSERIRE DAL RUN DEL GATE | ctest `T-M3B-FLAT-05.{n3,n9,n31}` |

Test statistici di classe S (controlli di coerenza) e P: esito, stime ed errore Monte Carlo di T-M3B-C02-03, 04, 05 e T-M3B-FLAT-03, 04: DA INSERIRE DAL RUN DEL GATE.

## Punto 4: T-M3A-CAL-07, T-M3A-CAL-09 (master), riesecuzioni

| Test | Prova attesa | Esito |
|---|---|---|
| T-M3A-CAL-07 | `MasterB15a.T_M3A_CAL_07_*` verde | DA INSERIRE DAL RUN DEL GATE |
| T-M3A-CAL-09 parte master | `MasterB16.T_M3A_CAL_09_*` e `VerifySelection.T_M3A_CAL_09_*` verdi | DA INSERIRE DAL RUN DEL GATE |
| T-M2R2-01…09 | rieseguiti, verdi | DA INSERIRE DAL RUN DEL GATE |
| T-M2R2R-01…05 | rieseguiti, verdi | DA INSERIRE DAL RUN DEL GATE |
| T-M2F-02 | `T-M2F-02.m2e_network` verde (nessuna syscall di rete sul percorso) | DA INSERIRE DAL RUN DEL GATE |

## Punto 5: T-FBK-01 parte M3.b

Prova attesa: ogni guasto si inietta sul percorso del prodotto e nessun percorso alternativo parte. T-M3B-C02-15 (`MasterB3`, `MasterB5`, `MasterB6`, `MasterB11`, `MasterB12`, `MasterB16`, `MasterKappa`; frame modificato, `support_incomplete`) e T-M3B-FLAT-06 (`MasterB3`, `MasterB8`, `MasterB9`, `MasterB16`); il guasto «stato del decoder diverso» è provato da `MasterB16.T_M3A_CAL_09_*` (nessun test porta il nome C02-15). Esito: DA INSERIRE DAL RUN DEL GATE.

## Punto 6: dato reale PUB-LFC-01

Prova attesa: T-M3B-C02-16 (`T-M3B-C02-16.analysis_{bias,dark,dark_flat,flat}`, `T-M3B-C02-16.masters_{bias,dark,dark_flat,flat}`) e T-M3B-FLAT-09 (`T-M3B-FLAT-09.band_{g,i}`) eseguiti, non saltati (il test fallisce se il dato manca; slow, seriali). Record: `M3b-B17.md`. Senza questo punto il gate resta aperto. Esito: DA INSERIRE DAL RUN DEL GATE.

## Punto 7: tracciabilità e protocollo

- `python3 -S tools/trace.py check` pulito per F-CAL-01, F-CAL-01.b e M3.b su `build/dev` (junit `dev-full`) e su `build/release`: blocco generato in fondo. Esito: DA INSERIRE DAL RUN DEL GATE.
- Protocollo `docs/evidence/M3b-protocollo.md` committato prima del primo record di misura (T-M3-ADR09-01): primo commit del protocollo `f24d1bb` (9 novembre 2026, 07:54), anteriore al primo commit di codice o di test dopo le unità documentali (B2 `5e413c3`, 09:04) e quindi a ogni record di misura (i test statistici sono in B5, `483b671`). Verifica sulla storia di git da ripetere al gate: DA INSERIRE DAL RUN DEL GATE.

## Punto 8: review e build pulita

Review PASS per ogni unità (tetto di 2 giri, `AGENTS.md`):

| Unità | Record | Chiusura |
|---|---|---|
| B0 | `M3b-B0.md` | review PASS al giro 2 (Argon) |
| B1 | `M3b-B1.md` | review PASS al giro 1 |
| B2 | `M3b-B2.md` | review PASS al giro 1 |
| B3 | `M3b-B3.md` | review PASS al giro 2 (Argon) |
| B3b | `M3b-B3b.md` | review PASS al giro 1 (Argon) |
| B4 | `M3b-B4.md` | review PASS al giro 1 (Argon) |
| B5 | `M3b-B5.md` | review PASS al giro 2 |
| B6 | `M3b-B6.md` | review PASS al giro 2 |
| B7 | `M3b-B7.md` | review PASS al giro 2 |
| B8 | `M3b-B8.md` | review PASS al giro 2 |
| B9 | `M3b-B9.md` | review PASS al giro 2 |
| B10 | `M3b-B10.md` | review PASS al giro 2 |
| B11 | `M3b-B11.md` | review PASS al giro 2 |
| B12 | `M3b-B12.md` | review PASS al giro 2 |
| B13 | `M3b-B13.md` | review PASS al giro 2 |
| B14 | `M3b-B14.md` | giro 2 FAIL con 1 MED sul piano (difetto del piano, non del codice); arbitraggio del coordinatore, piano corretto, nessun terzo giro |
| B15a | `M3b-B15a.md` | review PASS al giro 2 (Claude Opus; Argon senza esito) |
| B15 | `M3b-B15.md` | review PASS al giro 2 |
| B16 | `M3b-B16.md` | review PASS al giro 2 |
| B17 | `M3b-B17.md` | review PASS al giro 2 |

Build pulita dev e release: DA INSERIRE DAL RUN DEL GATE.

## Limiti

- **Lettore RAW delle statistiche** (`plans/M3b.md` §9): dal lettore non si provocano lo stop `raw.libraw_error` né il cancel durante la decodifica (nessun gancio verso il decoder); la classificazione è provata sulla funzione pura `raw_decode_error_is_file_outcome` (B15; `docs/decisions/M3b-lacune.md`, «Decisioni successive (B16)»). Il lettore è provato con gli esiti di file `raw.data_error`, `raw.file_unsupported`, `raw.not_cfa`, `raw.max_pixels` e con digest, `input.missing`, budget e cancel prima e durante la lettura verificata.
- Byte del file letto (FITS e RAW) fuori dal pool fra la lettura verificata e la prenotazione (B15, B16): da chiudere con la lettura verificata prenotata nel pool, in un'unità che tocchi quel confine. File RAW letti due volte nel piano (B16).
- Voce `raw_dark_mediana`: la factory decodifica fuori dal pool e senza cancel; è la voce minima di conformità, non il percorso del prodotto (B16).
- Dato reale (B17): N = 3 per ruolo (7 per i bias), mono, nessun CFA, un solo filtro per flat; `A` è una stima conservativa da 3 frame; un pixel saturo per banda è escluso da `A` e invalido nei master del flat. La parte CFA e RAW è coperta dai test sintetici di B10, B15, B16; il confronto fra backend (ADR-07) è fuori da M3.b.
- Limiti dichiarati dai contratti (C-02.b §b.14, C-02.b-flat §f.14): piano dei metodi robusti non distorto solo sotto `iid_gaussian` e campioni continui; selezione congelata di `sigma_clip` senza garanzia; `scale_fixed` non derivata; il nero del decoder è sentinella dell'offset. Il report li dichiara come ipotesi non verificate.
- Chiavi dei nodi: `master_algorithm_version` nuova (B3, B7) rende `stale` i master `.a` già prodotti (`plans/M3b.md` §11).
- Mutazioni: i record delle unità riportano i conteggi (B3 12/12, B3b 5/5, B4 5/5, B6 7/7, B7 11/11, B8 15/15, B9 11/11, B10 5/5, B11 8/8 + 4/4, B12 15/15, B13 12/12, B14 14/14, B15a 16/16, B15 mutazioni del giro 1 tutte rilevate, conteggio nel suo record); B5, B16 e B17: non riportati nelle fonti.
- I limiti e i LOW residui di ogni unità restano validi come dichiarati nel suo record (`docs/evidence/M3b-B*.md`).
- Rinviati: DA INSERIRE DAL RUN DEL GATE.

## Esiti del run

| Voce | Valore |
|---|---|
| Comando | DA INSERIRE DAL RUN DEL GATE |
| Commit | DA INSERIRE DAL RUN DEL GATE |
| Passati/totali | DA INSERIRE DAL RUN DEL GATE |
| Non passati | DA INSERIRE DAL RUN DEL GATE |
| Esito del gate | DA INSERIRE DAL RUN DEL GATE |

Conteggi e hash generati:

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano. DA INSERIRE DAL RUN DEL GATE (serve il junit completo).
<!-- evidence:end -->
