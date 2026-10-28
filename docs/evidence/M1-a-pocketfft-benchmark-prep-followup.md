# M1.a: followup della preparazione benchmark pocketfft

## Perimetro ed esito

Il proprietario ha autorizzato la correzione dei tre rilievi MED e i soli test
mock/unitari. Sono stati modificati `benchmark.py`, `test_protocol.py`, `PLAN.md`
e `manifest.json` nel namespace `tmp/m1/fft-pocket-benchmark/`.
Questo documento registra le prove; non sostituisce la review indipendente del delta.

Esito del turno: 38 test mock/unitari PASS, exit0. Compilazioni, worker nativi,
FFT e benchmark reali: 0 RUN. I nove negativi nativi e i 192 casi finiti del
nuovo driver restano NON ESEGUITI. Nessuna misura assoluta è stata acquisita.

`driver.cpp` e l'adapter sono invariati. Restano invariati i 192 descrittori,
l'oracle analitico, i confini `total_s`/`adapter_s`, il supervisore esterno,
il profilo003 e le prove storiche. Non sono stati usati Git, rete, installazioni,
indici, deleghe o override di modello.

## Diff dei rilievi

### MED1: terminazione ordinata locale

Il runner definisce ora `OwnedGroup`, `group_stopped` e `cleanup` locali.
`Child.close` tenta la chiusura normale se ha ricevuto READY e non ha già tentato
QUIT. Un timeout o errore non provoca un secondo QUIT. Un avvio senza READY
passa alla terminazione tramite segnale, perché il protocollo non è inizializzato.

Se il gruppo persiste, il runner invia SIGTERM, attende al massimo1 s e passa a
SIGKILL se necessario, con ulteriore attesa massima5 s. Il QUIT di cleanup usa
timeout1 s per la risposta e1 s per uscita/drain. Il percorso normale mantiene
i timeout10 s già previsti. L'escalation riguarda la terminazione dello stesso
processo, non un fallback del candidato o un retry dell'esperimento.

Ogni segnale verifica prima il leader con `waitid(...WNOWAIT)` e controlla PID,
sessione e gruppo. La riserva resta attiva durante il wait consumante e viene
rilasciata solo dopo il reap riuscito. Il tentativo di reap chiude preventivamente
i segnali; un reap fallito mantiene la riserva e blocca i lanci successivi.
Un'identità persa vieta ulteriori segnali e conserva il processo come irrisolto.
Census, reap e chiusure producono diagnosi indipendenti; il supervisore esterno
non viene modificato né usato per il teardown del driver.

Estratti del diff, non una riproduzione completa delle nuove funzioni:

```diff
-        self.owner, self.selector = utility.OwnedGroup(), None
+        self.owner, self.selector = OwnedGroup(), None
+        self.ready, self.quit_attempted = False, False

-    def finish(self):
+    def finish(self, timeout=10):
+        self.quit_attempted = True

-        errors = safety().cleanup(self.owner, self.selector)
+        graceful = (lambda: self.finish(timeout=1)) if self.ready and not self.quit_attempted else None
+        errors, termination = cleanup(self.owner, self.selector, graceful)
```

La ricevuta `process.json` registra anche il percorso di terminazione.
Gli errori irrisolti mantengono il blocco di nuovi lanci e gli intermedi.

### MED2: IQR singleton

`timing_summary` calcola n, mediana, MAD non scalata, IQR, minimo e massimo.
`absolute_summary` applica la funzione a entrambi i timer senza alterare la
verifica delle 21 coppie p/r per caso. Non introduce rapporti o IC singleton.

Convenzione: ordinati i dati, Qp interpola linearmente all'indice zero-based
`(n-1)*p`; IQR = Q0.75 - Q0.25. È la convenzione Hyndman-Fan tipo7, implementata
con `statistics.quantiles(times, n=4, method='inclusive')`.

```diff
+def timing_summary(times):
+    median = statistics.median(times)
+    quartiles = statistics.quantiles(times, n=4, method='inclusive')
+    return dict(n=len(times), median=median,
+                mad=statistics.median(abs(v - median) for v in times),
+                iqr=quartiles[2] - quartiles[0], min=min(times), max=max(times))
```

I test verificano valori predeterminati:

- `[9,0,2,1]`: mediana1.5, MAD1, Q1=0.75, Q3=3.75, IQR3.
- Cinque0, sei2, cinque10, cinque100: mediana2, MAD2, IQR8.
- `1..7` ripetuto tre volte: mediana4, MAD2, IQR4; timer adapter scalato per1/2:
  IQR2.

### MED3: copertura e conteggio

Il conteggio AST e quello del runner unittest concordano: 38 metodi.
Dei 22 metodi `ProtocolTests`, 20 erano già predisposti e due coprono IQR/MAD.
I 16 nuovi metodi `LifecycleTests` esercitano il codice reale del runner con
Popen, waitid, segnali, selector, letture OS, clock, census e ricevute mock.

Copertura: READY/RUN/QUIT normale, QUIT da close, eccezione dell'oracle,
timeout QUIT, SIGTERM senza SIGKILL, escalation completa, nessun secondo QUIT,
timeout READY, broken pipe, identità persa, reap fallito, segnali vietati dopo
reap, SIGKILL inefficace con attesa finita, chiusure indipendenti, census fallito,
leader zombie riservato con discendente vivo e Popen fallito prima dello startup.
Gli assert dentro i mock verificano la riserva durante segnali e wait consumante.

