# M1.f: completamento pattern4097, freeze005

**Tutti i 24 casi ammessi e i quattro confronti corrispondono allo schedule.** Quattro clean, sedici errori attesi, quattro positivi aggiuntivi. Nessun inatteso, retry, fallback o FIRSTFAIL nuovo; `NOT_RUN=[]`. Incremento24/38 ricette e4/11 agreement; **totali30/38 e7/11**. M1.f resta aperta.

Autore: GPT-6.1 Sol, OpenAI, dichiarato dal runtime, non attestato. Nessun override del modello o delega. Costi/token non esposti. Lette review19, protocol50, harness114, worker262 e parti necessarie dell'osservatore precedente; non riletto il piano4500.

## Schedule chiuso prima dell'esecuzione

Outer nuovo: `tmp/m1/f-hash/turns/freeze005-pattern4097-completion-001/`. `preflight.json` registra tutti i28 argv, aspettative, codici, contatori/eventi attesi, target assenti e budget. `schedule.json` è chiuso prima del primo Popen; SHA dei byte canonici senza newline: **`0b722ebd1e6c977613cb644faee1b087b6d937cee75dbd5c9d41d5ecde9de786`**. Il launcher ricontrolla questo hash, non rigenera lo schedule dopo errori.

- Clean: chunk1048576 e8388608, ciascuno con hashlib e openssl.
- Negativi: gli otto scenari congelati, entrambi gli engine, chunk4096.
- Positivi: sealed-write-attempt e reopen-unchanged, entrambi gli engine, chunk4096.
- Compare: soltanto le due coppie clean nuove e le due coppie positive. Nessun confronto su negativi.

Run-id espliciti: `freeze005-p4097-completion-<scenario>-<chunk>-<engine>-001`. Ogni confronto pubblica `agreement-openssl.json` nel corrispondente nuovo run hashlib. Nessun producer invocato, nessun originale rigenerato; le copie working dei negativi sono quelle previste dalle ricette congelate.

## Invocazioni e binding

Ogni entry primaria usa `/bin/bash --noprofile --norc -c`, seguito dai byte integrali catturati del trusted template005, `--`, entry e argomenti. `REVIEW_APPROVAL_SHA256_005` contiene il pin esterno. Nessun import operativo diretto: harness e worker ripetono la guardia. Argv realmente passati ed ambiente completo sono nei28 `invocation.json`; timestamp e comandi degli osservatori nei28 `launch.json`.

| Binding verificato | SHA-256 |
|---|---|
| freeze005, 13 sorgenti | `6db59077e7bdcdcec612b3e51b14ca963a86b8add66e5d3f5a0d760ea8d8e94a` |
| guard005 | `ffd274d0342c42fcc2c2a08f98088a3eea7f4398bf507af08f25ed64c3a6615a` |
| template005 | `3e2d0df8b0000e220f5a78488a80471a9595d7ec0641f171c93b54535e920209` |
| approval005 | `d85df7bb7ad27312d191d20b64b45950ac5ecbadda0c99a0c1c152a9ebde443c` |
| protocollo reale | `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba` |

Finestra UTC effettiva: **2026-10-26T07:53:55.119069376Z / 07:54:07.006910+00:00**, dato di provenienza, non confronto temporale. Ambiente privato ROOT/HOME/TMP/XDG, Python `-I -B -S`, OPENSSL_CONF=/dev/null, locale C.UTF-8 e thread numerici uno. Namespace0700, file UID1000/nlink1/no022, identità FD/entry e NOFOLLOW verificati. Nessun ampliamento di permessi o aggiornamento di approval.

## Esiti scientifici e contratti negativi

Tutti i28 bootstrap primari e gli osservatori terminano con exit0. **Otto worker positivi exit0; sedici worker negativi exit1 atteso.** Per questi ultimi harness=`EXPECTED_ERROR`, exit0: è il successo dell'assertion di arresto, non un digest riuscito o un errore scientifico occultato.

| Scenario negativo, due engine ciascuno | Codice reale richiesto e ottenuto |
|---|---|
| truncate | ERR_LENGTH |
| trailing | ERR_LENGTH |
| declared-size | ERR_MANIFEST |
| stale-generation | ERR_MANIFEST |
| mutate-unread | ERR_CONTENT |
| read-error | ERR_READ |
| engine-error | ERR_ENGINE |
| reopen-mutated | ERR_CONTENT |

Per tutti: error.json e marker completi, workerexit1, codice esatto, risultato e marker risultato assenti, `result_published=false`, preservazione originali confermata e contatore dell'altro backend zero. Verificati anche contatori del backend scelto ed eventi preregistrati. Le working copy mostrano precisamente troncamento/byte aggiunto/metadata alterati o singolo byte mutato; nessuna fixture cancellata. Le mutazioni intercalate conservano size/mtime come dichiarato ma vengono respinte: non è uno stress test di writer paralleli.

