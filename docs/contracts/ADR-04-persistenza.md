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

- Dentro: crash di processo (SIGKILL) in qualunque punto; modifiche accidentali o esterne ai file derivati, rilevate prima dell'uso (identità dev/ino/dimensione/mtime/ctime, digest); symlink e percorsi negli input non fidati (file importati, manifest e contenuto dei bundle di export).
- Fuori: un processo malevolo con lo stesso UID che manipola i file del progetto durante un'operazione (può comunque riscrivere `project.db`); un dispositivo che mente su `fsync` o che corrompe dati già sincronizzati.
- Dentro dal R4 (D17): la perdita di energia secondo il modello POSIX di `fsync`, simulata in spazio utente (sezione «Perdita di energia»).
- Conseguenza: i controlli di identità sono difese contro modifiche rilevabili, non contro una corsa deliberata fra controllo e uso. `SQLITE_OPEN_NOFOLLOW` non è usabile con `/proc/self/fd`: l'identità del temporaneo del backup si controlla subito prima dell'apertura e di nuovo (completa, ctime incluso) prima della pubblicazione.

## Scelte di implementazione (M2.c)

- La classe è `astro::sqlite_dag_store` (implementa `dag_store`); `project_store` nel testo è lo stesso concetto.
- `node_edges` per gli ingressi file usa `input_sha256` e `input_size` (non `input_id`): `put_node` non conosce i record di `inputs`.
- `job_state::queued` non è prodotto in M2; la recovery tratta comunque `queued` come `running`. Aggiunto `interrupted`.
- Tutte le operazioni sui file del progetto sono relative a un fd di directory verificato (`openat`, `O_NOFOLLOW`, `O_DIRECTORY`, `mkdirat`, `renameat`); temporanei con nome casuale (`getrandom`) e `O_EXCL`. `backups/` o `artifacts/` symlink: `security`, `store.symlink` (backup: `schema`, `store.backup_failed` con causa nel contesto).
- `read`/`write` interrotti da segnale (EINTR) e trasferimenti parziali si riprendono: è gestione POSIX del singolo trasferimento, non un retry dopo guasto. Un errore reale non si ripete.
- `stored_file` porta l'id dell'istanza dello store e l'identità del file scritto (dev, ino, dimensione, mtime); `commit_job`, `complete_job` e `add_artifact` accettano solo file scritti da `write_files` di questa istanza e ancora identici (`integrity`, `store.file_identity_changed`; altrimenti `invalid_input`, `store.file_not_written_by_store`). Ruoli ripetuti nel commit: `dag.duplicate_output_role`.
- `set_publish_role` è una mutazione della ricetta: ricollega lo slot, incrementa la generazione, crea uno snapshot (il ruolo è in `snapshot_entries.publish_role`) e azzera il redo.
- Il backup pre-migrazione è creato con `O_CREAT|O_EXCL|O_NOFOLLOW` su nome casuale, fsync sullo stesso fd, identità (dev/ino) verificata prima e dopo il rename (`store.backup_identity`).
- I test di crash simulano la morte del processo (SIGKILL); la perdita di energia ha un test proprio (T-M2R4-04) con il modello della sezione «Perdita di energia».

## Database

`PRAGMA user_version` è la versione di schema; M2 definisce la versione 1. Tabelle minime (nomi C++ dei record fra parentesi):

| Tabella | Campi | Invariante |
|---|---|---|
| `inputs` (`input_record`) | `id`, `path`, `size_bytes`, `sha256`, `role` | Digest e dimensione inseriti nella stessa transazione del record. |
| `nodes` (`node_record`) | `node_key` (PK), `operator_id`, `algorithm_version`, `serialization_version`, `params_canonical`, `precision`, `backend`, `seed` | Riga mai aggiornata dopo l'inserimento. |
| `node_edges` | `child_key`, `ordinal`, `input_name`, `parent_key` o `input_id` | `ordinal` è la posizione in `node_spec.inputs`; nessun ciclo (ADR-05). |
| `artifacts` (`artifact_record`) | `id`, `node_key`, `generation`, `rel_path`, `size_bytes`, `sha256`, `kind`, `present`, `output_role` | Inserita solo dopo il passo 5 della scrittura atomica, nella stessa transazione che chiude il job. `output_role` distingue gli output di un job (distinti fra loro). |
| `slots` (`slot_record`) | `slot_name`, `current_generation`, `target_node_key` (nullo se `detached`), `artifact_id`, `publish_role` (default `primary`), `detached` | `artifact_id` cambia solo nella pubblicazione (ADR-05) e in undo/redo (sotto). La pubblicazione e il ricollegamento usano l'output il cui ruolo è `publish_role`. |
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

