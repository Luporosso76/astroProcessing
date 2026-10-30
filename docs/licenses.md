# Inventario licenze delle dipendenze

Versioni e licenze lette dal sistema (28 ottobre 2026; hash e moduli aggiunti il 29 ottobre 2026, NFR-LIC-01.c): pacchetto trovato con `dpkg -S` sul file `.so`/`.pc`, licenza da `/usr/share/doc/<pacchetto>/copyright`. Obiettivo di licenza del prodotto: GPL-3.0-or-later (D-02, ADR-12). Questo non è un parere legale.

Regola (U12, 30 ottobre 2026; `plans/M3a.md`, U12): la licenza di ogni dipendenza diretta si legge dal file di licenza upstream, mai dal `copyright` di un pacchetto Debian. Dipendenza diretta: una libreria compilata nel progetto o una libreria di sistema dichiarata in `NEEDED` di un eseguibile del prodotto o di `libastrocore`. Per una libreria compilata nel progetto il file è quello del tarball in `third_party/<nome>/` (copiato invariato accanto al tarball, con SHA-256 in `SHA256SUMS`); per una libreria diretta di sistema è il file di licenza del rilascio upstream della versione installata, scaricato dal sito ufficiale, con URL e SHA-256. La sezione «Librerie compilate nel progetto» applica già la regola. Le righe delle librerie dirette di sistema nelle tabelle seguenti (SQLite, OpenSSL `libcrypto`, FFTW, libtiff, zlib, libjpeg-turbo, oltre alle librerie di runtime del compilatore e di glibc) riportano ancora la licenza dal `copyright` Debian: rileggerle alla fonte upstream è l'obbligo 12 del contratto U12, da chiudere con la sua implementazione. Il `copyright` Debian resta la fonte ammessa solo per le dipendenze transitive. Esempio della differenza: il `copyright` Debian di `libraw23t64` dà LibRaw come LGPL-2.1 «or (at your option) any later version», mentre il `COPYRIGHT` upstream di LibRaw 0.22.3 dice «GNU LESSER GENERAL PUBLIC LICENSE version 2.1», senza «or later».

## Dipendenze di progetto

| Dipendenza | Versione | Pacchetto | Licenza (copyright Debian) | Compatibile GPL-3.0-or-later | Uso |
|---|---|---|---|---|---|
| CFITSIO | 4.6.3 | libcfitsio-dev | Testo HEASARC permissivo, tag Debian `NOSA` ma non NASA Open Source Agreement 1.3 ( uso, copia, modifica, redistribuzione senza costo con avviso); file accessori X11, GPL-2+/GPL-3+ con eccezioni | Sì: permissiva | core (I/O FITS), solo funzioni da memoria |
| SQLite | 3.46.1 | libsqlite3-dev | Pubblico dominio (autore D. R. Hipp) | Sì: nessun vincolo | core (store) |
| OpenSSL (libcrypto) | 3.5.5 | libssl-dev | Apache-2.0 | Sì con GPL-3; no con GPL-2-only (il prodotto è GPL-3.0-or-later) | core (SHA-256) |
| FFTW3 (double, float) | 3.3.10 | libfftw3-dev | GPL-2+ | Sì: GPL-2+ include GPL-3; impone GPL al prodotto distribuito | solo test (`test_deps`); candidata per il core |
| Eigen | 3.4.0 | libeigen3-dev | MPL-2 (parti BSD-3-clause, LGPL-2.1+ con alternativa MPL-2) | Sì: MPL-2 compatibile con GPL-3 | non ancora collegata; candidata per il core |
| LibRaw | 0.21.5 | libraw-dev | LGPL-2.1-or-later o CDDL-1.0 (a scelta; si sceglie LGPL); `RawSpeed/rawspeed_xmldata.cpp` CC-BY-SA-3.0, ritenuto assente dal binario (ipotesi, sezione «NFR-LIC-01.d») | Sì: LGPL-2.1-or-later collegabile da codice GPL-3 | collegata dal core da U2 di M3.a (`decode_raw_buffer`); decoder RAW del core (ADR-08.b, audit NFR-LIC-01.d sotto) |
| libtiff | 4.7.0 | libtiff-dev | Hylafax (permissiva, stile BSD) | Sì: permissiva | non ancora collegata; writer TIFF e fixture DNG (ADR-08.b, audit NFR-LIC-01.d sotto) |
| Little CMS 2 | 2.17 | liblcms2-dev | MIT (parti IJG, GPL-2+/GPL-3 per contributi accessori) | Sì: MIT | caricata come dipendenza transitiva di `libraw_r.so.23` da U2 di M3.a; il core non ne chiama le funzioni; gestione colore |
| Qt | 6.10.2 | qt6-base-dev | LGPL-3 o GPL-2 (moduli); GPL-3 con eccezione Qt; BSD-3-clause, Expat per parti | Sì: Qt sotto GPL-3/LGPL-3 | solo GUI; non collegata al core |
| GoogleTest | 1.17.0, copia in `third_party/googletest` | googletest (copia) | BSD-3-clause | Sì: permissiva | solo test |

Moduli usati dal core (simboli indefiniti di `libastrocore.a`, `nm -u`, 29 ottobre 2026):
- CFITSIO: `ffomem`, `ffimem` (apertura da memoria), `ffclos`, `ffmahd`, `ffghadll`, `ffgiprll`, `ffgpv`, `ffppr`, `ffpkys`, `ffpkyd`, `ffprec`, `ffpscl`, `ffcrim`, `ffdkey`, `ffvcks`, `ffgmsg`, `ffgerr`, `ffcmsg`. Nessuna funzione di apertura da file o da URL (ADR-13, `tools/check_offline.sh`).
- libcrypto: solo EVP SHA-256 (`EVP_MD_CTX_new`, `EVP_sha256`, `EVP_DigestInit_ex`, `EVP_DigestUpdate`, `EVP_DigestFinal_ex`, `EVP_MD_CTX_free`). Nessun modulo di rete (`libssl` non è collegata dal core).
- SQLite: API C di base (`sqlite3_open_v2`, `prepare_v2`, `step`, `bind_*`, `column_*`, `exec`, `backup_*`, `db_config`, `busy_timeout`).
- FFTW3: collegata solo da `test_deps`; il core non la usa ancora.
- GoogleTest: solo test.

