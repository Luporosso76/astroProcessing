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
