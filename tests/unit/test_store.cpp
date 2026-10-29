#include <fcntl.h>
#include <sys/stat.h>

#include "store_test_support.hpp"

namespace {

using namespace store_test;

std::unique_ptr<sqlite_dag_store> make_project(const std::string& name, fs::path* dir_out = nullptr) {
  const fs::path dir = fresh_dir(name);
  if (dir_out != nullptr) *dir_out = dir;
  return sqlite_dag_store::create(dir);
}

TEST(Store, T_M2C_01_create_save_reopen_keeps_the_same_logical_state) {
  fs::path dir;
  std::map<std::string, std::string> canonical;
  {
    auto store = make_project("t01", &dir);
    store->add_input(input_record{0, "/data/light.fits", 2880, std::string(64, 'a'), "light"});
    const auto v1 = diamond(2).resolve();
    store->apply_recipe(v1, diamond_slots());
    for (const auto& n : v1) canonical[n.spec.key()] = n.spec.canonical();
    node_spec seeded("op.seed", 3, 2, params_of(1.5), {}, sample_type::f64, "cpu_scalar", 18446744073709551615ull);
    store->put_node(seeded);
    canonical[seeded.key()] = seeded.canonical();
    for (const auto& [slot, local] : diamond_slots()) run_and_commit(*store, slot, "payload-" + slot + local);
    const auto v2 = diamond(9).resolve();
    store->apply_recipe(v2, diamond_slots());
    for (const auto& n : v2) canonical[n.spec.key()] = n.spec.canonical();
  }
  const std::string before = dump_logical(dir);
  {
    auto store = sqlite_dag_store::open(dir, open_intent::read_write);
    EXPECT_EQ(store->mode(), open_mode::read_write);
    EXPECT_TRUE(store->recovery().interrupted_jobs.empty());
    EXPECT_TRUE(store->recovery().artifacts_marked_absent.empty());
    for (const auto& [key, text] : canonical) {
      const auto node = store->get_node(key);
      ASSERT_TRUE(node.has_value()) << key;
      EXPECT_EQ(node->canonical(), text);
      EXPECT_EQ(node->key(), key);
    }
    const node_spec d = *store->get_node(keys_of(diamond(2).resolve()).at("d"));
    ASSERT_EQ(d.inputs().size(), 2u);
    EXPECT_EQ(d.inputs()[0].input_name(), "x");
    EXPECT_EQ(d.inputs()[1].input_name(), "y");
    EXPECT_EQ(store->inputs().at(0).sha256, std::string(64, 'a'));
    EXPECT_EQ(store->snapshot_entries(*store->history().current_snapshot_id).size(), 4u);
  }
  EXPECT_EQ(dump_logical(dir), before);
}

TEST(Store, T_M2C_03_recovery_removes_orphans_only_inside_artifacts) {
  fs::path dir;
  std::string kept_name;
  {
    auto store = make_project("t03", &dir);
    const auto nodes = diamond(2).resolve();
    store->apply_recipe(nodes, {{"s", "a"}});
    kept_name = run_and_commit(*store, "s", "kept").rel_path;
  }
  const std::string orphan_name = std::string(64, 'b') + ".bin";
  write_text(dir / "artifacts" / ".tmp-0123", "partial");
  write_text(dir / "artifacts" / orphan_name, "orphan");
  write_text(dir / "outside.txt", "keep me");
  fs::create_directories(dir.parent_path() / "t03_sibling");
  write_text(dir.parent_path() / "t03_sibling" / "x", "keep me too");
  {
    auto store = sqlite_dag_store::open(dir, open_intent::read_write);
    EXPECT_EQ(store->recovery().removed_temporaries, std::vector<std::string>{".tmp-0123"});
    EXPECT_EQ(store->recovery().removed_orphans, std::vector<std::string>{orphan_name});
    EXPECT_EQ(list_dir(dir / "artifacts"), std::vector<std::string>{kept_name});
    EXPECT_EQ(store->read_slot("s").state, freshness::current);
  }
  EXPECT_EQ(read_text(dir / "outside.txt"), "keep me");
  EXPECT_EQ(read_text(dir.parent_path() / "t03_sibling" / "x"), "keep me too");
}

TEST(Store, T_M2C_03_recovery_marks_rows_whose_file_is_missing_or_has_another_size_absent) {
  fs::path dir;
  artifact_record gone, shrunk, fine;
  {
    auto store = make_project("t03b", &dir);
    store->apply_recipe(diamond(2).resolve(), {{"s1", "a"}, {"s2", "b"}, {"s3", "c"}});
    gone = run_and_commit(*store, "s1", "will vanish");
    shrunk = run_and_commit(*store, "s2", "will shrink");
    fine = run_and_commit(*store, "s3", "intact");
  }
  fs::remove(dir / "artifacts" / gone.rel_path);
  write_text(dir / "artifacts" / shrunk.rel_path, "x");
  auto store = sqlite_dag_store::open(dir, open_intent::read_write);
  EXPECT_EQ(store->recovery().artifacts_marked_absent, (std::vector<std::uint64_t>{gone.id, shrunk.id}));
  EXPECT_EQ(store->read_slot("s1").state, freshness::missing);
  EXPECT_EQ(store->read_slot("s2").state, freshness::missing);
  EXPECT_EQ(store->read_slot("s3").state, freshness::current);
  EXPECT_EQ(read_text(dir / "artifacts" / shrunk.rel_path), "x");  // nessuna sostituzione del file
}

TEST(Store, T_M2C_04_failed_fsync_or_rename_fails_the_job_without_row_and_without_retry) {
  for (const std::string point : {"artifact.fsync_file", "artifact.rename", "artifact.fsync_dir"}) {
    auto store = make_project("t04");
    store->apply_recipe(diamond(2).resolve(), {{"s", "a"}});
    int hits = 0;
    store->set_fault_injector([&](std::string_view p) { return p == point ? (++hits, EIO) : 0; });
    sqlite_dag_store* raw = store.get();
    testing::internal::CaptureStderr();
    const job_outcome out = run_job(*store, "s", [&](const job_record& job) {
      const auto bytes = bytes_of("content");
      const auto files = raw->write_files({artifact_content{bytes, "bin", "image", std::nullopt}});
      artifact_record a;
      a.node_key = job.node_key;
      a.sha256 = files[0].digest().sha256;
      a.size_bytes = files[0].digest().size_bytes;
      a.rel_path = files[0].rel_path();
      return job_result{a, {}};
    });
    testing::internal::GetCapturedStderr();
    EXPECT_EQ(out.exit_code, 2) << point;
    EXPECT_EQ(out.job.state, job_state::failed) << point;
    ASSERT_TRUE(out.stop.has_value());
    EXPECT_EQ(out.stop->category(), error_category::io) << point;
    EXPECT_EQ(hits, 1) << point;
    EXPECT_TRUE(store->artifacts_for(out.job.node_key).empty()) << point;
    EXPECT_EQ(store->read_slot("s").state, freshness::missing) << point;
  }
}

TEST(Store, T_M2C_05_unknown_newer_schema_is_read_only_and_leaves_no_trace) {
  fs::path dir;
  { make_project("t05", &dir)->apply_recipe(diamond(2).resolve(), {{"s", "a"}}); }
  {
    raw_db raw(dir);
    ASSERT_EQ(raw.exec("PRAGMA user_version = 2"), SQLITE_OK);
  }
  const std::string before = read_text(dir / "project.db");
  const auto listing = list_dir(dir);
  for (const auto intent : {open_intent::read_only, open_intent::read_write}) {
    {
      auto store = sqlite_dag_store::open(dir, intent);
      EXPECT_EQ(store->mode(), open_mode::read_only_unknown_schema);
      EXPECT_EQ(store->read_slot("s").state, freshness::missing);
      EXPECT_STOP(store->put_node(diamond(2).resolve().front().spec), error_category::schema, "store.read_only");
      EXPECT_STOP(store->create_slot("t", keys_of(diamond(2).resolve()).at("a")), error_category::schema, "store.read_only");
      EXPECT_STOP(store->start_job("s"), error_category::schema, "store.read_only");
      EXPECT_STOP(store->undo(), error_category::schema, "store.read_only");
    }
    EXPECT_EQ(read_text(dir / "project.db"), before);
    EXPECT_EQ(list_dir(dir), listing);
  }
  {
    auto store = sqlite_dag_store::open(dir, open_intent::read_only, open_options{2, {}, {}});
    EXPECT_EQ(store->mode(), open_mode::read_only_requested);
    EXPECT_STOP(store->start_job("s"), error_category::schema, "store.read_only");
  }
  EXPECT_EQ(list_dir(dir), listing);
}

TEST(Store, T_M2C_05_pending_wal_blocks_read_only_open) {
  fs::path dir;
  { make_project("t05w", &dir); }
  const child_outcome out = run_child(
      [&](const fault_injector&) {
        auto store = sqlite_dag_store::open(dir, open_intent::read_write);
        store->apply_recipe(diamond(2).resolve(), {{"s", "a"}});
        ::kill(::getpid(), SIGKILL);
      },
      0);
  ASSERT_TRUE(out.killed);
  ASSERT_GT(fs::file_size(dir / "project.db-wal"), 0u);
  const std::string before = read_text(dir / "project.db");
  EXPECT_STOP(sqlite_dag_store::open(dir, open_intent::read_only), error_category::schema, "store.wal_pending_read_only");
  EXPECT_EQ(read_text(dir / "project.db"), before);
  auto store = sqlite_dag_store::open(dir, open_intent::read_write);
  EXPECT_TRUE(store->get_slot("s").has_value());
}

TEST(Store, T_M2C_06_migration_backup_content_and_rollback) {
  const open_options good{2, {schema_migration{1, {"ALTER TABLE settings ADD COLUMN note TEXT"}}}, {}};
  const open_options broken{2, {schema_migration{1, {"ALTER TABLE settings ADD COLUMN note TEXT", "THIS IS NOT SQL"}}}, {}};
  fs::path dir;
  { make_project("t06", &dir)->apply_recipe(diamond(2).resolve(), {{"s", "a"}}); }
  const std::string logical = dump_logical(dir);

  EXPECT_STOP(sqlite_dag_store::open(dir, open_intent::read_write, broken), error_category::schema, "store.migration_failed");
  EXPECT_EQ(dump_logical(dir), logical);
  {
    raw_db db(dir, true);
    EXPECT_EQ(db.query("PRAGMA user_version"), "1|\n");
    EXPECT_EQ(db.query("SELECT name FROM pragma_table_info('settings') WHERE name='note'"), "");
  }
  EXPECT_TRUE(fs::exists(dir / "backups" / "project.v1.db"));

  { auto store = sqlite_dag_store::open(dir, open_intent::read_write, good); }
  {
    raw_db db(dir, true);
    EXPECT_EQ(db.query("PRAGMA user_version"), "2|\n");
    EXPECT_EQ(db.query("SELECT name FROM pragma_table_info('settings') WHERE name='note'"), "note|\n");
  }
  fs::create_directories(dir.parent_path() / "t06_backup_view");
  fs::copy_file(dir / "backups" / "project.v1.db", dir.parent_path() / "t06_backup_view" / "project.db");
  EXPECT_EQ(dump_logical(dir.parent_path() / "t06_backup_view"), logical);
  raw_db backup(dir.parent_path() / "t06_backup_view", true);
  EXPECT_EQ(backup.query("PRAGMA user_version"), "1|\n");
  EXPECT_STOP(sqlite_dag_store::open(dir, open_intent::read_write, open_options{3, {}, {}}), error_category::schema,
              "store.migration_failed");
}

TEST(Store, T_M2C_07_corrupt_database_stops_with_integrity_and_is_not_repaired) {
  fs::path dir;
  {
    auto store = make_project("t07", &dir);
    store->apply_recipe(diamond(2).resolve(), diamond_slots());
    for (int i = 0; i < 60; ++i) store->put_node(node_spec("op.fill", 1, 1, params_of(i), {}, sample_type::f32, "cpu_scalar", std::nullopt));
  }
  std::string bytes = read_text(dir / "project.db");
  ASSERT_GT(bytes.size(), 8192u);
  std::fill(bytes.begin() + 4096, bytes.end(), '\xA5');
  write_text(dir / "project.db", bytes);
  EXPECT_STOP(sqlite_dag_store::open(dir, open_intent::read_write), error_category::integrity, "store.corrupt");
  EXPECT_EQ(read_text(dir / "project.db"), bytes);
  EXPECT_STOP(sqlite_dag_store::open(dir, open_intent::read_only), error_category::integrity, "store.corrupt");
  EXPECT_EQ(read_text(dir / "project.db"), bytes);
}

TEST(Store, T_M2C_08_second_process_is_refused_while_the_lock_is_held) {
  fs::path dir;
  auto store = make_project("t08", &dir);
  const child_outcome out = run_child(
      [&](const fault_injector&) {
        for (const auto intent : {open_intent::read_write, open_intent::read_only}) {
          try {
            sqlite_dag_store::open(dir, intent);
          } catch (const stop_error& e) {
            if (e.category() == error_category::io && e.code() == "store.locked") continue;
          }
          throw std::runtime_error("apertura non rifiutata");
        }
      },
      0);
  EXPECT_FALSE(out.killed);
  EXPECT_EQ(out.exit_code, 0);
  EXPECT_STOP(sqlite_dag_store::open(dir, open_intent::read_only), error_category::io, "store.locked");
  store.reset();
  EXPECT_EQ(sqlite_dag_store::open(dir, open_intent::read_only)->mode(), open_mode::read_only_requested);
}

TEST(Store, T_M2C_09_foreign_files_are_refused_without_touching_or_creating_anything) {
  const fs::path dir = fresh_dir("t09");
  fs::create_directories(dir);
  EXPECT_STOP(sqlite_dag_store::open(dir, open_intent::read_write), error_category::invalid_input, "store.not_a_project");
  EXPECT_TRUE(list_dir(dir).empty());
  write_text(dir / "project.lock", "");
  EXPECT_STOP(sqlite_dag_store::open(dir, open_intent::read_write), error_category::invalid_input, "store.not_a_project");
  write_text(dir / "project.db", "this is not a database, just text of some length");
  auto listing = list_dir(dir);
  for (const auto intent : {open_intent::read_write, open_intent::read_only}) {
    EXPECT_STOP(sqlite_dag_store::open(dir, intent), error_category::invalid_input, "store.not_a_project");
  }
  EXPECT_EQ(read_text(dir / "project.db"), "this is not a database, just text of some length");
  EXPECT_EQ(list_dir(dir), listing);
  fs::remove(dir / "project.db");
  {
    sqlite3* db = nullptr;
    ASSERT_EQ(sqlite3_open((dir / "project.db").c_str(), &db), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(db, "CREATE TABLE t(x); INSERT INTO t VALUES(1);", nullptr, nullptr, nullptr), SQLITE_OK);
    sqlite3_close(db);
  }
  const std::string foreign = read_text(dir / "project.db");
  listing = list_dir(dir);
  for (const auto intent : {open_intent::read_write, open_intent::read_only}) {
    EXPECT_STOP(sqlite_dag_store::open(dir, intent), error_category::invalid_input, "store.not_a_project");
  }
  EXPECT_EQ(read_text(dir / "project.db"), foreign);
  EXPECT_EQ(list_dir(dir), listing);
}

TEST(Store, T_M2C_10_running_jobs_become_interrupted_never_completed) {
  fs::path dir;
  std::uint64_t running_id = 0, done_id = 0;
  {
    auto store = make_project("t10", &dir);
    store->apply_recipe(diamond(2).resolve(), {{"s", "a"}, {"t", "b"}});
    run_and_commit(*store, "t", "finished");
    done_id = 1;
    running_id = store->start_job("s").id;
  }
  auto store = sqlite_dag_store::open(dir, open_intent::read_write);
  EXPECT_EQ(store->recovery().interrupted_jobs, std::vector<std::uint64_t>{running_id});
  EXPECT_EQ(store->get_job(running_id)->state, job_state::interrupted);
  EXPECT_EQ(store->get_job(done_id)->state, job_state::completed);
  EXPECT_EQ(store->read_slot("s").state, freshness::missing);
}

TEST(Store, T_M2C_11_registered_originals_are_never_touched_by_the_store_workflow) {
  const fs::path dir = fresh_dir("t11");
  fs::create_directories(dir);
  const fs::path original = dir / "original.fits";
  write_text(original, std::string(5000, 'x'));
  const auto bytes = bytes_of(read_text(original));
  const content_digest digest = digest_of(bytes);
  struct stat before {};
  ASSERT_EQ(::stat(original.c_str(), &before), 0);
  {
    auto store = sqlite_dag_store::create(dir / "project");
    store->add_input(input_record{0, original.string(), digest.size_bytes, digest.sha256, "light"});
    recipe_graph g;
    g.add("dec", entry("fits.decode", 1, {recipe_input{"file", digest}}));
    store->apply_recipe(g.resolve(), {{"s", "dec"}});
    run_and_commit(*store, "s", "derived");
  }
  const auto after_bytes = bytes_of(read_text(original));
  EXPECT_EQ(digest_of(after_bytes), digest);
  struct stat after {};
  ASSERT_EQ(::stat(original.c_str(), &after), 0);
  EXPECT_EQ(after.st_mtim.tv_sec, before.st_mtim.tv_sec);
  EXPECT_EQ(after.st_mtim.tv_nsec, before.st_mtim.tv_nsec);
  EXPECT_EQ(after.st_ino, before.st_ino);
}

TEST(Store, T_M2C_12_symlink_in_artifacts_stops_recovery_and_deletes_nothing) {
  fs::path dir;
  { make_project("t12", &dir); }
  const fs::path outside = dir.parent_path() / "t12_outside";
  fs::create_directories(outside);
  write_text(outside / "secret", "do not delete");
  write_text(dir / "artifacts" / ".tmp-aaaa", "tmp");
  fs::create_symlink(outside / "secret", dir / "artifacts" / (std::string(64, 'c') + ".bin"));
  EXPECT_STOP(sqlite_dag_store::open(dir, open_intent::read_write), error_category::security, "store.symlink");
  EXPECT_EQ(read_text(outside / "secret"), "do not delete");
  EXPECT_TRUE(fs::exists(dir / "artifacts" / ".tmp-aaaa"));
  fs::remove(dir / "artifacts" / (std::string(64, 'c') + ".bin"));
  fs::create_directory_symlink(outside, dir / "artifacts" / "linkdir");
  EXPECT_STOP(sqlite_dag_store::open(dir, open_intent::read_write), error_category::security, "store.symlink");
  EXPECT_TRUE(fs::exists(outside / "secret"));
  fs::remove(dir / "artifacts" / "linkdir");
  fs::remove_all(dir / "artifacts");
  fs::create_directory_symlink(outside, dir / "artifacts");
  EXPECT_STOP(sqlite_dag_store::open(dir, open_intent::read_write), error_category::security, "store.symlink");
  EXPECT_TRUE(fs::exists(outside / "secret"));
}

TEST(Store, T_M2C_13_snapshots_are_immutable) {
  fs::path dir;
  {
    auto store = make_project("t13", &dir);
    store->apply_recipe(diamond(2).resolve(), diamond_slots());
    store->apply_recipe(diamond(9).resolve(), diamond_slots());
    store->undo();
    store->redo();
  }
  const std::string before = dump_logical(dir);
  {
    raw_db db(dir);
    for (const char* sql : {"UPDATE snapshots SET parent_id=NULL", "DELETE FROM snapshots", "UPDATE snapshot_entries SET slot_name='x'",
                            "DELETE FROM snapshot_entries", "UPDATE nodes SET backend='x'", "DELETE FROM nodes",
                            "UPDATE node_edges SET input_name='x'", "DELETE FROM node_edges"}) {
      EXPECT_NE(db.exec(sql), SQLITE_OK) << sql;
    }
  }
  EXPECT_EQ(dump_logical(dir), before);
}

TEST(Store, T_M2B_11_schema_version_is_outside_the_node_key_even_after_a_migration) {
  fs::path dir;
  std::string key, canonical;
  {
    auto store = make_project("t11b", &dir);
    const auto nodes = diamond(2).resolve();
    store->apply_recipe(nodes, {{"s", "d"}});
    key = nodes.back().spec.key();
    canonical = nodes.back().spec.canonical();
  }
  const open_options v2{2, {schema_migration{1, {"ALTER TABLE settings ADD COLUMN note TEXT"}}}, {}};
  auto store = sqlite_dag_store::open(dir, open_intent::read_write, v2);
  const node_spec node = *store->get_node(key);
  EXPECT_EQ(node.key(), key);
  EXPECT_EQ(node.canonical(), canonical);
  EXPECT_EQ(canonical.find("user_version"), std::string::npos);
}

TEST(Store, T_M2C_25_create_on_existing_path_is_refused_untouched) {
  const fs::path dir = fresh_dir("t25");
  fs::create_directories(dir);
  write_text(dir / "keep", "me");
  EXPECT_STOP(sqlite_dag_store::create(dir), error_category::security, "store.target_exists");
  EXPECT_EQ(list_dir(dir), std::vector<std::string>{"keep"});
  const fs::path file = fresh_dir("t25f");
  write_text(file, "file");
  EXPECT_STOP(sqlite_dag_store::create(file), error_category::security, "store.target_exists");
  EXPECT_EQ(read_text(file), "file");
  fs::create_symlink(dir, fresh_dir("t25l"));
  EXPECT_STOP(sqlite_dag_store::create(fs::path(ASTRO_TEST_TMP) / "m2c" / "t25l"), error_category::security, "store.target_exists");
}

TEST(Store, T_M2C_23_identical_outputs_are_reused_and_conflicting_names_are_security_errors) {
  fs::path dir;
  auto store = make_project("t23", &dir);
  store->apply_recipe(diamond(2).resolve(), {{"s", "a"}, {"t", "b"}});
  const artifact_record first = run_and_commit(*store, "s", "same bytes");
  const artifact_record second = run_and_commit(*store, "t", "same bytes");
  EXPECT_EQ(first.rel_path, second.rel_path);
  EXPECT_NE(first.id, second.id);
  EXPECT_EQ(list_dir(dir / "artifacts"), std::vector<std::string>{first.rel_path});

  const auto bytes = bytes_of("other content");
  const std::string name = digest_of(bytes).sha256 + ".bin";
  auto attempt = [&] { return store->write_files({artifact_content{bytes, "bin", "image", std::nullopt}}); };
  write_text(dir / "artifacts" / name, "tampered");
  EXPECT_STOP(attempt(), error_category::security, "store.digest_name_conflict");
  fs::remove(dir / "artifacts" / name);
  fs::create_directory(dir / "artifacts" / name);
  EXPECT_STOP(attempt(), error_category::security, "store.digest_name_conflict");
  fs::remove(dir / "artifacts" / name);
  write_text(dir / "outside_target", "x");
  fs::create_symlink(dir / "outside_target", dir / "artifacts" / name);
  EXPECT_STOP(attempt(), error_category::security, "store.digest_name_conflict");
  EXPECT_EQ(read_text(dir / "outside_target"), "x");
  fs::remove(dir / "artifacts" / name);
  EXPECT_EQ(list_dir(dir / "artifacts"), std::vector<std::string>{first.rel_path});
  const auto files = attempt();
  EXPECT_EQ(files[0].rel_path(), name);
}

TEST(Store, write_files_rejects_unexpected_digest_and_bad_extension) {
  auto store = make_project("twrite");
  const auto bytes = bytes_of("abc");
  EXPECT_STOP(store->write_files({artifact_content{bytes, "bin", "image", content_digest{std::string(64, '0'), 3}}}),
              error_category::integrity, "store.expected_digest_mismatch");
  EXPECT_STOP(store->write_files({artifact_content{bytes, "../x", "image", std::nullopt}}), error_category::invalid_input,
              "store.bad_extension");
  EXPECT_TRUE(list_dir(store->root() / "artifacts").empty());
  const auto ok = store->write_files({artifact_content{bytes, "bin", "image", digest_of(bytes)}});
  EXPECT_EQ(ok[0].digest(), digest_of(bytes));
}

}  // namespace