## Librerie caricate dai binari

Insieme di `ldd` sui binari di test della build `release` e della build `dev` (la build `dev` aggiunge le librerie dei sanitizer). Non esistono ancora eseguibili del prodotto: il core è la libreria statica `libastrocore.a`, e i binari di test sono i soli che la collegano. L'hash è lo SHA-256 del file realmente caricato (`realpath`) sulla macchina di sviluppo; un aggiornamento del pacchetto cambia l'hash e `T-M2R0-01` fallisce finché la riga non viene riletta.

Dipendenze transitive di CFITSIO: la `libcfitsio` di Ubuntu è costruita con supporto curl e trascina `libcurl-gnutls`, `libgnutls`, `libssh2`, `libldap`, `libssl` e altre. Il core non chiama queste librerie (nessun simbolo di rete, `T-M2F-01`), ma sono caricate dal linker dinamico; `tools/check_offline.sh` vieta `libcurl`, `libssl` e `libgnutls` solo tra i `NEEDED` diretti dei binari.

| Libreria (soname) | File reale | Pacchetto | Versione | SHA-256 | Licenza (copyright Debian) | Compatibile GPL-3.0-or-later | Origine |
|---|---|---|---|---|---|---|---|
| `ld-linux-x86-64.so.2` | `/usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2` | libc6 | 2.43-2ubuntu2.4 | `bda779ea9e9ad234f60315477d7cfd0bdde861cd8ddd9df8cdaf62f18c4cec14` | LGPL-2.1+ (parti GPL-2+ con eccezione di collegamento, LGPL-3+, ISC, Unicode-DFS-2016) | Sì: collegamento dinamico | transitiva, richiesta da libc.so.6, libcom_err.so.2, libgcc_s.so.1... |
| `libasan.so.8` | `/usr/lib/x86_64-linux-gnu/libasan.so.8.0.0` | libasan8 | 16-20260322-1ubuntu1 | `b49a5ec136d444f8b40cf24964c20ef01434fc4e6dcf68dc272f5028d65b5496` | GPL-3 con GCC Runtime Library Exception 3.1 | Sì: solo build dev | diretta |
| `libbrotlicommon.so.1` | `/usr/lib/x86_64-linux-gnu/libbrotlicommon.so.1.2.0` | libbrotli1 | 1.2.0-3ubuntu0.1 | `7f3acc17bffda6f12f7b022e9386514cfd83e77de67e71218bcbf19ec3577a99` | MIT | Sì | transitiva, richiesta da libbrotlidec.so.1 |
| `libbrotlidec.so.1` | `/usr/lib/x86_64-linux-gnu/libbrotlidec.so.1.2.0` | libbrotli1 | 1.2.0-3ubuntu0.1 | `470ed7f6b9053163b65b83efebc53bb98ca0d7a5a30ead2b545a4e5574da9c91` | MIT | Sì | transitiva, richiesta da libcurl-gnutls.so.4 |
| `libbz2.so.1.0` | `/usr/lib/x86_64-linux-gnu/libbz2.so.1.0.4` | libbz2-1.0 | 1.0.8-6ubuntu0.1 | `7138aeb748f73c150b1d34d67de69139a441910cd7b269076ba3c8b986dea9a7` | BSD-variant (licenza bzip2) | Sì | transitiva, richiesta da libcfitsio.so.10 |
| `libc.so.6` | `/usr/lib/x86_64-linux-gnu/libc.so.6` | libc6 | 2.43-2ubuntu2.4 | `85e64f97e348786a8fb4d9f3d52fec289e2fb86bba20f0731dfe61990525e0f7` | LGPL-2.1+ (parti GPL-2+ con eccezione di collegamento, LGPL-3+, ISC, Unicode-DFS-2016) | Sì: collegamento dinamico | diretta; anche richiesta da libasan.so.8, libbrotlicommon.so.1, libbrotlidec.so.1... |
| `libcfitsio.so.10` | `/usr/lib/x86_64-linux-gnu/libcfitsio.so.10.0.0` | libcfitsio10t64 | 4.6.3-1 | `7e9bb1f8af5fae3c7deb443665cb6013056695f3a9a55ac98f16dfbcc3775d71` | Testo HEASARC permissivo (tag Debian NOSA, SAO, X11); accessori GPL-2+/GPL-3+ con eccezioni | Sì: permissiva | diretta |
| `libcom_err.so.2` | `/usr/lib/x86_64-linux-gnu/libcom_err.so.2.1` | libcom-err2 | 1.47.2-3ubuntu4 | `6fb3115b11fe7e14c26f3e086c4dc1fb41061130869a66c976e0ea69fdb912d6` | Expat (MIT SIPB) per lib/et; BSD-3-Clause per com_right.c; resto di e2fsprogs GPL-2 | Sì | transitiva, richiesta da libgssapi_krb5.so.2, libkrb5.so.3 |
| `libcrypto.so.3` | `/usr/lib/x86_64-linux-gnu/libcrypto.so.3` | libssl3t64 | 3.5.5-1ubuntu3.7 | `5385f0436ac2e284ba8c5fb1488874057c29c04be72dea32add832694f381e28` | Apache-2.0 | Sì con GPL-3; no con GPL-2-only | diretta; anche richiesta da libldap.so.2, libsasl2.so.2, libssh2.so.1... |
| `libcurl-gnutls.so.4` | `/usr/lib/x86_64-linux-gnu/libcurl-gnutls.so.4.8.0` | libcurl3t64-gnutls | 8.18.0-1ubuntu2.8 | `4ec6c148167d43b75ce9f3b6b271d0e048848e7d53ff1935e25150fdb4fe22eb` | curl (permissiva), ISC, X11 | Sì | transitiva, richiesta da libcfitsio.so.10 |
| `libffi.so.8` | `/usr/lib/x86_64-linux-gnu/libffi.so.8.2.0` | libffi8 | 3.5.2-4 | `1a0dc86f787f73e025a6e521056360afcbe70f2a82cd808132fefc2b4ee95daa` | Expat (MIT) | Sì | transitiva, richiesta da libp11-kit.so.0 |
| `libfftw3.so.3` | `/usr/lib/x86_64-linux-gnu/libfftw3.so.3.6.10` | libfftw3-double3 | 3.3.10-2fakesync1build3 | `de77e73eb77a10462e27cb851c0a68e9064d0b6d72b90f6be3edcf64b6d2bdc7` | GPL-2+ | Sì: GPL-2+ include GPL-3; impone GPL al distribuito | diretta |
| `libfftw3f.so.3` | `/usr/lib/x86_64-linux-gnu/libfftw3f.so.3.6.10` | libfftw3-single3 | 3.3.10-2fakesync1build3 | `8fae0bc5d15b6caf5bb843f6a9328d2b08cd5440bef97722401cffdebc553611` | GPL-2+ | Sì: GPL-2+ include GPL-3; impone GPL al distribuito | diretta |
| `libgcc_s.so.1` | `/usr/lib/x86_64-linux-gnu/libgcc_s.so.1` | libgcc-s1 | 16-20260322-1ubuntu1 | `9d339ecb409578d6a5d587e6c537a8f9589b8a13fefba30d167433a4b5758bee` | GPL-3 con GCC Runtime Library Exception 3.1 | Sì | diretta; anche richiesta da libasan.so.8, libstdc++.so.6, libubsan.so.1 |
| `libgmp.so.10` | `/usr/lib/x86_64-linux-gnu/libgmp.so.10.5.0` | libgmp10 | 2:6.3.0+dfsg-5ubuntu2 | `fda9699eef15deda5f1c626e9140377a7f5d88c41516a54278ac02429cb20fa5` | GPL-2+ o LGPL-3+ | Sì | transitiva, richiesta da libgnutls.so.30, libhogweed.so.6, librtmp.so.1 |
| `libgnutls.so.30` | `/usr/lib/x86_64-linux-gnu/libgnutls.so.30.41.1` | libgnutls30t64 | 3.8.12-2ubuntu1.1 | `ebcc6a4dfae904dc956feb61cef5c36917cf36c19c5718d3c2be7e377ac65d27` | LGPL-3+ o GPL-2+ (libreria principale) | Sì | transitiva, richiesta da libcurl-gnutls.so.4, librtmp.so.1 |
| `libgomp.so.1` | `/usr/lib/x86_64-linux-gnu/libgomp.so.1.0.0` | libgomp1 | 16-20260322-1ubuntu1 | `645478cbe100d1465b9d45f7047bca3965358f4d399937e544f55ac7a924d5a3` | GPL-3 con GCC Runtime Library Exception 3.1 | Sì | transitiva da libraw_r.so.23 (core, U2 di M3.a) |
| `libgssapi_krb5.so.2` | `/usr/lib/x86_64-linux-gnu/libgssapi_krb5.so.2.2` | libgssapi-krb5-2 | 1.22.1-2ubuntu4.1 | `18e2ba0b2f421b780e7265337e52996f382857df63d0f9e63f391a75a14cdbe7` | MIT Kerberos (permissiva) | Sì | transitiva, richiesta da libcurl-gnutls.so.4 |
| `libhogweed.so.6` | `/usr/lib/x86_64-linux-gnu/libhogweed.so.6.11` | libhogweed6t64 | 3.10.2-1 | `67bdc12f1ffe643142d5e185a6a8f654fd38cd14e77bae68201cf9aa71b00b61` | LGPL-3+ o GPL-2+ | Sì | transitiva, richiesta da libgnutls.so.30, librtmp.so.1 |
| `libidn2.so.0` | `/usr/lib/x86_64-linux-gnu/libidn2.so.0.4.0` | libidn2-0 | 2.3.8-4build1 | `cdb48484c43a795c3722e286c31eb6238b4b7c3780043895dd9fa6b6041f78da` | LGPL-3+ o GPL-2+ | Sì | transitiva, richiesta da libcurl-gnutls.so.4, libgnutls.so.30, libpsl.so.5 |
| `libjpeg.so.8` | `/usr/lib/x86_64-linux-gnu/libjpeg.so.8.2.2` | libjpeg-turbo8 | 2.1.5-4ubuntu4 | `90a1eebead4d7c1abc46cf9c66c5392c60fc2c16f038af20fc7efbd0d8dd427f` | BSD-BY-LC-NE (IJG), BSD-3-clause, Zlib | Sì: permissiva, con l'obbligo di citazione IJG nella documentazione | transitiva da libraw_r.so.23 (core, U2 di M3.a) |
| `libk5crypto.so.3` | `/usr/lib/x86_64-linux-gnu/libk5crypto.so.3.1` | libk5crypto3 | 1.22.1-2ubuntu4.1 | `ee77156da39841edfaa7c5fcdca4cf98ae34d60323095617b484188a4d9965e3` | MIT Kerberos (permissiva) | Sì | transitiva, richiesta da libgssapi_krb5.so.2, libkrb5.so.3 |
| `libkeyutils.so.1` | `/usr/lib/x86_64-linux-gnu/libkeyutils.so.1.10` | libkeyutils1 | 1.6.3-6ubuntu3 | `c0c8f8d5adec4abe76fa462d8ad7c010f50c61e2b67b9feaa6f44e462c12cf41` | LGPL-2+ | Sì | transitiva, richiesta da libkrb5.so.3 |
| `libkrb5.so.3` | `/usr/lib/x86_64-linux-gnu/libkrb5.so.3.3` | libkrb5-3 | 1.22.1-2ubuntu4.1 | `aca607790a4349c5053390fa22b24e717b2c5ea101d7b468ece6507140c6ff7b` | MIT Kerberos (permissiva) | Sì | transitiva, richiesta da libgssapi_krb5.so.2 |
| `libkrb5support.so.0` | `/usr/lib/x86_64-linux-gnu/libkrb5support.so.0.1` | libkrb5support0 | 1.22.1-2ubuntu4.1 | `8cc61cecc016710e7134c1621b5a6537c55eea91d2a074f6f05fa6f6e36ec8d6` | MIT Kerberos (permissiva) | Sì | transitiva, richiesta da libgssapi_krb5.so.2, libk5crypto.so.3, libkrb5.so.3 |
| `liblber.so.2` | `/usr/lib/x86_64-linux-gnu/liblber.so.2.0.200` | libldap2 | 2.6.10+dfsg-1ubuntu5 | `e2f9e23d75b37be7bdcdc39d2f74e2355a68f2ccd86e7f6534969ad8b359834b` | OpenLDAP-2.8 (permissiva) | Sì | transitiva, richiesta da libcurl-gnutls.so.4, libldap.so.2 |
| `liblcms2.so.2` | `/usr/lib/x86_64-linux-gnu/liblcms2.so.2.0.17` | liblcms2-2 | 2.17-1ubuntu0.2 | `046e89ccd894994728028a8f00c2424b79cff4e3d9832530f964edddf1078d1b` | MIT | Sì | transitiva da libraw_r.so.23 |
| `libldap.so.2` | `/usr/lib/x86_64-linux-gnu/libldap.so.2.0.200` | libldap2 | 2.6.10+dfsg-1ubuntu5 | `0aa894b6d72690723015f6056bb6730a1cca41a2fac8224ef38cd7880c8e7860` | OpenLDAP-2.8 (permissiva) | Sì | transitiva, richiesta da libcurl-gnutls.so.4 |
| `libm.so.6` | `/usr/lib/x86_64-linux-gnu/libm.so.6` | libc6 | 2.43-2ubuntu2.4 | `1c488db6f4fe8cecb72456bc018de000b7a9b539c2994864b0416b5e8371b3d8` | LGPL-2.1+ (parti GPL-2+ con eccezione di collegamento, LGPL-3+, ISC, Unicode-DFS-2016) | Sì: collegamento dinamico | diretta; anche richiesta da libasan.so.8, libcfitsio.so.10, libfftw3.so.3... |
| `libnettle.so.8` | `/usr/lib/x86_64-linux-gnu/libnettle.so.8.11` | libnettle8t64 | 3.10.2-1 | `68c2f760d329a85ce1aa5cfc5e48ffece235258159005726798ad14e6f96a789` | LGPL-3+ o GPL-2+ | Sì | transitiva, richiesta da libcurl-gnutls.so.4, libgnutls.so.30, libhogweed.so.6... |
| `libnghttp2.so.14` | `/usr/lib/x86_64-linux-gnu/libnghttp2.so.14.29.2` | libnghttp2-14 | 1.68.0-2ubuntu0.2 | `2655558fee787cb4724ad59c72ce75b20847efc92a1c52fc202aea1d812c9b20` | MIT | Sì | transitiva, richiesta da libcurl-gnutls.so.4 |
| `libp11-kit.so.0` | `/usr/lib/x86_64-linux-gnu/libp11-kit.so.0.4.8` | libp11-kit0 | 0.26.2-2 | `2611eeb039dbed48839d90afe3d9ad3e5bd9edca2989af05dc62a93c3ff41fed` | BSD-3-clause | Sì | transitiva, richiesta da libgnutls.so.30 |
| `libpsl.so.5` | `/usr/lib/x86_64-linux-gnu/libpsl.so.5.3.4` | libpsl5t64 | 0.21.2-1.1build2 | `18d125c725a498d66f971ceb60bfc7cf660f6a0149b54e5caa3f4651c3d2b3c6` | MIT | Sì | transitiva, richiesta da libcurl-gnutls.so.4 |
| `libraw_r.so.23` | `/usr/lib/x86_64-linux-gnu/libraw_r.so.23.0.0` | libraw23t64 | 0.21.5b-1ubuntu1.1 | `3484c3eecb3614c845cadd4c4de59ff124c1646317e34438e98394e2edab3010` | LGPL-2.1-or-later o CDDL-1.0 (si sceglie LGPL) | Sì: collegamento dinamico | diretta: decoder RAW del core (ADR-08.b, U2 di M3.a) |
| `libresolv.so.2` | `/usr/lib/x86_64-linux-gnu/libresolv.so.2` | libc6 | 2.43-2ubuntu2.4 | `6b99ecf94bcb81732ed1e81615084a5cfe7383eb89ce83106fa51077409ce609` | LGPL-2.1+ (parti GPL-2+ con eccezione di collegamento, LGPL-3+, ISC, Unicode-DFS-2016) | Sì: collegamento dinamico | transitiva, richiesta da libkrb5.so.3 |
| `librtmp.so.1` | `/usr/lib/x86_64-linux-gnu/librtmp.so.1` | librtmp1 | 2.4+20151223.gitfa8646d.1-3 | `fceac319eac32ab0d6e9d6d641f0abf2f4aa097aa1a1355ea9e7419e81d45b62` | LGPL-2.1+ o GPL-2+ | Sì | transitiva, richiesta da libcurl-gnutls.so.4 |
| `libsasl2.so.2` | `/usr/lib/x86_64-linux-gnu/libsasl2.so.2.0.25` | libsasl2-2 | 2.1.28+dfsg1-9ubuntu3 | `25163eb0a23ffe7d4170f9e219886dfdb831bbb1031a2cfac2227284a7f51e02` | BSD-3-Clause-Attribution (stile BSD) | Sì | transitiva, richiesta da libldap.so.2 |
| `libsqlite3.so.0` | `/usr/lib/x86_64-linux-gnu/libsqlite3.so.0.8.6` | libsqlite3-0 | 3.46.1-9ubuntu0.3 | `4633a4be13fdf05cb0f859e04fb06077a1fe30a90e405721a23873bd1f3d88a8` | Pubblico dominio | Sì | diretta |
| `libssh2.so.1` | `/usr/lib/x86_64-linux-gnu/libssh2.so.1.0.1` | libssh2-1t64 | 1.11.1-1ubuntu0.26.04.4 | `34287a1e8942e6fdf1c154e8c615a8dab7a69dc6a4e8d76913afb323fdc6fd6f` | BSD-3-clause | Sì | transitiva, richiesta da libcurl-gnutls.so.4 |
| `libssl.so.3` | `/usr/lib/x86_64-linux-gnu/libssl.so.3` | libssl3t64 | 3.5.5-1ubuntu3.7 | `63b47f444efa588e291e9b650a5ba2ab02f856356f2d50f13a2e477db9119e09` | Apache-2.0 | Sì con GPL-3; no con GPL-2-only | transitiva, richiesta da libldap.so.2 |
| `libstdc++.so.6` | `/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.35` | libstdc++6 | 16-20260322-1ubuntu1 | `5bb0d21308f123b6ad46c6f35b42cedfcb8d6d439a53aa3dae04d880aaffdde3` | GPL-3 con GCC Runtime Library Exception 3.1 | Sì | diretta; anche richiesta da libubsan.so.1 |
| `libtasn1.so.6` | `/usr/lib/x86_64-linux-gnu/libtasn1.so.6.6.5` | libtasn1-6 | 4.21.0-2 | `1255353abfae7b9f52aedb0264876cd4c852f13c2acf3e2203ebd2aab79bce02` | LGPL-2.1+ (libreria) | Sì | transitiva, richiesta da libgnutls.so.30 |
| `libubsan.so.1` | `/usr/lib/x86_64-linux-gnu/libubsan.so.1.0.0` | libubsan1 | 16-20260322-1ubuntu1 | `1bdb7eae5853277e4d5dfa17431a63944feb16289fa707d8e4d1d8d3825de68e` | GPL-3 con GCC Runtime Library Exception 3.1 | Sì: solo build dev | diretta |
| `libunistring.so.5` | `/usr/lib/x86_64-linux-gnu/libunistring.so.5.2.0` | libunistring5 | 1.3-2build1 | `c762a289c02f716a12549d544a811715126067b9a6377057671d3fad9453a1fc` | LGPL-3+ o GPL-3+ | Sì | transitiva, richiesta da libgnutls.so.30, libidn2.so.0, libpsl.so.5 |
| `libz.so.1` | `/usr/lib/x86_64-linux-gnu/libz.so.1.3.1` | zlib1g | 1:1.3.dfsg+really1.3.1-1ubuntu3.2 | `27fa6c6cb94fb5b099ead61758a300dbc392d1dbf307fff6c16a9b8e57fdc9de` | Zlib | Sì | transitiva, richiesta da libcfitsio.so.10, libcrypto.so.3, libcurl-gnutls.so.4... |
| `libzstd.so.1` | `/usr/lib/x86_64-linux-gnu/libzstd.so.1.5.7` | libzstd1 | 1.5.7+dfsg-3 | `060eeb79531d435306665fd78329d8a9dd579f95876c5960bb67937825362a80` | BSD-3-clause o GPL-2 | Sì | transitiva, richiesta da libcrypto.so.3, libcurl-gnutls.so.4 |

