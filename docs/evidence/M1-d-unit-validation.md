# M1.d UNIT33-1: prima esecuzione controllata

2026-10-25. Autore: modello dichiarato dal runtime `openai/gpt-6.1-sol`.
Esito della sola ricetta `M1.d-unit-primary-1`: **UNIT_LIFECYCLE_COMPLETE**.
Una invocazione, exit 0, dodici test eseguiti e PASS, zero skip/error/failure,
zero NOT_RUN. Nessun retry, correzione dopo esecuzione o cambio di id/ricetta.
Questo risultato **non è M1.d PASS**.

## Ammissione, pin e comando effettivo

Turno esclusivo `m1d-unit33-001`, autorizzato in chat dopo la review indipendente
`docs/evidence/M1-d-unit-static-review.md`. Il supervisore esterno usa soltanto
stdlib. Ha verificato prima di qualsiasi import candidato:

- Preparazione: `17d94b0ce16116866a05839d8dad22a0442b1cdd82c0302ef04bc19d1172a8e2`.
- Tutti i quindici file elencati dal manifest, compresi tre sorgenti nuovi,
  piano/evidence di preparazione, receipt e snapshot v5.
- Otto sorgenti v5 originali contro i pin delle copie e manifest v5
  `b45cc3bb16043e50e2b82b6d8d7fc54eddb21bf3dc82dec50e973b7e1a3741ab`.
