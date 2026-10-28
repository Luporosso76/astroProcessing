# Pocketfft: target PASS, suite fermata al primo FAIL

26 ottobre 2026. Nuovo turno autorizzato dopo STOP002. Il test mirato passa;
la successiva e unica invocazione della suite termina con un FAIL dopo 36 test.
Esito complessivo `STOP_SUITE`. Nessun fix o retry dopo il fallimento.
Slot runtime liberato dopo raccolta effettiva di entrambi i figli.

Agente esecutore: subagente, modello dichiarato dal runtime
`openai/gpt-6.1-sol`. Costi non esposti. Nessuna delega ulteriore.

## Causa CLI del turno002 e controllo statico

Prima delle esecuzioni è stata letta la stdlib del Python effettivo:
`/usr/bin/python3`, versione
`3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.
File `/usr/lib/python3.14/unittest/main.py`, SHA-256
`12de7da0fea92dbff8713ebe9904ada6dbd1e0a334d58861c6287ccfc0a261d2`.
Il ramo del parser alle righe 176-180 è:

```python
        if self.failfast is None:
            parser.add_argument('-f', '--failfast', dest='failfast',
                                action='store_true',
                                help='Stop on first fail or error')
            self.failfast = False
```

La suite chiama già `unittest.main(verbosity=2, failfast=True)`. Un controllo
AST conferma il valore True invariato. Il parser non espone quindi `-f`:
questo spiega l'errore CLI del turno002 senza richiedere modifiche ai test.
Nessun nuovo framework o test preparatorio.

Prima della creazione risultavano assenti `tests-attempt-003` e questo report.
Il controllo statico del nuovo outer rispetto a outer002 verifica soltanto:
nuove destinazioni, rimozione dell'argomento CLI `-f` e aggiunta della
conservazione dei nove file del turno002 e del suo report. Nessun altro delta.

## Comandi ed esiti reali

Directory di lavoro `/opt/workspace/astroProcessor`. Unica invocazione outer:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/tests-attempt-003/outer.py
```

Primo comando figlio:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-observation-tests-001/test_runtime_observation.py IdentityTests.test_hash_read_must_be_bounded
```

Exit 0, `Ran 1 test in 0.047s`, `OK`: 1 PASS, 0 FAIL, 0 ERROR, 0 skip,
0 expectedFailure. PID=PGID=SID=1094853, raccolto con `Popen.wait`.
Durata outer osservata 0.2221746880095452 secondi, non un benchmark.

Soltanto dopo quel risultato è stato eseguito, una sola volta:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-observation-tests-001/test_runtime_observation.py
```

Exit 1. Il failfast già nel sorgente ferma naturalmente la suite:
36 test eseguiti su 37 metodi contati via AST, 35 PASS, 1 FAIL, 0 ERROR,
0 skip e 0 expectedFailure osservati. Un metodo non viene eseguito.
PID=PGID=SID=1094859, raccolto con `Popen.wait`.
Durata outer osservata 1.138598060933873 secondi, non un benchmark.

Il traceback naturale completo resta in `suite37/stderr.raw`; estratto decisivo:

```text
FAIL: test_peak_34111558_includes_headers_and_rejects_before_namespace (__main__.QualificationTests.test_peak_34111558_includes_headers_and_rejects_before_namespace)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/opt/workspace/astroProcessor/tmp/m1/fft-pocket/runtime-observation-tests-001/test_runtime_observation.py", line 527, in test_peak_34111558_includes_headers_and_rejects_before_namespace
    self.assertEqual(peak, 34111558)
    ~~~~~~~~~~~~~~~~^^^^^^^^^^^^^^^^
AssertionError: 34111550 != 34111558

----------------------------------------------------------------------
Ran 36 tests in 0.722s

FAILED (failures=1)
```

Nessuna diagnosi ulteriore o modifica del valore atteso dopo lo STOP.
La ricevuta suite registra `count=36`; i suoi campi error/fail/skip/
expected_failure restano null perché l'outer002 li valorizza solo su `OK`.
I conteggi sopra derivano dallo stderr completo, non da quei campi null.
Exit outer 2. La ricevuta complessiva riporta `STOP_SUITE`, `targeted_pass=true`,
`suite_launched=true`, `slot_released=true`. Nessun segnale è stato inviato;
l'outer non uccide su parole `ERROR` o `FAIL`.

## Hash e conservazione

