#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "astro/budget.hpp"
#include "astro/json.hpp"
#include "astro/memory_pool.hpp"
#include "astro/scheduler.hpp"
#include "astro/spill.hpp"

namespace astro {

class sqlite_dag_store;

template <typename T>
struct tunable {
  T value{};
  std::string origin;  // non vuota
  friend bool operator==(const tunable&, const tunable&) = default;
};

struct dir_quota {
  std::filesystem::path dir;  // assoluta
  std::uint64_t quota_bytes = 0;
  friend bool operator==(const dir_quota&, const dir_quota&) = default;
};

constexpr const char* resource_config_setting_key = "resources.config";
constexpr const char* vram_null_reason = "nessun backend GPU nel prodotto fino a M7";

// Configurazione delle risorse (ADR-06, estensione R3.1). I default portano l'origine dichiarata.
struct resource_config {
  std::optional<std::uint64_t> rss_target_bytes;
  std::optional<dir_quota> scratch;
  std::optional<dir_quota> disk_cache;
  std::optional<std::vector<unsigned>> cpu_set;
  std::optional<unsigned> worker_count;    // nullopt = auto
  std::optional<std::uint32_t> tile_rows;  // nullopt = auto
  tunable<std::size_t> job_queue_capacity;
  tunable<std::size_t> task_queue_capacity;
  tunable<std::uint64_t> report_margin_bytes;
  tunable<budget_ratio> fraction_normal;
  tunable<budget_ratio> fraction_pressure;
  tunable<double> psi_threshold;

  resource_config();
  friend bool operator==(const resource_config&, const resource_config&) = default;
};

// invalid_input resources.<campo>.
void validate_resource_config(const resource_config& config);
// invalid_input sched.cpu_set (CPU fuori dall'affinita'), sched.worker_count (worker oltre l'insieme).
void validate_cpu_set(const resource_config& config, const affinity_fn& affinity = system_affinity);

json resource_config_json(const resource_config& config);
resource_config parse_resource_config(const json& doc);  // invalid_input resources.setting_malformed
void save_resource_config(sqlite_dag_store& store, const resource_config& config);
std::optional<resource_config> load_resource_config(const sqlite_dag_store& store);

budget_tuning budget_tuning_of(const resource_config& config);
scheduler_tuning scheduler_tuning_of(const resource_config& config);

// Ingresso del pianificatore per la calibrazione (formule in ADR-06, estensione R3.1).
struct planner_input {
  std::uint64_t budget_bytes = 0;
  std::size_t cpus = 1;
  std::uint32_t width = 0, height = 0;
  std::uint64_t out_sample_bytes = 0;
  std::uint64_t input_peak_bytes = 0;
  std::uint64_t header_bound_bytes = 0;
};

struct resource_plan {
  unsigned worker_count = 1;
  std::uint32_t tile_rows = 1;
  std::uint64_t task_count = 0;
  std::size_t task_queue_capacity = 1;
  std::uint64_t shared_bytes = 0;
  std::uint64_t per_task_bytes = 0;
  std::uint64_t peak_bytes = 0;
  std::uint64_t working_set_bytes = 0;
  std::uint64_t minimum_feasible_bytes = 0;
  std::string frame_residency = "whole";
  std::string master_residency = "whole";
  bool budget_below_working_set = false;
  std::vector<std::string> user_fixed;
};

// Grandezze del modello che non dipendono dal budget (nessuno stop per budget). invalid_input resources.planner_input
// per ingressi nulli o non rappresentabili, sched.worker_count per un worker esplicito oltre le CPU.
struct plan_bounds {
  std::uint32_t tile_rows = 1;
  std::uint64_t task_count = 0;
  std::uint64_t shared_bytes = 0;
  std::uint64_t per_task_bytes = 0;
  std::uint64_t minimum_feasible_bytes = 0;
  std::uint64_t working_set_bytes = 0;
};
plan_bounds compute_plan_bounds(const planner_input& input, const resource_config& config);

// Stop budget sched.job_exceeds_budget (con minimum_feasible_bytes) se il minimo o il piano esplicito non entrano.
resource_plan plan_resources(const planner_input& input, const resource_config& config);
json resource_plan_json(const resource_plan& plan);

struct disk_cache_usage {
  std::uint64_t bytes = 0;
  std::uint64_t entries = 0;
};
disk_cache_usage measure_disk_cache(const std::filesystem::path& dir);
void enforce_disk_cache_quota(const std::filesystem::path& dir, std::uint64_t quota_bytes);
void remove_disk_cache_entries(const std::filesystem::path& dir, const std::vector<std::string>& names);

struct monitor_inputs {
  const memory_pool* pool = nullptr;
  const resource_plan* plan = nullptr;       // picco stimato
  const planner_input* planner = nullptr;    // soglie budget_below_working_set / budget_below_minimum
  const resource_config* config = nullptr;   // con planner: obbligatoria
  const budget_snapshot* budget = nullptr;
  const spill_area* scratch = nullptr;
  std::uint64_t resident_cache_bytes = 0;
  std::filesystem::path proc_root = "/proc";
};

struct resource_sample {
  json document;
  bool rss_over_target = false;
  bool disk_cache_over_quota = false;
  bool budget_below_working_set = false;
  bool budget_below_minimum = false;
};

// io monitor.status se <proc_root>/self/status non e' leggibile o manca VmRSS / VmHWM.
resource_sample sample_resources(const monitor_inputs& inputs);

}  // namespace astro
