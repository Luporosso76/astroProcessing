# M1.e: validazione reale delle regressioni supervisor

**Esito finale: 18/18 PASS, zero failure/error/skip e zero unraisable.**
Ambito: supervisor con oggetti FakePopen in memoria. Nessun engine, pilot,
kernel GPU, fault hardware, validate GPU, confirm o benchmark comparativo
eseguito. Questo risultato **non è una prova GPU** e non chiude M1.

Modello dichiarato dal runtime: `openai/gpt-6.1-sol`. Nessuna delega. Runtime
osservato nel figlio: Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.
La versione è stata raccolta in questo run, non trasferita dai report precedenti;
il GCC nella stringa è il compilatore di Python, non una nuova build candidata.
Costi/token non esposti. Nessun valore finanziario inventato.

## Correzione del finding

Letto test_followup.py intero, 255 righe precedenti, e la firma effettiva di
require_receipt in run.py. Il test mismatch ora conserva tutte le chiavi del
binding e l'identico `cpu_reference_lock_sha256="unit"`; muta soltanto recipe,
aggiungendo `other_GT=True` e mantenendo `fake_unit_only=True`. Gli assert
verificano esplicitamente chiavi e CPU freeze preservati. La regex `mismatch`
non è stata modificata: viene esercitato il confronto ricetta/GT, non il guard
del binding incompleto.

Nuovo test separato: `test_incomplete_binding_rejected_before_comparison`.
Rimuove soltanto cpu_reference_lock_sha256 e richiede esattamente
`CPU freeze/recipe binding required`, con Popen non chiamato. Nessun gate
produttivo dello spike indebolito: run.py conserva SHA-256
`f1d6bf74e0937773ee11d5315382d9c660de2df96ed98426c993491ab62c107a`.

Prima del patch: snapshot write-once di PREP003, fonti, piano e report storico
di 112 righe in `tmp/m1/e-vulkan/snapshots/pre-supervisor-validation-003/`.
Manifest SHA-256:
`0d8be481462f45baa10241b581d92c4db5d8ec843910ecc2a2c0461df86644b8`.
PREP003, report storico, piani e quattro artefatti build-003 sono stati
rihashati e verificati invariati alla fine. Nessuna ricevuta storica riscritta.

## Comandi effettivi ed esecuzioni conservate

Eseguiti `du -s -B1 tmp`, `df -B1 .`, snapshot e parsing AST dei due sorgenti
test/launcher. Poi, dalla radice:

```bash
/usr/bin/python3 -I -B tmp/m1/e-vulkan/supervisor_validation.py --output tmp/m1/e-vulkan/supervisor-validation-001
/usr/bin/python3 -I -B tmp/m1/e-vulkan/supervisor_validation.py --source-version 005 --output tmp/m1/e-vulkan/supervisor-validation-002
```

Il launcher crea un solo figlio Python per la suite e lo raccoglie tramite il
proprio Popen. Prima di importare i test, il figlio installa un audit hook che
blocca subprocess, fork/exec, segnali OS e ctypes.dlopen. I test sostituiscono
Popen con oggetti Python, non con innocui processi stub: **nessun subprocess
reale interno alla suite**, nessun caricamento candidato. Tentativi rilevati:
zero in entrambi i run. Il du di preflight è un processo metadata separato,
sincrono e raccolto; non è un candidato o una misura comparativa.

### Tentativo 001: conservato, non accettato come run pulito

Unittest e processo hanno restituito 18 PASS, exit 0; stderr però conteneva
`Exception ignored while finalizing file` e `OSError: stdin-close` su FakePipe.
Il mock close intenzionalmente fallito restava attivo sul finalizzatore della
fixture. Nessun failure unittest, ma errore reale di igiene della fixture:
questo run non è usato come prova finale pulita.

Non modificati stdout, stderr o receipt originali. La nuova qualificazione
`supervisor-validation-001-qualification.json` registra NOT_ACCEPTED_UNRAISABLE_FIXTURE.
Prima della correzione è stato congelato un ulteriore snapshot di fonti/log:
`snapshots/pre-supervisor-fixture-fix-001/`, manifest SHA-256
`63d9d2c9153b35c5eaecde49e4da977fce11b3c6ca279d40bdce6bedf43fdcc2`.

Correzione: addCleanup ripristina il close originale di FakePipe e lo chiude
dopo gli assert sul cleanup fallito. Il launcher ora raccoglie anche gli
unraisable, forza gc.collect e fallisce la qualificazione se ne osserva uno.
Nessuna modifica al cleanup/gate del runner candidato.

### Tentativo 002: run finale pulito

Source version `supervisor-source-005`; 18 scoperti/eseguiti, 18 PASS,
0 failure, 0 error, 0 skip, 0 unraisable. Exit figlio/launcher: 0.
Stdout completo:

```json
{"status": "PASS", "tests_run": 18, "blocked_process_or_native_attempts": []}
```

Stderr completo conservato in `supervisor-validation-002/stderr.txt`: 18 righe
test `ok`, `Ran 18 tests in 0.027s`, `OK`, senza eccezioni ignorate. La durata
è diagnostica unittest, non un campione CPU/GPU o un risultato trasferito.

