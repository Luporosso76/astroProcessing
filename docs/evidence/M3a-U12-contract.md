# M3.a U12: contratto delle librerie compilate nel progetto e sonde di ADR-08 su LibRaw 0.22.3

Stato: **FAIL al giro 2 (tetto di 2 giri), correzioni applicate e in attesa dell'arbitraggio del coordinatore**. Unità: contratto `docs/contracts/U12-third-party.md` (nuovo) ed emendamenti di `docs/contracts/ADR-08-decoder-pack.md`, `docs/contracts/ADR-13-offline.md`, `docs/licenses.md`. Solo testo e sonde usa-e-getta in `tmp/u12/`: nessun codice di prodotto, nessun test del repository scritto o eseguito, nessun file in `third_party/`.

| Campo | Valore |
|---|---|
| Unità | M3.a / U12, contratto |
| Scopo | Fissare versioni, fonti, licenze e opzioni di build di LibRaw, CFITSIO e WCSLIB compilate nel progetto; rifare sulla nuova LibRaw le sonde dei fatti di ADR-08.b |
| Comando | `tmp/u12/scripts/build_libs.sh all` (compila in `tmp/u12/build/`, installa in `tmp/u12/inst/`, 4 job, `TMPDIR` in `tmp/u12/tmpdir`); `tmp/u12/probes/run.sh > tmp/u12/run5.log 2>&1` (uscita 0; `ulimit -c 0`; `run4.log` è l'esecuzione precedente, prima della correzione dei formati della sonda di U2); `tools/review.sh u12contract-contract1 tmp/u12-contract-facts.md` |
| Input | Commit `83d0f73`; tarball scaricati il 30 ottobre 2026 in `tmp/u12/dl/` (URL e SHA-256 nel contratto U12, §1); 8 file di PUB-RAW-01 in `data/public/raw-pixls-us/`, sola lettura, SHA-256 stampati in `run5.log` e uguali a `docs/datasets/PUB-RAW-01.md`; DNG sintetici dal generatore dei test (`tests/fixtures/syn_dng.hpp`, SHA-256 `dd62bccd32bc239e8389eb9aa5b8457e74850030888fc07e6413b9ee8d39162c`) e DNG deflate scritti con libtiff 4.7.0 |
| Versioni | LibRaw 0.22.3 (varianti `noomp` e `omp`, `libraw_r.so.25.0.0` SHA-256 `6d208d992b49bdcd52e194d067521365357afd3ef65dc81d09e978ddb8e512e6` e `89ce61fd1611ff329137c2b704180d875057e9e315d787c9d0a9e2cdaf58052d`); LibRaw 0.21.5 di sistema (`3484c3eecb3614c845cadd4c4de59ff124c1646317e34438e98394e2edab3010`); CFITSIO 4.7.0 e 4.6.3 di sistema; WCSLIB 8.10 e 8.4 di sistema; g++ 15.2.0; zlib 1.3.1; libtiff 4.7.0 |
| Ambiente | Linux 7.0.0-38-generic x86_64, macchina di sviluppo |
| Sonde | Sorgenti delle sonde di U2 (`u2probe.cpp`: originale `188816887d9a3a611d0953c1d4160e2f5a3f710b66549ad36736cbb04a0bb509`, uguale al record di U2; dopo la review del giro 1 le stampe di `linear_max` passano da `%ld` a `%lld` con conversione esplicita, perché nella 0.22.3 il campo è `unsigned`: `4e54203f84638fa6c333e7ee42abbd622fddfeb91cf985690b146c40b6b97fb4`; il log della 0.22.3 è identico prima e dopo la correzione), U3 (`u3probe.cpp`, `a399c147a7a5239358a17b00f7bf4cbc616944a419e85fa1091ccd487b365262`) e U4 (`u4probe.cpp`, `1b1b689f86698418d03f6c85fcfb1659468882a0f1cd043fc447a80902bd79ee`; `rafcheck.cpp`, `d79df52ff474a4ed718c74550fe08c98292fe335de3dc8ec3e790f86cc974e7a`), con hash uguali a quelli dei record `M3a-ADR08-U3/U4`; nuove: `extra.cpp` (`ca4523efd5bfed5bf015f5e5ddca237abb4c68976ab77a9914a4dd857011bca1`), `deflate.cpp` (`79f1b8cf9d0cd7e2c59076d41ede6e33cc006b4cb9a6afecf545d6fc28a2b409`), `fitsprobe.c` (`1c9cd680018fd483726d791fe2103373c693f6952db9fbb21216accf0b8c1bc3`), `wcsprobe.c` (`e2d61900b28d5d5671185ab7d6a30e29600fb833880888657bfa189986f63b0c`); `run.sh` (`99e43b91ce946af9e7f6c8e8e54cd2e612b308d2a4193c1b1bf30ff9d2b5eab5`). Tutto in `tmp/u12/probes/`, fuori da Git. Ogni sonda LibRaw è compilata tre volte: `old` (0.21.5 di sistema), `new` (0.22.3 senza OpenMP, opzioni del prodotto), `newomp` (0.22.3 con OpenMP); log in `tmp/u12/out/<variante>/` |
| Log | `tmp/u12/run5.log` (`88c9bfb32af3bd1481330a2a8a4df96673e62390840324f8fa892bd1d410282c`, 0 avvisi `-Wformat`); `out/old/u2.log` di `run5.log` `082b9fb695d5b16073324c3a6e24f68b2c8690e46cf664918586fc6da8365f71` (cambia a ogni esecuzione nelle righe `raw_image[0..2]` dei casi in virgola mobile col bit tolto; quello di `run4.log` era `0667b5226ad8f785765773541b32f802f80e5f4ef0b2594eff774823287f9cf1`); gli altri log hanno gli hash del giro 1 in `tmp/u12-contract-facts.md`; `tmp/u12/out/{old,new,newomp}/*.log` (hash in `tmp/u12-contract-facts.md`) |
| Risultato | `build_libs.sh` e `run.sh` con uscita 0. Una sola differenza di LibRaw 0.22.3 sulle sonde di U2, U3, U4 (virgola mobile col bit di conversione tolto: `raw_image` nullo); fatti nuovi su capacità, OpenMP, deflate, JPEG XL e VC-5; CFITSIO senza rete; Adobe DNG SDK escluso per licenza (decisione del proprietario) |
| Output | `tmp/u12/run5.log`, `tmp/u12/out/`; file del blocco generato sotto |
| Ambito | Contratto U12 e emendamenti di ADR-08.b (§1, §2.1, §2.2, §2.3, §2.5, §2.6, §2.7, §2.9, §4, §6, §7), ADR-13.a, `docs/licenses.md`. Fuori: `third_party/`, CMake, codice e test, che seguono il contratto |
| Limiti | Sezione «Limiti» sotto |
| Esito | FAIL al giro 2 (tetto). Arbitrato dal coordinatore: correzioni del giro 2 lette sul delta `tmp/u12-arbitrato.diff` del worktree e accettate; il paragrafo OpenMP nasce da una richiesta del coordinatore; velocità senza OpenMP dichiarata ipotesi non misurata |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Confronto delle sonde (0.21.5 contro 0.22.3)

| Sonda | Esito |
|---|---|
| U2 (geometria con origini dispari, H3 e `black_pattern_anchor`, virgola mobile, curva e digest, `cblack`, temperatura, `is_fuji_rotated`) | `diff out/old/u2.log out/new/u2.log`: 14 righe, solo la versione e i 4 casi in virgola mobile col bit di conversione tolto (0.21.5: `raw_image` non nullo con valori non definiti; 0.22.3: `raw_image` nullo). `new` e `newomp` identici. `linear_max` cambia tipo (`long` → `unsigned`), valori uguali |
| U3 memoria (`max_raw_memory_mb` minimo = `M` su 12 DNG sintetici e 8 file) | Identico salvo la riga della versione |
| U3 thread (a/b/c) | Identico: 0 creazioni in `open_buffer` e `unpack` con 1 e 2 thread OpenMP su 9 campioni per tutte e tre le varianti; controllo (c) 1 creazione |
| U4 (avvisi, `PARSEFUJI_PROCESSED`, confronto col lettore RAF, ora con `mktime` sotto `TZ` diversi) | `out/old/u4.log`, `out/new/u4.log`, `out/newomp/u4.log` identici byte per byte (`e68bdbfccddb570b067427ba50eacab38cd7c50bd33cedde98e23a08a180933c`). Fusi: DNG sintetico con `UTC0`, `Europe/Rome`, `America/New_York`, vuota, assente; file reali con `UTC0` ed `Europe/Rome` |
| Extra | VC-5 richiede il GPR SDK di GoPro, JPEG XL il DNG SDK (sorgenti di LibRaw). 0.22.3: `version` `0.22.3-Release`, `versionNumber` `0x001603`, `libraw_capabilities` `0xc0` (0.21.5: `0x80`); valori iniziali di `user_black`, `user_cblack`, `user_sat`, `rawparams.options` (`0x2`), `max_raw_memory_mb` (2048) uguali; `raw_pitch` 0 dopo `open_buffer` e `2 · raw_width` dopo `unpack` su 9 campioni, uguale; `Compression` 52546 e 9: 0.22.3 `open_buffer` 0, `LIBRAW_DECODER_UNSUPPORTED_FORMAT`, `unpack` −2; 0.21.5 `open_buffer` −2 |
| Deflate | Interi a 16 bit: `unpack` −100008 in tutte le varianti; virgola mobile a 32 bit: decodifica solo con 0.22.3 (zlib) |
| Collegamento | `NEEDED`, simboli di rete, `GOMP_*` e funzioni di tempo delle librerie: in `run5.log`, «collegamento delle librerie compilate» |
| CFITSIO | 4.7.0: 0 librerie e 0 chiamate di rete, URL rifiutati con stato 124; 4.6.3: 8 librerie di rete, 12 chiamate, stato 104 |
| WCSLIB | 8.10 e 8.4: stessa trasformazione TAN (83.756520910077, 22.102231047141) |

## Limiti

- Sonde sulle varianti condivise; il prodotto collega gli archivi statici (contratto U12 §5).
- I sorgenti delle sonde di U2, U3, U4 leggono membri protetti di LibRaw e chiamano `adjust_bl`: solo per capire la libreria, vietato al prodotto.
- Nessuna sonda su un DNG JPEG XL reale: il caso è misurato col solo tag `Compression` riscritto.

## Review

| Giro | Artefatto | Esito |
|---|---|---|
| 1 | `.omc/artifacts/ask/codex-u12contract-contract1-20261030-2136.md` | FAIL: 3 MED, 1 LOW. (1) regola delle licenze limitata alle librerie compilate, contro `plans/M3a.md` («ogni dipendenza diretta»): regola estesa, obbligo 12 di U12 per le dipendenze dirette di sistema; (2) VC-5 attribuito al DNG SDK invece che al GPR SDK: corretto in ADR-08.b §1, §7 e U12 §3, §5; (3) `%ld` su `linear_max` `unsigned` nella sonda di U2: formati corretti, sonde rieseguite (`run5.log`), log della 0.22.3 invariato, cambio di tipo scritto in ADR-08.b §2.4; LOW: citazione di LGPL-2.1 §3 resa fedele, con il cambio degli avvisi |
| 2 | `.omc/artifacts/ask/codex-u12contract-contract2-20261030-2144.md` | FAIL: 3 MED, 1 LOW, 1 fuori delta. (1) T-U12-08 non discriminante su un collegamento a `OpenMP::OpenMP_CXX` senza chiamate: aggiunti i controlli su comandi di compilazione e collegamento e su `NEEDED`; (2) «nessuna velocità persa» non misurata: riscritta, equivalenza dei tempi dichiarata ipotesi; (3) delta oltre i rilievi: il paragrafo OpenMP è una richiesta del coordinatore, all'arbitraggio; LOW e fuori delta: hash di `out/old/u2.log` e riferimenti a `run5.log` e alla sonda corretta allineati. Correzioni dopo il tetto di 2 giri, non riviste: arbitraggio del coordinatore (`tmp/u12-arbitrato.diff`) |

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-08-decoder-pack.md` | `b5abcafc55e824f9d7b2ac19a08fae98e66e70e125e33561ee38a8841c93ae98` |
| `docs/contracts/ADR-13-offline.md` | `4312bd3ad66eecd9a56d9402f6639dabcdd30c2792455fa321e2b7a5ae42b135` |
| `docs/contracts/U12-third-party.md` | `846641dec75e4f9a6e03a97c8bae5affa1ff1bfdb79dd76cd159c082e1ad6484` |
| `docs/licenses.md` | `b9ffb9334ff642779c86cbd8c553b36f6b07887526801a5571e66fb0f2b6ffe1` |

<!-- evidence:end -->