## Esecuzione e ambiente

Comando effettivo del solo interprete di test:

```text
/usr/bin/python3 -I -B -S /opt/workspace/astroProcessor/tmp/m1/fft-pocket-benchmark/test_protocol.py -v
```

Un launcher Python stdlib inline ha creato esclusivamente
`tmp/m1/fft-pocket-benchmark/prep-followup-mock-001/` con mode0700 e cache privata.
Ha impostato AS268435456 byte, file1048576 byte, core0 e CPU50 s, ereditati dal
figlio di test; non sono stati campionati indipendentemente dentro il figlio.
Deadline esterna55 s, own Popen in nuova sessione, osservazione WNOWAIT e reap
consumante; TERM1 s/KILL5 s erano previsti solo in caso di timeout del test.
Il figlio è terminato normalmente: nessun segnale reale inviato.

Ambiente registrato: Linux, Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.
La stringa GCC descrive l'interprete, non una compilazione del driver in questo turno.
Ambiente child minimale, HOME/TMPDIR/XDG_CACHE_HOME nella cache privata.

`stderr.log`: `Ran 38 tests in 0.087s`, `OK`. `stdout.log` vuoto.
Exit0, nessun timeout; durata esterna dell'interprete0.2113744020462036 s.
Questi sono tempi del test harness, non campioni prestazionali FFT.

Le ricevute `before.json` e `result.json` fissano comandi, budget, hash dei due
Python effettivamente testati e risultato. I quattro file del turno sono
autenticati in `manifest.json/followup_test_artifacts`; nessuna prova è stata
sovrascritta e non è stato necessario un secondo tentativo.

## Verifiche statiche e limiti

È stato eseguito `ast.parse` su tutti i Python del namespace (`benchmark.py`,
`test_protocol.py`), senza importare il supervisore o candidati. Esito PASS.
Il controllo statico ha verificato i 12 pin sorgente del manifest, i quattro
artefatti del turno e il conteggio38; i test hanno importato solo il nuovo runner.

La validazione sintattica completa C++ resta NON VERIFICATA. Il driver188 righe
è invariato e il suo SHA-256 coincide con la consegna precedente. Le ricerche
locali non hanno trovato un parser tree-sitter disponibile: i moduli Python
`tree_sitter`/`tree_sitter_cpp`, il comando tree-sitter e il grammar WASM nei
percorsi CodeGraph esaminati mancavano. Non è una prova che nessun parser esista
sull'intero host. Non sono stati eseguiti compilatori, neppure `-fsyntax-only`,
né sono stati installati strumenti. Un hash invariato non attesta sintassi C++.

Questa limitazione non chiude il gate C++, la review del delta, l'inventario
runtime del futuro binario o le licenze. Le prove mock non certificano il
teardown di processi reali, processi ostili fuori sessione o power loss.

## Hash finali e preservazione

SHA-256 dei file modificati, dopo i test e l'allineamento documentale:

| File nel namespace benchmark | Byte | SHA-256 |
| --- | ---: | --- |
| `benchmark.py` | 31820 | `fca630935cb43b7b8e47bd6900f757d69ea306c447efc63a08c6969fb0ef7bba` |
| `test_protocol.py` | 17455 | `55a415fdd0cb1f052befd4e157121241fe3f3f370e04c94257afbe0770c01c41` |
| `PLAN.md` | 8234 | `483feacd05b827650de60acc370bd5a1edb3f66f414ea202c9060c7fd1b0aca9` |
| `manifest.json` | 12610 | `32dcdf923232891771ff47d9f8bd0c412fc30ec329fef97f4e0c7304f3a9dd16` |

Baseline prima del followup: benchmark
`61806f663a3c077f3994106cce0e1d804ae577342240f751a4b31f7dbd157fff`, test
`0c74cf62c79e2c2c8744c3956434558b882cb01eed1e37c233919132fe5a75bd`, piano
`c3e02a34cfc1a0bc468bf27bf29727780bba3db41a4ffd0c8bb19e9726e5bde1`, manifest
`48eb51a6042dd38bc9c3f4470fde76dba5b16a5c10c9343f72a970cc60e2a582`.

Il manifest contiene i pin aggiornati dei sorgenti e degli artefatti mock.
Il suo hash viene registrato qui, senza tentare un'autoreferenza nel manifest.

Driver invariato: `23116dbf3aa9ea42fca97b882532c0f74579761e2b5c1b4d75387c62749b2cd0`.
Adapter invariato: `089232eaca3c3d6f73655952db64ed3683ebfbf456066ad46ff893bba38ad090`.
Supervisore esterno invariato: `ad776c3bac3a0b953b2b8a8ac85dd6c395ecf0e5c85a3bf95179d4553ab21cb4`.

I sette `tmp/**/FIRSTFAIL*` conservano l'aggregato SHA-256
`5c32e5fa5d456d46df2443a550d69b4a44ee1d16a6d9522d8c2fc1f6bf37817c`,
calcolato con la convenzione già dichiarata nel manifest. Nessun FIRSTFAIL è
stato rimosso, modificato o creato dal turno mock PASS. `docs/progress.md` e
gli indici non sono stati toccati.
