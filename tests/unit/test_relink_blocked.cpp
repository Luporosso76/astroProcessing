// U9 (M3.a), F-IO-08: nodi bloccati con motivo (RL-03), sblocco senza ricalcolo (RL-07), storia, undo, redo e riapertura
// (RL-06). Casi: T-M3A-09 a, b, c, d. Contratto: docs/contracts/F-IO-08-ricollegamento.md §4, §6, §10.
#include "astro/json.hpp"
#include "relink_support.hpp"

namespace {

using namespace rl;

constexpr std::size_t default_work = 65536;

const std::vector<std::string> recipe_tables{"nodes",    "node_edges",       "slots",           "snapshots", "snapshot_entries",
                                             "history_cursor", "recovery_reports", "artifacts", "jobs"};

struct built {
  world w;
  chain c;
  std::vector<input_record> ids;  // light_0, light_1, dark, flat, dark_flat
  std::unique_ptr<sqlite_dag_store> store;
};

// Due import (imp0, imp1) e due calibrazioni (cal0 dipende da light_0, cal1 da light_1; i master sono in comune), tutti
// `current`.
built build_chain(const std::string& name) {
  built b;
  b.w = make_world(name);
  b.c = make_chain(b.w.base / "orig", 2);
  b.store = sqlite_dag_store::create(b.w.proj);
  b.ids = setup_chain_project(*b.store, b.c, 2);
  runner_env env;
  const workflow wf = make_workflow(*b.store, b.c, env, 2, [&](std::size_t k) { return b.c.lights[k].path.string(); });
  const workflow_result r = run_workflow(*b.store, wf);
  EXPECT_EQ(r.exit_code, 0);
  for (const char* slot : {"imp0", "imp1", "cal0", "cal1"}) EXPECT_EQ(b.store->read_slot(slot).state, freshness::current) << slot;
  return b;
}

const blocked_edge* edge_with(const slot_block& b, const std::string& sha) {
  for (const auto& e : b.edges) {
    if (e.sha256 == sha) return &e;
  }
  return nullptr;
}

void expect_blocked_by(const std::vector<slot_block>& blocks, const std::string& slot, const input_record& rec, const orig& o,
                       const std::string& reason) {
  const slot_block* b = block_of(blocks, slot);
  ASSERT_NE(b, nullptr) << slot;
  EXPECT_TRUE(b->blocked) << slot;
  const blocked_edge* e = edge_with(*b, o.sha);
  ASSERT_NE(e, nullptr) << slot;
  EXPECT_FALSE(e->available) << slot;
  ASSERT_EQ(e->blockers.size(), 1u) << slot;
  EXPECT_EQ(e->blockers.front().input_id, rec.id) << slot;
  EXPECT_EQ(e->blockers.front().path, o.path.string()) << slot;
  EXPECT_EQ(e->blockers.front().reason, reason) << slot;
}

// Stop del job al confine (ADR-05): la generazione dello slot sale di 1, i file primari del nodo restano gli stessi e nasce
// un solo artefatto nuovo, di ruolo `provenance`, che dichiara `failed` con lo stop atteso. Lo slot continua a puntare
// all'artefatto primario precedente.
void expect_boundary_stop_recorded(const built& b, const std::string& slot, std::uint64_t generation_before,
                                   const std::vector<std::uint64_t>& ids_before, std::optional<std::uint64_t> artifact_before,
                                   const std::string& code) {
  SCOPED_TRACE(slot);
  const std::string node_key = b.store->get_slot(slot)->target_node_key;
  EXPECT_EQ(b.store->get_slot(slot)->current_generation, generation_before + 1) << "un job fermo registra l'arresto";
  std::vector<artifact_record> added;
  for (const artifact_record& a : b.store->artifacts_for(node_key)) {
    const bool known = std::find(ids_before.begin(), ids_before.end(), a.id) != ids_before.end();
    if (known) continue;
    added.push_back(a);
  }
  EXPECT_EQ(b.store->artifacts_for(node_key).size(), ids_before.size() + 1) << "un solo artefatto nuovo";
  ASSERT_EQ(added.size(), 1u);
  EXPECT_EQ(added.front().output_role, "provenance");
  EXPECT_EQ(added.front().generation, generation_before + 1);
  const json doc = parse_json(st::read_text(b.store->artifact_path(added.front().rel_path)));
  const json* state = doc.find("state");
  ASSERT_NE(state, nullptr);
  EXPECT_EQ(state->as_string(), "failed");
  const json* stop = doc.find("stop");
  ASSERT_NE(stop, nullptr);
  const json* stop_code = stop->find("code");
  ASSERT_NE(stop_code, nullptr);
  EXPECT_EQ(stop_code->as_string(), code);
  EXPECT_EQ(b.store->read_slot(slot).artifact->id, artifact_before) << "nessun primario pubblicato";
  EXPECT_EQ(b.store->read_slot(slot).state, freshness::current);
}

std::vector<std::uint64_t> artifact_ids(const built& b, const std::string& slot) {
  std::vector<std::uint64_t> ids;
  for (const artifact_record& a : b.store->artifacts_for(b.store->get_slot(slot)->target_node_key)) ids.push_back(a.id);
  return ids;
}

// T-M3A-09 a: A spostato; gemello con copia byte-identica e poi con un byte diverso (RL-03, limite di RL-03).
TEST(RelinkBlocked, T_M3A_09a_dependents_blocked_with_reason_and_execution_stops_at_boundary) {
  built b = build_chain("09a");
  const orig& a = b.c.lights[0];
  const std::string slots_before = slot_rows(b.w.proj, true);
  const std::string tables_before = dump_tables(b.w.proj, recipe_tables, true);
  const std::vector<std::uint64_t> artifacts_imp0 = artifact_ids(b, "imp0");
  const std::optional<std::uint64_t> artifact_imp0 = b.store->read_slot("imp0").artifact->id;
  const std::uint64_t generation_imp0 = b.store->get_slot("imp0")->current_generation;

  fs::create_directories(b.w.base / "spostato");
  fs::rename(a.path, b.w.base / "spostato" / a.path.filename());

  const auto blocks = b.store->blocked_slots(b.store->input_status());
  EXPECT_EQ(blocked_names(blocks), (std::set<std::string>{"cal0", "imp0"}));
  expect_blocked_by(blocks, "imp0", b.ids[0], a, "input.missing");
  expect_blocked_by(blocks, "cal0", b.ids[0], a, "input.missing");
  ASSERT_NE(block_of(blocks, "imp1"), nullptr);
  ASSERT_NE(block_of(blocks, "cal1"), nullptr);
  EXPECT_FALSE(block_of(blocks, "imp1")->blocked);
  EXPECT_FALSE(block_of(blocks, "cal1")->blocked);
  // Il blocco riguarda l'esecuzione, non i risultati: stato, artefatto e generazione di ogni slot non cambiano.
  EXPECT_EQ(slot_rows(b.w.proj, true), slots_before);
  EXPECT_EQ(dump_tables(b.w.proj, recipe_tables, true), tables_before);
  EXPECT_EQ(b.store->read_slot("imp0").state, freshness::current);

  // Esecuzione dello slot di import con il vecchio percorso: stop al confine, nessun percorso alternativo.
  {
    runner_env env;
    const import_request req = import_request_of(a);
    std::vector<std::string> opened;
    probe_scope probe([&](std::string_view point, std::string_view detail) {
      if (point == "input.open") opened.emplace_back(detail);
    });
    const job_outcome out = run_import_job(*b.store, "imp0", req, env.pool, env.cancel);
    EXPECT_NE(out.exit_code, 0);
    ASSERT_TRUE(out.stop.has_value());
    EXPECT_EQ(out.stop->category(), error_category::io);
    EXPECT_EQ(out.stop->code(), "input.missing");
    const auto job = b.store->get_job(out.job.id);
    ASSERT_TRUE(job.has_value());
    EXPECT_EQ(job->state, job_state::failed);
    EXPECT_EQ(job->stop_category, std::optional<error_category>(error_category::io));
    EXPECT_EQ(job->stop_code, "input.missing");
    ASSERT_TRUE(out.provenance.stop.has_value());
    EXPECT_EQ(out.provenance.stop->code(), "input.missing");
    EXPECT_EQ(opened, (std::vector<std::string>{a.path.string()})) << "nessun job su un altro percorso";
    expect_boundary_stop_recorded(b, "imp0", generation_imp0, artifacts_imp0, artifact_imp0, "input.missing");
  }
}

TEST(RelinkBlocked, T_M3A_09a_twin_identical_copy_is_warning_changed_bytes_stop) {
  built b = build_chain("09atwin");
  const orig& a = b.c.lights[0];
  const std::string recorded = primary_artifact_row(b.w.proj, "imp0", true);
  ASSERT_FALSE(recorded.empty());

  // Copia byte-identica con inode nuovo: bloccato come `input.modified`; il job completa con lo stesso output.
  replace_with_new_inode(a.path, a.content);
  {
    const auto blocks = b.store->blocked_slots(b.store->input_status());
    EXPECT_EQ(blocked_names(blocks), (std::set<std::string>{"cal0", "imp0"}));
    expect_blocked_by(blocks, "imp0", b.ids[0], a, "input.modified");
    expect_blocked_by(blocks, "cal0", b.ids[0], a, "input.modified");
    runner_env env;
    const job_outcome out = run_import_job(*b.store, "imp0", import_request_of(a), env.pool, env.cancel);
    EXPECT_EQ(out.exit_code, 0) << (out.stop ? out.stop->what() : "");
    EXPECT_EQ(out.job.state, job_state::completed);
    EXPECT_EQ(primary_artifact_row(b.w.proj, "imp0", true), recorded) << "output di digest e nome uguali a quelli registrati";
  }

  // Stessa dimensione, un byte diverso: bloccato come `input.modified`; il job si ferma nel confine.
  bytes flipped = a.content;
  flipped[3003] ^= 0x01;
  replace_with_new_inode(a.path, flipped);
  {
    const auto blocks = b.store->blocked_slots(b.store->input_status());
    expect_blocked_by(blocks, "imp0", b.ids[0], a, "input.modified");
    EXPECT_EQ(blocked_names(blocks), (std::set<std::string>{"cal0", "imp0"}));
    const std::vector<std::uint64_t> artifacts_before = artifact_ids(b, "imp0");
    const std::optional<std::uint64_t> artifact_before = b.store->read_slot("imp0").artifact->id;
    const std::uint64_t generation_before = b.store->get_slot("imp0")->current_generation;
    runner_env env;
    const job_outcome out = run_import_job(*b.store, "imp0", import_request_of(a), env.pool, env.cancel);
    EXPECT_NE(out.exit_code, 0);
    ASSERT_TRUE(out.stop.has_value());
    EXPECT_EQ(out.stop->category(), error_category::integrity);
    EXPECT_EQ(out.stop->code(), "input.digest_mismatch");
    expect_boundary_stop_recorded(b, "imp0", generation_before, artifacts_before, artifact_before, "input.digest_mismatch");
  }
}

std::filesystem::path move_a(built& b) {
  const fs::path moved = b.w.base / "spostato" / b.c.lights[0].path.filename();
  fs::create_directories(moved.parent_path());
  fs::rename(b.c.lights[0].path, moved);
  return moved;
}

input_record relink(sqlite_dag_store& store, std::uint64_t id, const fs::path& to) {
  std::vector<std::byte> work(default_work);
  return store.relink_input(id, to.string(), std::span<std::byte>(work), no_cancel());
}

// T-M3A-09 b: ricollegamento di A con digest uguale: nessuno slot bloccato, nessun job, nessuna scrittura sulla ricetta;
// resume senza job; ricalcolo esplicito dal nuovo percorso con digest e nome uguali (RL-06, RL-07).
TEST(RelinkBlocked, T_M3A_09b_relink_unblocks_without_recompute) {
  built b = build_chain("09b");
  const std::string recorded_cal0 = primary_artifact_row(b.w.proj, "cal0", true);
  ASSERT_FALSE(recorded_cal0.empty());
  const fs::path moved = move_a(b);
  ASSERT_EQ(blocked_names(b.store->blocked_slots(b.store->input_status())), (std::set<std::string>{"cal0", "imp0"}));
  const std::string tables_before = dump_tables(b.w.proj, recipe_tables, true);
  const std::size_t jobs_before = job_count(b.w.proj, true);

  const input_record done = relink(*b.store, b.ids[0].id, moved);
  EXPECT_EQ(done.path, moved.string());

  const auto states = b.store->input_status();
  for (const auto& s : states) EXPECT_EQ(s.status, "present") << s.input_id;
  const auto blocks = b.store->blocked_slots(states);
  EXPECT_TRUE(blocked_names(blocks).empty());
  EXPECT_EQ(dump_tables(b.w.proj, recipe_tables, true), tables_before) << "ricetta, slot, snapshot, job e artefatti invariati";
  EXPECT_EQ(job_count(b.w.proj, true), jobs_before);

  // Resume con i percorsi correnti: fasi sicure riusate, nessun job.
  {
    runner_env env;
    const auto current_path = [&](std::size_t k) {
      for (const auto& in : b.store->inputs()) {
        if (in.id == b.ids[k].id) return in.path;
      }
      throw std::runtime_error("input assente");
    };
    const workflow wf = make_workflow(*b.store, b.c, env, 2, current_path);
    const workflow_result r = resume_workflow(*b.store, wf);
    EXPECT_EQ(r.exit_code, 0);
    ASSERT_EQ(r.steps.size(), 4u);
    for (const auto& step : r.steps) EXPECT_EQ(step.action, step_action::reused) << step.slot_name;
    EXPECT_EQ(job_count(b.w.proj, true), jobs_before);
    EXPECT_EQ(dump_tables(b.w.proj, recipe_tables, true), tables_before);
  }

  // Artefatto primario della calibrazione rimosso, riapertura, ricalcolo esplicito dal nuovo percorso.
  const std::string rel = recorded_cal0.substr(0, recorded_cal0.find('|'));
  const fs::path artifact_file = b.store->artifact_path(rel);
  ASSERT_TRUE(fs::exists(artifact_file));
  b.store.reset();
  fs::remove(artifact_file);
  b.store = sqlite_dag_store::open(b.w.proj, open_intent::read_write);
  calibration_recipe recipe = b.c.recipes[0];
  recipe.light.path = moved.string();
  calibration_job_options opt;
  opt.worker_count = 2;
  opt.tile_rows = 4;
  runner_env env;
  const job_outcome out = run_calibration_job(*b.store, "cal0", recipe, env.pool, opt, env.cancel);
  EXPECT_EQ(out.exit_code, 0) << (out.stop ? out.stop->what() : "");
  EXPECT_EQ(primary_artifact_row(b.w.proj, "cal0", true), recorded_cal0) << "digest uguale a quello registrato e nome riusato";
}

// T-M3A-09 c: chiusura e riapertura dopo il ricollegamento (RL-06).
TEST(RelinkBlocked, T_M3A_09c_reopen_keeps_resolved_reference) {
  built b = build_chain("09c");
  const fs::path moved = move_a(b);
  const input_record done = relink(*b.store, b.ids[0].id, moved);
  const std::size_t reports_before = count_rows(b.w.proj, "recovery_reports", true);
  const std::string logical_before = dump_all(b.w.proj, true);
  b.store.reset();
  const std::string relinks_before = query(b.w.proj, "SELECT * FROM input_relinks ORDER BY input_id, revision");
  ASSERT_FALSE(relinks_before.empty());

  auto store = sqlite_dag_store::open(b.w.proj, open_intent::read_write);
  const auto inputs = store->inputs();
  ASSERT_EQ(inputs.size(), 5u);
  EXPECT_EQ(inputs[0].id, b.ids[0].id);
  EXPECT_EQ(inputs[0].path, moved.string());
  EXPECT_EQ(inputs[0].dev, done.dev);
  EXPECT_EQ(inputs[0].ino, done.ino);
  EXPECT_EQ(inputs[0].ino, stat_of(moved).ino());
  EXPECT_EQ(inputs[0].format, "fits");
  EXPECT_EQ(query(b.w.proj, "SELECT * FROM input_relinks ORDER BY input_id, revision", true), relinks_before);
  const auto states = store->input_status();
  for (const auto& s : states) EXPECT_EQ(s.status, "present") << s.input_id;
  EXPECT_TRUE(blocked_names(store->blocked_slots(states)).empty());
  EXPECT_EQ(count_rows(b.w.proj, "recovery_reports", true), reports_before) << "nessuna riga nuova in recovery_reports";
  store.reset();
  EXPECT_EQ(dump_all(b.w.proj), logical_before) << "stato logico (tutte le tabelle) uguale a prima della chiusura";
}

recipe_graph diamond_file(const content_digest& d, double b_param) {
  recipe_graph g;
  g.add("a", st::entry("op.a", 1, {recipe_input{"file", d}}));
  g.add("b", st::entry("op.b", b_param, {recipe_input{"in", std::string("a")}}));
  g.add("c", st::entry("op.c", 3, {recipe_input{"in", std::string("a")}}));
  g.add("d", st::entry("op.d", 4, {recipe_input{"x", std::string("b")}, recipe_input{"y", std::string("c")}}));
  return g;
}

struct history_project {
  world w;
  orig f;
  std::uint64_t id = 0;
  std::unique_ptr<sqlite_dag_store> store;
  fs::path moved;
};

history_project make_history_project(const std::string& name) {
  history_project h;
  h.w = make_world(name);
  h.f = make_orig(h.w.base / "orig" / "f.fits", light_bytes(0));
  h.store = sqlite_dag_store::create(h.w.proj);
  h.id = register_input(*h.store, h.f).id;
  const content_digest d{h.f.sha, h.f.size};
  h.store->apply_recipe(diamond_file(d, 2).resolve(), st::diamond_slots());
  h.store->apply_recipe(diamond_file(d, 7).resolve(), st::diamond_slots());  // cambio di parametro: snapshot nuovo
  h.moved = h.w.base / "nuovo" / "f.fits";
  fs::create_directories(h.moved.parent_path());
  fs::rename(h.f.path, h.moved);
  return h;
}

std::string positions(const history_project& h) { return dump_tables(h.w.proj, {"inputs", "input_relinks"}, true); }

// T-M3A-09 d: cambio di parametro, ricollegamento, undo, redo; e ricollegamento dopo un undo con redo disponibile (RL-06).
TEST(RelinkBlocked, T_M3A_09d_undo_redo_do_not_touch_positions_and_relink_is_not_a_snapshot) {
  history_project h = make_history_project("09d");
  const std::size_t snapshots = count_rows(h.w.proj, "snapshots", true);
  ASSERT_GE(snapshots, 2u);
  const history_state history_before = h.store->history();
  const std::string cursor_before = dump_tables(h.w.proj, {"history_cursor", "snapshots", "snapshot_entries"}, true);
  EXPECT_EQ(blocked_names(h.store->blocked_slots(h.store->input_status())), (std::set<std::string>{"sa", "sb", "sc", "sd"}));

  relink(*h.store, h.id, h.moved);
  EXPECT_EQ(count_rows(h.w.proj, "snapshots", true), snapshots) << "il ricollegamento non crea snapshot";
  EXPECT_EQ(dump_tables(h.w.proj, {"history_cursor", "snapshots", "snapshot_entries"}, true), cursor_before);
  EXPECT_EQ(h.store->history().current_snapshot_id, history_before.current_snapshot_id);
  EXPECT_EQ(h.store->history().redo_snapshot_id, history_before.redo_snapshot_id);

  const std::string positions_after_relink = positions(h);
  const std::string shape = blocks_shape(h.store->blocked_slots(h.store->input_status()));
  EXPECT_TRUE(blocked_names(h.store->blocked_slots(h.store->input_status())).empty());
  h.store->undo();
  EXPECT_EQ(positions(h), positions_after_relink) << "undo non ripristina la posizione precedente";
  EXPECT_EQ(blocks_shape(h.store->blocked_slots(h.store->input_status())), shape);
  h.store->redo();
  EXPECT_EQ(positions(h), positions_after_relink) << "redo non cambia la posizione";
  EXPECT_EQ(blocks_shape(h.store->blocked_slots(h.store->input_status())), shape);
}

TEST(RelinkBlocked, T_M3A_09d_relink_after_undo_keeps_redo_available) {
  history_project h = make_history_project("09dredo");
  h.store->undo();
  const history_state before = h.store->history();
  ASSERT_TRUE(before.redo_snapshot_id.has_value()) << "dopo un undo il redo è disponibile";
  const std::size_t snapshots = count_rows(h.w.proj, "snapshots", true);
  const std::string cursor_before = dump_tables(h.w.proj, {"history_cursor", "snapshots", "snapshot_entries"}, true);

  relink(*h.store, h.id, h.moved);
  const history_state after = h.store->history();
  EXPECT_EQ(after.current_snapshot_id, before.current_snapshot_id);
  EXPECT_EQ(after.redo_snapshot_id, before.redo_snapshot_id) << "il ricollegamento non azzera il redo";
  EXPECT_EQ(count_rows(h.w.proj, "snapshots", true), snapshots);
  EXPECT_EQ(dump_tables(h.w.proj, {"history_cursor", "snapshots", "snapshot_entries"}, true), cursor_before);

  const std::string positions_after_relink = positions(h);
  const std::string shape = blocks_shape(h.store->blocked_slots(h.store->input_status()));
  EXPECT_NO_THROW(h.store->redo());
  EXPECT_EQ(positions(h), positions_after_relink);
  EXPECT_EQ(blocks_shape(h.store->blocked_slots(h.store->input_status())), shape);
  EXPECT_TRUE(blocked_names(h.store->blocked_slots(h.store->input_status())).empty());
}

}  // namespace
