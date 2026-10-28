# M1.e batch: correzioni della review 002

Preparazione 003, solo sorgenti e controlli statici. Corretti i tre finding
comunicati dal revisore indipendente tramite il coordinatore. **Nessun test,
import di moduli preparati/candidato, dummy, engine, CPU/GPU, build o benchmark
eseguito**. Nessuna nuova review o auto-approvazione attribuita a questo turno.

Autore: `openai/gpt-6.1-sol`, identità dichiarata dal runtime. Python osservato:
`3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`. Costi/token non esposti.

## Snapshot002 prima degli edit

`tmp/m1/e-vulkan-batch/snapshots/002/` contiene dodici copie esclusive con byte,
SHA-256 e mode dei file originali, verificati subito e dopo le modifiche.
Inclusi PREP001/002, report 002 di 181 righe e piano di 147 righe. I mode dei
file copiati sono `0o664`, come registrato nel manifest; nessun chmod degli
originali. Non si attesta un blocco di scrittura universale dello storage.

Manifest snapshot SHA-256:
`9fa615c9ca62e0323f63453529933f224c20ef080d042bdcbe318c857c804bd4`.
PREP001/002 e report 002 correnti rimangono identici alle copie. Il piano è
stato aggiornato prima dei dati con il contratto `batch-envelope-003`.

## Finding 1: errore di censimento non salta l'escalation

`OwnedSession.cleanup()` distingue errore d'identità, errore di censimento e
fallimento del singolo segnale. Un censimento /proc fallito o oltre il limite
non esce dal cleanup prima dei tentativi successivi. Se l'identità trattenuta
resta verificabile, tenta SIGINT, TERM e KILL esclusivamente sul gruppo OWN.
La grazia cooperativa resta limitata; un censimento indisponibile non genera
un ciclo illimitato di diagnostiche. Ogni errore effettivo viene conservato.

Identità non verificabile o leader già raccolto vietano segnali di gruppo.
Il controllo del returncode precede qualsiasi eventuale uso del PGID; WNOWAIT
rimane il metodo di osservazione prima del reap. Nessun poll nel nuovo supervisor.
Assenza non provata resta errore anche se tutti i tentativi di segnale terminano.

Nuovi mock verificano gli effettivi tentativi INT/TERM/KILL dopo census failure
e membership overflow, con wait del leader soltanto dopo i segnali. Non si
limitano ad asserire STOP. Un altro mock vieta segnali con identità indisponibile.

## Finding 2: sessioni irrisolte anche con leader raccolto

Il registro primario ora è `UNRESOLVED_SESSIONS`; `UNREAPED` è soltanto alias di
compatibilità per i mock precedenti. `session_resolved` è distinto da `reaped`.
Residui o assenza non provata conservano la sessione nel registro anche quando
wait del leader ritorna exit 0. Il guard del prossimo supervise si ferma prima
di Popen. Anche errori imprevisti in cleanup_all conservano la sessione e non
saltano il cleanup delle altre.

Il nuovo mock riproduce leader raccolto con successo e discendenti irrisolti,
poi verifica il rifiuto del lancio successivo e l'assenza di spawn. Un mock
separato vieta killpg dopo il reap anche se il PID numerico sembra coincidere.
Non si segnala mai il vecchio PGID dopo perdita dell'identità o raccolta.

La fixture residui dei 17 test precedenti usa ora un registro simulato con
handle fittizio, senza creare un orfano per far passare il test. Il ramo legacy
`orphan` del dummy raccoglie comunque il proprio sleep prima di ritornare.
I test OWN reali già predisposti per timeout e FSIZE conservano i propri handle;
non sono stati eseguiti. I registri isolati dei mock non cancellano stato reale.

## Finding 3: bound dei sidecar applicato prima dell'adozione

La riserva successo di due record più un blocco non viene più dedotta dal solo
limite FAIL per-file. Il nuovo guard comprende file del wrapper e staging:

- stdout deve coincidere byte per byte con il solo marcatore finale della fase;
  stderr deve essere vuoto. Verificato il codice reale del vecchio CLI, riga 508:
  stampa `json.dumps({"status": report["status"], "phase": args.phase})` più newline.
  Gli status delle quattro fasi restano quelli del runner congelato.
- Registrazione e finale devono rispettare byte e schema del record successo,
  binding reciproco, cardinalità/identità dei membri e sessione risolta.
- Diagnostica/staging successo devono essere vuoti. File inattesi, symlink,
  directory non previste e allocazioni oltre schema producono STOP.
- Si contano byte e blocchi allocati, inclusi i blocchi proiettati del payload
  finale esatto, **prima della sua unica write**. Nessun successo provvisorio
  seguito da riscrittura. La write usa i byte già renderizzati.

