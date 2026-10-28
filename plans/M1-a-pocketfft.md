# M1.a: qualifica separata pocketfft, preparazione

## Ambito

Stato del gate numerico: PLANNED. Questo file preregistra una qualifica C++ CPU
isolata, non un nuovo confronto di linguaggi e non un ampliamento del MVP.
Obiettivo: verificare un'opzione FFT con grant permissivo senza assumere che FFTW
GPL diventi una dipendenza del prodotto dopo M1. Nessuna adozione qui.

I confronti storici C++/Rust con FFTW 3.3.10 restano validi e immutabili. Non
diventano misure pocketfft. Nessun nuovo risultato viene attribuito a Python,
NumPy o Rust. Un eventuale binding Rust richiede in futuro ABI e versione fissati.
La licenza del prodotto resta riservata al proprietario.

## Prerequisiti

- M0 chiusa; spike isolati in `tmp/` ammessi, sorgenti di prodotto esclusi.
- Fonte documentale: `docs/evidence/M1-fft-licence-candidates.md`, 94 righe.
  Il gate documentale non dimostra numerica, sicurezza completa o diritti su ogni
  dipendenza futura.
- Header upstream già acquisito, commit
  `4d865cba24359b05b37da4e75aee24498c3b78e7`, 121769 byte, SHA-256
  `518a95c552aeaee352aa30122137cd9e135d23c91e2863d9272aaa758744c86d`.
  `tmp/m1-fft-tools/LICENSE.md`, 1498 byte, SHA-256
  `a85ca13fdf90160b64a0698215868c13b74d835ad0a4e2ba44713b8c5058a056`.
  Non modificare, ricopiare senza notices o rimpiazzare gli input upstream.
- Review indipendente dei file effettivi prima della build. Prima della qualifica
  serve anche review della build e dell'inventario runtime effettivo. Non esiste
  un'auto-approvazione. Questa preparazione non esegue tali gate.
- Turno esclusivo del coordinatore prima di ogni futura esecuzione; ricalcolare
  quota globale e disponibilità strumenti locali. Nessuna installazione o rete.

## Deliverable

Solo `tmp/m1/fft-pocket/`, questo nuovo piano e
`docs/evidence/M1-a-pocketfft-preparation.md`, con followup separato
`docs/evidence/M1-a-pocketfft-followup.md`. L'adapter è header-only; il worker
CLI serve solo allo spike. Il supervisore e l'oracle usano Python stdlib.
Il nuovo `followup-manifest-001.json` fissa byte/hash di sorgenti, piano, upstream, flag e
parametri oracle prima di qualsiasi dato. I risultati futuri vivono in namespace
nuovi sotto la stessa directory, mai sovrascritti.
Il profilo corrente è `followup-manifest-003.json`, con provenienza dal 002.
La preregistrazione locale del budget build segue il FIRSTFAIL AS256, senza
modificare la baseline o `plans/M1-spikes.md`. Il nuovo record è
`docs/evidence/M1-a-pocketfft-build-budget-preparation.md`.
Gli otto file della prima consegna, incluso il report di 148 righe, sono salvati
prima delle modifiche in `snapshots/pre-followup-001/`, con hash e mode originali
nel manifest dello snapshot. Copie write-protected, creazione esclusiva; nessuna
prova di immutabilità contro un proprietario privilegiato. Manifest/record/report
originali restano storici, non attestano i sorgenti corretti.

Il validatore v3 esistente importa NumPy, moduli del precedente confronto e
fixture della serie storica: l'interfaccia letta non è adatta a questa qualifica
stdlib e procedurale. Nessun suo import, modifica o bypass. Il piccolo `Popen`
locale usa deadline, pipe non bloccanti, log limitati, cleanup del proprio gruppo
e sole directory private. Non è un framework di autenticazione o process-tree.

## Ordine e gate

1. PREP: verificare pin upstream, leggere il contratto API, scrivere sorgenti e
   preregistrazione; solo AST Python e controlli testuali. Nessun compilatore,
   import di codice proprio, worker, FFT, test o benchmark in questo turno.
2. REVIEW: leggere direttamente i file e il manifest. Qualsiasi finding blocca
    il solo ramo colpito. Non consumare un turno runtime per correggere alla cieca.
   I due finding della review ricevuta riguardano identità del gruppo processi
   e fsync delle directory; le correzioni richiedono nuova review, non auto-PASS.
