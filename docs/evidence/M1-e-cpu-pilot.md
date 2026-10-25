# M1.e: primo pilot CPU nativo e lock da revisionare

**CPU_PILOT_COMPLETE, exit 0**, soltanto il caso autorizzato
`affine-f32-17-19-s19`, forward, batch 1. Un processo C++ CPU, due warm-up e
sette campioni; nessun altro candidato o tupla eseguito. Lock consegnato per
**review indipendente prima di qualsiasi GPU**. M1 non chiusa.

Autore: `openai/gpt-6.1-sol`, identità dichiarata dal runtime. Nessuna delega;
costi/token non esposti. Nessuna fonte, piano, PREP, ricevuta storica, review,
progress o Git modificati. Scritti soltanto il nuovo output e questo record.

## Ammissione e preflight effettivi

Letta la review supervisor indipendente, 26 righe, PASS limitato a 18/18 mock:
`docs/evidence/M1-e-supervisor-review.md`, SHA-256
`f934ee33751af90050cc93c943332dc04f65bafb46aa22e13d00cfce04a1dd49`.
Verificata la ricevuta supervisor corrente contro il digest
`36da6443f820bdca2fe4f10a88b9f225f33b8eb4af269f54fe1f9168589e146b`,
log e undici hash sorgente/piani, zero failure/error/skip/unraisable.

Verificati a bytehash PREP004
`d19f9d7082f9cb31cb21b0a98d6d1a52044cd3474b6fd42099f383c87d4befe0`,
otto fonti, binario, indice, tre shader, protocollo padre e preregistrazione
`f584cc83dab84d1fa35101085834c0601e53d9f20eb997dbf8514e81424715df`.
Verificati originali/GT e output assente. Nessun import di altri candidati.

Eseguiti `du -s -B1 tmp` e `df -B1 .`. Preflight immediato: 859021312 byte
globali più margine 16777216, sotto 1073741824; disponibili 124894089216 byte.
Payload previsto e osservato nel ready: 3876 byte, sotto 256 MiB. HOME, TMPDIR,
XDG_CACHE_HOME e XDG_CONFIG_HOME puntano a build-004 dentro root/tmp; cache Mesa
disabilitata, Python `-I -B`, OpenSSL config `/dev/null`, OMP_NUM_THREADS=1.
Guardia RSS del worker 512 MiB best effort, non garanzia anti-OOM.

## Unico comando candidato eseguito

Root `/opt/workspace/astroProcessor`:

```bash
/usr/bin/python3 -I -B tmp/m1/e-vulkan/run.py --phase pilot --preparation tmp/m1/e-vulkan/preparation-004.json --case affine-f32-17-19-s19 --direction forward --batch 1 --reference-pool native-only --reviewed-execution yes --output tmp/m1/e-vulkan/pilot-affine-004
```

Avviato una sola volta con argv identico, tramite Popen con ambiente ristretto.
Un osservatore esterno ha catturato stdout/stderr e campionato soltanto il PID
driver e i suoi figli tramite proc. Non modifica codice, richiesta o precisione.
Stdout effettivo: `{"status": "CPU_PILOT_COMPLETE", "phase": "pilot"}`.
Stderr driver vuoto. Exit driver e worker: entrambi 0; nessun retry del pilot.

Output sotto `tmp/m1/e-vulkan/pilot-affine-004/`: pilot/report, reference-lock,
trace CPU, stdout/stderr driver, execution-observer e verifica finale. File
creati esclusivamente e poi resi read-only; nessun risultato precedente sostituito.

## Caso, oracle e profilo

17 righe x 19 colonne, stride 19 contiguo, 323 pixel reali float32 little-endian.
SYN-DYADIC `x[i]=(((73*i+19)%1024)-512)/1024`; affine `3*x/4+1/8`.
GT indipendente già preregistrato: numeratori int64, denominatore 4096,
equivalente `(3*u+512)/4096`, non output di un candidato. Input 1292 byte,
GT originale 2584 byte; confronto numerico esatto con tolleranza zero, nessuna
riduzione o epsilon. I sette output registrati risultano tutti verificati.

- Input SHA: `f83e783fdfa1db8112564ca7c8660f24d6e1a1b391e32bd528b8b6c19b928641`.
- GT SHA: `a513bfacbd97f5bd1fecc07f95816ca57e9347efe64cde5931d947b10e05f55b`.
- Descriptor SHA: `c1bd41b73d1a2968f13bb077ab71a40082b55882d6c3f4c4d6ecb84dccaadeff`.
- Binario SHA: `af2253a5b8cfe6e756094c3235c57c4e0f264d25da7ee9a30a33edb7df0c074d`.

Reference selezionata: **cpp-direct**, profilo workspace-reuse. La famiglia
binaria include FFTW, ma questo caso affine non crea/esegue un piano FFTW.
Input, output e GT host riusati; nessuna immagine GPU residente. Il pool
native-only rende il riferimento condizionato: non migliore CPU generale o
scelta stack. Nessun CI comparativo o vincitore derivato da questi sette dati.

## Startup, warm-up e campioni realmente prodotti

Un solo startup: load 50674 ns, setup CPU 9467 ns, startup esterno del worker
22157286 ns. Quest'ultimo comprende anche preflight/avvio/lettura ready, non
soltanto inizializzazione del binario. Processo worker totale 189005740 ns;
driver totale osservato 272892513 ns. Nessuna cache OS/storage svuotata.

