# M1-a: validazione pocketfft 004

## Esito e perimetro

Il solo turno autorizzato ha prodotto `PASS_CANDIDATE_232_PENDING_INDEPENDENT_REVIEW`.
Il supervisor e il driver sono terminati con uscita 0. Tutti i 232 casi hanno un risultato durevole con `pass_=true`; `NOT_RUN=0`.
Questo documento consegna le prove al revisore indipendente e non approva il candidato.

L'ammissione esplicita del coordinatore segue la review READY `ses_e87781c12ffeOQJOXVt5qF7MuM`.
Il driver conserva nel campo storico `review_reported_by_coordinator` il riferimento `ses_e8786ecdeffevq850qfn4xZk0L`: non lo si interpreta come identificatore della nuova ammissione.
L'esecutore ha usato il driver prescritto senza modificarlo, dopo verifica SHA-256 e dimensione.
Ha creato esclusivamente `tmp/m1/fft-pocket/validate-turn-followup-004/outer.py`, copia byte-identica di 17327 byte, con directory e file di modo `0700`.

Non sono stati eseguiti altri test, build, retry o esperimenti. Nessuna modifica a wrapper, CLI, sorgenti, profilo, piani, progress o indici.
I risultati dei precedenti test mirati 2/2 e della suite mock 37/37 sono prerequisiti autenticati, non nuove esecuzioni di questo turno.

## Comando e ambiente osservati

