# M1.e Vulkan: follow-up alla review security

Stato effettivo: **PREPARED_NOT_RUN**. Correzioni implementate e ricompilate;
nessun engine, GPU, pilot, benchmark o test di regressione eseguito. M1 resta
aperta. La review precedente riguarda il testo precedente: non vale come
approvazione indipendente di questo nuovo diff.

Modello dichiarato dal runtime: `openai/gpt-6.1-sol`. Nessuna delega. Letti i
sorgenti effettivi run.py 280 righe, prepare.py 276, engine.cpp 523, README 94
e preregistrazione 156 prima della modifica. Nessun altro namespace modificato.

## Snapshot precedente, write-once

Prima degli edit è stato creato esclusivamente
`tmp/m1/e-vulkan/snapshots/pre-followup-001/`: dodici file con fonti, ricetta,
indice casi, protocollo padre e ricevuta precedente; i quattro vecchi artefatti
binari/shader sono collegati tramite digest. Copie con creazione esclusiva,
fsync, file modo 0444 e directory 0555. È protezione operativa, non hardware WORM.

SHA-256 del manifest snapshot:
`c880264b2e6e654c37e31a1c4d7da9f78461ae2ca53ddc1fb49919aa801e60f8`.
Tutte le copie sono state rihashate dopo gli edit. Verificati invariati anche
preparation-002, vecchia ricevuta M1-e-vulkan-preparation, protocollo M1-spikes
e quattro vecchi artefatti build-002. Nessuna ricevuta storica riscritta.

## Diff effettivo

Diff senza Git: `tmp/m1/e-vulkan/followup-diff.patch`, SHA-256
`124767a24f9db44a611a2bc9f5355417dac56a064024027d012e54d49866b92e`.

- `run.py`, 280 -> 511 righe: owner registra anche costruttori incompleti;
  cleanup indipendente per ogni proprio Popen, gruppi di errori e report
  strutturati; framing parziale/oversize rifiutato. Nessun segnale a PID/gruppi
  esterni. Le ricevute vengono controllate prima di qualsiasi worker di CONFIRM.
- Nuova fase `validate`, senza statistiche comparative. Verifica CPU/GPU contro
  GT e controllo uint end-to-end. FAULTS richiede VALIDATE; CONFIRM richiede
  entrambe le ricevute PASS, compatibili e con artefatti integri. Non avvia
  automaticamente pilot, validazione o backend sostitutivi.
- Binding delle ricevute: ricetta, hash lock CPU, fonti/binario/shader, protocollo
  e preregistrazione, descriptor/originali/GT, identità osservata completa di
  hardware/driver/coda/timestamp. Il nuovo ready deve coincidere prima del run.
  Ogni processo richiesto deve essere raccolto; errori di close impediscono PASS.
- `engine.cpp`, 523 -> 566 righe: ready con identità reale e allocation API;
  contatori incrementati ai memcpy host e vkCmdCopyBuffer effettivi; delta
  getrusage user/system per richiesta, inclusi GT/tracing ed escluso startup.
  Risoluzione nativa microsecondi convertita in ns, non precisione al ns.
- Controllo copie predisposto: 16 uint32 finite, inclusi ±0 e subnormali, layout
  8x2 stride 25 con padding NaN inutilizzato. Input 200 byte, output 64 byte.
  Il futuro gate richiede memcpy host, upload/download registrati, submit/fence
  completate e confronto bitwise. Conta API/payload espliciti, non traffico bus
  fisico o allocatori interni: questi ultimi restano null/NOT_OBSERVABLE.
- `test_followup.py`, 255 righe: 17 regressioni predisposte con fake Popen, per
  constructor/config/register falliti, close multipli, framing parziale/EOF/
  oversize e ricevute mancanti, mutate, parziali o incompatibili. NON ESEGUITE;
  nessuna fixture fake è una prova di funzionamento hardware.
- README e piano aggiornati; prepare.py include anche il nuovo sorgente test
  nel manifest. Shader, pin AMD/UUID/type, rifiuto software, f32/f64/no cast,
  tolleranza zero, fairness workspace-reuse e dodici hash dei tool invariati.

## Verifiche realmente eseguite

Prima: `du -s -B1 tmp`, `df -B1 .`, snapshot stdlib senza engine. Poi parsing
AST dei tre sorgenti Python, senza importarli o eseguire regressioni.

```bash
/usr/bin/python3 -I -B tmp/m1/e-vulkan/prepare.py --build --series 003
```

Exit 0, PREPARED_NOT_RUN. Il builder ha ricontrollato i dodici hash e la catena
locale firma Ubuntu -> indice -> record -> deb -> prefix prima dei payload.
Ha compilato tre shader GLSL, eseguito spirv-val e introspezione, poi g++ host.
Il log host-build è vuoto: nessun warning emesso. Non ha lanciato il binario.

Float32 aritmetico: point 12 operatori, FFT 11; nessun NoContraction mancante.
Copy: zero operatori floating. Digest SPIR-V identici alla build precedente;
nessuna modifica manuale del bytecode o promessa di FloatControls non presenti.

Dopo la build: ricontrollati hash di otto sorgenti, piano, binario, indice casi
e tre shader, più snapshot e vecchi artefatti. AST ancora valido. Con difflib
è stato scritto il nuovo diff esclusivo, senza Git. Questi controlli non
eseguono le funzioni del runner, gli assert di regressione o il codice C++.

| Artefatto finale | SHA-256 |
|---|---|
| `preparation-003.json` | `9b858ee2a5d31fe5b25d0450a1b5d5ed4b49ede613354f541fc77fad76ae34e5` |
| `build-003/engine` | `af2253a5b8cfe6e756094c3235c57c4e0f264d25da7ee9a30a33edb7df0c074d` |
| `run.py` | `f1d6bf74e0937773ee11d5315382d9c660de2df96ed98426c993491ab62c107a` |
| `engine.cpp` | `8cc0ff377575a31f68a4396c3a396ffd9c6d0e38162682ff62d27bcf62bbb1e1` |
| `test_followup.py` | `fabd9dccf2d9d1271a7156c1b5edb56bc0b2ce665e9dffdafa94cfa1acdcece6` |
| `plans/M1-e-vulkan.md` | `f584cc83dab84d1fa35101085834c0601e53d9f20eb997dbf8514e81424715df` |

I path relativi non qualificati nella tabella sono sotto `tmp/m1/e-vulkan/`.
Gli altri hash, argv, versioni e log restano nel nuovo preparation-003.

## Risorse e gate non eseguiti

Scratch globale iniziale: 855748608 byte; preflight build: 856576000;
ultimo du: 857026560, sotto 1073741824 con margine di build 16 MiB.
Namespace e-vulkan: 1560576 byte all'ultimo controllo. Snapshot campionati,
non quota atomica contro crescita esterna. Nessun file altrui rimosso.

Guardie 256 MiB payload, RSS 512 MiB best effort e tmp globale 1 GiB invariate.
La misura RSS dei tool riguarda il Popen osservato, non garantisce il totale
dei discendenti del compiler o OOM del sistema. Nessuna misura di heap/RSS/
CPU time del nuovo engine è stata raccolta; i nuovi campi sono implementati,
non risultati empirici.

Ancora aperti: review indipendente del diff nuovo, tutte le 17 regressioni,
pilot CPU separato e lock, VALIDATE numerica/copie reali, FAULTS, ricevute
compatibili e 21 coppie confermative. Nessuna ricevuta validate/faults reale
è stata prodotta qui. Il riferimento native-only non dimostra la migliore CPU
generale; f64 e gli altri limiti scientifici restano invariati.
