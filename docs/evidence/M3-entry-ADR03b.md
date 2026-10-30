# M3.entry, ADR-03.b: tipi estesi del modello pixel e maschere

Stato: **review PASS al giro 2 di 3** (un rilievo LOW aperto). Unità: M3.entry, riga «ADR-03.b» di `plans/M3.md`. Solo testo: nessun codice, nessun test eseguito.

| Campo | Valore |
|---|---|
| Unità | M3.entry / ADR-03.b |
| Scopo | Fissare prima del codice i tipi estesi: origine `decoder` dei metadati, stato del decoder RAW, RGB da debayer con incertezza distinta fra campione misurato e interpolato, portata del piano di varianza, coverage, maschere fotografiche distinte da invalidità e coverage |
| Comando | `python3 -S tools/trace.py ids --stage M3.entry`; `codegraph_explore` su `src/core/include/astro/image.hpp` e `src/core/image.cpp`; `grep` sugli header di LibRaw installati; `python3 -S tools/evidence.py docs/evidence/M3-entry-ADR03b.md` |
| Input | Piano attivo (SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`): §7.14 (righe 866-877), §8.4 (righe 952-974), §8.5 (righe 976-988), §8.6.2 (righe 1044-1052), §8.7 (righe 1054-1067), §9.6 (righe 1276-1286), §9.8 (righe 1302-1312), §10.4 C-03 (righe 1387-1400), §10.12 C-11 (righe 1550-1566), F-IO-02 e F-CAL-08 (righe 1813, 1847), testo ADR-03 (riga 2564), appendice D righe 3387-3389, 3686, 4045, 4081, 4126. `plans/M3.md` (tabella M3.entry, righe M3.a, M3.d, M3.g, M3.j). Contratti ADR-03 (parte `.a` e R1), ADR-08.b (§2.3, §2.4, §2.6, §2.9), ADR-17.a. `/usr/include/libraw/libraw_types.h` righe 751-754, 770, 854; `/usr/include/libraw/libraw_const.h` riga 72 |
| Versioni | LibRaw 0.21.5 (header installati); worktree da `9f1270a` |
| Ambiente | Linux 7.0.0-38-generic x86_64, macchina di sviluppo |
| Risultato | Sezione «Estensione ADR-03.b» in `docs/contracts/ADR-03-pixel-maschere.md` (§b.1–b.11); 12 ID di test pianificati `T-M3A-TYP-01…12` in `docs/test-matrix.md`; righe aggiornate in `docs/contracts/README.md` e `docs/decisions/README.md` |
| Output | File elencati nel blocco generato sotto; artefatti di review in `.omc/artifacts/ask/` |
| Ambito | Contratto dei tipi. Fuori: codice, test, modelli di covarianza (ADR-17.b), metodo di debayer (C-03), generazione e persistenza delle maschere (C-11, ADR-04.b), ledger dell'offset (C-01.b, ADR-16.b) |
| Esito | Review PASS al giro 2 (Codex `gpt-6-astra`); vedi «Review» |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Decisioni

- `metadata_origin` con il valore `decoder` (vincolo di ADR-08.b §2.6); conflitto con `declared` come per `header`; l'origine resta provenienza, la rilettura FITS dà `header`.
- `decoder_state` nell'immagine con la rappresentazione di LibRaw di nero, bianco, linearizzazione, finestra visibile e indici di colore della cella 2×2; valido solo con fase `raw` e layout `cfa`; non scrivibile in FITS (stop `fits.decoder_state`), si ricostruisce dall'originale.
- RGB da debayer: piani R, G, B; `demosaic_info` con CFA d'origine, metodo, `measured_preserved` e modo di propagazione della varianza; la classe `measured`/`interpolated` di ogni campione si ricava dai campi, senza un piano aggiuntivo; keyword FITS `ASTRD*`.
- Varianza dei campioni interpolati: identità della varianza di una combinazione lineare (GUM, JCGM 100:2008 §5.1.2, §5.2.2), esatta per pesi fissi sotto `diagonal_independent` di ADR-17.a; pesi dipendenti dai dati dichiarati come approssimazione da misurare in C-03.
- `variance_scope` (`independent`, `marginal`): il piano di un RGB da debayer è marginale; un operatore che assume l'indipendenza si ferma su un ingresso marginale; covarianze ad ADR-17.b.
- Coverage come piano geometrico in `[0, 1]`, definita anche sui pixel invalidi, HDU `COVERAGE`; bit 9 `support_incomplete`.
- Maschera fotografica = `edit_mask` di ADR-03.a, un piano; tipi distinti senza conversioni; `mask_info` per tipo, revisione, rasterizzatore e sorgenti.

## Limiti

- Ordine R, G, B non verificabile su RGB esterni; `capture_time_libraw` senza fuso orario.
- `decoder_state` non persistito in FITS; per i master da RAW la decisione è di ADR-16.b.
- Varianze marginali senza covarianze fino ad ADR-17.b; errore di `fixed_weight_approximation` non misurato.
- Riferimenti GUM e Malvar-He-Cutler non riletti in questa unità (nessun accesso alla rete).
- Alpha, capability, tile e piramide fuori da `.b`.

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-adr03b-round1-20261030-0301.md` | `tmp/adr03b-round1.diff` | FAIL | 4 MED, 1 LOW: indici di colore LibRaw dei verdi non conservati in `decoder_state`; valore di `variance_scope` non verificato; `equivalent_content` sui campi nuovi senza prova discriminante; assenza dell'API di `edit_mask` attribuita a un test che non la verifica; correlazione fra canali affermata in generale |
| 2 | `.omc/artifacts/ask/codex-adr03b-round2-20261030-0307.md` | `tmp/adr03b-round2.diff` | PASS | Rilievi del giro 1 chiusi; 1 LOW aperto: T-M3A-TYP-12 non dice come costruire la coppia «solo origine diversa», dato che §b.2 ammette solo `source.origin = decoder` (da chiarire nell'unità che scrive il test) |

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-03-pixel-maschere.md` | `ebd000bd2f7ce1ca59fb85ff7cf7494d7379253495257fdd9955d9d440053ccb` |
| `docs/contracts/README.md` | `353e714178f05198679290fc445edac14ca3aa881f4f1d4e91c44d7c8df00b7c` |
| `docs/decisions/README.md` | `860b4a0b676f845260ed4a326d228c44b1b89ac58910261949fb640d354cad9b` |
| `docs/test-matrix.md` | `e4b5c8d30854c61d040e84dc2f61a5961bbf3092572780d8782ec4bcb1393d39` |

<!-- evidence:end -->
