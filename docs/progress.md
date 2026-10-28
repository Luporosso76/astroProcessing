# Stato del progetto astroProcessor

Ultimo aggiornamento: 28 ottobre 2026. Unica fonte dello stato corrente; risultati e limiti nei record di evidenza.

## Ora

- **M0 PASS. M1 DONE. M2 IN PROGRESS** (`plans/M2.md`): M2.g, M2.f, M2.a, M2.b DONE (`docs/evidence/M2-{g,f,a,b}.md`). Core: errori, immagine, FITS da memoria, hash, JSON canonico, nodi DAG, slot e provenance; 122 test con ASan/UBSan.
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
| M1.d | `docs/evidence/M1-closure-001.md` | PARZIALE: spill a budget fisso esatto, guasti con exit 2; protocollo completo al gate M2.d |
| M1.e | `docs/evidence/M1-e-*.md` | Vulkan corretto; prestazioni sul microcaso non rappresentative |
| M1.f | `docs/evidence/M1-closure-001.md` | SHA-256 circa 2,1 GB/s in memoria, 1,27 GB/s da file |

I record M1 precedenti (pocketfft, supervisori, ritiro Rust, contratti 16M) restano come storico; i loro rami sono chiusi dall'ADR e non richiedono altre azioni.

## Prossima unità

M2.c: store SQLite durevole dietro l'interfaccia `dag_store`, scrittura atomica degli artefatti, recovery, migrazioni, snapshot e undo/redo, export con manifest (`docs/contracts/ADR-04-persistenza.md`, T-M2C-01…25; chiude anche T-M2B-11). Poi M2.d e M2.e.

## Limiti aperti

- Gate M4.a: latenza pan/zoom, accessibilità, IME, colore ICC, confronto con QML.
- Gate M2.d: protocollo M1.d completo (40 GiB logici, spill fisico, page cache contabilizzata).
- Gate M2.c: migrazione, backup e schema ignoto in SQLite.
- Nessun dataset CFA reale ridistribuibile trovato: debayer M3.d senza dato reale per ora.
- Cataloghi: Gaia e ESA Tycho2 solo uso non commerciale; APASS senza grant di redistribuzione dimostrato. Nessun pack adottato.
- M10 non autorizzato. CodeGraph/Graphify: indici assenti, non creati.
