# M1.a: prima validazione runtime del contratto di errore v3

25 ottobre 2026. Candidato **`v3-followup-001`**. **PASS della build e dell'insieme di validazione qui dichiarato**, al primo tentativo runtime. Nessun benchmark o confronto di prestazioni con la serie 002. **M1.a complessivo non è dichiarato PASS.** La review indipendente dei nuovi risultati resta da svolgere.

Autore: `openai/gpt-6.1-sol`, modello autodichiarato dal runtime, non attestazione. Nessuna delega. Costi e token non esposti. Il coordinatore ha ammesso esplicitamente questa sessione esclusiva e riferito la review statica indipendente `ses_e8c4fd1faffex3f7sPIaEJ0Tce`. In questo incarico non sono stati eseguiti altri spike, compute GPU o misure comparative. Nessuna rete, installazione, operazione Git, modifica di prodotto o accesso a segreti.

## Ammissione, quota e input congelati

Sorgenti e destinazioni restano nel namespace `tmp/m1/a-fault-contract-v3/`. Prima del runtime sono stati verificati gli **11 file attivi** rispetto a `static-checks-followup-001.json`; nessuno è cambiato durante build, validazione o audit. Non è stata necessaria alcuna correzione del candidato, nuova versione o ripetizione dei test.

Protocollo invariato: `plans/M1-spikes.md`, SHA-256 **`1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`**. I 120 casi e le fixture delle serie precedenti sono stati riutilizzati in sola lettura. Nessuna rigenerazione, riduzione di qualità, epsilon nuova o reinterpretazione dei tempi 002.

Preflight reale, non valore atteso: **794,128,384 byte** in blocchi di file regolari sotto tmp; `du -s -B1 tmp`: **795,942,912 byte**. Verificata disponibilità di **48 MiB + 8 MiB di margine**, contro il cap fisico globale di **1,073,741,824 byte**. Il wrapper ha poi applicato il proprio controllo da 48 MiB e il validatore i controlli prima degli output grandi. Ricevuta: `runtime-attempt-001/preflight.json`.

La toolchain Rust diretta esisteva già; non è stato usato rustup o Cargo. La directory tecnica della toolchain è stata risolta prima di isolare HOME. HOME, CARGO_HOME e RUSTUP_HOME del processo sono stati indirizzati sotto `runtime-attempt-001/`; temporanei e cache configurabili sotto la tmp del progetto. Nessuna cache personale usata come destinazione.

## Comandi e build eseguiti

Nella radice `/opt/workspace/astroProcessor`, con `ROOT`, `TMPDIR`, `PYTHONDONTWRITEBYTECODE=1`, `PYTHONNOUSERSITE=1` e toolchain diretta configurati:

```bash
bash tmp/m1/a-fault-contract-v3/validate.sh --admitted-test-session
```

Il comando ha effettivamente compilato C++ e Rust, verificato il manifest e avviato `validate.py` su **`validation-followup-001/`**. Exit **0**. Stdout/stderr e codice d'uscita sono conservati in `runtime-attempt-001/{wrapper.stdout,wrapper.stderr,exit-code.txt}`. Lo stderr del wrapper è vuoto.

Build nuova in `build-followup-001/`, senza sovrascrivere alcun artefatto storico:

- C++: `-std=c++20 -O3 -fno-fast-math -ffp-contract=off -Wall -Wextra -Wpedantic`, flag/link FFTW da `pkg-config --cflags --libs fftw3 fftw3f`.
- Rust diretto: `--edition=2024 -C opt-level=3 -C target-cpu=x86-64 -l fftw3 -l fftw3f`.
- Entrambi gli stderr di compilazione sono vuoti: nessun warning o errore osservato. Nessuna build sanitizer eseguita in questa sessione.

Versioni realmente registrate dalla build e dal validatore:

| Componente | Versione |
|---|---|
| G++ | `g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0` |
| Rust | `rustc 1.99.0 (b940084d7 2026-09-28)` |
| FFTW double/float | `3.3.10` / `3.3.10` |
| Python | `3.14.4` |
| NumPy | `2.3.5` |

Manifest e versioni: `build-followup-001/{sources-before-build.sha256,binaries.sha256,cpp-version.txt,rust-version.txt,fftw-version.txt}` e `validation-followup-001/manifest-before-results.json`.

## Conteggi prima e dopo

