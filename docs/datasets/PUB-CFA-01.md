# PUB-CFA-01: Seestar S30, sub CFA di (4) Vesta (Zenodo 17843622)

- Fonte: Zenodo, DOI 10.5281/zenodo.17843622, https://zenodo.org/records/17843622, titolo «Seestar S50 vs Seestara S30 observational data for comparison», autore Markus Pössel, pubblicato il 6 dicembre 2025. File del record: `S50vsS30comparisonFiles.tgz`, 35016942317 byte, MD5 pubblicato `cf661de6bec6d285a47fc86dd137d11a`.
- Licenza: Creative Commons Attribution 4.0 International, dichiarata nel record (campo `metadata.license.id = cc-by-4.0`, letto dall'API del record il 29 ottobre 2026). Ridistribuibile con attribuzione. Attribuzione: Markus Pössel, «Seestar S50 vs Seestara S30 observational data for comparison», Zenodo, doi:10.5281/zenodo.17843622. Non è un parere legale.
- Acquisizione: solo ciò che serve. Il `.tgz` non si è scaricato per intero. Lo script `tmp/r1/fetch_cfa.py` (non versionato) legge il flusso HTTPS con `tarfile` in modalità stream (`r|gz`), estrae il primo membro che sta in una cartella `*_sub/` ed è un `.fit` e si ferma. Letti 2 969 600 byte compressi su 35 016 942 317, 20 membri visti. Membro estratto: `VergleicheS50vsS30/S30/Vesta_sub/Light_Vesta_10.0s_IRCUT_20250531-013040.fit`, 4 152 960 byte. Il flusso è stato interrotto alla chiusura del membro; il MD5 dell'archivio completo non è stato verificato, perché l'archivio non è stato scaricato. Il controllo del campione è lo SHA-256 sotto, registrato prima del test.
- Copia locale: `data/public/zenodo-17843622/first_sub.fits` (esclusa da git; il nome è fisso, quello originale è nel membro sopra). Il campo `TELESCOP` dell'header contiene un identificativo del dispositivo: il file non va ridistribuito dal repository.
- Contenuto: FITS mono-HDU, BITPIX 16, `BSCALE = 1`, `BZERO = 32768`, 1080×1920, `BAYERPAT = 'GRBG'`, `IMAGETYP = 'Light'`, `FILTER = 'IRCUT'`, `EXPTIME = 10`, `INSTRUME = 'Seestar S30'`, `GAIN = 200`, `XBINNING = YBINNING = 1`. L'header non ha `XBAYROFF`, `YBAYROFF`, `BUNIT`, `LINEAR`: l'import dichiara gli offset CFA (0, 0) e la fase (`raw`); la linearità resta `unknown`.

```
760b9aa547c9192919749cd0bdb93b034a72787ef081bb558713b3b653aedd06  first_sub.fits
```

Limiti: un solo sub, nessun dark o flat dichiarato; la fase `raw` e gli offset CFA sono dichiarazioni dell'import, non attestazioni della fonte; il pattern GRBG è quello dell'header di questo dispositivo e non si estende ad altri sub (verifica per camera in M3.d). Uso in R1: import, inventario, lettura di `BAYERPAT`, roundtrip bit-esatto della data unit (T-M2R1-09). Il debayer è fuori da M2 (M3.d).
