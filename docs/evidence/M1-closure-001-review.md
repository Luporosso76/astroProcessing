# Review indipendente: chiusura M1 e ADR dello stack

28 ottobre 2026. Revisore: agente `critic` su Claude Sonnet 5.5, diverso dall'autore (coordinatore su Claude Opus 5.5). Oggetto: `docs/decisions/ADR-M1-stack.md`, `docs/evidence/M1-closure-001.md`, codice e grezzi in `tmp/m1/b-viewer/` e `tmp/m1/claude-df/`.

## Ricalcolato dal revisore

- SHA-256 dei quattro export `.bgra`: coincidono con `result.json` e con il record.
- Oracle del viewer riscritto da zero in NumPy con aritmetica intera, senza importare `oracle.py` né `core.cpp`, per `claude-pv-wl-d1-002` (786432 pixel) e `claude-pv-xcb-d2-001` (3145728 pixel): byte-identici.
- Numeri di hash e spill contro i JSON grezzi; digest del file da 1 GiB contro `sha256sum`; cifre citate da `M1-a.md`, `M1-c-*.md`, `M1-e-confirm-affine.md`.
- Non rieseguiti: viewer, `spillbench`, `hashbench`.

## Prima verifica: FAIL

Tre bloccanti, tutti di motivazione, nessuno di misura:

1. ADR-04 scartava JSON per assenza di transazioni, mentre `M1-c-validation.md` dà JSON 46/46 contro SQLite 44/44 sui crash test; la migrazione non era provata.
2. ADR-02 presentava come evidenza un test che confronta l'immagine del core, non lo scanout, e non confronta toolkit.
3. M1.d eseguito con un protocollo diverso dalla preregistrazione senza dichiararlo.

Più MED su: causa FFT asserita senza misura, rapporto GPU non rappresentativo, residui aperti di `M1-a.md`, clausola preflight per ADR-06, RSS contro page cache, `progress.md` non allineato su D-02. LOW su: copertura del viewer, SHA-NI dedotto, argomento contro Rust.

## Correzioni

ADR riscritto: ADR-04 con motivazione dichiarata non misurata e gate M2.c; ADR-02 provvisoria fino al gate M4.a; M1.d dichiarato PARZIALE con deviazione registrata e protocollo completo al gate M2.d; ipotesi FFT, limiti GPU, residui M1-a, clausola preflight, page cache, SHA-NI e copertura del viewer aggiunti. `progress.md` riallineato nello stesso aggiornamento.

## Seconda verifica: PASS

Bloccanti e MED chiusi. Residui corretti dal coordinatore: ADR-01 ora si rivaluta se M4.a sostituisce Qt; il rinvio GPU cita NC-07 e NFR-GPU-01 (righe 370, 808, 2007) invece di "roadmap §21"; questo verbale crea il file citato.
