# M1: context-003, suite47 eseguita

26 ottobre 2026. **PASS runtime: 47/47**, zero failure, error e skip.
Una sola invocazione del launcher recensito, nessun retry o modifica dei sorgenti.
Il PASS riguarda questa suite sintetica, non ammette operazioni sulle 168 fixture.

## Ammissione e controlli prima del candidato

Il coordinatore ha dichiarato concluso PREP007, chiusi i writer coordinati e
assenti lettori del namespace store. Il solo altro agente era reviewer viewer
readonly. Questa è un'attestazione delle attività coordinate, non un censimento
universale dei processi. Il precedente STOP002 resta valido e immutato.

Preflight stdlib `/usr/bin/python3 -I -B -S -`, PID **864986**, nella workspace
`/opt/workspace/astroProcessor`. Nessun import candidato prima dei controlli.
Attraversamento directory fd-relative `O_DIRECTORY|O_NOFOLLOW`; file aperti con
`O_NOFOLLOW|O_NOATIME|O_NONBLOCK`, regolari owned single-link, lettura massima
1 MiB, SHA-256 e fstat invariato durante ogni lettura.

Namespace lifecycle: UID/GID1000/1000, modo **0700**, device66306,
inode29137873. Prima di creare artefatti verificata l'assenza di `context-003`,
`context-turn-003`, `admit-context-003.json` senza seguire link.

Verificati PREP, sei pin preservati, quattro sorgenti runtime, piano e review.
Verificati anche admission001, primo FIRSTFAIL e report002. Mappa completa e
metadata effettivi sono nel raw `context-turn-003/preflight.json`.
La review READY47 mantiene scope STATIC_ONLY; il nuovo preflight autentica i byte
e i metadata correnti, senza fingere una review indipendente runtime precedente.

Nuova ammissione schema2 dopo i gate: run-id `context-003`, 47 test,
prenotazione16777216 byte, writer esclusivi e reader store chiusi. Autore e
revisore distinti conservati dall'ammissione precedente, con autorizzazione nuova
del coordinatore e binding del record review ai suoi byte correnti.

Pin review: `efeb9b9bf2a63a9a167509f664f18b9fccb66f01451cfd66e82708b289f5d50b`.
Pin hardening-review: `28be8c71ebf0229d7bc9edb4936e509746bc2c84a7a2f84e81fb35446db29352`.
Pin PREP: `f5e166cf8b24f6695eb7f21219aab538358f0b09357c79c1242da2b565a62133`.

| Quattro sorgenti runtime | SHA-256 pre = post |
|---|---|
| `v2/store.py` | `48cd729268eb185f1dfcba163fc8b6e0937d1e7a2a845573e41102fc0fcc1ce0` |
| `v2/test_store.py` | `30ff6ac9d62af94238dcd3ebe5e4e9ef764c661cf2dffb639a1ee6c8824494b7` |
| `context-lifecycle-tests/test_context_lifecycle.py` | `7d68a0d37d4236d0e1324a134679c03910417bec6d517942f6c04365e14bccb5` |
| `context-lifecycle-tests/run_suite.py` | `7b0bef90fbe061003cbc3284f13020945193b8262e3eed618944f64d95c0d504` |

I percorsi della tabella sono relativi a `tmp/m1/fixture-store/`.
Tutti i pin preservati e i report001/002 sono stati ricontrollati dopo la raccolta.

## Comando, ambiente e processi

CLI primaria realmente avviata, cwd alla radice del progetto:

```bash
/usr/bin/python3 -I -B -S tmp/m1/fixture-store/context-lifecycle-tests/run_suite.py --run-id context-003 --admission tmp/m1/fixture-store/context-lifecycle-tests/admit-context-003.json --admission-sha256 fae9971cd5312fe9334264e2d31b337c61498c53bf2250c8f6c5fbd409fb37ba
```

`launch-intent.json` conserva argv ed env esatti. PATH `/usr/bin:/bin`,
`PYTHONDONTWRITEBYTECODE=1`, `OPENSSL_CONF=/dev/null`. HOME, TMPDIR,
XDG_CACHE_HOME e PYTHONPYCACHEPREFIX del launcher puntano a directory private
sotto `context-turn-003/`. Il runner invariato crea cache/tmp sotto `context-003/`
e passa al worker un env esplicito: HOME non è impostata nel worker.
Non si attesta quindi una HOME privata ereditata dal worker. Cache e temporanei
del worker hanno percorsi espliciti privati dentro ROOT; questa suite non richiede
accesso alla home personale. Nessuna modifica del launcher per alterare l'env.

Directory outer, home/cache/tmp outer, run e cache/tmp run ricontrollate owned0700.
Ammissione e file outer0600; intent e risultati del runner0444, log unittest0600.

| Processo proprio | PID | Raccolta ed exit |
|---|---|---|
| Preflight e supervisore esterno | 864986 | comando shell exit0 |
| Launcher primario | 864987 | wait sul Popen originale, exit0 |
| Worker unittest | 864988 | runner: `REAPED`, exit0 |

