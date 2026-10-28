# M1.f: unico GREEN causale finish/record

## 1. Unità

M1.f, regression test mock della diagnosi causale in finish/record.
Risultato unico: `PASS_MOCK_TARGET1`. Non è approvazione del controller o della suite.

## 2. Scopo

Eseguire una sola volta lo stesso target che aveva riprodotto la perdita delle
cause fsync, senza modificare test, fixture o asserzioni.
Ammissione dopo review `ses_e8048933effeK930GxoS8nKVHI`, `READY_FOR_TARGETED_GREEN`.
Il coordinatore dichiara autenticazione14 file e AST33 simboli immutati.
Le verifiche locali dell'autore non estendono quella dichiarazione all'intero archivio.

## 3. Comando

Launcher, unica invocazione:

```sh
/usr/bin/python3 -I -B -S tmp/m1/f-pattern16m-producer-persist-chain-green-test-001/outer.py
```

Unico comando figlio:

```sh
/usr/bin/python3 -I -B -S tmp/m1/f-pattern16m-producer-001/test_controller.py AdversarialCollectionMocks.test_finish_preserves_fsync_cause_when_finally_close_also_fails
```

Nessun `-f`; failfast=True resta nel codice. Nessun'altra invocazione, suite o retry.

## 4. Input

File in `tmp/m1/f-pattern16m-producer-001/`, SHA-256 prima/dopo identici:

- `controller.py`: `b98b579896b73fb1e361bb6af5ff6bbf69f1d64da3dc2fe7a424ba23873da62a`.
- `test_controller.py`: `3f6b8167602b24de68dad111e6ce50a016eb0ca0bfda3e0e5528a81132351f4e`.
- `manifest.json`: `edb6567c22ecce42333291e18b320a726bfaeba3040de0d700bf89b72ca75df2`.
- `README.md`: `54373390d7d7174e598be6a2ca0476f9d481f1921d07a0a71e4fc0fbbf205989`.
- `SHA256SUMS`: `9259272c6dff23879af8786c0020b4cf17bf3d7a76f07af47473e2f1cfdd0649`.

Autenticati anche tutti gli otto entry del followup e il relativo SHA256SUMS,
per14 file correnti complessivi:

- `followup-persist-chain-001/SHA256SUMS`:
  `41f4f7dff9b230f09bb36da549402815962c8e72ac0ddc4b416b067ebf57ea95`.
- Diff `followup-persist-chain-001/persist-chain.patch`:
  `b4e2b22abf342ce4f17fc4ecbeeb5873f3df0fa5250567291f431628fbd43639`.

I31 test e gli helper/import restano byte-identici al RED. Conteggio AST31,
selezione1: non31 test eseguiti.
Il target chiama finish e record reali con OS interamente simulato, sei eccezioni
distinte e contesti creati naturalmente da Python, senza assegnare catene.

Outer002 immutato:
`378a2d9bc344ce1295691ada31714240a6c332c98ba410a0f75d5c87b1009456`.
Base RED immutata:
`1cbfdb25fd328c22de5eb147e7ed370240d7ffbb5abc20e70597a3965dfa4aa4`.
Nuovo derivato:
`31b1b597bcb6c5a4a070f2f9f4145d034c3410eff3ebc7225014c7539927a4cc`.
Dal RED cambia solo RUN. `proof.json` dimostra AST identico delle19 funzioni non-main
e del resto del modulo esclusi RUN/COMMAND/main. Main mantiene i soli adattamenti
metadata già ammessi: percorso AST, totale31, target1 e summary singolare.
Nessuna modifica al lifecycle, ai limiti o alla persistenza del launcher.

