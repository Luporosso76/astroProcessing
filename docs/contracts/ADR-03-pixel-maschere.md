# Contratto ADR-03.a: modello pixel e maschere

Stadio: ADR-03.a, tipi minimi (piano §20, registro riga 3388). Lettore FITS: parte minima di ADR-08.a, usata da M2.a. Unità che lo implementa: M2.a. Errori: modello comune in [ADR-05](ADR-05-dag-provenance.md#modello-derrore-comune).

## Tipi

Namespace `astro`. Tutti i campi interpretati sono `std::optional`: assente vale `std::nullopt`, mai un valore di comodo.

| Tipo | Campi o valori | Invarianti |
|---|---|---|
| `sample_type` | `f32`, `f64` | Nessun tipo intero in memoria. |
| `pixel_unit` | `adu`, `electron`, `dimensionless` | L'unità viaggia con l'immagine; nessuna operazione la cambia in silenzio. |
| `linearity` | `linear`, `nonlinear` | M2.e accetta solo `linear`. |
| `cfa_pattern` | `rggb`, `bggr`, `grbg`, `gbrg` | Pattern ignoto: stop, mai indovinato. |
| `cfa_layout` | `pattern`, `x_offset`, `y_offset` (0 o 1) | Il canale di un pixel è `pattern` traslato di `(x+x_offset, y+y_offset)`. G1 e G2 sono canali distinti. |
| `image_geometry` | `width`, `height`, `planes` (`uint32_t`), `binning_x`, `binning_y` (`std::optional<uint32_t>`) | `width, height, planes ≥ 1`. Prodotto calcolato con controllo di overflow. |
| `image` | `geometry`, `plane_layout`, `type`, `unit`, `linearity`, `cfa` (`std::optional<cfa_layout>`), `samples`, `invalid`, `variance` (`std::optional<variance_plane>`), `metadata` | Vedi sotto. |
| `invalid_mask` | un `uint16_t` per pixel e per piano, bit `invalid_reason` | Stessa geometria di `samples`. |
| `edit_mask` | un `float` in [0, 1] per pixel | Tipo distinto da `invalid_mask`; nessuna conversione implicita fra i due. |

`samples`: buffer contiguo, ordine piano-riga-colonna, allineato a 64 byte, posseduto dall'immagine. Dimensione in byte uguale a `width·height·planes·sizeof(type)`. Nessuna vista non posseduta sopravvive all'immagine.

`cfa` presente implica `planes == 1`. `plane_layout` ∈ {`mono`, `cfa`, `rgb`, `multi_plane`}: `rgb` quando `planes == 3` e l'header lo dichiara, `multi_plane` per ogni altro `planes > 1`.

M2.a legge, conserva e riscrive tutti e quattro i layout con roundtrip bit-esatto (F-IO-01). M2.e accetta solo `mono` e `cfa`: `rgb` e `multi_plane` producono `unsupported`, `calib.plane_layout`, prima di ogni calcolo (T-M2E-17).

### Bit di `invalid_reason`

| Bit | Nome | Origine |
|---|---|---|
| 0 | `source_nonfinite` | Campione NaN o ±Inf nel file. |
| 1 | `source_flagged` | Maschera di invalidità letta dal file. |
| 2 | `saturated` | Valore ≥ soglia di saturazione dichiarata. |
| 3 | `master_invalid` | Pixel invalido in un master usato. |
| 4 | `q_nonpositive` | `Q ≤ 0`. |
| 5 | `q_nonfinite` | `Q` NaN o ±Inf. |
| 6 | `q_below_min` | `0 < Q < q_min`. |
| 7 | `q_unreliable` | Con `Q > 0` finito: incertezza relativa di `Q` oltre la soglia dichiarata. |
| 8 | `result_nonfinite` | Risultato non finito con input validi. |

Bit 9–15 riservati: un bit riservato acceso in lettura produce `invalid_input`.

Immagine letta da file: i campioni restano quelli del file, anche sui pixel marcati, perché il roundtrip è bit-esatto. La maschera è l'HDU `INVALID`, se presente, letto verbatim: i bit scritti dal prodotto tornano identici. `source_nonfinite` si aggiunge solo su un campione non finito i cui bit letti dal file sono 0. `source_flagged` è per maschere esterne al prodotto e non viene mai aggiunto dal lettore.

Invarianti verificati in ogni immagine prodotta da un operatore:

- un campione non finito ha almeno un bit acceso (T-M2A-08);
- un pixel con un bit acceso ha campione NaN e varianza NaN (T-M2A-08);
- la maschera di un output è l'unione dei bit degli input sullo stesso pixel più quelli dell'operatore (T-M2E-14).

Un pixel invalido non riceve mai epsilon, zero o interpolazione.

### Metadati

`frame_metadata` contiene i campi interpretati: `exposure_s`, `gain` (impostazione della camera), `conversion_gain_e_per_adu` (`EGAIN`), `offset_adu`, `temperature_c`, `filter`, `camera`, `imagetyp`, `saturation_adu`, `pedestal_adu`, `roi_x`, `roi_y` (`XORGSUBF`, `YORGSUBF`), `readout_mode` (`READOUTM`), `row_order` (`ROWORDER`), ognuno `std::optional`. Ogni valore porta la sua `metadata_source`: `header` (con il nome della keyword) o `declared` (dalla ricetta).

`header_cards`: tutte le card a 80 caratteri, in ordine, verbatim. Le card non interpretate restano qui e sono elencate in `uninterpreted_keywords` (F-IO-01).

Un valore `declared` per un campo già presente nell'header con valore diverso produce `incompatible`, codice `metadata.declared_conflict`. L'origine (`header` o `declared`) è provenienza e vive nella provenance di ADR-05, non nel FITS: lo scrittore scrive ogni valore presente (anche `declared`) nella sua keyword; la rilettura lo restituisce con origine `header`. L'identità dei roundtrip (T-M2A-09) confronta valori, layout, maschere e varianza, non l'origine.

Keyword interpretate: `EXPTIME`, `GAIN`, `EGAIN`, `OFFSET`, `CCD-TEMP`, `SATURATE`, `PEDESTAL` (reali), `FILTER`, `INSTRUME`, `IMAGETYP`, `READOUTM`, `ROWORDER` (stringhe), `XORGSUBF`, `YORGSUBF` (interi), `BUNIT` (`adu`, `electron`, `dimensionless`, senza distinzione di maiuscole; altro valore: `unsupported`, `fits.bunit`), `LINEAR` (logico: `T` = `linear`), `XBINNING`, `YBINNING`, `BAYERPAT`, `XBAYROFF`, `YBAYROFF`, `CTYPE3 = 'RGB'` (con `planes == 3`). Il tipo della card è verificato, nessuna coercizione (`EXPTIME = T` o `'60'`: `invalid_input`, `fits.keyword_value`). Una keyword interpretata duplicata è `invalid_input`, `fits.duplicate_keyword`; una stringa interpretata che prosegue su card `CONTINUE` è `unsupported`, `fits.continue`. Lo scrittore aggiunge in coda le card dei campi presenti nell'immagine e assenti da `header_cards`, poi rilegge l'header scritto: un campo in conflitto con `header_cards` è `invalid_input`, `fits.header_inconsistent`.

Offset CFA: `XBAYROFF`/`YBAYROFF` si riferiscono ai dati come memorizzati. La richiesta di lettura può dichiararli (`fits_read_options`, origine `declared`); dichiarati e presenti nell'header con valore diverso: `incompatible`, `metadata.declared_conflict`. `BAYERPAT` senza offset né header né dichiarati: `metadata_missing`, `fits.bayer_offset_missing`. Offset dichiarati per un'immagine non CFA: `invalid_input`, `fits.declared_offset_unused`. `ROWORDER` si registra senza trasformare i dati.

## Lettore e scrittore FITS

Lettura dal buffer in memoria già sottoposto a digest (ADR-05), con `fits_open_memfile`; scrittura in memoria con `fits_create_memfile` ([ADR-13](ADR-13-offline.md)). File sorgente aperto solo in lettura.

| BITPIX | Tipo in memoria | Note |
|---|---|---|
| 8 con `BSCALE = 1` e `BZERO` ∈ {0, −128}; 16 con `BSCALE = 1` e `BZERO` ∈ {0, 32768}; -32 | `f32` | Conversione esatta: byte senza segno e con segno, int16 e uint16. |
| 8, 16 con altro `BZERO`/`BSCALE`; 32, 64, -64 | `f64` | Per BITPIX 64, un intero con modulo > 2^53 produce `unsupported`. |

`source_encoding` conserva `bitpix`, `bzero`, `bscale` e `naxis` (1, 2 o 3 come dichiarato; `NAXIS3 = 1` resta `NAXIS = 3`). Ogni campione deve essere invertibile esattamente con `r = round((v − bzero)/bscale)`, `r·bscale + bzero == v`: in lettura, altrimenti `unsupported`, `fits.scaling_not_exact`; in scrittura, un valore non rappresentabile esattamente è `numeric`, `fits.precision_loss`, fuori range `fits.value_out_of_range`, non finito in encoding intero `fits.nonfinite_in_integer_encoding`. `BITPIX 64` richiede `BSCALE = 1`, `BZERO = 0` (`unsupported`, `fits.int64_scaling`), modulo ≤ 2^53 (`fits.int64_range`); BITPIX −32/−64 richiedono scala identità (`fits.float_scaling`); `BLANK` in immagine intera: `unsupported`, `fits.blank`. Riscrivere con lo stesso encoding riproduce byte per byte la data unit: per i casi interi la riconversione arrotonda all'intero più vicino e il test verifica l'identità (T-M2A-02).

HDU in uscita: primaria con i campioni; `EXTNAME = 'INVALID'` (BITPIX 16, `BZERO = 32768`) solo se almeno un bit è acceso; `EXTNAME = 'VARIANCE'` se presente (stesso BITPIX float dei campioni, scala identità: altrimenti `invalid_input`, `fits.extension_encoding`). Geometria diversa dal primario: `fits.extension_geometry`. Bit riservato in `INVALID`: `invalid_input`, `fits.invalid_reserved_bits`. La rilettura ricostruisce `image` identica (T-M2A-09).

### Limiti (NFR-SEC-01)

Costanti in `astro::fits_limits`, coerenti fra loro: `max_file_bytes` copre i campioni f64 più `INVALID` e `VARIANCE` f64 (18 byte per campione) più gli header di tutti gli HDU.

| Limite | Valore |
|---|---|
| `max_naxis` | 3 |
| `max_axis_length` | 65 536 |
| `max_planes` | 16 |
| `max_samples` | 2^28 per immagine, tutti i piani |
| `max_hdus` | 8 |
| `max_header_cards` | 10 000 per HDU |
| `max_file_bytes` | 2^28 · 18 + 16 MiB = 4 624 MiB |

Preflight, prima di leggere il file per intero e prima di ogni allocazione:

0. Il budget si controlla due volte: dopo il preflight dal file, e di nuovo da `decode_fits` sul buffer effettivamente letto (che può differire dal file pianificato).
1. `open` in sola lettura, `fstat` sullo stesso fd: file non regolare: `invalid_input`, `fits.not_regular`; dimensione oltre `max_file_bytes`: `limit_exceeded`, `fits.max_file_bytes`;
2. lettura con `pread` dei soli blocchi di header (al più `max_hdus · max_header_cards · 80` byte) e controllo di NAXIS, assi, piani, campioni, HDU e card;
3. picco = dimensione del file + campioni · `sizeof(type)` + maschera (2 byte per campione) + varianza, se l'HDU esiste. Il picco entra nel `job_plan` di ADR-06 e si prenota prima della lettura; oltre il budget: `budget`, `sched.job_exceeds_budget`;
4. lettura intera nel buffer prenotato, digest (ADR-05), decode. La dimensione letta deve essere quella del `fstat`: altrimenti `integrity`, `input.size_changed`.

Oltre un limite: `limit_exceeded`, codice `fits.<nome_limite>`, nessuna allocazione dei campioni (T-M2A-05).

### Errori

| Condizione | Categoria | Codice |
|---|---|---|
| Stato CFITSIO non nullo | `invalid_input` | `fits.cfitsio_status`, con stato e testo CFITSIO nel contesto |
| File troncato o header malformato | `invalid_input` | `fits.malformed` |
| Immagine compressa a tile, tabelle come dato principale, NAXIS = 0 | `unsupported` | `fits.hdu_kind` |
| `CHECKSUM` o `DATASUM` presente e non corrispondente | `integrity` | `fits.checksum` |
| `BAYERPAT` con valore non riconosciuto | `unsupported` | `fits.bayer_pattern` |
| Immagine dichiarata CFA senza pattern | `metadata_missing` | `fits.bayer_pattern_missing` |
| Keyword strutturale duplicata, `PCOUNT`/`GCOUNT` negativi, BITPIX non valido, assi non positivi | `invalid_input` | `fits.malformed` |
| CFITSIO e la scansione dell'header leggono HDU diversi | `invalid_input` | `fits.structure_mismatch` |
| Estensione di tipo non immagine, `PCOUNT`/`GCOUNT` non standard | `unsupported` | `fits.hdu_kind` |
| Estensione immagine con `EXTNAME` diverso da `INVALID`/`VARIANCE` | `unsupported` | `fits.extension_kind` |
| Pattern CFA con `planes > 1`; offset CFA diverso da 0 e 1 | `invalid_input` | `fits.cfa_planes`, `fits.bayer_offset` |
| Errore CFITSIO in scrittura (dati già validati) | `io` | `fits.cfitsio_status` |

Codici di validazione dell'immagine (`validate_image`, scrittore): `image.geometry`, `image.size_overflow` (`limit_exceeded`), `image.samples_size`, `image.alignment`, `image.mask_size`, `image.variance_size`, `image.layout`, `image.cfa_planes`, `image.cfa_offset`, `image.mask_reserved_bits`; scrittore: `fits.encoding`, `fits.card_length`, `fits.card_structural`, `fits.card_value`.

Lo scrittore produce solo un buffer in memoria; nessuna sua API accetta un percorso. Il buffer arriva su disco solo con la scrittura atomica di [ADR-04](ADR-04-persistenza.md#scrittura-atomica) o con l'export, che applicano i controlli su destinazione, symlink e input registrati.

## Test

| ID | Clausola |
|---|---|
| T-M2A-01 | Invarianti di `image`: geometria, overflow della dimensione, allineamento. |
| T-M2A-02 | Roundtrip bit-esatto per ogni BITPIX della tabella, con e senza `BZERO`/`BSCALE`. |
| T-M2A-03 | Card conservate in ordine; non interpretate elencate. |
| T-M2A-04 | Keyword assente: campo `nullopt`; conflitto `declared`/header: stop. |
| T-M2A-05 | Ogni limite al confine passa, confine+1 si ferma senza allocazione; file non regolare e file che cresce fra `fstat` e lettura: stop; picco oltre il budget: stop prima della lettura. |
| T-M2A-06 | File malformati e troncati, fuzz breve: stop controllato, nessun crash sotto ASan/UBSan. Parte M2 di T-FBK-01 (decoder che fallisce). |
| T-M2A-07 | CFA: quattro pattern e offset; pattern ignoto o assente: stop. |
| T-M2A-08 | Immagine letta: campioni marcati invariati, `source_nonfinite` su ogni non finito. Immagine prodotta: invarianti NaN. `edit_mask` modificata non cambia `invalid_mask`. |
| T-M2A-09 | Estensioni `INVALID` e `VARIANCE`: scrittura e rilettura identiche. |
| T-M2A-10 | Lo scrittore restituisce un buffer identico a quello riletto; nessuna API su percorso (verifica dei simboli in T-M2F-01). |
| T-M2A-11 | Checksum errato: stop `integrity`. |
| T-M2A-12 | HDU non supportati: stop `unsupported`. |
| T-M2A-13 | RGB e multi-plane (NAXIS3) letti, conservati e riscritti bit-esatti (F-IO-01). |

## Aggiunte di M2.e

- Campi dichiarabili dalla ricetta oltre a `frame_metadata`: `binning_x`, `binning_y`, `unit`, `linearity` (`declared_image_fields`). Un valore già presente nell'header e diverso dal dichiarato: `incompatible`, `metadata.declared_conflict`; uguale: nessun effetto. Assente nell'header: vale il dichiarato. I valori dichiarati entrano nei parametri del nodo di calibrazione (chiave del nodo) e il report riporta l'origine (`header` o `declared`) di ogni metadato.
- T-M2E-14, unione delle maschere: la maschera di uscita contiene la unione bit a bit dei bit di light, dark, flat e dark-flat; se un master (dark, flat, dark-flat) ha qualunque bit acceso sul pixel si aggiunge anche `master_invalid`.

## Aggiunte di R1: fase, linearità, inventario, anteprima, keyword non interpretate

Unità R1 di `plans/M2-residui.md` (F-IO-01, F-IO-05, F-IO-10, NFR-SEC-01 sull'inventario e sull'anteprima). Decisioni D1 e D2 del piano.

### Fase di processo e linearità (F-IO-10, D1)

| Tipo | Valori | Note |
|---|---|---|
| `process_phase` | `raw`, `calibrated`, `prestacked` | Dichiarata all'import ed obbligatoria: nessun valore di comodo. |
| `linearity_class` | `linear`, `nonlinear`, `unknown` | `unknown` è un valore esplicito: non è mai trattato come `linear`. È la classe di un'immagine con `lin` assente. |

- `image.phase` è `std::optional<metadata_value<process_phase>>`: origine `header` (keyword `ASTRPHAS`, stringa `raw`, `calibrated` o `prestacked`, senza distinzione di maiuscole; altro valore: `unsupported`, `fits.phase`) oppure `declared` (`fits_read_options::declared_phase`). Il confronto fra immagini ignora l'origine, come per gli altri metadati.
- La linearità resta `image.lin` (`LINEAR`). `fits_read_options::declared_linearity` (`linearity_class`) la dichiara. Header e dichiarazione in conflitto, anche `unknown` dichiarato contro un `LINEAR` presente: `incompatible`, `metadata.declared_conflict`, con il campo nel contesto. `linear` o `nonlinear` dichiarati su header privo di `LINEAR` valgono con origine `declared`; `unknown` dichiarato su header privo di `LINEAR` non scrive nulla e l'import lo registra con origine `declared`.
- `ASTRPHAS` è interpretata e lo scrittore la emette quando `image.phase` è valorizzata e la card manca; la rilettura la restituisce con origine `header`. Un `ASTRPHAS` diverso dalla fase dichiarata: `incompatible`, `metadata.declared_conflict`, campo `phase`.
- `decode_fits` senza `ASTRPHAS` e senza `declared_phase` lascia `phase` vuota: il decoder non inventa una fase. `inventory_fits` riporta la fase se c'è; `run_import_job` e la calibrazione la richiedono: assente, `metadata_missing`, `import.phase_missing` o `calib.metadata.phase`.
- L'output della calibrazione porta `phase = calibrated` (origine `header` alla rilettura) e conserva `lin`.
- `image.lin_origin` conserva l'origine di `lin` (`header` o `declared`, vuota se `lin` è assente); non entra nel confronto fra immagini. Il report di calibrazione e la provenance la riportano. `equivalent_content` confronta anche il valore della fase (presente contro assente è una differenza), ignorandone l'origine.
- Il job di calibrazione controlla fase e linearità dai soli header e dalle dichiarazioni della ricetta (`peek_header`, senza prenotazioni nel pool) di tutti e quattro gli ingressi prima del piano e di ogni lettura di campioni; `prepare_calibration` li controlla prima di `check_image`. Stessi codici in entrambi (`require_calibration_phase`, `require_calibration_linearity`). `image.lin_origin` è valorizzata solo se `lin` è presente.

Tabella di compatibilità operazione × fase × linearità, scritta prima del codice (`require_operation_allowed`, T-M2R1-05). Il preflight applica la riga prima di qualunque lettura di campioni: l'operazione non ammessa è uno stop `incompatible` senza output.

| Operazione | Fase ammessa | Linearità ammessa | Esito se non ammessa |
|---|---|---|---|
| `inventory`, `preview`, `export_bundle` | qualunque | qualunque | sempre ammesse |
| `master_create`, `calibrate` | `raw` | `linear` | fase: `incompatible`, `op.phase_blocked`; linearità: `incompatible`, `op.linearity_blocked` (la fase si controlla per prima) |
| `linear_ancestor_required` (C-14) | qualunque | `linear` | `incompatible`, `op.linear_ancestor_missing` |

I quattro ingressi della calibrazione (light, dark, flat, dark-flat) devono essere `raw` e `linear`; in R2 la tabella si estende ai master prodotti dal prodotto. Calibrato e prestacked importati si inventariano, si visualizzano e si esportano; su input `nonlinear` o `unknown` il preflight dichiara il limite C-14 («antenato lineare verificabile assente», piano 10.15) e non esiste alcuna operazione che inverta uno stretch o che sostituisca l'operatore.

### Segnalazione delle keyword non interpretate (F-IO-01)

`header_cards` e `uninterpreted_keywords` (unione senza duplicati, in ordine di prima comparsa) restano come in M2.a. Da R1 l'elenco compare anche: nel report di import (`uninterpreted_keywords`), nel report di calibrazione (per ogni ingresso, `uninterpreted_keywords`) e nella provenance dei job che leggono il file (`details.observed_inputs[].uninterpreted_keywords`, [ADR-05](ADR-05-dag-provenance.md)). Le card commento (`COMMENT`, `HISTORY`) sono elencate come qualunque altra keyword non interpretata; le card a keyword vuota no. `ASTRPHAS` è interpretata e non compare nell'elenco.

### Inventario (F-IO-05, D2)

`inventory_fits(const fits_input&, const fits_read_options&, memory_pool&)` legge con `pread` i soli blocchi di header di tutti gli HDU (nessun byte di data unit: un `pread` verso un offset di dati è un difetto, T-M2R1-01) e restituisce `fits_inventory`: dimensione del file, byte di header letti, per HDU indice, `EXTNAME`, `BITPIX`, assi, `BSCALE`, `BZERO`, offset di header e dati, numero di card; per il primario `layout`, `cfa`, unità, linearità (`image.lin`), fase, metadati interpretati, `header_cards`, `uninterpreted_keywords`. Si applicano gli stessi limiti di `fits_limits` e gli stessi errori della scansione (NFR-SEC-01); l'inventario non verifica `CHECKSUM` né il digest dei dati (richiederebbero i pixel).

Memoria: l'header vive in memoria per la durata della chiamata. La prenotazione nel pool, presa prima della scansione e senza attesa, è

`inventory_reserve_bytes(file_bytes) = 4096 + min(⌊file_bytes/80⌋, max_hdus · max_header_cards) · 128`

(contabilità convenzionale: 128 byte per card, tra testo, oggetto stringa e allocatore). Il pool limita le prenotazioni, non l'RSS ([ADR-06](ADR-06-budget-scheduler.md)). Budget sotto il minimo: `budget`, `sched.job_exceeds_budget`, con `minimo` e `budget` nel contesto; nessuna lettura dell'header.

HDU compressi a tile (tabelle binarie con `ZIMAGE`): il lettore M2 li rifiuta già (`unsupported`, `fits.hdu_kind`), quindi la clausola D2 sulla decodifica intera di un HDU compresso è vuota in M2 e nessun percorso di codice la gestisce; T-M2R1-10 verifica che inventario e anteprima si fermino.

### Anteprima a blocchi (F-IO-05, D2)

`make_preview(const fits_input&, const preview_request&, memory_pool&, const cancel_token*)` calcola, per ogni piano del primario (HDU non compresso, tutti i `BITPIX`), la media per blocco di `block_width × block_height` campioni (`preview_request`, entrambi in [1, `max_axis_length`], altrimenti `invalid_input`, `preview.block`). I blocchi di bordo sono più piccoli.

- Valore di un campione: `v = raw · BSCALE + BZERO` in f64 (stessa formula del lettore). Somma ricorsiva in f64 nell'ordine riga per riga dentro il blocco, con il solo contributo dei campioni finiti (`NaN`, ±Inf esclusi); media = somma / numero di campioni finiti, arrotondata a f32. Un blocco senza campioni finiti vale `NaN` e `finite_counts = 0`: nessun valore sostitutivo.
- Le estensioni `INVALID` e `VARIANCE` non si leggono: l'anteprima è sui soli campioni del primario. Limite dichiarato.
- Lettura per fasce di `block_height` righe di un piano con `pread` nella fascia prenotata; la fascia è l'unico buffer che scala con la larghezza. Un `pread` fallito è `io`, `fits.read`; fine file prematura `integrity`, `input.size_changed` o `io`, `input.short_read` ([ADR-05](ADR-05-dag-provenance.md)).
- Prenotazione, in una volta sola e senza attesa: `preview_reserve_bytes = inventory_reserve_bytes(file_bytes) + fascia + somme + uscita`, con fascia = `width · min(block_height, height) · |BITPIX|/8` (byte), somme = `⌈width/block_width⌉ · 12` (una somma f64 e un contatore u32 per colonna di blocchi), uscita = `⌈width/block_width⌉ · ⌈height/block_height⌉ · planes · 8` (un f32 e un u32 per blocco). Dopo la scansione dell'header, `need = preview_reserve_bytes`; budget sotto `need`: `budget`, `sched.job_exceeds_budget` con `minimo`. La fascia e le somme sono `pool_buffer` della prenotazione (un eccesso è `pool.reservation_exceeded`); il risultato riporta `reserved_bytes` e `observed_peak_bytes` (massimo di byte impegnati), sempre `observed_peak_bytes ≤ reserved_bytes`.
- Un cancel richiesto fra due fasce: `cancelled`, `sched.cancelled`.
- Una somma di blocco non finita (overflow di f64 con soli campioni finiti): `numeric`, `preview.sum_overflow`; una media finita in f64 ma oltre il campo di f32: `numeric`, `preview.f32_overflow`. Piano e blocco nel contesto; nessuna media sostitutiva. Una media non nulla che in f32 diventa zero o subnormale è un arrotondamento a f32 dichiarato, non un errore.
- `fits_input` registra dimensione, `mtime` e `ctime` all'apertura. `inventory_fits`, `make_preview`, `digest_fits_input` e i job di import, thumbnail e rebuild confrontano il file con quella fotografia dopo aver consumato i byte: dimensione diversa `integrity`, `input.size_changed`; `mtime` o `ctime` diversi `integrity`, `input.modified`. È il modello di identità di ADR-04 (stat), non una difesa contro una corsa deliberata.

Errori aggiuntivi:

| Condizione | Categoria | Codice |
|---|---|---|
| `ASTRPHAS` con valore non riconosciuto | `unsupported` | `fits.phase` |
| Fase assente all'import | `metadata_missing` | `import.phase_missing` |
| Fase o linearità dichiarate in conflitto con l'header | `incompatible` | `metadata.declared_conflict` |
| Operazione non ammessa dalla tabella | `incompatible` | `op.phase_blocked`, `op.linearity_blocked`, `op.linear_ancestor_missing` |
| Blocco dell'anteprima fuori intervallo | `invalid_input` | `preview.block` |
| Somma o media del blocco non finita | `numeric` | `preview.sum_overflow`, `preview.f32_overflow` |
| File cambiato dopo l'apertura | `integrity` | `input.size_changed`, `input.modified` |
| Budget sotto la prenotazione minima | `budget` | `sched.job_exceeds_budget` |

### Test di R1

| ID | Clausola |
|---|---|
| T-M2R1-01 | Inventario con data unit che risponde EIO a ogni offset di pixel (osservatore di `pread` del test): riesce, e i byte letti, contati dall'osservatore, sono al più header + 2880. Esatta. |
| T-M2R1-02 | Prenotazione di inventario e anteprima uguale alla formula, `observed_peak_bytes ≤ reserved_bytes`, picco del pool = prenotazione; budget sotto il minimo: stop con il minimo nel contesto; budget uguale al minimo: riesce. Esatta. |
| T-M2R1-03 | Anteprima contro l'oracle del test (media per blocco in f64 dai valori del generatore): tolleranza `γ_k · max\|x\|` con `k` = campioni sommati del blocco, più ½ ulp f32; zero sui valori dyadic. Un campione non finito: escluso; blocco tutto non finito: `NaN` e conteggio 0. |
| T-M2R1-05 | Matrice fase × linearità × operazione: ammesse solo le righe della tabella; le altre si fermano prima del calcolo, senza output. Dichiarazione in conflitto con l'header: stop. `unknown` mai lineare. |
| T-M2R1-06 | Calibrato e prestacked senza RAW: inventario, anteprima ed export; il preflight C-14 su `nonlinear` e `unknown` mostra il limite e non modifica i campioni. |
| T-M2R1-07 | Le keyword non interpretate nei report e nella provenance coincidono con l'insieme calcolato dal test sulle card generate. |
| T-M2R1-09 | PUB-LFC-01 e PUB-CFA-01 (hash registrato prima): importati e inventariati, `BAYERPAT` letto, roundtrip bit-esatto. |
| T-M2R1-10 | Fuzz breve di inventario e anteprima (header e data unit corrotti): stop controllato, nessun crash sotto ASan/UBSan, nessuna allocazione oltre il limite. |

## Fuori da M2

Tile e piramide; alpha; coverage e capability; maschere di editing usate dagli operatori; XISF e RAW (ADR-08.b, M3.a); unità `electron` prodotte da conversione di gain.

## Estensione ADR-03.b: tipi estesi (M3)

Stadio: ADR-03.b, tipi estesi (registro riga 3389; aggregato ADR-03 riga 3387, testo riga 2564). Archi: ADR-03.b `requires_all` ADR-03.a (riga 4081); F-IO-02 e C-11 `decided_by` ADR-03.b (righe 3686, 4126). Piano: §7.14 (asset di maschera), §8.4 (CFA e debayer), §8.6.2 (coverage, maschere, alpha), §9.6 (stato lasciato dal decoder), §9.8 (propagazione), §10.4 C-03 campi 1, 6 e 7, §10.12 C-11 campi 2, 3, 6, 7 e 10. Dipendenze fra contratti: [ADR-08.b](ADR-08-decoder-pack.md) §2.3, §2.4 e §2.6 (origine `decoder`), [ADR-17.a](ADR-17-varianza.md) (modello diagonale). Unità che lo implementano: M3.a (origine `decoder`, stato del decoder), M3.d (RGB da debayer), M3.g (coverage), M3.j (maschere fotografiche). Il tipo per gli input prestacked è già in «Aggiunte di R1» (`process_phase::prestacked`) e non cambia.

Ogni clausola è una **regola** (vincolo del prodotto, verificato dal test indicato), un **fatto** (con la fonte) o un'**ipotesi** (dichiarata, con il test o l'unità che la prova). Le parti di ADR-03.a sopra restano valide; i punti che cambiano sono elencati in §b.8.

Fuori da ADR-03.b: tile e piramide; alpha (§b.7); capability di misura (§9.8); modelli di covarianza fra campioni e fra canali (ADR-17.b); scelta del metodo di debayer e sua validazione (C-03, M3.d); generazione, edit e persistenza delle maschere (C-11, ADR-04.b, M3.j); uso del nero nella calibrazione e ledger dell'offset (C-01.b, ADR-16.b).

### b.1 Origine `decoder` dei metadati

`metadata_origin` ∈ {`header`, `declared`, `decoder`}. `decoder`: valore letto dal decoder RAW di ADR-08.b; `metadata_source.keyword` contiene il nome del campo di LibRaw da cui viene (per esempio `other.shutter`, `color.maximum`). Regole, verificate da T-M3A-TYP-01:

- L'origine `decoder` vale per ogni campo che porta un'origine: i campi di `frame_metadata`, `image.phase`, `image.lin_origin`, `decoder_state` (§b.2).
- Un valore `declared` diverso da un valore `decoder` già presente: `incompatible`, `metadata.declared_conflict`, come per `header`; uguale: nessun effetto.
- L'origine resta provenienza: l'uguaglianza e `equivalent_content` non la confrontano. Lo scrittore FITS scrive il valore nella sua keyword; la rilettura lo restituisce con origine `header`, mentre l'origine `decoder` resta nella provenance (ADR-05) e nel record del decoder (ADR-08.b §2.9).
- Il lettore FITS non produce mai l'origine `decoder`.

Campo nuovo in `frame_metadata`, richiesto da ADR-08.b §2.6 («ora di acquisizione»): `capture_time_libraw` (`metadata_value<std::int64_t>`), il valore di `other.timestamp` di LibRaw (fatto: tipo `time_t`, `/usr/include/libraw/libraw_types.h` riga 854). LibRaw non dichiara il fuso orario: il campo non è un istante UTC e non si scrive in `DATE-OBS`. Keyword `ASTRTSTP` (intera), interpretata. `camera` da decoder = `idata.make`, uno spazio, `idata.model`.

### b.2 Stato lasciato dal decoder

`image.decoder_state` (`std::optional<decoder_state>`), presente solo nelle immagini prodotte dal decoder RAW. Porta, con origine `decoder`, i dati di §9.6 che servono a chi consuma l'immagine (C-01.b, G-OFFSET); il record completo resta quello di ADR-08.b §2.9.

| Campo | Tipo | Contenuto |
|---|---|---|
| `raw_width`, `raw_height` | `uint32_t` | Dimensioni del buffer grezzo di LibRaw |
| `window` | `x0`, `y0`, `width`, `height` (`uint32_t`) | Area visibile importata nel buffer grezzo (ADR-08.b §2.3) |
| `black` | `uint32_t` | `color.black` |
| `cfa_color_index` | `std::array<uint8_t, 4>` | Indice di colore di LibRaw `FC(r, c)` dei quattro siti della cella 2×2 all'origine dell'area visibile, in ordine `(0, 0)`, `(0, 1)`, `(1, 0)`, `(1, 1)` (riga, colonna; ADR-08.b §2.3) |
| `black_per_color` | `std::array<uint32_t, 4>` | `color.cblack[0..3]`, per indice di colore di LibRaw |
| `black_pattern_rows`, `black_pattern_cols` | `uint32_t` | `color.cblack[4]`, `color.cblack[5]` |
| `black_pattern` | `std::vector<uint32_t>` | `color.cblack[6…]`, `rows · cols` valori |
| `white` | `uint32_t` | `color.maximum` |
| `data_maximum` | `std::optional<uint32_t>` | `color.data_maximum`, se non nullo |
| `linear_max` | `std::optional<std::array<int64_t, 4>>` | `color.linear_max[0..3]` (`long` in LibRaw), se almeno uno è non nullo |
| `linearization` | `identity`, `decoder_curve` | ADR-08.b §2.4 |
| `curve_sha256` | `std::optional<std::array<uint8_t, 32>>` | Presente se e solo se `linearization = decoder_curve` |
| `source` | `metadata_source` | Origine `decoder` |

Fatti: tipi dei campi in `/usr/include/libraw/libraw_types.h` righe 751-754 e 770. `cfa_color_index` serve perché il pattern di `cfa_layout` non distingue i verdi con indice 1 da quelli con indice 3, e `black_per_color` è per indice. Il nero del singolo campione si ricava da questi campi con la formula dell'ipotesi H3 di ADR-08.b §2.4; il tipo conserva la rappresentazione di LibRaw e non la valuta. `saturation_adu` di `frame_metadata` vale `white` con origine `decoder` (ADR-08.b §2.6).

Invarianti (`validate_image`, violazione `invalid_input`, `image.decoder_state`; T-M3A-TYP-02):

- `layout = cfa`, `phase` presente e uguale a `raw`, `source.origin = decoder`;
- `window.width = geometry.width`, `window.height = geometry.height`, `x0 + width ≤ raw_width`, `y0 + height ≤ raw_height`, calcolati senza overflow;
- `black_pattern.size() = black_pattern_rows · black_pattern_cols ≤ 4098` (fatto: `LIBRAW_CBLACK_SIZE = 4104`, `/usr/include/libraw/libraw_const.h` riga 72, meno i sei elementi iniziali); una sola delle due dimensioni nulla è una violazione;
- `curve_sha256` presente se e solo se `linearization = decoder_curve`;
- `cfa_color_index` coerente con `cfa`: indice 0 sul sito rosso, 2 sul sito blu, 1 o 3 sui due siti verdi (le combinazioni ammesse da ADR-08.b §2.3).

Un'operazione che produce un'immagine con `phase` diversa da `raw` non copia `decoder_state`: lo consuma e lo registra nella provenance (C-01.b). Persistenza: lo scrittore FITS non rappresenta `decoder_state`; un'immagine che lo porta non si scrive: `unsupported`, `fits.decoder_state`, nessun buffer (T-M3A-TYP-02). L'immagine si ricostruisce decodificando di nuovo l'originale immutabile (ADR-04, ADR-05). Limite dichiarato: un master costruito da RAW ha bisogno dello stato del decoder dei suoi frame, e dove lo porta (descrittore derivato dal nodo, ADR-16.b) non si decide qui.

### b.3 RGB da debayer

Il debayer produce un'immagine con `layout = rgb`, `planes = 3`, `cfa` assente, stessa geometria e stessa unità dell'ingresso CFA. Ordine dei piani: 0 = R, 1 = G, 2 = B, regola del prodotto per ogni immagine `rgb`; un file RGB esterno con un ordine diverso non è riconoscibile dall'header (limite dichiarato). I due verdi della CFA (G1, G2, canali distinti in ADR-03.a) finiscono nel piano G; quale sito CFA ha generato un campione resta ricavabile da `demosaic.source_cfa`.

`image.demosaic` (`std::optional<demosaic_info>`):

| Campo | Tipo | Contenuto |
|---|---|---|
| `source_cfa` | `cfa_layout` | Pattern e offset della CFA di ingresso |
| `method`, `method_version` | `std::string` non vuota, `uint32_t` | Algoritmo e versione (scelta in C-03) |
| `measured_preserved` | `bool` | Il metodo lascia invariato, bit per bit, il campione CFA nel piano del suo canale |
| `variance_propagation` | `linear_weights`, `fixed_weight_approximation` | Come è stata ottenuta la varianza dei campioni stimati (sotto) |

Classe di un campione, funzione `sample_kind(x, y, plane)` ricavata dai campi, senza un piano aggiuntivo: `measured` se `measured_preserved` è vero e `plane` è il piano del canale che `source_cfa` assegna al pixel `(x, y)` con la regola di ADR-03.a (`pattern` traslato di `(x + x_offset, y + y_offset)`); altrimenti `interpolated`. Con `measured_preserved` falso ogni campione è `interpolated`. T-M3A-TYP-04.

Invarianti (`validate_image`, `invalid_input`, `image.demosaic`; T-M3A-TYP-04): `demosaic` presente implica `layout = rgb`, `planes = 3`, `cfa` assente, `method` non vuoto, offset di `source_cfa` in {0, 1}.

Validità di `demosaic` dopo altre operazioni: un'operazione per pixel e per piano (stesso pixel e stesso piano in ingresso e in uscita, per esempio scala o sottrazione per canale) lo copia; un'operazione il cui campione di uscita dipende da più posizioni, da più piani o da una geometria diversa (resampling, convoluzione, combinazione di canali) produce `demosaic` assente, e l'incertezza dei suoi campioni vive solo nel piano di varianza. La regola si verifica nei test dell'operatore, nella sua unità (C-06 in M3.g, C-09 in M3.k).

FITS (T-M3A-TYP-05): keyword interpretate, scritte quando `demosaic` è presente e lette solo come insieme completo: `ASTRDPAT` (stringa, pattern con i valori di `BAYERPAT`), `ASTRDXOF`, `ASTRDYOF` (interi 0 o 1), `ASTRDMTH` (stringa), `ASTRDVER` (intero ≥ 0), `ASTRDKEP` (logico, `measured_preserved`), `ASTRDVAR` (stringa, `variance_propagation`). Insieme incompleto: `invalid_input`, `fits.demosaic_incomplete`; presenti con `layout` diverso da `rgb`: `invalid_input`, `fits.demosaic_layout`; valore non riconosciuto: `unsupported`, `fits.demosaic_value`. `BAYERPAT` su un'immagine con `planes > 1` resta `fits.cfa_planes` di ADR-03.a: la CFA d'origine di un RGB non usa mai `BAYERPAT`.

#### Varianza dei campioni

Il piano di varianza di ADR-03.a e ADR-17.a è già per pixel e per piano (`variance_plane`, stessa geometria e `sample_type`), quindi un RGB porta una varianza per canale. Le regole seguenti dicono che cosa contiene; il valore numerico lo calcola il metodo di C-03.

- Campione `measured`: varianza uguale a quella del campione CFA calibrato da cui è copiato.
- Campione `interpolated` di un metodo lineare a pesi fissi (`variance_propagation = linear_weights`): se `ŷ = Σ_i w_i x_i` con pesi `w_i` che dipendono solo dalla posizione e dal pattern, `Var(ŷ) = Σ_i Σ_j w_i w_j Cov(x_i, x_j)`. È l'identità della varianza di una combinazione lineare; riferimento pubblico, non riletto in questa unità: legge di propagazione dell'incertezza della GUM, JCGM 100:2008, §5.1.2 eq. (10) senza correlazioni e §5.2.2 eq. (13) con correlazioni. Con l'ingresso nel modello `diagonal_independent` di ADR-17.a si riduce a `Σ_i w_i² Var(x_i)`, esatta rispetto a quel modello, anche con pesi negativi (filtri lineari come quelli di Malvar, He e Cutler, ICASSP 2004, riferimento di C-03 non letto in questa unità). Calcolo in f64, un solo arrotondamento al tipo di uscita, come ADR-17.a.
- Campione `interpolated` di un metodo con pesi che dipendono dai dati (edge-aware): la propagazione al primo ordine (GUM §5.1.2) richiede le derivate dei pesi rispetto ai dati. La formula a pesi fissi le omette ed è un'approssimazione, dichiarata con `variance_propagation = fixed_weight_approximation`. Il suo errore non è misurato: C-03 lo misura, con soglia fissata prima, prima di ammettere in P0 un metodo di questa classe. Ipotesi.
- Un campione invalido della CFA non entra mai nei pesi con il suo valore: o il metodo lo esclude con una regola dichiarata in C-03, o il campione di uscita eredita l'unione dei bit (regola T-M2E-14 di ADR-03.a) ed è invalido.

`ŷ` è in generale correlato con i campioni misurati da cui deriva e con i campioni stimati vicini (correlazione spaziale); i piani dello stesso pixel possono essere correlati fra loro quando i loro pesi condividono campioni di ingresso (correlazione fra canali), e non lo sono, sotto `diagonal_independent`, quando li costruiscono insiemi disgiunti di campioni. Il piano diagonale non rappresenta queste covarianze: contiene solo le varianze **marginali** (§b.4). La rappresentazione delle covarianze spetta ad ADR-17.b.

### b.4 Portata del piano di varianza

`image.variance_scope` (`std::optional<variance_scope>`):

- `independent`: il piano è la covarianza completa sotto il modello `diagonal_independent` di ADR-17.a;
- `marginal`: il piano contiene solo la diagonale di una covarianza che ha termini fuori diagonale non rappresentati (debayer, resampling).

Regole (T-M3A-TYP-06):

- Un'immagine prodotta da un operatore con `variance` presente ha `variance_scope` presente. La calibrazione di ADR-17.a produce `independent`; il debayer produce `marginal` (con tre piani e un solo canale misurato per pixel, ogni RGB da debayer ha campioni `interpolated`). `operator_output_invariants_hold` verifica la presenza e, con `demosaic` presente, il valore `marginal`; il valore `independent` della calibrazione lo verifica T-M3A-TYP-06 sull'uscita della calibrazione.
- Un operatore che assume l'indipendenza fra campioni non accetta un ingresso `marginal` e si ferma con `unsupported`, `var.marginal_input` prima di ogni calcolo: la calibrazione di ADR-17.a, il debayer (entrambe le formule di §b.3 assumono l'ingresso `diagonal_independent`) e ogni somma o media su più pixel che dichiari un'incertezza. Quale modello di ADR-17.b sblocchi questi casi lo decide ADR-17.b.
- Un'immagine letta da file senza la keyword ha `variance_scope` assente; per la calibrazione vale, invariata, la dichiarazione `variance_source` della ricetta (ADR-17.a).
- FITS: keyword `ASTRVSCP` (stringa `independent` o `marginal`, senza distinzione di maiuscole), scritta quando `variance_scope` è presente. Valore diverso: `unsupported`, `fits.variance_scope`. Keyword senza HDU `VARIANCE`: `invalid_input`, `fits.variance_scope_without_plane`.

