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
| `docs/contracts/ADR-08-decoder-pack.md` | `44cc42cda59c6b958f1911a643a07ddc0fe02f7e82aa01885d43b672051a4195` |
| `docs/contracts/U13-dng-decoder.md` | `396393dd3d01f44f7ab1042cbf1b17a15b2a2aba99fb8d04eed13ad89895ba61` |
| `src/core/dng_decode.cpp` | `337735e3da8c1237796b971b7a27520a5c3674970e5b6363cacf2fab2ed008e1` |
| `src/core/dng_internal.hpp` | `ac95b16fc39ffd6fd7668cbcaeba341f534b7f4af484811839705e1e8cb3685d` |
| `src/core/include/astro/input_format.hpp` | `4f916d0018988fcb99482d6e8458d28d50cda011d7d9c25fdcbd4e229d5ca4d4` |
| `src/core/include/astro/raw_decode.hpp` | `693013684e5512f4e08be7114f13facb5cddc41b00c11e24d43da49aacd22dba` |
| `src/core/raw_decode.cpp` | `962885ee61bb61c8c72002f357367922909a1699d7e16fbdf48d5cb023d1513b` |
| `tests/fixtures/syn_dng.hpp` | `6262496fd537e5557b53140f492a2ddc96214508f0514e306ef12d0fc106025f` |
| `tests/unit/test_dng_u13a.cpp` | `b1e8690671cae687c2d7036ef80d67c42de6053b504a7753b883dfdaa9bdf6db` |
| `tests/unit/test_dng_u13a_alloc.cpp` | `d1008157600e5a1b0080364ddc67e0978cd8fe0706096fa2d8e6ce3d56929541` |

- Test release (`tmp/release-junit.xml`): totali 1221, fast 1199, slow 22, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 1199, fast 1199, slow 0, falliti 0
<!-- evidence:end -->
