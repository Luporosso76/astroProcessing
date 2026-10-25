# M1.e: primo confronto confermativo affine CPU/Vulkan

25 ottobre 2026. Esecuzione del coordinatore, modello `openai/gpt-6.1-sol`. Risultati primari reali; review indipendente dei campioni ancora da registrare. Nessuna chiusura complessiva M1.

## Ricetta e ammissione

Caso `affine-f32-17-19-s19`, forward, batch 1, 323 pixel. Riferimento `cpp-direct`, profilo `workspace-reuse`, pool `native-only`: non il migliore di tutti i linguaggi. Pilot separato, VALIDATE e nove FAULTS avevano review indipendente positiva. Nessun campione pilot trasferito a questa serie.

```bash
/usr/bin/python3 -I -B tmp/m1/e-vulkan/run.py --phase confirm --preparation tmp/m1/e-vulkan/preparation-004.json --case affine-f32-17-19-s19 --direction forward --batch 1 --reference-lock tmp/m1/e-vulkan/pilot-affine-004/reference-lock.json --validate-receipt tmp/m1/e-vulkan/validate-affine-004/validate-receipt.json --faults-receipt tmp/m1/e-vulkan/faults-affine-004/faults-receipt.json --reference-pool native-only --reviewed-execution yes --output tmp/m1/e-vulkan/confirm-affine-004
```

Output e registro esterno creati esclusivamente, senza collisioni o retry. Controllati prima e dopo i pin di PREP004, runner, engine, lock e ricevute; verificati prima tutti gli otto source pin, protocolli e preregistrazione. Il runner verifica inoltre shader, indice e input/GT. Cap tmp fisico 1073741824 byte: osservati 860889088 byte dopo l'esecuzione, sotto cap. Non è una prenotazione OS universale.

Ambiente del solo processo: HOME, TMP e directory XDG sotto `tmp/m1/e-vulkan/confirm-execution-004/`; cache shader disabilitata; thread OMP/BLAS richiesti uno. Python isolato, nessun nuovo accesso di rete o installazione. Driver e toolchain sono quelli fissati dalla preparazione: engine GCC 15.2, Vulkan Mesa `26.0.8-1ubuntu0.3`, device AMD `1002:1681`, RADV REMBRANDT integrato, UUID `00000000e50000000000000000000000`, queue 0, timestamp 64 bit con periodo 10 ns. Nessun renderer software selezionato.

## Esiti osservati

- Exit driver 0, stderr vuoto, stato `CONDITIONAL_NATIVE_REFERENCE_SERIES_COMPLETE`.
- Tre cluster con nuovi processi CPU/GPU: sei raccolti, exit 0 e cleanup senza errori.
- Due warm-up e sette round per cluster/candidato: 21 coppie confermative, 54 richieste totali. Ordine CPU/GPU alternato; nessun altro benchmark ammesso nello stesso turno.
- Tutti i campioni pubblicati riportano `output_compared=true`, confronto numerico esatto. CPU: un kernel CPU e zero dispatch GPU; GPU: un dispatch, una submit accettata e una fence, zero kernel CPU, upload/download 1292/1292 byte.
- Trace conservate: nove eventi per processo CPU e 88 per processo GPU. Queste includono warm-up; non sono ulteriori coppie temporali.

| Metrica `total_ns` | CPU | GPU |
|---|---:|---:|
| Mediana | 39523 | 408811 |
| MAD | 6272 | 74970 |
| Minimo | 26580 | 269702 |
| Massimo | 46216 | 1664239 |

Statistica preregistrata `exp(mediana(log(T_CPU/T_GPU)))`: **0.08539116741822347**. Bootstrap appaiato gerarchico, tre cluster e sette round, 2000 ricampionamenti, seed 4059; IC percentile lineare 95% **[0.040999118692929416, 0.11143010199039907]**.

Il rapporto è minore di uno: questo percorso GPU è più lento sul microcaso 17x19. Non dimostra inferiorità GPU su immagini grandi, convoluzioni o FFT. Non adottato né sostituito alcun backend del prodotto.

## Artefatti e limiti

- `tmp/m1/e-vulkan/confirm-affine-004/report.json`: SHA-256 `b563b4e7a9d9949a506ecb3a9ea5f82658050c340efa93d606954e65bd55b25f`.
- `tmp/m1/e-vulkan/confirm-execution-004/execution.json`: SHA-256 `5e5496ed21af18b3101dbeba9ab5ddbea4108e5c98d08706bcdf46e1b15e1625`.
- PREP004: `d19f9d7082f9cb31cb21b0a98d6d1a52044cd3474b6fd42099f383c87d4befe0`.
- Lock CPU: `b479a22a75af76b5e494f23c1af1d0ccdfc19ffe36d1eb6ef043a51343a26e79`.
- Ricevuta FAULTS: `4bab5798f3f45f4c81148c91f6097e89fe3e9968e946e2094cd995b352893c3f`.

`total_ns` comprende scansioni contrattuali/finitezza, tracing, kernel e trasferimenti pertinenti; non è tempo puro del kernel. Il confronto dell'output con GT avviene dopo il ritorno di `run()` ed è escluso da `total_ns`, registrato separatamente in `verify_ns` (`engine.cpp:523-530`). Anche setup e startup sono separati. Trace e dispatch hanno un peso significativo su 323 pixel. Precisazione richiesta dalla review indipendente: numeri e intervallo non cambiano. Cache non dichiarata fredda, host non fisicamente isolato; soltanto tre cluster, intervallo descrittivo limitato a questa configurazione.

RSS osservato e contatori API non provano limiti universali del processo o assenza di allocazioni interne. Nessun audit degli interni driver, stabilità su altri hardware, f64 qualificato, tolleranza generale FFT o conservazione scientifica M3. Nessun risultato inferito per le altre 66 tuple preparate separatamente. Review indipendente dei raw richiesta prima di usare questa serie in una decisione.
