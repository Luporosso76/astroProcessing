// R4 (plans/M2-residui.md): recovery con report persistito, resume esplicito delle fasi sicure, perdita di energia
// simulata, regola hardlink, report di export. Contratto: ADR-04 «Estensione R4», ADR-05 (provenance del resume).
#include <sys/stat.h>
#include <unistd.h>

#include <set>

#include "astro/memory_pool.hpp"
#include "astro/workflow.hpp"
#include "calib_support.hpp"
#include "powerloss_vfs.hpp"
#include "store_test_support.hpp"

namespace {

namespace st = store_test;
using namespace astro;
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------------------------------------------
// Workflow di K nodi di calibrazione (stessi master, K light diversi): il job multi-nodo di T-M2R4-01, 02 e 04.

struct frames {
  std::vector<calibration_recipe> recipes;
  std::vector<fs::path> files;  // originali: light_0..light_{K-1}, dark, flat, dark_flat
};

std::string slot_of(std::size_t k) { return "cal" + std::to_string(k); }

frames make_frames(const std::string& name, std::size_t k_count) {
  const fs::path dir = st::fresh_dir(name + "_frames");
  fs::create_directories(dir);
  const cal::set_def base = cal::synth_set(16, 12, -32);
  frames f;
  const std::pair<const char*, const cal::frame_def*> masters[3] = {
      {"dark.fits", &base.dark}, {"flat.fits", &base.flat}, {"dark_flat.fits", &base.dark_flat}};
  for (std::size_t k = 0; k < k_count; ++k) {
    cal::set_def s = base;
    for (std::size_t i = 0; i < s.light.v.size(); ++i) {
      s.light.v[i] = base.dark.v[i] + (base.light.v[i] - base.dark.v[i]) * (1.0 + 0.25 * static_cast<double>(k));
    }
    const fs::path light = dir / ("light_" + std::to_string(k) + ".fits");
    cal::write_bytes(light, cal::file_of(s.light, s.w, s.h));
    f.files.push_back(light);
    calibration_recipe r = cal::recipe_of(s);
    r.light.path = light.string();
    r.dark.path = (dir / masters[0].first).string();
    r.flat.path = (dir / masters[1].first).string();
    r.dark_flat.path = (dir / masters[2].first).string();
    f.recipes.push_back(std::move(r));
  }
  for (const auto& [file, def] : masters) {
    cal::write_bytes(dir / file, cal::file_of(*def, base.w, base.h));
    f.files.push_back(dir / file);
  }
  return f;
}

content_digest file_digest(const fs::path& p) { return digest_of(st::bytes_of(st::read_text(p))); }

// Progetto con gli originali registrati, un nodo e uno slot per passo.
void setup_project(const fs::path& dir, const frames& f) {
  auto store = sqlite_dag_store::create(dir);
  for (const fs::path& p : f.files) {
    const content_digest d = file_digest(p);
    store->add_input(input_record{0, p.string(), d.size_bytes, d.sha256, "frame"});
  }
  for (std::size_t k = 0; k < f.recipes.size(); ++k) {
    const node_spec node = make_calibration_node(f.recipes[k]);
    store->put_node(node);
    store->create_slot(slot_of(k), node.key());
  }
}

struct runner_env {
  memory_pool pool{std::uint64_t{256} << 20};
  cancel_token cancel;
};

workflow make_workflow(sqlite_dag_store& store, const frames& f, runner_env& env, const fault_injector& hook) {
  workflow wf;
  wf.name = "calib";
  wf.hook = hook;
  for (std::size_t k = 0; k < f.recipes.size(); ++k) {
    calibration_job_options opt;
    opt.worker_count = 2;
    opt.tile_rows = 4;
    if (hook) opt.hook = [hook](std::string_view p) { (void)hook("calib." + std::string(p)); };
    const calibration_recipe* recipe = &f.recipes[k];
    wf.steps.push_back(workflow_step{slot_of(k), make_calibration_node(*recipe), [&store, &env, recipe, opt, k] {
                                       return run_calibration_job(store, slot_of(k), *recipe, env.pool, opt, env.cancel);
                                     }});
  }
  return wf;
}

std::vector<job_record> all_jobs(const sqlite_dag_store& store) {
  std::vector<job_record> out;
  for (std::uint64_t id = 1;; ++id) {
    const auto j = store.get_job(id);
    if (!j) break;
    out.push_back(*j);
  }
  return out;
}

std::size_t jobs_of(const sqlite_dag_store& store, const std::string& node_key) {
  std::size_t n = 0;
  for (const auto& j : all_jobs(store)) n += j.node_key == node_key ? 1 : 0;
  return n;
}

std::vector<artifact_record> with_role(const sqlite_dag_store& store, const std::string& node_key, const std::string& role) {
  std::vector<artifact_record> out;
  for (const auto& a : store.artifacts_for(node_key)) {
    if (a.output_role == role) out.push_back(a);
  }
  return out;
}

// Digest di immagine e report pubblicati, per slot e ruolo.
std::map<std::string, std::string> output_digests(const sqlite_dag_store& store, std::size_t k_count) {
  std::map<std::string, std::string> out;
  for (std::size_t k = 0; k < k_count; ++k) {
    const slot_view v = store.read_slot(slot_of(k));
    if (v.state != freshness::current) {
      ADD_FAILURE() << slot_of(k) << " non current";
      continue;
    }
    for (const char* role : {calibration_image_role, calibration_report_role, calibration_report_text_role}) {
      for (const auto& a : with_role(store, v.artifact->node_key, role)) {
        if (a.generation == v.artifact->generation) out[slot_of(k) + "/" + role] = a.sha256;
      }
    }
  }
  return out;
}

void child_workflow(const fs::path& dir, const frames& f, const fault_injector& injector) {
  open_options options;
  options.fault = injector;
  auto store = sqlite_dag_store::open(dir, open_intent::read_write, options);
  runner_env env;
  const workflow wf = make_workflow(*store, f, env, injector);
  if (run_workflow(*store, wf).exit_code != 0) throw std::runtime_error("workflow fermato");
}

// Posizioni (1-based) di un punto per passo, nell'ordine in cui compaiono.
std::vector<std::size_t> positions(const std::vector<std::string>& points, const std::string& name) {
  std::vector<std::size_t> out;
  for (std::size_t i = 0; i < points.size(); ++i) {
    if (points[i] == name) out.push_back(i + 1);
  }
  return out;
}

json read_json_artifact(const sqlite_dag_store& store, const artifact_record& a) {
  return parse_json(st::read_text(store.artifact_path(a.rel_path)));
}

TEST(Recovery, T_M2R4_01_sigkill_at_every_point_then_explicit_resume_reuses_only_safe_phases_and_matches_the_oracle) {
  constexpr std::size_t k_count = 3;
  const frames f = make_frames("r401", k_count);
  std::map<std::string, std::string> oracle;
  {
    const fs::path dir = st::fresh_dir("r401_oracle");
    setup_project(dir, f);
    auto store = sqlite_dag_store::open(dir, open_intent::read_write);
    runner_env env;
    ASSERT_EQ(run_workflow(*store, make_workflow(*store, f, env, {})).exit_code, 0);
    oracle = output_digests(*store, k_count);
    ASSERT_EQ(oracle.size(), 3 * k_count);
  }
  std::vector<std::string> points;
  {
    const fs::path dir = st::fresh_dir("r401_dry");
    setup_project(dir, f);
    points = st::record_points([&](const fault_injector& inj) { child_workflow(dir, f, inj); });
  }
  const std::vector<std::size_t> begins = positions(points, "workflow.step_begin");
  const std::vector<std::size_t> commits = positions(points, "commit.after_commit");
  ASSERT_EQ(begins.size(), k_count);
  ASSERT_EQ(commits.size(), k_count);
  ASSERT_EQ(positions(points, "workflow.step_end").size(), k_count);
  ASSERT_EQ(positions(points, "calib.preflight.done").size(), k_count);

  for (std::size_t i = 1; i <= points.size(); ++i) {
    SCOPED_TRACE("kill al punto " + std::to_string(i) + " (" + points[i - 1] + ")");
    const fs::path dir = st::fresh_dir("r401_run");
    setup_project(dir, f);
    const st::child_outcome out = st::run_child([&](const fault_injector& inj) { child_workflow(dir, f, inj); }, static_cast<int>(i));
    ASSERT_TRUE(out.killed) << "exit " << out.exit_code;
    std::set<std::size_t> safe;
    std::optional<std::size_t> interrupted;
    for (std::size_t k = 0; k < k_count; ++k) {
      if (commits[k] <= i) safe.insert(k);
      if (begins[k] < i && i < commits[k]) interrupted = k;
    }
    auto store = sqlite_dag_store::open(dir, open_intent::read_write);
    const std::vector<job_record> after_open = all_jobs(*store);
    EXPECT_EQ(after_open.size(), safe.size() + (interrupted ? 1 : 0)) << "l'apertura non deve avviare job";
    for (const auto& j : after_open) EXPECT_NE(j.state, job_state::running);
    const recovery_report& rep = store->recovery();
    std::set<std::string> safe_slots;
    for (const auto& p : rep.safe_phases) {
      safe_slots.insert(p.slot_name);
      EXPECT_EQ(p.artifact_ids.size(), 4u) << "immagine, due report e provenance verificati";
      EXPECT_TRUE(p.defects.empty());
    }
    std::set<std::string> expected_slots;
    for (const std::size_t k : safe) expected_slots.insert(slot_of(k));
    EXPECT_EQ(safe_slots, expected_slots) << "fasi sicure elencate anche senza interventi";
    if (interrupted) {
      ASSERT_EQ(rep.incomplete_results.size(), 1u);
      EXPECT_EQ(rep.incomplete_results[0].slot_name, slot_of(*interrupted));
      EXPECT_EQ(rep.incomplete_results[0].node_key, make_calibration_node(f.recipes[*interrupted]).key());
    } else {
      EXPECT_TRUE(rep.incomplete_results.empty());
    }
    EXPECT_EQ(store->recovery_reports().size(), interrupted ? 1u : 0u) << "un report si persiste solo se la recovery interviene";
    EXPECT_TRUE(rep.damaged_phases.empty());
    // Provenance attesa del resume: le fasi sicure del report, ciascuna con i digest dei suoi output in ordine di id.
    json reused_items = json::array();
    for (const auto& p : rep.safe_phases) {
      json digests = json::array();
      for (const auto& a : store->artifacts_for(p.node_key)) {
        if (std::find(p.artifact_ids.begin(), p.artifact_ids.end(), a.id) != p.artifact_ids.end()) digests.push(json::string(a.sha256));
      }
      json item = json::object();
      item.set("slot_name", json::string(p.slot_name));
      item.set("node_key", json::string(p.node_key));
      item.set("generation", json::uinteger(p.generation));
      item.set("artifacts", std::move(digests));
      reused_items.push(std::move(item));
    }
    const std::string expected_reused = canonical_json(reused_items);

    runner_env env;
    const workflow wf = make_workflow(*store, f, env, {});
    const workflow_result r = resume_workflow(*store, wf);
    ASSERT_EQ(r.exit_code, 0);
    ASSERT_EQ(r.steps.size(), k_count);
    for (std::size_t k = 0; k < k_count; ++k) {
      const std::string key = wf.steps[k].node.key();
      EXPECT_EQ(r.steps[k].action, safe.count(k) != 0 ? step_action::reused : step_action::executed) << slot_of(k);
      const auto provenance = with_role(*store, key, provenance_output_role);
      ASSERT_EQ(provenance.size(), 1u) << "esecuzioni del nodo " << slot_of(k);
      EXPECT_EQ(jobs_of(*store, key), safe.count(k) != 0 || interrupted != k ? 1u : 2u) << slot_of(k);
      const json doc = read_json_artifact(*store, provenance[0]);
      const json* resume = doc.find("resume");
      if (safe.count(k) != 0) {
        EXPECT_EQ(resume, nullptr) << "un passo riusato conserva la provenance dell'esecuzione originale";
      } else {
        ASSERT_NE(resume, nullptr) << slot_of(k);
        EXPECT_EQ(resume->keys(), (std::vector<std::string>{"definition_sha256", "reused", "workflow"}));
        EXPECT_EQ(resume->find("workflow")->as_string(), "calib");
        const std::string definition = *store->get_setting("workflow.calib");
        EXPECT_EQ(resume->find("definition_sha256")->as_string(), sha256_hex(st::bytes_of(definition)));
        EXPECT_EQ(canonical_json(*resume->find("reused")), expected_reused) << "passi riusati: tutti e solo i sicuri, in ordine";
      }
    }
    EXPECT_EQ(output_digests(*store, k_count), oracle);
  }
}

void expect_stopped_at(const workflow_result& r, const workflow& wf, const sqlite_dag_store& store, std::size_t at,
                       int exit_code, error_category category, const std::string& code) {
  EXPECT_EQ(r.exit_code, exit_code);
  ASSERT_EQ(r.steps.size(), wf.steps.size());
  ASSERT_EQ(r.steps[at].action, step_action::executed);
  ASSERT_TRUE(r.steps[at].outcome.has_value());
  EXPECT_EQ(r.steps[at].outcome->exit_code, exit_code);
  ASSERT_TRUE(r.steps[at].outcome->stop.has_value());
  EXPECT_EQ(r.steps[at].outcome->stop->category(), category);
  EXPECT_EQ(r.steps[at].outcome->stop->code(), code);
  for (std::size_t k = at + 1; k < wf.steps.size(); ++k) {
    EXPECT_EQ(r.steps[k].action, step_action::not_reached) << slot_of(k);
    EXPECT_FALSE(r.steps[k].outcome.has_value());
    EXPECT_EQ(jobs_of(store, wf.steps[k].node.key()), 0u) << "nessun job dopo il passo fermato: " << slot_of(k);
  }
}

TEST(Recovery, T_M2R4_01_a_stopped_or_cancelled_step_closes_the_workflow_in_run_and_in_resume) {
  constexpr std::size_t k_count = 3;
  const frames f = make_frames("r401s", k_count);
  {
    SCOPED_TRACE("passo fermato: input alterato dopo la ricetta");
    const fs::path dir = st::fresh_dir("r401s_fail");
    setup_project(dir, f);
    auto store = sqlite_dag_store::open(dir, open_intent::read_write);
    runner_env env;
    const workflow wf = make_workflow(*store, f, env, {});
    const fs::path light = f.recipes[1].light.path;
    const std::string original = st::read_text(light);
    std::string altered = original;
    altered[altered.size() - 1] = static_cast<char>(altered.back() ^ 1);
    st::write_text(light, altered);
    const workflow_result r = run_workflow(*store, wf);
    EXPECT_EQ(r.steps[0].action, step_action::executed);
    expect_stopped_at(r, wf, *store, 1, exit_code_stop, error_category::integrity, "input.digest_mismatch");
    const workflow_result again = resume_workflow(*store, wf);
    EXPECT_EQ(again.steps[0].action, step_action::reused);
    expect_stopped_at(again, wf, *store, 1, exit_code_stop, error_category::integrity, "input.digest_mismatch");
    st::write_text(light, original);
    const workflow_result done = resume_workflow(*store, wf);
    ASSERT_EQ(done.exit_code, 0);
    EXPECT_EQ(done.steps[0].action, step_action::reused);
    EXPECT_EQ(done.steps[1].action, step_action::executed);
    EXPECT_EQ(done.steps[2].action, step_action::executed);
  }
  {
    SCOPED_TRACE("passo annullato");
    const fs::path dir = st::fresh_dir("r401s_cancel");
    setup_project(dir, f);
    auto store = sqlite_dag_store::open(dir, open_intent::read_write);
    runner_env env;
    env.cancel.request();
    const workflow wf = make_workflow(*store, f, env, {});
    expect_stopped_at(run_workflow(*store, wf), wf, *store, 0, exit_code_cancelled, error_category::cancelled,
                      "sched.cancelled");
    expect_stopped_at(resume_workflow(*store, wf), wf, *store, 0, exit_code_cancelled, error_category::cancelled,
                      "sched.cancelled");
  }
}

TEST(Recovery, T_M2R4_02_invalid_workflow_definitions_and_foreign_jobs_are_refused) {
  constexpr std::size_t k_count = 2;
  const frames f = make_frames("r402d", k_count);
  const fs::path dir = st::fresh_dir("r402d_run");
  setup_project(dir, f);
  auto store = sqlite_dag_store::open(dir, open_intent::read_write);
  runner_env env;
  const workflow good = make_workflow(*store, f, env, {});
  const auto refused = [&](const char* label, const std::function<void(workflow&)>& change, const char* code) {
    SCOPED_TRACE(label);
    workflow wf = good;
    change(wf);
    EXPECT_STOP(run_workflow(*store, wf), error_category::invalid_input, code);
    EXPECT_STOP(resume_workflow(*store, wf), code == std::string("workflow.definition") ? error_category::invalid_input
                                                                                        : error_category::resume_mismatch,
                code == std::string("workflow.definition") ? code : "resume.unknown_workflow");
    EXPECT_TRUE(all_jobs(*store).empty());
    EXPECT_FALSE(store->get_setting("workflow.calib").has_value());
  };
  refused("nome vuoto", [](workflow& wf) { wf.name.clear(); }, "workflow.definition");
  refused("nome con maiuscole", [](workflow& wf) { wf.name = "Calib"; }, "workflow.definition");
  refused("nome troppo lungo", [](workflow& wf) { wf.name = std::string(65, 'a'); }, "workflow.definition");
  refused("nessun passo", [](workflow& wf) { wf.steps.clear(); }, "workflow.definition");
  refused("slot ripetuto", [](workflow& wf) { wf.steps[1].slot_name = wf.steps[0].slot_name; }, "workflow.definition");
  refused("passo senza esecuzione", [](workflow& wf) { wf.steps[1].run = nullptr; }, "workflow.definition");
  refused("target dello slot diverso dal nodo", [](workflow& wf) { std::swap(wf.steps[0].node, wf.steps[1].node); },
          "workflow.slot_target");
  refused("slot assente", [](workflow& wf) { wf.steps[1].slot_name = "cal9"; }, "workflow.slot_target");
  {
    SCOPED_TRACE("un passo che esegue il job di un altro slot");
    workflow wf = good;
    wf.steps[0].run = good.steps[1].run;
    EXPECT_STOP(run_workflow(*store, wf), error_category::invalid_input, "workflow.step_job");
    EXPECT_EQ(jobs_of(*store, good.steps[0].node.key()), 0u);
  }
}

TEST(Recovery, T_M2R4_02_resume_with_a_different_recipe_parameters_or_backend_is_resume_mismatch_and_runs_nothing) {
  constexpr std::size_t k_count = 3;
  const frames f = make_frames("r402", k_count);
  std::vector<std::string> points;
  {
    const fs::path dir = st::fresh_dir("r402_dry");
    setup_project(dir, f);
    points = st::record_points([&](const fault_injector& inj) { child_workflow(dir, f, inj); });
  }
  const fs::path dir = st::fresh_dir("r402_run");
  setup_project(dir, f);
  const std::size_t kill_at = positions(points, "calib.preflight.done")[1];
  ASSERT_TRUE(st::run_child([&](const fault_injector& inj) { child_workflow(dir, f, inj); }, static_cast<int>(kill_at)).killed);

  auto store = sqlite_dag_store::open(dir, open_intent::read_write);
  runner_env env;
  const workflow good = make_workflow(*store, f, env, {});
  const auto must_not_run = []() -> job_outcome {
    ADD_FAILURE() << "un passo e' stato eseguito da un resume rifiutato";
    throw std::runtime_error("passo eseguito da un resume rifiutato");
  };
  const auto variant = [&](const std::function<void(workflow&)>& change) {
    workflow wf = good;
    for (auto& s : wf.steps) s.run = must_not_run;
    change(wf);
    return wf;
  };
  const auto state_of = [&] {
    return std::make_pair(all_jobs(*store).size(), st::list_dir(dir / "artifacts"));
  };
  const auto before = state_of();
  const auto expect_refused = [&](const char* label, const workflow& wf, const char* code) {
    SCOPED_TRACE(label);
    EXPECT_STOP(resume_workflow(*store, wf), error_category::resume_mismatch, code);
    EXPECT_EQ(state_of(), before);
  };

  calibration_recipe other = f.recipes[1];
  other.q_min = 0.02;
  expect_refused("parametri", variant([&](workflow& wf) { wf.steps[1].node = make_calibration_node(other); }), "resume.recipe");
  expect_refused("ordine", variant([](workflow& wf) { std::swap(wf.steps[0], wf.steps[1]); }), "resume.recipe");
  expect_refused("passo mancante", variant([](workflow& wf) { wf.steps.pop_back(); }), "resume.recipe");
  expect_refused("passo in piu'", variant([&](workflow& wf) { wf.steps.push_back(workflow_step{"cal9", wf.steps[0].node, must_not_run}); }),
                 "resume.recipe");
  expect_refused("backend", variant([](workflow& wf) {
                   const node_spec& n = wf.steps[2].node;
                   wf.steps[2].node = node_spec(n.operator_id(), n.algorithm_version(), n.serialization_version(), n.params(),
                                                n.inputs(), n.precision(), "cpu_avx2", n.seed());
                 }),
                 "resume.recipe");
  expect_refused("workflow ignoto", variant([](workflow& wf) { wf.name = "altro"; }), "resume.unknown_workflow");

  const node_spec changed = make_calibration_node(other);
  store->put_node(changed);
  const std::string original_target = store->get_slot("cal1")->target_node_key;
  store->retarget_slot("cal1", changed.key());
  {
    SCOPED_TRACE("target dello slot cambiato");
    workflow wf = variant([](workflow&) {});
    EXPECT_STOP(resume_workflow(*store, wf), error_category::resume_mismatch, "resume.recipe");
    EXPECT_EQ(state_of(), before);
  }
  // Un resume rifiutato non ha consumato nulla: con la ricetta identica il resume riprende dai passi sicuri.
  store->retarget_slot("cal1", original_target);
  const workflow_result r = resume_workflow(*store, good);
  ASSERT_EQ(r.exit_code, 0);
  EXPECT_EQ(r.steps[0].action, step_action::reused);
  EXPECT_EQ(r.steps[1].action, step_action::executed);
  EXPECT_EQ(r.steps[2].action, step_action::executed);

  const std::string key0 = good.steps[0].node.key();
  const std::string key1 = good.steps[1].node.key();
  {
    SCOPED_TRACE("fase danneggiata da un digest: stop, nessun job");
    const fs::path image = store->artifact_path(store->read_slot("cal0").artifact->rel_path);
    const std::string original = st::read_text(image);
    std::string altered = original;
    altered[altered.size() - 1] = static_cast<char>(altered.back() ^ 1);
    st::write_text(image, altered);
    const auto jobs_before = all_jobs(*store).size();
    EXPECT_STOP(resume_workflow(*store, variant([](workflow&) {})), error_category::integrity, "resume.artifact_digest");
    EXPECT_EQ(all_jobs(*store).size(), jobs_before);
    st::write_text(image, original);
  }
  {
    SCOPED_TRACE("file di una riga present sparito dopo la recovery: stop io, nessun job");
    const auto texts = with_role(*store, key1, calibration_report_text_role);
    ASSERT_FALSE(texts.empty());
    const fs::path file = dir / "artifacts" / texts.back().rel_path;
    const std::string bytes = st::read_text(file);
    fs::remove(file);
    const auto jobs_before = all_jobs(*store).size();
    EXPECT_STOP(resume_workflow(*store, variant([](workflow&) {})), error_category::io, "resume.artifact_missing");
    EXPECT_EQ(all_jobs(*store).size(), jobs_before);
    st::write_text(file, bytes);
  }
  {
    SCOPED_TRACE("fase danneggiata solo da un output absent: il passo si riesegue");
    const auto prov = with_role(*store, key1, provenance_output_role);
    ASSERT_EQ(prov.size(), 1u);
    store.reset();
    fs::remove(dir / "artifacts" / prov[0].rel_path);
    store = sqlite_dag_store::open(dir, open_intent::read_write);
    ASSERT_EQ(store->recovery().damaged_phases.size(), 1u);
    EXPECT_EQ(store->recovery().damaged_phases[0].slot_name, "cal1");
    ASSERT_EQ(store->recovery().damaged_phases[0].defects.size(), 1u);
    EXPECT_EQ(store->recovery().damaged_phases[0].defects[0].cause, phase_defect_cause::absent);
    const workflow again = make_workflow(*store, f, env, {});
    const workflow_result r2 = resume_workflow(*store, again);
    ASSERT_EQ(r2.exit_code, 0);
    EXPECT_EQ(r2.steps[0].action, step_action::reused);
    EXPECT_EQ(r2.steps[1].action, step_action::executed);
    EXPECT_EQ(r2.steps[2].action, step_action::reused);
    EXPECT_EQ(jobs_of(*store, key0), 1u);
  }
  {
    SCOPED_TRACE("file ripristinato dopo la recovery con riga present = 0: absent, il passo si riesegue riusando il nome");
    const slot_view v = store->read_slot("cal0");
    ASSERT_EQ(v.state, freshness::current);
    const fs::path image = dir / "artifacts" / v.artifact->rel_path;
    const std::string bytes = st::read_text(image);
    store.reset();
    fs::remove(image);
    store = sqlite_dag_store::open(dir, open_intent::read_write);
    st::write_text(image, bytes);
    store.reset();
    store = sqlite_dag_store::open(dir, open_intent::read_write);
    ASSERT_EQ(store->recovery().damaged_phases.size(), 1u);
    const phase_record& damaged = store->recovery().damaged_phases[0];
    EXPECT_EQ(damaged.slot_name, "cal0");
    ASSERT_EQ(damaged.defects.size(), 1u);
    EXPECT_EQ(damaged.defects[0].cause, phase_defect_cause::absent);
    EXPECT_EQ(damaged.defects[0].found_sha256, v.artifact->sha256);
    const workflow again = make_workflow(*store, f, env, {});
    const workflow_result r3 = resume_workflow(*store, again);
    ASSERT_EQ(r3.exit_code, 0);
    EXPECT_EQ(r3.steps[0].action, step_action::executed);
    EXPECT_EQ(r3.steps[1].action, step_action::reused);
    EXPECT_EQ(store->read_slot("cal0").artifact->sha256, v.artifact->sha256);
    EXPECT_EQ(jobs_of(*store, key0), 2u);
  }
  {
    SCOPED_TRACE("primario mancante e secondario di dimensione diversa (marcato absent, file presente): stop");
    const std::string key2 = good.steps[2].node.key();
    const slot_view v = store->read_slot("cal2");
    ASSERT_EQ(v.state, freshness::current);
    const auto reports = with_role(*store, key2, calibration_report_text_role);
    ASSERT_FALSE(reports.empty());
    const artifact_record& report = reports.back();
    store.reset();
    fs::remove(dir / "artifacts" / v.artifact->rel_path);
    st::write_text(dir / "artifacts" / report.rel_path, st::read_text(dir / "artifacts" / report.rel_path) + "x");
    store = sqlite_dag_store::open(dir, open_intent::read_write);
    ASSERT_EQ(store->recovery().damaged_phases.size(), 1u);
    const phase_record& damaged = store->recovery().damaged_phases[0];
    EXPECT_EQ(damaged.slot_name, "cal2");
    std::set<std::pair<std::uint64_t, phase_defect_cause>> causes;
    for (const auto& d : damaged.defects) causes.insert({d.artifact_id, d.cause});
    EXPECT_EQ(causes, (std::set<std::pair<std::uint64_t, phase_defect_cause>>{{v.artifact->id, phase_defect_cause::absent},
                                                                              {report.id, phase_defect_cause::digest}}));
    const auto jobs_before = all_jobs(*store).size();
    const workflow again = make_workflow(*store, f, env, {});
    EXPECT_STOP(resume_workflow(*store, again), error_category::integrity, "resume.artifact_digest");
    EXPECT_EQ(all_jobs(*store).size(), jobs_before);
  }
}

TEST(Recovery, T_M2R4_03_the_recovery_report_reread_after_a_second_reopen_matches_the_provoked_interventions) {
  const fs::path dir = st::fresh_dir("r403");
  recovery_report expected;
  {
    auto store = sqlite_dag_store::create(dir);
    store->apply_recipe(st::diamond(2).resolve(), st::diamond_slots());
    const artifact_record a = st::run_and_commit(*store, "sa", "payload a");
    const artifact_record b = st::run_and_commit(*store, "sb", "payload b");
    const artifact_record c = st::run_and_commit(*store, "sc", "payload c");
    // Slot `se` sul nodo di `sa`: stessa generazione di `sa` per slot diversi, il job produttore si distingue da job_id.
    store->create_slot("se", a.node_key);
    const job_record e_job = store->start_job("se");
    const std::vector<artifact_record> e = store->commit_job(
        e_job.id, store->write_files({artifact_content{st::bytes_of("payload e"), "bin", "image", std::nullopt},
                                      artifact_content{st::bytes_of("provenance e"), "json", "provenance", std::nullopt,
                                                       provenance_output_role}}));
    ASSERT_EQ(e.size(), 2u);
    ASSERT_EQ(e[0].generation, a.generation);
    const job_record running = store->start_job("sd");
    fs::remove(store->artifact_path(b.rel_path));
    fs::remove(store->artifact_path(e[1].rel_path));
    st::write_text(store->artifact_path(c.rel_path), "payload X");
    st::write_text(dir / "artifacts" / ".tmp-0123456789abcdef", "partial");
    const std::string orphan = digest_of(st::bytes_of("orphan")).sha256 + ".bin";
    st::write_text(dir / "artifacts" / orphan, "orphan");
    expected.interrupted_jobs = {running.id};
    expected.removed_temporaries = {".tmp-0123456789abcdef"};
    expected.removed_orphans = {orphan};
    expected.artifacts_marked_absent = {b.id, e[1].id};
    expected.incomplete_results = {incomplete_result{running.id, "sd", running.node_key, running.generation}};
    expected.safe_phases = {phase_record{"sa", a.node_key, a.generation, a.job_id, {a.id}, {}}};
    expected.damaged_phases = {
        phase_record{"sb", b.node_key, b.generation, b.job_id, {},
                     {phase_defect{b.id, b.rel_path, phase_defect_cause::absent, b.sha256, ""}}},
        phase_record{"sc", c.node_key, c.generation, c.job_id, {},
                     {phase_defect{c.id, c.rel_path, phase_defect_cause::digest, c.sha256,
                                   digest_of(st::bytes_of("payload X")).sha256}}},
        phase_record{"se", a.node_key, e[0].generation, e_job.id, {e[0].id},
                     {phase_defect{e[1].id, e[1].rel_path, phase_defect_cause::absent, e[1].sha256, ""}}}};
    ASSERT_TRUE(a.job_id.has_value());
  }
  recovery_report analysis = expected;
  analysis.interrupted_jobs.clear();
  analysis.removed_temporaries.clear();
  analysis.removed_orphans.clear();
  analysis.artifacts_marked_absent.clear();
  // Dopo la prima recovery il file di `sb` si ripristina: la riga resta present = 0 (nessuna riparazione), il difetto
  // resta `absent` con il digest trovato uguale all'atteso.
  ASSERT_EQ(analysis.damaged_phases[0].slot_name, "sb");
  analysis.damaged_phases[0].defects[0].found_sha256 = analysis.damaged_phases[0].defects[0].expected_sha256;
  {
    auto store = sqlite_dag_store::open(dir, open_intent::read_write);
    EXPECT_EQ(store->recovery(), expected);
    EXPECT_EQ(store->recovery_reports(), std::vector<recovery_report>{expected});
    EXPECT_EQ(store->read_slot("sb").state, freshness::missing);
  }
  st::write_text(dir / "artifacts" / expected.damaged_phases[0].defects[0].rel_path, "payload b");
  const auto listing = st::list_dir(dir / "artifacts");
  {
    auto store = sqlite_dag_store::open(dir, open_intent::read_write);
    EXPECT_EQ(store->recovery(), analysis) << "la seconda apertura non rimuove e non marca nulla";
    EXPECT_EQ(store->recovery_reports(), (std::vector<recovery_report>{expected, analysis}));
  }
  EXPECT_EQ(st::list_dir(dir / "artifacts"), listing);
  {
    auto store = sqlite_dag_store::open(dir, open_intent::read_write);
    EXPECT_EQ(store->recovery(), analysis);
    EXPECT_EQ(store->recovery_reports(), (std::vector<recovery_report>{expected, analysis})) << "stato invariato, nessuna riga";
  }
  {
    auto store = sqlite_dag_store::open(dir, open_intent::read_only);
    EXPECT_EQ(store->recovery_reports(), (std::vector<recovery_report>{expected, analysis}));
  }
  EXPECT_EQ(parse_recovery_report(recovery_report_canonical(expected)), expected);
  EXPECT_STOP(parse_recovery_report("{\"format\":\"astro.recovery.v2\"}"), error_category::invalid_input,
              "store.recovery_report_malformed");
  EXPECT_STOP(parse_recovery_report(" " + recovery_report_canonical(expected)), error_category::invalid_input,
              "store.recovery_report_malformed");
  {
    st::raw_db db(dir);
    EXPECT_NE(db.exec("UPDATE recovery_reports SET report_canonical='{}'"), SQLITE_OK);
    EXPECT_NE(db.exec("DELETE FROM recovery_reports"), SQLITE_OK);
  }
}

// ---------------------------------------------------------------------------------------------------------------
// Perdita di energia (D17): stato durevole a ogni punto di sync.

// Nomi e contenuti degli artefatti secondo il modello POSIX: contenuto all'ultimo fsync del file, nome all'ultimo fsync
// della directory.
struct artifact_model {
  fs::path dir;
  std::map<std::string, int> live;
  std::map<std::string, int> durable_names;
  std::map<int, std::string> content;
  int next = 1;

