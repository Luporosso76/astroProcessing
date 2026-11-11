// M3.b, unità B10: blocchi, worker, determinismo, memoria (C-02.b §b.8, §b.11; C-02.b-flat §f.8, §f.11; estensione
// ADR-06 R2.r ed «Estensione M3.b del piano del master»).
// Test:
// - T-M3B-C02-10, parte metodi: per `single`, `mean`, `weighted_mean`, `median`, `winsorized` e `mean` + `sigma_clip`,
//   mono e CFA, casi di `c02_10_cases` (37 × 71, n ∈ {3, 4, 11}, outlier standard, seed 2026110963): campioni, piano
//   VARIANCE e maschera del job con `tile_rows` ∈ {1, 7, 64} e worker ∈ {1, 4} uguali bit per bit a `combine_master`,
//   e digest dell'artefatto primario uguale fra le sei configurazioni;
// - T-M3B-FLAT-08, parte valori: gli stessi controlli per ogni metodo con `median_ratio` (`flat_08_cases`, 37 × 71,
//   n ∈ {3, 4, 11}, mono e CFA, seed 2026110968);
// - uscita f32: invarianza bit a bit; il singolo arrotondamento è verificato da T-M3B-C02-01;
// - T-M3B-MEM-01 e T-M2R2R-02, estensione ai metodi: il minimo del job è il modello di ADR-06 per ogni metodo e per
//   `median_ratio`, il job completa con budget uguale al minimo (tile 1, un worker) e si ferma con minimo − 1 prima di
//   ogni lettura (prova: il gancio `frames.read` non scatta e `peak_reserved_bytes() == 0`); con budget ampio e
//   pianificatore automatico il master è quello della memoria.
// Fuori da questo file: report (B11), candidati permutati (B14), riapertura e ricostruzione del nodo (B16).
//
// Nessun valore atteso dal prodotto: il riferimento di ogni confronto è `combine_master` sugli stessi frame; la
// correttezza dei valori è di T-M3B-C02-01…07 e T-M3B-FLAT-01, 02. Le maschere dei casi contengono pixel non finiti
// su righe di confine dei blocchi (0, 6, 7, 8, 63, 64, 70), così la maschera non è vuota.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "master_support.hpp"
#include "store_test_support.hpp"
#include "syn_cal.hpp"

