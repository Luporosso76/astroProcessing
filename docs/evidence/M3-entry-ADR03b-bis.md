# M3.entry, ADR-03.b (completamento): alpha, capability di misura, tabella delle operazioni

Stato: **review PASS al giro 3 di 3**, nessun rilievo aperto. Unità: M3.entry, completamento di ADR-03.b. Solo testo: nessun codice, nessun test eseguito.

| Campo | Valore |
|---|---|
| Unità | M3.entry / ADR-03.b, completamento (§b.12–b.18) |
| Scopo | Scrivere nella parte `.b` dell'aggregato ADR-03 ciò che mancava: alpha per l'export TIFF (C-10.b, M3.s), capability di misura (§9.8), tabella delle operazioni allineata ad ADR-16.b §b.2; rendere costruibile la coppia di T-M3A-TYP-12 (rilievo LOW aperto della review di ADR-03.b) |
| Comando | `grep`/`sed -n` sul piano e sui contratti; lettura di `src/core/include/astro/operations.hpp` e `src/core/operations.cpp` per i nomi della tabella; `grep` su `/usr/include/x86_64-linux-gnu/tiff.h`; `python3 -S tools/evidence.py docs/evidence/M3-entry-ADR03b-bis.md` |
| Input | Piano attivo (SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`): §7.3 (righe 686-708), §7.13 (righe 845-856), §8.6.2 (righe 1044-1050), §9.8 (righe 1302-1312), §10.11 C-10 (righe 1534-1548), §18.1-18.3 (righe 2503-2513), NFR-EXP-01 (riga 2001), M3.s (riga 2701), testo ADR-03 (riga 2564), appendice D righe 3387-3389, 3439-3441, 4044-4045, 4081, 4123-4126. Contratti: ADR-03 (parte `.a`, R1, §b.1–b.11), C-02 (fase dei master, righe 30, 225, 229). ADR-16.b in scrittura: `.claude/worktrees/agent-ae93e7a97ecbe3a23/docs/contracts/ADR-16-stati-master.md`, SHA-256 `320218d1dc2c689c77afd0141e990e937b066395886dd3f4402d9a9097a5681a`, §b.1–b.10, sola lettura. Review precedente: `docs/evidence/M3-entry-ADR03b.md` |
| Versioni | libtiff 4.7.0 (`libtiff-dev` 4.7.0-3ubuntu5, header installati); worktree da `cb35d89` |
| Ambiente | Linux 7.0.0-38-generic x86_64, macchina di sviluppo |
| Risultato | §b.12–b.18 in `docs/contracts/ADR-03-pixel-maschere.md`; 6 ID di test pianificati `T-M3A-TYP-13…18` in `docs/test-matrix.md`; riga di T-M3A-TYP-12 aggiornata; righe in `docs/contracts/README.md` e `docs/decisions/README.md` |
| Output | File elencati nel blocco generato sotto; artefatti di review in `.omc/artifacts/ask/` |
| Ambito | Contratto dei tipi. Fuori: codice, test, codifica TIFF (C-10.b), insiemi accettati dalle misure (contratti delle misure), tile e piramide |
| Esito | Review PASS al giro 3 (Codex `gpt-6-astra`); vedi «Review» |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Decisioni

- Alpha è un tipo separato (`alpha_plane`), non un campo di `image`, non associato (colore mai premoltiplicato), prodotto solo da `derive_alpha` con una regola dichiarata fra `validity`, `coverage`, `edit_mask`; ogni regola dà 0 sui pixel invalidi. Nessuna regola di default. Non si scrive in FITS.
- Capability di misura: elenco chiuso di codici di limitazione, non un booleano. I codici di storia si memorizzano in `image.measure` e si uniscono lungo il DAG; quelli ricavabili dai campi (linearità, varianza) si calcolano e non si memorizzano; campioni interpolati e varianza approssimata sono memorizzati, perché `demosaic` sparisce dopo resampling e convoluzione. L'idoneità si decide per misura con l'insieme accettato dal contratto della misura; lo stop è `measure.limit_blocked`. Persistenza FITS con `ASTRMCAP` e `ASTRML01…99`.
- Calibrazione `.b`: codici dalle voci `omitted` del ledger, dallo stato di `light_offset` e dalle ipotesi della ricetta (H-D', limite termico, nero del decoder).
- Tabella delle operazioni: `calibrate` resta per la ricetta `.a`; tre righe nuove per la `.b` (light, master `raw`, master preparati), assegnate per ruolo e stato secondo ADR-16.b §b.2.
- T-M3A-TYP-12: `equivalent_content` ignora tutto `decoder_state.source`; la coppia del test differisce in `source.keyword`.

## Limiti

- ADR-16.b non è ancora nel repository: i riferimenti valgono per la versione con lo SHA-256 indicato.
- TIFF 6.0 non riletto (nessun accesso alla rete): vale il fatto dell'header di libtiff.
- Il «master di qualità dubbia» di §9.8 non ha un codice; gli insiemi accettati dalle misure non sono scritti.
- `history.unknown` si basa sulla fase dichiarata.

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-adr03c-round1-20261030-0348.md` | `tmp/adr03c-round1.diff` | FAIL | 4 MED: `samples.interpolated` e `variance.fixed_weight_approximation` derivati da `demosaic` si perdevano dopo resampling; combinazione «tutte insieme» di T-M3A-TYP-16 non costruibile; `require_measure_ready` senza caso positivo; T-M3A-TYP-17 senza ingressi con codici già memorizzati |
| 2 | `.omc/artifacts/ask/codex-adr03c-round2-20261030-0354.md` | `tmp/adr03c-round2.diff` | FAIL | Rilievi del giro 1 chiusi; 1 MED: T-M3A-TYP-16 non verificava l'obbligo di `variance.fixed_weight_approximation` nell'invariante d'uscita |
| 3 | `.omc/artifacts/ask/codex-adr03c-round3-20261030-0355.md` | `tmp/adr03c-round3.diff` | PASS | Rilievo del giro 2 chiuso; nessun rilievo |

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-03-pixel-maschere.md` | `ebd000bd2f7ce1ca59fb85ff7cf7494d7379253495257fdd9955d9d440053ccb` |
| `docs/contracts/README.md` | `496c611387d092c9c6e88a02df05b01a0edf2a1c79a85d4f83040443c0df0c53` |
| `docs/decisions/README.md` | `867ee555140e2581eb9754fdabc9d14d31aebc53661a5379b8b8f0ad4f85b961` |
| `docs/test-matrix.md` | `ae4950eb8715bec20553b0c2c8ae38701cfa5fac4e60f3f154267a5c856714c5` |

<!-- evidence:end -->