Controllo: `python3 -S tools/check_licenses.py docs/licenses.md <directory dei binari>` (ctest `T-M2R0-01.licenses`) esegue `ldd` su ogni eseguibile ELF della directory e richiede, per ogni libreria, una riga con soname, file reale, SHA-256 uguale al calcolato e licenza non vuota. Con `--self-test` verifica che una riga mancante, un hash alterato o una licenza vuota facciano fallire il controllo.

Note:
- Nessuna incompatibilità con GPL-3.0-or-later trovata. L'unico vincolo da rispettare: OpenSSL (Apache-2.0) non è compatibile con GPL-2-only, quindi la licenza non deve mai scendere a GPL-2-only.
- GoogleTest si compila dalla copia interna `third_party/googletest` (provenienza e hash in `PROVENANCE.md` e `SHA256SUMS`); il build non legge le installazioni di GoogleTest del sistema.
- Il SBOM degli artefatti distribuiti si costruisce quando esisterà un artefatto da distribuire; qui c'è l'inventario di adozione.

## NFR-LIC-01.d: audit di adozione M3 (LibRaw, libtiff)

Regola di verifica all'ingresso M3: `docs/contracts/ADR-08-decoder-pack.md` §4. Audit del 30 ottobre 2026 sulla macchina di sviluppo, prima che un binario del progetto colleghi queste librerie: pacchetto da `dpkg -S` sul file reale, versione da `dpkg-query -W`, licenza dal file `/usr/share/doc/<pacchetto>/copyright` del pacchetto installato, SHA-256 calcolati. Dipendenze transitive da `ldd` e `readelf -d` (`NEEDED`) di `libraw_r.so.23.0.0` e `libtiff.so.6.1.0`. Non è un parere legale.

