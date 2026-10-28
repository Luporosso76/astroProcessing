# M1 A: ritiro tecnico del solo cpp.bin storico

Esito osservato: `RETIRED_SINGLE_AUTHORIZED_TARGET`. Un solo unlink eseguito; `tmp/m1/a/series-001/work/cpp.bin` è assente. Nessun altro file è stato ritirato. Il ramo scientifico resta `ABORTED_BEFORE_SAMPLES`.

Ricevute: [`tmp/m1/a/retire-series001-cpp-002/`](../../tmp/m1/a/retire-series001-cpp-002/).

- `authorization.json` conserva il mandato e il READY condizionato della review `ses_e87d8ef4cffecy54WH4vj4XGa0` come comunicati dal coordinatore. Non attesta una lettura della sessione originale, non accessibile con gli strumenti disponibili. L'ultimo mandato assegna questo turno esclusivo senza altri writer o candidati attivi.
- `preflight.json`, SHA-256 `a71e41b180f2c75a5f84100690f1802883f8763b7cddff36db26a8e10d30d091`, è stato persistito con `fsync` prima dell'unlink, insieme ad autorizzazione, script e directory della ricevuta.
- `final.json`, SHA-256 `67d943748f1596fe8d577add464daf8ed6bce806886241947900b09294de0351`, registra assenza, stat del descrittore ancora aperto dopo unlink, chiusura, spazio e conservazione delle fonti. Anche questa ricevuta e le directory hanno ricevuto `fsync`.

Comando eseguito una sola volta, uscita 0:

```bash
python3 -B tmp/m1/a/retire-series001-cpp-002/retire.py
```

Prima dell'esecuzione sono stati riconciliati percorso e namespace: erano presenti soltanto `authorization.json` e `retire.py`, senza precedenti preflight/final/stop. Controllata la sintassi AST dello script; nessun test o worker eseguito.

Autenticati manifest preparatorio `d1c5b57c5d60c593107d556b3249086b88e663694c76c1d8035b951ece0d5885`, verification `a6d3f24c28e118586214e74be1a5c911d627ba82a07e1aef383d2bd19a5f8ca1` e tutti i 16 pin. Candidato: 33554432 byte, device `66306`, inode `29005314`, UID/GID `1000/1000`, nlink `1`, SHA-256 `59edfa8e06f28acd2763d69c0bac41c5593c0033c694e74da0b743d6f77b225c`. Ultimo PASS C++ confermato alla riga 286 del journal, caso `fft-mode-f64-1024-1024-s1041`.

Due consumer check freschi hanno esaminato 7 processi ciascuno, nello stesso UID e perimetro progetto/discendenti. Nessun consumer del candidato, mapping, eseguibile candidato attivo o processo progetto irrisolto. Nel secondo check il solo descrittore del verificatore era aperto in lettura. Quattro processi non classificabili non sono stati stabiliti come appartenenti al progetto; restano fuori dalla prova cooperativa. Nessuna lettura di env o cmdline.

Ogni componente del percorso è stato aperto con `O_NOFOLLOW`; payload con `O_NOATIME`. Dopo il preflight durevole, hash streaming del descrittore trattenuto, confronto stat/fstat e identità della directory riaperta hanno preceduto l'unica chiamata `os.unlink('cpp.bin', dir_fd=work_fd)`. Il controllo hash finale è terminato a `1793020572098571584` ns; unlink iniziato a `1793020572098709631` ns. La directory ha ricevuto `fsync`. Lo stat del descrittore dopo unlink conserva device/inode e dimensione, con nlink `0`; il descrittore è poi stato chiuso.

Spazio disponibile osservato prima dell'unlink: `125332004864` byte. Con descrittore ancora aperto: invariato. Dopo chiusura: `125365559296` byte. Incremento disponibile e libero osservato: **33554432 byte (32 MiB)**; un inode disponibile in più. Sono osservazioni del filesystem, non una quota globale atomica o un'ammissione FFT. Una futura FFT richiede un nuovo preflight.

I 15 pin di evidenza residui sono stati riletti dopo il ritiro e risultano invariati. Conservati ricevute preparatorie, documento preparatorio, fonti, originali, FIRSTFAIL, journal, summary, manifest, snapshot, versioni, oracle, bounds, stderr e fixture. Nessun archive/compress/chmod, test, build, FFT, Git, rete, installazione, delega, indice o modifica progress.

Limite: il modello è cooperativo e senza writer concorrenti, come autorizzato. `NOFOLLOW`, identità e unlink relativo al descrittore non costituiscono compare-and-unlink atomico contro sostituzioni ostili ABA dello stesso UID. Non viene rivendicata una prova universale sui processi.
