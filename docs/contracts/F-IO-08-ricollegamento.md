# Contratto F-IO-08: originali mancanti, spostati e ricollegamento (M3.a)

Stadio: F-IO-08, P0, M3.a (registro riga 3163; testo riga 1819 del piano). Archi: F-IO-08 `requires_all` F-PRJ-01 e F-IO-03 (righe 3693, 3694); M3.a `requires_all` F-IO-08 (riga 4422). Lacuna chiusa: L3 di `plans/M3a.md`. Estende [ADR-04](ADR-04-persistenza.md) (schema 1, `inputs`, snapshot, undo e redo, recovery, regola hardlink). Dipendenze: [BASE](BASE-confini.md) (OB-11, OB-12, emendamento L1), [F-IO-03](F-IO-03-classificazione.md) (CL-02, CL-03, CL-04, CL-05), [ADR-05](ADR-05-dag-provenance.md) (identità degli input, chiave del nodo). Errori: [modello comune](ADR-05-dag-provenance.md#modello-derrore-comune).

Accettazione (riga 1819): «File mancanti o spostati sono rilevati e ricollegabili senza corrompere il progetto; il ricollegamento verifica l'hash; i nodi dipendenti restano bloccati e il motivo è visibile; il salvataggio non perde i riferimenti risolti.»

Regole di lettura: come in F-IO-03. Ogni clausola è una **regola** (con il test che la verifica), un **fatto** (con la fonte) o un'**ipotesi** o un **limite** dichiarati. Ogni obbligo `RL-NN` ha un solo **proprietario**; i nomi nuovi di funzione e di colonna sono proposte, il proprietario resta unico.

Perché un file a parte e non una sezione di ADR-04: ADR-04 è il contratto di M2, già chiuso, con le estensioni R0.b, R1 e R4; F-IO-08 è un'unità di M3.a con propri ID di test, proprietari e review. Un file separato tiene il delta di review limitato a questo testo e lascia invariate le clausole di M2. ADR-04 ne riporta solo il rinvio («Estensione F-IO-08»).

Fuori da F-IO-08:

- ricerca automatica di un originale, per nome o per contenuto, in qualunque directory: vietata (RL-02);
- esecuzione di un nodo con i percorsi presi dallo store: oggi i job ricevono percorso e digest dal chiamante (`import_request`, `import_job.hpp:28`); la composizione dei job dal progetto è di F-IO-03 (CL-04) e del job `.b` (M3.c), che seguono il vincolo di §7;
- interfaccia grafica dello stato degli input e del ricollegamento (F-UX-01, M4): si progetta col proprietario prima di ogni lavoro sulla GUI (AGENTS.md, «Autonomia»). Qui solo funzioni del core chiamabili da CLI e test.

## 1. Fatti dal codice (commit `311fb7a`)

Letti con `codegraph explore`, `grep` e `sed -n` su `src/core/`; nessuna build, nessun test eseguito.

- `inputs` ha `id`, `path`, `size_bytes`, `sha256`, `role`, `dev`, `ino` e nessun trigger (`sqlite_project.cpp:25-26`). `add_input` (`sqlite_dag_store.cpp:500-526`) legge `(st_dev, st_ino)` con `stat` sul percorso dato (che segue i symlink), con `io` `input.missing` se assente e `invalid_input` `store.bad_input` se non regolare; non legge il contenuto e accetta percorsi relativi. `inputs()` (`:528-534`) restituisce le righe in ordine di id.
- La chiave del nodo non dipende dal percorso. Gli archi verso file portano solo `input_sha256` e `input_size` (`insert_node_locked`, `sqlite_dag_store.cpp:119-131`); i nodi di I/O hanno come ingresso il solo digest (`io_node`, `import_job.cpp:40-45`; `make_import_node`, `:258-263`, parametri senza percorso). La provenance registra i digest degli input, non i percorsi (`provenance.cpp:96-98`).
- `(dev, ino)` degli originali servono alla regola hardlink (ADR-04, D18): `original_inodes` (`sqlite_recovery.cpp:29-34`) li legge da `inputs` per la recovery (`:314-320`), le fasi (`:252`) e la scrittura atomica (`sqlite_files.cpp:83`); l'export li confronta con la destinazione (`project_export.cpp:56-72`, `:243-245`), e vieta come destinazione il percorso di un input registrato (`:63-67`).
- Undo e redo (`move_history`, `sqlite_history.cpp:47-95`) aggiornano solo `slots` e `history_cursor`; la recovery (`recover`, `sqlite_recovery.cpp:311`) opera solo in `artifacts/`. Nessuno dei due legge o scrive `inputs`, salvo il controllo degli inode.
- `reverify_input` (`hash.cpp:136-147`) riapre il percorso, confronta la dimensione prima di leggere (`integrity` `input.size_changed`), calcola il digest a pezzi con il controllo del cancel prima di ogni pezzo (`digest_input`, `:123`), lo confronta con `require_input_digest` (`:87`, unico emettitore di `input.digest_mismatch`) e verifica che il file sia invariato durante la lettura (`check_unchanged`). Restituisce `void`; la chiamano `calibration_job.cpp:293`, `calibration_blocks.cpp:387`, `master.cpp:1065`, `import_job.cpp:204`, tutti ignorando un valore di ritorno.

Conseguenza: un ricollegamento che conserva digest e dimensione non cambia alcuna chiave di nodo, alcun arco, alcuno snapshot né alcuna provenance. È questo che permette lo sblocco senza ricalcolo (RL-07).

## 2. Record della posizione

**RL-01 — Identità immutabile, posizione corrente unica, storia append-only.**

- Decisione: `inputs` resta la sola casa della **posizione corrente** di un originale (`path`, `dev`, `ino`); l'identità (`id`, `sha256`, `size_bytes`, `role`, `format`) non cambia mai. Ogni cambio di posizione lascia una riga in una tabella append-only. Motivo: i consumatori della posizione (`inputs()`, `original_inodes`, il controllo dell'export) leggono già `inputs` e restano invariati; con righe di posizione in una tabella separata ognuno dovrebbe scegliere la revisione corrente, e un consumatore che leggesse la prima riga userebbe un percorso vecchio. Inoltre l'inode di un file vecchio, se cancellato, può essere riusato da un file qualsiasi, anche da un artefatto: tenerlo nell'insieme degli originali darebbe uno stop `fs.original_inode` falso. L'insieme degli originali contiene quindi solo le posizioni correnti.
- Schema 1, esteso prima di ogni rilascio come in ADR-04 «Estensione R4» (nessun progetto rilasciato da migrare):
  - `inputs.format` (testo non nullo): la dichiarazione di formato del confine (BASE §2.5, emendamento L1: `fits`, `raw`), validata contro l'elenco unico delle dichiarazioni. Serve a riaprire il candidato di un ricollegamento con il limite e i codici del suo formato, che il confine richiede dichiarato e mai dedotto. Valore fuori elenco o vuoto: `invalid_input`, `store.bad_input`. Chi registra lo passa: F-IO-03 CL-04 passa il formato del tipo di job che sceglie (`io.import` → `fits`, `io.raw_import` → `raw`); i chiamanti di M2 passano `fits`;
  - tabella `input_relinks`: `input_id` (riferimento a `inputs.id`), `revision` (intero ≥ 1, chiave primaria con `input_id`), `old_path`, `old_dev`, `old_ino`, `new_path`, `new_dev`, `new_ino`, `previous_status` (motivo di RL-02 osservato subito prima, o `present`), `record_version` = 1;
  - trigger: `UPDATE` di `inputs` che cambia `id`, `sha256`, `size_bytes`, `role` o `format`: abort; `UPDATE` di `inputs` la cui nuova terna (`path`, `dev`, `ino`) non è uguale a `new_*` della riga di `input_relinks` di revisione massima per quell'input: abort; `DELETE` di `inputs`: abort; `UPDATE` e `DELETE` di `input_relinks`: abort (come `snapshots`). Così la posizione cambia solo insieme alla sua riga di storia.
