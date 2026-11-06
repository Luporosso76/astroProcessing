# M3.a, sotto-unità U13a: decoder DNG del progetto, instradamento, strutture e tag

Stato: **chiusa**. Test: review PASS al giro 2. Codice: review FAIL al giro 2 (1 MED: offset di IFD ripetuto senza esito dichiarato), arbitraggio del coordinatore (contratto emendato, correzioni di codice e di test), review sul delta PASS (blocco «Review»).

| Campo | Valore |
|---|---|
| Unità | M3.a / U13a (test e codice) |
| Scopo | Un solo punto di scelta fra LibRaw e decoder del progetto prima di ogni chiamata a LibRaw o a zlib (U13 §3); lettura verificata di IFD0, catena e `SubIFDs` con tetto di 256 IFD; controlli dei tag del perimetro (§4, §6); prenotazione di memoria, cancel e decompressione deflate dei segmenti (§5, §7). Dopo l'ultimo segmento il decoder si ferma con `unsupported`, `raw.dng_decoder_incomplete`: la costruzione dell'immagine è di U13b |
| ID | T-U13-01, T-U13-02, T-U13-06; emendamenti di ADR-08.b (§11 del contratto) |
| Comando | Integrazione: build pulita, `tools/check.sh` e `ctest --preset release` completo (blocco generato). Mutazioni: `tools/mutate.sh` (patch in `tmp/u13a-mut/`, `tmp/u13a-r1-mut/`, `tmp/u13a-arb-mut/` del worktree) |
| Versioni | GCC 15.2.0, CMake 4.2.3, Ninja, zlib di sistema, LibRaw 0.22.3 (third_party) |
| Ambiente | Linux 7.0.0-38-generic x86_64, 16 CPU; worktree `.claude/worktrees/opus-u13ac` sulla base `5dc34ce` |
| Autore e data | Test: agente Claude Opus 5.5 (abbonamento); test dell'arbitraggio: agenti Claude Sonnet 5.5 (API). Codice: agente Claude Opus 5.5 (abbonamento). Review: Claude Opus 5.5 (Argon fuori quota). Coordinatore: Claude Opus 5.5. 5 e 6 novembre 2026 |

## Decisioni del coordinatore

- U13 §3 punto 2: senza `DNGVersion` in IFD0 il file va a LibRaw (`tiff_not_dng`) senza leggere catena e `SubIFDs`; con `DNGVersion` si leggono catena e `SubIFDs`, al massimo 256 IFD distinti (il 257° dà `limit_exceeded`, `raw.max_ifds`, con `funzione`, `offset`, `limite`); il ciclo si controlla sulla catena; un offset già letto si legge e si conta una volta sola, con la classificazione della prima lettura (`SubIFDs` prima della catena).
- U13 §4: la cella CFA è uno dei quattro pattern Bayer; {0, 2, 1, 1} non lo è.
- `max_route_ifds` sta in `astro::raw_limits` con gli altri limiti di ADR-08.b §2.1.
- `BlackLevel` assente: nessuna tabella di R·C valori materializzata (un file di poche centinaia di byte con `BlackLevelRepeatDim` 65535 × 65535 avrebbe allocato circa 17 GB).

## Mutazioni

Tutte rilevate con `tools/mutate.sh`.

| Gruppo | Patch |
|---|---|
| Codice, giro 1 | `m1-route-libraw-32` (LibRaw scelto per il deflate intero a 32 bit), `m2-missing-tag`, `m3-segment-bounds`, `m4-error-to-libraw` (errore del decoder del progetto passato a LibRaw) |
| Test dopo il giro 1 | `max-ifds-chain`, `max-ifds-subifds`, `max-ifds-boundary`, `max-ifds-after-table`, `non-dng-after-chain`, `cfa-not-bayer`, `cfa-table-gbrg`, `black-dim-allocation`, `tag-repeated`, `bits-sampleformat-count`, `cfa-count-upper`, `default-scale-zero-denominator`, `black-dim-zero`, `black-delta-count`, `black-delta-denominator` |
| Arbitraggio | `repeated-counted-twice`, `repeated-is-structure-error`, `catena-prima` (catena letta prima di `SubIFDs`) |

## Limiti

- Confine con U13b: ogni DNG valido instradato al decoder del progetto si ferma con `raw.dng_decoder_incomplete` dopo la decompressione; nessuna immagine prodotta in U13a.
- Il tetto di 256 IFD è una scelta del progetto, non della specifica (U13 §13).
- La sonda di allocazione del caso `BlackLevelRepeatDim` sta in un eseguibile separato (`test_dng_u13a_alloc`), perché ridefinisce `operator new`.

## Review

- Test, giro 1 FAIL (2 MED), giro 2 PASS.
- Codice, giro 1 FAIL (3 MED), giro 2 FAIL (1 MED). Arbitraggio: review sul delta `tmp/api/u13acode-arb-review.json` PASS con 2 LOW (classificazione di un IFD raggiunto per due vie; test del tetto che non rilevava da solo `repeated-counted-twice`), chiusi.

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-08-decoder-pack.md` | `6e020ced8da401f8d26c69e610275020f00762ed5b358d0a98c94ffa20392fd8` |
| `docs/contracts/U13-dng-decoder.md` | `04d2ff233693090bdb607238b31a16b70f4c7aee767ddbe517042b6efd7717e6` |
| `src/core/dng_decode.cpp` | `f8fa0661919e241a4090a99e0fc50c61b374d42238c6bc339e1d77fcc91422d3` |
| `src/core/dng_internal.hpp` | `a2a09c607fc1499740e4c1dec90aebb84a3674a17f80dff62df28d8d55f03e20` |
| `src/core/include/astro/input_format.hpp` | `40e4924aaf3a808ed54e394cc187cf2be1efb075c699dc73a1ba5af2ff6421d7` |
| `src/core/include/astro/raw_decode.hpp` | `7ab60332832da275e2f625f635ac86a6218b2aa1847d41ac45b7d28ea6b917f4` |
| `src/core/raw_decode.cpp` | `0e8ce66a76ebf3704db608ff5e1facf827b3aa132ca56c33d952ff7c96c80f90` |
| `tests/fixtures/syn_dng.hpp` | `2253a1bda17f45101d32b6744d56821aa4b0a8b524c74d78e0a0b372016e04c4` |
| `tests/unit/test_dng_u13a.cpp` | `b1e8690671cae687c2d7036ef80d67c42de6053b504a7753b883dfdaa9bdf6db` |
| `tests/unit/test_dng_u13a_alloc.cpp` | `d1008157600e5a1b0080364ddc67e0978cd8fe0706096fa2d8e6ce3d56929541` |

- Test release (`tmp/release-junit.xml`): totali 1081, fast 1063, slow 18, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 1063, fast 1063, slow 0, falliti 0
<!-- evidence:end -->
