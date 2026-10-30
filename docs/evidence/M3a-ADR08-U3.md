# M3.a: emendamento di ADR-08.b dopo i test di U3

Stato: **CHIUSO per arbitraggio del coordinatore dopo il giro 2**. Unità: emendamento del contratto `docs/contracts/ADR-08-decoder-pack.md` (§2.1 `max_raw_memory_mb`, §2.7, §2.8, §6 righe T-M3A-RAW-06 e T-M3A-RAW-08, §7) sui fatti emersi dai test di U3. Solo testo del contratto e una sonda usa-e-getta in `tmp/`: nessun codice di prodotto, nessun test del repository scritto o eseguito.

| Campo | Valore |
|---|---|
| Unità | M3.a / ADR-08.b, emendamento dopo U3 |
| Scopo | Fissare il valore di `max_raw_memory_mb` e la prenotazione di `raw_image` sulla soglia misurata di LibRaw; separare il controllo dello strumento di T-M3A-RAW-08 dalla prova di H6 |
| Comando | `bash tmp/adr08-u3/run.sh > tmp/adr08-u3/run3.log 2>&1` (compila con `g++ -std=c++20 -O1 -Wall -Wextra -o probe probe.cpp $(pkg-config --cflags --libs libraw_r)`, `ulimit -c 0`, `TMPDIR` in `tmp/adr08-u3/`); `tools/review.sh adr08-u3-round2 tmp/adr08-u3-facts2.md`; `python3 -S tools/evidence.py docs/evidence/M3a-ADR08-U3.md` |
| Input | Commit `33762a9`; sonda dei test di U3, sola lettura, nel worktree `.claude/worktrees/agent-aaa1690a9c1c9919e` (`tmp/u3-probe/p.cpp`, `q.cpp`); 8 file di PUB-RAW-01 in `/opt/workspace/astroProcessor/data/public/raw-pixls-us/`, sola lettura, SHA-256 stampati nel log e uguali agli 8 valori di `docs/datasets/PUB-RAW-01.md`; DNG sintetici scritti dal generatore dei test |
| Versioni | LibRaw 0.21.5 (`LibRaw::version()` = `0.21.5-Release`, `pkg-config libraw_r` 0.21.5), pacchetto `libraw23t64` 0.21.5b-1ubuntu1.1, `libraw_r.so.23.0.0` SHA-256 `3484c3eecb3614c845cadd4c4de59ff124c1646317e34438e98394e2edab3010`; `libgomp1` 16-20260322-1ubuntu1; g++ 15.2.0 |
| Ambiente | Linux 7.0.0-38-generic x86_64, macchina di sviluppo |
| Sonda | `tmp/adr08-u3/probe.cpp`, `tmp/adr08-u3/run.sh` (`017ebc3c943586b5e68ca055b887f45464936deb753f7cfc080a2aada942ba3b`), copia del generatore dei test `tmp/adr08-u3/syn_dng.hpp` (`dd62bccd32bc239e8389eb9aa5b8457e74850030888fc07e6413b9ee8d39162c`, uguale a quello del repository e del worktree dei test). Log di riferimento, uscita 0: `tmp/adr08-u3/run3.log` (`1311838180726aa83d59e875a34d6c77c3809b9e419598474b7e8e3ff3ebb504`), sonda SHA-256 `a399c147a7a5239358a17b00f7bf4cbc616944a419e85fa1091ccd487b365262`, che registra a parte il codice di `open_buffer` della prova con il limite impostato dopo e il codice con limite 0 per ogni campione. `tmp/adr08-u3/run2.log` (`d8882bca101e2f90293e6ea643eda061e8f251f62f2defe09b640e5c33a95222`, sonda `5a3e821e4a899867dbea63ea1dedfd1d02cfeb7da822c6d56fefed099879ad49`, giro 2) e `tmp/adr08-u3/run1.log` vengono da versioni precedenti della sonda; i valori comuni coincidono, nessun valore della tabella è cambiato con `run3.log`. Fuori da Git (`tmp/`) |
| Risultato | Soglia di `max_raw_memory_mb` misurata su 20 campioni e uguale alla formula in tutti; nessuno dei 9 campioni attraversa una regione parallela di LibRaw; contratto emendato |
| Output | File elencati nel blocco generato sotto |
| Ambito | I 4 rilievi MED del giro 1 e le clausole che ne dipendono. Fuori: §2.2–§2.6, §2.9, §3–§5 del contratto; test e codice di U3 |
| Esito | FAIL al giro 2 con 2 MED (tetto di 2 giri); corretti su arbitraggio del coordinatore, che legge il delta (`tmp/adr08-u3-arbitrato.diff`); vedi «Review» |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Misure

