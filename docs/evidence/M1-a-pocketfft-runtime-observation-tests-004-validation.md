# Pocketfft: due target budget PASS, suite37 PASS

26 ottobre 2026. Nuovo turno esplicitamente ammesso dopo STOP003 e correzione
della fixture. Eseguiti una volta i due target budget e, soltanto dopo exit 0
con `Ran 2 tests` e `OK`, una sola invocazione della suite completa.
Esito `PASS_MOCK37`. Slot runtime liberato dopo raccolta effettiva dei due figli.

Agente esecutore: subagente, modello dichiarato dal runtime
`openai/gpt-6.1-sol`. Costi non esposti. Nessuna ulteriore delega.

## Autenticazione prima dell'esecuzione

Directory di lavoro `/opt/workspace/astroProcessor`. Verificata l'assenza del
namespace `tmp/m1/fft-pocket/tests-attempt-004` e di questo report prima della
creazione. Nomi espliciti, scritture esclusive, nessun overwrite o autorename.

Prima di qualsiasi import del supervisor è stato verificato il test corrente:
SHA-256 `e8333f5cfd3d6df5888e88175de5cb4020eea458267c0f55f0f120359ff0058e`.
La copia prechange in `peak-fixture-fix-001` conserva il precedente SHA-256
`75e3b24cc4f933cae49ec751193e319a33035d904ecf980f6176af93199ed6a8`.
Il confronto byte per byte conferma soltanto tre sostituzioni di `34111558`
con `34111550`: nome del test, valore atteso del picco e soglia del test di
ammissione esatta. Il diff ricalcolato coincide con `fixture.diff` preservato.
Verificati anche tutti i pin in `preserved-pins.json`, usando la copia prechange
per il vecchio hash del test, e l'uguaglianza del supervisor con il suo snapshot.
Il controllo AST conferma `failfast=True`; nessun argomento CLI `-f`.

La correzione di fixture era già presente e autorizzata all'ingresso del turno.
Il calcolo dichiarato è header 31 byte più payload 17055744 byte per file:
due file richiedono `2 * (31 + 17055744) = 34111550` byte.
Il valore storico `34111558` non rappresentava quel calcolo.
I test eseguiti sotto verificano ora l'ammissione al limite e il rifiuto
quando manca un byte, senza modificare il supervisor o allentare il gate.

## Comandi e risultati

Unica invocazione dell'outer:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/tests-attempt-004/outer.py
```

L'outer004 riusa il testo autenticato dell'outer003, SHA-256
`7e1b8c60a8786fc8712cd5c4ecc9d25eb5f033b1cc911ef7ddf2efba1b429c6d`.
Le sostituzioni esplicite, ciascuna verificata univoca, cambiano soltanto
destinazioni, pin del test, lista/conteggio dei target e conservazione003/fix.
La supervisione Popen, quota, limiti, output e raccolta resta quella del turno003.
SHA-256 del testo di supervisione effettivamente eseguito:
`e8a61f2456a9de4ab16196d95bfa3339bc71f2da4a3078e678762c9cdc828c1f`.
SHA-256 del file `outer.py` nuovo:
`65ec2c0f4ca05d1375115d2100dd74b849347c6e35b0039bba6835019d3bda82`.

Primo e unico comando per i due target:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-observation-tests-001/test_runtime_observation.py QualificationTests.test_peak_34111550_includes_headers_and_rejects_before_namespace QualificationTests.test_exact_peak_budget_is_admitted
```

Exit 0, 2 PASS, 0 FAIL, 0 ERROR, 0 skip, 0 expectedFailure.
Stderr termina con:

```text
Ran 2 tests in 0.076s

OK
```

PID=PGID=SID=1102185, raccolto realmente con `Popen.wait`.
Durata outer osservata 0.2662394419312477 secondi, non un benchmark.

Soltanto dopo quel PASS, unica invocazione della suite:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-observation-tests-001/test_runtime_observation.py
```

Exit 0, 37 PASS su 37 metodi contati staticamente, 0 FAIL, 0 ERROR,
0 skip, 0 expectedFailure. Tutte le 37 righe dei metodi terminano con `ok`.
Stderr termina con:

```text
Ran 37 tests in 0.827s

