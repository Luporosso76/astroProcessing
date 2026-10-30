# Contratto ADR-05.a: DAG, generazione e provenance

Stadio: ADR-05.a, DAG minimo (registro riga 3394). Piano §7.4 (righe 710-736), F-PRJ-03, F-PRJ-07, NFR-FBK-01. Unità: M2.b; integrazione durevole in M2.c ([ADR-04](ADR-04-persistenza.md)). Hash: SHA-256 via OpenSSL EVP, chunk 1 MiB ([ADR-M1-stack](../decisions/ADR-M1-stack.md)).

## Modello d'errore comune

Vale per tutti i contratti di `docs/contracts/`.

```cpp
namespace astro {
enum class error_category {
  invalid_input, limit_exceeded, metadata_missing, incompatible, unsupported,
  numeric, io, integrity, schema, graph, stale_generation, budget,
  cancelled, resume_mismatch, security,
};
class stop_error : public std::exception {
  error_category category; std::string code; std::string message;
  std::vector<std::pair<std::string, std::string>> context;
};
}
```

- `code` è stabile, `<modulo>.<condizione>` in `snake_case`; i test asseriscono categoria e codice.
- Si lancia al punto di rilevazione. Un `catch` intermedio può solo aggiungere contesto e rilanciare la stessa categoria. Nessun `catch` prosegue.
- Il confine (CLI, runner dei job) cattura, scrive la provenance dell'arresto, marca il job `failed` o `cancelled`, rilascia le prenotazioni ed esce con codice **2**; per la categoria `cancelled` il codice è **3**, così un annullamento si distingue da un guasto. Un arresto prima del lavoro del job si registra allo stesso modo e si propaga come eccezione (sezione [Provenance](#provenance)).
- Una violazione di invariante interno è un bug: `ASTRO_ASSERT` chiama `std::abort`. Non è una categoria.
- Nessuna categoria attiva un percorso alternativo (NC-21). T-FBK-01 lo verifica tracciando le chiamate.

## Identità degli input

`content_digest`: `sha256` (32 byte, esadecimale minuscolo in testo) più `size_bytes` (`uint64_t`). Percorso e mtime non sono identità.

Prima di ogni uso numerico, il file si legge per intero in un buffer (dimensione prenotata in ADR-06, limiti di ADR-03). Digest e dimensione si calcolano su quel buffer e si confrontano col record di `inputs`. Il decode usa lo stesso buffer: il digest è legato ai byte effettivamente usati. Stato in sessione: `unverified` fino a questo controllo.

| Condizione | Categoria | Codice |
|---|---|---|
| Digest o dimensione diversi dal record | `integrity` | `input.digest_mismatch` |
| File assente | `io` | `input.missing` |
| Dimensione del file cambiata rispetto a `fstat` (cresciuto, o fine file prima del previsto) | `integrity` | `input.size_changed` |
| Lettura che restituisce meno byte senza cambio di dimensione, mtime o ctime dall'apertura | `io` | `input.short_read` |
| Lettura che restituisce meno byte con mtime o ctime cambiati dall'apertura (file riscritto durante la lettura) | `integrity` | `input.modified` |

L'errore ferma solo i nodi che dipendono da quell'input; i rami indipendenti proseguono (T-M2B-09).

## Nodo

`astro::node_spec`, immutabile dopo la costruzione (nessun setter):

| Campo | Tipo | Note |
|---|---|---|
| `operator_id` | `std::string` | Es. `fits.decode`, `calib.light`. Per `fits.decode`, `fits_read_options` (offset CFA dichiarati) e i valori applicati con `declare_metadata` sono parametri del nodo ed entrano nella sua chiave. |
| `algorithm_version` | `uint32_t` | Cambia quando cambia il contratto numerico. |
| `serialization_version` | `uint32_t` | Cambia quando cambia la serializzazione dei parametri. |
| `params` | JSON canonico | RFC 8785 (JCS). NaN e ±Inf rifiutati (`invalid_input`, `dag.param_nonfinite`). Interi oltre 2^53 come stringhe. |
| `inputs` | vettore ordinato di (`input_name`, `node_key` o `content_digest`) | L'ordine è parte dell'identità. |
| `precision` | `sample_type` di calcolo | ADR-03. |
| `backend` | `std::string` | Backend fissato nel preflight, es. `cpu_scalar`. |
| `seed` | `std::optional<uint64_t>` | `nullopt` se il risultato non dipende da rumore simulato. |

`node_key` = SHA-256 di `"astro.node.v1\n"` concatenato al JCS dell'oggetto con tutti i campi sopra. Un input file entra come `"input:<sha256>:<size>"`. I metadati rilevanti entrano attraverso il digest del file o i parametri dichiarati; la maschera viaggia con l'immagine del nodo padre.

