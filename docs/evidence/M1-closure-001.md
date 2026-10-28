# M1: prove conclusive b, d, f

28 ottobre 2026. Ambiente: Linux 7.0.0-38-generic x86_64, AMD Ryzen 7 6800H (16 thread, avx2, sha_ni), 30 GiB RAM, NVMe ext4. GCC 15.2.0, CMake di sistema, Qt 6.10.2, OpenSSL 3.5.5. Sessione grafica Wayland (mutter) con XWayland. Codice usa e getta, non integrato.

## M1.b: primo frame Qt Widgets

Sorgenti: `tmp/m1/b-viewer/{core.cpp,qt_viewer.cpp,run_io.cpp,viewer.qrc,CMakeLists.txt}` (preparati in precedenza, non modificati).
Build: `cmake -S tmp/m1/b-viewer -B tmp/m1/b-viewer/build-claude-001 -DCMAKE_BUILD_TYPE=Release && cmake --build ... -j4`, exit 0, nessun warning bloccante.
Lanciatore: `tmp/m1/b-viewer/claude_preview.py --platform {xcb,wayland} --dpr {1,2} --qt-scale-factor {0.5,1,2} --run-id <id>`. Confronto con `oracle.snapshot` (implementazione integer/Fraction indipendente) sullo stato x=2048, y=2048, zoom 1/8, frame 0, STF 1; controllo aggiuntivo dell'hash dell'export contro quello loggato dal viewer.

| Run | Piattaforma | dpr | Exit | Primo frame / wall ms | Pixel confrontati | Diversi | SHA-256 export |
|---|---|---|---|---|---|---|---|
| claude-pv-wl-d1-002 | wayland | 1 | 0 | 268,5 / 314,8 | 786432 | 0 | `dc55efe3722d9044…` |
| claude-pv-xcb-d1-003 | xcb (XWayland) | 1 | 0 | 220,3 / 264,4 | 786432 | 0 | `dc55efe3722d9044…` |
| claude-pv-xcb-d2-001 | xcb (XWayland) | 2 | 0 | 628,2 / 715,5 | 3145728 | 0 | `120f256631fcbee6…` |
| claude-pv-wl-d2-001 | wayland | 2 | 0 | 486,9 / 515,2 | 3145728 | 0 | `120f256631fcbee6…` |

Output identico tra piattaforme a parità di dpr. Il coordinatore ha riletto `result.json` e ricalcolato l'hash del `.bgra` di due run: coincidono.

I run `claude-pv-xcb-d1-001/002` si sono fermati con `UNEXPECTED_DPR` (exit non nullo, nessun export): con `Xft.dpi=192` la sessione xcb ha dpr 2 a `QT_SCALE_FACTOR=1`. Il viewer rifiuta correttamente il dpr inatteso (`qt_viewer.cpp:276`); il protocollo fissa la mappa fattore→dpr.

Causa dei fallimenti precedenti (`M1-b-preview-005`, `M1-b-build-006`): guard dell'harness `run.py` (metriche del proprio processo, `provenance.json`, ambiente grafico), non il codice del viewer.

Limiti: xcb è XWayland, non X11 nativo. L'oracle confronta l'immagine prodotta dal core ed esportata dal viewer (`exportViewport`), non i pixel a schermo: lo scanout non è osservato. Copertura: un solo stato (zoom 1/8, STF attivo); il ramo dei pixel invalidi è coperto (192 pixel a dpr 1, 767 a dpr 2), padding, maschera e split no. L'oracle condivide la specifica con `core.cpp`, non il codice. Latenza pan/zoom, accessibilità, IME, gestione colore ICC del display e QML/GTK4 non misurati qui: passano al gate M4.a.

## M1.f: costo dell'hash

Sorgente: `tmp/m1/claude-df/hashbench.cpp`, SHA-256 via OpenSSL EVP. Vettori NIST ("", "abc", 10⁶ × 'a') PASS; digest del file da 1 GiB identico a `sha256sum` (`6ea8912c7b22e8c2…`). 3 processi × 7 ripetizioni dopo 2 warm-up.

| Sorgente | Mediana GB/s (chunk 64 KiB / 1 MiB / 8 MiB) |
|---|---|
| Memoria 16 MiB | 2,06 / 2,17 / 2,07 |
| Memoria 128 MiB | 2,05 / 2,18 / 2,09 |
| Memoria 1 GiB | 2,11 / 2,08 / 2,03 |
| File 1 GiB, page cache calda, `read()` | 1,26 / 1,27 / 1,29 |

La dimensione del chunk non incide. Limiti: un thread, solo cache calda; l'uso di SHA-NI da parte di OpenSSL è dedotto dal throughput, non verificato. Un sub-frame da 50 MB costa circa 25 ms in memoria, circa 40 ms in streaming da file. Grezzi: `tmp/m1/claude-df/results/hash_*.json`.

## M1.d: pool con prenotazioni e spill

Sorgente: `tmp/m1/claude-df/spillbench.cpp`. Dataset: 16 frame float32 8192×8192 (256 MiB l'uno, 4 GiB), valori dyadic esatti. Somma pixel per pixel a tile da 4 MiB, 4 thread, pread/pwrite, verifica bit a bit contro GT intera.

| Budget | Picco RSS | Frame residenti | Spill scritto = letto | Wall (3 corse) | Pixel errati |
|---|---|---|---|---|---|
| 512 MiB | 291,6–291,9 MiB | 1 | 4,03 GB | 6,62 / 5,98 / 5,30 s | 0 |
| 1024 MiB | 803,6–803,8 MiB | 3 | 3,49 GB | 4,73 / 5,64 / 5,72 s | 0 |

| Guasto | Exit | Esito |
|---|---|---|
| Prenotazione singola 1 GiB con budget 512 MiB | 2 | Rifiuto con diagnosi, nessun output, tile non ridotti |
| Budget 16 MiB sotto il working set di 32 MiB | 2 | Rifiuto con diagnosi |
| Directory di spill a sola lettura | 2 | `Permission denied`, nessun ripiego in memoria |
| Directory di spill inesistente | 2 | `No such file or directory` |

**Deviazione dal protocollo preregistrato** (`plans/M1-spikes.md`: pool 32 MiB, tile 2 MiB, 1/2/4 worker, 40 GiB logici, spill fisico 256 MiB). Questa prova usa 4 GiB fisici, tile 4 MiB, budget 512/1024 MiB e 4 thread, per verificare in poco tempo il meccanismo prenotazione–spill–stop prima di scriverlo nel prodotto. M1.d resta **PARZIALE** rispetto alla preregistrazione; il protocollo completo diventa il gate di M2.d sul codice di prodotto.

"Budget rispettato" significa picco RSS sotto il budget: la page cache dello spill non è contabilizzata (niente cgroup né `O_DIRECT`). La GT usa la stessa formula del generatore: verifica accumulo e roundtrip su file, non la formula. I guasti di budget collaudano il pool del bench, non un modulo di prodotto. Limiti: dataset 4 GiB, non 40 GiB; niente `O_DIRECT`, la page cache incide sui tempi; variazione 10–15% tra corse. Gli errori I/O a metà esecuzione escono con codice 1, non 2: il prodotto unifica i codici d'uscita. Grezzi: `tmp/m1/claude-df/results/spill_*.json`; spill e file da 1 GiB cancellati dopo le misure.

## Esito

M1.b, M1.d e M1.f producono i dati per la decisione registrata in `docs/decisions/ADR-M1-stack.md`. Review indipendente: vedi `docs/evidence/M1-closure-001-review.md`.