namespace {

using namespace mst;
namespace sc = astro_fixture::syn_cal;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr std::uint64_t kMiB = std::uint64_t{1} << 20;
constexpr double kFlatBound = 32;     // A, come T-M3B-FLAT-01 (stessa famiglia di flat)
// `flat_norm_rel_budget`: stessa famiglia di flat di T-M3B-FLAT-01 (`pattern_flat`: livelli c_(f,c) =
// s_c (1000 + ⌊3000 f/(n − 1)⌋), rapporto 4, G = 1, R = 5, β ∈ [−32, 32]) con A = 32 e n ≤ 11; il controllo 4 del
// preflight (A ρ_c ≤ budget · m_H,c) dipende da n e dai livelli, non dalla dimensione dell'immagine, quindi vale il
// calcolo di test_master_b9.cpp (T_M3B_FLAT_01_non_dyadic_cases...): A ρ ≤ 34.8 < 0.02 m_H nel caso peggiore n = 11.
constexpr double kFlatBudget = 0.02;

// ---------------------------------------------------------------- metodi

enum class kind { single, mean, weighted, median, winsorized, sigma_clip };

const char* name_of(kind k) {
  switch (k) {
    case kind::single: return "single";
    case kind::mean: return "mean";
    case kind::weighted: return "weighted_mean";
    case kind::median: return "median";
    case kind::winsorized: return "winsorized";
    case kind::sigma_clip: return "sigma_clip";
  }
  return "";
}

// Parametri non inerti per ogni n dei casi: κ = 1 per n ∈ {3, 4} ((n − 1)² > κ² n), κ = 2 per n = 11; tre iterazioni,
// almeno 2 tenuti; pesi 2, 3, 2, 3, … (2 max w < W per n ≥ 3); g = 1 (n ≤ 4) o 2 (n = 11).
void apply(kind k, master_recipe& r) {
  const std::size_t n = r.frames.size();
  r.combine = combine_method::mean;
  r.rejection = master_rejection::none;
  switch (k) {
    case kind::single:
      r.combine = combine_method::single;
      r.offset_adu = 10;
      r.conversion_gain_e_per_adu = 1.5;
      r.read_noise_adu = 2;
      break;
    case kind::mean: break;
    case kind::weighted:
      r.combine = combine_method::weighted_mean;
      for (std::size_t f = 0; f < n; ++f) r.frames[f].weight = 2.0 + static_cast<double>(f % 2);
      break;
    case kind::median: r.combine = combine_method::median; break;
    case kind::winsorized:
      r.combine = combine_method::winsorized;
      r.winsor_g = n >= 11 ? 2.0 : 1.0;
      break;
    case kind::sigma_clip:
      r.rejection = master_rejection::sigma_clip;
      r.clip_kappa_low = r.clip_kappa_high = n >= 11 ? 2.0 : 1.0;
      r.clip_max_iter = 3;
      r.clip_min_kept = 2;
      break;
  }
}

master_recipe flat_ratio_recipe(const group& g, kind k, sample_type out) {
  master_recipe r = recipe_of(g);
  r.flat_norm = flat_normalization::median_ratio;
  r.flat_level_rel_tol.reset();
  apply(k, r);
  set_flat_additive_bound(r, kFlatBound);
  set_flat_norm_budget(r, k == kind::mean ? std::optional<double>{} : std::optional<double>{kFlatBudget});
  r.output_type = out;
  return r;
}

master_recipe dark_recipe(const group& g, kind k, sample_type out) {
  master_recipe r = recipe_of(g);
  apply(k, r);
  r.output_type = out;
  return r;
}

// ---------------------------------------------------------------- frame

constexpr std::array<std::size_t, 7> kPokeRows = {0, 6, 7, 8, 63, 64, 70};

// Un campione non finito per riga di confine, su frame e colonne diversi.
void poke_nonfinite(group& g) {
  for (std::size_t k = 0; k < kPokeRows.size(); ++k) {
    const std::size_t i = kPokeRows[k] * g.w + (3 + 5 * k) % g.w;
    g.frames[k % g.frames.size()].v[i] = kNaN;
  }
}

group dark_group(const sc::frames& x, std::uint32_t w, std::uint32_t h, std::size_t n, bool cfa) {
  group g = make_group(master_role::dark, w, h, n);
  for (std::size_t f = 0; f < n; ++f) {
    g.frames[f].v = x[f];
    if (cfa) g.frames[f].hdr.bayer = "RGGB";
  }
  poke_nonfinite(g);
  return g;
}

group flat_group(const sc::flat_set& s, const sc::frames& x) {
  group g = make_group(master_role::flat, s.set.width, s.set.height, s.frames());
  for (std::size_t f = 0; f < s.frames(); ++f) {
    g.frames[f].hdr.offset = sc::exact_double(s.offset, "offset");
    if (s.cfa) g.frames[f].hdr.bayer = "RGGB";
    g.frames[f].v = x[f];
  }
  poke_nonfinite(g);
  return g;
}

// ---------------------------------------------------------------- esecuzione del job

struct run_cfg {
  unsigned workers;
  std::uint32_t rows;
};
constexpr std::array<run_cfg, 6> kConfigs = {run_cfg{1, 1}, run_cfg{4, 1}, run_cfg{1, 7},
                                             run_cfg{4, 7}, run_cfg{1, 64}, run_cfg{4, 64}};

struct task_counter {
  std::atomic<std::size_t> started{0};
  task_counter() {
    cpu_test::set_task_injector([this](std::string_view point) {
      if (point == "cpu.task_start") started.fetch_add(1);
      return 0;
    });
  }
  ~task_counter() { cpu_test::set_task_injector(nullptr); }
  task_counter(const task_counter&) = delete;
  task_counter& operator=(const task_counter&) = delete;
};

struct job_run {
  image decoded;
  std::string sha;
  job_plan plan;
  std::size_t tasks = 0;
};

// Piano atteso: `plan_resources` sugli ingressi del job (come `expect_job_plan` di test_master.cpp).
resource_plan expected_plan(const group& g, const master_recipe& r, std::uint64_t budget, const resource_config& config) {
  planner_input in;
  in.budget_bytes = budget;
  in.cpus = cpu_count();
  in.width = g.w;
  in.height = g.h;
  in.out_sample_bytes = sample_size(r.output_type);
  for (const mframe& f : g.frames) {
    in.input_peak_bytes = std::max(in.input_peak_bytes, plan_fits(file_of(f, g.w, g.h)).peak_bytes);
  }
  in.header_bound_bytes = 3 * (std::uint64_t{fits_limits::max_header_cards} * 80 + 2 * 2880);
  in.frame_count = g.frames.size();
  in.master_row_channels = g.frames.front().hdr.bayer.empty() ? 1 : 2;
  return plan_resources(in, config);
}

std::optional<job_run> run_job(const fs::path& root, const group& g, const master_recipe& r, std::uint64_t budget,
                               const std::optional<run_cfg>& cfg) {
  auto store = sqlite_dag_store::create(root);
  const node_spec node = make_master_node(r);
  store->put_node(node);
  store->create_slot("master", node.key());
  memory_pool pool(budget);
  master_job_options o;
  if (cfg) {
    o.resources.worker_count = cfg->workers;
    o.resources.tile_rows = cfg->rows;
  }
  std::optional<job_plan> seen;
  o.on_job_plan = [&](const job_plan& p) { seen = p; };
  task_counter counter;
  const job_outcome out = run_master_quiet(*store, "master", r, pool, o);
  if (out.exit_code != 0) {
    ADD_FAILURE() << "job fallito: " << (out.stop ? out.stop->code() + " — " + out.stop->what() : "senza stop");
    return std::nullopt;
  }
  if (!seen) {
    ADD_FAILURE() << "piano del job non osservato";
    return std::nullopt;
  }
  const resource_plan expected = expected_plan(g, r, budget, o.resources);
  EXPECT_EQ(seen->shared_bytes, expected.shared_bytes);
  EXPECT_EQ(seen->per_task_bytes, expected.per_task_bytes);
  EXPECT_EQ(seen->worker_count, expected.worker_count);
  EXPECT_EQ(seen->tile_rows, expected.tile_rows);
  EXPECT_EQ(seen->queue_capacity, expected.task_queue_capacity);
  EXPECT_LE(pool.peak_reserved_bytes(), budget);
  const auto roles = by_role(*store, node.key());
  job_run run;
  run.decoded = decode_fits(read_artifact(*store, roles.at("primary")), cal::big_budget);
  run.sha = roles.at("primary").sha256;
  run.plan = *seen;
  run.tasks = counter.started.load();
  return run;
}

std::optional<master_result> memory_master(const group& g, const master_recipe& r, const std::string& what) {
  try {
    return combine_master(r, images_of(g));
  } catch (const stop_error& e) {
    ADD_FAILURE() << what << ": stop " << e.code() << " — " << e.what();
    return std::nullopt;
  }
}

std::size_t invalid_pixels(const image& im) {
  return static_cast<std::size_t>(
      std::count_if(im.invalid.bits.begin(), im.invalid.bits.end(), [](std::uint16_t b) { return b != 0; }));
}

// Master, piano e maschera del job con ogni configurazione uguali bit per bit alla memoria; stesso digest dell'artefatto.
void expect_invariant(const std::string& what, const group& g, const master_recipe& r) {
  SCOPED_TRACE(what);
  const std::optional<master_result> memory = memory_master(g, r, what);
  if (!memory) return;
  // Ogni pixel marcato (un campione NaN per riga di confine, posizioni distinte) è invalido nel master.
  for (std::size_t k = 0; k < kPokeRows.size(); ++k) {
    const std::size_t i = kPokeRows[k] * g.w + (3 + 5 * k) % g.w;
    ASSERT_NE(memory->output.invalid.bits[i], 0) << "pixel marcato " << i << " valido";
  }
  ASSERT_GE(invalid_pixels(memory->output), kPokeRows.size()) << "la maschera deve avere i pixel marcati";
  ASSERT_LT(invalid_pixels(memory->output), memory->output.invalid.bits.size()) << "la maschera non deve essere piena";
  const fs::path dir = fresh_dir("b10-" + what);
  master_recipe rr = r;
  write_frames(g, rr, dir, "frame");
  std::set<std::string> digests;
  for (const run_cfg& c : kConfigs) {
    SCOPED_TRACE(std::to_string(c.workers) + " worker, tile_rows " + std::to_string(c.rows));
    const std::optional<job_run> run =
        run_job(dir / ("w" + std::to_string(c.workers) + "-t" + std::to_string(c.rows)), g, rr, 256 * kMiB, c);
    if (!run) continue;
    EXPECT_EQ(run->plan.worker_count, c.workers);
    EXPECT_EQ(run->plan.tile_rows, c.rows);
    EXPECT_EQ(run->tasks, (std::size_t{g.h} + c.rows - 1) / c.rows) << "un task per blocco di righe";
    EXPECT_TRUE(same_bits(run->decoded, memory->output));
    digests.insert(run->sha);
  }
  EXPECT_EQ(digests.size(), 1u);
}

// ---------------------------------------------------------------- T-M3B-C02-10, parte metodi

void c02_10_method(kind k) {
  const sc::gaussian_cases cases = sc::c02_10_cases();
  const std::vector<sc::frames> x = sc::realize_cases(cases);
  for (std::size_t c = 0; c < cases.sets.size(); ++c) {
    const std::size_t n = k == kind::single ? 1 : cases.n[c];
    if (k == kind::single && c != 0) continue;  // un solo frame: un solo caso
    for (const bool cfa : {false, true}) {
      const std::string tag = std::string(name_of(k)) + "-n" + std::to_string(n) + (cfa ? "-cfa" : "-mono");
      sc::frames frames = x[c];
      frames.resize(n);
      const group g = dark_group(frames, cases.sets[c].width, cases.sets[c].height, n, cfa);
      expect_invariant(tag, g, dark_recipe(g, k, sample_type::f64));
    }
  }
}

TEST(MasterB10, T_M3B_C02_10_single_is_bit_identical_across_tile_rows_workers_blocks_and_memory) {
  c02_10_method(kind::single);
}
TEST(MasterB10, T_M3B_C02_10_mean_is_bit_identical_across_tile_rows_workers_blocks_and_memory) { c02_10_method(kind::mean); }
TEST(MasterB10, T_M3B_C02_10_weighted_mean_is_bit_identical_across_tile_rows_workers_blocks_and_memory) {
  c02_10_method(kind::weighted);
}
TEST(MasterB10, T_M3B_C02_10_median_is_bit_identical_across_tile_rows_workers_blocks_and_memory) {
  c02_10_method(kind::median);
}
TEST(MasterB10, T_M3B_C02_10_winsorized_is_bit_identical_across_tile_rows_workers_blocks_and_memory) {
  c02_10_method(kind::winsorized);
}
TEST(MasterB10, T_M3B_C02_10_sigma_clip_is_bit_identical_across_tile_rows_workers_blocks_and_memory) {
  c02_10_method(kind::sigma_clip);
}

// ---------------------------------------------------------------- T-M3B-FLAT-08, parte valori

void flat_08_method(kind k) {
  const sc::flat_cases cases = sc::flat_08_cases();
  const std::vector<sc::frames> x = sc::realize_cases(cases);
  for (std::size_t c = 0; c < cases.sets.size(); ++c) {
    const std::string tag = std::string("flat-") + name_of(k) + "-" + std::to_string(c) + "-" + cases.names[c];
    std::string safe = tag;
    std::replace(safe.begin(), safe.end(), '/', '-');
    std::replace(safe.begin(), safe.end(), '=', '_');
    const group g = flat_group(cases.sets[c], x[c]);
    expect_invariant(safe, g, flat_ratio_recipe(g, k, sample_type::f64));
  }
}

TEST(MasterB10, T_M3B_FLAT_08_median_ratio_mean_is_bit_identical_across_tile_rows_workers_blocks_and_memory) {
  flat_08_method(kind::mean);
}
TEST(MasterB10, T_M3B_FLAT_08_median_ratio_weighted_mean_is_bit_identical_across_tile_rows_workers_blocks_and_memory) {
  flat_08_method(kind::weighted);
}
TEST(MasterB10, T_M3B_FLAT_08_median_ratio_median_is_bit_identical_across_tile_rows_workers_blocks_and_memory) {
  flat_08_method(kind::median);
}
TEST(MasterB10, T_M3B_FLAT_08_median_ratio_winsorized_is_bit_identical_across_tile_rows_workers_blocks_and_memory) {
  flat_08_method(kind::winsorized);
}
TEST(MasterB10, T_M3B_FLAT_08_median_ratio_sigma_clip_is_bit_identical_across_tile_rows_workers_blocks_and_memory) {
  flat_08_method(kind::sigma_clip);
}

// ---------------------------------------------------------------- uscita f32

TEST(MasterB10, T_M3B_C02_10_f32_output_is_bit_identical_across_tile_rows_workers_blocks_and_memory) {
  const sc::gaussian_cases cases = sc::c02_10_cases();
  const std::vector<sc::frames> x = sc::realize_cases(cases);
  const std::size_t c = cases.sets.size() - 1;  // n = 11
  for (const kind k : {kind::single, kind::mean, kind::weighted, kind::median, kind::winsorized, kind::sigma_clip}) {
    for (const bool cfa : {false, true}) {
      const std::size_t n = k == kind::single ? 1 : cases.n[c];
      sc::frames frames = x[c];
      frames.resize(n);
      const group g = dark_group(frames, cases.sets[c].width, cases.sets[c].height, n, cfa);
      expect_invariant(std::string("f32-") + name_of(k) + (cfa ? "-cfa" : "-mono"), g, dark_recipe(g, k, sample_type::f32));
    }
  }
}

TEST(MasterB10, T_M3B_FLAT_08_f32_output_is_bit_identical_across_tile_rows_workers_blocks_and_memory) {
  const sc::flat_cases cases = sc::flat_08_cases();
  const std::vector<sc::frames> x = sc::realize_cases(cases);
  for (std::size_t c = 0; c < cases.sets.size(); ++c) {
    if (cases.names[c].rfind("n=4/", 0) != 0) continue;
    const group g = flat_group(cases.sets[c], x[c]);
    for (const kind k : {kind::mean, kind::weighted, kind::median, kind::winsorized, kind::sigma_clip}) {
      expect_invariant(std::string("f32-flat-") + name_of(k) + (cases.sets[c].cfa ? "-cfa" : "-mono"), g,
                       flat_ratio_recipe(g, k, sample_type::f32));
    }
  }
}

// ---------------------------------------------------------------- T-M3B-MEM-01 e T-M2R2R-02 sui metodi

struct budget_case {
  std::string tag;
  group g;
  master_recipe r;
  std::uint64_t cr;
};

std::vector<budget_case> budget_cases() {
  std::vector<budget_case> out;
  const sc::gaussian_cases dc = sc::c02_10_cases();
  const std::vector<sc::frames> dx = sc::realize_cases(dc);
  const std::size_t d = dc.sets.size() - 1;  // n = 11
  for (const kind k : {kind::single, kind::mean, kind::weighted, kind::median, kind::winsorized, kind::sigma_clip}) {
    for (const bool cfa : {false, true}) {
      const std::size_t n = k == kind::single ? 1 : dc.n[d];
      sc::frames frames = dx[d];
      frames.resize(n);
      group g = dark_group(frames, dc.sets[d].width, dc.sets[d].height, n, cfa);
      master_recipe r = dark_recipe(g, k, sample_type::f64);
      out.push_back({std::string("dark-") + name_of(k) + (cfa ? "-cfa" : "-mono"), std::move(g), std::move(r), cfa ? 2u : 1u});
    }
  }
  const sc::flat_cases fc = sc::flat_08_cases();
  const std::vector<sc::frames> fx = sc::realize_cases(fc);
  for (std::size_t c = 0; c < fc.sets.size(); ++c) {
    if (fc.names[c].rfind("n=11/", 0) != 0) continue;
    for (const kind k : {kind::mean, kind::weighted, kind::median, kind::winsorized, kind::sigma_clip}) {
      group g = flat_group(fc.sets[c], fx[c]);
      master_recipe r = flat_ratio_recipe(g, k, sample_type::f64);
      out.push_back({std::string("flat-ratio-") + name_of(k) + (fc.sets[c].cfa ? "-cfa" : "-mono"), std::move(g),
                     std::move(r), fc.sets[c].cfa ? 2u : 1u});
    }
  }
  return out;
}

TEST(MasterB10, T_M3B_MEM_01_job_minimum_is_the_model_for_every_method_and_the_job_completes_at_the_minimum) {
  for (budget_case& c : budget_cases()) {
    SCOPED_TRACE(c.tag);
    const std::optional<master_result> memory = memory_master(c.g, c.r, c.tag);
    if (!memory) continue;
    const fs::path dir = fresh_dir("b10-mem-" + c.tag);
    write_frames(c.g, c.r, dir, "frame");
    const std::uint64_t minimum = expected_minimum(c.g, c.r, c.cr);

    // Minimo − 1: stop dichiarato con il minimo del modello, prima di ogni lettura e prenotazione.
    {
      auto store = sqlite_dag_store::create(dir / "below");
      const node_spec node = make_master_node(c.r);
      store->put_node(node);
      store->create_slot("master", node.key());
      bool frames_read = false;
      master_job_options o;
      o.hook = [&](std::string_view point) { frames_read = frames_read || point == "frames.read"; };
      memory_pool pool(minimum - 1);
      const job_outcome out = run_master_quiet(*store, "master", c.r, pool, o);
      EXPECT_EQ(out.exit_code, exit_code_stop);
      ASSERT_TRUE(out.stop.has_value());
      EXPECT_EQ(out.stop->category(), error_category::budget);
      EXPECT_EQ(out.stop->code(), "sched.job_exceeds_budget");
      std::optional<std::string> declared;
      for (const auto& [key, value] : out.stop->context()) {
        if (key == "minimum_feasible_bytes") declared = value;
      }
      EXPECT_EQ(declared, std::optional<std::string>(std::to_string(minimum)));
      EXPECT_FALSE(frames_read);
      EXPECT_EQ(pool.peak_reserved_bytes(), 0u);
      EXPECT_EQ(by_role(*store, node.key()).count("primary"), 0u);
    }

    // Budget uguale al minimo: tile 1, un worker, master della memoria.
    const std::optional<job_run> at = run_job(dir / "at", c.g, c.r, minimum, std::nullopt);
    if (!at) continue;
    EXPECT_EQ(at->plan.tile_rows, 1u);
    EXPECT_EQ(at->plan.worker_count, 1u);
    EXPECT_EQ(at->tasks, std::size_t{c.g.h});
    EXPECT_TRUE(same_bits(at->decoded, memory->output));
  }
}

TEST(MasterB10, T_M2R2R_02_methods_with_automatic_workers_and_tile_give_the_master_of_the_memory) {
  for (budget_case& c : budget_cases()) {
    SCOPED_TRACE(c.tag);
    const std::optional<master_result> memory = memory_master(c.g, c.r, c.tag);
    if (!memory) continue;
    const fs::path dir = fresh_dir("b10-auto-" + c.tag);
    write_frames(c.g, c.r, dir, "frame");
    const std::optional<job_run> run = run_job(dir / "auto", c.g, c.r, 256 * kMiB, std::nullopt);
    if (!run) continue;
    EXPECT_TRUE(same_bits(run->decoded, memory->output));
  }
}

}  // namespace
