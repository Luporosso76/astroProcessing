# M1: context-002 fermato prima dell'ammissione

26 ottobre 2026. Esito `STOP_NOT_RUN`, **0/47**. Nessun launcher avviato.

## Primo blocco

`run_suite.py:150` richiede `global_writers_exclusive is True`, oltre a
`readers_closed is True`. Il brief assegna uno slot runtime esclusivo, ma consente
scritture PREP concorrenti nell'altro namespace viewer. L'esclusività dei processi
di test non dimostra quella dei writer globali richiesta dal launcher.

Non è stata fabbricata un'ammissione con questa dichiarazione. Non è stato avviato
alcun candidato per verificare se il gate potesse essere aggirato. Nessun retry,
fix, chmod o cancellazione. Il coordinatore deve prima chiudere i writer globali
e attestare il requisito; questa consegna non autorizza un altro avvio.

## Verifiche reali in sola lettura

Interprete di preflight PID **857545**, exit **2**, comando effettivo:

```bash
PYTHONDONTWRITEBYTECODE=1 OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S -
```

Lo stdin era un heredoc stdlib con `os`, `stat`, `hashlib`, `json`, `sys`.
Ha aperto la radice e ogni componente relativa con descriptor di directory
`O_DIRECTORY|O_NOFOLLOW`. I file sono stati aperti con
`O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_NOATIME`, verificati regolari, UID effettivo,
single-link e dimensione massima 1 MiB. Lettura in blocchi da 65536 byte,
confronto fstat pre/post e SHA-256. Nessun import candidato o test.

Directory `tmp/m1/fixture-store/context-lifecycle-tests/`: UID/GID 1000/1000,
modo reale **0700**, device 66306, inode 29137873. Nessuna modifica metadata.
Assenza verificata con `stat(..., follow_symlinks=False)` fd-relative:
`context-001`, `context-002`, `context-turn-002`, `admit-context-002.json`.

Sono stati autenticati 15 file. Tutti regolari owned single-link, stabili durante
la singola lettura. Sei pin preservati e quattro pin sorgente runtime coincidono.
La verifica non prova l'assenza di modifiche future da parte di altri writer.

Prefissi: C = `tmp/m1/fixture-store/context-lifecycle-tests/`,
V = `tmp/m1/fixture-store/v2/`, E = `docs/evidence/`.

| File | SHA-256 osservato |
|---|---|
| C`preparation-receipt.json` | `f5e166cf8b24f6695eb7f21219aab538358f0b09357c79c1242da2b565a62133` |
| C`test_context_lifecycle.py` | `7d68a0d37d4236d0e1324a134679c03910417bec6d517942f6c04365e14bccb5` |
| C`run_suite.py` | `7b0bef90fbe061003cbc3284f13020945193b8262e3eed618944f64d95c0d504` |
| V`store.py` | `48cd729268eb185f1dfcba163fc8b6e0937d1e7a2a845573e41102fc0fcc1ce0` |
| V`test_store.py` | `30ff6ac9d62af94238dcd3ebe5e4e9ef764c661cf2dffb639a1ee6c8824494b7` |
| V`preparation-receipt.json` | `4859708afa18c031e51afebcf6fd1a11cb3f435b36d826ba689bc32b5b389146` |
| V`run_small_suite.py` | `bb112640d452fb7fac1754c4db2221ad8104fa4fcf6a627adaaa3959a208d33e` |
| V`static_check.py` | `d4a8cf1628a7c2906c172920642755ddf82c73ece226f4731b0a53d00f400eed` |
| E`M1-fixture-store-followup.md` | `2d29db958a6a36ce170f425439bf7e3b502874e532aef0b40f83b80fbdec48db` |
| `plans/M1-fixture-context-tests.md` | `2843b1b55d4738ae9727e8cca069fd84741b089b4860c3f488e4878139b3bbe1` |
| C`admit-context-001.json` | `7bf7c0e02237d0edc4cf1a811a8f507aad3e1de462e2213401d17481e67490bf` |
| E`M1-fixture-context-validation.md` | `9f6e4973676634e3960adc61d507f3910921deaa9596d88f7d227294794877e7` |
| E`M1-fixture-context-followup-review.md` | `efeb9b9bf2a63a9a167509f664f18b9fccb66f01451cfd66e82708b289f5d50b` |
| E`M1-fixture-context-delivery-hardening.md` | `f98945150ebc8d4429970819f1739ca4a267cabb37a7825c2361a5e94ef9349a` |
| E`M1-fixture-context-hardening-review.md` | `28be8c71ebf0229d7bc9edb4936e509746bc2c84a7a2f84e81fb35446db29352` |

Ammissione001 e precedente FIRSTFAIL restano intatti. Nessuna nuova ammissione,
outer directory o directory run è stata creata, perché manca il prerequisito.

## Raw diagnostico

Estratti verbatim dell'oggetto JSON emesso dal preflight. Non sono un log unittest.
L'output completo del tool comprende anche i metadata dei 15 file sopra elencati.

```text
  "budget_preflight": "NOT_REACHED",
  "candidate_imports": 0,
  "child_started": false,
  "diagnosis": "global_writers_exclusive cannot be asserted: concurrent PREP writes permitted by coordinator brief",
```

```text
  "global_writer_exclusivity_verified": false,
  "launcher_started": false,
  "namespace": {
    "device": 66306,
    "gid": 1000,
    "inode": 29137873,
    "mode": "0o700",
    "uid": 1000
  },
  "outer002_created": false,
  "pending_owned_child": false,
  "platform": "linux",
  "preflight_pid": 857545,
  "preserved_pins_verified": 6,
  "python": "3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]",
  "required_gate": "run_suite.py:150 requires global_writers_exclusive is True",
  "rss_preflight": "NOT_REACHED",
  "run_id": "context-002",
  "runtime_source_pins_verified": 4,
  "status": "STOP_NOT_RUN",
  "tests_expected": 47,
  "tests_run": 0
```

## Limiti e handoff

Linux, Python 3.14.4 come sopra. Modello dichiarato dall'ambiente
`openai/gpt-6.1-sol`; costi e token non esposti. Nessun override o delega.
Letti integralmente AGENTS55, piano73, followup-review26, validation109,
delivery-hardening38, hardening-review23, runner260 e receipt60 righe.
Gli altri sorgenti sono stati letti soltanto come byte per l'hash.

Non raggiunti: misura fisica whole-tmp, spazio libero, RSS e isolamento runtime
HOME/cache/tmp. Il cap 1 GiB, la prenotazione 16 MiB e il margine 16 MiB sono
requisiti non verificati in questo turno. Nessuna prenotazione runtime acquisita.
Il comando CLI primario non è partito e non viene sostituito da un altro launcher.

24 legacy, 18 lifecycle e 5 supervisor restano non eseguiti in questo turno.
Zero child creati, nessun child da raccogliere e nessuna attestazione di reap.
Questo descrive soltanto processi propri di questo turno, non processi altrui.
Nessuna operazione reale archivio/retire o lettura dei 168 payload. Nessuna rete,
installazione, Git, indice, modifica a progress o modifica a runner/test.

Consegna stabile per review indipendente del blocco e delle osservazioni effettive.
