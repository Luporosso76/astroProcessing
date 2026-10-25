# M1.f: otto regressioni freeze004, turno 002

Esito primario: **8/8 metodi PASS**, zero failure, errori o skip. Bootstrap e osservatore esterno terminati con **exit 0**, una sola invocazione ammessa, nessun retry. Questo risultato riguarda soltanto le regressioni preregistrate, non chiude M1.f e non ammette producer o empty/abc.

Autore: GPT-6.1 Sol, OpenAI, nome dichiarato dal runtime. Costi/token non esposti. Nessuna delega. Codice reale degli otto metodi e delle guardie già letto nel turno precedente; in questo turno letti hardening198, log13, summary1 e invocation49. Hash e AST mirati, senza rileggere il piano completo. Runtime: Python 3.14.4, GCC 15.2.0, Linux, UID 1000; stringa esatta in `preflight.json`.

## Ammissione e preflight

Nuovo turno esplicito del coordinatore, distinto dal precedente FIRSTFAIL 0/8. La ricevuta di hardening è stata verificata contro **`4668735bf8f29d9548779c0fd797c4d952b7d5032bf1e34bf347ca6b7617967c`**. Le sue 17 righe descrivono **15 file 0664→0644 e due 0600→0600**, non 17 cambi di modo. La ricevuta attribuisce al coordinatore la correzione e la conservazione di byte/inode/mtime; questo turno verifica lo stato corrente, non retroattesta l'operazione.

Verificati prima dell'avvio: 13 hash sorgente, i 17 file di consegna con UID1000/nlink1/nessun bit 022, file regolari, identità FD/entry e traversal NOFOLLOW. Namespace f-hash e scratch privati 0700. I target interni `regressions/freeze004-regressions` e `runs/regression-schema-freeze004-regressions` erano assenti: nessun overwrite o cambio automatico del run-id.

| Artefatto | SHA-256 verificato prima e dopo |
| --- | --- |
| Freeze004 | `f880d0c7d23838f068254874bff96d04d81e0aae048e80a9755c1e035e7c8e91` |
| guard.py | `e52324b8c609feb52f15f2134af80c7b9738f50b3daba2ef9356ef708940a649` |
| Invocazione catturata, 1623 byte | `3b1d6e8ea3111ba059058741a0a2c02da7daa760fc063460c7fce0d2c653ca29` |
| Approval, 269 byte | `d61cd9a8a8c0d77cddef3332a9813ebbbc092e16f59bf942856e8414733fdcc1` |
| Protocollo reale | `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba` |

Approval schema1/status APPROVED, allowed_entries esattamente `future_regressions`. Nessun refresh o modifica dell'approval. FIRSTFAIL storico preservato, SHA **`e30095885a7462b123cc4b7cacf11cea51d17d93019f77251f883a74a6083422`**, mode0400. Il postflight confronta byte, inode, mtime, UID, nlink e modi dei file verificati: invariati durante questo turno.

## Comando effettivo e isolamento

Scratch esterno nuovo: `tmp/m1/f-hash/turns/freeze004-regressions-002/`. Osservatore avviato una volta con:

```text
env -i PATH=/usr/bin:/bin TMPDIR=/opt/workspace/astroProcessor/tmp PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 -I -B -S /opt/workspace/astroProcessor/tmp/m1/f-hash/turns/freeze004-regressions-002/supervisor.py
```

L'osservatore ha verificato il pin esterno del comando catturato, confrontato il suo literal con quello del guard pinnato e passato gli stessi byte a `/usr/bin/python3 -I -B -S -c`. Ha sostituito soltanto l'argomento approval con il pin fornito dal coordinatore. Entry/argomenti esatti: `future_regressions --run-id freeze004-regressions`. Nessuna riscrittura del bootstrap e nessun import locale prima della catena verificata. SHA del literal: **`afde76dc61b10f2cc260f6bcc9acee39a329fb107c2b9fc1805f6eaa1f155c7e`**.

`invocation.json` conserva argv completo, literal, cwd e tutte le variabili effettivamente passate. Ambiente ricostruito da zero: PATH `/usr/bin:/bin`, HOME/TMPDIR/TMP/TEMP e XDG cache/config/data/state soltanto nel nuovo scratch, OPENSSL_CONF `/dev/null`, Python isolato senza site/userconfig/bytecode, locale C.UTF-8, TZ UTC e thread numerici fissati a uno. Non sono stati usati HOME o configurazioni personali.

Osservatore nuovo, SHA **`cfeaf4bcfae9794a9678dbc06197b569a9c8b4a752ae39e4c168a4cfef468290`**, AST controllato prima dell'avvio. L'unico fchmod effettuato da questo autore ha assegnato 0600 a questo nuovo script e 0400 alle nuove ricevute/log: nessun permesso di sorgenti o originali modificato. L'osservatore usa una propria sessione, pidfd e subreaper limitato al proprio processo; non modifica configurazioni di sistema. Non è un sandbox OS.

## Risultato osservato

Log integrale `stderr.txt`, 1076 byte; stdout vuoto. Finale reale:

```text
Ran 8 tests in 0.411s

OK
```

Gli otto nomi sono registrati con `... ok` nel log:

| Metodo | Esito e copertura |
| --- | --- |
| test_allocated_counts_links_without_target_traversal | PASS: contabilizzazione dei link via stat NOFOLLOW, apertura del target sorvegliata, stessi link rifiutati come input candidato |
| test_cleanup_race_and_wait_failure | PASS: due rami, race ProcessLookupError e errore di wait, causa originale conservata |
| test_collection_preserves_original_exception | PASS: tre rami scan/proc/KeyboardInterrupt, causa originale conservata |
| test_no_agreement_output_for_bad_records | PASS: record sotto runs ammesso, Failure ERR_RESULT_SCHEMA con generation, nessun agreement o marker |
| test_preflight_symlink_hardlink_and_identity | PASS: preflight, ancestor/intermedio/leaf symlink, hardlink e FD/entry sostituiti |
| test_review_gate_and_external_pins | PASS: receipt invalidi/assente e tre probe bootstrap con pin errato, rifiutati prima del producer |
| test_schema_each_field_and_exact_pair | PASS: 37 campi negativi, quattro profili CLI sintetici, canonical/cross-field/trace/coppia esatta e istanze dichiarate |
| test_source_and_actual_protocol_pins | PASS: cinque mutazioni su copie e protocollo simulato, senza mutare fonti reali |

I **37+4 casi** sono assertion sui record sintetici, non 41 test autonomi e non le 38 ricette di hashing. Le rispettive tabelle sono state contate via AST e i cicli sono completati nel metodo passato. unittest espone otto metodi, non un totale delle assertion o dei sottocasi riusciti: non è stato aggiunto un contatore runtime. `postflight.json` registra esplicitamente questa distinzione.

Summary primario prodotto dal candidato in `tmp/m1/f-hash/regressions/freeze004-regressions/summary.json`: PASS, methods8/failures0/errors0, UID1000. Marker `.complete` confrontato con il digest dei byte. Zero skip riscontrati: tutti gli otto metodi hanno riga `ok`, nessun metodo saltato.

## Processi, target e limiti

PID bootstrap **196785**, raccolto dall'osservatore con exit0. Cinque rami cleanup hanno completato le assertion preregistrate: stessa eccezione originale, returncode presente e ChildProcessError su waitpid successivo. I dieci file stdout/stderr dei piccoli figli sono vuoti. Il campionamento esterno ha osservato quattro PID: bootstrap e tre probe di guardia **196791, 196792, 196793**. I PID dei cinque figli cleanup, molto brevi, non sono stati conservati dal campionamento; non si presenta questa lista come traccia esaustiva dei fork. Nessun orfano adottato e nessun errore di raccolta registrato. L'osservatore ha segnalato soltanto il proprio gruppo e i pidfd ottenuti dai propri discendenti.

I tre probe preregistrati contengono `prepare_inputs --case empty` nell'argv ma hanno pin deliberatamente errato: non avviano l'entrypoint producer. Il test ha verificato returncode nonzero; i singoli codici e i loro stderr non sono persistiti. Nessuna esecuzione di producer/worker di contenuto, CLI OpenSSL o digest di dataset empty/abc. `datasets` resta assente.

Postflight read-only: entrambi i target esterni **simulati dentro lo scratch** conservano byte attesi, mode0400 e nlink1. I link di fault injection sono stati rimossi dal cleanup già preregistrato, non per correggere un esito. Working copy A/B intatte; agreement.json e marker assenti. Verificati 104 file di artefatto, con hash e metadati in `postflight.json`. Nessuna lettura del contenuto di target di altri namespace.

- Tutto tmp, includendo blocchi directory e symlink senza seguirli: **859709440 byte** prima dell'esecuzione; massimo campionato **860561408 byte**; postflight read-only **860577792 byte** prima di scrivere la sua ricevuta. Tetto **1073741824 byte**, riserva finita **16777216 byte**.
- Picco RSS aggregato campionato dei discendenti: **47927296 byte**, sotto **536870912 byte** best effort. Maxrss dei figli raccolti: **26604 KiB**. Campionamento non prova ogni picco istantaneo.
- RLIMIT_AS **268435456 byte per processo**, RLIMIT_FSIZE **2097152 byte**, core disabilitati, deadline esterna **180 s**. Durata osservatore **0.54645244195126 s**, dato di controllo, non benchmark.
- Managed richiesto **256 MiB**: fixture e artefatti limitati alla riserva e address-space limit imposto; nessun contatore dedicato del managed aggregato è implementato nelle regressioni. Non si spaccia RLIMIT_AS per un contatore managed di prodotto.

## Ricevute e prossimo gate

Tutti i percorsi seguenti sono relativi al nuovo scratch esterno:

| Ricevuta | SHA-256 |
| --- | --- |
| preflight.json | `3e656ac1622945e28bbaea330fb61fd0cb8f8d736e0969aed689b9115abee4ab` |
| invocation.json | `27ab423c85eac07460a8be0f010d7329bf1333c1423ee17b5beb32b787e69c4c` |
| runtime.json | `78196edeac094db2416584e0c10efd3a54dc983dd4a5d7f63245973a8901aa99` |
| postflight.json | `309c1f1e8ef2c26f9c6beedd9458f1cb7120a34e0ff741872a44a3305b470040` |
| stderr.txt | `18f0e344eb5d8f8a78db126f6c30b78afba5afe740536a22817757b57018461d` |

SHA summary primario: **`e2d64c0dfdcecf58870dc8dea31603f3737bef97589b3cddd3cbf14b23b4fe5e`**. Ricevute/log esterni creati senza overwrite, fsync e mode0400; ciò non è l'attributo immutable del kernel. Nessun FIRSTFAIL nuovo: il precedente rimane prova distinta e invariata.

Le **38 ricette** e gli **11 agreement di piano** restano non eseguiti. Gli agreement sintetici in memoria sono assertion di schema, non confronti di engine su dataset. Nessun dataset128MiB, misura 3p/2 warm-up/7 round, bootstrap statistico o cache lookup: queste parti restano NON IMPLEMENTATE.

Consegna ferma alla review indipendente di log, summary e invarianti. Serve nuova ammissione separata prima di qualsiasi entry producer; questo verbale non è un'approval. Nessuna modifica a progress, indici, piani, sorgenti o altri namespace, né Git/rete/installazioni/Siril/licenze.
