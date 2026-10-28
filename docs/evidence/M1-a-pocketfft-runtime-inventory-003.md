# M1.a pocketfft: inventario statico runtime e licenze 003

## Esito e perimetro

STATIC_INVENTORY_COLLECTED_REVIEW_PENDING. Inventariati il worker autenticato,
il suo interprete ELF e quattro librerie host. Raccolti 13 archi NEEDED,
versioni richieste/offerte, percorsi canonici, symlink, hash, proprietari dei
pacchetti e notices locali. Nessun PASS licenze o autorizzazione numerica.

Zero esecuzioni del worker, build, FFT o benchmark. Non usati `ldd`, loader
`--list`, CLI del candidato o import di codice candidato. Nessuna rete, Git,
delega, installazione, modifica di sistema o creazione di indici. Nessun cambio
a sorgenti candidati, preregistrazione, handoff precedenti, binario, raw storici,
progress o matrice test. Gli output nuovi sono esclusivamente questo report e
`tmp/m1/fft-pocket/runtime-inventory-003/`.

Il mandato riferisce la review documentale BUILD
`ses_e883b1228ffeknnGVqei8OgK8A`. Il riferimento viene dal coordinatore: qui non
è stata recuperata o rivalidata quella sessione. Il report BUILD resta
PASS_BUILD_ONLY. Licenza del prodotto e adozione restano UNDECIDED.

## Input e autenticazione

Il collector ha ricontrollato tramite SHA-256/stat i quattro pin del mandato:

```text
build-followup-003/worker, 180200 byte
0f724e26f0bce62431390a35f8e8168504e051e26f2266c50355a14a37a585da
followup-manifest-003.json
9e66b7ed388813da15d0abe7cb04ce6f704af02a64f29640ad6639523c807a75
docs/evidence/M1-a-pocketfft-build-003.md
20deaf30688daea1dececdf2532d6ea2a9711ef76d2e153404b5a48b3fd21c6d
build-turn-followup-003/handoff.json
e66808eb6ba8b8520f51368a0181ef5502e02961b2c180745988d00ec8707d70
```

I percorsi non assoluti, salvo il report, sono relativi a
`tmp/m1/fft-pocket/`. Sono stati autenticati anche i pin upstream dell'header e
di LICENSE. Le aperture dei target canonici usano O_NOFOLLOW anche sui componenti
directory. I symlink osservati vengono registrati e ricontrollati. Identità stat
e hash coincidono prima/dopo per 37 fonti; i metadati di 132 file pocketfft
preesistenti coincidono nelle snapshot e nel controllo successivo.

Sono conservati cache/configurazioni del loader e pagine man locali, senza
leggere ambiente corrente, dati personali o `/proc/*/environ`. Il solo uso di
`/proc/self/fd/N` passa a readelf un descrittore ELF già aperto in sola lettura.
Il collector non legge il piano principale di circa 4500 righe. Finestre del
piano locale lette semanticamente: righe 1-112 e 240-277, 150 righe; report BUILD:
195 righe. I copyright GCC/glibc, 1641/1418 righe, sono copiati integralmente e
analizzati come dati; la lettura manuale ha riguardato estratti e campi mirati.

## Comandi e ambiente

