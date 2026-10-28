# M1.a pocketfft: build 003 eseguita, nessuna qualifica numerica

## Esito

PASS_BUILD_ONLY. Un solo lancio del supervisore, PID `917720`, exit `0`,
raccolto con waitid. Compilazione e acquisizione metadati ELF: exit `0`.
Nessun errore di esecuzione, cleanup o controllo finale. Nessun retry.
Il worker prodotto è un ELF di 180200 byte; non è stato eseguito.
Zero casi FFT, benchmark, GUI o nuovi backend. Nessuna adozione dedotta.

L'ammissione del coordinatore riguarda solo questo turno BUILD. La review
`ses_e88478cf0ffeZkrXH3wReaTRxL`, comunicata nel mandato, conferma i 36 mock
della preregressions-003; non costituisce prova di compilazione né review di
questa build. Nessun seguito automatico, inventario runtime completo o
qualifica dei 232 casi è autorizzato da questo risultato.

## Gate prima del lancio

Autenticati 97 percorsi mediante byte/SHA-256 e identità stat prima/dopo
lettura. Aperture O_NOFOLLOW su file e componenti directory, senza import del
candidato. Sorgenti correnti, piano locale, upstream, manifest precedenti,
FIRSTFAIL storico e snapshot sono coerenti con i pin esistenti.

I mapping degli snapshot sono quelli del driver preregressioni autenticato:
`baseline_snapshot`: campo `snapshot_file`, 8 copie;
`regressions_baseline_snapshot`: campo `file`, 22 copie;
`build_budget_baseline_snapshot`: chiavi relative di `files`, 29 copie.
Controllati i mode write-protected delle copie. Il piano principale e
`plans/M1-spikes.md` sono verificati per byte/hash, senza rilettura semantica
del piano principale. Nessun pin è stato rigenerato dagli output nuovi.

Il manifest corrente, 6720 byte, conserva SHA-256:

```text
9e66b7ed388813da15d0abe7cb04ce6f704af02a64f29640ad6639523c807a75
```

Il wrapper autentica il vecchio driver attraverso handoff/artifact-manifest
esistenti. Esegue solo le sue definizioni stdlib fino alla riga 111 mediante
AST, non il suo turno né un import di sorgenti candidati. Riusa il pin presente
nel driver autenticato e lo confronta con l'unico pin del mandato nel wrapper.

Verificato il binding handoff/report/artifact-manifest, quindi 18 record delle
preregressioni: risultati, complete, setup limiti e raw. La complete lega
conteggio 36, success true, manifest corrente e hash dei risultati. I 36 nomi
distinti con `ok` coincidono con i metodi AST e con execution.json; failure,
errori, skip, expectedFailure e unexpectedSuccess sono zero. Nessun rerun mock.
Entrambe le autenticazioni storiche dei 97 percorsi coincidono anche per stat
con quelle di questo turno.

La CLI effettiva è stata letta in `supervisor.py`: arità 4, sottocomando build,
nome nuovo e nome delle preregressioni. Non è stata modificata o provata con
un lancio esplorativo. Build, outer e report risultavano assenti. GCC e readelf
locali erano disponibili; nessuna installazione o rete.

## Comandi eseguiti

Directory di lavoro: `/opt/workspace/astroProcessor`. Un solo comando outer:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/build-turn-driver-followup-003.py
```

Il suo unico sito Popen avvia, senza shell o preexec:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/supervisor.py build build-followup-003 preregressions-003
```

Il supervisore esegue soltanto i tre comandi previsti dalla ricetta:
`/usr/bin/g++ --version`, la compilazione con i flag immutati del manifest e
`/usr/bin/readelf -d -V` sul nuovo worker. Il comando completo del compilatore
è conservato in `build-followup-003/manifest-before-build.json`.
GCC osservato: `g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0`.
Compile stdout/stderr vuoti; stdout/stderr del supervisore vuoti e salvati.
L'output versione GCC e il testo readelf sono nei rispettivi JSON raw.

Dopo il turno, un comando `/usr/bin/python3 -I -B -S -` con heredoc stdlib
ha controllato soltanto artefatti, hash/stat e AST del wrapper. Nessun import
del candidato, nuovo wait, segnale o lancio del compilatore. Esito:
`PASS_ARTIFACT_CHECK_ONLY_NO_CANDIDATE_RERUN`.

## Risorse e limiti reali

Preflight scratch globale: 957382656 byte allocati, 5445 inode distinti;
hardlink deduplicati, symlink non seguiti, nessuna fixture storica letta.
Quota 1073741824 byte, riserva 67108864 e margine 16777216;
residuo oltre riserva/margine 32473088 byte. Spazio filesystem disponibile
125335920640 byte; MemAvailable 19452432384 byte.

Il setup reale build registra requested/inherited/effective, stato APPLIED:
AS 1073741824, RSS best effort 536870912, FSIZE 67108864 e CORE 0 byte.
Ogni coppia soft/hard effettiva è esattamente il valore richiesto.
AS/RSS/FSIZE ereditati illimitati; CORE soft 0, hard illimitato.
Il wrapper verifica tutti gli hard ereditati prima del lancio; il candidato
verifica tutti gli hard prima di ridurne uno. Nessun min, retry o launcher.
I figli ereditano questi limiti; non sono campionati indipendentemente per
attestare i rispettivi rlimit.

Il setup validate storico richiesto/effettivo resta AS 268435456, RSS 536870912,
FSIZE 67108864, CORE 0; nessun worker validate è stato avviato qui.
Deadline candidate immutate: build 180 s, metadati 10 s, worker 120 s non usata.
Log candidate massimo combinato 65536 byte. Outer: deadline 240 s per contenere
le fasi sequenziali, cleanup 5 s, 65536 byte per stream; nessun rlimit applicato
dall'outer a sé stesso. Il candidato applica i propri limiti prima del dispatch.

