# M1.d: correzioni del candidato, sola preparazione

Data: 2026-10-25. Autore: modello dichiarato dal runtime
`openai/gpt-6.1-sol`. Questa ricevuta attesta soltanto snapshot, modifiche e
verifiche statiche. Non attesta review indipendente, test dinamici o gate M1.d.
Il candidato non è stato importato né eseguito. Nessun worker, benchmark o
processo di carico è stato avviato.

## Perimetro e letture

Modificati soltanto `tmp/m1/d/pool.py`, `tmp/m1/d/run.py` e
`tmp/m1/d/README.md`; aggiunti questa ricevuta, snapshot e receipt nella directory
assegnata. Nessuna delega, rete, installazione, operazione Git o indicizzazione.
Nessun accesso ai rami degli altri autori. Nessun codice di prodotto.

Letture iniziali: `AGENTS.md`, righe 1-52; `plans/M1-spikes.md`, soltanto
113-122; precedenti `pool.py`, 1-516, `run.py`, 1-579, `README.md`, 1-166.
Il diff è stato letto con `diff -u` contro lo snapshot dei soli tre file propri.
Il piano scientifico da 4452 righe non è stato riletto o hashato in questa fase.

## Finding trattati

1. Pin delle fonti separati. `PROTOCOL_SHA256` identifica esattamente
   `plans/M1-spikes.md` con
   `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.
   `SCIENTIFIC_SHA256` identifica separatamente
   `ASTRO_POSTPROCESSING_PLAN_v2.2.1.md` con
   `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`.
   Il futuro preflight controlla entrambi i file reali. La correzione statica
   verifica le costanti dichiarate, non il contenuto attuale delle due fonti.
2. `Job.errors` conserva dizionari di stringhe, non eccezioni o traceback.
   I worker rilasciano le memoryview e i riferimenti ai tre bytearray.
   Il job aspetta la quiescenza, esce dall'except originale, elimina i payload e
   soltanto dopo restituisce quota e lease. L'errore esterno nasce dopo la
   pulizia, senza catena originale. Se un worker resta vivo, non restituisce le
   prenotazioni e aspetta il supervisore, come prima.
3. Dopo fsync e fstat, lo spill richiede dimensione esatta e
   `st_blocks * 512 >= 256 MiB` prima di lettura di verifica e pubblicazione.
   Un file compresso o sottoallocato produce STOP
   `SPILL_PHYSICAL_ALLOCATION`, senza `SPILL_COMPLETED`. Il parziale resta.
   Formula periodica, oracle, dimensioni, filesystem e backend non cambiano.

## Regressioni predisposte, NON ESEGUITE

- Preflight: pin reali e fixture in memoria con ogni hash alterato, con controllo
  del percorso e del codice `SOURCE_PIN_MISMATCH`.
- Preflight: guardia spill con 0, `256 MiB - 512` e 256 MiB allocati sintetici.
  I valori sintetici non costituiscono prova di spill fisico. Il caso reale
  `spill-256MiB` controlla anche allocazione, byte scritti e byte riletti.
- `worker-failure`: weakref ai bytearray reali tramite sottoclasse allocata
  direttamente, senza copia o wrapper che sopravvive al buffer. Il worker deve
  osservare vivi input, output e scratch da 2 MiB. L'evento release deve osservare
  morti tutti i cinque payload, inclusi controllo e bitmap, e worker quiescenti.
  La seconda ammissione sullo stesso pool non può trovare vivi payload precedenti.
  Entrambe le ammissioni eseguono il guasto del worker. Il catch controlla che
  il traceback esterno non conservi payload e non abbia cause/context originali.

Restano identici i 21 nomi/esiti bounded e i 15 errori attesi; le regressioni
non eliminano casi. Restano tre full da 40 GiB, con 1/2/4 worker, NON ESEGUITI.
32 MiB processati non valgono 40 GiB. Il pool non è un limite RSS universale.
Il bootstrap appaiato e la serie formale di misure restano NON IMPLEMENTATI.

## Snapshot e hash fissati

Snapshot conservato prima di qualsiasi modifica:
`tmp/m1/d/corrections-before-20261025-01/`. I tre file sono stati copiati
byte per byte senza sovrascrivere percorsi esistenti. Lo script statico ha
verificato ogni copia contro `receipt.json`.

SHA-256 precedenti:

```text
6e489e6fab422715226edb203ffca1ea6255b6b7532f7b45517146c6eee0b173  pool.py
357cb48d49e0fe3dd02a3b96e73a13a94cb59b991fc3c00244320fa606d84787  run.py
e9fef9c09f773e406d5987ec627d5cd1dfe07e0bf80fdecd375f5e27fb9655f9  README.md
4c8428e1a087b9bc6fe464943902ecc27edfe9e33ae4485ba10030ff2966df3c  receipt.json
```

SHA-256 del candidato corretto, relativi a `tmp/m1/d/`:

```text
399f13308a9bc45d6938045ea15b8f27cf742551fbe6f5c95eba7c7cf19f3d50  pool.py
90464d67bee562719bb667dca79cfc53f4b9e14b1e3829ae3290876542a953ea  run.py
a42fb0d83bd3d15292d90fe6b0f44d454463d43186992504616a6df99caa15e1  README.md
28e10b8848b0d281cb9172992802ce88040072dc2ccf63f960d96c18c3df4f13  corrections-static-20261025-01.json
```

Nuove dimensioni: `pool.py` 24102 byte, 539 righe; `run.py` 37400 byte,
704 righe; `README.md` 12041 byte, 201 righe.

## Verifica statica realmente eseguita

Ambiente: Linux x86_64, kernel `7.0.0-38-generic`; Python
`3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.
Directory di lavoro: `/opt/workspace/astroProcessor`.
Esito comando: exit 0. AST parse: OK per entrambi i Python. Confronto dei casi:
identici allo snapshot, 21 bounded, 15 errori attesi; lista full presente.
Receipt: `tmp/m1/d/corrections-static-20261025-01.json`, stato `STATIC_ONLY`,
`candidate_imports=false`, `candidate_execution=false`, `dynamic_tests=NOT_RUN`.

