// ADR-06, estensione R3.2, sul job di calibrazione reale: residenza a blocchi (T-M2R3-01), ENOSPC (T-M2R3-05),
// checkpoint e resume (T-M2R3-06), pressione all'ammissione (T-M2R3-07).
#include <gtest/gtest.h>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <fstream>
#include <map>
#include <thread>

#include "astro/fits_blocks.hpp"
#include "astro/hash.hpp"
#include "calib_support.hpp"
#include "m2d_support.hpp"

namespace {

using namespace cal;
using m2d::fake_system;
using m2d::wait_until;

constexpr std::uint64_t MiB = std::uint64_t{1} << 20;
constexpr std::uint64_t HB = 3 * (std::uint64_t{fits_limits::max_header_cards} * 80 + 2 * 2880);

struct disk_case {
  fs::path dir;
  set_def s;
  calibration_recipe recipe;
};

using file_builder = std::function<fx::bytes(const frame_def&, std::uint32_t, std::uint32_t, int)>;

fx::bytes default_builder(const frame_def& f, std::uint32_t w, std::uint32_t h, int) { return file_of(f, w, h); }

disk_case make_case(const std::string& name, set_def s, const file_builder& builder = default_builder) {
  disk_case c;
  c.dir = fresh_dir("r32/" + name);
  c.s = std::move(s);
  c.recipe = recipe_of(c.s);
  const std::pair<const char*, const frame_def*> frames[4] = {
      {"light.fits", &c.s.light}, {"dark.fits", &c.s.dark}, {"flat.fits", &c.s.flat}, {"dark_flat.fits", &c.s.dark_flat}};
  calibration_input_spec* specs[4] = {&c.recipe.light, &c.recipe.dark, &c.recipe.flat, &c.recipe.dark_flat};
  for (int i = 0; i < 4; ++i) {
    const fx::bytes bytes = builder(*frames[i].second, c.s.w, c.s.h, i);
    write_bytes(c.dir / frames[i].first, bytes);
    specs[i]->path = (c.dir / frames[i].first).string();
    specs[i]->digest = digest_of(bytes);
  }
  fs::create_directories(c.dir / "scratch");
  return c;
}

struct project {
  std::unique_ptr<sqlite_dag_store> store;
  node_spec node;
};

project make_project(const disk_case& c, const std::string& name) {
  project p{sqlite_dag_store::create(c.dir / name), make_calibration_node(c.recipe)};
  p.store->put_node(p.node);
  p.store->create_slot("cal", p.node.key());
  return p;
}

const cancel_token& never_cancelled() {
  static const cancel_token token;
  return token;
}

job_outcome run_quiet(project& p, const disk_case& c, memory_pool& pool, const calibration_job_options& o,
                      const cancel_token& cancel = never_cancelled()) {
  testing::internal::CaptureStderr();
  try {
    job_outcome out = run_calibration_job(*p.store, "cal", c.recipe, pool, o, cancel);
    testing::internal::GetCapturedStderr();
    return out;
  } catch (...) {
    testing::internal::GetCapturedStderr();
    throw;
  }
}

std::map<std::string, std::string> digests_by_role(const project& p) {
  std::map<std::string, std::string> out;
  for (const artifact_record& a : p.store->artifacts_for(p.node.key())) {
    if (a.output_role != "provenance") out[a.output_role] = a.sha256;
  }
  return out;
}

void expect_only_stop_provenance(const project& p) {
  const std::vector<artifact_record> all = p.store->artifacts_for(p.node.key());
  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all[0].output_role, "provenance");
  const fs::path dir = p.store->root() / "artifacts";
  EXPECT_EQ(m2d::entry_count(dir), 1u) << "nessun temporaneo fra gli artefatti";
}

std::string context_of(const stop_error& e, const std::string& key) {
  for (const auto& [k, v] : e.context()) {
    if (k == key) return v;
  }
  return "<assente>";
}

resource_config config_for(const disk_case& c, std::uint64_t quota = 1ull << 32) {
  resource_config cfg;
  cfg.scratch = dir_quota{fs::absolute(c.dir / "scratch"), quota};
  return cfg;
}

// Formule del contratto (ADR-06, estensione R3.2), ricalcolate dal test sui file su disco.
struct formula {
  std::uint64_t whole_min = 0, blocks_shared = 0, blocks_per_task = 0, blocks_min = 0, staging = 0;
};

