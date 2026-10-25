# M1.f: primo contenuto tiny, freeze005

Esito: **due producer, quattro clean chunk1 e due confronti completati**, tutti exit0. Quattro risultati uguali ai GT letterali indipendenti dal candidato. Nessun retry, fallback o FIRSTFAIL nuovo. **Non è un PASS dell'intera M1.f**, né una misura comparativa.

Autore: GPT-6.1 Sol, OpenAI, nome dichiarato dal runtime; costi/token non esposti. Lette le CLI reali producer44/harness114/worker262/compare26, fixture112, schema103, protocol50 e review005 di 25 righe. Le otto regressioni005 non sono state ripetute. Nessuna delega.

## Ammissione, comando e pin

Nuova area esterna: `tmp/m1/f-hash/turns/freeze005-tiny-validation-001/`. Esecuzione seriale dal **2026-10-25T20:42:22.351153530Z** al **2026-10-25T20:42:24.880929+00:00**. Prima dell'avvio: target dataset/run/confronti assenti, namespace0700, file regolari UID1000/nlink1/no022 e identità FD/entry verificata via NOFOLLOW; quota globale e riserva16MiB disponibili.

Pin ricontrollati prima di ogni entry:

| Artefatto | SHA-256 |
| --- | --- |
| guard005 | `ffd274d0342c42fcc2c2a08f98088a3eea7f4398bf507af08f25ed64c3a6615a` |
| freeze005, 13 sorgenti | `6db59077e7bdcdcec612b3e51b14ca963a86b8add66e5d3f5a0d760ea8d8e94a` |
| approval005, 327 byte | `d85df7bb7ad27312d191d20b64b45950ac5ecbadda0c99a0c1c152a9ebde443c` |
| trusted-invocation-005.txt | `3e2d0df8b0000e220f5a78488a80471a9595d7ec0641f171c93b54535e920209` |
| protocollo reale | `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba` |

Ogni Popen primario usa `/bin/bash --noprofile --norc -c`, seguito dalla **stringa integrale catturata e verificata** del trusted template, poi `--`, entry e argomenti della tabella. Il pin approval è passato in `REVIEW_APPROVAL_SHA256_005`. Non viene eseguito un modulo operativo direttamente e non si riapre un launcher modificabile come unica autorità. I worker vengono avviati soltanto dall'harness e ripetono la catena guard/pin.

Argv completi, literal ed ambiente sono nei singoli `<fase>/invocation.json`; comandi dell'osservatore e timestamp in `<fase>/launch.json`. Ambiente primario isolato: HOME/TMP/XDG nei nuovi scratch, PATH `/usr/bin:/bin`, OPENSSL_CONF `/dev/null`, Python `-I -B -S`, nessun userconfig o bytecode. L'harness usa per i figli il proprio ambiente congelato e directory tmp/cache/config private sotto f-hash.

| Fase | Entry e argomenti effettivi | PID primario raccolto |
| --- | --- | --- |
| producer-empty | `prepare_inputs --case empty` | 255427 |
| producer-abc | `prepare_inputs --case abc` | 255434 |
| empty-hashlib | `harness --engine hashlib --case empty --chunk 1 --scenario clean --run-id freeze005-tiny-empty-hashlib-001` | 255448 |
| empty-openssl | `harness --engine openssl --case empty --chunk 1 --scenario clean --run-id freeze005-tiny-empty-openssl-001` | 255458 |
| abc-hashlib | `harness --engine hashlib --case abc --chunk 1 --scenario clean --run-id freeze005-tiny-abc-hashlib-001` | 255470 |
| abc-openssl | `harness --engine openssl --case abc --chunk 1 --scenario clean --run-id freeze005-tiny-abc-openssl-001` | 255480 |
| compare-empty | `compare_results` sui due `runs/freeze005-tiny-empty-<engine>-001/worker/result.json`, `--output runs/freeze005-tiny-empty-hashlib-001/agreement-openssl.json` | 255492 |
| compare-abc | `compare_results` sui due `runs/freeze005-tiny-abc-<engine>-001/worker/result.json`, `--output runs/freeze005-tiny-abc-hashlib-001/agreement-openssl.json` | 255501 |

I path compare sopra sono abbreviati rispetto a f-hash: l'argv reale contiene i path assoluti. Le CLI esistenti sono state usate senza modifiche. Otto bootstrap primari, quattro worker interni: tutti exit0, nessuna safety-stop; nessun altro caso ammesso.

## Oracle, snapshot e comparabilità

| Input letterale | Byte | SHA-256 GT, ottenuto da entrambi gli engine |
| --- | --- | --- |
| empty | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| abc, hex616263 | 3 | `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad` |

GT fissati prima dei candidati, non ricavati dal producer o da uno degli engine. Producer: byte esatti, generation1, manifest e ready coerenti, dati/configurazioni mode0400/nlink1. Receipt del producer significa CONTENT_ORACLE_VERIFIED_NOT_CRYPTO_ORACLE, non un risultato di hashing. I file pubblicati mantengono byte, inode, mtime, modi ed ownership durante tutte le fasi successive.

