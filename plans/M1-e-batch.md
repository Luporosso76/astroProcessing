# M1.e: batch, preparazione 003

## Correzioni review 002, prima dei dati

Snapshot002 conserva byte/hash/mode di PREP002, report di 181 righe, piano di
147 righe e sorgenti prima degli edit. L'ambiente ora è `batch-envelope-003`;
le descrizioni 002 sotto restano antecedenti per i vincoli invariati.

Errore di censimento non salta l'escalation OWN se l'identità trattenuta resta
verificabile. Identità non verificabile o leader già raccolto vietano segnali
di gruppo. Registro di sessioni irrisolte indipendente dal reap del leader:
residui o assenza non provata impediscono ogni lancio successivo nello stesso processo.

Il bound successo dei sidecar è ora applicato: stdout esatto del vecchio CLI,
stderr vuoto, registrazione/finale entro schema e diagnostica/staging vuoti.
Prima dell'unica write finale si dimensiona il payload esatto e si contano i
blocchi allocati/proiettati. La verifica si ripete prima di adottare gli artefatti.
Overflow o schema errato produce STOP nella riserva FAIL, conservando i raw,
senza pubblicare un successo provvisorio o troncare log per ottenere PASS.

Tutti i 37 test precedenti restano predisposti; i casi residui usano registri
simulati, non orfani reali. Nessun test o candidato ammesso in questo turno.
Evidenza nuova: `docs/evidence/M1-e-batch-cleanup-followup.md`.

## Ambito e stato

IN PROGRESS, solo preparazione dopo review indipendente della 001. Nessun
import dei moduli preparati, test, processo candidato, build o benchmark in
questo turno. Tutte le 66 tuple batch rimangono NOT_RUN. Nessuna adozione autonoma
di tiny004: FAULTS con review PASS e CONFIRM principale reale completato IN REVIEW
sono contesto del coordinatore, non campioni o gate acquisiti dal batch.

Le fonti batch vivono in `tmp/m1/e-vulkan-batch/`. La destinazione futura,
espressamente autorizzata, è `tmp/m1/e-vulkan/batch-001/<tuple>/<phase>`.
Non è un cambio automatico di percorso. Il precedente blocco namespace era
risolvibile senza cambiare il runner. Nessun output candidato viene creato ora.

## Prerequisiti e fonti

PREP004 `d19f9d7082f9cb31cb21b0a98d6d1a52044cd3474b6fd42099f383c87d4befe0`;
prereg GPU `f584cc83dab84d1fa35101085834c0601e53d9f20eb997dbf8514e81424715df`;
indice `c958973283f14d7a0377c8b5e728b2d04918081229ce2d8a953ee81305d4629b`.
Sorgenti8, engine, shader3, protocollo padre e fixture/GT rimangono quelli della
baseline. Nessun runner copiato/importato o sorgente baseline modificato.
`batch-envelope-002` aggiunge un ambiente esplicito RLIMIT_FSIZE/sessione; non
aggiunge retroattivamente hash o proprietà ai vecchi lock/ricevute.

Prima degli edit: `snapshots/001/` conserva a bytehash controller, venti mock,
manifest, ricevuta statica, vecchio piano e vecchia evidenza. La 001 resta storica.

## Deliverable e ordine dei gate

- `tuples.json`: medesime 66 righe, 42 forward e 24 inverse, batch 1 f32.
- `controller.py`: comandi concreti del runner, root separate, parent privati
  O_NOFOLLOW/create-only, collision STOP, guard receipt/ledger e file chiusi.
- `session_child.py`, `supervisor.py`: barriera pre-exec, sessione propria,
  cleanup cooperativo/escalation e pubblicazione finale dopo tutti i close.
- `budget.py`: schemi, conteggi, limiti per-file e contabilità dell'intero batch.
- Venti mock originali invariati e nuovi mock/dummy OWN in `test_followup.py`.
- `preparation-002.json` e `docs/evidence/M1-e-batch-followup.md`: solo prove statiche.

1. Consegnare sorgenti stabili e verifica AST, senza importarli o eseguire test.
2. Review indipendente del diff effettivo e dell'envelope. Primo passo dinamico
   successivo autorizzabile: eseguire la suite mock/dummy OWN, non altri candidati.
3. Solo dopo PASS dei test e nuova ammissione esclusiva: preflight globale e
   launch di una fase esplicita della tupla successiva. Non riparare gate mancanti.
4. Per tupla: pilot CPU native-only, review del lock a hash chiuso, VALIDATE,
   review, FAULTS, review, CONFIRM e review. Ricetta/source/device/GT/prereg/CPU-lock
   devono coincidere; assenza/mismatch/FAIL/cleanup incompleto fermano il turno.
5. Un'interruzione conserva il primo FAIL e raw parziali. Non diventa unavailable
   o PASS. Le tuple non avviate restano NOT_RUN. Nessun retry nello stesso turno.

Il launcher non ha una CLI esecutiva. Gli helper sono preparati per integrazione
controllata; la semplice exit 0 del child non è gate numerico. Il ledger completo
resta `NUMERICAL_COMPLETE_AWAITING_INDEPENDENT_REVIEW`, mai PASS automatico.
Whole-batch/M1.e non sono PASS senza quattro fasi e review per tutte le 66 tuple.

## Contratto scientifico e statistiche

Resta la prereg GPU congelata: 6 affine, 6 convd, 6 warp e 24 FFT, forward/inverse
solo dove implementati. Nessun epsilon, cast, fixture o oracle nuovo. Inverse
FFT usa il GT analitico indipendente, non forward candidato. f64 rimane STOP
primario atteso nei fault, mai caso f32 accettato. Le famiglie non si sostituiscono.

