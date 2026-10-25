# M1.e: prima VALIDATE GPU, caso congelato

**PASS VALIDATE PRIMARY soltanto**, exit 0: affine CPU/GPU contro GT esatto e
controllo copie uint32 end-to-end riusciti. Una sola invocazione della fase
validate, nessun retry, FAULTS, CONFIRM, altra tupla scientifica, 21 campioni,
CI o benchmark comparativo. **Review indipendente necessaria prima di procedere.**
M1 resta aperta; native-only non significa migliore CPU generale o scelta stack.

Autore: `openai/gpt-6.1-sol`, identità dichiarata dal runtime. Nessuna delega.
Ammissione esclusiva e review CPU/lock ricevute esplicitamente dal coordinatore;
non è stato atteso o fabbricato un ulteriore verbale. Nessuna fonte, lock,
PREP, piano, indice, progress, output storico o Git modificati. Soltanto nuovo
output validate-affine-004 e questo record.

## Preflight e comando effettivo

Ricontrollati a bytehash prima dell'avvio: PREP004
`d19f9d7082f9cb31cb21b0a98d6d1a52044cd3474b6fd42099f383c87d4befe0`,
lock CPU `b479a22a75af76b5e494f23c1af1d0ccdfc19ffe36d1eb6ef043a51343a26e79`,
engine `af2253a5b8cfe6e756094c3235c57c4e0f264d25da7ee9a30a33edb7df0c074d`,
otto fonti, tre shader, case index/manifest, originali/GT, pilot, protocollo
padre e preregistrazione. Verificato supervisor corrente 18/18 PASS e relativi
hash/log. Output e nuovo record erano assenti: nessun overwrite o autorename.

Protocollo padre SHA `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`;
prereg SHA `f584cc83dab84d1fa35101085834c0601e53d9f20eb997dbf8514e81424715df`.
I documenti mutabili docs/progress.md e docs/evidence/README.md sono esclusi
correttamente dai binding numerici. Nessuna mutazione delle fonti vere ignorata.

Eseguiti du/df tecnici. Preflight immediato: tmp globale 859303936 byte,
margine 16777216, disponibili 124893736960; cap 1073741824 rispettato. Payload
primario pianificato 9044 byte, copy-control 1128; l'engine verifica anche i
VkMemoryRequirements reali prima di allocare, senza ridurre dati o qualità.

Root `/opt/workspace/astroProcessor`, unico comando candidato:

```bash
/usr/bin/python3 -I -B tmp/m1/e-vulkan/run.py --phase validate --preparation tmp/m1/e-vulkan/preparation-004.json --case affine-f32-17-19-s19 --direction forward --batch 1 --reference-lock tmp/m1/e-vulkan/pilot-affine-004/reference-lock.json --reference-pool native-only --reviewed-execution yes --output tmp/m1/e-vulkan/validate-affine-004
```

Argv identico avviato una volta da Popen. HOME/TMP/XDG/cache sotto build-004
in root/tmp, Mesa disk cache disabilitata, OMP=1, OpenSSL config /dev/null,
Python -I -B. Per confinare eventuali diagnostiche del system exception hook,
APPORT_REPORT_DIR punta al nuovo output/diagnostics; nessuna diagnostica è stata
necessaria. Il controllo tecnico esterno osserva solo il proprio driver e i
suoi figli; non modifica algoritmo, precisione, richieste o codice.

Stdout driver: `{"status": "VALIDATION_COMPLETE", "phase": "validate"}`.
Stderr driver vuoto. Nessun errore runtime o primo FAIL da ritentare.

## Identità e versioni realmente osservate

- AMD vendor `0x1002`, device `0x1681`, INTEGRATED_GPU, nome
  `AMD Radeon Vega Mobile Gfx (RADV REMBRANDT)`.
- Device UUID `00000000-e500-0000-0000-000000000000`; driver UUID hex
  `414d442d4d4553412d44525600000000`.
- API dispositivo 1.4.335, raw 4211023; driver ID 3, `radv`,
  `Mesa 26.0.8-1ubuntu0.3`, driverVersion raw 109051912.
- Queue family 0, flags 15 comprendenti compute, queueCount 1;
  timestampValidBits 64, timestampPeriod 10 ns, raw f32 bits 1092616192.
- llvmpipe LLVM 21.1.8 è enumerato come CPU, selected_match=false. Un solo
  match AMD per contesto; nessun device software selezionato.
- RTE32, denorm-preserve32, signed-zero/Inf/NaN-preserve32 e shaderFloat64
  risultano supportati nelle proprietà. Non significa FloatControls execution
  modes attivati o f64 validato: ricetta eseguita esclusivamente f32.

Identità completa uguale fra affine e copy-control, riportata nei ready e
nella ricevuta. Python osservato adesso: 3.14.4, build Aug 20 2026, GCC 15.2.0.
Il loader Vulkan non è stato versionato nuovamente; compiler/shader tool non
eseguiti in questo turno. La provenienza rimane PREP004 immutabile, non versioni
o tempi compiler rietichettati come misure correnti.

## Risultato primario e percorso reale

Caso 17x19, stride 19, 323 pixel reali float32 little-endian, affine
`3*x/4+1/8`. GT int64 con denominatore 4096, input/GT immutabili già preregistrati.
CPU cpp-direct e GPU sono i due percorsi primari esplicitamente selezionati,
non sostituzioni dopo errore. Entrambi: output_compared=true, exact-numeric,
tolleranza zero. Nessuna modifica a metodo, precisione o epsilon.

