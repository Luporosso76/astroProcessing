# M1.d UNIT33-1: preparazione primaria standalone

Data: 2026-10-25. Modello dichiarato dal runtime: `openai/gpt-6.1-sol`.
PREP soltanto: nessun import candidato, test, preallocazione, worker, GUI o
benchmark. Nessuna delega, rete, installazione, Git, indice o scrittura su v5.

## Contratto e scopo

Si applica il chiarimento indipendente: cap pool 32 MiB, RSS best effort,
scratch comune 1 GiB con preflight/riserva conservativa/controlli osservati.
Non si aggiunge un requisito di massimo universale dei metadata delle syscall.
Gamma v5 non viene chiamata e nessun PROOF_STATUS viene modificato.

Questa è una ricetta PRIMARY nuova e limitata a 12 regressioni lifecycle,
`M1.d-unit-primary-1`, non una modalità nascosta della vecchia ricetta.
Non esegue il main v5 e non ridefinisce M1.d PASS. I 21 bounded, 15 errori
attesi, tre full da 40 GiB e spill fisico 256 MiB restano separati e NOT_RUN.
Il default nativo pool 32 MiB e i tre buffer da 2 MiB per worker 1/2/4 non
cambiano; questa suite non ne prova il comportamento sotto carico.

## Snapshot e pin prima di ogni futuro import

Otto sorgenti reali v5 più manifest copiati byte per byte in
`tmp/m1/d-unit/snapshot-v5/`. La receipt `frozen-v5.json` contiene hash e byte.
Il suo SHA-256 è
`e3205c838b7a7fd3cbc71ff3fc879aa7948d108852daed5ee127e7b20c8d988b`.
Manifest v5 atteso:
`b45cc3bb16043e50e2b82b6d8d7fc54eddb21bf3dc82dec50e973b7e1a3741ab`.
Preparazione v5 congelata:
`4d45734822bfbdc71dc96869224c50057b9950a7f7c1df0172bcf9d262326350`.

Il driver usa solo stdlib per verificare quel manifest, gli otto file correnti,
le copie e i propri sorgenti. Solo dopo aggiunge i percorsi e importa le API
candidate. Ripete i pin dopo la suite. Non usa snapshot alternativi per
continuare se il sorgente atteso cambia: mismatch produce stop, non fallback.
Nessun vecchio report, snapshot o receipt è stato riscritto.

## Contatti API concreti e adapter dichiarati

Sono stati letti admission.py 330 righe, test_lifecycle.py 410, disk.py 201,
bounds.py 105 e manifest v5 206; gli altri sorgenti sono stati letti per hash e
snapshot. Non sono stati caricati moduli candidati per ispezionarli.

Due API non accettano direttamente la nuova geometria:

- `admission.Arena.__init__` sceglie bounds.plan(unit), senza parametro layout;
  quel protocollo prenota 97381119 byte e non è questa ricetta;
- `disk.release_verified_intermediate` richiede nome/status scientifici e
  chiama verify_intermediate, che verifica realmente 256 MiB. Non può convalidare
  33 byte senza cambiarne il contratto.

`UnitArena` costruisce esplicitamente il nuovo layout, poi riusa le classi e i
metodi REALI v5: Partitions, Slot, commit/read JSON, borrow, mapping,
close_mapping e guard consumer. La sua check legge st_blocks reali e verifica
cap/riserva; non sostituisce proprietà con contatori fittizi. initial_blocks
è la lettura vera del fd, non un valore inventato per un test.

Il rilascio UNIT33 è un helper primario distinto: fsync, inspect_retained v5,
confronto reale di dimensione/digest, receipt final/staging mediante Slot v5,
readback del digest, identità stabile, guard consumer, unlink del solo successo,
fsync directory e occupazione osservata. Non chiama un verificatore 256 MiB
patchato e non dichiara SPILL_COMPLETED. Il file rimane identificato come
`unit-33B/scratch/spill.complete`, non come prova scientifica 256 MiB.

Undici corpi test sono ereditati dalla classe v5 senza riscrittura. Setup e
invoke usano coordinate/protocollo primari. Il test negativo di verifica mantiene
i tre assert originali, ma usa un digest atteso errato contro file reale: non
patcha una assertion o una funzione di verifica scientifica. Gli spy e fault
fsync/unlink/pwrite restano quelli necessari ai controlli negativi e delegano
le operazioni normali alle primitive native. Nessun mock di metadati/proprietà
viene usato per far passare un test di produzione.

## Inventario dedicato e budget

