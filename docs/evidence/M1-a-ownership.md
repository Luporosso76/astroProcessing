# M1.a: ownership C ABI e maschere

Esito: **PASS limitato alla validazione descritta**, con review indipendente ancora da svolgere. Nessuna chiusura automatica di M1.a o M1. Nessun benchmark comparativo, installazione, rete, operazione Git, modifica del prodotto o decisione di licenza.

Autore: GPT-6.1 Sol, OpenAI, dichiarato dal runtime, non attestato. Nessun subagente; costi/token non esposti. Letti AGENTS 52 righe e progress 37; riutilizzato il contesto operativo già letto, senza rileggere il piano di 4.5k righe. Scritture solo nel nuovo namespace `tmp/m1/ownership/` e in questo nuovo record. Nessuna modifica ai sorgenti/fixture/serie precedenti o ai report M1-a/M1-e sotto review.

## Contratto fissato prima delle prove

Fonte: `tmp/m1/ownership/CONTRACT.md`, SHA-256 `9119223c9d55771d02806a2a05a7682ad5c3d1ea3b2095010531ec26ddd41b8c`. Ogni tentativo conserva contratto e sorgenti in uno snapshot ZIP con manifest precedente alla build.

- Linux x86_64 little-endian; dati float32 IEEE, maschera uint16, API/schema 1. Nessun cambio automatico di dtype, endian, versione o backend.
- Un owner Rust possiede due payload stabili, dati e maschera. Handle `(slot,generation)`, lease esclusivo identificato e revision verificati prima degli accessi. Slot riusato incrementa generation, senza wrap.
- Owner release con lease attivo: **BUSY**, nessuna free. End lease una volta; seconda chiusura **LEASE**. Owner destroy una volta; seconda distruzione **STALE**. I test di metadati obsoleti non dereferenziano memoria liberata.
- Binding Python: basi ctypes delle ndarray trattengono il guard del lease e l'owner, anche attraverso slice. Ultima base distrutta chiude il lease; se resta soltanto quel riferimento, poi distrugge l'owner.
- Single-thread: nessun accesso NumPy contemporaneo al consumer nativo. Il C ABI non può revocare pointer arbitrari né contare alias costruiti fuori dal binding. Un chiamante C che chiude manualmente il lease mentre usa ancora il pointer viola il contratto: non si dichiara sicurezza universale contro abusi C/NumPy.

Layout del caso: immagine logica **3×5**, backing **3×8**, stride dati **32 byte**, stride mask **16 byte**. Pointer, owner, generation, lease, revision e stride sono tracciati in `events.jsonl`. Dimensioni ABI verificate fra header C++, Rust repr(C) e ctypes: descriptor **80**, view **80**, mask **48**, stats **112 byte**.

## Semantica verificata, non soltanto pointer

Bits: 1 INVALID, 2 SATURATED, 4 USER_EXCLUDED; zero valido. Ogni combinazione conosciuta 1..7 esclude il pixel; bit ignoto in un pixel logico ferma tutta la chiamata. Padding fuori dal dominio e preservato, anche con sentinel mask `0xFFFF`.

Operatore del prototipo: `y=2*x+1` soltanto sui validi. Negativi ammessi; validi NaN o con risultato non finito rifiutati prima di scrivere. Esclusi invariati bitwise, inclusi NaN e -0. Ogni commit incrementa insieme revision dati/mask. Un edit NumPy è staging sotto lease e richiede commit esplicito; non si promette rollback di scritture raw illecite del chiamante.

Sequenza reale: C++ modifica il payload Rust; Rust vede quelle modifiche; NumPy modifica lo stesso payload; C++ legge i valori modificati da NumPy. GT Fraction su fixture dyadic e confronto bitwise float32, tolleranza **zero**. Sono verificati anche dati esclusi e padding invariati.

## Comandi realmente eseguiti

Dalla radice `/opt/workspace/astroProcessor`:

