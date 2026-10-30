# M2-R4b: passi di import di R1 nel workflow di recovery e cache delle miniature nel test hardlink

Stato: **ESEGUITO, review chiusa (PASS al giro 2)**, 29 ottobre 2026. Giro 1 (`.omc/artifacts/ask/codex-r4b-round1-20261029-2153.md`): FAIL, 2 MED sulla forza dei controlli del test hardlink della cache e 2 LOW, corretti nel delta `tmp/r4b-round2.diff`. Giro 2 (`.omc/artifacts/ask/codex-r4b-round2-20261029-2203.md`): PASS con un LOW (i confronti con lo stato precedente sono dopo i due rifiuti, non dopo ciascuno), lasciato.

ID del registro (`python3 -S tools/trace.py ids --stage M2`): F-PRJ-06 e NFR-SEC-03 (le parti R1 pianificate in `docs/evidence/M2-R4.md`), NFR-DATA-01.a (riesecuzione dopo la chiusura di F-PRJ-06, arco 4031). Test: T-M2R4-01, T-M2R4-04, T-M2R4-05.

## Ambiente

Linux 7.0.0-38-generic x86_64; GCC 15.2.0 (Ubuntu 15.2.0-16ubuntu1); CMake 4.2.3; Ninja 1.13.2; Python 3.14.4; base `a9ebfd1` (`work/m0-m1`) più le modifiche di R4.b non committate. Piano attivo SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`. Build e test con `CMAKE_BUILD_PARALLEL_LEVEL=4`, `CTEST_PARALLEL_LEVEL=4`, `TMPDIR` e `CCACHE_DIR` sotto `tmp/` del worktree. `data` è un link simbolico a `../../../data` del repository principale, in sola lettura, creato nel worktree per i test `REAL_DATA`; nessun dato copiato.

## Stato generato (hash, conteggi, tracciabilità)

Rigenerato con `python3 -S tools/evidence.py docs/evidence/M2-R4b.md` dalla sorgente `docs/evidence/M2-R4b.evidence`.

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-04-persistenza.md` | `83861d7d5db28b2f45b7748a0af91044e2cd56565c3fa9295733c0561163401c` |
| `src/core/import_job.cpp` | `945d3ee4045c7ff15e2292551a5cc34384ef4431b09181cfffe6816c9e0df487` |
| `src/core/thumbnail_cache.cpp` | `732030d91566f407df2125963ec36ce39b0d3476d3d16e1edc69cd8d6905279d` |
| `src/core/workflow.cpp` | `9eaadacfce9fb87b2d5dc4998411efc27d15c4d6084cd48595ae446a50ffa84a` |
| `tests/trace/M2.tsv` | `f559fc60e9888c586cc4917d095f222ffd8fc5d25b0196a67becfa891025bdcc` |
| `tests/unit/test_recovery.cpp` | `884b87b25700b53ff9263a0b20b26139e798ed92d8368866e5ca3c08fee0ccaf` |

- Test release (`tmp/release-junit.xml`): totali 694, fast 687, slow 7, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 688, fast 688, slow 0, falliti 0
- Tracciabilità (`tests/trace/M2.tsv`, build `build/release`, junit `tmp/release-junit.xml`): trace: 63 ID nello stadio M2, 0 non coperti
<!-- evidence:end -->

## Modifiche

Il prodotto non cambia. Cambiano il contratto (ADR-04, sezione «Estensione R4», paragrafo «Integrazione con R1» e righe T-M2R4-01, 04, 05), `tests/unit/test_recovery.cpp`, `tests/trace/M2.tsv` e `docs/test-matrix.md`.

## Test eseguiti