Tutto è derivato da `recipe.py`, prima dei dati e senza scegliere un numero
per farlo entrare. Niente regioni generiche da 1 MiB per ogni riga non usata.
Schema JSON: chiavi reali, contatori signed-int128 da 40 byte, path relativi
massimi 112 caratteri, digest hex da 66 byte con quote. Stringhe variabili
riservate fino a 12 byte per carattere, anche non-BMP. Le capacità includono
newline e punteggiatura con arrotondamento conservativo.

| Artefatto | Numero | Payload massimo per regione |
|---|---:|---:|
| fixture reali | 12 | 33 |
| ENOSPC reale distinto | 1 | 4096 |
| fault Slot | 3 | 33 + header 16 |
| raw log fixture | 12 | 34 + header 16 |
| receipt UNIT33 final e staging | 24 | 4546 + header 16 |
| release receipt final e staging | 24 | 1434 + header 16 |
| trace JSONL completa | 1 | 70709325 + header 16 |
| runner log | 1 | 9020 + header 16 |
| result final + due staging fissi | 3 | 158843 + header 16 |
| errore driver final + staging | 2 | 853 + header 16 |

Totale 70 slot nell'arena, payload/header complessivi **71341727 byte**.
I fixture sono inode separati; i fault Slot sono tre regioni reali dell'arena.
Il JSON risultato contiene metadati/hash prima e dopo ogni caso, eventuale
file ENOSPC, esiti effettivi/NOT_RUN, slot terminali e ammissione effettiva.
I JSON error/runner contengono riferimenti a trace integrali, non duplicati
tagliati. Trace e source snapshot permettono di ricostruire ogni indirizzo.

### Trace, runner e ricorsione

Recursion limit nativo letto dal processo di preparazione: 1000; non è stato
modificato. Il futuro driver richiede quel valore e il Python esatto v5.
Il modello riserva 2000 frame per eccezione, inclusi reraise; 14 nodi di
eccezione da primaria, quattro context test, tre spy, due chiusure native,
check in finally, due fasi unittest e handler driver. Gli errori previsti sono
catturati dalle assertion e non consumano questi record.

Massimo testo messaggio: `32*(6*256+64)+2*8192+512=68096` caratteri, da liste
diagnostiche e operandi massimi dei test chiusi. I filename/function sono ASCII
senza escape, con massimi 256/128; frame encoded 432. Si riservano 12 byte per
carattere del messaggio. Il nodo vale 1683536 byte, record 23569774 e tre record
body/teardown/driver 70709325. Le cause/context, i cicli tramite riferimenti e
gli eventuali children vengono registrati senza elisione di frame o messaggi.
Non si duplicano sorgenti/locals nelle trace: si conservano tutti filename,
funzioni e numeri di riga, con sorgenti immutabili e runtime identificato.

Un valore fuori schema non viene accorciato per ottenere PASS: il driver ferma
la ricetta con TRACE_SCHEMA_VIOLATION. Il dominio e la derivazione della trace
richiedono review prima dell'esecuzione, non una prova universale di errori
arbitrari o plugin. Non ci sono discovery, plugin, worker o input dati esterni.

### Arrotondamenti e riserva metadata

Quantum primario 4096, verificato prima dei dati senza cambiare filesystem.
Dati arrotondati: arena 71344128, dodici fixture 49152, ENOSPC 4096:
**71397376 byte**. Nuovi oggetti: 15 file (lock e arena inclusi), 38 directory.
Margine metadata esplicito: due quanti per oggetto e per BASE già esistente,
`2*4096*(15+38+1)=442368`. È riserva ingegneristica conservativa accompagnata
da osservazioni, NON massimo universale di ext4, quota OS o garanzia RSS.

Riserva totale **71839744 byte**. Prenotata prima dei fixture e indipendente
dagli esiti; non cresce né sceglie dati/log più piccoli per adattarsi allo spazio.
Baseline locale comprende codice, snapshot, receipt e tutti gli output unit
precedenti. Quota esterna del coordinatore comprende TUTTO ciò che sta fuori
da d-unit, incluso il vecchio unit conservato, senza scansioni degli altri rami.

Condizione: `baseline_locale + esterna + 71839744 <=1073741824`.
Massimo ammesso per baseline+esterna: **1001902080**.
Budget mancante esatto: `max(0, baseline_locale + esterna - 1001902080)`.
Nessuna quota corrente è stata stimata come zero o misurata in questa preparazione.
Senza quei due dati non esiste un deficit numerico attuale onesto da riportare.

