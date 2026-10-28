# M1: follow-up PREP del gate operativo

Consegna stabile: `READY_FOR_REVIEW_NOT_RUNTIME_PASS`. Correzione preparata del MED
segnalato nella review: i gate della prima versione risiedevano soltanto nella CLI,
mentre helper operativi come `remove_copies` potevano essere chiamati direttamente.
La review indipendente del nuovo testo resta necessaria; questo record non la sostituisce.
Nessuna fase reale, import del candidato/unit test, suite, compilazione, benchmark,
rete, Git, delega o indice eseguito. Byte di payload fixture letti: **0**.
Nessun contenuto dei namespace attivi viewer/D/F/batch è stato letto o modificato.

## Preservazione prima della correzione

Prima di modificare la nuova versione è stato creato, verificato e sincronizzato
`tmp/m1/fixture-store/history/prep-001/`: 14 copie, 691 819 byte di sorgenti/JSON/testi.
Comprende tutti i 13 file con SHA elencati nella PREP primaria, piano compreso,
più il report primario da 245 righe. Il report storage attestato è fra quei 13 file.
Non esistevano ricevute di fasi reali nello store: non ne è stata inventata una.
La nuova `snapshot-receipt.json` registra origine, dimensioni, righe, id e SHA delle copie.
File snapshot 0444, directory 0555, fsync dei file/directory e della ricevuta prima
dell'avvio della copia di lavoro v2. Questi permessi e hash non sono un immutable flag OS.

SHA-256 snapshot receipt:
`6f53479ce723b07c15e8a4a9d7977b5d2d9550b4356de820f0a4edec9f837280`.
I 13 SHA live e il report primario restano identici. Il report conserva SHA
`8ae68f86021fdd9e1e596e50f5c31407eea3d496dcff102f85634e9f21754d23`.
La PREP primaria, il piano da 57 righe, i vecchi sorgenti/test/static checker e
la mappa da 83 righe non sono stati riscritti. V1 è storico non qualificato per
operazioni reali: il suo comando archive resta una proposta storica, non un via libera.

## Gate negli helper v2

Nuovi file esclusivamente in `tmp/m1/fixture-store/v2/`, oltre a questo record.
`store.py` non importa la versione precedente. Conserva gli otto pin scientifici,
il codice di allowlist/closure e i controlli NOFOLLOW, tipo, nlink, UID, SHA e byte.

`admitted_phase` è l'unico punto che emette un `PhaseContext`. Prima dell'emissione:

1. Verifica SHA del JSON esterno, schema 2, fase/run-id/tool SHA e review della preparazione.
2. Autentica fonti stabili, manifest chiuso 168 e allowlist esatta dei path della fase.
3. Esegue preflight finito e acquisisce LOCK_EX non bloccante sulla lease persistente.
4. Verifica i binding archive/verify/review e, per release, materializzazione e risultati riusciti.
5. Verifica la LOCK_EX già detenuta, poi registra il contesto scoped nel processo corrente.

La sola creazione bootstrap di `readers.lock` precede il contesto, necessariamente:
avviene soltanto in archive, dopo ammissione pinned, review, path chiusi e preflight.
Non crea work, stage o ZIP. Le altre fasi aprono una lease già esistente senza crearla.

Dieci ingressi operativi iniziano con `need_context`: remove_copies, phase_archive,
phase_verify, phase_materialize, check_results, start_work, phase_retire,
phase_release, perform_phase e authorize_write. Il costruttore pubblico rifiuta
la costruzione diretta. Un contesto non emesso, scaduto, di un altro processo/fase
o con work/files/rows/path/receipt/admission diversi viene rifiutato prima dell'operazione.
I binding sono copiati e sigillati con hash del contenuto; mutare gli argomenti del
chiamante non amplia il contesto. All'uscita dallo scope il registro viene revocato
prima di chiudere il fd. Un contesto con lease chiusa, sostituita o non più esclusiva
non viene riattivato automaticamente.

Il controllo della lease usa fstat, il pathname NOFOLLOW e soltanto i metadati Linux
del proprio `/proc/<pid>/fdinfo/<fd>`. Richiede una voce FLOCK ADVISORY WRITE del
proprio PID/inode, senza tentare di riacquisire una lock persa. Non usa F_GETLK come
prova di flock. L'effettivo comportamento fdinfo rimane da qualificare a runtime.