### b.5 Coverage

Grandezza (§8.6.2): frazione geometrica dell'area di un pixel di uscita coperta da campioni validi in ingresso. Non è la somma dei pesi, non è una maschera di invalidità, non è una maschera fotografica, non è alpha. Il numero di frame che contribuiscono a un pixel dello stack è un'altra grandezza: la definisce C-07 e non usa questo tipo.

`coverage_plane`: un campione per pixel e per piano, stessa geometria e stesso `sample_type` dell'immagine, buffer allineato a 64 byte; `image.coverage` è `std::optional<coverage_plane>`. Invarianti (`validate_image`, `invalid_input`, `image.coverage`; T-M3A-TYP-07):

- ogni valore è finito e in `[0, 1]`, anche sui pixel invalidi: la coverage è geometrica e resta definita dove il campione è `NaN` (eccezione esplicita alla regola «campione e varianza NaN» di ADR-03.a);
- in un'immagine prodotta da un operatore, coverage 0 su un piano implica il bit `support_incomplete` (§b.6) su quel pixel e quel piano;
- con coverage in `(0, 1)` la validità segue la politica dei bordi dichiarata dall'operatore (rinormalizzare, invalidare, accettare la perdita di flusso; §8.6.2, C-06).

FITS: HDU `EXTNAME = 'COVERAGE'`, stesso BITPIX float dei campioni, scala identità (altrimenti `invalid_input`, `fits.extension_encoding`), geometria del primario (`fits.extension_geometry`). Un valore non finito o fuori da `[0, 1]` letto dal file: `invalid_input`, `fits.coverage_range`. Lo scrittore scrive l'HDU se `coverage` è presente; rilettura identica (T-M3A-TYP-07).

