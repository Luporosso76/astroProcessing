#include "astro/resources.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <fstream>
#include <regex>
#include <sstream>

#include "astro/sqlite_dag_store.hpp"
#include "sqlite_internal.hpp"

namespace astro {

namespace {

constexpr const char* gate_origin = "default: misurato sul PC di sviluppo, docs/evidence/M2-gate.md";
constexpr const char* declared_origin = "default dichiarato, non misurato";
constexpr std::uint64_t mib = std::uint64_t{1} << 20;
constexpr std::uint64_t per_task_bytes_per_column = 2 * sizeof(double) + sizeof(std::uint16_t);

[[noreturn]] void invalid(const std::string& code, const char* message, stop_error::context_list context = {}) {
  throw stop_error(error_category::invalid_input, code, message, std::move(context));
}

[[noreturn]] void malformed(const char* message, const std::string& field = {}) {
  throw stop_error(error_category::invalid_input, "resources.setting_malformed", message,
                   field.empty() ? stop_error::context_list{} : stop_error::context_list{{"campo", field}});
}

constexpr std::uint64_t json_integer_max = (std::uint64_t{1} << 53) - 1;

void check_persistable(std::uint64_t value, const char* field) {
  if (value > json_integer_max) invalid(std::string("resources.") + field, "valore oltre 2^53 - 1, non persistibile");
}

void check_origin(const std::string& origin, const char* field) {
  if (origin.empty()) invalid(std::string("resources.") + field, "origine mancante");
}

void check_dir_quota(const std::optional<dir_quota>& v, const char* field) {
  if (!v) return;
  check_persistable(v->quota_bytes, field);
  if (v->dir.empty() || !v->dir.is_absolute() || v->quota_bytes == 0) {
    invalid(std::string("resources.") + field, "directory assoluta e quota > 0 obbligatorie",
            {{"dir", v->dir.string()}, {"quota_bytes", std::to_string(v->quota_bytes)}});
  }
}

void check_ratio(const budget_ratio& r, const char* field) {
  if (r.num == 0 || r.num > r.den || r.den > 1000) {
    invalid(std::string("resources.") + field, "rapporto non valido: 0 < num <= den <= 1000",
            {{"num", std::to_string(r.num)}, {"den", std::to_string(r.den)}});
  }
}

}  // namespace

resource_config::resource_config()
    : job_queue_capacity{::astro::job_queue_capacity, declared_origin},
      task_queue_capacity{4, declared_origin},
      report_margin_bytes{mib, declared_origin},
      fraction_normal{{3, 8}, gate_origin},
      fraction_pressure{{1, 4}, gate_origin},
      psi_threshold{budget_psi_threshold, gate_origin},
      pressure_poll_ms{1000, declared_origin} {}

void validate_resource_config(const resource_config& c) {
  if (c.rss_target_bytes && *c.rss_target_bytes == 0) invalid("resources.rss_target_bytes", "l'obiettivo RSS deve essere > 0");
  if (c.rss_target_bytes) check_persistable(*c.rss_target_bytes, "rss_target_bytes");
  check_persistable(c.job_queue_capacity.value, "job_queue_capacity");
  check_persistable(c.task_queue_capacity.value, "task_queue_capacity");
  check_persistable(c.report_margin_bytes.value, "report_margin_bytes");
  check_persistable(c.pressure_poll_ms.value, "pressure_poll_ms");
  check_dir_quota(c.scratch, "scratch");
  check_dir_quota(c.disk_cache, "disk_cache");
  if (c.cpu_set) {
    const std::vector<unsigned>& v = *c.cpu_set;
    if (v.empty() || !std::is_sorted(v.begin(), v.end()) || std::adjacent_find(v.begin(), v.end()) != v.end() ||
        v.back() >= CPU_SETSIZE) {
      invalid("resources.cpu_set", "insieme di CPU non vuoto, crescente e senza duplicati");
    }
  }
  if (c.worker_count && *c.worker_count < 1) invalid("resources.worker_count", "worker_count deve essere >= 1");
  if (c.tile_rows && *c.tile_rows < 1) invalid("resources.tile_rows", "tile_rows deve essere >= 1");
  if (c.job_queue_capacity.value < 1) invalid("resources.job_queue_capacity", "capacita' >= 1");
  check_origin(c.job_queue_capacity.origin, "job_queue_capacity");
  if (c.task_queue_capacity.value < 1) invalid("resources.task_queue_capacity", "capacita' >= 1");
  check_origin(c.task_queue_capacity.origin, "task_queue_capacity");
  if (c.report_margin_bytes.value < 1) invalid("resources.report_margin_bytes", "margine >= 1");
  check_origin(c.report_margin_bytes.origin, "report_margin_bytes");
  check_ratio(c.fraction_normal.value, "fraction_normal");
  check_origin(c.fraction_normal.origin, "fraction_normal");
  check_ratio(c.fraction_pressure.value, "fraction_pressure");
  check_origin(c.fraction_pressure.origin, "fraction_pressure");
  if (c.fraction_pressure.value.num * c.fraction_normal.value.den > c.fraction_normal.value.num * c.fraction_pressure.value.den) {
    invalid("resources.fraction_pressure", "la frazione sotto pressione supera quella normale");
  }
  const double psi = c.psi_threshold.value;
  if (!std::isfinite(psi) || psi <= 0.0 || psi > 100.0) invalid("resources.psi_threshold", "soglia PSI fuori da (0, 100]");
  check_origin(c.psi_threshold.origin, "psi_threshold");
  if (c.pressure_poll_ms.value < 1) invalid("resources.pressure_poll_ms", "intervallo >= 1 ms");
  check_origin(c.pressure_poll_ms.origin, "pressure_poll_ms");
}

void validate_cpu_set(const resource_config& c, const affinity_fn& affinity) {
  if (!c.cpu_set) return;
  cpu_set_t allowed;
  CPU_ZERO(&allowed);
  if (const int err = affinity(&allowed); err != 0) {
    sqlint::throw_errno(error_category::io, "sched.cpu_count", "sched_getaffinity fallita", err);
  }
  for (const unsigned cpu : *c.cpu_set) {
    if (cpu >= CPU_SETSIZE || !CPU_ISSET(cpu, &allowed)) {
      invalid("sched.cpu_set", "CPU fuori dall'affinita' del processo", {{"cpu", std::to_string(cpu)}});
    }
  }
  if (c.worker_count && *c.worker_count > c.cpu_set->size()) {
    invalid("sched.worker_count", "worker_count oltre le CPU dell'insieme",
            {{"worker_count", std::to_string(*c.worker_count)}, {"cpu_set", std::to_string(c.cpu_set->size())}});
  }
}

budget_tuning budget_tuning_of(const resource_config& c) {
  return budget_tuning{c.fraction_normal.value, c.fraction_pressure.value, c.psi_threshold.value};
}

scheduler_tuning scheduler_tuning_of(const resource_config& c) {
  scheduler_tuning t;
  t.max_queued_jobs = c.job_queue_capacity.value;
  if (c.cpu_set) t.cpu_set = *c.cpu_set;
  return t;
}

std::shared_ptr<pressure_gate> make_pressure_gate(const resource_config& c, const budget_sources& sources) {
  validate_resource_config(c);
  auto gate = std::make_shared<pressure_gate>();
  gate->sample = [sources] { return read_memory_pressure(sources); };
  gate->threshold = c.psi_threshold.value;
  gate->poll = std::chrono::milliseconds(static_cast<std::int64_t>(c.pressure_poll_ms.value));
  return gate;
}

// ---------------------------------------------------------------- JSON

namespace {

json opt_u64(const std::optional<std::uint64_t>& v) { return v ? json::uinteger(*v) : json::null(); }

json dir_quota_json(const std::optional<dir_quota>& v) {
  if (!v) return json::null();
  json o = json::object();
  o.set("dir", json::string(v->dir.string()));
  o.set("quota_bytes", json::uinteger(v->quota_bytes));
  return o;
}

json tunable_json(std::uint64_t value, const std::string& origin) {
  json o = json::object();
  o.set("value", json::uinteger(value));
  o.set("origin", json::string(origin));
  return o;
}

json ratio_json(const tunable<budget_ratio>& t) {
  json o = json::object();
  o.set("num", json::uinteger(t.value.num));
  o.set("den", json::uinteger(t.value.den));
  o.set("origin", json::string(t.origin));
  return o;
}

const json& need(const json& doc, const char* key) {
  const json* v = doc.find(key);
  if (v == nullptr) malformed("campo mancante", key);
  return *v;
}

void expect_keys(const json& doc, std::initializer_list<const char*> allowed, const char* what) {
  if (doc.type() != json::kind::object) malformed("oggetto atteso", what);
  for (const std::string& k : doc.keys()) {
    if (std::find_if(allowed.begin(), allowed.end(), [&](const char* a) { return k == a; }) == allowed.end()) {
      malformed("chiave sconosciuta", k);
    }
  }
  if (doc.keys().size() != allowed.size()) malformed("chiavi mancanti", what);
}

std::uint64_t as_u64(const json& v, const char* field) {
  if (v.type() != json::kind::integer || v.as_integer() < 0) malformed("intero non negativo atteso", field);
  return static_cast<std::uint64_t>(v.as_integer());
}

std::uint32_t as_u32(const json& v, const char* field) {
  const std::uint64_t value = as_u64(v, field);
  if (value > std::numeric_limits<std::uint32_t>::max()) malformed("valore oltre 32 bit", field);
  return static_cast<std::uint32_t>(value);
}

std::optional<std::uint64_t> opt_from(const json& v, const char* field) {
  if (v.type() == json::kind::null) return std::nullopt;
  return as_u64(v, field);
}

std::optional<dir_quota> dir_quota_from(const json& v, const char* field) {
  if (v.type() == json::kind::null) return std::nullopt;
  expect_keys(v, {"dir", "quota_bytes"}, field);
  const json& dir = need(v, "dir");
  if (dir.type() != json::kind::string) malformed("stringa attesa", field);
  return dir_quota{dir.as_string(), as_u64(need(v, "quota_bytes"), field)};
}

template <typename T>
tunable<T> tunable_from(const json& v, const char* field) {
  expect_keys(v, {"value", "origin"}, field);
  const json& origin = need(v, "origin");
  if (origin.type() != json::kind::string) malformed("stringa attesa", field);
  return tunable<T>{static_cast<T>(as_u64(need(v, "value"), field)), origin.as_string()};
}

tunable<budget_ratio> ratio_from(const json& v, const char* field) {
  expect_keys(v, {"num", "den", "origin"}, field);
  const json& origin = need(v, "origin");
  if (origin.type() != json::kind::string) malformed("stringa attesa", field);
  return tunable<budget_ratio>{{as_u64(need(v, "num"), field), as_u64(need(v, "den"), field)}, origin.as_string()};
}

}  // namespace

json resource_config_json(const resource_config& c) {
  json o = json::object();
  o.set("rss_target_bytes", opt_u64(c.rss_target_bytes));
  o.set("scratch", dir_quota_json(c.scratch));
  o.set("disk_cache", dir_quota_json(c.disk_cache));
  if (c.cpu_set) {
    json a = json::array();
    for (const unsigned cpu : *c.cpu_set) a.push(json::uinteger(cpu));
    o.set("cpu_set", std::move(a));
  } else {
    o.set("cpu_set", json::null());
  }
  o.set("worker_count", c.worker_count ? json::uinteger(*c.worker_count) : json::null());
  o.set("tile_rows", c.tile_rows ? json::uinteger(*c.tile_rows) : json::null());
  o.set("job_queue_capacity", tunable_json(c.job_queue_capacity.value, c.job_queue_capacity.origin));
  o.set("task_queue_capacity", tunable_json(c.task_queue_capacity.value, c.task_queue_capacity.origin));
  o.set("report_margin_bytes", tunable_json(c.report_margin_bytes.value, c.report_margin_bytes.origin));
  o.set("fraction_normal", ratio_json(c.fraction_normal));
  o.set("fraction_pressure", ratio_json(c.fraction_pressure));
  json psi = json::object();
  psi.set("value", json::number(c.psi_threshold.value));
  psi.set("origin", json::string(c.psi_threshold.origin));
  o.set("psi_threshold", std::move(psi));
  o.set("pressure_poll_ms", tunable_json(c.pressure_poll_ms.value, c.pressure_poll_ms.origin));
  json vram = json::object();
  vram.set("value", json::null());
  vram.set("reason", json::string(vram_null_reason));
  o.set("vram", std::move(vram));
  return o;
}

resource_config parse_resource_config(const json& doc) {
  expect_keys(doc, {"rss_target_bytes", "scratch", "disk_cache", "cpu_set", "worker_count", "tile_rows",
                    "job_queue_capacity", "task_queue_capacity", "report_margin_bytes", "fraction_normal",
                    "fraction_pressure", "psi_threshold", "pressure_poll_ms", "vram"},
              "resources.config");
  resource_config c;
  c.rss_target_bytes = opt_from(need(doc, "rss_target_bytes"), "rss_target_bytes");
  c.scratch = dir_quota_from(need(doc, "scratch"), "scratch");
  c.disk_cache = dir_quota_from(need(doc, "disk_cache"), "disk_cache");
  const json& cpus = need(doc, "cpu_set");
  if (cpus.type() == json::kind::array) {
    std::vector<unsigned> v;
    for (const json& item : cpus.items()) v.push_back(as_u32(item, "cpu_set"));
    c.cpu_set = std::move(v);
  } else if (cpus.type() != json::kind::null) {
    malformed("array o null atteso", "cpu_set");
  }
  if (need(doc, "worker_count").type() != json::kind::null) c.worker_count = as_u32(need(doc, "worker_count"), "worker_count");
  if (need(doc, "tile_rows").type() != json::kind::null) c.tile_rows = as_u32(need(doc, "tile_rows"), "tile_rows");
  c.job_queue_capacity = tunable_from<std::size_t>(need(doc, "job_queue_capacity"), "job_queue_capacity");
  c.task_queue_capacity = tunable_from<std::size_t>(need(doc, "task_queue_capacity"), "task_queue_capacity");
  c.report_margin_bytes = tunable_from<std::uint64_t>(need(doc, "report_margin_bytes"), "report_margin_bytes");
  c.fraction_normal = ratio_from(need(doc, "fraction_normal"), "fraction_normal");
  c.fraction_pressure = ratio_from(need(doc, "fraction_pressure"), "fraction_pressure");
  const json& psi = need(doc, "psi_threshold");
  expect_keys(psi, {"value", "origin"}, "psi_threshold");
  const json& pv = need(psi, "value");
  const json& po = need(psi, "origin");
  if ((pv.type() != json::kind::integer && pv.type() != json::kind::number) || po.type() != json::kind::string) {
    malformed("valore o origine non validi", "psi_threshold");
  }
  c.psi_threshold = {pv.type() == json::kind::integer ? static_cast<double>(pv.as_integer()) : pv.as_number(),
                     po.as_string()};
  c.pressure_poll_ms = tunable_from<std::uint64_t>(need(doc, "pressure_poll_ms"), "pressure_poll_ms");
  const json& vram = need(doc, "vram");
  expect_keys(vram, {"value", "reason"}, "vram");
  if (need(vram, "value").type() != json::kind::null || need(vram, "reason").type() != json::kind::string ||
      need(vram, "reason").as_string() != vram_null_reason) {
    malformed("la VRAM deve valere null con il motivo dichiarato", "vram");
  }
  validate_resource_config(c);
  return c;
}

void save_resource_config(sqlite_dag_store& store, const resource_config& c) {
  validate_resource_config(c);
  store.put_setting(resource_config_setting_key, canonical_json(resource_config_json(c)));
}

std::optional<resource_config> load_resource_config(const sqlite_dag_store& store) {
  const std::optional<std::string> stored = store.get_setting(resource_config_setting_key);
  if (!stored) return std::nullopt;
  return parse_resource_config(parse_json(*stored));
}

// ---------------------------------------------------------------- pianificatore

namespace {

__extension__ typedef unsigned __int128 u128;

std::uint64_t narrow(u128 v) {
  if (v > ~std::uint64_t{0}) invalid("resources.planner_input", "grandezza non rappresentabile in 64 bit");
  return static_cast<std::uint64_t>(v);
}

std::uint64_t ceil_div(std::uint64_t a, std::uint64_t b) { return a / b + (a % b != 0); }

}  // namespace

plan_bounds compute_plan_bounds(const planner_input& in, const resource_config& config) {
  validate_resource_config(config);
  if (in.width == 0 || in.height == 0 || in.cpus == 0 || in.out_sample_bytes == 0) {
    invalid("resources.planner_input", "larghezza, altezza, CPU e byte per campione devono essere > 0");
  }
  if (config.worker_count && *config.worker_count > in.cpus) {
    invalid("sched.worker_count", "worker_count oltre le CPU utilizzabili",
            {{"worker_count", std::to_string(*config.worker_count)}, {"cpu_count", std::to_string(in.cpus)}});
  }
  plan_bounds b;
  const u128 samples = static_cast<u128>(in.width) * in.height;
  const u128 out_buffers = samples * (2 * static_cast<u128>(in.out_sample_bytes) + sizeof(std::uint16_t));
  const u128 shared = static_cast<u128>(in.input_peak_bytes) + out_buffers + (out_buffers + in.header_bound_bytes) +
                      samples * sizeof(double) + config.report_margin_bytes.value;
  b.shared_bytes = narrow(shared);
  b.per_task_bytes = narrow(static_cast<u128>(in.width) * per_task_bytes_per_column);
  b.minimum_feasible_bytes = narrow(static_cast<u128>(b.shared_bytes) + b.per_task_bytes);
  b.tile_rows = config.tile_rows ? *config.tile_rows
                                 : static_cast<std::uint32_t>(std::min<std::uint64_t>(
                                       64, std::max<std::uint64_t>(1, ceil_div(in.height, 4 * in.cpus))));
  b.task_count = ceil_div(in.height, b.tile_rows);
  const std::uint64_t parallel = std::min<std::uint64_t>(in.cpus, b.task_count);
  b.working_set_bytes = narrow(static_cast<u128>(b.shared_bytes) + static_cast<u128>(parallel) * b.per_task_bytes);
  return b;
}

namespace {

// Modello della residenza a blocchi (ADR-06, estensione R3.2): stesso tile della residenza intera.
plan_bounds block_bounds(const planner_input& in, const resource_config& config, std::uint32_t tile_rows) {
  if (in.block_raw_bytes_per_row == 0 || in.block_band_bytes_per_row == 0 || in.block_regions == 0 ||
      (in.normalization_channels != 1 && in.normalization_channels != 4)) {
    invalid("resources.planner_input", "ingressi della residenza a blocchi mancanti o non validi");
  }
  plan_bounds b;
  b.tile_rows = tile_rows;
  b.task_count = ceil_div(in.height, tile_rows);
  const u128 t = tile_rows, w = in.width, e = in.out_sample_bytes;
  const u128 in_band = t * (static_cast<u128>(in.block_raw_bytes_per_row) + in.block_band_bytes_per_row);
  const u128 per_task = in_band + t * w * (2 * e + 2) + w * per_task_bytes_per_column;
  const u128 template_bytes = (u128{1} << 20) + 2880 +
                              2 * (static_cast<u128>(in.header_bound_bytes) + 3 * 2880 * ceil_div(8 * in.width, 2880)) +
                              w * per_task_bytes_per_column;
  const u128 shared = in_band + 32 * static_cast<u128>(b.task_count) * in.block_regions +
                      static_cast<u128>(in.normalization_channels) * 4 * 256 * 8 + template_bytes +
                      config.report_margin_bytes.value;
  b.shared_bytes = narrow(shared);
  b.per_task_bytes = narrow(per_task);
  b.minimum_feasible_bytes = narrow(static_cast<u128>(b.shared_bytes) + b.per_task_bytes);
  const std::uint64_t parallel = std::min<std::uint64_t>(in.cpus, b.task_count);
  b.working_set_bytes = narrow(static_cast<u128>(b.shared_bytes) + static_cast<u128>(parallel) * b.per_task_bytes);
  return b;
}

}  // namespace

resource_plan plan_resources(const planner_input& in, const resource_config& config) {
  const plan_bounds whole = compute_plan_bounds(in, config);
  const bool blocks = in.checkpoint || in.budget_bytes < whole.minimum_feasible_bytes;
  if (blocks && in.checkpoint && !config.scratch) {
    invalid("calib.checkpoint_scratch", "il checkpoint del job richiede lo scratch nella configurazione");
  }
  const plan_bounds b = blocks && config.scratch ? block_bounds(in, config, whole.tile_rows) : whole;
  const char* residency = blocks && config.scratch ? "blocks" : "whole";
  const auto exceeds = [&](std::uint64_t peak) {
    stop_error e(error_category::budget, "sched.job_exceeds_budget",
                 "il picco del job supera il budget", {{"picco", std::to_string(peak)},
                 {"budget_bytes", std::to_string(in.budget_bytes)},
                 {"minimum_feasible_bytes", std::to_string(b.minimum_feasible_bytes)},
                 {"shared_bytes", std::to_string(b.shared_bytes)},
                 {"per_task_bytes", std::to_string(b.per_task_bytes)},
                 {"residenza", residency}});
    if (blocks && !config.scratch) e.add_context("residenza_blocchi", "scratch non dichiarato");
    return e;
  };
  if (in.budget_bytes < b.minimum_feasible_bytes) throw exceeds(b.minimum_feasible_bytes);
  resource_plan p;
  p.tile_rows = b.tile_rows;
  p.task_count = b.task_count;
  p.shared_bytes = b.shared_bytes;
  p.per_task_bytes = b.per_task_bytes;
  p.minimum_feasible_bytes = b.minimum_feasible_bytes;
  p.whole_minimum_bytes = whole.minimum_feasible_bytes;
  p.working_set_bytes = whole.working_set_bytes;
  p.task_queue_capacity = config.task_queue_capacity.value;
  if (config.worker_count) {
    p.worker_count = *config.worker_count;
    p.peak_bytes = narrow(static_cast<u128>(b.shared_bytes) + static_cast<u128>(p.worker_count) * b.per_task_bytes);
    if (p.peak_bytes > in.budget_bytes) throw exceeds(p.peak_bytes);
    p.user_fixed.push_back("worker_count");
  } else {
    const std::uint64_t fit = (in.budget_bytes - b.shared_bytes) / b.per_task_bytes;
    p.worker_count = static_cast<unsigned>(std::min<std::uint64_t>({in.cpus, b.task_count, fit}));
    p.peak_bytes = b.shared_bytes + std::uint64_t{p.worker_count} * b.per_task_bytes;
  }
  if (config.tile_rows) p.user_fixed.push_back("tile_rows");
  p.budget_below_working_set = in.budget_bytes < whole.working_set_bytes;
  if (blocks) {
    p.frame_residency = "blocks";
    p.master_residency = "blocks";
    p.staging_bytes = narrow(static_cast<u128>(in.width) * in.height * (2 * static_cast<u128>(in.out_sample_bytes) + 2));
    if (p.staging_bytes > config.scratch->quota_bytes) {
      throw stop_error(error_category::budget, "spill.quota_exceeded", "lo staging supera la quota dello scratch",
                       {{"staging_bytes", std::to_string(p.staging_bytes)},
                        {"quota", std::to_string(config.scratch->quota_bytes)}});
    }
    p.spill = spill_config{config.scratch->dir, config.scratch->quota_bytes};
  }
  return p;
}

json resource_plan_json(const resource_plan& p) {
  json o = json::object();
  o.set("worker_count", json::uinteger(p.worker_count));
  o.set("tile_rows", json::uinteger(p.tile_rows));
  o.set("task_count", json::uinteger(p.task_count));
  o.set("task_queue_capacity", json::uinteger(p.task_queue_capacity));
  o.set("shared_bytes", json::uinteger(p.shared_bytes));
  o.set("per_task_bytes", json::uinteger(p.per_task_bytes));
  o.set("peak_bytes", json::uinteger(p.peak_bytes));
  o.set("working_set_bytes", json::uinteger(p.working_set_bytes));
  o.set("minimum_feasible_bytes", json::uinteger(p.minimum_feasible_bytes));
  o.set("frame_residency", json::string(p.frame_residency));
  o.set("master_residency", json::string(p.master_residency));
  o.set("budget_below_working_set", json::boolean(p.budget_below_working_set));
  o.set("whole_minimum_bytes", json::uinteger(p.whole_minimum_bytes));
  o.set("staging_bytes", json::uinteger(p.staging_bytes));
  json fixed = json::array();
  for (const std::string& f : p.user_fixed) fixed.push(json::string(f));
  o.set("user_fixed", std::move(fixed));
  return o;
}

// ---------------------------------------------------------------- cache su disco

namespace {

class dir_handle {
 public:
  explicit dir_handle(const std::filesystem::path& dir) {
    fd_ = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd_ < 0) {
      sqlint::throw_errno(error_category::io, "cache.dir", "directory della cache non apribile (assente o symlink)",
                          errno, {{"dir", dir.string()}});
    }
  }
  ~dir_handle() { ::close(fd_); }
  dir_handle(const dir_handle&) = delete;
  dir_handle& operator=(const dir_handle&) = delete;
  int fd() const { return fd_; }

