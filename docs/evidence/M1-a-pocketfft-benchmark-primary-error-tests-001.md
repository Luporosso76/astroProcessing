# M1.a: test della diagnosi primaria benchmark, turno 001

## Esito e perimetro

26 ottobre 2026. Stato: `PASS_MOCK_PENDING_INDEPENDENT_REVIEW`.
Un solo turno ammesso dal coordinatore: 47 test mock/unitari PASS, exit0.
Il runner ha eseguito 22 ProtocolTests, 16 LifecycleTests e nove CombinedFailureTests.
Nessun retry, skip, expectedFailure, compilatore, worker nativo, simulatore figlio o benchmark.
La review indipendente resta pendente; questo report non approva la patch né chiude M1.

Sono cambiati soltanto `test_protocol.py`, `manifest.json` e lo stato/attribuzione
del `PLAN.md`, oltre ai nuovi artefatti e a questo report. `benchmark.py` è invariato.
Il PLAN rinvia all'audit F06: il mandato storico era ambiguo sull'esecuzione mock;
non prova un'autorizzazione specifica del proprietario. Il report storico resta immutato.

Il precedente PASS38 appartiene al benchmark
`fca630935cb43b7b8e47bd6900f757d69ea306c447efc63a08c6969fb0ef7bba`.
Non è stato trasferito alla patch: il nuovo conteggio47 viene dal log del turno corrente.
Non è stato eseguito un confronto red/green sul sorgente storico: l'ammissione
consentiva un unico run sul fix stabile. Non si dichiara una regressione red-to-green misurata.

## Comandi e autenticazione

Dalla radice `/opt/workspace/astroProcessor`:

```sh
python3 -I -B -S tmp/m1/fft-pocket-benchmark/primary-error-tests-001/outer.py
```

Il launcher ha eseguito esattamente:

```sh
python3 -I -B -S tmp/m1/fft-pocket-benchmark/test_protocol.py
```

`unittest.main(failfast=True, verbosity=2)` termina al primo fallimento naturale.
Il launcher non interpreta le parole ERROR/FAIL per uccidere il processo.
Il loader carica una sola volta i metodi dichiarati, senza duplicare quelli ereditati.

Prima del runtime: AST parse dei tre Python, conteggio47 e verifica dei 12 pin
del manifest. Il test verifica nuovamente i pin prima di importare `benchmark.py`.
Il supervisore e i candidati non vengono importati. Prima delle modifiche sono stati
autenticati manifest `116c2d48cd916bca5e0442a5451158115602838e991db8c050b6ad0149cbc732`,
test `55a415fdd0cb1f052befd4e157121241fe3f3f370e04c94257afbe0770c01c41` e diff coder
`48c3d32b7c2fb6d7dedb7ec6740739d62ffe7d3df3623a4d8abaac856ea08042`.

`primary-error-mock-001/before.json` e `result.json` attestano pin identici prima/dopo
l'esecuzione. Manifest eseguito: `3d47b6a484a1a4673eb0accb6295698616ed6dfa75e12283cecfc145c19b4824`.
PLAN eseguito: `2bcb8fbbaee6069e4fc19034ab575b856d2c4d539502d281205178712cfceda8`.
Solo manifest e PLAN hanno poi ricevuto l'esito documentale; nessuna riesecuzione.

## Risorse e processo reale del solo interprete

Il precedente outer era inline e non conservato come sorgente. Il nuovo `outer.py`
rende ispezionabile lo stesso schema di ammissione, con i cap richiesti:
wall55 s, CPU50 s, AS268435456, FSIZE1048576, NOFILE64, CORE0.
I limiti resource vengono applicati prima dell'exec del test; non sono stati
campionati indipendentemente dentro il figlio. stdout/stderr hanno ciascuno cap1 MiB.
Umask077, directory0700 e ambiente minimale con HOME/TMP/TMPDIR/TEMP/XDG privati.

Preflight corrente: 934445056 byte allocati sotto `tmp/`, `st_blocks*512` e
hardlink deduplicati per device/inode. Quota1073741824, riserva67108864,
margine16777216, budget log/receipt2424832 = `2*1048576 + 4*65536 + 16*4096`.
Somma1020755968, entro quota. Il picco benchmark67674112 non è stato riservato:
questo turno non avvia candidati. Allocazione osservata dopo il test934465536.
Queste misure non ammettono un successivo turno nativo.

PID reale dell'interprete1235669, nuova sessione; WNOWAIT e verifica gruppo/sessione
prima del wait consumante. Exit0, reap riuscito, reserved=false,
signals_closed=true, slot_blocked=false, termination=[], timed_out=false.
Nessun segnale reale inviato. TERM1 s/KILL5 s restano rami di timeout non esercitati.
Il launcher controlla l'assenza di discendenti vivi nel proprio gruppo prima del reap.
Gli errori del launcher conservano stato UNRESOLVED/blocco, senza nuovi lanci.

Ambiente: Linux, Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.
Il riferimento GCC descrive l'interprete, non una build effettuata qui.
Log: `Ran 47 tests in 0.104s`, `OK`. Durata esterna0.2870906791649759 s,
non misura prestazionale FFT. stdout0 byte; stderr5002 byte.

## Copertura dimostrata