### b.6 Bit di invalidità aggiunto

| Bit | Nome | Origine |
|---|---|---|
| 9 | `support_incomplete` | Il campione di uscita non ha il supporto che l'operatore richiede: bordo dell'immagine fuori dal kernel, vicini validi insufficienti secondo la regola dichiarata dall'operatore (debayer, resampling), coverage 0 |

I bit riservati diventano 10–15 (`reserved_mask = 0xFC00`). Un file con il bit 9 acceso, rifiutato da ADR-03.a come riservato, si legge; un bit fra 10 e 15 resta `invalid_input`, `fits.invalid_reserved_bits` (T-M3A-TYP-08).

### b.7 Maschere fotografiche

La maschera fotografica di C-11 è `edit_mask` di ADR-03.a: un valore f32 in `[0, 1]` per pixel, un solo piano, che si applica a tutti i piani dell'immagine. Le maschere per canale non sono in P0 (F-POST-08.a).

Regole di tipo (T-M3A-TYP-09):

- `invalid_mask`, `edit_mask`, `coverage_plane` e `variance_plane` sono tipi distinti, senza conversioni implicite né esplicite fra loro; verifica in compilazione con `std::is_convertible` e `std::is_constructible`, in entrambe le direzioni.
- `edit_mask` non ha alcuna API che legga o scriva `invalid`, `coverage` o `variance` di un'immagine: il valore della maschera non entra nei bit di invalidità né nella coverage. Un pixel invalido resta invalido qualunque sia il valore della maschera; uno 0 della maschera non rende invalido un pixel (C-11 campo 7). La precedenza sui dati la verifica G-MASK in M3.j. L'assenza dell'API non ha un test: è un controllo della review sul diff di ogni unità che tocca `edit_mask` (nessun metodo e nessuna funzione libera che riceva un `edit_mask` e un riferimento non costante a `image`, `invalid_mask`, `coverage_plane` o `variance_plane`), dichiarato come tale.
- Una maschera usata con un'immagine ne ha `width` e `height`; altrimenti `invalid_input`, `mask.edit_geometry`. Il controllo è `check_edit_mask(const image&, const edit_mask&)`, chiamato dagli operatori di C-11 prima del calcolo.
- Valori `NaN`, ±Inf o fuori da `[0, 1]` restano `mask.edit_range` (ADR-03.a). Il feather e le altre operazioni non producono valori fuori intervallo (C-11 campo 6): un valore fuori intervallo prodotto da un operatore è uno stop, non un clip silenzioso.

