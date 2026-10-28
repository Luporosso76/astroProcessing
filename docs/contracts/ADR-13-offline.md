# Contratto ADR-13.a: core offline

Stadio: ADR-13.a (registro riga 3414). Piano §20 ADR-13, NFR-SEC-04 (nessun download automatico). Verifica in M2.f, mantenuta da ogni unità successiva.

## Clausole

- `libastrocore` e gli eseguibili di test non chiamano API di rete: `socket`, `connect`, `bind`, `listen`, `accept`, `getaddrinfo`, `gethostbyname`, `curl_*`, `BIO_s_connect`, `BIO_new_connect`, `OSSL_HTTP_*`.
- Nessun download implicito. Fixture e dataset pubblici entrano solo come file locali già presenti, con licenza e digest registrati.
- CFITSIO solo da memoria: `fits_open_memfile` (`ffomem`) in lettura e `fits_create_memfile` (`ffimem`) in scrittura, con il buffer scritto dal protocollo atomico di ADR-04. Mai le aperture per nome (`ffopen`, `ffdkopn`, `ffdopn`, `fftopn`, `ffiopn`, `ffeopn`, `ffinit`), che accettano URL `http://` e `ftp://`.
- Dipendenze dichiarate in `readelf -d` (`NEEDED`) dei binari del progetto: nessuna fra `libcurl*`, `libssl*`, `libgnutls*`.

## Dipendenza transitiva dichiarata

Il CFITSIO di sistema dichiara `NEEDED libcurl-gnutls.so.4`: il processo carica libcurl anche se il core non la usa. Le clausole sopra impediscono di raggiungerla dal codice del prodotto. La verifica a runtime lo controlla. Per eliminarla serve un CFITSIO compilato senza curl: decisione fuori da questo contratto.

## Test

| ID | Clausola |
|---|---|
| T-M2F-01 | `tools/check_offline.sh`, eseguito da `ctest`: simboli indefiniti di `libastrocore`, degli eseguibili del prodotto e degli oggetti compilati dai sorgenti dei test; nessuno dei simboli di rete elencati, nessuna apertura CFITSIO per nome; `readelf -d` degli eseguibili del prodotto: nessun `NEEDED` vietato. Il runtime di GoogleTest è escluso: contiene uno `StreamingListener` su socket attivo solo con `--gtest_stream_result_to`, opzione mai usata, e non entra nel prodotto. |
| T-M2F-02 | Workflow M2.e completo sotto `strace -f -e trace=network`: zero chiamate di rete. |

## Fuori da M2

Funzioni online opt-in, catalog pack, modelli AI (ADR-13.b, M8).

## Decisione sulla dipendenza transitiva

Il CFITSIO di distribuzione dipende da `libcurl-gnutls.so.4`, che quindi entra nel processo. Si accetta: il prodotto usa le librerie di sistema. L'offline si garantisce sul comportamento, non sul grafo dei `NEEDED` transitivi: CFITSIO solo da memoria, nessuna apertura per nome, verifiche T-M2F-01 (simboli e `NEEDED` diretti) e T-M2F-02 (nessuna syscall di rete). Un CFITSIO ricompilato senza curl si rivaluta solo se T-M2F-02 fallisce.
