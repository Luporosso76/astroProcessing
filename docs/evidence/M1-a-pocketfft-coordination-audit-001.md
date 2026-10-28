# M1.a: audit del coordinamento pocketfft 001

## Perimetro, fonti e metodo

26 ottobre 2026. Record documentale delle verifiche di fedeltà e delle disposizioni del coordinatore, non nuova prova FFT. Lo stato corrente ha una sola fonte: `docs/progress.md`.

L'audit `data` della sessione `ses_e8748b00fffe0WdY3JPL8G32hC` confronta sette compressioni riuscite, due deleghe, un tentativo fallito e un checkpoint: 11 unità. Ha letto 16 messaggi Gemini e recuperato 12 sommari antecedenti. Il testo stabile e il JSON concordano su otto finding: due HIGH, cinque MED e un LOW. Questa redazione ha letto entrambi, non ha ripetuto l'accesso alla cronologia.

La review `ses_e87418d54ffe0H8Drmu9F93pbP`, comunicata dal coordinatore, non rileva falsa chiusura scientifica nei 232 casi reali o nei 37 mock. Segnala due riferimenti mancanti nell'indice. La review delta nella stessa sessione rileva invece un MED residuo nel benchmark: il fallimento del cleanup può perdere la diagnosi primaria. Questo record non è una review indipendente della correzione.

Fonti stabili lette integralmente e SHA-256 calcolati in questa sessione:

| Fonte | Righe lette | SHA-256 |
| --- | ---: | --- |
| `tmp/model-audit/gemini-fidelity-001.md` | 206 | `1b1d4a41ec4f08f84c042ba04bc75a1894cd82adea1ac81e30356b29ff021287` |
| `tmp/model-audit/gemini-fidelity-001.json` | 200 | `e8447d0ff161aafcedd215e64ab5a21edbd23bd250b685c08cdde2a019742414` |
| `tmp/m1/fft-pocket-benchmark/PLAN.md` | 128 | `483feacd05b827650de60acc370bd5a1edb3f66f414ea202c9060c7fd1b0aca9` |
| `docs/evidence/M1-a-pocketfft-benchmark-prep-followup.md` | 177 | `86e2ed084eb94a122bc4c75fcf536d992d93355cd5566b0f56489adbc2145801` |
| `docs/evidence/M1-a-pocketfft-validation-003.md` | 148 | `a18f229299981fcdf2c2104d08af4288dc5a5b700083f26d4b7e1fab92dad7c1` |
| `docs/evidence/M1-a-pocketfft-runtime-observation-tests-004-validation.md` | 159 | `7108a82a0f39d4508c3a2d1c853016a0fc9195dd5f84e01b356a05857b9e697f` |

Letti anche `docs/progress.md` (59 righe) e `docs/evidence/README.md` (166 righe) prima della modifica: 1243 righe complessive degli otto file. Comando di autenticazione eseguito dalla radice `/opt/workspace/astroProcessor`, exit 0:

```sh
sha256sum tmp/model-audit/gemini-fidelity-001.md tmp/model-audit/gemini-fidelity-001.json tmp/m1/fft-pocket-benchmark/PLAN.md docs/evidence/M1-a-pocketfft-benchmark-prep-followup.md docs/evidence/M1-a-pocketfft-validation-003.md docs/evidence/M1-a-pocketfft-runtime-observation-tests-004-validation.md
```

Ambiente di redazione: Linux, directory di progetto indicata sopra; versione di `sha256sum` non rilevata. Autore: subagente documentazione, modello dichiarato dal runtime `openai/gpt-6-astra`, senza override. Costi e token non disponibili. Output: questo record, aggiornamento dello stato e dell'indice. Esito documentale PARZIALE: disposizioni registrate, correzione e verifiche residue non concluse.

## Finding e disposizioni

