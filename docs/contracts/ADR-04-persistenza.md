# Contratto ADR-04.a: persistenza, autorità e recovery

Stadio: ADR-04.a (registro riga 3391). Decisione: SQLite autorità, JSON per manifest e report ([ADR-M1-stack](../decisions/ADR-M1-stack.md)). Piano §7.5. Requisiti F-PRJ-01, F-PRJ-02, F-PRJ-04, F-PRJ-06, F-PRJ-07, NFR-SEC-03. Unità: M2.c. Errori: [modello comune](ADR-05-dag-provenance.md#modello-derrore-comune).

## Layout del progetto

```
<progetto>/project.db          autorità unica (SQLite)
<progetto>/project.lock        lock esclusivo del processo (flock)
<progetto>/artifacts/          output derivati, nome = <sha256>.<estensione>
<progetto>/backups/            copie pre-migrazione
```

Gli originali stanno fuori dal progetto, referenziati per percorso e digest. Il progetto non li scrive mai (F-PRJ-02).

## Modello di minaccia

- Dentro: crash di processo (SIGKILL) in qualunque punto; modifiche accidentali o esterne ai file derivati, rilevate prima dell'uso (identita' dev/ino/dimensione/mtime/ctime, digest); symlink e percorsi negli input non fidati (file importati, manifest e contenuto dei bundle di export).
- Fuori: un processo malevolo con lo stesso UID che manipola i file del progetto durante un'operazione (puo' comunque riscrivere `project.db`); perdita di corrente e guasti del dispositivo (la durabilita' fsync non e' verificata dai test).
- Conseguenza: i controlli di identita' sono difese contro modifiche rilevabili, non contro una corsa deliberata fra controllo e uso. `SQLITE_OPEN_NOFOLLOW` non e' usabile con `/proc/self/fd`: l'identita' del temporaneo del backup si controlla subito prima dell'apertura e di nuovo (completa, ctime incluso) prima della pubblicazione.

## Scelte di implementazione (M2.c)

- La classe e' `astro::sqlite_dag_store` (implementa `dag_store`); `project_store` nel testo e' lo stesso concetto.
- `node_edges` per gli ingressi file usa `input_sha256` e `input_size` (non `input_id`): `put_node` non conosce i record di `inputs`.
- `job_state::queued` non e' prodotto in M2; la recovery tratta comunque `queued` come `running`. Aggiunto `interrupted`.
- Tutte le operazioni sui file del progetto sono relative a un fd di directory verificato (`openat`, `O_NOFOLLOW`, `O_DIRECTORY`, `mkdirat`, `renameat`); temporanei con nome casuale (`getrandom`) e `O_EXCL`. `backups/` o `artifacts/` symlink: `security`, `store.symlink` (backup: `schema`, `store.backup_failed` con causa nel contesto).
- `read`/`write` interrotti da segnale (EINTR) e trasferimenti parziali si riprendono: e' gestione POSIX del singolo trasferimento, non un retry dopo guasto. Un errore reale non si ripete.
- `stored_file` porta l'id dell'istanza dello store e l'identita' del file scritto (dev, ino, dimensione, mtime); `commit_job`, `complete_job` e `add_artifact` accettano solo file scritti da `write_files` di questa istanza e ancora identici (`integrity`, `store.file_identity_changed`; altrimenti `invalid_input`, `store.file_not_written_by_store`). Ruoli ripetuti nel commit: `dag.duplicate_output_role`.
- `set_publish_role` e' una mutazione della ricetta: ricollega lo slot, incrementa la generazione, crea uno snapshot (il ruolo e' in `snapshot_entries.publish_role`) e azzera il redo.
- Il backup pre-migrazione e' creato con `O_CREAT|O_EXCL|O_NOFOLLOW` su nome casuale, fsync sullo stesso fd, identita' (dev/ino) verificata prima e dopo il rename (`store.backup_identity`).
- I test di crash simulano la morte del processo (SIGKILL); non provano la durabilita' contro perdita di corrente.

## Database

`PRAGMA user_version` è la versione di schema; M2 definisce la versione 1. Tabelle minime (nomi C++ dei record fra parentesi):

