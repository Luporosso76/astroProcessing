# M1.b: regressioni mock del supervisore008

Esito: `PASS_MOCK_ONLY`. Un solo turno ammesso, una sola esecuzione della suite:
**41 PASS, 0 failure, 0 errori, 0 skip**. Sono i 27 casi precedenti e i 14 nuovi
casi008. Il log conserva l'esito individuale di tutti i 41 metodi; anche
`execution-receipt.json` contiene nomi ed esiti. Nessun retry, fix o salto.
Questo turno non aggiunge test e non dichiara un precedente risultato RED.

Ammissione ricevuta dal coordinatore: review indipendente `READY_STATIC41`,
sessione `ses_e88602b69ffeepp969JnKU38Ax`. La review non è stata ripetuta qui.
Modello dichiarato dal runtime della sessione: `openai/gpt-6.1-sol`.
Nessun override di modello o failover. Nessuna delega.

## Autenticazione prima del figlio

Verificati SHA-256 primari:

```text
source-hashes-008.json 9bd245f921bd0cc72a74176215e041b8e3bfd623551ece98a940f7ad4d599ea2
handoff-008.json       c8f4acab47fdf1df5a1d2e7a1a977beeca429e94bddba44e5cc0881f4dd589c9
```

Autenticati i 20 sorgenti/piano del manifest, gli artefatti del handoff,
il report008, il report007, baseline scientifica e protocollo169. Per i record
con metadati sono stati confrontati byte, righe, mode, nlink e SHA-256.
Il launcher rifiuta symlink e file non regolari o con più link; verifica che
inode, dimensione e timestamp non cambino durante ciascuna lettura.
Le stesse fonti sono state riverificate prima di Popen e dopo il reaping.

Il percorso dello snapshot è quello dichiarato nel preparatore autenticato:
`tmp/m1/b-viewer/snapshots/before-supervisor-followup-008/`.
Verificati `sources-007.zip`, SHA-256
`d43f0bd734858d8f9d7f73c8856cbd7a5145f3d94bcfefdc49dc0ed5bdf649b5`,
e `receipt.json`, SHA-256
`12e8f4fa30897662307e672adbc338bf7c0417ed8d7aa53c95222a7b0da43dae`.
Verificati tutti i 28 membri per hash e mode, senza membri duplicati o extra,
e tutti i 20 pin007 nei byte dello snapshot. Verificati il prefisso007 del
piano e i FIRSTFAIL005/006 nei percorsi effettivi registrati nello snapshot.
Non è stato cercato uno snapshot FFT né inferito un layout da altri turni.

Il launcher ha controllato l'assenza degli output futuri008 indicati nei
controlli statici, incluso `pending-own-child.json`. AST e nomi dei test
corrispondono esattamente ai 41 metodi preregistrati, di cui 14 nuovi.
Questi controlli precedono l'import del supervisore e la suite.
L'immutabilità verificata riguarda i file autenticati; non si dichiara una
nuova conformità globale delle 560 voci dello snapshot storico.

## Comando e ambiente

Comando preregistrato nel piano:

```bash
/usr/bin/python3 -I -B -S tmp/m1/b-viewer/supervisor_regressions.py
```

Comando effettivo dell'unico figlio, con pathname assoluto perché `watch`
imposta cwd a `tmp/m1/b-viewer`:

```bash
/usr/bin/python3 -I -B -S /opt/workspace/astroProcessor/tmp/m1/b-viewer/supervisor_regressions.py
```

Invocazione del launcher, dalla radice del progetto:

```bash
/usr/bin/python3 -I -B -S - < tmp/m1/b-viewer/execute-regressions-008.txt
```

Output nuovi ed esclusivi in `tmp/m1/b-viewer/regressions-turn-008/`.
Il launcher crea la directory con mode0700 e umask0077; file e receipt usano
creazione esclusiva. HOME, TMPDIR e directory XDG di cache/config/data/state
sono private sotto tale directory. L'ambiente del figlio è una lista esplicita
registrata nel preflight; non eredita credenziali o configurazioni personali.

Ambiente effettivo: Linux x86_64, kernel `7.0.0-38-generic`;
Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.
Framework: `unittest` e `unittest.mock` della stdlib. Non sono stati invocati
compilatori; GCC è la versione registrata nel runtime Python.

