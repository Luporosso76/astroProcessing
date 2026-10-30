# Contratto ADR-13.a: core offline

Stadio: ADR-13.a (registro riga 3414). Piano §20 ADR-13, NFR-SEC-04 (nessun download automatico). Verifica in M2.f, mantenuta da ogni unità successiva. Emendamento U12 (30 ottobre 2026): CFITSIO compilata nel progetto senza curl e senza driver di rete; sezioni «CFITSIO senza rete» e «Test».

## Clausole

- `libastrocore` e gli eseguibili di test non chiamano API di rete: `socket`, `connect`, `bind`, `listen`, `accept`, `getaddrinfo`, `gethostbyname`, `curl_*`, `BIO_s_connect`, `BIO_new_connect`, `OSSL_HTTP_*`.
- Nessun download implicito. Fixture e dataset pubblici entrano solo come file locali già presenti, con licenza e digest registrati.
- CFITSIO solo da memoria: `fits_open_memfile` (`ffomem`) in lettura e `fits_create_memfile` (`ffimem`) in scrittura, con il buffer scritto dal protocollo atomico di ADR-04. Mai le aperture per nome (`ffopen`, `ffdkopn`, `ffdopn`, `fftopn`, `ffiopn`, `ffeopn`, `ffinit`), che accettano URL `http://` e `ftp://`.
- Dipendenze dichiarate in `readelf -d` (`NEEDED`) dei binari del progetto: nessuna fra `libcurl*`, `libssl*`, `libgnutls*`.
- Emendamento U12: nessuna libreria di rete caricata dal processo. La chiusura di `ldd` di ogni eseguibile del prodotto e dei test non contiene `libcurl*`, `libssl*`, `libgnutls*`, `libssh*`, `libldap*`, `liblber*`, `librtmp*`, `libnghttp*`, `libngtcp*`, `libpsl*`, `libgssapi*`, `libkrb5*`, `libsasl2*` ([U12](U12-third-party.md) obbligo 7). `libcrypto` resta ammessa (SHA-256, nessun modulo di rete).

## CFITSIO senza rete (emendamento U12)

CFITSIO 4.7.0 si compila nel progetto ([U12](U12-third-party.md)) con `-DUSE_CURL=OFF` e senza `HAVE_NET_SERVICES` (`-DCMAKE_HAVE_GETHOSTBYNAME=OFF -DCMAKE_HAVE_CONNECT=OFF`) e si collega staticamente. Fatti (sonda di U12 sulla variante condivisa con le stesse opzioni, `docs/evidence/M3a-U12-contract.md`):

| Misura | CFITSIO 4.7.0 del progetto | CFITSIO 4.6.3 di sistema |
|---|---|---|
| `NEEDED` | `libm.so.6`, `libz.so.1`, `libc.so.6` | anche `libcurl-gnutls.so.4`, `libbz2.so.1.0` |
| Simboli indefiniti fra `socket`, `connect`, `bind`, `listen`, `accept`, `getaddrinfo`, `gethostbyname`, `curl_*` | 0 | 9 |
| Librerie nella chiusura di `ldd` dell'eseguibile della sonda con nome fra `libcurl`, `libssl`, `libgnutls`, `libssh`, `libldap`, `librtmp`, `libnghttp`, `libpsl` | 0 | 8 |
| `fits_open_file` su `http://127.0.0.1:9/x.fits` e `ftp://127.0.0.1:9/x.fits` | stato 124, «no I/O driver for this URLtype» | stato 104, «could not open the named file» |
| Chiamate di rete sotto `strace -f -e trace=network` durante quelle aperture | 0 | 12 (`socket`, `connect` verso 127.0.0.1:9) |
| Giro scrittura e lettura da memoria (`ffimem`, `ffomem`) | riuscito, valori uguali | riuscito, valori uguali |

La regola «CFITSIO solo da memoria» resta: senza driver di rete un'apertura per nome di un URL non apre una connessione, ma resta un'apertura per nome vietata al prodotto.

## Test

| ID | Clausola |
|---|---|
| T-M2F-01 | `tools/check_offline.sh`, eseguito da `ctest`: simboli indefiniti di `libastrocore`, degli eseguibili del prodotto e degli oggetti compilati dai sorgenti dei test; nessuno dei simboli di rete elencati, nessuna apertura CFITSIO per nome; `readelf -d` degli eseguibili del prodotto: nessun `NEEDED` vietato. Il runtime di GoogleTest è escluso: contiene uno `StreamingListener` su socket attivo solo con `--gtest_stream_result_to`, opzione mai usata, e non entra nel prodotto. |
| T-M2F-02 | Workflow M2.e completo sotto `strace -f -e trace=network`: zero chiamate di rete. |
| T-U12-07 | Chiusura di `ldd` degli eseguibili del prodotto e dei test senza librerie di rete; archivio di CFITSIO senza simboli di rete ([U12](U12-third-party.md) obbligo 7). |

## Fuori da M2

Funzioni online opt-in, catalog pack, modelli AI (ADR-13.b, M8).

## Decisione sulla dipendenza transitiva

Fino a U12 il CFITSIO di distribuzione dichiarava `NEEDED libcurl-gnutls.so.4`, che entrava nel processo, e l'offline si garantiva solo sul comportamento. Con U12 (regola «Componenti» di `AGENTS.md`) la decisione cambia: CFITSIO si compila nel progetto senza curl e senza driver di rete, e nessuna libreria di rete entra nel processo. Restano le clausole sul comportamento (solo memoria, nessuna apertura per nome, T-M2F-01, T-M2F-02).
