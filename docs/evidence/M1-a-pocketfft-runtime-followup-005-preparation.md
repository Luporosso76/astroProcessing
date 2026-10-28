# M1.a pocketfft runtime followup 005: diagnosi e sola preparazione

Stato: `PREPARED_NOT_EXECUTED_REVIEW_REQUIRED`. Questa consegna non dichiara PASS dei test né chiude MED003. Sono consentiti soltanto lettura dei file salvati, AST e hash. Test, import dei moduli preparati e verifica dei runtime host restano non eseguiti.

## Evidenza del guasto 004

Ho letto integralmente `runtime-inventory-followup-004/tests-attempt-001/stdout.raw` (173 righe), `stderr.raw` (0 righe), `receipt.json` (477 righe) e `completion-summary.json` (57 righe). Il report test 004 ha SHA256 `72bc1f91342293f7b920a9ed11fbb86477a07e8206735ee488c40174e0ad4974`.

Lo stdout originale contiene 21 risultati: 12 ok, 8 failure e 1 errore; non contiene skip. L'errore del positivo risale a `verify.py:151`, con `VerificationError: version header grammar`. Gli otto negativi falliti ricevono questa diagnosi anziché quella del proprio confronto. Non sono otto guasti indipendenti già diagnosticati o test di comportamento riusciti. La ricevuta conserva exit 1, `child_collected=true`, `slot_free=true`, PID 955456. Questi dati appartengono al turno precedente; 005 non ha creato processi di test.

Pin storici verificati dal comando di preparazione:

| Artefatto 004 | SHA256 |
| --- | --- |
| `preparation-manifest.json` | `e77bc98e296a8d411d798f02591004dc2dee9935d175a2f5e1cb647d87ff7c34` |
| `preparation-checks.json` | `39d9e46cc34fe804606df118bc420634348f2da987d0c11b6359378c81bd6987` |
| `tests-attempt-001/stdout.raw` | `e50035dabce7b215a837134d15db16c4c16e1fe74d4644fe80f6373bb614e2bf` |
| `tests-attempt-001/stderr.raw` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `tests-attempt-001/receipt.json` | `0fc86809882ccc91c6f6fd7881387207640445c23bb228a32e887f8fa5a19e27` |

La ricevuta `preparation-checks.json` di 005 è la prova dell'effettivo confronto, non un risultato della suite.

## Causa provata da sorgente e raw pinned

Il parser 004, `verify.py:150`, richiede letteralmente:

```python
match = re.fullmatch(r"Version " + kind + r" section '[^']+' contains (\d+) entries:", header)
```

Il raw autentico `runtime-inventory-003/elf-libc.so.6.stdout:984-990` contiene:

```text
Version needs section '.gnu.version_r' contains 1 entry:
 Addr: 0x0000000000026ac8  Offset: 0x00026ac8  Link: 5 (.dynstr)
  000000: Version: 1  File: ld-linux-x86-64.so.2  Cnt: 4
  0x0010:   Name: GLIBC_2.2.5  Flags: none  Version: 50
  0x0020:   Name: GLIBC_2.3  Flags: none  Version: 49
  0x0030:   Name: GLIBC_2.35  Flags: none  Version: 48
  0x0040:   Name: GLIBC_PRIVATE  Flags: none  Version: 47
```

SHA256 del raw: `134d200cb5bee873be6f613caabd3a71844a9800ff61913f1e1d8e377b15c07d`, 93619 byte. Il pin deriva dall'artifact manifest 003 autenticato. `entry:` non può corrispondere al suffisso letterale `entries:`. La riga Addr è presente. Questo identifica un errore del parser rispetto all'osservazione salvata, non una corruzione da correggere nel raw.

L'ordine `LABELS` è worker, loader, libc, libm, libgcc, libstdc++. I raw worker e loader usano rispettivamente `4 entries` e `7 entries`; libc è il primo oggetto con la forma singolare. Il traceback di 004 non stampa il label, ma questo ordine e i testi salvati spiegano staticamente la prima diagnosi. Non ho eseguito il parser per ricostruirla.

Ho letto i raw worker, loader, libgcc e libm, oltre alle sezioni version needs/definitions di libc e libstdc++, e confrontato gli header Addr salvati di tutti e sei gli oggetti. I plurali degli altri header restano legittimi. Non ho interrogato readelf o il runtime corrente.

Esiste anche una fixture sintetica malformata: `test_verify.py:109` di 004 usa `contains 1 entries:`. Il test sui confini delle note risultava ok perché parser e fixture condividevano il plurale errato. Non dimostrava la compatibilità con il singolare autentico. Nel solo clone 005 la fixture usa `contains 1 entry:`. La sezione, la nota estranea e l'asserzione `['MOCK_BASE']` restano identiche.

## Delta minimo e controlli conservati

Tutti i file nuovi stanno in `tmp/m1/fft-pocket/runtime-inventory-followup-005/`, salvo questo report. Nessuna scrittura in 003 o 004. `preservation-before.json` registra 16 file: tutti i 12 file di 004, i suoi due report e i due FIRSTFAIL originali/snapshot nominati. Il comando di preparazione verifica che gli hash siano identici e che l'insieme dei file 004 non sia cambiato. Confronta inoltre i sei record sorgente del manifest 004 con gli originali.