Prima dell'avvio questa versione non aveva risultati runtime. L'audit ha ricontato gli artefatti grezzi, non soltanto letto il summary.

| Insieme | Prima | Dopo | Esito effettivo |
|---|---:|---:|---|
| Finiti | 0 | 360 | 120 casi immutati × 3 backend; tutti corretti |
| Vecchi errori | 0 | 36 | Exit 2, diagnosi, nessuna risposta o output pubblicato |
| Inverse NaN/±Inf | 0 | 36 | Real/imag, float32/64, tre backend; diagnosi specifica, exit 2, nessun output |
| Copie speciali | 0 | 6 | Confronto bitwise superato |
| Parser C++ | 0 | 14 | Suffissi, stride concatenato al percorso reale, overflow, extra/troncamenti rifiutati; EOF puliti e riga finale completa accettati |
| IPC controllato | 0 | 10 | Parziali senza newline, handshake invalido/timeout, righe eccessive, RSS, pipe chiusa e frammentazione valida verificati |
| Mutation anti-dispatch | 0 | 2 | Tentativi rust/numpy da contesto cpp rifiutati, nessun launch alternativo |

Finiti: tolleranza zero dove preregistrata, compresa la FFT analitica con inverse indipendente; per convr vale la formula **B puntuale invariata**. I bound di 12 casi convr sono stati congelati prima dei nuovi output e applicati ai 36 confronti candidati. Tutti i 360 verdetti grezzi hanno `pass_=true`. Nessuna fixture scartata per ottenere l'esito.

I vecchi errori coprono NaN spaziale, shape, budget, size, kernel sconosciuto e precisione sconosciuta. Tutti i 36 test inverse hanno prodotto `nonfinite inverse input`. Il parser ha rifiutato anche i sette token con stride concatenato al percorso di un file reale, prima della pubblicazione dell'output.

Le due mutation hanno imposto exit 2 nel sottoprocesso di probe, condizione verificata dal validatore. Gli stderr conservati riportano entrambi `STOP: dispatch audit failed`. L'audit successivo ha verificato nei journal un tentativo, un rifiuto e zero launch per ciascuna mutation. Nessun summary accettato è stato prodotto dai probe.

## Journal osservati e cleanup

Ricontati **107 journal**, con **105 launch worker/fixture** e altrettante uscite con lo stesso PID. I due journal mutation contengono soltanto tentativo e rifiuto. Le orchestrazioni CLI dei probe e i processi del compilatore non rientrano in questo conteggio dei dispatcher.

| Backend esplicito | Launch osservati | Richieste inviate osservate |
|---|---:|---:|
| C++ | 41 | 159 |
| Rust | 27 | 146 |
| NumPy | 27 | 146 |
| Fixture IPC | 10 | 4 |
| Totale | 105 | 455 |

I contatori derivano dagli eventi dei journal e sono stati confrontati con i risultati grezzi. Ogni richiesta si riferisce al PID/backend del launch osservato. Non sono contatori attesi presentati come misura. I test selezionano esplicitamente i tre candidati; non passano automaticamente a un altro backend dopo un errore.

Guardie IPC effettivamente intervenute: **3 deadline**, **2 line-size**, **1 RSS**. Gli altri due casi negativi riguardano handshake malformato e stdout chiuso mentre il figlio resta vivo. Tutti e dieci i casi IPC hanno verificato raccolta del processo e chiusura degli stream. **Zero interventi del watchdog o cleanup di soccorso.** Nessuno dei 105 PID registrati risultava ancora presente al controllo successivo; nessun processo esterno è stato terminato.

Le uscite osservate dei 105 worker/fixture sono: **14 con codice 0**, **83 con codice 2**, **8 con codice -9**. Gli 83 sono i 72 errori di input più 11 richieste parser invalide. Gli otto SIGKILL sono stop intenzionali dei soli figli IPC controllati, eseguiti dalle guardie o dal cleanup del costruttore, non timeout del watchdog. Questi errori iniettati sono esiti attesi, non tentativi di validazione falliti.

Il default resta 180 s/512 MiB RSS; i test IPC hanno usato la deadline più stretta di 0.5 s e, soltanto per provocare il caso RSS senza grandi allocazioni, il limite di 1 KiB. Nessun cambiamento a dimensione, dtype o qualità dei casi numerici.

## Audit eseguito dopo la validazione

