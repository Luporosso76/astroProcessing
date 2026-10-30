# PUB-RAW-01: RAW di più marche da raw.pixls.us, solo file CC0

- Stato: ACQUISITO il 30 ottobre 2026 (coordinatore, `tmp/acq/rawpixls.py`): licenza CC0 riletta sui metadati attuali per gli 8 file, SHA-256 dei byte scaricati uguale a quello sotto per gli 8 file, file in sola lettura in `data/public/raw-pixls-us/`. Licenza SPDX: `CC0-1.0`.
- Fonte: raw.pixls.us, https://raw.pixls.us/. Metadati dal repository `https://raw.pixls.us/json/getrepository.php?set=all`, letti il 29 ottobre 2026: copia `tmp/datasets-research/rpu_repo.json` nella copia principale del progetto (esclusa da git), 1 248 361 byte, SHA-256 `3908c07909ebc66d6d0a8ddbdb56e1bc4f480f4e5b1703af256d7c9ca82e55f5`. Il 30 ottobre 2026 la stessa URL restituisce un testo della stessa lunghezza (1 248 361 caratteri); il contenuto delle righe scelte non è stato riletto dalla rete.
- Licenza: per file. Ogni file scelto ha nei metadati il collegamento `https://creativecommons.org/publicdomain/zero/1.0/` (Creative Commons 0, pubblico dominio). Il repository contiene anche file CC BY-NC-SA 4.0: sono esclusi, perché fuori dalla lista chiusa di ADR-08.b §4.3. Attribuzione non richiesta da CC0; si cita la fonte. Non è un parere legale.
- Rilascio: il repository non ha versioni. L'identità di ogni file è il numero in `getfile.php/<id>` più lo SHA-256 pubblicato accanto al file, riportato sotto prima dell'acquisizione.
- Acquisizione (passo separato, deciso dal coordinatore): download da `https://raw.pixls.us/getfile.php/<id>/nice/<nome>` in `data/public/raw-pixls-us/<id>/<nome>`, esclusa da git. All'acquisizione si rilegge la licenza della riga nei metadati: un file non più CC0 non si acquisisce e il record torna in review. SHA-256 calcolato sul file scaricato e confrontato con quello sotto: un valore diverso ferma l'acquisizione di quel file.
- Uso: T-M3A-RAW-09 (coerenza del decoder su formati reali, X-Trans come caso di stop), T-M3A-RAW-10 (file seme del fuzz), T-M3A-RAW-11. Nessuna verità indipendente dei campioni (ADR-08.b §7).

## File scelti

Criteri, fissati prima dell'acquisizione: una marca per riga fra quelle chieste da T-M3A-RAW-09; formato nativo della marca (niente DNG, che proverebbe il convertitore e non il formato); intera superficie del sensore (niente sRAW, mRAW, CRAW, niente modi di crop come «APS-C crop» nella nota del repository); dove il repository offre la scelta, compressione senza perdita o assente; preferenza per modelli diffusi nell'astrofotografia amatoriale. La preferenza per modello è una scelta, non una misura.

