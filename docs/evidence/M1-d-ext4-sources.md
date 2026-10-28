# M1.d: riscontro circoscritto delle ipotesi ext4

2026-10-25. **Scheduler STOP; nessun test eseguito. Profilo e runtime restano UNPROVEN.** Ricerca primaria limitata a inserimento degli extent e conversione unwritten/writeback. Nessuna modifica al candidato, nessun import, preallocazione, ioctl, mount, device, superblocco raw, configurazione, Git o delega. Unica scrittura: questo nuovo report; nessun materiale temporaneo scaricato su disco.

Modello dichiarato dall'ambiente: `openai/gpt-6-astra`, non verificato indipendentemente. Costi/token non esposti. Letti una sola volta integralmente con `read`: `docs/evidence/M1-d-admission-v5.md` 1–273, `tmp/m1/d/fs_bounds.py` 1–67, `tmp/m1/d/fs-metadata-v5-20261025-01.json` 1–49. Non letti pool, writer, test o altri sorgenti locali. La chiusura dei quattro bug statici è contesto del mandato, non una nuova review eseguita qui.

## Risposta breve

1. **170 non è una proprietà generale del formato o dello split ext4.** Il kernel divide al punto d'inserimento, non obbligatoriamente a metà. Una prova che usa soltanto `340/2` è insufficiente.
2. **Non è stato trovato un controesempio al caso ristretto**: file vuoto, soli inserimenti realmente in coda logica, nessuna rimozione/conversione intercalata, una frontiera per livello. Gli split in coda lasciano il vecchio nodo pieno e il nuovo nodo sulla frontiera. Questa lettura sostiene il caso ristretto, ma non certifica il calcolo completo né l'host.
3. **Preallocato non significa metadata invarianti.** Le conversioni unwritten possono dividere extent e far crescere l'albero. Il costo di 3.49 GB è un valore della formula proposta, non una misura né un minimo richiesto da ext4.
4. Una sequenza preregistrata write/fsync per blocco può ridurre gli intervalli pendenti e la prenotazione per operazione. Non elimina metadata persistenti, costo complessivo, errori o necessità di provare il profilo.

## Fonti e copertura reale

Fonte primaria: repository ufficiale di Linus Torvalds, tag **v6.12**, tramite HTTPS `execute`/`fetch`, estrazione in memoria. È una versione storica fissata per rendere controllabile il ragionamento, **non** il kernel installato e non una dichiarazione sull'ultima versione disponibile. Nessun commit della build Ubuntu `7.0.0-38-generic #38-Ubuntu` è attestato. Nessuna equivalenza delle implementazioni viene dedotta dal numero di versione.

| URL | Letto effettivamente |
|---|---|
| <https://raw.githubusercontent.com/torvalds/linux/v6.12/fs/ext4/extents.c> | Intervalli 87–266, 1052–1468, 1975–2224, 3150–3715, 3889–4068, 4474–4555, 4700–4845; estratti sovrapposti. File di 6157 righe ricevuto in memoria, **non letto integralmente** |
| <https://raw.githubusercontent.com/torvalds/linux/v6.12/fs/ext4/fsync.c> | File intero: helper nojournal/journal, parent sync, `ext4_sync_file` e gestione errori |
| <https://raw.githubusercontent.com/torvalds/linux/v6.12/Documentation/filesystems/ext4/ifork.rst> | File intero, comprese sezioni Extent Tree e Inline Data; `blockmap.rst` incluso per riferimento **non aperto** |
| <https://raw.githubusercontent.com/torvalds/linux/v6.12/Documentation/filesystems/ext4/bigalloc.rst> | File intero; contiene anche commenti storici riferiti a maggio 2015 |
| <https://raw.githubusercontent.com/torvalds/linux/v6.12/Documentation/filesystems/ext4/inline.rst> | Risposta `404: Not Found`, **non verificato**; nessun contenuto inventato |

Non letti integralmente allocator multiblocco, journal/JBD2, inode/writeback, libc `posix_fallocate`, gestione quote, ACL/xattr, percorso errori complessivo o sorgenti/patch Ubuntu. La funzione `ext4_alloc_file_blocks` è letta fino al return, non la riga di chiusura; nessun comportamento ulteriore le viene attribuito. La prima ricerca di `ext4_split_extent_at` aveva intercettato la dichiarazione iniziale; la definizione effettiva è stata poi letta nell'intervallo 3150–3440.

## 1. Tentativo di falsificazione dell'occupazione minima 170

