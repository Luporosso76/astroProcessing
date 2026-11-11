# Gate M3.b: master completi (F-CAL-01.b)

Stato: **PASS (test 1829/1829 in dev-full e 1829/1829 in release; review del gate PASS al giro 2; build pulita e controllo di equivalenza al Punto 8).** Il run è del 2026-11-11 sul commit `45fc9a90b8b9010e51af6180e059c8ce9df85ce9`. Clausola: `plans/M3b.md`, sezione 9 «Gate M3.b» (8 punti). Albero delle unità B0…B17: ramo `m3b-gate` su `45fc9a9` (B16 `9bb95ef`; LOW residui di B15, B16, B17 in `3d2c6f2`, delta letto dalla review del gate senza rilievi). `bd31fc4`, citato nel record di B16, è il commit di B16 prima del rebase e non è antenato di `45fc9a9`.

Criterio di uscita (`plans/M3.md`, riga di M3.b; C-02.b §b.9, C-02.b-flat §f.9): **master riproducibili bit-esatti dal progetto in ordine canonico; report completo; frame incompatibile escluso prima della scelta**.

| Campo | Valore |
|---|---|
| Unità | Gate della slice M3.b |
| Scopo | Verificare gli 8 punti del gate M3.b sul lotto finale (unità B0–B17 chiuse) |
| Comando | `tmp/gate/run.sh` (build dev con `tools/check.sh --step build`, `cmake --preset release`, `cmake --build --preset release`, `ctest --preset dev-full`, `ctest --preset release`; vedi «Esecuzione») |
| Commit del run | `45fc9a90b8b9010e51af6180e059c8ce9df85ce9`. Il ramo `m3b-gate` è `45fc9a9` più `71c25a4` (solo `tools/review.sh`), una riga di commento in `tools/model-routing.tsv` e i documenti del gate: il run è su `45fc9a9`, il resto sono strumenti e documenti, non codice del prodotto |
| Dati | PUB-LFC-01: SHA-256 dell'archivio `example-cryo-LFC.tar.bz2` `373cfadcbfa4172516c146c8d07b0485e77f73a7af02712d71fc775e33fce659` e SHA-256 dei singoli file FITS nel record `docs/datasets/PUB-LFC-01.md`; PUB-RAW-01 (`docs/datasets/`) è letto solo dai test di M3.a (`test_batch_classification`, `test_raw_import`, `test_pack`, `test_dng_u13b`, `fuzz_raw_decode`, `test_batch_analysis`, `raw_threads`), compresi nel run completo; nessun test `test_master_b*` lo legge (vedi «Limiti») |
| Versioni | g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0 (`CMAKE_CXX_COMPILER_VERSION` 15.2.0 nella configurazione di `build/release`); CMake 4.2.3; CTest 4.2.3; LibRaw 0.22.3 (`third_party/libraw/PROVENANCE.md`); libtiff 4.7.2 (`third_party/libtiff/PROVENANCE.md`); SHA-256 del piano attivo `ASTRO_POSTPROCESSING_PLAN_v2.2.1.md`: `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094` |
| Ambiente | Linux 7.0.0-43-generic; AMD Ryzen 7 6800H, 8 core, 16 thread (`lscpu`); RAM 30 GiB totali (`free -g`, misurata a record compilato); preset `dev`, `dev-full` e `release`; `CMAKE_BUILD_PARALLEL_LEVEL=5`, `CTEST_PARALLEL_LEVEL=2`; `TMPDIR` e `CCACHE_DIR` nel progetto. Altre deleghe hanno fatto build leggere sulla stessa macchina durante il run (vedi «Esiti del run») |
| Esito | Test: 1829/1829 passati in dev-full (15110 s) e 1829/1829 in release (10040 s), nessun test fallito, in timeout, saltato o non eseguito; trace pulita. Gate: PASS, review del gate (revisore separato) PASS al giro 2 il 2026-11-11; build pulita dev+release e controllo di equivalenza su `71c25a4` (Punto 8) |
| Autore e data | Coordinatore (sessione Claude); run 2026-11-11 dalle 15:23:44 alle 22:24:31 (+01:00); record compilato dai log e dai junit del run il 2026-11-11 |

## Esecuzione

Un solo run, script `tmp/gate/run.sh` (copiato con `rc.txt`, `00-commit.txt` e i log `01…06` in `tmp/gate/` del worktree `m3b-gate`), nel worktree `.claude/worktrees/m3b-b16` sul commit `45fc9a90b8b9010e51af6180e059c8ce9df85ce9`, dalle 2026-11-11T15:23:44+01:00 alle 22:24:31+01:00. Variabili: `CMAKE_BUILD_PARALLEL_LEVEL=5`, `CTEST_PARALLEL_LEVEL=2`, `TMPDIR=<progetto>/tmp`, `CCACHE_DIR=<progetto>/tmp/cache/ccache`. Directory di build del run: `build/dev` e `build/release` di `m3b-b16`.

| Passo | Comando | rc | Durata |
|---|---|---|---|
| 01-build-dev | `tools/check.sh --step build` | 0 | 36 s |
| 02-conf-release | `cmake --preset release` | 0 | 3 s |
| 03-build-release | `cmake --build --preset release` | 0 | 58 s |
| 04-dev-full | `ctest --preset dev-full --output-junit tmp/dev-full-junit.xml` | 0 | 15110 s |
| 05-release | `ctest --preset release --output-junit tmp/release-junit.xml` | 0 | 10040 s |
| 06-trace | `python3 -S tools/trace.py check --stage M3.b --map tests/trace/M3b.tsv --build-dir build/{dev,release} --ctest-junit tmp/{dev-full,release}-junit.xml` | 0 e 0 | log `06-trace-dev.log`, `06-trace-release.log` |

Le build 01 e 03 del run erano **incrementali** sopra la build dell'unità B16 (3 unità compilate, 87 link): non sono build pulite. La build pulita dev+release dell'albero del gate è nel Punto 8.

Archivio fuori dai worktree (a cura del coordinatore): junit e log del gate (`tmp/gate/`, i due junit del run, i junit diagnostici) in `/opt/workspace/astroProcessor/.omc/artifacts/gate-M3b-20261111/`. Junit copiati in `tmp/dev-full-junit.xml` e `tmp/release-junit.xml` di questo worktree: SHA-256 dev-full `778b8f318edd122b610984559ae1482c027a3e10a883e75dd03f9ba0dfc20a5e`, release `18f7e62c324cb402a751a257ccf259a9eae6188a99e19b2f7bef2110825bc3f5`. Contati sui due junit: `tests="1829"`, `failures="0"`, `disabled="0"`, `skipped="0"` in entrambi, 1829 `testcase` con `status="run"`, nessun elemento `failure` o `skipped`; etichette fast 1772 e slow 57 in entrambi i preset (`04-dev-full.log`, `05-release.log`). Il PASS è un solo run completo sullo stesso commit in cui ogni test del preset è passato (`AGENTS.md`, `CLAUDE.md` §3): un test in timeout, saltato o non eseguito non è PASS; il rilancio del singolo test è diagnosi.

Comandi del gate (1: eseguito in forma incrementale nel run e in forma pulita sul commit `71c25a4`, Punto 8; 5: eseguito dal coordinatore dopo la build pulita, il blocco in fondo è generato):

1. Build pulite dev e release; `tools/check.sh` (a passi se un comando supera 600 s).
2. `ctest --preset dev-full` (sanitizer, slow compresi) con `--output-junit tmp/dev-full-junit.xml`.
3. `ctest --preset release` con `--output-junit tmp/release-junit.xml`.
4. `python3 -S tools/trace.py check --stage M3.b --map tests/trace/M3b.tsv --build-dir build/dev --ctest-junit tmp/dev-full-junit.xml` e lo stesso su `build/release` col junit release.
5. `python3 -S tools/evidence.py docs/evidence/M3-b.md` (genera il blocco in fondo; eseguito dal coordinatore dopo la build pulita, `--check` con uscita 0).

## Punto 1: run completo

Prova attesa: `ctest --preset dev-full` e `ctest --preset release` sullo stesso commit, ogni test passato.

| Preset | Comando | Commit | Passati/totali | Non passati |
|---|---|---|---|---|
| dev-full | `ctest --preset dev-full --output-junit tmp/dev-full-junit.xml` | `45fc9a90b8b9010e51af6180e059c8ce9df85ce9` | 1829/1829 (fast 1772, slow 57; 15110 s) | nessuno (elenco vuoto: 0 fallimenti, 0 timeout, 0 saltati, 0 non eseguiti) |
| release | `ctest --preset release --output-junit tmp/release-junit.xml` | `45fc9a90b8b9010e51af6180e059c8ce9df85ce9` | 1829/1829 (fast 1772, slow 57; 10040 s) | nessuno (elenco vuoto: 0 fallimenti, 0 timeout, 0 saltati, 0 non eseguiti) |

