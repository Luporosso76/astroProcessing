# Inventario licenze delle dipendenze

Versioni e licenze lette dal sistema (28 ottobre 2026): pacchetto trovato con `dpkg -S` sul file `.so`/`.pc`, licenza da `/usr/share/doc/<pacchetto>/copyright`. Obiettivo di licenza del prodotto: GPL-3.0-or-later (D-02, ADR-12). Questo non e' un parere legale.

| Dipendenza | Versione | Pacchetto | Licenza (copyright Debian) | Compatibile GPL-3.0-or-later | Uso |
|---|---|---|---|---|---|
| CFITSIO | 4.6.3 | libcfitsio-dev | Testo HEASARC permissivo, tag Debian `NOSA` ma non NASA Open Source Agreement 1.3 ( uso, copia, modifica, redistribuzione senza costo con avviso); file accessori X11, GPL-2+/GPL-3+ con eccezioni | Si: permissiva | core (I/O FITS) |
| SQLite | 3.46.1 | libsqlite3-dev | Pubblico dominio (autore D. R. Hipp) | Si: nessun vincolo | core (store) |
| OpenSSL (libcrypto) | 3.5.5 | libssl-dev | Apache-2.0 | Si con GPL-3; no con GPL-2-only (il prodotto e' GPL-3.0-or-later) | core (SHA-256) |
| FFTW3 (double, float) | 3.3.10 | libfftw3-dev | GPL-2+ | Si: GPL-2+ include GPL-3; impone GPL al prodotto distribuito | core (FFT) |
| Eigen | 3.4.0 | libeigen3-dev | MPL-2 (parti BSD-3-clause, LGPL-2.1+ con alternativa MPL-2) | Si: MPL-2 compatibile con GPL-3 | non ancora collegata; candidata per il core |
| LibRaw | 0.21.5 | libraw-dev | LGPL-2.1 o CDDL-1.0 (a scelta); documentazione CC-BY-SA-3.0 | Si: LGPL-2.1 collegabile da codice GPL-3 | non ancora collegata; ingresso RAW del core |
| libtiff | 4.7.0 | libtiff-dev | Hylafax (permissiva, stile BSD) | Si: permissiva | non ancora collegata; I/O TIFF |
| Little CMS 2 | 2.17 | liblcms2-dev | MIT (parti IJG, GPL-2+/GPL-3 per contributi accessori) | Si: MIT | non ancora collegata; gestione colore |
| Qt | 6.10.2 | qt6-base-dev | LGPL-3 o GPL-2 (moduli); GPL-3 con eccezione Qt; BSD-3-clause, Expat per parti | Si: Qt sotto GPL-3/LGPL-3 | solo GUI; non collegata al core |
| GoogleTest | 1.17.0, copia in `third_party/googletest` | googletest (copia) | BSD-3-clause | Sì: permissiva | solo test |

Note:
- Nessuna incompatibilita' con GPL-3.0-or-later trovata. L'unico vincolo da rispettare: OpenSSL (Apache-2.0) non e' compatibile con GPL-2-only, quindi la licenza non deve mai scendere a GPL-2-only.
- GoogleTest si compila dalla copia interna `third_party/googletest` (provenienza e hash in `PROVENANCE.md` e `SHA256SUMS`); il build non legge le installazioni di GoogleTest del sistema.
