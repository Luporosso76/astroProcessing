# PUB-REG-01: ESO EFOSC2, NGC 2808, filtro R#642, una notte

- Stato: ACQUISITO (2026-11-11). Licenza SPDX: `CC-BY-4.0`.
- Fonte: ESO Science Archive Facility, https://archive.eso.org/. Strumento EFOSC2 al NTT (La Silla), programma `092.D-0449(A)`. Selezione dalla tabella `dbo.raw` del servizio TAP `https://archive.eso.org/tap_obs/sync`, interrogata il 30 ottobre 2026 e ripetuta il 11 novembre 2026 (CSV grezzo e testo della query in `data/public/eso-092.D-0449A/tap-query.csv` e `tap-query.adql`; 59 righe, i 48 `dp_id` del record tutti presenti e nessuno diverso).
- Licenza: ESO Data Access Policy del 7 novembre 2022, https://archive.eso.org/cms/eso-data-access-policy.html, letta il 30 ottobre e riletta l'11 novembre 2026 (testo invariato, «Last Update: 07.11.22»): «All data in the ESO archive retain ESO's copyright» e sono «distributed according to the terms of the Creative Commons Attribution 4.0 International license». Per chi ridistribuisce: «Credit to the ESO provenance of the data must be acknowledged, and the file headers preserved». Le calibrazioni sono «public immediately after the observations»; i dati scientifici dopo il periodo proprietario. Colonna `release_date` del TAP: 2014-02-23 per i 22 file della sera, 2014-02-24 per gli 11 della notte avanzata (flat di cielo e bias del mattino), 2015-02-24 per i 15 light; tutte scadute all'acquisizione. Non è un parere legale.
- Attribuzione: «Based on observations collected at the European Southern Observatory under ESO programme 092.D-0449(A)».
- Download: `https://dataportal.eso.org/dataPortal/file/<dp_id>` (URL `access_url` del TAP), senza autenticazione, HTTP 200, uno alla volta, nome dal `Content-Disposition`: `<dp_id>.fits.Z` (compress Unix, `.Z`). I 48 file pesano 68880107 byte compressi e 102850560 decompressi (2142720 byte ciascuno: 7 blocchi di header e 737 di dati, BITPIX 16, nessuna estensione). I FITS decompressi sono conservati in `data/public/eso-092.D-0449A/fits/`, accanto ai file consegnati: i test leggono quelli e ne verificano lo SHA-256.
- Header: i file si conservano come consegnati dall'archivio; nessun test li riscrive. Un estratto, se servirà, conserva l'header originale intero.
- Rilascio: l'archivio non ha versioni; l'identità di ogni file è il suo `dp_id`.
- Hash: l'archivio non pubblica uno SHA-256 per file. Gli SHA-256 sono calcolati sul file consegnato (`.fits.Z`) e sul FITS decompresso (`gzip -dc`), blocco in fondo. Integrità indipendente dal progetto: tutti i 48 header portano `DATASUM` e la somma di 32 bit dei blocchi di dati lo conferma per 48 su 48; `CHECKSUM` è assente in tutti. **Gli hash non sono riproducibili da un nuovo download**: l'archivio riscrive l'header a ogni richiesta (cartellino `COMMENT processed by hotfly ... on <data e ora UTC>`); verificato con un secondo download di `EFOSC.2014-02-24T01:24:23.691`: SHA-256 del `.Z` diverso, pixel uguali (stesso `DATASUM`). L'hash del record identifica la copia acquisita l'11 novembre 2026, non il `dp_id`; l'identità dei dati di pixel è `DATASUM`.
- Copia locale: `data/public/eso-092.D-0449A/` (esclusa da git), un file `<dp_id>.fits.Z` per `dp_id`, più `tap-query.csv` e `tap-query.adql`.
- Uso: T-REG-02 (M3.f), registrazione e stacking nello stesso filtro (f, g, h), E2E pubblico di M3.s e benchmark B1, Gate M3 punto 1; hint reali (puntamento negli header) per M3.p.

## File scelti

