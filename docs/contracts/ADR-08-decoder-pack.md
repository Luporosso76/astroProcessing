# Contratto ADR-08.b: decoder RAW e catalog pack

Stadio: ADR-08.b, RAW e cataloghi (registro riga 3404; aggregato ADR-08 riga 3402, testo riga 2569). Archi: ADR-08.b `requires_all` ADR-08.a e NFR-LIC-01.d (righe 4088, 4089); F-IO-02 e C-10.b `decided_by` ADR-08.b (righe 3685, 4125); C-18 `decided_by` ADR-08.b (riga 4135); G-PACK `requires_all` NFR-LIC-01.d (riga 4373). Piano: §9.6 (contratto del decoder), §17.4 (pack, D-21), §10.19 (C-18). Stadio `.a` (FITS, M2): lettore e scrittore di [ADR-03](ADR-03-pixel-maschere.md), CFITSIO solo da memoria ([ADR-13](ADR-13-offline.md)). Errori: [modello comune](ADR-05-dag-provenance.md#modello-derrore-comune). Licenze: `docs/licenses.md`, sezione «NFR-LIC-01.d». Scelta dei cataloghi: `docs/datasets/M3-candidates.md`, «Decisioni del coordinatore». Prima slice che lo usa: M3.a (decoder, G-PACK su pack sintetico); poi M3.p (solver), M3.q (colore), M3.s (writer TIFF).

Ogni clausola è di uno di tre tipi: **regola** (vincolo del prodotto, verificato dal test indicato), **fatto** (letto o misurato in questa unità, con la fonte) o **ipotesi** (comportamento della libreria non ancora provato; il test indicato la conferma o la smentisce prima del codice che ci si appoggia). Un'ipotesi smentita blocca la clausola che ne dipende e riapre il contratto con review; non diventa un ripiego.

Emendamento dopo i test di U2 (30 ottobre 2026): §1, §2.2, §2.3, §2.4, §2.6, §6, §7. I fatti marcati «sonda» sono misurati con LibRaw 0.21.5 di sistema (`libraw_r.so.23`, SHA-256 `3484c3eecb3614c845cadd4c4de59ff124c1646317e34438e98394e2edab3010`) da un programma usa-e-getta sui DNG sintetici del generatore dei test e sugli 8 file di PUB-RAW-01; comando, log e numeri nel record `docs/evidence/M3a-ADR08-U2.md`.

Fuori da ADR-08.b: XISF e TIFF in ingresso (F-IO-06, P1, M6); debayer (M3.d); uso dell'offset nella calibrazione (C-01.b, G-OFFSET); formato del TIFF in uscita e profilo ICC (C-10.b, ADR-10); algoritmo del solver e del colore (C-18, C-16); distribuzione dei pack (M5, ADR-12).

## 1. Librerie

| Libreria | Uso ammesso | Uso vietato |
|---|---|---|
| LibRaw 0.21.5 (`libraw_r`, variante thread-safe), LGPL-2.1-or-later scelta fra LGPL e CDDL-1.0 | `LibRaw::open_buffer` su un buffer già letto dal prodotto; `unpack`; lettura di `rawdata.raw_image` e dei metadati pubblici di `imgdata`; `is_floating_point`, `is_fuji_rotated`; scrittura di `rawparams.options` prima di `open_buffer` (§2.2); `set_progress_handler`, `set_dataerror_handler`; `version` | Apertura per nome o per stream (`open_file`, `libraw_open_file`, `libraw_open_wfile`, `open_datastream` con datastream su file); `raw2image`, `raw2image_ex`, `subtract_black`, `adjust_bl`, `dcraw_process` e ogni elaborazione successiva (nero, scaling, bilanciamento, demosaic, profili); lettura di membri protetti o interni (`tiff_ifd`, `libraw_internal_data`, `get_internal_data_pointer`) |
| libtiff 4.7.0, Hylafax | Scrittura in memoria con `TIFFClientOpen` e procedure di I/O su buffer: writer TIFF di M3.s (formato in C-10.b) e scrittura delle fixture DNG sintetiche nei test | `TIFFOpen`, `TIFFOpenW`, `TIFFOpenExt`, `TIFFFdOpen` e ogni apertura per nome o descrittore; lettura di TIFF in ingresso nel prodotto (P1) |

Fatti (questa unità, 30 ottobre 2026): pacchetti `libraw23t64` 0.21.5b-1ubuntu1.1 e `libtiff6` 4.7.0-3ubuntu5, `pkg-config --modversion libraw_r libtiff-4` = `0.21.5`, `4.7.0`; `LIBRAW_MAJOR/MINOR/PATCH_VERSION` = 0, 21, 5 in `/usr/include/libraw/libraw_version.h`. `libraw_capabilities()` della libreria installata restituisce `0x80` (solo `LIBRAW_CAPS_JPEG`): la libreria dichiara di essere costruita senza RawSpeed, senza DNG SDK e senza zlib (`tmp/adr08b/caps.py`, ctypes). Ipotesi H0: `RawSpeed/rawspeed_xmldata.cpp` (CC-BY-SA-3.0 nel copyright Debian) non è nel binario; la sostengono la capacità RawSpeed assente, l'assenza del simbolo `_rawspeed_data_xml` (`nm -D --defined-only`) e della stringa `<Camera` (`strings`), ma il log di build del pacchetto non è stato letto. Limite dichiarato: se H0 fosse falsa, il binario conterrebbe dati CC-BY-SA-3.0 e l'audit di LibRaw andrebbe rifatto prima della distribuzione (M5). Conseguenza della capacità zlib assente: un DNG con compressione deflate non è decodificabile da questa build (atteso `raw.file_unsupported` o `raw.data_error`, da registrare in M3.a). Hash, licenze e dipendenze transitive in `docs/licenses.md`.

Il binario `libraw_r.so.23` dichiara `NEEDED libgomp.so.1` e `libraw_r.pc` porta `-fopenmp`: LibRaw può aprire regioni parallele OpenMP durante la decodifica (§2.7).

## 2. Contratto del decoder RAW (§9.6)

### 2.1 Ingresso

- Il prodotto legge l'originale con il protocollo di ADR-03 (`open` in sola lettura, `fstat` sullo stesso descrittore, `pread`), calcola lo SHA-256 sui byte letti (identità dell'input, ADR-05) e passa a `open_buffer` lo stesso buffer, che resta valido e immutato fino alla distruzione dell'oggetto LibRaw. Regola; test T-M3A-RAW-07.
- Limiti (NFR-SEC-01), costanti in `astro::raw_limits`, fissati prima di ogni misura:

| Limite | Valore | Controllo | Esito oltre il limite |
|---|---|---|---|
| `max_file_bytes` | 1 073 741 824 (1 GiB) | `fstat`, prima della lettura | `limit_exceeded`, `raw.max_file_bytes`, nessuna allocazione |
| `max_pixels` | 268 435 456 (2^28) su `raw_width · raw_height` | dopo `open_buffer`, prima di `unpack` | `limit_exceeded`, `raw.max_pixels`, nessun `unpack` |

  Motivazione dei valori, stima non misurata: un sensore da 150 Mpx a 16 bit occupa circa 300 MB non compresso, sotto entrambi i limiti con margine; il limite superiore protegge dalle dimensioni dichiarate da un file malformato. `rawparams.max_raw_memory_mb` si imposta alla parte della prenotazione destinata a `raw_image` (§2.8), arrotondata per difetto al MiB.
- Un oggetto LibRaw per decodifica, allocato sullo heap, mai condiviso fra thread.

### 2.2 Che cosa il decoder non fa

Regola: dopo `unpack` il prodotto legge `rawdata.raw_image` (campioni `uint16` della CFA) e non chiama alcuna funzione che modifichi i campioni. Parametri dell'utente lasciati al valore iniziale e verificati prima di `open_buffer`: `user_black = -1`, `user_cblack[0..3] = -1`, `user_sat = -1`; un valore diverso è un difetto (`ASTRO_ASSERT`).

Regola: prima di `open_buffer` il prodotto toglie il bit `LIBRAW_RAWOPTIONS_CONVERTFLOAT_TO_INT` da `rawparams.options`. Fatto (sonda): il valore iniziale di `rawparams.options` è `0x2`, cioè il bit è attivo; con il bit attivo `unpack` di un DNG in virgola mobile a 16, 24 o 32 bit riesce, `float_image` resta nullo, `raw_image` contiene interi convertiti da LibRaw e `color.maximum` passa dal `WhiteLevel` scritto (4000) a 16383: una conversione silenziosa. Con il bit tolto prima di `open_buffer` `float_image` è valorizzato. Toglierlo dopo `open_buffer` e prima di `unpack` ha dato lo stesso esito; la regola fissa comunque il momento prima di `open_buffer`, insieme agli altri parametri. Lo stop sui dati in virgola mobile è in §2.3.

