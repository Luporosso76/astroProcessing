# M1.f: unico regression test del setup, GREEN

Esito: `TARGETED_GREEN`. Unica invocazione del metodo autorizzato: **1 test, OK**.
Scope: **unico regression test**, non suite, qualifica completa o producer ready.
Nessuna modifica a sorgenti, fixture, asserzioni, manifest, README o checksum canonici.
Nessun retry, skip, probe, secondo run del controller o candidato nativo.

## Fonti autenticate prima dell'import

Ammissione dopo review indipendente `READY_FOR_TARGETED_GREEN`, sessione
`ses_e8070867dffe1fWCXZ651O041W`.
Autenticati i cinque file correnti più gli otto file del followup setup: **13 file**.
Gli stessi pin sono stati verificati dopo l'esecuzione.

SHA-256 correnti, invariati durante questo incarico:

- `controller.py`, 35953 byte:
  `9e58d6732248b4ffaceac151ced1ce0f68761d8164ccb003074245d940ceb8d6`.
- `test_controller.py`:
  `0b911cf919c18801661907a03406cd20fa8857c394a03681e3a08bf5f99c1dc0`.
- `manifest.json`:
  `fdd0a497abdb071e60ef8b3679652ad470a7e48c9ba8c6852f7e04cc92a6345a`.
- `README.md`:
  `3114291c977799f334d95fbd89817f4fd0c1d2a56b0e464566ed41ab8d460949`.
- `SHA256SUMS`:
  `91c0ecb9e9c0c9cff9ebf5289bd7894e2908fd4f5064d05b66189ca804300144`.
- `followup-setup-diagnostic-001/SHA256SUMS`:
  `4ec7b7cba5b780e508e474cc43a7712f5053ac69251f914125d09c82fdf3fb13`.
- `followup-setup-diagnostic-001/setup-diagnostic.patch`:
  `41e3eb054525607bd90ba8d2d520cd84428c62d16d1c07fd5663c37d06bdf747`.

Il test è byte-identico al RED: tutti i 20 metodi conservano quindi anche l'AST.
Conteggio AST verificato: **20**; selezionato **1**; **19 baseline non eseguiti**.
I cinque file correnti sono anche copiati nel nuovo `before/`, senza alterare gli originali.

## Launcher e comando unico

Directory di controllo: `tmp/m1/f-pattern16m-producer-setup-green-test-001/`.
RUN: `tmp/m1/f-pattern16m-producer-setup-green-001/`, creato soltanto da main.
Nessuna collisione o autorename.

Comando launcher:

```sh
/usr/bin/python3 -I -B -S tmp/m1/f-pattern16m-producer-setup-green-test-001/outer.py
```

Unico comando figlio:

```sh
/usr/bin/python3 -I -B -S tmp/m1/f-pattern16m-producer-001/test_controller.py SetupFailureMocks.test_setup_failure_without_output_retains_visible_diagnosis
```

Nessun `-f`; resta il failfast interno. Il controller main e producer005 non vengono eseguiti.
Python osservato: `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`, Linux.

Fonte outer002 immutata:
`378a2d9bc344ce1295691ada31714240a6c332c98ba410a0f75d5c87b1009456`.
Base derivata del RED immutata:
`4504155bca5e4cc37e25e45ae82b5928ecbc080b9cc3370ba3b1929235f8b051`.
Nuovo derivato GREEN:
`84b9ac9e38b6f53d053974b161d546c6010404aae141007090dddcf03e9da85c`.

Dal derivato RED cambia solo il binding RUN. Il confronto con outer002 conferma
AST identico per tutte le **19 funzioni non-main** e il resto del modulo esclusi
RUN, COMMAND e main. Main conserva soltanto gli adattamenti metadata già ammessi:
percorso AST, totale 20, selezionato 1 e summary singolare.
Nessuna nuova logica di lifecycle, ownership, cleanup, persistenza o limiti.
BASE/EXPECTED originali proteggono anche il PREP17; i pin producer sono autenticati
dal controllo esterno. Diff e prova AST sono in `outer.patch` e `proof.json`.

## Esito e confronto con il RED

Raw naturale: `Ran 1 test in 0.003s`, `OK`.
Il tempo è quello di unittest, non il wall time completo del launcher.

Launcher PID **260399**, exit **0**. Python unittest PID **260405**, exit **0**.
Ricevuta: `reaped=true`, `reserved=false`, `slot_blocked=false`, nessun segnale reale.
Ledger primaria, cleanup e persistenza esterni: tutti **0**.
Lo slot del Python unittest è libero dopo questa raccolta reale; nessuna raccolta
di producer viene dichiarata.