Schema più recente con WAL non vuoto (anche solo scoperto dopo la connessione): `schema`, `store.wal_pending_future_schema`. Nessuna apertura può applicare o troncare il WAL (la connessione di sondaggio chiude senza checkpoint); `project.db` e WAL restano invariati.

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
- completo solo se `COMMIT` esiste, il suo digest corrisponde a `manifest.json`, ogni file corrisponde a dimensione e digest (calcolato in streaming, memoria costante) e non ci sono altri file. Altrimenti `integrity`, `export.incomplete`. Un file sostituito o modificato durante la verifica (identità dev/ino/dimensione/mtime/ctime diversa a fine lettura): `integrity`, `export.changed_during_verify`. Il manifest deve avere `versions` completo (product, source_revision, source_dirty, cfitsio, sqlite, openssl), nodi unici e catena chiusa (ogni padre presente); altrimenti `export.manifest_malformed`. L'export non è atomico: il marker lo rende verificabile.

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

ENOSPC si prova iniettando il guasto nelle chiamate di scrittura del prodotto (fault injector), non su un filesystem pieno (una modifica di sistema, vietata dall'`AGENTS.md`). Per l'export i punti di scrittura sono `export.file.{create,write,fsync_file,rename,fsync_dir}`, `export.report.*` (report di export, estensione R4), `export.manifest.*` e `export.commit.*`, più `export.mkdir`. Con ENOSPC iniettato a ciascuno: stop `io`, `export.write` (anche per `mkdir`, la cui causa iniettata resta nel contesto), `errno = 28` nel contesto; nessun file temporaneo residuo; la destinazione non contiene `COMMIT` e `verify_export` non la dichiara completa (assente o `integrity`, `export.incomplete`); gli originali e lo stato dello store restano invariati (T-M2R0-03). Nessun retry e nessun percorso alternativo.

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
## Estensione R4: recovery, resume, report di export, hardlink, perdita di energia

Unità R4 di `plans/M2-residui.md` (D15, D16, D17, D18). Requisiti F-PRJ-06, NFR-REP-01.a (recovery, export), NFR-SEC-03. Schema 1 esteso prima di ogni rilascio: nessun progetto esistente da migrare.

### Schema

- `inputs` ha in più `dev` e `ino` (interi non nulli): identità `(st_dev, st_ino)` dell'originale, letta con `stat` da `add_input` sul percorso registrato. Percorso assente: `io`, `input.missing`; non regolare: `invalid_input`, `store.bad_input`.
- `artifacts` ha in più `job_id` (riferimento a `jobs`, nullo per le righe registrate con `add_artifact` fuori da un commit): il job che ha inserito la riga con `commit_job` o `complete_job`. La generazione è per slot, quindi `(node_key, generazione)` non identifica un job; `job_id` sì.
- `recovery_reports(id, report_canonical)`: report persistiti, in JSON canonico. Righe mai aggiornate né cancellate (trigger come `snapshots`).
- `settings` con chiave `workflow.<nome>`: definizione del workflow (sotto).

### Fase sicura (D16)

Uno slot attaccato ha una **fase** se il suo artefatto pubblicato (`artifact_id`) esiste ed è del nodo target, `present` o no: è il risultato completato della ricetta corrente. Il **job produttore** è il job registrato nella riga dell'artefatto pubblicato (`job_id`); i suoi **output** sono tutte le righe di `artifacts` con quel `job_id`, inserite dalla stessa transazione di commit, provenance compresa, `present` o no. Se `job_id` è nullo, l'unico output è l'artefatto pubblicato. Ogni output si controlla leggendo il file con `openat` e `O_NOFOLLOW` sotto `artifacts/` e con la regola hardlink:

- file assente: difetto `absent` (riga già `present = 0`, o marcata dalla recovery nella stessa apertura), trovato vuoto. Il nome è libero e il nodo si può ricostruire;
- riga `present = 0` e file presente con dimensione e digest registrati (per esempio un file ripristinato dopo la recovery): difetto `absent`, trovato uguale all'atteso. La riga non si aggiorna (nessuna riparazione); il nodo si ricostruisce e la scrittura atomica riusa il nome (passo 4, contenuto uguale);
- file presente con dimensione o digest diversi da quelli registrati, anche se la riga è `present = 0` (la recovery marca le dimensioni diverse senza rimuovere il file): difetto `digest`, con digest atteso e trovato. Il nome è occupato da un contenuto diverso;
- file presente, di dimensione e digest registrati, e riga `present`: output verificato.

La fase è **sicura** se ogni output è verificato, altrimenti è **danneggiata**, con un difetto per ogni output non verificato. Uno slot senza fase (mai pubblicato per il nodo target, o pubblicato per un altro nodo) è da calcolare.

Seconda forma di D16, il checkpoint valido di un nodo: vale per i job che dichiarano checkpoint con risultati parziali durevoli (scheduler, T-M2D-09). In M2 nessun job di prodotto li ha (la calibrazione tiene le righe parziali in memoria): un job interrotto dentro un nodo riparte da capo, e il report non ha un campo per i checkpoint.

### Report di recovery (D15)

A ogni apertura in `read_write`, in quest'ordine:

1. **Analisi senza scritture.** Scansione di `artifacts/` con la regola hardlink, prima della connessione SQLite (sotto); poi, con la connessione, lettura dei job `running` o `queued`, delle righe il cui file manca o ha dimensione diversa, dei file senza riga.
2. **Rimozioni.** `unlink` dei temporanei e degli orfani trovati, poi `fsync` della directory. Il report elenca solo le rimozioni riuscite: se il processo muore prima della transazione, quei file sono già spariti e la recovery successiva non li ritrova.
3. **Transazione.** Job marcati `interrupted`, righe marcate `present = 0`, calcolo delle fasi (sopra) per ogni slot attaccato, persistenza del report.

`recovery_report` (in memoria con `recovery()`, persistito in `recovery_reports`):

| Campo | Contenuto |
|---|---|
| `interrupted_jobs` | Job marcati `interrupted` da questa recovery |
| `removed_temporaries`, `removed_orphans` | Nomi rimossi da questa recovery |
| `artifacts_marked_absent` | Righe marcate `present = 0` da questa recovery |
| `incomplete_results` | Ogni job `interrupted` (anche di aperture precedenti) il cui slot non ha ancora un artefatto `current` del suo nodo: job, slot, `node_key`, generazione. Sono i risultati incompleti, mai pubblicati |
| `safe_phases` | Slot, `node_key`, generazione e id degli output verificati, per slot |
| `damaged_phases` | Slot, `node_key`, generazione e difetti (`artifact_id`, `rel_path`, causa `absent` o `digest`, digest atteso e trovato) |

Il report si persiste se almeno un campo diverso da `safe_phases` non è vuoto e se differisce dall'ultimo report persistito (riaperture ripetute dello stesso stato non aggiungono righe). Un progetto chiuso in modo pulito e intatto non produce righe. `recovery_reports()` li restituisce in ordine di id. Formato `astro.recovery.v1`. Il calcolo delle fasi rilegge gli output di ogni fase a ogni apertura: è un costo lineare nella dimensione dei risultati correnti, scelto per non dichiarare sicura una fase senza averla verificata.

All'apertura non riparte nulla: la recovery propone (report) e il resume è un comando separato.

### Workflow e resume (D15, D16)

Un workflow è un elenco ordinato di passi `(slot, nodo)`: ogni passo esegue un job sullo slot. `run_workflow` richiede che il target di ogni slot sia il nodo del passo (`invalid_input`, `workflow.slot_target`), registra la definizione in `settings` (`workflow.<nome>`, JSON canonico `astro.workflow.v1` con slot e `node_key` in ordine) ed esegue i passi in ordine. Il primo passo fermato chiude il workflow con il suo codice di uscita; i passi successivi non partono. Nome del workflow `[a-z0-9_-]{1,64}`, almeno un passo, slot distinti (`invalid_input`, `workflow.definition`). Un passo deve eseguire il job del proprio slot e del proprio nodo (`invalid_input`, `workflow.step_job`).

`resume_workflow` è il solo modo di riprendere. Prima di eseguire qualunque passo:

1. definizione registrata assente: `resume_mismatch`, `resume.unknown_workflow`;
2. definizione diversa da quella data (passi, ordine, slot, nodi, quindi parametri, versioni, precisione e backend, che entrano nella chiave): `resume_mismatch`, `resume.recipe`, con passo, atteso e trovato nel contesto;
3. target di uno slot diverso dal nodo del passo, o slot staccato o assente: `resume_mismatch`, `resume.recipe`;
4. per ogni passo, classificazione della fase (sopra), per tutti i passi prima di eseguirne uno. Una fase danneggiata con almeno un difetto `digest`, su un output primario o secondario: `integrity`, `resume.artifact_digest` (il nome del file è occupato da un contenuto diverso e il nodo non si può riscrivere). File di una riga `present` sparito dopo la recovery: `io`, `resume.artifact_missing`.

Nessuno di questi errori esegue un passo né apre un job. Poi le fasi sicure si riusano senza job; le altre (slot senza fase, o fase danneggiata solo da difetti `absent`: file mancanti, nomi liberi, o file ripristinati col contenuto registrato, nomi riusabili) si eseguono in ordine, con la stessa regola di arresto di `run_workflow`. La provenance di ogni job eseguito dal resume dichiara il resume (ADR-05). Un resume con ricetta diversa non diventa mai un'esecuzione nuova: per quella serve `run_workflow`.

### Regola hardlink (D18, NFR-SEC-03)

Un file gestito dal progetto con `st_nlink ≠ 1`: `security`, `fs.hardlink`. Una destinazione di export esistente, o un file gestito, con lo stesso `(st_dev, st_ino)` di un originale registrato: `security`, `fs.original_inode`, controllato prima di `fs.hardlink` dove gli originali sono già leggibili. Ogni controllo avviene prima della prima scrittura o rimozione dell'operazione:

- apertura: `project.lock` (sul descrittore), `project.db`, `project.db-wal`, `project.db-shm` e ogni voce di `artifacts/`, prima della connessione SQLite (che crea `-wal` e `-shm`). Qui gli originali non sono ancora leggibili: un hardlink di un originale ha comunque `st_nlink ≥ 2` e si ferma come `fs.hardlink`;
- scrittura atomica: se il nome finale è noto prima del temporaneo (digest dei byte in memoria, o nome dato), il file esistente con quel nome si controlla prima di creare il temporaneo; il controllo si ripete sul file riusato al passo 4 (`EEXIST`);
- export: destinazione esistente (`fs.original_inode`, poi `export.target_exists`) e ogni artefatto da copiare, tutti prima di creare la directory; ogni artefatto si ricontrolla all'apertura per la copia;
- fasi e resume: ogni output riletto per la verifica del digest.

Il controllo vale per i file che il progetto scrive, rimuove, riusa o pubblica; gli originali restano fuori dal progetto e si leggono soltanto. Un rifiuto lascia invariati `project.db`, il contenuto di `artifacts/` e l'originale (digest, inode, `nlink`, `mtime`, permessi).

### Report di export (NFR-REP-01.a, C-10 campo 7)

L'export scrive `export_report.json` (JSON canonico, `astro.export_report.v1`) con il protocollo atomico, dopo i file e prima di `manifest.json`. Il report elenca per ogni file `rel_path`, `size_bytes`, `sha256`, `slot_name`, `generation`, `node_key`, `kind` e `representation` = `artifact_copy` (copia byte per byte dell'artefatto: maschere, varianza e metadati restano quelli dell'artefatto). `non_representable` è l'elenco dei metadati che il formato di destinazione non rappresenta: vuoto, perché l'export non converte formati, con il motivo in `non_representable_reason`. Il manifest ha un quinto campo `report` con `rel_path` = `export_report.json`, `size_bytes` e `sha256`. `verify_export` richiede il campo con quel nome (`invalid_input`, `export.manifest_malformed`), verifica il file come gli altri (assente o digest diverso: `integrity`, `export.incomplete`) e richiede che il report sia JSON canonico con formato noto, lo stesso elenco di file del manifest nello stesso ordine e con gli stessi sei campi (`rel_path`, `size_bytes`, `sha256`, `slot_name`, `generation`, `node_key`), `kind` stringa non vuota, `representation` = `artifact_copy`, `non_representable` vuoto e `non_representable_reason` non vuoto (`integrity`, `export.report_mismatch`). Il nome è riservato: uno slot che lo produrrebbe è rifiutato (`invalid_input`, `export.slot_name`).

### Perdita di energia (D17)

Modello POSIX di `fsync`, simulato in spazio utente dal test T-M2R4-04:

- database: un VFS SQLite di test, sovrapposto a `unix` e scelto con `open_options.vfs`, ricorda per ogni file il contenuto all'ultimo `xSync`. Le scritture non sincronizzate si perdono. Un file mai sincronizzato non esiste; `xDelete` è durevole subito; `-shm` non è durevole (SQLite lo ricostruisce dal WAL);
- artefatti: lo store notifica a un osservatore di test (`open_options.observer`) gli eventi `create`, `fsync`, `rename`, `unlink` e `fsync_dir` della scrittura atomica. Il file riusato al passo 4 (`EEXIST`, contenuto uguale) si sincronizza con `fsync` prima del commit, come il temporaneo, ed emette `fsync`; Il contenuto di un file è durevole all'ultimo `fsync`, vuoto se non c'è; un nome è durevole all'ultimo `fsync` della directory;
- a ogni `xSync`, a ogni `xDelete`, a ogni `fsync` e dopo la chiusura si costruisce lo stato durevole, lo si riapre con il VFS normale e si verificano gli invarianti: `PRAGMA integrity_check` = `ok`; ogni riga `present` ha un file durevole col suo digest; ogni job `completed` ha i suoi output `present`; l'artefatto pubblicato da ogni slot `current` ha un job produttore (`job_id`) `completed` dello stesso nodo; nessun `.tmp-*` né orfano dopo la recovery.

Limiti dichiarati: un dispositivo che mente su `fsync`, il riordino dei metadati del filesystem oltre il modello e la directory del progetto stessa, creata prima della misura, restano fuori. I percorsi d'errore della scrittura atomica non sono nel giornale (li copre T-M2C-04).

### Test dell'estensione

| ID | Clausola |
|---|---|
| T-M2R4-01 | SIGKILL del figlio a ogni punto (confini di passo, hook della calibrazione e dell'import, scrittura atomica, commit) di un workflow di K nodi di calibrazione, e di uno con I passi di import seguiti da K calibrazioni. Dopo la riapertura nessun job nuovo; il report elenca come fasi sicure esattamente i passi già pubblicati, anche con il kill fra due passi; un job interrotto è un risultato incompleto. Dopo `resume_workflow` i passi sicuri non hanno job né provenance nuovi (una sola esecuzione conclusa per nodo, tentativi contati dai job), e i digest di immagine e report coincidono con quelli di un progetto separato eseguito senza kill. Esatta. Un passo fermato (exit 2) o annullato (exit 3), in `run_workflow` e in `resume_workflow`, chiude il workflow con il suo codice e la sua causa: i passi successivi restano `not_reached` e non aprono job. |
| T-M2R4-02 | Resume con passi, ordine, parametri o backend diversi, o con un target di slot cambiato: `resume_mismatch`, nessun job nuovo, nessun file nuovo. Workflow mai registrato: `resume.unknown_workflow`. Fase danneggiata da un difetto `digest` (primario alterato; primario mancante più secondario di dimensione diversa, che la recovery marca `present = 0` lasciando il file): `resume.artifact_digest`, nessun job. Fase danneggiata solo da difetti `absent` (file mancante, o file ripristinato con riga `present = 0`): il passo si riesegue. |
| T-M2R4-03 | Interventi provocati (job in corso, temporaneo, orfano, file mancante, digest alterato, provenance mancante di un job completato): il report della prima riapertura coincide con l'atteso costruito dal test; la seconda riapertura, dopo il ripristino del file mancante, non rimuove né marca nulla e persiste l'analisi cambiata (difetto `absent` con trovato uguale all'atteso); una terza riapertura non aggiunge righe. Il kill fra due passi committati (nessun intervento, nessuna riga persistita, fasi sicure elencate in `recovery()`) è in T-M2R4-01. |
| T-M2R4-04 | Perdita di energia a ogni punto di sync, dopo ogni `xDelete` e dopo la chiusura dello store, con gli invarianti sopra, sia su un workflow di calibrazioni sia su uno con passi di import; lo stato durevole finale ha tutti gli slot `current`. Invariante esatto. |
| T-M2R4-05 | Hardlink di un originale in `artifacts/` (orfano alla riapertura e nome riusato in scrittura), come destinazione di export, un artefatto con `nlink` 2 (riapertura, export, fase, scrittura), `project.db` con `nlink` 2, e nella cache delle miniature di R1 un originale collegato come voce o come temporaneo e una voce con `nlink` 2: stop `security`, nessuna scrittura né rimozione (nella scrittura atomica nessun punto `create`, `write` o `fsync` precede il rifiuto); dell'originale invariati digest, inode, `nlink`, `mtime` e permessi. |
| T-M2R4-06 | Report di export presente, elencato nel manifest con il digest e verificato da `verify_export`; report alterato, assente, fuori dal manifest, con un altro nome, con un elemento in meno o con un solo campo diverso (manifest e `COMMIT` coerenti col report alterato): stop. |

**Integrazione con R1 (unità R4.b).** Il piano chiede in T-M2R4-01 il job di import di R1 su K file più la calibrazione, e in T-M2R4-05 anche la cache delle thumbnails di R1. R4.b estende i due test senza cambiare il prodotto:

- T-M2R4-01 e T-M2R4-04 hanno una seconda esecuzione su un workflow di 2 passi di import (`run_import_job` sui primi due light) seguiti da 2 calibrazioni. Ogni asserzione vale per ogni passo: un passo di import ha 3 output (report JSON, report testuale, provenance) e un passo di calibrazione 4; l'hook `digest.done` del job di import è un punto di kill (`import.digest.done`); la lista `reused` della provenance del resume segue l'ordine dei passi del workflow, non quello alfabetico degli slot del report di recovery. Nella perdita di energia i fsync attesi sono `4·K + 3·I` più quello del file riusato (primo passo, report testuale), K calibrazioni e I import.
- T-M2R4-05 ha un caso sulla cache `cache/thumbnails/` (`thumbnail_cache`, job `thumbnail` e `cache.rebuild`): un originale collegato col nome di una voce, un originale collegato come temporaneo `.tmp-*` e una voce con `nlink` 2 si fermano come `security` `cache.hardlink`, senza scritture né rimozioni (elenco, `mtime` e `ctime` della directory della cache, `nlink`, digest, inode e `mtime` dei file invariati, nessun punto `cache.entry.*` toccato, nessun calcolo, nessun output pubblicato: dello stop resta solo la provenance; il cleanup controlla tutti i temporanei prima di rimuoverne uno).

Limite dichiarato: la cache applica la propria regola `st_nlink = 1` (`cache.hardlink`, `cache.symlink`, `cache.not_regular`) e non conosce gli inode degli originali registrati, quindi non emette `fs.original_inode`. Un originale collegato nella cache ha sempre `st_nlink ≥ 2` e si ferma come `cache.hardlink`. Un file con lo stesso `(st_dev, st_ino)` di un originale e `st_nlink` 1 (originale spostato nella cache) non è coperto né provato da un test (l'originale non è più al suo percorso registrato).

## Fuori da M2

Branch e migrazioni reali (ADR-04.b); asset utente e event log delle maschere dipinte; garbage collection con lease; branch della storia (redo multipli); export di slot non correnti.
