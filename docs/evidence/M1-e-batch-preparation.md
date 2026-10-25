# M1.e: preparazione statica batch 001

Esito: preparazione statica consegnata, **non pronta per esecuzione**. Nessun
FAULTS004, pilot, VALIDATE, CONFIRM, test mock, build o benchmark avviato. Nessun
import del candidato o dei moduli nuovi. Nessuna delega, rete, installazione,
indice, modifica a Git, progress o prove precedenti. La review TESTS di
VALIDATE004 è contesto comunicato dal coordinatore, non una review di questo batch.

Autore: modello `openai/gpt-6.1-sol`, dichiarato dal runtime. Token e costi non
esposti. Python osservato nel controllo statico:
`3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.

## Scope e fonti effettive

Creati soltanto `tmp/m1/e-vulkan-batch/`, `plans/M1-e-batch.md` e questo record.
Il namespace contiene manifest delle tuple, controller preparatorio, 20 mock
non eseguiti e ricevuta `preparation-001.json`. Nessuna fixture materializzata.

Letti integralmente `run.py` (511 righe), `cases-index.json` (644), prereg GPU
`plans/M1-e-vulkan.md` (194) e convenzione `PLANS.md` (46). Letti anche gli
estratti necessari di prepare/PREP004 ed engine per costi/input/output e FFTW.
Non letto il piano scientifico lungo; non toccate fonti F/D/viewer.

Ricalcolati gli hash PREP004, delle otto fonti, binario, tre shader, indice e
manifest casi, protocollo/prereg. Corrispondono alla baseline. Pin completi
registrati in `preparation-001.json`; fonti vere distinte dai documenti mutabili
di coordinamento. Il controllo non attesta una nuova verifica numerica dei GT
né una ricostruzione degli oltre 185 hash storici.

## Copertura corretta dell'indice

L'indice contiene **42 casi, 66 tuple**, non 54: 6 affine, 6 convd, 6 warp e
24 FFT. Per FFT: impulso, costante, cosx, cosy, sinx e siny, ciascuno con quattro
combinazioni N/layout. Ogni FFT ha forward e inverse implementati dalla CLI.
Manifest: 42 forward, 24 inverse, batch 1 e dtype f32 per ogni riga.

`tuples.json` elenca tutte le righe, in ordine dell'indice, con costi CPU/GPU e
pair simultaneo. Un controllo statico indipendente dal controller ha ricostruito
le 66 righe e verificato uguaglianza esatta, unicità, ordine e costi. Nessuna
famiglia omessa o sostituita con affine. I padding dell'inverse rimangono nel
descrittore originale; il runner legge il prefisso complesso contiguo del GT
indipendente e il relativo offset già implementato.

Nessun nuovo numerics, oracle, epsilon o conversione. f64 non è una tupla
numerica accettata: resta STOP primario atteso nei fault. Validità FFT limitata
alle fixture dyadic preregistrate. CPU diretta per affine/convd/warp; FFTW f32
con piano riusato per entrambe le direzioni FFT. Confronto native-only condizionato,
non migliore CPU generale e non scelta dello stack M1 dai dati del microcaso.

## Controller e guard preparati

Le funzioni leggono sorgenti e artefatti con O_NOFOLLOW su tutti i componenti,
verificano recipe/source/GT/prereg/device/lock e rifiutano ricevute mancanti,
incompatibili, parziali o con cleanup fallito. Il controllo batch è aggiuntivo:
i subprotocolli scientifici rimangono nel runner congelato, non vengono copiati
o sostituiti. Nessun nuovo hash batch viene imposto retroattivamente a PREP004.

Ledger separato: NOT_RUN iniziale, PARTIAL, FAIL terminale o PASS soltanto a
completamento delle fasi. Helper per checkpoint numerati write-once e catena
hash; fonte batch/piano/manifest/tuple devono restare identici al resume.
Prima failure ferma il turno; le tuple successive restano NOT_RUN. Non esiste
un percorso che avvii un gate mancante per riparare una ricevuta. L'adozione
delle prove nel ledger deve comunque passare i verifier: il digest da solo non
è prova numerica. Questa integrazione non è ancora operativa.

Prototipo supervisor diretto: Popen argv/PID, exit/reap, stdout/stderr nonblocking,
deadline, RSS best effort, righe parziali/oversize e cleanup separato per ogni
handle registrato. Log esterni limitati a 64 KiB per stream; su overflow conserva
il prefisso e STOP. Nessun segnale verso PID estranei o gruppi. Non è ancora
qualificato per i nipoti nativi avviati dal runner.

Mock preparati, non eseguiti: missing receipt, wrong recipe, failed/nonzero
child, cleanup di handle orfano proprio e indipendenza tra cleanup falliti,
righe parziali/oversize, collisione output, symlink/traversal, NOT_RUN ledger,
false PASS da exit zero, checkpoint fonte/catena errati e rifiuto del launcher.
Il mock dell'orfano riguarda un Popen proprio, **non prova cleanup dei nipoti**.
Mancano ancora test integrati del supervisor, deadline/costruttore e albero
reale dei processi. AST PASS non equivale a regressioni PASS.

## Blocchi concreti prima dei dati

1. Il runner congelato ammette output solo sotto `tmp/m1/e-vulkan/` (riga 429)
   e lock/ricevute sotto lo stesso namespace (413, 276). Il namespace nuovo
   `e-vulkan-batch` è esterno. `candidate_command()` produce sempre STOP prima
   di Popen. Nessun autorename, symlink, output fuori mandato o copia del runner
   aggira il problema. Occorre definire e revisionare una nuova integrazione
   prima dei dati, mantenendo distinta l'eventuale continuità della vecchia ricetta.
2. Non qualificati cleanup dei nipoti/PID nativi reali e limiti hard delle
   scritture interne al driver. Il controllo del figlio diretto non basta.
3. Riserva nominale dell'intero piano incompatibile con il tmp osservato.
4. Mancano review indipendente del nuovo sorgente ed esecuzione mock autorizzata.
5. Nessuna ammissione attuale a FAULTS004 o ad altre tuple.

Non viene pubblicata alcuna CLI esecutiva batch. La preparazione fornisce guard
e piano di completamento, non un launcher pronto o un PASS scientifico.

## Memoria e tmp: misure statiche, non tempi candidato

Costi ricavati dalle dimensioni esplicite dell'engine. Con I byte input e O byte
output logico, host H=I+2O. CPU pixelwise H, CPU FFT H+2O; GPU H+4B, con B=I
per pixelwise e B=O per FFT. Il pair somma CPU idle e GPU attiva. Non sommare
tutte le tuple come payload contemporaneo; rimangono sequenziali.

Massimo nominale simultaneo: 100663296 byte per inverse FFT 1024. Non include
interni opachi FFTW/driver, che restano unknown, mai zero. Il requisito reale
VkMemoryRequirements rimane un gate dell'engine, cap managed 268435456 byte.
Driver/worker RSS 536870912 best effort ciascuno; nessuna promessa aggregata
anti-OOM o osservazione hardware nuova.

Controllo metadata effettivo del tmp: `os.walk(followlinks=False)`, `lstat`,
somma `st_blocks*512` una volta per device/inode, directory incluse. Osservati
859713536 byte, disponibili filesystem 124893224960. Con margine 16777216 il
tmp corrente rispetta cap 1073741824. F può aggiungere prove durante il turno:
questo valore è un'osservazione puntuale, non un lock globale dello storage.

Riserva nominale manifest: 1 MiB artefatti per fase, 4 MiB per tupla,
276824064 byte per tutte le 66 tuple. Tmp + riserva + margine = 1153314816,
oltre cap di 79572992 byte. **Il piano completo non passa questo preflight
nominale.** Nessun file archiviato/rimosso, nessuna riduzione di copertura.
La riserva non è un hard cap delle tracce interne: va qualificata prima dei dati.

## Continuità tiny004, non adottata automaticamente

Ricontrollati soltanto i digest noti:

- Lock CPU: `b479a22a75af76b5e494f23c1af1d0ccdfc19ffe36d1eb6ef043a51343a26e79`.
- VALIDATE: `66dce9d16e844ae8d900e77bd626f041a6ef96c4a9812775afdffdff35d542e1`.

Nessun nuovo ledger attribuisce già PASS a questi gate. La continuità richiede
tutti i binding identici e review indipendenti, non un nuovo hash retroattivo
al vecchio engine. Sette campioni pilot non valgono 21 coppie confirm.

## Verifiche effettive e hash di consegna

Eseguito `/usr/bin/python3 -I -B -` con stdin di audit statico standard library:
AST dei due moduli, JSON/costi/copertura contro l'indice, bytehash dei pin e
metadati tmp. Exit 0. Nessun processo figlio da quello script, nessun import
dei moduli preparati. Il comando AST ripetibile, anche indicato nel piano, è:

```bash
/usr/bin/python3 -I -B - <<'PY'
import ast
from pathlib import Path
for path in sorted(Path('tmp/m1/e-vulkan-batch').glob('*.py')):
    ast.parse(path.read_bytes(), filename=str(path))
    print('AST_OK', path)