| ID | Test `ctest` | Cosa verifica |
|---|---|---|
| T-M2R4-01 | `Recovery.T_M2R4_01_sigkill_at_every_point_of_a_workflow_of_import_and_calibration_steps_then_resume_matches_the_oracle` | La stessa matrice di T-M2R4-01 (corpo comune `sigkill_matrix`), con 2 passi di `run_import_job` sui primi due light seguiti da 2 calibrazioni. SIGKILL del figlio a ogni punto registrato in una prova a secco, compreso `import.digest.done` (hook del job di import). Dopo la riapertura: nessun job nuovo, fasi sicure del report = passi con `commit.after_commit` raggiunto (3 output per un import, 4 per una calibrazione). Dopo `resume_workflow`: passi sicuri `reused`, una sola provenance e un solo job per nodo (2 per l'interrotto), provenance del resume con la lista `reused` nell'ordine dei passi; digest di report (JSON e testuale) degli import e di immagine e report delle calibrazioni uguali a quelli di un progetto separato eseguito senza kill |
| T-M2R4-04 | `Recovery.T_M2R4_04_simulated_power_loss_in_a_workflow_of_import_and_calibration_steps_keeps_the_project_consistent` | Corpo comune `powerloss_matrix`, con 2 import e 2 calibrazioni su VFS di test: a ogni `xSync`, `xDelete`, `fsync` del file e della directory e dopo la chiusura si costruisce lo stato durevole e si verificano `integrity_check` = `ok`, ogni riga `present` con file e digest, ogni job `completed` con i suoi output (`primary`, `report_text`, `provenance` per un import; più `report` per una calibrazione), ogni slot `current` con job produttore `completed`, nessun file senza riga. Conteggi: 4·K + 3·I `fsync` di file più quello del file riusato (report testuale del primo import), 4·K + 3·I di directory |
| T-M2R4-05 | `Recovery.T_M2R4_05_hardlinks_in_the_thumbnail_cache_stop_as_security_without_writes_or_removals` | Cache `cache/thumbnails/` di R1: (a) originale collegato col nome di una voce: `run_thumbnail_job` e `run_cache_rebuild_job` si fermano `security` `cache.hardlink`, elenco della directory, digest, inode, `nlink`, `mtime` e permessi invariati, nessun calcolo, nessun output pubblicato da `thumb` e `rebuild` (solo la provenance dello stop), directory della cache con `mtime` e `ctime` invariati e nessun punto `cache.entry.*` toccato; (b) originale collegato come `.tmp-*`, accanto a un temporaneo regolare che lo precede nell'ordine: `cleanup_temporaries` `cache.hardlink`, nessuno dei due rimosso; (c) voce calcolata e poi collegata altrove (`nlink` 2): job e rebuild `cache.hardlink`, voce, pubblicazione precedente, contatore dei calcoli e directory invariati |

I test esistenti `Recovery.T_M2R4_01_sigkill_at_every_point_then_explicit_resume_…` e `Recovery.T_M2R4_04_simulated_power_loss_at_every_sync_point_…` ora chiamano gli stessi corpi con zero passi di import: stesse asserzioni di prima. Un'asserzione cambia: la lista `reused` attesa segue l'ordine dei passi (prima coincideva con l'ordine alfabetico degli slot del report); l'ordine dei passi è quello che il prodotto produce.

## Rosso prima del codice

Il prodotto non cambia, quindi non esiste un rosso da codice mancante. La prima esecuzione dei test nuovi (`tmp/r4b-red.log`) è fallita per due difetti dei test, non del prodotto, corretti prima di qualunque altra misura: l'ordine atteso della lista `reused` (alfabetico invece che dei passi) e lo slot del rebuild (il nodo `cache.rebuild` è distinto da quello `thumbnail`, `import.node_mismatch`). Dopo la correzione i test passano sul prodotto di `a9ebfd1`. La capacità di fallire senza la funzione la provano le mutazioni sotto.

## Mutazioni

`tools/mutate.sh` (preset `release`), patch in `tmp/mut/r4b-mNN.patch`, log in `tmp/r4b-mutations-mNN.log`, riepilogo in `tmp/r4b-mutations-summary.log`. Selezione: i soli test `Recovery` interessati (m01, m02, m06: T-M2R4-05; m03: T-M2R4-01 e 04 con import; m04: T-M2R4-01 con import; m05: T-M2R4-04). Prima esecuzione di m05 (`tmp/r4b-mutations-m05-first-survived.log`): SOPRAVVISSUTA, perché toglieva solo la chiamata `fsync` e lasciava l'evento che l'osservatore del test usa per costruire lo stato durevole (il modello è guidato dagli eventi); la patch è stata riscritta togliendo anche l'evento e allora è rilevata. m03 e m04 non provano la durabilità: provano che i passi di import compaiono nella matrice.

| ID | File | Mutazione | Esito |
|---|---|---|---|
| m01 | `thumbnail_cache.cpp` | cache: controllo `st_nlink != 1` tolto da `check` e da `read_entry` | RILEVATA |
| m02 | `thumbnail_cache.cpp` | `cleanup_temporaries` senza `check` dei temporanei | RILEVATA |
| m03 | `import_job.cpp` | il job di import non pubblica il report testuale | RILEVATA |
| m04 | `workflow.cpp` | resume: una fase con meno di 4 output non è riusata (gli import vengono rieseguiti) | RILEVATA |
| m06 | `thumbnail_cache.cpp` | `cleanup_temporaries` controlla ogni temporaneo subito prima di rimuoverlo invece di controllarli tutti prima della prima rimozione | RILEVATA |
| m05 | `sqlite_internal.cpp` | file riusato (`EEXIST`) senza `fsync` e senza l'evento `fsync` verso l'osservatore | RILEVATA (T-M2R4-04, entrambe le esecuzioni: conteggio dei `fsync` e invarianti dello stato durevole) |

## Limiti

- La cache delle miniature applica la propria regola `st_nlink = 1` (`cache.hardlink`, `cache.symlink`, `cache.not_regular`) e non conosce gli inode degli originali: non emette `fs.original_inode`. Un originale collegato nella cache ha `st_nlink ≥ 2` e si ferma come `cache.hardlink`; un file con lo stesso `(st_dev, st_ino)` di un originale e `st_nlink` 1 (originale spostato nella cache) non è coperto né provato (ADR-04, integrazione con R1).
- I passi di import del workflow sono solo i primi due light; il thumbnail e il `cache.rebuild` non sono passi del workflow di recovery (nessun requisito li chiede lì); la cache è provata nel solo test hardlink.
- `Spill.a_failed_allocation_in_resident_bytes_does_not_leak_the_mapping` (fuori perimetro) è fallito una volta nell'esecuzione parallela di `tools/check.sh` (conteggio delle mappature del processo 341 invece di 337) e passa isolato e nelle esecuzioni successive: sensibile al carico, non toccato da R4.b.