È stato aggiunto soltanto un auditor di artefatti sotto `runtime-attempt-001/`, fuori dagli undici file del candidato ammesso. Non avvia candidati, fixture o kernel:

```bash
/usr/bin/python3 -B tmp/m1/a-fault-contract-v3/runtime-attempt-001/audit.py
```

Exit **0**; ricevuta `runtime-attempt-001/runtime-audit.json`, log `audit.stdout`, `audit.stderr` vuoto e `audit-exit-code.txt` uguale a 0. La ricevuta include hash degli artefatti raw, conteggi, versioni, quota e controllo di liveness dei PID registrati. Anche il successivo `sha256sum --check MANIFEST.sha256` sullo snapshot pre-followup ha restituito 0, con log conservato nello stesso tentativo.

**295 file protetti invariati**, verificati tramite SHA-256 prima/dopo; includono **147 file della serie 001**, **138 della 002**, sorgenti originali, protocollo e report storici. Verificati anche i relativi insiemi di percorsi, non soltanto il contenuto dei file già elencati. **168 file di fixture distinti** ricontrollati e **11 file attivi invariati**. Lo snapshot di 12 file e le ricevute precedenti restano intatti. I report storici del contratto restano di **85 e 78 righe**, senza modifiche.

Quota dopo l'audit: **800,518,144 byte** in blocchi dei file regolari; `du -s -B1 tmp`: **802,373,632 byte**, sotto 1 GiB. Sono fotografie e controlli preventivi, non una misura continua del picco fisico. Gli altri autori potevano preparare piccoli file; il delta globale non viene attribuito interamente a questa esecuzione.

## Hash di consegna

Percorsi relativi a `tmp/m1/a-fault-contract-v3/`:

```text
d210dc015bfcf6c4b3cc87be434b298f6d2614b1fc2a635fdef72135ce89ceeb  build-followup-001/core-cpp
71c22ae3a9485b4240ca9f1e63e870d28b52d2d1a260f6289ef471060570d7f6  build-followup-001/core-rust
5b262b1a2f8786793fa494cd6b3c2db9a0ec4919a639a867446b51a34c658b5f  build-followup-001/sources-before-build.sha256
72146fb6459fb262737444579c4b736da6ac05beef3b8bfaaacc53a06147dc69  runtime-attempt-001/preflight.json
55b781f518ef55b6a7acf151d5d622d494daa259a9362c1cea51cad02f714995  runtime-attempt-001/runtime-audit.json
d247261cf29f0a67ab9e76a9eee1eeb07cc0be9e80373fe2a8f8a86f4885abfc  runtime-attempt-001/audit.py
4b6d9313b264a2a9d6c4ee8727080565fc2d99ef4880f47dcb2512501542c81a  validation-followup-001/finite.jsonl
479a1d2404134f660559af6537f075a2004fb03f84ef72584ae6aee80345d7e0  validation-followup-001/summary.json
```

## Tentativi falliti, limiti e primo lavoro incompleto

**Un solo tentativo build+validate, exit 0; un solo audit degli artefatti, exit 0. Nessun tentativo runtime fallito o retry da omettere.** Tutte le diagnosi dei guasti attesi e i journal sono conservati. Nessuna riscrittura o rietichettatura delle serie precedenti.

L'osservabilità riguarda il nostro dispatcher e i figli controllati, non gli interni FFTW/NumPy, loader o thread transitori. L'ordine dei controlli prima del kernel è verificato sul sorgente stabile; non è stato aggiunto un tracer indipendente dell'ingresso nelle librerie. C++ e Rust condividono FFTW. Questa prova non estende la correttezza FFT a spettri generali, non è validazione astronomica M3 e non qualifica dipendenze o supporto Linux multipiattaforma.

Quota e RSS sono guardie best effort, non promessa anti-OOM o garanzia di terminabilità universale. Nessuna misura di prestazioni nuova, adozione di stack o PASS complessivo M1.a/M1 deriva da questi risultati. Ownership, maschere e gli altri gate non sono coperti da questa sessione.

**Primo lavoro incompleto:** review indipendente dei risultati e dell'auditor effettivi, prima di usarli per chiudere i finding runtime. La sessione esclusiva ha terminato tutti i test ammessi; nessun figlio è rimasto in esecuzione. Non rieseguire il wrapper sulle stesse destinazioni write-once: eventuali nuove prove richiedono snapshot, manifest e output nuovi, senza sovrascrivere questo tentativo.
