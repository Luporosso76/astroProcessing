# M1.a: GREEN della diagnosi annidata, turno 001

## Esito

26 ottobre 2026. `PASS_MOCK_PENDING_INDEPENDENT_REVIEW`.
Target8 PASS exit0, poi suite55 PASS exit0, senza skip o retry. Sono 55 metodi
unici e 63 esecuzioni, non 63 casi distinti. AST, loader e conteggio del runner
concordano: 22 ProtocolTests, 16 LifecycleTests, 17 CombinedFailureTests.

Il benchmark applicato, 39596 byte, conserva SHA-256
`3dbd3fe291b8fa1641df46ba3be5235f4d142b4ff93a4a1351ed9a7f05ba2829`.
Il RED precedente sul benchmark6a aveva dimostrato la perdita della diagnosi;
questo turno dimostra la conservazione sul fix applicato, con gli assert sui
wrapper richiesti dal contratto. La review del delta test e delle prove resta
indipendente e pendente. Nessuna approvazione nativa, prestazionale o chiusura M1.

## Modifiche e copertura

Modificati solo test_protocol.py e manifest.json, oltre ai nuovi namespace/report.
Benchmark, proposta applicata, PLAN, supervisore e driver sono invariati.

La regressione mantiene TextIOWrapper/BufferedWriter/raw faulting, tre raw.write,
catena Python naturale wrapper/wrapper/originale, registro allegato prima del
wrapper, STOP exit2 e blocco del lancio mock. Primaria e receipt_errors devono
restare identici al registro originale. Teardown deve essere esattamente:
originale invariato, wrapper intermedio, wrapper esterno. I due record hanno
phase `finalization.exception-wrapper`, exception_role `wrapper`, tipo OSError,
messaggi esatti e catene context rispettivamente di uno e due elementi.
Il test confronta l'intera lista e verifica record distinti. stdout conserva il
registro prima e il FIRSTFAIL mock dopo, senza affidarsi al solo attributo Python.

Sette metodi aggiunti, tutti caricati ed eseguiti:

- Branch cause/context: recupero del registro sul context anche con cause presente.
- Registro condiviso: primaria e secondarie non duplicate; wrapper preservato.
- Registri distinti: primaria del cause prioritaria, altra primaria classificata
  `additional-primary`, entrambe le liste secondarie preservate esattamente.
- Ciclo: terminazione e flag cycle con registro preservato.
- Limite64: truncated esplicito, primaria null se il registro è oltre il limite,
  record chain-limit e nessun contenuto inventato dal registro non visitato.
- error_record: ciclo e catena65, confronto completo delle cause e dei flag.
- FIRSTFAIL non scrivibile su catena naturale annidata: exit2, stderr entro65536,
  messaggio wrapper100000 caratteri limitato a1024, primaria e secondarie originali
  preservate, errore ricevuta aggiuntivo e blocco dei lanci.

L'helper preesistente che confronta process.json e FIRSTFAIL ora richiede anche
il nuovo `chain_scan={limit:64,truncated:false,cycle:false}`. Nessun campo originale
viene escluso dal confronto; non è un subset permissivo. Gli altri assert storici
restano invariati. I grafi artificiali servono soltanto ai test puri di traversal;
la regressione IO e il test di persistenza fallita usano cause/context naturali.

## Comandi effettivi

```sh
python3 -I -B -S tmp/m1/fft-pocket-benchmark/primary-error-chain-green-tests-001/outer.py
```