- Proprietario: `schema_v1` (`sqlite_project.cpp`) per DDL e trigger; `sqlite_dag_store::add_input` per la validazione di `format`.
- Prova: T-M3A-08d (trigger), T-M3A-08b (formato).

## 3. Rilevamento

**RL-02 — Stato degli originali solo sui percorsi registrati, solo a richiesta.**

- `input_status()` restituisce, per ogni riga di `inputs` in ordine di id, lo stato della posizione corrente ottenuto con un solo `stat` sul percorso registrato (che segue i symlink, come `add_input`). Vale la prima riga della tabella, nell'ordine, che descrive l'osservazione:

| Osservazione | Stato | Motivo |
|---|---|---|
| `ENOENT` o `ENOTDIR` | `missing` | `input.missing` |
| altro errore di `stat` | `inaccessible` | `input.stat_failed`, con `errno` e testo |
| non regolare | `changed` | `input.not_regular` |
| file regolare con `(st_dev, st_ino)` diversi dal record | `changed` | `input.modified` |
| stesso `(st_dev, st_ino)`, dimensione diversa | `changed` | `input.size_changed` |
| file regolare, `(st_dev, st_ino)` e dimensione uguali al record | `present` | — |

  Ogni voce riporta `input_id`, percorso, digest e dimensione registrati, `(dev, ino)` registrati e, se `stat` è riuscito, trovati. I codici `input.missing`, `input.modified`, `input.size_changed` hanno lo stesso significato che hanno nel confine (BASE OB-11); `input.not_regular` e `input.stat_failed` sono nuovi (§9). Un errore su un input non interrompe l'esame degli altri: è il suo stato, con l'`errno`, non un errore ingoiato.
