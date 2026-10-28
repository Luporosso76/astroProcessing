# M1: pocketfft C++ come candidato da valutare, non adottato

Data: 2026-10-25. **Gate documentale soddisfatto:** versione precisa, testo BSD a tre clausole e prerequisiti API identificati per una futura valutazione isolata. **Nessuna adozione, prova numerica, compilazione, link, esecuzione o benchmark.** Non è una certificazione legale dell'intero prodotto né una chiusura di M1.

Modello dichiarato dall'ambiente: OpenAI `openai/gpt-6-astra`, identità non verificata indipendentemente. Nessuna delega; costi/token non esposti. Nessun accesso a namespace viewer/D/F, configurazioni personali, home, credenziali, device, Git o indici. Scritti solo il nuovo namespace `tmp/m1-fft-tools/` e questo report. Un solo upstream consultato; tutti gli endpoint raggiunti, nessun tentativo alternativo necessario.

## Fonte esatta, byte e autenticità

Repository primario: <https://github.com/mreineck/pocketfft>, branch richiesto `cpp`. API pubblica <https://api.github.com/repos/mreineck/pocketfft/commits/cpp> ha risolto **`4d865cba24359b05b37da4e75aee24498c3b78e7`**, data committer **2026-10-18T19:13:45Z**. Il branch è mobile; i tre file successivi sono stati richiesti con URL contenente il commit, non il branch. La data è metadato del commit, non data di release certificata.

Base immutabile usata: `https://raw.githubusercontent.com/mreineck/pocketfft/4d865cba24359b05b37da4e75aee24498c3b78e7/`.

| File locale | Byte ricevuti | SHA-256 |
|---|---:|---|
| `commit.json` | 17789 | `392c638f556ca1ca4227c25cdcc1e06a8e55ee7f43df6ce31c3c5ba5f95bf309` |
| `LICENSE.md` | 1498 | `a85ca13fdf90160b64a0698215868c13b74d835ad0a4e2ba44713b8c5058a056` |
| `pocketfft_hdronly.h` | 121769 | `518a95c552aeaee352aa30122137cd9e135d23c91e2863d9272aaa758744c86d` |
| `README.md` | 12761 | `3589655d89dadcceead2047ea7c88264e77fc6264bfe537a0ab9aeb496819f1c` |

Totale testi scaricati **153817 byte**, sotto **2097152 byte**. Provenance completa con URL di ogni file in `tmp/m1-fft-tools/provenance.json`. HTTPS con verifica certificati curl abilitata; nessun `-k`. L'API restituisce `verified: false`, `reason: unsigned`: **nessuna firma crittografica dell'autore attestata**. Hash locali identificano i byte ricevuti; non autenticano da soli l'autore. Non ricostruito/verificato indipendentemente il grafo degli oggetti Git.

## Licenza letta, incluse le notices interne

