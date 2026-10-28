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

## Fuori da M2

Tile e piramide; alpha; coverage e capability; maschere di editing usate dagli operatori; XISF e RAW (ADR-08.b, M3.a); unità `electron` prodotte da conversione di gain.
