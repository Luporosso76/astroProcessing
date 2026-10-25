# M1.f: regressioni freeze005, turno 001

**8/8 metodi PASS**, zero failure, errori o skip. Bootstrap e osservatore: **exit0**, una sola invocazione, nessun retry. Esito limitato alle regressioni; non è un PASS M1.f né un'ammissione di altre entry. Slot libero alla consegna di questo handoff.

Autore: GPT-6.1 Sol, OpenAI, nome dichiarato dal runtime; costi/token non esposti. Runtime effettivo in `preflight.json`: Python 3.14.4, GCC15.2.0, Linux, UID1000. Letti osservatore004 di 301 righe, template16, probe266-313 e log13; nessuna nuova analisi del piano o review autonoma sostitutiva. Nessuna delega.

## Turno e comando realmente eseguito

Scratch esterno nuovo: `tmp/m1/f-hash/turns/freeze005-regressions-001/`. Finestra dell'osservatore, registrata in `launch.json`: **2026-10-25T20:08:31.395409668Z** fino a **2026-10-25T20:08:32.203195+00:00**. Tempi nuovi di questa esecuzione, non ereditati dalla 004.

Il comando ammesso corrisponde al template005 con `REVIEW_APPROVAL_SHA256_005=d85df7bb7ad27312d191d20b64b45950ac5ecbadda0c99a0c1c152a9ebde443c` ed entry `future_regressions --run-id freeze005-regressions-001`. L'argv effettivo del Popen è `/bin/bash`, `--noprofile`, `--norc`, `-c`, **la stringa integrale del template catturato**, `--`, `future_regressions`, `--run-id`, `freeze005-regressions-001`. Non viene riaperto un launcher modificabile dopo il controllo: il payload verificato viene passato in memoria alla shell. Argv completo, byte del literal ed ambiente effettivo sono in `invocation.json`; il payload non è riscritto o sintetizzato dal candidato.

Ambiente ricostruito da zero: PATH `/usr/bin:/bin`; HOME/TMPDIR/TMP/TEMP e XDG cache/config/data/state dentro il nuovo scratch; OPENSSL_CONF `/dev/null`; Python `-I -B -S`, niente bytecode/userconfig, locale C.UTF-8, TZ UTC, thread numerici uno. Non sono stati letti HOME o configurazioni personali.

Osservatore adattato prima dei dati dal qualificato004: cambiati soltanto pin, template005/Bash, variabile del pin e run-id. Hash nuovo **`13d83cbd05ffb83c79daef12164fb280dba23cc16c5a5d7ce31a48da92b22b9d`**; helper e corpo monitor/cleanup confrontati via AST, invariati salvo argv della Popen. Differenza e hash conservati in `observer-delta.patch` e `observer-preparation.json`. Nessun modulo operativo eseguito direttamente fuori dalla guardia. Subreaper limitato al processo osservatore, sessioni proprie e pidfd; nessuna modifica a configurazioni di sistema.

## Preflight e conservazione

Verificati pin esterni di guard005 `ffd274d0342c42fcc2c2a08f98088a3eea7f4398bf507af08f25ed64c3a6615a`, freeze005 `6db59077e7bdcdcec612b3e51b14ca963a86b8add66e5d3f5a0d760ea8d8e94a`, template `3e2d0df8b0000e220f5a78488a80471a9595d7ec0641f171c93b54535e920209` e protocollo `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`. Confrontati tutti i **13 sorgenti**. Approval005: **327 byte**, mode0600, schema1/status APPROVED e lista esatta future_regressions, prepare_inputs, harness, worker, compare_results; digest coincidente con il pin fornito sopra.

Directory private0700; file regolari UID1000/nlink1 senza bit022, identità FD/entry e traversal NOFOLLOW. Target interni005 e tre run-id dei probe inizialmente assenti; nessun overwrite o autorename. Il target004 non è stato riusato.

Postflight: **161 file preesistenti** invariati per byte, inode, mtime, dimensione, UID/GID, nlink e modi. Ctime non usato come gate. Inclusi approval004, FIRSTFAIL storico mode0400, 8/8 storici, fonti, protocollo, approval005, freeze e report005 di 43 righe. Nessun chmod su originali, source rewrite o approval refresh. L'osservatore sigilla soltanto nuove ricevute/log; la creazione dei nuovi artefatti usa modi privati.

## Oracle effettivi

Log candidato: stdout **0 byte**, stderr **1076 byte**, otto righe `... ok`, finale `Ran 8 tests in 0.581s` e `OK`. Summary primario nuovo: `tmp/m1/f-hash/regressions/freeze005-regressions-001/summary.json`, PASS/methods8/failures0/errors0, marker `.complete` verificato. Il suo SHA `e2d64c0dfdcecf58870dc8dea31603f3737bef97589b3cddd3cbf14b23b4fe5e` coincide con il summary004 perché lo schema deterministico non include il run-id; percorso, inode, mtime, log ed invocazione di questa prova sono nuovi e registrati.