Deadline runner120s inclusi5s di cleanup; outer wait140s. Nessun timeout,
segnale o errore di controllo/cleanup osservato. Durata outer osservata
2.4216166948899627s; non è un'asserzione temporale di correttezza.
La ricevuta runner mantiene prudenzialmente `slot_release_allowed:false`.
Il supervisore esterno attesta `true` soltanto dopo wait del launcher e lettura
della ricevuta con `own_child_reaped:true`, `ownership_reserved:false`.
Nessun PENDING_UNREAPED proprio. Slot riconsegnabile al coordinatore, non
autorizzazione automatica a un altro candidato.

## Esiti reali e raw

`context-003/unittest.log` contiene tutti i **47 nomi ed esiti ok**:
24 legacy, 18 lifecycle, 5 supervisor con double deterministici. Worker-result:
`status:PASS`, `tests_run:47`, `failures:0`, `errors:0`, `skipped:0`,
`failfast:true`, `fixtures:45`. Nessun conteggio basato sui subtest.

Finale verbatim del raw unittest:

```text
----------------------------------------------------------------------
Ran 47 tests in 2.002s

OK
```

Raw launcher vuoto, exit0. Ricevuta runner `CONTEXT_LIFECYCLE_COMPLETE`.
Log, intent e ricevute sono conservati senza cancellazioni. Prefisso di tutti
gli artefatti seguenti: `tmp/m1/fixture-store/context-lifecycle-tests/`.

| Artefatto | Byte | SHA-256 |
|---|---:|---|
| `admit-context-003.json` | 2085 | `fae9971cd5312fe9334264e2d31b337c61498c53bf2250c8f6c5fbd409fb37ba` |
| `context-turn-003/preflight.json` | 6659 | `bc1da22386563c809764a6ae6a56bee2a33af9a69b1f0659dd3a23742c8ba644` |
| `context-turn-003/launch-intent.json` | 1214 | `4268267f9a4c1006bf7ddaa52eef2b81de4fdbdc664402a59eb9562d75b4ab21` |
| `context-turn-003/launcher.log` | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `context-turn-003/outer-receipt.json` | 3212 | `76d7eb8d64c5240ffe1f9bf26f3e5a658fe7c449cb5cd78cc42faa78b977b202` |
| `context-003/intent.json` | 935 | `a7eb7fd56c7d5c53bc47590529460175dc507541e89d541049c6e34202870c1f` |
| `context-003/unittest.log` | 5949 | `edd420c95b279886de6fbe20172c3ff5172c43c296033de2a04f0fcffee3d9cc` |
| `context-003/worker-result.json` | 758 | `39f03750f009af4290b5161d9091d2abbf25a9b17d7a2baeb6eb8017e513e12a` |
| `context-003/receipt.json` | 2144 | `1a7d78a5ae229460d5e8be2895037a74439084674aa54252ed217b6a20d00a9f` |

## Consumi osservati

Accounting whole-tmp sui metadata, inode unici, directory NOFOLLOW e controllo
identità: prima955031552 byte, 4978 inode. Unità4096 byte, spazio disponibile
125338406912 byte. Cap1073741824, prenotazione16777216, margine16777216:
somma988585984 byte, sotto il cap. Nessuna quota OS contro writer non coordinati.

Guard reale del runner: prima955064320, dopo956219392 byte. Accounting outer
successivo:956223488 byte, 5315 inode, delta1191936 byte rispetto al preflight.
L'ultima misura precede la scrittura di outer-receipt e questo report; non è
presentata come consumo finale esatto dell'intera consegna documentale.

RSS preflight20254720 byte; massimo outer21626880 byte. `RUSAGE_CHILDREN`
osservato dal supervisore esterno:72970240 byte. Sono high-water mark Linux,
non una somma RSS simultanea né una quota garantita. Limiti invariati del runner:
AS512MiB, core0; worker CPU60s e file1MiB. Log5949 byte sotto1MiB.

## Limiti e consegna

Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`, Linux.
Modello dichiarato dall'ambiente `openai/gpt-6.1-sol`; costi/token non esposti.
Nessun override o delega. Nessun Git, rete, installazione, indice, build, GUI,
benchmark, modifica a progress/index o ai sorgenti.

La suite usa inventario e closure scientifica sintetici. I cinque test supervisor
usano mock e non dimostrano processi reali in D-state o terminazione OS garantita.
I rifiuti e il contesto effettivo sono ora osservati dalla suite, non soltanto
pianificati. Non sono risultati scientifici, FULL verify o closed168.

Nessun archivio scientifico o payload delle168 fixture è stato aperto, né sono
state avviate fasi reali archive/retire/materialize/release. I test legacy
recensiti creano e leggono piccoli `synthetic.zip` con contenuto `b'abc'` per
duplicati e link: non si dichiara falsamente zero ZIP sintetici. Nessuno ZIP
scientifico o autorevole rientra nel turno. Non è stata indebolita questa
copertura per evitare le fixture minute previste dalla suite47 autorizzata.

Consegna stabile per review indipendente dei file effettivi. Admission001 e
FIRSTFAIL001/STOP002 restano preservati; questo è un turno distinto ammesso.