- Protocollo `plans/M1-spikes.md`:
  `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.
- Piano scientifico:
  `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`.

Nessun piano o stato è stato riscritto. Gli stessi pin risultano invariati dopo
il processo e nella rilettura degli artefatti. Runtime effettivo:
`3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`, recursion limit 1000.

`unit.lock` era assente e `lifecycle-01` assente. Il supervisore ha aperto i
componenti dei percorsi con O_NOFOLLOW. Il lock creato dal driver risulta file
regolare, UID 1000, modo 0600, nlink 1. Output e directory HOME/TMP/XDG sono
0700, UID 1000; nessun cambio di permessi è stato usato per superare un gate.
Il contratto cooperativo esclude fd/mapping/lettori esterni sui fixture;
non si dichiara un audit globale dei descrittori del sistema.

Comando del supervisore eseguito una volta, dalla radice:

```bash
OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S tmp/m1/d-unit/validation-supervisor-001.py
```

Unico Popen candidato, argv effettivo con quota fresca:

```bash
OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S tmp/m1/d-unit/run.py --preparation-sha256 17d94b0ce16116866a05839d8dad22a0442b1cdd82c0302ef04bc19d1172a8e2 --exclusive-admission-id m1d-unit33-001 --external-scratch-bytes 868347904 --private-no-external-readers --output tmp/m1/d-unit/lifecycle-01
```

HOME, TMPDIR/TMP/TEMP e XDG_CONFIG_HOME/CACHE_HOME/DATA_HOME/STATE_HOME/RUNTIME_DIR
puntano a sottodirectory private di `tmp/m1/d-unit/validation-001/environment/`.
L'ambiente child è costruito da una lista esplicita, senza credenziali o variabili
personali ereditate. Dopo il run quelle directory non contengono file.
Non è una prova syscall di assenza di ogni accesso esterno: stdlib e loader
leggono i normali file runtime. Nessun dato/cache candidato esterno è stato richiesto.

## Preflight globale reale e risorse

Il conteggio ha incluso metadata di TUTTO `tmp/`, non solo d-unit, usando
`st_blocks*512`. I link simbolici non vengono seguiti; gli hard link sono contati
per voce del namespace, conservativamente. La quota esterna comprende quindi
gli altri output e le prove precedenti conservate, non un vecchio dato stimato.

| Misura | Byte |
|---|---:|
| Globale prima della receipt preflight | 868626432 |
| Locale d-unit prima della receipt | 278528 |
| Quota esterna fresca, globale meno locale | 868347904 |
| Globale dopo receipt, immediatamente prima del child | 868630528 |
| Locale alla partenza e nel risultato del driver | 282624 |
| Riserva primaria | 71839744 |
| Globale alla partenza più riserva | 940470272 |
| Cap comune | 1073741824 |
| Margine all'ammissione | 133271552 |
| Globale dopo il child | 940183552 |
| Incremento osservato durante il child | 71553024 |
| Globale dopo persistenza stdout/stderr | 940195840 |

Il preflight è stato scritto una volta, prima dei dati, con ambiente, pin, quota,
CLI e limiti. Dopo quella scrittura il supervisore ha ricontrollato che la quota
esterna fosse identica e che globale+riserva rientrasse nel cap. Nessun refresh
automatico o riuso del preflight. Una riserva separata di 65536 byte proteggeva
la scrittura della sola receipt preflight, prima della verifica finale.

Supervisione: deadline 300 s; RSS obiettivo best effort 536870912 byte, campionato
su `/proc/<PID-proprio>/status` e integrato con ru_maxrss dopo wait. RSS massimo
osservato **30089216 byte**. RLIMIT_AS 1073741824 byte, dichiarato prima dei dati:
lascia spazio alle serializzazioni complete della ricetta e non prova un hard RSS.
RLIMIT_FSIZE 71341727 byte, compatibile con l'arena; core dump disabilitati.
Guard stdout 980091 e stderr 23569774 byte: un superamento avrebbe fermato il
child, senza convertire log troncati in PASS. Ricevuti integralmente 10256 byte
stdout e zero stderr. Nessun guard attivato, quattro campioni globali.

Tempo diagnostico del supervisore 0.9419495350448415 s; unittest riporta 0.630 s.
Non sono campioni di benchmark o misure scientifiche preregistrate.
Le osservazioni non escludono picchi intermedi non campionati e non costituiscono
quota OS o massimo universale dei metadata. La riserva metadata resta
ingegneristica. Il latch vieta COMPLETE dopo una violazione osservata, non crea
una barriera universale contro ogni operazione dopo eccezioni catturate.

## Dodici verdetti effettivi

Tutti i casi riportano `execution=EXECUTED`, `outcome=PASS` nel JSON stdout,
nel result committed e nel runner interno. Non sono conteggi ricavati dal piano.

| Test | Esito |
|---|---|
| test_durable_receipt_precedes_release_and_blocks_are_observed | PASS |
| test_verification_failure_keeps_full_file | PASS |
| test_receipt_sync_failure_keeps_full_file_and_staging | PASS |
| test_mutation_early_cleanup_is_rejected | PASS |
| test_mutation_omitted_receipt_sync_is_rejected | PASS |
| test_partial_quota_is_persistent_not_a_returned_reservation | PASS |
| test_unproven_phase_bound_never_admits | PASS |
| test_exported_mapping_blocks_release_until_closed | PASS |
| test_slot_short_write_failure_rejects_reuse | PASS |
| test_slot_fsync_failure_rejects_reuse | PASS |
| test_slot_flush_failure_rejects_reuse | PASS |
| test_enospc_short_write_preserves_error | PASS |

La suite conserva sette casi originali e cinque aggiunti. Undici corpi test v5
sono ereditati; il caso negativo di verifica è l'adattamento primario già
preregistrato, con digest errato contro file reale. Nessuna assertion scientifica
o PROOF_STATUS è stata modificata. Failfast era attivo, ma non ci sono stati
errori inattesi: il percorso STOP/NOT_RUN del runner non viene attestato come
esercitato da questa invocazione positiva.

## Fault dichiarati e primitive reali

Gli spy fsync/unlink/Slot.write verificano l'ordine e delegano alle primitive
reali salvo i fault esplicitamente selezionati dai test. Non simulano l'intero
filesystem o i metadati. File/arena sono reali, con pwrite, fsync, mmap e close.

- Verifica negativa: digest atteso volutamente errato, nessuna sostituzione del
  verificatore scientifico 256 MiB. Il fixture resta integro.
- Fsync receipt: OSError iniettato; staging intent rimane e fixture resta.
- Cleanup anticipato e fsync receipt omesso: mutazioni intenzionali respinte
  dagli assert prima dell'unlink; nessun file fallito viene eliminato.
- Mapping: memoryview esportata reale; mmap.close solleva BufferError. Il test
  verifica registro mapping vivo, rilascio vietato, poi vista rilasciata e
  close_mapping riuscito. Non è un mock di BufferError.
- Slot short-write: prima pwrite nativa di due byte, poi ritorno zero iniettato;
  intent e payload restano. Fsync finale e flush falliti sono OSError iniettati.
  Riuso/read/replace da vecchio e nuovo oggetto vengono respinti senza riparazione.
- ENOSPC: codice SPILL_ENOSPC generato dall'iniezione deterministica v5, non da
  esaurimento del volume. Richieste verificate `(0,4096)`, `(2048,2048)`; native
  pwrite scrivono 2048+2048 byte nel file reale da 4096. Non copre altri split.
- Il test UNPROVEN esercita il rifiuto del costruttore Ledger, non l'ammissione
  OS/FS Gamma o il main v5. Il Job ENOSPC non avvia worker completi.

## Durabilità, readback e conservazione

Il solo unlink positivo segue gli assert del protocollo primario: consumer
propri chiusi, raw flush, lettura/hash reale, receipt staging fsync, final fsync,
readback digest e identità, nuova verifica consumer, unlink, fsync directory,
rilascio osservato e receipt RELEASED. La prova d'ordine deriva dagli spy/assert
eseguiti; non è una traccia syscall indipendente né una prova power-loss.

Readback offline dell'arena, senza import candidati o nuova invocazione test:

- 70 header/payload letti agli offset finiti. Arena logica 71341727 byte,
  allocazione osservata 71348224 byte; non si è assunto l'arrotondamento teorico.
- Receipt positiva committed 1, 731 byte, SHA-256
  `217a8bbc8a4d5e4b76f385b62001437d63f3d55f4d948dd6a5e143644ee057b6`.
- Release positiva committed 1, 223 byte, digest receipt corrispondente,
  `namespace_allocated_bytes_removed=4096`. Il relativo fixture è assente.
- Undici fixture da 33 byte restano integri, SHA-256 comune
  `6ed6653dba3d6aee8110c6e23de37f15c609b8a6e29aa164d1acace604e5cb7e`.
- File ENOSPC conservato: 4096 byte logici/allocati, SHA-256
  `80f1830e2934a1c06ceb7512d00bb936a9437c80411da172c1a274238b974795`.
- Receipt staging del fault fsync: header `(used=715, committed=2)` conservato.
- Short-write fault: `(6,2)`, payload hex `4f4b61620000` conservato.
- Fsync finale fault: header disco `(6,1)`, payload `4f4b61626364`; il nome resta
  terminale nel risultato. Header committed non equivale a sync riuscito.
- Flush fault: header disco `(2,1)`, payload `4f4b`; nome terminale registrato.
- Result e primo staging committed 1, 9594 byte identici; runner committed 1,
  1483 byte. Trace/error e secondo staging result vuoti, non omessi o cancellati.
  Le quattro regioni terminali sono elencate nel result e nell'inspection.

Nessuna rmtree, cleanup di failure, riuso degli staging o cancellazione di prove.
Le eccezioni intenzionali catturate non popolano la trace degli errori inattesi.
Il budget massimo di ricorsione/trace non è stato raggiunto e non viene validato
empiricamente da questo run riuscito.

## Artefatti e hash

Directory prove: `tmp/m1/d-unit/validation-001/`; output primario:
`tmp/m1/d-unit/lifecycle-01/`. JSON/log primari sono regioni dell'arena,
non file separati inventati dall'ispezione. `inspection.json` registra offset,
header e SHA-256 di tutti i payload, runner completo e ricevute decodificate.

| Artefatto | SHA-256 |
|---|---|
| validation-supervisor-001.py | 45316efe5332603fc8a602895756692c9f54ae3089bd5768fd0f012fb88184e2 |
| validation-001/preflight.json | 109f82d224974b4a64194bfd5e5c0dc7925f042b19a9fde1809f78efe4dcd47e |
| validation-001/supervision.json | 1eeb5dab8683a118cb260d96152e68e5ef98eafd367a6ad053364b7834f6ac2e |
| validation-001/stdout.txt | 9abafbea0915ed150c0582e05c693aa40614fe4e97416917cadc00a542802e3f |
| validation-001/stderr.txt | e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| validation-001/inspect-artifacts.py | 3d65394378917739b88cd7b48045411c3c336b1cd1a9a5447ec40654ac0691b4 |
| validation-001/inspection.json | 527feb77de6eff2198e7803caae18fc016d4d2122d754a3519f7326489a5131d |
| lifecycle-01/unit.arena | 74368ac04303bcd62a1987f41f4bdfb348a71f1a55fec844b487fb1d891fa28a |

Comando di readback effettivamente eseguito una volta, senza test:

```bash
OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S tmp/m1/d-unit/validation-001/inspect-artifacts.py
```

Il supervisore ha atteso/raccolto il solo PID Popen **270444**, exit 0. Nessun
segnale inviato, nessun altro PID fermato. Pipe chiuse e processo raccolto;
le directory private e gli artefatti rimangono. Non si dichiara un audit dei
processi estranei. Nessuna ricerca filesystem, rete, installazione, Git,
modifica di indici, licenze, Siril, progress o sorgenti congelati.

La verifica conclusiva dei pin e del cap, dopo il readback e questo report,
è registrata in `tmp/m1/d-unit/validation-001/final-audit.json`. Separa il
conteggio prima della propria scrittura dai campioni runtime: non sostituisce
il preflight e non avvia altre prove. Le scritture di documentazione/ispezione
successive al child non fanno parte dell'incremento primario riportato sopra.

I 21 bounded, i 15 errori attesi, i tre stream full da 40 GiB e lo spill fisico
256 MiB restano separati e non eseguiti in questo turno. Nessuna prova del pool
32 MiB sotto carico, hard RSS, benchmark, misura formale o M1.d complessivo PASS.