Memoria, modo `probe mem`. Per ogni campione la sonda prova `max_raw_memory_mb` da 0 in su, ciascuno con un oggetto LibRaw nuovo e il valore impostato prima di `open_buffer`, fino al primo `unpack` riuscito (`min_ok`); poi verifica `min_ok + 1` e ripete `min_ok` e `min_ok − 1` impostati dopo `open_buffer` (il valore iniziale dell'oggetto è 2048). `M = ⌈2 · raw_width · (raw_height + 8) / 2^20⌉`.

| Campione (`raw_width` × `raw_height`) | `raw_image` (byte) | 2 · w · (h + 8) (byte) | ⌈`raw_image`⌉ MiB | `M` | `min_ok` | `M − 1` |
|---|---|---|---|---|---|---|
| DNG 64 × 48 | 6 144 | 7 168 | 1 | 1 | 1 | `LIBRAW_TOO_BIG` |
| DNG 2048 × 1040 | 4 259 840 | 4 292 608 | 5 | 5 | 5 | `LIBRAW_TOO_BIG` |
| DNG 2048 × 1024 | 4 194 304 | 4 227 072 | 4 | 5 | 5 | `LIBRAW_TOO_BIG` |
| DNG 2048 × 1023 | 4 190 208 | 4 222 976 | 4 | 5 | 5 | `LIBRAW_TOO_BIG` |
| DNG 2048 × 1016 | 4 161 536 | 4 194 304 | 4 | 4 | 4 | `LIBRAW_TOO_BIG` |
| DNG 2048 × 1017 | 4 165 632 | 4 198 400 | 4 | 5 | 5 | `LIBRAW_TOO_BIG` |
| DNG 1024 × 512 | 1 048 576 | 1 064 960 | 1 | 2 | 2 | `LIBRAW_TOO_BIG` |
| DNG 1024 × 504 | 1 032 192 | 1 048 576 | 1 | 1 | 1 | `LIBRAW_TOO_BIG` |
| DNG 1024 × 505 | 1 034 240 | 1 050 624 | 1 | 2 | 2 | `LIBRAW_TOO_BIG` |
| DNG 1024 × 511 | 1 046 528 | 1 062 912 | 1 | 2 | 2 | `LIBRAW_TOO_BIG` |
| DNG 2050 × 1023 | 4 194 300 | 4 227 100 | 4 | 5 | 5 | `LIBRAW_TOO_BIG` |
| DNG 4000 × 3000 | 24 000 000 | 24 064 000 | 23 | 23 | 23 | `LIBRAW_TOO_BIG` |
| Fujifilm X-A1 (5120 × 3288) | 33 669 120 | 33 751 040 | 33 | 33 | 33 | `LIBRAW_TOO_BIG` |
| Olympus E-M10 (4640 × 3472) | 32 220 160 | 32 294 400 | 31 | 31 | 31 | `LIBRAW_TOO_BIG` |
| Sony ILCE-7S (4288 × 2848) | 24 424 448 | 24 493 056 | 24 | 24 | 24 | `LIBRAW_TOO_BIG` |
| Panasonic DMC-GH4 (4816 × 3472) | 33 442 304 | 33 519 360 | 32 | 32 | 32 | `LIBRAW_TOO_BIG` |
| Fujifilm X-E1 (5120 × 3288) | 33 669 120 | 33 751 040 | 33 | 33 | 33 | `LIBRAW_TOO_BIG` |
| Canon EOS 6D (5568 × 3708) | 41 292 288 | 41 381 376 | 40 | 40 | 40 | `LIBRAW_TOO_BIG` |
| Pentax K-x (4352 × 2868) | 24 963 072 | 25 032 704 | 24 | 24 | 24 | `LIBRAW_TOO_BIG` |
| Nikon D750 (6032 × 4032) | 48 642 048 | 48 738 560 | 47 | 47 | 47 | `LIBRAW_TOO_BIG` |

In tutti i 20 campioni (`run3.log`): `min_ok = M`; `min_ok + 1` riesce; impostato dopo `open_buffer` (che riesce, codice 0, registrato a parte), `M` riesce e `M − 1` dà `LIBRAW_TOO_BIG` (−100012); il valore 0, registrato per ogni campione, dà `LIBRAW_TOO_BIG`; `raw_pitch` vale 0 dopo `open_buffer` e `2 · raw_width` dopo `unpack`. Il caso 2048 × 1016 (2 · w · (h + 8) uguale a 4 MiB esatti, passa con 4) mostra che il confronto di LibRaw è stretto; i casi 2048 × 1024, 2048 × 1023, 1024 × 512, 1024 × 505, 1024 × 511, 2048 × 1017 e 2050 × 1023 mostrano che arrotondare per eccesso il solo `raw_image` darebbe un valore troppo basso.

Thread, modo `probe thr`, un processo per campione: (a) LibRaw con `omp_set_num_threads(1)`, (b) con `omp_set_num_threads(2)`, conteggio delle chiamate a `pthread_create` interposto negli intervalli `open_buffer` e `unpack`; poi le creazioni del processo fino a quel punto; poi (c) una regione parallela propria a due thread; poi una seconda regione propria a due thread. Nella sonda (c) segue (a) e (b) nello stesso processo: è equivalente al processo separato del contratto solo perché (b) non ha creato thread; il test di T-M3A-RAW-08 esegue (c) in un processo nuovo separato (§2.7).

| Campione | (a) `open_buffer` / `unpack` | (b) `open_buffer` / `unpack` | Creazioni prima di (c) | (c) | Seconda regione |
|---|---|---|---|---|---|
| DNG 2048 × 1040 e ciascuno degli 8 file di PUB-RAW-01 | 0 / 0 | 0 / 0 | 0 | 2 thread, 1 creazione | 2 thread, 0 creazioni |

Tutte le decodifiche riescono (`open_buffer` e `unpack` = 0).

## Rilievi del giro 1 e decisioni

| # | Rilievo (MED) | Decisione nel contratto |
|---|---|---|
| 1 | Arrotondamento per eccesso e «non oltre la prenotazione» incompatibili sotto 1 MiB | §2.1: `M = max(1, ⌈2 · raw_width · (raw_height + 8) / 2^20⌉)`, impostato dopo `open_buffer` e prima di `unpack`; §2.8: la parte di `raw_image` della prenotazione è `M · 2^20` byte, quindi il limite coincide con la prenotazione per costruzione. La regola del giro 1 (arrotondare `raw_image`) era sbagliata al confine: corretta sulla soglia misurata. §2.8 non usa più `raw_pitch · raw_height`, che vale 0 prima di `unpack`. Caso sotto 1 MiB misurato: 64 × 48 → 1 |
| 2 | T-M3A-RAW-06 senza valore esatto né confine del MiB | Riga di §6 con cinque casi e valori attesi (64 × 48 → 1, 2048 × 1040 → 5, 2048 × 1016 → 4, 2048 × 1017 → 5, 2048 × 1024 → 5), uguaglianza esatta su `max_raw_memory_mb` e sui byte prenotati, `M` calcolato dal test dalle dimensioni scritte, controllo della libreria `M − 1` → `LIBRAW_TOO_BIG` |
| 3 | §2.7 e §7 non allineati col nuovo §6 | §2.7: (a) e (b) in un processo nuovo per campione, (c) in un altro processo nuovo separato (arbitraggio dopo il giro 2); (c) prova solo lo strumento; condizione necessaria per H6: (b) con almeno una creazione e (a) con 0; con (b) a 0 il test registra «H6 non provata». §6 riga T-M3A-RAW-08 uguale. §7: T-M3A-RAW-08 verde non vale come prova di H6; nessuno dei 9 campioni soddisfa la condizione |
| 4 | Esiti senza log verificabile | Questo record, con sonda, log, versioni e SHA-256 degli input; il contratto lo cita in §2.1 e §2.7 |

## Limiti

- La sonda prova LibRaw direttamente, non il codice del prodotto.
- La forma `2 · raw_width · (raw_height + 8)` descrive la soglia misurata su 20 campioni (DNG non compressi e 7 formati proprietari); la sorgente di LibRaw non è letta. Per i formati non misurati è un'ipotesi; se è errata, l'effetto è uno stop `raw.too_big`, non un dato sbagliato.
- Nessun campione disponibile attraversa una regione parallela di LibRaw in `open_buffer` o `unpack`: H6 resta non provata. Il riuso dei thread di libgomp è misurato solo con regioni proprie della sonda.
- Il picco di RSS non è misurato qui (lo registra T-M3A-RAW-06).

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-adr08-u3-round1-20261030-1751.md` | primo giro (`tmp/adr08-u3-round1.diff`, fatti `tmp/adr08-u3-facts1.md`) | FAIL | 4 MED, sopra |
| 2 | `.omc/artifacts/ask/codex-adr08-u3-round2-20261030-1759.md` | `tmp/adr08-u3-round2.diff`, fatti `tmp/adr08-u3-facts2.md` | FAIL | 2 MED, corretti su arbitraggio del coordinatore: (1) il controllo (c) dopo (b) può dare 0 creazioni proprio quando (b) ne ha create (riuso del pool di libgomp), e far fallire un test corretto; va in un processo nuovo separato da (a) e (b), in §2.7, §6 e in questo record; (2) la sonda non registra a parte l'esito di `open_buffer` nella prova «dopo `open_buffer`» (`after_rc` parte da 0) né il codice ottenuto con limite 0 per i campioni con `M > 1` (il log lo mostra solo per 64 × 48 e 1024 × 504) |
| Arbitraggio | coordinatore, nessun terzo giro | `tmp/adr08-u3-arbitrato.diff` | — | (1) §2.7, riga T-M3A-RAW-08 di §6 e questo record: (c) in un processo nuovo separato da (a) e (b); (2) sonda corretta e rieseguita (`run3.log`): codici di `open_buffer` e del limite 0 registrati per ogni campione, nessun valore cambiato |

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-08-decoder-pack.md` | `8268d69f2d07206c8f86f7c775c6823e82061b173f736cb115929e0f43bc2359` |

<!-- evidence:end -->