Ipotesi H1, provata sui DNG sintetici: per i formati Bayer, `raw_image` dopo `unpack` contiene i valori memorizzati nel file, senza sottrazione del nero né scaling, salvo l'eventuale curva di linearizzazione del formato (§2.4). Misura (sonda): i campioni dell'area visibile sono uguali, bit per bit, ai valori scritti con `ActiveArea` di origine (0,0), (1,0), (0,1), (1,1), (3,0), (2,2) e nero per posizione scritto; stesso esito nei test di U2 eseguiti contro una realizzazione usa-e-getta. Test T-M3A-RAW-02. Per i formati di marca resta ipotesi (§7).

### 2.3 Geometria, crop e pattern CFA

- L'immagine importata è l'area visibile di LibRaw: origine `(left_margin, top_margin)` nel buffer grezzo, dimensioni `width × height`. Il crop rispetto al buffer grezzo (`raw_width × raw_height`) è dichiarato nella geometria dell'immagine e nella provenance come `{x0, y0, w, h}`. Le righe e colonne mascherate (nero ottico) non si importano: limite dichiarato, nessuna stima del nero dai pixel mascherati. `raw_inset_crops` (crop di default del formato) si registra come metadato e non si applica. `sizes.flip` si registra come orientamento e non si applica.
- `ActiveArea` di un DNG con origine dispari. Fatto (sonda): su un asse con origine dispari LibRaw 0.21.5 porta il margine al pari successivo e riduce di 1 la dimensione, lasciando fermo il bordo opposto: area larga 40 con origine x = 1 → `left_margin` 2, `width` 39; origine x = 3 → `left_margin` 4, `width` 39; l'asse y si comporta allo stesso modo; origini 0 e 2 restano invariate. Per i DNG i margini dopo `open_buffer` sono quindi pari (origini misurate, (x, y): (0,0), (1,0), (0,1), (1,1), (3,0), (2,2)) e un margine nullo viene solo da un'origine nulla. L'origine scritta nel file resta solo in un membro protetto di LibRaw (`tiff_ifd[].t_lm`, `t_tm`), che il prodotto non legge (§1). Regola: anche in questo caso l'immagine importata è l'area visibile di LibRaw; campioni e pattern dell'area importata sono quelli del file (H1, H2); la riga o colonna iniziale dell'area attiva esclusa da LibRaw non si importa, come le righe mascherate, e il crop `{x0, y0, w, h}` dichiarato è quello effettivo nel buffer grezzo. Motivazione della scelta: nessun valore importato differisce dal file, quindi non serve uno stop per la geometria; uno stop non sarebbe comunque realizzabile, perché un margine pari maggiore di zero non distingue un'origine pari da una dispari arrotondata; importare l'area del file richiederebbe membri interni di LibRaw o un secondo parser TIFF nel prodotto, esclusi da questo contratto (una unità successiva può proporli con review). L'esclusione non è osservabile dal prodotto: limite dichiarato (§7). Il solo valore che l'arrotondamento rende errato è il nero per posizione, trattato in §2.4.
- Pattern: solo CFA Bayer 2×2 con `colors = 3`. Il prodotto ricava il pattern della cella 2×2 all'origine dell'area visibile da `idata.filters` con la convenzione di LibRaw `FC(r, c) = (filters >> (((r << 1 & 14) | (c & 1)) << 1)) & 3` (fatto: `LibRaw::FC`, `/usr/include/libraw/libraw.h` riga 304), in coordinate dell'area visibile, e lo traduce in `cfa_layout` di ADR-03 (`pattern` fra `rggb`, `bggr`, `grbg`, `gbrg`, offset `(0, 0)`). La cella deve contenere un rosso (indice 0), un blu (indice 2) e due verdi (indice 1 o 3); `filters` deve ripetere la stessa cella 2×2 su tutte le otto righe codificate. La provenance registra anche la parità `(left_margin mod 2, top_margin mod 2)`, così il passaggio dal buffer grezzo all'area visibile è dichiarato; per i DNG vale sempre `(0, 0)` (punto precedente).
- Ipotesi H2, provata sui DNG sintetici: `FC` con coordinate dell'area visibile dà il colore del campione `raw_image[(top_margin + r) · raw_pitch/2 + left_margin + c]`. Riferimento: nel DNG l'origine del motivo CFA è l'angolo in alto a sinistra di `ActiveArea` (Adobe DNG Specification 1.6.0.0, p. 21, PhotometricInterpretation: «The origin of the repeating CFA pattern is the top-left corner of the ActiveArea rectangle»). Misura (sonda): con il motivo ancorato così, il colore dato da `FC` coincide con quello scritto in ogni sito visibile per origini (0,0), (1,0), (0,1), (1,1), (3,0), (2,2): dopo l'arrotondamento del margine LibRaw riallinea `filters` alla nuova origine. Test T-M3A-RAW-01 con i quattro pattern e `ActiveArea` di DNG con origine pari e dispari.

Stop prima di produrre l'immagine (`unsupported`; nessun ripiego su un'altra interpretazione):

