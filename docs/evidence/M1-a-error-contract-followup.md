# M1.a: follow-up statico del contratto di errore

Consegna stabile del 25 ottobre 2026, candidato **`v3-followup-001`**, ancora mai compilato, importato o eseguito. Correzioni preparate per i due MED del parser C++ e del supervisore IPC. **Validazione dinamica NON ESEGUITA; nessuna chiusura autonoma dei finding o del gate M1.a.**

Autore: `openai/gpt-6.1-sol`, modello autodichiarato dal runtime, non attestato. Nessuna delega. Costi e token non esposti. Accessi iniziali: core.cpp righe 95-120, run.py 1-210, validate.py 265 righe, check_static.py 53, validate.sh 36, handoff.json 18; poi diff dei file interessati e metadati tecnici. Nessuna rilettura integrale del piano o di M0.

## Versione precedente preservata prima delle modifiche

Namespace esclusivo: `tmp/m1/a-fault-contract-v3/`. Prima delle patch sono stati copiati sorgenti, script e tutte le tre ricevute storiche in **`snapshots/pre-followup-001/`**. `provenance.json` registra gli hash dei file copiati e nessuna differenza rispetto agli hash della ricevuta precedente. I sorgenti storici non sono il candidato attivo.

Manifest dello snapshot: `snapshots/pre-followup-001/MANIFEST.sha256`, SHA-256 **`11ede783a966b937fe384a3c277c1fa04995e73aed366a84d9d523b8d24758f8`**. Il controllo statico ha verificato ogni file elencato. Nessuna ricevuta precedente è stata cancellata o sovrascritta.

`candidate-version.json` identifica il candidato nuovo e separa le destinazioni future: `build-followup-001/` e `validation-followup-001/`. Il vecchio report `docs/evidence/M1-a-error-contract.md` resta di 85 righe, SHA-256 **`87f4caa029dd7f0c56b752d889e2937763a04326f8c7536d901fe565bd5d3acc`**.

Protocollo congelato: `plans/M1-spikes.md`, SHA-256 **`1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`**, ricontrollato senza modificarlo. Nessuna scrittura negli originali `tmp/m1/src`, serie 001/002, ownership, M1.c, e-vulkan, progress o report altrui. Nessuna rete, installazione, operazione Git, lettura di segreti o temporaneo esterno.

## Modifiche al candidato

### Parser C++

`src/core.cpp` estrae prima esattamente otto token whitespace in un array di stringhe. Solo dopo converte h, w e stride tramite `std::from_chars`, richiedendo esito senza errore e consumo dell'intero token. Suffissi, overflow, campo mancante o campo aggiuntivo causano stop prima del kernel e dell'output, con diagnosi `STOP:` ed exit 2. EOF pulito e righe whitespace restano validi; una richiesta completa senza newline finale resta valida.

È preparata la regressione concreta di sette token con stride concatenato al percorso di un input reale. Il vecchio parser poteva separare il prefisso numerico e riutilizzare il suffisso come campo input; il nuovo controlla il numero dei token prima di convertire qualsiasi dimensione. Restano test per suffissi su tutte e tre le dimensioni, extra e troncamenti.

### Lettura IPC e cleanup

`src/run.py` rende non bloccante il FD stdout del proprio Popen. Il supervisore usa `os.read` incrementale, non `stdout.readline()`. Il buffer conserva le righe già ricevute e resta entro **65,537 byte**; il limite della singola riga, escluso newline, è **64 KiB**. Non è un limite su immagini, precisione o output scientifici: riguarda soltanto il messaggio IPC di stato.

La deadline monotona non si rinnova all'arrivo di frammenti. Ogni giro ricontrolla RSS e deadline, anche senza newline. I default restano **180 s** e **512 MiB RSS**, con polling massimo di 20 ms. EOF della pipe non attende più indefinitamente un processo ancora vivo.

Il costruttore libera selector e stream e termina/raccoglie il proprio Popen se l'handshake fallisce. Le guardie di deadline, RSS e dimensione della riga registrano un evento `ipc_guard` e fanno cleanup prima dell'errore. `close` è idempotente; kill e wait riguardano esclusivamente il Popen posseduto, mai PID esterni o gruppi di processi. Le wait hanno guardie di 10 s; un loro fallimento resta errore, non successo.

Il journal di launch e richieste osservati e i due mutation audit anti-dispatch restano attivi. Nessun contatore costante reintrodotto e nessun backend sostitutivo. L'osservabilità resta limitata al dispatcher strumentato, non agli interni FFTW/NumPy. I controlli inverse NaN/Inf e i bound scientifici non sono stati allentati.

## Test aggiunti o conservati: soltanto previsti

