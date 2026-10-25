# M1.e: nove arresti attesi della ricetta Vulkan congelata

Esito primario: `EXPECTED_FAULTS_COMPLETE`, exit0 del supervisore, nove worker con exit2 atteso e raccolta confermata dal runner. Nessuna uscita numerica pubblicata nei nove casi. Non è un confronto prestazionale né un PASS complessivo M1.

Esecutore e autore: coordinatore OpenAI GPT-6.1 Sol, identità del runtime, 25 ottobre 2026. Costi/token non esposti. Esecuzione ammessa dopo la review indipendente della VALIDATE e dopo lo stop del preflight hash; nessun altro candidato o benchmark ammesso nello stesso turno. Review indipendente degli esiti ancora necessaria.

## Comando, ambiente e input

```bash
/usr/bin/python3 -I -B tmp/m1/e-vulkan/run.py --phase faults --preparation tmp/m1/e-vulkan/preparation-004.json --case affine-f32-17-19-s19 --direction forward --batch 1 --reference-lock tmp/m1/e-vulkan/pilot-affine-004/reference-lock.json --validate-receipt tmp/m1/e-vulkan/validate-affine-004/validate-receipt.json --reference-pool native-only --reviewed-execution yes --output tmp/m1/e-vulkan/faults-affine-004
```

Avviato una sola volta con Popen e ambiente esplicito: PATH di sistema, locale C, HOME/TMP/XDG dentro progetto, cache Mesa disabilitata, OMP1, OpenSSL config `/dev/null`, Python isolato senza bytecode. APPORT_REPORT_DIR confinato nell'output tecnico. Stdout `EXPECTED_FAULTS_COMPLETE`, stderr0byte. Il processo driver è stato atteso e raccolto; argv, ambiente, PID ed esito sono nella ricevuta tecnica.

Bytehash verificati prima e dopo: PREP004 `d19f9d7082f9cb31cb21b0a98d6d1a52044cd3474b6fd42099f383c87d4befe0`; engine `af2253a5b8cfe6e756094c3235c57c4e0f264d25da7ee9a30a33edb7df0c074d`; lock `b479a22a75af76b5e494f23c1af1d0ccdfc19ffe36d1eb6ef043a51343a26e79`; VALIDATE `66dce9d16e844ae8d900e77bd626f041a6ef96c4a9812775afdffdff35d542e1`; runner `f1d6bf74e0937773ee11d5315382d9c660de2df96ed98426c993491ab62c107a`. Otto fonti PREP verificate prima dell'avvio; il runner ripete i controlli su fonti, shader, protocollo, caso e originali/GT. Nessuna modifica a codice o fixture.

Versioni e identità hardware della ricetta: engine compilato da GCC15.2, shader dalla PREP004 autenticata; Python3.14.4 già osservato nei turni precedenti, non un nuovo probe in questo wrapper. AMD1002:1681 integrata, RADV REMBRANDT, Mesa26.0.8, queue0 secondo VALIDATE compatibile. Il report dei fault indica questa origine e ricontrolla l'identità sui ready riusciti; non attribuisce una nuova osservazione hardware ai rifiuti precedenti al ready.

## Esiti effettivi

| Guasto primario preregistrato | Diagnosi osservata | Dispatch / submit / fence |
|---|---|---|
| adapter-mismatch | Adapter fissato assente/ambiguo; nessuna selezione software o altro device | 0 / 0 / 0 |
| alloc | Errore iniettato prima di vkAllocateMemory | 0 / 0 / 0 |
| budget | Buffer gestiti rifiutati prima delle allocazioni device | 0 / 0 / 0 |
| badshader | Shader rifiutato, nessun sostituto | 0 / 0 / 0 |
| input-nan | Input non finito fuori contratto | 0 / 0 / 0 |
| output-mutation | Mismatch allo scalare0, tolleranza zero | 1 / 1 / 1 |
| input-saturated | Ampiezza fuori contratto, nessuna saturazione sostitutiva | 0 / 0 / 0 |
| device-lost | VK_ERROR_DEVICE_LOST iniettato prima della submission | 1 / 0 / 0 |
| f64 | Contratto f64 non validato, nessun cast f32 | 0 / 0 / 0 |

Tutti: `published_output=null`, `cpu_kernel_calls=0`, processo creato e raccolto, exit2, cleanup_errors vuoto. Un thread osservato per worker; massimo RSS campionato73.916.416byte, best effort. Il wrapper verifica il cap scratch1GiB con margine16MiB prima dell'avvio e managed/RSS restano i limiti del prototipo, non una garanzia anti-OOM.

L'output-mutation avviene dopo un lavoro GPU completato e viene rifiutato prima della pubblicazione. Il device-lost è un'iniezione prima della submit, non un guasto reale del driver o una perdita hardware provocata. Il dispatch registrato per quel caso è command recording, non submission accettata. Nessuna promessa sugli interni opachi del driver.

## Artefatti e limiti

- `tmp/m1/e-vulkan/fault-execution-004/execution.json`: `38163380279c5c8ba7aaebbdae0c0b85024883b945fcbdc0cf33542127621b82`.
- `tmp/m1/e-vulkan/faults-affine-004/report.json`: `629c7ec82b66585941457da9221f197df50a051a6fb40554d7eb5cc90a438284`.
- `tmp/m1/e-vulkan/faults-affine-004/faults-receipt.json`: `4bab5798f3f45f4c81148c91f6097e89fe3e9968e946e2094cd995b352893c3f`.

La ricevuta lega i dieci artefatti interni, lock, VALIDATE, ricetta e device. Raw e FIRSTFAIL eventuali non sono stati sovrascritti o eliminati; in questo turno nessun errore inatteso o retry. Il confronto riguarda soltanto questo prototipo e questa tupla, non tutti i driver, modelli o guasti del prodotto. FAULTS richiede ora verifica indipendente; CONFIRM resta non ammesso finché i relativi gate non sono verificati.