 private:
  int fd_ = -1;
};

[[noreturn]] void unsafe(const std::string& name, const char* why) {
  throw stop_error(error_category::security, "cache.entry_unsafe", "voce della cache non ammessa",
                   {{"voce", name}, {"motivo", why}});
}

struct stat checked_stat(int dirfd, const std::string& name) {
  struct stat st;
  if (::fstatat(dirfd, name.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0) {
    if (errno == ENOENT) {
      throw stop_error(error_category::metadata_missing, "cache.entry_missing", "voce della cache assente",
                       {{"voce", name}});
    }
    sqlint::throw_errno(error_category::io, "cache.dir", "stat della voce fallita", errno, {{"voce", name}});
  }
  if (S_ISLNK(st.st_mode)) unsafe(name, "symlink");
  if (!S_ISREG(st.st_mode)) unsafe(name, "non regolare");
  if (st.st_nlink != 1) unsafe(name, "st_nlink diverso da 1");
  return st;
}

}  // namespace

disk_cache_usage measure_disk_cache(const std::filesystem::path& dir) {
  const dir_handle handle(dir);
  const int scan_fd = ::dup(handle.fd());
  if (scan_fd < 0) sqlint::throw_errno(error_category::io, "cache.dir", "dup fallita", errno);
  DIR* d = ::fdopendir(scan_fd);
  if (d == nullptr) {
    const int err = errno;
    ::close(scan_fd);
    sqlint::throw_errno(error_category::io, "cache.dir", "fdopendir fallita", err);
  }
  std::vector<std::string> names;
  errno = 0;
  while (const dirent* e = ::readdir(d)) {
    const std::string name = e->d_name;
    if (name != "." && name != "..") names.push_back(name);
  }
  const int read_err = errno;
  ::closedir(d);
  if (read_err != 0) sqlint::throw_errno(error_category::io, "cache.dir", "lettura della directory fallita", read_err);
  disk_cache_usage usage;
  for (const std::string& name : names) {
    const struct stat st = checked_stat(handle.fd(), name);
    usage.bytes += static_cast<std::uint64_t>(st.st_size);
    ++usage.entries;
  }
  return usage;
}

void enforce_disk_cache_quota(const std::filesystem::path& dir, std::uint64_t quota_bytes) {
  const disk_cache_usage usage = measure_disk_cache(dir);
  if (usage.bytes > quota_bytes) {
    throw stop_error(error_category::budget, "cache.quota_exceeded", "la cache su disco supera la quota",
                     {{"bytes", std::to_string(usage.bytes)}, {"quota_bytes", std::to_string(quota_bytes)}});
  }
}

void remove_disk_cache_entries(const std::filesystem::path& dir, const std::vector<std::string>& names) {
  static const std::regex allowed("[A-Za-z0-9._-]{1,128}");
  for (const std::string& name : names) {
    if (!std::regex_match(name, allowed) || name == "." || name == "..") {
      invalid("cache.entry_name", "nome di voce non ammesso", {{"voce", name}});
    }
  }
  const dir_handle handle(dir);
  for (const std::string& name : names) (void)checked_stat(handle.fd(), name);
  for (const std::string& name : names) {
    if (::unlinkat(handle.fd(), name.c_str(), 0) != 0) {
      sqlint::throw_errno(error_category::io, "cache.dir", "rimozione della voce fallita", errno, {{"voce", name}});
    }
  }
}

// ---------------------------------------------------------------- monitor

namespace {

std::uint64_t status_bytes(const std::string& text, const char* field) {
  const std::string key = std::string(field) + ":";
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    if (line.compare(0, key.size(), key) != 0) continue;
    std::istringstream rest(line.substr(key.size()));
    unsigned long long value = 0;
    std::string unit;
    if (rest >> value >> unit && unit == "kB") return value * 1024;
    break;
  }
  throw stop_error(error_category::io, "monitor.status", "campo assente o non interpretabile in /proc/self/status",
                   {{"campo", field}});
}

}  // namespace

