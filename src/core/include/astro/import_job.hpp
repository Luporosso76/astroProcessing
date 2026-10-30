#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "astro/budget.hpp"
#include "astro/cancel.hpp"
#include "astro/fits.hpp"
#include "astro/inventory.hpp"
#include "astro/job.hpp"
#include "astro/memory_pool.hpp"
#include "astro/node.hpp"
#include "astro/thumbnail_cache.hpp"

namespace astro {

constexpr const char* import_operator_id = "io.import";
constexpr const char* thumbnail_operator_id = "io.thumbnail";
constexpr const char* cache_rebuild_operator_id = "cache.rebuild";
constexpr const char* import_report_format = "astro.import_report.v1";
constexpr const char* import_report_role = "primary";
constexpr const char* import_report_text_role = "report_text";
constexpr const char* cache_rebuild_cause_corrupt = "cache.corrupt";
constexpr const char* cache_rebuild_cause_user = "user_request";

struct import_request {
  std::string path;
  content_digest digest;  // identita' attesa del file
  // Fase obbligatoria (D1); linearita' e offset CFA solo se il file non li dichiara.
  fits_read_options read_options;
  // Solo test: chiamato con "digest.done" dopo la verifica del digest e prima della lettura dell'header.
  std::function<void(std::string_view)> hook = nullptr;
};

using job_hook = std::function<void(std::string_view)>;

node_spec make_import_node(const import_request& request);
node_spec make_thumbnail_node(const content_digest& source, const preview_request& request);
node_spec make_cache_rebuild_node(const content_digest& source, const preview_request& request,
                                  const std::string& cause);

// Documento JSON del report di import e sua resa testuale.
json import_report_json(const fits_inventory& inventory, const content_digest& digest);
std::string import_report_text(const json& report);

// I tre job di I/O passano da job_scheduler::run con un job_plan di un task: il picco (blocco del digest piu' memoria
// del task) e' prenotato dal pool, in coda FIFO, prima del primo task; il digest si verifica nel `prepare`.
// `resources`, se presente, va nella provenance (budget.resources) e da' allo scheduler cpu_set e coda dei job.

// Job di import su uno store durevole: verifica il digest in streaming, inventaria i soli header, pubblica
// report JSON e testuale. Fase assente: metadata_missing import.phase_missing.
job_outcome run_import_job(sqlite_dag_store& store, const std::string& slot_name, const import_request& request,
                           memory_pool& pool, const cancel_token& cancel,
                           const std::optional<budget_snapshot>& budget = std::nullopt,
                           const resource_config* resources = nullptr);

// Job `thumbnail`: riusa o calcola la voce e pubblica un riferimento; l'esito e' in provenance.details.cache.
job_outcome run_thumbnail_job(sqlite_dag_store& store, const std::string& slot_name, const std::string& path,
                              const content_digest& digest, const preview_request& request, thumbnail_cache& cache,
                              memory_pool& pool, const cancel_token& cancel,
                              const std::optional<budget_snapshot>& budget = std::nullopt, const job_hook& hook = {},
                              const resource_config* resources = nullptr);

// Job esplicito `cache.rebuild`. `cause`: cache.corrupt o user_request.
job_outcome run_cache_rebuild_job(sqlite_dag_store& store, const std::string& slot_name, const std::string& path,
                                  const content_digest& digest, const preview_request& request, const std::string& cause,
                                  thumbnail_cache& cache, memory_pool& pool, const cancel_token& cancel,
                                  const std::optional<budget_snapshot>& budget = std::nullopt, const job_hook& hook = {},
                                  const resource_config* resources = nullptr);

}  // namespace astro
