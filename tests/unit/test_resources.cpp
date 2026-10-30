// R3.1: configurazione delle risorse, pianificatore, monitor, cache su disco (ADR-06, estensione R3.1).
// T-M2R3-02, T-M2R3-03, T-M2R3-04 (parte pianificatore), T-M2R3-08, T-M2R3-09.
#include <gtest/gtest.h>
#include <sched.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <optional>
#include <cerrno>
#include <cstring>
#include <cmath>
#include <fstream>
#include <thread>
#include <vector>
#include <mutex>
#include <set>
#include <tuple>

#include "astro/resources.hpp"
#include "astro/sqlite_dag_store.hpp"
#include "m2d_support.hpp"

namespace {

using namespace m2d;

// Caso della tabella del contratto: W = 1000, H = 1000, E = 4, P = 4 000 000, HB = 100 000, M = 1 048 576.
// shared = 33 148 576, per_task = 18 000, minimo = 33 166 576 (calcolati a mano nel contratto).
constexpr std::uint64_t too_big = std::uint64_t{1} << 53;  // primo intero non persistibile in JSON
constexpr std::uint64_t shared_hand = 33'148'576, per_task_hand = 18'000, minimum_hand = 33'166'576;

planner_input table_input(std::uint64_t budget, std::size_t cpus) {
  planner_input in;
  in.budget_bytes = budget;
  in.cpus = cpus;
  in.width = 1000;
  in.height = 1000;
  in.out_sample_bytes = 4;
  in.input_peak_bytes = 4'000'000;
  in.header_bound_bytes = 100'000;
  return in;
}

std::string context_value(const stop_error& e, const std::string& key) {
  for (const auto& [k, v] : e.context()) {
    if (k == key) return v;
  }
  return "<assente>";
}

// ---------------------------------------------------------------- T-M2R3-02

TEST(Resources, T_M2R3_02_planner_matches_the_hand_computed_table) {
  const resource_config cfg;
  {
    const resource_plan p = plan_resources(table_input(104'857'600, 4), cfg);
    EXPECT_EQ(p.tile_rows, 63u);
    EXPECT_EQ(p.task_count, 16u);
    EXPECT_EQ(p.worker_count, 4u);
    EXPECT_EQ(p.shared_bytes, shared_hand);
    EXPECT_EQ(p.per_task_bytes, per_task_hand);
    EXPECT_EQ(p.working_set_bytes, 33'220'576u);
    EXPECT_EQ(p.peak_bytes, 33'220'576u);
    EXPECT_EQ(p.minimum_feasible_bytes, minimum_hand);
    EXPECT_FALSE(p.budget_below_working_set);
    EXPECT_TRUE(p.user_fixed.empty());
    EXPECT_EQ(p.task_queue_capacity, 4u);
    EXPECT_EQ(p.frame_residency, "whole");
  }
  {
    const resource_plan p = plan_resources(table_input(33'220'575, 16), cfg);
    EXPECT_EQ(p.tile_rows, 16u);
    EXPECT_EQ(p.task_count, 63u);
    EXPECT_EQ(p.worker_count, 3u);
    EXPECT_EQ(p.working_set_bytes, 33'436'576u);
    EXPECT_TRUE(p.budget_below_working_set);
  }
  {
    resource_config c = cfg;
    c.worker_count = 8;
    try {
      (void)plan_resources(table_input(33'220'575, 16), c);
      ADD_FAILURE() << "stop attesto";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.category(), error_category::budget);
      EXPECT_EQ(e.code(), "sched.job_exceeds_budget");
      EXPECT_EQ(context_value(e, "picco"), "33292576");
      EXPECT_EQ(context_value(e, "minimum_feasible_bytes"), "33166576");
    }
  }
  {
    resource_config c = cfg;
    c.tile_rows = 10;
    const resource_plan p = plan_resources(table_input(104'857'600, 2), c);
    EXPECT_EQ(p.tile_rows, 10u);
    EXPECT_EQ(p.task_count, 100u);
    EXPECT_EQ(p.worker_count, 2u);
    EXPECT_EQ(p.user_fixed, std::vector<std::string>{"tile_rows"});
  }
  EXPECT_EQ(plan_resources(table_input(minimum_hand, 16), cfg).worker_count, 1u);
  EXPECT_STOP(plan_resources(table_input(minimum_hand - 1, 16), cfg), error_category::budget,
              "sched.job_exceeds_budget");
}

TEST(Resources, T_M2R3_02_explicit_values_are_never_changed_and_a_fitting_explicit_plan_is_kept) {
  resource_config c;
  c.worker_count = 2;
  c.tile_rows = 7;
  const resource_plan p = plan_resources(table_input(104'857'600, 16), c);
  EXPECT_EQ(p.worker_count, 2u);
  EXPECT_EQ(p.tile_rows, 7u);
  EXPECT_EQ(p.user_fixed, (std::vector<std::string>{"worker_count", "tile_rows"}));
  EXPECT_EQ(p.peak_bytes, shared_hand + 2 * per_task_hand);
  c.worker_count = 17;
  EXPECT_STOP(plan_resources(table_input(104'857'600, 16), c), error_category::invalid_input, "sched.worker_count");
}

TEST(Resources, T_M2R3_02_degenerate_inputs_are_rejected) {
  const resource_config cfg;
  planner_input in = table_input(104'857'600, 4);
  in.width = 0;
  EXPECT_STOP(plan_resources(in, cfg), error_category::invalid_input, "resources.planner_input");
  in = table_input(104'857'600, 0);
  EXPECT_STOP(plan_resources(in, cfg), error_category::invalid_input, "resources.planner_input");
  in = table_input(104'857'600, 4);
  in.input_peak_bytes = ~std::uint64_t{0};
  EXPECT_STOP(plan_resources(in, cfg), error_category::invalid_input, "resources.planner_input");
}

// ---------------------------------------------------------------- T-M2R3-04 (formula del minimo)

TEST(Resources, T_M2R3_04_minimum_feasible_is_in_the_error_context_and_equals_the_formula) {
  const resource_config cfg;
  for (const std::uint64_t width : {10u, 1000u, 4096u}) {
    planner_input in = table_input(0, 4);
    in.width = static_cast<std::uint32_t>(width);
    in.height = 100;
    const std::uint64_t s = width * 100;
    const std::uint64_t out = s * (2 * 4 + 2);
    const std::uint64_t expected = in.input_peak_bytes + out + (out + in.header_bound_bytes) + 8 * s + 1'048'576 + 18 * width;
    EXPECT_EQ(compute_plan_bounds(in, cfg).minimum_feasible_bytes, expected);
    in.budget_bytes = expected;
    EXPECT_EQ(plan_resources(in, cfg).worker_count, 1u);
    in.budget_bytes = expected - 1;
    try {
      (void)plan_resources(in, cfg);
      ADD_FAILURE() << "stop attesto";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.code(), "sched.job_exceeds_budget");
      EXPECT_EQ(context_value(e, "minimum_feasible_bytes"), std::to_string(expected));
      EXPECT_EQ(context_value(e, "budget_bytes"), std::to_string(expected - 1));
    }
  }
}

// ---------------------------------------------------------------- T-M2R3-08