## Contratto operativo preregistrato

- Turno globale esclusivo, root nuova privata, assenza fd/mapping esterni e id
  del coordinatore obbligatori. Lock locale non sostituisce il turno globale.
- Controlli prima di ogni CASE e dopo mutazioni; unit IO esegue pwrite/fsync,
  mmap.close con BufferError, registri terminali e short-write ENOSPC reali.
- Al primo imprevisto failfast; gli altri nomi restano NOT_RUN. Eccezioni
  intenzionali catturate dalle assertion non sono errori del runner.
- Nessuna rmtree/riutilizzazione: fixture falliti, raw, trace, receipt e staging
  restano. Solo un successo verificato viene unlinkato, dopo receipt e chiusure.
- Superamento osservato del cap o della riserva è VIOLATION e exit non zero,
  mai PASS. Finalizzazione limitata a slot preallocati; il latch non si azzera.
  Una seconda revisione result registra una violazione emersa nel commit finale.
- Sono controlli osservati, non impedimenti universali del kernel. Non è
  garantita una receipt durevole se l'I/O stesso è indisponibile o dopo SIGKILL;
  non si dichiarano cleanup o completezza in tali casi.

## Consegna e prossimo passo

Codice concreto: `tmp/m1/d-unit/run.py` (entry), `recipe.py` (schema/budget),
`support.py` (adapter geometrico e API native), snapshot e `frozen-v5.json`.
Piano: `plans/M1-d-unit.md`. Manifest nuovo: `tmp/m1/d-unit/preparation.json`.
Solo AST/sintassi e calcoli statici indipendenti sono stati eseguiti; recipe.layout
e le funzioni candidate non sono state importate o chiamate per calcolare i dati.

Verifica statica di consegna: `ast.parse` e `compile(..., 'exec')` senza exec
su dieci Python (tre nuovi e sette copie v5); JSON parse, hash delle copie e
confronto con gli originali. Interprete effettivo:
`3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.
I bytecode compilati restano soltanto in memoria: nessun pyc/cache creato.
Il latch di stop del driver precede la serializzazione degli errori: un guasto
della diagnostica non può qualificare dodici test come COMPLETE.

Comando di sola verifica riproducibile dalla radice:

```bash
OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S - <<'PY'
import ast, hashlib, json
from pathlib import Path
root = Path.cwd()
base = root / 'tmp/m1/d-unit'
manifest = json.loads((base / 'preparation.json').read_bytes())
for name, expected in manifest['sources'].items():
    path = root / name
    assert not path.is_symlink(), name
    data = path.read_bytes()
    assert hashlib.sha256(data).hexdigest() == expected, name
    if path.suffix == '.py':
        ast.parse(data, filename=name)
        compile(data, name, 'exec', dont_inherit=True)
frozen = json.loads((base / 'frozen-v5.json').read_bytes())
for name, info in frozen['sources'].items():
    assert hashlib.sha256((base.parent / 'd' / name).read_bytes()).hexdigest() == info['sha256'], name
assert hashlib.sha256((base.parent / 'd' / frozen['manifest']['name']).read_bytes()).hexdigest() == frozen['manifest']['sha256']
print('STATIC_ONLY: source pins and syntax OK; runtime NOT_RUN')
PY
```

Serve review indipendente di adapter, campo trace e budget, poi quota aggiornata
e ammissione del coordinatore. Nessuna richiesta al proprietario per questi
passi tecnici. Non si toccano progress/index, v5 o gli altri namespace.

Comando FUTURO pianificato, NON ESEGUITO, dalla radice:

```bash
OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S tmp/m1/d-unit/run.py --preparation-sha256 "${REVIEWED_PREPARATION_SHA256:?serve hash della review}" --exclusive-admission-id "${EXCLUSIVE_ADMISSION_ID:?serve turno esclusivo}" --external-scratch-bytes "${EXTERNAL_SCRATCH_BYTES:?serve quota aggiornata}" --private-no-external-readers --output tmp/m1/d-unit/lifecycle-01
```

Il valore REVIEWED_PREPARATION_SHA256 deve essere quello consegnato alla review,
non l'hash ricalcolato per accettare sorgenti cambiati. Il comando può produrre
solo evidenza UNIT_LIFECYCLE_COMPLETE o STOP/VIOLATION, non M1.d PASS.
Tutti i dodici test sono oggi NOT_RUN. Nessuna misura formale, bootstrap
statistico, compatibilità sotto carico o hard assurance RSS viene attestata.
