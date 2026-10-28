# M1.a: review della catena diagnostica primaria

26 ottobre 2026. Esito della review indipendente `ses_e87418d54ffe0H8Drmu9F93pbP`: **PASS_SCOPED**, senza rilievi residui nel perimetro codice e mock. Il MED sulla perdita del registro diagnostico annidato è chiuso. Non è un'ammissione nativa, prestazionale o distributiva, né una chiusura M1.

Questo verbale registra la review e l'autenticazione separate comunicate dal coordinatore. La redazione ha letto i report effettivi e i metadati di applicazione sotto elencati; non ha rieseguito test né svolto una nuova auto-approvazione del codice. Lo stato corrente e il prossimo gate restano in `docs/progress.md`.

## Prove e copertura della review

Il PASS47 appartiene al suo SHA storico e non copriva il MED annidato. RED001 è FIRSTFAIL_UNEXPECTED: la fixture senza BufferedWriter non raggiungeva la premessa richiesta. RED002 interpone BufferedWriter e riproduce la perdita del registro sul sorgente precedente, con tre write e catena context naturale. Il suo exit1 è il fallimento atteso del test; l'exit0 dell'outer indica soltanto classificazione riuscita.

I metadati apply registrano l'applicazione byte-identica della proposta revisionata, con zero GREEN a quel momento. Il GREEN successivo verifica il fix sul nuovo SHA: otto target PASS, poi suite55 PASS, entrambi exit0 senza skip o retry. Sono 63 esecuzioni, ma 55 metodi unici: 22 ProtocolTests, 16 LifecycleTests, 17 CombinedFailureTests.

La review indipendente ha esaminato questi controlli effettivi in `test_protocol.py`:

- Righe585-665: TextIOWrapper/BufferedWriter, tre write e catena naturale. Primaria e `receipt_errors` restano esattamente identici al registro precedente. Il teardown contiene l'originale invariato e precisamente due wrapper distinti; confrontati tipo, fase, ruolo, messaggi e catene context di uno e due elementi. La fase è `finalization.exception-wrapper`, il ruolo `wrapper`.
- Righe675-790: visita di cause e context, registri condivisi senza duplicazioni, registri distinti con priorità e `additional-primary`, cicli e limite64 con `truncated` esplicito. Nessun registro non visitato viene inventato. FIRSTFAIL non persistibile conserva le diagnosi, termina con exit2 e stderr bounded, bloccando nuovi lanci.
- Righe804-815: loader senza omissioni dei metodi. Il confronto esatto include `chain_scan={limit:64,truncated:false,cycle:false}`, senza escludere campi originali.

I grafi artificiali riguardano i test puri di attraversamento; la regressione IO e il test di persistenza fallita usano cause/context naturali. Gli oggetti JSON prima/dopo verificano il contenuto serializzato, non soltanto un attributo dell'eccezione.

## Autenticazione separata e identità

Il coordinatore comunica 22 file autenticati in sola lettura, inclusi i dodici pin sorgente del manifest e report, sorgenti, diff, outer, risultati e raw pertinenti. Ha usato apertura relativa a FD con `O_NOFOLLOW` su tutti i componenti, file regolari posseduti, `nlink=1`, massimo1 MiB, `fstat` stabile e SHA-256. Questo verbale non estende il controllo a tutti gli storici.

Il controllo del coordinatore conferma conteggio AST22+16+17, otto `ok` target e 55 `ok` unici nella suite, con target sottoinsieme della suite. Due receipt riportano exit0, reap riuscito, nessuna riserva o segnale. Gli stdout JSON confermano primaria e ricevuta invariate, teardown originale più esattamente due wrapper con le catene richieste e `chain_scan` completo.

Il primo parser read-only dei log del coordinatore non gestiva la docstring unittest sulla seconda riga e ha prodotto un'asserzione del verificatore. Non era un FAIL dei test. Corretto soltanto quel parser, la verifica è passata senza rieseguire test o prodotto.

