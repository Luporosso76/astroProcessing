# M1.e batch: follow-up preparazione 002

Corretti sorgenti concreti dopo i finding indipendenti comunicati dal coordinatore.
Esito esclusivamente statico: AST 7/7, **nessun import dei moduli preparati o del
candidato, test, processo dummy/engine, build, pilot, GPU o benchmark eseguito**.
Nessuna delega, Git, rete, installazione, indice, Siril, modifica di licenza o
scrittura in altri namespace. Non è una nuova review indipendente o auto-approvazione.

Autore: `openai/gpt-6.1-sol`, identità dichiarata dal runtime. Python osservato:
`3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`. Costi/token non esposti.

## Snapshot prima degli edit

`tmp/m1/e-vulkan-batch/snapshots/001/` conserva a byte identici controller,
venti mock originali, manifest, preparation-001, vecchio piano e vecchia evidenza.
Creazione esclusiva, fsync, confronto dei byte e SHA-256, file resi read-only.
Manifest snapshot SHA-256:
`a8cb32031c425a0bc5322c88ef8b530b413174a24230bb62982f3f014ccb6bb8`.
Snapshot ricontrollato dopo gli edit. La ricevuta e l'evidenza 001 restano invariate.
Il testo attuale del vecchio report hashato nello snapshot è quello realmente
consegnato, non un digest ricavato dal riassunto della conversazione.

## Namespace: finding corretto nel codice

Sorgenti sotto `tmp/m1/e-vulkan-batch/`; output futuro esplicito
`tmp/m1/e-vulkan/batch-001/<case--direction--b1-f32>/<phase>`. È la destinazione
ora autorizzata, compatibile con BASE del vecchio runner; il blocco precedente
non era strutturale. `candidate_command()` costruisce argv reali della CLI
congelata e verifica i gate, invece di fermarsi sempre per namespace.

`claim_output_root()` è create-only. `prepare_phase_parent()` crea soltanto il
parent privato, non la directory di fase che crea il runner. Cammino O_NOFOLLOW,
owner/mode e collisioni verificati nuovamente prima del Popen. Lock e ricevute
restano nell'albero del vecchio BASE. Nessun runner copiato o modificato.
**Nessuna di queste funzioni è stata eseguita; batch-001 non è stato creato.**

Ricalcolati PREP004, source8, engine, shader3, indice e protocollo/prereg:
hash invariati. 66 righe del manifest identiche alla 001, batch 1 f32,
42 forward e 24 inverse da 42 casi. GT, tolleranza zero e pin numerici non
cambiano. Nessuna nuova fixture o conversione f64. Le letture numeriche dei
GT non sono state ripetute: il nuovo guard ne verificherà i byte prima dei dati.

Tiny004: FAULTS con review PASS, CONFIRM principale reale completato IN REVIEW,
come comunicato dal coordinatore. Nessuna adozione nel ledger, nessun suo
campione letto/riusato o trasformato in misura batch.

## Sessione propria e chiusura concreta

`session_child.py` applica RLIMIT_FSIZE hard/soft prima del runner e attende
un token su pipe. Il supervisor usa `start_new_session=True`, registra Popen
e identità PID/starttime/PGID/sessione, salva registrazione pre-exec e solo
dopo sblocca exec. I worker congelati ereditano gruppo e sessione.

Contratto esplicito non ostile/non-escape: nessun setsid/setpgid nei discendenti.
Il censimento raccoglie solo metadati dei membri propri. Il leader rimane non
raccolto grazie a waitid WNOWAIT; nessun poll nel nuovo supervisor. Prima di
ogni segnale di gruppo si ricontrolla l'identità trattenuta. Cleanup cooperativo
SIGINT, poi TERM/KILL soltanto al proprio gruppo, controllo nipoti residui e
raccolta del figlio diretto dopo l'ultima possibile escalation.