TEST(Resources, T_M2R3_08_defaults_carry_declared_origins) {
  const resource_config c;
  EXPECT_EQ(c.fraction_normal.value, (budget_ratio{3, 8}));
  EXPECT_EQ(c.fraction_pressure.value, (budget_ratio{1, 4}));
  EXPECT_EQ(c.psi_threshold.value, 10.0);
  EXPECT_EQ(c.job_queue_capacity.value, 16u);
  EXPECT_EQ(c.task_queue_capacity.value, 4u);
  EXPECT_EQ(c.report_margin_bytes.value, 1'048'576u);
  EXPECT_NE(c.fraction_normal.origin.find("M2-gate.md"), std::string::npos);
  EXPECT_NE(c.psi_threshold.origin.find("M2-gate.md"), std::string::npos);
  EXPECT_NE(c.job_queue_capacity.origin.find("non misurato"), std::string::npos);
  for (const std::string* o : {&c.job_queue_capacity.origin, &c.task_queue_capacity.origin, &c.report_margin_bytes.origin,
                               &c.fraction_normal.origin, &c.fraction_pressure.origin, &c.psi_threshold.origin}) {
    EXPECT_FALSE(o->empty());
  }
}

TEST(Resources, T_M2R3_08_fractions_and_psi_threshold_change_the_automatic_budget) {
  fake_system sys("res_budget");
  sys.meminfo(8 * 1024 * 1024, 4 * 1024 * 1024);  // 4 GiB disponibili
  resource_config c;
  EXPECT_EQ(compute_automatic_budget(sys.sources(), budget_tuning_of(c)).budget_bytes, 1536 * MiB);  // 3/8
  c.fraction_normal = {{1, 2}, "configurato"};
  EXPECT_EQ(compute_automatic_budget(sys.sources(), budget_tuning_of(c)).budget_bytes, 2048 * MiB);
  EXPECT_EQ(compute_automatic_budget(sys.sources(), budget_tuning_of(c)).fraction, 0.5);
  sys.psi("some avg10=12.00 avg60=1.00 avg300=0.50 total=12345\n");
  // 12 >= 10: frazione sotto pressione 1/4.
  EXPECT_EQ(compute_automatic_budget(sys.sources(), budget_tuning_of(c)).budget_bytes, 1024 * MiB);
  c.psi_threshold = {13.0, "configurato"};  // 12 < 13: frazione normale
  EXPECT_EQ(compute_automatic_budget(sys.sources(), budget_tuning_of(c)).budget_bytes, 2048 * MiB);
  c.psi_threshold = {12.0, "configurato"};  // soglia inclusa
  EXPECT_EQ(compute_automatic_budget(sys.sources(), budget_tuning_of(c)).budget_bytes, 1024 * MiB);
  c.fraction_pressure = {{1, 8}, "configurato"};
  EXPECT_EQ(compute_automatic_budget(sys.sources(), budget_tuning_of(c)).budget_bytes, 512 * MiB);
  // Senza argomento valgono i default.
  EXPECT_EQ(compute_automatic_budget(sys.sources()).budget_bytes, 1024 * MiB);
}

