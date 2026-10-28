# M1 A: preparazione del ritiro del solo output C++ di series-001

Stato: `PREPARED_ONLY_NOT_RETIRED`. Nessun ritiro eseguito. La review indipendente deve precedere un'operazione di ritiro distinta.

## Ricevuta e oggetto

- Namespace: [`tmp/m1/a/retire-series001-cpp-001/`](../../tmp/m1/a/retire-series001-cpp-001/).
- [`manifest.json`](../../tmp/m1/a/retire-series001-cpp-001/manifest.json), SHA-256 `d1c5b57c5d60c593107d556b3249086b88e663694c76c1d8035b951ece0d5885`.
- [`retire-proposal.json`](../../tmp/m1/a/retire-series001-cpp-001/retire-proposal.json), SHA-256 `94f3b4a168f18edd9b747f870dd0056411103adc4dfef3cc8fc1d45e6fa77577`.
- `verification.json` registra il controllo finale di hash, identità, fonti fissate, sintassi AST e budget; fissa anche questo documento.
- Unico oggetto: `tmp/m1/a/series-001/work/cpp.bin`, 33554432 byte, SHA-256 `59edfa8e06f28acd2763d69c0bac41c5593c0033c694e74da0b743d6f77b225c`.
- Identità: device `66306`, inode `29005314`, UID/GID `1000/1000`, file regolare, modo `0664`, nlink `1`. Il manifest conserva anche blocchi e timestamp nanosecondo.

Comando eseguito con uscita 0:

```bash
python3 -B tmp/m1/a/retire-series001-cpp-001/prepare.py
```

Lo script è monouso: un secondo avvio si arresta se trova le ricevute. Non avvia candidati, worker, build o test scientifici. Ambiente: Python `3.14.4`, kernel `7.0.0-38-generic`, `x86_64`.

Prima del payload: disponibilità `125332328448` byte e `34475898` inode; riserva `1073741824` byte e `128` inode. Budget nuovi output `1048576` byte, inferiore al cap `1073741824` byte. Manifest e proposta occupano insieme `20688` byte; nessuna copia del candidato. I JSON e lo script hanno ricevuto `fsync`, così come la directory della ricevuta.

## Collegamento storico verificato

SHA-256 streaming, blocchi da 1 MiB, `O_NOFOLLOW` su ogni componente del percorso e `O_NOATIME` sul file. `fstat` prima/dopo e `stat` del percorso coincidono, inclusi dimensione, identità, owner, nlink e timestamp. Il candidato resta presente e invariato.

Il journal contiene 288 risultati PASS per 96 casi. Ultimo risultato C++: riga **286**, caso `fft-mode-f64-1024-1024-s1041`. Hash di `quality`, `metrics`, fixture attesa dichiarata in `cases.json` e contenuto attuale coincidono. Le ultime due righe appartengono a Rust e NumPy. Nessun confronto numerico nuovo è stato eseguito; la fixture non è stata riletta.

Fonti effettive: `run.py:340-353` sovrascrive il singolo output e rinomina gli insuccessi `FAIL-*`, poi chiude i worker; `run.py:392-405` verifica il manifest prima di campioni e nuovi worker. Il manifest della ricevuta fissa gli hash delle fonti lette.

Verificato anche `run.py` nello snapshot storico: hash uguale al manifest originale. Le righe storiche `266-291` confermano sovrascrittura, rinomina degli insuccessi e chiusura; `318-329` pongono il controllo sorgenti prima della directory di misura e dei worker. Snapshot SHA-256 `421c1f8fc2bc417ea383f818deb39c72725f1acfe7f223fd496314b6aeb11e99`.

`series-status.json` registra `ABORTED_BEFORE_SAMPLES`, uscita `1`, drift di `plans/M1-spikes.md`. Assenti `samples.jsonl`, `cold-process.jsonl` e directory storica `measurement`. Il PASS della validazione resta limitato alle vecchie fixture: non rende PASS la misura abortita.

## Consumer e chiusura cooperativa

Due osservazioni `/proc`, prima e dopo lettura, hanno esaminato 7 processi ciascuna: stesso UID, cwd/eseguibile nel progetto, più discendenti dello stesso UID. Nessun descrittore o mapping riferiva device/inode del candidato. Raccolti solo PID, parent, start-time e metadati necessari; nessuna lettura di env o cmdline.

Limiti: 4 processi dello stesso UID non erano classificabili per permessi. Restano fuori altri UID, namespace esterni e interpreti con cwd esterna non discendenti. Le osservazioni non impediscono aperture future e non provano un'assenza universale. Il committente dichiara assente un runtime writer concorrente; il revisore parallelo legge soltanto un altro delta FFT.

Consumer noti verificati: `audit.py:60,84-103` usa soltanto output di `series-002`; `final_checks.py:21-27` legge manifest, snapshot e stato di `series-001`, non questo candidato. `series-002/oracle.json` dichiara `reused_results: false` e riusa soltanto fixture immutabili. L'ispezione mirata non equivale a una ricerca esaustiva di consumer dinamici.

La ricevuta chiude durevolmente, come abbandonato rispetto a questo solo output, il ramo storico di misura già abortito. Nessun resume o nuova validazione/misura deve riaprire questo percorso storico. Non cambia lo stato scientifico né alcun file storico.

La proposta richiede soltanto review indipendente e successivo ritiro distinto. Prima del ritiro si ricontrollano percorso, identità, hash e chiusura cooperativa; mismatch o consumer attivo impongono STOP. Restano conservati journal, summary, manifest, snapshot, versioni, oracle, bounds, stderr, FIRSTFAIL e fixture. Nessun altro candidato rientra nella proposta.