CPU: una chiamata kernel, zero dispatch/submit GPU. GPU: zero chiamate al
backend CPU, un dispatch registrato (due gruppi x256), una submit accettata,
una fence completata; copie upload/download 1292 byte ciascuna, un record per
direzione. Memcpy host upload/download 1292 byte ciascuno, packing zero.
Traccia reale: host upload -> copy buffer -> dispatch -> copy download ->
submit accettata -> fence completata -> query -> host download -> verifica GT.

Le barrier sono presenti nel percorso di command recording della fonte hashata
con vkCmdPipelineBarrier; **non esiste un evento runtime separato per ogni
barrier**. Il risultato sincronizzato corretto non sostituisce un audit generale
di tutte le dipendenze Vulkan o l'esecuzione con validation layers.

Diagnostica GPU singola, non benchmark: T_total 686568 ns, recording 141944,
submit call 198380, fence wait 256488, host copies 45455, GT comparison 17292.
Query device: upload 3440 ns, kernel 8400, download/intervallo finale 2080.
Raw timestamp `[102664576255179,102664576255523,102664576256403,102664576257243,102664576257451]`.
Non è stato calcolato un rapporto CPU/GPU, CI o speedup.

## Copy-control eseguito, non simulato

Secondo contesto GPU, previsto dalla stessa fase: uint shader senza operatori
floating, 8x2, stride 25. Tutti i 16 pattern preregistrati presenti: ±0,
subnormali min/max positivi/negativi, normali e massimi finiti. Padding delle
34 word inattive: quiet NaN `0x7fc12345`, caricato ma non letto logicamente.

Input 200 byte, GT/output logico 64. Confronto bitwise-f32 riuscito su tutti i
pattern, output_compared=true. Osservati memcpy host 200/64 byte, una copia
Vulkan per direzione, un dispatch (un gruppo), submit/fence completate, zero
CPU backend calls. Shader e identità hardware corrispondono ai pin.
Raw timestamp `[102664584114132,102664584114368,102664584115120,102664584115772,102664584115980]`.

T_total diagnostico 552640 ns; query kernel 6520, upload 2360, download 2080.
Non prova aritmetica generale su subnormali, NaN o f64. I buffer output GPU
non vengono salvati né hashati dal worker: sono conservati verdetto in-process,
GT indipendente, fixture e tracce; nessun output hash inventato.

## Memoria, RSS e raccolta processi

Heap API osservati: 5570600960 byte flags 0 e 11141210112 flags 1. Sono dimensioni
riportate, non memoria libera né 512 MiB di VRAM dedicata: dispositivo integrato.
Affine: quattro allocazioni API da 1296 byte, totale 5184; host payload 3876,
managed osservabile totale 9060. Copy: quattro da 208, totale 832; host 328,
totale 1160. Allineamento incluso; entrambi sotto cap managed 256 MiB.
Allocatori interni driver/librerie restano null/NOT_OBSERVABLE, non zero.

PID driver 177445 raccolto, exit 0. Figli nativi osservati e associati tramite
argv unico per ruolo: CPU affine 177455, GPU affine 177458, GPU copy 177465.
Tutte e tre le ricevute process_created=true, reaped=true, exit 0,
cleanup_errors vuoto. Nessun segnale necessario, nessun processo estraneo
segnalato. I record nativi non contengono PID: l'associazione usa l'osservatore
proc; i PID dei figli metadata brevissimi possono sfuggire al campionamento.

RSS worker campionata CPU 5324800 byte, GPU affine 73707520, GPU copy 73687040;
driver 25800704. Thread osservati: 1, senza pretesa di censire ogni transiente.
Guardie 512 MiB best effort; nessuna garanzia anti-OOM o misura aggregata di
memoria interna. Ultimo du 859394048 byte, sotto 1 GiB. Nessun nuovo file
persistente fuori dall'output; nessuna mutazione dei vecchi file monitorati.

## Ricevuta stabile e gate successivo

Tutti i path relativi seguenti sono sotto `tmp/m1/e-vulkan/validate-affine-004/`:

| Artefatto | SHA-256 |
|---|---|
| `validate-receipt.json` | `66dce9d16e844ae8d900e77bd626f041a6ef96c4a9812775afdffdff35d542e1` |
| `report.json` | `643f957bd48393c531da0693fe385b80e6eaf9d9cce460c93fb057f7fd4ba99b` |
| `execution-observer.json` | `67b8e599d4859306fe62d70d3b32db635682c84a3c0d1413bf14877d662e6987` |
| `verification.json` | `5f1ba43d062982852c4fa0cdbc995cc3eae371a7e840b4bd5014dcdcac9e4610` |
| `runtime-metadata.json` | `ca979159b3c4db798bcbfb14dec9581b2184e0a92d51cd02751b5ec4aae453cd` |

La ricevuta PASS lega schema/kind, lock CPU, ricetta/fonti/binario/shader,
protocollo/preregistrazione, originali/GT, identità effettiva e sette hash degli
artefatti primari chiusi. Questi hash, binding, fixture 16 pattern e pin sono
stati ricontrollati dopo l'esecuzione, senza un altro dispatch. Fonti e piani
invariati; indici mutabili esclusi, come precisato dal coordinatore.

Nessun nuovo compiler, installazione, rete, download, licenza, Siril, indice,
Git o altro namespace modificato. Nessun dato confermativo o conclusione sulle
prestazioni; nessuna prova degli interni opachi del driver o fallback interno
non osservabile. **Primo incompleto: review indipendente della VALIDATE e della
ricevuta. FAULTS e CONFIRM non avviati e non ammessi automaticamente.**
