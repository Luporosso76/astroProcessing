# Pocketfft: preparazione test runtime-observation-001

26 ottobre 2026. Stato: PREPARED_NOT_RUN, consegna stabile per review indipendente.
33 metodi unittest stdlib preparati; zero test eseguiti, zero import di progetto,
worker, loader o FFT. Nessun PASS runtime dichiarato. I 232 casi FFT restano bloccati.
Modello dichiarato dall'ambiente: openai/gpt-6.1-sol. Costi non esposti.

## Artefatti e verifiche effettive

Namespace nuovo: `tmp/m1/fft-pocket/runtime-observation-tests-001/`.
Suite: `test_runtime_observation.py`, 658 righe, 33754 byte, SHA-256
`f96db0b0e3925fef78d22dc5020ea2dade2f816d51dea25f013e9b2eb2d9dd2d`.
Ricevuta: `preparation-static.json`. Python 3.14.4, Linux,
radice `/opt/workspace/astroProcessor`.

Verificati solo hash, AST e compilazione sintattica senza eseguire il codice.
Supervisor: 35010 byte, SHA-256
`9b547767f2d5994cde59d8b807ee0129ad770ac48ab6920df37b6a9851a1354f`, coincidente.
Letti supervisor (736 righe), report001 (102), static001 (42), diff001 (212).
Lo snapshot precedente è stato letto come byte per il confronto AST.

Comando statico riproducibile, equivalente ai controlli eseguiti:

```sh
/usr/bin/python3 -I -B -S - <<'PY'
import ast, hashlib, json
from pathlib import Path
base = Path('tmp/m1/fft-pocket')
raw = (base / 'supervisor.py').read_bytes()
assert hashlib.sha256(raw).hexdigest() == '9b547767f2d5994cde59d8b807ee0129ad770ac48ab6920df37b6a9851a1354f'
p = base / 'runtime-observation-tests-001/test_runtime_observation.py'
suite = p.read_bytes()
assert hashlib.sha256(suite).hexdigest() == 'f96db0b0e3925fef78d22dc5020ea2dade2f816d51dea25f013e9b2eb2d9dd2d'
tree = ast.parse(suite, filename=str(p))
compile(tree, str(p), 'exec')
assert sum(isinstance(n, ast.FunctionDef) and n.name.startswith('test_')
           for c in tree.body if isinstance(c, ast.ClassDef) for n in c.body) == 33
names = json.loads((base / 'runtime-observation-001-static.json').read_text())['unchanged_ast']
assert len(names) == 16
def symbols(raw):
    return {n.name: ast.dump(n, include_attributes=False) for n in ast.parse(raw).body
            if isinstance(n, (ast.FunctionDef, ast.ClassDef))}
before = symbols((base / 'snapshots/pre-runtime-observation-001/supervisor.py').read_bytes())
after = symbols(raw)
assert all(before[n] == after[n] for n in names)
print('STATIC_ONLY: 33 planned methods; 16 unchanged AST symbols')
PY
```

Il report001, riga46, dice 17 simboli: è un refuso. La ricevuta ne elenca 16,
tutti verificati invariati rispetto allo snapshot. Lo storico resta intatto.

## Copertura predisposta, non eseguita

- RuntimeMap: sei oggetti completi, incompleti poi completi, assenti fino all'uscita,
  pre-exec, uscita durante exe/maps o dopo lettura, ownership persa, pathname/inode,
  unexpected/deleted, no-exec, grammatica/UTF-8, cap esatto e cap+1, stat mutata.
- Errori lettura, scrittura, flush, fsync, sync directory e ricevuta JSON:
  nessuna receipt in memoria; integrazione read/write/fsync con FIRSTFAIL,
  intermedi conservati e nessun COMPLETE. FIRSTFAIL preesistente non sovrascritto.
- Identità: hash valido, pin errato, fstat/stat mutati, file non regolare,
  dimensione errata/oltre cap, open/read falliti. Cleanup reale senza segnali dopo
  reap o perdita ownership; run reale con cleanup dopo errore observer.
- Qualify reale con I/O virtuale: 232 chiamate simulate, observer solo088,
  192 confronti mock, contratto stderr, nessun campione blocca al caso088,
  recheck finale mutato blocca COMPLETE. Provenienza build/validate separata;
  build corrente rifiuta il pin storico prima di compiler/namespace.
- Prenotazione massima esatta 34111558 byte, inclusi entrambi gli header;
  quota insufficiente di un byte ferma prima del namespace e dei figli.
  Verificato anche il confine ammesso e run senza observer.

LOW non risolto: `runtime_identities` legge fino a EOF senza un limite cumulativo
dopo il fstat iniziale. La mutazione finale provoca STOP ma non limita la lettura.
`test_hash_read_must_be_bounded_known_low` richiede il limite e resta un normale
test rosso previsto sul sorgente pinnato, senza skip/expectedFailure o falso PASS.
Simula 257 blocchi da 65536 byte, riusando un unico blocco; nessun payload host.
Il fallimento è previsto dall'analisi, non osservato: oggi la suite non è stata avviata.

## Turno futuro e limiti

Dopo review indipendente, unico turno mock previsto, non eseguito qui:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-observation-tests-001/test_runtime_observation.py
```

Il runner autentica il supervisor prima di caricarlo. Nega Popen, open host,
waitid, killpg e fsync non esplicitamente simulati. Nessun worker/loader reale;
fixture e oracle scientifici sono mock nei test di controllo e non vengono qualificati.
Budget preparazione: nuovi file sotto 64 KiB complessivi; nessun payload FFT,
cache o temporaneo esterno. Il turno mock usa I/O virtuale, senza scratch su disco;
RSS e durata non misurati. Nessun nuovo preregistro scientifico.

Supervisor, worker, manifest/pin001, build003 e test storici non modificati.
Oracle e tolleranze restano intatti; AST invarianti verificati, nessuna prova numerica nuova.
Nessun intervento su series001, progress, piani o indici; nessuna rete/installazione/delega.
Deviazione: eseguito inizialmente un `git status --short` in sola lettura, contro NoGit.
Nessuna scrittura Git o pubblicazione.
