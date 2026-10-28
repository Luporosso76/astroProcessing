# M1.f: regressione causale finish/record

## 1. Unità

M1.f, diagnosi unitaria del controller producer pattern16m. Stato della consegna:
`CANDIDATE_PENDING_REVIEW`. Nessuna approvazione del controller o della suite.

## 2. Scopo

Verificare che finish conservi anche la causa fsync quando il finally di record
solleva un distinto errore close, oltre alla primaria del controller già presente.
La review `ses_e8048933effeK930GxoS8nKVHI` ha indicato il MED e giudicato fedeli,
ma NOTREADY, i dieci mock precedenti. Questa ammissione riguarda un solo nuovo metodo.

## 3. Comando

Launcher derivato, unica invocazione:

```sh
/usr/bin/python3 -I -B -S tmp/m1/f-pattern16m-producer-persist-chain-red-test-001/outer.py
```

Unico comando figlio, senza `-f`:

```sh
/usr/bin/python3 -I -B -S tmp/m1/f-pattern16m-producer-001/test_controller.py AdversarialCollectionMocks.test_finish_preserves_fsync_cause_when_finally_close_also_fails
```

Il runner mantiene failfast=True. Nessuna suite o seconda invocazione eseguita.

## 4. Input

File correnti in `tmp/m1/f-pattern16m-producer-001/`, autenticati prima dell'import
e invariati durante l'esecuzione, SHA-256:

- `controller.py`: `9e58d6732248b4ffaceac151ced1ce0f68761d8164ccb003074245d940ceb8d6`.
- `test_controller.py`: `3f6b8167602b24de68dad111e6ce50a016eb0ca0bfda3e0e5528a81132351f4e`.
- `manifest.json`: `082fdea31ec1bff26c7c776e830eee3b8e369b8f43a5f5c197d767af3a330fd4`.
- `README.md`: `c8e1e253d8a5149662d45ffc905199da9153bfea134f6619e70eac3339ff5369`.
- `SHA256SUMS`: `db7a9764d0be6684179fd25adb0594bae295de6b047ccd5be2dcb4812a08d284`.

Snapshot dei cinque originali, pin precedenti e diff in
`tmp/m1/f-pattern16m-producer-persist-chain-test-prep-001/`.
Test precedente: `33fcea08a506c6a9e978ef2f9b3d6a45fe482619402d8f398426d68f1ac2ebcc`.
I 30 metodi precedenti, helper e import conservano byte/AST; il modulo ora contiene
31 metodi, ma ne è stato eseguito soltanto uno.

Outer002 immutato:
`378a2d9bc344ce1295691ada31714240a6c332c98ba410a0f75d5c87b1009456`.
Nuovo derivato:
`1cbfdb25fd328c22de5eb147e7ed370240d7ffbb5abc20e70597a3965dfa4aa4`.
`proof.json` verifica AST identico delle 19 funzioni non-main e del resto del modulo,
esclusi RUN, COMMAND e main. Cambiano soltanto binding e metadati ammessi: target,
conteggio AST31/selezione1, percorso e summary singolare. Nessun nuovo lifecycle.

La fixture usa sei eccezioni distinte, due per ciascuno dei tre tentativi.
finish e record sono reali; open/write/fsync/close sono interamente simulati.
Python crea naturalmente close.__context__=fsync; il test non assegna catene.

## 5. Versioni

Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.
Solo stdlib; nessun compilatore o candidato eseguito.
Piano v2.2.1 indicato dalle istruzioni del progetto, non letto o riautenticato qui.

## 6. Ambiente

Linux da `sys.platform`. Kernel, CPU, RAM, GPU/driver e tipo filesystem:
**non disponibili per questa prova**, senza scansione host.
Le sole misure filesystem necessarie ai gate usano st_blocks e statvfs.
HOME/TMP/XDG sono privati nel RUN, con umask 077.

## 7. Risultato

`REPRODUCED_FINISH_CAUSAL_LOSS`: fallimento sull'asserzione causale finale,
non `FIXTURE_UNEXPECTED`. Raw naturale: `Ran 1 test in 0.006s`, `FAILED (failures=1)`.
Il tempo è quello di unittest, non il wall time completo.

Prima dell'asserzione causale sono passati i controlli su primaria invariata,
tre close osservabili, tre chiamate record, contesti naturali, sei oggetti distinti,
assenza di figli/Popen e nessuna operazione controller non simulata.

Nei soli canali esposti da finish sono stati osservati:

- Primaria originale conservata, ValueError `original primary`.
- Tre secondarie: `close injected attempt 1`, `2`, `3`, nelle fasi persist dei tre record.
- Eccezione finale RuntimeError generica; nessuna causa fsync nella catena esposta,
  negli argomenti, nelle note o nell'eventuale attributo report.