Outer, HOME/cache/TMP e directory child sono proprie, mode 0700, sotto ROOT.
Ambiente esplicito: PATH `/usr/bin:/bin`, LANG/LC_ALL `C`, TZ `UTC`,
OMP_NUM_THREADS `1`. Il supervisore riceve HOME/cache/TMP distinti nell'outer;
ogni suo child riceve HOME/XDG_CACHE_HOME/TMPDIR nella propria cache privata.
Verificate 11 directory nuove private e sei HOME/cache/TMP vuote dopo il turno.

RSS massimo campionato del supervisore: 22425600 byte. Il run del compilatore
riporta 3067904 byte sul solo leader g++, non su cc1plus o sull'albero GCC.
Versione GCC e readelf hanno rss_observed false: zero non significa assenza
di memoria consumata. RLIMIT_RSS è best effort su Linux; AS1GiB è per processo,
non un limite aggregato GCC. Nessun picco aggregato è stato misurato.
Scratch massimo campionato: 958402560 byte; dopo autenticazione finale del
driver 957726720 byte, residuo 32129024 oltre riserva/margine. I raw successivi
aggiungono blocchi. Il guard non è una quota kernel atomica.
Tempo outer osservato 4.683131847996265 s: durata operativa, non benchmark.

## Raccolta e conservazione

SIGCHLD default verificato. Il wrapper crea PID `917720` con start_new_session,
osserva con waitid WNOWAIT e verifica SID/PGID prima del segnale al gruppo OWN.
SIGKILL precede il reap; census del gruppo senza altri membri vivi. Waitid
raccoglie exit 0; nessun segnale dopo reap. Pipe/selector chiusi, log fsync,
nessun errore di cleanup; pending_unreaped false e slot_free true nella ricevuta.

Per i child del supervisore, la prova disponibile è il percorso run() fissato
dal sorgente autenticato, ritorno senza errori, build/metadati exit 0 e assenza
di child-stop/FIRSTFAIL. Non è stata raccolta una traccia indipendente dei PID
di GCC/readelf. Il census outer riguarda il gruppo del supervisore, non i
gruppi separati dei suoi child; non certifica discendenti ostili fuori gruppo.
Nessun artefatto è stato cancellato. Cache vuote non implicano una cancellazione
misurata. Il FIRSTFAIL AS256 storico resta intatto.

Prima/dopo coincidono i 97 record, i 18 record preregressioni e metadata di 111
file FFT esistenti, incluso il wrapper appena creato. `docs/progress.md` e
`docs/test-matrix.md` conservano byte/hash/stat. Nessun progress o indice scritto.

## ELF e confine dell'inventario

Worker verificato per magic ELF e hash, non eseguito. readelf riporta NEEDED:
`libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6`.
Le versioni simbolo osservate sono registrate in `artifact-verification.json`.
Non sono stati risolti loader, percorsi effettivi delle librerie, dipendenze
transitive o licenze runtime. Nessun licence PASS, adozione o correttezza FFT
segue dalla sola compilazione e dai metadati NEEDED/versioni.

## Raw e hash SHA-256

Percorsi seguenti relativi a `tmp/m1/fft-pocket/`:

```text
build-turn-driver-followup-003.py, 22918 byte
8a9c3ac591d6a80ff8d02b3b7fc88b6d130199f104721b98ff076e69d516da12
build-turn-followup-003/execution.json, 3454 byte
5274605b242522ad8958ad6d2b83b810f247188b3d539e96990fee9972a49b65
build-followup-003/manifest-before-build.json, 8629 byte
cc29691ccc1db74d6eed749be54df4ab2ac4d5c101c0d43ce5558384913a2ed8
build-followup-003/build-result.json, 523 byte
652f9b6e749afea3dd0ad16872c628047fedba72cac268d1d58badd5df06110b
build-followup-003/build-pin.json, 248 byte
d85b577537a3681f5b1ed6df36830626740878fd3a286a6a9934e05cb8a19655
build-followup-003/worker, 180200 byte
0f724e26f0bce62431390a35f8e8168504e051e26f2266c50355a14a37a585da
build-followup-003/elf-dependencies.json, 4455 byte
0892530b72b099239d2304560b3c1e51b848595770770b81ab29c7a3b6e19781
build-turn-followup-003/artifact-manifest.json, 9176 byte
3bd436a6b6d0995cf2b4097949523c07a7e4056f27750070f9a346ba0ea7228e
```

L'artifact-manifest fissa 20 file nuovi, prima della propria creazione; include
verifica finale, preflight, autenticazioni prima/dopo e stdout/stderr vuoti.
L'handoff separato lega report e manifest senza modificare i raw precedenti.

## Ambiente e handoff

Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`,
`/usr/bin/python3`; Linux `7.0.0-38-generic`, x86_64. Modello dichiarato
dall'ambiente: GPT-6.1 Sol, `openai/gpt-6.1-sol`. Nessun override;
failover runtime, costi e token non esposti, nessuna attestazione inventata.
Nessuna delega, rete, Git, installazione, lettura di segreti o indice.

Fonti lette semanticamente: `supervisor.py` 606 righe, piano locale 277,
driver preregressioni 234, manifest003 221, static003 70, report preregressioni
205, handoff34, execution133 e artifact-manifest246. Sono stati ispezionati
anche i JSON risultati/preflight/build/ELF; i controlli byte/stat sono integrali.
Il piano principale non è stato riletto semanticamente.

Consegna stabile: build riuscita, artefatti conservati, PID raccolto e slot
libero secondo i limiti di osservazione dichiarati. Restano separati review
della build, inventario runtime/licenze e qualsiasi eventuale turno numerico.
