# M1.e: preflight GPU reale

Unità: M1.e. Scopo: verificare API, dispositivo e qualificabilità del percorso hardware prima dei confronti CPU. Esito: **PARZIALE, preflight completato; benchmark compute NON ESEGUITO**. Nessuno speedup o backend di prodotto scelto.

Autore: GPT-6.1 Sol, OpenAI, identità dichiarata dal runtime. Nessun subagente. Costi e token non esposti. Accessi: AGENTS 52 righe, progress 35, chiusura M0 64, review operativa 23, protocollo iniziale 169; rettifica protocollo righe 40-84 e review 18 righe. Non è stata riletta integralmente la 2.2.1.

## Comandi eseguiti

Dalla radice `/opt/workspace/astroProcessor`:

```bash
source tmp/m1/env.sh
/usr/bin/python3 -B tmp/m1/src/gpu_preflight.py --backend vulkan
/usr/bin/python3 -B tmp/m1/src/gpu_preflight.py --backend opengl
```

Il primo comando applica le variabili preregistrate e colloca temporanei/cache configurabili sotto `tmp/`. Il wrapper esegue letteralmente `vulkaninfo --summary` oppure `glxinfo -B`, mai un kernel compute. I due comandi sono casi espliciti distinti, non un retry dopo guasto. Entrambi i tool e i wrapper hanno restituito **0**. Il codice zero significa probe riuscito, non qualificazione del backend.

Output e manifest: `tmp/m1/e/{vulkan,opengl}/{stdout.txt,stderr.txt,capabilities.json,manifest.json}`. Sono conservati stdout/stderr sanitizzati: valori display omessi e prefissi home personali sostituiti; nessuna immagine o pixel dell'utente raccolto. Il manifest registra gli hash di script, ambiente, protocollo e output. SHA dello script: `39e0fa6db01e68f3f9d32870f46e4712779f0e550db04897e88edca4dc576cc3`.

Questi probe precedono la rettifica statistica/FFT. Il loro protocollo registrato resta `93468f1ceb4afcd735a822a3b87589e963917260ce6d87cfdf0269c6721cc9ff`: non sono rietichettati con il nuovo hash. Le osservazioni di capacità non sono campioni confermativi o prestazioni. Il protocollo stabile successivo è `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.

## Risultati osservati

- Vulkan instance **1.4.341**; GPU integrata AMD **0x1002:0x1681**, `AMD Radeon Vega Mobile Gfx (RADV REMBRANDT)`, API dispositivo **1.4.335**, driver `radv`, Mesa **26.0.8-1ubuntu0.3**.
- La stessa enumerazione presenta anche `PHYSICAL_DEVICE_TYPE_CPU`, llvmpipe/LLVM **21.1.8**. È un dispositivo software distinto, non qualificato come GPU hardware. Non è stato selezionato né usato per calcolo.
- OpenGL: `direct rendering: Yes`, `Accelerated: yes`, stesso vendor/device AMD, renderer radeonsi/rembrandt, core **4.6**, GLSL **4.60**. `glxinfo` riporta memoria video **512 MB**, unificata; i valori liberi sono una fotografia, non un budget o minimo di prodotto.
- `glslc`, `glslangValidator`, `spirv-val` assenti al probe effettivo dello script. Vulkan compute non compilato; nessuno SPIR-V scritto a mano o recuperato dalla rete.
- OpenCL rimane escluso per header/ICD mancanti rilevati in M0; non è stato eseguito un nuovo probe OpenCL in questa unità.

## Verdetto limitato

Esiste hardware con API compute-capable; questo non prova un nostro percorso compute utilizzabile. Il probe OpenGL descrive il contesto creato da glxinfo, ma non implementa selezione vincolante dell'adattatore e rifiuto del renderer software nel futuro kernel. Non dimostra che ogni fallback del loader/driver possa essere impedito. **OpenGL NON QUALIFICATO per il benchmark hardware in questa serie**, senza affermare un'impossibilità definitiva.

Vulkan resta bloccato dai tool shader mancanti. Non si sostituisce OpenCL, OpenGL o CPU al percorso Vulkan. Il wrapper conta zero invocazioni di backend alternativi; non è una prova T-FBK-01 del prodotto o di tutti gli interni driver.

NON ESEGUITI: allocazioni compute, copie host/device, kernel, sincronizzazione, test errore shader/submission, distacco GPU, misure VRAM, confronto di speedup o intervallo GPU/CPU. Nessuna CPU di riferimento GPU selezionata sui campioni confermativi: GPU non misurata, quindi nessun CI GPU inventato.

## Ripresa

M1.a CPU prosegue indipendentemente, con record [M1-a](M1-a.md). Il ramo GPU richiede un percorso compilabile e un preflight del prototipo che imponga adattatore e arresto senza software/altro backend; poi error injection, pilot CPU separato e misura confermativa secondo protocollo stabile. Nessuna installazione, rete, modifica Git o di sistema eseguita.
