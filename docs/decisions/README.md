# Registro ADR

Registro compatto delle 17 decisioni architetturali del piano. Il testo normativo, le opzioni e le evidenze richieste stanno in `ASTRO_POSTPROCESSING_PLAN_v2.2.1.md` §20; qui c'è solo lo stato. Nessuno stack è scelto. Lo stato di avanzamento è in `docs/progress.md`.

## Stato

Le soluzioni implementative restano **proposte** fino ai rispettivi gate. La [politica d'ingresso M0](M0-policy.md) prepara licenze e dataset per gli spike; non sceglie la licenza del prodotto né chiude i gate successivi. La decisione tecnica è delegata agli agenti, ma richiede le misure e le verifiche elencate: la delega non sostituisce una prova.

Le decisioni D-19, D-20 e D-21 non aggiungono ADR. Si riflettono in ADR-09 (deconvoluzione), ADR-10 e ADR-13 (colore, WCS, cataloghi offline) e ADR-12 (pack come decisione separata dalla licenza del prodotto).

## Voci

| ID | Tema | Gate | Stato |
|---|---|---|---|
| ADR-01 | Linguaggio e componenti del core | M1 | Nessuno scelto: candidati C++20/23, Rust, Python con core nativo e C#/.NET, tutti da misurare |
| ADR-02 | GUI e comunicazione con il core | M1 | Nessuna scelta: Qt Widgets, QML, GTK4 e Avalonia da confrontare su viewer, licenza, accessibilità, colore, IME, packaging |
| ADR-03 | Modello pixel, precisione e maschere | M2 ingresso; estensioni M3 | Proposto: immagine tipizzata, f32 candidato, maschera distinta da invalidità e alpha |
| ADR-04 | Persistenza del progetto | M2 ingresso | Proposto: SQLite sola autorità, cache derivata, scrittura atomica, schema ignoto in sola lettura |
| ADR-05 | DAG, cache e invalidazione | M2 ingresso | Proposto: DAG immutabile, hash di contenuto, token di generazione |
| ADR-06 | RAM, scheduler e spill | M2 ingresso | Proposto: pool con prenotazioni, code bounded, spill, stop con errore se il budget non basta |
| ADR-07 | CPU, SIMD e GPU | M1, M7 | Proposto: CPU primaria, dispatch SIMD nel preflight, GPU facoltativa senza sostituzioni |
| ADR-08 | Import RAW, FITS e XISF | M2 ingresso; estensioni M3 | Proposto: librerie mature con licenze verificate, contratto del decoder esplicito |
| ADR-09 | Calibrazione, registrazione, stacking e deconvoluzione P0 | Minimo M2 ingresso; operatori M3 ingresso | Proposto: casi delle sezioni 8 e 9, ground truth, una sola formulazione P0 |
| ADR-10 | Colore, WCS e maschere | M3, M4 | Proposto: separare lineare e display, propagare maschere e covarianza, WCS non certifica il colore |
| ADR-11 | Plugin e API di scripting | M9 | Proposto: rinviare a P2; se introdotto, API versionata e sandbox |
| ADR-12 | Packaging, release e licenza | M0, M5 | Proposto: confronto packaging; licenza del prodotto riservata al proprietario; pack separati |
| ADR-13 | Offline, cataloghi e AI | M2, M8 | Proposto: core e Advisor offline, funzioni online opt-in, pack locali con licenza verificata |
| ADR-14 | Confini deep sky, planetario e scientifico | M0, M9 | Proposto: core condiviso, workflow e test separati |
| ADR-15 | Motore di analisi e Advisor | M3, M4 | Proposto come soluzione tecnica; il requisito P0 è vincolante, default e costo da decidere |
| ADR-16 | Calibrazione incompleta | Minimo M2 ingresso; estensioni M3 | Proposto come matrice e default; il requisito P0 è vincolante |
| ADR-17 | Covarianza opzionale | Minimo M2 ingresso; estensioni M3 e M6 | Proposto: diagonale più NPS locale, correlazioni dei master, nessuna matrice densa globale |

## Licenza del prodotto

Resta una decisione riservata al proprietario e non è fissata da nessun ADR. ADR-12 prepara il confronto, non sceglie. Nessuna licenza viene dichiarata prima della decisione registrata.

## Quando un ADR cambia stato

Un ADR passa a deciso solo con un verbale che cita evidenza, gate, alternative scartate e limiti residui. L'evidenza va in `docs/evidence/`; il verbale aggiorna questa tabella. Una preferenza personale non basta.
