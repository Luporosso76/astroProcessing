# M1: turno context-001 fermato al preflight

Esito: **STOP_NOT_RUN**, non PASS e non failure dei test.
La distinta ammissione consentiva un solo avvio della suite da 47 test, dopo i
preflight. Il primo controllo inatteso ha fermato il turno prima del launcher.
Nessuna correzione, chmod, retry, modalità alternativa o modifica dei fingerprint.
Il controllo di sola lettura successivo serve solo a documentare causa e pin post-STOP.

## Primo errore e stato del turno

Diagnosi esatta: `namespace not owned/non-writable by others`.
Il namespace `tmp/m1/fixture-store/context-lifecycle-tests/` appartiene all'UID
effettivo 1000, GID 1000, ma ha modo **0775**. Il controllo di proprietà passa;
quello che vieta scrittura group/other fallisce per il bit group-write 0020.
Device 66306, inode 29137873. L'attraversamento prima del controllo usa directory
fd-relative O_DIRECTORY/NOFOLLOW e fstat; non segue alias di directory.
Il controllo diagnostico successivo conferma che `context-001` è ancora assente.

PID dell'interprete di preflight: **824186**. Non è il PID del candidato o del
supervisore runtime: nessun launcher, worker o Popen è stato avviato.
Test eseguiti **0/47**, import candidato/test **0**, child da raccogliere **0**.
Non esiste un child `PENDING_UNREAPED`, né una prenotazione trattenuta per un child.
Nessun file minuto o log unittest è stato creato. Il namespace del run è assente.
Il turno viene riconsegnato al coordinatore: non si inventa un reap o un PASS
per dichiarare risolto il gate di esecuzione.

## Controlli raggiunti e non raggiunti

Superati prima del primo errore:

- Ammissione 1600 byte, file regolare owned single-link, NOFOLLOW, modo 0600,
  identità stabile durante lettura e SHA-256 esatto.
- JSON senza chiavi duplicate, schema 2, ADMIT, scope/run corretti, 47 attesi,
  prenotazione dichiarata 16 777 216 byte, turno esclusivo e reader chiusi.
- Review indipendente PASS legata alla stessa mappa dei quattro sorgenti.
- SHA della PREP e sei pin preservati, più esatta copertura dei quattro sorgenti
  runtime e loro SHA. Le sovrapposizioni store/test v2 non sono nuovi sorgenti.

Non raggiunti a causa del primo errore: preflight fisico whole tmp, spazio libero,
RSS, isolamento HOME/cache del child e avvio del launcher. Il controllo di assenza
del target è stato eseguito soltanto dopo STOP, come diagnosi readonly.
Nessuna osservazione di questi gate mancanti viene presentata come PASS.

Budget ammesso, non consumato da un run: cap globale 1 GiB, prenotazione 16 MiB,
margine 16 MiB, RSS best effort 512 MiB. Non è stata rilevata una nuova quota
fisica dello spazio perché il controllo directory ha fermato prima la sequenza.
Nessuna misura stale della PREP sostituisce il prossimo preflight completo.
Costi e token del modello non esposti dal runtime; nessun servizio a pagamento usato.

## Comando e versione

Comando ammesso ma **non avviato**, con flag e valori separati:

```bash
PYTHONDONTWRITEBYTECODE=1 OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S tmp/m1/fixture-store/context-lifecycle-tests/run_suite.py --run-id context-001 --admission tmp/m1/fixture-store/context-lifecycle-tests/admit-context-001.json --admission-sha256 7bf7c0e02237d0edc4cf1a811a8f507aad3e1de462e2213401d17481e67490bf
```