Il launcher ha eseguito in sequenza, con failfast configurato nel test:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket-benchmark/test_protocol.py CombinedFailureTests.test_nested_results_close_preserves_failure_ledger CombinedFailureTests.test_chain_branch_cause_and_context CombinedFailureTests.test_chain_shared_ledger_deduplicates CombinedFailureTests.test_chain_distinct_ledgers_primary_priority CombinedFailureTests.test_chain_cycle_terminates_with_ledger CombinedFailureTests.test_chain_limit_does_not_invent_unseen_ledger CombinedFailureTests.test_chain_error_record_cycle_and_limit CombinedFailureTests.test_chain_firstfail_write_failure_keeps_nested_ledger
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket-benchmark/test_protocol.py
```

Il secondo comando è partito soltanto dopo exit0, otto `ok` e nessuno skip del
primo. Non sono stati inviati segnali per parole ERROR/FAIL. Nessuna ulteriore
invocazione unittest, microprobe, compilazione, worker o benchmark.

## Ammissione e processi reali

Dodici pin autenticati prima dell'import; il test li ricontrolla prima di caricare
benchmark.py. Prima/dopo target e prima/dopo suite riportano gli stessi quattro
pin di benchmark, test, manifest e PLAN. Manifest runtime:
`d150123f76c8123f40bc0954e10807e95edef0a27e5d106e8480cf96c7573874`.
Il manifest finale differisce solo per la registrazione dell'esito successiva.

Preflight correnti globali tmp, st_blocks e hardlink deduplicati:
935075840 byte prima del target, 935100416 prima della suite.
Quota1073741824, riserva67108864, margine16777216; budget2424832 per fase,
derivato da `2*1048576 + 4*65536 + 16*4096`. Prima di creare il namespace il
launcher verifica anche la capienza per entrambi i budget. Nessuna riserva nativa.

Per interprete: wall55 s, CPU50 s, AS268435456, FSIZE1048576, NOFILE64, CORE0.
Umask077, ROOT0700, HOME/TMP/XDG privati; stdout/stderr ciascuno entro1 MiB.
Cap resource applicati prima dell'exec, non ricampionati dentro il figlio.
Linux, Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.

| Fase | PID | Esito | Tempo unittest | Durata outer |
| --- | ---: | --- | ---: | ---: |
| target | 1271679 | 8 PASS, exit0 | 0.012 s | 0.20322644687257707 s |
| suite | 1271685 | 55 PASS, exit0 | 0.125 s | 0.33506565587595105 s |

Entrambi: WNOWAIT e verifica identità/sessione/gruppo, reap consumante riuscito,
reserved=false, signals_closed=true, slot_blocked=false, timed_out=false,
termination=[]. Nessun segnale reale. Il launcher verifica assenza di discendenti
vivi nel proprio gruppo prima del reap. Rami timeout TERM/KILL non esercitati.
Questi tempi descrivono test Python, non prestazioni FFT.

## Artefatti e hash finali

Snapshot/diff/outer: `tmp/m1/fft-pocket-benchmark/primary-error-chain-green-tests-001/`.
Ricevute distinte: `primary-error-chain-green-001/target/` e `suite/`, ciascuna con
before.json, result.json, stdout.log e stderr.log. Nessun overwrite/autoname.

| File | SHA-256 |
| --- | --- |
| test_protocol.py | `36df80306fc70236043da7105fc48a50a2c92fb6033e30bdac3a2a522cf83c6e` |
| manifest.json | `f74bf0c7ac780aed6b9b1bf003c656d975a0a0023c5858aad0e8030c770e0858` |
| PLAN.md, invariato | `dac8f28071c9abf285caac7209193039a158d15206ad161ce7f4158837c61463` |
| source.diff | `43b9b915e0a3555d1fe32f523e927f3c4adfee056ca062ae0eff046c5df3e2a7` |
| outer.py | `fc401d7fc2b854cea16e9880863e968ab2e793de7f407d57958a340af6ec0403` |
| target/result.json | `927be2e324f9179c5cd35f8edff76d4ced3c986b96af50942392a2282b1184d8` |
| suite/result.json | `6590e136b7941874f66187dc0502b2a3dc08028d813becc19efe1622c88a5ad3` |
| target/stderr.log | `250ccecb71ea705e626c3eae443d26cbf7fad497cbf2407a49c379eb7051be77` |
| suite/stderr.log | `bf620da003f1c19596552fdc45f6c15b649f65a68f28cd031631235a9efac96a` |
| stdout.log, identico nelle due fasi | `57123c8f7599d587cec3a608696061cfed6f2dd6142a0f6e0a4d0e51e3691cdb` |

238 file protetti verificati invariati. Tutti i nove FIRSTFAIL restano identici;
nessun nuovo FIRSTFAIL reale. Il sottoinsieme storico7 conserva aggregato
`5c32e5fa5d456d46df2443a550d69b4a44ee1d16a6d9522d8c2fc1f6bf37817c`.
Prove38/47, red001 inatteso e red002 riprodotto conservano sorgenti ed esiti propri.
Pin completi e verifiche in `primary-error-chain-green-tests-001/after.json`.

I writer delle ricevute e gli OS/Popen del benchmark sono mock. La serializzazione
JSON non dimostra durabilità filesystem, teardown nativo o scienza192. Nessun Git,
rete, installazione, indice, delega o override modello. Modello runtime dichiarato
`openai/gpt-6-astra`; costi/token non esposti. Review indipendente ancora richiesta.
