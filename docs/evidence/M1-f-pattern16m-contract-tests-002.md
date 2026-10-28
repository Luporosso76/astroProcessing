# M1.f: mock del launcher e test del contratto JSON

Esito osservato: **8 mock PASS, poi 4 test del contratto PASS**.
Una sola esecuzione per fase, senza fix, retry, skip o modifiche alle asserzioni.
Scope: mock mirati del lifecycle e `CONTRACT_JSON_NOT_ENFORCEMENT`.
Nessun candidato nativo o producer eseguito. Nessuna qualifica generale del launcher.

## Ammissione e fonti

Il coordinatore ha ammesso questa sequenza dopo la review `PATCH_SCOPED_OK`
della sessione `ses_e80e2302bffe1T8sBPBsoT5oQx`.
Prima e dopo le fasi sono stati autenticati i 17 file PREP e i sei file launcher.
STOP001, il relativo report e tutti i FIRSTFAIL preesistenti restano invariati.
L'inventario dinamico ha trovato **10** FIRSTFAIL, non un numero prefissato di nove.
Nessun nuovo FIRSTFAIL è stato prodotto.

Pin principali SHA-256:

- Launcher originale: `378a2d9bc344ce1295691ada31714240a6c332c98ba410a0f75d5c87b1009456`.
- Mock: `1f9d91a09ec13526c10b22486ee651871aff2d5b3d487b306878cc1fcec04225`.
- Test contratto: `aa569fc5002a1d279c85bdf3fb9fe5076a999317ac9fca548163e3d00956ef88`.
- Manifest: `4574dae9b4666933b0c3283da17947cdeb1a06383c55c199dbe06551302a66ff`.

Python osservato: `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`, Linux.
Nessuna fonte 005 è stata qualificata. Nessun contenuto del producer001 è stato letto
o modificato. Le scansioni globali hanno letto soltanto metadati necessari al
conteggio fisico e i FIRSTFAIL protetti. Nessun Git, rete, installazione o indice.

## Bootstrap limitato ai mock

Directory di controllo: `tmp/m1/f-pattern16m-contract-execution-002/`.
Il bootstrap è una copia derivata dell'outer002 immutato, non del vecchio outer storico.
SHA-256 `bootstrap.py`:
`d2c74e958354bf7358155666dd8b3c0d15c1a9ec8cc7da13ee6f24f059c5d51e`.

Le sole otto sostituzioni riguardano RUN, comando, percorso AST del mock,
conteggio otto, messaggi dei conteggi, summary otto e conteggio osservato.
`bootstrap.patch` conserva il diff effettivo.
`bootstrap-proof.json` registra l'uguaglianza AST di tutte le **19 funzioni diverse
da main**, comprese lifecycle, observe, send, reap, cleanup, persist, finish,
limits e funzioni di quota. Anche il resto del modulo ha AST identico,
esclusi main e i binding RUN/COMMAND autorizzati.
Non è stata introdotta una nuova logica di ownership.
Il mock importa e prova l'outer originale.

## Comandi ed esiti distinti

Fase mock, launcher:

```sh
/usr/bin/python3 -I -B -S tmp/m1/f-pattern16m-contract-execution-002/bootstrap.py
```

Unico comando figlio della fase mock:

```sh
/usr/bin/python3 -I -B -S tmp/m1/f-pattern16m-contract-launcher-002/test_outer_mock.py
```

Risultato naturale: `Ran 8 tests in 0.011s`, `OK`.
Launcher PID **3798824**, uscita **0**. Figlio PID **3798825**, uscita **0**,
raccolto con waitid; `reaped=true`, `reserved=false`, `slot_blocked=false`.
Nessun segnale reale inviato. Ledger primaria, cleanup e persistenza tutti a zero.
Le iniezioni di errore nei mock restano simulate; non sono errori del controllo esterno.

Dopo PASS, raccolta e invarianti verificati, fase contratto, launcher originale:

```sh
/usr/bin/python3 -I -B -S tmp/m1/f-pattern16m-contract-launcher-002/outer.py
```

Unico comando figlio della fase contratto:

```sh
/usr/bin/python3 -I -B -S tmp/m1/f-pattern16m-turn-001/test_contract.py
```