Notte 23–24 febbraio 2014 (UTC). Conteggi dalla risposta TAP filtrata per strumento `EFOSC`, intervallo `2014-02-23T12:00` – `2014-02-24T12:00`, `dp_type = 'BIAS'` oppure `filter_path = 'R#642'`: 21 bias, 6 flat a cupola R#642, 6 flat di cielo R#642, 26 `OBJECT` R#642 di cui 15 da 150 s. Si acquisiscono i 15 light da 150 s di NGC 2808 e tutte le calibrazioni della notte nello stesso filtro, 48 file; circa 1,9 MB ciascuno secondo il record di ricerca (`docs/datasets/M3-candidates.md`), stima non misurata.

| Ruolo | `dp_id` | Esposizione s |
|---|---|---|
| light NGC 2808 | `EFOSC.2014-02-24T01:24:23.691`, `T01:27:33.994`, `T01:30:44.457`, `T02:06:54.865`, `T02:10:06.218`, `T02:13:16.732`, `T03:02:44.406`, `T03:05:54.650`, `T03:09:05.073`, `T04:02:13.533`, `T04:05:25.387`, `T04:08:35.971`, `T05:02:30.327`, `T05:05:41.751`, `T05:08:52.194` (tutti `EFOSC.2014-02-24`) | 150 |
| bias, sera | `EFOSC.2014-02-23T20:17:37.847`, `T20:18:08.429`, `T20:18:38.861`, `T20:19:09.463`, `T20:19:40.074`, `T20:20:10.836`, `T20:20:40.748`, `T20:21:11.390`, `T20:21:41.881`, `T20:22:12.483`, `T20:22:42.875` (tutti `EFOSC.2014-02-23`) | 0 |
| bias, mattina | `EFOSC.2014-02-24T10:04:57.700`, `T10:05:50.550`, `T10:06:43.450`, `T10:07:36.231`, `T10:08:28.841`, `T10:09:20.971`, `T10:10:13.762`, `T10:11:06.542`, `T10:11:59.362`, `T10:12:52.142` (tutti `EFOSC.2014-02-24`) | 0 |
| flat a cupola R#642 | `EFOSC.2014-02-23T20:12:34.990` (1,0 s); `T20:13:10.203`, `T20:13:57.121`, `T20:14:43.649`, `T20:15:30.537`, `T20:16:16.776` (12,41 s) | 1,0; 12,41 |
| flat di cielo R#642 | `EFOSC.2014-02-23T23:56:12.538` (1,0 s), `T23:56:48.712` (12,0), `T23:57:35.280` (14,2), `T23:58:24.080` (17,1), `T23:59:16.309` (20,8), `EFOSC.2014-02-24T00:00:11.641` (25,6) | 1,0–25,6 |

Nessun dark nella risposta TAP per quella notte: la calibrazione disponibile è bias e flat. La scelta della ricetta (D-16) la fa l'analisi G-CAL sui file acquisiti, non questo record.

Esposizione e filtro letti dagli header (`EXPTIME`, `HIERARCH ESO INS FILT1 NAME`); tutti i 48 file hanno `NAXIS1 = NAXIS2 = 1030`. Tabella per ruolo (valori uguali in tutti i file del gruppo, salvo dove indicato):

| Gruppo | File | EXPTIME s | Filtro | Bin | Gain e-/ADU (`OUT1 GAIN`) | RON e- | Lettura | WCS |
|---|---|---|---|---|---|---|---|---|
| light | 15 | 150,0005–150,0006 | R#642 | 2×2 | 0,91 | 7,80 | `fastL` | `RA---TAN`/`DEC--TAN`, 3 puntamenti da 5 |
| bias sera | 11 | 0 | Free | 2×2 | 0,91 | 7,80 | `fastL` | no |
| bias mattina | 10 | 0 | Free | 2×2 | 0,93 | 7,20 | `normL` | no |
| flat cupola | 6 | 1,0005; 12,4105–12,4106 | R#642 | 2×2 | 0,91 | 7,80 | `fastL` | no |
| flat cielo | 6 | 1,0005; 12,0005; 14,2006; 17,1006; 20,8005; 25,6006 | R#642 | 2×2 | 0,91 | 7,80 | `fastL` | no |