- Regole:
  - nessuna ricerca: `input_status` non elenca directory, non apre file, non confronta nomi né contenuti di file diversi da quelli registrati. Un file con lo stesso nome o lo stesso contenuto altrove non cambia lo stato e non cambia il record. Nessun ricollegamento automatico, nemmeno per nome identico nella stessa directory;
  - solo a richiesta: lo chiama il chiamante (CLI, test, in M4 la GUI). Né l'apertura dello store né la recovery lo chiamano: un progetto si apre identico con gli originali presenti, spostati o cancellati, e la recovery resta confinata ad `artifacts/` (ADR-04);
  - nessuna scrittura: lo stato non si persiste. È un'osservazione del filesystem, rifatta a ogni chiamata; uno stato salvato diventerebbe falso al primo spostamento successivo. Funziona anche su uno store in sola lettura;
  - nessuna lettura del contenuto: il digest si verifica solo nel confine all'uso (OB-11) e nel ricollegamento (RL-04).
- Proprietario: `sqlite_dag_store::input_status` (`sqlite_dag_store.cpp`, accanto ad `add_input`).
- Prova: T-M3A-07a, 07b, 07c.

## 4. Nodi bloccati

**RL-03 — Blocco derivato, con motivo, per slot.**

- `blocked_slots(stati)` è una funzione pura delle righe dello store e degli stati di RL-02 passati come argomento (nessun `stat`). Per ogni slot attaccato (gli slot `detached` non hanno target e non si esaminano), in ordine di nome:
  1. gli archi verso file dell'antenato transitivo del nodo target, cioè le coppie (`input_sha256`, `input_size`) di `node_edges` del target e dei suoi padri, in ordine di digest e dimensione;
  2. per ogni arco, gli input registrati con lo stesso digest e la stessa dimensione;
  3. esito dell'arco: `available` se almeno uno di essi è `present`, con l'elenco ordinato dei loro `input_id`; `blocked` altrimenti, con l'elenco di quegli input e del loro motivo; `blocked` con motivo `input.unregistered` se nessun input registrato ha quel digest.
- Lo slot è `blocked` se almeno un arco lo è; l'uscita porta slot, `node_key` e archi bloccati con i motivi (JSON canonico `astro.input_status.v1`, insieme alle voci di RL-02). È il «motivo visibile» dell'accettazione.
- Copie con lo stesso digest: un arco è disponibile se una copia registrata è presente, perché il contenuto è identificato dal digest (ADR-05) e il confine lo riverifica all'uso; nessun record cambia. Coerente con F-IO-03 CL-05, che tratta copie concordi come un solo input.
- Il blocco riguarda l'esecuzione, non i risultati: lo stato dello slot (`current`, `stale`, `missing`), il suo artefatto e la sua generazione non cambiano. Un risultato già pubblicato resta leggibile, perché sta in `artifacts/`.
- Esecuzione di un nodo bloccato: il job legge l'input dal confine, che si ferma con `io` `input.missing` (o `integrity` `input.modified`, `input.digest_mismatch`) prima di usare un pixel, con il codice nel record del job e nella provenance (OB-11). Decisione: il runner non aggiunge un secondo controllo di presenza, perché sarebbero due proprietari dello stesso obbligo; nessun percorso alternativo, nessuna copia sostitutiva.
- Limite dichiarato, `input.modified`: il confine confronta il contenuto, non l'inode registrato (`check_path_names` confronta il percorso col descrittore appena aperto). Se al percorso registrato c'è un file diverso con gli stessi byte, nessuno impedisce l'esecuzione: il job usa byte verificati per digest e dimensione, quindi l'input giusto (ADR-05), e il risultato è quello del nodo. Se i byte differiscono, il confine si ferma con `input.digest_mismatch` o `input.size_changed`. Per lo stato `input.modified` il blocco di RL-03 è quindi un avviso visibile, che chiede all'utente il ricollegamento (RL-05) per registrare la nuova identità; per `input.missing`, `input.not_regular` e `input.stat_failed` l'esecuzione si ferma sempre nel confine.
- Proprietario: `sqlite_dag_store::blocked_slots`.
- Prova: T-M3A-09a, 09e, 09f.

