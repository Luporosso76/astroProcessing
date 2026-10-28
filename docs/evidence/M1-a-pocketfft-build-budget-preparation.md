# M1.a pocketfft: preparazione del budget build

## Consegna e confini

PREP ONLY. Nessun import di sorgenti propri, test, compilatore, worker, FFT o
benchmark eseguito. La verifica ammessa usa soltanto AST, JSON e hash tramite
stdlib. I 36 metodi sono preparati, non eseguiti. Serve una review indipendente
dei file effettivi prima delle regressioni; la nuova build richiede un turno
separato dopo PASS e review delle nuove regressioni.

La decisione architect comunicata dal coordinatore è
`ses_e88701970ffeS6qAT6ryjpDEvc`: `plans/M1-spikes.md:41` distingue buffer
pianificati 256 MiB, RSS osservato 512 MiB e scratch 1 GiB; non impone AS256 al
compilatore. Il vecchio AS256 per ogni child appartiene alla preregistrazione
locale. Questo record e il manifest 003 la aggiornano senza cambiare baseline,
piano M1-spikes, algoritmo, flag, upstream, 232 casi o tolleranze zero.

Il FIRSTFAIL precedente resta in `tmp/m1/fft-pocket/build-followup-001/`:
exit 1, `virtual memory exhausted: Cannot allocate memory\n`.
I 3059712 byte di RSS campionato riguardano il leader, non dimostrano il
fabbisogno del compilatore. Il budget AS1GiB è una proposta, non una misura.
La ricevuta storica `preregressions-002/complete.json` certifica 20 PASS sul
manifest 002; non autorizza queste nuove fonti e non viene aggiornata.

## Delta e limiti

- `supervisor.py`: selezione esplicita build/validate, validazione di arità e
  sottocomando prima di applicare hard limit; preflight di tutti gli hard
  ereditati prima della prima riduzione. Nessun `min(limit, hard)`.
- Build: AS 1073741824 byte per supervisore e ciascun figlio ereditante, anche
  nei comandi metadati. Validate e preregressioni: AS 268435456 byte; il worker
  eredita AS256. Non è un cap aggregato dell'albero GCC.
- Richiesta RSS 536870912 byte best effort, file 67108864 byte, core 0,
  log 65536 byte; deadline build 180 s, worker 120 s, metadati 10 s.
  Scratch globale 1073741824 byte, riserva 67108864 e margine 16777216 invariati.
- `limits()` registra richiesta, valori ereditati ed effettivi con readback.
  Hard incompatibile, errore syscall o readback diverso producono STOP. Se una
  lettura fallisce, il valore effettivo è null con errore, non un valore inventato.
  I manifest futuri registrano il supervisore; l'eredità dei figli è dichiarata,
  non misurata indipendentemente. Prima del namespace, lo STOP include il record
  strutturato su stderr. Nessun launcher, preexec, fallback o retry aggiunto.
- `run()` e cleanup OWN sono invariati nell'AST. Il campionamento legge solo
  `/proc/<leader>/status`; non monitora tutto GCC e non garantisce un cap RSS
  aggregato. Restano il blocco dei nuovi lanci dopo cleanup irrisolto e il
  divieto di segnali dopo reap.
- `regressions.py`: 13 metodi cleanup e 7 barriere originali preservati, 16 nuovi
  mock per selezione, ordine, limiti invariati, errori setrlimit/getrlimit,
  readback, hard insufficiente, coerenza manifest e ricevute obsolete.
  Il gate build richiede ora 36 PASS e il pin del manifest 003.
- `profile()` verifica i budget delle due fasi, log/deadline/quota, numero dei
  metodi e provenienza 002 oltre ai pin già presenti. I manifest build/validate
  includono il record dei limiti; quello delle regressioni lo conserva a sua volta.

## Snapshot e pin

Prima degli edit: `tmp/m1/fft-pocket/snapshots/pre-build-budget-003/`,
29 file, inclusi piano locale, sorgenti, manifest 002, ricevute precedenti e
FIRSTFAIL della build. Copie e directory write-protected; non è immutabilità
contro un proprietario privilegiato. Lo snapshot non duplica gli snapshot più
vecchi, che restano al loro posto.

Manifest snapshot: 6074 byte,
`12dd9fe572070be03ac4a71603739df17e35be859b9702ff9d8178986c0b3eed`.
Manifest 002: 5249 byte,
`76784fa8e727d254411295f534150ee93f54521776cadb050fd0cad75642494a`.
Manifest 003: 6720 byte,
`9e66b7ed388813da15d0abe7cb04ce6f704af02a64f29640ad6639523c807a75`.
Il record `tmp/m1/fft-pocket/static-build-budget-003.json` contiene byte, righe
e SHA-256 dei cinque sorgenti fissati e del manifest corrente. Il piano locale
ha 277 righe, il supervisore 606, le regressioni 553; adapter 93 e worker 140
sono invariati. Il manifest corrente ha 221 righe.

## Verifica statica eseguita

Directory: `/opt/workspace/astroProcessor`, Linux della sessione.
Python stdlib: `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.
Nessun altro runtime o versione del compilatore acquisito in questo turno.
Comando eseguito, exit 0; output conservato nel record statico citato:

```sh
python3 -I -B -S - <<'PY'
from pathlib import Path
import ast, hashlib, json, stat, sys
root = Path('/opt/workspace/astroProcessor')
here = root / 'tmp/m1/fft-pocket'
snapshot = here / 'snapshots/pre-build-budget-003'
def pin(path):
    data = path.read_bytes()
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
manifest_path = here / 'followup-manifest-003.json'
manifest = json.loads(manifest_path.read_text())
prior = json.loads((here / 'followup-manifest-002.json').read_text())
for name, expected in manifest['sources'].items():
    assert pin(root / name) == expected, name