  void event(std::string_view ev, std::string_view name, std::string_view other) {
    const std::string n(name);
    if (ev == "create") {
      live[n] = next++;
    } else if (ev == "fsync") {
      content[live.at(n)] = st::read_text(dir / n);
    } else if (ev == "rename") {
      live[std::string(other)] = live.at(n);
      live.erase(n);
    } else if (ev == "unlink") {
      live.erase(n);
    } else if (ev == "fsync_dir") {
      durable_names = live;
    } else {
      ADD_FAILURE() << "evento ignoto " << ev;
    }
  }

  std::map<std::string, std::string> durable() const {
    std::map<std::string, std::string> out;
    for (const auto& [n, id] : durable_names) out[n] = content.count(id) != 0 ? content.at(id) : std::string();
    return out;
  }
};

struct durable_state {
  std::map<std::string, std::string> db;
  std::map<std::string, std::string> artifacts;
  bool operator==(const durable_state&) const = default;
};

void materialize(const fs::path& dir, const durable_state& s) {
  fs::create_directories(dir / "artifacts");
  fs::create_directories(dir / "backups");
  st::write_text(dir / "project.lock", "");
  for (const auto& [n, bytes] : s.db) st::write_text(dir / n, bytes);
  for (const auto& [n, bytes] : s.artifacts) st::write_text(dir / "artifacts" / n, bytes);
}

void check_durable_invariants(const fs::path& dir, std::size_t k_count) {
  std::set<std::string> referenced;
  {
    auto store = sqlite_dag_store::open(dir, open_intent::read_write);
    const std::vector<job_record> jobs = all_jobs(*store);
    for (const auto& j : jobs) {
      EXPECT_NE(j.state, job_state::running);
      if (j.state != job_state::completed) continue;
      std::set<std::string> roles;
      for (const auto& a : store->artifacts_for(j.node_key)) {
        if (a.job_id == j.id && a.present) roles.insert(a.output_role);
      }
      EXPECT_EQ(roles, (std::set<std::string>{"primary", "report", "report_text", "provenance"})) << "job " << j.id;
    }
    for (std::size_t k = 0; k < k_count; ++k) {
      const slot_view v = store->read_slot(slot_of(k));
      if (v.state != freshness::current) continue;
      ASSERT_TRUE(v.artifact->job_id.has_value()) << slot_of(k);
      const auto producer = store->get_job(*v.artifact->job_id);
      ASSERT_TRUE(producer.has_value()) << slot_of(k);
      EXPECT_EQ(producer->state, job_state::completed) << slot_of(k) << " current senza job produttore completed";
      EXPECT_EQ(producer->node_key, v.artifact->node_key);
    }
  }
  st::raw_db db(dir, true);
  EXPECT_EQ(db.query("PRAGMA integrity_check"), "ok|\n");
  const std::string rows = db.query("SELECT rel_path, sha256, present FROM artifacts ORDER BY id");
  std::istringstream in(rows);
  for (std::string line; std::getline(in, line);) {
    const auto p1 = line.find('|');
    const auto p2 = line.find('|', p1 + 1);
    const std::string rel = line.substr(0, p1);
    const std::string sha = line.substr(p1 + 1, p2 - p1 - 1);
    if (line.substr(p2 + 1, 1) != "1") continue;
    referenced.insert(rel);
    ASSERT_TRUE(fs::exists(dir / "artifacts" / rel)) << rel;
    EXPECT_EQ(file_digest(dir / "artifacts" / rel).sha256, sha) << rel;
  }
  for (const std::string& n : st::list_dir(dir / "artifacts")) {
    EXPECT_EQ(referenced.count(n), 1u) << "file senza riga present: " << n;
  }
}

TEST(Recovery, T_M2R4_04_simulated_power_loss_at_every_sync_point_keeps_the_project_consistent) {
  constexpr std::size_t k_count = 2;
  const frames f = make_frames("r404", k_count);
  // Un output del passo 0, ricavato da un progetto separato: lo si ripristina in artifacts/ senza fsync durante il
  // workflow, cosi' la scrittura atomica lo riusa (EEXIST) e deve renderlo durevole prima del commit.
  std::string reused_name;
  std::string reused_bytes;
  {
    const fs::path oracle = st::fresh_dir("r404_oracle");
    setup_project(oracle, f);
    auto store = sqlite_dag_store::open(oracle, open_intent::read_write);
    runner_env env;
    ASSERT_EQ(run_workflow(*store, make_workflow(*store, f, env, {})).exit_code, 0);
    const auto texts = with_role(*store, make_calibration_node(f.recipes[0]).key(), calibration_report_text_role);
    ASSERT_EQ(texts.size(), 1u);
    reused_name = texts[0].rel_path;
    reused_bytes = st::read_text(store->artifact_path(reused_name));
  }
  const fs::path dir = st::fresh_dir("r404_run");
  setup_project(dir, f);
  powerloss::recorder rec;
  for (const char* n : {"project.db", "project.db-wal"}) {
    if (fs::exists(dir / n)) rec.durable[n] = st::read_text(dir / n);
  }
  artifact_model model;
  model.dir = dir / "artifacts";
  std::vector<durable_state> states;
  const auto capture = [&] {
    durable_state s{rec.durable, model.durable()};
    if (states.empty() || !(states.back() == s)) states.push_back(std::move(s));
  };
  std::size_t db_syncs = 0, file_syncs = 0, dir_syncs = 0;
  rec.on_sync = [&] {
    ++db_syncs;
    capture();
  };
  powerloss::install(&rec);
  {
    open_options options;
    options.vfs = powerloss::vfs_name;
    options.observer = [&](std::string_view ev, std::string_view n, std::string_view other) {
      model.event(ev, n, other);
      if (ev == "fsync") ++file_syncs;
      if (ev == "fsync_dir") ++dir_syncs;
      if (ev == "fsync" || ev == "fsync_dir") capture();
    };
    auto store = sqlite_dag_store::open(dir, open_intent::read_write, options);
    runner_env env;
    bool planted = false;
    const fault_injector plant = [&](std::string_view point) {
      if (point == "workflow.step_begin" && !planted) {
        planted = true;
        st::write_text(dir / "artifacts" / reused_name, reused_bytes);
        model.live[reused_name] = model.next++;  // nome durevole, contenuto mai sincronizzato
        model.durable_names[reused_name] = model.live[reused_name];
      }
      return 0;
    };
    ASSERT_EQ(run_workflow(*store, make_workflow(*store, f, env, plant)).exit_code, 0);
    ASSERT_TRUE(planted);
  }
  capture();  // stato durevole dopo la chiusura (checkpoint e cancellazione del WAL)
  powerloss::install(nullptr);
  // Eventi obbligatori per passo: 4 file con fsync del file e della directory; almeno avvio del job e commit nel WAL
  // (db_syncs conta xSync e xDelete).
  EXPECT_EQ(file_syncs, 4 * k_count + 1) << "piu' il fsync del file riusato";
  EXPECT_EQ(dir_syncs, 4 * k_count);
  EXPECT_GE(db_syncs, 2 * k_count);
  ASSERT_GE(states.size(), 2u);
  std::size_t with_all_current = 0;
  for (std::size_t i = 0; i < states.size(); ++i) {
    SCOPED_TRACE("stato durevole " + std::to_string(i) + " di " + std::to_string(states.size()));
    const fs::path state_dir = st::fresh_dir("r404_state");
    materialize(state_dir, states[i]);
    check_durable_invariants(state_dir, k_count);
    auto store = sqlite_dag_store::open(state_dir, open_intent::read_only);
    bool all = true;
    for (std::size_t k = 0; k < k_count; ++k) all = all && store->read_slot(slot_of(k)).state == freshness::current;
    with_all_current += all ? 1 : 0;
    if (i + 1 == states.size()) {
      EXPECT_TRUE(all) << "lo stato durevole finale, dopo la chiusura, deve avere tutti gli slot pubblicati";
      EXPECT_EQ(states[i].db.count("project.db-wal"), 0u) << "la chiusura cancella il WAL dopo il checkpoint";
    }
  }
  EXPECT_LT(with_all_current, states.size()) << "gli stati intermedi devono precedere la pubblicazione";
}

// ---------------------------------------------------------------------------------------------------------------
// Regola hardlink (D18).

struct file_facts {
  std::string sha256;
  ino_t ino = 0;
  nlink_t nlink = 0;
  timespec mtime{};
  mode_t mode = 0;
  bool operator==(const file_facts& o) const {
    return sha256 == o.sha256 && ino == o.ino && nlink == o.nlink && mtime.tv_sec == o.mtime.tv_sec &&
           mtime.tv_nsec == o.mtime.tv_nsec && mode == o.mode;
  }
};

file_facts facts_of(const fs::path& p) {
  struct stat s {};
  EXPECT_EQ(::stat(p.c_str(), &s), 0) << p;
  return file_facts{file_digest(p).sha256, s.st_ino, s.st_nlink, s.st_mtim, s.st_mode};
}

struct hardlink_fixture {
  fs::path base;
  fs::path project;
  fs::path original;
  artifact_record a;
};

hardlink_fixture hardlink_project(const std::string& name) {
  hardlink_fixture h;
  h.project = st::fresh_dir(name);
  h.base = h.project.parent_path() / (name + "_side");
  fs::create_directories(h.base);
  h.original = h.base / "original.fits";
  st::write_text(h.original, "original pixels");
  auto store = sqlite_dag_store::create(h.project);
  const content_digest d = file_digest(h.original);
  const input_record in = store->add_input(input_record{0, h.original.string(), d.size_bytes, d.sha256, "light"});
  struct stat s {};
  EXPECT_EQ(::stat(h.original.c_str(), &s), 0);
  EXPECT_EQ(in.dev, static_cast<std::uint64_t>(s.st_dev));
  EXPECT_EQ(in.ino, static_cast<std::uint64_t>(s.st_ino));
  store->apply_recipe(st::diamond(2).resolve(), st::diamond_slots());
  h.a = st::run_and_commit(*store, "sa", "payload a");
  store->start_job("sb");  // job in corso: una recovery che scrive lo marcherebbe interrupted
  return h;
}

// mtime e ctime della directory del progetto: una connessione SQLite che crea e poi cancella -wal e -shm li cambia
// anche se i file non restano (timestamp a grana fine dopo un'osservazione).
std::pair<timespec, timespec> dir_times(const fs::path& dir) {
  struct stat s {};
  EXPECT_EQ(::stat(dir.c_str(), &s), 0) << dir;
  return {s.st_mtim, s.st_ctim};
}

bool same_times(const std::pair<timespec, timespec>& a, const std::pair<timespec, timespec>& b) {
  return a.first.tv_sec == b.first.tv_sec && a.first.tv_nsec == b.first.tv_nsec && a.second.tv_sec == b.second.tv_sec &&
         a.second.tv_nsec == b.second.tv_nsec;
}

std::string job_states(const fs::path& project) {
  st::raw_db db(project, true);
  return db.query("SELECT id, state FROM jobs ORDER BY id");
}

TEST(Recovery, T_M2R4_05_hardlinks_of_originals_or_project_files_stop_as_security_without_writes_or_removals) {
  {
    SCOPED_TRACE("originale in artifacts/ come orfano: recovery");
    const hardlink_fixture h = hardlink_project("r405a");
    const std::string name = file_digest(h.original).sha256 + ".bin";
    ASSERT_EQ(::link(h.original.c_str(), (h.project / "artifacts" / name).c_str()), 0);
    const file_facts before = facts_of(h.original);
    const auto listing = st::list_dir(h.project / "artifacts");
    const std::string db_before = file_digest(h.project / "project.db").sha256;
    const auto times_before = dir_times(h.project);
    // Prima della connessione gli originali non sono leggibili: l'hardlink si ferma per st_nlink >= 2.
    EXPECT_STOP(sqlite_dag_store::open(h.project, open_intent::read_write), error_category::security, "fs.hardlink");
    EXPECT_TRUE(same_times(dir_times(h.project), times_before)) << "nessuna connessione aperta (nessun -wal o -shm creato)";
    EXPECT_EQ(facts_of(h.original), before);
    EXPECT_EQ(before.nlink, 2u);
    EXPECT_EQ(st::list_dir(h.project / "artifacts"), listing);
    EXPECT_EQ(file_digest(h.project / "project.db").sha256, db_before);
    EXPECT_NE(job_states(h.project).find("|running|"), std::string::npos);
  }
  if (HasFailure()) return;  // un caso fallito chiude il test: diagnosi del primo caso, uscita corta
  {
    SCOPED_TRACE("originale come nome riusato dalla scrittura atomica (cache degli artefatti)");
    const hardlink_fixture h = hardlink_project("r405b");
    auto store = sqlite_dag_store::open(h.project, open_intent::read_write);
    const std::string name = file_digest(h.original).sha256 + ".bin";
    ASSERT_EQ(::link(h.original.c_str(), (h.project / "artifacts" / name).c_str()), 0);
    const file_facts before = facts_of(h.original);
    const auto listing = st::list_dir(h.project / "artifacts");
    const auto bytes = st::bytes_of(st::read_text(h.original));
    std::vector<std::string> points;
    store->set_fault_injector([&](std::string_view p) {
      points.emplace_back(p);
      return 0;
    });
    EXPECT_STOP(store->write_files({artifact_content{bytes, "bin", "image", std::nullopt}}), error_category::security,
                "fs.original_inode");
    EXPECT_TRUE(points.empty()) << "il rifiuto precede la creazione del temporaneo: " << (points.empty() ? "" : points[0]);
    // Due output, il secondo sul nome dell'hardlink: nessun file del primo viene scritto.
    EXPECT_STOP(store->write_files({artifact_content{st::bytes_of("output nuovo"), "bin", "image", std::nullopt},
                                    artifact_content{bytes, "bin", "image", std::nullopt, "report"}}),
                error_category::security, "fs.original_inode");
    EXPECT_TRUE(points.empty()) << "nessun output scritto prima del rifiuto: " << (points.empty() ? "" : points[0]);
    EXPECT_EQ(facts_of(h.original), before);
    EXPECT_EQ(st::list_dir(h.project / "artifacts"), listing);
  }
  if (HasFailure()) return;
  {
    SCOPED_TRACE("originale come destinazione dell'export");
    const hardlink_fixture h = hardlink_project("r405c");
    auto store = sqlite_dag_store::open(h.project, open_intent::read_write);
    const fs::path target = h.base / "export_target";
    ASSERT_EQ(::link(h.original.c_str(), target.c_str()), 0);
    const file_facts before = facts_of(h.original);
    EXPECT_STOP(store->export_slots(target, {"sa"}), error_category::security, "fs.original_inode");
    EXPECT_EQ(facts_of(h.original), before);
    EXPECT_EQ(facts_of(target), before);
  }
  if (HasFailure()) return;
  {
    SCOPED_TRACE("artefatto del progetto con nlink 2");
    const hardlink_fixture h = hardlink_project("r405d");
    const fs::path extra = h.base / "extra_link";
    const file_facts original_before = facts_of(h.original);
    {
      auto store = sqlite_dag_store::open(h.project, open_intent::read_write);
      ASSERT_EQ(::link(store->artifact_path(h.a.rel_path).c_str(), extra.c_str()), 0);
      EXPECT_STOP(store->export_slots(h.base / "export_d", {"sa"}), error_category::security, "fs.hardlink");
      EXPECT_FALSE(fs::exists(h.base / "export_d"));
      EXPECT_STOP(store->check_phase("sa"), error_category::security, "fs.hardlink");
      std::vector<std::string> points;
      store->set_fault_injector([&](std::string_view p) {
        points.emplace_back(p);
        return 0;
      });
      EXPECT_STOP(store->write_files({artifact_content{st::bytes_of("payload a"), "bin", "image", std::nullopt}}),
                  error_category::security, "fs.hardlink");
      EXPECT_TRUE(points.empty()) << "il rifiuto precede la creazione del temporaneo";
      store->set_fault_injector({});
      store->start_job("sc");
    }
    const auto listing = st::list_dir(h.project / "artifacts");
    const std::string db_before = file_digest(h.project / "project.db").sha256;
    const std::string jobs_before = job_states(h.project);
    EXPECT_STOP(sqlite_dag_store::open(h.project, open_intent::read_write), error_category::security, "fs.hardlink");
    EXPECT_EQ(st::list_dir(h.project / "artifacts"), listing);
    EXPECT_EQ(file_digest(h.project / "project.db").sha256, db_before);
    EXPECT_EQ(job_states(h.project), jobs_before);
    EXPECT_EQ(facts_of(extra).nlink, 2u);
    EXPECT_EQ(facts_of(h.original), original_before);
  }
  if (HasFailure()) return;
  {
    SCOPED_TRACE("project.db con nlink 2");
    const hardlink_fixture h = hardlink_project("r405e");
    const fs::path extra = h.base / "db_link";
    ASSERT_EQ(::link((h.project / "project.db").c_str(), extra.c_str()), 0);
    const std::string db_before = file_digest(h.project / "project.db").sha256;
    const auto times_before = dir_times(h.project);
    EXPECT_STOP(sqlite_dag_store::open(h.project, open_intent::read_write), error_category::security, "fs.hardlink");
    EXPECT_STOP(sqlite_dag_store::open(h.project, open_intent::read_only), error_category::security, "fs.hardlink");
    EXPECT_EQ(file_digest(h.project / "project.db").sha256, db_before);
    EXPECT_TRUE(same_times(dir_times(h.project), times_before)) << "nessuna connessione aperta (nessun -wal o -shm creato)";
  }
  if (HasFailure()) return;
  {
    SCOPED_TRACE("originale assente all'import");
    const fs::path dir = st::fresh_dir("r405f");
    auto store = sqlite_dag_store::create(dir);
    EXPECT_STOP(store->add_input(input_record{0, (dir / "missing.fits").string(), 1, std::string(64, 'a'), "light"}),
                error_category::io, "input.missing");
    EXPECT_STOP(store->add_input(input_record{0, (dir / "artifacts").string(), 1, std::string(64, 'a'), "light"}),
                error_category::invalid_input, "store.bad_input");
  }
}

// ---------------------------------------------------------------------------------------------------------------
// Report di export (NFR-REP-01.a, C-10 campo 7).

void rewrite_bundle(const fs::path& dir, const std::string& manifest) {
  st::write_text(dir / "manifest.json", manifest);
  st::write_text(dir / "COMMIT", sha256_hex(st::bytes_of(manifest)));
}

TEST(Recovery, T_M2R4_06_export_report_is_in_the_bundle_listed_in_the_manifest_and_checked_by_verify_export) {
  const fs::path project = st::fresh_dir("r406");
  auto store = sqlite_dag_store::create(project);
  store->apply_recipe(st::diamond(2).resolve(), st::diamond_slots());
  st::run_and_commit(*store, "sb", "payload b");
  st::run_and_commit(*store, "sd", "payload d");
  const fs::path out = project.parent_path() / "r406_export";
  fs::remove_all(out);
  store->export_slots(out, {"sb", "sd"});
  const export_manifest m = verify_export(out);
  ASSERT_TRUE(fs::exists(out / export_report_name));
  const std::string report_text = st::read_text(out / export_report_name);
  EXPECT_EQ(m.report, digest_of(st::bytes_of(report_text)));
  const json manifest = parse_json(st::read_text(out / "manifest.json"));
  const json* listed = manifest.find("report");
  ASSERT_NE(listed, nullptr);
  EXPECT_EQ(listed->find("rel_path")->as_string(), export_report_name);
  const json report = parse_json(report_text);
  EXPECT_EQ(canonical_json(report), report_text);
  EXPECT_EQ(report.find("format")->as_string(), export_report_format);
  EXPECT_TRUE(report.find("non_representable")->items().empty());
  EXPECT_FALSE(report.find("non_representable_reason")->as_string().empty());
  ASSERT_EQ(report.find("files")->items().size(), m.files.size());
  for (std::size_t i = 0; i < m.files.size(); ++i) {
    const json& e = report.find("files")->items()[i];
    EXPECT_EQ(e.keys().size(), 8u);
    EXPECT_EQ(e.find("rel_path")->as_string(), m.files[i].rel_path);
    EXPECT_EQ(static_cast<std::uint64_t>(e.find("size_bytes")->as_integer()), m.files[i].size_bytes);
    EXPECT_EQ(e.find("sha256")->as_string(), m.files[i].sha256);
    EXPECT_EQ(e.find("slot_name")->as_string(), m.files[i].slot_name);
    EXPECT_EQ(static_cast<std::uint64_t>(e.find("generation")->as_integer()), m.files[i].generation);
    EXPECT_EQ(e.find("node_key")->as_string(), m.files[i].node_key);
    EXPECT_EQ(e.find("representation")->as_string(), "artifact_copy");
    EXPECT_EQ(e.find("kind")->as_string(), "image");
  }

  const fs::path bad = project.parent_path() / "r406_bad";
  const auto copy_bundle = [&] {
    fs::remove_all(bad);
    fs::copy(out, bad, fs::copy_options::recursive);
  };
  {
    SCOPED_TRACE("report alterato");
    copy_bundle();
    std::string t = report_text;
    t[t.size() / 2] = t[t.size() / 2] == 'a' ? 'b' : 'a';
    st::write_text(bad / export_report_name, t);
    EXPECT_STOP(verify_export(bad), error_category::integrity, "export.incomplete");
  }
  {
    SCOPED_TRACE("report assente");
    copy_bundle();
    fs::remove(bad / export_report_name);
    EXPECT_STOP(verify_export(bad), error_category::integrity, "export.incomplete");
  }
  const std::string manifest_text = st::read_text(out / "manifest.json");
  {
    SCOPED_TRACE("manifest senza report");
    copy_bundle();
    json doc = json::object();
    for (const auto& k : manifest.keys()) {
      if (k != "report") doc.set(k, *manifest.find(k));
    }
    rewrite_bundle(bad, canonical_json(doc));
    EXPECT_STOP(verify_export(bad), error_category::invalid_input, "export.manifest_malformed");
  }
  {
    SCOPED_TRACE("report con un altro nome");
    copy_bundle();
    json doc = json::object();
    for (const auto& k : manifest.keys()) {
      if (k != "report") doc.set(k, *manifest.find(k));
    }
    json r = json::object();
    r.set("rel_path", json::string("other.json"));
    r.set("sha256", json::string(m.report.sha256));
    r.set("size_bytes", json::uinteger(m.report.size_bytes));
    doc.set("report", std::move(r));
    rewrite_bundle(bad, canonical_json(doc));
    EXPECT_STOP(verify_export(bad), error_category::invalid_input, "export.manifest_malformed");
  }
  // Report alterato con manifest e COMMIT ricalcolati: bundle coerente, contenuto del report diverso dal manifest.
  const auto with_report = [&](const json& changed) {
    copy_bundle();
    const std::string new_report = canonical_json(changed);
    st::write_text(bad / export_report_name, new_report);
    json doc = json::object();
    for (const auto& k : manifest.keys()) {
      if (k != "report") doc.set(k, *manifest.find(k));
    }
    const content_digest d = digest_of(st::bytes_of(new_report));
    json r = json::object();
    r.set("rel_path", json::string(export_report_name));
    r.set("sha256", json::string(d.sha256));
    r.set("size_bytes", json::uinteger(d.size_bytes));
    doc.set("report", std::move(r));
    rewrite_bundle(bad, canonical_json(doc));
  };
  const auto replaced = [](const json& object, const std::string& key, const json& value) {
    json out = json::object();
    for (const auto& k : object.keys()) out.set(k, k == key ? value : *object.find(k));
    return out;
  };
  {
    SCOPED_TRACE("report con un elemento in meno");
    json files = json::array();
    files.push(report.find("files")->items()[0]);
    with_report(replaced(report, "files", files));
    EXPECT_STOP(verify_export(bad), error_category::integrity, "export.report_mismatch");
  }
  const json& first = report.find("files")->items()[0];
  const std::vector<std::pair<std::string, json>> single_field{
      {"rel_path", json::string("other.bin")},
      {"size_bytes", json::uinteger(m.files[0].size_bytes + 1)},
      {"sha256", json::string(std::string(64, 'f'))},
      {"slot_name", json::string("other")},
      {"generation", json::uinteger(m.files[0].generation + 1)},
      {"node_key", json::string(std::string(64, 'e'))},
      {"kind", json::string("")},
      {"representation", json::string("converted")},
  };
  for (const auto& [key, value] : single_field) {
    SCOPED_TRACE("campo del report alterato: " + key);
    json files = json::array();
    files.push(replaced(first, key, value));
    for (std::size_t i = 1; i < report.find("files")->items().size(); ++i) files.push(report.find("files")->items()[i]);
    with_report(replaced(report, "files", files));
    EXPECT_STOP(verify_export(bad), error_category::integrity, "export.report_mismatch");
  }
  {
    SCOPED_TRACE("metadato dichiarato non rappresentabile");
    json declared = json::array();
    declared.push(json::string("WCS"));
    with_report(replaced(report, "non_representable", declared));
    EXPECT_STOP(verify_export(bad), error_category::integrity, "export.report_mismatch");
  }
  {
    SCOPED_TRACE("motivo vuoto");
    with_report(replaced(report, "non_representable_reason", json::string("")));
    EXPECT_STOP(verify_export(bad), error_category::integrity, "export.report_mismatch");
  }
  {
    SCOPED_TRACE("formato del report ignoto");
    with_report(replaced(report, "format", json::string("astro.export_report.v2")));
    EXPECT_STOP(verify_export(bad), error_category::integrity, "export.report_mismatch");
  }
  {
    SCOPED_TRACE("controllo positivo del costruttore di bundle: report identico");
    with_report(report);
    EXPECT_EQ(verify_export(bad).files.size(), m.files.size());
  }
  EXPECT_EQ(st::read_text(out / "manifest.json"), manifest_text);
  {
    SCOPED_TRACE("slot che produrrebbe il nome riservato");
    const node_spec a = *store->get_node(store->get_slot("sa")->target_node_key);
    store->create_slot("export_report", a.key());
    const job_record job = store->start_job("export_report");
    store->commit_job(job.id, store->write_files({artifact_content{st::bytes_of("{}"), "json", "image", std::nullopt}}));
    EXPECT_STOP(store->export_slots(project.parent_path() / "r406_reserved", {"export_report"}), error_category::invalid_input,
                "export.slot_name");
  }
}

}  // namespace
