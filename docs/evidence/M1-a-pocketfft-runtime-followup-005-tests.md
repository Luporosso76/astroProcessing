# M1.a pocketfft runtime followup 005: turno43unit005, soli test

Esito: `PASS_43_UNIT_TESTS_ONLY`. Un'unica esecuzione, exit 0, 43 risultati `ok`, 0 failure, 0 errori, 0 skip. I 5 positivi e i 38 negativi preregistrati sono stati eseguiti. MED003 resta aperto; serve review indipendente dei risultati.

## Autorizzazione e comando

Il coordinatore ha assegnato questo unico turno writer/runtime dopo `READY_STATIC43` della review `ses_e880b0fbfffeO52Yuj7PWAIjDB`. La preparazione non autorizzava da sola l'esecuzione. Il turno corrente autorizza soltanto la prima suite 005, non il secondo comando host nel preregistro.

Root e working directory: `/opt/workspace/astroProcessor`.

```sh
/usr/bin/timeout --signal=TERM --kill-after=5s 55s /usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-inventory-followup-005/test_verify.py
```

Un supervisore Python isolato, passato via stdin e non salvato tra le sorgenti, ha applicato prima del processo figlio CPU 30 s, AS 268435456 byte, FSIZE 1048576 byte per stream e core dump 0. Il comando timeout conserva 55 s di wall e 5 s di grazia. Il supervisore prevedeva raccolta entro 62 s e kill del solo gruppo posseduto in caso di superamento; quel ramo non è stato usato. Nessun retry.

Python effettivo: `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`, eseguibile `/usr/bin/python3`. Sistema osservato con `os.uname()`: Linux, kernel `7.0.0-38-generic`, `#38-Ubuntu SMP PREEMPT_DYNAMIC Fri Sep  4 09:10:14 UTC 2026`, x86_64. Nessun comando separato per interrogare versioni di librerie host.

Intervallo UTC del supervisore: `2026-10-26T11:29:58.985706+00:00` fino a `2026-10-26T11:29:59.298306+00:00`. Durata osservata del comando: `0.2189501579850912` s. La suite riporta `Ran 43 tests in 0.109s`; il tempo è una misura, non un'asserzione di correttezza.

## Preflight e preservazione

Prima dell'avvio, il supervisore ha verificato l'assenza di `tests-attempt-001` sotto 005 e di questo report. Ha creato soltanto il nuovo tentativo, con umask 077 e directory 0700. Ha controllato proprietà e tipo delle directory di progetto coinvolte; `fft-pocket`, namespace 005 e tentativo sono privati 0700.

Le letture di autenticazione hanno attraversato ogni componente con `O_NOFOLLOW`, aperto file regolari del proprietario corrente, imposto il limite di 16 MiB e confrontato stat prima/dopo e sul nome. La ricevuta conserva stat completi, byte, righe e SHA256 per 41 record: 6 ancore 003, 11 ulteriori input 003, 8 sorgenti/documenti 005 e 16 storici. Non si tratta di un'autenticazione dell'intera storia del progetto.

Il supervisore ha confrontato i record con il manifest pinned, verificato i record di `preservation-before.json` e l'insieme completo dei file 004. Dopo il test ha riletto gli stessi 41 record: hash, byte e stat risultano identici. I due FIRSTFAIL nominati, i raw 004 e i report 004 sono preservati. Ha inoltre ricontrollato dopo l'esecuzione i pin di manifest e checks 005.

| Artefatto autenticato | SHA256 pre e post |
| --- | --- |
| `preparation-manifest.json` 005 | `a7752c9e8612afc975adac40231c510d6f379b1989f6ddb96c103e56b270c9ef` |
| `preparation-checks.json` 005 | `21e7ae7e654c7145e7111ef790bb461b8edba474a99a55ab8fabc08edbfbd90a` |
| Report preparazione 005 | `766387061ee8662bf1f65dfb294b60c634957397abe4487d5608a38849c7fbd9` |
| `test_verify.py` 005 | `025f56e6fac513072c6c67828f8dad91fa29cb9baebc5f96021df0aeb90f833b` |
| `verify.py` 005 | `303545d4263bfda66f5245e1b6c04228a6ae93a753c4318c8af61eeffb9d67a9` |

