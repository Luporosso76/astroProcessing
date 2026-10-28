# Pocketfft: singolo tentativo mock37, FIRSTFAIL

26 ottobre 2026. Consegna stabile: **FIRSTFAIL**, nessun PASS della suite.
Slot runtime liberato dopo raccolta effettiva del figlio diretto PID 1079987.
Modello dichiarato dall'ambiente: openai/gpt-6-astra. Costi non esposti.

## Esecuzione e risultato

Eseguito una sola volta, dalla radice `/opt/workspace/astroProcessor`:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-observation-tests-001/test_runtime_observation.py
```

Outer: `tmp/m1/fft-pocket/tests-attempt-001/outer.py`, invocato con
`/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/tests-attempt-001/outer.py`.
Il controllo preliminare ha trovato assenti sia il namespace sia questo report.
Nessun autorename, retry, modifica alle fonti, skip o expectedFailure introdotto.

Primo e unico risultato catturato, conservato integralmente in `stderr.raw`:

```text
test_hash_read_must_be_bounded (__main__.IdentityTests.test_hash_read_must_be_bounded) ... ERROR
```

L'outer ha fermato il figlio alla prima segnalazione ERROR mediante SIGKILL,
prima del riepilogo unittest. Exit del figlio: **-9**; exit dell'outer: **2**.
Durata misurata: 0.2001728059258312 secondi, non un benchmark.
Risultati osservati: 1 ERROR, 0 FAIL, 0 PASS, 0 skip, 0 expectedFailure.
I conteggi finali della suite sono sconosciuti: `receipt.json` li registra null.
37 metodi sono stati contati via AST prima dell'avvio, non dichiarati eseguiti.
Il conteggio esatto dei metodi eventualmente iniziati dopo il primo ERROR e prima
del segnale non è disponibile. L'outer rileva l'errore via pipe; non modifica il runner unittest.

**Limite diagnostico:** il traceback non era ancora stato emesso, perché unittest
lo stampa nel riepilogo finale. Non viene attribuita una causa all'ERROR.
Non è stato rilanciato il test per ottenerlo; nessuna correzione tentata.

## Autenticazione e conservazione

Prima dell'import di progetto sono stati verificati SHA-256 e `SOURCE_SHA`:

- Supervisor: `ad776c3bac3a0b953b2b8a8ac85dd6c395ecf0e5c85a3bf95179d4553ab21cb4`.
- Suite: `43d6cdac13b1805cc44476b450ab6352366a6b435401e2f61c71db7222ded5b7`.
- Manifest corrente: `a658f12d0f273d411d6e163141e983d282e872602d0b1f0e5df9f05415bf3b35`.

Verificati anche i pin del profilo build003, della ricevuta build003 e delle cinque
fonti del profilo, sostituendo solo il supervisor storico con lo snapshot previsto.
Dieci file autenticati hanno hash pre/post identici, elencati in `preflight.json`
e `postflight.json`. Copie pre-esecuzione dei cinque input correnti sono conservate
in `tests-attempt-001/snapshots/`. Nessun import del supervisor nell'outer.

Artefatti durevoli nello stesso namespace: `preflight.json`, `launch.json`,
`stdout.raw`, `stderr.raw`, `postflight.json`, `receipt.json`.
Scritture esclusive con fsync dei file e della directory.

- stdout: 0 byte, SHA-256 `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.
- stderr: 97 byte, SHA-256 `5ac2aab528bd868553edc1e361a998c59daa009601c95c5c472956140f33a47d`.

## Ambiente, budget e ownership

Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.
Linux `Linux-7.0.0-38-generic-x86_64-with-glibc2.43`.
HOME, TMPDIR, TMP, TEMP e XDG_CACHE_HOME puntano a directory private 0700
nel nuovo namespace sotto ROOT; umask 0077, ambiente esplicito, stdin DEVNULL.

Occupazione fisica globale `tmp/`, deduplicata per device/inode tramite st_blocks:
927543296 byte prima del figlio, 927555584 byte dopo, prima della ricevuta finale.
Entrambe rispettano quota 1073741824 byte, riserva 67108864 byte e margine
16777216 byte. Prima dell'avvio è stato verificato anche spazio per 3 MiB aggiuntivi.

Limiti del figlio, soft=hard: AS 268435456 byte, CPU 60 secondi,
FSIZE 1048576 byte, CORE 0, NOFILE 64. Wall timeout outer 120 secondi;
raccolta dopo stop entro 10 secondi. Output massimo 1048576 byte per stream.
Non sono limiti RSS misurati, né una qualificazione delle prestazioni.

Il figlio è stato avviato in nuova sessione. PID, PGID e SID registrati in
`launch.json`. Prima del segnale l'outer ha osservato esclusivamente il figlio
diretto con `waitid(P_PID, ..., WEXITED|WNOHANG|WNOWAIT)`, mantenendo il PID
riservato. Ha segnalato quel PID, non gruppi o processi estranei.
`Popen.wait(timeout=10)` ha eseguito la raccolta reale e restituito -9.

## Perimetro

È un tentativo di test stdlib con mock, non i 232 casi scientifici FFT.
Nessun worker, build, lettura di mappe host reali, ldd o qualificazione loader.
Nessun Git, rete, installazione, delega, indice o aggiornamento progress.
Fonti immutate. Nessuna prova numerica o approvazione runtime derivata da questo esito.