## 5. Ricollegamento

**RL-04 — Il candidato si verifica nel confine, per dimensione e SHA-256.**

- La verifica del candidato è `reverify_input` (`hash.cpp`), con la dichiarazione di formato di `inputs.format` (BASE §2.5, emendamento L1, «Regola, riverifica») e `opened` nullo. Nell'ordine:
  1. apertura col protocollo unico del confine (OB-12: `O_RDONLY`, file regolare, limite del formato prima di ogni lettura);
  2. dimensione diversa dal record: `integrity` `input.size_changed`, senza leggere un byte del contenuto;
  3. SHA-256 a pezzi di un buffer di lavoro fisso, con il cancel prima di ogni pezzo; digest diverso: `integrity` `input.digest_mismatch` (`require_input_digest`, unico emettitore);
  4. file cambiato durante la lettura: `integrity` `input.modified` o `input.size_changed` (`check_unchanged`).
- Unica modifica a `reverify_input`: restituisce `(st_dev, st_ino)` del descrittore verificato, letti con `fstat` sullo stesso descrittore (`fits_detail::identity_of`, `fits.hpp`). Il ricollegamento registra proprio l'identità del file verificato, non quella di un secondo `stat`. I quattro chiamanti attuali ignorano il valore e non cambiano (§1). Nessuna seconda funzione di verifica: i digest si confrontano solo in `hash.cpp` (uscita di U0).
- Rifiuto: lo stop del confine si propaga invariato, con percorso, atteso e letto nel contesto. Nessuna sostituzione, nessun «accetta comunque», nessun ricollegamento parziale.
- Proprietario: `reverify_input` (`hash.cpp`).
- Prova: T-M3A-08a, 08b.

**RL-05 — Un'unica funzione cambia la posizione, in una transazione.**

- `sqlite_dag_store::relink_input(input_id, nuovo_percorso, work, check_cancel)`, nell'ordine:
  1. store in sola lettura: `schema`, `store.read_only`;
  2. `input_id` assente: `invalid_input`, `relink.unknown_input`;
  3. percorso non assoluto: `invalid_input`, `relink.path_not_absolute`. Il percorso si verifica e si registra esattamente com'è dato, byte per byte, senza normalizzazione: una normalizzazione lessicale (`..` dopo un symlink) potrebbe indicare un file diverso da quello verificato;
  4. percorso dentro il progetto, dopo la risoluzione dei symlink del prefisso esistente (`weakly_canonical`, la stessa regola di `export.target_forbidden`, `project_export.cpp:45-60`): `security`, `relink.target_forbidden`. Un originale non sta mai nel progetto (ADR-04, «Layout»);
  5. `stat` del candidato, prima di leggerne il contenuto: identità uguale a quella di un file gestito dal progetto, cioè `project.lock`, `project.db`, `project.db-wal`, `project.db-shm`, ogni voce di `artifacts/` e di `cache/thumbnails/` (scansione con la regola di ADR-04, «Regola hardlink»): `security`, `fs.original_inode`. Così un collegamento fisico esterno di un file che il progetto scrive non diventa mai un originale (RL-08), e la recovery successiva non si fermerebbe sullo stesso inode (`sqlite_recovery.cpp:314-320`);
  6. verifica del candidato (RL-04); identità del descrittore verificato diversa da quella del passo 5 (file sostituito nel frattempo): `integrity`, `relink.identity_changed`;
  7. una sola transazione: stato precedente dell'input calcolato dalla stessa funzione di RL-02 (nessuna seconda classificazione dello stato), `INSERT` in `input_relinks` con `revision` = massimo + 1, poi `UPDATE` di `inputs` (`path`, `dev`, `ino`); punto di guasto `relink.commit` del `fault_injector` dello store prima del `COMMIT`.
