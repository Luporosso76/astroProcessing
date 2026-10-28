# M1.f: stop prima dei test del contratto

Esito: `STOP_STATIC_OUTER_CLEANUP_NOT_PROVEN`. Nessun test eseguito.
Scope: `CONTRACT_JSON_NOT_ENFORCEMENT`. Questa ispezione non qualifica il runtime.

## Comando e conteggi

Comando autorizzato, non eseguito:

```sh
/usr/bin/python3 -I -B -S tmp/m1/f-pattern16m-turn-001/test_contract.py
```

Il codice effettivo contiene quattro metodi `test_`, contati con `ast.parse` senza importarlo.
Il runner dichiara `unittest.main(failfast=True)`; il comando non aggiunge `-f`.
Esecuzioni del test: 0. PID ed exit code del test: assenti. Nessun figlio da raccogliere.
Non dichiaro una raccolta reale né un handoff runtime `slotfree`.

Le ispezioni stdlib hanno usato `/usr/bin/python3 -I -B -S -`.
Versione osservata: Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`, piattaforma `linux`.

## Primo impedimento

Ho letto lo schema prescritto:
`tmp/m1/fft-pocket-benchmark/primary-error-chain-green-tests-001/outer.py`.
SHA-256: `fc401d7fc2b854cea16e9880863e968ab2e793de7f407d57958a340af6ec0403`.

Il `Popen` alle righe 95-96 precede il `try` alla riga 116. Il ramo `except`
alle righe 147-149 registra l'errore e blocca lo slot, ma non termina né raccoglie
il figlio. Non esiste un `finally` di cleanup. Un errore di `observe()` disabilita
i segnali; non dimostra la raccolta. Lo schema usa file diretti, non selector.
Non posso quindi attestare il cleanup finito richiesto su ogni errore post-Popen.

L'adattamento ammesso riguarda comando, fonti, destinazioni e conteggi.
Correggere il lifecycle richiederebbe una modifica diversa. Ho applicato lo STOP
al primo impedimento, senza fix, retry o esecuzione del test.

## Autenticazione e conservazione

Autenticati tutti i 14 file richiesti: sei correnti più `SHA256SUMS`,
`followup-budget-001/budget.patch` e sei originali `before/`, incluso il vecchio
`SHA256SUMS`. Le fonti 005 non sono state riautenticate.

Pin del test: `aa569fc5002a1d279c85bdf3fb9fe5076a999317ac9fca548163e3d00956ef88`.
Pin manifest: `4574dae9b4666933b0c3283da17947cdeb1a06383c55c199dbe06551302a66ff`.
Pin `SHA256SUMS`: `76a9ec3e022d4e3e909218b21b4ef8e8c88e952fa99caaaaa4cd7e0b576891ba`.
Pin PLAN: `c5fd621a15a7566a847394db59b97788836e2e9767827f3415bd16301eb78039`.

`inspection.json` conserva hash, dimensioni e mode dei 17 file del namespace
PREP e dei nove FIRSTFAIL preesistenti. Due snapshot finali coincidono.
Questo confronto riguarda l'ispezione finale, non un intervallo runtime mai avvenuto.
Il nuovo FIRSTFAIL non sostituisce nessuno dei nove precedenti.

## Limiti non attivati

Il profilo richiesto resta wall 55 s, CPU 50 s, AS 268435456, FSIZE 1048576,
NOFILE 64, CORE 0, log stdout/stderr massimi 1 MiB ciascuno, budget aggiuntivo
complessivo 4 MiB, umask 077 e directory private nel progetto.
Non l'ho attivato: nessun test è partito. Nessun preflight di capacità runtime
è stato attestato; nessuna riserva è stata utilizzata per autorizzare un figlio.

Cap globale richiesto 1073741824, riserva 67108864, margine 16777216.
Il conteggio fisico cooperativo non sarebbe una prenotazione atomica.
62324736 riguarda il futuro producer a sette step, non questi test.
936648704 resta una misura storica del MOCK Rust, non una misura corrente né un delta.
146210816 resta la soglia futura B più riserva e margine, non credito disponibile.

## Artefatti e stato

Namespace creato senza collisione: `tmp/m1/f-pattern16m-contract-test-001/`.

- `raw.txt`: `2642c0e99bacc6e299faddc799c3e19b5df292967beaf27b31e35be82fc1a308`.
- `FIRSTFAIL.json`: `d796307e58e4b3753b2b200321e41acb080c25fdf19a3dfc851b70b26dda73b5`.
- `inspection.json`: `488c3fd8c493bb7a0d832c90d37641a50f35773c1b5facadc27f81c060117513`.

Controller ancora assente; 0 producer RUN, 30/38 e 7/11 invariati.
Nessuna promozione del conteggio scientifico, nessun `READY_RUN`, nessun PASS del test.
Occorre una decisione del coordinatore sul conflitto tra cleanup richiesto e adattamento minimo dello schema.
