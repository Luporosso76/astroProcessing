# M3.entry, ADR-08.b e NFR-LIC-01.d: contratto del decoder e dei pack, audit di licenza

Stato: **review FAIL al giro 3 di 3, chiusa dall'arbitraggio del coordinatore (variante a), senza quarto giro**. Unità: M3.entry, riga «ADR-08.b, NFR-LIC-01.d» di `plans/M3.md`. Solo testo e audit di licenza: nessun codice, nessun test eseguito.

| Campo | Valore |
|---|---|
| Unità | M3.entry / ADR-08.b, NFR-LIC-01.d |
| Scopo | Chiudere all'ingresso il contratto del decoder RAW (§9.6) e dei catalog pack, e registrare l'audit di licenza delle librerie adottate |
| Comando | `python3 -S tools/trace.py ids --stage M3.entry`; `pkg-config --modversion libraw_r libtiff-4`; `dpkg -S`, `dpkg-query -W`, `sha256sum`, `ldd`, `readelf -d`, `nm -D --defined-only`, `strings` sui file di libreria; `python3 -S tmp/adr08b/inv.py <soname…>` e `python3 -S tmp/adr08b/lic.py <pacchetto…>` (script non versionati: hash, pacchetto, versione e righe `License:` dei file `copyright`); `python3 -S tools/check_licenses.py docs/licenses.md /opt/workspace/astroProcessor/build/release/tests/unit` |
| Input | Piano attivo (SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`): §9.5–9.6 (righe 1259-1286), §10.19 (C-18), §17.4 (righe 2484-2497), riga 2018 (NFR-LIC-01), riga 2569 (ADR-08), appendice D righe 3402-3404, 3560-3593, 3685, 4088-4089, 4125, 4135, 4373, 4398-4403. `plans/M3.md` (tabella M3.entry, «Dipendenze esterne»). `docs/datasets/M3-candidates.md`. Header `/usr/include/libraw/libraw.h`, `libraw_types.h`, `libraw_const.h`, `libraw_version.h`. File `copyright` dei pacchetti elencati in `docs/licenses.md`, sezione «NFR-LIC-01.d», con il loro SHA-256 |
| Versioni | LibRaw 0.21.5 (`libraw23t64` 0.21.5b-1ubuntu1.1), libtiff 4.7.0 (`libtiff6` 4.7.0-3ubuntu5); dipendenze transitive con versione in `docs/licenses.md` |
| Ambiente | Linux 7.0.0-38-generic x86_64, macchina di sviluppo; worktree da `60a6cc8` |
| Risultato | Contratto `docs/contracts/ADR-08-decoder-pack.md`; audit in `docs/licenses.md`; 22 ID di test pianificati in `docs/test-matrix.md`. `check_licenses.py` sui binari di test della build release esistente: `licenze ok: 40 librerie caricate` (la tabella delle librerie caricate resta valida dopo la modifica) |
| Output | File elencati nel blocco generato sotto; artefatti di review in `.omc/artifacts/ask/` |
| Ambito | Contratto e audit di licenza. Fuori: codice, test, acquisizione dei campioni raw.pixls.us, audit degli estratti 2MASS e SDSS, lettura della specifica DNG e della documentazione online di LibRaw (nessun accesso alla rete) |
| Esito | Review FAIL al giro 3 (un MED su T-M3A-RAW-08), rilievo chiuso per arbitraggio del coordinatore; vedi «Review» e «Arbitraggio» |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Fonti lette

- Registro dell'appendice D per ADR-08, ADR-08.a, ADR-08.b, NFR-LIC-01.c/d, G-PACK, G-STOP, P-SOLVE, P-PACK-MISSING, F-IO-02, F-IO-04, C-18, F-REG-04.a, F-POST-05.a.1.
- §9.6 (contratto del decoder), §17.4 (pack e D-21), §10.19 (C-18), riga 2018 (NFR-LIC-01).
- Contratti esistenti: ADR-03 (metadati, CFA, limiti), ADR-05 (modello d'errore), ADR-13 (offline), ADR-16 (ledger).
- Header di LibRaw installati: API (`open_buffer`, `unpack`, callback), campi di `imgdata` (`filters`, `cblack`, `black`, `maximum`, `curve`, margini), codici d'errore e di avviso, `LIBRAW_MAX_ALLOC_MB_DEFAULT`.
- File `copyright` dei pacchetti installati (hash in `docs/licenses.md`).
- Riferimenti citati e non letti in questa unità: Adobe DNG Specification; Górski et al. 2005 (HEALPix); Skrutskie et al. 2006 (2MASS); pagine di licenza IRSA e SDSS, già lette in `M3-candidates.md` il 29 ottobre 2026.

## Decisioni

- LibRaw solo da memoria (`open_buffer` + `unpack`), lettura di `raw_image` senza nessuna funzione di elaborazione; area visibile importata con crop dichiarato; solo Bayer 2×2; X-Trans, dati non CFA, virgola mobile, più immagini, sensori ruotati: stop.
- Nero, bianco e curva di linearizzazione dichiarati con origine `decoder`, mai sottratti o applicati dal prodotto all'import; stato del dato `raw`, operazioni del firmware `unknown`.
- Errore di dati segnalato da LibRaw = stop anche se `unpack` riesce; avvisi ammessi in lista chiusa, ogni altro avviso = stop.
- OpenMP di LibRaw limitato a un thread nel worker prenotato; limiti `max_file_bytes` 1 GiB e `max_pixels` 2^28.
- libtiff solo con `TIFFClientOpen` in memoria, per il writer TIFF e le fixture DNG.
- Pack: directory con manifest JSON canonico (identità = SHA-256 del manifest), file con hash, licenze per componente, copertura HEALPix NESTED con inclusione conservativa, indice per cella; G-PACK nel preflight con hash completo; pack mancante, corrotto o fuori copertura = stop del ramo, senza ricerca, download o cambio di pack.
- Cataloghi: 2MASS PSC per l'astrometria, SDSS per il colore nel suo footprint, colore tutto-cielo BLOCKED (decisioni del coordinatore in `M3-candidates.md`).
- NFR-LIC-01.d: regola di adozione per librerie e pack, lista chiusa delle licenze ammesse per i componenti di pack. Audit di LibRaw, libtiff e transitive: nessuna incompatibilità con GPL-3.0-or-later, sotto l'ipotesi H0 che `rawspeed_xmldata.cpp` (CC-BY-SA-3.0) sia assente dal binario (capacità RawSpeed assente in `libraw_capabilities()`, simbolo e stringhe assenti; log di build non letto).

## Limiti

- Sei ipotesi sul comportamento di LibRaw (H1–H6) non provate: le provano i test di M3.a prima del codice che le usa. H0 (dati RawSpeed assenti dal binario) senza test: si chiude con il log di build del pacchetto prima di M5.
- `libraw_capabilities()` = `0x80` (`python3 -S tmp/adr08b/caps.py`): build senza RawSpeed, DNG SDK e zlib; i DNG deflate non si decodificano.
- Nessuna verità indipendente per i campioni dei RAW di marca; origine del nero (file o tabella) non distinguibile; pixel mascherati non importati; allocazioni interne di LibRaw non prenotate; crash di LibRaw non contenuto nel processo; `BlackLevelDeltaH/V` non coperti.
- Audit degli estratti 2MASS e SDSS non fatto; dimensione dei pack e costo dell'hash nel preflight non misurati.
- Le righe delle nuove librerie non sono ancora nella tabella «Librerie caricate dai binari»: vanno copiate al primo collegamento (M3.a).

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-adr08b-round1-20261030-0238.md` | `tmp/adr08b-round1.diff` | FAIL | 9 MED, 1 LOW: manifest e file non elencati, codifica di dati e indice, nullabilità, `pack_id`/`pack_version`, RawSpeed affermato come fatto, contesto degli errori di LibRaw, RAW-08 non discriminante, casi mancanti in RAW-03, chiave non verificata in RAW-12; directory e file regolare |
| 2 | `.omc/artifacts/ask/codex-adr08b-round2-20261030-0244.md` | `tmp/adr08b-round2.diff` | FAIL | 2 MED: RAW-08 non attribuisce la crescita CPU alla regione OpenMP; casi negativi mancanti in PACK-04 e PACK-08 |
| 3 | `.omc/artifacts/ask/codex-adr08b-round3-20261030-0247.md` | `tmp/adr08b-round3.diff` | FAIL | 1 MED: RAW-08 conta solo i thread vivi dopo `unpack` (thread creati e terminati sfuggono) e il censimento iniziale precede `open_buffer` |

Rilievo aperto dopo il giro 3: T-M3A-RAW-08 (§2.7 e §6 del contratto). Tutti gli altri rilievi dei giri 1 e 2 risultano chiusi dal revisore.

## Arbitraggio

Decisione del coordinatore sul rilievo del giro 3, 30 ottobre 2026: variante (a), nel processo, senza `strace`, nessun quarto giro di review. Delta applicato: `tmp/adr08b-arbitrato.diff`.

- T-M3A-RAW-08 conta le creazioni di thread, non i thread vivi: `pthread_create` definito nell'eseguibile di test (prevale su libc nel collegamento dinamico), contatore atomico, inoltro con `dlsym(RTLD_NEXT)`; contatori distinti per `open_buffer` e `unpack` con lettura prima e dopo. H6 provata con 0 creazioni in entrambi gli intervalli sotto `omp_set_num_threads(1)`; controllo positivo con due thread OpenMP: almeno una creazione, altrimenti il test non prova nulla. Limite dichiarato: i thread creati con `clone` diretto non si vedono; che libgomp usi `pthread_create` lo verifica il controllo positivo.
- Vincolo per l'unità ADR-03.b: `metadata_source` deve includere il valore `decoder` (ADR-08.b §2.6).
- Prefissi `T-M3A-RAW`, `T-M3A-PACK`, `T-M3A-LIC` confermati; le altre unità di M3.entry usano famiglie proprie.
- H0 (dati RawSpeed assenti dal binario, log di build del pacchetto da leggere) resta un limite dichiarato da chiudere prima di M5.

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-08-decoder-pack.md` | `b5abcafc55e824f9d7b2ac19a08fae98e66e70e125e33561ee38a8841c93ae98` |
| `docs/contracts/README.md` | `353e714178f05198679290fc445edac14ca3aa881f4f1d4e91c44d7c8df00b7c` |
| `docs/decisions/README.md` | `860b4a0b676f845260ed4a326d228c44b1b89ac58910261949fb640d354cad9b` |
| `docs/licenses.md` | `b9ffb9334ff642779c86cbd8c553b36f6b07887526801a5571e66fb0f2b6ffe1` |
| `docs/test-matrix.md` | `b02ba9fc0df97c4347e39938cf498475ab00f8930e8559c6e39a7ad9aa80663d` |

<!-- evidence:end -->
