# M3, base comune, intervento I5: casi che rilevano le mutazioni sopravvissute

Stato: **PASS** (review al giro 2). Unità: M3, base comune, riga «Prove» del §4 di `docs/contracts/BASE-confini.md`. Solo test: nessuna modifica a `src/`; `tests/unit/test_conformance.cpp` non è modificato (SHA-256 nel blocco generato, uguale a `03c2efc`).

| Campo | Valore |
|---|---|
| Unità | M3 / base comune, intervento I5 (test) |
| Scopo | Un caso che passa sul prodotto attuale e fallisce con ciascuna delle nove mutazioni sopravvissute a I2 e I3: `m5` (I2); `m04`, `m06`, `m07`, `m08`, `m09`, `m12`, `m15`, `m18` (I3) |
| Comando | `cmake --preset release`, `cmake --build --preset release`, `ctest --preset release -R Gaps` (log `tmp/basei5-build-release.log`); mutazioni: `tmp/basei5-mut/run.sh` e `run2.sh` (`tools/mutate.sh`, 4 job); controlli: `tmp/b5check.sh` (build e ctest dev e release, `tools/lint.sh`, `trace.py check`) |
| Input | Codice a `03c2efc` (ramo `m3-base`), nessuna modifica di prodotto; patch di I2 e I3 copiate in `tmp/basei5-mut/` (la patch di `m5` è riscritta sul sorgente attuale: `verify_digest` non esiste più) |
| Versioni | GCC 15.2.0, CMake 4.2.3; preset `release` e `dev` (ASan e UBSan) |
| Ambiente | Linux 7.0.0-38-generic x86_64, 16 CPU, worktree isolato senza il collegamento `data`, 4 job |
| Risultato | 9 test ctest nuovi (7 corpi: 4 `TEST` di `HashGaps` e 3 `TEST_P`, di cui `ConformanceGapsIo` in 3 istanze, `ConformanceGapsMaster` e `ConformanceGapsBlocks` in 1), tutti verdi su release e dev, stabili in 25 ripetizioni sul file finale (`ctest --preset release -R Gaps --repeat until-fail:25`, log `tmp/basei5-repeat.log`: 225 esecuzioni passate). Le nove mutazioni sono RILEVATE. Release completo: 617 test, 5 falliti (T-03, di I4, e i 4 test su dati reali, per il prerequisito `data` assente); dev: 611 test, 2 falliti (T-03 e `Fits.REAL_DATA_PUB_LFC_01`) |
| Output | Log e junit sotto `tmp/` citati nel comando |
| Ambito | OB-07 (cancel nei `prepare` dei tipi di I/O e nel `finalize` del master) e OB-11 (header del piano, input aperto invariato, riverifica al percorso). Fuori: ogni altra mutazione non sopravvissuta, gli altri obblighi |
| Esito | Le nove mutazioni sono rilevate; nessuna equivalente tra quelle chieste. Review PASS al giro 2 (vedi sotto) |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Sonnet 5.5), 30 ottobre 2026 |

## Casi nuovi

File `tests/unit/test_conformance_gaps.cpp` (registrato in `tests/unit/CMakeLists.txt`; include `test_conformance.cpp` per il banco di prova, quindi `-Wsubobject-linkage` è spento per quel solo file, e `gtest_discover_tests` filtra `*ConformanceGaps*:HashGaps.*`: i casi di `test_conformance.cpp` non sono duplicati in ctest).