I nove nuovi metodi usano il vero unwinding di `Child.close` e `stop` con OS,
Popen, selector, clock e writer sostituiti. Il writer serializza e deserializza JSON
in uno store indipendente: gli assert leggono le ricevute salvate, non soltanto
l'attributo dell'eccezione. Non è una prova di persistenza su disco/fsync/power loss.

- Oracle primario + stdout.close: entrambe le ricevute conservano fase, tipo e
  messaggio distinti; cause esplicita LookupError preservata. QUIT/reap riusciti,
  segnali chiusi, slot bloccato per close irrisolto.
- Oracle + reap: diagnosi primaria e ChildProcessError restano separate;
  owner riservato, UNRESOLVED conserva Popen, segnali vietati e nuovo lancio bloccato.
- READY errato + stderr.close: primaria `startup.ready`, cleanup indipendente,
  SIGTERM/reap senza QUIT su protocollo non inizializzato.
- Runtime-map + selector.close: primaria `startup.runtime-map`, context implicito
  LookupError serializzato, QUIT/reap e blocco del lancio successivo.
- Creazione selector dopo Popen + reap fallito: owner non perso, riserva e
  UNRESOLVED mantenuti, chiusura di tutte le pipe verificata.
- Registrazione stderr dopo prima registrazione riuscita + selector.close:
  fase precisa `startup.selector.register.stderr`, SIGTERM/reap, blocco successivo.
- process.json write + oracle: nessuna ricevuta processo fittizia; FIRSTFAIL salva
  primaria e `receipt.process.json`, exit2 e blocco del lancio.
- FIRSTFAIL write + oracle + close: process.json conserva entrambe le diagnosi;
  stderr strutturato esplicita `FIRSTFAIL_persisted=false`, le tre categorie,
  exit2 e cap65536 byte. Nessuna ricevuta FIRSTFAIL fittizia.
- FIRSTFAIL write con primaria100000 caratteri: stderr resta entro cap,
  messaggio ridotto a1024 caratteri, exit2 e blocco anche senza Popen precedente.

I 38 test esistenti sono rimasti invariati nei loro assert e sono stati rieseguiti.
Coprono anche QUIT/TERM/KILL, WNOWAIT, riserva durante segnale/reap e divieto
di segnali dopo reap. Le vecchie fixture, i 192 descrittori, oracle, timer e IQR
non sono stati modificati. Nove negativi nativi e validate192 restano non eseguiti.

## Snapshot, hash e preservazione

Namespace creati senza overwrite o autoname: `primary-error-tests-001/` per
snapshot/diff/outer e `primary-error-mock-001/` per l'unico run.
Il diff completo è `primary-error-tests-001/source.diff`.
`before.json`/`after.json` del primo namespace registrano hash e verifiche.

SHA-256 finali, percorsi relativi a `tmp/m1/fft-pocket-benchmark/`:

| File | Byte | SHA-256 |
| --- | ---: | --- |
| benchmark.py | 36183 | `6a8a548ca1088c99ce2296444f1117f481b487b345bdd319a3444f33c0ca5a1c` |
| test_protocol.py | 28446 | `21e588ecd550ee119a8305a1e3e78b5119c9c5593096de9ea824704bc0bf1f2e` |
| PLAN.md | 8647 | `dac8f28071c9abf285caac7209193039a158d15206ad161ce7f4158837c61463` |
| manifest.json | 15149 | `985e5c8ff5c1bcb8948faed11d69951eb6b6483d08fe6889ea0f9b59f9746837` |
| primary-error-tests-001/source.diff | 21798 | `408e3f94704fcf76a2b3bab971282e9f0bab8abb8e544e2cfaa43aa1ea4787e2` |
| primary-error-tests-001/outer.py | 7088 | `a83bbeb8b615ee24ae76dd2396818a70a2f3294234c653647d6ddc561f4f97c3` |
| primary-error-mock-001/before.json | 2131 | `94783e215871bb5e6242ca28b344e646947fade070e7b7fc596d72c4a27ad0ac` |
| primary-error-mock-001/result.json | 1153 | `662ff0264b27f09178edfb8a630d6782a712000bc88cc58377c669004f617c91` |
| primary-error-mock-001/stderr.log | 5002 | `8e572073f5fe1293c58eb1890c551c00e6b53939f198e7f86205e4b5d8cdf438` |

188 file protetti verificati invariati; 12 pin sorgente finali coerenti.
I sette FIRSTFAIL storici sono identici, aggregato
`5c32e5fa5d456d46df2443a550d69b4a44ee1d16a6d9522d8c2fc1f6bf37817c`.
Il turno PASS non aggiunge FIRSTFAIL reale. Le ricevute dei fault attesi nei test
sono soltanto oggetti mock e non entrano nel conteggio storico.
Nessuna modifica a progress, indici, driver, adapter, supervisore o report storici.
Nessun Git, rete, installazione, indice, segreto o delega.

Autore: subagente tests; modello dichiarato dal runtime `openai/gpt-6-astra`,
senza override. Costi/token non esposti. Fonti lette: benchmark751 righe,
test precedente389, manifest167, PLAN128, report storico177, audit63,
snapshot preservation-before112, REPORT coder completo; nessuna baseline4500.
La review indipendente del sorgente e dei nuovi hash compete al revisore successivo.
