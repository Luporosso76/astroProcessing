# M3: candidati per dataset pubblici e cataloghi (record di ricerca)

**Record di ricerca, non di adozione.** Nessun dataset è adottato da questo file: ogni adozione richiede il proprio record `docs/datasets/PUB-*.md` con fonte, licenza, hash e copia locale, come `PUB-LFC-01.md`.

- Data di verifica delle licenze: 29 ottobre 2026, alla fonte primaria (pagina dell'archivio, record Zenodo o file di licenza). Dove la fonte primaria non dichiara una licenza, il candidato è "non adottabile".
- Nessun dataset scaricato. In `tmp/datasets-research/` (escluso da git) restano solo metadati: il JSON del repository raw.pixls.us (1,2 MB), il PDF delle AAVSO Data Usage Guidelines e due sonde HTTP a intervallo (primi 1–2 MB) degli archivi Zenodo 17843622 e 3989223, per leggere header tar e FITS.
- Non è un parere legale.

## Candidati

### 1. PUB-REG-01: sequenza multi-frame, stesso campo e stesso filtro

| Candidato | Licenza e URL | Contenuto e dimensione | Esito |
|---|---|---|---|
| ESO EFOSC2@NTT, NGC 2808, prog. 092.D-0449(A), filtro R#642 | CC BY 4.0, copyright ESO; la policy chiede il credito ESO e "file headers preserved"; calibrazioni pubbliche subito dopo l'osservazione. https://archive.eso.org/cms/eso-data-access-policy.html (policy del 7 novembre 2022) | Da query TAP su `dbo.raw`: 67 light da 150 s in 4 notti (24–27 febbraio 2014), circa 1,9 MB ciascuno, 128 MB in tutto. Nella prima notte: 21 bias, 6 sky flat R, 5 dome flat R. CCD mono | Adatto. Rischi: ammasso globulare affollato; gli estratti devono conservare l'header originale intero |
| ESO EFOSC2, MAXI-J0556-332, prog. 092.D-0588(A) | CC BY 4.0 (ESO), come sopra | Circa 55 frame per filtro in B, V, R, I da 120 s, campo poco affollato | Adatto, riserva |
| Zenodo 3989223, "WASP-74b transit 2020-07-28" | CC BY 4.0, dichiarata nel record: https://zenodo.org/records/3989223 | ASI183MM-Pro mono, filtro R, light da 30 s, dark e bias, nessun flat. Un `.tar` non compresso da 15057827840 byte, frame 5496×3672 da 40 MB, circa 370 membri (stima) | Adatto come CMOS amatoriale. Range HTTP verificato (206): estrazione di singoli membri possibile |
| SDSS Stripe 82, frame per epoca | Pubblico dominio: "All SDSS data released in our public data releases are considered in the public domain." https://www.sdss.org/collaboration/image-use-policy/ | 70–90 scansioni dello stesso campo e filtro, con WCS; frame già calibrati (drift scan, senza bias e flat grezzi), circa 3–4 MB `.bz2` | Adatto a registrazione e stacking, non alla calibrazione |
| Astropy CCD Guide, esempio 2 (Zenodo 3245296) | CC BY 4.0: https://zenodo.org/records/3245296 | 2 light (kelt-16b, r), 483 MB | Non adatto: meno di 5 light |
| Cosmic-CoNN LCO CR Dataset (Zenodo 5034763) | Zenodo dichiara CC BY 2.0; la fonte LCO dichiara CC BY-NC 4.0: https://lco.global/observatory/acknowledgments/ | 3 frame per campo, 43 GB | Non adottabile: conflitto di licenza, NC a monte |
| Archivio LCO | CC BY-NC 4.0: https://lco.global/observatory/acknowledgments/ | – | Non adottabile (NC) |
| NOIRLab Astro Data Archive | Nessuna licenza trovata: https://astroarchive.noirlab.edu/about | – | Non adottabile |
| ZTF (IRSA) | Solo i termini generali IRSA: "Most data served by IRSA is public, with no usage restrictions." https://irsa.ipac.caltech.edu/data_use_terms.html; nessun testo specifico per ZTF | – | Non adottabile finché non si trova un testo specifico |

### 2. PUB-DEEP-01: immagine profonda con nebulosità diffusa e stelle

| Candidato | Licenza e URL | Contenuto e dimensione | Esito |
|---|---|---|---|
| HST ACS/WFC drz/drc di un campo nebulare (MAST) | "science data become available for public use without restriction" dopo l'accesso esclusivo: https://archive.stsci.edu/hst/proprietary_rights.html | Stack di poche esposizioni, con WCS e PSF modellabile; 150–300 MB per file, riducibili con un cutout | Adatto, il più pulito legalmente. Rumore e PSF diversi da quelli amatoriali |
| Zenodo 17843622, "Seestar S50 vs Seestara S30" (M. Pössel), stack | CC BY 4.0, dichiarata nel record: https://zenodo.org/records/17843622; coordinate GPS alterate dall'autore | Stack di IC 434, IC 1805, M16, M42, M31, M51, M81/82, M10, M12. Un solo `.tgz` da 35016942317 byte | Adatto al dominio amatoriale. Rischio: gzip non indicizzabile, raggiungere un file può richiedere di scorrere fino a 35 GB; formato degli stack non verificato |
| ESO VPHAS+ (OmegaCAM) su M8 | CC BY 4.0 (ESO), URL al punto 1 | g, r, i, Hα, u; pawprint MEF da circa 190 MB | Parziale: nebulosa forte, ma esposizioni singole da 25–120 s, non stack |
| SDSS Stripe 82 coadd | Pubblico dominio (URL SDSS al punto 1) | Stack profondi con WCS | Quasi senza nebulosità (alta latitudine galattica) |
| Legacy Surveys DR10 coadd | "Images are licensed under a Creative Commons Attribution 4.0 International License": https://www.legacysurvey.org/acknowledgment/ (frase riferita ai layer del viewer) | Coadd g, r, z con WCS | Poca nebulosità; va confermato che la licenza copra anche i FITS |
| Immagini divulgative ESO/ESA/NASA | – | JPEG/TIFF non lineari | Non adatte: dati non lineari |

### 3. CFA astronomico ridistribuibile (M3.d)

| Candidato | Licenza e URL | Contenuto e dimensione | Esito |
|---|---|---|---|
| Zenodo 17843622, sub Seestar | CC BY 4.0: https://zenodo.org/records/17843622 | Dall'header letto con la sonda: FITS BITPIX 16, 1080×1920, `BAYERPAT='GRBG'`, `IMAGETYP='Light'`, `FILTER='IRCUT'`, RA e DEC; circa 4 MB per sub. Cartelle `*_sub` (es. `M 16_sub`, `M 10_sub`) | Unico CFA astronomico ridistribuibile trovato. Rischi: acquisizione (vedi punto 2); `TELESCOP` contiene un identificativo del dispositivo; pattern da verificare per camera; nessun dark o flat dichiarato |
| raw.pixls.us | CC0 per file (punto 4) | Scene diurne | Solo per pattern e geometria, non per le metriche astronomiche |

### 4. RAW multivendor (M3.a): raw.pixls.us

| Voce | Esito |
|---|---|
| Fonte | https://raw.pixls.us/, metadati da `https://raw.pixls.us/json/getrepository.php?set=all` |
| Licenza | Per file. Su 2020 campioni: 1874 CC0 1.0 (https://creativecommons.org/publicdomain/zero/1.0/) e 146 CC BY-NC-SA 4.0 (http://creativecommons.org/licenses/by-nc-sa/4.0/). Chi carica dichiara di avere tutti i diritti e rilascia in CC0; foto di persone escluse "for legal reasons" |
| Marche, campioni CC0 | Canon 329, Nikon 283, Sony 199, Fujifilm 144, Olympus 68, Pentax 75, Panasonic 373; inoltre Leica 36, Ricoh 6 |
| DNG | 262 in tutto, tutti CC0 tranne 5. DNG CC0 delle marche richieste: Canon 50, Pentax 36, Nikon 2; inoltre Leica 15, Ricoh 6. Nessun DNG Sony, Fujifilm, Olympus o Panasonic |
| Dimensione | Circa 56 GB in tutto; il file CC0 più piccolo per marca va da 1,5 a 9 MB. SHA-256 pubblicato per ogni file |
| Esito | Adottabile filtrando i soli file CC0. Rischi: escludere i file CC BY-NC-SA; molti Fujifilm della serie X sono X-Trans (servono Fuji Bayer, oppure si usano come test di stop); i DNG da Adobe DNG Converter non provano il formato nativo |

### 5. Pack di catalogo per solver assistito e colore relativo (M3.p/q)

| Catalogo | Licenza e URL | Bande e contenuto | Sottoinsieme ridistribuibile | Esito |
|---|---|---|---|---|
| Gaia DR3 (incluso GSPC) | "Gaia data are distributed under the CC BY-NC 3.0 IGO license": https://www.cosmos.esa.int/web/gaia-users/license; l'uso commerciale richiede autorizzazione a data.licences@esa.int: https://www.cosmos.esa.int/web/esdc/terms-and-conditions | G, BP, RP; GSPC con sintetiche B, V e g, r, i | Solo non commerciale | Adottato (proprietario, 30 ottobre 2026): il pack è un componente dati separato dal codice GPL, con la propria licenza CC BY-NC 3.0 IGO e l'attribuzione; distribuzione non commerciale, release commerciale solo con autorizzazione scritta ESA |
| Hipparcos, Tycho-2 | Nessuna licenza sulla pagina ESA (https://www.cosmos.esa.int/web/hipparcos/tycho-2); valgono i termini dell'archivio ESA: CC BY-NC 3.0 IGO | Tycho-2: 2,5 M stelle, BT e VT | Solo non commerciale | Non adottabili |
| APASS DR10 | AAVSO Data Usage Guidelines del 16 luglio 2026 (https://aavso.org/wp-content/uploads/2026/07/Data-Usage-Guidelines-2026-07.pdf): solo riconoscimento e citazione | B, V, g', r', i' | Nessuna concessione | Non adottabile |
| ATLAS-REFCAT2 | "These HLSP data products are licensed for use under CC BY 4.0": https://archive.stsci.edu/hlsp/atlas-refcat2 | Circa 1 G stelle fino a m≈19, g, r, i, z (più J, H, K), tutto il cielo; circa 47 GB `.tbz` | Dichiarato dall'autore, ma eredita astrometria Gaia DR2 e stelle brillanti da Tycho-2, APASS DR9 e Yale BSC | Il migliore tecnicamente, non adottabile oggi |
| UCAC4 (USNO) | Il readme primario non ha licenza ("free of charge", citare il paper): https://ftp.imcce.fr/pub/catalogs/UCAC4/readme_u4.txt; solo data.nasa.gov (fonte secondaria) indica "U.S. Government Works" | 113,8 M stelle, completo a circa mag 16; APASS DR6 B, V, g, r, i più 2MASS; circa 8,9 GB | Non dichiarato; componenti di terzi (APASS e, da verificare, Hipparcos/Tycho-2 per le stelle brillanti) | Non adottabile oggi; chiudibile con conferma scritta USNO |
| Pan-STARRS DR2 | Nessuna licenza sulla pagina PS1: https://outerspace.stsci.edu/display/PANSTARRS/; MAST dice solo "Most data hosted at MAST are in the public domain": https://archive.stsci.edu/data_use.html | g, r, i, z, y; satura a circa mag 13–14; declinazione > −30° | Non dichiarato | Non adottabile; inoltre inadatto sotto mag 14 |
| 2MASS PSC | "Most data served by IRSA is public, with no usage restrictions": https://irsa.ipac.caltech.edu/data_use_terms.html; il "public domain" per cataloghi e immagini compare solo in una mail dell'helpdesk citata nel copyright Debian (fonte secondaria) | Circa 470 M sorgenti, solo J, H, Ks, epoca 1998–2001, senza moti propri | Sì, con il riconoscimento richiesto | Adottabile solo per l'astrometria del solver; inutile per il colore |
| Index astrometry.net serie 4200 (da 2MASS) | File http://data.astrometry.net/4200/LICENSE: testo GPL-2; "or later" solo dal copyright Debian | Index precostruiti | Sì, GPL | Facoltativo; "or later" da confermare |
| SDSS (catalogo) | Pubblico dominio (URL SDSS al punto 1) | u, g, r, i, z; circa 1/3 del cielo; satura a r≈14 | Sì | Adottabile legalmente; non copre tutto il cielo né le stelle di mag 12–14 |
| Yale Bright Star Catalogue | Nessuna licenza alla fonte (HEASARC/ADC) | Circa 9100 stelle fino a V 6,5 | Non dichiarato | Non adottabile e insufficiente |

Stima progettuale del pack, non misurata, con circa 28–32 byte per stella più indice: circa 3 M stelle e 0,1 GB fino a mag 12; circa 20–25 M stelle e 0,6–0,8 GB fino a mag 14.

### 6. RGB broadband con WCS (M3.q) e corpus di accettazione (M3.r)

| Candidato | Licenza e URL | Contenuto | Esito |
|---|---|---|---|
| SDSS frame g, r, i con il catalogo SDSS | Pubblico dominio (URL SDSS al punto 1) | Frame calibrati con WCS (2048×1489) e catalogo fotometrico nello stesso sistema | Adatto a M3.q; campi disgiunti utilizzabili nel corpus |
| Legacy Surveys DR10 coadd g, r, z | CC BY 4.0 per le "images" (URL al punto 2) | Coadd con WCS | Riserva; da confermare la copertura dei FITS |
| ESO EFOSC2, MAXI-J0556-332, B, V, R, I | CC BY 4.0 (ESO) | Grezzi con calibrazioni; solo puntamento, senza soluzione WCS | Corpus e solver |
| Zenodo 3989223 (WASP-74b) | CC BY 4.0 | CMOS amatoriale con difetti reali | Corpus |
| Zenodo 17843622, sub Seestar | CC BY 4.0 | OSC suburbano: gradienti, vignettatura, hot pixel | Corpus, se acquisito |

## Raccomandazioni

1. PUB-REG-01: sequenza ESO EFOSC2 NGC 2808 R#642 (5–20 light più bias e flat R della stessa notte, circa 50–100 MB), CC BY 4.0, con l'header ESO originale conservato e il credito "Based on observations collected at the European Southern Observatory", programma 092.D-0449(A). Riserva: WASP-74b, estratto per Range.
2. PUB-DEEP-01: drz/drc HST ACS di un campo nebulare, ritagliato con un cutout. Lo stack Seestar è più vicino al dominio d'uso, ma richiede di scorrere un tgz da 35 GB.
3. CFA M3.d: sub Seestar di Zenodo 17843622, CC BY 4.0; unico candidato.
4. RAW M3.a: raw.pixls.us con il filtro `License == CC0` file per file e il SHA-256 pubblicato.
5. Pack di catalogo: Gaia DR3 per astrometria e colore (decisione del proprietario, tabella sotto); SDSS e 2MASS restano come riferimenti indipendenti di confronto.
6. M3.q: frame SDSS g, r, i con il catalogo SDSS. M3.r: corpus su campi e frame disgiunti dalle fixture di sviluppo.

## Decisioni del coordinatore

| Ambito | Decisione |
|---|---|
| PUB-REG-01 | ESO EFOSC2, NGC 2808, prog. 092.D-0449(A), filtro R#642. Riserva: Zenodo 3989223 (WASP-74b) |
| RAW M3.a | raw.pixls.us, solo file CC0; esclusi i file CC BY-NC-SA 4.0 |
| CFA M3.d | Zenodo 17843622, sub Seestar (PUB-CFA-01). Acquisizione nei residui M2 (R1, T-M2R1-09); in M3.d solo l'uso per il debayer |
| PUB-DEEP-01 | Cutout HST ACS drz/drc di un campo nebulare, da scegliere in M3.i |
| Cataloghi di colore e astrometria (M3.p, M3.q) | Decisione del proprietario (30 ottobre 2026): il prodotto usa le fonti migliori con licenza certa, non quelle incerte. Gaia DR3 (astrometria e fotometria, comprese le sintetiche GSPC dagli spettri XP) sotto CC BY-NC 3.0 IGO, con l'attribuzione richiesta da ESA; il pack di catalogo è un componente dati separato dal codice GPL e porta la propria licenza. La distribuzione M0–M9 è non commerciale; una release commerciale (M10, già bloccata) richiede prima l'autorizzazione scritta di ESA (data.licences@esa.int). Scartato ATLAS-REFCAT2: licenza CC BY dichiarata ma componenti ereditati incerti. Il coordinatore acquisisce l'estratto (nessun download al proprietario), con record `docs/datasets/PUB-*.md` |
| Solver M3.p | Gaia DR3 per l'astrometria (vedi riga dei cataloghi); 2MASS PSC solo come confronto |
| Colore nel footprint SDSS | Catalogo SDSS (pubblico dominio) |
| Corpus M3.r | Campi disgiunti dalle fixture di sviluppo |