```bash
source tmp/m1/ownership/env.sh
/usr/bin/python3 -B tmp/m1/ownership/run_all.py --attempt 001
/usr/bin/python3 -B tmp/m1/ownership/run_all.py --attempt 002
/usr/bin/python3 -B tmp/m1/ownership/run_all.py --attempt 003
```

L'ambiente sposta cache e temporanei configurabili nel namespace ownership, imposta i thread dichiarati a uno e risolve `RUST_TOOLCHAIN_DIR` senza pubblicare percorsi personali o usare rustup/Cargo. Il runner compila Rust cdylib O2, target x86-64, panic abort, e C++ shared O2 con no-fast-math/no-FMA contraction; esegue poi la validazione ctypes/NumPy. Comandi completi, copiabili con questi parametri, in `attempt-003/{build-rust,build-cpp,validation}.json`.

Versioni effettive: **rustc 1.99.0** (`b940084d7`, 2026-09-28), **G++ 15.2.0**, **Python 3.14.4**, **NumPy 2.3.5**. Sorgenti: `src/owner.rs`, `src/consumer.cpp`, `src/bridge.h`, `test_bridge.py`, `run_all.py`, `env.sh`. Nessuna dipendenza scaricata.

I tentativi sono write-once: non rilanciare gli stessi ID sovrascrivendo risultati. Una replica usa un ID nuovo e richiede prima headroom verificato; non cancella le serie in review.

## Esiti e fallimenti conservati

| Tentativo | Rust build | C++ build | Validazione | Esito |
|---|---:|---:|---:|---|
| 001 | 0 | 1 | NON ESEGUITA | Errore const nel cast C++, conservato |
| 002 | 0 | 0 | 0 | 69 eventi con asserzioni PASS |
| 003 | 0 | 0 | 0 | 75 eventi con asserzioni PASS |

Errore 001: `casts away qualifiers` nel cast del pointer mask read-only. Corretto il tipo destinazione in `const uint16_t*`, senza cambiare contratto o tolleranza. Log e snapshot del sorgente fallito preservati. Il tentativo 003 aggiunge errori in pixel tardivi della scansione, per verificare che non vengano pubblicate modifiche ai validi precedenti, e rende esplicito il contatore finale C++.

Validazioni eseguite:

- Overflow di width/height, stride/shape non validi, payload oltre budget; creazione rifiutata prima dell'allocazione e out-handle sentinel invariato.
- Handle nullo/inesistente, generation falsa o realmente obsoleta dopo riuso dello slot, lease scaduto o sostituito, borrow con revision obsoleta; out-view in errore invariata.
- Mask shape/stride/dtype/endian/version/known bits/revision errati; vista NumPy di tipo, shape o endian sbagliato rifiutata senza conversione.
- Bit pixel ignoto, NaN valido e overflow affine sia all'inizio sia più avanti nella scansione: dati, mask e revision invariati. Errori restituiti come status C ABI nonzero e diagnostica simbolica nel log; il processo di test termina zero solo perché erano esiti attesi.
- Pointer contraffatto rifiutato prima di dereferenziare il payload. Su ogni caso negativo eseguito contro i due consumer come casi separati: contatori dei kernel invariati, **zero kernel numerici alternativi**. La validazione comune Rust dell'owner non è un algoritmo numerico di ripiego.
- Double end/destroy controllati; secondo borrow e owner destroy con viste vive rifiutati. Slice sola mantiene il lease; distruzione delle ultime basi libera una volta. Il weakref dell'owner Python conferma che le basi lo mantengono vivo e poi lo rilasciano.

Contatori finali reali: **3 owner creati/3 distrutti**, **3 lease aperti/3 chiusi**, **6 payload pubblicati/6 liberati**, **432 byte richiesti e di capacità registrata/432 liberati**, **zero owner e lease attivi**. Kernel riusciti: C++ **2**, Rust **1**; un edit NumPy esplicito, nessun percorso sostitutivo su errore. `rejected_calls=52` è il contatore interno delle chiamate che attraversano il registry, non il numero totale di test negativi.