| Tabella | Campi | Invariante |
|---|---|---|
| `inputs` (`input_record`) | `id`, `path`, `size_bytes`, `sha256`, `role` | Digest e dimensione inseriti nella stessa transazione del record. |
| `nodes` (`node_record`) | `node_key` (PK), `operator_id`, `algorithm_version`, `serialization_version`, `params_canonical`, `precision`, `backend`, `seed` | Riga mai aggiornata dopo l'inserimento. |
| `node_edges` | `child_key`, `ordinal`, `input_name`, `parent_key` o `input_id` | `ordinal` è la posizione in `node_spec.inputs`; nessun ciclo (ADR-05). |
| `artifacts` (`artifact_record`) | `id`, `node_key`, `generation`, `rel_path`, `size_bytes`, `sha256`, `kind`, `present`, `output_role` | Inserita solo dopo il passo 5 della scrittura atomica, nella stessa transazione che chiude il job. `output_role` distingue gli output di un job (distinti fra loro). |
| `slots` (`slot_record`) | `slot_name`, `current_generation`, `target_node_key` (nullo se `detached`), `artifact_id`, `publish_role` (default `primary`), `detached` | `artifact_id` cambia solo nella pubblicazione (ADR-05) e in undo/redo (sotto). La pubblicazione e il ricollegamento usano l'output il cui ruolo e' `publish_role`. |
| `jobs` (`job_record`) | `id`, `slot_name`, `node_key`, `generation`, `state`, `stop_category`, `stop_code` | `state` ∈ `job_state`. |
| `snapshots` (`snapshot_record`) | `id`, `parent_id` | Storia logica; non è cache. Mai aggiornata né cancellata. |
| `snapshot_entries` | `snapshot_id`, `slot_name`, `node_key` | Inserite con lo snapshot, poi immutabili. |
| `history_cursor` | `current_snapshot_id`, `redo_snapshot_id` | Una sola riga. |
| `settings` | `key`, `value_canonical` | Budget manuale (ADR-06). |

`job_state`: `queued`, `running`, `completed`, `failed`, `cancelled`, `interrupted`. Solo `completed` ha artefatti pubblicati.

## Creazione e apertura

`astro::project_store::create(path)`: directory nuova (esistente: `security`, `store.target_exists`). Crea `project.lock`, `artifacts/`, `backups/`, poi il database con `PRAGMA application_id = 0x41535450`, `journal_mode = WAL`, schema 1, `user_version = 1`, e chiude con `wal_checkpoint(TRUNCATE)`. Solo qui si scrivono `application_id` e `journal_mode`.

`astro::project_store::open(path, open_intent)`, `open_intent` ∈ {`read_write`, `read_only`}, restituisce l'`open_mode` effettivo: `read_write`, `read_only_requested`, `read_only_unknown_schema`.

1. `project.lock` aperto senza `O_CREAT` (assente: `invalid_input`, `store.not_a_project`); `flock` esclusivo non bloccante (occupato: `io`, `store.locked`; nessun retry).
2. Verifica senza connessione SQLite: lettura dei primi 100 byte di `project.db` con `read`. Magic diverso da `SQLite format 3\0` o `application_id` (offset 68) diverso: `invalid_input`, `store.not_a_project`. Nessun file creato o modificato (T-M2C-09).
3. Sola lettura (intenzione o schema ignoto): `project.db-wal` assente o vuoto, connessione `file:...?mode=ro&immutable=1`; nessun `-wal` o `-shm` creato. WAL non vuoto: `schema`, `store.wal_pending_read_only`; il WAL si applica solo aprendo in `read_write` con una versione che conosce lo schema.
4. Lettura-scrittura: connessione normale con i soli pragma di connessione `synchronous = FULL`, `foreign_keys = ON`, `busy_timeout = 0`. `journal_mode` diverso da `wal`: `integrity`, `store.journal_mode`.
5. `PRAGMA quick_check` diverso da `ok`: `integrity`, `store.corrupt`. Nessuna riparazione (T-FBK-01, store corrotto).
6. `user_version` letto dalla connessione. Maggiore del noto: chiusura e riapertura come al punto 3, `open_mode = read_only_unknown_schema`; ogni scrittura produce `schema`, `store.read_only`. Minore: migrazione.
7. Recovery, solo in `read_write`.

Schema piu' recente con WAL non vuoto (anche solo scoperto dopo la connessione): `schema`, `store.wal_pending_future_schema`. Nessuna apertura puo' applicare o troncare il WAL (la connessione di sondaggio chiude senza checkpoint); `project.db` e WAL restano invariati.

## Scrittura atomica

Ogni file derivato, report incluso:

