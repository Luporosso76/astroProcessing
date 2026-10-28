# M1.d v3: preregistrazione del lifecycle, prima delle prove

Data: 2026-10-25. Modello dichiarato dal runtime: `openai/gpt-6.1-sol`.
Mandato: correggere il nuovo finding lifecycle della review indipendente
`ses_e8c11b44bffe2NSoGlS6xqlh6a`. Questa ricevuta non è una review indipendente.
Preparazione soltanto: nessun import del candidato, test, worker o benchmark.
Nessuna ammissione runtime è stata concessa o consumata in questo incarico.

## Perimetro e snapshot prima delle patch

Letture iniziali effettive: `tmp/m1/d/pool.py`, 539 righe;
`tmp/m1/d/run.py`, 704 righe; `tmp/m1/d/README.md`, 201 righe.
È stato elencato soltanto `tmp/m1/d/` per individuare i file da congelare.
Nessuna rilettura o modifica del protocollo FROZEN di 169 righe, del piano
scientifico o dei documenti storici. Nessun accesso agli altri prototipi.
Nessuna delega, rete, installazione, operazione Git, hardlink o codice di prodotto.

Prima di qualsiasi patch è stato creato
`tmp/m1/d/lifecycle-before-20261025-01/`, copia byte per byte di tutti gli otto
file allora presenti, incluse le due receipt e lo snapshot precedente.
Creazione esclusiva, nessuna sovrascrittura. Il manifest è
`snapshot-receipt.json`, SHA-256:

```text
2bf28eff25e4432f165540b84ff50ef5033af6aa81852b3bbbc47be92b83170c
```

Pin v2 congelati:

```text
399f13308a9bc45d6938045ea15b8f27cf742551fbe6f5c95eba7c7cf19f3d50  pool.py
90464d67bee562719bb667dca79cfc53f4b9e14b1e3829ae3290876542a953ea  run.py
a42fb0d83bd3d15292d90fe6b0f44d454463d43186992504616a6df99caa15e1  README.md
28e10b8848b0d281cb9172992802ce88040072dc2ccf63f960d96c18c3df4f13  corrections-static-20261025-01.json
```

Le copie e le receipt storiche non sono state riscritte. La nuova prova statica
è `tmp/m1/d/lifecycle-static-20261025-01.json`.

## Equivalenza scientifica e modifica dichiarata

I 21 casi bounded, i 15 errori attesi e i loro nomi/codici restano identici alla
v2. Il confronto AST della mappa `CASES` li verifica contro lo snapshot.
Restano stream da 16 tile, 32 MiB reali per 1/2/4 worker, distinti dai tre full
da 40 GiB ciascuno. Il full procedural non materializza uno spill aggiuntivo.
Pool 32 MiB, tile 2 MiB, dtype, formula periodica, oracle Fraction, checksum,
halo e guasti restano invariati. Il cap disco comune resta 1 GiB.

La modifica preregistrata riguarda soltanto la vita del file sintetico del caso
riuscito `spill-256MiB`, più verifiche, receipt e contabilità necessarie.
Quel `spill.complete` non è un originale utente né una prova fallita: diventa
un intermedio controllato, eliminabile soltanto dopo verifica e receipt durevole.
Non si eliminano parziali, file falliti, log grezzi, fonti o receipt.
Non si cambiano dati, filesystem, backend o dimensioni per far passare la quota.

La ricetta passa da `M1.d-isolated-v2` a `M1.d-isolated-v3`;
il lifecycle è `M1.d-synthetic-intermediate-v3`. Il manifest futuro hasha anche
`disk.py`, `test_lifecycle.py` e questa preregistrazione. La quota esterna e
l'ammissione esclusiva sono ora dati di ammissione per fase, non parametri
scientifici: possono essere aggiornati tra validate e measure senza cambiare
algoritmo. Le receipt v2 non possono essere riusate come validazione v3.

I pin dichiarati delle fonti restano separati:
`plans/M1-spikes.md` =
`1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`;
`ASTRO_POSTPROCESSING_PLAN_v2.2.1.md` =
`95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`.
Non sono stati ricalcolati sui file esterni allo scope in questo incarico.

## Sequenza di commit e rilascio dell'intermedio riuscito

1. Il worker scrive, fsync, controlla size e `st_blocks * 512 >= 256 MiB`,
   rilegge tutti i byte e verifica ogni tile contro digest e somma oracle.
   Aggiunge SHA-256 dell'intero file e lo confronta con l'oracle periodico.
