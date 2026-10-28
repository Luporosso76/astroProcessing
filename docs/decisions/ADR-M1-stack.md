# ADR M1: stack tecnico

Stato: **ADOTTATO** il 28 ottobre 2026, dopo la review indipendente `docs/evidence/M1-closure-001-review.md` (PASS alla seconda verifica).

Decide ADR-01, ADR-07.a e la strategia hash di M1.f. Fissa ADR-04 e ADR-06 come scelte di progetto da verificare nei gate M2.c e M2.d. Fissa ADR-02 come scelta provvisoria da confermare al gate M4.a.

## Decisioni

| ADR | Decisione | Forza dell'evidenza |
|---|---|---|
| ADR-01 linguaggio | **C++20**, GCC 15.2, CMake. Un solo linguaggio nel prodotto; core headless in libreria separata dalla GUI. | `M1-a.md`: tutti i candidati corretti (360/360, fault-stop 36/36, ASan/UBSan 120/120 per C++). Prestazioni miste e vicine: convoluzione f32 C++/Rust 0,847 [0,818; 0,869]; warp 3,14 ms C++, 3,70 Rust, 8,41 NumPy; affine più veloce in Rust. Il criterio decisivo non è la velocità ma un solo linguaggio senza ponte FFI e la GUI Qt nativa in C++. Se il gate M4.a sostituisce Qt, ADR-01 si rivaluta. |
| ADR-02 GUI | **Qt 6 Widgets, provvisoria.** Confermata o sostituita al gate M4.a su latenza pan/zoom, accessibilità, IME e colore ICC, con QML come alternativa da misurare in quel gate. | `M1-closure-001.md`: build e rendering del core tramite il viewer Qt Widgets riusciti su Wayland e XWayland, dpr 1 e 2, export identico all'oracle. Non è un confronto fra toolkit: lo scanout non è osservato e QML e GTK4 non sono misurati. La scelta può restare provvisoria perché M2 e M3 sono headless. |
| ADR-04 persistenza | **SQLite** come autorità del progetto; JSON per manifest di export e provenance leggibili. | `M1-c-validation.md` e `M1-c-measurement.md`: entrambi i candidati superano i crash test (SQLite 44/44, JSON 46/46); SQLite è più lento (1,09 sul ciclo completo, commit 7,2 ms contro 3,0). La scelta poggia su ragioni **non misurate in M1**: transazioni su più record, query sul DAG, migrazioni di schema. Migrazione con backup, recovery e schema ignoto in sola lettura sono il gate di M2.c (piano righe 1832, 2584). |
| ADR-07.a CPU/GPU | Percorso CPU di riferimento; SIMD scelto nel preflight. **Nessun backend GPU nel prodotto fino a M7**, come da NC-07 e NFR-GPU-01 (piano righe 370, 808, 2007). | `M1-e-*`: correttezza Vulkan e nove fault confermati. Il rapporto CPU/GPU 0,085 [0,041; 0,111] sul microcaso 17×19 non è rappresentativo delle immagini reali e non motiva il rinvio, che segue la roadmap. |
| M1.f hash | **SHA-256 via OpenSSL EVP**; chunk di streaming 1 MiB. | `M1-closure-001.md`: circa 2,1 GB/s in memoria e 1,27 GB/s da file a cache calda, a un thread; chunk irrilevante. |
| ADR-06 vincoli | Pool con prenotazioni esplicite e spill nello scratch scelto dall'utente. Residenza e spill sono fissati nella ricetta dichiarata nel preflight, non adattati a runtime. Budget sotto il working set, prenotazione eccedente o scratch non scrivibile: stop con diagnosi, mai riduzione dei tile o ripiego in memoria. | `M1-closure-001.md`: prova **parziale** rispetto al protocollo preregistrato (vedi lì la deviazione). RSS sotto il budget, 0 pixel errati, quattro guasti con exit 2. Il protocollo completo è il gate di M2.d sul codice di prodotto. |
| FFT | **FFTW 3.3.10.** | Licenza GPL ammessa dal proprietario (D-02, 28 ottobre 2026). Che il ritardo FFT di C++ e Rust in `M1-a.md` dipenda dalla creazione dei piani a ogni chiamata è un'ipotesi non verificata: riuso e determinismo dei piani si misurano in M2. |

## Alternative scartate

- **Rust:** prestazioni non migliori nel complesso; richiederebbe un binding verso Qt, non misurato, o un'altra GUI.
- **Python/NumPy nel core:** warp 2,7 volte più lento; costringerebbe a un core misto.
- **GTK4:** header C assenti nell'ambiente; il binding disponibile richiede Python nel prodotto. **QML:** non scartato, resta l'alternativa misurata al gate M4.a.
- **JSON come autorità:** commit più rapido e crash test superati, ma senza transazioni multi-record né migrazioni; resta per i manifest.
- **pocketfft:** era motivato dall'evitare la GPL di FFTW, ora ammessa. Ramo chiuso; nessun file cancellato.
- **BLAKE3:** non disponibile localmente; SHA-256 non è il collo di bottiglia rispetto all'I/O.

## Rischi aperti e dove si chiudono

- Latenza pan/zoom, accessibilità, IME, colore ICC, confronto con QML: gate M4.a.
- Protocollo M1.d completo (pool 32 MiB, tile 2 MiB, 1/2/4 worker, 40 GiB logici, spill fisico): gate M2.d. La page cache dello spill non è contabilizzata nell'RSS: M2.d misura anche quella (cgroup o `O_DIRECT`).
- Migrazione, backup e schema ignoto in SQLite: gate M2.c.
- Ownership della memoria tra moduli e contratto delle maschere, aperti in `M1-a.md`: con un solo linguaggio non c'è ponte FFI; il contratto delle maschere è ADR-03 in M2.g.
- Riuso e determinismo dei piani FFTW: M2.
- Uso effettivo di SHA-NI da parte di OpenSSL: dedotto dal throughput, da verificare in M2.b.
- Emissione del file `LICENSE` (orientamento GPL-3.0-or-later, compatibile con FFTW GPL-2+ e Qt): ADR-12, prima della prima release.