- Geometria: finestra 1030×1030 su CCD 2048×2048 (`ccd40`, LORAL, pixel 15 µm), un'uscita, `PRSCX = 6`, `OVSCY = 6`, `BZERO = 32768`, `BSCALE = 1`, BITPIX 16 (uint16). Strumento `EFOSC`, telescopio `ESO-NTT`.
- Light, WCS reale: `CTYPE1/2 = RA---TAN/DEC--TAN`, `CRPIX1 = 509,550`, `CRPIX2 = 493,725`, `CD1_1 = CD2_2 = 6,688889E-05` deg/pixel (0,2408 arcsec/pixel), `CD1_2 = CD2_1 = 0`, `EQUINOX 2000`, `RADECSYS FK5`. `CDELT1/2 = 2,0` sono il binning, non una scala. Puntamento (`RA`, `DEC`, uguali a `CRVAL1/2` a meno di 1e-6): `137,897375 -64,90331` (5 file), `137,898682 -64,90276` (5), `137,896068 -64,90387` (5): dithering di circa 3 arcsec.
- Calibrazioni: `CTYPE1/2 = PIXEL`, `CRVAL = 1`, `CRPIX = 0,5`, `CDELT = 2`: nessun WCS celeste. `RA`/`DEC` dei bias, dei flat a cupola e dei flat di cielo sono il puntamento del telescopio in quel momento (`354,693907 -23,33517` sera, `42,55 -19,54` cielo, `245,875 -53,3486` mattina), non un campo.
- Il campo 1030×1030 con pixel binnato 0,24 arcsec copre circa 4,1 arcmin di lato.

Hash SHA-256 nel formato `<sha256>  <percorso relativo a data/public/eso-092.D-0449A/>`, prima i file consegnati, poi i FITS decompressi (nome senza `.Z`; conservati in `fits/` accanto ai file consegnati, che i test leggono e verificano):