## Punto 2: test del gate

Conteggio fatto sui due junit: i test della suite elencata con l'identificatore del test (`T_M3B_C02_10_` ecc.) nel nome. Stessi conteggi e stessi esiti in dev-full e in release. I due preset hanno 1829 test ciascuno ma non lo stesso insieme di nomi: `BuildInfo.SanitizersAreActiveInDevBuild` solo in dev-full, `T-U12-12.system_licenses` solo in release.

| Verifica | Test | Record | Esito |
|---|---|---|---|
| Bit-esattezza e riapertura, metodi | T-M3B-C02-10: `MasterB10.*` (metodi), `MasterB11.*` (report), `MasterB14.*` (candidati permutati), `MasterB16.*` (riapertura del progetto, nodo nello store) | M3b-B10.md, M3b-B11.md, M3b-B14.md, M3b-B16.md | PASS: 20 test (B10 7, B11 1, B14 4, B16 8), tutti passati in dev-full e in release |
| Bit-esattezza e riapertura, flat | T-M3B-FLAT-08: stessi suite, parte valori e job | M3b-B10.md, M3b-B11.md, M3b-B14.md, M3b-B16.md | PASS: 12 test (B10 6, B11 1, B14 2, B16 3), tutti passati in dev-full e in release |
| Report del master | T-M3B-C02-11: `MasterB11.*`, `MasterB14.*` (sezione «Analisi») | M3b-B11.md, M3b-B14.md | PASS: 27 test (B11 21, B14 6), tutti passati in dev-full e in release |
| Report del flat | T-M3B-FLAT-07: `MasterB11.*`, `MasterB14.*` | M3b-B11.md, M3b-B14.md | PASS: 14 test (B11 8, B14 6), tutti passati in dev-full e in release |
| Esclusione prima della scelta | T-M3B-C02-08: `MasterB12.*`, `MasterB13.*`, `MasterB14.*`, `MasterB15.*`, `MasterB15a.*`, `MasterB16.*` | M3b-B12.md, M3b-B13.md, M3b-B14.md, M3b-B15a.md, M3b-B15.md, M3b-B16.md | PASS: 166 test (B12 79, B13 46, B14 19, B15 14, B15a 5, B16 3), tutti passati in dev-full e in release |

## Punto 3: tutti gli altri test di M3.b

`T-M3B-C02-01…16`, `T-M3B-FLAT-01…09` e i cinque nuovi di `plans/M3b.md` §7 (T-M3B-DSC-01, T-M3B-MEM-01, T-M3B-ANA-01, T-M3B-CFA-01, T-M3B-JOB-01), più T-M3B-DEC-01 e T-M3A-TYP-08. Ogni riga con il suo ctest e il suo record è in `docs/test-matrix.md` (sezioni «M3.entry: ID di lavoro di C-02.b» e «C-02.b-flat»). Esito per test, contato sui due junit (test con l'identificatore `T_M3B_..._` nel nome, compresi i ctest slow con nome `T-M3B-...`; stesso numero e tutti passati in dev-full e in release):

| Test | n. test | | Test | n. test | | Test | n. test |
|---|---|---|---|---|---|---|---|
| T-M3B-C02-01 | 13 | | T-M3B-C02-11 | 27 | | T-M3B-FLAT-03 | 1 |
| T-M3B-C02-02 | 13 | | T-M3B-C02-12 | 12 | | T-M3B-FLAT-04 | 5 |
| T-M3B-C02-03 | 5 | | T-M3B-C02-13 | 6 | | T-M3B-FLAT-05 | 4 |
| T-M3B-C02-04 | 9 | | T-M3B-C02-14 | 14 | | T-M3B-FLAT-06 | 30 |
| T-M3B-C02-05 | 9 | | T-M3B-C02-15 | 19 | | T-M3B-FLAT-07 | 14 |
| T-M3B-C02-06 | 7 | | T-M3B-C02-16 | 8 | | T-M3B-FLAT-08 | 12 |
| T-M3B-C02-07 | 1 | | T-M3B-FLAT-01 | 9 | | T-M3B-FLAT-09 | 2 |
| T-M3B-C02-08 | 166 | | T-M3B-FLAT-02 | 2 | | T-M3B-DSC-01 | 10 |
| T-M3B-C02-09 | 24 | | T-M3B-C02-10 | 20 | | T-M3B-MEM-01 | 12 |
| T-M3B-ANA-01 | 3 | | T-M3B-CFA-01 | 17 | | T-M3B-JOB-01 | 10 |
| T-M3B-DEC-01 | 6 | | T-M3A-TYP-08 | 6 | | | |

Tutti passati (nessun `failure` né `skipped` nei junit). Nessuno degli ID del Punto 3 ha zero test nei junit. I conteggi dei test con ID nel nome non sono i soli test della riga: le righe di `docs/test-matrix.md` elencano anche suite che non portano l'ID nel nome (per esempio la conformità T-M3BASE per T-M3B-JOB-01); quei test sono nel run completo del Punto 1.

Caratterizzazioni senza soglia, da registrare qui con le stime e senza soglia inventata (le fonti delle unità non riportano le stime; i test le stampano sullo stdout di ctest, decisione di B9 giro 1):

| Test | Stime e parametri (`K`, `P`, seed dal protocollo `M3b-protocollo.md`) | Fonte |
|---|---|---|
| T-M3B-C02-06 (`sigma_clip`, selezione congelata; numero dei casi con intervalli sovrapposti tolti prima della misura) | Stime `E[V]/Var(M)` come stampate (`R` = replicazioni, 20 lotti, `nu` 8187904 in tutte le celle; `support_incomplete` 0 e altri invalidi 0 in tutte): `n=11 clip_max_iter=1` R 80: 0.99992053 (media dei piani 1.4548795, varianza raggruppata dei master 1.4549951, errore standard 0.00044962483); `n=11 clip_max_iter=5` R 80: 0.99992053 (stessi valori); `n=21 clip_max_iter=1` R 40: 0.98885309 (0.75752654, 0.76606581, es 0.00049641691); `n=21 clip_max_iter=5` R 40: 0.98877771 (0.75750137, 0.76609875, es 0.00049701012); `n=51 clip_max_iter=1` R 16: 0.96618628 (0.30877547, 0.31958171, es 0.0005237466); `n=51 clip_max_iter=5` R 16: 0.96419894 (0.30851224, 0.31996742, es 0.00052954592). Identiche in dev-full e in release (le durate stampate differiscono). L'uscita completa è intera già nel junit del run (910 byte, sotto i 1024). Il numero dei casi con intervalli sovrapposti tolti prima della misura non appartiene a C02-06 (protocollo §3.3): lo stampano i test di T-M3B-C02-02, tabella sotto. Nessuna soglia. `K`, `P`, seed: protocollo `M3b-protocollo.md`, riga T-M3B-C02-06 (K 2000, 4096, seed 2026110921…2026110923) | ctest `T-M3B-C02-06.<cella>` (6 celle) |
| T-M3B-C02-07 (quantizzazione della mediana) | Deterministico, nessun seme; le uscite complete (riesecuzione) coincidono col prefisso di 1024 byte visibile nei due junit del run, dev-full e release. Tabella completa sotto («Stime complete»), colonna `Var(M)/E[V]` come la stampa il test; nessuna soglia. Il junit del run è troncato a 1024 byte e il suo prefisso visibile coincide con l'inizio dell'uscita completa | `MasterB5.T_M3B_C02_07_*` |
| T-M3B-FLAT-05 (piano robusto eteroschedastico) | Tutte le 220 celle nelle tabelle sotto («Stime complete»), da riesecuzione diagnostica; nel junit del run le uscite sono troncate a 1024 byte, i prefissi visibili coincidono con l'inizio delle uscite complete (dev-full e release: l'identità fra i due preset è provata sul prefisso). Nessuna soglia | ctest `T-M3B-FLAT-05.{n3,n9,n31}` |

