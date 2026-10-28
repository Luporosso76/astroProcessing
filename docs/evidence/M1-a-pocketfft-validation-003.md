# Pocketfft: turno validate003 interrotto dall'outer

26 ottobre 2026. Esito `FIRSTFAIL` dell'outer, non fallimento numerico del
candidato. Una sola invocazione del supervisor, terminata con segnale 9 e
raccolta realmente. Tutti i 232 casi sono `NOT_RUN`. Nessun retry o fix.
Il namespace scientifico `tmp/m1/fft-pocket/validate-followup-003` è assente.
Questo turno non produce un PASS FFT, runtime, prestazioni o M1.

Agente esecutore: subagente, modello dichiarato dal runtime
`openai/gpt-6.1-sol`. Nessuna delega; costi non esposti.

## Comando e primo inatteso

Directory di lavoro `/opt/workspace/astroProcessor`. Prima dell'esecuzione sono
state verificate le assenze di outer, target e report. L'outer nuovo è
`tmp/m1/fft-pocket/validate-turn-followup-003`, creato 0700 senza overwrite.
La sola verifica AST dell'outer ha dato `AST PASS`.

Unica invocazione dell'outer, exit 2:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/validate-turn-followup-003/outer.py
```

Unico comando candidato passato a Popen:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/supervisor.py validate validate-followup-003 build-followup-003
```

Il preflight corrente ha passato autenticazione, runtime pin, ricevute,
limiti ereditati e spazio. Il primo inatteso è un errore introdotto nell'outer:

```text
execution: KeyError('manifest_pin')
```

Il blocco riusato da `build-turn-driver-followup-003.py`, righe 263-368,
legge `preflight['manifest_pin']` alla riga sorgente 281, mentre il nuovo
preflight espone `profile_pin`. La valutazione avviene dopo Popen e prima
della scrittura di `launch.json` e della registrazione delle pipe nel selector.
`launch.json` non è stato creato. Il PID è conservato in `execution.json`.
La successiva diagnosi `qualification supervisor failed` è conseguenza
dell'interruzione, non un secondo tentativo o un errore dell'oracle.

Il sorgente dell'outer e il blocco riusato restano intatti per la review.
Non è stata corretta la chiave e non è stato rilanciato il comando.

## Raccolta e copertura reale

PID=PGID=SID del supervisor posseduto: `1111118`. L'outer ha verificato
ownership tramite `waitid(WEXITED|WNOHANG|WNOWAIT)` e PGID/SID, inviato
`SIGKILL` al proprio gruppo prima del reap, verificato il census vuoto e
raccolto il leader con `waitid(WEXITED|WNOHANG)`. Exit candidato `-9`.
Nessun segnale dopo il reap. Pipe e selector chiusi; log sincronizzati.
`cleanup_errors=[]`, `pending_unreaped=false`, `slot_free=true`.

La durata osservata dall'outer è `0.02929353597573936` secondi, non un benchmark.
I log outer stdout/stderr sono entrambi vuoti. Poiché l'errore precede la
registrazione delle pipe, non dimostrano l'assenza di output emesso dal figlio.
Non esiste una ricevuta dei limiti effettivamente applicati dal supervisor.

Risultati durevoli: 0; ricevute `observed-before-oracle`: 0; worker scientifici:
0, sulla base del target assente e dell'ordine del codice autenticato, che crea
il target prima di ogni worker. Tutti i casi restano `NOT_RUN`: 192 finiti,
14 errori base, 24 non finiti e 2 collisioni. Non esistono `results.jsonl`,
`complete.json`, campione maps del caso 088 o intermedi del turno.
Nessuna osservazione RSS del leader disponibile. Non sono cambiati metodo
di osservazione, formule, tolleranze, flag, budget del worker o copertura.

## Autenticazione corrente e conservazione

Pin SHA-256 verificati prima dell'invocazione e invariati nel postflight:

| File sotto `tmp/m1/fft-pocket/` | SHA-256 |
| --- | --- |
| `supervisor.py` | `ad776c3bac3a0b953b2b8a8ac85dd6c395ecf0e5c85a3bf95179d4553ab21cb4` |
| `validation-observation-001.json` | `a658f12d0f273d411d6e163141e983d282e872602d0b1f0e5df9f05415bf3b35` |
| `runtime-observation-tests-001/test_runtime_observation.py` | `e8333f5cfd3d6df5888e88175de5cb4020eea458267c0f55f0f120359ff0058e` |
| `followup-manifest-003.json` | `9e66b7ed388813da15d0abe7cb04ce6f704af02a64f29640ad6639523c807a75` |
| `build-followup-003/worker` | `0f724e26f0bce62431390a35f8e8168504e051e26f2266c50355a14a37a585da` |