| Condizione | Codice |
|---|---|
| `filters = 9` (X-Trans) | `raw.xtrans` |
| `filters = 0`, `raw_image` nullo dopo `unpack` (dati a 3 o 4 canali, `color3_image`, `color4_image`) | `raw.not_cfa` |
| `is_floating_point() ≠ 0` dopo `open_buffer`, prima di `unpack`; `float_image` non nullo dopo `unpack` (DNG in virgola mobile) | `raw.float_data` |
| `is_foveon ≠ 0`, `colors ≠ 3`, cella 2×2 non conforme o `filters` non periodico 2×2 | `raw.cfa_pattern` |
| `idata.raw_count > 1` (file con più immagini) | `raw.multi_image` (nessuna scelta implicita dell'immagine) |
| `sizes.pixel_aspect ≠ 1` o `is_fuji_rotated() ≠ 0` (sensore ruotato) | `raw.geometry` |

Regola: le condizioni della tabella si valutano prima di ogni lettura di `raw_image`. Fatti:

- Virgola mobile (sonda): `is_floating_point()` vale 1 dopo `open_buffer` per i DNG in virgola mobile a 16, 24 e 32 bit, con il bit di conversione di §2.2 attivo o tolto. Con il bit tolto `raw_image` dopo `unpack` non è nullo e contiene valori non definiti (diversi fra due esecuzioni sullo stesso file: 12648, 27041, 32757 e 12648, 14017, 30315 per i primi tre campioni del DNG a 32 bit; registrati nel record): leggerlo prima dello stop darebbe un'immagine priva di significato.
- Sensore ruotato (header): `fuji_width` non è un campo di `sizes`; è il campo di `libraw_internal_output_params_t` (`libraw_types.h` righe 136-143) nella parte interna `libraw_internal_data.internal_output_params`, restituito dal metodo pubblico `is_fuji_rotated()` (`libraw.h` righe 268-271); la copia pubblica `rawdata.ioparams.fuji_width` è valorizzata solo da `unpack`. Il prodotto chiama `is_fuji_rotated()` dopo `open_buffer`. Valore misurato (sonda): 0 sul DNG sintetico e sugli 8 file di PUB-RAW-01; il caso non nullo si può provare solo per iniezione (T-M3A-RAW-03, pianificato).

### 2.4 Livelli: nero, bianco, linearizzazione

- Nero dichiarato con origine `decoder`, nella rappresentazione di LibRaw dopo `unpack`: `color.black` (scalare), `color.cblack[0..3]` (per indice di colore), `color.cblack[4]`, `color.cblack[5]` (dimensioni del motivo spaziale) e `color.cblack[6…]` (motivo). Il decoder non lo sottrae: `black_applied_by_decoder = none`. Fatto (sonda): per un DNG con `BlackLevelRepeatDim` 2×2 LibRaw mette i quattro valori nel motivo (`cblack[4] = cblack[5] = 2`, `cblack[6…9]` nell'ordine del file) e lascia `cblack[0..3] = 0`; `cblack[0..3]` non nulli si sono visti solo su un formato di marca (Pentax K-x di PUB-RAW-01: 1, 10, 0, 10).
- Ipotesi H3, riformulata dopo la misura: il nero del campione visibile `(r, c)` è `black + cblack[FC(r, c)] + P(r, c)`, con `P(r, c) = cblack[6 + (r mod cblack[4]) · cblack[5] + (c mod cblack[5])]` se `cblack[4] · cblack[5] > 0`, altrimenti 0. Riferimento: nel DNG l'origine del motivo di `BlackLevel` è l'angolo in alto a sinistra di `ActiveArea` (DNG 1.6.0.0, p. 26, BlackLevel: «The origin of this pattern is the top-left corner of the ActiveArea rectangle»). Misura (sonda; test di U2 contro la realizzazione usa-e-getta): la formula è esatta con nero scalare, con motivo 2×2 per le origini (0,0) e (2,2) e, con origine dispari, quando il motivo è invariante sull'asse spostato (64, 64, 80, 80 con origine (1,0); 64, 72, 64, 72 con origine (0,1); uniforme con origine (1,1)); è smentita con motivo 2×2 non uniforme e origine dispari su un asse, in ogni sito visibile (1 248 su 1 248 per (1,0), 1 240 su 1 240 per (0,1), 1 209 su 1 209 per (1,1), 1 248 su 1 248 per (3,0)). Causa: LibRaw arrotonda il margine (§2.3) e riallinea `filters`, ma non il motivo `cblack[6…]`, che resta ancorato all'origine del file mentre la formula lo legge dall'origine arrotondata. H3 vale quindi solo dove lo spostamento non cambia il valore; la regola seguente ferma gli altri casi.
- Regola (stop): per un DNG (`idata.dng_version ≠ 0`) con `R = cblack[4]`, `C = cblack[5]` e `R · C > 0`, il prodotto si ferma prima di produrre l'immagine con `unsupported`, `raw.black_pattern_anchor` se su un asse con margine maggiore di zero il motivo non è invariante per uno spostamento di un passo su quell'asse: `left_margin > 0` ed esiste `(i, j)` con `cblack[6 + i·C + j] ≠ cblack[6 + i·C + ((j + 1) mod C)]`, oppure `top_margin > 0` ed esiste `(i, j)` con `cblack[6 + i·C + j] ≠ cblack[6 + ((i + 1) mod R)·C + j]`. Motivazione: correggere l'ancoraggio richiede l'origine scritta nel file, che l'API pubblica non espone (§2.3); un margine pari maggiore di zero può venire da un'origine dispari, quindi lo stop copre anche le origini pari maggiori di zero, dove la formula sarebbe esatta (per esempio (2,2)): limite dichiarato, l'import dei DNG con motivo di nero non invariante sull'asse con margine maggiore di zero non è supportato. Con margine nullo su un asse non c'è stato arrotondamento su quell'asse (§2.3); con motivo invariante sull'asse spostato lo spostamento non cambia il nero: in entrambi i casi H3 vale (misura sopra). Formati diversi dal DNG: nessuna verità indipendente sull'ancoraggio del motivo; la formula è la convenzione di LibRaw (§7).
- Bianco dichiarato con origine `decoder`: `color.maximum`; si registrano anche `color.data_maximum` e `color.linear_max[0..3]` quando non nulli. Non si applica `adjust_maximum` (appartiene alle funzioni vietate in §1). Fatti (sonda, dopo `unpack`): `data_maximum` vale 0 sul DNG sintetico e sugli 8 file di PUB-RAW-01; un valore non nullo non è stato osservato e si può provare solo per iniezione (T-M3A-RAW-02, pianificato). `linear_max` vale 0 sul DNG sintetico e su Fujifilm X-A1, X-E1 e Pentax K-x, ed è non nullo, uguale sui quattro canali, su Canon EOS 6D (15070), Nikon D750 (3827), Sony ILCE-7S (15360), Olympus E-M10 (4093) e Panasonic DMC-GH4 (4079).
- Curva: dopo `unpack` il prodotto confronta `color.curve[0..65535]` con l'identità su tutte le 65 536 voci e registra `linearization ∈ {identity, decoder_curve}` e, per `decoder_curve`, il digest della curva. Regola (byte del digest): SHA-256 di 131 072 byte, le voci `curve[0]`, …, `curve[65535]` in ordine di indice, ciascuna come intero senza segno a 16 bit little-endian, indipendentemente dall'ordine dei byte della macchina.
- Fatto (DNG 1.6.0.0, p. 84, «Linearization»: «If the raw value is greater than the size of the table, it is mapped to the last entry of the table»; sonda): con una `LinearizationTable` di N < 65 536 voci LibRaw estende la curva con l'ultima voce (`curve[i] = tabella[N − 1]` per `i ≥ N`; 0 differenze su 65 536 voci per N = 4096) e un campione scritto oltre la tabella si legge uguale all'ultima voce (5000 → 4095 con la tabella identica di 4096 voci). Conseguenza: una tabella identica di N < 65 536 voci dà `decoder_curve`, perché la curva non è l'identità sopra N − 1 e cambia i campioni oltre la tabella; danno `identity` solo una tabella identica di 65 536 voci o l'assenza della tabella. Il digest atteso dai test si calcola dalla tabella scritta, estesa con l'ultima voce e serializzata come sopra; la sonda lo trova uguale al digest di `color.curve` (tabella identica di 4096 voci: `2fe87d203699ad6efe1a4da3c45691a072f2d91b8fabea89bd0bfcb61fbf0264`; tabella `3i + 5` di 4096 voci: `a3089d2ad8be7b57af4ad35ff19bd2aa94fca3ac23a0f676cff7f47a826b6662`).
- Ipotesi H4, provata sui DNG sintetici: con una `LinearizationTable` DNG non identica, `raw_image` contiene i valori già mappati dalla tabella e `color.curve` la riporta (sonda; test di U2 contro la realizzazione usa-e-getta). Test T-M3A-RAW-04.
- Il nero e il bianco possono venire dal file o dalle tabelle per modello di LibRaw: l'API non distingue le due origini. Limite dichiarato; la provenance registra la versione di LibRaw, che determina le tabelle.
- `BlackLevelDeltaH` e `BlackLevelDeltaV` del DNG non sono coperti da test: il nero dichiarato potrebbe non includerli. Limite dichiarato.

### 2.5 Avvisi ed errori di LibRaw

- `set_dataerror_handler`: il gestore del prodotto registra file e offset dell'errore; se è stato chiamato almeno una volta, la decodifica si ferma dopo il ritorno di `unpack` con `integrity`, `raw.data_error`, anche se `unpack` ha restituito `LIBRAW_SUCCESS`: un errore di dati segnalato dalla libreria non diventa mai un'immagine accettata, indipendentemente da come LibRaw prosegue dopo la segnalazione.
- `set_progress_handler`: il gestore controlla il token di cancel di ADR-06; un cancel restituisce un valore non nullo, LibRaw ritorna `LIBRAW_CANCELLED_BY_CALLBACK` e il prodotto si ferma con `cancelled`, `raw.cancelled` (uscita 3).
- `imgdata.process_warnings` dopo `unpack`. Registrati in provenance e ammessi, perché non toccano i campioni grezzi: `BAD_CAMERA_WB`, `NO_METADATA`, `NO_EMBEDDED_PROFILE`, `NO_INPUT_PROFILE`, `BAD_OUTPUT_PROFILE`, `NO_BADPIXELMAP`, `BAD_DARKFRAME_FILE`, `BAD_DARKFRAME_DIM`. Ogni altro bit (`NO_JPEGLIB`, `RAWSPEED*`, `PARSEFUJI_PROCESSED`, `DNGSDK_PROCESSED`, `DNG_IMAGES_REORDERED`, `DNG_STAGE2_APPLIED`, `DNG_STAGE3_APPLIED`, `FALLBACK_TO_AHD`, `NO_JASPER`, bit sconosciuti): `unsupported`, `raw.warning`, con il nome nel contesto.
- Codici di ritorno di `open_buffer` e `unpack`:

| Codice LibRaw | Categoria | Codice |
|---|---|---|
| `LIBRAW_FILE_UNSUPPORTED` | `unsupported` | `raw.file_unsupported` |
| `LIBRAW_DATA_ERROR`, `LIBRAW_IO_ERROR`, `LIBRAW_BAD_CROP` | `integrity` | `raw.data_error` |
| `LIBRAW_TOO_BIG`, `LIBRAW_UNSUFFICIENT_MEMORY`, `LIBRAW_MEMPOOL_OVERFLOW` | `limit_exceeded` | `raw.too_big` |
| `LIBRAW_CANCELLED_BY_CALLBACK` | `cancelled` | `raw.cancelled` |
| ogni altro valore non nullo | `invalid_input` | `raw.libraw_error` |

Per ogni riga, anche quando più codici di LibRaw confluiscono nello stesso codice del prodotto, il contesto dello stop contiene la funzione (`open_buffer` o `unpack`), il valore numerico restituito da LibRaw e il testo di `libraw_strerror`; per `raw.data_error` da gestore anche file e offset riportati dal gestore.

Ipotesi H5: LibRaw cattura le proprie eccezioni interne e le converte nei codici sopra. Un crash di LibRaw dentro il processo (segnale) non è uno stop controllato; T-M3A-RAW-10 ne misura l'occorrenza. Un crash trovato su un input raggiungibile blocca il gate di M3.a; la correzione è una nuova unità (per esempio decodifica in un processo separato), mai un ripiego a runtime.

### 2.6 Immagine e metadati prodotti

- Campioni f32 in `adu`, uguali al valore `uint16` letto (esatto: ogni intero ≤ 65535 < 2^24 è rappresentabile in f32), layout `cfa` di ADR-03, nessuna maschera di invalidità impostata dal decoder, nessuna varianza.
- `process_phase = raw`, origine `decoder`. Linearità `linear`, origine `decoder`: ipotesi dichiarata e registrata in provenance (dati del sensore, dopo l'eventuale curva del formato, lineari fino al bianco); la non linearità vicino alla saturazione non è misurata. Il valore di origine `decoder` è un'estensione di `metadata_source` di ADR-03 che ADR-03.b deve accogliere (dipendenza fra contratti).
- Metadati interpretati, ognuno `std::optional` con origine `decoder`: `exposure_s` (`other.shutter`), `gain` come ISO (`other.iso_speed`), ora di acquisizione (`other.timestamp`), `camera` (`idata.make`, `idata.model`), `temperature_c` (`makernotes.common.SensorTemperature`, regola sotto), `saturation_adu` (= bianco). Un campo che LibRaw non valorizza resta assente: nessun valore di comodo.
- Temperatura. Fatto (header): `SensorTemperature` è un campo di `libraw_metadata_common_t` (`libraw_types.h` riga 875), membro `common` di `libraw_makernotes_t` (riga 1052), cioè `imgdata.makernotes.common.SensorTemperature`; `other` non ha campi di temperatura. Fatto (sonda): prima di `open_buffer` vale 0; dopo `unpack` vale −1000 quando il file non porta il dato (DNG sintetico e 7 degli 8 file di PUB-RAW-01) e 32 su Olympus E-M10. Regola: `temperature_c` è assente se e solo se il valore letto dopo `unpack` è −1000; altrimenti è quel valore. Le altre temperature di `common` (`CameraTemperature`, `SensorTemperature2` e le altre) non si leggono e non sostituiscono mai `SensorTemperature`: su Canon EOS 6D `CameraTemperature` vale 22 con `SensorTemperature` −1000, e `temperature_c` resta assente. Ipotesi: −1000 è il solo valore con cui LibRaw 0.21.5 segnala il dato assente (misurato su 9 file, sorgente non letta).
- Metadati non conservati: makernote, miniature, profili, bilanciamento del bianco della camera. Si recuperano dall'originale, immutabile e identificato dal suo SHA-256 (ADR-04, ADR-05).
- Metadati di F-IO-04 non forniti dal decoder (emendamento L4, M3.a). F-IO-04 (piano, riga 1815) chiede esposizione, ora, camera, sensore, scala pixel, orientamento e WCS «quando presente». Per i RAW esposizione, ora e camera sono i campi sopra; l'orientamento è `sizes.flip`, registrato e non applicato (§2.3). Sensore, scala pixel e WCS sono **assenti**. In questo contratto «sensore» si legge come caratteristiche del sensore diverse dalla temperatura (dimensione fisica del pixel, identità del sensore); `temperature_c` resta il campo sopra.
  - Fatto (letto in `/usr/include/libraw/libraw_types.h` di LibRaw 0.21.5, 30 ottobre 2026): nessun campo comune di LibRaw porta la dimensione fisica del pixel, un'identità del sensore o un WCS. LibRaw espone la focale dell'obiettivo (`other.focal_len`, riga 853; `FocalLengthIn35mmFormat` nelle strutture dell'obiettivo, righe 1014 e 1032), la posizione GPS dello scatto (`other.parsed_gps`, riga 857), che non è un WCS, e solo in alcune strutture di makernote per marca un campo `SensorWidth`/`SensorHeight` (Canon riga 299, Nikon riga 519, Ricoh riga 601), con significato e unità che questo contratto non verifica.
  - Regola: sensore, dimensione del pixel, scala pixel e WCS non si stimano né si deducono da focale, makernote, GPS, modello della camera o tabelle esterne; un valore di comodo è vietato come per gli altri metadati. Nel record del decoder (§2.9) e nell'export della provenance di F-IO-04 compaiono come chiavi presenti con valore JSON `null`: `sensor`, `pixel_size_um`, `pixel_scale_arcsec`, `wcs`, più `absent_reason = "not_provided_by_decoder"`. Nell'immagine nessun campo li porta: `frame_metadata` di ADR-03 non ha campi per queste grandezze (`image.hpp:147-160` al commit `311fb7a`) e il decoder non ne aggiunge. Un valore di queste grandezze può venire solo da un'altra operazione, con origine e nodo propri (per esempio il WCS risolto da C-18); il record del decoder non si aggiorna. Test T-M3A-RAW-13.

### 2.7 Thread

Regola: la decodifica gira nel worker prenotato dal pool di ADR-06; prima di `open_buffer` il prodotto chiama `omp_set_num_threads(1)` nel thread del worker (il core collega così `libgomp`, già caricata da LibRaw). Ipotesi H6: così le regioni OpenMP di LibRaw usano un solo thread. Test T-M3A-RAW-08, nel processo del test, senza `strace`: l'eseguibile di test definisce `pthread_create`, che nel collegamento dinamico prevale sulla definizione di libc; la definizione incrementa un contatore atomico e inoltra alla funzione reale ottenuta con `dlsym(RTLD_NEXT, "pthread_create")`. Due intervalli con contatori distinti, ciascuno con lettura prima e dopo: `open_buffer` e `unpack`. Il conteggio è delle creazioni, quindi include i thread creati e terminati dentro l'intervallo. H6 è provata quando, con `omp_set_num_threads(1)`, le creazioni sono 0 in entrambi gli intervalli. Controllo positivo: la stessa decodifica con due thread OpenMP (`omp_set_num_threads(2)`) deve dare almeno una creazione; altrimenti il campione non attraversa una regione parallela, il test non prova nulla e H6 resta un'ipotesi non provata. Limite dichiarato: i thread creati per altre vie (`clone` diretto) non si vedono; l'ipotesi che libgomp crei i thread con `pthread_create` la verifica il controllo positivo.

### 2.8 Memoria

Prenotazione nel pool prima di `unpack`, dai metadati letti da `open_buffer`: byte del file (già prenotati dalla lettura) + `raw_pitch · raw_height` (`raw_image`) + campioni f32 dell'area visibile (`4 · width · height`) + maschera di invalidità di ADR-03. Budget insufficiente: `budget`, `sched.job_exceeds_budget`, prima di `unpack`. Le allocazioni interne di LibRaw diverse da `raw_image` (buffer di decompressione) non sono prenotate: limite dichiarato, coerente con ADR-06 (il pool limita le prenotazioni, non l'RSS); T-M3A-RAW-06 misura il picco di RSS e lo registra, senza soglia.

### 2.9 Record del decoder

Per ogni decodifica riuscita la provenance (ADR-05) contiene, e la chiave del nodo include:

| Campo (§9.6) | Valore |
|---|---|
| Libreria | `LibRaw::version()`, pacchetto e SHA-256 di `libraw_r.so` caricata |
| Offset o nero applicato | `none`; nero dichiarato (§2.4) |
| Scala applicata | `none`; `linearization` e digest della curva |
| Crop e geometria CFA | crop `{x0, y0, w, h}`, parità dei margini, pattern risultante, `raw_inset_crops`, `flip` |
| Metadati conservati o eliminati | elenco dei campi interpretati e presenti; categorie non conservate (§2.6) |
| Metadati assenti di F-IO-04 | `sensor`, `pixel_size_um`, `pixel_scale_arcsec`, `wcs` con valore `null`; `absent_reason = "not_provided_by_decoder"` (§2.6, emendamento L4) |
| Stato del dato | `raw` (nessuna operazione del decoder sui campioni). Le operazioni fatte dal firmware della camera, per esempio un nero tagliato a zero, non sono osservabili: `camera_processing = unknown` |
| Avvisi | `process_warnings` ammessi |

Determinismo: lo stesso file con la stessa libreria produce campioni e record identici (T-M3A-RAW-12).

## 3. Catalog pack

### 3.1 Struttura

Un pack è una directory con:

- `manifest.json`: JSON canonico di ADR-05; l'identità del pack (`pack_digest`) è lo SHA-256 dei byte canonici del manifest.
- File di dati e indice elencati nel manifest, ciascuno con dimensione e SHA-256.
- `licenses/`: testo di licenza o dichiarazione di ogni componente; anche questi file stanno in `files[]`.

Nessun altro file: un file presente nella directory, diverso da `manifest.json` e non elencato in `files[]`, è un errore (§3.4). Il manifest non sta in `files[]` (dovrebbe contenere il proprio hash): la sua integrità la verifica `pack_digest`.

Campi obbligatori del manifest (`pack_format = 1`):

| Campo | Contenuto |
|---|---|
| `pack_format` | Versione dello schema; intero |
| `pack_id`, `pack_version` | Identificativo e versione del pack |
| `purpose` | Sottoinsieme di {`astrometry`, `photometry`} |
| `components[]` | Uno per catalogo sorgente: `name`; `source` (editore e URL della fonte primaria); `release` (rilascio e data); `license` (identificativo SPDX, oppure `LicenseRef-public-domain` con la dichiarazione della fonte); `license_file` (percorso in `licenses/`); `license_evidence` (record `docs/datasets/PUB-*.md`); `attribution` (testo richiesto dalla fonte, anche vuoto se non richiesto); `bands[]` (nome e sistema fotometrico); `frame` (`ICRS`) ed `epoch`; `selection` (limiti di magnitudine e flag di qualità applicati); `count` (righe) |
| `columns[]` | Nome, tipo (`f64`, `f32`, `u32`, `u16`), unità, componente di origine, `nullable` (booleano) e, se `nullable`, `null_bit` (indice di bit in `flags`, 0–31) di ogni colonna; obbligatorie `ra_deg`, `dec_deg` (f64, non nullabili) e `flags` (u32, non nullabile) |
| `flag_bits[]` | Bit di `flags` usati come flag di qualità: indice e significato. Ogni indice di `null_bit` e di `flag_bits[]` compare una sola volta; i bit non dichiarati valgono 0 in ogni riga |
| `data_file`, `index_file` | Percorsi del file dati e del file indice; entrambi elencati in `files[]` |
| `coverage` | HEALPix NESTED (Górski et al. 2005): `order` e lista ordinata delle celle. Una cella è elencata solo se il footprint dei componenti la copre interamente secondo la fonte; celle parziali escluse |
| `index_order` | Ordine HEALPix dell'indice, uguale a `coverage.order` |
| `files[]` | Percorso relativo, dimensione in byte, SHA-256 |
| `count` | Righe totali del file dati |
| `builder` | Nome e versione dello strumento che ha costruito il pack, SHA-256 degli input |

File dati: record a lunghezza fissa little-endian, senza padding, nell'ordine di `columns` (lunghezza = somma delle dimensioni dei tipi), ordinati per cella HEALPix NESTED all'ordine di `coverage`. File indice: una voce di 24 byte per cella di `coverage`, nello stesso ordine, con tre interi `u64` little-endian: cella, prima riga, numero di righe; nessuna intestazione. La fixture è il pack sintetico di T-M3A-PACK-01, scritto dal test con questo schema.

Valore assente: ammesso solo nelle colonne `nullable`, che devono essere di tipo `f32` o `f64`; le colonne intere non sono mai nullabili. Il valore è assente se e solo se il suo `null_bit` vale 1, e in quel caso il campo è `NaN`; un `NaN` con bit a 0, un valore finito con bit a 1, un `NaN` o un infinito in una colonna non nullabile, un bit non dichiarato a 1 sono riga incoerente (`integrity`, `pack.row_invalid`), verificata sulle righe che il lettore legge. Nessun valore sentinella. Limite: `flags` ha 32 bit, condivisi fra assenze e flag di qualità.

### 3.2 Selezione nella ricetta

La ricetta che richiede un pack (solver assistito con P-SOLVE, colore relativo) contiene `pack_id`, `pack_version`, `pack_digest` e un solo percorso assoluto della directory. Nessun elenco di percorsi di ricerca, nessun pack di default, nessun download (ADR-13, NFR-SEC-04). Il `pack_digest` entra nella chiave del nodo e nella provenance.

### 3.3 G-PACK

Si esegue nel preflight, prima della prima operazione che usa il pack, su ogni pack della ricetta. Tutti i controlli sono binari: nessuna soglia numerica.

1. Il percorso della ricetta è una directory; `manifest.json` esiste ed è un file regolare; il manifest è JSON canonico, `pack_format` è supportato, i campi obbligatori sono presenti e non vuoti (salvo `attribution`); `columns[]`, `flag_bits[]`, `data_file`, `index_file` e `index_order` rispettano le regole di §3.1 (tipi nullabili, bit unici, file elencati).
2. Lo SHA-256 del manifest è uguale a `pack_digest` della ricetta; `pack_id` e `pack_version` del manifest sono uguali a quelli della ricetta.
3. Ogni file di `files[]` esiste, è regolare, ha la dimensione e lo SHA-256 dichiarati; nella directory nessun file diverso da `manifest.json` e non elencato. Hash completo a ogni preflight, senza cache; costo stimato, non misurato: lineare nella dimensione del pack.
4. Ogni componente ha `license` nella lista ammessa (§4) e `license_file` presente con hash corretto; `license_evidence` nomina un record esistente in `docs/datasets/` (controllo dell'audit, T-M3A-LIC-01; nel prodotto distribuito il record entra nell'SBOM di M5).
5. Somma dei `count` dei componenti, `count` del manifest, righe del file dati (dimensione / lunghezza del record, con resto nullo) e somma delle righe dell'indice coincidono; la dimensione del file indice è 24 volte il numero di celle; l'indice copre esattamente le celle di `coverage`, in ordine, con intervalli contigui a partire dalla riga 0.
6. `purpose` contiene lo scopo dell'operazione (`astrometry` per C-18, `photometry` per C-16) e `bands` contiene le bande che la ricetta richiede.
7. Copertura: se la regione del campo è nota nel preflight (hint di C-18 con raggio, oppure WCS importato validato da G-WCS), tutte le celle HEALPix che intersecano la regione allargata del raggio angolare massimo di una cella all'ordine di `coverage` sono in `coverage`. L'inclusione è conservativa: può rifiutare un campo coperto al bordo, mai accettare un campo scoperto. Se la regione non è nota nel preflight, il controllo si ripete sulla regione stimata prima della query al catalogo, con lo stesso esito.

### 3.4 Pack mancante, corrotto o fuori copertura

Esito sempre: stop del ramo dipendente prima dell'operazione (G-STOP, P-PACK-MISSING), provenance dell'arresto con il codice; gli altri rami proseguono (ADR-06, `run_branches`); il progetto resta apribile. Nessuna ricerca di un altro pack, nessun download, nessun cambio del pack selezionato, nessuna esecuzione parziale sulla parte coperta.

| Caso | Categoria | Codice |
|---|---|---|
| Directory o manifest assenti | `metadata_missing` | `pack.missing` |
| Manifest non canonico, campo mancante o vuoto | `integrity` | `pack.manifest_invalid` |
| `pack_format` non supportato | `unsupported` | `pack.format` |
| Digest del manifest diverso dalla ricetta | `incompatible` | `pack.digest_mismatch` |
| `pack_id` o `pack_version` del manifest diversi dalla ricetta | `incompatible` | `pack.id_mismatch` |
| File elencato assente, dimensione o SHA-256 diversi | `integrity` | `pack.file_missing`, `pack.size_mismatch`, `pack.hash_mismatch` |
| File non elencato | `integrity` | `pack.unlisted_file` |
| Conteggi o indice incoerenti | `integrity` | `pack.count_mismatch`, `pack.index_invalid` |
| Licenza fuori lista, file di licenza assente, record di audit assente | `incompatible` | `pack.license_not_allowed`, `pack.license_file`, `pack.license_evidence` |
| Scopo o banda non presenti | `incompatible` | `pack.purpose`, `pack.band_missing` |
| Regione fuori copertura | `incompatible` | `pack.out_of_coverage` |
| Riga letta incoerente | `integrity` | `pack.row_invalid` |

**Prova in M3.a (emendamento L6).**

- Fatti, letti al commit `311fb7a`. In M3.a nessun operatore del prodotto usa un pack: i consumatori sono C-18 e C-16, nelle slice M3.p e M3.q (intestazione di questo contratto). `run_branches(sqlite_dag_store&, std::vector<branch_files_job>, …)` (`scheduler.hpp:179`) esegue per ogni ramo un `run_job_files` con il `work` passato dal chiamante e ferma solo i discendenti del ramo fallito (`blocked`), mentre i rami indipendenti proseguono (`scheduler.hpp:169-178`; test `T_M2B_09_…` in `test_scheduler.cpp:977` sullo store in memoria e `P7_branches_on_a_durable_store_…` in `test_store_review.cpp:837` sullo store SQLite). `tools/check_job_types.py` confronta il registro dei tipi solo con le costanti `*_operator_id` di `src/core/include/astro/*.hpp` e con le funzioni di `src/core/*.cpp` che chiamano `run_job_files` (righe 7-9).
- Regola per M3.a. T-M3A-PACK-02 prova «stop del solo ramo dipendente» con un job del test: un `branch_files_job` scritto nel test, il cui `work` chiama come primo passo la funzione di G-PACK del prodotto sul pack della ricetta e solo dopo un'operazione fittizia che conta le proprie chiamate. Il DAG del test ha un ramo con il job del test, un suo discendente e un ramo indipendente, composti da `run_branches` sullo store SQLite. Atteso: ramo del pack `failed` con `metadata_missing` `pack.missing` e provenance del suo nodo persistita (OB-02 di [BASE](BASE-confini.md)); discendente `blocked` con `blocked_by_slot` uguale al ramo del pack; ramo indipendente `completed`; contatore dell'operazione a zero; nessun `openat` fuori dal percorso della ricetta durante la chiamata di G-PACK (osservatore attivo solo attorno a quella chiamata: le aperture dello store fatte dal runner, per esempio in `artifacts/`, sono fuori dall'osservazione); store riaperto dopo l'esecuzione senza errori.
- Il job del test non è un tipo di job del prodotto: nessuna voce in `registered_job_types()`, nessuna costante `*_operator_id` in `src/core/include/astro/`, nessun codice in `src/core/` (BASE §3.1, punto 3). Il suo `operator_id` è una stringa del test, diversa da ogni costante del prodotto. La suite di conformità T-M3BASE non lo copre, perché copre solo i tipi registrati.
- Limiti: M3.a non prova che l'operatore del prodotto chiami G-PACK nel proprio preflight prima della prima operazione che usa il pack (§3.3), né se il suo arresto si registri prima del lavoro (OB-01) o nel lavoro (OB-02): nel job del test lo stop nasce dentro `work`, quindi la provenance è quella di OB-02. Il discendente `blocked` non ha provenance propria (BASE OB-03, limite). Per T-FBK-01 la parte M3.a prova solo che G-PACK si ferma senza aprire file fuori dal percorso della ricetta. La conferma con l'operatore del prodotto (T-M3A-PACK-02 ripetuto sull'operatore di C-18 e T-AST-ASSIST01) spetta a M3.p.

### 3.5 Cataloghi scelti

Dalla tabella «Decisioni del coordinatore» di `docs/datasets/M3-candidates.md` (licenze lette alla fonte primaria il 29 ottobre 2026; non è un parere legale):

| Uso | Catalogo | Stato |
|---|---|---|
| Astrometria del solver assistito (C-18, M3.p) | 2MASS Point Source Catalog (Skrutskie et al. 2006), bande J, H, Ks | Scelto. Audit dell'estratto prima dell'adozione in M3.p: alla fonte primaria IRSA si legge solo «no usage restrictions»; il «public domain» compare in una fonte secondaria. Se l'audit fallisce, M3.p torna BLOCKED |
| Colore relativo nel footprint SDSS (C-16, M3.q) | Catalogo fotometrico SDSS, bande u, g, r, i, z | Scelto. Pubblico dominio alla fonte primaria; rilascio fissato nel record `PUB-*` all'adozione |
| Colore relativo fuori dal footprint SDSS | nessuno | BLOCKED: nessun catalogo broadband ridistribuibile copre tutto il cielo. Sblocco solo con autorizzazione scritta ESA o conferma USNO per UCAC4; decide il proprietario |
| Scartati | Gaia DR3, Hipparcos e Tycho-2 (NC), APASS DR10 (nessuna concessione), ATLAS-REFCAT2 (componenti ereditati), UCAC4 e Pan-STARRS DR2 (licenza non dichiarata), Yale BSC (nessuna licenza), index astrometry.net serie 4200 (non necessari: l'indice si costruisce dal pack) | Non adottabili o non necessari |

Limiti dichiarati della scelta, da trattare nei contratti C-18 e C-16: 2MASS è nel vicino infrarosso, epoca 1997–2001 e senza moti propri (l'errore di posizione cresce con l'intervallo di epoca); SDSS copre circa un terzo del cielo e satura intorno a r ≈ 14. Dimensione dei pack: solo la stima progettuale di `M3-candidates.md` (circa 0,1 GB fino a mag 12, 0,6–0,8 GB fino a mag 14), non misurata.

G-PACK in M3.a si costruisce e si prova solo con pack sintetici generati dal test (seed fisso, componente `synthetic`, licenza `CC0-1.0`). Gli estratti 2MASS e SDSS si costruiscono e si adottano in M3.p e M3.q, dopo il loro audit.

### 3.6 Scelte fissate dai test di U10 (coordinatore, 30 ottobre 2026)

I test di U10 (`tests/unit/test_pack.cpp`, review Codex PASS al giro 2) fissano punti che §3.1–§3.4 lasciavano aperti. Il coordinatore li adotta come parte del contratto:

- Chiavi JSON dei sotto-oggetti del manifest: `size_bytes`, `sha256`, `index`, `meaning`, `order`, `cells`, `name`/`system`, `builder{name, version, inputs_sha256}`.
- `license_evidence` è un percorso relativo alla radice delle evidenze, sotto `docs/datasets/`, senza `..`; la licenza del record si legge dalla riga «Licenza SPDX: \`<id>\`». Licenza del record diversa da quella del manifest: `pack.license_evidence`.
- `CC-BY-4.0` senza attribuzione: `pack.license_not_allowed`; `license_file` assente o fuori da `licenses/`: `pack.license_file`.
- Percorso assoluto o con `..` in `files[]`: `pack.manifest_invalid`.
- Dimensione diversa: `pack.size_mismatch`, controllata prima dell'hash; file elencato sostituito da una directory: `pack.file_missing`; percorso della ricetta che è un file, o `manifest.json` che è una directory: `pack.missing`.
- Errore del sistema operativo che non è un'assenza (per esempio `EACCES`) su un file del pack o del record di audit: `io` `pack.io` (aggiunto con U10, arbitraggio del coordinatore).
- `flag_bits` è un campo obbligatorio non vuoto: `flag_bits: []` è `pack.manifest_invalid` (lettura letterale di §3.1, applicata in U10).
- Link simbolici: un link elencato in `files[]` è `pack.file_missing`, uno non elencato `pack.unlisted_file`, un `manifest.json` link `pack.missing`; la directory della ricetta segue un eventuale link.
- Aperti, da chiudere in U11 con un caso di test di un autore diverso: codice per una directory relativa nella ricetta (§3.2 la vieta, §3.4 non assegna un codice; oggi si apre relativa alla directory corrente); `audit.evidence_root` vuoto (oggi non rifiutato); caso di `flag_bits: []`.

## 4. Regola di licenza all'ingresso M3 (NFR-LIC-01.d)

Prima dell'adozione di una libreria nuova (prima build di un binario che la collega) o di un pack:

1. Libreria: riga in `docs/licenses.md` con versione, pacchetto, file reale, SHA-256 del file caricato, licenza letta dal file `copyright` del pacchetto installato (con lo SHA-256 del file `copyright`), compatibilità con GPL-3.0-or-later, moduli usati; le dipendenze transitive caricate (`ldd`) con le stesse colonne. T-M2R0-01 (`tools/check_licenses.py`) fallisce se un binario carica una libreria senza riga.
2. Pack: un record `docs/datasets/PUB-*.md` per ogni componente con fonte primaria, testo della licenza citato con data di lettura, rilascio, URL, hash dei file acquisiti; licenza nel manifest uguale a quella del record (T-M3A-LIC-01).
3. Licenze ammesse per un componente di pack distribuito con il prodotto: `LicenseRef-public-domain` (dichiarazione alla fonte primaria), `CC0-1.0`, `CC-BY-4.0` (con `attribution` non vuota), `GPL-2.0-or-later`, `GPL-3.0-or-later`. Ogni altra licenza, comprese le clausole NC, ND e SA, richiede una revisione di questo contratto con review.
4. Un audit non è dichiarato finito finché il record non esiste (piano riga 2018).

Audit eseguito in questa unità per LibRaw, libtiff e le loro dipendenze transitive: `docs/licenses.md`, sezione «NFR-LIC-01.d». Esito: nessuna incompatibilità con GPL-3.0-or-later.

## 5. Offline

Estensione delle clausole di ADR-13.a: fra i simboli indefiniti di `libastrocore` e degli eseguibili del prodotto non compaiono `libraw_open_file`, `libraw_open_wfile`, `_ZN6LibRaw9open_file*`, `TIFFOpen`, `TIFFOpenW`, `TIFFOpenExt`, `TIFFFdOpen`. T-M2F-02 si ripete sul workflow con decodifica RAW e G-PACK.

## 6. Test pianificati

Etichette di lavoro, da riportare in `docs/test-matrix.md`. Tolleranze fissate qui, prima di ogni misura. Fixture DNG (FIX-SYN-DNG-01) scritte da un generatore TIFF dei test (`tests/fixtures/syn_dng.hpp`), che non condivide codice con LibRaw, con libtiff né con il prodotto. Riferimento per i tag DNG: Adobe DNG Specification 1.6.0.0 (dicembre 2021), letta il 30 ottobre 2026 da `https://helpx.adobe.com/content/dam/help/en/photoshop/pdf/dng_spec_1_6_0_0.pdf`, SHA-256 `7f6e7867569c1daeae7f6835cd0a6a769e47e36b109594a8b3cc321d2327e569`; pagine citate: 21 (origine del motivo CFA), 26 (origine del motivo di `BlackLevel`), 42 (`ActiveArea`), 84 (linearizzazione). Le fixture dichiarano `DNGVersion` 1.4.0.0: che le clausole citate valgano già nella 1.4 è un'ipotesi, la 1.4 non è stata letta.

Copertura dei DNG sintetici (fatti, sonda). FIX-SYN-DNG-01 non può produrre: `temperature_c` (il DNG sintetico non ha makernote), `cblack[0..3]` non nulli (LibRaw mette il nero per canale del DNG nel motivo, §2.4), `linear_max` e `data_maximum` non nulli, `is_fuji_rotated() ≠ 0`. In U2 questi casi si proveranno per iniezione sui campi letti (dopo `open_buffer` per i campi di §2.3, dopo `unpack` per livelli e metadati). Su dati reali li prova T-M3A-RAW-09 in U4 (PUB-RAW-01, solo coerenza, nessuna verità sui valori): `temperature_c` presente su Olympus E-M10 e assente sugli altri sette file, `cblack[0..3]` non nulli su Pentax K-x, `linear_max` non nulli su Canon, Nikon, Sony, Olympus e Panasonic. `data_maximum` non nullo e `is_fuji_rotated() ≠ 0` non hanno un file reale: solo iniezione, pianificata e non ancora eseguita (§7).

| ID | Verifica | Tolleranza o criterio |
|---|---|---|
| T-M3A-RAW-01 | DNG sintetici non compressi a 16 bit: quattro pattern × `ActiveArea` con origine (0,0), (1,0), (0,1), (1,1). Campioni, crop dichiarato e pattern risultante (H2); per le origini dispari crop atteso secondo §2.3 (margine al pari successivo, dimensione ridotta di 1, bordo opposto fermo), calcolato dall'area scritta. Metadati di §2.6; `SensorTemperature` iniettato dopo `unpack`: 25 → `temperature_c` 25 con origine `decoder`; −1000 → assente | Esatta: 0 su campioni, pattern, crop, metadati |
| T-M3A-RAW-02 | Nero scalare e per canale con `BlackLevelRepeatDim` 2×2, `WhiteLevel` noto. Campioni non sottratti (H1). Nero per campione dalla formula di H3, uguale a quello scritto (ancorato all'origine di `ActiveArea`): nero scalare con origine pari e dispari; motivo non uniforme con origine (0,0); motivo invariante sull'asse spostato con origine dispari (per esempio 64, 64, 80, 80 con origine (1,0)); motivo uniforme con origine (1,1). Motivo non invariante sull'asse con margine maggiore di zero (64, 72, 80, 96 con origini (1,0), (0,1), (1,1), (2,2)): stop. Per iniezione dopo `unpack`: `cblack[0..3]` non nulli (termine per colore di H3), `data_maximum` e `linear_max` non nulli registrati, nulli assenti; bianco | Esatta: 0; stop `unsupported`, `raw.black_pattern_anchor`, nessuna immagine |
| T-M3A-RAW-03 | Ogni riga della tabella di §2.3. Da fixture DNG: X-Trans (`CFARepeatPatternDim` 6×6), `LinearRaw` a 3 campioni, pattern non 2×2, DNG in virgola mobile a 16, 24 e 32 bit (stop dopo `open_buffer`, prima di `unpack`), file con più immagini. Bit di §2.2: il gancio subito prima di `open_buffer` legge le opzioni effettive e trova `LIBRAW_RAWOPTIONS_CONVERTFLOAT_TO_INT` tolto; DNG in virgola mobile a 32 bit con `is_floating_point()` iniettato a 0 dopo `open_buffer`: stop `raw.float_data` dal controllo di `float_image` dopo `unpack`, che con il bit attivo resterebbe nullo (§2.2) e lascerebbe passare l'immagine. Da iniezione dopo `open_buffer` (`fault_injector` sui campi letti): `is_foveon ≠ 0`, `colors ≠ 3`, `pixel_aspect ≠ 1`, `is_fuji_rotated() ≠ 0`, `is_floating_point() ≠ 0`, `filters` con cella 2×2 non conforme | Per ogni caso: stop con categoria e codice della tabella; nessuna immagine |
| T-M3A-RAW-04 | `LinearizationTable` identica di 65 536 voci (`identity`); identica di 4096 voci (`decoder_curve`; campione scritto oltre la tabella letto uguale all'ultima voce); non identica (H4): campioni uguali alla tabella applicata, `linearization` dichiarata, digest uguale a quello calcolato dal test dalla tabella scritta estesa con l'ultima voce e serializzata come in §2.4 | Esatta: 0 |
| T-M3A-RAW-05 | File vuoto, troncato a più lunghezze, byte casuali, dimensione oltre `max_file_bytes`, dimensioni dichiarate oltre `max_pixels`; gestore di errore di dati invocato dal `fault_injector` con `unpack` riuscito; bit di avviso fuori lista iniettati; ogni codice di ritorno della tabella di §2.5 iniettato, per `open_buffer` e per `unpack` | Stop con categoria e codice della tabella; contesto con funzione, valore di LibRaw e `libraw_strerror` uguali a quelli iniettati (file e offset per il gestore); nessuna allocazione dei campioni oltre il limite |
| T-M3A-RAW-06 | Prenotazione prima di `unpack`; budget sotto il minimo: stop prima di `unpack`; `max_raw_memory_mb` impostato; picco di RSS registrato | Stop binario; RSS solo registrato |
| T-M3A-RAW-07 | Nessuna apertura per nome: simboli vietati di §5 assenti; sotto `strace -f -e trace=network,openat` nessuna syscall di rete e nessun `openat` nella directory dell'input dopo la lettura del prodotto; SHA-256 calcolato sullo stesso buffer passato a `open_buffer` | Binaria |
| T-M3A-RAW-08 | Creazioni di thread (`pthread_create` interposto nell'eseguibile di test) negli intervalli `open_buffer` e `unpack` (H6, §2.7), con controllo positivo a due thread OpenMP | Con il limite: 0 creazioni in ciascun intervallo; controllo positivo: almeno 1 creazione, altrimenti il test non prova H6 |
| T-M3A-RAW-09 | Campioni raw.pixls.us solo CC0, un file per marca (Canon, Nikon, Sony, Fujifilm Bayer, Olympus, Pentax, Panasonic), hash registrato prima nel record `PUB-*`: CFA con pattern fra i quattro, nero e bianco dichiarati, stato `raw`, metadati con origine; un Fujifilm X-Trans: stop `raw.xtrans`. Nessuna verità indipendente dei campioni: solo coerenza (campioni in `[0, 65535]`, pattern valido). Copertura dei metadati reali come nel paragrafo «Copertura dei DNG sintetici». Fatto (sonda), da trattare in §2.5 prima di U4: Fujifilm X-A1 e X-E1 danno `process_warnings = 0x10000` (`LIBRAW_WARN_PARSEFUJI_PROCESSED`) dopo `unpack`, un bit fuori dalla lista ammessa: con §2.5 com'è oggi X-A1 si ferma con `raw.warning` | Binaria |
| T-M3A-RAW-10 | Fuzz con seed fisso: 5 000 mutazioni per file seme (fixture di 01–04 e campioni di 09), build dev con ASan/UBSan, entro il 70% delle CPU | Ogni input: immagine valida o stop con categoria; zero crash e zero abort. LibRaw di sistema non è strumentata: ASan/UBSan coprono solo il codice del progetto |
| T-M3A-RAW-11 | Record del decoder di §2.9 completo per ogni decodifica riuscita di 01, 02, 04, 09 | Binaria |
| T-M3A-RAW-12 | Due decodifiche dello stesso file: digest dei campioni, record e chiave del nodo identici. Poi un campo del record di §2.9 alla volta cambiato per iniezione (versione di LibRaw, nero, linearizzazione e digest della curva, crop, parità dei margini, pattern, stato): la chiave del nodo cambia ogni volta | Esatta; chiave diversa per ogni variazione |
| T-M3A-RAW-13 | Emendamento L4 (§2.6). Record del decoder di ogni decodifica riuscita di 01, 02, 04, 09 ed export della provenance: le chiavi `sensor`, `pixel_size_um`, `pixel_scale_arcsec`, `wcs` presenti con valore `null` e `absent_reason`; poi `other.focal_len`, `FocalLengthIn35mmFormat` e `other.parsed_gps` valorizzati per iniezione dopo `open_buffer` (`fault_injector` sui campi letti, come T-M3A-RAW-03): stesse chiavi `null`; nessun campo di `frame_metadata` valorizzato oltre quelli elencati in §2.6 | Binaria |
| T-M3A-PACK-01 | Pack sintetico valido: G-PACK passa; `pack_digest` in ricetta, chiave e provenance | Binaria |
| T-M3A-PACK-02 | Pack assente: stop `pack.missing` del solo ramo dipendente, altri rami completati, nessuna lettura fuori dal percorso della ricetta (osservatore dei `openat` attivo solo durante la chiamata di G-PACK). In M3.a con il job del test di §3.4 («Prova in M3.a»); con l'operatore del prodotto in M3.p | Binaria; zero chiamate all'operazione |
| T-M3A-PACK-03 | Corruzione: un byte cambiato in dati, indice, manifest e licenza; file mancante; file in più; dimensione diversa | Codici di §3.4; zero chiamate all'operazione |
| T-M3A-PACK-04 | Manifest: ogni campo obbligatorio tolto o vuoto; `pack_format` sconosciuto; JSON non canonico; digest diverso dalla ricetta; ricetta con digest corretto e `pack_id` o `pack_version` diversi; colonna intera `nullable`; `null_bit` o `flag_bits` duplicati; `null_bit` fuori da 0–31; `index_order` diverso da `coverage.order`; `data_file` o `index_file` non in `files[]` | Codici di §3.4 (`pack.id_mismatch` per id e versione, `pack.manifest_invalid` per lo schema) |
| T-M3A-PACK-05 | Licenza fuori lista (per esempio `CC-BY-NC-4.0`), `CC-BY-4.0` senza attribuzione, `license_file` assente | `pack.license_not_allowed`, `pack.license_file` |
| T-M3A-PACK-06 | Copertura sintetica nota a ordine basso: regione interna, a cavallo del bordo, esterna; oracolo indipendente dall'implementazione HEALPix del prodotto (da fissare nel piano di M3.a prima della misura) | Interna passa; a cavallo ed esterna: `pack.out_of_coverage`; nessun falso accetto |
| T-M3A-PACK-07 | Pack `astrometry` per un'operazione `photometry`; banda richiesta assente | `pack.purpose`, `pack.band_missing` |
| T-M3A-PACK-08 | Conteggi del manifest, del file dati e dell'indice incoerenti; dimensione del file dati non multipla del record; indice di dimensione errata, non contiguo o con celle fuori ordine. Righe: assenza valida (bit a 1 e `NaN`) accettata; bit a 1 con valore finito, `NaN` con bit a 0, `NaN` in colonna non nullabile, infinito in colonna non nullabile, bit non dichiarato a 1 | `pack.count_mismatch`, `pack.index_invalid`; caso valido accettato; gli altri `pack.row_invalid` |
| T-M3A-PACK-09 | Fuzz con seed fisso del lettore di manifest e indice: 5 000 mutazioni per file seme, ASan/UBSan | Stop controllato o pack valido; zero crash |
| T-M3A-LIC-01 | Ogni `license_evidence` nomina un record esistente la cui licenza coincide con quella del manifest; controllo negativo con record assente e licenza diversa | Binaria |

Test del piano che questi alimentano: T-IO-01 (import RAW, M3.s), T-AST-ASSIST01 (pack mancante e fuori catalogo, M3.p), T-FBK-01 parte M3 (decoder che fallisce, pack mancante: nessun percorso alternativo eseguito), T-M2F-01 e T-M2F-02 ripetuti, T-M2R0-01 rieseguito con i binari che collegano LibRaw e libtiff.

## 7. Limiti dichiarati

- Nessuna verità indipendente sui campioni dei RAW di marca: la correttezza bit per bit è provata solo sui DNG sintetici. Per i formati proprietari vale l'ipotesi che LibRaw li decodifichi correttamente.
- H1, H2 e H4 sono provate solo sui DNG sintetici (sonda e test di U2 contro una realizzazione usa-e-getta, non sul codice del prodotto); H3 è riformulata (§2.4) e vale fuori dai casi fermati da `raw.black_pattern_anchor`; H5 e H6 non sono provate a questo stadio. Il codice che vi si appoggia si accetta solo con i rispettivi test verdi sul prodotto.
- DNG con `ActiveArea` di origine dispari: la prima riga o colonna dell'area attiva non si importa (arrotondamento di LibRaw, §2.3) e il prodotto non può osservarlo.
- DNG con motivo di nero non invariante sull'asse con margine maggiore di zero: stop `raw.black_pattern_anchor`, anche con origine pari, dove il nero sarebbe esatto (§2.4).
- `data_maximum` non nullo e `is_fuji_rotated() ≠ 0` si possono provare solo per iniezione (test pianificati in T-M3A-RAW-02 e T-M3A-RAW-03, non ancora eseguiti sul prodotto); temperatura, `cblack[0..3]` e `linear_max` reali solo su PUB-RAW-01 (U4). Il marcatore −1000 della temperatura assente è misurato, non letto nella sorgente di LibRaw.
- H0 (dati RawSpeed CC-BY-SA-3.0 assenti dal binario) non ha un test: si chiude solo leggendo il log di build del pacchetto, prima della distribuzione (M5).
- La build di LibRaw installata non ha zlib (`libraw_capabilities()`): i DNG con compressione deflate non si decodificano.
- Origine del nero e del bianco (file o tabella di LibRaw) non distinguibile; operazioni del firmware non osservabili.
- Sensore, scala pixel e WCS dei RAW non sono disponibili dal decoder: restano `null`, mai stimati (§2.6, emendamento L4). Un campo di makernote per marca che li descrivesse non è letto.
- Pixel mascherati non importati.
- Allocazioni interne di LibRaw non prenotate.
- Un crash di LibRaw su input malformato non è contenuto nel processo.
- Linearità `linear` dei RAW è una dichiarazione del decoder, non una misura.
- Gli audit degli estratti 2MASS e SDSS non sono fatti: G-PACK si prova su pack sintetici. Il colore fuori dal footprint SDSS è BLOCKED.
- Dimensione dei pack e costo dell'hash completo nel preflight: stime non misurate.
