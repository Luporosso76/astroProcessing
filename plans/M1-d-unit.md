# ExecPlan: M1.d UNIT33-1 standalone

## Obiettivo e perimetro

Eseguire in un futuro turno autorizzato soltanto le dodici regressioni lifecycle
v5, con file nativi piccoli e un inventario dedicato. Questa preparazione non
esegue import candidati, test, preallocazioni, worker o benchmark.

Scritture autorizzate: nuovo `tmp/m1/d-unit/`, questo piano e nuova evidence
`docs/evidence/M1-d-unit-preparation.md`. V5, progress, indici, protocollo e piano
scientifico restano immutati. Non è un completamento o una ridefinizione di M1.d.

## Fonti e snapshot

- Otto sorgenti v5 e manifest verificati prima della preparazione; copie byte
  in `tmp/m1/d-unit/snapshot-v5/`, receipt `frozen-v5.json`.
- Manifest v5 SHA-256:
  `b45cc3bb16043e50e2b82b6d8d7fc54eddb21bf3dc82dec50e973b7e1a3741ab`.
- Il futuro driver verifica manifest, otto sorgenti e snapshot PRIMA degli import.
- Poi verifica i propri sorgenti contro `preparation.json`, il cui hash deve
  provenire dalla review, non essere ricalcolato per accettare modifiche.

## Decisione primaria

Il contratto chiarito richiede preflight, riserva conservativa e controlli
osservati del cap scratch 1 GiB. Non richiede una prova universale dei metadata.
Gamma v5 resta ricerca separata e i vecchi flag UNPROVEN non vengono abilitati.

`UnitArena` è un adapter geometrico esplicito: riusa Slot, Partitions,
borrow/mapping/close_mapping e commit JSON v5. Costruisce soltanto il layout
UNIT33; check misura occupazione reale, non finge un valore costante.
Il verificatore scientifico v5 da 256 MiB non viene patchato. Il rilascio unit
ha un proprio protocollo da 33 byte con hash reale, receipt durevole e guard
consumer. Undici corpi test v5 sono ereditati; la verifica negativa usa gli
stessi tre assert con un digest atteso errato, senza mock scientifico.

## Prerequisiti della futura esecuzione

1. Review indipendente di codice, geometria e schema/budget degli errori.
2. Turno esclusivo globale, root privata e nessun fd/mapping esterno sui fixture.
3. Quota esterna aggiornata: comprende TUTTO ciò che sta fuori da d-unit,
   incluso il vecchio unit conservato. Nessuna scansione degli altri prototipi.
4. Python esatto v5, recursion limit nativo 1000, quantum osservato 4096.
5. Output nuovo. Nessun resume, riuso di artefatti falliti o fallback automatico.

## Riserva derivata, non ceiling universale

Schema e formule: `tmp/m1/d-unit/recipe.py`.

- 12 fixture da 33 byte, un file ENOSPC da 4096 byte.
- Tre regioni fault da 33 byte più header 16.
- 12 raw log da 34 byte; receipt/release native unit con final/staging distinti.
- Trace strutturata completa: fino a tre record (body, teardown, driver),
  recursion e reraise inclusi. Nessuna elisione di frame o taglio di messaggi.
- Runner, risultati, errore e staging/finalizzazione dedicati.
- Arena: 71341727 byte; dati arrotondati, fixture comprese: 71397376.
- Riserva metadata dichiarata: 442368 byte, due quanti per file/directory e
  directory BASE. È un margine ingegneristico controllato, non un teorema FS.
- Riserva totale: **71839744 byte**.

Condizione prima dei dati:
`baseline_d-unit + quota_esterna + 71839744 <= 1073741824`.
Quindi baseline+quota esterna devono essere <=1001902080.
Budget mancante: `max(0, baseline_d-unit + quota_esterna - 1001902080)`.
I due termini correnti non sono stati misurati/forniti in questa preparazione.
Non si deduce l'ammissione dal solo spazio libero del filesystem.

## Sequenza e gate

- Verificare pin, parametri, identità runtime e preflight prima dei dati.
- Acquisire lock locale; il turno globale resta contratto del coordinatore.
- Prenotare le partizioni complete e creare arena/fixture native con fsync.
- Controllare occupazione dopo mutazioni e prima/dopo ogni CASE.
- Eseguire i 12 nomi fissi con failfast; le eccezioni intenzionali restano
  dentro gli assert. Al primo imprevisto fermare, mantenendo NOT_RUN gli altri.
- Conservare raw/trace, file reali, slot terminali e staging falliti. Unlink solo
  del fixture riuscito, dopo verifica, receipt e chiusura consumer propri.
- Cap o riserva superati: latch VIOLATION, nessun ulteriore CASE, exit non zero.
  La finalizzazione usa soltanto regioni già prenotate; non converte VIOLATION
  in PASS. Un risultato iniziale viene corretto in una seconda revisione fissa
  se la mutazione finale rivela una violazione, preservando il primo staging.
- COMPLETE richiede tutti i 12 PASS effettivi, nessuno skip/errore/violazione.
  Non costituisce PASS dei 21 bounded/15 errori/3 full o dello spill 256 MiB.

## Stato e prossimo comando

Codice concreto e AST preparati. Tutti i test restano NOT_RUN. Prima serve review
indipendente; nessun comando candidato è autorizzato in questo turno.

Forma del futuro comando dalla radice, solo dopo review e ammissione reale:

```bash
OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S tmp/m1/d-unit/run.py --preparation-sha256 "${REVIEWED_PREPARATION_SHA256:?serve hash della review}" --exclusive-admission-id "${EXCLUSIVE_ADMISSION_ID:?serve turno esclusivo}" --external-scratch-bytes "${EXTERNAL_SCRATCH_BYTES:?serve quota aggiornata}" --private-no-external-readers --output tmp/m1/d-unit/lifecycle-01
```

Evidenze attese: `unit.arena`, fixture conservati, trace integrale, runner e
result/error JSON negli slot descritti dal manifest. Nessun worker completo,
misura formale o bootstrap statistico implementato/eseguito.