TEST(Resources, T_M2R3_08_job_queue_capacity_stops_job_n_plus_1) {
  for (const std::size_t capacity : {std::size_t{1}, std::size_t{3}}) {
    resource_config c;
    c.job_queue_capacity = {capacity, "configurato"};
    memory_pool pool(8 * MiB);
    reservation hold = pool.reserve(8 * MiB, "holder");
    job_scheduler scheduler(pool, system_affinity, scheduler_tuning_of(c));
    EXPECT_EQ(scheduler_tuning_of(c).max_queued_jobs, capacity);
    cancel_token cancel;
    std::vector<std::thread> waiting;
    std::vector<std::string> outcomes(capacity);
    const node_spec node = test_node("queue");
    job_plan plan;
    plan.shared_bytes = MiB;
    plan.per_task_bytes = MiB;
    for (std::size_t i = 0; i < capacity; ++i) {
      waiting.emplace_back([&, i] {
        try {
          job_spec spec(node, 1, [](task_context&) { return std::string(); });
          scheduler.run(plan, spec, cancel);
          outcomes[i] = "completed";
        } catch (const stop_error& e) {
          outcomes[i] = e.code();
        }
      });
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (pool.waiting_count() < capacity && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (pool.waiting_count() != capacity) {
      cancel.request();
      hold.release();
      for (std::thread& t : waiting) t.join();
      FAIL() << "i job in coda non hanno raggiunto la capacita'";
    }
    job_spec spec(node, 1, [](task_context&) { return std::string(); });
    // Il token e' gia' richiesto: se la coda non fosse piena il job attenderebbe e si fermerebbe con sched.cancelled
    // invece di bloccare il test.
    cancel_token probe_cancel;
    probe_cancel.request();
    EXPECT_STOP(scheduler.run(plan, spec, probe_cancel), error_category::budget, "sched.queue_full");
    cancel.request();
    hold.release();
    for (std::thread& t : waiting) t.join();
    // Dopo il cancel un job in coda o si ferma con sched.cancelled o, se ha gia' preso il pool, termina.
    for (const std::string& o : outcomes) EXPECT_TRUE(o == "completed" || o == "sched.cancelled") << o;
  }
}

TEST(Resources, T_M2R3_08_task_queue_capacity_and_margin_change_the_plan) {
  resource_config c;
  c.task_queue_capacity = {9, "configurato"};
  EXPECT_EQ(plan_resources(table_input(104'857'600, 4), c).task_queue_capacity, 9u);
  c = resource_config{};
  c.report_margin_bytes = {2'097'152, "configurato"};
  EXPECT_EQ(compute_plan_bounds(table_input(0, 4), c).shared_bytes, shared_hand + 1'048'576);
}

TEST(Resources, T_M2R3_08_domains_are_rejected) {
  const auto bad = [](auto&& mutate, const char* code) {
    resource_config c;
    mutate(c);
    EXPECT_STOP(validate_resource_config(c), error_category::invalid_input, code);
  };
  bad([](resource_config& c) { c.rss_target_bytes = 0; }, "resources.rss_target_bytes");
  bad([](resource_config& c) { c.scratch = dir_quota{"relativa", 1}; }, "resources.scratch");
  bad([](resource_config& c) { c.scratch = dir_quota{"/tmp/x", 0}; }, "resources.scratch");
  bad([](resource_config& c) { c.disk_cache = dir_quota{"", 1}; }, "resources.disk_cache");
  bad([](resource_config& c) { c.cpu_set = std::vector<unsigned>{}; }, "resources.cpu_set");
  bad([](resource_config& c) { c.cpu_set = std::vector<unsigned>{2, 1}; }, "resources.cpu_set");
  bad([](resource_config& c) { c.cpu_set = std::vector<unsigned>{1, 1}; }, "resources.cpu_set");
  bad([](resource_config& c) { c.worker_count = 0; }, "resources.worker_count");
  bad([](resource_config& c) { c.tile_rows = 0; }, "resources.tile_rows");
  bad([](resource_config& c) { c.job_queue_capacity.value = 0; }, "resources.job_queue_capacity");
  bad([](resource_config& c) { c.task_queue_capacity.value = 0; }, "resources.task_queue_capacity");
  bad([](resource_config& c) { c.report_margin_bytes.value = 0; }, "resources.report_margin_bytes");
  bad([](resource_config& c) { c.fraction_normal.value = {0, 8}; }, "resources.fraction_normal");
  bad([](resource_config& c) { c.fraction_normal.value = {9, 8}; }, "resources.fraction_normal");
  bad([](resource_config& c) { c.fraction_normal.value = {1, 1001}; }, "resources.fraction_normal");
  bad([](resource_config& c) { c.fraction_pressure.value = {1, 2}; }, "resources.fraction_pressure");  // > normale
  bad([](resource_config& c) { c.psi_threshold.value = 0; }, "resources.psi_threshold");
  bad([](resource_config& c) { c.psi_threshold.value = 100.5; }, "resources.psi_threshold");
  bad([](resource_config& c) { c.psi_threshold.value = std::nan(""); }, "resources.psi_threshold");
  bad([](resource_config& c) { c.rss_target_bytes = too_big; }, "resources.rss_target_bytes");
  bad([](resource_config& c) { c.scratch = dir_quota{"/tmp/x", too_big}; }, "resources.scratch");
  bad([](resource_config& c) { c.disk_cache = dir_quota{"/tmp/x", too_big}; }, "resources.disk_cache");
  bad([](resource_config& c) { c.job_queue_capacity.value = too_big; }, "resources.job_queue_capacity");
  bad([](resource_config& c) { c.task_queue_capacity.value = too_big; }, "resources.task_queue_capacity");
  bad([](resource_config& c) { c.report_margin_bytes.value = too_big; }, "resources.report_margin_bytes");
  bad([](resource_config& c) { c.cpu_set = std::vector<unsigned>{CPU_SETSIZE}; }, "resources.cpu_set");
  {
    resource_config edge;
    edge.rss_target_bytes = too_big - 1;
    edge.cpu_set = std::vector<unsigned>{CPU_SETSIZE - 1};
    EXPECT_NO_THROW(validate_resource_config(edge));
    const std::string text = canonical_json(resource_config_json(edge));
    EXPECT_EQ(parse_resource_config(parse_json(text)), edge);
  }
  bad([](resource_config& c) { c.psi_threshold.origin.clear(); }, "resources.psi_threshold");
  bad([](resource_config& c) { c.job_queue_capacity.origin.clear(); }, "resources.job_queue_capacity");
}

// ---------------------------------------------------------------- T-M2R3-09

resource_config full_config() {
  resource_config c;
  c.rss_target_bytes = 123'456'789;
  c.scratch = dir_quota{"/tmp/astro-scratch", 5'000'000'000};
  c.disk_cache = dir_quota{"/tmp/astro-cache", 7'000'000};
  c.cpu_set = std::vector<unsigned>{0, 1};
  c.worker_count = 2;
  c.tile_rows = 32;
  c.job_queue_capacity = {5, "configurato"};
  c.task_queue_capacity = {6, "configurato"};
  c.report_margin_bytes = {2'000'000, "configurato"};
  c.fraction_normal = {{1, 2}, "configurato"};
  c.fraction_pressure = {{1, 5}, "configurato"};
  c.psi_threshold = {7.5, "configurato"};
  return c;
}

TEST(Resources, T_M2R3_09_config_is_persisted_and_revalidated_on_reopen) {
  const fs::path root = fresh_dir("res_store") / "project";
  {
    auto store = sqlite_dag_store::create(root);
    EXPECT_EQ(load_resource_config(*store), std::nullopt);
    save_resource_config(*store, full_config());
    EXPECT_EQ(load_resource_config(*store), full_config());
    resource_config bad = full_config();
    bad.worker_count = 0;
    EXPECT_STOP(save_resource_config(*store, bad), error_category::invalid_input, "resources.worker_count");
    EXPECT_EQ(load_resource_config(*store), full_config());
    save_resource_config(*store, resource_config{});
    EXPECT_EQ(load_resource_config(*store), resource_config{});
    save_resource_config(*store, full_config());
  }
  {
    auto store = sqlite_dag_store::open(root, open_intent::read_only);
    EXPECT_EQ(load_resource_config(*store), full_config());
    EXPECT_STOP(save_resource_config(*store, full_config()), error_category::schema, "store.read_only");
  }
  {
    // Un valore alterato fuori dal dominio o con chiavi sconosciute e' rifiutato in lettura, non riparato.
    auto store = sqlite_dag_store::open(root, open_intent::read_write);
    const std::string good = canonical_json(resource_config_json(full_config()));
    const auto replaced = [&](const std::string& from, const std::string& to) {
      std::string t = good;
      const std::size_t at = t.find(from);
      EXPECT_NE(at, std::string::npos) << from;
      t.replace(at, from.size(), to);
      return t;
    };
    store->put_setting(resource_config_setting_key, good.substr(0, good.size() - 1) + ",\"zz_extra\":1}");
    EXPECT_STOP(load_resource_config(*store), error_category::invalid_input, "resources.setting_malformed");
    store->put_setting(resource_config_setting_key, replaced("\"worker_count\":2", "\"worker_count\":0"));
    EXPECT_STOP(load_resource_config(*store), error_category::invalid_input, "resources.worker_count");
    store->put_setting(resource_config_setting_key, replaced("\"tile_rows\":32", "\"tile_rows\":\"auto\""));
    EXPECT_STOP(load_resource_config(*store), error_category::invalid_input, "resources.setting_malformed");
    // Classificazione: intero negativo o tipo errato = setting_malformed; tipo corretto ma fuori dominio = resources.<campo>.
    for (const char* negative : {"\"worker_count\":-1", "\"tile_rows\":-1", "\"rss_target_bytes\":-1"}) {
      SCOPED_TRACE(negative);
      const std::string field = std::string(negative).substr(0, std::string(negative).find(':'));
      const std::string original = field == "\"rss_target_bytes\"" ? "\"rss_target_bytes\":123456789"
                                   : field == "\"worker_count\"" ? "\"worker_count\":2" : "\"tile_rows\":32";
      store->put_setting(resource_config_setting_key, replaced(original, negative));
      EXPECT_STOP(load_resource_config(*store), error_category::invalid_input, "resources.setting_malformed");
    }
    // Il parser legge come intero fino a 2^53 incluso: 2^53 e' un intero oltre il limite configurativo (2^53 - 1) e dà
    // resources.<campo>; oltre 2^53 e' un numero non intero (limite del parser): setting_malformed. Un reale fuori dominio (psi_threshold) e' un dominio, non un tipo.
    for (const auto& [from, to, code] : std::vector<std::tuple<std::string, std::string, std::string>>{
             {"\"rss_target_bytes\":123456789", "\"rss_target_bytes\":9007199254740994", "resources.setting_malformed"},
             {"\"rss_target_bytes\":123456789", "\"rss_target_bytes\":9007199254740992", "resources.rss_target_bytes"},
             {"\"rss_target_bytes\":123456789", "\"rss_target_bytes\":0", "resources.rss_target_bytes"},
             {"\"rss_target_bytes\":123456789", "\"rss_target_bytes\":9007199254740991", ""},
             {"\"value\":5}", "\"value\":0}", "resources.job_queue_capacity"},
             {"\"value\":7.5}", "\"value\":-1}", "resources.psi_threshold"}}) {
      SCOPED_TRACE(to);
      store->put_setting(resource_config_setting_key, replaced(from, to));
      if (code.empty()) {
        EXPECT_NO_THROW(load_resource_config(*store));
      } else {
        EXPECT_STOP(load_resource_config(*store), error_category::invalid_input, code.c_str());
      }
    }
    // Valori oltre 32 bit: rifiutati, non troncati.
    for (const auto& [from, to] : std::vector<std::pair<std::string, std::string>>{
             {"\"worker_count\":2", "\"worker_count\":4294967297"},
             {"\"tile_rows\":32", "\"tile_rows\":4294967297"},
             {"\"cpu_set\":[0,1]", "\"cpu_set\":[4294967296,4294967297]"}}) {
      SCOPED_TRACE(to);
      store->put_setting(resource_config_setting_key, replaced(from, to));
      EXPECT_STOP(load_resource_config(*store), error_category::invalid_input, "resources.setting_malformed");
    }
  }
}

TEST(Resources, T_M2R3_08_budget_tuning_is_validated_at_the_api_boundary) {
  fake_system sys("res_tuning");
  const auto stop_with = [&](budget_tuning t) {
    EXPECT_STOP(compute_automatic_budget(sys.sources(), t), error_category::invalid_input, "budget.tuning");
  };
  budget_tuning t;
  t.normal = {1, 0};
  stop_with(t);
  t = {};
  t.normal = {0, 8};
  stop_with(t);
  t = {};
  t.pressure = {5, 4};
  stop_with(t);
  t = {};
  t.pressure = {1, 2};  // sopra la normale
  stop_with(t);
  t = {};
  t.normal = {1, 1001};
  stop_with(t);
  t = {};
  t.normal = {9, 8};  // oltre 1, ordine rispettato
  stop_with(t);
  t = {};
  t.normal = {1001, 1001};  // denominatore oltre 1000, ordine rispettato
  stop_with(t);
  t = {};
  t.pressure = {0, 4};
  stop_with(t);
  t = {};
  t.pressure = {1, 1001};  // denominatore oltre 1000, ordine rispettato
  stop_with(t);
  t = {};
  t.psi_threshold = std::nan("");
  stop_with(t);
  t = {};
  t.psi_threshold = 0.0;
  stop_with(t);
  t = {};
  t.psi_threshold = 100.5;
  stop_with(t);
  EXPECT_NO_THROW(compute_automatic_budget(sys.sources(), budget_tuning{}));
}

cpu_set_t current_affinity() {
  cpu_set_t set;
  CPU_ZERO(&set);
  EXPECT_EQ(::sched_getaffinity(0, sizeof(set), &set), 0);
  return set;
}

TEST(Resources, T_M2R3_09_cpu_set_is_applied_to_workers_only_and_validated_against_the_affinity) {
  const cpu_set_t allowed = current_affinity();
  std::vector<unsigned> cpus;
  for (unsigned i = 0; i < CPU_SETSIZE && cpus.size() < 2; ++i) {
    if (CPU_ISSET(i, &allowed)) cpus.push_back(i);
  }
  ASSERT_EQ(cpus.size(), 2u) << "servono almeno due CPU nell'affinita'";
  resource_config c;
  c.cpu_set = cpus;
  c.worker_count = 2;
  validate_cpu_set(c);

  memory_pool pool(64 * MiB);
  job_scheduler scheduler(pool, system_affinity, scheduler_tuning_of(c));
  const node_spec node = test_node("affinity");
  job_plan plan;
  plan.shared_bytes = MiB;
  plan.per_task_bytes = MiB;
  plan.worker_count = 2;
  std::mutex mutex;
  std::vector<std::set<unsigned>> seen;
  job_spec spec(node, 8, [&](task_context&) {
    cpu_set_t set = current_affinity();
    std::set<unsigned> now;
    for (unsigned i = 0; i < CPU_SETSIZE; ++i) {
      if (CPU_ISSET(i, &set)) now.insert(i);
    }
    std::lock_guard lock(mutex);
    seen.push_back(std::move(now));
    return std::string();
  });
  cancel_token cancel;
  const cpu_set_t before = current_affinity();
  scheduler.run(plan, spec, cancel);
  ASSERT_EQ(seen.size(), 8u);
  for (const auto& s : seen) EXPECT_EQ(s, std::set<unsigned>(cpus.begin(), cpus.end()));
  const cpu_set_t after = current_affinity();
  EXPECT_TRUE(CPU_EQUAL(&before, &after));  // il thread chiamante non cambia

  // CPU fuori dall'affinita' (iniettata): stop prima di ogni task.
  const affinity_fn only_cpu0 = [](cpu_set_t* set) {
    CPU_ZERO(set);
    CPU_SET(0, set);
    return 0;
  };
  resource_config outside;
  outside.cpu_set = std::vector<unsigned>{0, 1};
  EXPECT_STOP(validate_cpu_set(outside, only_cpu0), error_category::invalid_input, "sched.cpu_set");
  job_scheduler strict(pool, only_cpu0, scheduler_tuning_of(outside));
  plan.worker_count = 1;
  std::atomic<int> ran{0};
  job_spec never(node, 4, [&](task_context&) { ++ran; return std::string(); });
  EXPECT_STOP(strict.run(plan, never, cancel), error_category::invalid_input, "sched.cpu_set");
  EXPECT_EQ(ran.load(), 0);
  // worker oltre le CPU dell'insieme.
  resource_config small;
  small.cpu_set = std::vector<unsigned>{cpus[0]};
  small.worker_count = 2;
  EXPECT_STOP(validate_cpu_set(small), error_category::invalid_input, "sched.worker_count");
  job_scheduler narrow(pool, system_affinity, scheduler_tuning_of(small));
  plan.worker_count = 2;
  EXPECT_STOP(narrow.run(plan, never, cancel), error_category::invalid_input, "sched.worker_count");
  EXPECT_EQ(ran.load(), 0);
}

TEST(Resources, T_M2R3_09_a_failed_affinity_application_stops_the_job_with_the_system_error) {
  // L'affinita' iniettata dichiara la CPU 1000, che il kernel non conosce: pthread_setaffinity_np fallisce davvero.
  // Precondizione ambientale: la macchina ha meno di 1000 CPU configurate.
  ASSERT_LT(::sysconf(_SC_NPROCESSORS_CONF), 1000);
  const affinity_fn fake = [](cpu_set_t* set) {
    CPU_ZERO(set);
    CPU_SET(1000, set);
    CPU_SET(0, set);
    return 0;
  };
  resource_config c;
  c.cpu_set = std::vector<unsigned>{1000};
  c.worker_count = 1;
  validate_cpu_set(c, fake);
  memory_pool pool(64 * MiB);
  job_scheduler scheduler(pool, fake, scheduler_tuning_of(c));
  job_plan plan;
  plan.shared_bytes = MiB;
  plan.per_task_bytes = MiB;
  std::atomic<int> ran{0};
  std::atomic<int> finalized{0};
  const node_spec node = test_node("apply");
  job_spec spec(node, 4, [&](task_context&) { ++ran; return std::string(); });
  spec.finalize = [&](job_context&) { ++finalized; };
  cancel_token cancel;
  try {
    scheduler.run(plan, spec, cancel);
    ADD_FAILURE() << "stop attesto";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::io);
    EXPECT_EQ(e.code(), "sched.cpu_set_apply");
    const auto has = [&](const char* key, const std::string& value) {
      return std::find(e.context().begin(), e.context().end(), std::make_pair(std::string(key), value)) !=
             e.context().end();
    };
    EXPECT_TRUE(has("errno", std::to_string(EINVAL)));
    EXPECT_TRUE(has("errno_text", std::strerror(EINVAL)));
  }
  EXPECT_EQ(ran.load(), 0);
  EXPECT_EQ(finalized.load(), 0);
  EXPECT_EQ(pool.reserved_bytes(), 0u);
}

TEST(Resources, T_M2R3_09_disk_cache_quota_and_confined_cleanup) {
  const fs::path dir = fresh_dir("res_cache");
  const fs::path outside = fresh_dir("res_cache_outside");
  write_text(dir / "a.bin", std::string(1000, 'a'));
  write_text(dir / "b.bin", std::string(500, 'b'));
  write_text(outside / "keep.txt", "keep");
  const disk_cache_usage usage = measure_disk_cache(dir);
  EXPECT_EQ(usage.bytes, 1500u);
  EXPECT_EQ(usage.entries, 2u);
  EXPECT_NO_THROW(enforce_disk_cache_quota(dir, 1500));
  EXPECT_STOP(enforce_disk_cache_quota(dir, 1499), error_category::budget, "cache.quota_exceeded");

  // Nomi che escono dalla directory o non ammessi.
  for (const char* name : {"../res_cache_outside/keep.txt", "a/b", "..", ".", "", "x y"}) {
    SCOPED_TRACE(name);
    EXPECT_STOP(remove_disk_cache_entries(dir, {name}), error_category::invalid_input, "cache.entry_name");
  }
  EXPECT_STOP(remove_disk_cache_entries(dir, {"absent.bin"}), error_category::metadata_missing, "cache.entry_missing");

  // Symlink verso un file esterno: stop, il bersaglio resta intatto.
  fs::create_symlink(outside / "keep.txt", dir / "link.bin");
  EXPECT_STOP(remove_disk_cache_entries(dir, {"link.bin"}), error_category::security, "cache.entry_unsafe");
  EXPECT_STOP(measure_disk_cache(dir), error_category::security, "cache.entry_unsafe");
  EXPECT_EQ(read_text(outside / "keep.txt"), "keep");
  EXPECT_TRUE(fs::is_symlink(dir / "link.bin"));
  fs::remove(dir / "link.bin");

  // Hardlink (nlink 2): stop, entrambi i nomi restano.
  fs::create_hard_link(dir / "a.bin", outside / "a.hard");
  EXPECT_STOP(remove_disk_cache_entries(dir, {"a.bin"}), error_category::security, "cache.entry_unsafe");
  EXPECT_STOP(measure_disk_cache(dir), error_category::security, "cache.entry_unsafe");
  EXPECT_TRUE(fs::exists(dir / "a.bin"));
  fs::remove(outside / "a.hard");

  // Sottodirectory: voce non regolare.
  fs::create_directory(dir / "sub");
  EXPECT_STOP(measure_disk_cache(dir), error_category::security, "cache.entry_unsafe");
  fs::remove(dir / "sub");

  remove_disk_cache_entries(dir, {"a.bin"});
  EXPECT_FALSE(fs::exists(dir / "a.bin"));
  EXPECT_TRUE(fs::exists(dir / "b.bin"));
  EXPECT_EQ(measure_disk_cache(dir).bytes, 500u);

  // Directory mancante o symlink.
  EXPECT_STOP(measure_disk_cache(dir / "nope"), error_category::io, "cache.dir");
  fs::create_directory_symlink(dir, outside / "dirlink");
  EXPECT_STOP(measure_disk_cache(outside / "dirlink"), error_category::io, "cache.dir");
  EXPECT_STOP(remove_disk_cache_entries(outside / "dirlink", {"b.bin"}), error_category::io, "cache.dir");
  EXPECT_TRUE(fs::exists(dir / "b.bin"));
}

// ---------------------------------------------------------------- T-M2R3-03

std::uint64_t status_kib(const char* field) {
  std::ifstream in("/proc/self/status");
  std::string line;
  const std::string key = std::string(field) + ":";
  while (std::getline(in, line)) {
    if (line.compare(0, key.size(), key) == 0) return std::stoull(line.substr(key.size())) * 1024;
  }
  ADD_FAILURE() << "campo assente in /proc/self/status: " << field;
  return 0;
}

std::uint64_t u64(const json& doc, const char* key) {
  const json* v = doc.find(key);
  EXPECT_NE(v, nullptr) << key;
  return v == nullptr ? 0 : static_cast<std::uint64_t>(v->as_integer());
}

TEST(Resources, T_M2R3_03_monitor_reports_pool_rss_scratch_cache_vram_and_signals) {
  fake_system sys("res_monitor");
  sys.meminfo(8 * 1024 * 1024, 4 * 1024 * 1024);
  const budget_snapshot budget = compute_manual_budget(parse_manual_budget("64MiB"), sys.sources());
  memory_pool pool(budget);
  reservation a = pool.reserve(10 * MiB, "a");
  { reservation b = pool.reserve(20 * MiB, "b"); }
  const fs::path scratch_dir = fresh_dir("res_monitor_scratch");
  spill_area area(spill_config{scratch_dir, 4 * MiB});
  spill_file& file = area.create_shared_file();
  std::vector<std::byte> bytes(3 * MiB, std::byte{1});
  file.write(0, bytes);
  const fs::path cache_dir = fresh_dir("res_monitor_cache");
  write_text(cache_dir / "x", std::string(2000, 'x'));

  resource_config cfg;
  cfg.rss_target_bytes = 1;  // sicuramente superato
  cfg.disk_cache = dir_quota{fs::absolute(cache_dir), 1000};
  const planner_input in = table_input(budget.budget_bytes, 4);
  const resource_plan plan = plan_resources(in, [&] { resource_config c; return c; }());  // 64 MiB >> minimo

  monitor_inputs mi;
  mi.pool = &pool;
  mi.plan = &plan;
  mi.planner = &in;
  mi.config = &cfg;
  mi.budget = &budget;
  mi.scratch = &area;
  mi.resident_cache_bytes = 777;
  const resource_sample s = sample_resources(mi);
  const json& d = s.document;
  const json& p = *d.find("pool");
  EXPECT_EQ(u64(p, "budget_bytes"), 64 * MiB);
  EXPECT_EQ(u64(p, "reserved_bytes"), 10 * MiB);
  EXPECT_EQ(u64(p, "peak_reserved_bytes"), 30 * MiB);
  EXPECT_EQ(u64(p, "waiting_count"), 0u);
  EXPECT_EQ(u64(d, "estimated_peak_bytes"), plan.peak_bytes);
  EXPECT_EQ(u64(d, "resident_cache_bytes"), 777u);
  const json& sc = *d.find("scratch");
  EXPECT_EQ(u64(sc, "used_bytes"), 3 * MiB);
  EXPECT_EQ(u64(sc, "peak_bytes"), 3 * MiB);
  EXPECT_EQ(u64(sc, "quota_bytes"), 4 * MiB);
  const json& dc = *d.find("disk_cache");
  EXPECT_EQ(u64(dc, "bytes"), 2000u);
  EXPECT_EQ(u64(dc, "quota_bytes"), 1000u);
  const json& rss = *d.find("rss");
  EXPECT_GT(u64(rss, "rss_bytes"), 0u);
  EXPECT_LE(u64(rss, "rss_bytes"), status_kib("VmHWM"));
  EXPECT_LE(u64(rss, "rss_peak_bytes"), status_kib("VmHWM"));
  EXPECT_EQ(u64(rss, "target_bytes"), 1u);
  const json& vram = *d.find("vram");
  EXPECT_EQ(vram.find("value")->type(), json::kind::null);
  EXPECT_EQ(vram.find("reason")->as_string(), vram_null_reason);
  const json& limit = *d.find("limit");
  EXPECT_EQ(u64(limit, "budget_bytes"), 64 * MiB);
  EXPECT_EQ(limit.find("origin")->as_string(), "manual:64MiB");
  EXPECT_TRUE(s.rss_over_target);       // solo evento riportato
  EXPECT_TRUE(s.disk_cache_over_quota);
  EXPECT_FALSE(s.budget_below_minimum);
  EXPECT_FALSE(s.budget_below_working_set);
  EXPECT_EQ(pool.reserved_bytes(), 10 * MiB);  // nessuna azione automatica

  // Cache entro quota: il segnale e' spento.
  resource_config roomy = cfg;
  roomy.disk_cache = dir_quota{fs::absolute(cache_dir), 2000};
  mi.config = &roomy;
  mi.planner = nullptr;
  const resource_sample within = sample_resources(mi);
  EXPECT_FALSE(within.disk_cache_over_quota);
  EXPECT_FALSE(within.document.find("disk_cache")->find("over_quota")->as_bool());
  mi.config = &cfg;
  mi.planner = &in;

  // Segnali distinti: sotto il working set (il piano riduce i worker) e sotto il minimo (il preflight si ferma).
  const budget_snapshot tight = [&] {
    budget_snapshot t = budget;
    t.budget_bytes = 33'220'575;
    return t;
  }();
  memory_pool tight_pool(33'220'575);
  planner_input tin = table_input(33'220'575, 16);
  mi.pool = &tight_pool;
  mi.budget = &tight;
  mi.plan = nullptr;
  mi.planner = &tin;
  mi.scratch = nullptr;
  mi.config = &cfg;
  resource_config plain;
  mi.config = &plain;
  const resource_sample below_ws = sample_resources(mi);
  EXPECT_TRUE(below_ws.budget_below_working_set);
  EXPECT_FALSE(below_ws.budget_below_minimum);
  EXPECT_EQ(below_ws.document.find("scratch")->type(), json::kind::null);
  EXPECT_EQ(below_ws.document.find("estimated_peak_bytes")->type(), json::kind::null);
  EXPECT_FALSE(below_ws.rss_over_target);

  budget_snapshot tiny = tight;
  tiny.budget_bytes = minimum_hand - 1;
  memory_pool tiny_pool(minimum_hand - 1);
  mi.pool = &tiny_pool;
  mi.budget = &tiny;
  const resource_sample below_min = sample_resources(mi);
  EXPECT_TRUE(below_min.budget_below_minimum);
  EXPECT_TRUE(below_min.budget_below_working_set);
  EXPECT_STOP(plan_resources(table_input(minimum_hand - 1, 16), plain), error_category::budget,
              "sched.job_exceeds_budget");
}

TEST(Resources, T_M2R3_03_monitor_rejects_an_unreadable_status_instead_of_inventing_values) {
  fake_system sys("res_monitor_status");
  const budget_snapshot budget = compute_manual_budget(parse_manual_budget("64MiB"), sys.sources());
  memory_pool pool(budget);
  monitor_inputs mi;
  mi.pool = &pool;
  mi.budget = &budget;
  mi.proc_root = sys.root / "proc";
  EXPECT_STOP(sample_resources(mi), error_category::io, "monitor.status");
  write_text(sys.root / "proc" / "self" / "status", "Name:\tx\nVmRSS:\t 100 kB\n");
  EXPECT_STOP(sample_resources(mi), error_category::io, "monitor.status");
  write_text(sys.root / "proc" / "self" / "status", "Name:\tx\nVmHWM:\t 200 kB\nVmRSS:\t 100 kB\n");
  const resource_sample s = sample_resources(mi);
  EXPECT_EQ(u64(*s.document.find("rss"), "rss_bytes"), 100 * 1024u);
  EXPECT_EQ(u64(*s.document.find("rss"), "rss_peak_bytes"), 200 * 1024u);
  EXPECT_EQ(s.document.find("rss")->find("target_bytes")->type(), json::kind::null);
}

// ---------------------------------------------------------------- T-M2R3-01: residenza a blocchi (casi g-o)

constexpr std::uint64_t whole_min_hand = 81'483'856, blocks_min_hand = 16'736'720;
constexpr std::uint64_t blocks_shared_hand = 11'552'720, blocks_per_task_hand = 5'184'000;

planner_input blocks_input(std::uint64_t budget, bool checkpoint = false) {
  planner_input in;
  in.budget_bytes = budget;
  in.cpus = 4;
  in.width = 1000;
  in.height = 1000;
  in.out_sample_bytes = 4;
  in.input_peak_bytes = 50'000'000;
  in.header_bound_bytes = 2'417'280;
  in.block_raw_bytes_per_row = 32'000;
  in.block_band_bytes_per_row = 40'000;
  in.block_regions = 8;
  in.normalization_channels = 1;
  in.checkpoint = checkpoint;
  return in;
}

resource_config with_scratch(std::uint64_t quota = 10'000'000) {
  resource_config c;
  c.scratch = dir_quota{"/scratch-di-prova", quota};
  return c;
}

void expect_stop_with_minimum(const planner_input& in, const resource_config& c, std::uint64_t minimum,
                              const char* residency) {
  try {
    (void)plan_resources(in, c);
    ADD_FAILURE() << "stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::budget);
    EXPECT_EQ(e.code(), "sched.job_exceeds_budget");
    EXPECT_EQ(context_value(e, "minimum_feasible_bytes"), std::to_string(minimum));
    EXPECT_EQ(context_value(e, "residenza"), residency);
  }
}

TEST(Resources, T_M2R3_01_planner_chooses_blocks_with_the_hand_computed_table) {
  {  // g
    const resource_plan p = plan_resources(blocks_input(whole_min_hand), with_scratch());
    EXPECT_EQ(p.frame_residency, "whole");
    EXPECT_EQ(p.master_residency, "whole");
    EXPECT_EQ(p.minimum_feasible_bytes, whole_min_hand);
    EXPECT_FALSE(p.spill);
  }
  {  // h
    const resource_plan p = plan_resources(blocks_input(whole_min_hand - 1), with_scratch());
    EXPECT_EQ(p.frame_residency, "blocks");
    EXPECT_EQ(p.master_residency, "blocks");
    EXPECT_EQ(p.tile_rows, 63u);
    EXPECT_EQ(p.task_count, 16u);
    EXPECT_EQ(p.per_task_bytes, blocks_per_task_hand);
    EXPECT_EQ(p.shared_bytes, blocks_shared_hand);
    EXPECT_EQ(p.minimum_feasible_bytes, blocks_min_hand);
    EXPECT_EQ(p.whole_minimum_bytes, whole_min_hand);
    EXPECT_EQ(p.worker_count, 4u);
    EXPECT_EQ(p.peak_bytes, blocks_shared_hand + 4 * blocks_per_task_hand);
    EXPECT_EQ(p.staging_bytes, 10'000'000u);
    EXPECT_TRUE(p.budget_below_working_set);
    ASSERT_TRUE(p.spill);
    EXPECT_EQ(p.spill->scratch_dir, fs::path("/scratch-di-prova"));
    EXPECT_EQ(p.spill->quota_bytes, 10'000'000u);
  }
  {  // i
    const resource_plan p = plan_resources(blocks_input(blocks_min_hand), with_scratch());
    EXPECT_EQ(p.frame_residency, "blocks");
    EXPECT_EQ(p.worker_count, 1u);
    EXPECT_EQ(p.peak_bytes, blocks_min_hand);
  }
  expect_stop_with_minimum(blocks_input(blocks_min_hand - 1), with_scratch(), blocks_min_hand, "blocks");  // j
  {  // k
    const resource_plan p = plan_resources(blocks_input(whole_min_hand, true), with_scratch());
    EXPECT_EQ(p.frame_residency, "blocks");
    EXPECT_EQ(p.worker_count, 4u);
  }
  {  // l
    try {
      (void)plan_resources(blocks_input(whole_min_hand - 1), resource_config{});
      ADD_FAILURE() << "stop atteso";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.code(), "sched.job_exceeds_budget");
      EXPECT_EQ(context_value(e, "minimum_feasible_bytes"), std::to_string(whole_min_hand));
      EXPECT_EQ(context_value(e, "residenza_blocchi"), "scratch non dichiarato");
    }
  }
  EXPECT_STOP(plan_resources(blocks_input(blocks_min_hand), with_scratch(9'999'999)), error_category::budget,
              "spill.quota_exceeded");  // m
  {  // n
    resource_config c = with_scratch();
    c.worker_count = 2;
    try {
      (void)plan_resources(blocks_input(blocks_min_hand), c);
      ADD_FAILURE() << "stop atteso";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.code(), "sched.job_exceeds_budget");
      EXPECT_EQ(context_value(e, "picco"), "21920720");
    }
  }
  EXPECT_STOP(plan_resources(blocks_input(blocks_min_hand, true), resource_config{}), error_category::invalid_input,
              "calib.checkpoint_scratch");  // o
  // Con budget ampio e senza checkpoint il piano di R3.1 resta identico anche con scratch dichiarato.
  const resource_plan whole = plan_resources(table_input(104'857'600, 4), with_scratch());
  const resource_plan r31 = plan_resources(table_input(104'857'600, 4), resource_config{});
  EXPECT_EQ(resource_plan_json(whole).find("worker_count")->as_integer(), resource_plan_json(r31).find("worker_count")->as_integer());
  EXPECT_EQ(whole.shared_bytes, r31.shared_bytes);
  EXPECT_EQ(whole.frame_residency, "whole");
}