formula formula_for(const disk_case& c, std::uint32_t t, bool planes, std::uint32_t channels = 1) {
  const std::uint64_t w = c.s.w, h = c.s.h, s = w * h;
  std::uint64_t p = 0, raw = 0, band = 0, regions = 0;
  std::uint64_t e_out = 0;
  int k = 0;
  for (const calibration_input_spec* spec : {&c.recipe.light, &c.recipe.dark, &c.recipe.flat, &c.recipe.dark_flat}) {
    const fits_input in = open_fits_input(spec->path);
    const fits_plan plan = plan_fits(in);
    p += plan.peak_bytes;
    const std::uint64_t e = sample_size(plan.type);
    if (k++ == 0) e_out = e;
    const std::uint64_t b = primary_sample_bytes(in);
    raw += w * (b + (plan.has_invalid ? 2 : 0) + (planes ? e : 0));
    band += w * (e + 2 + (planes ? e : 0));
    regions += 1 + (plan.has_invalid ? 1 : 0) + (planes ? 1 : 0);
  }
  const std::uint64_t m = 1 * MiB;
  const std::uint64_t out_buffers = s * (2 * e_out + 2);
  formula f;
  f.whole_min = p + out_buffers + (out_buffers + HB) + 8 * s + m + 18 * w;
  const std::uint64_t tasks = (h + t - 1) / t;
  const std::uint64_t in_band = t * (raw + band);
  f.blocks_per_task = in_band + t * w * (2 * e_out + 2) + 18 * w;
  const std::uint64_t tpl = MiB + 2880 + 2 * (HB + 3 * 2880 * ((8 * w + 2879) / 2880)) + 18 * w;
  f.blocks_shared = in_band + 32 * tasks * regions + channels * 4 * 256 * 8 + tpl + m;
  f.blocks_min = f.blocks_shared + f.blocks_per_task;
  f.staging = s * (2 * e_out + 2);
  return f;
}

std::uint64_t status_kib(const char* key) {
  std::ifstream in("/proc/self/status");
  std::string line;
  const std::string prefix = std::string(key) + ":";
  while (std::getline(in, line)) {
    if (line.rfind(prefix, 0) == 0) return std::stoull(line.substr(prefix.size()));
  }
  ADD_FAILURE() << key << " assente";
  return 0;
}

// ---------------------------------------------------------------- T-M2R3-01

TEST(CalibBlocks, T_M2R3_01_dataset_eight_times_the_budget_is_bit_identical_to_the_ample_budget) {
  const disk_case c = make_case("big", synth_set(1024, 1024, -64));
  constexpr std::uint32_t tile = 16;
  const formula f = formula_for(c, tile, false);
  const std::uint64_t budget = 12 * MiB;
  ASSERT_GE(f.whole_min, 8 * budget) << "il working set deve essere almeno 8 volte il budget";
  ASSERT_LE(f.blocks_min, budget);

  resource_config cfg = config_for(c);
  cfg.tile_rows = tile;
  std::map<std::string, std::string> blocks_digests;
  {
    project p = make_project(c, "blocks");
    memory_pool pool(budget);
    calibration_job_options o;
    o.resources = cfg;
    std::optional<job_plan> seen;
    o.on_job_plan = [&](const job_plan& jp) { seen = jp; };
    const std::uint64_t rss_before = status_kib("VmRSS"), hwm_before = status_kib("VmHWM");
    const job_outcome out = run_quiet(p, c, pool, o);
    const std::uint64_t rss_after = status_kib("VmRSS"), hwm_after = status_kib("VmHWM");
    RecordProperty("rss_kib_before", std::to_string(rss_before));
    RecordProperty("rss_kib_after", std::to_string(rss_after));
    RecordProperty("hwm_kib_before", std::to_string(hwm_before));
    RecordProperty("hwm_kib_after", std::to_string(hwm_after));
    RecordProperty("working_set_bytes", std::to_string(f.whole_min));
    RecordProperty("budget_bytes", std::to_string(budget));
    RecordProperty("pool_peak_bytes", std::to_string(pool.peak_reserved_bytes()));
    ASSERT_EQ(out.exit_code, 0) << (out.stop ? out.stop->what() : "");
    ASSERT_TRUE(seen);
    ASSERT_TRUE(seen->spill) << "residenza a blocchi: lo spill e' dichiarato nel piano";
    EXPECT_EQ(seen->tile_rows, tile);
    EXPECT_EQ(seen->shared_bytes, f.blocks_shared);
    EXPECT_EQ(seen->per_task_bytes, f.blocks_per_task);
    EXPECT_GT(pool.peak_reserved_bytes(), 0u);
    EXPECT_LE(pool.peak_reserved_bytes(), budget);
    EXPECT_EQ(pool.reserved_bytes(), 0u);
    blocks_digests = digests_by_role(p);

    // Tolleranze di T-M2E-02 (T-M2E-03: C = S·N/K con K = 1) su un sottoinsieme di pixel, con N esatto.
    std::vector<double> q;
    q.reserve(c.s.flat.v.size());
    for (std::size_t i = 0; i < c.s.flat.v.size(); ++i) q.push_back(c.s.flat.v[i] - c.s.dark_flat.v[i]);
    std::sort(q.begin(), q.end());
    const double n = (q[q.size() / 2 - 1] + q[q.size() / 2]) / 2;
    for (const artifact_record& a : p.store->artifacts_for(p.node.key())) {
      if (a.output_role != "primary") continue;
      const std::string text = read_text(p.store->artifact_path(a.rel_path));
      const image out_img = decode_fits(std::vector<std::uint8_t>(text.begin(), text.end()), big_budget);
      for (std::size_t i = 0; i < c.s.light.v.size(); i += 1009) {
        const std::uint32_t x = static_cast<std::uint32_t>(i % 1024), y = static_cast<std::uint32_t>(i / 1024);
        const rat truth = exact::from_double(fx::synth_sky(x, y, 0) / 8192.0) * exact::from_double(n);
        ASSERT_TRUE(close_to(truth, sample_at(out_img, i), 6, false)) << i;
      }
    }
  }
  {
    project p = make_project(c, "whole");
    memory_pool pool(big_budget);
    calibration_job_options o;
    o.resources = cfg;
    std::optional<job_plan> seen;
    o.on_job_plan = [&](const job_plan& jp) { seen = jp; };
    const job_outcome out = run_quiet(p, c, pool, o);
    ASSERT_EQ(out.exit_code, 0);
    ASSERT_TRUE(seen);
    EXPECT_FALSE(seen->spill) << "budget ampio: residenza intera";
    EXPECT_EQ(digests_by_role(p), blocks_digests);
    EXPECT_EQ(blocks_digests.size(), 3u);
  }
  EXPECT_EQ(m2d::entry_count(c.dir / "scratch"), 0u);
}