3. PREREGRESSIONI, futuro primo turno prima di qualsiasi compilatore: dopo review
    e ammissione del coordinatore, eseguire 36 metodi stdlib in `regressions.py`.
   Mock di waitid/gruppi/pipe e barriere, nessun processo reale o dummy, worker,
   FFT o compilatore. Casi: leader OWN vivo e zombie riservato; identità persa,
   reaped, sessione errata; nessun segnale dopo reap; fallimenti signal/census/reap
   e close indipendenti; blocco next-launch; SIGCHLD non default rifiutato;
   run senza poll/wait; fsync file/directory prima erase, omissioni e post-erase;
   mkdir/JSON; ordine pubblicazione C++ con discriminatore di omissione testuale.
    I 20 metodi originali restano invariati. Sedici nuovi metodi mock coprono
    selezione build/validate, ordine prima dei hard limit, budget invariati,
    errori setrlimit/getrlimit, readback, hard ereditato insufficiente, coerenza
    del profilo e rifiuto delle ricevute con vecchio conteggio o pin.
    I mock non certificano processi reali né power loss. I 36 sono PLANNED qui.
   Il turno corrente resta a Ftiny: nessuna preregressione avviata in PREP.
4. BUILD, futura: verificare nuovamente tutti i pin; creare `build-followup-003` con mkdir
    esclusivo. Scrivere manifest prima della build; acquisire versione GCC,
    compilare e registrare hash del binario, exit e metadati ELF. Stop su errore.
    Il comando richiede la ricevuta completa delle 36 preregressioni con hash del
    manifest 003, oltre a review/ammissione esterne. Nessuna ricevuta corrente esiste.
    Le 20 preregressioni PASS del 002 e la loro review non ammettono nuove fonti.
    La build precedente resta FIRSTFAIL, senza retry automatico né overwrite.
5. Inventariare e rivedere runtime effettivi, risoluzione del loader, versioni,
   byte/hash e licenze separate. `readelf -d -V` espone NEEDED/versioni, non
   risolve da solo l'inventario completo o le sue licenze. Conservare notices
   dell'header incluse attribuzioni interne, oltre a LICENSE BSD-3-Clause.
6. VALIDATE, futura e separatamente ammessa: verificare manifest e build-pin;
    creare `validate-followup-003`; scrivere manifest prima dei risultati. Prima 14 errori,
   24 ingressi non finiti e due collisioni; poi 192 fixture finite, una alla volta.
   Non cancellare un fallimento; conservare FIRSTFAIL e intermedi del caso.
7. Review indipendente di formule, risultati e copertura. Solo dopo correttezza
   e inventario runtime si potrà preregistrare un esperimento prestazionale nuovo.
   Questa unità non contiene API di benchmark e non chiude M1.

## Contratto scientifico

Array complesso H per W, assi ordinati `{0,1}`, ordine riga-colonna, stride in
byte `{(W+P)*sizeof(std::complex<T>), sizeof(std::complex<T>)}`, P in `{0,17}`.
Input e output hanno stesso dtype, forma e layout. Nessun in-place o alias
parziale: due vettori distinti di oggetti effettivi `std::complex<T>`.
Il formato file è header ASCII canonico
`PFFT1 DTYPE H W P DIRECTION\n`, seguito da coppie real/imag IEEE little-endian,
incluso il padding. Nessuna conversione f32/f64; il worker legge scalari T e
costruisce oggetti complessi, senza reinterpretare byte arbitrari come oggetti.

`c2c<T>` riceve `fct` di tipo T, non sempre double. Forward usa esponente
negativo e `fct=T(1)`. Inverse usa esponente positivo e `fct=T(1)/T(H*W)`.
Le forme ammesse sono esclusivamente 32 quadrato e 1024 quadrato; normalizzazione
e coefficienti delle fixture sono diadici esatti nei due dtype. Il fattore si
applica una volta, come indica `general_nd`, poi l'upstream usa 1 sugli altri assi.

Ricetta primaria: GCC, Linux x86_64, `-std=c++17 -O2 -fexceptions
-fno-fast-math -ffp-contract=off -fno-tree-vectorize -fno-tree-slp-vectorize
-fno-strict-aliasing -march=x86-64 -mtune=generic -Wall -Wextra -Wpedantic`.
Macro fissate nell'adapter: `POCKETFFT_NO_MULTITHREADING`,
`POCKETFFT_NO_VECTORS`, `POCKETFFT_CACHE_SIZE=0`; nthreads esplicitamente 1.
Eccezioni abilitate; vietati NO_EXCEPTIONS, override di namespace e allocatore
POSIX opzionale. Rounding FE_TONEAREST richiesto, non corretto automaticamente.
La disabilitazione SIMD upstream non sostituisce i flag di autovettorizzazione.

Il pubblico upstream accetta `std::complex<T>*`, ma internamente usa viste
`detail::cmplx<T>` tramite cast. La garanzia standard di accesso ai componenti
scalari di std::complex non prova aliasing legale con qualsiasi struct.
Questa qualifica è quindi esplicitamente GCC con `-fno-strict-aliasing`, oggetti
tipizzati e controllo del layout; non rivendica portabilità ISO generale,
assenza di ogni rischio di lifetime upstream o audit completo. La review deve
valutare questo confine. Nessun altro sorgente FFTW o Siril viene usato.