// ---------------------------------------------------------------- T-M2R3-07: porta della pressione e monitor

TEST(Resources, T_M2R3_07_pressure_poll_is_a_persisted_parameter_with_origin) {
  const resource_config d;
  EXPECT_EQ(d.pressure_poll_ms.value, 1000u);
  EXPECT_EQ(d.pressure_poll_ms.origin, "default dichiarato, non misurato");
  resource_config c;
  c.pressure_poll_ms = {25, "configurato"};
  EXPECT_EQ(parse_resource_config(resource_config_json(c)), c);
  const fs::path root = fresh_dir("res_poll") / "project";
  auto store = sqlite_dag_store::create(root);
  save_resource_config(*store, c);
  EXPECT_EQ(load_resource_config(*store), c);
  c.pressure_poll_ms.value = 0;
  EXPECT_STOP(validate_resource_config(c), error_category::invalid_input, "resources.pressure_poll_ms");
  c.pressure_poll_ms = {too_big, "configurato"};
  EXPECT_STOP(validate_resource_config(c), error_category::invalid_input, "resources.pressure_poll_ms");
  c.pressure_poll_ms = {5, ""};
  EXPECT_STOP(validate_resource_config(c), error_category::invalid_input, "resources.pressure_poll_ms");
}

TEST(Resources, T_M2R3_07_system_pressure_gate_reads_psi_with_the_configured_threshold) {
  fake_system sys("res_gate");
  resource_config c;
  c.psi_threshold = {12.5, "configurato"};
  c.pressure_poll_ms = {7, "configurato"};
  const std::shared_ptr<pressure_gate> g = make_pressure_gate(c, sys.sources());
  EXPECT_EQ(g->threshold, 12.5);
  EXPECT_EQ(g->poll, std::chrono::milliseconds(7));
  EXPECT_EQ(g->sample(), std::nullopt);
  sys.psi("some avg10=13.25 avg60=1.00 avg300=0.50 total=12345\nfull avg10=0.00 avg60=0.00 avg300=0.00 total=0\n");
  EXPECT_EQ(g->sample(), std::optional<double>(13.25));
  EXPECT_EQ(read_memory_pressure(sys.sources()), std::optional<double>(13.25));
  sys.psi("rotto\n");
  EXPECT_STOP(g->sample(), error_category::invalid_input, "budget.psi_parse");
}