```
fe2255884088716bced606cca5e93d5579f135c39ef2d421c4d920ea839e414a  EFOSC.2014-02-23T20:12:34.990.fits.Z
49a156c4460b82af288ec9b446423c971fbd41fd8a0b9170326f4de894e0c647  EFOSC.2014-02-23T20:13:10.203.fits.Z
13944e8c47d4796490fea7e6447365b60b64a732f1d5842553ec163011a4e9ac  EFOSC.2014-02-23T20:13:57.121.fits.Z
e435ffc65054c8b102c1c67bd761781fed72f25b0e0b0152ec97bf75699c12d8  EFOSC.2014-02-23T20:14:43.649.fits.Z
f1231902a16f808fec2db3c02f5876caef01c6e94be48a857288fbaf367de976  EFOSC.2014-02-23T20:15:30.537.fits.Z
727e781e526b6cd8f87ca0176674309264d4c9216e034fc30ed18b1949c65fd9  EFOSC.2014-02-23T20:16:16.776.fits.Z
7dbfe82993120fd629697e0292c81ed99075a95cbcc9310beb4051879ef8c6fc  EFOSC.2014-02-23T20:17:37.847.fits.Z
d85517ed7821c1c9ab1fdbe6b72503826152d73aeae643794c78d21bec1847b4  EFOSC.2014-02-23T20:18:08.429.fits.Z
dbd9356e1ea8a9736819c69d4db28cbaa958220664bd8cc6c70621619a961969  EFOSC.2014-02-23T20:18:38.861.fits.Z
08422f1323a1ffddfe6b1026ca5e1c3490c6de03a56c7e0bfb243432732a026a  EFOSC.2014-02-23T20:19:09.463.fits.Z
31272e39153821631fb8b8d9e81b65a521a9d740473195b71b0ed9565bf3f09c  EFOSC.2014-02-23T20:19:40.074.fits.Z
0fff0adf61cca2c7f15bff754e49ac09e37fd495760e0cbc50ad203fe0cff5fd  EFOSC.2014-02-23T20:20:10.836.fits.Z
b5d2bbb534bb901cc72d16e8ddac84bf000cb3e3c2911484731b69bafddf15a9  EFOSC.2014-02-23T20:20:40.748.fits.Z
aa800a05bd6823365573b869dbd5eb418ad80f74816906734d050724607fb62f  EFOSC.2014-02-23T20:21:11.390.fits.Z
71c461add9479547095346d13a2a183f280f4a9badcdb1882f59140adb0512f1  EFOSC.2014-02-23T20:21:41.881.fits.Z
925d224cc34d3f36f0f1b98006dd64fc2bf971dffdd67023aa5567c7a9fe05e0  EFOSC.2014-02-23T20:22:12.483.fits.Z
212bbb2b247d3d25045ed14f7c19ec800c7fbe427a0e9f63d902ec32ef70c854  EFOSC.2014-02-23T20:22:42.875.fits.Z
e0c2572dec25e73efdbe0d5135faf8f3e7f88a191463fdf6d1588910c5f25f71  EFOSC.2014-02-23T23:56:12.538.fits.Z
0d5d92d81a57958bfd3b1852c6432f364865ae4d882a34973c73003dcaf272fd  EFOSC.2014-02-23T23:56:48.712.fits.Z
14c27ee135cc0e0c96c5b6674e7dc7eaff269702789b38c9785388a0e97d948a  EFOSC.2014-02-23T23:57:35.280.fits.Z
e9757203ba368bbe931df612d05587be4f398e07cc4c2f5d8c1062069eaabae2  EFOSC.2014-02-23T23:58:24.080.fits.Z
c6af0c9076c5cffefadd8bd81815722a0928a7d398e65a867bd932482619971e  EFOSC.2014-02-23T23:59:16.309.fits.Z
e049b2cae37ed4f99cd31d8f72e80a62348a3fb88846124b81bcb41de80412b3  EFOSC.2014-02-24T00:00:11.641.fits.Z
804c793d1575dc46263f385105dfddb622cf2d75e27f70ee2eb837dc4d924a2b  EFOSC.2014-02-24T01:24:23.691.fits.Z
91db563a811bdc6afd1be9c0ac7f0b9f643dc773abc5cd3fc62a65458c58eaf8  EFOSC.2014-02-24T01:27:33.994.fits.Z
1a5f9aa8618de8aff90eee5222b608debd66d54832f780f1d1d1b9df772b1090  EFOSC.2014-02-24T01:30:44.457.fits.Z
3b1fff91ee4316f35d2a79f73b6fdb6a79fa82ca166ff3738b11fe73e6574a4e  EFOSC.2014-02-24T02:06:54.865.fits.Z
210e5fbffb03c3e37cc612d7aa907937f2ed97880acef4f8a9c34ab854557a72  EFOSC.2014-02-24T02:10:06.218.fits.Z
552963bd3955c2c3e735223218e1a61e4a947004218151f75807e9d01254aa3f  EFOSC.2014-02-24T02:13:16.732.fits.Z
0196ff3012536d9ed08c643516b7fc2e7a75e490165d153841d092b25edc3a67  EFOSC.2014-02-24T03:02:44.406.fits.Z
726b1e9f66646f26331044271c5296627b9abaac8b4f1fc40695e9821a4a1a73  EFOSC.2014-02-24T03:05:54.650.fits.Z
3c7b004cf6af34fa710bd7af145e47e18fb9412a91aa58c8858b6a4311f306f8  EFOSC.2014-02-24T03:09:05.073.fits.Z
8147e6cdfac15758fe0116b3369a5fa865b7bbdc2661d17e7feee44dbc6b7b68  EFOSC.2014-02-24T04:02:13.533.fits.Z
a0a8e2bd887b4ae2dbb46dec407ec304ee3bba5028296d31c609912bd1602e02  EFOSC.2014-02-24T04:05:25.387.fits.Z
555f1099cae4dcaf7da0f828f0fb7be422b82fcd80e93f3217af68ca9e301004  EFOSC.2014-02-24T04:08:35.971.fits.Z
9788f73eb91381c81e9fa801914a06d0563e3292e374dedb65ff1ae708780195  EFOSC.2014-02-24T05:02:30.327.fits.Z
7660e292980aeb5782f60e60eddb3daa6306bad29596df8d372d8f674bf74066  EFOSC.2014-02-24T05:05:41.751.fits.Z
efab52253265b0661d0c4787cb49e790ef037a96f616e7700632bc2ff0fdd0c3  EFOSC.2014-02-24T05:08:52.194.fits.Z
961f35d347c1ab91f55c70f43d13fe449574e62c51c3b709761e30831581af39  EFOSC.2014-02-24T10:04:57.700.fits.Z
55b47b3248e169b39c1a5e2fe8948ffa9ba7581841a50691b7be920e3a493f8c  EFOSC.2014-02-24T10:05:50.550.fits.Z
110d0b5f5c4e38ab98b00f1e57f130618510dc247924ed3c95fde2375bf9d8ed  EFOSC.2014-02-24T10:06:43.450.fits.Z
8522c2778b8228ead1023bb8d7fc92f8b8adf6e137d6a6092b60d2c816becbca  EFOSC.2014-02-24T10:07:36.231.fits.Z
7deac81c4e25411699537984d5e6c43ab163ccfcdb51f008f5b6d3ab050be783  EFOSC.2014-02-24T10:08:28.841.fits.Z
01dcce3baf0e825fca5cea2408ae2093dbd97f436d51362801b93b6bd82eaa97  EFOSC.2014-02-24T10:09:20.971.fits.Z
6a06c42d9ae4abb6a130334d465f2b785fe6e997a8b51b77a5a693c8d8011d48  EFOSC.2014-02-24T10:10:13.762.fits.Z
b85e82d56a495e707835644b25d2b31116599c04543eab4731d7a79bcbdff1b7  EFOSC.2014-02-24T10:11:06.542.fits.Z
9c736ad609281893e47cad0b63ea0d3ac10e5f90a002c965f6311b4412aefd0e  EFOSC.2014-02-24T10:11:59.362.fits.Z
c4ef77ccdf9f16ce846b01f6e594569ee869097a870043b14893ad0696416366  EFOSC.2014-02-24T10:12:52.142.fits.Z
db19448708cc57e04902cd22c701808b6eb7230f7c7072331021586b735871cb  tap-query.adql
da57885a88f7cc3ed8dd4a0a2f0020f57fef7c2908668dc3e5953216d3558cc9  tap-query.csv
592db7373bb56bba0ad1a2bff3568f2c95b14f854a669d2cbcb512e7124d9b3b  fits/EFOSC.2014-02-23T20:12:34.990.fits
dc3772dca7ec924c635403a1fb621c9a0280d01f2cf8d10cf49450525499c232  fits/EFOSC.2014-02-23T20:13:10.203.fits
03cd25e919cd1ee53e855452405f3cded9237a703c2c17a0d84dd3c75275b3e4  fits/EFOSC.2014-02-23T20:13:57.121.fits
2e0db1e1253bc994096c91a6497c2a46e4560462930ab246305f5c62060d3cd5  fits/EFOSC.2014-02-23T20:14:43.649.fits
2986a5ec4809ac26a5ef640c1deaa0a13109570ddf668d4ecb4024c35f69be01  fits/EFOSC.2014-02-23T20:15:30.537.fits
a12102e4e15ed16ac374da4e39dcb93127af69a5b964faac694ad6ecbe5dff32  fits/EFOSC.2014-02-23T20:16:16.776.fits
7538e1c49e3aecb2c123db2ef9f1a1b25be752878b35c2b3c027332b82fdf04c  fits/EFOSC.2014-02-23T20:17:37.847.fits
e4f4ea7e3b70fc1c0cc30051b301f98d21f32642f270bbe04f25cfeaf7057d9a  fits/EFOSC.2014-02-23T20:18:08.429.fits
2e4ebf635255fdd0185b9ca5735c653cd57093731fa6efffdd9a4a81b09c8961  fits/EFOSC.2014-02-23T20:18:38.861.fits
6bb24c5e0a4fba686bdc1c83cec58ccbcff6a8df7af032e793623cbbf9c92f01  fits/EFOSC.2014-02-23T20:19:09.463.fits
4da00599872e8b7601ea2fe408633059de01f340066f498a5e947a843376e562  fits/EFOSC.2014-02-23T20:19:40.074.fits
53db67656a02526e520632c7dfe8a39ce993d8b655498d4d21cc8fff58814d38  fits/EFOSC.2014-02-23T20:20:10.836.fits
8ac7150d8ac6e6d85e6902f6dc219585d0cee09aa9710de44765bfab7dd3ca42  fits/EFOSC.2014-02-23T20:20:40.748.fits
73ae2444905cafc57bea1315abf7332b326ef5a38a6e82118858f73c6f195ce8  fits/EFOSC.2014-02-23T20:21:11.390.fits
26e2769815d7de17b166d75a817cc4ade2f71ecad4d828ded57a5d0740c665c5  fits/EFOSC.2014-02-23T20:21:41.881.fits
bad318f438cd175788f9ec5fff838437ab6774c71aec28e6ec612166a79bdd24  fits/EFOSC.2014-02-23T20:22:12.483.fits
c1294a43e0c7ce191eb0b4674846768ab628f689c8b3c2674a0ac14a0482231a  fits/EFOSC.2014-02-23T20:22:42.875.fits
12254d2e040543102c4a69a1a8b02525c152b955a919abbca44ea73589840e00  fits/EFOSC.2014-02-23T23:56:12.538.fits
f4e76ed8e9cd649fede4cbedd344695ea628555c954dcc21ed5b43c00e3d440f  fits/EFOSC.2014-02-23T23:56:48.712.fits
e4b52fd8a76f2422fc277ce8c1f51e2c3fefc38f140354b9aa3902f0e3fbf311  fits/EFOSC.2014-02-23T23:57:35.280.fits
68aedd58b880d24cca34792dd30a4d7ac93fd009dde99e1ffa6b7013214c02dd  fits/EFOSC.2014-02-23T23:58:24.080.fits
472d8ba80f12f782aa2633b1f3a1a1940c4e74f5c704a4f270cb7f67c6f02921  fits/EFOSC.2014-02-23T23:59:16.309.fits
30508391c817bdd4ab5e596f9d777f23f5c0a9be7d5e03573378894159e714c5  fits/EFOSC.2014-02-24T00:00:11.641.fits
85186733cb18e1430cf4950b49ff1b5cbdc4678d3ce20ada05aa221a26d49bb0  fits/EFOSC.2014-02-24T01:24:23.691.fits
bebccbe8b9fa19c9171fb1a69d70033e2a57bbea51fc8e3f0ebb066e32ee83da  fits/EFOSC.2014-02-24T01:27:33.994.fits
029da6ea009a0b0e5cef276c943d75622b579eb9258c7013d033b60dfc553f43  fits/EFOSC.2014-02-24T01:30:44.457.fits
620ca74d9ea249ac9d2bfae220a0c9252e299a8164aa8bd596acc46504f11d1b  fits/EFOSC.2014-02-24T02:06:54.865.fits
ecb9f5652d2711dd539517058004d40bf13fde3caf3fd84a6b44d841d7b305be  fits/EFOSC.2014-02-24T02:10:06.218.fits
d54ceb634f5738c297de5ee957c6a5f8ee41b04c943fbd22afb15e657b279dd7  fits/EFOSC.2014-02-24T02:13:16.732.fits
cc94db2f15d4ba0c430002d1a6fec8066b5d76c605b11123f16150e820dc4c96  fits/EFOSC.2014-02-24T03:02:44.406.fits
71ebc755f72faf0859c5df82b92d969d9e79341553cc6a3a466e1c29cf9c5c37  fits/EFOSC.2014-02-24T03:05:54.650.fits
5bc0cdde1b11f1766591454ff906ceb6c67554ceddcd5d4197d9ca8becd6f56f  fits/EFOSC.2014-02-24T03:09:05.073.fits
2d1fb2de66cfc713d6f7e5bebc6d805574ffcb84749ad7b4ad4fdc9c2ad6bfc0  fits/EFOSC.2014-02-24T04:02:13.533.fits
29d652d004311db7739a1d3f4a80727485477e0dc48c7ec0de09cd845cb15c82  fits/EFOSC.2014-02-24T04:05:25.387.fits
122ad84f5b939655001cb4daa44f22bcec7faa5ea1ab9d5b0b4b5b23366f2674  fits/EFOSC.2014-02-24T04:08:35.971.fits
77b6a7b20ce8323732662591c64fddb1dffd91e675b25a9f024a6dbff18a510f  fits/EFOSC.2014-02-24T05:02:30.327.fits
058a1b09159aa7ecbcd789de5913c316565a6ef8e71b12209f5d7cd6bfcd2dc7  fits/EFOSC.2014-02-24T05:05:41.751.fits
602364b7a92160dc3c12aa0d46d3b45c7dee27fbf6bce88373d62c693dac55bf  fits/EFOSC.2014-02-24T05:08:52.194.fits
df4b41c8fb4642b9ec9156306e52958e23377f5d38dc2c15f267c418138954df  fits/EFOSC.2014-02-24T10:04:57.700.fits
8f889dcbb08b8ce48f90567d1c0d6fcd5d1c119256481c114965fa68db5ff661  fits/EFOSC.2014-02-24T10:05:50.550.fits
0a5342fcbfa495a6fe8e9ee5bcd6d90c056ce23b2761ed3e46fa945babccf2c1  fits/EFOSC.2014-02-24T10:06:43.450.fits
6a28f1541efd90da276f19e08824e5a92c80fb5dbf8b692d20ef24bb5c5d8aa6  fits/EFOSC.2014-02-24T10:07:36.231.fits
bbec6a9e0f3e9a7f108dd25b53f50f96e087ffffe51e58351d815257d58f9428  fits/EFOSC.2014-02-24T10:08:28.841.fits
e1de58fe3fc494e19533f161c9feed544e10bc6c9b7ee8381fe5c769073a601a  fits/EFOSC.2014-02-24T10:09:20.971.fits
34dd690e42caf64d32d04e2e14e77955890020b22fb539e64769456abbdb6d26  fits/EFOSC.2014-02-24T10:10:13.762.fits
b3a2516b816261bf32e9462e28697c94d5ddc43ce8b1e2044fed3557c0d00a29  fits/EFOSC.2014-02-24T10:11:06.542.fits
2bea3a3c6eb8e1f98f0ca9571b574d94c80403cdde0e2e9bfe91d5b2024560ae  fits/EFOSC.2014-02-24T10:11:59.362.fits
40b49dc571f11670ed0485104eada29bb8970af3d7bf7a996968045afddd2224  fits/EFOSC.2014-02-24T10:12:52.142.fits
```

