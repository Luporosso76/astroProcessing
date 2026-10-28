# M1.a pocketfft runtime followup 004: turno dei 21 test

## Esito

**STOP_TEST_FAILURE**. Una sola invocazione del comando preregistrato: 21 test effettivi, 12 `ok`, 8 failure, 1 errore, 0 skip. Exit code 1. Nessun PASS della suite, nessuna correzione o seconda esecuzione.

Il finding MED003 resta aperto. Non è stato eseguito il comando host di `verify.py`. Questo turno non attesta checksum dei runtime host, binding live, licenza commerciale o risultati FFT.

## Autorizzazione e fonti

Il coordinatore ha autorizzato soltanto i 21 test, dopo la review `ses_e881b1d59ffe7CtxKsjrhbED2i`, comunicata come `READY_STATIC21`. La sola review non attestava il risultato runtime. I negativi LOW mancanti non sono stati aggiunti né contati.

Sono stati letti il report di preparazione, `PLAN.md`, `preregistration.json`, `preparation-manifest.json`, `preparation-checks.json`, `test_verify.py` e `verify.py`. Nessuna modifica a questi file, a 003, a progress, index o piani. Il wrapper nuovo è `tmp/m1/fft-pocket/runtime-inventory-followup-004/run-tests-once.py`.

Prima del test, il wrapper ha autenticato i due pin ricevuti:

- `preparation-manifest.json`: SHA256 `e77bc98e296a8d411d798f02591004dc2dee9935d175a2f5e1cb647d87ff7c34`.
- `preparation-checks.json`: SHA256 `39d9e46cc34fe804606df118bc420634348f2da987d0c11b6359378c81bd6987`.

Dal vero schema `input_records` e `source_records` ha ricavato e verificato 23 record: sei ancore, undici ulteriori input nominati e sei sorgenti/documenti. Non ha trascritto nuovi pin per questi record. Byte, righe e SHA256 corrispondono al manifest; gli stessi record corrispondono anche dopo il processo. La ricevuta contiene entrambe le liste complete. Le righe hashate non attestano una lettura semantica dell'agente o tutta la storia di 003.

## Gate prima dell'avvio

I file autenticati sono stati aperti attraverso componenti `O_NOFOLLOW`, con controllo di proprietario, file regolare, dimensione massima e identità stabile. La directory antenata `tmp/m1/fft-pocket` e la nuova directory `tests-attempt-001` erano del proprietario effettivo UID 1000, modo `0700`. Il target era nuovo; i raw sono stati creati con apertura esclusiva. La directory 004 preesistente non è stata cambiata: la privacy deriva dall'antenato `0700` e dal nuovo target `0700`.

L'esclusività del writer deriva dal mandato del coordinatore; non è una prova ricavata da un censimento dei processi del sistema. Non sono state lanciate deleghe o attività runtime parallele.

Allocazione di `tmp/` prima del comando: 960184320 byte. Cap: 1073741824 byte; riserva: 67108864 byte; margine: 16777216 byte. Il gate ha richiesto che allocazione, riserva e margine rientrassero insieme nel cap. Spazio filesystem disponibile: 125333008384 byte. Namespace 004: 77824 byte prima, 135168 byte dopo, entro il cap preregistrato di 8388608 byte. Dopo il comando `tmp/` occupava 960241664 byte.

Ogni stream aveva un bound di 1048576 byte tramite `RLIMIT_FSIZE`. Limiti del figlio: CPU 30 secondi, address space 268435456 byte, core dump disabilitato. Il comando include timeout 55 secondi e grace 5 secondi. Il wrapper aveva inoltre raccolta finita con `wait(timeout=65)`; tale limite non è intervenuto.

L'ambiente completo è nella ricevuta. `HOME`, `TMPDIR`, `TMP`, `TEMP` e tutte le variabili XDG impostate puntavano a directory nuove `0700` sotto `tests-attempt-001`. Python è stato isolato con `-I -B -S`. Nessuna cache Python creata dal comando.

## Comando e raccolta