L'ambiente figlio non eredita HOME o cache utente. `HOME`, `TMPDIR`, `TMP`, `TEMP`, `XDG_CACHE_HOME`, `XDG_CONFIG_HOME`, `XDG_DATA_HOME`, `XDG_STATE_HOME` e `XDG_RUNTIME_DIR` puntano ciascuno a una directory nuova 0700 sotto il tentativo. PATH è `/usr/bin:/bin`, LANG e LC_ALL sono `C`, `PYTHONDONTWRITEBYTECODE=1`. La ricevuta registra tutti i percorsi.

Il gate globale usa blocchi allocati sotto `ROOT/tmp`: cap 1073741824 byte, riserva 67108864 byte, margine 16777216 byte. Il namespace ha cap 8388608 byte e prenotazione per due stream da 1048576 byte più 262144 byte per ricevute.

| Misura | Prima | Dopo il test, prima della serializzazione finale della ricevuta |
| --- | ---: | ---: |
| Allocazione `ROOT/tmp` | 960348160 | 960425984 |
| Allocazione più riserva e margine | 1044234240 | 1044312064 |
| Allocazione namespace 005 | 90112 | 167936 |
| Spazio filesystem disponibile | 125332807680 | 125332729856 |

Entrambi i gate sono passati. Il controllo conclusivo di consegna, successivo alla scrittura di questo report, è registrato separatamente nel nuovo `completion-summary.json` del tentativo.

## Risultati nominativi

I nomi e gli esiti seguenti provengono dallo stdout originale. Il supervisore li ha confrontati con le 43 definizioni AST preregistrate; nessun nome manca o compare due volte.

```text
LocalChecksums.test_actual_binary_mismatch_despite_true_boolean: ok
LocalChecksums.test_declared_digest_mismatch_despite_true_boolean: ok
LocalChecksums.test_duplicate_checksum_same_count: ok
LocalChecksums.test_duplicate_package_path: ok
LocalChecksums.test_false_checksum_boolean: ok
LocalChecksums.test_mock_checksum_positive: ok
LocalChecksums.test_omitted_checksum: ok
LocalChecksums.test_raw_package_digest_mismatch: ok
StaticRelations.test_added_available_but_unrequired_node: ok
StaticRelations.test_added_duplicate_version_check: ok
StaticRelations.test_added_edge: ok
StaticRelations.test_added_version_check_provider: ok
StaticRelations.test_definition_section_does_not_include_note_names: ok
StaticRelations.test_duplicate_required_node_in_both_derived_tables: ok
StaticRelations.test_malformed_definition_child: ok
StaticRelations.test_malformed_dynamic_row: ok
StaticRelations.test_malformed_need_child: ok
StaticRelations.test_malformed_version_offset: ok
StaticRelations.test_omitted_definition: ok
StaticRelations.test_omitted_edge: ok
StaticRelations.test_omitted_edge_replaced_by_duplicate_same_count: ok
StaticRelations.test_omitted_object: ok
StaticRelations.test_omitted_required_node_in_both_derived_tables: ok
StaticRelations.test_omitted_version_check: ok
StaticRelations.test_omitted_version_check_replaced_by_duplicate_same_count: ok
StaticRelations.test_plural_header_rejects_singular_word: ok
StaticRelations.test_raw_mock_duplicate_cache_candidate: ok
StaticRelations.test_raw_mock_duplicate_requirement_index: ok
StaticRelations.test_raw_mock_duplicate_version_provider: ok
StaticRelations.test_raw_mock_missing_requirement_row: ok
StaticRelations.test_raw_pin_mismatch: ok
StaticRelations.test_saved_libc_single_entry_header_positive: ok
StaticRelations.test_saved_relations_positive: ok
StaticRelations.test_saved_worker_plural_header_positive: ok
StaticRelations.test_single_entry_header_rejects_plural_word: ok
StaticRelations.test_version_address_missing: ok
StaticRelations.test_version_address_rejects_non_hex: ok
StaticRelations.test_version_address_rejects_trailing_text: ok
StaticRelations.test_version_address_rejects_wrong_link_section: ok
StaticRelations.test_version_header_rejects_arbitrary_word: ok
StaticRelations.test_version_header_rejects_non_numeric_count: ok
StaticRelations.test_version_header_rejects_trailing_text: ok
StaticRelations.test_wrong_selected_path: ok
```

