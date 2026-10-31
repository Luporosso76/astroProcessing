# CFITSIO

- Versione: 4.7.0, agosto 2026
- URL del tarball: https://heasarc.gsfc.nasa.gov/FTP/software/fitsio/c/cfitsio-4.7.0.tar.gz
- Pagina di rilascio: https://heasarc.gsfc.nasa.gov/fitsio/
- Data di scaricamento: 2026-10-30
- SHA-256 del tarball: ce573bbea8e75b429f8c3d3e86498741ba3dc9628a1530d2f65268397ad059e8
- Hash pubblicato: nessuno (la directory FTP non elenca file di checksum; fiducia al primo scaricamento via HTTPS)
- Licenza: NASA, testo permissivo in `License.txt` (`licenses/License.txt` del tarball)
- Opzioni di build (CMake del tarball, collegamento statico): `-DBUILD_SHARED_LIBS=OFF`, `-DUSE_CURL=OFF`, `-DUSE_BZIP2=OFF`, `-DUSE_PTHREADS=ON`, `-DTESTS=OFF`, `-DUTILS=OFF`, `-DCMAKE_HAVE_GETHOSTBYNAME=OFF`, `-DCMAKE_HAVE_CONNECT=OFF` (le ultime due impediscono i driver di rete `http`, `https`, `ftp`, `root`)
- Modifiche locali: nessuna

Il tarball è quello upstream invariato; `License.txt` è estratto senza modifiche. Contratto: `docs/contracts/U12-third-party.md`.