## Copie, allocazioni e risorse

Per il ponte dati/mask, ndarray non owning, pointer uguali a quelli Rust e `shares_memory` sui relativi alias. Le modifiche sono causalmente visibili dai tre linguaggi: **zero copie payload nel ponte dimostrato**, non zero allocazioni dell'intera applicazione.

Allocazioni/copie controllate dal test: tre coppie di Vec Rust, 144 byte per owner; capacità letta realmente dal container e bilanciata alla distruzione. Snapshot e array GT copiano **2976 byte** complessivi; fixture NumPy volutamente di tipo errato alloca altri **15 byte**. Ledger per operazione in `results/summary.json`. Allocazioni di runtime, loader, allocator, ctypes/ndarray metadata e temporanei interni dei confronti non sono inventariate: nessuna contabilità completa delle malloc rivendicata.

Preflight iniziale: namespace/report assenti, tmp **959434752 byte**, riserva aggiuntiva **16 MiB**. Il runner ricontrolla tutto `tmp/` prima di ogni build/validazione e richiede altri 8 MiB liberi nel budget. Alla fine del tentativo 003: tmp **1015595008 byte**, namespace ownership **1269760 byte**, entrambi entro i limiti. I dati/serie precedenti non sono stati cancellati o spostati. Il totale tmp può cambiare per le preparazioni degli altri autori e non è attribuito tutto a questo test.

RSS osservato del figlio Rust build **121848 KiB**, del processo Python con le librerie native **39244 KiB**. Monitor best effort dei PID direttamente creati: il valore del driver G++ non comprende automaticamente cc1plus, né garantisce di cogliere picchi tra campioni. Nessuna garanzia universale anti-OOM. Massimo payload per owner del contratto 1 MiB, prove da 144 byte; nessun benchmark o misura comparativa CPU eseguito.

## Artefatti stabili e residui

`attempt-003/`: `sources-before-build.json`, `source-snapshot.zip`, `frozen-inputs-before.json`, `preflight.json`, `versions.json`, comandi/exit/stdout/stderr delle tre fasi, hash dei binari prima dei test, `results/{events.jsonl,summary.json}`, `final.json`. Tentativi 001/002 conservati con i loro sorgenti e risultati, senza rietichettatura.

I dodici hash congelati, inclusi i report M1-a/M1-e, il protocollo, il manifest della serie-002 e i suoi sorgenti/binari, sono invariati. Questa verifica non riscrive il vecchio report e non attribuisce alla serie-002 questi nuovi test.

Verifica finale eseguita, exit **0**: `source tmp/m1/ownership/env.sh; /usr/bin/python3 -B tmp/m1/ownership/verify.py`. Ricontrolla tre snapshot, sorgenti correnti, dodici hash congelati, contatori, copie dichiarate, sintassi Python/bash del nuovo record e budget totale. Ricevuta con hash di questo report in `tmp/m1/ownership/handoff.json`; nessun kernel o benchmark viene rieseguito da quel comando. Il totale tmp all'ultima lettura era **815247360 byte**, distinto dallo snapshot di fine tentativo 003; nessuna cancellazione o spostamento di dati è stata eseguita da questo agente.

Prossimo passo: review indipendente del nuovo contratto, unsafe Rust, binding ctypes e risultati. Restano non provati concorrenza, chiamanti C non conformi, sanitizer/Miri di questo nuovo ponte e semantica delle maschere degli operatori M3. Restano separati l'inventario generale delle copie e il controllo dell'ambiente floating-point della serie numerica. La scelta stack e M1.b/c/d/e/f non si chiudono con questo PASS locale. Nessuna nuova pipeline GPU o installazione shader avviata.