`mask_info`, i dati della maschera richiesti da C-11 campo 10 e da §7.14 (T-M3A-TYP-10):

| Campo | Tipo | Regola |
|---|---|---|
| `kind` | `painted`, `imported`, `derived`, `combined` | §7.14: le prime due sono asset autorevoli, le altre derivate |
| `revision` | `uint64_t` ≥ 1 | Revisione della maschera |
| `rasterizer` | `std::optional<std::string>` (identificatore e versione) | Presente se e solo se `kind` è `painted` o `combined` |
| `sources` | elenco di (`node_id`, `revision`) | Vuoto per `painted` e `imported`; un elemento per `derived`; almeno due per `combined` |

Violazione: `invalid_input`, `mask.info`. Generatori, eventi, rasterizzatore, cicli di dipendenza e persistenza sono di C-11, ADR-04.b e M3.j; il formato su disco della maschera non è deciso qui.

Alpha non è un campo di `image` in ADR-03.b. Se un formato di export lo richiede, la regola sta nel suo contratto (C-10.b), che non ricava alpha da `invalid_mask`, `edit_mask` o `coverage` senza una regola dichiarata.

### b.8 Modifiche ad ADR-03.a

- `metadata_origin` ha il valore `decoder` (§b.1); `frame_metadata` ha `capture_time_libraw`.
- `image` ha `decoder_state`, `demosaic`, `variance_scope`, `coverage`; `equivalent_content` li confronta tutti, salvo l'origine di `decoder_state.source` (T-M3A-TYP-12).
- Bit 9 definito; riservati 10–15.
- Keyword interpretate in più: `ASTRTSTP`, `ASTRDPAT`, `ASTRDXOF`, `ASTRDYOF`, `ASTRDMTH`, `ASTRDVER`, `ASTRDKEP`, `ASTRDVAR`, `ASTRVSCP`. Tipo della card verificato senza coercizione; duplicati e `CONTINUE` come le altre (ADR-03.a).
- HDU `COVERAGE` ammesso, oltre a `INVALID` e `VARIANCE`.
- Limiti: `max_file_bytes` copre campioni, `INVALID`, `VARIANCE` e `COVERAGE` f64, cioè 26 byte per campione: `2^28 · 26 + 16 MiB = 6 672 MiB`. Gli altri limiti non cambiano. Il preflight aggiunge la coverage al picco se l'HDU esiste. T-M2A-05 si riesegue sul nuovo confine (T-M3A-TYP-11).
- `operator_output_invariants_hold` aggiunge: `variance_scope` presente se c'è `variance`, uguale a `marginal` se c'è anche `demosaic`; coverage finita in `[0, 1]`; coverage 0 implica `support_incomplete`; `demosaic` solo con `layout = rgb`.