Comando eseguito, esclusivamente standard library e analisi AST:

```bash
OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B - <<'PY'
import ast
import hashlib
import json
import os
from pathlib import Path
import sys
base = Path('/opt/workspace/astroProcessor/tmp/m1/d')
snapshot = base / 'corrections-before-20261025-01'
old = json.loads((snapshot / 'receipt.json').read_bytes())
result = {'status': 'STATIC_ONLY', 'author_model_declared': 'openai/gpt-6.1-sol',
          'python': sys.version, 'platform': sys.platform, 'uname': list(os.uname()),
          'candidate_imports': False, 'candidate_execution': False,
          'dynamic_tests': 'NOT_RUN', 'files': {}, 'snapshot_verified': {}}
for name in ('pool.py', 'run.py', 'README.md'):
    prior = (snapshot / name).read_bytes()
    assert hashlib.sha256(prior).hexdigest() == old['files'][name]['sha256']
    result['snapshot_verified'][name] = old['files'][name]
    data = (base / name).read_bytes()
    result['files'][name] = {'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data), 'lines': len(data.splitlines())}
    if name.endswith('.py'):
        ast.parse(data, filename=str(base / name))
        result['files'][name]['ast_parse'] = 'OK'
tree = ast.parse((base / 'run.py').read_bytes())
previous = ast.parse((snapshot / 'run.py').read_bytes())
def assignment(tree, name):
    return next(node.value for node in tree.body if isinstance(node, ast.Assign)
                and any(isinstance(target, ast.Name) and target.id == name for target in node.targets))
cases = ast.literal_eval(assignment(tree, 'CASES'))
assert cases == ast.literal_eval(assignment(previous, 'CASES'))
assert len(cases) == 21 and sum(value is not None for value in cases.values()) == 15
assert ast.literal_eval(assignment(tree, 'PROTOCOL_SHA256')) == '1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba'
assert ast.literal_eval(assignment(tree, 'SCIENTIFIC_SHA256')) == '95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094'
full = ['full-1', 'full-2', 'full-4']
assert any(isinstance(node, ast.List) and all(isinstance(item, ast.Constant) for item in node.elts)
           and [item.value for item in node.elts] == full for node in ast.walk(tree))
result['static_checks'] = {'cases_identical_to_snapshot': True, 'bounded_cases': len(cases),
                           'expected_errors': 15, 'full_cases_literal_present': full,
                           'protocol_and_scientific_constants_match_separate_declarations': True}
result['snapshot_receipt_sha256'] = hashlib.sha256((snapshot / 'receipt.json').read_bytes()).hexdigest()
receipt = base / 'corrections-static-20261025-01.json'
with receipt.open('x', encoding='utf-8') as stream:
    json.dump(result, stream, indent=2)
    stream.write('\n')
print(json.dumps(result, indent=2))
print('receipt_sha256=' + hashlib.sha256(receipt.read_bytes()).hexdigest())
PY
```

La receipt usa creazione esclusiva: non rieseguire il comando sullo stesso
percorso per sovrascriverla. Questa verifica non esegue le funzioni candidate,
non prova quiescenza o vita dei buffer e non misura blocchi fisici dello spill.
Tutte queste prove richiedono la futura validazione ammessa dal coordinatore.

## Handoff: primo comando futuro

Prima serve l'ammissione esplicita del coordinatore. Le misure concorrenti
incompatibili devono essere terminate. Il coordinatore deve verificare e fissare
`EXTERNAL_SCRATCH_BYTES`, inclusa la quota già riservata, senza usare zero per
comodità. Il harness conta anche snapshot e receipt proprie nella quota comune.
La directory di output deve essere nuova. Nessun comando seguente è stato eseguito.

```bash
cd /opt/workspace/astroProcessor
export TMPDIR="$PWD/tmp" PYTHONDONTWRITEBYTECODE=1 PYTHONNOUSERSITE=1
export XDG_CACHE_HOME="$PWD/tmp/cache" XDG_CONFIG_HOME="$PWD/tmp/config"
export OPENSSL_CONF=/dev/null
/usr/bin/python3 -I -B tmp/m1/d/run.py --unit d --phase validate --scope bounded --backend python-stdlib --external-scratch-bytes "${EXTERNAL_SCRATCH_BYTES:?serve la quota esterna verificata}" --output tmp/m1/d/series-corrections-01
```

La prima prova incompleta resta `pool-over-budget`, NON ESEGUITA.
Se i pin differiscono o lo scratch non ammette la ricetta fissa, il preflight
deve fermarsi prima dei worker. Se lo spill reale non raggiunge i blocchi
richiesti, quel caso deve fallire senza cambiare dati o percorso.
