# Stato del progetto astroProcessor

Ultimo aggiornamento: 29 ottobre 2026. Unica fonte dello stato corrente; risultati e limiti nei record di evidenza.

## Ora

- **M0 PASS. M1 DONE. M2 IN PROGRESS** (`plans/M2.md`): M2.g, M2.f, M2.a, M2.b, M2.c, M2.d, M2.e DONE (`docs/evidence/M2-{g,f,a,b,c,d,e}.md`). Core: errori, immagine, FITS da memoria, hash, JSON canonico, nodi DAG, slot, provenance, store SQLite durevole, snapshot undo/redo, export con manifest, budget di memoria, pool, scheduler con cancel, checkpoint/resume e spill, calibrazione FITS `dark_matched_flat_dark_flat` (L−D, Q, maschere, varianza, report; dato reale PUB-LFC-01); 313 test dev (ASan/UBSan, fast), 315 release (312 fast + 3 slow: T-M2D-13, T-M2E-13, T-M2E-21/26).
- Stack adottato in `docs/decisions/ADR-M1-stack.md`, review indipendente PASS in `docs/evidence/M1-closure-001-review.md`: C++20, Qt 6 Widgets (provvisoria fino al gate M4.a), SQLite, SHA-256 via OpenSSL, FFTW, CPU di riferimento e nessuna GPU fino a M7.
- Licenza (D-02): il proprietario ammette la GPL per il prodotto (28 ottobre 2026); `LICENSE` da emettere con ADR-12.
- Procedura snellita (`AGENTS.md`, sezione "Ritmo di lavoro"): un record per unità, errori corretti nello stesso turno, harness minimi; gate scientifici e divieto di fallback nel prodotto invariati.
- D-19/D-20 approvate P0. D-21: preferenza per pack inclusi, subordinata alle verifiche di licenza.
- Repository su `work/m0-m1`, origin ufficiale. Ultimo commit pubblicato: `1dd08e4`.
- Codex e Gemini 4 Argon non disponibili; Gemini Flash via `agy` solo per testi e lingue (`AGENTS.md`).

## Evidenze principali

| Unità | Record | Esito |
|---|---|---|
| M0 | `docs/evidence/M0-plan-review.md`, `M0-environment.md`, `M0-closure.md` | PASS |
| M1.a | `docs/evidence/M1-a.md`, `M1-a-review.md` | Numerica corretta per C++, Rust, NumPy; prestazioni miste |
| M1.b | `docs/evidence/M1-closure-001.md` | Viewer Qt Widgets: primo frame identico all'oracle su Wayland e XWayland, dpr 1 e 2 |
| M1.c | `docs/evidence/M1-c-validation.md`, `M1-c-measurement.md` | Crash test SQLite 44/44, JSON 46/46; SQLite 1,09 volte più lento |
| M1.d | `docs/evidence/M1-closure-001.md`, `M2-d.md` | Spill a budget fisso esatto, guasti con exit 2; protocollo completo chiuso in M2.d (T-M2D-13) |
| M1.e | `docs/evidence/M1-e-*.md` | Vulkan corretto; prestazioni sul microcaso non rappresentative |
| M1.f | `docs/evidence/M1-closure-001.md` | SHA-256 circa 2,1 GB/s in memoria, 1,27 GB/s da file |

I record M1 precedenti (pocketfft, supervisori, ritiro Rust, contratti 16M) restano come storico; i loro rami sono chiusi dall'ADR e non richiedono altre azioni.

## Prossima unità

Gate M2 (`plans/M2.md`): verifica complessiva della milestone. Slice FITS con L−D, Q, maschere, metadati, incertezza e report; DAG durevole, recovery, cancel e worker; dato reale. **Da non avviare finché il proprietario non riprende.** M2.e chiusa con `docs/evidence/M2-e.md` (T-M2E-01…32, T-FBK-01 parte M2 e T-M2F-02 ESEGUITI).

Ritmo misurato di M2 (solo informazione): avviata il 28 ottobre 2026; M2.g, M2.f, M2.a e M2.b chiuse il 28 ottobre; M2.c, M2.d e M2.e il 29 ottobre (date dei commit e dei record).

## Limiti aperti

- M2.e: checkpoint/resume non collegato alla calibrazione (`checkpoint_every` 0); master non combinati dal prodotto (le medie sul dato reale le costruisce il test); overscan non ritagliato (`L − D` ≈ −73 ADU registrato senza soglia); input letti due volte (riverifica del digest dopo il preflight); alcune guardie protette solo insieme (mutazione su T_M2E_29, quoziente f64 non isolabile); stima di N non valida su flat vignettato (rapporto 8,33, nessun gate, ADR-17 `iid_gaussian_fc`); T-M2E-13 e T-M2E-21/26 non eseguiti sotto ASan/UBSan.
- Gate M4.a: latenza pan/zoom, accessibilità, IME, colore ICC, confronto con QML.
- M2.d: soglie F-RES-01 (0.50/0.25/10.0) invariate e non confermate da carichi PSI reali; costo per task (alloc/free di 3 buffer da 2 MiB) con 1 worker lento e variabile (60-102 s per 40 GiB), non indagato; cancel cooperativo; resume riesegue i task dopo l'ultimo checkpoint; page cache dello spill fuori dal budget (misura per cgroup assente); TSan non nei preset (eseguito a mano durante lo sviluppo); un `pwrite` fallito lascia l'estensione addebitata fino alla distruzione dell'area; `ctest --preset release` esegue anche lo slow (circa 107 s).
- M2.c: durabilità contro perdita di corrente non provata (fuori dal modello di minaccia); processo malevolo con lo stesso UID fuori dal modello; `readdir` in `read_directory` senza test di iniezione.
- Vincolo: prima di qualsiasi lavoro sulla GUI ci si ferma col proprietario (`AGENTS.md`).
- Nessun dataset CFA reale ridistribuibile trovato: debayer M3.d senza dato reale per ora.
- Cataloghi: Gaia e ESA Tycho2 solo uso non commerciale; APASS senza grant di redistribuzione dimostrato. Nessun pack adottato.
- M10 non autorizzato. CodeGraph/Graphify: indici assenti, non creati.
