# M1.a: licenza sorgente pocketfft

## Metadati del record

- Unità: M1.a.
- Scopo: registrare condizioni e limiti della licenza dei sorgenti pocketfft locali esaminati.
- Comando: comando del coordinatore in «Autenticazione locale»; il comando hash della precedente redazione è riportato in `docs/evidence/M1-f-pattern16m-producer-setup-green-review-001.md`, sezione «Fonti e limiti della redazione». Nessun ricalcolo delle fonti in questa correzione di formato.
- Input: fonti e intervalli in «Fonti e condizioni», percorsi completi e hash dei tre file in «Autenticazione locale»; hash dei manifest non disponibile nella redazione, come precisato sotto.
- Versioni: Python osservato in «Autenticazione locale»; versione sha256sum non disponibile. Versioni GCC/libstdc++/glibc del futuro ELF non disponibile, fuori perimetro. Piano attivo v2.2.1, senza nuova autenticazione.
- Ambiente: Linux, radice `/opt/workspace/astroProcessor`; kernel, CPU, RAM, GPU/driver e filesystem non disponibile.
- Risultato: confronto source-only concluso dalla ricerca sotto indicata; precedente comando hash della redazione exit0. Nessun test o binario prodotto.
- Output: `docs/evidence/M1-a-pocketfft-source-license-001.md`; hash trascritti in «Autenticazione locale». Percorso di log autonomo stdout/stderr non disponibile; nessun notice creato o modificato.
- Ambito: testi pertinenti delle fonti locali; esclusioni in «Limiti e obblighi futuri».
- Esito: PASS nel solo perimetro PASS_SCOPED_SOURCE_LICENSE; nessuna scelta della licenza del progetto.
- Limiti: provenienza dichiarata, non byte-identità integrale con upstream remoto; ulteriori limiti in «Fonti e condizioni» e «Limiti e obblighi futuri».
- Autore e data: subagente documentazione, 28 ottobre 2026; modello runtime non disponibile. Ricerca attribuita alla sessione sotto indicata; costi non disponibile.

28 ottobre 2026. Ricerca `ses_e80ae9f57ffeNWxLhHF4ywdzaR` conclusa **PASS_SCOPED_SOURCE_LICENSE**. Esito limitato ai testi pertinenti del sorgente pocketfft locale, non alla licenza del progetto, al nuovo ELF o alla distribuzione del prodotto.

## Fonti e condizioni

Fonti primarie locali lette dalla redazione: `tmp/m1-fft-tools/pocketfft_hdronly.h`, righe1-42 e2917-2919; `tmp/m1-fft-tools/LICENSE.md`, righe1-25. `tmp/m1/fft-pocket-benchmark/manifest.json` e `tmp/m1/fft-pocket/followup-manifest-003.json`, consultati tramite ricerche testuali mirate, dichiarano entrambi commit upstream `4d865cba24359b05b37da4e75aee24498c3b78e7` e conservazione dei notice completi. SHA-256 di questi due manifest non disponibile nella redazione: non è stato calcolato né sostituito con pin preventivi.

La BSD-3-Clause consente uso, modifica e redistribuzione in forma sorgente o binaria, anche commerciale, rispettando queste condizioni:

- Conservare copyright, condizioni e disclaimer nelle redistribuzioni sorgente.
- Riprodurre gli stessi notice nella documentazione o negli altri materiali distribuiti con i binari.
- Non usare nomi dei titolari o contributori per endorsement o promozione senza previo permesso scritto.

L'header contiene attribuzioni più complete del LICENSE: Max-Planck-Society2010-2024, Peter Bell, titolari delle trasformate DCT-IV dispari e contributori delle ricerche good_size. LICENSE riporta Max-Planck-Society2010-2018. Occorre conservare entrambi, senza sostituire l'header con il solo LICENSE.

Le righe2917-2919 dichiarano che la porzione derivata da FFTW3 `apply_re11()` è rilasciata BSD a tre clausole con il permesso di Matteo Frigo e Steven G. Johnson. Va conservata anche questa nota insieme ai copyright DCT-IV. Tale derivazione non dimostra una dipendenza runtime da FFTW e non giustifica l'attribuzione della GPL al sorgente esaminato.

La ricerca ha confrontato i testi pertinenti e la provenienza dichiarata del commit. Non attesta la byte-identità integrale del file locale con l'upstream remoto. Non sono stati consultati nuovi URL in questa redazione.

## Autenticazione locale

Il coordinatore ha autenticato indipendentemente tre file con FD relativi, NOFOLLOW, NOATIME, NONBLOCK, file regolari posseduti, nlink1 e fstat stabile. Ha poi eseguito dalla radice il comando riproducibile:

```sh
sha256sum -- tmp/m1-fft-tools/pocketfft_hdronly.h tmp/m1-fft-tools/LICENSE.md tmp/m1/fft-pocket-benchmark/driver.cpp
```

| File | Byte | SHA-256 |
| --- | ---: | --- |
| `tmp/m1-fft-tools/pocketfft_hdronly.h` | 121769 | `518a95c552aeaee352aa30122137cd9e135d23c91e2863d9272aaa758744c86d` |
| `tmp/m1-fft-tools/LICENSE.md` | 1498 | `a85ca13fdf90160b64a0698215868c13b74d835ad0a4e2ba44713b8c5058a056` |
| `tmp/m1/fft-pocket-benchmark/driver.cpp` | 8669 | `23116dbf3aa9ea42fca97b882532c0f74579761e2b5c1b4d75387c62749b2cd0` |

Sono tre file unici, non sei test. Il driver identifica il candidato locale: il suo hash non certifica una licenza del nostro codice, che non è stata scelta. La redazione ha ricalcolato i tre hash con `sha256sum`, insieme agli hash del report GREEN e del relativo SUMS, senza rieseguire l'autenticazione FD del coordinatore.

Versione osservata dal coordinatore con `/usr/bin/python3 -I -B -S --version`: `Python 3.14.4`. Ambiente di redazione Linux, directory `/opt/workspace/astroProcessor`; versione utility hash e costi non rilevati. Le ricerche testuali mirate nei manifest hanno confermato commit e disposizione sui notice; non equivalgono alla verifica dell'intero profilo runtime.

## Limiti e obblighi futuri

Nessun sorgente o binario è stato distribuito; nessun notice copiato o modificato. GCC, libstdc++, glibc, inventario ELF e licenze del packaging richiedono verifiche separate sul futuro binario. Questo record non adotta stack o licenza del progetto, non certifica conformità distributiva complessiva e non autorizza release commerciale o M10.

Nessun nuovo runtime, build, test, rete o installazione. Stato e prossima azione restano in `docs/progress.md`; review documentale di questo record dopo la consegna.