assert pin(root / manifest['prior_manifest']['path']) == {key: manifest['prior_manifest'][key] for key in ('bytes', 'sha256')}
assert manifest['oracle'] == prior['oracle']
assert manifest['upstream'] == prior['upstream']
assert manifest['recipe']['flags'] == prior['recipe']['flags']
assert manifest['recipe']['macros'] == prior['recipe']['macros']
for name, key in (('pocketfft_hdronly.h', 'header'), ('LICENSE.md', 'license')):
    assert pin(root / 'tmp/m1-fft-tools' / name) == manifest['upstream'][key]
sources = {name: ast.parse((here / name).read_text(), filename=name) for name in ('supervisor.py', 'regressions.py')}
old_sources = {name: ast.parse((snapshot / 'tmp/m1/fft-pocket' / name).read_text(), filename=name) for name in sources}
def symbols(tree):
    return {node.name: node for node in tree.body if isinstance(node, (ast.FunctionDef, ast.ClassDef))}
old_tests = symbols(old_sources['regressions.py'])
new_tests = symbols(sources['regressions.py'])
for name in ('CaseFixture', 'Cleanup', 'DirectoryBarriers', 'worker_barriers', 'namespace_state'):
    assert ast.dump(old_tests[name]) == ast.dump(new_tests[name]), name
counts = {node.name: sum(isinstance(child, ast.FunctionDef) and child.name.startswith('test_') for child in node.body)
          for node in sources['regressions.py'].body if isinstance(node, ast.ClassDef)}
assert counts == {'CaseFixture': 0, 'Cleanup': 13, 'DirectoryBarriers': 7, 'LimitBudgets': 16}
assert manifest['preregressions']['methods'] == 36
supervisor = symbols(sources['supervisor.py'])
old_supervisor = symbols(old_sources['supervisor.py'])
changed = [name for name in old_supervisor if ast.dump(old_supervisor[name]) != ast.dump(supervisor[name])]
assert changed == ['limits', 'profile', 'build', 'qualify', 'main'], changed
snapshot_manifest = json.loads((snapshot / 'snapshot-manifest.json').read_text())
assert len(snapshot_manifest['files']) == 29
for name, expected in snapshot_manifest['files'].items():
    copy = snapshot / name
    assert pin(copy) == {key: expected[key] for key in ('bytes', 'sha256')}, name
    assert not stat.S_IMODE(copy.stat().st_mode) & 0o222, name
    if name not in ('plans/M1-a-pocketfft.md', 'tmp/m1/fft-pocket/supervisor.py', 'tmp/m1/fft-pocket/regressions.py'):
        assert pin(root / name) == pin(copy), name
assert pin(snapshot / 'snapshot-manifest.json') == manifest['build_budget_baseline_snapshot']['manifest']
paths = list(manifest['sources']) + ['tmp/m1/fft-pocket/followup-manifest-003.json']
record = {'kind': 'AST_HASH_ONLY_NO_PROJECT_IMPORT_EXECUTION', 'python': sys.version,
          'ast': 'PARSED_NOT_IMPORTED', 'planned_methods': counts, 'original_20_classes': 'AST_IDENTICAL',
          'changed_supervisor_functions': changed, 'new_supervisor_functions': sorted(set(supervisor) - set(old_supervisor)),
          'snapshot_files_verified': 29, 'historical_files_unchanged': 26,
          'current_profile_and_upstream_pins': 'MATCH', 'tests_run': 0, 'builds_run': 0, 'workers_run': 0,
          'pins': {name: {**pin(root / name), 'lines': len((root / name).read_bytes().splitlines())} for name in paths},
          'frozen': {name: pin(root / name) for name in ('plans/M1-spikes.md', 'ASTRO_POSTPROCESSING_PLAN_v2.2.1.md')}}
print(json.dumps(record, sort_keys=True, indent=2))
PY
```

Esito: due AST validi senza import; 20 metodi originali preservati nell'AST,
16 nuovi metodi contati; pin correnti/upstream coerenti. Verificati i 29 file
dello snapshot e i 26 file storici correnti non editati, incluso FIRSTFAIL.
Questo controllo non prova che i mock passino o che la build riesca.

## Metadati e handoff

Agente: subagente di programmazione della sessione delegata. Modello dichiarato
nella configurazione fornita: GPT-6.1 Sol, `openai/gpt-6.1-sol`.
Non è disponibile un'attestazione runtime separata del modello o del failover;
non sono stati impostati override. Costi e token non sono esposti.
Nessuna delega ulteriore, rete, installazione o operazione Git.

Fonti lette prima degli edit: supervisore 554 righe, regressioni 378, manifest
002 180, piano locale 250, PLANS 46; M1-spikes solo righe 30-49, inclusa 41.
Letti inoltre FIRSTFAIL 4 righe, build-result 16, manifest-before-build 209 e
ricevuta preregressioni 12. Adapter, worker e upstream verificati tramite hash,
non dichiarati letti semanticamente. Nessuna lettura di viewer008.

Gate ancora aperti: review indipendente PREP sui byte fissati dal 003, 36
preregressioni, loro review, nuova build separata, inventario runtime e successiva
qualifica numerica. I comandi futuri sono nel piano locale; nessuno è ammesso
dal solo completamento di questa preparazione.