Quattro snapshot `linux-memfd-sealed-v1`, dimensioni0/3 corrette, quattro identità distinte. Il worker controlla F_GET_SEALS dopo F_ADD_SEALS; trace e schema attestano mask15 prima del digest. Identity del trace uguale alla cache key; riferimenti sorgente/generation/configurazione coerenti. Contatori contenuto totali: **hashlib2, openssl2**; ogni ricetta invoca solo l'engine scelto, contatore dell'altro zero. Nessuna cache lookup.

Due confronti reali **AGREEMENT_ONLY**: stessi byte/spec/versione, clean/chunk1 e ricetta non-backend, differenti catture sealed; istanza sorgente comune dichiarata (`source_instances_distinct=false`). Il comparator valida entrambi i record prima dell'output; marker/checksum verificati. Chunk interno CLI OpenSSL non controllato: chunk1 non implica identico buffering dei due backend.

Runtime effettivo: Python **3.14.4**, adapter **_hashlib**, libreria **OpenSSL 3.5.5 27 Jan 2026**. CLI: **OpenSSL 3.5.5 27 Jan 2026 (Library: OpenSSL 3.5.5 27 Jan 2026)**, default richiesto esplicitamente con `-provider default -propquery provider=default`. Provider interno hashlib **NOT_INTROSPECTED**. I due percorsi possono condividere OpenSSL: **non è garantita indipendenza algoritmica**. I GT letterali validano i vettori tiny; l'agreement non sostituisce un oracle indipendente per altri contenuti o una prova di snapshot generico/ABA.

## Risorse, raccolta e conservazione

Osservatore derivato dal qualificato005, SHA **`7d184f64b220b54fdb0515ff9697eeae4eddab8ad5a4bc6a7a0b1b0a3331fff2`**. Aggiunti dispatch seriale e controlli degli artefatti tiny; helper, monitor nonblocking e cleanup confrontati via AST e invariati. Deadline180s per fase, gruppo proprio, pidfd e subreaper locale; nessuna configurazione di sistema modificata.

Otto Popen primari raccolti, quattro worker exit0 riportati dai rispettivi harness. Discendenti diretti osservati degli harness: **255449, 255459, 255471, 255486**; argv non sempre catturato. Sono osservati16 PID complessivi, inclusi figli CLI; il campionamento **non è una traccia esaustiva di fork/argv**. Nessun adottato o errore di raccolta registrato. Stderr primari/osservatore e stdout/stderr dei quattro worker vuoti; stdout primario degli harness contiene i controller, non misure.

Massimo RSS aggregato campionato **52187136 byte**, sotto il limite best-effort **536870912 byte**. Massimo RSS worker riportato **22964 KiB**, sotto **524288 KiB**. Managed prenotato **34603008 byte** per empty, **34603011** per abc, entro256MiB; memfd incluso nella ricetta. Non esiste un contatore dedicato del managed aggregato. RLIMIT_AS256MiB per processo è un limite supplementare, non quel contatore. RLIMIT_FSIZE2MiB e core disabilitati.

Conteggio globale conservativo dei blocchi tmp, directory/link compresi senza traversal dei target: massimo campionato durante le fasi **867688448 byte**, postflight read-only **867848192 byte** prima della sua ricevuta, contro1GiB; riserva16MiB. Campionamento RSS non prova ogni picco istantaneo; nessun claim di sandbox OS. Questi dati sono guardie di risorsa, non benchmark o confronto di prestazioni.

**279 file preesistenti** preservati per hash, modi, UID/GID, nlink, inode e mtime, inclusi source/protocollo/approval e prove precedenti. Nessun overwrite, chmod di originali, rilassamento di gate o aggiornamento automatico dei pin. Ctime non usato come oracle. Nuovi artefatti creati esclusivamente nel namespace ammesso; **117 file** verificati nel postflight, con hash e metadati. Le altre dimensioni, fault e regressioni non sono stati eseguiti.

## Ricevute e prossimo incompleto

Manifest degli artefatti e verifica finale: `turns/freeze005-tiny-validation-001/postflight.json`, SHA **`d1e28645297ae7b55ae3a74921658c900d9571c979ebb9d06a2017dbbf0ab022`**. Contiene SHA dei quattro result.json, dei due agreement, originali, log, controller, argv e PID osservati. Ogni fase conserva runtime/verification/launch e stdout/stderr; pubblicazioni e marker sono stati verificati senza rieseguire candidati. Non è stato creato un FIRSTFAIL nuovo.

Completate **4/38 ricette** e **2/11 agreement** del protocollo; restano34 ricette e nove agreement. Prima voce contenuto incompleta: producer **pattern4097** e clean chunk4096, solo dopo review di questa prova e nuova ammissione esclusiva. Nessun 4097/16/128MiB, altro fault, 3p/7, throughput, vincitore o cache lookup avviato. Misure e cache lookup restano NON IMPLEMENTATE; M1.f resta aperta.

Handoff fermo alla review indipendente, slot libero. Nessuna modifica a progress/index/piani, vecchi record o sorgenti; nessun Git, rete, installazione, indice, licenza, Siril o delega.