// Frame con INVALID e VARIANCE, CFA, saturazione: tutto quello che la lettura a blocchi deve riprodurre.
fx::bytes planes_builder(const frame_def& f, std::uint32_t w, std::uint32_t h, int index) {
  fx::raw_hdu p;
  p.bitpix = -32;
  p.axes = {w, h};
  p.cards = fx::calib_cards(f.hdr);
  for (const double v : f.v) fx::put_be(p.data, fx::bits_of(static_cast<float>(v)), 4);
  std::vector<fx::raw_hdu> hdus = {p};
  if (index <= 1) {  // light e dark con INVALID
    fx::raw_hdu m;
    m.primary = false;
    m.bitpix = 16;
    m.axes = {w, h};
    m.cards = {fx::card_real("BZERO", 32768.0), fx::card_str("EXTNAME", "INVALID")};
    for (std::size_t i = 0; i < f.v.size(); ++i) {
      const std::uint16_t bits = (i + static_cast<std::size_t>(index)) % 23 == 0 ? invalid_reason::source_flagged : 0;
      fx::put_be(m.data, static_cast<std::uint16_t>(bits ^ 0x8000u), 2);
    }
    hdus.push_back(m);
  }
  fx::raw_hdu v;
  v.primary = false;
  v.bitpix = -32;
  v.axes = {w, h};
  v.cards = {fx::card_str("EXTNAME", "VARIANCE")};
  for (std::size_t i = 0; i < f.v.size(); ++i) {
    fx::put_be(v.data, fx::bits_of(static_cast<float>(4.0 + static_cast<double>(i % 7) / 4)), 4);
  }
  hdus.push_back(v);
  return fx::build_fits(hdus);
}

set_def cfa_set(std::uint32_t w, std::uint32_t h) {
  set_def s = synth_set(w, h, -32);
  for (frame_def* fr : {&s.light, &s.dark, &s.flat, &s.dark_flat}) fr->hdr.bayer = "RGGB";
  s.light.hdr.saturation = 60000;
  s.flat.hdr.saturation = 11500;
  s.light.v[11] = 61000;  // saturato
  s.flat.v[40] = 11600;   // saturato: escluso da N e marcato nell'uscita
  s.flat.v[5] = s.dark_flat.v[5];
  return s;
}

void use_planes(calibration_recipe& r, normalization_mode mode) {
  for (calibration_input_spec* in : {&r.light, &r.dark, &r.flat, &r.dark_flat}) {
    in->variance = variance_spec{variance_source::plane, std::nullopt};
  }
  r.normalization = mode;
}

TEST(CalibBlocks, T_M2R3_01_cfa_planes_invalid_and_saturation_are_identical_between_residencies) {
  disk_case c = make_case("cfa", cfa_set(37, 29), planes_builder);
  use_planes(c.recipe, normalization_mode::cfa_channel_median);
  std::map<std::string, std::string> reference;
  {
    project p = make_project(c, "whole");
    memory_pool pool(big_budget);
    calibration_job_options o;
    o.resources = config_for(c);
    const job_outcome out = run_quiet(p, c, pool, o);
    ASSERT_EQ(out.exit_code, 0) << (out.stop ? out.stop->what() : "");
    reference = digests_by_role(p);
    ASSERT_EQ(reference.size(), 3u);
  }
  int run = 0;
  for (const std::uint32_t tile : {1u, 3u, 7u, 29u}) {
    for (const unsigned workers : {1u, 2u, 3u}) {
      SCOPED_TRACE("tile " + std::to_string(tile) + " worker " + std::to_string(workers));
      project p = make_project(c, "blocks" + std::to_string(run));
      memory_pool pool(big_budget);
      calibration_job_options o;
      o.resources = config_for(c);
      o.resources->tile_rows = tile;
      o.resources->worker_count = workers;
      o.checkpoint = calibration_job_options::checkpoint_options{"cfa" + std::to_string(run++), 1000, false};
      std::optional<job_plan> seen;
      o.on_job_plan = [&](const job_plan& jp) { seen = jp; };
      const job_outcome out = run_quiet(p, c, pool, o);
      ASSERT_EQ(out.exit_code, 0) << (out.stop ? out.stop->what() : "");
      ASSERT_TRUE(seen && seen->spill);
      EXPECT_EQ(digests_by_role(p), reference);
    }
  }
  EXPECT_EQ(m2d::entry_count(c.dir / "scratch"), 0u) << "checkpoint e staging rimossi dopo la pubblicazione";
}

