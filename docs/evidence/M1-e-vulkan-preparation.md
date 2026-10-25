# M1.e Vulkan: ricevuta della preparazione

Esito effettivo: **PREPARED_NOT_RUN**. Autenticazione locale, compilazione host/
shader e validazione statica riuscite. Il binario candidato non è stato
eseguito, nemmeno per un preflight. Nessun dispatch, pilot CPU, fault test,
campione numerico GPU, confronto prestazionale o compute PASS.

Autore: `openai/gpt-6.1-sol`, identità dichiarata dal runtime. Nessuna delega;
costi e token non esposti. Letti integralmente AGENTS 52 righe, progress 40,
M1-spikes 169, M1-e 41, M1-e-tools 88, manifest tool 55 e PLANS 46. Letti anche
record di acquisizione/firma/versioni, metadati Vulkan preesistenti e porzioni
dei manifest/oracle/timing della serie CPU stabile 002. Non riletto il piano
scientifico lungo. Nessun sorgente attivo ownership/a-fault/c/d letto o cambiato.

## Comandi realmente eseguiti

Dalla radice del progetto:

```bash
du -s -B1 tmp
df -B1 .
command -v g++
command -v gpgv
command -v dpkg-deb
stat -c '%s %n' tmp/m1/a/series-001/fixtures/fft-impulse-f32-32-32-s32.input.bin tmp/m1/a/series-001/fixtures/fft-impulse-f32-32-32-s32.gt.bin tmp/m1/a/series-001/fixtures/warp-f32-17-19-s19.gt-int64.bin
/usr/bin/python3 -I -B tmp/m1/e-vulkan/prepare.py --build --series 001
/usr/bin/python3 -I -B tmp/m1/e-vulkan/prepare.py --build --series 002
```

Le due preparazioni sono riuscite, exit 0. La prima resta conservata come
revisione precedente. La 002 compila il sorgente finale dopo aggiunta del pin
dell'indice casi e dei controlli di ampiezza; non reinterpreta risultati GPU,
che non esistono. I manifest immutabili degli originali non sono stati modificati.
Lo stat ha confermato il formato reale: FFT forward originale real-only,
4096 byte per 32² f32; GT analitico complesso forward+inverse, 16384 byte.
Il packing complesso esplicito resta incluso nel futuro T_total di CPU e GPU.

`prepare.py` registra argv, exit, log/hash e preflight per ogni comando. Sono
stati effettivamente eseguiti gpgv con keyring pubblico esplicito e HOME/GNUPGHOME
locali; dpkg-deb in sola modalità stream; glslangValidator e spirv-val tre volte
per serie; g++ per linkare il candidato. La 002 registra anche `g++ --version`:
**Ubuntu 15.2.0-16ubuntu1, GCC 15.2.0**. Build flags:
`-std=c++20 -O2 -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off -pthread`;
link `-lvulkan -lfftw3f`. Entrambi i log host-build sono vuoti: nessun warning
emesso. Il binario linkato non è stato avviato. Nessun maintainer script,
installazione, rete, indice, comando Git, GUI o scrittura fuori dai nuovi
namespace autorizzati.

## Autenticità ricontrollata prima dei payload

Manifest tool SHA-256 confermato:
`7a3723f5ff1f37cdce923c6d390af220a34c672adb3fc817e81aa46b975cdb7f`.
Tutte le dodici voci hanno size e digest corrispondenti. Verificati nuovamente
InRelease tramite gpgv, signer
`F6ECB3762474EDA9D21B7022871920D1991BC93C`, Packages.xz contro la sezione SHA256
firmata, record dei due pacchetti contro indice decompresso in streaming, deb
contro record e due eseguibili prefix contro i membri dei deb. Il symlink
glslangValidator risolve nel prefisso al glslang autenticato. Anche i digest
dei precedenti record runtime coincidono con i file attuali.

Trust anchor: solo `/usr/share/keyrings/ubuntu-archive-keyring.gpg`, con hash
previsto. Nessuna chiave segreta o configurazione personale letta. Restano i
limiti già documentati: provisioning originario del keyring non auditato,
nessuna garanzia di freshness/replay della release, nessuna prova di build
riproducibile o autorizzazione al bundling dei tool. Uso locale secondo il
record M1-e-tools; nessuna nuova licenza del prodotto.