2. Il worker pubblica `spill.complete` e sincronizza la directory. La sua quota
   trasferisce il charge da riserva attiva a blocchi persistenti: non dichiara
   restituita la quota del file ancora presente.
3. Il supervisore aspetta il processo, verifica exit/result e conserva stdout e
   stderr grezzi. Sincronizza i log prima del rilascio. Riapre il file senza
   seguire symlink; richiede file regolare, link count 1 e identità stabile.
4. Il supervisore rilegge tutti i 256 MiB in chunk da 64 KiB. Confronta ogni byte
   con il periodo ricostruito mediante Fraction, conta chunk/byte e confronta
   SHA-256 integrale, oracle e risultato worker. Size e blocchi restano obbligatori.
5. Scrive `intermediate-receipt.json` con classificazione preregistrata,
   recipe hash, percorso, device/inode/size/blocks/link/mtime/ctime, chunk,
   byte verificati, SHA-256 integrale e oracle, risultato worker e hash dei log.
   Sincronizza file e directory, rilegge la receipt e verifica il digest.
   I log condivisi sono identificati dal prefisso immutabile di N byte, non
   dallo SHA di un file che i casi successivi continueranno ad appendere.
6. Ricontrolla l'identità del solo intermedio riuscito. Solo allora esegue
   l'unico unlink di `disk.py`, sincronizza la directory e chiude il descrittore.
   Richiede che l'occupazione osservata sia diminuita almeno dei blocchi del file.
7. Scrive separatamente `intermediate-release.json` senza riscrivere la receipt.
   Prima di ammettere `spill-cancel`, richiede entrambe le receipt coerenti con
   la ricetta, file completo assente, almeno 256 MiB rilasciati e quota sufficiente.
   Il caso deve poi terminare esattamente con `CANCELLED` dopo scrittura reale,
   non con `SCRATCH_QUOTA` dovuto al file precedente.

Qualsiasi verifica o commit della receipt fallito conserva il completo, gli
eventuali staging e i log. Un guasto del worker conserva il parziale e i suoi
blocchi persistenti. Il ramo si ferma senza pulizia automatica o nuova ammissione.
Un errore dopo un unlink già riuscito non può ricreare quel nome atomicamente:
resta la receipt precedente al punto distruttivo e la serie si ferma. Non si
attesta una cancellazione reversibile, una garanzia contro ogni guasto del
filesystem o la resistenza a power-loss. Non si rigenera il file come ripiego.

## Derivazione del disco per fase e blocco esplicito

Rimossa la costante `SPILL_BYTES + 64 MiB`, priva di derivazione dal protocollo.
La RAM non entra nella riserva disco.

- Validate: massimo un intermedio attivo da 256 MiB. I guasti previsti conservano
  al massimo una prima pwrite da 65536 byte, una da 4096 byte e il marker scratch
  non-directory di 31 byte: 69663 byte logici. Un errore inatteso ferma la serie,
  conservando anche un file più grande; non avvia un altro caso.
- Measure: zero spill aggiuntivo. I tre full procedural non richiedono 256 MiB
  ciascuno né una riserva comune di 320 MiB. Tutti i file precedenti sono baseline.
- Report/staging: il cap JSON esistente resta 1 MiB, senza riduzioni. Validate
  ha quattro report finali e due receipt, più un solo staging simultaneo:
  limite logico conservativo 7 MiB. Measure ha quattro report più staging: 5 MiB.
- Mancano bound dimostrati per log grezzi complessivi, reservation log, samples,
  diagnostica d'errore e crescita fisica dati/metadata/CoW/directory. Una dimensione
  di frammento da statvfs non dimostra da sola tali bound. Non basta il cap di una
  singola riga per limitare il totale di uno stream.

Pertanto **ausiliari fisici <=8 MiB NON DIMOSTRATI**. La nuova ricetta non sceglie
un margine più piccolo per entrare nella quota. `phase_disk_plan` restituisce
`null` per i termini ignoti, `proof_status=UNPROVEN` e ammissione `NOT_RUN`.
Il CLI ferma entrambe le fasi con exit 2 e `DISK_BOUND_NOT_PROVEN`, prima di lock,
output o worker. Anche `Ledger` rifiuta un piano non provato. Nessun flag CLI
consente di trasformare questo stato in un'ammissione.

