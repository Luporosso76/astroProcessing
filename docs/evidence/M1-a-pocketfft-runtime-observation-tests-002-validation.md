# Pocketfft: STOP sul comando mirato, suite37 non avviata

26 ottobre 2026. Il comando mirato richiesto termina con exit 2 durante il parsing
degli argomenti. Non esegue test. La condizione exit 0 e unittest 1 OK non è
soddisfatta: la suite37 non viene avviata. Nessuna correzione, retry o rimozione
di `-f`. Slot runtime liberato dopo raccolta effettiva del PID 1090567.

Agente esecutore: subagente, modello dichiarato dal runtime
`openai/gpt-6.1-sol`. Costi non esposti. Nessuna delega ulteriore.

## Comandi ed esito

Directory di lavoro: `/opt/workspace/astroProcessor`. Prima della creazione sono
risultati assenti sia `tmp/m1/fft-pocket/tests-attempt-002` sia questo report.
Il nuovo namespace usa nomi espliciti e scritture esclusive, senza overwrite o
autorename. Non viene usato `tests-attempt-001/outer.py`.

Invocazione dell'outer:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/tests-attempt-002/outer.py
```

Primo e unico comando figlio, eseguito una sola volta:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-observation-tests-001/test_runtime_observation.py -f IdentityTests.test_hash_read_must_be_bounded
```

Stderr integrale, 254 byte, conservato in `tests-attempt-002/target/stderr.raw`:

```text
usage: test_runtime_observation.py [-h] [-v] [-q] [--locals] [--durations N]
                                   [-c] [-b] [-k TESTNAMEPATTERNS]
                                   [tests ...]
test_runtime_observation.py: error: unrecognized arguments: -f
```

Exit figlio 2, exit outer 2. Nessun segnale inviato. Nessun traceback presente:
il runner termina naturalmente sul parsing. L'outer non interrompe su stringhe
`ERROR` o `FAIL`. Durata osservata 0.1687388529535383 secondi, non un benchmark.

Unittest non emette alcun riepilogo `Ran` o `OK`. I campi count/error/fail/skip/
expected_failure nella ricevuta sono null, non zero successi presunti.
Test eseguiti: nessuno, per errore del parser antecedente alla loro esecuzione.
I 37 metodi sono soltanto contati staticamente via AST. Il namespace `suite37`
non esiste. La ricevuta complessiva riporta `STOP_TARGET`, `suite_launched=false`
e `slot_released=true`.

## Autenticazione e conservazione

Prima dell'avvio e dell'import di codice di progetto sono stati verificati:

| File sotto `tmp/m1/fft-pocket/` | SHA-256 |
| --- | --- |
| `runtime-observation-tests-001/test_runtime_observation.py` | `75e3b24cc4f933cae49ec751193e319a33035d904ecf980f6176af93199ed6a8` |
| `supervisor.py` | `ad776c3bac3a0b953b2b8a8ac85dd6c395ecf0e5c85a3bf95179d4553ab21cb4` |
| `validation-observation-001.json` | `a658f12d0f273d411d6e163141e983d282e872602d0b1f0e5df9f05415bf3b35` |

Verificati anche `SOURCE_SHA`, i pin del manifest build003, della ricevuta
build003 e delle cinque fonti del profilo, usando lo snapshot previsto per
il supervisor storico. La lettura del test conferma `RUNTIME_FILES` sostituito
con `new=()` e `unittest.main(verbosity=2, failfast=True)`; nulla è modificato.

`authenticated-inputs.json` elenca 28 file con byte e SHA-256: dieci input/pin,
tutti i dodici file preesistenti sotto `tests-attempt-001`, compresi snapshot,
ricevuta e stderr del vecchio FIRSTFAIL, e sei file `FIRSTFAIL*` sotto `tmp/`.
I controlli immediatamente precedenti al figlio e successivi alla raccolta
restituiscono gli stessi pin per tutti i 28 file.

Artefatti nuovi sotto `tmp/m1/fft-pocket/tests-attempt-002/`:

- `outer.py`, `authenticated-inputs.json`, `receipt.json`;
- `target/preflight.json`, `target/launch.json`, `target/postflight.json`;
- `target/stdout.raw`, `target/stderr.raw`, `target/receipt.json`;
- directory private di ambiente sotto `target/`.

SHA-256 dell'outer:
`9b79d188b5d29fee57b5409bc8cb58478aa8236609135a052eaa4d6880036387`.
Stdout: 0 byte, SHA-256
`e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.
Stderr: 254 byte, SHA-256
`b89976e5cc143b94b303eb5f31cff715c9bfac701878b46aecc75d66bed6a14b`.
Log e JSON sono sincronizzati con fsync dei file e delle directory.

## Ambiente, budget e raccolta

Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.
Piattaforma `Linux-7.0.0-38-generic-x86_64-with-glibc2.43`.
Ambiente esplicito: HOME, TMPDIR, TMP, TEMP e directory XDG cache/config/data/
runtime private 0700 sotto ROOT; umask 0077, stdin DEVNULL.

Occupazione fisica di `ROOT/tmp/`, deduplicata per device/inode con st_blocks:
927784960 byte al preflight, 927801344 al postflight, prima delle ricevute finali.
Quota 1073741824 byte, riserva 67108864 byte, margine 16777216 byte.
Il preflight ammette separatamente 6291456 byte di budget log, senza consumare
riserva o margine. Cap 1048576 byte per stream; stderr completo entro cap.

Limiti figlio soft=hard: AS 268435456 byte, CPU 60 secondi, FSIZE 1048576 byte,
NOFILE 64, CORE 0. Wall 120 secondi e cleanup 10 secondi. Non sono misure RSS.

Popen crea una nuova sessione. `launch.json` registra PID=PGID=SID=1090567.
L'outer osserva solo il proprio figlio con
`waitid(P_PID, ..., WEXITED|WNOHANG|WNOWAIT)`, verifica PGID/SID e mantiene
il PID riservato prima della raccolta. Gli eventuali segnali sono limitati
al gruppo proprio verificato; in questa esecuzione non ne invia.
`Popen.wait` esegue la raccolta reale e restituisce 2. Pipe chiuse, selector
chiuso e log sincronizzati prima della ricevuta di rilascio dello slot.

## Limiti

Questa esecuzione non dimostra il comportamento del test mirato o della suite:
il parser impedisce di avviarli. Il perimetro previsto resta stdlib con mock,
non una prova OS di ownership, mappe o loader e non una qualifica numerica FFT.
Nessun 232 FFT, worker, build, GUI, host runtime verifier, Git, rete,
installazione, indice o modifica a progress/index. Fonti e vecchi FIRSTFAIL
restano invariati. Si consegna STOP senza fix o ulteriori esecuzioni.