// OB-20: il FITS pubblicato, a residenza intera e a blocchi, porta esattamente l'unione dei codici dei quattro ingressi.
TEST(CalibBlocks, T_M3BASE_23_OB_20_published_measure_is_the_exact_union_in_both_residencies) {
  set_def s = synth_set(12, 9, -32);
  add_distinct_overlapping_measures(s);
  const disk_case c = make_case("ob20_union", s);
  for (const bool blocks : {false, true}) {
    project p = make_project(c, blocks ? "blocks" : "whole");
    memory_pool pool(big_budget);
    calibration_job_options o;
    o.resources = config_for(c);
    if (blocks) o.checkpoint = calibration_job_options::checkpoint_options{"ob20", 1000, false};
    std::optional<job_plan> seen;
    o.on_job_plan = [&](const job_plan& jp) { seen = jp; };
    const job_outcome out = run_quiet(p, c, pool, o);
    ASSERT_EQ(out.exit_code, 0) << blocks << (out.stop ? out.stop->what() : "");
    ASSERT_TRUE(seen);
    EXPECT_EQ(seen->spill.has_value(), blocks) << "residenza effettiva diversa da quella del caso";
    int images = 0;
    for (const artifact_record& a : p.store->artifacts_for(p.node.key())) {
      if (a.output_role != "primary") continue;
      ++images;
      const std::string text = read_text(p.store->artifact_path(a.rel_path));
      const image img = decode_fits(std::vector<std::uint8_t>(text.begin(), text.end()), big_budget);
      ASSERT_TRUE(img.measure.has_value()) << blocks;
      EXPECT_EQ(img.measure->limits, distinct_overlapping_union()) << blocks;
    }
    EXPECT_EQ(images, 1) << blocks;
  }
}

stop_error stop_of(const disk_case& c, const std::string& name, bool blocks) {
  project p = make_project(c, name);
  memory_pool pool(big_budget);
  calibration_job_options o;
  o.resources = config_for(c);
  if (blocks) o.checkpoint = calibration_job_options::checkpoint_options{name, 1000, false};
  std::optional<job_plan> seen;
  o.on_job_plan = [&](const job_plan& jp) { seen = jp; };
  const job_outcome out = run_quiet(p, c, pool, o);
  EXPECT_TRUE(seen) << "l'arresto atteso viene dopo il piano";
  if (seen) {
    EXPECT_EQ(seen->spill.has_value(), blocks) << "residenza effettiva diversa da quella del caso";
  }
  EXPECT_EQ(out.exit_code, 2);
  expect_only_stop_provenance(p);
  if (!out.stop) return stop_error(error_category::integrity, "test.no_stop", "nessuno stop");
  return *out.stop;
}

TEST(CalibBlocks, T_M2R3_01_stops_are_the_same_in_both_residencies) {
  {
    set_def s = cfa_set(12, 9);
    disk_case c = make_case("stop_var", s, [](const frame_def& f, std::uint32_t w, std::uint32_t h, int index) {
      fx::bytes b = planes_builder(f, w, h, index);
      if (index == 1) {  // varianza negativa sul pixel 17 del dark
        const std::size_t var_hdu_data = b.size() - 2880;
        const std::uint32_t neg = fx::bits_of(-1.0f);
        for (int k = 0; k < 4; ++k) b[var_hdu_data + 17 * 4 + k] = static_cast<std::uint8_t>(neg >> (24 - 8 * k));
      }
      return b;
    });
    use_planes(c.recipe, normalization_mode::global_median);
    const stop_error whole = stop_of(c, "whole", false), blocks = stop_of(c, "blocks", true);
    EXPECT_EQ(whole.code(), "var.plane_values");
    EXPECT_EQ(blocks.code(), whole.code());
    EXPECT_EQ(context_of(blocks, "indice"), "17");
    EXPECT_EQ(context_of(blocks, "input"), context_of(whole, "input"));
  }
  {
    set_def s = synth_set(10, 8, -32);
    s.flat.v = s.dark_flat.v;  // Fc = 0 ovunque: N non positiva
    const disk_case c = make_case("stop_n", s);
    const stop_error whole = stop_of(c, "whole", false), blocks = stop_of(c, "blocks", true);
    EXPECT_EQ(whole.code(), "calib.normalization_invalid");
    EXPECT_EQ(blocks.code(), whole.code());
    EXPECT_EQ(context_of(blocks, "canale"), context_of(whole, "canale"));
    EXPECT_EQ(context_of(blocks, "N"), context_of(whole, "N"));
  }
}