- **Parser: 14 casi previsti**, contro gli 11 precedenti. Aggiunti suffisso sulla larghezza, suffisso sullo stride e stride concatenato al percorso reale. Restano suffisso sull'altezza, overflow, extra, troncamenti, due EOF puliti e riga finale completa senza newline.
- **IPC: 10 casi previsti** in `ipc_regressions.py`, con figlio controllato `ipc_fixture.py`: handshake parziale, silenzioso, malformato e sovradimensionato; stdout chiuso mentre il figlio resta vivo; risposta parziale e sovradimensionata; RSS senza newline; frammentazione valida e due righe nella stessa lettura.
- Ogni caso IPC verifica launch/uscita con lo stesso PID osservato, processo raccolto e stream chiusi. Il fixture ha identità esplicita `ipc-fixture`, registrata solo nel contesto del test e poi rimossa; non viene presentato come backend C++, Rust o NumPy.
- I test IPC usano deadline **0.5 s**, più stretta del default; il solo caso RSS imposta **1 KiB** per provocare il rifiuto senza grandi allocazioni. Un watchdog separato di **3 s** può terminare soltanto il Popen creato dal caso. Qualsiasi intervento del watchdog o cleanup di soccorso fa fallire il test; non può trasformare un hang in PASS. La guardia di completamento di **2.5 s** è un limite del test, non benchmark o SLA.
- Conservati **360 confronti finiti**, **36 vecchi errori**, **36 inverse NaN/±Inf**, **6 copie speciali** e **2 mutation audit**. Nessuna fixture eliminata, epsilon cambiata o reinterpretazione dei tempi della serie 002.

Questi test non sono stati importati né avviati. Non esistono risultati numerici, processi fixture osservati o prove runtime di cleanup in questa consegna. La raccolta dei figli e il controllo RSS sono da verificare sui figli controllati; non costituiscono una garanzia universale contro OOM o processi non terminabili dal kernel.

## Controlli effettivamente eseguiti

Solo parsing, hash e metadati, con Python **3.14.4** e `bash -n`. Nessuna compilazione C++/Rust, import dei candidati, fixture IPC, test dinamico o benchmark. Il checker importa soltanto librerie standard.

```bash
/usr/bin/python3 -B tmp/m1/a-fault-contract-v3/check_static.py --output tmp/m1/a-fault-contract-v3/static-checks-followup-001.json
```

Exit **0**: **7 file Python** validi per `ast.parse`; script bash sintatticamente valido; integrità dello snapshot, protocollo congelato e hash originali verificati. Nuova ricevuta separata: **`static-checks-followup-001.json`**, SHA-256 **`a8ad707607271e6034ef8d8467b8b68c813a52e1423ad9cc61961ad9b0baa9df`**. Gli hash completi degli undici file attivi sono nella ricevuta.

Alla fotografia della ricevuta, blocchi dei file regolari sotto tmp: **815,394,816 byte**, cap **1,073,741,824 byte**. `du -s -B1 tmp` successivo: **817,184,768 byte**; namespace v3 incluso snapshot: **229,376 byte**. Altri autori possono modificare l'occupazione globale: questi valori non sono una prenotazione. In questo passaggio sono stati aggiunti soltanto piccoli sorgenti e metadati.

SHA-256 dei file modificati o aggiunti, relativi al namespace v3:

```text
b248b84cd6b8a06c487568825dfdd3e02662418367abbd803b3b1603b15363df  src/core.cpp
ec67bd263d300c734e74c1a13489c72af38cbd9199a52381625486973b8eda98  src/run.py
d8917f372546e8fa9be4854b4b820b7525c0cc7c22a661813aeb95da3404f1f5  validate.py
3421f44aa31743c3d1a5f12d68b6edbec7ae6a6ad276b4846161672f5fdbd9c6  ipc_fixture.py
cd1cf8d6fe4c13f231a80b80037d0d650a1eb81ee999bc373764d43a75e154d4  ipc_regressions.py
6be8f765905c864f66009257009150b1a174d818b9fe0de0b1b3bcf8af68b3c5  check_static.py
d559890c4c479021db7c9c26a398cf333594e94f9767b838ce89248591c66753  validate.sh
06687fabc06f88d48ad7415b58f0425ef262d56fe568c57054c4bb4263c89e72  candidate-version.json
```

## Primo comando futuro, non eseguito

**Attendere una nuova sessione test esclusiva ammessa dal coordinatore.** M1.c ha già l'ammissione; questo incarico non ne acquisisce una. Il flag seguente non acquisisce un lock. `RUST_TOOLCHAIN_DIR` deve identificare la toolchain diretta già disponibile.

```bash
bash tmp/m1/a-fault-contract-v3/validate.sh --admitted-test-session
```

Lo script creerà esclusivamente le nuove directory di build/validazione follow-up, controllerà quota e hash e registrerà versioni effettive prima dei test. Un guasto produrrà stop e diagnosi, senza ridurre dataset, precisione o candidati. Sessione dinamica e review indipendente restano il primo lavoro incompleto; gli eventuali risultati avranno un record nuovo, senza riscrivere questo rapporto o quello precedente.