Le scritture operative passano callback che ricontrollano contesto e path esatto
prima di O_EXCL/write/fsync; il ciclo write-all ripete il gate prima di ogni write.
L'unico unlink resta in remove_copies, con nuovo gate immediatamente prima dell'unlink,
id/path/SHA ricontrollati e intent durevole. La allowlist di scrittura è chiusa per fase:
ricevute nominate, ZIP soltanto in archive, due stage soltanto in materialize,
intent di unlink enumerati soltanto in retire/release. Nessuna discovery per cancellare file.

Retire richiede archive PASS e FULL verify PASS di 168 membri e 847 288 776 byte
confrontati con gli originali, audit indipendente pinned e un'ammissione distinta.
Prima di remove_copies, phase_retire riautentica l'archivio; il gate rifiuta il
remove diretto anche con contesto se quel controllo non è attivo.
Release può selezionare solo le copie nuove della materializzazione PASS compatibile.
Prima del remove richiede risultati riusciti, child reaped, reader chiusi, review
del coordinatore, SHA degli artefatti e fsync dei file/directory. Scrive inoltre
`results-durable.json` prima di impostare il gate di rilascio. Non seleziona prove fallite,
log, originali utente, sorgenti, manifest o l'archivio autorevole persistente.

I primitivi minuti new_file/write_all/save, il sink bounded e la validazione dei byte
restano separati dagli ingressi operativi: servono anche a test/prove piccoli e non
costituiscono un'API di migrazione. `materialization_inputs` è un primitivo di sola
lettura che conserva l'ordine archivio-prima/controllo-target. Il contesto protegge
da chiamate operative accidentali, non da codice Python ostile con lo stesso UID,
introspezione, monkey-patching, modifica del registro o uso diretto di os.unlink.
Il pin esterno è ancora la radice di fiducia; le stringhe author/reviewer non autenticano persone.

## Regressioni preparate e primo budget piccolo

Restano tutti gli 11 nomi/test originari, senza skip. Dieci corpi test sono identici
via AST. Il caso unexpected-target/archive-first usa ora il primitivo readonly reale,
conservando lo stesso target inatteso, l'ordine verificato e il confronto dei byte;
non costruisce un falso contesto per aggirare il gate nuovo. I mock originari restano.

Nuova classe senza mock: 13 regressioni, 24 test complessivi. Chiama direttamente
remove, archive, verify, materialize, retire, release, creazione work, dispatcher,
sync risultati e authorizer senza contesto. Verifica l'eccezione specifica del gate
e confronta bytes/id/size/mode/UID/GID/nlink/mtime/ctime del file minuto e i nomi
del namespace prima/dopo. Copre inoltre costruttore diretto, contesto non emesso
e bootstrap senza ammissione. Nessun successo viene simulato come esito della suite.
Test eseguiti durante questo follow-up: **0**. Le prove dinamiche di scadenza reale,
fase errata e path errato con un contesto legittimamente emesso restano da qualificare;
non si inventano ammissioni reali per farle passare durante PREP.

Il nuovo `run_small_suite.py` richiede una propria distinta ammissione esterna
schema 2, scope `fixture-store-small-suite`, run-id, `tests_expected: 24`,
`reservation_bytes: 8388608`, turno globale esclusivo, coordinatore e review PASS.
`source_hashes` deve coprire esattamente store.py, test_store.py e run_small_suite.py
con i nuovi SHA. Solo dopo questi controlli importa candidato e unit test.
Prenota **8 MiB**, inclusi 4 MiB metadati/prove e 4 MiB dati minuti/arrotondamenti,
oltre al margine globale 16 MiB. Non prenota 128 MiB per ogni unit test.
Log limitato a 1 MiB, intent/ricevuta con SHA del log e conteggi effettivi, fsync,
nessun cleanup o retry anche in errore. Una suite riuscita non ammette nessuna fase dello store.

Restano invariati: ZIP massimo 128 MiB, metadati 4 MiB, margine 16 MiB, cap globale
1 GiB, buffer esplicito 256 KiB, guard RSS best effort e RLIMIT_AS 512 MiB.
SHA completo e confronto byte per byte non diventano CRC-only. Restano 168 fixture,
847 288 776 byte logici e 66 chiusure di due payload interi, massimo 21 041 152 byte.
Ogni fase reale richiede schema 2 e `paths` ordinati esattamente come il proprio scope;
le ricevute precedenti devono essere schema 2 del medesimo tool e le ammissioni distinte.
Nessuna conversione automatica delle ricevute v1 o fallback a loose/rigenerazione.