## Artefatti finali e controlli statici eseguiti

Preregistrazione nuova: `plans/M1-e-vulkan.md`, SHA-256
`e1c7098258f755c803e92ced832072e89155a8c85258c2ada4ff7cbbc3f63259`.
Il protocollo padre M1-spikes è stato verificato contro
`1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.
Questo non è l'hash del piano scientifico lungo.

| Artefatto finale | SHA-256 |
|---|---|
| `tmp/m1/e-vulkan/preparation-002.json` | `223d160786ef4678f9e2d8ef00208eb7c6ee877e79c64ea3ae6c84a51689f32e` |
| `build-002/engine` | `ce9582d113abea55fe6c8951501e6bd0dfa50e8ddb07bc12caeab0463e272e59` |
| `build-002/point.spv` | `a9c545dec72431a3f7bd7486b577d2ffb1333f373f1df5bf76c71da0dc65bc9b` |
| `build-002/fft.spv` | `b722bc80c7bf22c50b8486126035ce11afbb81776de15e735ffb1bb3b712320c` |
| `build-002/copy.spv` | `97b3cf8d42bf51b78f619bd68d90c3f6506cb092e75d81611ab15f3c2cdc990c` |

Sorgenti: engine.cpp 523 righe, prepare.py 276, run.py 280; shader point 38,
FFT 46, copy 12; README 94. I sette hash dei sorgenti stanno nel report finale.
L'indice locale congela **42 descriptor** estratti dai casi stabili, non 42 test
eseguiti. I byte delle fixture saranno ricontrollati dal driver prima del pilot
e delle esecuzioni GPU; non sono stati tutti letti/hashati in questa preparazione.

SPIR-V target Vulkan 1.2: tutti e tre i moduli validati, exit 0. Introspezione:
point 12 operatori floating, FFT 11; Float32 presente, Float64 assente, nessun
operatore aritmetico auditato privo di NoContraction. Copy: zero tipi/operatori
floating. Execution mode osservato: LocalSize 256x1x1. Non presenti execution
modes FloatControls: non se ne dichiara l'attivazione. Queste proprietà statiche
non provano correttezza numerica o comportamento del dispositivo.

Eseguito parsing AST di prepare.py/run.py senza importarli durante quel
controllo. Con uno script Python stdlib sono stati ricontrollati tutti e sette
i digest sorgente, preregistrazione, binario, indice casi e tre shader contro
preparation-002: assert tutti soddisfatti. run.py non è mai stato eseguito.

## Risorse osservate e limiti aperti

Primo du della sessione: 1015828480 byte globali. Preflight serie 002:
802480128 byte; ultimo du: **803012608 byte**, sotto 1073741824. Namespace nuovo:
851968 byte. La manutenzione concorrente autorizzata dal coordinatore ha
cambiato il totale: questo agente non ha rimosso o reso sparse file altrui.
Ogni comando di build ha ricontrollato margine 16 MiB e spazio disponibile.
Sono snapshot, non una quota atomica contro crescita esterna.

RSS campionata e address-space limit sono riportati per i processi tool. La
RSS osservata di g++ riguarda il launcher Popen, **non l'intero albero cc1plus/
assembler/linker**; non sostiene una misura aggregata del compilatore. Gli
address-space limit vengono ereditati, ma non garantiscono RSS/OOM aggregati.
Nessuna memoria GPU o heap del nuovo processo candidato è stata misurata.

Restano NON ESEGUITI: preflight hardware del nuovo binario, tutti i kernel e
confronti GT, subnormal/±0 copy-control, guasti, pilot separato e lock CPU,
tre cluster/21 coppie e bootstrap. f64 resta rifiutato senza cast fino al
contratto validato. Il driver preparato supporta riferimenti **native-only**:
non sceglie la migliore CPU generale; occorrono pilot matched/adattatori più
ampi per quel gate. Il generatore/runner copy-control non è ancora implementato.
Gli interni del driver Vulkan non osservabili non diventano contatori zero.

**Primo gate incompleto: review indipendente dei sorgenti finali.** Poi, soltanto
con il via del coordinatore, pilot CPU separato e congelamento del riferimento
prima di qualsiasi numerica GPU. Nessun risultato di questa ricevuta chiude
il gate compute o il confronto M1.e.