PY
```

| File | Righe | SHA-256 |
|---|---:|---|
| `controller.py` | 426 | `558fc15588f1aaab9e62d703db676847c5e7b9d7d0fb659c59b77d6522c331a8` |
| `test_controller.py` | 165 | `7dadab538984668c225641dfa8b0e2197d92b3618c1c7dadf84ddffab97440ea` |
| `tuples.json` | 91 | `daee49ce35807324a51b289d267617e1fb4b16d44a149a885d8787d56a09acc5` |
| `plans/M1-e-batch.md` | 147 | `984c7929d941d677a9c7841d139e58a24277f9312aea23a077d94bc32ec318ff` |

Ricevuta statica `preparation-001.json` (70 righe), SHA-256
`c9c51489248e53af8a5526efe65870698b5591a5b6aa52b4a5e220e68df42fd3`.
Il successivo controllo AST esatto sopra e il ricontrollo JSON/hash delle quattro
fonti hanno avuto exit 0, sempre senza import dei moduli o test.

AST: 2/2 PASS. Copertura statica: 66/66 righe uguali all'espansione dell'indice.
Mock: 20 preparati, **0 eseguiti**, nessun PASS attribuito. Nessuna review
indipendente di questi sorgenti svolta in questo turno senza delega.

Misure future CONFIRM: protocollo originale tre cluster, due warm-up e sette
round per cluster, CPU/GPU alternati con processo inattivo idle, 2000 bootstrap
appaiati seed 4059, exp della mediana log dei rapporti T_total, percentili
lineari 2,5/97,5. Startup/cold/warm/planning, copie/kernel/fence/preflight e
verifica GT separati come da prereg, senza cherry-pick. Nessuna statistica
nuova prodotta qui. Whole-M1 e scelta stack rimangono gate distinti.

Prossimo comando consentito: soltanto AST sopra. FAULTS004 e tutte le altre
tuple restano NON AMMESSI fino al turno esplicito del coordinatore. M1 aperta.