| Marca | Modello e modo (dai metadati) | Mpx e nota (dai metadati) | id | Nome | Dimensione dichiarata | Caricato | Ruolo |
|---|---|---|---|---|---|---|---|
| Canon | EOS 6D, `RAW` | 20,65; nota descrittiva della scena | 799 | `Canon - EOS 6D - RAW.CR2` | 22,13 MB | 2017-01-08 | Bayer |
| Nikon | D750, `12bit compressed (Lossless) (3:2)` | 24,32; `12-bit` | 897 | `Nikon - D750 - 12bit 12bit compressed (Lossless) (3:2).NEF` | 19,16 MB | 2017-01-15 | Bayer |
| Sony | ILCE-7S, `14bit compressed (3:2)` | 12,21; `Fullframe non-crop` | 1581 | `Sony - ILCE-7S - 14bit 14bit compressed (3:2).ARW` | 12,50 MB | 2017-07-13 | Bayer |
| Fujifilm | X-A1, `12bit uncompressed (3:2)` | 16,83; nessuna nota | 1214 | `Fujifilm - X-A1 - 12bit 12bit uncompressed (3:2).RAF` | 24,95 MB | 2017-03-23 | Bayer (ipotesi) |
| Olympus | E-M10, `16bit (4:3)` | 16,11; nessuna nota | 1331 | `Olympus - E-M10 - 16bit (4:3).ORF` | 14,55 MB | 2017-04-10 | Bayer |
| Pentax | K-x, `12bit (3:2)` | 12,48; nessuna nota | 831 | `Pentax - K-x - 12bit (3:2).PEF` | 13,54 MB | 2017-01-12 | Bayer |
| Panasonic | DMC-GH4, `4:3` | 15,99; `4:3` | 2571 | `Panasonic - DMC-GH4 - 4:3.RW2` | 18,68 MB | 2018-05-10 | Bayer |
| Fujifilm | X-E1, `12bit uncompressed (3:2)` | 16,83; nessuna nota | 3098 | `Fujifilm - X-E1 - 12bit 12bit uncompressed (3:2).RAF` | 24,94 MB | 2019-04-22 | X-Trans (ipotesi): stop `raw.xtrans` |

Lo stesso nome di file Sony esiste anche con id 1582, nota `APS-C crop`, 5,27 Mpx: escluso dal criterio della superficie intera. Il nome non identifica il file: lo identificano id e SHA-256.

Totale dichiarato: circa 151 MB. Le dimensioni sono quelle stampate dal repository; l'unità (MB o MiB) non è dichiarata.

SHA-256 pubblicati dal repository, percorsi relativi a `data/public/raw-pixls-us/`:

```
360842779d08fe4805b4a2fe4979f06fdc6da982c85c3acb181f25bcf4c9f536  799/Canon - EOS 6D - RAW.CR2
baae8dfc05a81a4a4034831a49ed99d129e797858ad876d31173a46d33b7d3ea  897/Nikon - D750 - 12bit 12bit compressed (Lossless) (3:2).NEF
7cc338a0abc8fdad32d61006f1f8f412297b93e040c7efba572eb2bd8f8e8be2  1581/Sony - ILCE-7S - 14bit 14bit compressed (3:2).ARW
b2ddb5fe31a3b976d12961c72a2689de8742739df3d8f757614eca65f8df2a61  1214/Fujifilm - X-A1 - 12bit 12bit uncompressed (3:2).RAF
d7f6ac99ee1f824e81ec0e71599f0f87f9b2e2675f7ddfdb21202281b8bdc5e0  1331/Olympus - E-M10 - 16bit (4:3).ORF
51a3f22d702353b246684dca79f83c79cfdbdf9892702ae5ac55dd469e239bb6  831/Pentax - K-x - 12bit (3:2).PEF
5ea2591972c3757596367e9a40677f9d7ccd7ce33719baeefa1ba12b9ee273a8  2571/Panasonic - DMC-GH4 - 4:3.RW2
0fa01fa1a674bcfa40f659225b4e66c5bedf15d2f78407ca1966dc8f38cd0e9f  3098/Fujifilm - X-E1 - 12bit 12bit uncompressed (3:2).RAF
```

## Limiti

- Il tipo di CFA per modello è un'ipotesi da fonti generali, non letta dai file: X-A1 Bayer, X-E1 X-Trans. Se il decoder smentisce una delle due, il file non si usa per il ruolo previsto e il record torna in review; nessun file sostitutivo scelto in automatico.
- Il file Sony usa la compressione del formato ARW, che può essere con perdita: in quel caso la curva del formato è una linearizzazione da dichiarare (ADR-08.b §2.4, H4), non un errore del campione.
- Il repository non è versionato: un file può cambiare o sparire. La difesa è lo SHA-256 registrato qui prima del download.
- La licenza è dichiarata da chi carica il file; il progetto non può verificarne la catena dei diritti.
- I file sono scene diurne: servono solo per formato, pattern e geometria, non per metriche astronomiche.
- Nessun Canon CR3, nessun DNG: i due contenitori non sono coperti da questo record.