Uno stato kernel non interrompibile, perdita d'identità o nipote residuo lascia
STOP/cleanup non provato. Un handle non raccolto rimane registrato e impedisce
nuovi lanci nello stesso processo. Non si promette cleanup universale o difesa
contro codice ostile. Il vecchio helper close_owned resta soltanto per i venti
mock originali e rifiuta veri Popen; non supervisiona più il candidato.

I close di pipe, gate, selector e file sono indipendenti; fsync/close falliti
restano nel risultato. Ricevuta finale **dopo ExitStack**, non prima. Un errore
di pubblicazione genera PublicationFailure e prova esterna separata; se fallisce
anche quella scrittura, l'eccezione conserva il risultato senza inventare una
ricevuta persistita. Exit 0 non è PASS numerico.

## Budget derivato e limite applicato prima dei dati

Eliminata la riserva arbitraria 1 MiB/fase. `budget.py` conta gli schemi reali:
27 campi numerici sample, cinque query raw, startup/device, processi, report,
receipt, duplicati pilot, fixture copy, trace e checkpoint. Richieste: pilot 9,
validate 3 incluso copy, confirm 27 per backend, fault 9 figli. Trace pixel
8 eventi/richiesta; FFT `6+2*(1+log2(N))+inverse`: 18/19 eventi a N32,
28/29 a N1024. Le famiglie e le direzioni rimangono separate.

Massimi dichiarati dell'ambiente 002, prima dei dati: topologia osservata con
due device enumerati, tre queue family e due heap; quattro allocazioni, stringhe
Vulkan 256 caratteri, path 512, diagnosi 4096, 32 nodi errore con otto note,
otto membri simultanei e 128 identità di sessione nel tempo. Unicode escapato,
indentazione, uint64 e token float finiti sono inclusi. Le sentinelle numeriche
del modello servono solo a dimensionare JSON, non sono campioni o input engine.

RLIMIT_FSIZE deriva dal massimo schema successo/fallimento per fase e si eredita
in runner/engine/du. Il vecchio limit_child non lo annulla. Nessun retrobinding
nel lock vecchio: la registrazione nuova lega separatamente batch-envelope-002,
source/plan hash, manifest, tupla e budget mediante digest della ricetta batch.
Anche se SIGXFSZ è ignorato da una libreria, file al cap, trace incompleto,
conteggi o file set fuori schema producono STOP dopo chiusura, non log tagliato
con PASS. La pubblicazione parent è limitata prima delle write; short write
sono errori. Il wrapper verifica anche arrotondamento filesystem e budget
globale prima di Popen, senza una quota/configurazione di sistema nuova.

Calcolo analitico statico indipendente, in stdin standard library senza import
o esecuzione di budget.py; non misure del candidato:

| Fase | Schema report B | RLIMIT_FSIZE B/file | Massimo successo B allocati | FAIL corrente B allocati |
|---|---:|---:|---:|---:|
| pilot | 63298 | 1298432 | 552960 | 13012992 |
| validate | 57086 | 1290240 | 692224 | 18096128 |
| faults | 260260 | 1495040 | 1355776 | 25481216 |
| confirm | 150093 | 1384448 | 737280 | 18038784 |

I massimi per fase includono file/directory e arrotondamento 4096, non quattro
immagini complete. I file interni congelati sono rispettivamente 4/8/11/7;
sei slot esterni riservano raw, registrazione, finale, errore pubblicazione e
diagnostica Apport. Successo usa gli schemi per ruolo; FAIL corrente riserva
tutti gli slot al limite OS. Schema startup trace 17732 B; registrazione/finale
successo arrotondati 159744 B, fallimento 970752 B. Le condizioni di fallimento
oltre lo schema restano bounded dal per-file, mai promosse a PASS.

Whole-batch, tutte le 264 fasi ancora future:

- Successo futuro: 215752704 B allocati.
- 265 checkpoint da 94208 B: 24965120 B.
- Massimo FAIL corrente più due file di chiusura/pubblicazione: 28471296 B.
- Parent directory: 274432 B; margine: 16777216 B.
- Riserva totale: **286240768 B**.

