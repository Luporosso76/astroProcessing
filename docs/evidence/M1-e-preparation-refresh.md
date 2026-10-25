# M1.e: refresh offline PREP004

**PREPARED_NOT_RUN**, exit 0. Solo nuova preparazione e questo record; nessuna
modifica alle fonti, test, runner, engine o piani in review. Verificati invariati
120 file preesistenti, inclusi PREP003, snapshot e ricevute storiche. Nessun
engine, pilot, regressione, GPU, validate/fault/confirm o benchmark eseguito.

Modello dichiarato dal runtime: `openai/gpt-6.1-sol`. Nessuna delega.

## Azioni effettive

Dalla radice: `du -s -B1 tmp`, `df -B1 .`, controllo ID 004 libero, manifest
write-once dei digest precedenti, poi:

```bash
/usr/bin/python3 -I -B tmp/m1/e-vulkan/prepare.py --build --series 004
```

Il builder ha ricontrollato dodici pin e catena firma/indice/deb/prefix locale;
gpgv exit 0. Tre compilazioni GLSL e tre spirv-val, tutte exit 0. Point/FFT:
Float32, rispettivamente 12/11 operatori aritmetici, nessun NoContraction
mancante; copy zero operatori floating. G++ Ubuntu 15.2.0-16ubuntu1, build host
exit 0, log senza warning. Il binario è stato ricompilato, non copiato dalla
vecchia build, anche se il digest coincide. Nessuna esecuzione del binario.

Preflight iniziale: 857751552 byte globali; builder: 857788416 byte più margine
16 MiB, spazio libero 124895363072. Ultimo du: 858165248 byte, sotto 1073741824.
TMP/HOME/cache dei tool sotto build-004. Controlli campionati, non quota atomica
contro crescita esterna; guardia RSS best effort sul Popen, non misura aggregata
di tutti i discendenti compiler o garanzia anti-OOM.

Rihashati fonti, piano, binario, indice casi e tre shader dopo la build. Esiti
e argv completi in preparation-004; verifica separata write-once in
`preparation-refresh-004-verification.json`. Nessuna rete/installazione, Git,
indice, modifica di licenza o import di altri candidati.

## Hash finali

Path relativi sotto `tmp/m1/e-vulkan/`, salvo il piano:

| Artefatto | SHA-256 |
|---|---|
| `preparation-004.json` | `d19f9d7082f9cb31cb21b0a98d6d1a52044cd3474b6fd42099f383c87d4befe0` |
| `build-004/engine` | `af2253a5b8cfe6e756094c3235c57c4e0f264d25da7ee9a30a33edb7df0c074d` |
| `build-004/cases-index.json` | `c958973283f14d7a0377c8b5e728b2d04918081229ce2d8a953ee81305d4629b` |
| `preparation-refresh-004-verification.json` | `767c8cabdb939bad1e36a4ac61b02a672911132e069a24ca0d1917c5ee005b14` |
| `plans/M1-e-vulkan.md`, invariato | `f584cc83dab84d1fa35101085834c0601e53d9f20eb997dbf8514e81424715df` |

Gli otto hash delle fonti attuali e i tre digest SPIR-V stanno nel nuovo report.
Nessun tempo/versione storico rietichettato: i nuovi comandi hanno log propri.

## Primo pilot successivo, NON ESEGUITO

Confermato nell'indice immutabile il caso esistente `affine-f32-17-19-s19`:
17 righe, 19 colonne, stride 19 contiguo, f32, batch 1. Dopo chiusura della
review suite e via del coordinatore, in esclusiva:

```bash
/usr/bin/python3 -I -B tmp/m1/e-vulkan/run.py --phase pilot --preparation tmp/m1/e-vulkan/preparation-004.json --case affine-f32-17-19-s19 --direction forward --batch 1 --reference-pool native-only --reviewed-execution yes --output tmp/m1/e-vulkan/pilot-affine-004
```

La directory pilot-affine-004 era assente alla verifica; non è stata creata.
Questo è un riferimento CPU native-only condizionato, non migliore CPU generale
o scelta di stack. Shortlist invariata. Dopo pilot e lock serve review del
lock prima di qualsiasi GPU validate/fault/confirm. M1 resta aperta.