TEST(Resources, T_M2R3_07_monitor_exposes_the_jobs_waiting_for_pressure) {
  fake_system sys("res_monitor_pressure");
  write_text(sys.root / "proc" / "self" / "status", "VmHWM:\t 200 kB\nVmRSS:\t 100 kB\n");
  const budget_snapshot budget = compute_manual_budget(parse_manual_budget("64MiB"), sys.sources());
  memory_pool pool(budget);
  pressure_gate g;
  g.threshold = 10.0;
  g.waiting_jobs = 2;
  monitor_inputs mi;
  mi.pool = &pool;
  mi.budget = &budget;
  mi.proc_root = sys.root / "proc";
  const json absent = sample_resources(mi).document;
  ASSERT_NE(absent.find("pressure"), nullptr);
  EXPECT_EQ(absent.find("pressure")->type(), json::kind::null);
  mi.pressure = &g;
  const resource_sample s = sample_resources(mi);
  const json* pressure = s.document.find("pressure");
  ASSERT_NE(pressure, nullptr);
  ASSERT_EQ(pressure->type(), json::kind::object);
  ASSERT_NE(pressure->find("threshold"), nullptr);
  EXPECT_EQ(u64(*pressure, "waiting_jobs"), 2u);
  EXPECT_EQ(pressure->find("threshold")->as_number(), 10.0);
}