- Stderr e stdout catturati vuoti; report UNRESOLVED, slot_may_release falso,
  candidate_launches e popen_attempts zero, collected falso.
- Diagnostiche complete=true, omitted=0, truncated=0: questi contatori descrivono
  i quattro riepiloghi osservati, non provano conservazione della catena raw.

Mancano tutte e tre le cause `fsync injected attempt 1`, `2`, `3`.
Il JSON diagnostico è **1984 byte**, con `expected_input` separato da `observed`.
Il controllo post ricalcola la perdita esclusivamente da `observed`, non cerca fsync
nel traceback unittest, nella fixture o negli input attesi.

Launcher PID **603891**, exit **2**; Python unittest PID **604002**, exit **1**.
Ricevuta: reaped=true, reserved=false, slot_blocked=false, nessun segnale reale.
Slot libero soltanto dopo questa raccolta reale del Python unittest, non di producer.
Il ledger primario esterno contiene il previsto errore summary senza OK;
cleanup e persistenza esterni hanno entrambi contatore zero.

## 8. Output

Preparazione e snapshot:
`tmp/m1/f-pattern16m-producer-persist-chain-test-prep-001/`.
Controllo, raw launcher, prova AST e osservazione JSON in `post.json`:
`tmp/m1/f-pattern16m-producer-persist-chain-red-test-001/`.
Raw unittest e ricevute:
`tmp/m1/f-pattern16m-producer-persist-chain-red-001/`.

- `stderr.log`, 3127 byte:
  `250526cbd9ecf077a777e701a7e4fde69c650a343294673c3b0923c3d188258e`.
- `stdout.log`, 0 byte:
  `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.
- `result.json` e `FIRSTFAIL.json`, 12663 byte ciascuno:
  `92fa2ea1617dcee575b65f6a95a600a44e3e3d1aa6506719154acbb9ac0b9196`.

Digest completi della consegna nel nuovo SHA256SUMS del controllo.

## 9. Ambito

Un solo regression test causale, senza fix, allentamenti, retry o skip.
Gli altri30 metodi, inclusi i19 baseline, non sono stati eseguiti.
Zero producer/candidati nativi, main del controller, dataset o build.
Nessun errore fsync/close OS reale o durability dimostrata: gli errori sono iniettati.

Sono rimasti invariati i137 record dell'inventario locale di conservazione
dell'autore e tutti gli11 FIRSTFAIL preesistenti. L'unico nuovo FIRSTFAIL è quello
del RUN RED. Incluse le precedenti prove RED/GREEN, setup/collection/adversarial
PREP, fonti005/approval e PREP7step. Il coordinatore dichiara15 file PREP autenticati:
questa prova **non estende l'autenticazione indipendente** ai137 record locali.

## 10. Esito

**FAIL del test; candidato RED causale `CANDIDATE_PENDING_REVIEW`.**
Non PASS, non approvazione della suite o del controller. MED ancora aperto, senza fix.

## 11. Limiti

Cap del Python unittest invariati: wall55 s, CPU50 s, AS268435456, FSIZE1048576, NOFILE64, CORE0;
log massimo1 MiB per canale. Core WNOWAIT/sessione propria/cleanup finito immutato;
nessun kill basato su ERROR/FAIL, traceback naturale conservato.
Nessun campionamento OS indipendente dei limiti o nuova osservazione del PID dopo reap.
Le deadline cooperative non rendono interrompibili syscall bloccate; questo caso non le qualifica.

Quota1073741824, riserva67108864 e margine16777216 separati; fase4194304,
totale setup più fase9437184, receipt_tail147456 byte.
Misure fisiche deduplicate `(dev, ino)`, `st_blocks*512`, nofollow:

- Prima del setup U938119168, fsfree81971298304.
- Preflight esterno U938332160, fsfree78959366144.
- Preflight interno U938340352, fsfree78791262208.
- Post interno U938360832.
- Post esterno prima della ricevuta U938401792, fsfree73922899968.

Al post esterno: crescita fase più tail208896<=4194304 e totale430080<=9437184.
Gate globale e fsfree superati con riserva/margine liberi. `final-audit.json`
registra il controllo conclusivo, conservando spazio per gli ultimi artefatti.
Snapshot cooperativi, non prenotazioni atomiche né ammissione futura del producer.
Envelope38993920, ceiling/evidenza fisica NULL, runproducer0 e readyfalse invariati.

## 12. Autore e data

Agente esecutore del turno, GPT-6.1 Sol, provider OpenAI, modello dichiarato `openai/gpt-6.1-sol`.
2026-10-28. Nessuna delega o override. Costi/token non disponibili dal runtime.
Letture correnti per hash/AST: controller793 righe, test805, manifest97, README115,
SHA256SUMS4; derivato375 e outer002374. Formato evidence letto da README185 righe.
Questi conteggi non attestano una review indipendente di tutti i record preservati.