Nel verificatore cambia soltanto l'AST di `parse_elf`, nel controllo dell'header versioni. Il match richiede tutta la riga, il conteggio numerico e il sostantivo esatto: `entry` soltanto con 1, `entries` altrimenti. Richiede inoltre tutta la riga Addr esadecimale, Offset esadecimale e Link numerico a `(.dynstr)`, anziché accettare qualsiasi riga che inizi con `Addr: `. Una riga vuota o mancante non salta il controllo. Non viene introdotto un parser alternativo.

Restano intatti `load_inputs`, i sei pin ancora validi, i confronti raw con l'artifact manifest, i contatori di sezioni/gruppi/figli e le verifiche dei duplicati. `exact_records` conserva molteplicità e campi. Restano intatti `verify_tables`, `parse_md5`, `verify_checksums`, i cinque percorsi canonici e il calcolo MD5 sui byte effettivi. Il main futuro continua a verificare SHA256/stat prima dei checksum. Nessuna lettura host è stata anticipata per preparare 005.

`source-delta.json` registra il diff del verificatore e i nomi dei test originali/aggiunti. Il controllo AST verifica che il modulo dei test originale, tolte le sole aggiunte e corretta la costante della fixture, sia identico. Verifica anche i byte sorgente di ogni metodo originale. Venti test originali restano byte-identici; il ventunesimo cambia soltanto `1 entries:` in `1 entry:`, differenza di -2 byte. Nessun test è cancellato, disabilitato o reso meno esigente.

## Test preparati, non eseguiti

Totale: 43 definizioni, 5 positive e 38 negative. Restano i 21 test originali, con 3 positivi e 18 negativi. Le 22 aggiunte comprendono due positivi: la sezione autentica libc con singolare e quattro requisiti esatti, e il plurale autentico worker confrontato con la tabella pinned.

| Nuovi negativi | Conteggio |
| --- | ---: |
| Arco aggiunto coerente nei campi ma assente dal raw worker | 1 |
| Record provider aggiunto per worker, assente dai requisiti raw | 1 |
| Provider duplicato aggiunto o sostituito a conteggio invariato | 2 |
| Requisito duplicato nelle due tabelle derivate | 1 |
| Provider raw duplicato o indice requisito raw duplicato | 2 |
| Titolo versioni malformato: singolare/plurale errati, testo extra, conteggio non numerico, parola arbitraria | 5 |
| Riga Addr malformata o mancante: testo extra, non esadecimale, sezione Link errata, omissione | 4 |
| Riga ELF malformata: dynamic, offset versione, figlio need, parent definition | 4 |
| Totale nuovi negativi | 20 |

I nuovi helper di mutazione richiedono un'unica occorrenza del bersaglio prima di modificarlo in memoria. Le attese storiche di 6 oggetti, 13 archi, 13 gruppi provider, 54 requisiti e 5 checksum restano attese preregistrate. Non sono risultati del codice 005. I test MD5 usano byte sintetici dichiarati.

## Verifica ammessa e artefatti

Comando eseguito soltanto per AST/hash:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-inventory-followup-005/prepare_static.py
```

Il comando non importa i moduli preparati. Autentica sei ancore e undici ulteriori input 003, confronta la lista completa con i 17 record del manifest 004, analizza AST dei tre script 005 e conta le 43 definizioni dei test. Registra sorgenti, byte, righe e SHA256 nel nuovo manifest. I conteggi di righe indicano file hashati, non letture semantiche integrali dell'agente.

`preparation-checks.json` registra l'esito effettivo `PASS_AST_AND_NAMED_LOCAL_HASHES_ONLY`, il pin del manifest e di `source-delta.json`, e i conteggi zero per test, ricostruzioni ELF, letture runtime host, import/esecuzioni del candidato, metadata tool, build e FFT. Il report non viene usato come sostituto della ricevuta. Nessun PASS funzionale segue dall'AST.

Modello dichiarato dal runtime: `openai/gpt-6.1-sol`, senza override. Costi, token e dettagli di eventuale failover non sono esposti. L'ambiente di lavoro è Linux, root `/opt/workspace/astroProcessor`, Python isolato con `-I -B -S`. Non è stata interrogata una nuova versione di tool o librerie host; le versioni nel report test 004 restano osservazioni storiche.

## Handoff e limiti aperti

La consegna è stabile per una nuova review indipendente del testo effettivo, non per auto-approvazione. Nessun test, verify host, worker, loader, ldd, build FFT, rete, Git, installazione, indice o delega in questo turno. Progress e index restano invariati. I comandi futuri e i budget sono soltanto preregistrati; il coordinatore deve assegnare il turno dopo la review. Un fallimento futuro richiede STOP, conservazione dei raw e nuova diagnosi, non autofix/retry.

MED003 resta aperto fino alla review e alle prove richieste. La presenza di version nodes non prova simboli, relocation, IFUNC o binding live; la cache salvata non prova la scelta attuale del loader. MD5 contro il database locale non è una firma o un nullaosta di licenza. Nessuna conclusione numerica FFT o adozione deriva da questa preparazione.

Il gap stderr001 persiste: `runtime-inventory-003/verification-attempt-001-failure.json` dichiara `tool output transcribed, not original stderr file`. Non ho ricreato il primo tentativo né inventato stderr. I FIRSTFAIL storici restano integralmente conservati.