Test statistici di classe S (controlli di coerenza) e P: T-M3B-C02-03, 04, 05 e T-M3B-FLAT-03, 04 passati in dev-full e in release (C02-03 5 test, C02-04 9, C02-05 9, FLAT-03 1, FLAT-04 5; i test slow sono controlli con soglia e l'esito è il passaggio del ctest). T-M3B-C02-05 non stampa stime (uscita vuota). Stime ed errori Monte Carlo: nelle tabelle sotto.

### Riesecuzione diagnostica delle stime (non parte del PASS)

Il junit del run tronca a 1024 byte l'uscita di parte dei test. Per leggere le stime intere il coordinatore ha rieseguito, sulla build pulita release (commit `71c25a4`, stesso codice di `45fc9a9`: la differenza è solo `tools/review.sh`), 39 test in tre comandi: (1) i 25 ctest slow con `ctest --preset release -R 'T-M3B-C02-0[34567]|T-M3B-FLAT-0[345]' --test-output-size-passed 4194304 --output-junit tmp/gate/caratterizzazioni-junit.xml -j 4` (uscita 0, log `tmp/gate/17-caratterizzazioni.log`); (2) il gtest di T-M3B-C02-07 con `ctest --preset release -R 'MasterB5\.T_M3B_C02_07' --test-output-size-passed 4194304 --output-junit tmp/gate/c0207-junit.xml` (uscita 0, 1/1 passato, 0,26 s, log `tmp/gate/18-c0207.log`); (3) i 13 gtest di T-M3B-C02-02 con `ctest --preset release -R 'C02[_-]02' --test-output-size-passed 4194304 --output-junit tmp/gate/c0202-junit.xml` (uscita 0, 13/13 passati, log `tmp/gate/19-c0202.log`). È una riesecuzione **diagnostica**, non parte del PASS del gate (che è il run su `45fc9a9`). Controllo fatto in questa compilazione: 25/25 testcase passati nella riesecuzione (nessun `failure` né `skipped`); per ognuno le righe di statistica del junit del run (`tmp/release-junit.xml`) coincidono carattere per carattere con le righe corrispondenti dell'uscita completa, escluse le durate stampate (`durata`, `estrazioni`, `combinazione`, `(N ms)`): per i 16 test interi nel junit del run il confronto è sulle righe `T-M3B`/di statistica, per i 9 troncati sul prefisso visibile (test deterministici); 0 differenze oltre le durate. Per i 13 test di C02-02 le righe del junit del run (`n = 11` e `n = 15`, dev-full e release) coincidono con le righe dell'uscita completa; la riga di `n = 31` è troncata nel run e si legge solo nella riesecuzione. Per C02-07 il prefisso di 1024 byte del junit del run coincide con l'inizio dell'uscita completa (3380 byte, 55 righe di dati). I percorsi della prima riga (`Running main() from`) sono normalizzati.

### Stime complete (dalla riesecuzione diagnostica)

**T-M3B-C02-07** (`σ ∈ {0.5, 1, 1.5, 2, 3, 5}`, `n ∈ {3, 9, 101}`, `φ ∈ {0, 0.25, 0.5}`; 54 celle, `err_rel` massimo 1.79e-14 (il test stampa «errore massimo 1.78748e-14»), colonne come stampate):

| sigma | n | phi | Var(M)/E[V] | err_rel |
|---|---|---|---|---|
| 0.5 | 3 | 0 | 0.925234898597437 | 2.63e-16 |
| 0.5 | 3 | 0.25 | 1.29796065508592 | 1.49e-16 |
| 0.5 | 3 | 0.5 | 1.67275398415054 | 3.82e-16 |
| 1 | 3 | 0 | 1.0936931325475 | 1.93e-16 |
| 1 | 3 | 0.25 | 1.09452338076936 | 4.05e-16 |
| 1 | 3 | 0.5 | 1.09535365293115 | 2.54e-16 |
| 1.5 | 3 | 0 | 1.0438857856897 | 1.44e-16 |
| 1.5 | 3 | 0.25 | 1.04388586091843 | 3.79e-16 |
| 1.5 | 3 | 0.5 | 1.04388593614715 | 1.33e-16 |
| 2 | 3 | 0 | 1.02507763481015 | 3.67e-16 |
| 2 | 3 | 0.25 | 1.02507763481053 | 6.49e-17 |
| 2 | 3 | 0.5 | 1.02507763481091 | 1.53e-16 |
| 3 | 3 | 0 | 1.01127343216253 | 1.66e-16 |
| 3 | 3 | 0.25 | 1.01127343216253 | 2.74e-16 |
| 3 | 3 | 0.5 | 1.01127343216253 | 7.13e-16 |
| 5 | 3 | 0 | 1.00408240566683 | 4.22e-16 |
| 5 | 3 | 0.25 | 1.00408240566683 | 4.23e-16 |
| 5 | 3 | 0.5 | 1.00408240566683 | 1.98e-17 |
| 0.5 | 9 | 0 | 0.26685719373025 | 3.01e-16 |
| 0.5 | 9 | 0.25 | 1.76566529679069 | 2.87e-16 |
| 0.5 | 9 | 0.5 | 4.41056607967463 | 1.03e-16 |
| 1 | 9 | 0 | 1.22272201936428 | 2.69e-16 |
| 1 | 9 | 0.25 | 1.38530383355172 | 6.25e-16 |
| 1 | 9 | 0.5 | 1.54967769621701 | 4.17e-16 |
| 1.5 | 9 | 0 | 1.17628900204958 | 4.45e-16 |
| 1.5 | 9 | 0.25 | 1.17930067094604 | 3.68e-16 |
| 1.5 | 9 | 0.5 | 1.18231265174035 | 2.22e-16 |
| 2 | 9 | 0 | 1.10244332801499 | 1.95e-16 |
| 2 | 9 | 0.25 | 1.10245761536081 | 5.12e-16 |
| 2 | 9 | 0.5 | 1.1024719027111 | 2.54e-17 |
| 3 | 9 | 0 | 1.04605892799735 | 1.07e-16 |
| 3 | 9 | 0.25 | 1.04605892800724 | 9.21e-16 |
| 3 | 9 | 0.5 | 1.04605892801714 | 6.75e-16 |
| 5 | 9 | 0 | 1.01667914668701 | 4.17e-16 |
| 5 | 9 | 0.25 | 1.01667914668701 | 6.75e-16 |
| 5 | 9 | 0.5 | 1.01667914668701 | 8.93e-16 |
| 0.5 | 101 | 0 | 2.86563422987429e-13 | 1.79e-14 |
| 0.5 | 101 | 0.25 | 0.00583360804420699 | 1.04e-14 |
| 0.5 | 101 | 0.5 | 47.3060421289972 | 1.18e-15 |
| 1 | 101 | 0 | 0.0035899643170044 | 1.09e-14 |
| 1 | 101 | 0.25 | 1.29868426319531 | 8.53e-17 |
| 1 | 101 | 0.5 | 14.9015738391988 | 3.19e-15 |
| 1.5 | 101 | 0 | 0.205383602448466 | 5.91e-16 |
| 1.5 | 101 | 0.25 | 2.27222277558729 | 8.56e-17 |
| 1.5 | 101 | 0.5 | 6.91859301903337 | 9.93e-15 |
| 2 | 101 | 0 | 0.704803884852389 | 1.47e-15 |
| 2 | 101 | 0.25 | 2.12566643089813 | 2.78e-15 |
| 2 | 101 | 0.5 | 3.95538369271272 | 7.3e-15 |
| 3 | 101 | 0 | 1.28385741049282 | 5.17e-16 |
| 3 | 101 | 0.25 | 1.58021384505509 | 1.97e-15 |
| 3 | 101 | 0.5 | 1.88277274248955 | 1.42e-14 |
| 5 | 101 | 0 | 1.20912845799246 | 1.95e-15 |
| 5 | 101 | 0.25 | 1.21120753565313 | 1.83e-15 |
| 5 | 101 | 0.5 | 1.21328673995197 | 1.34e-14 |

**T-M3B-FLAT-05**, `E[V]/Var(M|r)` stimato `± errore` (N 261888 per cella, `support_incomplete` 0 e altri invalidi 0 in tutte; 220 celle = 60 per `n=3` + 80 per `n=9` + 80 per `n=31`; per riga `rapporto` e regime, per colonna metodo e classe `g=1/2` o `g=3/2`; `sigma_clip` con soglie uguali `k = 1` (`n=3`), `2.5` (`n=9`), `3` (`n=31`) come stampate):

`n=3`:

| rapporto | regime | median g=1/2 | median g=3/2 | winsorized g=1 g=1/2 | winsorized g=1 g=3/2 | mean + sigma_clip k=1 g=1/2 | mean + sigma_clip k=1 g=3/2 |
|---|---|---|---|---|---|---|---|
| 1 | fotonico | 1.0002 ± 0.00326733 | 0.995748 ± 0.00284549 | 1.0002 ± 0.00326733 | 0.995748 ± 0.00284549 | 0.136099 ± 0.00036933 | 0.135255 ± 0.000397022 |
| 1 | lettura | 0.996899 ± 0.00344693 | 0.994749 ± 0.0027786 | 0.996899 ± 0.00344693 | 0.994749 ± 0.0027786 | 0.135072 ± 0.000418998 | 0.134786 ± 0.000476666 |
| 1.5 | fotonico | 1.01501 ± 0.00403404 | 1.01644 ± 0.00437662 | 1.01501 ± 0.00403404 | 1.01644 ± 0.00437662 | 0.137538 ± 0.000554949 | 0.138656 ± 0.00071941 |
| 1.5 | lettura | 1.06855 ± 0.0034856 | 1.07269 ± 0.00221629 | 1.06855 ± 0.0034856 | 1.07269 ± 0.00221629 | 0.145643 ± 0.000569585 | 0.145678 ± 0.00044694 |
| 2 | fotonico | 1.05228 ± 0.0033791 | 1.05509 ± 0.00382276 | 1.05228 ± 0.0033791 | 1.05509 ± 0.00382276 | 0.142576 ± 0.000315919 | 0.142937 ± 0.00052926 |
| 2 | lettura | 1.2202 ± 0.00434526 | 1.21086 ± 0.00366436 | 1.2202 ± 0.00434526 | 1.21086 ± 0.00366436 | 0.163487 ± 0.000606532 | 0.162961 ± 0.000564835 |
| 3 | fotonico | 1.14111 ± 0.00323373 | 1.14654 ± 0.0032185 | 1.14111 ± 0.00323373 | 1.14654 ± 0.0032185 | 0.153781 ± 0.000526468 | 0.154693 ± 0.000494172 |
| 3 | lettura | 1.64091 ± 0.00562234 | 1.59448 ± 0.00543225 | 1.64091 ± 0.00562234 | 1.59448 ± 0.00543225 | 0.204932 ± 0.000918724 | 0.200955 ± 0.00101167 |
| 4 | fotonico | 1.24435 ± 0.00465256 | 1.24382 ± 0.00323298 | 1.24435 ± 0.00465256 | 1.24382 ± 0.00323298 | 0.166211 ± 0.000698283 | 0.166025 ± 0.000518825 |
| 4 | lettura | 2.20752 ± 0.0118844 | 2.12531 ± 0.00816228 | 2.20752 ± 0.0118844 | 2.12531 ± 0.00816228 | 0.246866 ± 0.00098862 | 0.241553 ± 0.00105001 |

`n=9`:

| rapporto | regime | median g=1/2 | median g=3/2 | winsorized g=1 g=1/2 | winsorized g=1 g=3/2 | winsorized g=4 g=1/2 | winsorized g=4 g=3/2 | mean + sigma_clip k=2.5 g=1/2 | mean + sigma_clip k=2.5 g=3/2 |
|---|---|---|---|---|---|---|---|---|---|
| 1 | fotonico | 0.997277 ± 0.00295307 | 1.00318 ± 0.0028157 | 0.997494 ± 0.0026821 | 1.00183 ± 0.00282494 | 0.997277 ± 0.00295307 | 1.00318 ± 0.0028157 | 0.993652 ± 0.00239895 | 0.998622 ± 0.00269408 |
| 1 | lettura | 1.00098 ± 0.00308855 | 0.998552 ± 0.00238834 | 0.998904 ± 0.00319992 | 1.00081 ± 0.00294129 | 1.00098 ± 0.00308855 | 0.998552 ± 0.00238834 | 0.996356 ± 0.00326842 | 0.998232 ± 0.00283466 |
| 1.5 | fotonico | 1.0125 ± 0.00274483 | 1.0087 ± 0.00305167 | 1.00478 ± 0.0030938 | 1.00114 ± 0.00350646 | 1.0125 ± 0.00274483 | 1.0087 ± 0.00305167 | 0.997774 ± 0.0029832 | 0.993447 ± 0.00330972 |
| 1.5 | lettura | 1.048 ± 0.00298901 | 1.04871 ± 0.00246224 | 1.01719 ± 0.00227752 | 1.01865 ± 0.00260214 | 1.048 ± 0.00298901 | 1.04871 ± 0.00246224 | 0.994922 ± 0.00220397 | 0.99797 ± 0.00222477 |
| 2 | fotonico | 1.03314 ± 0.00420282 | 1.03673 ± 0.0037878 | 1.01305 ± 0.0031713 | 1.01477 ± 0.00384714 | 1.03314 ± 0.00420282 | 1.03673 ± 0.0037878 | 0.995065 ± 0.00318796 | 0.996116 ± 0.00365501 |
| 2 | lettura | 1.14245 ± 0.00359664 | 1.14125 ± 0.00265934 | 1.05454 ± 0.00352668 | 1.05287 ± 0.00397063 | 1.14245 ± 0.00359664 | 1.14125 ± 0.00265934 | 0.992409 ± 0.00353161 | 0.994564 ± 0.00354803 |
| 3 | fotonico | 1.09558 ± 0.00288355 | 1.09224 ± 0.00271509 | 1.03795 ± 0.00336535 | 1.03628 ± 0.002958 | 1.09558 ± 0.00288355 | 1.09224 ± 0.00271509 | 0.995376 ± 0.00332452 | 0.994034 ± 0.00312367 |
| 3 | lettura | 1.40769 ± 0.00360596 | 1.3889 ± 0.00498738 | 1.14744 ± 0.00259078 | 1.14322 ± 0.00433076 | 1.40769 ± 0.00360596 | 1.3889 ± 0.00498738 | 0.990159 ± 0.00236069 | 0.991656 ± 0.00369486 |
| 4 | fotonico | 1.15393 ± 0.00254827 | 1.15607 ± 0.00387954 | 1.0636 ± 0.00279579 | 1.06349 ± 0.00285652 | 1.15393 ± 0.00254827 | 1.15607 ± 0.00387954 | 0.995568 ± 0.00267701 | 0.993396 ± 0.0025704 |
| 4 | lettura | 1.71585 ± 0.00364417 | 1.68072 ± 0.00531115 | 1.25105 ± 0.00239947 | 1.24532 ± 0.00457845 | 1.71585 ± 0.00364417 | 1.68072 ± 0.00531115 | 0.984262 ± 0.00234593 | 0.986515 ± 0.00273146 |

`n=31`:

| rapporto | regime | median g=1/2 | median g=3/2 | winsorized g=1 g=1/2 | winsorized g=1 g=3/2 | winsorized g=15 g=1/2 | winsorized g=15 g=3/2 | mean + sigma_clip k=3 g=1/2 | mean + sigma_clip k=3 g=3/2 |
|---|---|---|---|---|---|---|---|---|---|
| 1 | fotonico | 1.00301 ± 0.00280127 | 1.00157 ± 0.00278962 | 1.00497 ± 0.0024283 | 1.00322 ± 0.00285883 | 1.00301 ± 0.00280127 | 1.00157 ± 0.00278962 | 0.980785 ± 0.00233534 | 0.978966 ± 0.00272578 |
| 1 | lettura | 1.0002 ± 0.00302743 | 1.00008 ± 0.00373795 | 0.993898 ± 0.00331471 | 0.999851 ± 0.0031966 | 1.0002 ± 0.00302743 | 1.00008 ± 0.00373795 | 0.970935 ± 0.00310976 | 0.976202 ± 0.00301199 |
| 1.5 | fotonico | 1.01495 ± 0.00320161 | 1.00783 ± 0.00358038 | 0.997957 ± 0.00332596 | 0.996832 ± 0.00328724 | 1.01495 ± 0.00320161 | 1.00783 ± 0.00358038 | 0.972055 ± 0.00326832 | 0.97015 ± 0.00342213 |
| 1.5 | lettura | 1.047 ± 0.00303554 | 1.03721 ± 0.00334053 | 1.00508 ± 0.00272793 | 0.999527 ± 0.00282792 | 1.047 ± 0.00303554 | 1.03721 ± 0.00334053 | 0.969501 ± 0.00263093 | 0.965313 ± 0.00281236 |
| 2 | fotonico | 1.03215 ± 0.00257833 | 1.0348 ± 0.00379021 | 1.00458 ± 0.00187036 | 1.00661 ± 0.00444893 | 1.03215 ± 0.00257833 | 1.0348 ± 0.00379021 | 0.971623 ± 0.00180474 | 0.974096 ± 0.00436798 |
| 2 | lettura | 1.13291 ± 0.00243882 | 1.12532 ± 0.00431633 | 1.01303 ± 0.00187283 | 1.01185 ± 0.00220694 | 1.13291 ± 0.00243882 | 1.12532 ± 0.00431633 | 0.958761 ± 0.00193468 | 0.957912 ± 0.0020259 |
| 3 | fotonico | 1.08014 ± 0.00212703 | 1.07745 ± 0.00258054 | 1.01171 ± 0.00253335 | 1.01283 ± 0.0032051 | 1.08014 ± 0.00212703 | 1.07745 ± 0.00258054 | 0.966525 ± 0.00247395 | 0.967474 ± 0.00307745 |
| 3 | lettura | 1.34925 ± 0.0034179 | 1.32886 ± 0.0033639 | 1.02753 ± 0.00300829 | 1.02869 ± 0.00315842 | 1.34925 ± 0.0034179 | 1.32886 ± 0.0033639 | 0.931388 ± 0.0029521 | 0.933606 ± 0.00272896 |
| 4 | fotonico | 1.12499 ± 0.00206015 | 1.12471 ± 0.0035588 | 1.01223 ± 0.00208111 | 1.01541 ± 0.00255815 | 1.12499 ± 0.00206015 | 1.12471 ± 0.0035588 | 0.954165 ± 0.00193576 | 0.955843 ± 0.00212283 |
| 4 | lettura | 1.58539 ± 0.00417004 | 1.55661 ± 0.00543189 | 1.0447 ± 0.00213975 | 1.04161 ± 0.00297677 | 1.58539 ± 0.00417004 | 1.55661 ± 0.00543189 | 0.911422 ± 0.00125828 | 0.91225 ± 0.00254695 |

**T-M3B-C02-02** (casi A–E del test, `n = 11, 15, 31`, 1024 pixel; fonte: `MasterB6.T_M3B_C02_02_{A…E}_*`, `test_master_b6.cpp:245,532`; la riesecuzione dà le stesse righe del run per `n = 11` e `15`). In tutte le 15 celle sono 0 i pixel decisi coi razionali esatti, gli esclusi per intervalli sovrapposti, gli esclusi per la regola I e gli intervalli non separati:

| Caso | n | pixel | decisi coi razionali esatti | esclusi per intervalli sovrapposti | esclusi per la regola I | intervalli non separati | `support_incomplete` | validi con rifiuti |
|---|---|---|---|---|---|---|---|---|
| A una iterazione | 11 | 1024 | 0 | 0 | 0 | 0 | 0 | 0 |
| A una iterazione | 15 | 1024 | 0 | 0 | 0 | 0 | 0 | 394 |
| A una iterazione | 31 | 1024 | 0 | 0 | 0 | 0 | 0 | 736 |
| B più iterazioni | 11 | 1024 | 0 | 0 | 0 | 0 | 0 | 0 |
| B più iterazioni | 15 | 1024 | 0 | 0 | 0 | 0 | 0 | 394 |
| B più iterazioni | 31 | 1024 | 0 | 0 | 0 | 0 | 0 | 736 |
| C asimmetrica κl=2.5 κh=4 | 11 | 1024 | 0 | 0 | 0 | 0 | 0 | 130 |
| C asimmetrica κl=2.5 κh=4 | 15 | 1024 | 0 | 0 | 0 | 0 | 0 | 298 |
| C asimmetrica κl=2.5 κh=4 | 31 | 1024 | 0 | 0 | 0 | 0 | 0 | 662 |
| D asimmetrica κl=4 κh=2.5 | 11 | 1024 | 0 | 0 | 0 | 0 | 0 | 292 |
| D asimmetrica κl=4 κh=2.5 | 15 | 1024 | 0 | 0 | 0 | 0 | 0 | 525 |
| D asimmetrica κl=4 κh=2.5 | 31 | 1024 | 0 | 0 | 0 | 0 | 0 | 909 |
| E clip_min_kept raggiunto | 11 | 1024 | 0 | 0 | 0 | 0 | 597 | 417 |
| E clip_min_kept raggiunto | 15 | 1024 | 0 | 0 | 0 | 0 | 601 | 421 |
| E clip_min_kept raggiunto | 31 | 1024 | 0 | 0 | 0 | 0 | 892 | 132 |

**T-M3B-C02-04** (`K`, `R` come stampati; `(a)` media di V nell'intervallo, `(b)` media X contro E con semiampiezza; `esclusi` e `fuori` 0 in tutte le celle):

| Test | K | R | metodo | (a) media V [intervallo] | (b) media X | E | semiampiezza | margine |
|---|---|---|---|---|---|---|---|---|
| n10 (n 10) | 3400 | 100 | median | 2.21264312 [2.208997798, 2.21745322] | 2.214083965 | 2.213222973 | 0.145930551 | 7.722016218e-12 |
| n10 (n 10) | 3400 | 100 | winsorized g=1 | 1.66942841 [1.666678032, 1.673057607] | 1.669767457 | 1.669865906 | 0.1096602803 | 1.982038288e-11 |
| n10 (n 10) | 3400 | 100 | winsorized g=2 | 1.780845203 [1.777911266, 1.78471661] | 1.781756261 | 1.781311897 | 0.1170823253 | 2.240285248e-11 |
| n10 (n 10) | 3400 | 100 | winsorized g=4 | 2.21264312 [2.208997798, 2.21745322] | 2.214083965 | 2.213222973 | 0.1459305511 | 2.471562949e-11 |
| n101 (n 101) | 256 | 8 | winsorized g=1 | 0.1587465136 [0.1586649277, 0.1588468816] | 0.1587129083 | 0.1587558883 | 0.03797574687 | 4.548161376e-11 |
| n101 (n 101) | 256 | 8 | winsorized g=2 | 0.1591627352 [0.1590809355, 0.1592633664] | 0.1591146317 | 0.1591721345 | 0.03807541452 | 4.546184805e-11 |
| n3 (n 3) | 2250 | 250 | median | 7.182212526 [7.149685294, 7.207864071] | 7.180610917 | 7.178737673 | 0.5842390776 | 0 |
| n3 (n 3) | 2250 | 250 | winsorized g=1 | 7.182212526 [7.149685294, 7.207864071] | 7.180610917 | 7.178737673 | 0.5842390776 | 1.858601762e-11 |
| n4 (n 4) | 2000 | 250 | median | 4.768248253 [4.755424446, 4.786996141] | 4.770965495 | 4.771193895 | 0.4102759029 | 1.12899877e-11 |
| n4 (n 4) | 2000 | 250 | winsorized g=1 | 4.768248253 [4.755424446, 4.786996141] | 4.770965495 | 4.771193895 | 0.410275903 | 1.834623001e-11 |
| n9_median (n 9) | 24300 | 108 | median | 2.657563318 [2.652239377, 2.663008477] | 2.657595863 | 2.657620502 | 0.06562915098 | 0 |
| n9_winsorized_g1 (n 9) | 24300 | 108 | winsorized g=1 | 1.868343324 [1.86460044, 1.872171427] | 1.868173539 | 1.868383525 | 0.04590224783 | 2.324628099e-11 |
| n9_winsorized_g2 (n 9) | 24300 | 108 | winsorized g=2 | 2.017959839 [2.013917226, 2.022094495] | 2.017842655 | 2.01800326 | 0.04963658505 | 2.259984366e-11 |
| n9_winsorized_g4 (n 9) | 24300 | 108 | winsorized g=4 | 2.657563318 [2.652239377, 2.663008477] | 2.657595863 | 2.657620502 | 0.06562915101 | 2.575788478e-11 |

**T-M3B-C02-03**:

| Test | R | esclusi | V fuori | M fuori | (a) media V | E | semiampiezza | max tau | (b) sd | intervallo sd | nu |
|---|---|---|---|---|---|---|---|---|---|---|---|
| weighted_mean_W1 | 200 | 0 | 0/8192000 | 0/8192000 | 10.93643985 | 10.9453125 | 0.3115668861 | 3.445910224e-11 | 3.308917908 | [3.305376315, 3.311364453] | 8187904 |
| weighted_mean_W2 | 250 | 0 | 0/8192000 | 0/8192000 | 1.678740566 | 1.6796875 | 0.04330107749 | 1.57949244e-11 | 1.296076516 | [1.294854806, 1.297200612] | 8187904 |

**T-M3B-FLAT-03** (righe come stampate):

```
T-M3B-FLAT-03 classe 0 R 50 N 5460000 esclusi 0 V fuori 0, M fuori 0
  (a) media V 207.2601763 E 207.2222222 semiampiezza 10.235864 (max tau 2.291302703e-10)
  (b) sd 14.39289834 in [14.37765624, 14.41277966] nu 5458635
  (c) media M − centro -0.001576552483 semiampiezza 0.02483290743 (max tau 2.752909012e-12)
T-M3B-FLAT-03 classe 1 R 50 N 5460000 esclusi 0 V fuori 0, M fuori 0
  (a) media V 260.3012271 E 260.3185185 semiampiezza 12.72126352 (max tau 2.774291907e-10)
  (b) sd 14.39594487 in [14.37765624, 14.41277966] nu 5458635
  (c) media M − centro -0.004173713199 semiampiezza 0.02483290743 (max tau 2.830735646e-12)
T-M3B-FLAT-03 classe 2 R 50 N 5460000 esclusi 0 V fuori 0, M fuori 0
  (a) media V 260.3101575 E 260.3185185 semiampiezza 12.72126352 (max tau 2.456910231e-10)
  (b) sd 14.39200479 in [14.37765624, 14.41277966] nu 5458635
  (c) media M − centro -0.0008333258131 semiampiezza 0.02483290743 (max tau 2.666422638e-12)
T-M3B-FLAT-03 classe 3 R 50 N 5460000 esclusi 0 V fuori 0, M fuori 0
  (a) media V 607.4272835 E 607.2222222 semiampiezza 29.93621658 (max tau 1.05452802e-09)
  (b) sd 24.64376061 in [24.61182173, 24.67194637] nu 5458635
  (c) media M − centro -0.01716024714 semiampiezza 0.04250922964 (max tau 7.047029627e-12)
T-M3B-FLAT-03 classe 4 R 50 N 5460000 esclusi 0 V fuori 0, M fuori 0
  (a) media V 660.2612979 E 660.3185185 semiampiezza 32.59746508 (max tau 1.183268594e-09)
  (b) sd 24.63800181 in [24.61182173, 24.67194637] nu 5458635
  (c) media M − centro -0.008421726736 semiampiezza 0.04250922964 (max tau 7.117417766e-12)
T-M3B-FLAT-03 classe 5 R 50 N 5460000 esclusi 0 V fuori 0, M fuori 0
  (a) media V 660.1728782 E 660.3185185 semiampiezza 32.59746508 (max tau 1.024234919e-09)
  (b) sd 24.65882519 in [24.61182173, 24.67194637] nu 5458635
  (c) media M − centro -0.005847319075 semiampiezza 0.04250922964 (max tau 6.962874721e-12)
T-M3B-FLAT-03 durata 38.08213314 s
```

**T-M3B-FLAT-04** (`U`, `D`: conteggi sopra e sotto; esclusi, indeterminati, `support_incomplete` e M fuori 0 in tutte le righe):

| Test | variante | N | U | D | soglia | max tau |
|---|---|---|---|---|---|---|
| n=11 | sigma_clip (a) | 1364000 | 681548 | 682452 | 684240 | 1.39211e-11 |
| n=3 | mediana (a) | 1364000 | 681623 | 682377 | 684240 | 4.66294e-12 |
| n=3 | mediana (b) | 2728000 | 571934 | 571174 | 1367168 | 4.72622e-12 |
| n=31 | sigma_clip (a) | 1364000 | 682137 | 681863 | 684240 | 3.01419e-11 |
| n=9 | mediana (a) | 1364000 | 682541 | 681459 | 684240 | 8.29847e-12 |
| n=9 | mediana (b) | 2728000 | 334473 | 334356 | 1367168 | 8.44347e-12 |
| n=9 | winsorized g=1 (a) | 1364000 | 681843 | 682157 | 684240 | 1.31148e-11 |
| n=9 | winsorized g=1 (b) | 2728000 | 238315 | 238837 | 1367168 | 1.32634e-11 |


## Punto 4: T-M3A-CAL-07, T-M3A-CAL-09 (master), riesecuzioni

| Test | Prova attesa | Esito |
|---|---|---|
| T-M3A-CAL-07 | `MasterB15a.T_M3A_CAL_07_*` verde | PASS: 8 test, passati in dev-full e in release |
| T-M3A-CAL-09 parte master | `MasterB16.T_M3A_CAL_09_*` e `VerifySelection.T_M3A_CAL_09_*` verdi | PASS: 9 test (`MasterB16` 5, `VerifySelection` 4), passati in dev-full e in release |
| T-M2R2-01…09 | rieseguiti, verdi | PASS: 20 test in dev-full e in release, tutti passati (01: 3, 02: 1, 03: 1, 04: 1 ctest slow `T-M2R2-04.monte_carlo`, 05: 4, 06: 2, 07: 4, 08: 3, 09: 1 ctest slow `T-M2R2-09.real_palomar_product_masters`); nessun ID senza test |
| T-M2R2R-01…05 | rieseguiti, verdi | PASS: 8 test in dev-full e in release, tutti passati (01: 3, 02: 2, 03: 1, 04: 1, 05: 1); nessun ID senza test |
| T-M2F-02 | `T-M2F-02.m2e_network` verde (nessuna syscall di rete sul percorso) | PASS: 1 test, passato in dev-full (0 s) e in release |

## Punto 5: T-FBK-01 parte M3.b

Prova attesa: ogni guasto si inietta sul percorso del prodotto e nessun percorso alternativo parte. T-M3B-C02-15 (`MasterB3`, `MasterB5`, `MasterB6`, `MasterB11`, `MasterB12`, `MasterB16`, `MasterKappa`; frame modificato, `support_incomplete`) e T-M3B-FLAT-06 (`MasterB3`, `MasterB8`, `MasterB9`, `MasterB16`); il guasto «stato del decoder diverso» è provato da `MasterB16.T_M3A_CAL_09_*` (nessun test porta il nome C02-15). Esito: PASS, tutti passati in dev-full e in release. T-M3B-C02-15: 19 test (`MasterB3` 1, `MasterB5` 4, `MasterB6` 1, `MasterB11` 4, `MasterB12` 4, `MasterB16` 3, `MasterKappa` 2). T-M3B-FLAT-06: 30 test (`MasterB3` 2, `MasterB8` 22, `MasterB9` 3, `MasterB16` 3). `MasterB16.T_M3A_CAL_09_*`: 5 test (stato del decoder diverso). T-FBK-01: 3 test con l'ID nel nome (parte M2 e altre milestone comprese). Stessi conteggi in dev-full e in release.

## Punto 6: dato reale PUB-LFC-01

Prova attesa: T-M3B-C02-16 (`T-M3B-C02-16.analysis_{bias,dark,dark_flat,flat}`, `T-M3B-C02-16.masters_{bias,dark,dark_flat,flat}`) e T-M3B-FLAT-09 (`T-M3B-FLAT-09.band_{g,i}`) eseguiti, non saltati (il test fallisce se il dato manca; slow, seriali). Record: `M3b-B17.md`. Senza questo punto il gate resta aperto. Esito: PASS, eseguiti e non saltati: gli 8 ctest di T-M3B-C02-16 e i 2 di T-M3B-FLAT-09 sono presenti nei junit con `status="run"` e senza `skipped`, passati in dev-full e in release. Durate in dev-full (s): `analysis_bias` 33, `analysis_dark` 14, `analysis_dark_flat` 25, `analysis_flat` 33, `masters_bias` 135, `masters_dark` 83, `masters_dark_flat` 149, `masters_flat` 144; `band_g` 73, `band_i` 84.

## Punto 7: tracciabilità e protocollo

- `python3 -S tools/trace.py check` pulito per F-CAL-01, F-CAL-01.b e M3.b su `build/dev` (junit `dev-full`) e su `build/release`: blocco generato in fondo. La mappa `tests/trace/M3b.tsv` è stata completata dopo il run (giro 1 della review del gate): alle righe `F-CAL-01` e `F-CAL-01.b` sono stati aggiunti i token T-M3B-C02-05, T-M3B-C02-11, T-M3B-FLAT-07, T-M3B-MEM-01, T-M3B-ANA-01, T-M3B-CFA-01, T-M3B-JOB-01 e le istanze di conformità `master_combine_{unica,dark_mediana,flat_media,raw_dark_mediana}` (68 test: 17 x 4 varianti); a `C-02.b` T-M3B-C02-05, T-M3B-C02-11, T-M3B-FLAT-07 e le istanze di conformità. Ogni token ha test presenti e passati in entrambi i junit del run (C02-05 9, C02-11 27, FLAT-07 14, MEM-01 12, ANA-01 3, CFA-01 17, JOB-01 10, conformità 17 per variante); la mappa non entra in build né in ctest. Esito della tracciabilità sul run, mappa originale: `python3 -S tools/trace.py check --stage M3.b --map tests/trace/M3b.tsv --build-dir build/dev --ctest-junit tmp/dev-full-junit.xml` uscita 0, «trace: 3 ID nello stadio M3.b, 0 non coperti»; lo stesso su `build/release` col junit release, uscita 0, stesso esito (log `tmp/gate/06-trace-dev.log`, `06-trace-release.log`; eseguiti in `m3b-b16` sul commit `45fc9a9`). I 3 ID dello stadio sono proprio `F-CAL-01`, `F-CAL-01.b` e `M3.b` (`python3 -S tools/trace.py ids --stage M3.b`): il controllo li copre tutti. Con la mappa completata `trace.py check` è stato rilanciato sui due junit del run (`build/dev` con `tmp/dev-full-junit.xml`, `build/release` con `tmp/release-junit.xml`, build pulite di questo worktree): uscita 0 e «3 ID nello stadio M3.b, 0 non coperti» in entrambi.
- Protocollo `docs/evidence/M3b-protocollo.md` committato prima del primo record di misura (T-M3-ADR09-01): primo commit del protocollo `f24d1bb` (9 novembre 2026, 07:54), anteriore al primo commit di codice o di test dopo le unità documentali (B2 `5e413c3`, 09:04) e quindi a ogni record di misura (i test statistici sono in B5, `483b671`). Verifica sulla storia di git ripetuta al gate (`git log --diff-filter=A -- docs/evidence/M3b-protocollo.md`, `git merge-base --is-ancestor`): `f24d1bb` (2026-11-09 07:54:15 +0100, versione 1, SHA-256 `83e300300952752e049536db3c3d792db83860ebddfafe8a851f24ae6e86fe00`) è antenato di `5e413c3` (2026-11-09 09:04:31) e di `483b671` (B5, 2026-11-09 15:05:17). La versione in vigore è la 3, entrata con `483b671` (stesso commit dei test statistici di B5; emendamenti: v2 `τ_k` come ampiezza dell'intervallo e criterio di inclusione, v3 nota sull'implementazione della regola I nell'oracle, criteri e valori invariati, versione 2 SHA-256 `4b06acb685f79ce9599037efb5b70e82b11fc5a6cf563686fff118c67238e8a9`): SHA-256 della v3 `e21760c7f68590d71bccee78088f1577ea0a1bd7e337bc758c3a4aa7f3e1ffc7` (`sha256sum docs/evidence/M3b-protocollo.md`; `git log` del file mostra solo `f24d1bb` e `483b671`, nessuna modifica successiva). Il primo record di misura è in `45fc9a9`, dopo la v3; PASS.

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
| B7 | `M3b-B7.md` | review PASS al giro 1 e al giro 2 |
| B8 | `M3b-B8.md` | review PASS al giro 2 |
| B9 | `M3b-B9.md` | review PASS al giro 2 |
| B10 | `M3b-B10.md` | review PASS al giro 1 e al giro 2 |
| B11 | `M3b-B11.md` | review PASS al giro 2 |
| B12 | `M3b-B12.md` | review PASS al giro 2 |
| B13 | `M3b-B13.md` | review PASS al giro 2 |
| B14 | `M3b-B14.md` | giro 2 FAIL con 1 MED sul piano (difetto del piano, non del codice); arbitraggio del coordinatore, piano corretto, nessun terzo giro |
| B15a | `M3b-B15a.md` | review PASS al giro 2 (Claude Opus; Argon senza esito) |
| B15 | `M3b-B15.md` | review PASS al giro 2 |
| B16 | `M3b-B16.md` | review PASS al giro 2 |
| B17 | `M3b-B17.md` | review PASS al giro 2 |

Build pulita dev e release, eseguita dal coordinatore in questo worktree sul commit `71c25a47a0a9a4b66f9ffe45457815aab42eba79` (`tmp/gate/clean-build.sh`; commit e orari in `tmp/gate/10-clean-commit.txt`, 2026-11-11 22:27:02-22:45:50; esiti in `tmp/gate/rc-clean.txt`; nessuna `build/` preesistente, cache ccache nuova):

| Passo | rc | Durata | Log |
|---|---|---|---|
| build dev (276 oggetti) | 0 | 495 s | `11-clean-build-dev.log` |
| configurazione release | 0 | 5 s | `12-clean-conf-release.log` |
| build release | 0 | 394 s | `13-clean-build-release.log` |
| `ctest --preset release -L fast` | 0, 1772/1772 passati | 233 s | `14-clean-release-fast.log` |
| `tools/check.sh --step labels` | 0 (1829 = 1772 fast + 57 slow) | 0 s | `15-labels.log` |
| `tools/check.sh --step lint` | 0 | 1 s | `16-lint.log` |

Fatti sulla build incrementale del run: ha ricompilato i sorgenti che differiscono fra l'albero di B16 e `45fc9a9` (`job_types.cpp`, `test_master_b16.cpp`, più `source_revision.cpp`) e rilinkato 87 eseguibili; `ctest --preset dev-full -N` e `ctest --preset release -N` sulle build pulite elencano gli stessi 1829 nomi dei due junit del run (verificato in questa compilazione, solo elenco). `71c25a4` esiste per il lint su `tools/review.sh`, file che né la build né i test leggono. Sulla build pulita **non è stata rieseguita la suite completa**: il PASS dei test è del run su `45fc9a9`; `71c25a4` differisce da `45fc9a9` solo per `tools/review.sh`. Warning del compilatore nei log (righe con `warning:`: 61 in dev, 41 in release; il coordinatore ne contava 67 e 47 con un altro criterio): vengono da `third_party/` (cfitsio, 36 in entrambi; LibRaw 0.22.3: `losslessjpeg.cpp`, `fuji.cpp`, `ext_preprocess.cpp`, `file_write.cpp`, 5 in entrambi) e, solo in dev (20 righe), dall'header di sistema libstdc++ `bits/std_function.h`, istanziato dall'uso di `std::regex` in `src/core/resources.cpp`: l'ultima non è un warning su codice del progetto, ma è innescata da un suo sorgente. Nessun warning ha come posizione un file di `src/` del progetto.

Controllo di equivalenza sulla build pulita (commit `71c25a4`, 11/11/2026 23:23 - 12/11/2026 00:12, script `tmp/gate/shards.sh`, esiti `tmp/gate/rc-shards.txt`), indicato dalla review del gate: `tools/check.sh --step test --shard K/6` per K = 1…6 (preset dev, sanitizer): rc=0 in tutti e sei, 297 + 297 + 295 + 296 + 296 + 296 = 1777 passati su 1777, nessun non passato (junit `tmp/dev-junit.K-of-6.xml`, uniti in `tmp/dev-junit.xml`); `ctest --preset release -L slow -E 'T-M3A-RAW-10|T-U12-02'`: rc=0, 46 passati su 46, nessun non passato, 1334 s (junit `tmp/gate/clean-release-slow-junit.xml`). È una diagnosi di equivalenza fra i binari della build pulita e quelli del run, non il PASS: il PASS resta il run unico su `45fc9a9`. Non rieseguiti sulla build pulita: gli slow del preset dev-full e i due slow esclusi (`T-M3A-RAW-10`, `T-U12-02`).

Review del gate (revisore separato): giro 1 FAIL con 3 MED e 15 LOW (`/opt/workspace/astroProcessor/.omc/artifacts/ask/review-M3b-gate-round1-claude-20261111.md`), PASS dei test confermato sulle fonti; un solo giro di correzioni su record e mappa; giro 2 PASS, 0 BLOCCANTE, 0 MED, 3 LOW di testo chiusi in questa versione (`/opt/workspace/astroProcessor/.omc/artifacts/ask/review-M3b-gate-round2-claude-20261111.md`). Il delta `3d2c6f2` (LOW residui dopo il PASS di B16) è stato letto dalla review del gate senza rilievi. Esito del giro 2: PASS, 2026-11-11. Il controllo di equivalenza qui sopra è finito dopo il giro 2 ed è riportato dal coordinatore: nessun test non passato.

## Limiti

- **Lettore RAW delle statistiche** (`plans/M3b.md` §9): dal lettore non si provocano lo stop `raw.libraw_error` né il cancel durante la decodifica (nessun gancio verso il decoder); la classificazione è provata sulla funzione pura `raw_decode_error_is_file_outcome` (B15; `docs/decisions/M3b-lacune.md`, «Decisioni successive (B16)»). Il lettore è provato con gli esiti di file `raw.data_error`, `raw.file_unsupported`, `raw.not_cfa`, `raw.max_pixels` e con digest, `input.missing`, budget e cancel prima e durante la lettura verificata.
- Byte del file letto (FITS e RAW) fuori dal pool fra la lettura verificata e la prenotazione (B15, B16): da chiudere con la lettura verificata prenotata nel pool, in un'unità che tocchi quel confine. File RAW letti due volte nel piano (B16).
- Voce `raw_dark_mediana`: la factory decodifica fuori dal pool e senza cancel; è la voce minima di conformità, non il percorso del prodotto (B16).
- Dato reale (B17): N = 3 per ruolo (7 per i bias), mono, nessun CFA, un solo filtro per flat; `A` è una stima conservativa da 3 frame; un pixel saturo per banda è escluso da `A` e invalido nei master del flat. La parte CFA e RAW è coperta dai test sintetici di B10, B15, B16; il confronto fra backend (ADR-07) è fuori da M3.b.
- Limiti dichiarati dai contratti (C-02.b §b.14, C-02.b-flat §f.14): piano dei metodi robusti non distorto solo sotto `iid_gaussian` e campioni continui; selezione congelata di `sigma_clip` senza garanzia; `scale_fixed` non derivata; il nero del decoder è sentinella dell'offset. Il report li dichiara come ipotesi non verificate.
- Chiavi dei nodi: `master_algorithm_version` nuova (B3, B7) rende `stale` i master `.a` già prodotti (`plans/M3b.md` §11).
- Mutazioni: i record delle unità riportano i conteggi (B3 12/12, B3b 5/5, B4 5/5, B6 7/7, B7 11/11, B8 15/15, B9 11/11, B10 5/5, B11 8/8 + 4/4, B12 15/15, B13 12/12, B14 14/14, B15a 16/16, B15 mutazioni del giro 1 tutte rilevate, conteggio nel suo record); B16 7/7 (giro 1, release, `tools/mutate.sh` su `^MasterB16\.`) e 3/3 (giro 2), da `tmp/review-facts-M3b-B16.md` e `tmp/review-facts-M3b-B16-r2.md` del progetto (riportati ora anche nel record `M3b-B16.md`, corretto nel giro di correzioni del gate); B5 e B17: non riportati nelle fonti, debito dichiarato.
- I limiti e i LOW residui di ogni unità restano validi come dichiarati nel suo record (`docs/evidence/M3b-B*.md`).
- Dato reale RAW: PUB-RAW-01 non è letto da nessun test di M3.b (`test_master_b*`); la chiave dei gruppi dei master con modelli diversi e lo stop di X-Trans (`plans/M3b.md:317`) sono provati solo dai test di M3.a e da RAW sintetici, non su RAW reali.
- Dipendenza dal carico dei due test sigkill di recupero (da 51-76 s a 1084-1111 s): al prossimo gate esecuzione seriale o run senza deleghe concorrenti, non un timeout più alto (debito).
- Rinviati: nessuno aperto dal run. Fuori dal gate restano i debiti già dichiarati sopra (lettura verificata prenotata nel pool, `raw_dark_mediana`, mutazioni di B5 e B17 non riportate, dipendenza dal carico dei due test sigkill di recupero).
- Stime dei test di caratterizzazione: nel junit del run le uscite sono troncate a 1024 byte; le stime complete vengono dalla riesecuzione diagnostica su `71c25a4` (stesso codice di `45fc9a9`), non parte del PASS. T-M3B-C02-05 non stampa stime; il numero dei casi con intervalli sovrapposti tolti è stampato dai test di T-M3B-C02-02 (Punto 3), non da C02-06. Il «identico in dev-full e in release» delle uscite troncate è provato solo sul prefisso di 1024 byte; al prossimo gate `--test-output-size-passed` va messo in `run.sh`, così le stime stanno nel junit del PASS.
- Il run del PASS è incrementale sopra la build di B16 (3 unità compilate, 87 link). La build pulita dev+release su `71c25a4` è stata fatta (Punto 8), ma su di essa non è stata rieseguita la suite completa, solo `-L fast` (1772/1772), i 39 test delle riesecuzioni diagnostiche (Punto 3) e il controllo di equivalenza del Punto 8.
- Carico della macchina: durante il run altre deleghe hanno fatto build leggere sulla stessa macchina; verso le 19:45 la pressione di I/O ha portato due test di recupero a 1084 s e 1111 s su un limite di 1500 s (il timeout predefinito di ctest per `test_recovery`, che non ha `TIMEOUT`: `tests/unit/CMakeLists.txt:32`; altri test lo impostano) (passati, nessun rilancio). Nel junit sono `Recovery.T_M2R4_01_sigkill_at_every_point_then_explicit_resume_reuses_only_safe_phases_and_matches_the_oracle` (1084 s) e `Recovery.T_M2R4_01_sigkill_at_every_point_of_a_workflow_of_import_and_calibration_steps_then_resume_matches_the_oracle` (1111 s), e sono del run **release** (nel dev-full durano 76 s e 61 s). Il ctest release è partito alle 19:37:11. Questi tempi dipendono dal carico: non sono un margine garantito.

## Esiti del run

| Voce | Valore |
|---|---|
| Comando | `tmp/gate/run.sh`: `ctest --preset dev-full --output-junit tmp/dev-full-junit.xml` e `ctest --preset release --output-junit tmp/release-junit.xml`, dopo `tools/check.sh --step build`, `cmake --preset release`, `cmake --build --preset release` (incrementali) |
| Commit | `45fc9a90b8b9010e51af6180e059c8ce9df85ce9` (ramo `m3b-gate` = questo commit + `71c25a4` + documenti e una riga di commento in `tools/model-routing.tsv`) |
| Passati/totali | dev-full 1829/1829 (fast 1772, slow 57; 15110 s); release 1829/1829 (fast 1772, slow 57; 10040 s) |
| Non passati | nessuno (0 fallimenti, 0 timeout, 0 saltati, 0 non eseguiti, in entrambi i preset) |
| Carico durante il run | Altre deleghe hanno fatto build leggere sulla stessa macchina. Verso le 19:45 la pressione di I/O ha portato due test di recupero (`Recovery.T_M2R4_01_sigkill_...`, nel run release) a 1084 s e 1111 s su un limite di 1500 s (timeout predefinito di ctest: `test_recovery` non ha `TIMEOUT`, `tests/unit/CMakeLists.txt:32`): passati, nessun rilancio |
| Trace | `tools/trace.py check --stage M3.b`: 3 ID, 0 non coperti, uscita 0 su `build/dev` e `build/release` |
| Build pulita | Fatta su `71c25a4`: dev rc 0, release rc 0, `ctest -L fast` 1772/1772, labels e lint ok; suite completa non rieseguita (Punto 8) |
| Stime complete | Riesecuzione diagnostica, 39/39 passati in tre comandi (25 ctest slow, C02-07, 13 test di C02-02), non parte del PASS (Punto 3) |
| Esito del gate | Test: PASS (un solo run completo, stesso commit, ogni test del preset passato). Gate: PASS (review del gate PASS al giro 2) |

Conteggi e hash generati:

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/evidence/M3b-protocollo.md` | `e21760c7f68590d71bccee78088f1577ea0a1bd7e337bc758c3a4aa7f3e1ffc7` |
| `docs/test-matrix.md` | `85529dc78e0dd00ff0a296efc3cec7965e156f930affedd1d0961d51d80477f2` |
| `plans/M3b.md` | `b19c7a29b4dd03752a5d81551ffe4ef56b8ace0094bf8e653c96878809bc3e5e` |
| `tests/trace/M3b.tsv` | `87626fca09bee84a6091e6ca873e177b78ffe0abdd4d71ddd87f02e400824f14` |
| `tools/trace.py` | `ef7185ebc60b152f3b8436c55ef7b94704bfddf33568a9ebf21939702c08f040` |

- Test dev-full (`tmp/dev-full-junit.xml`): totali 1829, fast 1772, slow 57, falliti 0
- Test release (`tmp/release-junit.xml`): totali 1829, fast 1772, slow 57, falliti 0
- Tracciabilità (`tests/trace/M3b.tsv`, build `build/dev`, junit `tmp/dev-full-junit.xml`): trace: 3 ID nello stadio M3.b, 0 non coperti
- Tracciabilità (`tests/trace/M3b.tsv`, build `build/release`, junit `tmp/release-junit.xml`): trace: 3 ID nello stadio M3.b, 0 non coperti
<!-- evidence:end -->
