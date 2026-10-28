# M1.a: riproduzione del MED annidato, red002

26 ottobre 2026. **REPRODUCED_KNOWN_MED**: unico test target, un fallimento atteso,
exit1. Nessuna correzione al benchmark, retry o suite completa. Manifest resta
`KNOWN_MED_PENDING_FIX`. Il PASS47 storico non chiude questo finding.

## Root cause della fixture e prova separata

Riconciliati red001, PID1251943 raccolto e report SHA
`28fde6527abb37ee2afa30dfde2f6c6a00eb7ccfe9c0d4c6fd85eeb6b051a11c`.
Letto il codice effettivo: `record` scrive, flush, fsync; `run_series` apre un
file testo con buffering predefinito e chiama `child.close()` nel finally interno.
La fixture precedente collegava direttamente BytesIO a TextIOWrapper e ometteva
il BufferedWriter del percorso file reale. Il buffer testo, da solo, non conserva
il pending write per il retry osservato nel percorso con buffer binario.

Microprobe IO separato, esplicitamente ammesso, senza import benchmark, unittest,
Popen o accesso a candidati:

```sh
python3 -I -B -S tmp/m1/fft-pocket-benchmark/primary-error-chain-tests-002/io_probe.py
```

`io_probe.json` dimostra sul Python corrente:
- collegamento diretto: `write, body-finally`, una sola eccezione originale;
- BufferedWriter interposto: `write, body-finally, write, write`, catena naturale
  context `wrapper close, wrapper close, original flush`; raw chiuso.

La fixture usa ora TextIOWrapper/BufferedWriter/BytesIO faulting. Solo lo strato
FileIO è sostituito dal raw in memoria. Non vengono assegnati cause/context.
Assert su sequenza completa, tre write, raw chiuso, messaggi dei tre errori e
identità del registro sull'eccezione originale. Nessun assert diagnostico allentato.

## Unico turno target e risultato

```sh
python3 -I -B -S tmp/m1/fft-pocket-benchmark/primary-error-chain-tests-002/outer.py
```

Il launcher esegue una sola volta:

```sh
python3 -I -B -S tmp/m1/fft-pocket-benchmark/test_protocol.py CombinedFailureTests.test_nested_results_close_preserves_failure_ledger
```

Log: `Ran 1 test in 0.004s`, `FAILED (failures=1)`. Fallisce a test_protocol.py:652:
`self.assertEqual(after['primary_error'], prior['primary_error'])`.
È il controllo del contratto diagnostico, non un prerequisito della fixture.
Il metodo contiene 21 siti assert, inclusi helper; non è un conteggio runtime.

Prima del fallimento sono passati i controlli sul registro originale, sul contesto
reale, sul blocco di nuovi lanci mock, su STOP bounded/exit2 e FIRSTFAIL persistito
nello store JSON mock. stdout conserva il confronto completo prima/dopo:

| Diagnosi | Prima del wrapper | FIRSTFAIL dopo stop |
| --- | --- | --- |
| Primaria | `result.persist`, OSError, `injected original results flush` | `main`, OSError, `injected wrapper results close` |
| Teardown | `teardown.stdout.close`, `injected nested stdout close` | `[]` |
| Ricevuta | `receipt.process.json`, `injected nested process receipt` | `[]` |

Il messaggio originale sopravvive soltanto nella catena context; fase primaria e
registri secondari non vengono preservati. La sequenza effettiva è
`raw.write, child.close, raw.write, raw.write`, contatore3. Gli assert successivi
su uguaglianza dei registri secondari non sono raggiunti per failfast; la loro
perdita è comunque visibile negli oggetti JSON salvati in stdout.

## Risorse, hash e conservazione

Dodici pin verificati prima dell'import e dopo il turno. Test e benchmark invariati
durante l'esecuzione; soltanto manifest aggiornato successivamente con l'esito.
Manifest eseguito: `39071d2d496445b8ca8b61f85a5693ad4566630d5beee659e73a023ecaf93cf9`.
Preflight934785024 byte, st_blocks con hardlink deduplicati, quota1 GiB,
riserva64 MiB, margine16 MiB, budget log/receipt2424832 byte. Nessuna riserva nativa.
Cap wall55 s, CPU50 s, AS256 MiB, FSIZE1 MiB, NOFILE64, CORE0;
umask077, ROOT0700, HOME/TMP/XDG privati. Linux, Python3.14.4.
PID1257945 osservato WNOWAIT e raccolto: exit1, nessun timeout/segnale, riserva
rilasciata, slot reale libero. Outer exit0 significa classificazione del RED
riuscita, non PASS del test. Durata esterna0.17913864506408572 s.

213 file protetti invariati, incluse prove47 e red001. Sette FIRSTFAIL storici
conservano aggregato `5c32e5fa5d456d46df2443a550d69b4a44ee1d16a6d9522d8c2fc1f6bf37817c`;
l'ottavo red001 è invariato, red002 aggiunge il nono. Nessun vecchio report modificato.
Snapshot/diff in `primary-error-chain-tests-002/`, log in `primary-error-chain-red-002/`.

| File | SHA-256 finale |
| --- | --- |
| benchmark.py, invariato | `6a8a548ca1088c99ce2296444f1117f481b487b345bdd319a3444f33c0ca5a1c` |
| test_protocol.py | `70b304067feb8df0893cd2dd20b33a1ce1bd7cc98a603d681415ffdb4881e076` |
| manifest.json | `ae98406900dc897ed5ffa6a790c538eebdc6f272c11e373d0955f60ea7f47b01` |
| source.diff | `f231abb641ce31976341b103921b5050586df1d4c9cf5dc05d9a26c286de5941` |
| stdout.log | `084d08a09e27e383a733c4c6a9ccc82267d0f88874ae97d1453732c81038f561` |
| stderr.log | `db6baa5f73dfcf7bff35791570a894c0a46208ea0ba9be1610e69c92106d4231` |
| result.json e FIRSTFAIL.json | `b2d1c2681cb33a410683787e6cd146c9638bac1d16c733f4604e54e1846736ce` |

Tutti i pin in `primary-error-chain-tests-002/after.json`. Proposta offline non letta
né applicata; PLAN, driver e supervisore invariati. Nessun nativo, Git, rete,
installazione o delega. Nessuna prova di durabilità filesystem, adozione o chiusura M1.