| Test | Obbligo | Cosa isola |
|---|---|---|
| `HashGaps.T_M3BASE_11_each_planned_header_field_alone_stops_the_read` | OB-11 | `read_verified_input` su un file reale con un piano che differisce per un solo campo alla volta: dimensione (`input.size_changed`), tipo, BITPIX, INVALID, VARIANCE (`input.header_changed`), ciascuno col nome del campo nel contesto |
| `HashGaps.T_M3BASE_11_bitpix_change_with_the_same_sample_type_is_header_changed` | OB-11 | piano di un file a 16 bit contro un file -32 della stessa geometria (stesso tipo `f32`): `require_planned_header` ferma su BITPIX |
| `HashGaps.T_M3BASE_11_read_verified_input_stops_a_file_modified_after_the_open` | OB-11 | digest e header attesi sono quelli del contenuto nuovo (un byte di dati cambiato nello stesso inode, mtime spostato con `utimensat`): resta solo `input.modified` del file aperto |
| `HashGaps.T_M3BASE_11_reverify_input_stops_an_opened_input_modified_since_the_open` | OB-11 | il percorso riaperto è invariato e ha il digest atteso, l'input aperto prima è cambiato: `input.modified` |
| `ConformanceGapsIo.T_M3BASE_07_prepare_cancel_in_a_multi_block_input` (import, miniatura, ricostruzione) | OB-07 | input f32 1000x1000 (4 blocchi di digest da 1 MiB): cancel a `sched.prepare` e cancel alla prima lettura del prepare; dopo la richiesta zero letture, zero task, zero `finalize`; esito `cancelled` `sched.cancelled` |
| `ConformanceGapsMaster.T_M3BASE_07_finalize_reverify_stops_at_the_cancel` | OB-07 | cancel a `sched.finalize` e cancel all'apertura del primo input riverificato; dopo la richiesta nessun input riaperto né letto |
| `ConformanceGapsBlocks.T_M3BASE_11_finalize_reverifies_the_digest_of_every_input` (calibrazione `blocchi`) | OB-11 | per ciascuno dei 4 ruoli: un byte riscritto da una mappatura condivisa a `sched.finalize`; esito `integrity` `input.digest_mismatch` |

## Mutazioni

`tools/mutate.sh` sul preset `release` con 4 job: baseline verde sui 9 test, patch, build, `ctest -R Gaps`, ripristino verificato con `cmp` (`tmp/basei5-mut/summary.txt`, `summary-2.txt`; log e patch per mutazione in `tmp/basei5-mut/`).

| Patch | Mutazione | Test che falliscono | Esito |
|---|---|---|---|
| `m5-ob07-io-prepare` | `prepare` dei tipi di I/O senza `check_cancel` e digest senza token | `ConformanceGapsIo` × 3 (letture dopo la richiesta: 4 e 3 invece di 0) | RILEVATA |
| `m04-ob11-header-size` | nessun confronto della dimensione del piano | `HashGaps.T_M3BASE_11_each_planned_header_field…` («nessuno stop per il campo dimensione») | RILEVATA |
| `m06-ob11-header-type` | nessun confronto del tipo | stesso test («campo tipo») | RILEVATA |
| `m07-ob11-header-hdus` | nessun confronto di INVALID e VARIANCE | stesso test («campo invalid» e «campo varianza») | RILEVATA |
| `m18-ob11-header-bitpix` | nessun confronto di BITPIX | stesso test e `…bitpix_change_with_the_same_sample_type…` | RILEVATA |
| `m08-ob11-read-unchanged` | nessun `check_unchanged` in `read_verified_input` | `…read_verified_input_stops_a_file_modified_after_the_open` | RILEVATA |
| `m09-ob11-reverify-opened` | `reverify_input` senza `check_unchanged` dell'input aperto | `…reverify_input_stops_an_opened_input_modified_since_the_open` | RILEVATA |
| `m12-ob11-blocks-reverify` | nessuna riverifica nel `finalize` a blocchi | `ConformanceGapsBlocks` («nessun arresto: esito riuscito») | RILEVATA |
| `m15-ob07-master-finalize-cancel` | nessun `check_cancel` nella riverifica del master | `ConformanceGapsMaster` (3 input riaperti e 3 letti dopo la richiesta, invece di 0) | RILEVATA |

Mutazioni aggiuntive, per mostrare che ogni parte del caso lavora (`summary-2.txt`):

