# Registro ADR

Registro compatto delle 17 decisioni architetturali del piano. Il testo normativo, le opzioni e le evidenze richieste stanno in `ASTRO_POSTPROCESSING_PLAN_v2.2.1.md` §20; qui c'è solo lo stato. Stack scelto in [ADR-M1-stack](ADR-M1-stack.md). Lo stato di avanzamento è in `docs/progress.md`.

## Stato

Gli stadi `.a` di ADR-01…09, 13, 16 e 17, C-01.a, C-02.a, C-10.a e D-03, D-04, D-05, D-09.a, D-17.a sono **APPROVATO (in review)** nel verbale [M2-entry](M2-entry.md) del 29 ottobre 2026: l'approvazione è confermata dalla review indipendente (D24), `VERIFICATO` si decide al gate M2. Le estensioni (`.b`, e le estensioni di R1…R5 di `plans/M2-residui.md`) si approvano prima del codice della loro unità. Le soluzioni implementative restano **proposte** fino ai rispettivi gate. La [politica d'ingresso M0](M0-policy.md) prepara licenze e dataset per gli spike; non sceglie la licenza del prodotto né chiude i gate successivi. La decisione tecnica è delegata agli agenti, ma richiede le misure e le verifiche elencate: la delega non sostituisce una prova.

Le decisioni D-19, D-20 e D-21 non aggiungono ADR. Si riflettono in ADR-09 (deconvoluzione), ADR-10 e ADR-13 (colore, WCS, cataloghi offline) e ADR-12 (pack come decisione separata dalla licenza del prodotto).

## Voci

| ID | Tema | Gate | Stato |
|---|---|---|---|
| ADR-01 | Linguaggio e componenti del core | M1 | **Deciso**: C++20, un solo linguaggio ([ADR-M1-stack](ADR-M1-stack.md)); APPROVATO (in review), [M2-entry](M2-entry.md) |
| ADR-02 | GUI e comunicazione con il core | M1, conferma M4.a | **Provvisorio**: Qt 6 Widgets, confronto con QML al gate M4.a ([ADR-M1-stack](ADR-M1-stack.md)); core headless APPROVATO (in review), [M2-entry](M2-entry.md) |
| ADR-03 | Modello pixel, precisione e maschere | M2 ingresso; estensioni M3 | `.a` APPROVATO (in review), [M2-entry](M2-entry.md); proposto per le estensioni: immagine tipizzata, f32 candidato, maschera distinta da invalidità e alpha |
| ADR-04 | Persistenza del progetto | M2 ingresso, verifica M2.c | **Deciso**: SQLite autorità, JSON per manifest; migrazione e recovery al gate M2.c; `.a` APPROVATO (in review), [M2-entry](M2-entry.md) |
| ADR-05 | DAG, cache e invalidazione | M2 ingresso | `.a` APPROVATO (in review), [M2-entry](M2-entry.md); proposto per le estensioni: DAG immutabile, hash di contenuto, token di generazione |
| ADR-06 | RAM, scheduler e spill | M2 ingresso, verifica M2.d | **Vincoli fissati**: prenotazioni, spill dichiarato nel preflight, stop senza riduzione; protocollo completo al gate M2.d; `.a` APPROVATO (in review), [M2-entry](M2-entry.md) |
| ADR-07 | CPU, SIMD e GPU | M1, M7 | **`.a` deciso**: CPU di riferimento, SIMD nel preflight, nessuna GPU fino a M7; contratto [ADR-07-cpu-simd](../contracts/ADR-07-cpu-simd.md), APPROVATO (in review), [M2-entry](M2-entry.md) |
| ADR-08 | Import RAW, FITS e XISF | M2 ingresso; estensioni M3 | `.a` (FITS) APPROVATO (in review), [M2-entry](M2-entry.md); `.b` (RAW e cataloghi): contratto [ADR-08-decoder-pack](../contracts/ADR-08-decoder-pack.md), review in `docs/evidence/M3-entry-ADR08b.md`; XISF fuori da P0 (F-IO-06, M6) |
| ADR-09 | Calibrazione, registrazione, stacking e deconvoluzione P0 | Minimo M2 ingresso; operatori M3 ingresso | `.a` (calibrazione minima) APPROVATO (in review), [M2-entry](M2-entry.md); proposto per gli operatori: casi delle sezioni 8 e 9, ground truth, una sola formulazione P0 |
| ADR-10 | Colore, WCS e maschere | M3, M4 | Proposto: separare lineare e display, propagare maschere e covarianza, WCS non certifica il colore |
| ADR-11 | Plugin e API di scripting | M9 | Proposto: rinviare a P2; se introdotto, API versionata e sandbox |
| ADR-12 | Packaging, release e licenza | M0, M5 | Proposto: confronto packaging; licenza del prodotto riservata al proprietario; pack separati |
| ADR-13 | Offline, cataloghi e AI | M2, M8 | `.a` APPROVATO (in review), [M2-entry](M2-entry.md); proposto per cataloghi e AI: core e Advisor offline, funzioni online opt-in, pack locali con licenza verificata |
| ADR-14 | Confini deep sky, planetario e scientifico | M0, M9 | Proposto: core condiviso, workflow e test separati |
| ADR-15 | Motore di analisi e Advisor | M3, M4 | Proposto come soluzione tecnica; il requisito P0 è vincolante, default e costo da decidere |
| ADR-16 | Calibrazione incompleta | Minimo M2 ingresso; estensioni M3 | `.a` APPROVATO (in review), [M2-entry](M2-entry.md); proposto come matrice e default; il requisito P0 è vincolante |
| ADR-17 | Covarianza opzionale | Minimo M2 ingresso; estensioni M3 e M6 | `.a` APPROVATO (in review), [M2-entry](M2-entry.md); proposto per le estensioni: diagonale più NPS locale, correlazioni dei master, nessuna matrice densa globale |

## Licenza del prodotto

Orientamento del proprietario (D-02, 28 ottobre 2026): il prodotto può essere GPL; dipendenze GPL/LGPL compatibili ammesse. Il file `LICENSE` si emette con ADR-12 prima della prima release.

## Quando un ADR cambia stato

Un ADR passa a deciso solo con un verbale che cita evidenza, gate, alternative scartate e limiti residui. L'evidenza va in `docs/evidence/`; il verbale aggiorna questa tabella. Una preferenza personale non basta.
