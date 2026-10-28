# M1.a pocketfft runtime followup 005: arresto del preflight

Esito: `STOP_PREFLIGHT`, supervisore exit 1. Il verificatore reale non è partito. Nessun retry, correzione o allentamento del controllo dopo l'arresto. MED003 resta aperto; questo turno non produce `PASS_STATIC_VERIFICATION`.

## Mandato e comando previsto

Il coordinatore ha autorizzato un solo turno del verificatore 005, dopo la review dei 43 test nella sessione `ses_e8803a229ffeTcwTFNRjsrl3xr`. Working directory: `/opt/workspace/astroProcessor`. Il comando è stato letto dal preregistro, senza aggiungere argomenti:

```sh
/usr/bin/timeout --signal=TERM --kill-after=5s 55s /usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-inventory-followup-005/verify.py
```

Questo comando è rimasto **non eseguito**. Un supervisore Python isolato via stdin ha arrestato il preflight prima di `Popen`, prima del gate fisico e prima della creazione dell'ambiente privato del figlio. Exit del verificatore: non disponibile, perché il processo non è stato creato.

## Autenticazione e binding dei test

Il supervisore ha autenticato 48 file locali prima di qualsiasi import del verificatore. Le aperture hanno attraversato i componenti con `O_NOFOLLOW`, richiesto file regolari del proprietario corrente entro 16 MiB e confrontato identità stat prima/dopo e sul nome. La ricevuta registra hash, byte, righe e stat per ciascun file.

- Manifest 005: `a7752c9e8612afc975adac40231c510d6f379b1989f6ddb96c103e56b270c9ef`.
- Checks 005: `21e7ae7e654c7145e7111ef790bb461b8edba474a99a55ab8fabc08edbfbd90a`.
- Report test 005: `b5f75e6c869036de565202520f239bfd7f1c9a74846cafd83747b42861178d6b`.
- Verificatore 005: `303545d4263bfda66f5245e1b6c04228a6ae93a753c4318c8af61eeffb9d67a9`.
- Sorgente test 005: `025f56e6fac513072c6c67828f8dad91fa29cb9baebc5f96021df0aeb90f833b`.

Sono passati i confronti dei 17 input 003, delle 8 sorgenti/documenti 005 e dei 16 record storici del manifest. Gli input comprendono raw readelf, cache e tabelle MD5 salvati. I record storici comprendono i due FIRSTFAIL nominati. Non è un'autenticazione dell'intera storia.

Il binding dei risultati precedenti ha confrontato le 43 definizioni AST con 43 nomi distinti `ok` nello stdout originale, il relativo riepilogo unittest, le ricevute e gli hash/stat delle sorgenti. Esito precedente confermato: 43 test, 5 positivi, 38 negativi, 0 failure, 0 errori, 0 skip, exit 0. Test rieseguiti in questo turno: 0. Nessun import di `verify.py` o `test_verify.py`.

## Primo errore e causa

Diagnosi originale del supervisore:

```text
unsafe output parent
```

Il controllo locale del supervisore richiedeva proprietario corrente e nessun bit di scrittura gruppo/altri per tutti gli antenati selezionati. Ha respinto la root del progetto, prima di raggiungere il gate del budget. La successiva diagnosi in sola lettura ha osservato UID corrente 1000 e questi permessi:

| Directory | UID | GID | Modo |
| --- | ---: | ---: | --- |
| `/opt/workspace/astroProcessor` | 1000 | 1000 | `0775` |
| `tmp` | 1000 | 1000 | `0775` |
| `tmp/m1` | 1000 | 1000 | `0775` |
| `tmp/m1/fft-pocket` | 1000 | 1000 | `0700` |
| `tmp/m1/fft-pocket/runtime-inventory-followup-005` | 1000 | 1000 | `0700` |

Il rifiuto deriva da questo controllo aggiuntivo del supervisore sugli antenati, non da un errore osservato del parser o dai checksum runtime. Il namespace effettivo 005 è privato. Il turno non ha modificato permessi e non ha ripetuto il comando con un controllo diverso.

Il gate fisico di 1 GiB, riserva 64 MiB e margine 16 MiB non è stato eseguito. HOME/cache/TMP privati e limiti del figlio non sono stati applicati, perché nessun figlio è partito. Non si attribuisce alcun PASS a questi gate.

## Artefatti e processi

Unica ricevuta nuova: `tmp/m1/fft-pocket/runtime-inventory-followup-005/verification-attempt-001/receipt.json`, 25726 byte, 862 righe, SHA256 `877b7388475287711297528a008dc0b917e8769cddd09d9bf94d1fa13b3bcbbf`.

Il supervisore ha creato il nuovo tentativo con umask `077` soltanto per conservare la ricevuta dell'arresto. Non esistono stdout/stderr del verificatore: non sono stati creati né ricostruiti. `child_started=false`, nessun PID figlio, `child_collected=false`, `slot_free=true`. Non c'è un figlio da raccogliere.

Durata del supervisore: `0.010235526133328676` s. Python: `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`; Linux x86_64. La ricevuta contiene intervallo UTC e kernel osservati. Modello dichiarato: `openai/gpt-6.1-sol`, nessun override. Costi, token e dettagli di failover non esposti.

## Copertura e limiti

Copertura reale del verificatore in questo turno: 0 oggetti, 0 archi NEEDED, 0 gruppi provider, 0 version requirements e 0 checksum host verificati. I conteggi attesi 6/13/13/54/5 restano non misurati da questo turno. Nessuno dei cinque runtime allowlisted è stato aperto dal verificatore. L'avvio normale dell'interprete non costituisce verifica delle librerie host.

Nessun worker, FFT, ldd, loader, build, acquisizione metadata, binding live, rete, Git, installazione, delega o nuova fixture. Le sole scritture persistenti sono la ricevuta nel nuovo tentativo e questo report. Nessuna modifica a 003, 004, sorgenti 005, raw storici, FIRSTFAIL, progress o index.

Il risultato non chiude MED003 e non autorizza conclusioni sui 232 casi FFT. Anche un futuro PASS statico non proverebbe binding live, simboli, relocation, IFUNC o licenza di distribuzione. Lo stderr originale 001 resta indisponibile e non è stato ricostruito. Serve consegna al coordinatore e review indipendente di questo arresto; nessun altro tentativo appartiene al turno corrente.
