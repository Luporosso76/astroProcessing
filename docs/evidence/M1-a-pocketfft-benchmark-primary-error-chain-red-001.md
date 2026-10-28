# M1.a: tentativo di riproduzione del MED annidato

26 ottobre 2026. **FIRSTFAIL_UNEXPECTED**, non `REPRODUCED_KNOWN_MED`.
Un solo test eseguito, un fallimento, exit1; outer exit2. Nessun retry o fix.
Manifest: `KNOWN_MED_PENDING_FIX`. Il PASS47 storico non chiude questo MED.

## Comando ed esito

```sh
python3 -I -B -S tmp/m1/fft-pocket-benchmark/primary-error-chain-tests-001/outer.py
```

Unico comando figlio effettivo:

```sh
python3 -I -B -S tmp/m1/fft-pocket-benchmark/test_protocol.py CombinedFailureTests.test_nested_results_close_preserves_failure_ledger
```

Atteso: perdita del registro durante il secondo flush di un vero TextIOWrapper,
dopo `record()` e `Child.close()`. Osservato: la fixture BytesIO ha ricevuto una
sola chiamata write, non due. Traceback naturale a test_protocol.py:613:
`self.assertEqual(raw.attempts, 2)`, `AssertionError: 1 != 2`.
Log: `Ran 1 test in 0.003s`, `FAILED (failures=1)`; stdout vuoto.

Il metodo contiene 17 siti di chiamata ad assert, inclusi helper e assertRaises;
non è un conteggio delle asserzioni runtime dei mock. Il controllo sulla prima
premessa della fixture fallisce. Gli assert su registro persistito, STOP/exit2
e blocco del lancio non vengono raggiunti. **Nessuna perdita diagnostica dimostrata
dal turno**, né confutazione del finding statico. Occorre una nuova ammissione
per correggere la fixture e riprodurre il caso prima di applicare il fix.

## Limiti, ownership e conservazione

Preflight: 12 pin sorgente autenticati prima dell'import; benchmark invariato.
Quota1 GiB, allocati934625280, riserva64 MiB, margine16 MiB, budget2424832 byte.
Cap invariati: wall55 s, CPU50 s, AS256 MiB, FSIZE1 MiB, NOFILE64, CORE0;
umask077, namespace0700 e HOME/TMP/XDG privati. Nessun candidato nativo.
Python3.14.4, Linux. PID1251943, WNOWAIT e reap consumante riusciti, nessun segnale,
nessun timeout, riserva rilasciata e slot libero. Durata outer0.1904114680364728 s.
Pin pre/post runtime identici; solo esito manifest aggiornato dopo il run.

Snapshot/diff: `tmp/m1/fft-pocket-benchmark/primary-error-chain-tests-001/`.
Log/ricevute: `tmp/m1/fft-pocket-benchmark/primary-error-chain-red-001/`.
201 file protetti verificati invariati, inclusi 188 precedenti e tutte le prove47.
I sette FIRSTFAIL storici conservano aggregato
`5c32e5fa5d456d46df2443a550d69b4a44ee1d16a6d9522d8c2fc1f6bf37817c`.
Si aggiunge un FIRSTFAIL reale per questo tentativo inatteso: totale8, non una
modifica dello storico. PLAN, benchmark e proposta coder non modificati;
proposta in scrittura non letta. Nessuna suite completa, delega, Git o operazione nativa.

## SHA-256 finali

| File | SHA-256 |
| --- | --- |
| benchmark.py | `6a8a548ca1088c99ce2296444f1117f481b487b345bdd319a3444f33c0ca5a1c` |
| test_protocol.py | `a0a8767453cc1f311d40e9b05f258cd6fe054f8d77337ac8a882966a2d0b7cbe` |
| manifest.json | `5ddaab4d0ed749c9d1c4b475a1d48f495e3b9626739086d0b14d03502b3838eb` |
| source.diff | `4bb681aed6140b3d4727c85a6b080ca6efbcd55588432e56a8bd1f7d77d7ba9b` |
| result.json e FIRSTFAIL.json | `5ad6bdcd5710b5a421487b1bdba57cee709503c99ae850a2ebb98d09df0fe172` |
| stderr.log | `8f15c9e5093b943b94058302a31a04fa6fe27cc9233eb5e6cf7875bc334ab9f1` |

Tutti i pin sono in `primary-error-chain-tests-001/after.json`.
Nessuna adozione, prestazione o chiusura M1. Modello runtime `openai/gpt-6-astra`, senza override.