Autenticati prima di ciascun figlio e verificati identici dopo la raccolta:

| File sotto `tmp/m1/fft-pocket/` | SHA-256 |
| --- | --- |
| `runtime-observation-tests-001/test_runtime_observation.py` | `75e3b24cc4f933cae49ec751193e319a33035d904ecf980f6176af93199ed6a8` |
| `supervisor.py` | `ad776c3bac3a0b953b2b8a8ac85dd6c395ecf0e5c85a3bf95179d4553ab21cb4` |
| `validation-observation-001.json` | `a658f12d0f273d411d6e163141e983d282e872602d0b1f0e5df9f05415bf3b35` |

Verificati anche `SOURCE_SHA`, pin del manifest build003 e della sua ricevuta,
cinque fonti del profilo con supervisor storico nello snapshot previsto.
`authenticated-inputs.json` elenca 38 file: i 28 già preservati nel turno002,
i nove file del turno002 e il suo report. Entrambi i postflight confermano
tutti i 38 pin invariati. Restano intatti outer002, raw, ricevute, snapshot,
report002 e i sei file `FIRSTFAIL*` precedenti sotto `tmp/`.

Outer003 SHA-256:
`7e1b8c60a8786fc8712cd5c4ecc9d25eb5f033b1cc911ef7ddf2efba1b429c6d`.
Outer002 preservato con SHA-256:
`9b79d188b5d29fee57b5409bc8cb58478aa8236609135a052eaa4d6880036387`.

| Stream | Byte | SHA-256 |
| --- | ---: | --- |
| target stdout | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| target stderr | 191 | `0921aa616cdc9efe38074b980d43931b43429aec89c7a273a517b4f14d960707` |
| suite stdout | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| suite stderr | 5229 | `03bee689e2c3ddbff2a3293ccd0f6f51f785216a5a476d90a1650794a8e9a266` |

Artefatti nuovi solo nel namespace esplicito
`tmp/m1/fft-pocket/tests-attempt-003/` e in questo report.
Il namespace contiene outer, pin autenticati, ricevuta complessiva e,
separatamente per `target/` e `suite37/`, preflight, launch, postflight,
ricevuta, stdout/stderr integrali e directory private di ambiente.
Scritture esclusive con fsync dei file e delle directory; nessun overwrite.

## Ambiente, gate e raccolta

Piattaforma `Linux-7.0.0-38-generic-x86_64-with-glibc2.43`.
HOME, TMPDIR, TMP, TEMP e XDG cache/config/data/runtime puntano a directory
private 0700 sotto ROOT. Umask 0077, ambiente esplicito, stdin DEVNULL.

Occupazione fisica di `ROOT/tmp/`, deduplicata per device/inode con st_blocks:

| Fase | Preflight byte | Postflight byte |
| --- | ---: | ---: |
| target | 927870976 | 927891456 |
| suite | 927932416 | 927952896 |

Quota 1073741824 byte, riserva 67108864 byte, margine 16777216 byte.
Ogni preflight verifica anche 6291456 byte di budget log separato.
Cap output 1048576 byte per stream; entrambi gli stderr restano completi.
Limiti figlio soft=hard: AS 268435456 byte, CPU 60 secondi,
FSIZE 1048576 byte, NOFILE 64, CORE 0. Wall 120 secondi, cleanup 10 secondi.
Non sono misure RSS o qualifiche prestazionali.

Ogni Popen crea una nuova sessione. L'outer usa solo il proprio PID con
`waitid(P_PID, ..., WEXITED|WNOHANG|WNOWAIT)`, verifica PGID/SID e mantiene
il PID riservato fino al vero `Popen.wait`. L'eventuale segnale riguarda solo
il gruppo proprio verificato; nessun segnale necessario in questo turno.
Entrambi i figli sono realmente raccolti. Pipe e selector chiusi, log
sincronizzati prima della ricevuta finale di rilascio dello slot.

## Limiti

Il target dimostra la regressione bounded-read con mock; la suite non passa.
Questi test non qualificano ownership OS, mappe host, loader o correttezza FFT.
Il test con `all_232` nel nome verifica chiamate simulate: nessun 232 FFT reale.
Nessun worker, build, GUI, host runtime verifier, Git, rete, installazione,
delega, indice, modifica alle fonti o aggiornamento progress/index.
Nessun allentamento di failfast, skip, fix o retry dopo il FAIL.