TEST(CalibBlocks, T_M2R3_01_blocks_minimum_passes_one_byte_less_stops_and_scratch_is_required) {
  const disk_case c = make_case("minimum", synth_set(512, 512, -32));
  constexpr std::uint32_t tile = 16;
  const formula f = formula_for(c, tile, false);
  ASSERT_LT(f.blocks_min, f.whole_min);
  resource_config cfg = config_for(c);
  cfg.tile_rows = tile;
  {
    project p = make_project(c, "ok");
    memory_pool pool(f.blocks_min);
    calibration_job_options o;
    o.resources = cfg;
    const job_outcome out = run_quiet(p, c, pool, o);
    ASSERT_EQ(out.exit_code, 0) << (out.stop ? out.stop->what() : "");
    EXPECT_EQ(pool.peak_reserved_bytes(), f.blocks_min) << "un worker: shared_b + per_task_b";
  }
  const auto expect_stop = [&](const std::string& name, std::uint64_t budget, const resource_config& rc,
                               const char* code, const std::string& minimum) {
    project p = make_project(c, name);
    memory_pool pool(budget);
    calibration_job_options o;
    o.resources = rc;
    int prepared = 0;
    o.hook = [&](std::string_view what) { prepared += what == "preflight.done"; };
    const job_outcome out = run_quiet(p, c, pool, o);
    ASSERT_EQ(out.exit_code, 2);
    ASSERT_TRUE(out.stop);
    EXPECT_EQ(out.stop->code(), code);
    if (!minimum.empty()) {
      EXPECT_EQ(context_of(*out.stop, "minimum_feasible_bytes"), minimum);
    }
    EXPECT_EQ(prepared, 0);
    expect_only_stop_provenance(p);
  };
  expect_stop("one_less", f.blocks_min - 1, cfg, "sched.job_exceeds_budget", std::to_string(f.blocks_min));
  resource_config no_scratch = cfg;
  no_scratch.scratch.reset();
  expect_stop("no_scratch", f.blocks_min, no_scratch, "sched.job_exceeds_budget", std::to_string(f.whole_min));
  resource_config small_quota = config_for(c, f.staging - 1);
  small_quota.tile_rows = tile;
  expect_stop("quota", f.blocks_min, small_quota, "spill.quota_exceeded", "");
}

TEST(CalibBlocks, T_M2R3_01_inputs_changed_after_the_preflight_stop_without_publication) {
  const disk_case c = make_case("changed", synth_set(64, 40, -32));
  const auto run_with = [&](const std::string& name, const std::function<void()>& change) {
    project p = make_project(c, name);
    memory_pool pool(big_budget);
    calibration_job_options o;
    o.resources = config_for(c);
    o.resources->tile_rows = 4;
    o.checkpoint = calibration_job_options::checkpoint_options{name, 1000, false};
    o.hook = [&](std::string_view what) {
      if (what == "preflight.done") change();
    };
    const job_outcome out = run_quiet(p, c, pool, o);
    EXPECT_EQ(out.exit_code, 2);
    expect_only_stop_provenance(p);
    return out.stop;
  };
  const fx::bytes flat = file_of(c.s.flat, c.s.w, c.s.h);
  std::optional<stop_error> modified = run_with("modified", [&] {
    fx::bytes changed = flat;
    changed[changed.size() - 2880 - 10] ^= 0x40;
    write_bytes(c.recipe.flat.path, changed);
  });
  ASSERT_TRUE(modified);
  EXPECT_EQ(modified->category(), error_category::integrity);
  EXPECT_TRUE(modified->code() == "input.consumed_digest" || modified->code() == "input.modified") << modified->what();
  write_bytes(c.recipe.flat.path, flat);
  std::optional<stop_error> removed = run_with("removed", [&] { fs::remove(c.recipe.dark.path); });
  ASSERT_TRUE(removed);
  EXPECT_EQ(removed->code(), "input.missing");
}

// ---------------------------------------------------------------- T-M2R3-05

bool has_errno(const stop_error& e, int err) { return context_of(e, "errno") == std::to_string(err); }

