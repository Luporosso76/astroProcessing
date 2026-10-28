# M2.b: hash, nodo DAG, slot e provenance

Stato: **DONE** con tre clausole PARZIALI dichiarate, 28 ottobre 2026. Autore: agente executor su Claude Sonnet 5.5. Review indipendente: agente code-reviewer su Claude Opus 5.5, due verifiche. Ambiente: Linux 7.0.0-38-generic x86_64, GCC 15.2.0, OpenSSL 3.5.5, CFITSIO 4.6.3.

## Consegna

- `src/core/{hash,json,node,dag_store,job,provenance}.cpp` e header: SHA-256 EVP a chunk, `read_verified_fits` che confronta digest e dimensione prima di qualunque analisi dell'header; JSON canonico RFC 8785 scritto da zero; `node_spec` immutabile con chiave `SHA-256("astro.node.v1\n" + JCS)`; `fits.decode` con opzioni di lettura e metadati dichiarati nella chiave; `memory_dag_store` con slot, generazioni, pubblicazione per confronto di generazione, `apply_recipe` atomico; `run_job` come unico confine (exit 2 stop, 3 cancel) che produce la provenance.
- `cmake/source_revision.cmake`: revisione git e flag dirty rigenerati a ogni build, limitati ai percorsi che entrano nella build; fuori da un checkout git la configure richiede `-DASTRO_SOURCE_REVISION`, validato, con dirty dichiarato sconosciuto. Verificato dal coordinatore in una copia fuori da git: senza valore exit 1, con valore build riuscita.
- Test: `tests/unit/test_{json,hash,dag,job}.cpp`, vettori NIST e RFC 8785, ordine dei parametri indifferente e degli input determinante.

Comando: `tools/check.sh` e build release. Esito: dev 122/122 (ASan+UBSan), release 121/121.

## Review

Prima verifica: FAIL. Bloccante: il digest veniva confrontato dopo `plan_fits`, quindi un header alterato a pari dimensione dava `fits.malformed` invece di `integrity` e byte non verificati venivano analizzati; il test T-M2B-08 non copriva il caso. MED: provenance non prodotta dal confine, versione CFITSIO troncata, commit git assente, T-M2B-09 tautologico, `apply_recipe` non atomico. Verificati dal revisore: numeri ECMAScript e ordine UTF-16 di JCS sui casi limite e sull'appendice B, digest contro hashlib, concorrenza di `run_job` sotto TSan (8 thread × 200 job).

Seconda verifica: PASS. Sonda ripetuta con TSan, anche `apply_recipe` concorrente a `run_job`. Quattro LOW chiusi dopo: costruttore obbligatorio di `recipe_entry`, dirty sconosciuto con revisione esplicita, dirty sui percorsi della build, `finish_job(completed)` fuori dal `try`.

## Clausole parziali

T-M2B-09 (blocco dei soli dipendenti) e il rilascio delle prenotazioni di T-M2B-12 dipendono dallo scheduler di M2.d; T-M2B-11 (`PRAGMA user_version` fuori dalla chiave) dallo store SQLite di M2.c. Registrate in `docs/test-matrix.md`.