1. crea `artifacts/.tmp-<128 bit casuali esadecimali>` con `O_CREAT | O_EXCL | O_NOFOLLOW`;
2. scrive e calcola SHA-256 sugli stessi byte; dimensione e digest confrontati con quelli attesi, se noti;
3. `fsync` del file;
4. `renameat2(..., RENAME_NOREPLACE)` verso `<sha256>.<estensione>`. Con `EEXIST`: apertura del file esistente con `O_NOFOLLOW`, `fstat` (file regolare), dimensione e SHA-256 ricalcolato. Uguali: il temporaneo si elimina e il file esistente si riusa. Diversi, o non regolare: `security`, `store.digest_name_conflict` (T-M2C-23);
5. `fsync` della directory;
6. un'unica transazione (`commit_job` / `dag_store::complete_job`, usata anche da `run_job`) inserisce `artifacts`, pubblica lo slot se la generazione coincide e chiude il job `completed`. Generazione cambiata: righe come cache, job `failed` con `dag.stale_publish`. Più righe di `artifacts` possono puntare allo stesso file. I file passati al passo 6 sono solo quelli prodotti dal writer atomico (`stored_file` non costruibile altrove); `complete_job` su un record esterno ricalcola digest e dimensione del file.

Ogni chiamata di sistema con esito negativo produce `io` con `errno` nel contesto e lascia il job `failed`. Un `fsync` fallito non si ripete.

Un job con più output li rinomina tutti prima dell'unica transazione del passo 6: tutti pubblicati o nessuno.

## Recovery

Alla riapertura in `read_write`:

- job in `running` o `queued` diventano `interrupted`; mai `completed` (F-PRJ-06);
- file `.tmp-*` in `artifacts/` vengono eliminati;
- file con nome finale senza riga in `artifacts` vengono eliminati: sono cache derivata non confermata;
- righe il cui file manca o ha dimensione diversa: `present = 0`, nodo da ricostruire; nessuna sostituzione del file.

La recovery opera solo dentro `artifacts/`, con `openat` e `O_NOFOLLOW`; un symlink trovato produce `security`, `store.symlink` e la recovery si ferma (NFR-SEC-03). L'elenco delle azioni è restituito in `recovery_report`.

## Migrazione

1. Copia con l'API `sqlite3_backup` in `backups/project.v<N>.db`, `fsync`, `quick_check` della copia. Un errore ferma prima di modificare il progetto (`schema`, `store.backup_failed`).
2. Migrazione in una sola transazione `BEGIN IMMEDIATE`.
3. Errore: rollback, progetto alla versione N intatto, `schema`, `store.migration_failed`.

M2 ha una sola versione di schema: il test usa una migrazione sintetica 1→2 definita solo nel test.

## Snapshot, undo e redo (F-PRJ-04)

Uno snapshot è l'insieme immutabile delle coppie (`slot_name`, `node_key`) di tutti gli slot attaccati. Si crea in una transazione dopo ogni cambio di ricetta (`apply_recipe` con almeno uno slot cambiato, `create_slot`, `retarget_slot`); `parent_id` è lo snapshot corrente; `redo_snapshot_id` diventa nullo. La generazione non si salva: non si ripristina mai un valore storico.

Undo e redo spostano `current_snapshot_id`, in una sola transazione:

- undo: verso `parent_id`; `redo_snapshot_id` = snapshot lasciato;
- redo: verso `redo_snapshot_id`;
- per ogni slot: `target_node_key` = `node_key` della coppia; `current_generation += 1`, così un job ancora in corso diventa stale (ADR-05);
- gli slot esistenti ma assenti dallo snapshot diventano `detached`: `target_node_key` nullo, `artifact_id` nullo (slot `missing`), `current_generation += 1` (un job in corso diventa stale), `start_job` rifiutato con `dag.slot_detached`; ricompaiono con redo o con una nuova ricetta che li nomina;
- per ogni slot: `artifact_id` = artefatto più recente con quel `node_key`, `present = 1` e `output_role = publish_role` dello slot; nessuno: `artifact_id` nullo, slot `missing`, da ricostruire dalla ricetta del nodo con le versioni dichiarate;
- nessuna riga di `nodes`, `node_edges` o `snapshots` si scrive o si riscrive. Un nodo indipendente non si ricalcola.

