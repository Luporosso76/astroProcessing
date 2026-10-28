# M1.f: errore di setup senza diagnosi esterna

Esito: `REPRODUCED_KNOWN_DIAGNOSTIC_DEFECT`, **non PASS**.
Il solo nuovo test è stato eseguito una volta e fallisce sul contratto diagnostico.
Nessuna correzione al controller, nessun retry, skip o expectedFailure.
I 19 test preesistenti non sono stati eseguiti. Zero producer e candidati nativi.

## Comandi ammessi ed eseguiti

Launcher derivato:

```sh
/usr/bin/python3 -I -B -S tmp/m1/f-pattern16m-producer-setup-test-001/outer.py
```

Unico comando figlio:

```sh
/usr/bin/python3 -I -B -S tmp/m1/f-pattern16m-producer-001/test_controller.py SetupFailureMocks.test_setup_failure_without_output_retains_visible_diagnosis
```

Il failfast resta quello interno del file. Nessun `-f`, main del controller o producer005 eseguito.
Python: `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`, Linux.

## Osservazione che riproduce il difetto

Il mock rende riusciti preflight e prctl, simula una lettura proc senza figli
iniziali e restituisce il parentfd finto 917. Il successivo mkdir runtime solleva
`PermissionError('setup mkdir injected')`. La preflight fuori dal try non è il
percorso fallito: il test ne verifica la riuscita e il successivo ingresso nel setup.

Il traceback naturale mostra il fallimento dell'ultima asserzione, dopo i controlli
del percorso e dei divieti di effetti reali. I fatti acquisiti durante `run()` sono:

- Ritorno **1**, nessuna eccezione propagata, stdout e stderr entrambi vuoti.
- **0** chiamate a finish e record; **0** ricevute simulate emesse.
- Causa primaria nel solo report locale: fase `controller`, tipo `PermissionError`,
  messaggio `setup mkdir injected`; stato locale finale `UNRESOLVED`.
- **0** chiamate Popen, **0** candidate_launches, launch_attempted falso e child assente.
- Nessuna apertura dell'outfd; il solo close del parentfd è simulato e verificato.

La causa nel report locale è visibile al test tramite stato condiviso, ma non al
chiamante di `run()`. Non viene quindi usata per soddisfare l'asserzione sulla
diagnosi esterna. Il JSON nel traceback è emesso dall'asserzione del test dopo il
ritorno di `run()`: non è una diagnosi prodotta dal controller.

CDLL/prctl, lettura proc, directory, mkdir, open/close/fsync, record, snapshot,
Popen, raccolta e segnali sono mock o sentinelle. Il preflight producer è simulato,
non superato realmente. Nessun gate fisico producer è stato aggirato o qualificato.
`collected=false` resta il valore iniziale: non prova l'esistenza di un figlio né
una raccolta. Zero lanci è dimostrato anche dalla spy Popen e dai controlli sul percorso.

## Exit, raccolta e diagnosi del launcher

Raw unittest: `Ran 1 test in 0.004s`, `FAILED (failures=1)`.
Il tempo è quello riportato da unittest, non il wall time dell'intero launcher.

Launcher PID **75048**, exit **2**. Python unittest PID **75049**, exit **1**.
La ricevuta del launcher registra `reaped=true`, `reserved=false`,
`slot_blocked=false`, nessun segnale inviato. Lo slot del Python unittest è libero
dopo questa raccolta reale. Non viene dichiarata alcuna raccolta di producer.

Il ledger primario esterno contiene il solo errore atteso del controllo summary:
`RuntimeError: STOP: selected-method natural unittest summary absent`.
Il test RED non termina con OK; non è stato forzato a PASS.
Ledger cleanup e persistenza esterni: entrambi **0**.
Il traceback completo e il FIRSTFAIL del launcher sono conservati in
`tmp/m1/f-pattern16m-producer-setup-red-001/`.
La conferma di raccolta viene dalla ricevuta e dal core waitid qualificato;
non è stata inventata una seconda osservazione OS del processo.

## Snapshot e pin prima/dopo

Prima delle modifiche sono stati copiati tutti i cinque file PREP in
`tmp/m1/f-pattern16m-producer-setup-test-001/before/` e autenticati con i pin ricevuti.
`before.json`, `proof.json` e `post.json` conservano hash, dimensioni e mode.