### b.9 Errori aggiunti

| Condizione | Categoria | Codice |
|---|---|---|
| Invarianti di `decoder_state` violati | `invalid_input` | `image.decoder_state` |
| Scrittura FITS di un'immagine con `decoder_state` | `unsupported` | `fits.decoder_state` |
| Invarianti di `demosaic` violati | `invalid_input` | `image.demosaic` |
| Keyword `ASTRD*` incomplete; su layout non `rgb`; con valore non riconosciuto | `invalid_input`; `invalid_input`; `unsupported` | `fits.demosaic_incomplete`; `fits.demosaic_layout`; `fits.demosaic_value` |
| `ASTRVSCP` non riconosciuta; senza HDU `VARIANCE` | `unsupported`; `invalid_input` | `fits.variance_scope`; `fits.variance_scope_without_plane` |
| Ingresso `marginal` a un operatore che assume indipendenza | `unsupported` | `var.marginal_input` |
| Invarianti di `coverage` violati; coverage letta fuori intervallo | `invalid_input` | `image.coverage`; `fits.coverage_range` |
| Maschera fotografica di geometria diversa dall'immagine | `invalid_input` | `mask.edit_geometry` |
| `mask_info` incoerente | `invalid_input` | `mask.info` |

### b.10 Test pianificati

