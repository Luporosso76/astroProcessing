# M1: PREP dei test sul ciclo di vita dei contesti

Stato: `PREP_ONLY_NOT_RUNTIME_PASS`. Test eseguiti: **0**.
Nessun import del candidato o dei test, processo candidato, fase store, archivio,
payload fixture, rete, Git, build, benchmark o indice. Lo slot F pattern4097 resta
fuori perimetro. Sono stati eseguiti soltanto interpreti per AST, hash e letture di
sorgenti/testi. Questa consegna non ammette l'esecuzione della suite.

## Finding e copertura preparata

Il finding MED riguarda la copertura, non un bypass dimostrato nel codice v2.
I 24 test precedenti fermano contesti assenti o non emessi prima dei gate successivi.
I nuovi 18 metodi unittest partono da `admitted_phase` reale: il contesto compare
nel registro, supera `need_context` con lease reale e viene revocato dallo scope.
Ogni fixture verifica prima che il contesto sia accettato. I rifiuti richiedono
la diagnosi del gate previsto, senza accettare il precedente "not issued".

Copertura prevista, ancora da eseguire:

- Contesto valido con binding e path esatti: controllo positivo senza mutazioni.
- Contesto realmente emesso poi revocato: tutti i dieci ingressi operativi.
- Lease sbloccata e binding alterato: tutti i dieci ingressi, con fase compatibile.
- Fase errata: sei ingressi con fase specifica; PID impossibile: gate processo.
- Lease chiusa, declassata a SH, pathname sostituito, fd sostituito con altro inode,
  oppure stesso inode riaperto con descrizione senza flock.
- Ammissione sostituita con gli stessi byte: controllo identità dopo emissione.
- Tutti i 18 campi `_BOUND`, argomenti diversi, mutazione dei file del chiamante,
  path di scrittura non ammesso o alias, intent non durevole e doppio start.
- Eccezione nello scope: osservatore della chiusura verifica la revoca precedente
  alla chiusura del fd; la chiamata successiva deve fallire come contesto scaduto.

I subtest non aumentano il conteggio: **18 nuovi + 24 invariati = 42 metodi**.
Ogni azione rifiutata confronta il namespace proprio e byte, device, inode, modo,
UID/GID, nlink, size, mtime e ctime dei piccoli file. atime è escluso perché le
letture della prova possono cambiarlo. Le alterazioni intenzionali precedono la
fotografia: il test prova che l'operazione rifiutata non aggiunge altre mutazioni.
Per la lease chiusa, la fotografia precede close e nessun fd viene aperto prima
del gate. L'EBADF della successiva chiusura nello scope è atteso dalla fixture;
non viene scambiato per il rifiuto di `need_context`.

## Prerequisiti sintetici e limiti

Le fixture rilocano ROOT e BASE nei propri run. Sostituiscono PINS con un insieme
vuoto e inventory con un file da 16 byte. Il guard interno restituisce zero perché
il launcher deve prenotare il run completo prima del child. Un adapter SHA limitato
al sorgente tool calcola il suo digest reale fuori dal ROOT rilocato; gli altri
hash restano quelli v2. Admission JSON, schema 2, tool/run/fase, SHA e identità del
JSON sono reali. Le stringhe di review sono esplicitamente sintetiche.

Archive usa `_phase_bindings` reale sul piccolo inventario. Le altre fasi usano
binding di ricevute precedenti sintetici, dichiarati nel test: non attestano archive,
FULL verify, risultati consumer o indipendenza scientifica. Restano reali emissione,
copia dei binding, sigillo, gate successivi, flock, fdinfo, pathname e revoca.
`start_work` reale crea soltanto intent minuti; nessun corpo di fase deve procedere
oltre il rifiuto. I test non provano il percorso completo di ammissione scientifica,
l'unlink su dati reali o l'invalidazione durante un trasferimento già iniziato.
Non è stato dimostrato alcun nuovo bug runtime. Non è stata inventata una prova RED
o GREEN: il mandato PREP vieta entrambe finché lo slot corrente resta attivo.

## Launcher futuro e budget

Il nuovo launcher eseguirà i 42 test in **un solo child Popen proprio**, aspettato
e reaped. `failfast=True` ferma la suite alla prima failure/error, anche nei subtest.
CPU child 60 secondi, wall parent 120 secondi, AS 512 MiB per processo, core disabilitato,
log massimo 1 MiB con RLIMIT_FSIZE nel child. Il parent uccide soltanto il proprio
Popen in timeout o errore. Non cerca PID esterni, non usa process-group kill e non
riprova. Un fallimento conserva intent, log, file e ricevute; exit non nullo.

Prenotazione primaria: **16 777 216 byte**. Derivazione: 128 fixture massime per
12 nodi da 4096 byte = 6 MiB; 24 test precedenti per 8 nodi = 0,75 MiB;
4 MiB metadati/prove, 1 MiB log e 1 MiB cache: 12,75 MiB, arrotondati a 16 MiB.
Le nuove fotografie impongono 12 nodi e 4096 byte per file; il contatore limita
le fixture a 128. L'unità filesystem deve essere al massimo 4096 byte.
Guard globale reale prima del child: allocato tmp + prenotazione + margine 16 MiB
entro 1 GiB, spazio libero e RSS. Il budget non sostituisce il turno esclusivo.