| Patch | Mutazione | Esito |
|---|---|---|
| `m5b-token` | solo il digest dei tipi di I/O senza token (resta il controllo all'ingresso) | RILEVATA |
| `m15a-loop-check` | solo il `check_cancel` per frame tolto (resta il callback del digest) | RILEVATA |
| `m15b-callback` | solo il callback del digest tolto (resta il controllo per frame) | RILEVATA |
| `m5a-entry-check` | solo il `check_cancel` all'ingresso del `prepare` dei tipi di I/O tolto | SOPRAVVISSUTA: equivalente, vedi sotto |

`m5a` non fa parte del debito (nessuna delle nove): sopravvive perché non cambia nulla di osservabile. Con un input non vuoto il digest controlla il token prima di ogni blocco, compreso il primo, e lancia lo stesso stop (stessa categoria e stesso codice `sched.cancelled`, testo del messaggio diverso); con un file vuoto (raggiungibile solo da `io.import`) il ciclo del digest non gira, ma lo scheduler controlla il cancel prima di avviare il task e lancia lo stesso stop (*ipotesi* dalla lettura di `scheduler.cpp`, non misurata: il caso del file vuoto non è stato eseguito con la mutazione). Un test che fissasse il testo del messaggio sarebbe una prova finta e non è scritto.

## Scelte

- **Banco di prova.** I casi di `m5`, `m12` e `m15` usano `type_case`, `probe` e `check_common` di `test_conformance.cpp`, che stanno in un namespace anonimo: il nuovo file lo include (`#include "test_conformance.cpp"`) invece di duplicarlo. Costo: il file si compila due volte; ctest esegue solo i casi nuovi.
- **Input a più blocchi (`m5`).** `replace_input` sostituisce il file del ruolo con un frame f32 1000x1000 e ricostruisce nodo e slot sul nuovo digest. Il blocco di digest è 1 MiB (`digest_reserve_bytes`), quindi 4 blocchi.
- **Cancel nel `finalize` (`m15`).** Il punto `sched.finalize` precede `finalize` e segue il controllo dello scheduler: il cancel richiesto lì arriva a `finalize`. Senza il controllo il master riverifica i 3 frame e il cancel emerge solo dopo (controllo dello scheduler dopo `finalize`): l'esito `cancelled` è lo stesso, per questo l'asserzione è sulle riaperture (`input.open`) e letture (`input.read`) dopo la richiesta.
- **Riverifica al percorso (`m12`).** In residenza a blocchi, con la riverifica tolta, restano `check_identity` (stesso inode al percorso, `check_unchanged` su dimensione, mtime e ctime) e il digest consumato dai task. Un cambio dopo l'ultimo task sfugge solo se non muta dimensione, mtime né ctime: una scrittura da `mmap(MAP_SHARED)` su una pagina già sporca (il test la tocca prima dell'avvio) non aggiorna i timestamp finché la pagina non viene scritta su disco. Con la riverifica presente lo stop è `integrity` `input.digest_mismatch`.
- **File aperto cambiato dopo l'apertura (`m08`, `m09`).** Un byte dei dati a `data_offset` è riscritto nello stesso inode (header e dimensione invariati) e `utimensat` sposta mtime, prima dell'apertura a un'epoca fissa e dopo la modifica di 10 s: il rilevamento non dipende dalla granularità dei timestamp del filesystem.
- **Campi del piano (`m04`, `m06`, `m07`, `m18`).** Il piano del file reale è copiato e un solo campo è cambiato; `read_verified_input` legge il file vero (digest e header uguali al piano), quindi il solo confronto da provare è quello del campo.

## Limiti

- Il caso di `m12` dipende dal comportamento del kernel sui timestamp delle scritture da mappatura condivisa: se una scrittura ne aggiornasse mtime o ctime, lo stop sarebbe `input.modified` da `check_identity` e il test fallirebbe con un codice diverso (non passerebbe in silenzio). Stabile in 25 ripetizioni (`tmp/basei5-repeat.log`) su questo kernel (7.0.0-38); il writeback della pagina fra il tocco e la scrittura lo renderebbe instabile, non falso verde.
- I casi di `m04`, `m06`, `m07`, `m18` costruiscono il piano modificando i campi di `fits_plan`, non con file reali che differiscono solo in quel campo (tranne il caso di BITPIX con un file a 16 bit e uno -32); la prova riguarda il confronto in `require_planned_header` e la sua chiamata da `read_verified_input`, non la lettura dell'header dal file.
- `m09` e `m08` sfruttano il confronto di mtime: valgono nei limiti di `mtime` e `ctime` del filesystem, come il prodotto.
- `tools/check.sh` si ferma al ctest dev per il rosso T-03 e per `Fits.REAL_DATA_PUB_LFC_01` (prerequisito `data` assente); lint e `trace.py check` sono stati lanciati a parte (`tmp/basei5-lint.log`: ok; `tmp/trace-check.txt`: 38 ID non coperti con il junit dev, che non contiene i test `slow` né quelli su dati reali: nessun ID cambia per questa unità, che non ne aggiunge).