Tolleranze e criteri fissati qui, prima di ogni misura. Oracle scritti a mano nel test, indipendenti dall'implementazione.

| ID | Verifica | Criterio |
|---|---|---|
| T-M3A-TYP-01 | Origine `decoder` su ogni campo di `frame_metadata`, su `phase` e su `lin_origin`; `declared` uguale: nessun effetto; diverso: `metadata.declared_conflict`; uguaglianza ed `equivalent_content` indipendenti dall'origine; scrittura e rilettura FITS: valori uguali, origine `header`; il lettore non produce mai `decoder` | Esatta |
| T-M3A-TYP-02 | Ogni invariante di `decoder_state` violato uno alla volta, compreso l'overflow di `x0 + width`, il motivo del nero con 4098 valori (passa) e 4099 (stop), `cfa_color_index` incoerente con il pattern; per ognuno dei quattro pattern, gli stati validi con lo stesso pattern e diversa assegnazione degli indici verdi (1/3, 3/1, 1/1) accettati e diversi fra loro; scrittura FITS con `decoder_state` presente | `image.decoder_state` per ogni violazione; `fits.decoder_state` e nessun buffer |
| T-M3A-TYP-03 | `capture_time_libraw`: `ASTRTSTP` scritta e riletta; `ASTRTSTP` stringa o reale | Esatta; `fits.keyword_value` |
| T-M3A-TYP-04 | `sample_kind` per i quattro pattern × quattro coppie di offset su una griglia 4×4 × 3 piani, contro una tabella scritta a mano; `measured_preserved` falso: tutto `interpolated`; ogni invariante di `demosaic` violato uno alla volta | Esatta: 0 differenze; `image.demosaic` |
| T-M3A-TYP-05 | Keyword `ASTRD*`: scrittura e rilettura identiche; ciascuna tolta una alla volta; presenti su `mono`, `cfa`, `multi_plane`; valori non riconosciuti; `BAYERPAT` su un RGB | `fits.demosaic_incomplete`, `fits.demosaic_layout`, `fits.demosaic_value`, `fits.cfa_planes` |
| T-M3A-TYP-06 | `ASTRVSCP` scritta e riletta con i due valori; valore ignoto; keyword senza `VARIANCE`; immagine prodotta con varianza e senza `variance_scope`: invariante falso; RGB con `demosaic` e `variance_scope = independent`: invariante falso; uscita della calibrazione: `variance_scope = independent`; calibrazione con un ingresso `marginal` | Esatta; stop con i codici di §b.4; zero calcoli nella calibrazione (contatore del `fault_injector`) |
| T-M3A-TYP-07 | Coverage: valori 0, 1, `−0`, `nextafter(1, 2)`, `nextafter(0, −1)`, `NaN`, ±Inf; coverage finita su pixel invalidi accettata; coverage 0 senza `support_incomplete` in un'immagine prodotta; HDU `COVERAGE` scritto e riletto; HDU con valore fuori intervallo, BITPIX diverso o geometria diversa | Confini e `−0` accettati, gli altri valori `image.coverage`; invariante falso; rilettura bit-esatta; `fits.coverage_range`, `fits.extension_encoding`, `fits.extension_geometry` |
| T-M3A-TYP-08 | Maschera `INVALID` con il bit 9 letta; con un bit fra 10 e 15 | Lettura riuscita; `fits.invalid_reserved_bits` |
| T-M3A-TYP-09 | `static_assert` di non convertibilità e non costruibilità fra `invalid_mask`, `edit_mask`, `coverage_plane`, `variance_plane`, in entrambe le direzioni; `check_edit_mask` con larghezza o altezza diverse | Compilazione; `mask.edit_geometry` |
| T-M3A-TYP-10 | `mask_info`: per ogni `kind`, `rasterizer` presente e assente, `sources` con 0, 1 e 2 elementi, `revision = 0` | Ammesse solo le combinazioni della tabella di §b.7; le altre `mask.info` |
| T-M3A-TYP-11 | T-M2A-05 rieseguito con il nuovo `max_file_bytes` e con l'HDU `COVERAGE` nel picco del preflight | Confine passa, confine+1 si ferma senza allocazione |
| T-M3A-TYP-12 | `equivalent_content` su coppie di immagini valide che differiscono in un solo elemento: presenza di `decoder_state`, `demosaic`, `variance_scope`, `coverage`, e per ciascuno un campo o un valore diverso; coppie che differiscono solo nell'origine di `decoder_state.source` | Diverse: `false` per ogni coppia; solo origine: `true` |

Test del piano che questi alimentano: T-IO-01 (import RAW, M3.s), i test di debayer di C-03 (M3.d), G-MASK (M3.j). I test della varianza dei campioni interpolati (oracle razionale per `linear_weights`, Monte Carlo per `fixed_weight_approximation`) sono di C-03 e si fissano nel suo campo 9.

### b.11 Limiti dichiarati

- Ordine dei piani R, G, B: regola del prodotto, non verificabile su un RGB esterno.
- `capture_time_libraw` non è un istante UTC: il fuso orario non è noto.
- `decoder_state` non si scrive in FITS; un master costruito da RAW lo porta nei parametri del suo nodo (ADR-16.b §b.7).
- Il piano di varianza di un RGB da debayer contiene solo varianze marginali; covarianze fra campioni e fra canali assenti fino ad ADR-17.b. Un operatore che assume l'indipendenza si ferma su questi ingressi.
- `fixed_weight_approximation` è un'approssimazione di errore non misurato: C-03 lo misura prima di usarla in P0.
- Alpha e capability di misura non sono tipi di questa estensione.

### b.12 Alpha

Grandezza (§7.3 righe 697-704, §8.6.2): canale di composizione o di trasparenza per il display e per l'export. Non è una maschera di invalidità, né una coverage, né una maschera fotografica. Serve all'export TIFF di C-10.b (NFR-EXP-01.b, M3.s).

Dipendenza: §§b.12–b.18 citano ADR-16.b in `docs/contracts/ADR-16-stati-master.md`, con §b.2 e §b.3 allineati a §b.14 da [C-01.b](C-01-calibrazione-light.md) (light fuori dominio: stop; master: `excluded`). Un cambio dei paragrafi citati riapre i punti che li citano.

`alpha_plane`: un valore f32 per pixel, un solo piano, buffer allineato a 64 byte. Non è un campo di `image`, non entra in `equivalent_content` e nessun operatore che produce campioni, varianza, `invalid` o `coverage` lo riceve. Invarianti (`validate_alpha(const image&, const alpha_plane&)`, T-M3A-TYP-13): `width` e `height` uguali a quelli dell'immagine, altrimenti `invalid_input`, `alpha.geometry`; ogni valore finito in `[0, 1]`, altrimenti `invalid_input`, `alpha.range`.