Il guard gira dopo ExitStack e tutti i tentativi di close. Su violazione cambia
il risultato in STOP prima della pubblicazione; la singola ricevuta FAIL usa la
riserva di fallimento. I raw non vengono tagliati o cancellati per ottenere PASS.
Il drain finale conserva anche i byte disponibili prodotti durante il cleanup,
prima di chiudere le pipe. Resta il cap di fallimento già dichiarato: oltre
quel cap si conserva il prefisso e l'errore, mai un log troncato con PASS. Non
si promette di acquisire output illimitato o futuro di un kernel non interrompibile.

`verify_closed_files()` richiede ora anche il sidecar chiuso e lo ricontrolla
prima di adottare artefatti numerici. Non basta più verificare i soli file interni.
Errori di fsync/close o pubblicazione restano fallimenti; la prova esterna di
pubblicazione fallita rimane nel percorso diagnostico riservato.

Nuovi mock coprono stderr grande ma sotto il cap FAIL, stdout extra,
registrazione/finale oltre schema, staging non vuoto, blocchi oltre riserva,
payload finale controllato prima di una sola write e adozione senza sidecar.
Il caso stderr verifica anche l'integrità completa dei raw usati nella fixture STOP.

## Bound, copertura e limiti

Calcolo statico indipendente dei modelli JSON, senza import/esecuzione di
budget.py: record successo arrotondato 159744 B, fallimento 970752 B, invariati
con il nuovo flag `session_resolved`. Sidecar successo massimo **348160 B**:
due record, un blocco stdout e sei directory; stderr e staging sono vuoti.
Il bound totale precedente **286240768 B** resta aritmeticamente invariato,
ma ora le condizioni di successo dei file gestiti sono applicate nel codice.

Osservazione metadata prima di generare diff/ricevuta: tmp 866496512 B.
Totale con la riserva: 1152737280 B, cap 1073741824 B, **gap 78995456 B**.
La crescita rispetto all'osservazione 002 non è un dato prestazionale; include
preparazioni e snapshot nello storage condiviso. Nessun file di prova archiviato,
rimosso o ridotto per superare il cap. Il preflight completo rimane non superato.
Non si afferma un teorema universale su scritture/numero file del driver opaco.

Le 66 tuple restano identiche: 42 forward, 24 inverse, batch 1 f32 da 42 casi.
Nessun campione tiny004 adottato, né nuova statistica. CPU native-only resta
condizionata, non migliore CPU generale. Nessun PASS whole-batch, M1.e o M1.

Ricontrollati a hash PREP004, source8, engine, shader3, indice e protocolli:
immutati. Non ricalcolati tutti i byte dei GT in questo turno; nessuna scrittura
su GT, vecchio namespace candidato o sue uscite. Nessun output batch-001 creato.

## Diff effettivo e verifiche eseguite

Diff reale generato dai byte snapshot002 contro i file attuali, senza Git:
`tmp/m1/e-vulkan-batch/cleanup-003.diff`, otto file, **+466/-31 righe**.
SHA-256: `ab93996c80af4d2035c7aa287d358e36fb8efc628bcd9b66541583a4a663fe54`.

Ricevuta statica `preparation-003.json`, SHA-256:
`811a55447bc6d299a031a0d80b5c1caff9684449dc88ee9fe0b2c3e83250566d`.
Contiene hash, mode e conteggi di tutte le dieci fonti, diff e osservazione budget.
Sorgenti principali: supervisor 362 righe, controller 705, budget 252;
nuovo test 180, test follow-up 249, venti mock originali 165. Piano 169 righe.

Comandi effettivi: `/usr/bin/python3 -I -B -` con stdin standard library per
snapshot byte/hash/mode, ast.parse, confronto degli AST dei 37 metodi test
preesistenti, conti JSON statici, bytehash baseline e difflib.unified_diff.
Exit 0. **AST 8/8**, venti mock originali invariati a byte, tutti i 37 metodi
preesistenti invariati come AST, 13 regressioni aggiunte: **50 predisposte,
zero eseguite**. Nessun modulo preparato caricato tramite import/exec.

Il codice è consegnato per review del diff e primo mock run autorizzabile;
non rimanda le tre correzioni a un altro report. La qualificazione runtime
rimane da dimostrare, non viene dedotta da sintassi o ragionamento statico.

Primo comando dopo review e ammissione esplicita, **NON ammesso/eseguito ora**:

```bash
/usr/bin/python3 -I -B -m unittest discover -s tmp/m1/e-vulkan-batch -p 'test_*.py' -v
```

Questo non autorizza nessuna delle 264 fasi scientifiche. Slot runtime attuale
F8 invariato. Nessuna delega, rete/installazione, modifica Git/licenza/indici,
Siril, credenziale o scrittura in altri namespace.
