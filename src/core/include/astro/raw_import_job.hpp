#pragma once

#include <optional>
#include <string>

#include "astro/budget.hpp"
#include "astro/cancel.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "astro/job.hpp"
#include "astro/json.hpp"
#include "astro/memory_pool.hpp"
#include "astro/node.hpp"
#include "astro/raw_decode.hpp"
#include "astro/resources.hpp"
#include "astro/scheduler.hpp"
#include "astro/sqlite_dag_store.hpp"

namespace astro {

constexpr const char* raw_import_operator_id = "io.raw_import";
constexpr const char* raw_import_image_role = "primary";

// Nodo `io.raw_import`: digest del file e record del decoder entrano nei parametri, quindi nella chiave (§2.9). `record`
// nullo = il file non e' stato decodificato quando il nodo e' stato costruito (il job lo ferma con lo stesso errore);
// ogni altro valore deve essere un record.
node_spec make_raw_import_node(const content_digest& digest, const json& record);

struct raw_import_request {
  std::string path;
  content_digest digest;  // identita' attesa del file
  node_spec node;         // costruito da make_raw_import_request; il job lo confronta con quello che ricostruisce
  // Arresto della lettura o della decodifica alla costruzione del nodo (il nodo ha allora record nullo): il job lo
  // rilancia identico, con categoria, codice e contesto, prima di aprire il file.
  std::optional<stop_error> identification_stop;
};

// Lettura verificata del file (formato RAW), decodifica e record, poi `make_raw_import_node` sul record. Se il file non e'
// leggibile, e' diverso dal digest o non si decodifica, il nodo ha record nullo e l'arresto e' in `identification_stop`
// (il costruttore del job minimo non ferma). Limite dichiarato: la lettura e la decodifica della costruzione non passano
// dal pool, dallo scheduler ne' dal token di cancel; sono limitate solo da raw_limits (§2.1).
raw_import_request make_raw_import_request(std::string path, content_digest digest);

// Ramo di `run_branches` per un import RAW: stesso lavoro e stessa richiesta di run_raw_import_job. `store`, `request`,
// `pool`, `cancel` e `resources` devono vivere fino al ritorno di run_branches.
branch_files_job make_raw_import_branch_job(sqlite_dag_store& store, const std::string& slot_name,
                                            const raw_import_request& request, memory_pool& pool,
                                            const cancel_token& cancel, const resource_config* resources = nullptr);

// Job di import RAW sullo scheduler (OB-08): `prepare` verifica il digest in streaming, il task legge e verifica i byte,
// decodifica (`decode_raw_buffer`, un solo thread, ADR-08.b §2.7); dopo lo scheduler il job costruisce il nodo con
// `make_raw_import_node`; `finalize` riverifica il file (OB-11). Il nodo costruito deve avere la chiave dello slot: altrimenti arresto
// invalid_input raw_import.node_mismatch. Pubblica l'immagine FITS (ruolo `primary`) e il record in `details.decoder`.
job_outcome run_raw_import_job(sqlite_dag_store& store, const std::string& slot_name, const raw_import_request& request,
                               memory_pool& pool, const cancel_token& cancel,
                               const std::optional<budget_snapshot>& budget = std::nullopt,
                               const resource_config* resources = nullptr);

}  // namespace astro