resource_sample sample_resources(const monitor_inputs& in) {
  ASTRO_ASSERT(in.pool != nullptr && in.budget != nullptr);
  ASTRO_ASSERT(in.planner == nullptr || in.config != nullptr);
  const std::filesystem::path status_path = in.proc_root / "self" / "status";
  std::ifstream status(status_path);
  if (!status) {
    throw stop_error(error_category::io, "monitor.status", "impossibile leggere /proc/self/status",
                     {{"percorso", status_path.string()}});
  }
  const std::string text((std::istreambuf_iterator<char>(status)), std::istreambuf_iterator<char>());
  resource_sample s;
  json d = json::object();
  json pool = json::object();
  pool.set("budget_bytes", json::uinteger(in.pool->budget_bytes()));
  pool.set("reserved_bytes", json::uinteger(in.pool->reserved_bytes()));
  pool.set("peak_reserved_bytes", json::uinteger(in.pool->peak_reserved_bytes()));
  pool.set("waiting_count", json::uinteger(in.pool->waiting_count()));
  d.set("pool", std::move(pool));
  d.set("estimated_peak_bytes", in.plan ? json::uinteger(in.plan->peak_bytes) : json::null());
  json rss = json::object();
  const std::uint64_t rss_bytes = status_bytes(text, "VmRSS");
  rss.set("rss_bytes", json::uinteger(rss_bytes));
  rss.set("rss_peak_bytes", json::uinteger(status_bytes(text, "VmHWM")));
  const std::optional<std::uint64_t> target = in.config ? in.config->rss_target_bytes : std::nullopt;
  rss.set("target_bytes", target ? json::uinteger(*target) : json::null());
  s.rss_over_target = target && rss_bytes > *target;
  rss.set("over_target", json::boolean(s.rss_over_target));
  d.set("rss", std::move(rss));
  d.set("resident_cache_bytes", json::uinteger(in.resident_cache_bytes));
  if (in.scratch != nullptr) {
    json sc = json::object();
    sc.set("used_bytes", json::uinteger(in.scratch->used_bytes()));
    sc.set("peak_bytes", json::uinteger(in.scratch->peak_bytes()));
    sc.set("quota_bytes", json::uinteger(in.scratch->quota_bytes()));
    d.set("scratch", std::move(sc));
  } else {
    d.set("scratch", json::null());
  }
  if (in.pressure != nullptr) {
    json pr = json::object();
    pr.set("waiting_jobs", json::uinteger(in.pressure->waiting_jobs.load()));
    pr.set("threshold", json::number(in.pressure->threshold));
    d.set("pressure", std::move(pr));
  } else {
    d.set("pressure", json::null());
  }
  if (in.config != nullptr && in.config->disk_cache) {
    const disk_cache_usage usage = measure_disk_cache(in.config->disk_cache->dir);
    s.disk_cache_over_quota = usage.bytes > in.config->disk_cache->quota_bytes;
    json dc = json::object();
    dc.set("bytes", json::uinteger(usage.bytes));
    dc.set("entries", json::uinteger(usage.entries));
    dc.set("quota_bytes", json::uinteger(in.config->disk_cache->quota_bytes));
    dc.set("over_quota", json::boolean(s.disk_cache_over_quota));
    d.set("disk_cache", std::move(dc));
  } else {
    d.set("disk_cache", json::null());
  }
  json vram = json::object();
  vram.set("value", json::null());
  vram.set("reason", json::string(vram_null_reason));
  d.set("vram", std::move(vram));
  json limit = json::object();
  limit.set("budget_bytes", json::uinteger(in.budget->budget_bytes));
  limit.set("mode", json::string(in.budget->mode == budget_mode::automatic ? "automatic" : "manual"));
  limit.set("origin", json::string(in.budget->mode == budget_mode::manual && in.budget->manual
                                       ? "manual:" + to_string(*in.budget->manual)
                                       : "automatic:fraction=" + std::to_string(in.budget->fraction)));
  d.set("limit", std::move(limit));
  if (in.planner != nullptr) {
    const plan_bounds b = compute_plan_bounds(*in.planner, *in.config);
    s.budget_below_minimum = in.pool->budget_bytes() < b.minimum_feasible_bytes;
    s.budget_below_working_set = in.pool->budget_bytes() < b.working_set_bytes;
  }
  json signals = json::object();
  signals.set("rss_over_target", json::boolean(s.rss_over_target));
  signals.set("disk_cache_over_quota", json::boolean(s.disk_cache_over_quota));
  signals.set("budget_below_working_set", json::boolean(s.budget_below_working_set));
  signals.set("budget_below_minimum", json::boolean(s.budget_below_minimum));
  d.set("signals", std::move(signals));
  s.document = std::move(d);
  return s;
}

}  // namespace astro