La traccia contiene nove ingressi CPU consecutivi, ordinali 1..9. Fonte
immutabile: due richieste warm-up seguite dai sette round. Ogni richiesta deve
verificare GT prima di restituire. **Le metriche raw dei due warm-up non sono
conservate dal runner**: non vengono inventate o riusate come campioni.

Tutti i valori sotto sono ns, provenienti dal nuovo pilot:

| r | T_total | kernel | verifica GT | CPU user | CPU system |
|---|---:|---:|---:|---:|---:|
| 1 | 50544 | 1032 | 17592 | 73000 | 0 |
| 2 | 39644 | 871 | 18094 | 62000 | 0 |
| 3 | 31017 | 581 | 10299 | 44000 | 0 |
| 4 | 31779 | 591 | 10199 | 45000 | 0 |
| 5 | 28853 | 591 | 10489 | 42000 | 0 |
| 6 | 39623 | 962 | 18194 | 61000 | 0 |
| 7 | 38922 | 872 | 17692 | 61000 | 0 |

T_total include scan/contratto, kernel e tracing del prototipo; verifica GT
separata. CPU user/system sono getrusage per richiesta, inclusa verifica/tracing,
escluso startup, risoluzione microsecondi convertita in ns. Zero system non
significa assenza universale di lavoro OS. Tempi esclusivamente esplorativi.

## PID, RSS, copie e assenza GPU osservabile

Driver PID **157547** raccolto dal Popen esterno, exit 0. Unico engine osservato
PID **157557**, figlio del driver, argv `--backend cpu --dtype f32 --op affine`,
shape/stride/batch corretti, fault none. La ricevuta del driver registra un solo
worker CPU `reaped=true`, exit 0, cleanup_errors vuoto. Essa non contiene il
campo PID: l'associazione usa l'unico comando nativo osservato, non un audit
waitpid completo. Figli metadata du brevi possono sfuggire al campionamento;
nessun segnale è stato inviato a processi estranei.

Thread osservati: 1. Picco RSS: driver 25645056 byte, worker campionato 5419008,
worker getrusage 16384000. Metodi/finestre differenti, tutti sotto guardia;
non sono un limite RSS aggregato o prova anti-OOM. Ultimo du prima del record:
859095040 byte, sotto 1 GiB.

Ready: device null, allocation_api_calls 0, vulkan_allocation_bytes 0. Tutti i
campioni: una chiamata CPU, zero dispatch/submit/fence/copied bytes Vulkan,
gpu_queries_applicable=false. Le query raw zero sono placeholder non applicabili,
non timestamp GPU misurati. Traccia soltanto cpu_kernel_entry, nessuna voce GPU.
Il pin AMD nel lock è configurazione futura, non nuova osservazione hardware.

Memcpy/packing espliciti zero per questa affine diretta; non significa zero
letture/scritture del kernel. Output buffer e digest dell'output non vengono
persistiti: GT comparison è verificata in-process. Allocatori interni restano
non osservabili. Nessuna attestazione degli interni di librerie/driver linkati,
né telemetria GPU globale; non è qualificazione hardware.

## Integrità finale e incidente del solo controllo metadata

Subito dopo il comando: tutti i 185 digest preesistenti monitorati invariati,
nessun nuovo file fuori dall'output. Un controllo metadata successivo, troppo
ampio, ha avuto exit 1 perché `docs/evidence/README.md` era cambiato nel frattempo.
Non è una fonte della ricetta; questo agente non lo ha scritto. Il traceback
e il fallimento del system exception hook sul nome stdin `-` sono conservati
in metadata-verification-001-error.txt; late-metadata-diff.json conserva i due
digest dell'indice. Nessun errore del pilot cancellato e nessun pilot ripetuto.

Il successivo controllo metadata ha verificato separatamente tutte le fonti e
binding contrattuali: otto sorgenti, PREP004, engine/index/shader, protocollo/
prereg, originali/GT e review supervisor invariati. Non è una nuova esecuzione
numerica e non modifica il metodo. Verifica completa in verification.json.

## Lock stabile da sottoporre a review indipendente

- `reference-lock.json`: `b479a22a75af76b5e494f23c1af1d0ccdfc19ffe36d1eb6ef043a51343a26e79`.
- `pilot.json` e `report.json`: `72085f7b6484870b0f743ca398b3725c6f29c042a8b099c145aa2e4f1e3830fd`.
- `cpu.trace.jsonl`: `80fdb074008aa4ae2e2ab42f79914cbe28dedbaa602cc5f56731a83e8c79274f`.
- `execution-observer.json`: `3b096d487e7b286ed082165897a07a3cc84ce5194f26882b3673dc7828705d2a`.
- `verification.json`: `77b070c57abec320cbaf7562c4552825ca8550e481188edaaa65793846a97fc4`.

Il lock lega caso/direzione/batch/dtype/profile, primaria CPU, otto fonti,
binario/shader, protocollo/preregistrazione, descriptor/input/GT e hash del
pilot. `pilot_only=true`, `gpu_data_collected=false`. I dati non sono campioni
confermativi. **Primo incompleto: review indipendente di questo lock.** Nessuna
GPU validate/fault/confirm o altra attività avviata automaticamente.