Osservazione metadata tmp: 862466048 B allocati, blocco 4096, disponibili
124890271744 B. Bound totale 1148706816 B contro cap 1073741824: **gap 74964992 B**.
È un'osservazione puntuale durante preparazioni altrui, non un lock dello storage.
Il preflight futuro rilegge tutto; non materializza le 66 tuple anticipatamente.
Il calcolo supporta un prefisso completato soltanto dopo prove verificate;
non sottrae da solo tiny004. Il wrapper corrente riserva conservativamente
tutto il batch, senza adozioni o sconti automatici.

Questo bound non prova impossibilità universale del driver. RLIMIT_FSIZE limita
il singolo file, non il numero di file arbitrari di codice opaco. Il modello
vale per file set congelato non ostile/cache disabilitata; interni driver/FFTW
restano unknown. Managed 256 MiB/RSS 512 MiB best effort invariati, senza promessa
aggregata anti-OOM. Nessun limite scelto perché entrava nel cap.

Il piano contiene un protocollo **candidato, non eseguito** di archivio lossless:
solo tupla completa e revisionata, nessun consumer attivo, bytehash originali,
verifica estrazione, ricevuta immutabile e riserva originale+archivio+verifica.
Non riguarda failed proofs e non garantisce compressione. Nessuna prova esistente
è stata archiviata, rimossa o modificata; nessuna eccezione automatica al cap.

## Verifiche statiche e prossimo passo reale

Eseguiti comandi `/usr/bin/python3 -I -B -` con stdin standard library per:
snapshot byte/hash prima degli edit; AST di sette moduli; uguaglianza esatta
delle 66 tuple e dei venti mock originali; hash delle fonti baseline; calcolo
analitico degli schemi; verifica AST di sessione privata, assenza poll e presenza
RLIMIT_FSIZE. Exit 0. Nessun modulo preparato caricato tramite import/exec.
Il calcolo analitico duplica solo l'aritmetica dei modelli, non esegue engine,
runner o gli helper preparati. Nessuna prova di runtime viene dedotta da AST.

Nuove prove preparate: constructor failure, errore post-spawn/pre-token, timeout,
albero nativo dummy OWN, orfano, identità riusata, stato non interrompibile,
close/pubblicazione falliti, eredità RLIMIT_FSIZE e conteggi/budget. I dummy usano
solo Python e `/usr/bin/sleep`; le future prove OWN restano nel namespace batch.
Venti mock originali invariati, 17 nuovi metodi: **37 preparati, zero eseguiti**.

Hash e conteggi righe di tutte le fonti in `preparation-002.json`:
controller 595, budget 251, supervisor 264, helper 23, dummy 23,
test originali 165 e nuovi 228. Piano 147 righe; manifest 92. Modello runtime
dichiarato, nessun identificativo/modello di revisore inventato.

Mancano review di questa versione, primo mock run e qualificazione runtime,
audit dei bound contro i subprotocolli congelati e integrazione finale di
ammissioni/adozione ledger. Non esiste CLI di esecuzione autonoma. Whole-batch
non è PASS finché tutte le 66 tuple hanno quattro fasi e review indipendenti.
Native-only rimane riferimento condizionato, non migliore CPU generale; la
decisione whole-M1 usa i tre comparandi separati. Nessuna statistica batch prodotta.

Primo comando **MOCK autorizzabile dopo review**, non eseguito ora:

```bash
/usr/bin/python3 -I -B -m unittest discover -s tmp/m1/e-vulkan-batch -p 'test_*.py' -v
```

Non un altro consolidamento documentale e non un'ammissione a candidati. Solo
dopo tale gate e nuovo turno esclusivo si potranno considerare dati batch,
senza retry/fallback o riuso dei campioni precedenti. M1.e/M1 restano aperte.