## 5. Versioni

Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`; solo stdlib.
Piano v2.2.1 indicato nelle istruzioni, non letto o riautenticato in questa prova.
Nessun GCC, build, Landlock o candidato eseguito.

## 6. Ambiente

Linux da sys.platform. Kernel, CPU, RAM, GPU/driver e tipo filesystem:
non disponibili, senza scansione host.
Turno esclusivo dichiarato dal coordinatore e file immutabili verificati pre/post;
nessun censimento OS indipendente della quiescenza.
HOME/TMP/XDG privati sotto il RUN, directory0700 e umask077.

## 7. Risultato

Raw naturale: `Ran 1 test in 0.004s`, `OK`.
Il tempo è quello riportato da unittest, non il wall time dell'intero launcher.
Launcher PID **925787**, exit **0**; Python unittest PID **925794**, exit **0**.
Ricevuta: reaped=true, reserved=false, slot_blocked=false, nessun segnale reale.
Ledger primaria/cleanup/persistenza del launcher: tutti **0**.
Slot libero dopo la raccolta reale del Python unittest; nessun reap di producer dichiarato.

Lo stesso metodo passa ora tutte le asserzioni: primaria immutata, tre close
osservabili, contesti fsync/close naturali, assenza di figli/Popen e preservazione
anche delle tre cause fsync nei soli canali esposti da finish.
Gli errori iniettati e lo stato UNRESOLVED della simulazione sono attesi: PASS
del test non significa successo delle scritture simulate o slot producer rilasciato.

Nel RED le tre cause fsync mancavano da report/eccezione/stderr osservabili;
la medesima asserzione falliva con exit1. Nel GREEN la lista missing è asserita vuota.
Il metodo non emette il pacchetto observation quando passa: non sono quindi
serializzati separatamente le righe del report GREEN o il suo grafo eccezioni.
Non si inventano quei valori, né si estraggono tramite tracing, probe o secondo run.
La prova deriva dalle asserzioni immutate, non da una ricerca nel traceback unittest
o nei token della fixture/input attesi.

## 8. Output

Controllo, snapshot dei cinque file, AST equivalence e raw launcher:
`tmp/m1/f-pattern16m-producer-persist-chain-green-test-001/`.
Raw unittest e ricevute:
`tmp/m1/f-pattern16m-producer-persist-chain-green-001/`.

- `before.json`, 7146 byte:
  `e8e39c546163e9ec8bc73b1b3214083a0debbcd2205b2719fec30b172a987969`.
- `result.json`, 13278 byte:
  `809c87c294cd67743c86c2358143aac0bf7789f07f342605e285fa40e1709c06`.
- `stderr.log`, 98 byte:
  `a626407facf1102b512cc38f00f6e7d889aa6280a5c03d1722ac6d1c4eb08dc5`.
- `stdout.log`, 0 byte:
  `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.

La ricevuta originale usa CHECKS_COMPLETE e final_success_attested=false per non
dichiarare successo prima del proprio fsync/close. Diagnosi finale PASS, exit zero
e controlli post sono conservati in aggiunta. `post.json`, `final-audit.json` e
SHA256SUMS del controllo congelano osservazioni, preservazione e artefatti.

## 9. Ambito

Unico regression test mock causale. Altri30 test NOT_RUN,19 baseline NOT_RUN,
producer0 RUN. Nessuna suite, operazione producer/CLI/dataset, policy o unlink reale.
Nessuna modifica a sorgenti, manifest, README, checksum canonici, progress o indice.

Inventario locale dell'autore: **177 file** invariati, inclusi PREP17, launcher6,
fonti005/approval, followup/snapshot e precedenti prove RED/GREEN/unit.
Tutti i **12 FIRSTFAIL** preesistenti, contati dinamicamente, restano invariati;
nessuno nuovo. Queste sono verifiche locali di preservazione, non autenticazione
indipendente dell'intero archivio. I14 file correnti sono verificati prima dell'import e dopo.

## 10. Esito

**PASS_MOCK_TARGET1**, limitato al target e ai pin sopra indicati.
Nessuna ammissione automatica a un altro turno o approvazione della suite31/controller.

## 11. Limiti

Cap del figlio Python: wall55 s, CPU50 s, AS268435456, FSIZE1048576, NOFILE64, CORE0;
log massimo1 MiB per canale. OWN/WNOWAIT/identity/TERM-KILL/reap sono quelli del core
qualificato immutato; nessun kill su parole ERROR/FAIL. Nessuna nuova osservazione
OS del PID dopo reap o campionamento indipendente dei limiti.

Quota1073741824, riserva67108864 e margine16777216 separati; fase4194304,
totale setup più fase9437184, tail147456 byte.
Misure fisiche correnti deduplicate `(dev, ino)`, st_blocks*512, nofollow:

- Prima del setup U938618880, fsfree73737404416.
- Preflight esterno U938819584, fsfree73757999104.
- Preflight interno U938827776, fsfree73757990912.
- Post interno U938848256.
- Post esterno prima della ricevuta U938872832, fsfree73757945856.

Al post: crescita fase più tail192512<=4194304; totale401408<=9437184.
Quota globale e fsfree superati lasciando riserva/margine liberi; l'ultimo controllo
è in final-audit.json, con spazio per gli ultimi artefatti. Il valore storico
U938418176 non è stato usato per ammettere la prova. Snapshot cooperativi, non prenotazioni atomiche.

Nessuna durability OS/FS, ceiling fisico o liveness di syscall bloccate qualificati.
Cause/context distinti, nodi condivisi/cicli, profondità/cap, saturazione, troncamento,
JSON worst-case e altre aspettative della suite richiedono prove/review separate.
Envelope38993920, ceiling/evidenza fisica NULL, runproducer0 e readyfalse invariati.

## 12. Autore e data

Esecutore del turno, GPT-6.1 Sol, provider OpenAI, modello dichiarato `openai/gpt-6.1-sol`;
2026-10-28. Nessuna delega o override. Costi/token non disponibili dal runtime.
Letture per hash/AST: controller823 righe, test805, manifest99, README137,
SHA256SUMS4, derivato375 e outer002374; patch del followup152 righe letta.
I conteggi non attestano review indipendente dell'intero inventario.
