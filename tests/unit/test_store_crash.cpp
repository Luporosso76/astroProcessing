// I kill simulano la morte del processo (SIGKILL) in punti del protocollo; non provano la durabilita' contro
// perdita di corrente o guasti del dispositivo.
#include "store_test_support.hpp"

namespace {

using namespace store_test;

struct scenario {
  fs::path dir;
  std::uint64_t job_id = 0;
};

scenario prepare(const std::string& name) {
  scenario s;
  s.dir = fresh_dir(name);
  auto store = sqlite_dag_store::create(s.dir);
  store->apply_recipe(diamond(2).resolve(), {{"s", "a"}});
  s.job_id = 1;
  return s;
}

// Corpo eseguito nel figlio: apre il progetto con l'iniettore, avvia il job e produce un output pubblicato.
void commit_body(const scenario& s, const fault_injector& injector) {
  open_options options;
  options.fault = injector;
  auto store = sqlite_dag_store::open(s.dir, open_intent::read_write, options);
  const job_record job = store->start_job("s");
  if (job.id != s.job_id) throw std::runtime_error("identificativo di job inatteso");
  const auto bytes = bytes_of("crash test payload");
  const auto files = store->write_files({artifact_content{bytes, "bin", "image", std::nullopt}});
  store->commit_job(job.id, files);
}

TEST(StoreCrash, T_M2C_02_sigkill_at_every_step_of_the_commit_protocol_leaves_a_coherent_project) {
  const scenario dry = prepare("t02_dry");
  const std::vector<std::string> points = record_points([&](const fault_injector& f) { commit_body(dry, f); });
  const std::vector<std::string> expected{"artifact.create", "artifact.write", "artifact.fsync_file", "artifact.rename",
                                          "artifact.fsync_dir", "commit.begin", "commit.row", "commit.inserted", "commit.before_commit",
                                          "commit.after_commit"};
  ASSERT_EQ(points, expected);
  const std::string final_name = digest_of(bytes_of("crash test payload")).sha256 + ".bin";

  for (std::size_t i = 1; i <= points.size(); ++i) {
    SCOPED_TRACE("kill prima di " + points[i - 1]);
    const scenario s = prepare("t02");
    const child_outcome out = run_child([&](const fault_injector& f) { commit_body(s, f); }, static_cast<int>(i));
    ASSERT_TRUE(out.killed) << "il figlio non e' stato ucciso, exit " << out.exit_code;
    auto store = sqlite_dag_store::open(s.dir, open_intent::read_write);
    const bool committed = points[i - 1] == "commit.after_commit";
    const job_record job = *store->get_job(s.job_id);
    const std::string node_key = job.node_key;
    if (committed) {
      EXPECT_EQ(job.state, job_state::completed);
      EXPECT_EQ(store->read_slot("s").state, freshness::current);
      ASSERT_EQ(store->artifacts_for(node_key).size(), 1u);
      EXPECT_EQ(list_dir(s.dir / "artifacts"), std::vector<std::string>{final_name});
      EXPECT_EQ(digest_of(bytes_of(read_text(s.dir / "artifacts" / final_name))).sha256,
                store->artifacts_for(node_key)[0].sha256);
      EXPECT_TRUE(store->recovery().interrupted_jobs.empty());
    } else {
      EXPECT_EQ(job.state, job_state::interrupted);
      EXPECT_EQ(store->recovery().interrupted_jobs, std::vector<std::uint64_t>{s.job_id});
      EXPECT_TRUE(store->artifacts_for(node_key).empty());
      EXPECT_EQ(store->read_slot("s").state, freshness::missing);
      EXPECT_TRUE(list_dir(s.dir / "artifacts").empty());
      // i = 2..4: temporaneo creato e non ancora rinominato; i >= 5: file finale senza riga
      EXPECT_EQ(store->recovery().removed_temporaries.size(), i >= 2 && i <= 4 ? 1u : 0u);
      EXPECT_EQ(store->recovery().removed_orphans.size(), i >= 5 ? 1u : 0u);
    }
  }
}

// Corpo della migrazione eseguito nel figlio, con un iniettore.
void migrate_body(const fs::path& dir, const open_options& base, const fault_injector& f) {
  open_options options = base;
  options.fault = f;
  sqlite_dag_store::open(dir, open_intent::read_write, options);
}

void expect_version_one_then_retry(const fs::path& dir, const open_options& base, const std::string& logical) {
  { auto store = sqlite_dag_store::open(dir, open_intent::read_write); }
  EXPECT_EQ(dump_logical(dir), logical);
  {
    raw_db db(dir, true);
    EXPECT_EQ(db.query("PRAGMA user_version"), "1|\n");
    EXPECT_EQ(db.query("SELECT name FROM pragma_table_info('settings') WHERE name='note'"), "");
  }
  { auto store = sqlite_dag_store::open(dir, open_intent::read_write, base); }
  raw_db db(dir, true);
  EXPECT_EQ(db.query("PRAGMA user_version"), "2|\n");
  EXPECT_EQ(db.query("SELECT name FROM pragma_table_info('settings') WHERE name='more'"), "more|\n");
}

TEST(StoreCrash, T_M2C_06_sigkill_inside_the_migration_transaction_and_in_the_backup_leaves_version_one) {
  const open_options base{2, {schema_migration{1, {"ALTER TABLE settings ADD COLUMN note TEXT", "ALTER TABLE settings ADD COLUMN more TEXT"}}}, {}};
  const fs::path dry = fresh_dir("t06k_dry");
  { sqlite_dag_store::create(dry)->apply_recipe(diamond(2).resolve(), {{"s", "a"}}); }
  const auto points = record_points([&](const fault_injector& f) { migrate_body(dry, base, f); });
  const auto publish = std::find(points.begin(), points.end(), "backup.before_publish");
  ASSERT_NE(publish, points.end());
  std::size_t statements = 0;
  for (std::size_t i = 1; i <= points.size(); ++i) {
    const bool in_transaction = points[i - 1] == "migration.statement";
    if (in_transaction) {
      ++statements;
      ASSERT_GT(static_cast<std::ptrdiff_t>(i - 1), publish - points.begin()) << "il punto non e' dopo il backup";
    }
    SCOPED_TRACE("kill al punto " + points[i - 1] + (in_transaction ? " (transazione di migrazione)" : " (backup)"));
    const fs::path dir = fresh_dir("t06k");
    { sqlite_dag_store::create(dir)->apply_recipe(diamond(2).resolve(), {{"s", "a"}}); }
    const std::string logical = dump_logical(dir);
    const child_outcome out = run_child([&](const fault_injector& f) { migrate_body(dir, base, f); }, static_cast<int>(i));
    ASSERT_TRUE(out.killed);
    expect_version_one_then_retry(dir, base, logical);
  }
  EXPECT_EQ(statements, 2u);
}

}  // namespace