Letti integralmente [LICENSE.md della versione](https://raw.githubusercontent.com/mreineck/pocketfft/4d865cba24359b05b37da4e75aee24498c3b78e7/LICENSE.md), 25 righe, e preambolo legale dell'header, righe 1–43. Testo esplicito:

> “Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met”

Le tre condizioni impongono conservazione di copyright/condizioni/disclaimer nei sorgenti, riproduzione nei materiali della distribuzione binaria e divieto di endorsement non autorizzato. Segue disclaimer AS IS e limitazione di responsabilità. Nessuna clausola NonCommercial o obbligo di apertura del sorgente del prodotto nel testo letto. **Inferenza limitata:** questo grant è compatibile, in linea di principio, con impiego commerciale e prodotto proprietario, rispettando notices e condizioni; non decide la licenza del progetto.

`LICENSE.md` cita Max-Planck-Society 2010–2018. Il preambolo dell'header è più completo: Max-Planck-Society 2010–2024, Peter Bell 2019–2020, Matteo Frigo/MIT per DCT-IV dispari, Tan Ping Liang/Peter Bell e Cris Luengo per altre parti. Autori dichiarati: Martin Reinecke, Peter Bell. **Conservare anche il preambolo del file, non solo LICENSE.md.**

Punto sensibile FFTW verificato direttamente nelle righe 2917–2919:

```cpp
// The following code is derived from the FFTW3 function apply_re11()
// and is released under the 3-clause BSD license with friendly
// permission of Matteo Frigo and Steven G. Johnson.
```

Questa è una dichiarazione upstream di permesso per quella porzione, accompagnata dalle notices dell'header. Non prova una relicenza generale di FFTW3 e non autorizza altri sorgenti FFTW. Non acquisita corrispondenza privata né certificata la catena di titolarità. Il riscontro legale riguarda **questo artefatto e i testi letti**, non qualsiasi fork, binding, pacchetto NumPy o dipendenza futura.

## Dipendenze e interfaccia CPU

Il README completo dichiara C++11, header-only, `float`, `double`, `long double`, FFT multidimensionali complesse/reali e stride. L'header verifica C++ >=201103L. Scansione completa delle direttive `#include`: **22 occorrenze**, tutte standard C++ tranne `pthread.h` opzionale; nessun include FFTW o altro header di progetto.

- Base: `<cmath>`, `<cstdlib>`, `<cstddef>`, `<cstdint>`, `<exception>`, `<stdexcept>`, `<memory>`, `<vector>`, `<complex>`, `<algorithm>`, `<limits>`.
- Cache non nulla: `<array>`, `<mutex>`.
- `POCKETFFT_NO_EXCEPTIONS`: `<cstdio>`.
- Thread abilitati: `<mutex>`, `<condition_variable>`, `<thread>`, `<queue>`, `<atomic>`, `<functional>`, `<new>`; con `POCKETFFT_PTHREADS`, anche `<pthread.h>` e `pthread_atfork`.
- `POCKETFFT_USE_POSIX_MEMALIGN`, su piattaforma POSIX, sceglie `posix_memalign`; altrimenti esiste allocatore allineato basato su malloc/free. È una scelta di build da fissare, non da cambiare dopo errore.

Dipendenze runtime effettive da compilatore, libreria standard, funzioni matematiche, allocatore e thread non sono state linkate né inventariate come binari. Le loro licenze restano separate. Header-only non significa zero allocazioni: workspace e piani usano memoria dinamica; `c2r` multiasse alloca anche un temporaneo complesso.

| Requisito | Riscontro statico | Prerequisito per futura valutazione |
|---|---|---|
| f32/f64 | API template, README supporta float/double; specializzazioni SIMD per entrambi | Verificare sul target dimensioni e formato IEEE richiesti, senza promozione silenziosa; nessun risultato numerico qui |
| CPU strided | `stride_t=std::vector<std::ptrdiff_t>`, stride in byte; negativi consentiti dal contratto | Validare range, overflow, allineamento e assenza di alias proibiti prima della chiamata |
| c2c/r2c/c2r | Interfacce template lette nelle righe 3649–3766 | Dichiarare assi, layout complesso, direzione e fattore `fct`; trasformate non normalizzate |
| In-place | README ammette c2c/r2r con stride identici, non r2c/c2r | Non riusare automaticamente la semantica/packing FFTW |
| STOP controllato | Eccezioni default; bad_alloc ed errori di argomento segnalati | Wrapper dovrà convertire fallimento in diagnosi/uscita non nulla, senza cambiare algoritmo/backend/precisione |

I controlli interni non validano tutto il contratto. `prod(shape)` moltiplica size_t senza controllo in quel punto; `c2c` ritorna subito se il prodotto è zero. Le varianti multiasse usano `axes.back()`: il chiamante deve rifiutare assi vuoti quando richiesti. Un alias parziale non si riduce al controllo `data_in==data_out`. Non dichiarare “ogni input invalido causa STOP” senza validazione del wrapper e successiva prova. Non scritto alcun wrapper.

## SIMD, thread e riproducibilità: controlli disponibili, non garanzie

| Controllo | Comportamento letto | Conseguenza |
|---|---|---|
| `POCKETFFT_NO_VECTORS` | Disabilita vettorizzazione esplicita pocketfft | Non disabilita automaticamente autovettorizzazione/FMA del compilatore; non garantisce bit-identità |
| SIMD abilitato | Dipende da compilatore e macro ISA, ad esempio SSE2/AVX/AVX512F; può essere disabilitato internamente se non supportato | Se una ricetta richiede SIMD, verificare la configurazione effettiva e rifiutare divergenze; non accettare silenziosamente scalarizzazione |
| `POCKETFFT_NO_MULTITHREADING` | `thread_count` restituisce 1; richieste nthreads ignorate | Ricetta seriale esplicita con `nthreads=1`, non richiesta multithread degradata |
| Thread abilitati, `nthreads` | Default API 1; 0 usa hardware_concurrency; richiesta ridotta in base alla forma | Numero richiesto non è una garanzia di parallelismo esatto |
| Pool interno | Costruttore default usa `max_threads` basato su hardware_concurrency | `nthreads=2` non implica solo due thread OS creati: budget e comportamento da verificare prima di un profilo multithread |
| `POCKETFFT_CACHE_SIZE` | Default 0; LRU opzionale | Fissare cache e budget esplicitamente; non presumere cache attiva dal testo introduttivo del README |
| `POCKETFFT_NO_EXCEPTIONS` | Diagnosi stderr e abort() | Non è equivalente a protocollo applicativo di stop/receipt. Per prima valutazione raccomandate eccezioni abilitate e confine di errore esplicito |

Le eccezioni nel lavoro parallelo vengono raccolte e rilanciate dopo attesa; la creazione dei thread, se fallisce, arresta il pool e rilancia. Non è una prova generale di cancellazione immediata o atomicità dell'output: un errore può lasciare output parziale, da non pubblicare come valido.

**Raccomandazione tecnica, non ricetta approvata:** prima valutazione isolata seriale esplicitamente selezionata, cache disabilitata, ISA/compiler/flags/versione runtime/rounding fissati; SIMD come variante separatamente dichiarata se necessaria. Non promettere bit-identità tra ISA, libm, toolchain o thread count. Non usare fast-math/reassociation come espediente per superare gate. Il codice seleziona inoltre algoritmi secondo forma/costi (README: Bluestein per certe lunghezze): questa strategia del backend deve essere dichiarata e valutata rispetto alla ricetta, mai spacciata per identità con FFTW o per fallback autorizzato dopo guasto.

## Copertura, comandi e limiti

Letti integralmente `LICENSE.md` 1–25 e `README.md` 1–285. Header di 3888 righe: letti **1–270, 650–1039, 2895–2969, 3645–3769**, totale 860 righe, più risultati grep su include/copyright/errori/thread. Non letti integralmente codelet, Bluestein, iteratori, cache o tutto l'header; nessun audit numerico/sicurezza completo. Metadati JSON ricevuti ma non letti integralmente: estratti SHA, committer date e campo verification. Nessun sorgente locale M1/protocollo riletto.

Preflight `du -s -B1 tmp`: **862965760 byte**. Guard prima del download: occupazione + **2097152** budget + **16777216** margine <= **1073741824**. Namespace nuovo mode 0700. Comandi curl con `-q --fail --silent --show-error --proto '=https' --max-time 60`; limite metadata **262144**, ciascuno dei tre file **524288** byte. Somma dei massimi 1835008 byte, inferiore al budget anche prima dei conteggi reali. TMPDIR sotto `tmp/m1-fft-tools`; nessun clone o cache manuale esterna.

URL metadata sopra, poi ciclo sui tre file alla base commit. Eseguiti `wc -lc`, `sha256sum`, `read`, `grep` e Python stdlib `python3 -I -B -S` soltanto per leggere JSON/provenance e verificare hash; **nessun import di candidato**, esecuzione dell'header, installazione o test. Nessuna opzione di curl disabilita TLS; `-q` evita configurazioni personali. Versioni degli strumenti non rilevate in questa ricerca.

## Esito operativo prima della chiusura M1

Il candidato ha evidenza primaria BSD-3-Clause sul commit congelato e interfaccia coerente con una futura prova CPU f32/f64 strided. Questo riduce il rischio di assumere FFTW GPL come unica opzione, **non dimostra ancora adottabilità tecnica, prestazioni o riproducibilità**. Non sceglie C++ contro Rust: un eventuale collegamento Rust richiederebbe binding/ABI/error boundary verificati, non acquisiti qui.

I tempi M1.a restano risultati validi del confronto C++/Rust con la stessa FFTW 3.3.10 locale; non diventano tempi pocketfft e non adottano FFTW. Backend interno NumPy ancora non attestato. Protocollo storico 169/SHA prefisso `1f04` invariato secondo il mandato, non ricalcolato in questa ricerca. Qualsiasi nuovo confronto richiede preregistrazione separata **prima dei dati**; nessuna rimisurazione avviata. La tolleranza zero delle vecchie fixture non è garanzia per un altro algoritmo: non allentare il gate per farlo passare.

Conservare questo candidato versionato per la futura valutazione autorizzata. Licenza irreversibile del prodotto resta decisione del proprietario; nessun cambio automatico di backend, fallback o falsa adozione. Gli altri rami proseguono indipendentemente.