Il precedente tentativo 004 aveva già riprodotto il guasto: 21 test, 12 ok, 8 failure, 1 errore, exit 1. Questo turno non lo ha rieseguito. La nuova suite supera il positivo sui raw salvati e i positivi distinti per singolare libc e plurale worker. Il controllo delle relazioni salvate restituisce 6 oggetti, 13 archi, 13 gruppi provider e 54 requisiti. Il positivo checksum confronta 5 insiemi di byte sintetici: non sono checksum host.

## Raw, ricevuta e raccolta processi

Tutti i percorsi di questa tabella sono relativi a `tmp/m1/fft-pocket/runtime-inventory-followup-005/tests-attempt-001/`.

| Artefatto | Byte | Righe | SHA256 |
| --- | ---: | ---: | --- |
| `stdout.raw` | 4687 | 48 | `870430b44b71d1689d9135cb787310182fa662f30c1ccb2a2b0a0c9b483d18ae` |
| `stderr.raw` | 0 | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `receipt.json` | 65205 | 2090 | `63e6a335092c1c57e0eac4099ba288f4c5e5fbe50b1f0ff410e097f1e7051686` |

Il figlio timeout posseduto aveva PID e gruppo `970908`, creato con nuova sessione. Il supervisore lo ha raccolto con `wait`, exit 0. Il controllo `killpg(970908, 0)` ha trovato il gruppo assente sia dopo wait sia nel cleanup finale. Non sono serviti segnali di cleanup. La ricevuta registra `child_collected=true`, `owned_group_absent_final=true`, `slot_free=true`.

## Ambito e limiti aperti

La suite importa soltanto il verificatore locale per chiamarne le funzioni testate; non esegue `verify.py` come script né il suo `main`. Il turno non ha avviato verificatori host, worker FFT, loader, ldd, build, GUI, rete, installazioni, Git, deleghe o indici. Nessun test 006 o test opzionale. Nessuna modifica a sorgenti, configurazioni, piani, fixture, progress o index. Le sole scritture persistenti sono il nuovo tentativo e questo nuovo report.

I runtime host non sono stati aperti dal codice di verifica; gli MD5 testati usano byte mock in memoria. L'interprete usa il normale runtime necessario al proprio avvio: questo non costituisce un inventario o una verifica dei checksum delle librerie host. L'analisi dei raw readelf salvati non è una nuova acquisizione né un decoder ELF indipendente. Version nodes non provano definizioni di simboli, relocation, IFUNC o binding live. La cache salvata non prova la selezione attuale del loader. Il database MD5 locale non è una firma né un'approvazione di licenza.

Restano non coperti separatamente Offset non esadecimale, Link non numerico e header all'ultima riga, come già dichiarato nella review statica. Il turno non ha aggiunto test o modificato il gate per coprirli. Il gap dello stderr originale 001 resta aperto. MED003, binding live e licenze restano aperti anche con questi 43 test superati; nessuna conclusione scientifica FFT o adozione segue dall'esito.

Modello dichiarato dal runtime: `openai/gpt-6.1-sol`, senza override. Costi, token e dettagli di failover non sono esposti. I conteggi di righe in ricevuta descrivono i file autenticati, non una dichiarazione di lettura semantica integrale. La consegna richiede review indipendente dei raw, della ricevuta e di questo report.