## Fixture e metrica preregistrate

Descrittori matematici nuovi, senza rileggere i circa 847 MB storici e senza
riusare il precedente codice di errore. Sei famiglie, ciascuna con ampiezza 1/2
sulla componente reale e poi ruotata per i sulla componente immaginaria:

- impulso nell'origine: spettro costante 1/2 su TUTTI i bin;
- costante 1/2: solo DC vale HW/2;
- sinx e siny: ripetizione esatta `[0,1,0,-1]/2` sull'asse scelto; spettro nullo
  salvo frequenze N/4 e 3N/4 su quell'asse, altro asse zero; valori rispettivi
  `-i*HW/4` e `+i*HW/4`;
- cosx e cosy: ripetizione esatta `[1,0,-1,0]/2`; stessi due bin, entrambi HW/4.

Forward riceve la funzione spaziale e confronta lo spettro analitico. Inverse
riceve direttamente lo spettro analitico, mai l'output del forward, e confronta
la funzione spaziale. La rotazione per i opera sui numeratori come `(-im,re)`.
L'oracle usa interi con denominatore 2, nessuna FFT, trigonometria o backend
numerico condiviso con il candidato. I valori restano esattamente rappresentabili
in f32/f64; questo NON garantisce che un algoritmo FFT li riproduca esattamente.

Matrice: 2 dtype per 2 forme per 2 padding per 2 direzioni per 6 famiglie per
2 componenti di ampiezza = 192 casi finiti. Ogni caso verifica entrambe le
componenti di TUTTI i bin, comprese le attese zero. Tolleranza assoluta 0 e
relativa 0, come il profilo originale; +0 e -0 numericamente uguali. Nessun
roundtrip-only. Se il gate ZERO fallisce, STOP con indice, componente, valore
hex e numeratore atteso. Vietati epsilon nuovi, oracle non-throwing, cambio
algoritmo, riduzione di qualità, precisione o copertura.

Padding P=17: sentinel quiet-NaN real/imag esplicita nell'input, non letta dal
worker e non parte dell'array attivo; l'output deve mantenere sentinel NaN nei
soli slot padding. P=0 non contiene sentinel. Un caso alla volta, righe scritte
in streaming, niente dataset massivo iniziale; massimo file circa 17 MiB.

Quattordici errori base concreti: f16; altezza `+32`; larghezza `32junk`;
padding `-1`; direzione `FORWARD`; altezza zero; intero
`18446744073709551616`; padding `18446744073709551615` con overflow;
token aggiuntivo; forma 16384 quadrato oltre budget; header dtype errato;
header layout errato; file troncato; byte finale aggiuntivo.
Altri 24 casi: f32/f64 per forward/inverse per real/imag per NaN/+Inf/-Inf,
sempre nell'ultimo campione attivo. Due collisioni: output o staging già
esistente con marker da preservare. Errori attesi: exit 2, stderr `STOP: `,
stdout vuoto, nessun nuovo output/staging. Non richiedono invocazione FFT.

## Risorse e pubblicazione

Preflight prima di allocare/chiamare: token esatti, dtype e IEEE identici,
dimensioni non nulle e ammesse, somma/prodotto senza overflow, offset in
ptrdiff_t, byte/stride e dimensione file esatti, header identico, real/imag attivi
finiti. Input/output propri non aliasati e allineati dai vettori tipizzati.
Memoria di lavoro configurata 256 MiB; in validate RLIMIT_AS del supervisore e
di ogni worker resta 268435456 byte. I due
buffer espliciti non possono superare insieme 128 MiB, lasciando spazio a
workspace/runtime. Un bad_alloc non cambia backend né formato.

Per il solo comando build il nuovo budget RLIMIT_AS è 1073741824 byte per il
supervisore e per ciascun figlio ereditante, inclusi compilatore e metadati.
È un budget proposto, non il fabbisogno misurato né un cap aggregato GCC.
La decisione architect `ses_e88701970ffeS6qAT6ryjpDEvc` distingue i buffer
pianificati 256 MiB, RSS osservato 512 MiB e scratch 1 GiB del piano M1-spikes
dal precedente AS256 locale applicato anche al compilatore. Questa modifica
resta PREP e richiede review indipendente prima delle regressioni e di una
nuova build separata; non ammette automaticamente nessuna esecuzione.

Validare prima arità e sottocomando. Leggere tutti gli hard limit ereditati
prima di ridurne uno. Hard insufficiente, errore setrlimit o readback diverso
producono STOP; non usare min(limit, hard), launcher, preexec o ripieghi.
Registrare limiti richiesti, ereditati ed effettivi del supervisore, con stato
e diagnosi; i figli li ereditano, senza misurazione indipendente. Se il setup
fallisce prima del namespace, la diagnosi strutturata resta su stderr.