TEST(CalibBlocks, T_M2R3_05_enospc_on_scratch_checkpoint_and_artifacts_stops_io_without_publication) {
  const disk_case c = make_case("enospc", synth_set(48, 40, -32));
  const auto options = [&](bool checkpoint, const std::string& name) {
    calibration_job_options o;
    o.resources = config_for(c);
    o.resources->tile_rows = 4;
    if (checkpoint) o.checkpoint = calibration_job_options::checkpoint_options{name, 2, false};
    return o;
  };
  {  // scratch, staging anonimo (blocchi scelti per budget): nessun file resta nello scratch
    const disk_case big = make_case("enospc_spill", synth_set(512, 512, -32));
    const formula f = formula_for(big, 4, false);
    ASSERT_LT(f.blocks_min, f.whole_min);
    project p = make_project(big, "spill");
    memory_pool tight(f.blocks_min);
    calibration_job_options o;
    o.resources = config_for(big);
    o.resources->tile_rows = 4;
    int calls = 0;
    o.fault = [&](std::string_view point) { return point == "spill.write" && ++calls == 3 ? ENOSPC : 0; };
    const job_outcome out = run_quiet(p, big, tight, o);
    ASSERT_EQ(out.exit_code, 2);
    ASSERT_TRUE(out.stop);
    EXPECT_EQ(out.stop->category(), error_category::io);
    EXPECT_EQ(out.stop->code(), "spill.write");
    EXPECT_TRUE(has_errno(*out.stop, ENOSPC)) << out.stop->what();
    EXPECT_GE(calls, 3);
    expect_only_stop_provenance(p);
    EXPECT_EQ(m2d::entry_count(big.dir / "scratch"), 0u);
    EXPECT_EQ(tight.reserved_bytes(), 0u);
  }
  {  // checkpoint: resta lo staging nominato, nessun checkpoint ne' temporaneo
    project p = make_project(c, "ckpt");
    memory_pool pool(big_budget);
    calibration_job_options o = options(true, "ckpt");
    o.fault = [&](std::string_view point) { return point == "checkpoint.write" ? ENOSPC : 0; };
    const job_outcome out = run_quiet(p, c, pool, o);
    ASSERT_EQ(out.exit_code, 2);
    ASSERT_TRUE(out.stop);
    EXPECT_EQ(out.stop->category(), error_category::io);
    EXPECT_TRUE(has_errno(*out.stop, ENOSPC)) << out.stop->what();
    expect_only_stop_provenance(p);
    EXPECT_TRUE(fs::exists(c.dir / "scratch" / "ckpt.stage"));
    EXPECT_FALSE(fs::exists(c.dir / "scratch" / "ckpt.ckpt.json"));
    EXPECT_EQ(m2d::entry_count(c.dir / "scratch"), 1u);
    fs::remove(c.dir / "scratch" / "ckpt.stage");
  }
  for (const bool blocks : {true, false}) {  // artefatti, in streaming e da memoria
    SCOPED_TRACE(blocks ? "blocchi" : "residenza intera");
    project p = make_project(c, blocks ? "art_blocks" : "art_whole");
    memory_pool pool(big_budget);
    calibration_job_options o = options(blocks, "art");
    int calls = 0;
    p.store->set_fault_injector([&](std::string_view point) { return point == "artifact.write" && ++calls == 1 ? ENOSPC : 0; });
    const job_outcome out = run_quiet(p, c, pool, o);
    ASSERT_EQ(out.exit_code, 2);
    ASSERT_TRUE(out.stop);
    EXPECT_EQ(out.stop->category(), error_category::io);
    EXPECT_TRUE(has_errno(*out.stop, ENOSPC)) << out.stop->what();
    p.store->set_fault_injector({});
    expect_only_stop_provenance(p);
    if (blocks) {
      fs::remove(c.dir / "scratch" / "art.stage");
      fs::remove(c.dir / "scratch" / "art.ckpt.json");
    }
  }
  EXPECT_EQ(m2d::entry_count(c.dir / "scratch"), 0u);
}

// ---------------------------------------------------------------- T-M2R3-06

calibration_job_options checkpointed(const disk_case& c, const std::string& name, unsigned workers, bool resume) {
  calibration_job_options o;
  o.resources = config_for(c);
  o.resources->tile_rows = 4;
  o.resources->worker_count = workers;
  o.checkpoint = calibration_job_options::checkpoint_options{name, 2, resume};
  return o;
}

std::map<std::string, std::string> continuous_reference(const disk_case& c) {
  project p = make_project(c, "continuous");
  memory_pool pool(big_budget);
  const job_outcome out = run_quiet(p, c, pool, checkpointed(c, "continuous", 2, false));
  EXPECT_EQ(out.exit_code, 0);
  return digests_by_role(p);
}

TEST(CalibBlocks, T_M2R3_06_killed_process_and_identical_resume_give_the_continuous_output) {
  const disk_case c = make_case("kill", synth_set(40, 120, -32));
  const std::map<std::string, std::string> reference = continuous_reference(c);
  ASSERT_EQ(reference.size(), 3u);
  const fs::path root = c.dir / "killed";
  { project p = make_project(c, "killed"); }
  const pid_t pid = ::fork();
  ASSERT_GE(pid, 0);
  if (pid == 0) {
    auto store = sqlite_dag_store::open(root, open_intent::read_write);
    memory_pool pool(big_budget);
    calibration_job_options o = checkpointed(c, "killed", 2, false);
    int checkpoints = 0;
    o.fault = [&](std::string_view point) {
      if (point == "checkpoint.create" && ++checkpoints == 4) ::raise(SIGKILL);
      return 0;
    };
    (void)run_calibration_job(*store, "cal", c.recipe, pool, o, never_cancelled());
    ::_exit(3);  // il job non doveva terminare
  }
  int status = 0;
  ASSERT_EQ(::waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL) << "stato " << status;
  ASSERT_TRUE(fs::exists(c.dir / "scratch" / "killed.ckpt.json"));
  ASSERT_TRUE(fs::exists(c.dir / "scratch" / "killed.stage"));
  const std::optional<checkpoint_record> saved = read_checkpoint(c.dir / "scratch", "killed");
  ASSERT_TRUE(saved);
  EXPECT_GE(saved->completed.size(), 6u);

  project p{sqlite_dag_store::open(root, open_intent::read_write), make_calibration_node(c.recipe)};
  EXPECT_EQ(p.store->recovery().interrupted_jobs.size(), 1u);
  memory_pool pool(big_budget);
  const job_outcome out = run_quiet(p, c, pool, checkpointed(c, "killed", 2, true));
  ASSERT_EQ(out.exit_code, 0) << (out.stop ? out.stop->what() : "");
  EXPECT_EQ(digests_by_role(p), reference);
  ASSERT_TRUE(out.provenance.details);
  const json* sched = out.provenance.details->find("scheduler");
  ASSERT_NE(sched, nullptr);
  ASSERT_NE(sched->find("tasks_resumed"), nullptr);
  EXPECT_EQ(static_cast<std::size_t>(sched->find("tasks_resumed")->as_integer()), saved->completed.size());
  EXPECT_FALSE(fs::exists(c.dir / "scratch" / "killed.ckpt.json"));
  EXPECT_FALSE(fs::exists(c.dir / "scratch" / "killed.stage"));
}