Casi effettivamente PASS, tutti sotto modulo `test_followup`:

- LifecycleTests.test_all_closes_attempted_and_all_errors_retained
- LifecycleTests.test_nonblocking_configuration_failure
- LifecycleTests.test_oversize_terminated_line
- LifecycleTests.test_oversize_unterminated_line
- LifecycleTests.test_partial_protocol_eof
- LifecycleTests.test_partial_protocol_line
- LifecycleTests.test_selector_constructor_failure
- LifecycleTests.test_selector_registration_failure
- LifecycleTests.test_stdin_close_failure_still_reaps_peer
- ReceiptTests.test_absent_receipts_never_spawn
- ReceiptTests.test_compatible_pair
- ReceiptTests.test_copy_gate_missing_rejected
- ReceiptTests.test_device_queue_mismatch_never_spawns
- ReceiptTests.test_failed_status_receipt_rejected
- ReceiptTests.test_incomplete_binding_rejected_before_comparison
- ReceiptTests.test_mutated_report_rejected
- ReceiptTests.test_partial_fault_receipt_rejected
- ReceiptTests.test_recipe_cpu_gt_mismatch_never_spawns

## Hash prima/dopo e nuova ricevuta

`source-before.json` e `source-after.json` registrano nove sorgenti e due piani:
le mappe dei digest sono identiche prima/dopo il run. I due file manifest hanno
digest diversi perché contengono metadata/strutture differenti, non perché le
fonti siano cambiate. Nessun nuovo native build o shader build è stato eseguito.
Il vecchio binario è soltanto un riferimento SHA immutabile, mai lanciato;
nessun tempo/versione compiler storico è stato copiato nella nuova validazione.

| Artefatto | SHA-256 |
|---|---|
| `supervisor-validation-002/receipt.json` | `36da6443f820bdca2fe4f10a88b9f225f33b8eb4af269f54fe1f9168589e146b` |
| `supervisor-validation-002/source-before.json` | `f2a5a0a7424be19b82d30b54561432d8ff0bbe2b7f0abbc09235f5114a26e519` |
| `supervisor-validation-002/source-after.json` | `4d3debefaa51fcdb36e38d374aaa444733fd2d770c67e70bee31cad397291c11` |
| `test_followup.py`, prima/dopo uguale | `a6529958bdfe75d87136c76fa36a60ef796d3faef464cdee133a8a54932d82b3` |
| `supervisor_validation.py`, prima/dopo uguale | `2dd4650fb5df391ce68c66beffe1b55e9cc48d4de39aaf18464d67fd4ec4ba58` |
| `supervisor-validation-002/stdout.txt` | `f7441dc9436eabb55cb4a250f8cfb707352db95fd74ace9772c8411df1e0ac03` |
| `supervisor-validation-002/stderr.txt` | `a0a2f8b2140d465d9dc713f90172f982b35f4b19f436b92f5902eff0c94f7376` |
| `supervisor-test-fix.diff` | `2c18508fe37e098e26d4b1a3583d5e555e959a8d6a52a6b55f99c93b7a2aed58` |

Path relativi sotto `tmp/m1/e-vulkan/`. Diff generato con difflib, senza Git.
Snapshot, log e ricevute nuovi sono creati esclusivamente, senza sovrascrittura.

## Cleanup, risorse e limiti

PID reali raccolti: tentativo 001, du 141345 e suite 141352; tentativo 002,
du 144216 e suite 144232. Tutti exit 0, suite reaped=true. Il numero 123456789
è soltanto l'attributo del FakePopen in memoria, **non un PID osservato o
segnalato**. Gli assert verificano wait sui fake posseduti e zero kill/wait
sulla sentinella estranea. Non provano cleanup di processi GPU reali.

Scratch globale preflight: 857407488 byte per 001, 857681920 per 002;
ultimo controllo 857739264, sotto 1073741824. Margine fisso 8 MiB per questa
suite; scratch/cache/HOME soltanto sotto root/tmp. RSS finale osservata dal
supervisore 31547392 byte, getrusage del figlio 31150080 byte: osservazioni
distinte best effort, sotto guardia 512 MiB. Non sono garanzie contro OS OOM.
Nessuna rete, installazione, credenziale, Siril, indice, Git o altro namespace.

I binding sono fixture mock: CPU freeze `unit`, recipe sintetica, schema/status,
artifact hash, elenco fault, flag copy-proof e identità/coda simulate. Le prove
verificano i rami del supervisor, **non** CPU lock reale, hardware AMD, memoria
unificata, copie reali, float64, FloatControls o tolleranza numerica GPU.

**Primo incompleto: CPU pilot esclusivo e nuovo lock.** Prima occorre allineare
una nuova PREP ai nuovi hash del test; PREP003 resta storico, non rietichettato.
Dopo il pilot serve review indipendente del lock, prima di GPU validate/fault/
confirm. Nessuna di queste attività successive è stata avviata qui.
