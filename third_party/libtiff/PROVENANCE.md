# libtiff

- Versione: 4.7.2, ultima release stabile (nella pagina del progetto «Latest Stable Release v4.7.2»); firma del 3 luglio 2026
- URL del tarball: https://download.osgeo.org/libtiff/tiff-4.7.2.tar.gz (firma: https://download.osgeo.org/libtiff/tiff-4.7.2.tar.gz.sig)
- Pagina del progetto: https://libtiff.gitlab.io/libtiff/ (note di rilascio: https://libtiff.gitlab.io/libtiff/releases/v4.7.2.html)
- Data di scaricamento: 2026-11-06
- SHA-256 del tarball: 672bd7d10aee4606171afb864f3570b83340f6a33e2c186dc0512f7145ffdf6a
- Verifica: firma OpenPGP staccata `tiff-4.7.2.tar.gz.sig` valida (`gpg --verify`, chiave RSA B1FA 7D81 EEB8 E663 9917 8B97 33EB BFC4 7B3D D87D, Even Rouault, scaricata da keys.openpgp.org per impronta in un portachiavi temporaneo dentro il progetto poi rimosso; la chiave non è certificata da una firma fidata, quindi l'autenticità del firmatario non è provata oltre la sua impronta). Il sito non pubblica un hash separato.
- Licenza: libtiff license (stile BSD, Sam Leffler e Silicon Graphics), testo in `LICENSE.md` estratto dal tarball senza modifiche; il file contiene anche gli avvisi dei codec interni (tif_lzw.c e altri)
- Uso: solo oracolo indipendente dei test di U13b (segmenti deflate e Predictor 2 delle fixture DNG, contratto U13 §12); collegata solo da eseguibili di test, mai dal prodotto
- Opzioni di build (CMake del tarball, statica, PIC): solo Deflate con la zlib di sistema; senza jpeg, old-jpeg, lzma, zstd, webp, jbig, lerc, libdeflate, C++, strumenti, test, contrib e documentazione
- Modifiche locali: nessuna

Contratto: `docs/contracts/U12-third-party.md`.