I tre probe hanno usato **future_regressions, entry ammessa**, cambiando un solo pin. Tutti exit **1**, stdout vuoto, target ineseguito e assente. Stderr integrale in hex e returncode reale sono conservati nei record `review/pin-probe-{guard,freeze,approval}.json` del run interno, con marker verificato:

| Pin errato | Ultima riga stderr effettiva | Byte stderr |
| --- | --- | --- |
| guard | `guard hash rejected; review required` | 37 |
| freeze | `RuntimeError: reviewed source manifest hash changed; STOP, review/new version required` | 1098 |
| approval | `RuntimeError: independent review receipt missing/mismatched; no writes or producer/worker` | 1104 |

I cinque rami cleanup scan/proc/interruzione/race/wait hanno completato le assertion sulla stessa eccezione originale, returncode presente e ChildProcessError al successivo waitpid. Dieci file stdout/stderr dei piccoli figli: vuoti. PID bootstrap **229172**, exit0 e raccolto. PID campionati: **229172, 229178, 229179, 229180**; nessun orfano adottato o errore di cleanup registrato. Il campionamento non ha conservato i PID dei cinque figli cleanup brevi e non costituisce una traccia completa dei fork. Nessun PID estraneo selezionato.

Passati anche schema e agreement: **37 campi negativi e quattro profili sintetici**, canonical/cross-field e coppia esatta. Il record generation2 sotto runs ha raggiunto Failure ERR_RESULT_SCHEMA generation, non SystemExit di whitelist; agreement e marker assenti. Link quota conteggiati senza traversal, ma rifiutati come path candidati. I due target esterni simulati nello scratch conservano byte attesi, mode0400/nlink1; working A/B intatte. La rimozione dei link è soltanto il cleanup preregistrato, non una correzione per ottenere PASS.

Conteggi runtime disponibili: **otto metodi, zero failure/errori/skip**. unittest non espone il totale delle assertion o dei sottocasi riusciti; nessun contatore aggiunto e nessun totale inventato. I 37+4 casi restano assertion di schema, non le 38 ricette. Verificati hash/metadati di **115 artefatti** in `postflight.json`, senza rieseguire il candidato.

## Risorse e limiti della prova

Tutto tmp, blocchi directory/link inclusi e senza seguirne i target: **863141888 byte** prima del candidato, picco campionato **863944704 byte**, **864247808 byte** durante il postflight prima della sua ricevuta. Riserva dichiarata **16777216 byte**, tetto **1073741824 byte**. Nessun inventario del contenuto di altri namespace.

RSS aggregato massimo campionato **47759360 byte**, sotto **536870912 byte best effort**; maxrss dei figli raccolti **27148 KiB**. RLIMIT_AS **268435456 byte per processo**, RLIMIT_FSIZE **2097152 byte**, core disabilitati, deadline esterna180s. Durata osservatore **0.7168545169988647s**, controllo del turno, non benchmark. Managed256MiB rimane limite dichiarato con address-space cap e fixture piccole; non esiste un contatore dedicato del managed aggregato, e RLIMIT_AS non viene presentato come tale. Campionamento RSS non prova ogni picco istantaneo; nessun claim di sandbox OS.

## Ricevute e handoff

Hash relativi allo scratch esterno:

- `invocation.json`: `00803b6aab4e31f40e2c4d55016996b613b4ce290273d096c22c1a6170208da8`.
- `runtime.json`: `f02b8aedafaf140913ac42b73750957022f31681a935d4f457600118371c2ea9`.
- `postflight.json`: `940a29e912cadf5fb9bd3ce498bd74b2adbafced6958ebe9c64ea569c7e1d000`.
- `launch.json`: `3ddbfedaa8847631aee6dd78b7ddbfd1c018d9ea4b62180adb40ecc76ac485b6`.
- `stderr.txt`: `60b6fdde844abf2371746a52a70cf82eaf4d5e4e4253fdaa4d646bfd4a4a9234`.

Nessun FIRSTFAIL nuovo; quello004 resta intatto. Nuovi file creati senza overwrite; ricevute in sola lettura operativa, non immutable del kernel contro proprietario/root. Nessun producer, hash worker di contenuto, CLI OpenSSL, empty/abc o dataset128MiB eseguito; datasets resta assente. Le 38 ricette e gli 11 agreement di piano non sono eseguiti. Misure e cache lookup restano NON IMPLEMENTATI.

Consegna alla review indipendente di questa prova. Slot libero, nessuna prosecuzione automatica: altre entry/casi richiedono un'altra ammissione. Nessuna modifica a progress, piani, indici, prove storiche o altri namespace; nessun Git, rete, installazione, Siril o licenza.