Verificati separatamente il supervisor storico nello snapshot
`snapshots/pre-runtime-observation-001/supervisor.py`, come fonte del profilo
build003, e il supervisor corrente di validazione. Autenticati anche upstream,
ricevuta build e le cinque fonti del profilo. I 59 pin di attempt004 sono
stati riletti e confrontati senza ricostruire né rieseguire la catena di test.
Il controllo delle ricevute target2 e suite37 conferma conteggi 2 e 37,
exit 0, zero fail/error/skip/expectedFailure e pin dei rispettivi raw.
La review `ses_e8786ecdeffevq850qfn4xZk0L` è comunicata dal coordinatore;
non viene presentata come nuova autenticazione indipendente.

La ricevuta statica verification002 è stata autenticata con SHA-256
`7fd44020b969d1a9989f043f75f4684ca742ba2759bdde45090cd79dbaf0f992`.
I cinque runtime host e il worker sono stati verificati con lettura bounded,
hash e stat stabili prima e dopo. `runtime-before.json` e `runtime-after.json`
coincidono. Questo controllo non osserva mappe né symbol binding.

`authentication-before.json` e `authentication-after.json` contengono 75 file
identici per byte/hash/stat. Anche i 346 record metadata dei file FFT
preesistenti coincidono. Progress, matrice test, indice delle evidenze,
`plans/M0-M1.md` e `AGENTS.md` sono invariati nel confronto protetto.
Ricevuta di ritiro CPP conservata; il solo `cpp.bin` già ritirato resta assente.
Non è stato cancellato alcun altro file. Vecchi FIRSTFAIL, snapshot e pin
restano intatti. Nessuna nuova suite, build, installazione, rete, indice,
modifica di prodotto o licenza.

Deviazione operativa separata: nella ricognizione iniziale è stato eseguito
`git status --short` in sola lettura, nonostante il divieto Git del mandato.
Nessuna operazione Git di scrittura, commit o push.

## Ambiente e spazio effettivo

Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.
Linux `7.0.0-38-generic`, x86_64. Nessun nuovo comando GCC eseguito.
HOME, TMPDIR, TMP, TEMP e XDG cache/config/data/runtime dell'outer puntano
a directory private 0700 sotto ROOT; ambiente esplicito, umask 0077.

Allocazione fisica globale tmp, deduplicata per device/inode con `st_blocks`:
`928374784` byte nel preflight; `928493568` byte nel postflight.
Spazio filesystem disponibile nel preflight: `125325119488` byte.
Cap `1073741824`, riserva `67108864`, margine `16777216` byte.
Il preflight ha incluso la coppia massima di `34111550` byte con header,
budget log `6291456` e allowance contabile metadata `4194304` byte.
Questi ultimi non modificano i gate scientifici o i limiti del candidato.
Il controllo non è una quota kernel atomica e assume il writer esclusivo.

Limiti richiesti invariati per validate: AS `268435456`, RSS best effort
`536870912`, FSIZE `67108864`, CORE `0` byte. Hard ereditati compatibili.
Worker deadline 120 secondi; log child combinati 65536 byte.
Guard outer 28800 secondi, cleanup 5 secondi, cap 65536 byte per stream.
Nessuno di questi valori è una misura delle prestazioni o del fabbisogno FFT.

## Ricevute stabili

Tutti i percorsi seguenti sono sotto `tmp/m1/fft-pocket/validate-turn-followup-003/`.

| Artefatto | Byte | SHA-256 |
| --- | ---: | --- |
| `outer.py` | 16216 | `0c701015562c117db2b9f6d110442b0125523aec9d9ba3f21a61c8b12eb57213` |
| `execution.json` e `FIRSTFAIL.json`, ciascuno | 1395 | `12187ce402294d2afa15a6fe5cb67362097bda450458ccbc3de28170e452d243` |
| `preflight.json` | 5006 | `e7eb252763935394ce09004889a7907764215409689008596ea3603f7250677d` |
| `authentication-before.json` e `authentication-after.json`, ciascuno | 25872 | `b95e86afc6ea7932d172ffb059993f854c064995231eb6c0ac955b80aa45853f` |
| `runtime-before.json` e `runtime-after.json`, ciascuno | 2436 | `5b31cf7906aed07cec9426eb0dd9a7370b3d37085c1ac131aac053b26e440258` |

Il ramo si ferma qui per handoff. Slot libero dopo raccolta effettiva;
qualifica scientifica non eseguita. Un eventuale turno corretto richiede
nuova ammissione del coordinatore, senza riutilizzare o alterare queste prove.