## Guard, raccolta e risultati

Il launcher usa il `watch`008 autenticato, senza modificarlo. Preflight:
ROOT/tmp fisico con inode unici **956850176 byte**; cap **1073741824 byte**;
spazio disponibile **125336506368 byte**. Conservata la riserva preregistrata
più ampia, **100663296 byte (96 MiB)**, come headroom conservativo della suite:
è un controllo di capacità, non un payload fisicamente preallocato.
Il margine soddisfa anche la riserva preview32 MiB. Non sono state ridotte
riserve o dimensioni per far passare il gate.

Guard del figlio: deadline180 s, RSS512 MiB best effort, AS768 MiB,
file32 MiB, log8 MiB; cleanup con wait entro il residuo monotonic di2.0 s.
Una sola sessione propria Popen e latch persistito prima del lancio.
Le pipe sono non bloccanti. Il cap ROOT/tmp resta controllato dal guard.

PID del figlio **894043**. Exit osservato con WNOWAIT0, quindi wait sullo
stesso handle riuscito con exit_code0: `child_reaped=true`,
`pending_unreaped=false`, `launch_latch_retained=false`.
Il latch è stato rimosso e il riferimento `PENDING_OWN_CHILD` è nullo.
**Slot libero soltanto dopo la raccolta verificata.** Nessun PID storico è
stato usato per ricostruire autorità. Nessun pending reale resta da riconciliare.

Il raw riporta `Ran 41 tests in 0.028s` e `OK`. Tempo esterno del guard
192935570 ns, dato descrittivo e non benchmark. Log5307 byte;
picco RSS osservato31858688 byte, massimo osservato1 thread,
nessuna osservazione proc persa. ROOT/tmp misurato dopo la suite, prima della
pubblicazione del receipt finale del launcher: **956899328 byte**.
I tempi non sono usati come asserzioni della suite.

I casi includono deadline residua ed esaurita, timeout di cleanup, handle e
latch trattenuti, divieto di Popen dopo pending, persistenza/rimozione latch
fallite, RSS parziale e completa oltre cap, CAP superato, deadline scaduta e
limiti esatti. Le letture proc, wait, segnali e Popen dei test sono sintetici.

## Artefatti e SHA-256

Percorsi relativi a `tmp/m1/b-viewer/`:

```text
execute-regressions-008.txt
abdece5e3d19679f0191fb584311b7ea8bd0879eac6204491dff1d253e8b3866
regressions-turn-008/preflight.json
a298fd9ae7eac23c388e210667412a3f087819cf6d1712b937de7c6971442fa0
regressions-turn-008/supervisor-regressions-008.log
5d6bd802699c560414882e31e080fc2ef925339326919df943fb0379a285eca0
regressions-turn-008/supervisor-regressions-008-receipt.json
e4fd22b6cdcef4e163d25e61f8321572a53afedc22c12e08b8f875a441881fd3
regressions-turn-008/execution-receipt.json
faf36f1f8d4527ac062c49f70e27dff383fd350db30325e8762908a51f972399
```

Sorgenti eseguiti:

```text
run.py                   198113df192f73f3a26a6866eb5bf08aa30f2048edfff31342b4d7eb5782173a
supervisor_regressions.py e91dfcb3317c63770c13cad725ec879cf4fe75bdef94b201057ab6ebc2e2c93f
```

## Limiti e perimetro

**0 test reali OS/D-state.** La raccolta reale dell'interprete della suite
dimostra soltanto che quel figlio è stato raccolto; non simula un figlio kernel
non raccoglibile. I mock non provano race reali, riuso PID, SIGKILL in D-state,
latenze kernel o persistenza bloccata. La deadline limita l'attesa Popen,
non ogni syscall o operazione I/O. La causa dell'ESRCH006 resta non provata.
Il receipt mantiene `antifallback_gate=INCOMPLETE`: nessuna qualifica GUI,
scientifica o antifallback del prodotto deriva da questo PASS.

Nessuna build007/008, GUI, oracle, FFT, worker nativo, rete, installazione,
Git, indice o lettura di segreti. Nessun sorgente, piano o manifest preesistente
modificato. Nessuna scrittura a progress o indici documentali. Sono stati
creati soltanto il launcher runtime, gli output nuovi e questa evidenza.