- Controller invariato:
  `f7ac591210356d40f2065be6a2ffdb47ff8851c14716cded27217fb2301bed5d`.
- README invariato:
  `c286afad5aeb660801e44eb60d42beaf9c469e0afd06572ffd11994d615de8cf`.
- Test prima: `f3142097a5adf84c433136cdb35ae1fafdfa2ef1e773e3ab28e83ed095a4e60d`;
  dopo: `0b911cf919c18801661907a03406cd20fa8857c394a03681e3a08bf5f99c1dc0`.
- Manifest prima: `cf22a50aa44e12f61cfe1334481d723d908561871b6bb4422cc0a93a314612b2`;
  dopo: `d3824d84ead5c5dfefebeb4966626785ebe4fd307441cc57974b38964f8fa2c7`.
- SHA256SUMS prima: `874443769defe4e92c6eea53001ccb7527380dcd08fef1b28d72ec1ba6afde03`;
  dopo: `e2bf9c38fe5de6b7fdd9d241923c78d76d543d9b984b637b29e97a07f467edd3`.

Il manifest cambia solo nel pin del test; SHA256SUMS solo nei due pin corrispondenti.
Status `PREP_PENDING_INDEPENDENT_REVIEW`, run_count 0, ready_run false e tutti i
campi ceiling/evidence/current capacity null restano invariati.
Gli otto file di `followup-collection-001/` sono invariati.
I 19 metodi preesistenti conservano byte e AST. Il modulo precedente conserva AST
identico rimuovendo soltanto la nuova classe. AST totale **20** test, selezionato **1**.
I cinque pin aggiornati sono stati verificati prima dell'import e dopo l'esecuzione.

## Derivazione del launcher e protezioni

Fonte outer002 invariata:
`378a2d9bc344ce1295691ada31714240a6c332c98ba410a0f75d5c87b1009456`.
Derivato:
`4504155bca5e4cc37e25e45ae82b5928ecbc080b9cc3370ba3b1929235f8b051`.

RUN e COMMAND cambiano; main cambia soltanto percorso AST, conteggio totale 20,
selezione 1, metadati e summary singolare. Tutte le **19 funzioni non-main** hanno
AST identico alla fonte, così come i restanti statement nonmetadata.
BASE ed EXPECTED originali restano invariati, proteggendo anche i 17 file del
contratto precedente. I nuovi cinque pin producer sono autenticati dal controllo
esterno prima e dopo; non si attribuisce questa verifica alla funzione protected originale.
`outer.patch` e `proof.json` conservano il confronto effettivo.

L'inventario dinamico ha trovato **10** FIRSTFAIL preesistenti, tutti invariati.
L'unico nuovo FIRSTFAIL è quello atteso nel RUN RED. Nessun conteggio globale è
stato fissato in anticipo a 10. Gli storici non sono stati modificati.

## Limiti e spazio di questo solo test Python

Limiti figli invariati: wall 55 s, CPU 50 s, AS 268435456, FSIZE 1048576,
NOFILE 64, CORE 0; HOME/TMP/XDG privati nel progetto, umask 077.
Log massimi 1 MiB ciascuno; traceback naturale senza kill su parole FAIL/ERROR.
Budget fase 4194304, setup complessivo conservativo 9437184,
receipt_tail 147456, quota globale 1073741824, riserva 67108864 e margine 16777216.

Misure correnti deduplicate `(dev, ino)`, `st_blocks*512`, nofollow:
U prima del setup **937263104**; preflight esterno **937443328**;
preflight interno **937451520**, filesystem disponibile **84102463488**;
post interno **937472000**; post esterno prima della ricevuta **937504768**,
filesystem disponibile **83820539904** byte.
Al post esterno, crescita fase più tail **200704 <= 4194304**;
crescita totale più tail **389120 <= 9437184**. Quota, riserva, margine e
filesystem residuo hanno superato i controlli. Il controllo conclusivo è in `final-audit.json`.

Queste sono misure cooperative del test Python, non ammissione futura del producer,
prenotazione atomica, ceiling fisico qualificato o prova di durability.
Nessun errore OS di permessi reale è stato osservato: PermissionError è iniettato.
Nessuna misura storica è stata riusata come capacità corrente.

Consegna congelata dal nuovo SHA256SUMS nella directory di controllo.
Unica invocazione conclusa; nessun ulteriore runtime autorizzato o eseguito.