Otto record risultato pubblicati, tutti con digest **`6dea11fd585c26a1d50de04c9fc6c9b3f6918df71bcd53eda777069cd5512243`**, coerente con il precedente consenso sul medesimo contenuto. Questo riferimento **non diventa un GT crittografico indipendente**: `expected_sha256` resta null. L'oracle indipendente dal producer verifica tutti i4097 byte mediante `(73*i+19)%256`, zero mismatch, generation1.

Dieci snapshot nei casi positivi: quattro clean, due sealed-write-attempt e quattro nelle due reopen-unchanged. Identità distinte, size4097 e mask richiesta15 verificata mediante `F_GET_SEALS` prima del digest; cache key/trace/ricetta coerenti. Il trace conserva la mask richiesta, non il valore integrale restituito dalla syscall. Entrambi i tentativi di scrittura restituiscono EPERM; ogni reopen crea due identità distinte e contatore selezionato2. Altri quattro snapshot appartengono ai negativi engine-error/reopen-mutated: **14 eventi sealed complessivi**, senza trasformarli in risultati pubblicati.

Contatori di tentativo contenuto: hashlib7/openssl7, incluse le iniezioni engine-error; non sono14 risultati. I quattro confronti verificano spec/versione/ricetta non-backend e restituiscono **AGREEMENT_ONLY**, con istanza comune dichiarata. Python 3.14.4/_hashlib e OpenSSL 3.5.5 27 Jan 2026; CLI default esplicito, provider hashlib NOT_INTROSPECTED. Possono condividere OpenSSL: nessuna indipendenza crittografica, nessun controllo del buffering interno CLI. Cache lookup e misure restano NON IMPLEMENTATE.

## PID, raccolta e limiti

La tabella macchina `postflight.json:stages` separa **PID primario Popen(Bash)**, ruoli campionati HARNESS/WORKER/COMPARE_RESULTS e rispettivi exit. Un PID può attraversare più stati via exec. Non si deduce il worker dalla sola parentela. Le osservazioni registrano PID/start ticks/parent/argv; non attestano ogni exec o fork.

Esempio: nel clean8388608 hashlib, primario/harness799532, worker799533. Quest'ultimo mostra inizialmente argv HARNESS ereditato prima dell'exec, poi WORKER: non è un secondo harness. Il raw è conservato; il postflight esplicita la transizione e l'ultimo ruolo significativo. Nessuna rettifica delle prove precedenti. Tutti i Popen primari raccolti, worker raccolti dagli harness secondo controller; nessun adottato o errore di cleanup registrato.

Stderr primari/osservatori e log worker vuoti; stdout primari harness coerenti con i controller. Per engine-error la CLI cattura internamente stderr, non persistito separatamente dal worker congelato: error.json è la diagnosi disponibile, senza inventare raw mancanti.

Osservatore nuovo SHA **`3f57a34e3863fd3af1df13920f25ed2991145cb6935068f5f132ff1d4e45d6ab`**: dispatch/validatori di aspettativa e logging read-only dei ruoli; monitor centrale/deadline/cleanup confrontati via AST, invariati. Deadline180s/fase, own session/pidfd/subreaper locale, RLIMIT_FSIZE=2MiB, core disabilitati. Non è un sandbox OS.

RSS aggregato massimo campionato **66891776 B** contro512MiB best-effort; massimo worker **22888 KiB** contro524288KiB. Managed prenotato **34607105 B** contro256MiB; distinto da RLIMIT_AS=256MiB per processo. Nessun contatore managed aggregato e nessuna prova di ogni picco istantaneo. Tutto tmp: massimo campionato **953716736 B**, postflight **954085376 B** prima della sua ricevuta, entro1GiB e riserva16MiB; nessun cleanup di storia per ottenere spazio.

## Conservazione e handoff

Ricontrollate prima del turno le398 righe storiche precedenti e i61 artefatti raw; poi **461 file preesistenti** preservati per hash, modi, UID/GID, nlink, inode e mtime. Originali0400 immutati. La precedente rettifica PID, limitata al report, è acquisita come baseline attuale; non viene riscritto il passato.

Postflight: **480 artefatti** verificati, inclusi risultati/errori/working copy, marker, controller, comandi e ruoli PID. `postflight.json` SHA **`e9e8c4f2421f290f5e5224c0157e09e6c456e4862528d2d8151aa624726c4bdd`**. `outcomes.json` elenca tutti i28 esiti e `NOT_RUN=[]`; nessun inatteso. Nuove ricevute O_EXCL/fsync, nessun overwrite o autorename.

Restano **8/38 ricette e4/11 agreement**: primo incompleto, producer **pattern16m** e clean chunk1048576, soltanto dopo review e nuova ammissione. Nessun altro dataset, benchmark, estensione automatica o nuova funzione prodotto. Nessun Git/rete/install/indici/Siril/delega; progress, piani e sorgenti invariati. **Slot libero all'handoff.**