## Saturazione

`saturation_adu` = 65535 nelle unità del file (BITPIX 16, `BZERO` 32768, `BSCALE` 1). Fonte: ESO, pagina «CCD#40 on EFOSC2», `https://eso.org/sci/facilities/lasilla/instruments/efosc/inst/Ccd40.html`, letta l'11 novembre 2026: «The CCD has a saturation limit of 65535 ADU (dynamic range of 16 bits ADC) which is considerably lower than the actual well saturation corresponding to 80000 ADU». L'header dei frame (`HIERARCH ESO DET CHIP1 ID` = `ccd40`, lettura `normal`, binning 2) non ha `SATURATE` né `DATAMAX`: il valore si dichiara nella ricetta di calibrazione. Controllo sul dato: il massimo di tre light e dei flat da 12-25 s è 65535.

## Limiti

- L'elenco dei `dp_id` era stato letto attraverso uno strumento di consultazione web che riassume il CSV. La query ripetuta l'11 novembre 2026 sul CSV grezzo ha confermato i 48 `dp_id` (59 righe totali: 21 bias, 6 flat a cupola, 6 flat di cielo, 26 `OBJECT`; i 48 sono un sottoinsieme esatto). Il CSV è conservato accanto ai file. Il testo ADQL salvato è quello usato per il CSV: filtri `instrument`, `date_obs`, `dp_type`, `filter_path` della descrizione del record.
- I 10 bias del mattino (`EFOSC.2014-02-24T10:*`) non sono del programma `092.D-0449(A)` ma di `59.A-9002(A)` (calibrazione), e sono letti con `READ SPEED = normL`, gain 0,93 e RON 7,20, mentre light, flat e bias della sera usano `fastL`, 0,91, 7,80. I bias del mattino non hanno la stessa modalità di lettura dei light: la scelta dei bias per il master è dell'analisi G-CAL, non di questo record.
- Gli hash non sono riproducibili da un nuovo download (header riscritto dall'archivio a ogni richiesta, vedi «Hash»): la ripetizione dell'acquisizione produce hash diversi con pixel identici.
- Lo stesso programma ha 73 righe NGC 2808 R#642 in quattro notti (24–27 febbraio 2014) secondo la risposta TAP del 30 ottobre; il record di ricerca ne riportava 67 light da 150 s. La differenza non è risolta: questo record usa una sola notte e non dipende dal totale.
- Canale di download provato l'11 novembre 2026: portale dati ESO, file pubblici senza autenticazione, 48 su 48 HTTP 200. I file arrivano compressi `.Z`.
- Ammasso globulare affollato: la registrazione lavora su un campo denso; è una proprietà del dato, dichiarata.
- I flat da 1,0 s possono essere esposizioni di prova; la loro esclusione, se serve, la decide l'analisi dei master, non questo record.