Risultato naturale: `Ran 4 tests in 0.001s`, `OK`.
Launcher PID **3835422**, uscita **0**. Figlio PID **3835530**, uscita **0**,
raccolto con waitid; `reaped=true`, `reserved=false`, `slot_blocked=false`.
Nessun segnale reale inviato. Ledger primaria, cleanup e persistenza tutti a zero.
Gli slot sono liberi dopo queste raccolte reali, non sulla base delle simulazioni.
I tempi riportati sono quelli di unittest, non misure del wall time dell'intero launcher.

I raw dei launcher, PID e codici d'uscita sono conservati nella directory di controllo.
I raw unittest e le ricevute originali sono rispettivamente in
`tmp/m1/f-pattern16m-launcher-mocks-002/` e
`tmp/m1/f-pattern16m-contract-test-002/`.
Le ricevute originali usano intenzionalmente `CHECKS_COMPLETE` e
`final_success_attested=false`: non dichiarano successo prima della propria persistenza.
La diagnosi finale PASS, l'uscita zero e i controlli post sono conservati separatamente.

## Limiti e spazio osservato

Entrambe le fasi hanno mantenuto il core identico e i limiti figli:
wall 55 s, CPU 50 s, AS **268435456**, FSIZE **1048576**, NOFILE **64**, CORE **0**.
HOME, TMPDIR, TMP, TEMP e XDG sono privati nei rispettivi RUN, con umask 077.
Log stdout e stderr limitati a 1 MiB ciascuno; budget aggiuntivo 4 MiB per fase.
I log figli osservati sono stdout **0 byte**, stderr **106 byte** per entrambe le fasi.
I log esterni dei launcher sono stati verificati entro 1 MiB ciascuno.
Nessun segnale dipende dal testo FAIL/ERROR; il traceback naturale non è stato filtrato.

Quota globale **1073741824**, riserva **67108864**, margine **16777216**.
Budget conservativo complessivo setup più due fasi: **9437184** byte, senza alzare quote.
`receipt_tail` resta **147456** byte, riservato ai due receipt e blocchi directory.

Misure fisiche cooperative, con deduplica `(dev, ino)`, `st_blocks*512` e nofollow:

- Prima del setup: U **936947712**, filesystem disponibile **85449838592** byte.
- Preflight esterno mock: U **936988672**, più 4 MiB per la fase.
- Preflight interno mock: U **936996864**, filesystem disponibile **85450387456** byte.
- Post interno mock: U **937017344**.
- Post esterno mock, prima delle relative ricevute controller: U **937037824**.
- Preflight interno contratto: U **937058304**, filesystem disponibile **85955129344** byte.
- Post interno contratto: U **937078784**.
- Post esterno contratto, prima di `contract-post.json`: U **937099264**,
  filesystem disponibile **85936001024** byte.

Le prove della prima fase sono incluse interamente nell'U della seconda.
Al post esterno del contratto: incremento dalla baseline della fase più receipt_tail
**188416 <= 4194304**; incremento dal setup più receipt_tail
**299008 <= 9437184**. Anche quota globale più riserva, margine e receipt_tail
e spazio filesystem residuo hanno superato i gate.
`final-audit.json` registra l'ultimo controllo, dopo la scrittura di questo report,
con spazio conservato per gli ultimi artefatti.

Le misure del filesystem disponibile sono snapshot, non una prenotazione atomica.
Non sono misure attribuite al producer né delta rispetto al valore storico MOCK Rust.
Il budget producer 62324736 non è stato usato per ammettere queste fasi.
Non sono stati misurati picchi RSS/CPU o limiti real-time delle syscall del kernel.

## Conservazione e perimetro finale

`initial.json`, `mocks-post.json` e `contract-post.json` conservano i pin completi
prima/dopo, i conteggi distinti e le diagnosi. `SHA256SUMS` nella directory di
controllo congela gli artefatti della consegna; comprende anche i due RUN e questo report.
Le sorgenti, i checksum PREP, lo storico outer, progress e indici non sono stati modificati.

Questa prova copre otto mock mirati del lifecycle e quattro test del contratto JSON.
Non qualifica enforcement del budget producer, controller scientifico, fonti 005,
candidati o fault reali del kernel. I percorsi di errore simulati non costituiscono
una dimostrazione generale di cleanup su ogni piattaforma.

Controller producer ancora assente; **0 producer RUN, 30/38 e 7/11 invariati**.
Nessuna promozione scientifica e nessun `READY_RUN`.