Esecuzioni del collector e del verificatore, dalla radice del progetto:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-inventory-003/capture.py
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-inventory-003/verify.py
```

Il collector esegue 21 comandi di soli strumenti metadati. Gli argv, l'ambiente,
gli exit e gli hash di stdout/stderr sono in `inventory.json`. Tutti hanno exit
0 e stderr vuoto. Le famiglie di comandi sono:

```text
/usr/bin/readelf --version
/usr/sbin/ldconfig --version
/usr/bin/dpkg-query --version
/usr/sbin/ldconfig -p
/usr/bin/readelf -W -h -l -d -V -n /proc/self/fd/N
/usr/bin/dpkg-query -S PATH
/usr/bin/dpkg-query -W -f=FORMAT PACKAGE...
```

Readelf 2.46; ldconfig/glibc 2.43; dpkg-query 1.23.7. Ambiente esplicito dei
soli strumenti: PATH `/usr/bin:/bin`, LANG/LC_ALL `C`, TZ `UTC`. Python
3.14.4, GCC 15.2.0; Linux `7.0.0-38-generic`, x86_64. I raw inglesi evitano i
problemi di traduzione osservati in una lettura esplorativa locale di readelf.
I comandi esplorativi non sono i raw canonici; le acquisizioni canoniche sono
quelle del collector. Le durate dei comandi sono dati operativi, non benchmark.

Modello dichiarato dal runtime: `openai/gpt-6.1-sol`. Nessun override. Failover,
costi e token non esposti. Nessuna attestazione di modello sostitutivo.

## Interprete, ricerca e chiusura transitiva

PT_INTERP del worker: `/lib64/ld-linux-x86-64.so.2`. `/lib64` è un symlink a
`usr/lib64`; il link finale punta a
`../lib/x86_64-linux-gnu/ld-linux-x86-64.so.2`. Target canonico:
`/usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2`.

La cache `/etc/ld.so.cache`, 187455 byte, ha SHA-256
`2d3e1291031f8b4676007eac7f470a412d95ce402aa846215703bc2719babe49`.
`ldconfig -p` mostra una sola voce `libc6,x86-64` per ciascuno dei cinque SONAME
della chiusura. Nessuna voce alternativa hwcap per quei nomi. Tutte puntano a
`/usr/lib/x86_64-linux-gnu/`. L'interprete della cache e PT_INTERP convergono
sullo stesso target canonico. `/etc/ld.so.preload` risulta assente.

Salvati `/etc/ld.so.conf` e i quattro file `.conf` inclusi. Citano le directory
multiarch, `/usr/local/lib`, libfakeroot e Oracle instantclient. Queste sono
fonti della cache, non una prova che il loader rilegga i `.conf` a ogni avvio.
Non sono state esplorate directory personali o altre librerie di quelle suite.

RPATH e RUNPATH risultano assenti in tutti e sei gli ELF. Nei raw non compaiono
DT_AUDIT, DT_DEPAUDIT, DT_FILTER, DT_AUXILIARY o flag NODEFLIB.
La chiusura NEEDED osservata è:

```text
worker: libstdc++.so.6, libm.so.6, libgcc_s.so.1, libc.so.6
libstdc++.so.6: libm.so.6, libc.so.6, ld-linux-x86-64.so.2, libgcc_s.so.1
libgcc_s.so.1: libc.so.6, ld-linux-x86-64.so.2
libm.so.6: libc.so.6, ld-linux-x86-64.so.2
libc.so.6: ld-linux-x86-64.so.2
ld-linux-x86-64.so.2: nessuna NEEDED
```

Questa è una risoluzione statica host condizionata, non la mappa di un processo
avviato. Presuppone stessi namespace/root, cache, file e ambiente esplicito del
futuro launcher. Non dimostra assenza futura di LD_PRELOAD, LD_LIBRARY_PATH,
LD_AUDIT, GLIBC_TUNABLES, opzioni del loader, secure-exec, nuove librerie hwcap o
caricamenti `dlopen`. L'ambiente del futuro worker non è stato eseguito né
osservato. NEEDED da solo non equivale a risoluzione runtime effettiva.

## Target, pacchetti e hash

I nomi seguenti sono relativi a `/usr/lib/x86_64-linux-gnu/`. Soltanto
`libstdc++.so.6` è un ulteriore symlink: punta a `libstdc++.so.6.0.35`.
Tutti i target sono file regolari di proprietà UID/GID 0; stat completi, symlink
e build-id sono nell'inventario machine-readable.

```text
ld-linux-x86-64.so.2, 254864 byte, libc6:amd64 2.43-2ubuntu2.4
bda779ea9e9ad234f60315477d7cfd0bdde861cd8ddd9df8cdaf62f18c4cec14
libc.so.6, 2190608 byte, libc6:amd64 2.43-2ubuntu2.4
85e64f97e348786a8fb4d9f3d52fec289e2fb86bba20f0731dfe61990525e0f7
libm.so.6, 1198376 byte, libc6:amd64 2.43-2ubuntu2.4
1c488db6f4fe8cecb72456bc018de000b7a9b539c2994864b0416b5e8371b3d8
libstdc++.so.6.0.35, 2735712 byte, libstdc++6:amd64 16-20260322-1ubuntu1
5bb0d21308f123b6ad46c6f35b42cedfcb8d6d439a53aa3dae04d880aaffdde3
libgcc_s.so.1, 187120 byte, libgcc-s1:amd64 16-20260322-1ubuntu1
9d339ecb409578d6a5d587e6c537a8f9589b8a13fefba30d167433a4b5758bee
```

`dpkg-query -S` prova l'attribuzione locale dei target. Cinque MD5 dei target
coincidono con i record locali `/var/lib/dpkg/info/PACKAGE.md5sums`; gli SHA-256
sono calcolati separatamente. Il database locale non è una firma indipendente.
Il toolchain BUILD GCC 15.2 non va confuso con questi runtime GCC 16.

## Versioni dei simboli

Il worker richiede a libstdc++ `CXXABI_1.3`, `CXXABI_1.3.9`, `GLIBCXX_3.4`,
`GLIBCXX_3.4.21`, `GLIBCXX_3.4.26`; a libgcc `GCC_3.0`; a libm
`GLIBC_2.2.5`; a libc `GLIBC_2.2.5`, `GLIBC_2.3.4`, `GLIBC_2.4`,
`GLIBC_2.14`, `GLIBC_2.32`, `GLIBC_2.33`, `GLIBC_2.34`.

Tutti i nodi richiesti nei 13 rapporti consumer/provider sono definiti dai
target host inventariati. La dipendenza transitiva libstdc++ richiede anche
`GLIBC_2.38`; libc/libm/loader usano inoltre nodi ABI e `GLIBC_PRIVATE`.
Questo non dimostra portabilità a glibc 2.34. Elenchi completi richiesti/offerti
e raw `readelf -V` sono conservati. Non è una verifica delle singole relocation,
di ogni simbolo esportato, degli IFUNC o della correttezza numerica.

## Licenze e notices locali

Per libstdc++ e libgcc, `/usr/share/doc/libstdc++6/copyright` e
`/usr/share/doc/libgcc-s1/copyright` convergono su
`/usr/share/doc/gcc-16-base/copyright`, appartenente a `gcc-16-base:amd64`
`16-20260322-1ubuntu1`. Il documento attribuisce GCC alla Free Software
Foundation, 1986-2025. Dichiara libgcc e libstdc++-v3 sotto GPL v3 o successiva
con GCC Runtime Library Exception 3.1. Sono conservati documento completo,
testo GPL-3 ed eccezione completa. L'eccezione richiede un Eligible Compilation
Process; non costituisce un'esenzione incondizionata per tutte le librerie GCC.

Loader, libc e libm appartengono a glibc. Fonte:
`/usr/share/doc/libc6/copyright`, attribuzione generale Free Software Foundation
1991-2026, licenza base LGPL-2.1+. Il file contiene anche grant per componenti
matematici e altri sorgenti, tra cui BSD, CORE-MATH, SunPro, ISC e LGPL con
eccezioni. È conservato integralmente, con stanzas Files/Copyright/License e
testi locali richiamati. Non si attribuisce ogni licenza dell'intero pacchetto
a ciascun binario. Manca una mappa oggetti compilati/sorgenti: il gate di
provenienza a granularità di file e conformità distributiva resta aperto.

Per pocketfft sono conservati LICENSE BSD-3-Clause, header completo e notice
iniziale con tutte le attribuzioni. Conservato anche il commento interno delle
righe 2917-2919 e il terminatore alla 2958: il codice derivato da `apply_re11()`
ha grant BSD a tre clausole con permesso di Matteo Frigo e Steven G. Johnson.
Le attribuzioni iniziali comprendono anche MIT, Tan Ping Liang e Cris Luengo.
Questo commento non aggiunge una dipendenza runtime FFTW né autorizza uso di
altro codice FFTW. Nessuna licenza del prodotto viene scelta.

I copyright completi hanno SHA-256:

```text
gcc-runtime-copyright.txt
d7815dd2364180835891947d96d808878204dea45b8597602a7952332195fbea
glibc-copyright.txt
c31d10900fdb95d1889005145a2205bb75e3bc018d3ec8265b81364901748e09
```

L'inventario verificato collega ogni copia al percorso locale, target canonico,
hash e stat della fonte. Le copie aggiuntive dei testi common-licenses coprono
tutti i riferimenti dei documenti di pacchetto, compresi quelli di componenti
non attribuiti al worker; non sono una lista di licenze assegnate al prodotto.
NEEDED non identifica startup object, frammenti inline degli header di sistema
o ogni porzione incorporata nel worker. Obblighi di redistribuzione, disponibilità
dei sorgenti corrispondenti e applicabilità dell'eccezione richiedono review
separata. Questo inventario non è un parere legale o un gate commerciale chiuso.

## Verifiche, errore conservato e budget

Parsing/compile AST dei due script: superati, senza import del candidato.
Il primo verificatore è terminato con exit 1: una regex includeva il punto finale
nel riferimento `/usr/share/common-licenses/GPL.`. Ricevuta e sorgente della prima
esecuzione sono conservati, insieme agli output parziali. Il bug del parser è
corretto nel verificatore corrente; tre casi mirati sui riferimenti passano.
Nessun readelf, loader, build o candidato è stato rilanciato per questa correzione.

Il collector grezzo includeva 13 nomi di sonde STAP del loader nel campo
`defined_versions`, perché il parser proseguiva oltre la sezione versioni.
La normalizzazione usa soltanto le entry Rev/Flags/Index/Name della sezione
corretta e ne verifica il conteggio dichiarato da readelf. Il raw resta intatto;
il file autorevole per i campi derivati è
`verification-002/inventory-verified.json`. Il loader ha sette definizioni di
versione, non venti. Le 13 verifiche dei requisiti passano anche sui dati corretti.

Il verificatore corretto supera 379 controlli statici di artefatti. Non sono i
232 casi FFT, test di esecuzione o una review indipendente. Non è stato eseguito
un test/lint di prodotto, estraneo al perimetro autorizzato di questa unità.

Preflight prima di creare il namespace: scratch 957751296 byte allocati,
5472 inode. Preflight del collector dopo la scrittura dello script: 957779968
byte, quota 1073741824, riserva 67108864, margine 16777216, residuo oltre
riserva/margine 32075776. Budget nuovo output: 8388608 byte; nessuna riduzione
automatica. Hardlink deduplicati; symlink non seguiti nel conteggio globale.
Spazio filesystem disponibile nel preflight del collector: 125335482368 byte.

Ogni stdout/stderr dei tool è limitato durante la lettura a 1048576 byte;
deadline 15 s per comando. Superamento o errore causa stop, non retry o fallback.
Il collector non applica nuovi rlimit AS/RSS; non esegue carichi candidati.
Il namespace, incluse verifiche e copie aggiuntive, occupava 2273280 byte prima
del manifest finale. Il manifest/handoff finale registra la misura successiva.
La guardia scratch non è una quota kernel atomica.

## Artefatti autorevoli e consegna

Percorsi seguenti relativi al namespace runtime-inventory-003:

```text
capture.py, 21900 byte
c090255608c5d88fdbe1364730a3165330a22f9c026964329894dfcdb1d310d5
inventory.json, 92976 byte, acquisizione grezza conservata
b58403ac3d7260742bc21127659e3eb15681bef30151f5d45b9b043c7a9ba154
verification-002/inventory-verified.json, 104775 byte
48d47c8f77ba18b6644dcf7157fc23e37c0879f615d5751afa9781c8073cb901
verification-002/verification.json, 46494 byte
98824c16b2dc2fcb66e5b51f2614c8ef186b2f53235dd845ad4d043b01545b44
```

`artifact-manifest.json` lega i nuovi artefatti e questo report; `handoff.json`
lega il manifest e i risultati statici senza modificare gli handoff precedenti.
Consegna destinata a review indipendente sul testo e sui raw effettivi. Restano
aperti review, binding dinamico non osservato, provenienza/conformità distributiva
e qualifica numerica. Nessuna validazione può partire automaticamente da questo
record; il coordinatore deve trattare separatamente i gate ancora aperti.