- Ammesso anche se la posizione corrente è `present` (l'utente sposta di proposito il riferimento) e anche verso lo stesso percorso con un file nuovo (per esempio un file ripristinato da backup, stato `input.modified`): è sempre una richiesta esplicita, con la stessa verifica.
- Tutto ciò che precede il `COMMIT` lascia lo store invariato: ogni stop dei passi 1–6 avviene prima della transazione; un errore nella transazione la annulla.
- Proprietario: `sqlite_dag_store::relink_input` (`sqlite_dag_store.cpp`), unica funzione che scrive `input_relinks` e aggiorna `inputs`.
- Prova: T-M3A-08a, 08b, 08d.

## 6. Storia, undo e redo, recovery

**RL-06 — Il ricollegamento non è una mutazione della ricetta.**

- Il ricollegamento non crea snapshot, non cambia `history_cursor` (in particolare non azzera il redo), non incrementa generazioni, non tocca `nodes`, `node_edges`, `slots`, `artifacts`, `jobs`, `snapshots`, `snapshot_entries`, `recovery_reports`, `settings`, né le revisioni di `input_classifications` (F-IO-03 CL-03: il ricollegamento conserva `input_id`, quindi la classificazione).
- Undo e redo non ripristinano una posizione precedente. Decisione: la posizione è un fatto del filesystem, non una scelta della ricetta, e la chiave dei nodi non ne dipende (§1); tornare a un percorso noto come mancante bloccherebbe di nuovo i nodi senza alcun beneficio. Per tornare a un percorso precedente si esegue un nuovo ricollegamento, con la stessa verifica.
- Recovery: il ricollegamento scrive solo nel database (`project.db` e WAL), nessun file in `artifacts/`, quindi la recovery non ha nulla da riparare. L'atomicità è della transazione SQLite: un crash prima del `COMMIT` lascia record e storia precedenti, dopo il `COMMIT` quelli nuovi. I trigger di RL-01 impediscono a un'altra scrittura di cambiare la posizione senza la sua riga di storia, non l'inserimento di una riga di storia isolata. La recovery non esamina gli originali (RL-02).
- Salvataggio: con `synchronous = FULL` (ADR-04, «Creazione e apertura») il ricollegamento committato sopravvive alla chiusura; la riapertura restituisce da `inputs()` il percorso e l'identità nuovi. Il roundtrip di F-PRJ-01 (T-M2C-01) include `inputs.format` e `input_relinks`.
- Proprietario: `relink_input` (sono le scritture che non fa). Undo, redo e recovery non cambiano.
- Prova: T-M3A-09b, 09c, 09d; T-M3A-08d.

**RL-07 — Sblocco senza ricalcolo.**

- Dopo un ricollegamento accettato, `blocked_slots` sugli stati nuovi non riporta più l'arco. Gli slot restano nello stato che avevano: un `current` resta `current`, con lo stesso artefatto e la stessa generazione; nessun job si apre.
- Fatto, per costruzione: digest e dimensione non cambiano, quindi la chiave del nodo, gli archi e la fase sicura (ADR-04, «Fase sicura») restano gli stessi; `resume_workflow` riusa le fasi sicure senza job.
- Uno slot senza fase, o con una fase danneggiata solo da difetti `absent` (per esempio l'artefatto pubblicato con `present = 0`; ADR-04, «Fase sicura»), si ricalcola solo con un'esecuzione esplicita, che legge dal nuovo percorso con la verifica di OB-11 e riusa il nome del file se il contenuto coincide (ADR-04, passo 4).
- Proprietario: `blocked_slots` per l'esito sbloccato; l'assenza di job e di scritture è RL-06 (`relink_input`). Il test lo prova sull'intera catena.
- Prova: T-M3A-09b.

## 7. Originali in sola lettura

**RL-08 — Né il rilevamento né il ricollegamento scrivono un originale.**

- `input_status` usa solo `stat`. `relink_input` apre il candidato solo col protocollo unico del confine (OB-12, `O_RDONLY`) e non scrive, non rinomina, non sposta, non cancella e non cambia i permessi né del candidato né del file alla posizione precedente. Nessun file si crea fuori dal progetto.
- Vincolo per chi compone i job dal progetto (F-IO-03 CL-04, il job `.b` di M3.c): il percorso di un input si prende da `inputs()` (posizione corrente), mai da un elenco proprio o da un percorso precedente. Il proprietario della posizione resta `relink_input`.
- Proprietario: il protocollo di apertura del confine (OB-12, già esistente); questo contratto non aggiunge aperture.
- Prova: T-M3A-08c.

## 8. Unità di codice

Stima, non misurata: circa 250 righe di prodotto (DDL e trigger, `format` in `add_input`, `input_status`, `blocked_slots`, `relink_input`, ritorno di `reverify_input`), sotto il tetto di ~400 di `plans/M3a.md`. Una sola unità (U9). Prima di cambiare `add_input` e `reverify_input`: `codegraph impact` su entrambi; oggi `add_input` ha 6 chiamanti (test `test_store.cpp`, `test_recovery.cpp`, `test_export.cpp`), che passano il formato. U9 richiede U0 (dichiarazioni di formato nel confine) e U8a (CL-04 che passa il formato).

## 9. Errori aggiunti

| Condizione | Categoria | Codice |
|---|---|---|
| `input_id` sconosciuto nel ricollegamento | `invalid_input` | `relink.unknown_input` |
| Percorso del ricollegamento non assoluto | `invalid_input` | `relink.path_not_absolute` |
| Percorso del ricollegamento dentro il progetto | `security` | `relink.target_forbidden` |
| Candidato sostituito fra il `stat` e la verifica | `integrity` | `relink.identity_changed` |
| Formato assente o fuori elenco in `add_input` | `invalid_input` | `store.bad_input` |

Motivi di stato (RL-02, RL-03), non errori: `input.not_regular`, `input.stat_failed`, `input.unregistered`, oltre a `input.missing`, `input.modified`, `input.size_changed`. Gli stop della verifica del candidato sono quelli del confine (RL-04); `fs.original_inode` è quello di ADR-04; `store.read_only` quello di ADR-04.

## 10. Test pianificati

Verità di riferimento indipendenti dall'implementazione:

- file scritti dal generatore delle fixture (FITS con header noti, DNG `FIX-SYN-DNG-01`), con SHA-256 e dimensione calcolati dal generatore;
- identità `(st_dev, st_ino)`, `nlink`, `mtime`, `ctime` e permessi letti dal test con `stat`;
- righe di `inputs`, `input_relinks` e di tutte le altre tabelle lette con SQL diretto, non con l'API; `sqlite3_total_changes` per l'assenza di scritture;
- tabelle degli stati e dei blocchi attesi scritte a mano dall'autore dei test.

Criteri tutti esatti o binari: nessuna tolleranza. Le lettere distinguono i casi di uno stesso ID.

| ID | Caso | Verifica | Criterio |
|---|---|---|---|
| T-M3A-07 | a | Progetto con sette input registrati e import completati; poi: input 1 rinominato nella stessa directory; input 2 spostato in una sottodirectory con lo stesso nome; input 3 cancellato, con un'esca: in una directory sorella un file con lo stesso nome e lo stesso contenuto; input 4 sostituito allo stesso percorso da una copia byte-identica (inode nuovo); input 5 con un byte in più; al percorso dell'input 6 una directory; input 7 intatto. Chiusura e riapertura in `read_write` e in sola lettura | Apertura riuscita in entrambi i modi; `quick_check` = `ok`; report di recovery uguale a quello di un progetto di controllo con gli originali intatti; tutte le tabelle (SQL) e `artifacts/` (elenco e digest) invariati; `input_status` uguale alla tabella scritta a mano: 1, 2 e 3 `input.missing`, 4 `input.modified`, 5 `input.size_changed`, 6 `input.not_regular`, 7 `present`; file spostati ed esca senza effetto: righe di `inputs` invariate, nessuna riga in `input_relinks` |
| T-M3A-07 | b | `input_status` su uno store aperto in sola lettura e in lettura-scrittura; mutazione che inserisce una scrittura dello stato o un ricollegamento per nome | Nessuna scrittura (`total_changes` invariato; `project.db` e WAL identici byte per byte in sola lettura); la mutazione fa fallire 07a o 07b |
| T-M3A-07 | c | Directory padre di un input senza permesso di esecuzione (test eseguito da un utente non root) insieme a input presenti e mancanti | Quell'input `inaccessible` `input.stat_failed` con `errno` = 13; gli altri con il loro stato; nessuno stop della funzione |
| T-M3A-08 | a | Ricollegamento dell'input spostato (FITS) e di un input RAW (`FIX-SYN-DNG-01`, `format = raw`) al nuovo percorso assoluto, con byte identici; ricollegamento allo stesso percorso dell'input 4 di 07a; ricollegamento di un input `present` a una sua copia altrove; percorso con un symlink seguito da `..` (`/a/link/../f`, con `link` che punta a un'altra directory) | Accettati; in `inputs` solo `path`, `dev`, `ino` cambiati, uguali al percorso dato byte per byte (anche nel caso con `..`, con l'identità del file a cui quel percorso porta, diversa da quella di `/a/f`) e all'identità letta dal test; `id`, `sha256`, `size_bytes`, `role`, `format` invariati; una riga di `input_relinks` per ricollegamento con revisione progressiva, terne vecchia e nuova e `previous_status` uguale a 07a |
| T-M3A-08 | b | Candidati: un byte cambiato a parità di dimensione; un byte in più e uno in meno; assente; directory; percorso relativo; percorso dentro il progetto e symlink esterno che punta dentro; collegamento fisico esterno di un artefatto con lo stesso contenuto dell'input, e di `project.lock`, `project.db`, `project.db-wal`, `project.db-shm` e di una voce di `cache/thumbnails/` (contenuto qualsiasi); candidato sostituito fra il `stat` e la verifica da un file con gli stessi byte e un inode diverso (punto `input.open` del `job_probe`); `input_id` sconosciuto; store in sola lettura; file allungato durante la lettura (punto `input.read` del `job_probe`); cancel durante il digest; `add_input` con formato vuoto e fuori elenco | Nell'ordine: `input.digest_mismatch`; `input.size_changed` con zero letture del contenuto (conteggio del punto `input.read`); `input.missing`; `<formato>.not_regular`; `relink.path_not_absolute`; `relink.target_forbidden` (entrambi); `fs.original_inode` per ogni collegamento, con zero letture del contenuto; `relink.identity_changed`; `relink.unknown_input`; `store.read_only`; `input.modified` o `input.size_changed`; annullamento; `store.bad_input`. In ogni rifiuto tutte le tabelle invariate (SQL) e `total_changes` invariato dall'inizio della chiamata |
| T-M3A-08 | c | Prima e dopo ogni caso di 08a e 08b, esclusi i file che il test stesso modifica (allungamento durante la lettura, sostituzione fra `stat` e verifica) | Del candidato e del file alla posizione precedente, se esiste: digest, dimensione, inode, `nlink`, `mtime`, `ctime` e permessi invariati; elenco e `mtime` delle loro directory invariati; nei casi in cui il test stesso sostituisce o allunga il candidato, il confronto delle directory si fa con lo stato registrato dal test subito dopo il proprio intervento; nei soli casi che arrivano all'apertura del candidato, descrittore aperto con accesso `O_RDONLY` (flag letti da `/proc/self/fdinfo` al punto del `job_probe` che riporta le aperture) |
| T-M3A-08 | d | Guasto al punto `relink.commit`; `SIGKILL` del figlio fra `INSERT` e `UPDATE` e dentro il `COMMIT`; poi riapertura. In SQL diretto: `UPDATE` di `inputs` su `sha256`, `size_bytes`, `role`, `format`, `id`; `UPDATE` di `path` senza riga di storia corrispondente; `DELETE` di `inputs`; `UPDATE` e `DELETE` di `input_relinks` | Guasto: record e storia precedenti; dopo il kill: o record e storia precedenti o entrambi nuovi, mai uno solo; ogni istruzione SQL abortita dal trigger |
| T-M3A-09 | a | Progetto con import degli input A e B e una catena di calibrazione (M2) che dipende da A, più uno slot indipendente su B, tutti `current`; A spostato. `blocked_slots(input_status())`; poi esecuzione dello slot di import di A con il vecchio percorso. In un progetto gemello: al percorso di A una copia byte-identica (inode nuovo), poi un file della stessa dimensione con un byte diverso; per ciascuno `blocked_slots` ed esecuzione dello slot di import con il percorso registrato | Slot che dipendono da A `blocked` con `input.missing`, `input_id` e percorso di A; slot di B non bloccato; stato, artefatto e generazione di ogni slot invariati; l'esecuzione si ferma con `io` `input.missing` nel job e nella provenance, nessun artefatto pubblicato, nessun job su un altro percorso. Gemello: in entrambi i casi slot `blocked` con `input.modified`; copia identica: job completato con output di digest uguale a quello registrato (limite di RL-03); byte diverso: stop `integrity` `input.digest_mismatch`, nessun artefatto pubblicato |
| T-M3A-09 | b | Ricollegamento di A (digest uguale); `blocked_slots`; `resume_workflow` del workflow della catena; poi artefatto primario della calibrazione rimosso, riapertura ed esecuzione esplicita dello slot dal nuovo percorso | Nessuno slot bloccato; nessun job, nessun artefatto, generazioni, `nodes`, `node_edges`, `slots`, `snapshots`, `history_cursor`, `recovery_reports` invariati (SQL); resume senza job (fasi sicure riusate); ricalcolo con digest uguale a quello registrato e nome riusato |
| T-M3A-09 | c | Chiusura e riapertura dopo 09b | `inputs()` con percorso e identità nuovi; `input_relinks` identica; `input_status` `present`; nessuno slot bloccato; nessuna riga nuova in `recovery_reports`; stato logico di T-M2C-01 (esteso a `format` e `input_relinks`) uguale a prima della chiusura |
| T-M3A-09 | d | Cambio di parametro (snapshot nuovo), ricollegamento, undo, redo; e ricollegamento dopo un undo, con redo ancora disponibile | `inputs` e `input_relinks` invariati da undo e redo; il ricollegamento non crea snapshot e non azzera `redo_snapshot_id`; `blocked_slots` invariato da undo e redo |
| T-M3A-09 | e | Due input con lo stesso digest: uno mancante, poi entrambi; un nodo con un arco verso un digest mai registrato | Uno mancante: arco `available` con l'id dell'input presente; entrambi: `blocked` con i due motivi; non registrato: `blocked` `input.unregistered` |
| T-M3A-09 | f | `blocked_slots` su stati costruiti a mano (senza filesystem) per una catena di tre livelli con slot `detached`; contatore delle chiamate a `stat` | Uscita uguale alla tabella scritta a mano; slot `detached` assenti; zero `stat`; ordine di slot e archi canonico |

Gate di F-IO-08 in M3.a: T-M3A-07, 08, 09 verdi, con U0 e U8a chiusi.

## 11. Ipotesi e limiti dichiarati

- Limite: `input_status` non legge il contenuto. Un file modificato al suo posto, con la stessa identità e la stessa dimensione, risulta `present`; lo ferma il confine all'uso (`input.digest_mismatch`, OB-11). Leggere il digest di ogni originale a ogni richiesta costerebbe una lettura completa di tutti gli input.
- Limite: fra la verifica del candidato e il `COMMIT` il file può cambiare (modello di minaccia di ADR-04: corsa deliberata fuori). Ogni uso successivo lo riverifica (OB-11).
- Limite: `add_input` accetta oggi percorsi relativi, che si risolvono rispetto alla directory corrente del processo; uno stesso progetto aperto da un'altra directory vedrebbe gli input come mancanti. Il ricollegamento richiede percorsi assoluti; rendere assoluti quelli di `add_input` spetta a U8a (CL-04) ed è un punto aperto. Decisione del coordinatore (arbitraggio, 30 ottobre 2026): da U8a `add_input` rifiuta un percorso non assoluto con `invalid_input` `store.bad_input`, con lo stesso criterio di RL-05; il caso entra nei test di U8a.
- Limite: in M3.a l'esecuzione prende i percorsi dal chiamante, non dallo store (§7, vincolo per i consumatori). Un chiamante che passasse un percorso diverso con il contenuto giusto eseguirebbe il nodo, perché il digest coincide, senza aggiornare il record.
- Limite: il controllo `fs.original_inode` del ricollegamento copre i file gestiti dal progetto (RL-05, passo 5); la cache delle miniature, per i propri controlli, continua a non conoscere gli inode degli originali (limite già dichiarato in ADR-04, «Integrazione con R1»).
- Fatto (rename(2), POSIX): uno spostamento con `rename` nello stesso filesystem conserva `(st_dev, st_ino)`. Il prodotto non lo usa per ritrovare il file: rileva solo l'assenza al percorso registrato.
- Stato del codice letto al commit `311fb7a` con `codegraph explore`, `grep` e `sed -n` sui file citati in §1: nessuna build, nessun test eseguito in questa unità.
