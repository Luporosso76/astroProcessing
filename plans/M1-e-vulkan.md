# M1.e: candidato Vulkan compute isolato

Stato: **IN PROGRESS, soltanto preparazione**. Nessun dato numerico GPU raccolto.
Questo piano precede qualsiasi dispatch del nuovo candidato. Non sostituisce i
probe storici, la loro evidenza o lo stato centrale. M1.c/d restano intatti.

## Ambito e prerequisiti

Sorgenti usa e getta soltanto in `tmp/m1/e-vulkan/`. Vulkan nativo C++, header e
loader locali 1.4.341; tool shader nel prefisso autenticato `tmp/m1-tools/prefix`.
Nessuna rete, installazione, codice Siril, modifica di Git o licenza di prodotto.
Preparazione autorizzata: verifica locale di provenienza, compilazione host e
shader, validazione e introspezione SPIR-V. Non avviare il binario compute,
worker di validazione, pilot o misure senza il via del coordinatore e review
indipendente dei sorgenti stabili. Nessuna misura concorrente.

Protocollo padre: `plans/M1-spikes.md`, SHA-256
`1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.
Questo hash riguarda il protocollo degli spike, non il piano scientifico lungo.
Non occorre rileggere quest'ultimo per questo lavoro.

Il manifest dei tool deve avere SHA-256
`7a3723f5ff1f37cdce923c6d390af220a34c672adb3fc817e81aa46b975cdb7f`.
Prima di eseguire payload: ricontrollare tutte le dodici voci, firma InRelease
con trust anchor Ubuntu pubblico locale, indice Packages.xz contro InRelease,
record dei due pacchetti contro indice, deb contro record, binari del prefisso
contro membri dei deb. Freshness e provisioning originale della chiave restano
non dimostrati. Non leggere chiavi segrete, HOME o cache personali.

## Identità hardware e risorse

Selezione nominativa unica: vendor `0x1002`, device `0x1681`, tipo
`VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU`, nome
`AMD Radeon Vega Mobile Gfx (RADV REMBRANDT)`, driver `radv`, device UUID
`00000000-e500-0000-0000-000000000000`. Richiedere driver ID RADV e coda compute
con timestamp validi. Zero o più di un match: STOP. Enumerare CPU/software è
ammesso, selezionarli no. Nessun tentativo su llvmpipe, lavapipe, GL o altra GPU.
API dispositivo osservata in precedenza: 1.4.335, Mesa 26.0.8; un cambiamento
viene registrato e richiede nuova serie, non viene nascosto.

Managed buffer massimi 256 MiB; RSS figlio osservata best effort, guardia
512 MiB. Il limite non impedisce universalmente OOM. Prima di ogni build e
successiva esecuzione verificare scratch globale `tmp/` tramite blocchi allocati,
cap 1 GiB, spazio libero e margine esplicito. Insufficienza: STOP/NOT_RUN, senza
riduzioni di shape, dtype o candidati. Nessun temporaneo esterno al progetto.
La GPU integrata usa heap unificati: registrare proprietà reali e allocation
requirements, non dedurre 512 MiB di VRAM dedicata dal vecchio probe GL.

## Contratto e fixture

f32 è la prima ricetta. f64 selezionato deve verificare `shaderFloat64` reale,
ma resta STOP finché shader, oracle e contratto f64 non sono validati. Mai cast
automatico a f32. Output finiti, confronto logico con tolleranza zero per i soli
casi analitici dyadic. Input NaN/Inf o subnormali nell'aritmetica sono fuori
contratto e producono STOP prima del dispatch; nessuna sanitizzazione.
Anche valori oltre l'ampiezza della fixture sono rifiutati, mai saturati: 1/2
per pixelwise, 1 per FFT spaziale e N² per lo spettro inverse analitico.
I controlli riguardano pixel/componenti logiche, non il padding di riga inutilizzato.
Un kernel uint separato prepara il controllo bitwise ±0/subnormali, senza
pretendere che la copia validi aritmetica su subnormali.

Riutilizzare in sola lettura `series-002/cases.json` e i path/hash immutabili
che esso riferisce in series-001/002. Non usare output candidati come GT e non
modificare originali. Selezionare affine, `convd` binomial 5x5 e warp `(1/4,3/4)`
su 17x19, 257x263 e 1024x1024, layout contiguo e stride +17 se presenti. Zero
padding. Affine `3*x/4+1/8`; pesi convolution prodotto `[1,4,6,4,1]/16`; warp
campiona `(x+1/4,y+3/4)` con pesi `[3,1;9,3]/16` e bordi nulli. Il GT intero
preregistrato ha denominatori 4096, 262144 e 16384 rispettivamente.

FFT complessa 2D radix-2, N=32/1024: impulso, costante, cos/sin periodo quattro
lungo ciascun asse. Forward segno negativo, non normalizzata; inverse segno
positivo e divisione per N². Forward contro spettro analitico indipendente;
inverse alimentata dal GT analitico, contro lo spaziale indipendente, mai dal
forward del candidato. Confrontare ogni componente reale/immaginaria. Per
twiddle esatti 1 e ±i usare costanti, evitando sin(pi/2) approssimato. Gli altri
twiddle del radix-2 restano generici; su queste fixture moltiplicano zeri o non
modificano il GT dyadic. Un residuo non nullo invalida il caso: niente epsilon
adattato ai risultati. La precisione FFT generale resta NON VALIDATA.

GLSL `precise`, build senza fast-math o contraction. Validare ogni SPIR-V e
ispezionare Float32, operatori floating-point e decorazioni NoContraction.
Registrare execution modes reali: non dichiarare FloatControls attivi se il
modulo non li contiene. Il dominio dyadic normale non richiede precisione su
subnormali o twiddle arbitrari. Nessuno SPIR-V generato o corretto a mano.

## Pilot e congelamento del riferimento, prima di numerica GPU

La primaria CPU per **ogni caso/operatore** deve provenire da un pilot separato,
con correttezza e profilo dichiarati, ed essere congelata in un lock JSON con
hash di sorgenti, binari, fixture, pilot e piano. Nessuna selezione sulle
21 coppie confermative. `series-002` può essere pilot storico soltanto per
casi/profili realmente compatibili: nessun suo campione entra nei nuovi CI.

Profilo primario matched: CPU FFTW f32 con piano/workspace riusati, GPU con
pipeline, descriptor, command pool e workspace riusati. Planning/compilazione
fuori dai warm di entrambi, misurati e dichiarati negli startup. Copie e
packing stride dentro T_total di entrambi. La selezione storica di FFT con
planning per chiamata non vale come pilot matched: occorre nuovo pilot CPU.
Il nuovo adapter nativo implementa C++ diretto e FFTW plan-reuse; un pilot che
seleziona Rust/NumPy non viene convertito automaticamente in C++. Finché manca
l'adapter selezionato, il confronto è NOT_RUN. Con soli riferimenti nativi non
si può proclamare la migliore CPU generale o un vincitore di stack.

Il lock CPU resta PENDING in questa preparazione. È un prerequisito bloccante
per qualsiasi raccolta numerica GPU, anche una validazione. Nessuna default
implicita del riferimento. Revisore e coordinatore devono verificare lock,
contratti e gate prima dell'esecuzione dinamica.

## Gate VALIDATE e FAULTS prima di CONFIRM

Sequenza obbligatoria: pilot CPU separato, lock CPU, VALIDATE, FAULTS, CONFIRM.
VALIDATE esegue una verifica CPU/GPU contro GT per caso/direzione/batch, senza
rapporti, CI o statistiche comparative. Questi dati non entrano nelle 21 coppie.
Inoltre esegue un controllo bitwise end-to-end separato: 16 word finite con
±0, subnormali, normali e massimi finiti; layout 8x2, stride 25, padding NaN
inutilizzato. Input 200 byte, output logico 64 byte. Oracle uint32 indipendente,
nessun float cast del payload e nessuna estensione del dominio aritmetico.

Le tracce contano i memcpy host e i vkCmdCopyBuffer registrati effettivamente;
submit accettata, fence completata e confronto bitwise verificano il percorso
upload/kernel uint/download. Non misurano traffico fisico della memoria unificata
né allocatori interni. Solo chiamate/dimensioni VkAllocateMemory dell'app sono
osservabili; byte degli allocatori interni restano null/NOT_OBSERVABLE.

Le nuove ricevute validate/faults sono write-once e legano ricetta, lock CPU,
fonti/binario/shader, protocollo padre/preregistrazione, descriptor e hash degli
originali/GT, più identità reale di device/driver/coda/timestamp. Hashano report,
fixture control e tracce chiuse. FAULTS richiede la ricevuta VALIDATE; CONFIRM
richiede entrambe, compatibili e PASS, con tutti i propri processi raccolti.
Assenza, mutazione, mismatch, FAIL o esito parziale fermano il ramo prima di
avviare qualsiasi worker, senza rieseguire automaticamente gate mancanti.
Il device osservato al nuovo ready deve coincidere prima del primo dispatch.

I worker vengono registrati nel proprio owner prima del Popen. Un errore del
costruttore dopo lo spawn raccoglie quel Popen; un errore in close non salta gli
altri. Gli errori originali e quelli di cleanup restano in gruppi strutturati.
Non si segnalano PID estranei o gruppi di processi. Regressioni constructor,
close multipli, protocollo parziale/oversize e ricevute incompatibili sono
preparate ma NON ESEGUITE in questo incarico.

## Misure confermative preregistrate, NON ESEGUITE

Tre cluster di processi nuovi p=1,2,3. Separare spawn/import/caricamento fixture,
creazione device, allocazioni, planning FFTW e pipeline creation dallo startup
totale esterno; `cold` non significa cache OS/driver/storage svuotate.
Per ogni caso e batch 1/8 sequenziale: due warm-up, poi sette round r=1..7 per
cluster. Conservare i due processi CPU/GPU e alternare richieste in round-robin;
il candidato inattivo non esegue kernel. Ventuno coppie dello stesso caso,p,r.
Nessuna compilazione CPU nascosta nel warm contro compilazione GPU esclusa.

T_total workspace-reuse: input host pronto, precondition scan, packing e copia
staging host, upload reale, barrier, tutti i dispatch, fence, download reale,
copia host e result-ready. Allocazioni iniziali fuori warm, registrate come
setup di entrambi; nessuna confusione con una ricetta all-inclusive per chiamata.
GT comparison separata, obbligatoria e non inclusa nel tempo prestazionale.
Riportare anche command recording, submit call, fence wait, query raw/validBits/
timestampPeriod e intervalli GPU upload/kernel/download; non scegliere soltanto
T_device. La nuova API raccoglie delta reali getrusage ru_utime/ru_stime per
richiesta, inclusi verifica GT e tracing, escluso startup; risoluzione nativa
microsecondi convertita in ns, non precisione al ns. Heap, VkMemoryRequirements,
byte copiati, CPU user/system, RSS e thread saranno osservati dinamicamente.
Nessuna osservazione engine è stata eseguita qui. Driver interni e thread transienti restano
limiti, non contatori inventati.

Statistica primaria `exp(median(log(T_CPU[p,r]/T_GPU[p,r])))`. Bootstrap gerarchico
appaiato, 2000 ricampionamenti, seed 4059: tre cluster con reinserimento, sette
coppie di round dentro ciascun cluster estratto; medesimi indici per i candidati.
Percentili 2,5/97,5. Soltanto tre cluster: inferenza limitata a questa serie.
Startup, batch e operatori restano separati. Beneficio condizionato al riferimento
congelato solo con CI interamente sopra 1, correttezza ed error gates chiusi;
nessun vincitore generale se cambiano modello di lavoro o profilo.

## Errori, prove e chiusura

Preparare adapter mismatch/UUID/tipo CPU, budget e allocation failure, bad shader,
submission DEVICE_LOST iniettata, input non finite, output mutato, f64 non
contrattualizzato. Errori: nonzero STOP, diagnosi, nessun risultato pubblicato,
nessun cambio backend. Tracce distinguono vkCmdDispatch registrato, submit
accettata e fence completata; un record command non prova kernel completato.
Non inventare contatori CPU/GPU alternativi o limiti sugli interni del driver.

Artefatti: sorgenti, `preparation-<serie>.json`, SPIR-V e audit statico, build log e hash
in `tmp/m1/e-vulkan/`; eventuale ricevuta in
`docs/evidence/M1-e-vulkan-preparation.md` soltanto con comandi realmente eseguiti.
Il follow-up usa soltanto la nuova `docs/evidence/M1-e-vulkan-followup.md`.
Fonti e ricetta precedenti sono congelate write-once in
`tmp/m1/e-vulkan/snapshots/pre-followup-001/`; vecchie build/ricevute non cambiano.
Nessuna misura in questa consegna. Rollback: fermare solo propri figli e
conservare artefatti; niente modifiche a serie altrui o history Git.

Gate aperti: review indipendente, pilot e lock CPU, hardware preflight del
prototipo, numerica GPU, fault tests, 21 coppie confermative. Nessun compute PASS.
Prossimo comando consentito in preparazione:
`/usr/bin/python3 -I -B tmp/m1/e-vulkan/prepare.py --build --series 004`.