RSS 512 MiB è best effort: RLIMIT_RSS non è un limite affidabile su Linux;
campionare VmRSS del solo leader child e fermare oltre soglia, registrare se non osservato.
Non misurare o dichiarare il picco dell'intero albero compilatore. Log child
massimo 65536 byte, file massimo 64 MiB, deadline 180 s build, 120 s worker,
10 s metadati. I tempi sono deadline, non campioni prestazionali.

Quota globale tmp 1 GiB sui blocchi allocati, conteggio metadata con hardlink
deduplicati, senza leggere fixture storiche. Riserva 64 MiB e margine 16 MiB
prima di avvio/fixture, controllo periodico durante child. È un guard, non una
quota kernel atomica: servono turno esclusivo e review del budget anche per la
build. Nessuna garanzia contro scrittori esterni concorrenti o burst non
osservati. Root nuova mode 0700; HOME, XDG_CACHE_HOME e TMPDIR solo in cache
private sotto ciascun child. Nessun temporaneo esterno o cleanup altrui.

Il supervisore è l'unico waiter e rifiuta SIGCHLD non default. Osserva il leader
con `waitid(WEXITED|WNOHANG|WNOWAIT)`, verificando sessione e gruppo uguali al PID
del child creato con start_new_session. Il leader resta non reaped durante ogni
segnale al gruppo OWN. Identità persa o returncode già raccolto vietano killpg.
Solo dopo i segnali e il census del gruppo viene tentato il reap, senza poll/wait
Popen; nessun segnale successivo. Census/reap/chiusura selector e di ciascuna
pipe sono tentativi indipendenti con errori raccolti. Un cleanup irrisolto blocca
ogni lancio successivo e l'adozione dei risultati; nessun successo no-orphan viene
dichiarato in quel caso. Il census riguarda membri vivi del gruppo OWN, non
discendenti ostili che cambiano sessione: non è un framework process-tree.

Il worker esegue FFT e controllo finiti prima di creare staging. Scrive staging
con O_EXCL/O_NOFOLLOW, fsync/close, pubblica con link senza overwrite; un errore
gestito rimuove solo artefatti propri. Un kill può lasciare staging privato:
non adottarlo. Anche output completo resta candidato non adottato finché
l'oracle non passa, scrive e sincronizza la ricevuta. Il worker sincronizza la
directory dopo creazione staging, dopo link e prima di unlink, poi dopo unlink.
mkdir e JSON sincronizzano le directory pertinenti; input, danni e marker hanno
barriere file/directory. Il supervisore fsync del log, poi directory del caso e
del log, PRIMA di eliminare intermedi; dopo unlink risincronizza la directory.
Errore di barriera prima erase conserva tutti gli intermedi; dopo erase produce
STOP e nessun complete. Sono ordinamenti da verificare, non una prova di power
loss o una certificazione del filesystem/storage. Fallimenti conservati; nessun
resume differente. I 232 casi scientifici e il contratto ABI non cambiano.

## Evidenza, rischi e chiusura

Preparazione: `docs/evidence/M1-a-pocketfft-preparation.md`; conteggi e hash nel
manifest storico e nel record statico. Correzioni e nuovi pin:
`docs/evidence/M1-a-pocketfft-followup.md`. Nessun report sostituisce la review.
Runtime GCC/libstdc++/libgcc/libm/libc e loader restano dipendenze da inventariare
separatamente dalla licenza BSD dell'header. Nessuna licenza di prodotto scelta.

Rollback: non integrare lo spike nel prodotto e lasciare i fallimenti intatti.
Input storici/upstream immutabili. Pin cambiato invalida build e risultati;
correzioni richiedono nuovo manifest e namespace fresco, mai overwrite o
reinterpretazione retroattiva. Matrice centrale e progress non modificati in
questa unità bounded; coordinatore integra solo dopo review.

Prossimo comando, SOLO dopo review PREP e assegnazione del turno, NON ESEGUITO:

```sh
python3 -I -B -S tmp/m1/fft-pocket/regressions.py preregressions-003
```

Dopo PASS reale delle preregressioni e relativa review, NON ESEGUITO:

```sh
python3 -I -B -S tmp/m1/fft-pocket/supervisor.py build build-followup-003 preregressions-003
```

Comando successivo, SOLO dopo inventario/runtime e review della build,
NON ESEGUITO:

```sh
python3 -I -B -S tmp/m1/fft-pocket/supervisor.py validate validate-followup-003 build-followup-003
```

Questa consegna non decide adottabilità, prestazioni, linguaggio o sicurezza
complessiva. Gate numerico e review restano aperti.