L'orchestrazione richiede ora esplicitamente GREEN: exit zero, summary singolare OK,
raccolta confermata e invarianti. Non applica più l'attesa RED della precedente consegna.
La ricevuta originale mantiene `CHECKS_COMPLETE` e `final_success_attested=false`;
PASS finale su stderr, exit zero e controlli post completano la prova di questa esecuzione.

Nel RED lo stesso metodo aveva osservato ritorno 1, stderr/stdout vuoti, nessuna
eccezione propagata e nessuna ricevuta: la primaria restava solo nel report locale.
Nel GREEN passano le asserzioni immutate su causa primaria, percorso mkdir fallito,
assenza di outfd/Popen/producer e osservabilità esterna della diagnosi tramite i
canali già presenti nel test: stderr, eccezione catturata o ricevuta simulata.
`run()` è chiamato una sola volta, sotto gli stessi mock e sentinelle.

Il test non emette il dizionario `observation` quando passa: quel dizionario era
visibile nel traceback RED dell'asserzione fallita. Non sono stati aggiunti tracing,
probe o una seconda chiamata per estrarre i locali del GREEN.
Gli attributi `SetupDiagnosticError.report`, `receipt_persisted` e l'identità esatta
dell'oggetto report non sono asseriti separatamente da questo metodo: la loro
implementazione è leggibile nella patch autenticata, non una nuova misura runtime qui.
Il GREEN dimostra il contratto di visibilità già asserito, senza estenderne il perimetro.

## Conservazione

Verificati prima/dopo, con hash, dimensioni e mode:

- Tutti i **21 file** producer, inclusi gli otto snapshot collection e gli otto setup.
- **24 file** della consegna RED, incluso report, raw, ricevute e snapshot originali.
- I **17 file PREP** del contratto e i **6 file launcher002**.
- I **9 pin** fonte005/approval/invocazione referenziati dal manifest, senza import
  dei candidati o nuova qualifica della relativa approval.
- Tutti gli **11 FIRSTFAIL** trovati dinamicamente; nessun FIRSTFAIL aggiunto o modificato.

`initial.json` conserva i pin iniziali; `post.json` conserva il confronto e le
osservazioni effettivamente disponibili. Nessun file canonico, progress o indice toccato.

## Limiti e preflight correnti

Cap figli invariati: wall **55 s**, CPU **50 s**, AS **268435456**, FSIZE **1048576**,
NOFILE **64**, CORE **0**. HOME/TMP/XDG privati nel progetto, umask 077.
Log massimi 1 MiB per canale; nessun kill basato su parole ERROR/FAIL.
WNOWAIT, sessione propria e reap restano quelli del core qualificato.

Quota globale **1073741824**, riserva **67108864**, margine **16777216** separati.
Budget fase **4194304**, totale setup più fase **9437184**, receipt_tail **147456**.
Conteggio fisico deduplicato `(dev, ino)`, `st_blocks*512`, senza seguire symlink.

Misure di questa esecuzione Python:

- Prima del setup: U **937652224**, filesystem disponibile **82909974528** byte.
- Preflight esterno: U **937795584**, disponibile **82909831168** byte.
- Preflight interno: U **937803776**, disponibile **82909822976** byte.
- Post interno: U **937824256**.
- Post esterno prima della relativa ricevuta: U **937848832**,
  disponibile **82909814784** byte.

Al post esterno: crescita fase più tail **192512 <= 4194304**;
crescita totale più tail **344064 <= 9437184**. Quota, riserva, margine e fsfree
superano i gate; il controllo conclusivo dopo il report è in `final-audit.json`.
Questi snapshot non sono prenotazioni atomiche, prove di durability o campionamenti
OS indipendenti dei limiti/attributi dell'eccezione. Nessun dato storico ammette il test.

## Stato e limiti aperti

Questo è soltanto il GREEN del regression test precedentemente RED.
I 19 baseline e gli adversarial waitpid/close/bounded diagnostics restano aperti.
L'ipotesi LOW su primary None richiede qualifica separata; non è stata testata,
corretta con placeholder o usata per autorizzare altro runtime.
Ceiling fisico ed evidenza restano NULL, capacità producer non attestata,
run_count producer **0**, ready_run false. Nessuna qualifica completa o producer ready.

Consegna congelata dal nuovo `SHA256SUMS` nella directory di controllo.
Nessun'altra invocazione dopo l'unico regression test.
