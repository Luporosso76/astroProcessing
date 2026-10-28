# M1.a: review della proposta di ritiro Rust

27 ottobre 2026. Review indipendente `ses_e87418d54ffe0H8Drmu9F93pbP`: **PASS_SCOPEDCODE_MOCKS**. Chiusi i due MED nel codice e nei mock; il ramo operativo resta bloccato. Questo esito non è READY_UNLINK, non autorizza la creazione di rust002 e non ammette esecuzioni native.

Il verbale registra la review sui file effettivi e i controlli separati comunicati dal coordinatore. La redazione ha letto il sorgente stabile, il followup, il report mock e la ricevuta; non ha ripetuto test, scansioni OS o review indipendente del codice. Lo stato corrente ha casa in `docs/progress.md`; i dettagli operativi restano nella proposta `tmp/m1/a/retire-series001-rust-001/`.

## Correzioni e copertura

Il primo MED lasciava UNKNOWN non bloccante. Il fix richiede classificazione risolta in entrambe le scansioni; ENOENT è distinto da EACCES verificando la scomparsa del processo. Il secondo MED lasciava controlli operativi fuori dal percorso STOP. Ora il marker single-use precede i controlli e il percorso protetto conserva il primario anche quando falliscono close o scrittura della ricevuta. Un errore di persistenza non diventa durabilità dimostrata.

La review statica del delta aveva concluso READY_CODE_PENDING_MOCK_TESTS, non READY_UNLINK. Il successivo turno mock copre otto metodi e17 scenari, non17 metodi:

- UNKNOWN alla prima e seconda scansione, processo scomparso contro processo ancora presente o illeggibile.
- Cinque errori dopo il marker, STOP simulato e rifiuto della seconda invocazione.
- Sei errori write/fsync/close su marker e preflight; errore primario combinato con close e STOP falliti.
- Ordine dei fsync prima dei controlli e dell'unico unlink simulato; sentinella sul vero `os.unlink`, zero chiamate.

Le dipendenze sono state sostituite prima dell'import e l'entrypoint escluso. Nessuna autorizzazione reale, apertura del target Rust, cancellazione o esecuzione nativa. Non è una prova di durabilità filesystem o sicurezza operativa del ritiro.

## Esecuzione e limite dell'outer

Comando registrato nel report primario, non rieseguito qui:

```text
python3 -I -B tmp/m1/a/retire-series001-rust-mock-001/outer.py
```

Unica esecuzione failfast, uscita naturale0 senza retry. Otto metodi unici terminano con PASS; raw con `Ran 8 tests` e `OK`, stdout vuoto. Ambiente dichiarato: Python3.14.4, Linux7.0.0-38-generic x86_64. Limiti: CPU50 s, wall55 s, AS256 MiB, FSIZE1 MiB, NOFILE64, CORE0, output aggregato1 MiB, ROOT0700 e ambiente privato.

La ricevuta identifica PID3373426, gruppo/sessione propri e start_ticks120790305; registra WNOWAIT, reap riuscito, returncode0 e violation null. Sono fatti delle ricevute, non una nuova osservazione OS del revisore o della redazione.

L'outer è nuovo, non un launcher già qualificato. La review delle righe83-113 rileva che il cleanup non è garantito per ogni eccezione dopo Popen e che timeout/overflow inviano KILL diretto. Quei percorsi non sono stati esercitati. Il limite non invalida gli otto PASS osservati, ma impedisce di qualificare l'outer per operazioni future. Nessun rerun o qualifica aggiuntiva viene autorizzato da questo verbale.

## Autenticazione e conservazione

Prima dei test il coordinatore aveva verificato indipendentemente cinque fonti con apertura relativa a FD, NOFOLLOW, file regolari posseduti, nlink1, limite dimensionale, fstat stabile e NOATIME. Dopo il turno ha autenticato otto file: sei contro gli hash di consegna (report, test, outer, receipt e due snapshot), più i due raw con SHA calcolati, non confrontati con hash esterni attesi.

Il controllo AST conferma otto metodi corrispondenti agli otto esiti unici nel log. I due snapshot dei cinque pin sono identici byte per byte. Le cinque fonti non sono state tutte riautenticate indipendentemente dopo il test: l'invarianza post-run è documentata dalle ricevute.

| Fonte | SHA-256 |
| --- | --- |
| `retire.py` corrente | `47bd28db171fc8ba30a38be29f0da76437e855d95c1a2178128d46da2f124f60` |
| Piano proposta, invariato secondo handoff | `a633022925b4af79d1d39fbb7549c6fea3a62c1a8672077d78675691fbd884dd` |
| Report mock | `6f47bd5a4da229e833913177bbb0e923619eb576f1f589caea34cc5a2440f764` |
| `test_mocks.py` | `0d79b1dcf292b79bee49b4ad2333ae2fffa34baf734a5e48d528de43fc45a29a` |
| `outer.py` | `303cc431b994ddf9cf26d249e2626d21177481ae4e3631087798e6edded75ab9` |
| `receipt.json` | `695500245f35ead0961b7138a7a1ea42a9c38d7a834157650c260a051d6c5cae` |
| `pins-pre.json` e `pins-post.json` | `ed117fa6719d6efe89848f1973b63ee7d3261e9fb9353f5c40a035b09bee1f51` |
| `suite.stderr`,882 byte | `c92fa9bf0564bd5e49b9649a1ea4995aea903f2f398c8817cf564e7cee58ca17` |
| `suite.stdout`,0 byte | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |

Rust, NumPy, fixture, originali e nove FIRSTFAIL restano conservati secondo il handoff. Nessuna provenienza o qualifica aggiuntiva si deduce dal solo PASS mock.

## Blocco operativo

La scansione read-only del coordinatore del27 ottobre ha osservato quattro processi UID1000 in stato S con cwd/exe EACCES. I metadati start erano stabili prima/dopo quella scansione:

| PID | start_ticks |
| ---: | ---: |
| 10627 | 2623858 |
| 10653 | 2623860 |
| 11226 | 2623931 |
| 1593539 | 16449300 |

Questo snapshot non è un controllo fresh per unlink e non dimostra identità storica con i PID della proposta, che non registrava starttime. Il permesso reale di ispezione manca; l'assenza di consumer non è dimostrata. Nessuna whitelist, modifica di privilegi/configurazione, ritiro alternativo o aumento dei cap risolve il gate. Il blocco riguarda il ramo operativo Rust, non tutti i rami indipendenti del progetto.

L'occupazione936648704 byte appartiene al preflight mock storico. Non rappresenta capienza corrente, prenotazione o ammissione futura. Nessun nuovo benchmark o build FFT eseguito; M1 resta aperta, senza stack adottato.

## Verifica di questa redazione

Letti `docs/progress.md` (66 righe), coda indice166-176 (11), report mock (33), `followup-001/REPORT.md` (63), `retire.py` (376) e `receipt.json` (29):578 righe. La redazione non ha letto il namespace M1.f in scrittura. Con `sha256sum`, exit0 dalla radice del progetto, ha ricalcolato gli hash di report mock, retire.py, receipt e followup; quest'ultimo è `1a748f1bb7f83d5389fca3af0242bc3d1a4b69d9c974322a9eaafeeba5a87b62`. Gli altri hash della tabella sono attribuiti alle verifiche del coordinatore, non ricalcolati qui. Nessun import, test, runtime candidato, Git, rete, installazione, indice o delega; nessuna modifica ai primari. Versione utility e costi non rilevati.