Semantica: alpha **non associato**. I campioni di colore non si moltiplicano mai per alpha, né in memoria né all'export: sarebbe una trasformazione dei pixel non dichiarata (C-10 campo 1). Fatto: libtiff 4.7.0 definisce per il tag `ExtraSamples` (338) i valori `EXTRASAMPLE_ASSOCALPHA = 1` (alpha associato) ed `EXTRASAMPLE_UNASSALPHA = 2` (alpha non associato) (`/usr/include/x86_64-linux-gnu/tiff.h` righe 328-331). Regola: l'export che scrive un `alpha_plane` in TIFF lo dichiara con il valore 2. Profondità, quantizzazione e regole offerte sono di C-10.b. La specifica TIFF 6.0 che definisce il tag non è riletta in questa unità.

Alpha esiste solo come uscita di `derive_alpha(const image&, alpha_rule, const edit_mask*)`, con la regola dichiarata nella ricetta dell'export. Non c'è una regola di default: senza regola il file non ha alpha. `inv(x, y)` è vero se il pixel ha almeno un bit di `invalid_reason` acceso in almeno un piano.

| `alpha_rule` | `α(x, y)` | Prerequisito |
|---|---|---|
| `validity` | 0 se `inv(x, y)`, altrimenti 1 | — |
| `coverage` | 0 se `inv(x, y)`, altrimenti `min_p coverage(x, y, p)` arrotondato a f32 | `coverage` presente (§b.5) |
| `edit_mask` | 0 se `inv(x, y)`, altrimenti `m(x, y)` | maschera passata, `check_edit_mask` superato (§b.7) |

Regole (T-M3A-TYP-14):

- Un solo verso: `derive_alpha` legge immagine e maschera e non le modifica; alpha non scrive bit di invalidità, coverage o maschere. α = 0 non rende invalido un pixel; α = 1 non rende valido un pixel invalido.
- Esattezza: `validity` ed `edit_mask` producono 0, 1 o la copia di un f32, senza arrotondamenti. `coverage` è esatta con campioni f32; con campioni f64 è un solo arrotondamento al più vicino del minimo in f32. Il risultato resta in `[0, 1]`: 0 e 1 sono rappresentabili in f32, quindi il float più vicino a un valore in `[0, 1]` non supera 1 e non scende sotto 0.
- Prerequisiti, controllati prima di allocare il piano: regola `coverage` senza coverage, `incompatible`, `alpha.coverage_missing`; regola `edit_mask` senza maschera, o maschera passata con un'altra regola, `invalid_input`, `alpha.mask_argument`; maschera di geometria diversa, `mask.edit_geometry`.
- Provenienza: `alpha_info` = regola e, per `edit_mask`, `node_id` e `revision` della maschera (`mask_info`, §b.7). Entra nei parametri del nodo di export e nel manifest (C-10.b).
- Tipi distinti: `alpha_plane` si aggiunge ai quattro tipi di §b.7, senza conversioni implicite né esplicite con `invalid_mask`, `edit_mask`, `coverage_plane`, `variance_plane`, in entrambe le direzioni (T-M3A-TYP-13).
- FITS: lo scrittore di ADR-03 non ha un HDU per alpha. Un FITS porta già `INVALID` e `COVERAGE`, da cui alpha si ricava con una regola della tabella.
- Memoria: 4 byte per pixel, prenotati dal job di export (ADR-06, C-10.b).

### b.13 Capability di misura

Grandezza (§7.3 righe 699 e 706, §7.13 riga 852, §9.8 riga 1312): che cosa l'immagine può sostenere in una misura. Non c'è un booleano di validità scientifica: la capability è l'elenco delle limitazioni note, e l'idoneità si decide per misura, insieme ai motivi.

Codici di limitazione, lista chiusa nell'ordine canonico della tabella. Un codice **memorizzato** viene dalla storia dell'immagine e si conserva in `image.measure`; un codice **derivato** si calcola dai campi dell'immagine e non si memorizza mai.