Conseguenze verificate:

- cambiare un campo cambia `node_key` (T-M2B-03);
- il cambio si propaga ai discendenti perché la loro chiave include quella del padre; un nodo a monte non toccato conserva la chiave e la cache (T-M2B-04);
- `PRAGMA user_version` del database non entra nella chiave (T-M2B-11);
- il backend eseguito deve coincidere con `backend`; se differisce: `ASTRO_ASSERT`, perché nessun codice M2 può cambiarlo.

Un arco che crea un ciclo produce `graph`, `dag.cycle`, alla costruzione e alla pubblicazione (T-M2B-05). Un padre assente produce `graph`, `dag.missing_parent`.

## Generazione e freschezza

Ogni risultato pubblicabile ha uno slot (tabella `slots` di ADR-04) con `current_generation` (`uint64_t`, monotono) e `target_node_key`, la chiave della ricetta corrente.

- Avvio del job, in una transazione: `current_generation += 1`, il job riceve quel valore.
- Pubblicazione, in una transazione: se `current_generation` è ancora quella del job, lo slot punta al nuovo artefatto. Altrimenti `stale_generation`, `dag.stale_publish`: l'artefatto resta come cache del suo `node_key` ma non diventa corrente (T-M2B-06).
- Lettura: `slot_view` con `artifact` (`std::optional`) e `freshness` ∈ {`current`, `stale`, `missing`}. `stale` quando il `node_key` dell'artefatto differisce da `target_node_key`; `missing` se l'artefatto manca o ha `present = 0`. Un risultato `stale` non si restituisce mai con `freshness = current`. Undo e redo cambiano `target_node_key` e incrementano la generazione ([ADR-04](ADR-04-persistenza.md#snapshot-undo-e-redo-f-prj-04)).

## Provenance

`provenance_record`, serializzato in JSON canonico e scritto come artefatto con il protocollo atomico. Uno per ogni job, riuscito o fermato:

- tutti i campi di `node_spec`, `node_key`, digest degli input;
- versione del prodotto (commit Git della build) e versioni di CFITSIO, SQLite, OpenSSL;
- generazione del job e stato finale;
- ledger delle correzioni applicate e omesse ([ADR-16](ADR-16-stati-master.md)) e modello di varianza ([ADR-17](ADR-17-varianza.md)), per i nodi di calibrazione;
- avvisi;
- digest degli output, solo se `completed`;
- arresto: categoria, codice, messaggio, contesto (provenance dell'arresto, NFR-FBK-01);
- resume (estensione R4, F-PRJ-06): solo per un job eseguito da `resume_workflow` ([ADR-04](ADR-04-persistenza.md#workflow-e-resume-d15-d16)), campo `resume` con `workflow` (nome), `definition_sha256` (SHA-256 della definizione registrata) e `reused`: per ogni passo riusato, slot, `node_key`, generazione e digest degli artefatti riverificati. Un job eseguito fuori da un resume non ha il campo, quindi la sua provenance resta byte per byte quella di prima.

Il riuso di un passo sicuro non apre un job e non scrive provenance. Le esecuzioni concluse di un nodo (riuscite o fermate) sono le sue provenance; i tentativi avviati sono i suoi job, compresi quelli `interrupted` (T-M2R4-01). Un job ucciso prima del commit non lascia provenance: resta `interrupted` nella tabella `jobs` ed entra nel report di recovery. Un job ucciso dopo il commit è `completed` e ha la sua provenance.

Se lo store non è scrivibile, la diagnosi va su stderr e il codice di uscita resta 2 (3 per `cancelled`); non si scrive provenance altrove.

**Arresto prima del lavoro (R5.d, NFR-FBK-01.a).** Un arresto al confine prima del lavoro del job (ambiente FP vietato, preparazione del job, store durevole passato a `run_job`, nodo del job assente o illeggibile; elenco in `job.hpp`) si registra senza leggere il nodo. Il job si chiude `failed` (`cancelled` per quella categoria) con categoria e codice. Se non è ancora avviato, lo si avvia solo per registrare l'arresto. Sullo store durevole la provenance dell'arresto (`stop_before_work_record`) si scrive come artefatto `provenance` del nodo. Il record contiene soltanto `node_key`, `versions`, `generation`, `state`, `stop` (categoria, codice, messaggio, contesto), `stop_before_work: true` ed eventualmente `resume`: stringhe e interi. Non porta parametri, `node_spec`, digest degli input, avvisi, budget né `details`: con l'ambiente FP vietato niente analizza o serializza numeri in virgola mobile. Lo stop si propaga poi al chiamante come eccezione, con la stessa categoria, lo stesso codice e lo stesso contesto. Il chiamante ne ricava il codice di uscita con `exit_code_for`. Se la registrazione fallisce, lo stop originale si propaga con l'errore secondario nel contesto `registrazione_arresto`. Non si registra un fallimento dell'avvio del job (`start_job`: slot inesistente o escluso, store in sola lettura, errore SQL): è la scrittura stessa del record che non riesce. L'errore si propaga invariato e nessun job esiste.

## Test

| ID | Clausola |
|---|---|
| T-M2B-01 | SHA-256 sui vettori NIST e su file a chunk multipli e non multipli di 1 MiB. |
| T-M2B-02 | JCS: ordine delle chiavi, numeri, escape; NaN rifiutato. |
| T-M2B-03 | Ogni campo di `node_spec` cambia la chiave. |
| T-M2B-04 | DAG a diamante: cambio di un parametro rinnova solo il nodo e i discendenti. |
| T-M2B-05 | Ciclo e padre assente rifiutati. |
| T-M2B-06 | Due job sullo stesso slot, il vecchio finisce dopo: pubblicazione rifiutata, slot `current` sul nuovo. |
| T-M2B-07 | Stesso size e mtime, contenuto diverso: rilevato prima dell'uso. |
| T-M2B-08 | File modificato dopo l'import: `integrity`, nessun decode. |
| T-M2B-09 | Input mancante: fermo solo il ramo dipendente. |
| T-M2B-10 | Provenance completa per un job riuscito e per uno fermato. |
| T-M2B-11 | Versione di schema esclusa dalla chiave. |
| T-M2B-12 | Ogni categoria al confine: exit 2 (3 per `cancelled`), job marcato, prenotazioni rilasciate. R5.d: arresto prima del lavoro (nodo assente o illeggibile, store durevole in `run_job`, causa non valida di `cache.rebuild`) registrato e propagato invariato; registrazione fallita con `registrazione_arresto`; avvio del job fallito propagato senza job e senza nuovo tentativo. |
| T-M2B-13 | `fits.decode`: cambiare `fits_read_options` o un valore `declare_metadata` cambia `node_key`; stessi parametri, stessa chiave. Da implementare in M2.b. |
| T-M2R4-01 | Estensione R4: la provenance di un job eseguito da un resume ha il campo `resume` con i passi riusati; un passo riusato non aggiunge provenance. |

## Aggiunte di R1: nodi di import, thumbnail e rebuild; fase e linearità nella provenance

- `provenance_record` ha il campo opzionale `details` (oggetto JSON), serializzato come `details` solo se presente. I job ne sono gli unici autori; non entra nella chiave del nodo.
- Un job che legge file ne elenca la lettura in `details.observed_inputs`: per ogni file `input_name`, `sha256`, `size_bytes`, `phase` e `linearity` con la rispettiva `origin` (`header` o `declared`; la linearità `unknown` non ha origine di header), e `uninterpreted_keywords` (F-IO-01, F-IO-10). Lo scrivono `run_import_job` e `run_calibration_job`.
- `details.cache` (job `thumbnail` e `cache.rebuild`): `action`, `key`, `entry`, `outcome` (`cache_hit`, `computed`, `rebuilt`); per `cache.rebuild` anche `cause` e `replaced_entry` (`name`, `found_digest`).
- Nodi (precisione `f64`, backend `cpu_scalar`, nessun seed): `io.import` (parametri `declared_phase`, `declared_linearity`, `declared_bayer_x_offset`, `declared_bayer_y_offset`, `report_version`), `io.thumbnail` (`block_width`, `block_height`, `cache_algorithm_version`) e `cache.rebuild` (gli stessi, più `target` e `cause`); ingresso: il digest del file sorgente. Un nodo `io.thumbnail` e un nodo `cache.rebuild` con gli stessi parametri hanno chiavi diverse.
- `fits_read_options` con `declared_phase` o `declared_linearity` entra nei parametri del nodo `fits.decode` solo se valorizzata (le chiavi dei nodi esistenti non cambiano).
- Il nodo di calibrazione riporta in `fits_read_options` di ogni ingresso anche `declared_phase`. La calibrazione richiede la fase `raw` e la linearità `linear` per tutti i suoi quattro ingressi ([ADR-03](ADR-03-pixel-maschere.md)).

## Fuori da M2

Ruleset e cache di ricerca (ADR-05.b); snapshot immutabili degli input; digest parziali per l'esplorazione; distinzione fra controllo di regressione e validazione indipendente; verifica di completezza per configurazione (§11.11).
