#include "store_test_support.hpp"

namespace {

using namespace store_test;

std::unique_ptr<sqlite_dag_store> make_project(const std::string& name, fs::path* dir_out = nullptr) {
  const fs::path dir = fresh_dir(name);
  if (dir_out != nullptr) *dir_out = dir;
  return sqlite_dag_store::create(dir);
}

std::string slot_state(const sqlite_dag_store& store, const std::vector<std::string>& slots) {
  std::string out;
  for (const auto& s : slots) {
    const slot_record r = *store.get_slot(s);
    out += s + ":" + r.target_node_key.substr(0, 8) + ":" +
           (r.artifact_id ? std::to_string(*r.artifact_id) : std::string("none")) + ":" +
           std::to_string(static_cast<int>(store.read_slot(s).state)) + "\n";
  }
  return out;
}

const std::vector<std::string> all_slots{"sa", "sb", "sc", "sd"};

TEST(History, T_M2C_14_change_undo_redo_restore_logical_state_and_relink_artifacts) {
  fs::path dir;
  std::string state1, state2;
  {
    auto store = make_project("t14", &dir);
    store->apply_recipe(diamond(2).resolve(), diamond_slots());
    for (const auto& s : all_slots) run_and_commit(*store, s, "v1-" + s);
    state1 = slot_state(*store, all_slots);
    store->apply_recipe(diamond(9).resolve(), diamond_slots());
    run_and_commit(*store, "sb", "v2-sb");
    run_and_commit(*store, "sd", "v2-sd");
    state2 = slot_state(*store, all_slots);
    EXPECT_NE(state1, state2);
  }
  const std::string nodes_before = nodes_digest(dir);
  {
    auto store = sqlite_dag_store::open(dir, open_intent::read_write);
    const std::uint64_t gen = store->get_slot("sa")->current_generation;
    store->undo();
    EXPECT_EQ(slot_state(*store, all_slots), state1);
    EXPECT_EQ(store->get_slot("sa")->current_generation, gen + 1);
    EXPECT_EQ(store->read_slot("sb").artifact->sha256, digest_of(bytes_of("v1-sb")).sha256);
    store->redo();
    EXPECT_EQ(slot_state(*store, all_slots), state2);
    store->undo();
    EXPECT_EQ(slot_state(*store, all_slots), state1);
    store->redo();
    EXPECT_EQ(store->get_slot("sa")->current_generation, gen + 4);
  }
  EXPECT_EQ(nodes_digest(dir), nodes_before);
}

TEST(History, T_M2C_15_undo_with_a_running_job_makes_its_publication_stale) {
  auto store = make_project("t15");
  const auto v1 = diamond(2).resolve();
  const auto v2 = diamond(9).resolve();
  store->apply_recipe(v1, {{"s", "b"}});
  const artifact_record old_artifact = run_and_commit(*store, "s", "old");
  store->apply_recipe(v2, {{"s", "b"}});
  const job_record job = store->start_job("s");
  EXPECT_EQ(job.node_key, keys_of(v2).at("b"));
  const std::uint64_t gen_at_start = store->get_slot("s")->current_generation;
  store->undo();
  EXPECT_GT(store->get_slot("s")->current_generation, gen_at_start);
  const auto bytes = bytes_of("late result");
  const auto files = store->write_files({artifact_content{bytes, "bin", "image", std::nullopt}});
  EXPECT_STOP(store->commit_job(job.id, files), error_category::stale_generation, "dag.stale_publish");
  EXPECT_EQ(store->get_job(job.id)->state, job_state::failed);
  EXPECT_EQ(store->read_slot("s").artifact->id, old_artifact.id);
  EXPECT_EQ(store->read_slot("s").state, freshness::current);
  EXPECT_EQ(store->artifacts_for(keys_of(v2).at("b")).size(), 1u);  // in cache sotto la sua chiave
}

TEST(History, T_M2C_16_undo_with_absent_artifact_leaves_slot_missing_and_rebuild_reuses_the_name) {
  fs::path dir;
  auto store = make_project("t16", &dir);
  const auto v1 = diamond(2).resolve();
  store->apply_recipe(v1, {{"sa", "a"}, {"sb", "b"}});
  const artifact_record ca = run_and_commit(*store, "sa", "v1-sa");
  const artifact_record cb = run_and_commit(*store, "sb", "v1-sb");
  store->apply_recipe(diamond(9).resolve(), {{"sa", "a"}, {"sb", "b"}});
  store->set_artifact_present(cb.id, false);
  fs::remove(dir / "artifacts" / cb.rel_path);
  store->undo();
  EXPECT_EQ(store->read_slot("sb").state, freshness::missing);
  EXPECT_FALSE(store->get_slot("sb")->artifact_id.has_value());
  EXPECT_EQ(store->get_slot("sa")->artifact_id, ca.id);
  EXPECT_EQ(store->read_slot("sa").state, freshness::current);
  EXPECT_FALSE(store->get_job(3).has_value());  // nessun job nuovo: il nodo indipendente non si ricalcola
  const artifact_record rebuilt = run_and_commit(*store, "sb", "v1-sb");
  EXPECT_EQ(rebuilt.rel_path, cb.rel_path);
  EXPECT_EQ(rebuilt.sha256, cb.sha256);
  EXPECT_EQ(store->read_slot("sb").state, freshness::current);
  store->set_artifact_present(rebuilt.id, false);
  const artifact_record again = run_and_commit(*store, "sb", "v1-sb");  // il file esiste ancora: nome riusato
  EXPECT_EQ(again.rel_path, cb.rel_path);
  EXPECT_EQ(list_dir(dir / "artifacts").size(), 2u);
}

TEST(History, T_M2C_17_undo_redo_beyond_limits_dangling_node_and_read_only_stop) {
  fs::path dir;
  {
    auto store = make_project("t17", &dir);
    EXPECT_STOP(store->undo(), error_category::invalid_input, "history.no_undo");
    EXPECT_STOP(store->redo(), error_category::invalid_input, "history.no_redo");
    store->apply_recipe(diamond(2).resolve(), diamond_slots());
    EXPECT_STOP(store->undo(), error_category::invalid_input, "history.no_undo");
    store->apply_recipe(diamond(9).resolve(), diamond_slots());
    store->undo();
    EXPECT_STOP(store->undo(), error_category::invalid_input, "history.no_undo");
    store->redo();
    EXPECT_STOP(store->redo(), error_category::invalid_input, "history.no_redo");
    store->undo();
  }
  {
    auto store = sqlite_dag_store::open(dir, open_intent::read_only);
    EXPECT_STOP(store->redo(), error_category::schema, "store.read_only");
    EXPECT_STOP(store->undo(), error_category::schema, "store.read_only");
  }
  {
    raw_db db(dir);
    ASSERT_EQ(db.exec("DROP TRIGGER nodes_immutable_delete"), SQLITE_OK);
    ASSERT_EQ(db.exec("PRAGMA foreign_keys=OFF"), SQLITE_OK);
    ASSERT_EQ(db.exec("DELETE FROM nodes WHERE node_key IN (SELECT node_key FROM snapshot_entries WHERE snapshot_id=2)"), SQLITE_OK);
  }
  auto store = sqlite_dag_store::open(dir, open_intent::read_write);
  const std::string before = slot_state(*store, all_slots);
  EXPECT_STOP(store->redo(), error_category::integrity, "history.dangling_node");
  EXPECT_EQ(slot_state(*store, all_slots), before);
}

TEST(History, T_M2C_18_new_snapshot_after_undo_clears_redo) {
  auto store = make_project("t18");
  store->apply_recipe(diamond(2).resolve(), diamond_slots());
  const auto first = *store->history().current_snapshot_id;
  store->apply_recipe(diamond(9).resolve(), diamond_slots());
  store->undo();
  EXPECT_TRUE(store->history().redo_snapshot_id.has_value());
  store->apply_recipe(diamond(11).resolve(), diamond_slots());
  const history_state h = store->history();
  EXPECT_FALSE(h.redo_snapshot_id.has_value());
  EXPECT_EQ(store->snapshot_parent(*h.current_snapshot_id), first);
  EXPECT_STOP(store->redo(), error_category::invalid_input, "history.no_redo");
  store->undo();
  EXPECT_EQ(store->history().current_snapshot_id, first);
}

TEST(History, apply_recipe_without_changes_creates_no_snapshot) {
  auto store = make_project("tsnap");
  store->apply_recipe(diamond(2).resolve(), diamond_slots());
  const auto before = store->history();
  EXPECT_TRUE(store->apply_recipe(diamond(2).resolve(), diamond_slots()).empty());
  EXPECT_EQ(store->history().current_snapshot_id, before.current_snapshot_id);
}

}  // namespace