Comando effettivo: `PYTHONDONTWRITEBYTECODE=1 OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S -`
con heredoc stdlib di preflight readonly, exit **2**. Nessun import di store o test;
il launcher viene letto soltanto come sorgente pinned, non eseguito.
Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`, Linux.
Il successivo heredoc readonly di metadati/hash termina con exit **0** e output
`POST_STOP_READONLY: pins_unchanged; no_launcher; no_child; no_retry`.
Modello self-report: `openai/gpt-6.1-sol`, senza override o deleghe.

## Raw del primo STOP

L'output integrale è conservato qui perché il piano non prevedeva un nuovo wrapper
o namespace esterno `context-turn-001`. Nessun wrapper è stato introdotto per il turno.

```json
{"admission_bytes": 1600, "admission_sha256": "7bf7c0e02237d0edc4cf1a811a8f507aad3e1de462e2213401d17481e67490bf", "candidate_imports": 0, "child_started": false, "diagnosis": "namespace not owned/non-writable by others", "error": "RuntimeError", "hashes": {"docs/evidence/M1-fixture-store-followup.md": "2d29db958a6a36ce170f425439bf7e3b502874e532aef0b40f83b80fbdec48db", "tmp/m1/fixture-store/context-lifecycle-tests/run_suite.py": "7b0bef90fbe061003cbc3284f13020945193b8262e3eed618944f64d95c0d504", "tmp/m1/fixture-store/context-lifecycle-tests/test_context_lifecycle.py": "7d68a0d37d4236d0e1324a134679c03910417bec6d517942f6c04365e14bccb5", "tmp/m1/fixture-store/v2/preparation-receipt.json": "4859708afa18c031e51afebcf6fd1a11cb3f435b36d826ba689bc32b5b389146", "tmp/m1/fixture-store/v2/run_small_suite.py": "bb112640d452fb7fac1754c4db2221ad8104fa4fcf6a627adaaa3959a208d33e", "tmp/m1/fixture-store/v2/static_check.py": "d4a8cf1628a7c2906c172920642755ddf82c73ece226f4731b0a53d00f400eed", "tmp/m1/fixture-store/v2/store.py": "48cd729268eb185f1dfcba163fc8b6e0937d1e7a2a845573e41102fc0fcc1ce0", "tmp/m1/fixture-store/v2/test_store.py": "30ff6ac9d62af94238dcd3ebe5e4e9ef764c661cf2dffb639a1ee6c8824494b7"}, "launcher_started": false, "prep_sha256": "f5e166cf8b24f6695eb7f21219aab538358f0b09357c79c1242da2b565a62133", "preserved_pins": 6, "python": "3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]", "runtime_source_pins": 4, "slot_child_pending": false, "status": "STOP_NOT_RUN", "supervisor_pid": 824186, "tests_run": 0}
```

## Byte pin pre/post

Tutti identici prima e dopo STOP. Prefisso dei file brevi:
`tmp/m1/fixture-store/context-lifecycle-tests/`.

| File | SHA-256 pre = post |
| --- | --- |
| `admit-context-001.json` | `7bf7c0e02237d0edc4cf1a811a8f507aad3e1de462e2213401d17481e67490bf` |
| `preparation-receipt.json` | `f5e166cf8b24f6695eb7f21219aab538358f0b09357c79c1242da2b565a62133` |
| `test_context_lifecycle.py` | `7d68a0d37d4236d0e1324a134679c03910417bec6d517942f6c04365e14bccb5` |
| `run_suite.py` | `7b0bef90fbe061003cbc3284f13020945193b8262e3eed618944f64d95c0d504` |
| `tmp/m1/fixture-store/v2/store.py` | `48cd729268eb185f1dfcba163fc8b6e0937d1e7a2a845573e41102fc0fcc1ce0` |
| `tmp/m1/fixture-store/v2/test_store.py` | `30ff6ac9d62af94238dcd3ebe5e4e9ef764c661cf2dffb639a1ee6c8824494b7` |
| `tmp/m1/fixture-store/v2/preparation-receipt.json` | `4859708afa18c031e51afebcf6fd1a11cb3f435b36d826ba689bc32b5b389146` |
| `tmp/m1/fixture-store/v2/run_small_suite.py` | `bb112640d452fb7fac1754c4db2221ad8104fa4fcf6a627adaaa3959a208d33e` |
| `tmp/m1/fixture-store/v2/static_check.py` | `d4a8cf1628a7c2906c172920642755ddf82c73ece226f4731b0a53d00f400eed` |
| `docs/evidence/M1-fixture-store-followup.md` | `2d29db958a6a36ce170f425439bf7e3b502874e532aef0b40f83b80fbdec48db` |

## Scope effettivo e limiti

Restano pianificati, non eseguiti: 24 regressioni v2, 18 lifecycle e 5 del supervisore.
Nessun test è stato saltato dalla suite: la suite non è partita. Non esiste un
conteggio unittest di skip/failure/error da trasformare artificialmente in zero PASS.
I rifiuti attesi dei test negativi non sono stati osservati. Il primo STOP è un
gate del preflight, non una loro expected negative né un difetto dimostrato dello store.

Le fixture dichiarano inventario, closure scientifica e binding precedenti sintetici.
I test del supervisore usano double deterministici; non provano D-state o processi reali.
Nessun ZIP, archivio autorevole, payload delle168 fixture o fase reale è stato aperto
o eseguito. Nessuna build, GUI, rete, installazione, indice o operazione Git.
Nessuna modifica a progress/index, sorgenti, ammissione o piano.

Prossimo incompleto: risolvere e rivedere il prerequisito directory privata, poi
ricontrollare tutti i gate non raggiunti prima di qualsiasi nuova esecuzione ammessa.
Questo record non autorizza correzioni, retry, suite alternativa o fase archive.
