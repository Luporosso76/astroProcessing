# M3.a, unità U8a: F-IO-03, proposta di classificazione, persistenza e import batch

Stato: **chiusa**. Test: review FAIL al giro 2 (1 MED), arbitraggio del coordinatore. Codice: review FAIL al giro 2 (3 MED), arbitraggio del coordinatore (resta il contratto: `import_batch` compone i job con `run_branches`), due review sul delta (blocco «Review»).

| Campo | Valore |
|---|---|
| Unità | M3.a / U8a (test e codice) |
| Scopo | Registrazione degli input del batch con un job d'import per file composto con `run_branches` (CL-04), proposta di classificazione da regole dichiarate (CL-01), revisioni append-only (CL-02), correzione dell'utente che prevale e si registra (CL-03), dichiarazioni di master esterni e `frame_metadata` validate prima della scrittura, `add_input` solo con percorsi assoluti |
| ID | F-IO-03 (parte U8a); T-M3A-05a, 05b, 05c (parte U8a), T-M3A-06b |
| Comando | Integrazione: build pulita, `tools/check.sh` e `ctest --preset release` completo (blocco generato). Mutazioni: `tools/mutate.sh` (patch in `tmp/u8ac-mut/` e `tmp/u8a-arb-mut/` del worktree) |
| Versioni | GCC 15.2.0, CMake 4.2.3, Ninja, SQLite di sistema |
| Ambiente | Linux 7.0.0-38-generic x86_64, 16 CPU; worktree `.claude/worktrees/api-u8ac` sulla base U4 + U5 + U9 |
| Autore e data | Test e codice: agenti Claude Sonnet 5.5 (API), distinti. Review: Claude Opus 5.5 (Argon fuori quota). Coordinatore: Claude Opus 5.5. 6 novembre 2026 |

## Decisioni del coordinatore

- CL-04 resta com'è: `import_batch` costruisce un `branch_files_job` per file e chiama una sola volta `run_branches`. `branch_files_job` porta il `job_request` fino a `run_job_files`, così controllo del nodo e risorse restano quelli del job singolo; `make_import_branch_job` e `make_raw_import_branch_job` sono usate anche da `run_import_job` e `run_raw_import_job`.
- Annullamento: dopo un ramo `cancelled` i successivi restano `not_started`, senza job, provenance né generazione nuova (T-M3A-05a). Con nodi senza archi un ramo non resta mai `not_started` o `blocked` dopo il primo `cancelled`: l'invariante è un `ASTRO_ASSERT`, non un ramo di stop.
- `input_record::format` è obbligatorio (U9): i test di `add_input` lo impostano, così i percorsi relativi falliscono per `require_absolute_input_path` e non per il formato.

## Mutazioni

| Patch | Cosa cambia | Esito |
|---|---|---|
| `m1_role_from_imagetyp_substring` | ruolo dedotto da una sottostringa di `IMAGETYP` | rilevata |
| `m2_relative_path_accepted` | percorso relativo accettato | rilevata |
| `m2b_add_input_unchecked` | controllo del percorso assoluto tolto da `add_input` | rilevata |
| `m3_classification_not_persisted` | revisione non committata | rilevata |
| `m3b_role_persisted_wrong` | ruolo scritto sbagliato | rilevata |
| `m4_imagetyp_conflict_not_signaled` | conflitto di `IMAGETYP` non segnalato | rilevata |
| `abs_path_always_true` | `require_absolute_input_path` sempre vero | rilevata |
| `additive_accepts_any` | valore fuori elenco di `additive` accettato | rilevata |
| `descriptor_rejects_all` | validatore che rifiuta ogni descrittore | rilevata |
| `stopall` | lo stop di un ramo ferma tutto il batch | rilevata |
| `sequential` | una chiamata a `run_branches` per file | sopravvissuta al primo giro (nessun test di annullamento), rilevata dopo i test di T-M3A-05a |

## Limiti

- L'annullamento richiesto prima della chiamata esce dal calcolo del digest, prima di `add_input` e di `run_branches`: nessun input registrato.
- I rami sono eseguiti in sequenza, in ordine di indice: `run_branches_with` non è concorrente (comportamento dello scheduler, non di U8a).
- Codici d'errore nuovi del codice non ancora nella tabella «Errori aggiunti» del contratto (`batch.open`, `batch.read`): LOW della review del giro 1, fuori dal perimetro dell'arbitraggio.

## Review

- Test, giro 1 FAIL (4 MED), giro 2 FAIL (1 MED); arbitraggio del coordinatore.
- Codice, giro 1 FAIL (3 MED), giro 2 FAIL (3 MED). Arbitraggio: review sul delta `tmp/api/u8acode-arb-review.json` FAIL (1 MED: nessun test di annullamento, `sequential` non equivalente); seconda review `tmp/api/u8a-cancel-review.json` PASS con 3 LOW (ramo `not_started` irraggiungibile, fuso nell'asserzione; input non registrati e guard RAII nei test di annullamento).

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/F-IO-03-classificazione.md` | `202fd1d1765a882599f1dd96eb93bd7530dc70ebdc532cf674119a5c9344a472` |
| `src/core/batch_classification.cpp` | `c80e92738b657bdaf53bd3041ef2af89e533ca029b54f96283e6d290cfa5a446` |
| `src/core/import_job.cpp` | `9eb47cef9ce9a8f69cab4f5dab0d8936cd1f802f88f369d2399eedef85ae5a85` |
| `src/core/include/astro/batch_classification.hpp` | `3ff40fc084e923fc70ad5c62dfcd60e72b9bada5eb3c928930233d1e8747c98d` |
| `src/core/include/astro/scheduler.hpp` | `262fd9f4614f94446a2caf10af85e383a0da052d77f290a7e84619dc5affa722` |
| `src/core/raw_import_job.cpp` | `fe515c94dbc5f278bd2c9d4e30d18e5ba495ca89a5344d1b909aa0758e79fa62` |
| `src/core/scheduler.cpp` | `a69db97e6e8c66106cc15ad6490322658722ae1e32911f9e0882368e6404c446` |
| `src/core/sqlite_classification.cpp` | `8cdca8a6d5a01e0b03244436038944d683b75d4ae66251d1a33f64ef40bcf6d4` |
| `src/core/sqlite_dag_store.cpp` | `72d97ae8f15069d3c4bbdf0c46b2ef81a1b9a50b4bc3a6d7d80f7ac3c6b0bd72` |
| `tests/unit/test_batch_classification.cpp` | `66545610b30f5ba3cd5397b0cd94e46e3ba16cf10c076048abf66fe8bd15ef64` |
| `tests/unit/test_input_paths.cpp` | `b7264ab06b2e3f3493bb4fa8b96e323bce0ee0ff47861b021c5987a3bfbc6e09` |

- Test release (`tmp/release-junit.xml`): totali 1221, fast 1199, slow 22, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 1199, fast 1199, slow 0, falliti 0
<!-- evidence:end -->
