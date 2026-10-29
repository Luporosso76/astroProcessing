// M2.e: job di calibrazione su store SQLite durevole, file su disco, budget/pool/scheduler di M2.d.
// T-M2E-11, T-M2E-12, T-M2E-16, parte M2 di T-FBK-01.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <map>

#include <sys/stat.h>

#include "astro/hash.hpp"
#include "calib_support.hpp"

namespace {

using namespace cal;

constexpr std::uint64_t MiB = std::uint64_t{1} << 20;

struct disk_case {
  fs::path dir;
  set_def s;
  calibration_recipe recipe;
};

disk_case make_case(const std::string& name, set_def s) {
  disk_case c;
  c.dir = fresh_dir(name);
  c.s = std::move(s);
  c.recipe = recipe_of(c.s);
  const std::pair<const char*, const frame_def*> frames[4] = {
      {"light.fits", &c.s.light}, {"dark.fits", &c.s.dark}, {"flat.fits", &c.s.flat}, {"dark_flat.fits", &c.s.dark_flat}};
  calibration_input_spec* specs[4] = {&c.recipe.light, &c.recipe.dark, &c.recipe.flat, &c.recipe.dark_flat};
  for (int i = 0; i < 4; ++i) {
    write_bytes(c.dir / frames[i].first, file_of(*frames[i].second, c.s.w, c.s.h));
    specs[i]->path = (c.dir / frames[i].first).string();
  }
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

job_outcome run_quiet(project& p, const disk_case& c, memory_pool& pool, const calibration_job_options& o = {},
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

std::size_t artifact_file_count(const project& p) {
  const fs::path dir = p.store->root() / "artifacts";
  if (!fs::exists(dir)) return 0;
  return static_cast<std::size_t>(std::distance(fs::directory_iterator(dir), fs::directory_iterator()));
}

std::map<std::string, artifact_record> by_role(const project& p) {
  std::map<std::string, artifact_record> out;
  for (const artifact_record& a : p.store->artifacts_for(p.node.key())) out[a.output_role] = a;
  return out;
}

// Un job fermato non pubblica output: l'unico artefatto del nodo e' la provenance dell'arresto.
void expect_only_stop_provenance(const project& p) {
  const std::vector<artifact_record> all = p.store->artifacts_for(p.node.key());
  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all[0].output_role, "provenance");
  EXPECT_EQ(artifact_file_count(p), 1u);
}

set_def job_set() {
  set_def s = synth_set(40, 30, -32);
  s.flat.v[5] = s.dark_flat.v[5];  // Fc = 0: pixel mascherato da q_nonpositive
  s.light.v[7] = std::numeric_limits<double>::quiet_NaN();
  s.light.hdr.saturation = 70000;  // dichiarata: nessun pixel saturo
  return s;
}

// ---------------------------------------------------------------- T-M2E-12

TEST(CalibJob, T_M2E_12_publishes_image_report_and_text_coherent_with_the_output) {
  const disk_case c = make_case("t12", job_set());
  project p = make_project(c, "store");
  memory_pool pool(256 * MiB);
  calibration_job_options o;
  o.worker_count = 2;
  o.tile_rows = 7;
  const job_outcome out = run_quiet(p, c, pool, o);
  ASSERT_EQ(out.exit_code, 0);
  EXPECT_EQ(out.provenance.state, job_state::completed);
  EXPECT_EQ(out.provenance.output_digests.size(), 3u);

  const auto roles = by_role(p);
  ASSERT_EQ(roles.size(), 4u);
  ASSERT_TRUE(roles.count("primary") && roles.count("report") && roles.count("report_text") && roles.count("provenance"));
  const slot_view view = p.store->read_slot("cal");
  ASSERT_EQ(view.state, freshness::current);
  EXPECT_EQ(view.artifact->sha256, roles.at("primary").sha256);

  const auto read_file = [&](const artifact_record& a) {
    const std::string text = read_text(p.store->artifact_path(a.rel_path));
    return std::vector<std::uint8_t>(text.begin(), text.end());
  };
  const std::vector<std::uint8_t> fits = read_file(roles.at("primary"));
  EXPECT_EQ(digest_of(fits).sha256, roles.at("primary").sha256);
  const image decoded = decode_fits(fits, big_budget);
  const calibration_result memory = calibrate(c.recipe, images_of(c.s));
  EXPECT_TRUE(equivalent_content(decoded, memory.output));
  EXPECT_TRUE(operator_output_invariants_hold(decoded));
  EXPECT_EQ(decoded.type, sample_type::f32);
  EXPECT_EQ(decoded.invalid.bits[5], invalid_reason::q_nonpositive);
  EXPECT_EQ(decoded.invalid.bits[7], invalid_reason::source_nonfinite);

  const std::string report_text_json = read_text(p.store->artifact_path(roles.at("report").rel_path));
  const json report = parse_json(report_text_json);
  EXPECT_EQ(canonical_json(report), report_text_json);
  EXPECT_EQ(read_text(p.store->artifact_path(roles.at("report_text").rel_path)), calibration_report_text(report));

  EXPECT_EQ(at(report, {"outputs", "calibrated_image", "sha256"}).as_string(), roles.at("primary").sha256);
  EXPECT_EQ(at(report, {"outputs", "calibrated_image", "size_bytes"}).as_integer(),
            static_cast<std::int64_t>(roles.at("primary").size_bytes));
  std::uint64_t invalid = 0;
  std::array<std::uint64_t, 9> per_bit{};
  for (const std::uint16_t bits : decoded.invalid.bits) {
    invalid += bits != 0;
    for (std::size_t b = 0; b < 9; ++b) per_bit[b] += (bits >> b) & 1u;
  }
  EXPECT_EQ(static_cast<std::uint64_t>(at(report, {"mask", "invalid_pixels"}).as_integer()), invalid);
  EXPECT_EQ(static_cast<std::uint64_t>(at(report, {"mask", "valid_pixels"}).as_integer()), 1200 - invalid);
  EXPECT_EQ(at(report, {"mask", "pixels_with_bit", "q_nonpositive"}).as_integer(), 1);
  EXPECT_EQ(at(report, {"mask", "pixels_with_bit", "source_nonfinite"}).as_integer(), 1);
  EXPECT_EQ(static_cast<std::uint64_t>(at(report, {"mask", "pixels_with_bit", "q_nonpositive"}).as_integer()), per_bit[4]);

  const char* inputs[4] = {"light", "dark", "flat", "dark_flat"};
  const calibration_input_spec* specs[4] = {&c.recipe.light, &c.recipe.dark, &c.recipe.flat, &c.recipe.dark_flat};
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(at(report, {"inputs", inputs[i], "digest_sha256"}).as_string(), specs[i]->digest.sha256);
  }
  EXPECT_EQ(at(report, {"inputs", "dark", "master_state", "additive"}).as_string(), "bias_included");
  EXPECT_EQ(at(report, {"inputs", "dark", "master_state", "state_source"}).as_string(), "declared");
  EXPECT_EQ(at(report, {"inputs", "flat", "master_state", "flat"}).as_string(), "raw");
  const auto& ledger = at(report, {"correction_ledger", "entries"}).items();
  ASSERT_EQ(ledger.size(), 4u);
  EXPECT_EQ(ledger[0].find("step")->as_string(), "dark_subtraction");
  EXPECT_EQ(ledger[0].find("status")->as_string(), "applied");
  EXPECT_EQ(ledger[0].find("master_digest")->as_string(), c.recipe.dark.digest.sha256);
  EXPECT_EQ(ledger[1].find("master_digest")->as_string(), c.recipe.dark_flat.digest.sha256);
  EXPECT_EQ(ledger[2].find("master_digest")->as_string(), c.recipe.flat.digest.sha256);
  EXPECT_EQ(ledger[3].find("step")->as_string(), "bias_subtraction");
  EXPECT_EQ(ledger[3].find("status")->as_string(), "not_applicable");
  EXPECT_EQ(ledger[3].find("reason")->as_string(), "bias_in_matched_dark");
  EXPECT_EQ(at(report, {"variance_model", "covariance_model"}).as_string(), "diagonal_independent");
  EXPECT_EQ(at(report, {"normalization"}).items().size(), 1u);
  EXPECT_GT(as_double(*at(report, {"normalization"}).items()[0].find("N")), 0.0);
  EXPECT_GT(at(report, {"compatibility"}).items().size(), 10u);
  EXPECT_NE(report_text_json.find("\"shared_master_digests\""), std::string::npos);
}

// ---------------------------------------------------------------- T-M2E-16

TEST(CalibJob, T_M2E_16_output_is_bit_identical_for_workers_and_tile_rows) {
  const disk_case c = make_case("t16", job_set());
  std::string reference_image, reference_report;
  int run = 0;
  for (const unsigned workers : {1u, 2u, 4u}) {
    for (const std::uint32_t tile_rows : {1u, 3u, 7u, 64u}) {
      project p = make_project(c, "store" + std::to_string(run++));
      memory_pool pool(256 * MiB);
      calibration_job_options o;
      o.worker_count = workers;
      o.tile_rows = tile_rows;
      const job_outcome out = run_quiet(p, c, pool, o);
      ASSERT_EQ(out.exit_code, 0) << workers << " " << tile_rows;
      const auto roles = by_role(p);
      if (reference_image.empty()) {
        reference_image = roles.at("primary").sha256;
        reference_report = roles.at("report").sha256;
      }
      EXPECT_EQ(roles.at("primary").sha256, reference_image) << workers << " " << tile_rows;
      EXPECT_EQ(roles.at("report").sha256, reference_report) << workers << " " << tile_rows;
    }
  }
}

// ---------------------------------------------------------------- T-M2E-11

TEST(CalibJob, T_M2E_11_master_modified_or_removed_after_preflight_stops_the_branch) {
  const disk_case c = make_case("t11", job_set());
  {
    project p = make_project(c, "modified");
    memory_pool pool(256 * MiB);
    calibration_job_options o;
    o.hook = [&](std::string_view point) {
      if (point != "preflight.done") return;
      frame_def changed = c.s.dark;
      changed.v[0] += 1.0;
      write_bytes(c.dir / "dark.fits", file_of(changed, c.s.w, c.s.h));
    };
    const job_outcome out = run_quiet(p, c, pool, o);
    EXPECT_EQ(out.exit_code, exit_code_stop);
    ASSERT_TRUE(out.stop.has_value());
    EXPECT_EQ(out.stop->category(), error_category::integrity);
    EXPECT_EQ(out.stop->code(), "input.digest_mismatch");
    EXPECT_EQ(out.job.state, job_state::failed);
    EXPECT_EQ(p.store->read_slot("cal").state, freshness::missing);
    expect_only_stop_provenance(p);
  }
  write_bytes(c.dir / "dark.fits", file_of(c.s.dark, c.s.w, c.s.h));
  {
    project p = make_project(c, "removed");
    memory_pool pool(256 * MiB);
    calibration_job_options o;
    o.hook = [&](std::string_view) { fs::remove(c.dir / "flat.fits"); };
    const job_outcome out = run_quiet(p, c, pool, o);
    EXPECT_EQ(out.exit_code, exit_code_stop);
    ASSERT_TRUE(out.stop.has_value());
    EXPECT_EQ(out.stop->category(), error_category::io);
    EXPECT_EQ(out.stop->code(), "input.missing");
    expect_only_stop_provenance(p);
  }
}

TEST(CalibJob, input_changed_before_the_first_read_stops_without_decoding) {
  const disk_case c = make_case("t11b", job_set());
  frame_def changed = c.s.light;
  changed.v[3] += 1.0;
  write_bytes(c.dir / "light.fits", file_of(changed, c.s.w, c.s.h));
  project p = make_project(c, "store");
  memory_pool pool(256 * MiB);
  const job_outcome out = run_quiet(p, c, pool);
  ASSERT_TRUE(out.stop.has_value());
  EXPECT_EQ(out.stop->category(), error_category::integrity);
  EXPECT_EQ(out.stop->code(), "input.digest_mismatch");
}

// Il file riletto deve avere la geometria del piano: un light corto pianificato e poi sostituito con quello atteso
// non puo' lasciare righe non calcolate pubblicate come valide.
TEST(CalibJob, T_M2E_31_geometry_changed_after_planning_stops) {
  const disk_case c = make_case("t31", job_set());
  frame_def shorter = c.s.light;
  shorter.v.resize(40 * 20);
  write_bytes(c.dir / "light.fits", file_of(shorter, 40, 20));
  project p = make_project(c, "store");
  memory_pool pool(256 * MiB);
  calibration_job_options o;
  o.hook = [&](std::string_view point) {
    if (point == "plan.done") write_bytes(c.dir / "light.fits", file_of(c.s.light, c.s.w, c.s.h));
  };
  const job_outcome out = run_quiet(p, c, pool, o);
  EXPECT_EQ(out.exit_code, exit_code_stop);
  ASSERT_TRUE(out.stop.has_value());
  EXPECT_EQ(out.stop->category(), error_category::integrity);
  EXPECT_TRUE(out.stop->code() == "input.size_changed" || out.stop->code() == "input.geometry_changed") << out.stop->code();
  expect_only_stop_provenance(p);
}

// Soglia CV 0,01 di iid_assumption nel report del job: flat con Fc = 10000 (1 +- a), CV = a.
TEST(CalibJob, T_M2E_26_report_iid_assumption_follows_the_cv_threshold) {
  for (const auto& [a, expected] : {std::pair<double, const char*>{0.005, "cv_below_threshold"}, {0.02, "not_verified"}}) {
    set_def s = job_set();
    for (std::size_t i = 0; i < s.flat.v.size(); ++i) {
      s.dark_flat.v[i] = 200;
      s.flat.v[i] = 200 + 10000.0 * (1.0 + (i % 2 == 0 ? a : -a));
    }
    s.flat.hdr.saturation = 0;
    const disk_case c = make_case("t26job_" + std::to_string(static_cast<int>(a * 1000)), s);
    project p = make_project(c, "store");
    memory_pool pool(256 * MiB);
    ASSERT_EQ(run_quiet(p, c, pool).exit_code, 0);
    const json report = parse_json(read_text(p.store->artifact_path(by_role(p).at("report").rel_path)));
    const json& ch = at(report, {"normalization"}).items()[0];
    EXPECT_EQ(ch.find("iid_assumption")->as_string(), expected) << a;
    EXPECT_EQ(ch.find("hypothesis")->as_string(), "iid_gaussian_fc");
    EXPECT_NEAR(as_double(*ch.find("fc_cv")), a, 1e-3);
  }
}

// ---------------------------------------------------------------- T-M2E-08 sul job, cancel, ricetta/nodo

TEST(CalibJob, incompatible_inputs_stop_before_any_output) {
  set_def s = job_set();
  s.dark.hdr.exposure = 61;
  const disk_case c = make_case("t08job", s);
  project p = make_project(c, "store");
  memory_pool pool(256 * MiB);
  const job_outcome out = run_quiet(p, c, pool);
  EXPECT_EQ(out.exit_code, exit_code_stop);
  ASSERT_TRUE(out.stop.has_value());
  EXPECT_EQ(out.stop->category(), error_category::incompatible);
  EXPECT_EQ(out.stop->code(), "calib.mismatch.exposure_s");
  EXPECT_EQ(out.provenance.state, job_state::failed);
  EXPECT_TRUE(out.provenance.output_digests.empty());
  expect_only_stop_provenance(p);
}

TEST(CalibJob, recipe_must_match_the_slot_node) {
  const disk_case c = make_case("node", job_set());
  project p = make_project(c, "store");
  disk_case other = c;
  other.recipe.q_min = 0.5;
  memory_pool pool(256 * MiB);
  const job_outcome out = run_quiet(p, other, pool);
  ASSERT_TRUE(out.stop.has_value());
  EXPECT_EQ(out.stop->code(), "calib.node_mismatch");
}

TEST(CalibJob, cancel_requested_publishes_nothing) {
  const disk_case c = make_case("cancel", job_set());
  project p = make_project(c, "store");
  memory_pool pool(256 * MiB);
  cancel_token cancel;
  calibration_job_options o;
  o.hook = [&](std::string_view) { cancel.request(); };
  const job_outcome out = run_quiet(p, c, pool, o, cancel);
  EXPECT_EQ(out.exit_code, exit_code_cancelled);
  EXPECT_EQ(out.job.state, job_state::cancelled);
  expect_only_stop_provenance(p);
}

// ---------------------------------------------------------------- T-M2B-10 (persistenza), T-M2C-11 (originali)

std::string persisted_provenance(const fs::path& project_dir, const std::string& node_key) {
  const auto reopened = sqlite_dag_store::open(project_dir, open_intent::read_only);
  std::string found;
  int count = 0;
  for (const artifact_record& a : reopened->artifacts_for(node_key)) {
    if (a.output_role != "provenance") continue;
    ++count;
    found = read_text(reopened->artifact_path(a.rel_path));
    EXPECT_EQ(digest_of(std::vector<std::uint8_t>(found.begin(), found.end())).sha256, a.sha256);
  }
  EXPECT_EQ(count, 1);
  return found;
}

TEST(CalibJob, T_M2B_10_provenance_is_persisted_for_success_stop_and_cancel_and_survives_reopen) {
  {
    const disk_case c = make_case("prov_ok", job_set());
    project p = make_project(c, "store");
    memory_pool pool(256 * MiB);
    const job_outcome out = run_quiet(p, c, pool);
    ASSERT_EQ(out.exit_code, 0);
    const std::string key = p.node.key();
    p.store.reset();
    EXPECT_EQ(persisted_provenance(c.dir / "store", key), provenance_canonical(out.provenance));
    EXPECT_NE(persisted_provenance(c.dir / "store", key).find("\"state\":\"completed\""), std::string::npos);
  }
  {
    const disk_case c = make_case("prov_stop", job_set());
    project p = make_project(c, "store");
    memory_pool pool(256 * MiB);
    calibration_job_options o;
    o.hook = [&](std::string_view) { fs::remove(c.dir / "flat.fits"); };
    const job_outcome out = run_quiet(p, c, pool, o);
    ASSERT_EQ(out.exit_code, exit_code_stop);
    const std::string key = p.node.key();
    p.store.reset();
    const std::string text = persisted_provenance(c.dir / "store", key);
    EXPECT_EQ(text, provenance_canonical(out.provenance));
    EXPECT_NE(text.find("\"state\":\"failed\""), std::string::npos);
    EXPECT_NE(text.find(out.stop->code()), std::string::npos);
  }
  {
    const disk_case c = make_case("prov_cancel", job_set());
    project p = make_project(c, "store");
    memory_pool pool(256 * MiB);
    cancel_token cancel;
    calibration_job_options o;
    o.hook = [&](std::string_view) { cancel.request(); };
    const job_outcome out = run_quiet(p, c, pool, o, cancel);
    ASSERT_EQ(out.exit_code, exit_code_cancelled);
    const std::string key = p.node.key();
    p.store.reset();
    const std::string text = persisted_provenance(c.dir / "store", key);
    EXPECT_EQ(text, provenance_canonical(out.provenance));
    EXPECT_NE(text.find("\"state\":\"cancelled\""), std::string::npos);
  }
}

struct file_state {
  std::string sha256;
  mode_t mode = 0;
  ino_t ino = 0;
  off_t size = 0;
  timespec mtime{};
  bool operator==(const file_state& o) const {
    return sha256 == o.sha256 && mode == o.mode && ino == o.ino && size == o.size && mtime.tv_sec == o.mtime.tv_sec &&
           mtime.tv_nsec == o.mtime.tv_nsec;
  }
};

file_state state_of(const fs::path& path) {
  struct stat st {};
  EXPECT_EQ(::stat(path.c_str(), &st), 0);
  const std::string text = read_text(path);
  return file_state{digest_of(std::vector<std::uint8_t>(text.begin(), text.end())).sha256, st.st_mode, st.st_ino, st.st_size,
                    st.st_mtim};
}

TEST(CalibJob, T_M2C_11_the_four_inputs_are_untouched_by_the_whole_calibration_workflow) {
  const disk_case c = make_case("t11_originals", job_set());
  const char* names[4] = {"light.fits", "dark.fits", "flat.fits", "dark_flat.fits"};
  std::array<file_state, 4> before;
  for (int i = 0; i < 4; ++i) before[i] = state_of(c.dir / names[i]);
  project p = make_project(c, "store");
  memory_pool pool(256 * MiB);
  const job_outcome out = run_quiet(p, c, pool);
  ASSERT_EQ(out.exit_code, 0);
  p.store.reset();
  for (int i = 0; i < 4; ++i) EXPECT_TRUE(before[i] == state_of(c.dir / names[i])) << names[i];
}

// ---------------------------------------------------------------- T-FBK-01, parte M2

TEST(CalibJob, T_FBK_01_failing_decoder_stops_without_alternative_path) {
  const disk_case c0 = make_case("fbk_decoder", job_set());
  disk_case c = c0;
  // Header valido, data unit troncata: il digest e' quello del file effettivo, quindi la verifica passa e il decode fallisce.
  fx::bytes broken = file_of(c.s.flat, c.s.w, c.s.h);
  broken.resize(broken.size() - 2880 - 100);
  write_bytes(c.dir / "flat.fits", broken);
  c.recipe.flat.digest = digest_of(broken);
  project p = make_project(c, "store");
  memory_pool pool(256 * MiB);
  const job_outcome out = run_quiet(p, c, pool);
  EXPECT_EQ(out.exit_code, exit_code_stop);
  ASSERT_TRUE(out.stop.has_value());
  EXPECT_EQ(out.stop->category(), error_category::invalid_input);
  EXPECT_EQ(out.stop->code().rfind("fits.", 0), 0u) << out.stop->code();
  expect_only_stop_provenance(p);
  EXPECT_EQ(p.store->read_slot("cal").state, freshness::missing);
}

TEST(CalibJob, T_FBK_01_insufficient_budget_stops_without_reducing_tiles_or_workers) {
  const disk_case c = make_case("fbk_budget", job_set());
  project p = make_project(c, "store");
  memory_pool pool(1 * MiB);
  calibration_job_options o;
  o.worker_count = 4;
  o.tile_rows = 64;
  const job_outcome out = run_quiet(p, c, pool, o);
  EXPECT_EQ(out.exit_code, exit_code_stop);
  ASSERT_TRUE(out.stop.has_value());
  EXPECT_EQ(out.stop->category(), error_category::budget);
  EXPECT_EQ(out.stop->code(), "sched.job_exceeds_budget");
  EXPECT_EQ(pool.peak_reserved_bytes(), 0u);
  expect_only_stop_provenance(p);
}

TEST(CalibJob, T_FBK_01_store_failure_during_commit_and_corrupt_store_stop) {
  const disk_case c = make_case("fbk_store", job_set());
  {
    project p = make_project(c, "inject");
    p.store->set_fault_injector([](std::string_view point) { return point == "commit.before_commit" ? 5 : 0; });
    memory_pool pool(256 * MiB);
    const job_outcome out = run_quiet(p, c, pool);
    EXPECT_EQ(out.exit_code, exit_code_stop);
    ASSERT_TRUE(out.stop.has_value());
    EXPECT_EQ(out.stop->category(), error_category::io);
    EXPECT_EQ(p.store->read_slot("cal").state, freshness::missing);
    EXPECT_EQ(pool.reserved_bytes(), 0u);
  }
  fs::path root;
  {
    project p = make_project(c, "corrupt");
    root = p.store->root();
    memory_pool pool(256 * MiB);
    ASSERT_EQ(run_quiet(p, c, pool).exit_code, 0);
  }
  std::string bytes = read_text(root / "project.db");
  ASSERT_GT(bytes.size(), 8192u);
  std::fill(bytes.begin() + 4096, bytes.end(), '\xA5');
  {
    std::ofstream out(root / "project.db", std::ios::binary | std::ios::trunc);
    out << bytes;
  }
  EXPECT_STOP(sqlite_dag_store::open(root, open_intent::read_write), error_category::integrity, "store.corrupt");
}

// ---------------------------------------------------------------- R3.1: pianificatore nel job reale

planner_input job_planner_input(const disk_case& c, std::uint64_t budget) {
  std::uint64_t peaks = 0;
  for (const calibration_input_spec* spec : {&c.recipe.light, &c.recipe.dark, &c.recipe.flat, &c.recipe.dark_flat}) {
    peaks += plan_fits(open_fits_input(spec->path)).peak_bytes;
  }
  const std::uint64_t header_bound = 3 * (std::uint64_t{fits_limits::max_header_cards} * 80 + 2 * 2880);
  return planner_input{budget, 4, 40, 30, 4, peaks, header_bound};
}

TEST(CalibJob, T_M2R3_04_job_with_budget_equal_to_the_minimum_runs_and_one_byte_less_stops_before_any_task) {
  const disk_case c = make_case("r3_minimum", job_set());
  const resource_config cfg;
  const std::uint64_t minimum = compute_plan_bounds(job_planner_input(c, 0), cfg).minimum_feasible_bytes;
  int prepared = 0;
  {
    project p = make_project(c, "ok");
    memory_pool pool(minimum);
    calibration_job_options o;
    o.resources = cfg;
    o.hook = [&](std::string_view) {};
    const job_outcome out = run_quiet(p, c, pool, o);
    ASSERT_EQ(out.exit_code, 0);
    EXPECT_EQ(pool.peak_reserved_bytes(), minimum);  // un worker: shared + per_task
    EXPECT_EQ(pool.reserved_bytes(), 0u);
  }
  {
    // Controllo indipendente: lo stesso budget con il percorso senza pianificatore (formula storica del job).
    project p = make_project(c, "legacy_ok");
    memory_pool pool(minimum);
    EXPECT_EQ(run_quiet(p, c, pool).exit_code, 0);
    project q = make_project(c, "legacy_stop");
    memory_pool small(minimum - 1);
    EXPECT_EQ(run_quiet(q, c, small).exit_code, 2);
  }
  {
    project p = make_project(c, "stop");
    memory_pool pool(minimum - 1);
    calibration_job_options o;
    o.resources = cfg;
    o.hook = [&](std::string_view what) { if (what == "preflight.done") ++prepared; };
    const job_outcome out = run_quiet(p, c, pool, o);
    EXPECT_EQ(out.exit_code, 2);
    EXPECT_EQ(prepared, 0);
    expect_only_stop_provenance(p);
    const fs::path root = p.store->root();
    const std::string key = p.node.key();
    p.store.reset();
    const std::string prov = persisted_provenance(root, key);
    EXPECT_NE(prov.find("sched.job_exceeds_budget"), std::string::npos);
    EXPECT_NE(prov.find("minimum_feasible_bytes"), std::string::npos);
    EXPECT_NE(prov.find(std::to_string(minimum)), std::string::npos);
  }
}

TEST(CalibJob, T_M2R3_02_job_adapts_workers_to_the_budget_and_keeps_explicit_values) {
  const disk_case c = make_case("r3_adapt", job_set());
  const resource_config cfg;
  const plan_bounds b = compute_plan_bounds(job_planner_input(c, 0), cfg);
  // Budget per esattamente 2 worker: il piano ne usa 2 (gli altri task attendono), mai tile o worker ridotti dopo.
  const std::uint64_t budget = b.shared_bytes + 2 * b.per_task_bytes;
  project p = make_project(c, "store");
  memory_pool pool(budget);
  calibration_job_options o;
  o.resources = cfg;
  ASSERT_EQ(run_quiet(p, c, pool, o).exit_code, 0);
  EXPECT_EQ(pool.peak_reserved_bytes(), budget);
  EXPECT_LE(pool.peak_reserved_bytes(), pool.budget_bytes());
  // Worker esplicito che non entra: stop, non riduzione.
  project q = make_project(c, "explicit");
  memory_pool pool2(budget);
  resource_config explicit_cfg;
  explicit_cfg.worker_count = 3;
  o.resources = explicit_cfg;
  EXPECT_EQ(run_quiet(q, c, pool2, o).exit_code, 2);
  expect_only_stop_provenance(q);
}

TEST(CalibJob, T_M2R3_08_configured_queue_capacity_tile_and_workers_reach_the_scheduler_plan) {
  const disk_case c = make_case("r3_wiring", job_set());
  resource_config cfg;
  cfg.task_queue_capacity = {9, "configurato"};
  cfg.tile_rows = 5;
  cfg.worker_count = 2;
  cfg.report_margin_bytes = {2 * MiB, "configurato"};
  std::optional<job_plan> seen;
  calibration_job_options o;
  o.resources = cfg;
  o.on_job_plan = [&](const job_plan& plan) { seen = plan; };
  project p = make_project(c, "store");
  memory_pool pool(256 * MiB);
  ASSERT_EQ(run_quiet(p, c, pool, o).exit_code, 0);
  ASSERT_TRUE(seen.has_value());
  EXPECT_EQ(seen->queue_capacity, 9u);
  EXPECT_EQ(seen->tile_rows, 5u);
  EXPECT_EQ(seen->worker_count, 2u);
  EXPECT_EQ(seen->shared_bytes, compute_plan_bounds(job_planner_input(c, 256 * MiB), cfg).shared_bytes);
  // Il margine configurato e' nel piano: 1 MiB in piu' del default.
  EXPECT_EQ(seen->shared_bytes, compute_plan_bounds(job_planner_input(c, 256 * MiB), resource_config{}).shared_bytes + MiB);
}

TEST(CalibJob, T_M2R3_08_defaults_and_origins_reach_the_provenance_and_the_output_does_not_change) {
  const disk_case c = make_case("r3_prov", job_set());
  resource_config cfg;
  cfg.fraction_normal = {{1, 2}, "configurato"};
  budget_snapshot snapshot;
  snapshot.mode = budget_mode::manual;
  snapshot.manual = parse_manual_budget("256MiB");
  snapshot.budget_bytes = 256 * MiB;
  snapshot.mem_total_bytes = 1024 * MiB;
  snapshot.mem_available_bytes = 512 * MiB;
  project p = make_project(c, "store");
  memory_pool pool(256 * MiB);
  calibration_job_options o;
  o.resources = cfg;
  testing::internal::CaptureStderr();
  const job_outcome out = run_calibration_job(*p.store, "cal", c.recipe, pool, o, never_cancelled(), snapshot);
  testing::internal::GetCapturedStderr();
  ASSERT_EQ(out.exit_code, 0);
  const std::string primary = by_role(p).at("primary").sha256;
  const fs::path root = p.store->root();
  const std::string key = p.node.key();
  p.store.reset();
  const json prov = parse_json(persisted_provenance(root, key));
  const json* recorded_budget = prov.find("budget");
  ASSERT_NE(recorded_budget, nullptr);
  ASSERT_NE(recorded_budget->find("resources"), nullptr);
  const json& res = *recorded_budget->find("resources");
  EXPECT_EQ(res.find("fraction_normal")->find("origin")->as_string(), "configurato");
  EXPECT_EQ(res.find("fraction_pressure")->find("origin")->as_string(), resource_config{}.fraction_pressure.origin);
  EXPECT_EQ(res.find("job_queue_capacity")->find("value")->as_integer(), 16);
  EXPECT_EQ(res.find("psi_threshold")->find("origin")->as_string(), resource_config{}.psi_threshold.origin);
  EXPECT_EQ(res.find("vram")->find("value")->type(), json::kind::null);
  // L'uscita calibrata e' identica a quella del percorso senza pianificatore (T-M2E-16: indipendente da tile e worker).
  project q = make_project(c, "legacy");
  memory_pool pool2(256 * MiB);
  ASSERT_EQ(run_quiet(q, c, pool2).exit_code, 0);
  EXPECT_EQ(primary, by_role(q).at("primary").sha256);
  // Senza snapshot del budget la configurazione non viene registrata (limite dichiarato in calibration.hpp).
  q.store.reset();
  project r = make_project(c, "no_snapshot");
  memory_pool pool3(256 * MiB);
  calibration_job_options plain;
  plain.resources = cfg;
  ASSERT_EQ(run_quiet(r, c, pool3, plain).exit_code, 0);
  const fs::path rroot = r.store->root();
  r.store.reset();
  EXPECT_EQ(parse_json(persisted_provenance(rroot, r.node.key())).find("budget"), nullptr);
}

}  // namespace
