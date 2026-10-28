# Pocketfft: delta lettura bounded

26 ottobre 2026. Consegna stabile per review indipendente del diff, non approvata.
Modello dichiarato dall'ambiente: openai/gpt-6.1-sol. Costi non esposti.
37 metodi preparati; **0 test eseguiti**, 0 import di progetto, worker, build,
loader o FFT. Nessun PASS runtime. I 232 casi scientifici non sono stati eseguiti.

## Modifiche

`runtime_identities` richiede `min(65536, remaining)`, con residuo della dimensione
iniziale validata. EOF anticipato provoca STOP. SHA-256 e confronto finale
fstat/stat restano presenti; una crescita non estende la lettura.

Il mock rispetta ogni richiesta e conta i byte restituiti, senza sottrarre una
presunta chiamata EOF. Copertura predisposta: short reads, dimensioni 0, 65536,
65537, cap esatto 16 MiB, crescita oltre cap, EOF anticipato. La crescita altera
il fstat finale: il confronto interrompe la catena prima dello stat per nome.
Nuovi assert distinguono osservazione `WEXITED|WNOHANG|WNOWAIT` e reap senza
`WNOWAIT`; PID, PGID e SID discordanti impediscono i segnali.

I 33 metodi precedenti restano coperti: 30 hanno AST identico, due acquisiscono
assert coerenti, il test bounded viene corretto e rinominato. Quattro metodi nuovi
portano il totale a 37. Nessuno skip, expectedFailure o allentamento dei gate.

## Snapshot e pin correnti

Snapshot creato e autenticato **prima** dell'edit:
`tmp/m1/fft-pocket/snapshots/pre-runtime-bounded-read-001/`.
Contiene i byte precedenti di supervisor, suite e manifest corrente.
Il suo `manifest.json` (2017 byte, 44 righe) ha SHA-256
`ccf311aa043f2e5ad0ca90566b46e7f5f4b234e657fc5d9c8541188ad82b4f3a`.
Registra anche i pin degli storici conservati in sede: source/diff/static001,
snapshot pre-runtime-observation-001, PREP e report test001. Nessuno modificato.
PREP e report001 descrivono i byte storici, non la suite corrente.

File correnti, letti integralmente come byte/AST:

| Percorso sotto `tmp/m1/fft-pocket/` | Byte | Righe | SHA-256 |
| --- | ---: | ---: | --- |
| `supervisor.py` | 35178 | 740 | `ad776c3bac3a0b953b2b8a8ac85dd6c395ecf0e5c85a3bf95179d4553ab21cb4` |
| `runtime-observation-tests-001/test_runtime_observation.py` | 37367 | 723 | `43d6cdac13b1805cc44476b450ab6352366a6b435401e2f61c71db7222ded5b7` |
| `validation-observation-001.json` | 2187 | 60 | `a658f12d0f273d411d6e163141e983d282e872602d0b1f0e5df9f05415bf3b35` |
| `runtime-bounded-read-001.diff` | 9419 | 186 | `c4fe753ac32c427c1a92c16c93c4289548d7b14969a5560a40f5f80f83b0eec9` |

Solo il pin supervisor cambia nel manifest corrente; il pin `SOURCE_SHA` della
suite coincide. Il diff confronta i tre file correnti con lo snapshot autenticato.

## Verifiche svolte e prossimo comando

Ambiente: Linux, `/opt/workspace/astroProcessor`, Python
`3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.
Eseguiti script stdlib con `/usr/bin/python3 -I -B -S -`: hash dello snapshot e
degli storici, confronto AST, `compile(ast.parse(...), ..., 'exec')` senza exec.
Esito statico: pin coerenti, sintassi compilabile, 37 metodi, 30 metodi precedenti
invariati. Nel supervisor ogni nodo top-level eccetto `runtime_identities` è
identico: scienza232, flags, oracle, AS256, cleanup e divieto di fallback invariati.
Limite: AST e sintassi non provano il comportamento dei mock o del runtime.

Dopo review indipendente del diff, singola suite futura, non eseguita qui:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-observation-tests-001/test_runtime_observation.py
```

Nessun Git, rete, installazione, delega, indice o aggiornamento progress.
Nessuna lettura del namespace dell'altro coder o di mappe host.
Supervisor e manifest tornano ai permessi originali 0444; snapshot in sola lettura.