// ---------------------------------------------------------------- T-M2R2R-01: modello del master (estensione R2.r)

// Tabella del contratto: n = 10, W = H = 1000, E = 4, P = 4 000 000, HB = 100 000, M = D = 1 048 576, K = 32.
// shared = 66 197 152, task(t) = 160 + 108 000 t, minimo automatico 66 305 312 (calcolati a mano nel contratto).
constexpr std::uint64_t master_shared_hand = 66'197'152, master_min_hand = 66'305'312;

planner_input master_input(std::uint64_t budget, std::size_t cpus) {
  planner_input in = table_input(budget, cpus);
  in.frame_count = 10;
  return in;
}

struct master_expect {
  std::uint32_t tile;
  std::uint64_t tasks;
  unsigned workers;
  std::uint64_t peak;
  bool below_working_set;
};

void expect_master_plan(const resource_plan& p, const master_expect& x) {
  EXPECT_EQ(p.tile_rows, x.tile);
  EXPECT_EQ(p.task_count, x.tasks);
  EXPECT_EQ(p.worker_count, x.workers);
  EXPECT_EQ(p.peak_bytes, x.peak);
  EXPECT_EQ(p.shared_bytes, master_shared_hand);
  EXPECT_EQ(p.per_task_bytes, 160 + 108'000 * std::uint64_t{x.tile});
  EXPECT_EQ(p.budget_below_working_set, x.below_working_set);
  EXPECT_EQ(p.frame_residency, "blocks");
  EXPECT_EQ(p.master_residency, "whole");
  EXPECT_FALSE(p.spill);
  EXPECT_EQ(p.staging_bytes, 0u);
  EXPECT_EQ(p.whole_minimum_bytes, p.minimum_feasible_bytes);
}

void expect_master_stop(const planner_input& in, const resource_config& c, const std::string& peak,
                        const std::string& minimum) {
  try {
    (void)plan_resources(in, c);
    ADD_FAILURE() << "stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::budget);
    EXPECT_EQ(e.code(), "sched.job_exceeds_budget");
    EXPECT_EQ(context_value(e, "picco"), peak);
    EXPECT_EQ(context_value(e, "minimum_feasible_bytes"), minimum);
    EXPECT_EQ(context_value(e, "budget_bytes"), std::to_string(in.budget_bytes));
    EXPECT_EQ(context_value(e, "residenza"), "master");
  }
}

TEST(Resources, T_M2R2R_01_master_planner_matches_the_hand_computed_table) {
  const resource_config cfg;
  {  // p
    const resource_plan p = plan_resources(master_input(104'857'600, 4), cfg);
    expect_master_plan(p, {63, 16, 4, 93'413'792, false});
    EXPECT_EQ(p.working_set_bytes, 93'413'792u);
    EXPECT_EQ(p.minimum_feasible_bytes, master_min_hand);
    EXPECT_TRUE(p.user_fixed.empty());
    EXPECT_EQ(p.task_queue_capacity, 4u);
  }
  expect_master_plan(plan_resources(master_input(93'413'791, 4), cfg), {63, 16, 3, 86'609'632, true});  // q
  expect_master_plan(plan_resources(master_input(70'000'000, 4), cfg), {35, 29, 1, 69'977'312, true});  // r
  expect_master_plan(plan_resources(master_input(master_min_hand, 4), cfg), {1, 1000, 1, master_min_hand, true});  // s
  expect_master_stop(master_input(master_min_hand - 1, 4), cfg, "66305312", "66305312");  // t
  resource_config two = cfg;
  two.worker_count = 2;
  {  // u
    const resource_plan p = plan_resources(master_input(104'857'600, 4), two);
    expect_master_plan(p, {63, 16, 2, 79'805'472, false});
    EXPECT_EQ(p.user_fixed, std::vector<std::string>{"worker_count"});
  }
  expect_master_plan(plan_resources(master_input(70'000'000, 4), two), {17, 59, 2, 69'869'472, true});  // v
  expect_master_stop(master_input(master_min_hand, 4), two, "66413472", "66305312");  // w
  resource_config tile10 = cfg;
  tile10.tile_rows = 10;
  {  // x
    const resource_plan p = plan_resources(master_input(104'857'600, 2), tile10);
    expect_master_plan(p, {10, 100, 2, 68'357'472, false});
    EXPECT_EQ(p.minimum_feasible_bytes, 67'277'312u);
    EXPECT_EQ(p.user_fixed, std::vector<std::string>{"tile_rows"});
  }
  resource_config tile400 = cfg;
  tile400.tile_rows = 400;
  expect_master_stop(master_input(104'857'600, 4), tile400, "109397312", "109397312");  // y
}

TEST(Resources, T_M2R2R_01_unrepresentable_master_plan_is_a_budget_stop_with_the_value_declared) {
  resource_config c;
  c.tile_rows = 0xFFFFFFFFu;
  planner_input in = master_input(~std::uint64_t{0}, 4);  // z
  in.frame_count = 13'106;
  in.width = 65'536;
  try {
    (void)plan_resources(in, c);
    ADD_FAILURE() << "stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::budget);
    EXPECT_EQ(e.code(), "sched.job_exceeds_budget");
    EXPECT_EQ(context_value(e, "per_task_bytes"), "non rappresentabile");
    EXPECT_EQ(context_value(e, "minimum_feasible_bytes"), "non rappresentabile");
    EXPECT_EQ(context_value(e, "picco"), "non rappresentabile");
    EXPECT_EQ(context_value(e, "residenza"), "master");
  }
  EXPECT_STOP(compute_plan_bounds(in, c), error_category::budget, "sched.job_exceeds_budget");
  // Tile automatico con un numero di frame che rende non rappresentabile anche task(1).
  planner_input many = master_input(~std::uint64_t{0}, 4);
  many.frame_count = std::uint64_t{1} << 62;
  EXPECT_STOP(plan_resources(many, resource_config{}), error_category::budget, "sched.job_exceeds_budget");
}

TEST(Resources, T_M2R2R_01_master_bounds_match_the_plan_and_unused_inputs_are_rejected) {
  const resource_config cfg;
  const plan_bounds b = compute_plan_bounds(master_input(104'857'600, 4), cfg);
  EXPECT_EQ(b.tile_rows, 63u);
  EXPECT_EQ(b.task_count, 16u);
  EXPECT_EQ(b.shared_bytes, master_shared_hand);
  EXPECT_EQ(b.per_task_bytes, 6'804'160u);
  EXPECT_EQ(b.minimum_feasible_bytes, master_min_hand);
  EXPECT_EQ(b.working_set_bytes, 93'413'792u);
  // Margine configurato: shared cresce della differenza.
  resource_config margin = cfg;
  margin.report_margin_bytes = {3 * 1'048'576, "configurato"};
  EXPECT_EQ(compute_plan_bounds(master_input(104'857'600, 4), margin).shared_bytes, master_shared_hand + 2 * 1'048'576);
  planner_input in = master_input(104'857'600, 4);
  in.checkpoint = true;
  EXPECT_STOP(plan_resources(in, cfg), error_category::invalid_input, "resources.planner_input");
  in = master_input(104'857'600, 4);
  in.block_raw_bytes_per_row = 1;
  EXPECT_STOP(plan_resources(in, cfg), error_category::invalid_input, "resources.planner_input");
  in = master_input(104'857'600, 4);
  in.block_band_bytes_per_row = 1;
  EXPECT_STOP(plan_resources(in, cfg), error_category::invalid_input, "resources.planner_input");
  in = master_input(104'857'600, 4);
  in.block_regions = 1;
  EXPECT_STOP(plan_resources(in, cfg), error_category::invalid_input, "resources.planner_input");
  in = master_input(104'857'600, 4);
  in.normalization_channels = 4;
  EXPECT_STOP(plan_resources(in, cfg), error_category::invalid_input, "resources.planner_input");
  in = master_input(104'857'600, 4);
  in.out_sample_bytes = 17;
  EXPECT_STOP(plan_resources(in, cfg), error_category::invalid_input, "resources.planner_input");
  // CPU sintetiche oltre 2^62: t0 = ceil(ceil(H / C) / 4) = 1, senza il prodotto 4 C.
  in = master_input(104'857'600, std::size_t{1} << 62);
  EXPECT_EQ(compute_plan_bounds(in, cfg).tile_rows, 1u);
  EXPECT_EQ(compute_plan_bounds(table_input(104'857'600, std::size_t{1} << 62), cfg).tile_rows, 1u);
  // Con frame_count = 0 il modello della calibrazione resta quello della tabella di R3.1.
  EXPECT_EQ(plan_resources(table_input(104'857'600, 4), cfg).shared_bytes, shared_hand);
}

}  // namespace