// Esecuzione annullata dopo il primo checkpoint: checkpoint e staging restano.
void cancelled_run(const disk_case& c, const std::string& name) {
  project p = make_project(c, name);
  memory_pool pool(big_budget);
  cancel_token cancel;
  calibration_job_options o = checkpointed(c, name, 1, false);
  int checkpoints = 0;
  o.fault = [&](std::string_view point) {
    if (point == "checkpoint.create" && ++checkpoints == 2) cancel.request();
    return 0;
  };
  const job_outcome out = run_quiet(p, c, pool, o, cancel);
  EXPECT_EQ(out.exit_code, exit_code_cancelled);
  expect_only_stop_provenance(p);
  EXPECT_TRUE(fs::exists(c.dir / "scratch" / (name + ".ckpt.json")));
  EXPECT_TRUE(fs::exists(c.dir / "scratch" / (name + ".stage")));
}

TEST(CalibBlocks, T_M2R3_06_different_resume_is_resume_mismatch_and_altered_staging_is_checkpoint_digest) {
  const disk_case c = make_case("mismatch", synth_set(40, 60, -32));
  cancelled_run(c, "mm");
  {
    project p = make_project(c, "mm_resume_workers");
    memory_pool pool(big_budget);
    const job_outcome out = run_quiet(p, c, pool, checkpointed(c, "mm", 2, true));
    ASSERT_EQ(out.exit_code, 2);
    ASSERT_TRUE(out.stop);
    EXPECT_EQ(out.stop->category(), error_category::resume_mismatch);
    EXPECT_EQ(out.stop->code(), "sched.resume_recipe");
    expect_only_stop_provenance(p);
  }
  {
    std::fstream staging(c.dir / "scratch" / "mm.stage", std::ios::binary | std::ios::in | std::ios::out);
    staging.seekp(3);
    staging.put('\x55');
  }
  {
    project p = make_project(c, "mm_resume_altered");
    memory_pool pool(big_budget);
    const job_outcome out = run_quiet(p, c, pool, checkpointed(c, "mm", 1, true));
    ASSERT_EQ(out.exit_code, 2);
    ASSERT_TRUE(out.stop);
    EXPECT_EQ(out.stop->category(), error_category::integrity);
    EXPECT_EQ(out.stop->code(), "sched.checkpoint_digest");
  }
  EXPECT_TRUE(fs::exists(c.dir / "scratch" / "mm.ckpt.json")) << "un resume rifiutato non tocca il checkpoint";
}

TEST(CalibBlocks, T_M2R3_06_identical_resume_after_cancel_completes_and_checkpoint_requires_scratch) {
  const disk_case c = make_case("resume", synth_set(40, 60, -32));
  const std::map<std::string, std::string> reference = continuous_reference(c);
  cancelled_run(c, "rs");
  {
    project p = make_project(c, "rs_resume");
    memory_pool pool(big_budget);
    const job_outcome out = run_quiet(p, c, pool, checkpointed(c, "rs", 1, true));
    ASSERT_EQ(out.exit_code, 0) << (out.stop ? out.stop->what() : "");
    EXPECT_EQ(digests_by_role(p), reference);
  }
  EXPECT_EQ(m2d::entry_count(c.dir / "scratch"), 0u);
  for (const bool with_resources : {true, false}) {
    project p = make_project(c, with_resources ? "noscratch_res" : "noscratch");
    memory_pool pool(big_budget);
    calibration_job_options o;
    if (with_resources) o.resources = resource_config{};
    o.checkpoint = calibration_job_options::checkpoint_options{"x", 2, false};
    const job_outcome out = run_quiet(p, c, pool, o);
    ASSERT_EQ(out.exit_code, 2);
    ASSERT_TRUE(out.stop);
    EXPECT_EQ(out.stop->code(), "calib.checkpoint_scratch");
  }
}

// ---------------------------------------------------------------- T-M2R3-07