File `copyright` letti:

| Pacchetto | Versione | SHA-256 di `copyright` | Licenze lette (righe `License:`) |
|---|---|---|---|
| libraw23t64 | 0.21.5b-1ubuntu1.1 | `82bde4b0195268dfa586f5cae16b06f16179c654976919cd6a86a3fdd7c7351b` | `Files: *` LGPL-2.1 (testo «version 2.1 … or (at your option) any later version») o CDDL-1.0; `RawSpeed/rawspeed_xmldata.cpp` CC-BY-SA-3.0; `debian/*` GPL-2+ |
| libtiff6 | 4.7.0-3ubuntu5 | `d73e31b03a0d7a515851601c19fee5792c7db03a312efacae146ddf9c91f86f1` | Hylafax (`Files: *` e `debian/*`) |
| libjpeg-turbo8 | 2.1.5-4ubuntu4 | `3821184e23385bf2cdc01f56bd999b34036fc3aa03d5135d41680374eb81bf8a` | BSD-BY-LC-NE (licenza IJG: obbligo di citare l'uso del codice IJG nella documentazione), BSD-3-clause, Zlib, Expat, NTP |
| liblcms2-2 | 2.17-1ubuntu0.2 | `420fd2c72204652c08a48548a0d1002edab3d9b180d3c416578c61f74092f460` | MIT (`Files: *`); GPL-3 per `plugins/fast_float/*` e `plugins/threaded/*`; IJG, GPL-2+ per file accessori |
| libgomp1 | 16-20260322-1ubuntu1 | `d7815dd2364180835891947d96d808878204dea45b8597602a7952332195fbea` | GPL-3 con GCC Runtime Library Exception 3.1 (formato non DEP-5; letto nel testo) |
| libwebp7, libsharpyuv0 | 1.5.0-0.1build1 | `725344c9200f6e6dad88d1ea88add2d673cdf1d8fbc3961bcd2693ab0c26d30d` | BSD-3-Clause; Apache-2.0 per parti |
| liblzma5 | 5.8.3-1ubuntu0.1 | `a9c3f6e1bf373b54e29704e31afd99f5405a32cbc7897608b0f15e9d544882d4` | 0BSD (`Files: *`); GPL-2+, LGPL-2.1+, dominio pubblico e permissive per file accessori |
| liblerc4 | 4.0.0+ds-5ubuntu2 | `6e694f6e6063e7b1ffb5f723ce4e696aa015944cf1b4d15e8f3ea7e90417a71d` | Apache-2.0 |
| libjbig0 | 2.1-6.1ubuntu3 | `89ab20c324a191652e94598f2144a65442faed4f14f77bbd10516130ee20f05a` | GPL-2+ |
| libdeflate0 | 1.23-2ubuntu1 | `b08d3438a0eadaee3103fba087a3e0f639506388ed71e7e2dfbb3e3484e9742c` | Expat |
| libzstd1 | 1.5.7+dfsg-3 | `2efe51c80ddc473bc2c91faec49dae645bf7f5a04824528fec816320432da2ae` | BSD-3-clause o GPL-2 |
| zlib1g | 1:1.3.dfsg+really1.3.1-1ubuntu3.2 | `9e5b96d63773a5d177ba264254390f792be07e41748ebd94730981c6cac31cc6` | Zlib |

Librerie che un binario collegato a LibRaw e libtiff caricherà, stesse colonne della tabella «Librerie caricate dai binari». Le righe di `libzstd.so.1`, `libz.so.1`, `libstdc++.so.6`, `libm.so.6`, `libc.so.6`, `libgcc_s.so.1` e `ld-linux-x86-64.so.2` ci sono già. Quando il primo binario collega LibRaw o libtiff (M3.a), l'autore copia queste righe nella tabella «Librerie caricate dai binari» e T-M2R0-01 le verifica; finché mancano, T-M2R0-01 fallisce. Con U2 di M3.a (core collegato a LibRaw) sono state copiate le righe di `libraw_r.so.23`, `libgomp.so.1`, `libjpeg.so.8` e `liblcms2.so.2`, con hash e versioni riletti dal sistema il 30 ottobre 2026 (`sha256sum`, `dpkg -S`, `dpkg-query -W`).

| Soname | File reale | Pacchetto | Versione | SHA-256 | Licenza (copyright Debian) | Compatibile GPL-3.0-or-later | Origine |
|---|---|---|---|---|---|---|---|
| `libraw_r.so.23` | `/usr/lib/x86_64-linux-gnu/libraw_r.so.23.0.0` | libraw23t64 | 0.21.5b-1ubuntu1.1 | `3484c3eecb3614c845cadd4c4de59ff124c1646317e34438e98394e2edab3010` | LGPL-2.1-or-later o CDDL-1.0 (si sceglie LGPL) | Sì: collegamento dinamico | diretta (ADR-08.b) |
| `libtiff.so.6` | `/usr/lib/x86_64-linux-gnu/libtiff.so.6.1.0` | libtiff6 | 4.7.0-3ubuntu5 | `d65b506791e2469e886c716a7993b5e65ce40015aef1d15a678804a9fc3c4844` | Hylafax | Sì: permissiva | diretta (ADR-08.b) |
| `libgomp.so.1` | `/usr/lib/x86_64-linux-gnu/libgomp.so.1.0.0` | libgomp1 | 16-20260322-1ubuntu1 | `645478cbe100d1465b9d45f7047bca3965358f4d399937e544f55ac7a924d5a3` | GPL-3 con GCC Runtime Library Exception 3.1 | Sì | transitiva da libraw_r.so.23; diretta se il core chiama `omp_set_num_threads` (ADR-08.b §2.7) |
| `libjpeg.so.8` | `/usr/lib/x86_64-linux-gnu/libjpeg.so.8.2.2` | libjpeg-turbo8 | 2.1.5-4ubuntu4 | `90a1eebead4d7c1abc46cf9c66c5392c60fc2c16f038af20fc7efbd0d8dd427f` | BSD-BY-LC-NE (IJG), BSD-3-clause, Zlib | Sì: permissiva, con l'obbligo di citazione IJG nella documentazione | transitiva da libraw_r.so.23 e libtiff.so.6 |
| `liblcms2.so.2` | `/usr/lib/x86_64-linux-gnu/liblcms2.so.2.0.17` | liblcms2-2 | 2.17-1ubuntu0.2 | `046e89ccd894994728028a8f00c2424b79cff4e3d9832530f964edddf1078d1b` | MIT | Sì | transitiva da libraw_r.so.23 |
| `libwebp.so.7` | `/usr/lib/x86_64-linux-gnu/libwebp.so.7.1.10` | libwebp7 | 1.5.0-0.1build1 | `0b477702bb43d90a1205813a9c211faa8dfef025a258b9d60663b37311b87c08` | BSD-3-Clause | Sì | transitiva da libtiff.so.6 |
| `libsharpyuv.so.0` | `/usr/lib/x86_64-linux-gnu/libsharpyuv.so.0.1.1` | libsharpyuv0 | 1.5.0-0.1build1 | `fced0bf76ec62fcce9b2cea1a91a9c61fbf6828e9e66b194cc6d779798907ff2` | BSD-3-Clause | Sì | transitiva da libwebp.so.7 |
| `liblzma.so.5` | `/usr/lib/x86_64-linux-gnu/liblzma.so.5.8.3` | liblzma5 | 5.8.3-1ubuntu0.1 | `515749273a91ca5e3b273b4025dc2c1f8827f90005baea0ab85ffb594e74f72f` | 0BSD | Sì | transitiva da libtiff.so.6 |
| `libLerc.so.4` | `/usr/lib/x86_64-linux-gnu/libLerc.so.4` | liblerc4 | 4.0.0+ds-5ubuntu2 | `b7c1fe626e31e8c3ecb80d428ffb3a8824fd241ec55c6edc8b12437412206060` | Apache-2.0 | Sì con GPL-3; no con GPL-2-only | transitiva da libtiff.so.6 |
| `libjbig.so.0` | `/usr/lib/x86_64-linux-gnu/libjbig.so.0` | libjbig0 | 2.1-6.1ubuntu3 | `19ae16694b0f2c442b367fd9dfecf8da68c9b8ded0bc9029c1d53a9ba91a4151` | GPL-2+ | Sì: GPL-2+ include GPL-3 | transitiva da libtiff.so.6 |
| `libdeflate.so.0` | `/usr/lib/x86_64-linux-gnu/libdeflate.so.0` | libdeflate0 | 1.23-2ubuntu1 | `0e33bbae9bd7f62fd4172ac04d9088736defa499491aacae723732ce4e905f94` | Expat | Sì | transitiva da libtiff.so.6 |

Moduli che il core userà (ADR-08.b §1): LibRaw `open_buffer`, `unpack`, `set_progress_handler`, `set_dataerror_handler`, `version`, lettura di `imgdata`; libtiff `TIFFClientOpen` e funzioni di scrittura su procedure di I/O in memoria. Nessuna apertura per nome (ADR-08.b §5).

Note dell'audit:
- Nessuna incompatibilità con GPL-3.0-or-later. Il vincolo di OpenSSL vale anche per Apache-2.0 di liblerc4: la licenza del prodotto non scende mai a GPL-2-only.
- `RawSpeed/rawspeed_xmldata.cpp` (CC-BY-SA-3.0, non compatibile con GPL-3.0) è ritenuto assente dal binario, come ipotesi: `libraw_capabilities()` = `0x80` (la libreria dichiara di essere costruita senza RawSpeed), `nm -D --defined-only libraw_r.so.23.0.0` non ha `_rawspeed_data_xml`, `strings` non trova `<Camera`. Il log di build del pacchetto non è stato letto: se l'ipotesi fosse falsa, l'audit di LibRaw si rifà prima della distribuzione (M5).
- `libraw_r.pc` richiede `lcms2`, il cui `.pc` aggiunge `-llcms2_fast_float -llcms2_threaded`: archivi statici di `liblcms2-dev` sotto GPL-3. Il linker estrae un oggetto da un archivio solo se un simbolo è richiesto; il core non chiama le funzioni dei plugin. In ogni caso GPL-3 è compatibile con il prodotto GPL-3.0-or-later.
- libjpeg-turbo (licenza IJG) richiede di citare l'uso del codice IJG nella documentazione del prodotto distribuito: da riportare nell'SBOM e nella documentazione di M5.
- Hash validi per questa macchina e questi pacchetti; un aggiornamento li cambia e T-M2R0-01 lo segnala dopo il collegamento.

## Librerie compilate nel progetto (U12)

Contratto: `docs/contracts/U12-third-party.md`. Licenze lette il 30 ottobre 2026 dai file upstream dei tarball scaricati dai siti ufficiali (URL e SHA-256 dei tarball nel contratto, §1). Collegamento statico nel prodotto; finché l'implementazione di U12 non sostituisce il collegamento, le righe di `libraw_r.so.23` e `libcfitsio.so.10` di sistema nelle tabelle sopra restano quelle caricate dai binari.

| Libreria | Versione | SHA-256 del tarball | File di licenza upstream (SHA-256) | Licenza letta | Scelta | Compatibile GPL-3.0-or-later |
|---|---|---|---|---|---|---|
| LibRaw | 0.22.3 | `789f3c33466210a1a0907f747a97c715d21bbf4af712f257fcbbd90111b227e4` | `COPYRIGHT` (`574c88f5dd59414cfc896293a63defbc2d525a418ed413d7ab7dcc43cf79c509`), `LICENSE.LGPL` (`eea173a556abac0370461e57e12aab266894ea6be3874c2be05fd87871f75449`), `LICENSE.CDDL` (`0e3098d2d54a12434715f6679ea408d57da5e8d613c385c58ecc6fe5d30cc81f`) | A scelta LGPL «version 2.1» o CDDL-1.0; codice da dcraw, DCB e FBDD (BSD-3-clause), X3F (stile BSD), parti di Adobe DNG SDK 1.4 (MIT), dichiarati in `COPYRIGHT` | LGPL-2.1-only | Sì: LGPL-2.1 §3 (`LICENSE.LGPL` righe 211-221) permette di applicare alla copia la GPL versione 2 o una versione più recente, cambiando gli avvisi; elenco FSF: «compatible with GPLv2 and GPLv3» |
| CFITSIO | 4.7.0 | `ce573bbea8e75b429f8c3d3e86498741ba3dc9628a1530d2f65268397ad059e8` | `licenses/License.txt` (`7d4fff8fb7a2044eb6b33df24515f68bda852aa1a1e85a15397187b86e098658`) | Testo NASA: uso, copia, modifica e distribuzione senza costo, con avviso di copyright e disclaimer in ogni copia | — | Sì: permissiva |
| WCSLIB | 8.10 | `b394d3306da9a52410f056136911496e599b5c48f1cff2f221563697077bbd8d` | `COPYING.LESSER` (`9ccf26cfe845e0eb8bb58053e47366e7ab6b697ae010f7650978d4b71b7d1fc1`), `COPYING` (`8ceb4b9ee5adedde47b31e975c1d90c73ad27b6b165a1dcd80c7c545eb65b903`) | LGPL-3.0-or-later (`README`: «version 3 of the License, or (at your option) any later version») | LGPL-3.0-or-later | Sì: elenco FSF, LGPL 3 «compatible with GPLv3» |

Note:
- Il tarball di LibRaw contiene `RawSpeed/rawspeed_xmldata.cpp` (intestazione «License: CC-BY-SA 3.0»), che la build del prodotto non compila (U12, obbligo 9). Il repository lo conserva dentro il tarball upstream invariato; ipotesi, non parere legale: è un'aggregazione con la sua licenza, che non tocca il prodotto.
- LibRaw usa zlib e libjpeg-turbo di sistema, CFITSIO solo zlib (righe `libz.so.1` e `libjpeg.so.8` sopra, con l'obbligo di citazione IJG).

## Adobe DNG SDK (escluso, decisione del proprietario)

Componente migliore per i DNG che LibRaw non decodifica da sola: DNG interi con compressione deflate e DNG 1.7 JPEG XL (`docs/contracts/ADR-08-decoder-pack.md` §1). Versione corrente alla fonte primaria (`https://helpx.adobe.com/camera-raw/digital-negative.html`, letta il 30 ottobre 2026 con un browser; le richieste senza browser ricevono 403): «1.7.1 Build 2768, October 13, 2026», scaricata da `https://download.adobe.com/pub/adobe/dng/dng_sdk_1_7_1_2768_20261013.zip` (SHA-256 `c1d3a545ed45b150e59af2e6aa0695c1551f0e2bed34ad87436898a05194d2b1`). Licenza: `dng_sdk_1_7_1/LICENSE.txt` dello zip (SHA-256 `89d75e1bfd65269c41ec57be67adacd48926af81554574d92742f96296ed9166`), «DNG SDK License Agreement» di Adobe Inc. Lo zip `dng_sdk_1_7_1.zip` allo stesso indirizzo (file del 2024, SHA-256 `5366d9abd0e623858573bfaf0291b5e352d704f51b322753a22f82af3ae6e0c2`) non contiene alcun file di licenza.

Clausole lette che pesano sulla compatibilità con GPL-3.0-or-later:
- §1: licenza «non-exclusive, worldwide, royalty free … to use, reproduce, prepare derivative works from, publicly display, publicly perform, distribute and sublicense the Software for any purpose»; la documentazione non si può modificare.
- §5: «If you choose to distribute the Software in a commercial product, you … agree to defend, indemnify and hold harmless Adobe against any losses, damages and costs arising from the claims, lawsuits or other legal actions arising out of such distribution». La GPL-3 §7(f) ammette come termine aggiuntivo solo l'indennizzo «with contractual assumptions of liability to the recipient» (`https://www.gnu.org/licenses/gpl-3.0.txt`, righe 382-386); §10 vieta «further restrictions».
- §8: legge della California e foro della contea di Santa Clara. L'elenco FSF (`https://www.gnu.org/licenses/license-list.html`) giudica incompatibili con la GPL licenze per la loro clausola di scelta della legge (IBM Public License 1.0: «a choice of law clause which makes it incompatible with the GNU GPL»; Common Public License 1.0).
- Lo SDK porta con sé XMP SDK, libjxl, brotli, highway, expat, zlib, libjpeg, ognuno con la propria licenza, non lette in questa unità.

Esito: compatibilità con GPL-3.0-or-later non accertata (clausole §5 e §8); per la regola «Componenti» di `AGENTS.md` lo SDK non entra e la scelta va al proprietario. Opzioni: (a) escluderlo e dichiarare il limite (DNG interi deflate fermati con `raw.data_error`, DNG JPEG XL con `raw.file_unsupported`); (b) chiedere un parere legale o un'autorizzazione scritta ad Adobe prima di adottarlo; (c) un decoder JPEG XL e deflate indipendente per i DNG, fuori da LibRaw, come unità nuova. Raccomandazione: (a) ora, (b) se i DNG JPEG XL diventano un formato d'ingresso richiesto. Non è un parere legale.

## Licenza di brevetto della specifica DNG

La pagina Adobe `https://helpx.adobe.com/camera-raw/digital-negative.html` (letta il 30 ottobre 2026), sezione «DNG Specification patent license», concede a chi legge o scrive file conformi alla specifica DNG una licenza gratuita sulle «Essential Claims» e chiede: «Any Compliant Implementation distributed under this license must include the following notice displayed in a prominent manner within its source code and documentation: "This product includes DNG technology under license by Adobe."». Il prodotto legge DNG tramite LibRaw: l'avviso entra nella documentazione e nei sorgenti del prodotto distribuito (M5, insieme alla citazione IJG). Non è un parere legale.