La fotografia metadata prima dello snapshot era 942 428 160 byte allocati in tmp/.
Un archive con prenotazione massima 128 MiB + 4 MiB + 16 MiB non entrerebbe in
quella fotografia. Non è un motivo per alzare il cap o cancellare prima le sorgenti.
Lo spazio reale dello ZIP resta ignoto; un futuro bound primario più piccolo deve
essere scelto esplicitamente prima dell'esecuzione, senza adattarlo dopo un errore.
La suite minuta ha un costo separato. Tutti i preflight devono comunque essere rifatti
quando il coordinatore assegna il turno, dopo la chiusura dello slot attivo.
Lease e st_blocks sono guard cooperative, non una quota OS o una prova contro writer
esterni/file scollegati ancora aperti. I costi di contesto/hash/extraction/proof sono
preparazione e cache-warming, mai tempi scientifici trasferiti alle vecchie serie.

## Verifica statica effettiva e hash

Comando effettivo, exit 0, senza import dei moduli preparati:

```bash
PYTHONDONTWRITEBYTECODE=1 OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S tmp/m1/fixture-store/v2/static_check.py
```

La ricevuta statica è `tmp/m1/fixture-store/v2/preparation-receipt.json`, nuova,
O_EXCL tramite redirezione noclobber, poi 0444 e fsync file/directory. Registra 33
letture integrali di sorgenti/JSON/testi: 14 origin/live, 14 snapshot, snapshot receipt
e quattro nuovi sorgenti. Non apre fixture o ZIP. Oltre a queste letture sono stati
letti AGENTS 52 righe e progress corrente 51 righe, senza modificarli.
Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`, Linux, UID 1000.
Modello self-report: `openai/gpt-6.1-sol`; costi/token non esposti.

Il checker confronta i 13 SHA e il report primario, verifica i permessi snapshot,
AST degli ingressi/context issuer/unlink, conservazione di guard/closure e dei test
originari. Riconta 168 path manifestati, 42 case e 66 chiusure dai JSON già attestati.
Sono verifiche strutturali/sintattiche, non una prova di sicurezza o di esecuzione riuscita.

| Nuovo file | Righe | SHA-256 |
| --- | ---: | --- |
| `v2/store.py` | 881 | `48cd729268eb185f1dfcba163fc8b6e0937d1e7a2a845573e41102fc0fcc1ce0` |
| `v2/test_store.py` | 208 | `30ff6ac9d62af94238dcd3ebe5e4e9ef764c661cf2dffb639a1ee6c8824494b7` |
| `v2/run_small_suite.py` | 124 | `bb112640d452fb7fac1754c4db2221ad8104fa4fcf6a627adaaa3959a208d33e` |
| `v2/static_check.py` | 140 | `d4a8cf1628a7c2906c172920642755ddf82c73ece226f4731b0a53d00f400eed` |

Prefisso dei file in tabella: `tmp/m1/fixture-store/`.
SHA preparation receipt: `4859708afa18c031e51afebcf6fd1a11cb3f435b36d826ba689bc32b5b389146`.
Il registro dello snapshot contiene tutti gli SHA primari; non vengono sostituiti da nuovi valori.

## Handoff e comandi futuri, non ammessi da PREP

Prossimo passo: review indipendente sul nuovo testo effettivo e sulla ricevuta statica.
Soltanto dopo review e distinta ammissione della suite minuta:

```bash
PYTHONDONTWRITEBYTECODE=1 OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S tmp/m1/fixture-store/v2/run_small_suite.py --run-id small-001 --admission tmp/m1/fixture-store/v2/admit-small-001.json --admission-sha256 "$SMALL_ADMISSION_SHA256"
```

Il file ADMIT e il suo SHA non sono stati prodotti. Dopo suite realmente riuscita,
review dei risultati e ulteriore ammissione archive, il comando futuro sarebbe:

```bash
PYTHONDONTWRITEBYTECODE=1 OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S tmp/m1/fixture-store/v2/store.py --phase archive --run-id archive-001 --admission tmp/m1/fixture-store/v2/admit-archive-001.json --admission-sha256 "$ARCHIVE_ADMISSION_SHA256"
```

Anche questa ammissione è assente. Verify, retire, materialize e release hanno
ciascuna fase/run-id/JSON/SHA distinti. Nessun comando concatenato le avvia.
Il vecchio runner parte soltanto separatamente, con lease condivisa per tutta
la vita dei reader; review e risultati durevoli precedono release. Resta il limite
di riproducibilità dei vecchi comandi con loose assenti, senza restore nascosto.