## Ricerca di altre copie

Controlli mancanti per classe nella stessa suite: gli altri campi di `require_planned_header` (larghezza, altezza, piani) e `reverify_input` con dimensione diversa sono già rilevati da I3 (`m05`, `m17`, `m19`). I controlli dopo il primo blocco di `calibration_job.cpp`, `calibration_blocks.cpp` (pezzi della P0), anteprima e inventario, citati da I2 come non distinti, non hanno una mutazione sopravvissuta registrata e non sono coperti da questa unità.

## Review

- Giro 1: `tools/review.sh basei5-round1 tmp/basei5-facts-round1.md` (Codex `gpt-6-astra`, sola lettura), artefatto `.omc/artifacts/ask/codex-basei5-round1-20261030-1132.md`: FAIL. [MED] le 25 ripetizioni non erano documentate in un log; [LOW] descrittore non chiuso se `file_size` o `mmap` falliscono in `shared_mapping`; [LOW] i limiti alle righe 111-112 del contratto citano ancora l'assenza di prove sul cancel nel `finalize` e sui digest multiblocco; [LOW] conteggio dei corpi; [LOW] titolo della scelta su `m08` e `m09`.
- Correzioni (delta `tmp/basei5-round2.diff`): log delle ripetizioni sul file finale; chiusura del descrittore nel costruttore; conteggio e titolo. Il LOW sul contratto non è corretto: il perimetro di questa unità sul contratto è la sola riga «Prove» del §4 (le righe 111-112 restano da allineare in un'unità successiva).
- Giro 2: `tools/review.sh basei5-round2 tmp/basei5-facts-round2.md`, artefatto `.omc/artifacts/ask/codex-basei5-round2-20261030-1138.md`: `Verdetto: PASS`.

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/BASE-confini.md` | `573fba6402d89abb5a8df3566792f36a85602519ff6525da755d7d5c0472eccb` |
| `docs/evidence/M3-base-I2.md` | `d6a86a508260ca1051fa82fc6855ad4e3ceef99f9d157a7b25d38367bc4d4306` |
| `docs/evidence/M3-base-I3.md` | `d14d526853d5f8f7b3c7feb43621f458d15dcf5c295a73d92cc443ed7c1f83fc` |
| `src/core/calibration_blocks.cpp` | `4a87981915c214997d1864f602a53b51f636012efd08f97be7f217bb640f1e21` |
| `src/core/hash.cpp` | `f8507547472b5b58b882ae069256530cb1a091b879a373a749afbfde563d0823` |
| `src/core/import_job.cpp` | `c3b73a0b879a25cccf50ab38f5c4ee4806647c807f73bcb79deea05610b1c5ed` |
| `src/core/master.cpp` | `1aba5a2b6688d8d1c4e198b065589283ce62a35e30bf3866933e1c3d4b35aa9b` |
| `tests/unit/CMakeLists.txt` | `11692a0d0102fa9e69f0e241a2c1ca72a8543622c451f34819721dd20946914e` |
| `tests/unit/test_conformance.cpp` | `a75bee61a509be32163cfe862cbb4f25a1a9187a256a4fd7c404c96d6799683b` |
| `tests/unit/test_conformance_gaps.cpp` | `c451f8af129f3194e2824541571ecc2f0cd7f389a9b36de80c855c48df46a9af` |

- Test release (`tmp/release-junit.xml`): totali 633, fast 626, slow 7, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 627, fast 627, slow 0, falliti 0
<!-- evidence:end -->
