# M3.entry, C-02.b: master completi

Stato: **APPROVATO** (review `c02b-round2` PASS). Unità: M3.entry, contratto C-02.b per la slice M3.b (riga M3.b di `plans/M3.md`). Solo testo: nessun codice di prodotto, nessun test eseguito.

| Campo | Valore |
|---|---|
| Unità | M3.entry / C-02.b |
| Scopo | Fissare prima del codice l'estensione `.b` dei master: metodi oltre `mean` + `none` con il piano di varianza di ciascuno, analisi del set candidato con esclusioni e gruppi (anche da RAW), proposta a priori secondo `n`, flat CFA per canale, report, test e soglie |
| Comando | `python3 -S tools/trace.py ids --stage M3.b`; `python3 -S tmp/c02b_algebra.py`; `python3 -S tools/evidence.py docs/evidence/M3-entry-C02b.md` |
| Input | Piano attivo (SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`): §10.1 (righe 1335-1352), §10.3 C-02 (1371-1385), F-CAL-01 (1840), §21.5 M3.b (2684), appendice D righe 3178-3180, 3429-3431, 3718-3720, 3727, 4070, 4071, 4113-4115, 4238. `plans/M3.md` (riga M3.b, questioni aperte). Contratti C-02.a, ADR-16.a/b, ADR-17.a/b, ADR-03 (R1, §b.6, §b.13, §b.14), C-01.b §2; D-16, D-17.b; `src/core/master.cpp` (`require_phase`, `require_linear`, `master_descriptor_from_node`), `src/core/operations.cpp`, `tests/unit/test_master.cpp` righe 287 e 291 |
| Versioni | Worktree da `f3b7909` |
| Ambiente | Linux 7.0.0-38-generic x86_64, macchina di sviluppo |
| Risultato | Sezione «Estensione C-02.b» in `docs/contracts/C-02-master.md` (§b.1–b.15); motivo `pedestal` in ADR-16.b §b.3 e caso in T-M3A-CAL-01; C-01.b §2 e limiti allineati; 16 ID pianificati `T-M3B-C02-01…16` in `docs/test-matrix.md`; questioni chiuse rimosse da `plans/M3.md`, una aperta aggiunta; riga C-02 in `docs/contracts/README.md` |
| Output | File elencati nel blocco generato sotto; artefatti di review in `.omc/artifacts/ask/` |
| Ambito | Contratto. Fuori: codice (M3.b), normalizzazione dei flat e metodi robusti sui flat, `Q` e master `bias_subtracted` del prodotto, pesi stimati dai dati, ADR-09.b e D-09.b |
| Esito | Review PASS al giro 2. Vedi «Review» |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Decisioni

- Metodi: `single` (`n = 1`), `mean`, `weighted_mean` con pesi dichiarati (`n ≥ 3`, `max a_f < 1/2`), `median` e `winsorized` (`n` fino a 1024), `mean` + `sigma_clip` (iterativo, non inerte secondo Samuelson). Sul flat solo `single` e `mean`.
- Piani di varianza: `noise_model` per `single`; `empirical` per `mean`; `empirical_weighted` per la media pesata, non distorto con varianze qualunque (coefficienti `c_f` derivati); `gaussian_reference` `κ s²` per mediana e winsorized, non distorto sotto `iid_gaussian`, con `κ` accurato a 10⁻⁹; `frozen_selection` per `sigma_clip`, approssimazione con `variance.fixed_weight_approximation` (ADR-17.b §b.1).
- Analisi prima della scelta: esclusioni per frame da una lista chiusa, gruppi per chiave esatta e per tolleranza rispetto al primo frame canonico (coerente col preflight), un nodo per gruppo, nessuna esclusione nell'esecuzione. Frame RAW: chiave con i campi strutturali del decoder, nero entro `decoder_black_abs_tol_adu`.
- Proposta a priori: `n = 1` `single` (solo con modello di rumore dichiarato), `n = 2` `mean`, `n ≥ 3` `median` (flat: `mean`), oltre 1024 `mean`.
- Flat CFA: livelli e tolleranza per canale.
- Determinismo: ordine canonico, ordine totale (valore, indice canonico) per le statistiche d'ordine, somme del report per riga.

## Questioni aperte

Chiuse da questo contratto:

- codice di stop per un ingresso del master in fase o linearità non ammessa: vale la tabella di ADR-03 (`incompatible`, `op.phase_blocked`, `op.linearity_blocked`); in M3.b cambiano `require_phase` e `require_linear` di `src/core/master.cpp` e le attese di `tests/unit/test_master.cpp` (C-02.b §b.13 punto 1);
- master con `pedestal_adu ≠ 0` nell'analisi `.b`: motivo `pedestal` in ADR-16.b §b.3.

Aperte, per il coordinatore:

- normalizzazione dei flat e metodi robusti sui flat (aggiunta alle questioni di `plans/M3.md`);
- ADR-09.b e D-09.b (`decided_by` di C-02.b).

## Verifiche dell'autore

- `python3 -S tmp/c02b_algebra.py` (SHA-256 `fb332ce4bb439fc26e19285962398b502313d393302ba7512bf8663b66bf0b60`). Il controllo lavora in aritmetica razionale su 3000 casi con seed. Verifica che lo stimatore pesato abbia valore atteso `Σ a_f² σ_f²` con varianze diverse, che `c_f > 0` nel dominio e che con pesi uguali dia `s²/n`. Verifica il controesempio fuori dominio (pesi `(3, 1, 1)`), la disuguaglianza di Samuelson con il caso d'uguaglianza e il primo `n = 11` a `κ = 3`. Per quadratura controlla `κ_med(3) = 1 − √3/π` e la scomposizione condizionata della winsorized contro un Monte Carlo per (5, 1), (7, 2), (9, 1). Esito: `verifica algebrica C-02.b: ok`. È una verifica delle derivazioni, non un test del prodotto.

## Limiti

- Piano dei metodi robusti non distorto solo sotto `iid_gaussian` e `continuous_samples`; scostamento da quantizzazione misurato, non corretto.
- Errore della selezione congelata non derivato.
- Nessun codice di capability per `gaussian_reference` e `noise_model`.
- Flat senza normalizzazione e senza metodi robusti.
- Nessun test eseguito.

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-c02b-round1-20261030-0637.md` | `tmp/c02b-round1.diff` | FAIL | 6 MED: statistiche di `.a` (`variance_cv_gaussian_homoscedastic`, `level_bias_estimate`) non definite per i nuovi metodi e per il flat `single`; soglia di saturazione per frame non gestita dall'analisi; proposta `mean` per un flat `flat_level` con `n = 2`; tolleranza della regola I senza l'errore di `κ` e zero richiesto su casi dyadic non esatti; errore dell'oracle di `κ` attribuito al solo dimezzamento del passo; intervallo di Chebyshev non derivato per il rapporto di T-M3B-C02-06. 1 LOW: «eccesso» da quantizzazione con segno variabile |
| 2 | `.omc/artifacts/ask/codex-c02b-round2-20261030-0643.md` | `tmp/c02b-round2.diff` | PASS | Rilievi del giro 1 corretti. 1 LOW: frase malformata sulla regola I, corretta dopo il PASS (una parola). Fuori delta: C-02.a, «Fuori da `.a`» (`median`), afferma in generale che la quantizzazione aumenta la varianza della mediana, mentre §b.3 ammette entrambi i segni: lavoro per un'unità successiva, perché tocca il testo `.a` |

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-16-stati-master.md` | `e443567e3fbb04693c761fd3dd7243a96a20806a49aa76cb42d63c26a43479fc` |
| `docs/contracts/C-01-calibrazione-light.md` | `5381f37ca6f41da7401c2f68c83bc82ed5d60ad2a32b8242283614b8c9ec8013` |
| `docs/contracts/C-02-master.md` | `7f9c0d34d41da5c97a6a587fa46e9675d7f47fe87ac042671c7bf73bcdd15c4e` |
| `docs/contracts/README.md` | `496c611387d092c9c6e88a02df05b01a0edf2a1c79a85d4f83040443c0df0c53` |
| `docs/test-matrix.md` | `3606bd7cf50005153f73ec040c41ae064e9b310a31d3344d6af11bd7d501e597` |
| `plans/M3.md` | `6539466708cc87d7a821794dcc1fc311bc3d796ce479ee7e2df6922e4548b18d` |

<!-- evidence:end -->