Le API preparate ammettono preventivamente ogni append/JSON e controllano
occupazione reale e limite di fase. Non costituiscono una dimostrazione del
bound, né sostituiscono una prova delle allocazioni concorrenti di writer e
worker: tale prova resta incompleta. Non cambiare soltanto `UNPROVEN` in
`PROVEN`; completarla richiede una nuova ricetta e review prima di eseguire.
Snapshot, receipt precedenti e parziali persistenti restano parte della baseline
`st_blocks` e non si sottraggono per comodità dal limite comune.

L'ammissione futura richiede anche quota esterna aggiornata ed esclusività
globale delle scritture per tutta la fase, amministrate dal coordinatore.
`--exclusive-admission-id` registra il contratto, non lo realizza. Il lock d
non è una riserva globale. La scansione ogni 250 ms resta diagnostica, non hard
guarantee. Nessuna ispezione degli altri rami sostituisce la quota dichiarata.

## Regressioni preparate, tutte NON ESEGUITE

La sequenza reale success->receipt->release->cancel resta nei 21 casi originali.
Si aggiungono sette test in `test_lifecycle.py`: ordine receipt/rilascio,
conservazione su mismatch, conservazione su fsync receipt fallito, mutation
cleanup anticipato, mutation fsync receipt omesso, quota persistente e rifiuto
dei piani fisici non provati.

I test d'ordine usano piccoli file sintetici e mock della sola verifica
scientifica. Uno spy indipendente osserva i syscall reali: entrambe le mutation
devono fallire prima dell'unlink. Non simulano un PASS fisico da 256 MiB.
Le fixture, incluse quelle fallite, restano nella directory assegnata; niente
rmtree automatico. L'esecuzione dei test richiede comunque nuova ammissione.
Il bootstrap appaiato e la serie formale di benchmark restano NON IMPLEMENTATI.
Non si dichiara un limite RSS universale o equivalenza tra 32 MiB e 40 GiB.

## Pin del candidato e prova statica

SHA-256, relativi a `tmp/m1/d/`:

```text
866b89883d8c336250afb44ca36f198351b937c9aac3335c8179b358348c0967  pool.py
86675177b44bf892af421496cfee5d58b031824e6800b4d3bd6eba08bb076f92  run.py
ccf55b1291c1f7fbfa17edf6a41ff2212ddfefce209d64232fdd87a124a78d20  README.md
dabbed32dab75abc1580456c338855e91f4cbd6fc137d83e0cfce0a1f56e8862  disk.py
fba4dde6a61be84a676436c1ef9fdbeb017c04c68dcf42044260083911e21c6b  test_lifecycle.py
```

Dimensioni: pool 25568 byte/569 righe; run 40142/731; README 17113/272;
disk 11354/223; test_lifecycle 6701/149.
Ambiente statico: Linux, Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.
AST parse effettivamente eseguito sui quattro Python, senza import/compile/exec.
La receipt statica conserva hash e conteggi, confronto CASES, pin, lista full,
numero di test, ordine lessicale verifica/receipt/unlink e verifica degli otto
file congelati. Questi controlli non provano il comportamento dinamico.

Il comando statico usa `OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -`
con soli `ast`, `hashlib`, `json`, `pathlib` e `sys`; crea esclusivamente
`tmp/m1/d/lifecycle-static-20261025-01.json`. Il suo contenuto distingue
`STATIC_ONLY`, test `NOT_RUN`, ammissione `NOT_RUN` e hash di preparazione.
L'hash di preparazione non è spacciato per il recipe hash del manifest runtime:
le fonti esterne verranno verificate soltanto da una futura ricetta ammissibile.

## Handoff

Primo requisito incompleto: dimostrazione del bound fisico per fase.
Primo caso scientifico incompleto: `pool-over-budget`, NON ESEGUITO.
Le receipt precedenti restano storiche; usare soltanto un nuovo percorso v3.
Nessun comando di validazione è autorizzato ora. Anche dopo l'ammissione esterna,
il candidato corrente deve restare NOT_RUN finché il bound non è dimostrato.

Forma del futuro comando, NON ESEGUITO, dalla radice del progetto:

```bash
OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B tmp/m1/d/run.py --unit d --phase validate --scope bounded --backend python-stdlib --external-scratch-bytes "${EXTERNAL_SCRATCH_BYTES:?serve quota esterna aggiornata}" --exclusive-admission-id "${EXCLUSIVE_ADMISSION_ID:?serve ammissione globale esclusiva}" --output tmp/m1/d/series-lifecycle-v3-01
```

Non si deve aggirare il gate per eseguire questa ricetta. Il prossimo lavoro
ammesso è review statica indipendente e chiusura della prova disco mancante.