I 24 test vecchi mantengono i byte originali, ma il launcher riloca il loro BASE
nel sottodirectory `legacy` del nuovo run. I nuovi contesti usano directory sorelle
proprie. Cache e temporanei: `cache` e `tmp` dentro lo stesso run sotto ROOT.
Nessuna pulizia, migrazione, restore o modifica ai sorgenti v2.

Ammissione futura: schema 2, status `ADMIT`, scope
`fixture-store-context-lifecycle-tests`, run_id, tests_expected `42`, reservation_bytes
`16777216`, global_writers_exclusive/readers_closed `true`, coordinator e author non vuoti.
`independent_review` deve contenere status `PASS`, reviewer diverso da author e la
stessa mappa source_hashes. La mappa copre esattamente i quattro path sotto indicati:
store v2, test v2, nuovo test e nuovo launcher. I pin v2 sono anche fissi nel launcher.
Nessun file ADMIT è stato creato. Il worker ripete l'autenticazione e compila/importa
gli stessi byte autenticati, senza riaprirli per l'esecuzione. Il report di successo
richiede 42 eseguiti, zero errori/failure/skip e child reaped con exit 0.

## Verifica statica effettiva

Interpreti eseguiti con:
`PYTHONDONTWRITEBYTECODE=1 OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S -`
e heredoc limitati a pathlib, AST, SHA-256, JSON e identificazione del runtime.
Ultima verifica su sorgenti finali: exit **0**. Output finale esatto:

```text
STATIC_ONLY: AST_OK; tests_planned=42; tests_executed=0; preserved_pins=6
```

L'heredoc confronta sei SHA preservati, applica `ast.parse` ai due test e al nuovo
launcher e impone conteggi `[24,18,0]`. Una verifica precedente controlla via AST
la presenza di `failfast=True` e `Popen`. Non importa questi file, non esegue assert
dei test e non prova flock/fdinfo. Python `3.14.4 (main, Aug 20 2026, 10:41:58)
[GCC 15.2.0]`, Linux x86_64. Modello self-report: `openai/gpt-6.1-sol`.
Costi e token non esposti dal runtime.

Letture testuali pertinenti: store v2 881 righe, test v2 208, receipt v2 221,
record follow-up 186, launcher v2 124 e PLANS.md 46. Il checker v2 da 140 righe
è stato letto come byte per hash/AST, non eseguito. Nessun piano lungo o storico letto.
La ricevuta `context-lifecycle-tests/preparation-receipt.json` registra hash e conteggi.

| File | Righe | SHA-256 |
| --- | ---: | --- |
| `tmp/m1/fixture-store/v2/store.py` | 881 | `48cd729268eb185f1dfcba163fc8b6e0937d1e7a2a845573e41102fc0fcc1ce0` |
| `tmp/m1/fixture-store/v2/test_store.py` | 208 | `30ff6ac9d62af94238dcd3ebe5e4e9ef764c661cf2dffb639a1ee6c8824494b7` |
| `tmp/m1/fixture-store/context-lifecycle-tests/test_context_lifecycle.py` | 271 | `4586fa64a2405b27cdc1d54d785ac800e251d8a1d094afbc1135e95b1b531eed` |
| `tmp/m1/fixture-store/context-lifecycle-tests/run_suite.py` | 197 | `9f61563454a979b72dc7a70c42da08c9f50589a72ce9e97df23130648d891028` |

Invariati anche v2/run_small_suite.py, v2/static_check.py, receipt v2 SHA
`4859708afa18c031e51afebcf6fd1a11cb3f435b36d826ba689bc32b5b389146` e record
da 186 righe SHA `2d29db958a6a36ce170f425439bf7e3b502874e532aef0b40f83b80fbdec48db`.
Il nuovo piano è `plans/M1-fixture-context-tests.md`; nessun vecchio piano è stato modificato.

## Handoff

Prima azione minima: review indipendente, in sola lettura, sui nuovi file effettivi.
Dopo review, chiusura dello slot corrente e distinta ammissione della suite, comando
futuro, **non eseguito e non autorizzato da questa PREP**:

```bash
PYTHONDONTWRITEBYTECODE=1 OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S tmp/m1/fixture-store/context-lifecycle-tests/run_suite.py --run-id context-001 --admission tmp/m1/fixture-store/context-lifecycle-tests/admit-context-001.json --admission-sha256 "$CONTEXT_ADMISSION_SHA256"
```

Un eventuale PASS della suite non ammette alcuna fase archive/verify/retire/materialize/release.
Se fallisce, registrare la prima failure e distinguere bug del test, ipotesi Linux e
bug v2. Non cambiare pin, togliere test, simulare lock o indebolire gate per farla passare.