Avvio del driver: `/usr/bin/python3 -I -B -S /opt/workspace/astroProcessor/tmp/m1/fft-pocket/validate-turn-followup-004/outer.py`.
Il driver ha effettuato una sola invocazione del comando seguente, con cwd `/opt/workspace/astroProcessor`:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/supervisor.py validate validate-followup-004 build-followup-003
```

Supervisor PID `1119979`, nuova sessione, stdin nullo. Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.
Sistema osservato: Linux x86_64, kernel `7.0.0-38-generic`.
`HOME`, cache, configurazione, dati, runtime XDG e temporanei puntavano a directory private sotto `validate-turn-followup-004/`.
`PFFT_RUN_ROOT` puntava a `validate-followup-004`; `OMP_NUM_THREADS=1`, `LANG=C`, `LC_ALL=C`, `TZ=UTC`.
L'ambiente completo è in `validate-turn-followup-004/preflight.json`.

## Autenticazione e invarianti

Il preflight ha autenticato 75 input, inclusi i 59 pin storici, le prove mock, build e runtime richieste.
Il postcontrollo ha verificato gli stessi byte e metadati; il registro finale contiene 76 elementi perché include anche il nuovo `complete.json`.
`pinpost=MATCH`. I registri dei metadati FFT preesistenti prima e dopo sono byte-identici.
Anche i registri protetti di `docs/progress.md`, `docs/test-matrix.md`, `docs/evidence/README.md`, `plans/M0-M1.md` e `AGENTS.md` sono byte-identici.
Build 003, piani e FIRSTFAIL storici restano preservati. La verifica precede la scrittura di questo nuovo report.

Pin SHA-256 dei componenti:

| Componente | SHA-256 |
| --- | --- |
| Driver sorgente e `outer.py` | `df9f9c5094af8c71ea1dd56988479550ce39e18be5a8bdfeee44b36e2d1a4fc4` |
| Supervisor corrente | `ad776c3bac3a0b953b2b8a8ac85dd6c395ecf0e5c85a3bf95179d4553ab21cb4` |
| Manifest observation | `a658f12d0f273d411d6e163141e983d282e872602d0b1f0e5df9f05415bf3b35` |
| Suite mock 37 | `e8333f5cfd3d6df5888e88175de5cb4020eea458267c0f55f0f120359ff0058e` |
| Worker build 003, 180200 byte | `0f724e26f0bce62431390a35f8e8168504e051e26f2266c50355a14a37a585da` |
| Profilo storico 003, 6720 byte | `9e66b7ed388813da15d0abe7cb04ce6f704af02a64f29640ad6639523c807a75` |

Il profilo storico non è il pin del supervisor corrente. `launch.json` lega il lancio al profilo corretto.
Il driver ha legato e serializzato i campi statici prima di `Popen`, aggiungendo poi il solo PID posseduto.
Flag e macro del worker restano quelli del profilo storico, senza ricompilazione.

## Risultati scientifici e casi di errore

| Gruppo | Eseguiti e superati | Uscita worker attesa e osservata |
| --- | ---: | ---: |
| Finiti | 192 | 0 |
| Errori di base | 14 | 2 |
| Non finiti | 24 | 2 |
| Collisioni | 2 | 2 |
| Totale | 232 | secondo gruppo |

Gli ID durevoli sono esattamente da 0 a 231. I 232 casi hanno anche `observed-before-oracle.json`.
I finiti coprono f32/f64, forward/inverse, componenti re/im, padding 0/17, dimensioni 32x32 e 1024x1024.
Le famiglie impulse, constant, sinx, siny, cosx e cosy hanno 32 casi ciascuna.
L'oracle analitico usa numeratore intero diviso 2 e confronta tutti i bin e le componenti, con tolleranze assoluta e relativa entrambe 0.
L'inversa usa uno spettro analitico, non l'output della forward. Gli zeri con segno valgono per uguaglianza numerica.
Il padding inattivo contiene sentinelle quiet NaN.

Gli errori di base coprono token e header non validi, dimensione zero, overflow, budget memoria, input troncato e dati finali inattesi.
I casi negativi non sono skip: hanno risultati durevoli e uscita 2 verificata.
Non restano `input.bin`, `candidate.bin` o `candidate.bin.part` nei casi. Il driver ha verificato l'assenza di FIRSTFAIL nel nuovo run.
Il cleanup degli intermedi riusciti è quello preregistrato nel supervisor; nessuna pulizia aggiuntiva è stata eseguita.

## Osservazione runtime e risorse

Il caso 088 ha acquisito una sola mappa del worker vivo, PID `1120484`, su impulse f32 forward re, 1024x1024, padding 0.
La ricevuta lega worker, loader, libc, libm, libgcc_s e libstdc++ alle identità attese.
`runtime-before.json` e `runtime-after.json` coincidono anche nei pin dei file runtime.
Questa prova osserva identità di file mappati in un istante di un processo. Non prova binding di simboli, tutti i 232 processi o l'intera durata delle esecuzioni.

I limiti validate risultano `APPLIED`, con coppie soft/hard esatte: AS `268435456`, RSS best effort `536870912`, file `67108864`, core `0` byte.
I figli ereditano questi limiti; non c'è una misura indipendente del loro readback.
Cap globale scratch `1073741824`, riserva `67108864`, margine `16777216`, coppia massima inclusi header `34111550` byte.
Il preflight include inoltre budget log `6291456` e metadati `4194304` byte.
Allocazione iniziale `928681984`, massimo campionato `965828608`, finale `931823616` byte.
Spazio filesystem disponibile al preflight `125324787712` byte.
RSS massimo campionato del supervisor leader `24571904` byte. Per i worker, 96/232 ricevute hanno un campione RSS; massimo osservato `38522880` byte.
I campioni non sono picchi garantiti né una misura aggregata dell'albero processi. RSS su Linux resta best effort.
Durata `397.2899593159091` secondi, non utilizzabile come benchmark.
Deadline outer `28800`, worker `120`, cleanup outer `5` secondi. Log outer limitati a `65536` byte per stream; osservati stdout 0 e stderr 0 byte.

## Raccolta e artefatti consegnati

La ricevuta registra `SIGKILL_OWN_GROUP_BEFORE_REAP`, censimento senza membri vivi e `WAITID_REAP_NO_LATER_SIGNALS`.
`reaped=true`, `pending_unreaped=false`, `slot_free=true`; `errors=[]` e `cleanup_errors=[]`.
Lo slot è libero sulla base della raccolta effettiva, non della sola conclusione dei casi.

I percorsi seguenti sono relativi a `tmp/m1/fft-pocket/`:

| Artefatto | Byte | SHA-256 |
| --- | ---: | --- |
| `validate-turn-followup-004/execution.json` | 22810 | `233d2a6f1b98a234fc6f61df8081a4f500253f40b6bef1e033aa7ca11aa93b3d` |
| `validate-turn-followup-004/preflight.json` | 5006 | `fde9266b7374f8f6ed2adb43e9cebbe2509a412eb5f9001d353231bbc180c3c9` |
| `validate-followup-004/complete.json` | 419 | `143a969c15f24955e93254cc43e44ca3fcfe300dbd93ca897949ca17a78919ab` |
| `validate-followup-004/results.jsonl` | 121421 | `7af21299f0c62d1e2151d1a401e25932b404631776241d591d04a6e0080ad604` |
| `validate-followup-004/case-088/runtime-map.json` | 1781 | `e3e815c129d04499094e4a0106361be6789ac9d230a3bd2d0a9d650ac62a1115` |
| `validate-followup-004/case-088/runtime-maps.raw` | 4371 | `0a42aa4113d3c1e7a02694e9a54b31bf8526f1124434ef71c48251086c90d6b1` |
| `validate-turn-followup-004/runtime-before.json` e `runtime-after.json` | 2436 ciascuno | `5b31cf7906aed07cec9426eb0dd9a7370b3d37085c1ac131aac053b26e440258` |

La review indipendente di questo turno resta da eseguire. Licenze runtime, adozione e gate ulteriori non ricevono approvazione da questi 232 casi.
I campi PREP/NOT_RUN dei manifest storici rimangono immutati e non sostituiscono le ricevute effettive di questo turno.
