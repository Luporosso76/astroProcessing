# Pocketfft: delta osservazione runtime, non eseguito

26 ottobre 2026. Review indipendente e test del delta ancora necessari. Nessuna FFT,
build, mock, invocazione loader/ldd o import del codice di progetto in questo turno.
Non modificati progress, indici, review005, worker, manifest storici o ricevute build.
Modello dichiarato: openai/gpt-6-astra; nessun override/delega. Costi non esposti.

## Delta e limiti

`tmp/m1/fft-pocket/runtime-observation-001.diff` contiene il delta del solo supervisor.
Snapshot precedente: `snapshots/pre-runtime-observation-001/`, copie 0400 con manifest.
Fonti nuove congelate 0444; protezione cooperativa, non contro il proprietario.

La validazione autentica separatamente il supervisor corrente e quello storico della
build003. Non attribuisce la build al sorgente modificato. La build non va ripetuta;
il comando build corrente rifiuta il pin storico del supervisor modificato.

Prima dei risultati verifica SHA-256 e identità stat stabili dei cinque runtime canonici
e del worker. Ripete il confronto alla fine. Nel solo caso088 già previsto
(f32, 1024x1024, padding0, forward, impulse/re) legge `/proc/PID/maps` del figlio
OWN vivo, dopo il riscontro dev/inode di `/proc/PID/exe`. Richiede esattamente i sei
oggetti attesi con mapping eseguibile; associa device/inode e pathname ai file
autenticati. Conserva raw e ricevuta prima dell'oracle. Campione assente/incompleto
al termine del caso: STOP, intermedi conservati, nessun retry o caso aggiunto.

Questo è un campione di identità dei file mappati, NON risoluzione di simboli,
relocation, IFUNC, audit di ogni caricamento o copertura di tutti i 232 processi.
Il campionamento non garantisce di raggiungere il worker prima dell'uscita.
L'identità filesystem assume writer cooperativi e antenati stabili: non protegge
contro sostituzioni ostili/ABA o modifiche temporanee ripristinate tra osservazioni.
La review deve accettare esplicitamente questo ambito rispetto alla parola
"live binding" del verbale003; non si dichiara quel gate già chiuso.

Nessun LD_DEBUG/LD_AUDIT, wrapper o libreria iniettata. Ambiente e stderr scientifico
restano invariati. Nessuna modifica a 232 casi, oracle, tolleranze, flag, backend o
AS256MiB. Il preflight aggiunto prenota input+output massimi, inclusi entrambi gli
header, oltre alle riserve esistenti. Non alza il cap.

## Verifiche effettive e pin

Eseguiti soltanto Python stdlib `-I -B -S -` per letture/hash, AST, snapshot e diff.
Nessun modulo di progetto importato. Python 3.14.4, GCC 15.2.0 nella versione Python;
Linux, radice `/opt/workspace/astroProcessor`. Ricevuta:
`tmp/m1/fft-pocket/runtime-observation-001-static.json`, SHA-256
`9c2526d226ad5fe6634305becc082d1202f7728c32f627ee99f4d1269951e6d4`.
Registra i 17 simboli con AST invariato e i pin; controlli sintattici, non test runtime.

- Supervisor: 35010 byte, `9b547767f2d5994cde59d8b807ee0129ad770ac48ab6920df37b6a9851a1354f`.
- Manifest separato `validation-observation-001.json`: 2187 byte,
  `282b99d0a4d981eb6ec3d44cd226394719f9aab1d50259dfec63fb914ea5daa3`.
- Diff: 11374 byte, `e0125444b0ed7588fccafbe154518db011c0f32da7aa6d6249f2f5abfe474cf4`.
- Profilo003 invariato: `9e66b7ed388813da15d0abe7cb04ce6f704af02a64f29640ad6639523c807a75`.
- Worker003 invariato: `0f724e26f0bce62431390a35f8e8168504e051e26f2266c50355a14a37a585da`.

Riletti i cinque file host canonici elencati nel nuovo manifest: tutti coincidono
con byte/SHA storici003. Letture NOFOLLOW del leaf, file regolari UID0, fstat stabile
prima/dopo e confronto stat del nome. Questo non è un'osservazione del processo FFT.

## Prima dell'esecuzione

Review indipendente sul diff effettivo, manifest e limiti. Preparare ed eseguire
soltanto test del delta: mappe complete/incomplete, processo pre-exec/uscito,
inode/path errato, oggetto inatteso/deleted, mapping non eseguibile, cap/grammatica,
pin/stat mutati, errore lettura/fsync e conservazione FIRSTFAIL, mancato campione
che impedisce COMPLETE, separazione provenienza build/validate, riserva scratch.
Verificare anche cleanup dopo errore dell'observer e stderr invariato. I 36 mock
storici non attestano questo delta; non sono stati ripetuti. Nessun test nuovo scritto
o eseguito in questo turno.

Comando futuro, soltanto dopo review, test delta, budget sufficiente e turno esclusivo:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/supervisor.py validate validate-followup-003 build-followup-003
```

## Scratch: sola ricognizione, nessuna rimozione autorizzata individuata

Scansione metadata di `tmp/`, st_blocks*512, inode deduplicati, symlink non seguiti:
960614400 byte prima dello snapshot/delta. Mancavano almeno 4870144 byte per i soli
34111488 byte input+output, oltre a riserva67108864 e margine16777216; esclusi header
e nuove prove. Serve nuovo conteggio al turno. Nessuna cancellazione/compressione.

| Percorso sotto tmp/ | Byte allocati | Evidenza e consumer | Decisione |
|---|---:|---|---|
| m1/a/series-001/work/cpp.bin | 33554432 | validation-summary PASS storico; series-status ABORTED_BEFORE_SAMPLES; nessuna ricevuta di retire o barriere durevoli per questo file verificata | Non ammesso |
| m1/a/series-001/work/rust.bin | 33554432 | Stessi documenti; consumer futuri non attestati chiusi da ricevuta nominativa | Non ammesso |
| m1/a/series-001/work/numpy.bin | 33554432 | Stessi documenti; summary non autorizza cancellazione | Non ammesso |
| m1/d-unit/lifecycle-01/unit.arena | 71348224 | M1-d-unit-validation.md:157-188: contiene risultati/ricevute primari e regioni fault conservate, non semplice scratch | Escluso |

I tre work file sono candidati soltanto a un'ulteriore verifica di autorizzazione,
non intermedi già liberabili. `src/run.py:328-353` conserva summary/risultati ma non
implementa una ricevuta di erase per questi file. `tests/audit.py:84-103` mostra
consumer dei work output, senza attestare una chiusura globale dei consumer storici.
Non ho letto né hashato i payload dei work file o delle fixture.

Le fixture series001/002 restano sorgenti persistenti, mai candidate. La closure
`fixture-store/read-closure.md` le associa ancora alle 66 tuple. Nessun archivio
scientifico qualificato individuato: `M1-fixture-context-validation-003.md:140-150`
attesta solo dati sintetici e nessuna fase archive/retire/materialize/release;
`fixture-store/v2/` contiene preparazione e sorgenti, non un archivio qualificato.
FIRSTFAIL, originali, sorgenti e prove non sono candidati. Risultato della ricognizione:
zero byte dimostrati già liberabili nel perimetro esaminato.