CPU `cpp-direct` per pixelwise/convoluzione/warp e `cpp-fftw-plan-reuse` per
entrambe le direzioni FFT. Profilo workspace-reuse matched; planning separato,
processi CPU/GPU persistenti e richieste alternate, candidato inattivo idle.
Native-only è condizionato, non migliore CPU generale o scelta stack dai microcasi.
La scelta whole-M1 richiede i tre core comparandi separati.

CONFIRM futuro mantiene tre cluster, due warm-up e sette round appaiati, 2000
bootstrap gerarchici seed 4059, `exp(median(log(T_CPU/T_GPU)))`, percentili lineari
2,5/97,5. Startup/cold/warm, preflight, copie, recording/kernel/fence e GT restano
separati da T_total. Nessuna statistica batch ora disponibile, né riuso dei sette
pilot o dei campioni tiny004. Non è dimostrata precisione FFT generale.

## Ownership e pubblicazione

Il supervisor avvia un helper con `start_new_session=True`; l'helper applica
RLIMIT_FSIZE e attende un token prima di exec del runner congelato. Il parent
registra Popen, PID/starttime/sessione/PGID e ricevuta pre-exec prima del token.
Contratto non ostile: runner/engine/du ereditano sessione e gruppo, senza setsid,
setpgid o escape. Non è isolamento contro codice ostile.

`waitid(WNOWAIT)` osserva l'uscita senza raccogliere il leader. Prima del reap:
identità trattenuta, censimento dei propri membri, SIGINT cooperativo, poi TERM
e KILL esclusivamente al proprio gruppo. Non usare poll/wait prematuri né PID
estranei. Nipoti residui, perdita identità o stato kernel non interrompibile
producono STOP/cleanup non provato. Non si promette cleanup universale.

Ogni handle ha cleanup indipendente; errori di fsync/close rimangono nel risultato.
La ricevuta finale si pubblica soltanto dopo ExitStack. Fallimento di pubblicazione
produce errore esterno con prova separata, mai successo. Se anche lo storage
esterno fallisce, l'eccezione porta il risultato: nessuna falsa prova persistita.

## Budget preregistrato dell'ambiente 002

Nessun MiB/fase scelto per far entrare il piano. Schemi reali: 9 richieste pilot,
3 validate incluso copy, 27 per backend confirm, nove figli fault. Trace pixel
8 eventi/richiesta; FFT `6+2*(1+log2(N))+inverse`. Startup: topologia osservata
vincolata a massimo due device enumerati, tre queue family, due heap e quattro
allocazioni. Stringhe Vulkan 256 caratteri, path 512, diagnosi 4096, 32 nodi
errore, otto note; JSON conta escaping, indentazione, uint64 e token float finiti.
Superamento dell'envelope è STOP, non log tagliato con PASS.

`budget.py` dimensiona report, receipt, trace, duplicati pilot, fixture control
200/64 byte, raw esterni, registrazione, errori di chiusura/pubblicazione, checkpoint
e directory arrotondate a blocchi di 4096 byte. RLIMIT_FSIZE hard e soft uguali
prima del candidato; engine/du lo ereditano senza cambiare sorgenti. Anche un
worker che ignora SIGXFSZ non ottiene PASS: file al cap, trace parziale/conteggi
errati, JSON o file set fuori schema sono rifiutati dopo chiusura.

Il limite per-file non prova un limite universale di numero file o scritture
opache del driver. I conteggi valgono per il file set congelato non ostile con
Mesa cache disabilitata. Nessuna quota filesystem nuova o modifica di sistema.
Interni opachi rimangono unknown; managed 256 MiB e RSS 512 MiB best effort
non significano anti-OOM aggregato.

Whole-batch conta i byte completati già nel tmp, successo futuro, massimo FAIL
corrente, checkpoint, diagnostica/chiusura e 16 MiB margine sotto cap 1 GiB.
Il prefisso completato si sottrae solo dopo verifica delle prove; non adottare
tiny004. Un gap del bound dichiarato è un preflight STOP, non prova universale
di impossibilità del driver. Valori analitici e limiti nel follow-up.

## Protocollo candidato di archivio, NON eseguito

Solo dopo tutte le quattro fasi e review della singola tupla, nessun consumer
attivo: eventuale archivio lossless nuovo, manifest path/bytehash originale,
verifica di estrazione bitwise e ricevuta archivio a hash chiuso. Riservare il
picco originale+archivio+verifica; compressione non garantisce risparmio.
Mai failed proofs o artefatti ancora referenziati da gate. Nessuna cancellazione,
archiviazione o estrazione automatica ora. Serve review prima dell'esecuzione
di questo protocollo; non è una via alternativa automatica per superare il cap.

## Evidenza, rollback e prossimo comando

Rollback: fermarsi senza eseguire, conservando snapshot001 e tutte le prove.
Nessun progress/index/Git aggiornato. Gate aperti: review del nuovo codice,
mock/dummy OWN, audit numerico dei bound e integrazione delle ammissioni/ledger.

Primo comando MOCK autorizzabile dopo review, **NON eseguito ora**:

```bash
/usr/bin/python3 -I -B -m unittest discover -s tmp/m1/e-vulkan-batch -p 'test_*.py' -v
```

Include dummy OWN Python e `/usr/bin/sleep`, nessun engine scientifico. Le loro
fixture temporanee restano nel namespace batch. Nessuna nuova esecuzione
candidato è ammessa da questa preparazione.