| Codice | Classe | Condizione |
|---|---|---|
| `history.unknown` | memorizzato | Un ingresso dell'operatore aveva `measure` assente e fase diversa da `raw`, o assente |
| `calib.omitted.dark_subtraction`, `calib.omitted.bias_subtraction`, `calib.omitted.dark_flat_subtraction`, `calib.omitted.flat_bias_subtraction`, `calib.omitted.flat_normalization`, `calib.omitted.flat_division` | memorizzato | Voce omonima del ledger di ADR-16.b §b.10 con `status = omitted` |
| `calib.offset.known_not_removed`, `calib.offset.unknown` | memorizzato | Voce `light_offset` di ADR-16.b §b.10 con quello stato |
| `calib.assumption.h_dprime` | memorizzato | Ricetta con `D'` o `DF'` (ipotesi H-D', ADR-16.b §b.2) |
| `calib.assumption.flat_thermal` | memorizzato | Ricetta con `flat.bias` (limite termico dichiarato, ADR-16.b §b.5) |
| `calib.assumption.decoder_black` | memorizzato | Ricetta con ingressi dotati di stato del decoder (nero come sentinella, ADR-16.b §b.7; ipotesi H3 di ADR-08.b §2.4) |
| `linearity.nonlinear` | derivato | `lin = nonlinear` |
| `linearity.unknown` | derivato | `lin` assente |
| `variance.absent` | derivato | `variance` assente |
| `variance.scope_unknown` | derivato | `variance` presente e `variance_scope` assente |
| `variance.marginal` | derivato | `variance_scope = marginal` (§b.4) |
| `variance.fixed_weight_approximation` | memorizzato | Aggiunto dal debayer con `variance_propagation = fixed_weight_approximation` (§b.3) e da ogni operatore che dichiara un'approssimazione della propagazione della varianza |
| `samples.interpolated` | memorizzato | Aggiunto dal debayer (§b.4: ogni RGB da debayer ha campioni `interpolated`) e da ogni operatore il cui campione di uscita è stimato da più posizioni di ingresso (resampling, convoluzione) |

`measure_capability`: `limits` (`std::vector<measure_limit>`), soli codici memorizzati, in ordine canonico, senza duplicati (`validate_image`, `invalid_input`, `image.measure`). `image.measure` è `std::optional<measure_capability>`: assente significa «non valutata», mai «nessuna limitazione».

Funzioni (T-M3A-TYP-16):

- `history_limits(const image&)`: `measure->limits` se `measure` è presente; altrimenti `{history.unknown}` se la fase non è `raw` o è assente, vuoto se è `raw`. La fase `raw` è una dichiarazione dell'import (R1) che il prodotto non verifica: la capability la eredita.
- `measure_limits(const image&)`: unione di `history_limits` e dei codici derivati veri, in ordine canonico.
- `merge_measure(ingressi, propri)`: unione, in ordine canonico, di `history_limits` di ogni ingresso e dei codici memorizzati che l'operatore aggiunge. Ogni operatore la usa per la propria uscita, quindi un codice memorizzato non sparisce lungo il DAG. Nessun operatore di ADR-03.b rimuove un codice. La funzione si verifica in T-M3A-TYP-16; che un operatore la usi si verifica nei test dell'operatore, nella sua unità (calibrazione `.b` in T-M3A-TYP-17).
- `assess_measure(const image&, const measure_acceptance&)` → `measure_assessment {limits, blocking}`: `limits` = `measure_limits`, `blocking` = i codici di `limits` assenti da `measure_acceptance`. L'insieme accettato lo dichiara il contratto di ciascuna misura, non questo. `require_measure_ready` con `blocking` non vuoto: `incompatible`, `measure.limit_blocked`, codici nel contesto, prima di ogni calcolo della misura. Una misura eseguita riporta `limits` nel suo report: sono i motivi e le limitazioni che §7.3 riga 706 chiede accanto a `measure_ready`.

Regole:

- `operator_output_invariants_hold`: `measure` presente e valido in ogni immagine prodotta da un operatore.
- Calibrazione `.b`: aggiunge i codici `calib.*` della tabella dal ledger e dalla ricetta del nodo (T-M3A-TYP-17). La ricetta `.a` di ADR-16.a non aggiunge codici. Debayer e resampling aggiungono `samples.interpolated` e, quando la dichiarano, `variance.fixed_weight_approximation`: memorizzati, i due codici restano anche dopo un'operazione che toglie `demosaic` (§b.3). `operator_output_invariants_hold` verifica che un'uscita con `demosaic` li porti (il secondo se `variance_propagation = fixed_weight_approximation`); che resampling e convoluzione aggiungano `samples.interpolated` si verifica nei test dell'operatore, nella sua unità (C-06 in M3.g, C-09 in M3.k).
- FITS (T-M3A-TYP-15): `ASTRMCAP` (logico, `T`) scritta quando `measure` è presente; `ASTRML01`…`ASTRML99` (stringhe), un codice memorizzato per card, numerate da `01` senza buchi, nell'ordine canonico. I codici memorizzati sono 14: due cifre bastano. In lettura: `ASTRMCAP = F`, card `ASTRMLnn` senza `ASTRMCAP`, numerazione con buchi, ordine non canonico o codice duplicato: `invalid_input`, `fits.measure_keywords`; codice derivato o ignoto: `unsupported`, `fits.measure_code`. Senza `ASTRMCAP`, `measure` è assente.

Le altre capability elencate al §7.3 riga 699 hanno già un tipo: CFA è `plane_layout`/`cfa`; calibrazione, stacking e analisi sono righe della tabella delle operazioni (§b.14), che i contratti di stacking e di analisi estendono; il display è sempre ammesso (`inventory`, `preview`, `export_bundle`). Di §7.13, `linear` è `lin` e `wcs_valid` è di ADR-10. Tile e piramide sono opzioni dell'aggregato ADR-03 (riga 2564, colonna «Opzioni»), non parte della proposta, e non sono tipi di `.b`.

### b.14 Tabella delle operazioni e ADR-16.b §b.2

Le righe di R1 restano. `calibrate` resta la riga della ricetta `.a` (ADR-16.a: quattro ingressi `raw`). La calibrazione `.b` usa tre righe nuove, assegnate a ogni ingresso dal suo ruolo e dal suo stato dichiarato secondo ADR-16.b §b.2:

| Operazione | Ingressi (ADR-16.b §b.2) | Fase ammessa | Linearità ammessa |
|---|---|---|---|
| `calibrate_light` | light | `raw` | `linear` |
| `calibrate_master_raw` | bias, `D`, `DF`, flat `raw` | `raw` | `linear` |
| `calibrate_master_prepared` | `D'`, `DF'`, flat `corrected_unnormalized`, flat `calibrated` | `raw`, `calibrated` | `linear` |

Regole (T-M3A-TYP-18):

- `prestacked` non è ammessa in nessuna delle tre righe.
- Esito di `require_operation_allowed`: codici di R1, la fase prima della linearità (`op.phase_blocked`, `op.linearity_blocked`), con il ruolo dell'ingresso nel contesto.
- Nell'analisi (G-CAL, ADR-16.b §b.1) un master fuori dalla sua riga è `excluded` con motivo `phase` o `linearity` (ADR-16.b §b.3), non uno stop. Un light fuori dalla sua riga ferma l'analisi con quei codici, perché senza light non esiste alcuna ricetta. Nel preflight del nodo valgono ADR-16.b §b.8 e §b.9.
- Un master di stato `unknown`, `scaled` o incompleto non ha riga: è `excluded` per stato (ADR-16.b §b.2) prima del controllo di fase.
- `master_create` resta come in R1. Se i master `bias_subtracted` di C-02.b hanno ingressi in un'altra fase, la riga la aggiunge C-02.b.
- Le misure applicano `linear_ancestor_required`, poi `require_measure_ready` (§b.13). `derive_alpha` è parte dell'export, già sempre ammesso.

### b.15 Modifiche a §b.1–b.11 e ad ADR-03.a

- Introduzione dell'estensione, «Fuori da ADR-03.b»: alpha e capability di misura escono dall'elenco (§b.12, §b.13). Unità che implementano le parti nuove: M3.a (capability, FITS, righe di §b.14 nell'analisi G-CAL), M3.c (codici della calibrazione `.b`), M3.s (alpha).
- §b.7, ultimo paragrafo: le regole per ricavare alpha sono in §b.12; C-10.b sceglie quali offrire e come codificarle. Alpha resta fuori da `image`.
- §b.8: `equivalent_content` non confronta `decoder_state.source` (origine e `keyword`, provenienza come in §b.1); sostituisce «salvo l'origine di `decoder_state.source`». Confronta `measure` (presenza ed elenco dei codici).
- §b.10, T-M3A-TYP-12: la coppia «solo origine», non costruibile perché §b.2 ammette solo `source.origin = decoder`, diventa «solo provenienza»: lo stesso `decoder_state` con `source.keyword` diverso (per esempio `color.black` e `color.maximum`), origine `decoder` in entrambe; criterio `true`. Le coppie che differiscono in `measure` sono in T-M3A-TYP-15.
- §b.11, ultimo punto: decade.
- ADR-03.a: keyword interpretate in più `ASTRMCAP` e `ASTRML01`…`ASTRML99` (tipo verificato senza coercizione, duplicati e `CONTINUE` come le altre); `operator_output_invariants_hold` richiede `measure`; tabella delle operazioni con le righe di §b.14. `max_file_bytes` non cambia: alpha non va in FITS e le keyword nuove sono al più 15 card, entro `max_header_cards`.

### b.16 Errori aggiunti

| Condizione | Categoria | Codice |
|---|---|---|
| Alpha di geometria diversa dall'immagine; valore non finito o fuori da `[0, 1]` | `invalid_input` | `alpha.geometry`; `alpha.range` |
| Regola `coverage` senza coverage | `incompatible` | `alpha.coverage_missing` |
| Regola `edit_mask` senza maschera, o maschera con un'altra regola | `invalid_input` | `alpha.mask_argument` |
| `measure` con codici derivati, duplicati o fuori ordine | `invalid_input` | `image.measure` |
| `ASTRMCAP = F`, `ASTRMLnn` senza `ASTRMCAP`, numerazione con buchi, ordine non canonico, duplicati | `invalid_input` | `fits.measure_keywords` |
| Codice derivato o ignoto in `ASTRMLnn` | `unsupported` | `fits.measure_code` |
| Limitazione non accettata dalla misura | `incompatible` | `measure.limit_blocked` |

### b.17 Test pianificati

Criteri fissati qui, prima di ogni misura; oracle scritti a mano nel test.

| ID | Verifica | Criterio |
|---|---|---|
| T-M3A-TYP-13 | `static_assert` di non convertibilità e non costruibilità fra `alpha_plane` e ciascuno dei quattro tipi di §b.7, in entrambe le direzioni; `validate_alpha` con valori 0, 1, `−0`, `nextafter(1, 2)`, `nextafter(0, −1)`, `NaN`, ±Inf e con larghezza o altezza diverse | Compilazione; confini e `−0` accettati, gli altri `alpha.range`; `alpha.geometry` |
| T-M3A-TYP-14 | `derive_alpha` su una griglia 3×3 a 2 piani, per ogni regola, contro una tabella scritta a mano: pixel invalido su un solo piano; coverage diversa fra i piani; coverage f64 `0.1` (atteso il float più vicino, `13421773 · 2^-27`); maschera con 0 su un pixel valido. Immagine e maschera prima e dopo: campioni, `invalid`, `coverage`, `variance` e maschera bit-identici. Prerequisiti mancanti, uno alla volta | Esatta: 0 differenze bit a bit; `alpha.coverage_missing`, `alpha.mask_argument`, `mask.edit_geometry`, nessun piano allocato (contatore del pool) |
| T-M3A-TYP-15 | `measure`: `validate_image` con codice derivato, duplicato, fuori ordine; FITS con 0, 1 e 14 codici memorizzati scritto e riletto; ogni caso di lettura di §b.16 uno alla volta; `ASTRMCAP` stringa; `equivalent_content` su una coppia che differisce solo per un codice e su una con `measure` presente contro assente | `image.measure`; rilettura identica; `fits.measure_keywords`, `fits.measure_code`, `fits.keyword_value`; `false` per entrambe le coppie |
| T-M3A-TYP-16 | `history_limits` con `measure` assente e fase `raw`, `calibrated`, `prestacked`, assente; `measure_limits` su un'immagine di base senza codici (`lin = linear`, `variance` presente, `variance_scope = independent`, `measure` vuota) con una condizione derivata accesa alla volta, e sulle combinazioni costruibili {`lin` assente, `variance` assente} → `linearity.unknown`, `variance.absent`; {`nonlinear`, `variance` senza `variance_scope`} → `linearity.nonlinear`, `variance.scope_unknown`; {`nonlinear`, `marginal`, `measure` con `samples.interpolated`} → `linearity.nonlinear`, `variance.marginal`, `samples.interpolated`, nell'ordine canonico; `merge_measure` con due ingressi a codici sovrapposti e un codice proprio; `assess_measure` con accettazione vuota, completa e parziale; `require_measure_ready` con `blocking` non vuoto, e con `blocking` vuoto sia sull'immagine di base sia con limitazioni tutte accettate; `operator_output_invariants_hold` su uscite altrimenti valide: senza `measure`; con `demosaic` senza `samples.interpolated`; con `variance_propagation = fixed_weight_approximation`, `samples.interpolated` presente e `variance.fixed_weight_approximation` assente; con `fixed_weight_approximation` ed entrambi i codici; con `linear_weights` e il solo `samples.interpolated` | Esatta contro la tabella; ordine canonico; `measure.limit_blocked` con i codici nel contesto; con `blocking` vuoto nessuno stop; invariante falso nei primi tre casi, vero negli ultimi due |
| T-M3A-TYP-17 | Uscita della calibrazione `.b` per la ricetta di default e per una esperta di ogni riga di ADR-16.b §b.6 (le configurazioni di T-M3A-CAL-04), più `D'`, `flat.bias` e un light RAW: `measure.limits` contro l'oracle ricavato dal ledger e dalla ricetta; ricetta `.a`: nessun codice proprio; un ingresso `calibrated` senza `measure`. Per la ricetta `.a` e per una `.b` con codici propri: light e master con `measure` che porta codici memorizzati distinti (per esempio `history.unknown` sul light, `samples.interpolated` sul flat) e sovrapposti (lo stesso codice su due ingressi, e un codice uguale a uno proprio della ricetta) | Esatta, codice per codice; `history.unknown` presente nel caso dell'ingresso senza `measure`; codici degli ingressi conservati, uniti ai propri, senza duplicati e in ordine canonico |
| T-M3A-TYP-18 | Le tre righe di §b.14 × fase (`raw`, `calibrated`, `prestacked`) × linearità (`linear`, `nonlinear`, `unknown`); nell'analisi un master fuori riga per ciascun ruolo e un light `calibrated`; T-M2R1-05 rieseguito sulle sei righe di R1 | Ammesse solo le celle della tabella, fase prima della linearità; master: `excluded` con motivo `phase` o `linearity`, nessuno stop; light: `op.phase_blocked`; T-M2R1-05 invariato |

### b.18 Limiti dichiarati

- Alpha esiste solo come derivato per l'export: la lettura di un TIFF con alpha non è coperta. La specifica TIFF 6.0 non è riletta; vale il fatto dell'header di libtiff.
- La regola `coverage` usa il minimo fra i piani: è una regola dichiarata, non una misura di qualità.
- I codici di limitazione coprono solo ciò che i contratti di M3 definiscono. Il «master di qualità dubbia» di §9.8 non ha un codice: ADR-16.b classifica i master solo come `usable` o `excluded`.
- `history.unknown` si basa sulla fase dichiarata: un file dichiarato `raw` ma elaborato fuori dal prodotto non si riconosce.
- Gli insiemi accettati dalle misure appartengono ai contratti delle misure, non ancora scritti.
- I riferimenti ad ADR-16.b valgono per la versione indicata in §b.12.