| Identità | SHA-256 |
| --- | --- |
| `benchmark.py` applicato e testato | `3dbd3fe291b8fa1641df46ba3be5235f4d142b4ff93a4a1351ed9a7f05ba2829` |
| `test_protocol.py` GREEN | `36df80306fc70236043da7105fc48a50a2c92fb6033e30bdac3a2a522cf83c6e` |
| `manifest.json` durante il runtime | `d150123f76c8123f40bc0954e10807e95edef0a27e5d106e8480cf96c7573874` |
| `manifest.json` finale | `f74bf0c7ac780aed6b9b1bf003c656d975a0a0023c5858aad0e8030c770e0858` |

Il manifest finale differisce da quello runtime solo per l'esito registrato dopo il turno. I pin prima/dopo runtime restano distinti dall'aggiornamento finale; non si attribuisce al runtime un manifest scritto successivamente.

## Comandi, ambiente e limiti

Comando GREEN registrato nel report primario, non rieseguito da questa redazione:

```sh
python3 -I -B -S tmp/m1/fft-pocket-benchmark/primary-error-chain-green-tests-001/outer.py
```

Il report conserva i comandi figli esatti e le ricevute in `primary-error-chain-green-001/target/` e `suite/`. Ambiente dichiarato dall'esecutore: Linux, Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`. I cap sono quelli del turno mock; non una riserva nativa.

Le ricevute attestano PID1271679 e1271685 raccolti senza segnali, non una nuova osservazione OS del revisore o di questa redazione. I rami timeout TERM/KILL non sono stati esercitati. I tempi registrati riguardano test Python, non prestazioni FFT.

Gli esecutori attestano 238 file protetti e nove FIRSTFAIL invariati; il coordinatore non li ha riautenticati tutti indipendentemente. Storici38/47 e RED conservano SHA ed esiti propri, senza PASS retroattivo. Il GREEN non crea un FIRSTFAIL nativo. L'audit coordinamento001 resta immutato: checkpoint storico invalido come ammissione, mandato ambiguo rettificato e limiti della ricostruzione storica conservati.

OS/Popen del benchmark e writer delle ricevute sono mock. Il PASS non certifica durabilità filesystem, teardown nativo, inventario del nuovo ELF, licenze, scienza192 o misure. Nessuna compilazione del nuovo track è ancora attestata. La disponibilità corrente dello scratch richiede un preflight separato; le occupazioni storiche non lo sostituiscono.

## Fonti lette e verifica documentale

Report sotto `docs/evidence/`; metadati sotto `tmp/m1/fft-pocket-benchmark/primary-error-chain-apply-001/`. Hash ricalcolati da questa redazione con `sha256sum`, exit0, dalla radice del progetto:

| Fonte | Righe lette | SHA-256 |
| --- | ---: | --- |
| `M1-a-pocketfft-benchmark-primary-error-tests-001.md` | 146 | `9d960ad7be6127ea4da87cee5cd3b3416caf3b49ad419a0fc3f36ed411442d35` |
| `M1-a-pocketfft-benchmark-primary-error-chain-red-001.md` | 63 | `28fde6527abb37ee2afa30dfde2f6c6a00eb7ccfe9c0d4c6fd85eeb6b051a11c` |
| `M1-a-pocketfft-benchmark-primary-error-chain-red-002.md` | 97 | `713af1d6dea342323aa1156d07f28c3cbe3d2b8f3a79cec61733b4a2f3bd6463` |
| `M1-a-pocketfft-benchmark-primary-error-chain-green-001.md` | 127 | `e861c010a5d1595fe5bef99a0d92edd3389eff8e460c747d6e9563a9800003c1` |
| `receipt-after.json` | 107 | `ad0171daff1f19b6d7ee138b0eb73fdc4e240d40feb77a8c2c45ca0609778117` |
| `hashes.json` | 37 | `b98ce838382ef0a92ced3e2ecd3bf4a11831e335ada539a4ccac188140f46e7d` |

Letti inoltre `docs/progress.md`, righe1-65, e la coda dell'indice, righe155-171: 659 righe complessive in questo aggiornamento. Il piano generale, i payload e il codice non sono stati riletti; la copertura del codice sopra è attribuita alla review indipendente comunicata, non alla redazione. Nessun test, import, build, rete, installazione, indice, Git o delega eseguiti qui. Versione dell'utility hash non rilevata; costi non disponibili.
