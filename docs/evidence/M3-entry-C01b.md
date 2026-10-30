# M3.entry, C-01.b: calibrazione del light, forma completa e G-OFFSET

Stato: **review PASS al giro 2 di 3** (nessun rilievo aperto). Unità: M3.entry, riga «C-01.b…C-19» di `plans/M3.md`, parte C-01.b (prima fetta usata da M3.a per G-OFFSET). Solo testo: nessun codice, nessun test eseguito.

| Campo | Valore |
|---|---|
| Unità | M3.entry / C-01.b |
| Scopo | Fissare prima del codice il contratto `.b` della calibrazione del light (template §10.1 a undici campi), l'uso del nero del decoder come offset del light (G-OFFSET), la propagazione della coverage e il dominio del light rispetto ai master |
| Comando | `python3 -S tools/trace.py ids --stage M3.entry`; `python3 -S tools/trace.py ids --stage M3.a`; `python3 -S tools/evidence.py docs/evidence/M3-entry-C01b.md` |
| Input | Piano attivo (SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`): §9.3 (righe 1227-1242), §9.6 (1276-1286), §9.8 (1302-1312), §9.9 (1314-1331), §10.1 (1335-1351), §10.2 C-01 (1355-1369), §10.15 C-14 (1604-1622), F-IO-04 (1815), F-CAL-02 e F-CAL-03 (1841, 1842); appendice D righe 3426-3428, 3560, 3590, 3730, 4068-4069, 4110-4112, 4372, 4396. `plans/M3.md` (riga M3.entry, slice a e c, questioni aperte). Contratti ADR-16.a/.b, D-16, ADR-03.b §b.2, §b.5, §b.13, §b.14, ADR-08.b §2.2-§2.9, ADR-17.a, C-02.a; `src/core/calibration.cpp` (codici `calib.unit`, `calib.plane_layout`), `src/core/fits_header.cpp` (keyword `OFFSET`) |
| Versioni | Worktree da `e55c512` |
| Ambiente | Linux 7.0.0-38-generic x86_64, macchina di sviluppo |
| Risultato | Contratto `docs/contracts/C-01-calibrazione-light.md` (undici campi, 11 ID pianificati `T-M3A-C01-01…11`); ADR-16.b §b.2 allineato (light fuori dominio: stop; master: `excluded`) e motivo `coverage` nella lista chiusa di §b.3; nota di dipendenza di ADR-03.b §b.12 e §b.18 aggiornata; righe in `docs/test-matrix.md` e `docs/contracts/README.md`; questioni aperte di `plans/M3.md` aggiornate |
| Output | File elencati nel blocco generato sotto; artefatti di review in `.omc/artifacts/ask/` |
| Ambito | Contratto. Fuori: codice, test, dark scalato (F-CAL-05), bad pixel map (F-CAL-04), regione di normalizzazione (F-CAL-03.b), covarianze (ADR-17.b) |
| Esito | Review PASS al giro 2 (Codex `gpt-6-astra`); vedi «Review» |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Decisioni

- Nero del decoder (G-OFFSET): il prodotto non lo sottrae mai. È sentinella della compatibilità fra ingressi RAW (ADR-16.b §b.7), voce `light_offset` del ledger e residuo dichiarato `b_L/Q` del percorso parziale. «Offset noto» significa dichiarato dal decoder e non sottratto, non misurato; un FITS con `OFFSET` resta `unknown`. Sottrarlo richiederebbe una misura che lo confronti con l'offset dello stesso light e una revisione di ADR-16.b, D-16 e ADR-03.b (alternativa B di §4, scartata per P0).
- Coverage: la calibrazione opera sulla griglia del sensore e non produce coverage; la validità passa nei bit di invalidità, da cui gli operatori geometrici ricavano la coverage. Un ingresso con coverage è fuori dominio (light: stop `calib.coverage_input`; master: `excluded` con motivo `coverage`), anche per la ricetta `.a`.
- Ingresso fuori dalla propria riga: il light ferma l'analisi (`op.phase_blocked`, `op.linearity_blocked`, `calib.unit`, `calib.plane_layout`, in quest'ordine), un master è `excluded`. ADR-16.b §b.2 allineato con una modifica minima.
- Metriche del §9.9 con soglie fissate prima della misura: sensibilità all'offset esatta (0 o 1), invarianza al nero dichiarato bit per bit, varianza esatta sui dyadic, insieme dei pixel invalidi esatto, flat residuo entro il bound di ADR-16.b §b.12.

## Limiti

- Residuo del percorso parziale dichiarato, non misurato (ipotesi H-BLK), senza componente termica.
- Unità `electron` e light RGB fuori dal dominio.
- Varianza `.b` senza bound non dyadic fino ad ADR-17.b.
- ADR-09.b non scritto; C-14 e C-16 devono dichiarare l'accettazione di `calib.offset.known_not_removed`.
- Master con `pedestal_adu ≠ 0` nell'analisi `.b`: questione aperta in `plans/M3.md`.
- Riferimenti della letteratura non riletti in questa unità.
- I blocchi generati di `M2-gate.md`, `M2-R1.md`, `M2-R2.md` contengono `ADR-16-stati-master.md` o `test-matrix.md` e richiedono i junit della build release (`tmp/dev-junit.xml`, `tmp/release-junit.xml`), assenti in questo worktree: vanno rigenerati dal coordinatore.

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-c01b-round1-20261030-0423.md` | `tmp/c01b-round1.diff` | FAIL | 5 MED: soglia del flat residuo in ADU invece che adimensionale; estremi del residuo dichiarato confrontati con un incremento per pixel; canale senza pixel validi non definito; T-M3A-C01-01 con FITS `unknown` anche dopo le catene che tolgono l'offset; T-M3A-C01-02 che rifiutava `flat.none`. 2 LOW: fixture negative di T-M3A-C01-06 non specificate; `support_incomplete` e bit di `Q` ambigui |
| 2 | `.omc/artifacts/ask/codex-c01b-round2-20261030-0428.md` | `tmp/c01b-round2.diff` | PASS | Rilievi del giro 1 chiusi; nessun rilievo nuovo |

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-03-pixel-maschere.md` | `ebd000bd2f7ce1ca59fb85ff7cf7494d7379253495257fdd9955d9d440053ccb` |
| `docs/contracts/ADR-16-stati-master.md` | `e443567e3fbb04693c761fd3dd7243a96a20806a49aa76cb42d63c26a43479fc` |
| `docs/contracts/C-01-calibrazione-light.md` | `063e14eacdbf25c92b31cf78c4bb6e19934d39fada23a06ba0c24d7fe8212162` |
| `docs/contracts/README.md` | `603cd99cb80049be168c75247bab3a392c6b597095ecf6a79675a7a1cb01173c` |
| `docs/test-matrix.md` | `b8393944660adc5c48dd20a9d58f96e9f33c7b2725cdce58012ede4f06e30b9a` |
| `plans/M3.md` | `e091bb75a8655635046cffda62dcd3b54005c69dfdbe65f9d99f7f03090f6757` |

<!-- evidence:end -->