OK
```

PID=PGID=SID=1102191, raccolto realmente con `Popen.wait`.
Durata outer osservata 1.2201736469287425 secondi, non un benchmark.
Exit outer 0. Ricevuta complessiva: `PASS_MOCK37`, `targeted_pass=true`,
`suite_launched=true`, `slot_released=true`.
Nessun segnale inviato, nessun retry, skip, fix o taglio di guardie.
La supervisione non interrompe sulle parole `ERROR` o `FAIL`.

## Pin, output e conservazione dei fallimenti precedenti

Input correnti autenticati prima di ciascun figlio e invariati dopo raccolta:

| File sotto `tmp/m1/fft-pocket/` | SHA-256 |
| --- | --- |
| `runtime-observation-tests-001/test_runtime_observation.py` | `e8333f5cfd3d6df5888e88175de5cb4020eea458267c0f55f0f120359ff0058e` |
| `supervisor.py` | `ad776c3bac3a0b953b2b8a8ac85dd6c395ecf0e5c85a3bf95179d4553ab21cb4` |
| `validation-observation-001.json` | `a658f12d0f273d411d6e163141e983d282e872602d0b1f0e5df9f05415bf3b35` |

Verificati anche `SOURCE_SHA`, pin manifest/ricevuta build003 e le cinque
fonti del profilo, con supervisor storico nello snapshot previsto.
`authenticated-inputs.json` elenca 59 file con byte e SHA-256.
Entrambi i postflight confermano tutti i 59 pin invariati.
Sono preservati i 12 file di attempt001, i 9 di attempt002, i 15 di attempt003,
i 5 di `peak-fixture-fix-001`, report002/report003 e i 6 file `FIRSTFAIL*`
precedenti sotto `tmp/`. Il precedente FAIL003 e il suo traceback restano prove
immutabili; questo PASS non li sostituisce.

| Stream | Byte | SHA-256 |
| --- | ---: | --- |
| target stdout | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| target stderr | 372 | `b88d42055ba270a5ab4d6fa99a1251b35974519147ebb4f44a7d1d88385c9bc8` |
| suite stdout | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| suite stderr | 4726 | `b0b5aa6e1287d33671bac1e74a961d47cb87d64b9a349bf4cf13b3234529cf83` |

Artefatti nuovi sotto `tmp/m1/fft-pocket/tests-attempt-004/`: outer, pin
autenticati, ricevuta complessiva e, separatamente in `target/` e `suite37/`,
preflight, launch, postflight, ricevuta, raw stdout/stderr completi e directory
private di ambiente. JSON e raw sincronizzati con fsync dei file e directory.

## Ambiente, budget e raccolta

Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.
Piattaforma `Linux-7.0.0-38-generic-x86_64-with-glibc2.43`.
HOME, TMPDIR, TMP, TEMP e XDG cache/config/data/runtime puntano a directory
private 0700 sotto ROOT. Umask 0077, ambiente esplicito, stdin DEVNULL.

Occupazione fisica di `ROOT/tmp/`, deduplicata per device/inode con st_blocks:

| Fase | Preflight byte | Postflight byte |
| --- | ---: | ---: |
| due target | 928112640 | 928133120 |
| suite37 | 928178176 | 928202752 |

Quota 1073741824 byte, riserva 67108864 byte, margine 16777216 byte.
Ogni preflight verifica anche 6291456 byte di budget log separato.
Cap output 1048576 byte per stream, stderr conservati integralmente entro cap.
Limiti figlio soft=hard: AS 268435456 byte, CPU 60 secondi, FSIZE 1048576 byte,
NOFILE 64, CORE 0. Wall 120 secondi, cleanup 10 secondi.
Questi limiti non sono misure RSS o qualifiche prestazionali.

Ogni figlio Popen ha sessione propria. L'outer osserva esclusivamente il proprio
PID con `waitid(P_PID, ..., WEXITED|WNOHANG|WNOWAIT)`, verifica PGID/SID
e mantiene il PID riservato fino alla raccolta reale con `Popen.wait`.
Gli eventuali segnali riguardano soltanto il gruppo proprio verificato;
in questo turno non ne sono stati necessari. Entrambi i figli sono raccolti,
pipe e selector chiusi e log sincronizzati prima del rilascio dello slot.

## Limiti del PASS

PASS di test stdlib con mock, non qualificazione OS di ownership/mappe/loader,
né prova numerica FFT. Le chiamate simulate del test con `all_232` nel nome
non eseguono 232 FFT reali. Nessun worker, build, GUI, host runtime verifier,
rete, Git, installazione, indice o delega. Nessuna modifica a fonti,
progress o index. Non si deriva da questo esito un'approvazione runtime reale.