Il wrapper esterno è stato invocato una volta con `/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-inventory-followup-004/run-tests-once.py`. Il suo unico `Popen` ha eseguito esattamente:

```sh
/usr/bin/timeout --signal=TERM --kill-after=5s 55s /usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-inventory-followup-004/test_verify.py
```

Intervallo UTC registrato dal wrapper: `2026-10-26T11:12:13.968132+00:00`–`2026-10-26T11:12:14.202598+00:00`. PID wrapper 955455; PID del `Popen` timeout 955456. Exit code raccolto: 1. Tempo monotono di esecuzione/raccolta: 0.11974885920062661 secondi. Il PID interno dell'interprete del test non è stato osservato e non viene dedotto. La review e il codice del test non prevedono ulteriori figli.

Il processo posseduto è stato effettivamente raccolto con `wait`; `child_collected=true` e `slot_free=true` sono nella ricevuta durevole. Nessun processo irrisolto viene dichiarato libero.

Ambiente osservato: Linux x86_64, kernel `7.0.0-38-generic`; Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`. La versione di GNU timeout non è stata interrogata. Modello dichiarato dal runtime: `openai/gpt-6.1-sol`, senza override. Costi, token e dettagli di eventuale failover non sono esposti e non sono dedotti.

## Risultato e diagnosi osservata

Lo stdout originale termina con:

```text
Ran 21 tests in 0.025s

FAILED (failures=8, errors=1)
```

Il positivo `test_saved_relations_positive` produce `VerificationError: version header grammar` in `verify.py:151`. Otto negativi falliscono perché ricevono la stessa diagnosi prima del confronto specifico atteso: nodo disponibile ma non richiesto, definizione omessa, arco omesso, arco sostituito da duplicato, requisito omesso nelle due tabelle, record versioni omesso, candidato cache duplicato e percorso selezionato errato. Questi negativi non dimostrano il comportamento previsto in questo turno.

Passano gli otto test checksum sintetici e quattro test della classe `StaticRelations`. I checksum usano byte privati sintetici, non librerie host. Il solo errore di grammatica osservato non identifica qui la causa completa: non è stata avviata un'indagine aggiuntiva né corretto il parser.

La suite preregistrata non usa failfast: ha completato i propri 21 casi nella singola invocazione. Dopo l'exit non nullo il ramo si è fermato. Non sono stati eseguiti retry, test aggiuntivi, skip, allentamenti dei gate o modifiche alle sorgenti.

## Artefatti conservati

Tutti i percorsi seguenti sono relativi a `tmp/m1/fft-pocket/runtime-inventory-followup-004/`:

| Artefatto | Byte | SHA256 |
| --- | ---: | --- |
| `run-tests-once.py` | 8696 | `7599487f6828d2e52e7d3056153abb3fc8a45439b499d51bbbe0b50d7aeacc37` |
| `tests-attempt-001/receipt.json` | 17274 | `0fc86809882ccc91c6f6fd7881387207640445c23bb228a32e887f8fa5a19e27` |
| `tests-attempt-001/stdout.raw` | 11676 | `e50035dabce7b215a837134d15db16c4c16e1fe74d4644fe80f6373bb614e2bf` |
| `tests-attempt-001/stderr.raw` | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |

Lo stderr originale è realmente vuoto: il runner unittest scrive su stdout. `completion-summary.json` è una derivazione successiva dei raw e della ricevuta, non una seconda esecuzione. Raw e ricevuta sono stati sincronizzati su disco. Il successivo controllo della raccolta ha letto soltanto artefatti locali.

Nessuna esecuzione diretta di `verify.py`, nessuna lettura esplicita delle librerie host per checksum, nessun worker, loader, ldd, build, FFT, rete, Git, installazione o indice. L'import di `verify.py` serve solo i test e non chiama `main()`.

## Handoff

Consegna stabile: **STOP_TEST_FAILURE**, slot runtime libero dopo raccolta. Conservare questo tentativo senza modificarlo. MED003, prova host distinta, gap LOW e limiti della preparazione restano aperti. Qualsiasi correzione o nuova esecuzione richiede un turno successivo autorizzato; questo turno non li avvia.