TEST(CalibBlocks, T_M2R3_07_job_waits_under_pressure_and_records_the_samples_in_the_provenance) {
  const disk_case c = make_case("pressure", synth_set(40, 30, -32));
  std::map<std::string, std::string> reference;
  {
    project p = make_project(c, "plain");
    memory_pool pool(big_budget);
    calibration_job_options o;
    o.resources = config_for(c);
    ASSERT_EQ(run_quiet(p, c, pool, o).exit_code, 0);
    reference = digests_by_role(p);
  }
  fake_system sys("r32_pressure_proc");
  const auto psi = [&](const char* avg10) {
    const fs::path dir = sys.root / "proc" / "pressure";
    fs::create_directories(dir);
    write_bytes(dir / "memory.next", [&] {
      const std::string text = std::string("some avg10=") + avg10 +
                               " avg60=1.00 avg300=0.50 total=1\nfull avg10=0.00 avg60=0.00 avg300=0.00 total=0\n";
      return std::vector<std::uint8_t>(text.begin(), text.end());
    }());
    fs::rename(dir / "memory.next", dir / "memory");
  };
  psi("12.00");
  resource_config cfg = config_for(c);
  cfg.pressure_poll_ms = {1, "configurato"};
  const std::shared_ptr<pressure_gate> gate = make_pressure_gate(cfg, sys.sources());
  project p = make_project(c, "pressed");
  memory_pool pool(big_budget);
  calibration_job_options o;
  o.resources = cfg;
  o.pressure = gate;
  std::optional<job_outcome> result;
  testing::internal::CaptureStderr();
  std::thread job([&] { result.emplace(run_calibration_job(*p.store, "cal", c.recipe, pool, o, never_cancelled())); });
  const bool waited = wait_until([&] { return gate->waiting_jobs.load() == 1; });
  EXPECT_TRUE(waited);
  EXPECT_EQ(pool.reserved_bytes(), 0u) << "sopra soglia il job non e' ammesso";
  psi("10.00");
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  EXPECT_EQ(gate->waiting_jobs.load(), 1u) << "soglia inclusa";
  psi("2.50");
  job.join();
  testing::internal::GetCapturedStderr();
  ASSERT_TRUE(result);
  const job_outcome& out = *result;
  ASSERT_EQ(out.exit_code, 0);
  EXPECT_EQ(digests_by_role(p), reference);
  ASSERT_TRUE(out.provenance.details);
  const json* admission = out.provenance.details->find("admission");
  ASSERT_NE(admission, nullptr);
  ASSERT_EQ(admission->type(), json::kind::object);
  const json* samples = admission->find("samples");
  const json* waits = admission->find("pressure_wait_samples");
  ASSERT_NE(samples, nullptr);
  ASSERT_NE(waits, nullptr);
  ASSERT_EQ(samples->items().size(), 2u);
  const json& first = samples->items().front();
  const json& last = samples->items().back();
  ASSERT_NE(first.find("psi"), nullptr);
  ASSERT_NE(first.find("admitted"), nullptr);
  ASSERT_NE(last.find("psi"), nullptr);
  ASSERT_NE(last.find("admitted"), nullptr);
  EXPECT_EQ(first.find("psi")->as_number(), 12.0);
  EXPECT_FALSE(first.find("admitted")->as_bool());
  EXPECT_EQ(last.find("psi")->as_number(), 2.5);
  EXPECT_TRUE(last.find("admitted")->as_bool());
  EXPECT_GE(waits->as_integer(), 2);
}

// ---------------------------------------------------------------- pubblicazione in streaming (store)

TEST(CalibBlocks, T_M2R3_05_streamed_artifact_requires_the_digest_and_propagates_source_errors) {
  const fs::path dir = fresh_dir("r32/stream_store");
  auto store = sqlite_dag_store::create(dir / "store");
  const std::string text(100000, 'x');
  const std::vector<std::uint8_t> bytes(text.begin(), text.end());
  const auto source_of = [&](std::size_t fail_after) {
    return [&bytes, fail_after, pos = std::size_t{0}](std::span<std::uint8_t> out) mutable -> std::size_t {
      if (pos >= fail_after) throw stop_error(error_category::io, "test.source", "sorgente fallita");
      const std::size_t n = std::min(out.size(), bytes.size() - pos);
      std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(pos), n, out.begin());
      pos += n;
      return n;
    };
  };
  const auto streamed = [&](std::optional<content_digest> expected, std::size_t fail_after) {
    artifact_content a{{}, "bin", "blob", std::move(expected), "primary"};
    a.source = source_of(fail_after);
    return a;
  };
  const fs::path artifacts = store->root() / "artifacts";
  EXPECT_STOP(store->write_files({streamed(std::nullopt, SIZE_MAX)}), error_category::invalid_input, "store.stream_digest");
  content_digest wrong = digest_of(bytes);
  wrong.size_bytes += 1;
  EXPECT_STOP(store->write_files({streamed(wrong, SIZE_MAX)}), error_category::integrity, "store.expected_digest_mismatch");
  EXPECT_STOP(store->write_files({streamed(digest_of(bytes), 70000)}), error_category::io, "test.source");
  EXPECT_EQ(fs::exists(artifacts) ? m2d::entry_count(artifacts) : 0u, 0u) << "nessun file e nessun temporaneo";
  const std::vector<stored_file> ok = store->write_files({streamed(digest_of(bytes), SIZE_MAX)});
  ASSERT_EQ(ok.size(), 1u);
  EXPECT_EQ(ok[0].digest(), digest_of(bytes));
  EXPECT_EQ(read_text(store->artifact_path(ok[0].rel_path())), text);
}

}  // namespace
