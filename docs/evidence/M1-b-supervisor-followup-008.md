# M1.b: preparazione008, cleanup bounded e gate integrati

**STABLE_PREP_ONLY; review indipendente008 PENDING, nessuna autoapprovazione.**
Review007 NONREADY comunicata dal coordinatore, sessione
`ses_e887a4837ffemKqHRwWie7c2iC`: P1 wait illimitata, P2 gate mock incompleti.
Modello dichiarato: `openai/gpt-6.1-sol`, nessun override modello/failover.

## Snapshot prima delle modifiche

Verificati tutti i 20 pin007 e conservati 28 membri per hash/mode in
`snapshots/before-supervisor-followup-008/sources-007.zip`, SHA-256:
`d43f0bd734858d8f9d7f73c8856cbd7a5145f3d94bcfefdc49dc0ed5bdf649b5`.
Receipt dello snapshot:
`12e8f4fa30897662307e672adbc338bf7c0417ed8d7aa53c95222a7b0da43dae`.
Prove007, FIRSTFAIL006/005, fonti scientifiche, preregistrazione169 e baseline
restano immutate. Il piano conserva integralmente il prefisso007.

## Delta predisposto

Cleanup: deadline monotonic di **2.0 secondi**, wait con solo tempo residuo,
anche zero. SIGKILL non prova raccolta. TimeoutExpired produce STOP,
PENDING_UNREAPED, exit_code null, handle vivo conservato e slot trattenuto.
L'eventuale exit WNOWAIT rimane distinto dal reaping confermato; diagnosi primaria
e di cleanup rimangono separate.

Prima di Popen si persiste un latch esclusivo senza autorità da PID.
Solo wait riuscito sullo stesso handle consente rimozione e sblocco.
Persistenza fallita impedisce Popen; timeout o rimozione fallita conservano
il blocco. Anche un nuovo interprete rifiuta un latch presente, senza leggerlo
per ricostruire autorità. Nessuna rimozione automatica dopo restart.

Letture proc007, ownership/WNOWAIT/SID/PGID, EACCES fatale, RSS mancante null
e cap restano invariati. La race proc resta una causa **plausibile, non provata**
dell'ESRCH006. Il bound riguarda wait, non syscall kernel o persistenza bloccate.

## Verifiche effettive e limiti

Linux x86_64, Python 3.14.4, GCC 15.2.0 del runtime Python. Comando eseguito:

```bash
/usr/bin/python3 -I -B -S - < tmp/m1/b-viewer/prepare-static-008.txt
```

Solo AST/hash/diff, nessun import del progetto. Nove AST validi; undici funzioni
storiche di run mantengono AST identico. **41 test mock predisposti, 14 nuovi,
zero eseguiti**: cleanup non raccoglibile, latch, RSS oltre cap, campione parziale
oltre cap, CAP, deadline e limiti esatti attraversano watch senza sostituire
il campionamento. Il comportamento non è validato dall'AST.

Confrontate 560 voci protette: 559 conformi. La prima verifica statica si è
fermata su `docs/progress.md`, cambiato durante PREP e non modificato qui.
Delta hash registrato nei controlli; nessuna attestazione di conformità totale.
Nessuna suite, build, GUI, candidato, rete, installazione, Git, indice o delega.

## Pin e prossimo passo

SHA-256, percorsi relativi a `tmp/m1/b-viewer/`:

```text
run.py                     198113df192f73f3a26a6866eb5bf08aa30f2048edfff31342b4d7eb5782173a
supervisor_regressions.py   e91dfcb3317c63770c13cad725ec879cf4fe75bdef94b201057ab6ebc2e2c93f
source-hashes-008.json      9bd245f921bd0cc72a74176215e041b8e3bfd623551ece98a940f7ad4d599ea2
source-diff-008.patch       9f3d89bb82dee349eb2739f38b2dc646ae4d5091eb7cbf54fe46b60b004fa498
preparation-checks-008.json 46423a842a8e2988d190e2066063f45792d177fa333ae850d33fc2b596382d97
```

`handoff-008.json` lega anche questo report. Prossimo passo: review indipendente
del diff effettivo; soltanto dopo, nuova ammissione distinta delle regressioni008.