La documentazione `ifork.rst` dichiara header e record da 12 byte e radice nell'inode con quattro extent. Per blocco 4096, capacità ordinaria esterna `floor((4096-12)/12)=340`; la coda checksum da quattro byte usa lo spazio residuo. **Capacità massima non implica occupazione minima.** La radice non può soddisfare 170 e va trattata separatamente.

Estratti esatti da `ext4_ext_split`, `extents.c` da riga 1052:

```c
/* FIXME: now decision is simplest: at current extent */
m = EXT_MAX_EXTENT(path[depth].p_hdr) - path[depth].p_ext++;
```

Il codice copia `m` record nella nuova foglia e li sottrae alla vecchia. Per gli indici inizializza il nuovo nodo con:

```c
neh->eh_entries = cpu_to_le16(1);
m = EXT_MAX_INDEX(path[i].p_hdr) - path[i].p_idx++;
```

**Controesempio strutturale al principio generale “ogni nodo non di frontiera è almeno mezzo pieno”:** una foglia piena con 340 record, inserimento non fondibile vicino all'inizio, percorso sul primo record e nuova chiave prima del secondo. Lo split può trasferire 339 record a destra e lasciare uno a sinistra; dopo l'inserimento il lato sinistro ha due record, non 170. La foglia sinistra non è la frontiera destra. È una derivazione dal ramo letto, non un test sul filesystem. Richiede inserimento interno, quindi **non falsifica da sola il caso fresh/append-only della riga 20 di fs_bounds.py**.

Per quel caso ristretto, all'ultima posizione `m=0`: il vecchio nodo rimane pieno, il nuovo riceve la nuova voce sulla frontiera. La crescita in profondità copia la radice piccola in un blocco esterno, che inizialmente può avere quattro voci ma è ancora frontiera. `ext4_ext_create_new_leaf` risale al primo indice con spazio; `ext4_ext_insert_extent` tenta prima merge e spazio disponibile. `ext4_alloc_file_blocks` avanza con `map.m_lblk += ret`. Questi elementi sono coerenti con un'induzione sulla sola inserzione destra, non con un invariante B-tree di riempimento a metà per ogni operazione.

**Gap esatto:** dimostrare che tutta la preallocazione concreta percorra soltanto quella successione, includendo normalizzazione, merge, crescita, retry e transitori/errori; provare il conteggio per livello, l'eccezione radice/frontiera e l'allocazione aggiuntiva. Il codice letto contiene `EXT4_GET_BLOCKS_NO_NORMALIZE` solo per richieste che stanno in un extent e retry ENOSPC: non basta osservare che l'offset avanza. Manca soprattutto il collegamento al sorgente/configurazione della build host. Non approvare `tight_append=True` sulla base della sola divisione 340/2. Non estendere l'eventuale prova append-only alle successive conversioni in punti interni del file.

## 2. Preallocazione, unwritten, pwrite e fsync

`ext4_fallocate` imposta `EXT4_GET_BLOCKS_CREATE_UNWRIT_EXT`, converte eventuali inline data e chiama `ext4_alloc_file_blocks`. Questo descrive il percorso nativo ext4 letto, non prova quale percorso libc abbia effettivamente seguito un futuro `posix_fallocate` su questo host.

Il commento di `ext4_split_extent` è esplicito:

> “It may result in splitting the extent into multiple extents (up to three)”

La definizione di `ext4_split_extent_at` inserisce il nuovo record con `ext4_ext_insert_extent`. Il percorso di conversione presenta ottimizzazioni di merge/trasferimento e zeroout; non bisogna assumere che riescano sempre. `ext4_ext_handle_unwritten_extents` distingue PRE_IO, conversione end_io e buffered write/writepage, e dichiara:

> “When writing into unwritten space, we should not fail to allocate metadata blocks for the new extent block if needed.”

L'impostazione `EXT4_GET_BLOCKS_METADATA_NOFAIL` è una scelta interna di allocazione, **non** una promessa di costo zero o una quota del progetto. I rami di errore possono aver già modificato metadata. Il codice contiene anche tentativi interni di zeroout in caso di split fallito; questa descrizione del kernel non autorizza un fallback del prodotto.

In `ext4_sync_file`, con journal:

```c
ret = file_write_and_wait_range(file, start, end);
```

Seguono commit del journal, eventuale flush e raccolta errori writeback. Il commento avverte: “Another task could have dirtied this inode.” Quindi la prenotazione non può coprire soltanto l'ultima pwrite se restano altri intervalli sporchi. Le conversioni possono iniziare in background prima della chiamata fsync: contabilizzare l'intera finestra writeback, non attribuire causalmente ogni allocazione all'istante della syscall fsync.

La proposta usa `Gamma=((n+2)*13+2)*4096`. Derivazione aritmetica indipendente, senza eseguire il candidato:

- 256 MiB allineati: `n=65536`, Gamma **3489775616 byte**, circa **3.49 GB decimali**, non 3.49 GiB.
- Un blocco allineato di 4096 byte: `n=1`, Gamma **167936 byte** per operazione secondo la stessa formula.

Non sono massimi kernel verificati. `A=13`, numero di mutazioni, attributi inode, arrotondamento cluster e transitori restano assunzioni da provare. Un fsync non riscrive necessariamente tutti i 256 MiB; il valore grande deriva dalla scelta pessimistica del modello, non da una necessità fisica dimostrata.

## Sequenza predeclared e distinzione dei budget

| Variante | Effetto possibile | Condizione/limite |
|---|---|---|
| Scrivere 256 MiB, poi un fsync | Una finestra pendente grande; modello attuale non ammette la relativa prenotazione | Non prova che ext4 consumi davvero 3.49 GB |
| Scrivere un blocco allineato, fsync riuscito, poi blocco successivo | Stessi offset, byte e dimensione finale; pendenti più piccoli, più syscall/commit | Nessun altro writer/mapping sporco; prenotazione valida dall'inizio della write fino al completamento del sync. Errori terminali, nessun avanzamento dopo errore |
| Slot unit con bound dai campi reali | Riduce riserve sovrastimate di serializzazione, non gli esperimenti | Occorre prova statica finita di ogni campo, escaping, errori/log, staging e header; nessun troncamento |

La seconda variante può conservare il **payload scientifico**, ma cambia operation cost, ordinamento di persistenza, scheduling e misure temporali. Non è una modifica invisibile alla ricetta. Va preregistrata prima di qualsiasi run; eventuali confronti richiedono lo stesso protocollo pertinente. Per 256 MiB sarebbero 65536 blocchi da 4096, non meno dati. Non è stata implementata o provata.

Il successo del sync può chiudere la riserva degli intervalli effettivamente completati sotto le ipotesi indicate. Non azzera i metadata nuovi ormai persistenti: questi vanno aggiunti all'occupazione confermata prima di ammettere l'operazione successiva. Se la contabilità proposta somma riserve separate per write e sync, occorre definire esplicitamente sovrapposizione, lifetime e riconciliazione; non riciclare due volte lo stesso budget. Fsync di un file non certifica da solo directory, altri inode o tutto il volume.

**Pool 32 MiB:** limite applicativo degli oggetti contabilizzati nel pool, non limite universale RSS/page cache/kernel. **Cap tmp 1 GiB:** ammissione cooperativa del progetto, non quota OS installata. Prenotazioni preventive con Gamma valido possono delimitare il modello di writer e metriche dichiarato; `du` successivo non trasforma tale modello in hard quota filesystem. Non è stata letta/configurata alcuna quota OS. Journal condiviso, writer esterni e consumo fisico del volume non coincidono con `st_blocks` del namespace.

La documentazione bigalloc afferma: “the minimum allocation unit becomes a cluster, not a block, even for directories”. `statfs` con blocco/frammento 4096 non chiude il gap sul cluster. `ifork.rst` descrive inline data dipendente da feature e flag dell'inode; il flag extents della sola directory non prova il profilo dei file futuri. Le osservazioni locali registrate restano insufficienti. Nessun controllo aggiuntivo su device/configurazioni viene eseguito.

## Chiusura bounded

**170 generale falsificato; 170 nel caso rigorosamente append-only non falsificato da questa lettura, ma non provato per l'host.** Il bound fsync da 3.49 GB è una proposta pessimistica non validata, non una legge ext4. Una diversa sequenza esplicita può ridurre la finestra di rischio senza ridurre il payload, ma non risolve da sola bootstrap, crescita cumulativa o feature ignote.

Per i 12 unit è lecito cercare un bound finito più stretto sui campi/log effettivi al posto del generico slot da 1 MiB. Qui quei writer non sono stati letti: **nessun nuovo numero di capacità è attestato**. Conservare 12 test, 21 bounded, tre full da 40 GiB, spill 256 MiB, pool/cache, cap 1 GiB e tutti i gate. Non modificare cap o payload per far passare l'ammissione.

Raccomandazione operativa: mantenere STOP su M1.d; non promuovere `PROFILE_STATUS` né eseguire admit/unit/run sulla sola presente ricerca. Il passo utile è una prova circoscritta del bound e del protocollo effettivamente scelto, oppure il calcolo statico più stretto dei reali slot unit, senza affermare che ciò risolva già il profilo FS. Nessun PASS, nessuna certificazione hard-quota universale. Gli altri rami proseguono: questa ricerca non ne è prerequisito.