| Condizione | Categoria | Codice |
|---|---|---|
| Undo senza `parent_id` | `invalid_input` | `history.no_undo` |
| Redo senza `redo_snapshot_id` | `invalid_input` | `history.no_redo` |
| Coppia con `node_key` assente da `nodes` | `integrity` | `history.dangling_node` |
| Progetto in sola lettura | `schema` | `store.read_only` |

## Export con manifest (F-PRJ-07)

Export esplicito di slot `current` in una directory nuova indicata dall'utente:

1. `mkdir` della directory; esistente o symlink: `security`, `export.target_exists`;
2. ogni file col protocollo atomico; nomi relativi validi secondo le regole sotto;
3. `manifest.json` col protocollo atomico;
4. per ultimo `COMMIT`, contenente lo SHA-256 di `manifest.json`, poi `fsync` della directory.

`export_manifest`, JSON canonico, `format = "astro.export.v1"`: per ogni file `rel_path`, `size_bytes`, `sha256`, `slot_name`, `generation`, `node_key`; per ogni nodo della catena `node_spec` completo (parametri, `algorithm_version`, `serialization_version`, `precision`, `backend`, `seed`), digest degli input, ledger delle correzioni; versione del prodotto e delle librerie.

`verify_export(dir)` tratta il manifest come input non fidato:

- `manifest.json` oltre 64 MiB: `limit_exceeded`, `export.manifest_size`; JSON non valido o campi mancanti: `invalid_input`, `export.manifest_malformed`;
- `rel_path` valido solo se: non vuoto, ≤ 4096 byte, componenti ≤ 255 byte, nessun NUL, nessun `/` iniziale o finale, nessun componente vuoto, `.` o `..`, nessun duplicato. Altrimenti `security`, `export.manifest_path`;
- ogni file si apre con `openat2` e `RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS`, poi `fstat` (file regolare). Symlink o non regolare: `security`, `export.manifest_path`;
- completo solo se `COMMIT` esiste, il suo digest corrisponde a `manifest.json`, ogni file corrisponde a dimensione e digest (calcolato in streaming, memoria costante) e non ci sono altri file. Altrimenti `integrity`, `export.incomplete`. Un file sostituito o modificato durante la verifica (identita' dev/ino/dimensione/mtime/ctime diversa a fine lettura): `integrity`, `export.changed_during_verify`. Il manifest deve avere `versions` completo (product, source_revision, source_dirty, cfitsio, sqlite, openssl), nodi unici e catena chiusa (ogni padre presente); altrimenti `export.manifest_malformed`. L'export non è atomico: il marker lo rende verificabile.

| Condizione | Categoria | Codice |
|---|---|---|
| Slot `stale` o `missing` | `stale_generation` | `export.not_current` |
| Destinazione dentro il progetto o uguale a un input registrato | `security` | `export.target_forbidden` |
| Errore di scrittura o `fsync` | `io` | `export.write` |

## Test

| ID | Clausola |
|---|---|
| T-M2C-01 | Crea, salva e riapre: stesso stato logico (input, nodi, archi in ordine, parametri, versioni, artefatti, snapshot) (F-PRJ-01). |
| T-M2C-02 | `SIGKILL` del figlio a ogni passo 1–6 e dentro il commit: riapertura coerente, nessun output parziale `completed`. |
| T-M2C-03 | Orfani temporanei e finali senza riga eliminati, solo dentro `artifacts/`. |
| T-M2C-04 | `fsync` e `rename` falliti per iniezione: job `failed`, nessuna riga, nessun retry. |
| T-M2C-05 | Schema più recente e `read_only` richiesto: scrittura rifiutata; `project.db` invariato byte per byte; nessun `-wal` o `-shm` creato; WAL pendente: stop. |
| T-M2C-06 | Migrazione con backup; guasto iniettato a metà: versione precedente intatta. |
| T-M2C-07 | Database corrotto: stop `integrity`, nessuna riparazione (T-FBK-01). |
| T-M2C-08 | Secondo processo, sia `read_write` sia sola lettura, mentre il primo tiene il lock: stop `store.locked`; con `immutable=1` nessuna lettura di uno stato in scrittura. |
| T-M2C-09 | Database SQLite estraneo, file non SQLite, `project.lock` assente: stop; file estraneo invariato, nessun file creato. |
| T-M2C-10 | Job interrotti marcati `interrupted`. |
| T-M2C-11 | Digest degli originali invariato dopo l'intero workflow M2.e (F-PRJ-02). |
| T-M2C-12 | Symlink in `artifacts/`: recovery ferma, nessuna cancellazione fuori. |
| T-M2C-13 | Snapshot immutabili: nessun `UPDATE`/`DELETE` su `snapshots` e `snapshot_entries`. |
| T-M2C-14 | Cambio di parametro, undo, redo: stato logico identico; `artifact_id` ricollegato; digest della tabella `nodes` invariato. |
| T-M2C-15 | Undo con job in corso: generazione incrementata, pubblicazione del job rifiutata come stale. |
| T-M2C-16 | Undo con artefatto `present = 0`: slot `missing`, ricostruzione con lo stesso digest e riuso del nome (passo 4); nodi indipendenti non ricalcolati. |
| T-M2C-17 | Undo e redo oltre i limiti, nodo mancante, sola lettura: stop. |
| T-M2C-18 | Nuovo snapshot dopo undo: redo azzerato. |
| T-M2C-19 | Manifest completo: un altro processo ricostruisce `node_key` e digest dai soli dati del manifest. |
| T-M2C-20 | `SIGKILL` a ogni passo dell'export: `verify_export` dichiara incompleto ogni bundle senza `COMMIT`. |
| T-M2C-21 | File o manifest alterati dopo `COMMIT`, file in più: incompleto. |
| T-M2C-22 | Export di slot non `current`, destinazione esistente, symlink o dentro il progetto: stop. |
| T-M2C-23 | Due job con output identico: secondo riuso senza errore; file col nome di un digest ma contenuto diverso o non regolare: `security`. |
| T-M2C-24 | Manifest malevolo: `..`, assoluto, NUL, `a//b`, `./a`, duplicato, symlink verso fuori, file non regolare, manifest oltre 64 MiB o malformato: stop, nessuna lettura fuori dalla directory. |
| T-M2C-25 | `create` su directory esistente: stop `store.target_exists`. |

## Aggiunte di R0.b e R1: disco pieno sull'export, report di import, cache delle thumbnails

### Disco pieno sull'export (C-10.a campo 10, D13)

ENOSPC si prova iniettando il guasto nelle chiamate di scrittura del prodotto (fault injector), non su un filesystem pieno (una modifica di sistema, vietata dall'`AGENTS.md`). Per l'export i punti di scrittura sono `export.file.{create,write,fsync_file,rename,fsync_dir}`, `export.manifest.*` e `export.commit.*`, più `export.mkdir`. Con ENOSPC iniettato a ciascuno: stop `io`, `export.write` (anche per `mkdir`, la cui causa iniettata resta nel contesto), `errno = 28` nel contesto; nessun file temporaneo residuo; la destinazione non contiene `COMMIT` e `verify_export` non la dichiara completa (assente o `integrity`, `export.incomplete`); gli originali e lo stato dello store restano invariati (T-M2R0-03). Nessun retry e nessun percorso alternativo.

### Report di import come artefatto (NFR-REP-01.a, F-IO-10)

`run_import_job` pubblica due file per slot: ruolo `primary`, tipo `import_report` (JSON canonico, `format = "astro.import_report.v1"`) e ruolo `report_text`, tipo `import_report_text` (resa testuale dello stesso documento). Il JSON contiene: digest e dimensione del sorgente, gli HDU dell'inventario, geometria, layout, CFA, metadati con origine, fase e linearità con origine, `header_cards`, `uninterpreted_keywords` e il preflight delle operazioni (tabella di [ADR-03](ADR-03-pixel-maschere.md)), con le diagnostiche C-14. Il report riletto dopo la riapertura dello store coincide con l'inventario (T-M2R1-08).

### Cache derivata delle thumbnails (F-IO-05, NFR-DATA-01.a, D3)

```
<progetto>/cache/thumbnails/<chiave>.<digest>.thm
```

- La directory si crea con `mkdirat` sul fd del progetto, `O_NOFOLLOW | O_DIRECTORY`; `cache` o `thumbnails` symlink o non directory: `security`, `cache.symlink`. Il progetto non richiede la cache per aprirsi; la recovery dello store non la tocca (opera solo in `artifacts/`).
- `chiave` = SHA-256 esadecimale di `"astro.cache.thumbnail.v1\n"` + JCS di `{source_sha256, source_size, block_width, block_height, algorithm_version}`; `digest` = SHA-256 del contenuto del file, nel nome. Contenuto: magic `ASTRTHM1`, sette `uint32` little-endian (`source_width`, `source_height`, `planes`, `block_width`, `block_height`, `width`, `height`), poi i campioni f32 e i conteggi u32 little-endian. Deterministico: gli stessi parametri producono gli stessi byte.
- Scrittura col protocollo atomico di questo contratto (temporaneo `O_EXCL`, `fsync`, `renameat2`, `fsync` della directory) con punti di guasto `cache.entry.*`.
- Lettura (`lookup`): ogni voce col prefisso `chiave.` si apre con `O_NOFOLLOW` (symlink: `security`, `cache.symlink`), deve essere regolare con `st_nlink = 1` (altrimenti `security`, `cache.hardlink`, nessuna lettura) e di dimensione ≤ della quota; il suo SHA-256 si ricalcola a ogni lettura. Digest diverso da quello nel nome, formato non valido, o più voci per la stessa chiave: `integrity`, `cache.corrupt`, con `atteso`, `trovato` e `voce` nel contesto. La voce non si serve e non si modifica. Voce assente: `computed`; voce valida: `cache_hit`.
- Memoria: ogni lettura di una voce prenota nel pool 2 volte la sua dimensione (buffer letto e anteprima analizzata) prima di leggerla, sulla dimensione dello stesso fd aperto che è anche il limite della lettura (una crescita concorrente è `limit_exceeded`, `cache.read`); la lettura di una voce da sostituire, 1 volta; il calcolo di una voce, 2 volte la dimensione della voce (anteprima e buffer serializzato) dopo `make_preview`. Prenotazione impossibile: `budget`, `sched.job_exceeds_budget` con `minimo` e `budget`.
- Quota: `cache_options::quota_bytes` (obbligatoria, nessun default). La somma delle dimensioni delle voci più la nuova, oltre la quota: `budget`, `cache.quota_exceeded`, prima di scrivere. Nessuna rimozione automatica di voci per fare posto.
- Cleanup confinato (`cleanup_temporaries`): elimina solo i `.tmp-*` regolari con `st_nlink = 1` dentro `cache/thumbnails/`, con `unlinkat` relativo al fd di directory; un `.tmp-*` symlink o con `st_nlink ≠ 1` è `security` e non si tocca nulla. Elenca ciò che ha rimosso.
- Ricostruzione: solo il job esplicito `cache.rebuild` (sotto). Un'esecuzione fallita del job `thumbnail` non ricostruisce mai (T-M2R1-04); nessun percorso ricostruisce da solo.

Job: `run_thumbnail_job` calcola o riusa la voce e pubblica un riferimento (`thumbnail_ref`, JSON: chiave, voce, digest, esito, geometria); l'esito (`cache_hit` o `computed`) è nella provenance (`details.cache`). `run_cache_rebuild_job` richiede una voce presente per la chiave, registra causa (`cache.corrupt` o `user_request`) e voce sostituita (nome, digest trovato), scrive la nuova voce con rename atomico sul nome finale e rimuove la vecchia solo se il nome è diverso. Assenza della voce: `invalid_input`, `cache.rebuild_nothing`.

### Test di R0.b e R1

| ID | Clausola |
|---|---|
| T-M2R0-03 | ENOSPC iniettato a ogni punto di scrittura dell'export: stop `io` con l'errno, nessun bundle completo secondo `verify_export`, destinazione senza `COMMIT`, originali invariati. Esatta. |
| T-M2R1-04 | Voce alterata di un byte: il job `thumbnail` si ferma con `cache.corrupt` e digest atteso e trovato; nessun output, il file resta identico byte per byte, la provenance non registra `cache.rebuild`. Esatta. |
| T-M2R1-08 | Il report di import riletto dopo la riapertura è coerente con l'inventario. |
| T-M2R1-11 | `cache.rebuild` sulla voce corrotta: nuova voce identica byte per byte all'originale, provenance con job, causa e voce sostituita; la richiesta successiva dà `cache_hit`. Esatta. |
| T-M2R1-12 | Voce assente: `computed`; valida: `cache_hit` senza ricalcolo (contatore); quota superata, symlink e hardlink nella cache: stop. Cleanup confinato. |

## Fuori da M2

Branch e migrazioni reali (ADR-04.b); asset utente e event log delle maschere dipinte; garbage collection con lease; branch della storia (redo multipli); export di slot non correnti.