| Finding | Evidenza e disposizione |
| --- | --- |
| F01 HIGH | Il checkpoint dichiara capienza scratch senza prova. I 931823616 byte storici sono già successivi al ritiro di `cpp.bin`. Con picco nuovo 67674112, riserva 67108864 e margine 16777216, il deficit storico rispetto a 1073741824 è 9641984 byte. Non è una misura attuale. Richiedere preflight corrente esclusivo; nessun cleanup extra autorizzato. |
| F02 HIGH | La sequenza del checkpoint saltava review build/ELF, inventario/licenze, validate192 più nove negativi e review dei risultati. Il checkpoint è INVALIDO come ammissione. I gate si ricavano dal piano attivo, dal PLAN del namespace e dalle evidenze effettive, non dalla compressione. |
| F03 MED | Protocollo corretto: `READY PFFTB1`, `RUN 1..9`, `RESULT`, `QUIT/BYE`. I primi venti unit erano sette positivi e tredici negativi, non undici/nove. I nove negativi nativi sono separati. L'IQR inizialmente omesso è stato aggiunto nel followup con convenzione tipo7 e vettori mock; questo non è una misura prestazionale. |
| F04 MED | Lo script storico su `manifest.get("files", {})` ha verificato 0/12 pin: nessun PASS di autenticazione. Il coordinatore comunica una successiva verifica di otto hash di consegna e, in un controllo successivo, dodici pin sorgente, quattro pin artefatto e l'aggregato dei sette FIRSTFAIL. Non si sommano questi controlli come file unici: possono sovrapporsi. Sono controlli della consegna precedente alla nuova patch, non dei file ora in scrittura. |
| F05 MED | Alcuni sommari perdevano limiti di review, hash non ricalcolati, assenza di osservazione OS e cap. Prima dell'ammissione occorre leggere i gate e i cap effettivi nel PLAN, che ne resta la casa canonica. Un hash riferito dall'autore non diventa autenticazione indipendente; un receipt non diventa review. |
| F06 MED | Il mandato coder era ambiguo sull'esecuzione mock. Non prova esecuzione abusiva e non dimostra autorizzazione specifica del proprietario. I 38 mock sono esiti reali dell'autore, non prove soltanto predisposte. I prossimi test spettano a `tests`, con ammissione distinta; la sintassi C++ resta pendente fino alla build autorizzata, senza nuova ricerca di parser. |
| F07 MED | Nessuna corrispondenza al glob non prova assenza di una directory o del track M1.f. Lo stato M1.f resta quello delle fonti canoniche richiamate in `docs/progress.md`; nessuna ricreazione del track è giustificata da quel glob. |
| F08 LOW | Il tentativo di compressione con placeholder malformati è fallito; il successivo è riuscito. Sono due tentativi, non due compressioni riuscite. Nessuna modifica retroattiva della storia. |

F01 e F02 sono neutralizzati come rischi di ammissione attraverso l'invalidazione del checkpoint e il ripristino dei gate. Non provano che un gate sia stato realmente saltato. L'audit non consente di attribuire la qualità a un modello né di dichiarare tutte le lacune chiuse.

## Rettifiche e conservazione delle prove

Il report primario `M1-a-pocketfft-benchmark-prep-followup.md` resta immutato. La frase iniziale sull'autorizzazione del proprietario, ripresa anche nel PLAN letto, non è comprovata dall'audit del mandato; qui si registra l'ambiguità senza inventare autorizzazioni o abusi. Il coordinatore comunica di aver verificato raw con 38 `ok`, exit 0, hash degli artefatti e conteggio AST di 22 ProtocolTests più 16 LifecycleTests. Le verifiche riguardano la consegna precedente alla correzione del MED residuo. Nessun PASS si trasferisce alla patch successiva.

L'indice aggiunge i due riferimenti omessi: `M1-a-pocketfft-validation-003.md` resta FIRSTFAIL con 232 NOT_RUN; `M1-a-pocketfft-runtime-observation-tests-004-validation.md` conserva PASS_MOCK37, preceduto dai due target budget. I 37 mock non sono 232 FFT reali. La successiva qualifica004 dei 232 casi resta PASS limitato, senza prestazioni, licenze, adozione o chiusura M1.

Il MED residuo riguarda diagnosi primaria e cleanup falliti insieme. Al momento del presente record, coder `ses_e8762fe0dffeEH84Bg0BFwoLfV` lavora in PREP; test combinati e review dei nuovi SHA sono pendenti. Non sono stati letti `benchmark.py`, `manifest.json` o snapshot primari in scrittura. Nessuna build, worker o benchmark nativi del nuovo track risultano avviati nel handoff del coordinatore; questa redazione non ha svolto un censimento OS.

## Ripresa obbligatoria

Il PLAN del namespace resta la fonte dei comandi e dei cap; non viene modificato da questo incarico. La lettura ha confermato limiti esterni mock di 55 s, AS 256 MiB e file 1 MiB; scratch globale 1 GiB con riserva 64 MiB e margine 16 MiB; root 0700 e ambiente privato. Questi valori non attestano disponibilità corrente.

La sequenza da rispettare è: fix stabile; review indipendente e test mock ammessi sui nuovi SHA; build AS 1 GiB; review di build/ELF e inventario/licenze del nuovo binario; validate192 più nove negativi nativi AS 256 MiB; review dei risultati; soltanto dopo measure con 576 startup, 1152 warmup e 4032 campioni. Ogni turno richiede preflight corrente PASS, un solo runtime alla volta e i prerequisiti del PLAN effettivamente verificati. I 232 casi storici non qualificano automaticamente il nuovo driver persistente.

## Limiti dell'audit

La mappatura storica annidata fra placeholder e intervalli originali è incompleta. Dodici sommari antecedenti recuperati non equivalgono al confronto integrale dei messaggi originari. Il successo di sette compressioni non certifica la fedeltà totale della cronologia. Gli hash delle fonti sanificate identificano gli input al momento del controllo, non ne provano l'immutabilità pregressa.

Le verifiche del coordinatore e la review delta sono attribuite al handoff ricevuto; non sono state rieseguite in questa redazione. Nessuna misura attuale dello spazio, verifica dei runtime host, prova scientifica, test, import, compilazione o ricerca esterna. Nessuna operazione Git, installazione, delega o modifica agli storici. Il MED residuo, i nuovi test e la review impediscono di dichiarare l'audit interamente chiuso.
