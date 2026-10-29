// I kill simulano la morte del processo (SIGKILL); non provano la durabilita' contro perdita di corrente.
#include <fcntl.h>
#include <sys/stat.h>

#include <type_traits>

#include "store_test_support.hpp"

namespace {

using namespace store_test;

std::unique_ptr<sqlite_dag_store> make_project(const std::string& name, fs::path* dir_out = nullptr) {
  const fs::path dir = fresh_dir(name);
  if (dir_out != nullptr) *dir_out = dir;
  return sqlite_dag_store::create(dir);
}

static_assert(!std::is_constructible_v<stored_file, std::string, content_digest, std::string, std::string>,
              "stored_file non deve essere costruibile fuori dal writer atomico");
static_assert(!std::is_default_constructible_v<stored_file>);

// Lascia nel progetto un WAL non vuoto con user_version futura, poi muore.
void leave_future_wal(const fs::path& dir, bool header_already_future) {
  const child_outcome out = run_child(
      [&](const fault_injector&) {
        sqlite3* db = nullptr;
        if (sqlite3_open_v2((dir / "project.db").c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) throw std::runtime_error("open");
        sqlite3_exec(db, "PRAGMA wal_autocheckpoint=0", nullptr, nullptr, nullptr);
        if (sqlite3_exec(db, "PRAGMA user_version=2", nullptr, nullptr, nullptr) != SQLITE_OK) throw std::runtime_error("uv");
        if (header_already_future) sqlite3_exec(db, "PRAGMA wal_checkpoint(PASSIVE)", nullptr, nullptr, nullptr);
        if (sqlite3_exec(db, "INSERT INTO settings VALUES('k','v')", nullptr, nullptr, nullptr) != SQLITE_OK) throw std::runtime_error("ins");
        ::kill(::getpid(), SIGKILL);
      },
      0);
  ASSERT_TRUE(out.killed);
}

TEST(Review, B1_future_schema_with_pending_wal_stops_without_touching_db_or_wal) {
  for (const bool header_future : {false, true}) {
    fs::path dir;
    { make_project("rv1", &dir); }
    leave_future_wal(dir, header_future);
    const std::string db_before = read_text(dir / "project.db");
    const std::string wal_before = read_text(dir / "project.db-wal");
    ASSERT_FALSE(wal_before.empty());
    for (const auto intent : {open_intent::read_write, open_intent::read_only}) {
      try {
        sqlite_dag_store::open(dir, intent);
        ADD_FAILURE() << "apertura riuscita";
      } catch (const stop_error& e) {
        EXPECT_EQ(e.category(), error_category::schema);
        EXPECT_TRUE(e.code() == "store.wal_pending_future_schema" || (intent == open_intent::read_only && e.code() == "store.wal_pending_read_only"))
            << e.code();
      }
      EXPECT_EQ(read_text(dir / "project.db"), db_before);
      EXPECT_EQ(read_text(dir / "project.db-wal"), wal_before);
    }
    EXPECT_STOP(sqlite_dag_store::open(dir, open_intent::read_write), error_category::schema, "store.wal_pending_future_schema");
  }
}

TEST(Review, B2_run_job_path_publishes_and_completes_in_one_transaction_under_sigkill) {
  auto prepare = [](const std::string& name) {
    const fs::path dir = fresh_dir(name);
    sqlite_dag_store::create(dir)->apply_recipe(diamond(2).resolve(), {{"s", "a"}});
    return dir;
  };
  auto body = [](const fs::path& dir, const fault_injector& inj) {
    open_options options;
    options.fault = inj;
    auto store = sqlite_dag_store::open(dir, open_intent::read_write, options);
    sqlite_dag_store* raw = store.get();
    testing::internal::CaptureStderr();
    const job_outcome out = run_job(*store, "s", [&](const job_record& job) {
      const auto bytes = bytes_of("run_job payload");
      const auto files = raw->write_files({artifact_content{bytes, "bin", "image", std::nullopt}});
      artifact_record a;
      a.node_key = job.node_key;
      a.sha256 = files[0].digest().sha256;
      a.size_bytes = files[0].digest().size_bytes;
      a.rel_path = files[0].rel_path();
      a.kind = "image";
      return job_result{a, {}};
    });
    testing::internal::GetCapturedStderr();
    if (out.exit_code != 0) throw std::runtime_error("run_job fallito");
  };
  const fs::path dry = prepare("rv2_dry");
  const auto points = record_points([&](const fault_injector& f) { body(dry, f); });
  ASSERT_GE(points.size(), 9u);
  for (std::size_t i = 1; i <= points.size(); ++i) {
    SCOPED_TRACE("kill prima di " + points[i - 1]);
    const fs::path dir = prepare("rv2");
    ASSERT_TRUE(run_child([&](const fault_injector& f) { body(dir, f); }, static_cast<int>(i)).killed);
    auto store = sqlite_dag_store::open(dir, open_intent::read_write);
    const job_record job = *store->get_job(1);
    const bool current = store->read_slot("s").state == freshness::current;
    EXPECT_EQ(current, job.state == job_state::completed);
    EXPECT_EQ(current, i == points.size());
    if (!current) {
      EXPECT_EQ(job.state, job_state::interrupted);
      EXPECT_TRUE(store->artifacts_for(job.node_key).empty());
    }
  }
}

artifact_record record_of(const stored_file& f, const std::string& node_key) {
  artifact_record a;
  a.node_key = node_key;
  a.kind = f.kind();
  a.sha256 = f.digest().sha256;
  a.size_bytes = f.digest().size_bytes;
  a.rel_path = f.rel_path();
  a.output_role = f.role();
  return a;
}

TEST(Review, B3_only_files_written_by_this_store_instance_can_be_committed) {
  fs::path dir;
  auto store = make_project("rv3", &dir);
  fs::path other_dir;
  auto other = make_project("rv3_other", &other_dir);
  store->apply_recipe(diamond(2).resolve(), {{"s", "a"}});
  other->apply_recipe(diamond(2).resolve(), {{"s", "a"}});
  const job_record job = store->start_job("s");
  const auto bytes = bytes_of("real");
  const auto files = store->write_files({artifact_content{bytes, "bin", "image", std::nullopt}});

  artifact_record external = record_of(files[0], job.node_key);
  external.rel_path = std::string(64, 'a') + ".bin";
  external.sha256 = std::string(64, 'a');
  write_text(dir / "artifacts" / external.rel_path, "fake");
  external.size_bytes = 4;
  EXPECT_STOP(store->complete_job(job.id, external), error_category::invalid_input, "store.file_not_written_by_store");
  EXPECT_STOP(store->add_artifact(external), error_category::invalid_input, "store.file_not_written_by_store");

  const job_record other_job = other->start_job("s");
  const auto foreign_bytes = bytes_of("foreign");
  const auto foreign = other->write_files({artifact_content{foreign_bytes, "bin", "image", std::nullopt}});
  EXPECT_STOP(store->commit_job(job.id, foreign), error_category::integrity, "store.file_identity_changed");
  EXPECT_STOP(store->complete_job(job.id, record_of(foreign[0], job.node_key)), error_category::invalid_input,
              "store.file_not_written_by_store");
  (void)other_job;

  // stessa dimensione, contenuto diverso, nuovo inode
  const fs::path target = dir / "artifacts" / files[0].rel_path();
  write_text(dir / "replacement", "XXXX");
  fs::rename(dir / "replacement", target);
  EXPECT_STOP(store->commit_job(job.id, files), error_category::integrity, "store.file_identity_changed");
  EXPECT_STOP(store->complete_job(job.id, record_of(files[0], job.node_key)), error_category::integrity, "store.file_identity_changed");
  fs::remove(target);
  EXPECT_STOP(store->commit_job(job.id, files), error_category::integrity, "store.file_identity_changed");
  EXPECT_EQ(store->get_job(job.id)->state, job_state::running);
  EXPECT_TRUE(store->artifacts_for(job.node_key).empty());

  const auto again = store->write_files({artifact_content{bytes, "bin", "image", std::nullopt}});
  artifact_record wrong_node = record_of(again[0], std::string(64, 'f'));
  EXPECT_STOP(store->complete_job(job.id, wrong_node), error_category::invalid_input, "dag.artifact_node_mismatch");
  const artifact_record done = store->complete_job(job.id, record_of(again[0], job.node_key));
  EXPECT_EQ(store->get_job(job.id)->state, job_state::completed);
  EXPECT_EQ(store->read_slot("s").artifact->id, done.id);
}

void rewrite_keeping_mtime(const fs::path& path, const std::string& content) {
  struct stat st {};
  ASSERT_EQ(::stat(path.c_str(), &st), 0);
  write_text(path, content);
  const timespec times[2] = {st.st_atim, st.st_mtim};
  ASSERT_EQ(::utimensat(AT_FDCWD, path.c_str(), times, 0), 0);
}

TEST(Review, T3_in_place_change_with_restored_mtime_is_caught_by_ctime_and_tokens_are_single_use) {
  fs::path dir;
  auto store = make_project("rv3c", &dir);
  store->apply_recipe(diamond(2).resolve(), {{"s", "a"}, {"t", "b"}});
  const job_record job = store->start_job("s");
  const auto bytes = bytes_of("abcd");
  const auto files = store->write_files({artifact_content{bytes, "bin", "image", std::nullopt}});
  rewrite_keeping_mtime(dir / "artifacts" / files[0].rel_path(), "WXYZ");
  EXPECT_STOP(store->commit_job(job.id, files), error_category::integrity, "store.file_identity_changed");

  const auto good_bytes = bytes_of("good");
  const auto good = store->write_files({artifact_content{good_bytes, "bin", "image", std::nullopt}});
  store->commit_job(job.id, good);
  const job_record second = store->start_job("t");
  EXPECT_STOP(store->commit_job(second.id, good), error_category::invalid_input, "store.file_not_written_by_store");
}

TEST(Review, T3_backup_temporary_replaced_by_symlink_or_modified_in_place_stops_the_migration) {
  const open_options base{2, {schema_migration{1, {"ALTER TABLE settings ADD COLUMN note TEXT"}}}, {}};
  for (const std::string point : {"backup.after_create", "backup.before_publish", "backup.before_publish_mtime"}) {
    SCOPED_TRACE(point);
    fs::path dir;
    { make_project("rv3b", &dir); }
    const fs::path outside = dir.parent_path() / "rv3b_outside";
    write_text(outside, "outside");
    open_options options = base;
    options.fault = [&](std::string_view p) {
      const std::string name = std::string(p);
      auto tmp = [&] { return dir / "backups" / list_dir(dir / "backups").front(); };
      if (name == "backup.after_create" && point == "backup.after_create") {
        const fs::path t = tmp();
        fs::remove(t);
        fs::create_symlink(outside, t);
      } else if (name == "backup.before_publish" && point == "backup.before_publish") {
        std::ofstream out(tmp(), std::ios::binary | std::ios::app);
        out << "x";
      } else if (name == "backup.before_publish" && point == "backup.before_publish_mtime") {
        struct stat st {};
        ::stat(tmp().c_str(), &st);
        std::fstream f(tmp(), std::ios::binary | std::ios::in | std::ios::out);
        f.write("Z", 1);
        f.close();
        const timespec times[2] = {st.st_atim, st.st_mtim};
        ::utimensat(AT_FDCWD, tmp().c_str(), times, 0);
      }
      return 0;
    };
    try {
      sqlite_dag_store::open(dir, open_intent::read_write, options);
      ADD_FAILURE() << "migrazione riuscita";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.code(), "store.backup_failed");
      bool cause = false;
      for (const auto& [k, v] : e.context()) cause |= (k == "causa" && v == "store.backup_identity");
      EXPECT_TRUE(cause);
    }
    EXPECT_EQ(read_text(outside), "outside");
    EXPECT_FALSE(fs::exists(dir / "backups" / "project.v1.db"));
    raw_db db(dir, true);
    EXPECT_EQ(db.query("PRAGMA user_version"), "1|\n");
  }
}

TEST(Review, T1_rewrite_in_place_between_digest_and_publication_is_an_integrity_error_including_reuse) {
  for (const bool reuse : {false, true}) {
    SCOPED_TRACE(reuse ? "riuso" : "nuovo");
    fs::path dir;
    auto store = make_project("rv1w", &dir);
    const auto bytes = bytes_of("payload-A");
    const std::string name = digest_of(bytes).sha256 + ".bin";
    if (reuse) store->write_files({artifact_content{bytes, "bin", "image", std::nullopt}});
    store->set_fault_injector([&](std::string_view p) {
      if (p == "artifact.fsync_dir") rewrite_keeping_mtime(dir / "artifacts" / name, "payload-B");
      return 0;
    });
    EXPECT_STOP(store->write_files({artifact_content{bytes, "bin", "image", std::nullopt}}), error_category::integrity,
                "store.file_identity_changed");
    store->set_fault_injector({});
  }
}

TEST(Review, M5_duplicate_output_roles_in_one_commit_are_refused) {
  auto store = make_project("rv5d");
  store->apply_recipe(diamond(2).resolve(), {{"s", "a"}});
  const job_record job = store->start_job("s");
  const auto x = bytes_of("one");
  const auto y = bytes_of("two");
  auto files = store->write_files({artifact_content{x, "bin", "image", std::nullopt}});
  const auto second = store->write_files({artifact_content{y, "bin", "image", std::nullopt}});
  files.push_back(second[0]);
  EXPECT_STOP(store->commit_job(job.id, files), error_category::invalid_input, "dag.duplicate_output_role");
  EXPECT_TRUE(store->artifacts_for(job.node_key).empty());
}

TEST(Review, M3_set_publish_role_relinks_bumps_generation_snapshots_and_clears_redo) {
  auto store = make_project("rv3r");
  store->apply_recipe(diamond(2).resolve(), {{"s", "b"}});
  const job_record job = store->start_job("s");
  const auto img = bytes_of("img");
  const auto rep = bytes_of("rep");
  const auto rows = store->commit_job(job.id, store->write_files({artifact_content{img, "bin", "image", std::nullopt, "primary"},
                                                                   artifact_content{rep, "json", "report", std::nullopt, "report"}}));
  const auto snap_before = *store->history().current_snapshot_id;
  const std::uint64_t gen = store->get_slot("s")->current_generation;
  store->set_publish_role("s", "report");
  EXPECT_EQ(store->get_slot("s")->artifact_id, rows[1].id);
  EXPECT_EQ(store->get_slot("s")->current_generation, gen + 1);
  EXPECT_EQ(store->snapshot_parent(*store->history().current_snapshot_id), snap_before);
  EXPECT_EQ(store->snapshot_entries(*store->history().current_snapshot_id).at(0).publish_role, "report");
  EXPECT_EQ(store->snapshot_entries(snap_before).at(0).publish_role, "primary");
  store->undo();
  EXPECT_EQ(store->get_slot("s")->artifact_id, rows[0].id);
  store->redo();
  EXPECT_EQ(store->get_slot("s")->artifact_id, rows[1].id);
  store->undo();
  store->set_publish_role("s", "nuovo");  // nessun artefatto di quel ruolo: missing
  EXPECT_FALSE(store->history().redo_snapshot_id.has_value());
  EXPECT_EQ(store->read_slot("s").state, freshness::missing);
  store->set_publish_role("s", "nuovo");  // invariato: nessuna mutazione
  EXPECT_EQ(store->get_slot("s")->current_generation, gen + 5);
}

TEST(Review, B4_backup_replaced_between_write_and_publish_is_refused) {
  const open_options base{2, {schema_migration{1, {"ALTER TABLE settings ADD COLUMN note TEXT"}}}, {}};
  fs::path dir;
  { make_project("rv4k", &dir); }
  open_options options = base;
  options.fault = [&](std::string_view point) {
    if (point == "backup.before_publish") {
      for (const auto& name : list_dir(dir / "backups")) {
        fs::remove(dir / "backups" / name);
        write_text(dir / "backups" / name, "replacement");
      }
    }
    return 0;
  };
  try {
    sqlite_dag_store::open(dir, open_intent::read_write, options);
    ADD_FAILURE() << "migrazione riuscita";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.code(), "store.backup_failed");
    bool cause = false;
    for (const auto& [k, v] : e.context()) cause |= (k == "causa" && v == "store.backup_identity");
    EXPECT_TRUE(cause);
  }
  EXPECT_FALSE(fs::exists(dir / "backups" / "project.v1.db"));
  raw_db db(dir, true);
  EXPECT_EQ(db.query("PRAGMA user_version"), "1|\n");
}

TEST(Review, B4_multi_output_publishes_the_slot_role_and_undo_relinks_the_same_role) {
  auto store = make_project("rv4");
  const auto v1 = diamond(2).resolve();
  store->apply_recipe(v1, {{"s", "b"}});
  auto run_two = [&](const std::string& tag) {
    const job_record job = store->start_job("s");
    const auto img = bytes_of("image-" + tag);
    const auto rep = bytes_of("report-" + tag);
    return store->commit_job(job.id, store->write_files({artifact_content{img, "bin", "image", std::nullopt, "primary"},
                                                          artifact_content{rep, "json", "report", std::nullopt, "report"}}));
  };
  const auto first = run_two("v1");
  ASSERT_EQ(first.size(), 2u);
  EXPECT_EQ(store->read_slot("s").artifact->id, first[0].id);
  EXPECT_EQ(store->read_slot("s").artifact->output_role, "primary");
  store->apply_recipe(diamond(9).resolve(), {{"s", "b"}});
  store->undo();
  EXPECT_EQ(store->get_slot("s")->artifact_id, first[0].id);  // non l'ultimo (il report)
  store->redo();
  store->undo();
  EXPECT_EQ(store->get_slot("s")->artifact_id, first[0].id);

  store->set_publish_role("s", "report");
  store->apply_recipe(diamond(9).resolve(), {{"s", "b"}});
  store->undo();
  EXPECT_EQ(store->get_slot("s")->artifact_id, first[1].id);
  const job_record job = store->start_job("s");
  const auto img = bytes_of("only image");
  EXPECT_STOP(store->commit_job(job.id, store->write_files({artifact_content{img, "bin", "image", std::nullopt, "primary"}})),
              error_category::invalid_input, "store.role_not_produced");
  EXPECT_EQ(store->get_job(job.id)->state, job_state::running);
  EXPECT_TRUE(store->artifacts_for(job.node_key).size() == 2u);
  EXPECT_STOP(store->write_files({artifact_content{img, "bin", "image", std::nullopt, "x"}, artifact_content{img, "bin", "image", std::nullopt, "x"}}),
              error_category::invalid_input, "store.bad_role");
  EXPECT_STOP(store->set_publish_role("s", "Bad Role"), error_category::invalid_input, "store.bad_role");
}

TEST(Review, B4_publish_refuses_an_artifact_of_another_role) {
  auto store = make_project("rv4b");
  store->apply_recipe(diamond(2).resolve(), {{"s", "a"}});
  const job_record job = store->start_job("s");
  const auto rep = bytes_of("report");
  const auto files = store->write_files({artifact_content{rep, "json", "report", std::nullopt, "report"}});
  artifact_record a;
  a.node_key = job.node_key;
  a.sha256 = files[0].digest().sha256;
  a.size_bytes = files[0].digest().size_bytes;
  a.rel_path = files[0].rel_path();
  a.output_role = "report";
  const artifact_record stored = store->add_artifact(a);
  EXPECT_STOP(store->publish(job.id, stored.id), error_category::invalid_input, "dag.artifact_role_mismatch");
}

TEST(Review, B5_undo_detaches_slots_created_after_the_snapshot_and_redo_restores_them) {
  auto store = make_project("rv5");
  store->apply_recipe(diamond(2).resolve(), {{"sa", "a"}});
  run_and_commit(*store, "sa", "a1");
  store->apply_recipe(diamond(2).resolve(), {{"sa", "a"}, {"sb", "b"}});
  const artifact_record b_art = run_and_commit(*store, "sb", "b1");
  const job_record running = store->start_job("sb");
  const std::uint64_t gen = store->get_slot("sb")->current_generation;
  store->undo();
  const slot_record sb = *store->get_slot("sb");
  EXPECT_TRUE(sb.detached);
  EXPECT_TRUE(sb.target_node_key.empty());
  EXPECT_FALSE(sb.artifact_id.has_value());
  EXPECT_EQ(sb.current_generation, gen + 1);
  EXPECT_EQ(store->read_slot("sb").state, freshness::missing);
  EXPECT_STOP(store->start_job("sb"), error_category::invalid_input, "dag.slot_detached");
  const auto bytes = bytes_of("late");
  EXPECT_STOP(store->commit_job(running.id, store->write_files({artifact_content{bytes, "bin", "image", std::nullopt}})),
              error_category::stale_generation, "dag.stale_publish");
  EXPECT_STOP(store->export_slots(fresh_dir("rv5_x"), {"sb"}), error_category::stale_generation, "export.not_current");
  EXPECT_EQ(store->read_slot("sa").state, freshness::current);
  store->redo();
  const slot_record back = *store->get_slot("sb");
  EXPECT_FALSE(back.detached);
  EXPECT_EQ(back.target_node_key, keys_of(diamond(2).resolve()).at("b"));
  // il risultato stale del job rifiutato resta in cache sotto il suo nodo: e' l'artefatto piu' recente
  EXPECT_EQ(back.artifact_id, store->artifacts_for(back.target_node_key).back().id);
  EXPECT_GT(*back.artifact_id, b_art.id);
  EXPECT_EQ(store->read_slot("sb").state, freshness::current);
  store->undo();
  store->apply_recipe(diamond(2).resolve(), {{"sa", "a"}, {"sb", "b"}});  // riattacco da ricetta
  EXPECT_FALSE(store->get_slot("sb")->detached);
}

TEST(Review, B6_symlinked_backups_or_artifacts_never_write_outside_the_project) {
  const open_options v2{2, {schema_migration{1, {"ALTER TABLE settings ADD COLUMN note TEXT"}}}, {}};
  fs::path dir;
  { make_project("rv6", &dir); }
  const fs::path outside = dir.parent_path() / "rv6_outside";
  fs::create_directories(outside);
  fs::remove_all(dir / "backups");
  fs::create_directory_symlink(outside, dir / "backups");
  try {
    sqlite_dag_store::open(dir, open_intent::read_write, v2);
    ADD_FAILURE() << "migrazione riuscita";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::schema);
    EXPECT_EQ(e.code(), "store.backup_failed");
    bool has_cause = false;
    for (const auto& [k, v] : e.context()) has_cause |= (k == "causa" && v == "store.symlink");
    EXPECT_TRUE(has_cause);
  }
  EXPECT_TRUE(list_dir(outside).empty());
  {
    raw_db db(dir, true);
    EXPECT_EQ(db.query("PRAGMA user_version"), "1|\n");
  }
  fs::remove(dir / "backups");
  fs::create_directory(dir / "backups");
  { auto store = sqlite_dag_store::open(dir, open_intent::read_write, v2); }
  EXPECT_TRUE(fs::exists(dir / "backups" / "project.v1.db"));
  EXPECT_EQ(list_dir(dir / "backups"), std::vector<std::string>{"project.v1.db"});

  fs::path dir2;
  auto store = make_project("rv6b", &dir2);
  const fs::path outside2 = dir2.parent_path() / "rv6b_outside";
  fs::create_directories(outside2);
  fs::remove_all(dir2 / "artifacts");
  fs::create_directory_symlink(outside2, dir2 / "artifacts");
  const auto bytes = bytes_of("x");
  EXPECT_STOP(store->write_files({artifact_content{bytes, "bin", "image", std::nullopt}}), error_category::security, "store.symlink");
  EXPECT_TRUE(list_dir(outside2).empty());
}

TEST(Review, M13_create_slot_and_retarget_slot_make_snapshots_and_clear_redo) {
  auto store = make_project("rv13");
  const auto nodes = diamond(2).resolve();
  store->apply_recipe(nodes, {});
  EXPECT_FALSE(store->history().current_snapshot_id.has_value());
  store->create_slot("s", keys_of(nodes).at("a"));
  const auto first = store->history().current_snapshot_id;
  ASSERT_TRUE(first.has_value());
  store->retarget_slot("s", keys_of(nodes).at("b"));
  EXPECT_EQ(store->snapshot_parent(*store->history().current_snapshot_id), first);
  store->undo();
  EXPECT_EQ(store->get_slot("s")->target_node_key, keys_of(nodes).at("a"));
  EXPECT_TRUE(store->history().redo_snapshot_id.has_value());
  store->retarget_slot("s", keys_of(nodes).at("c"));
  EXPECT_FALSE(store->history().redo_snapshot_id.has_value());
}

TEST(Review, M16_sigkill_inside_a_multi_output_commit_is_all_or_nothing_and_errors_roll_back) {
  auto prepare = [](const std::string& name) {
    const fs::path dir = fresh_dir(name);
    sqlite_dag_store::create(dir)->apply_recipe(diamond(2).resolve(), {{"s", "a"}});
    return dir;
  };
  auto body = [](const fs::path& dir, const fault_injector& inj) {
    open_options options;
    options.fault = inj;
    auto store = sqlite_dag_store::open(dir, open_intent::read_write, options);
    const job_record job = store->start_job("s");
    const auto img = bytes_of("multi image");
    const auto rep = bytes_of("multi report");
    store->commit_job(job.id, store->write_files({artifact_content{img, "bin", "image", std::nullopt, "primary"},
                                                   artifact_content{rep, "json", "report", std::nullopt, "report"}}));
  };
  const fs::path dry = prepare("rv16_dry");
  const auto points = record_points([&](const fault_injector& f) { body(dry, f); });
  EXPECT_EQ(std::count(points.begin(), points.end(), "commit.row"), 2);
  for (std::size_t i = 1; i <= points.size(); ++i) {
    SCOPED_TRACE("kill prima di " + points[i - 1]);
    const fs::path dir = prepare("rv16");
    ASSERT_TRUE(run_child([&](const fault_injector& f) { body(dir, f); }, static_cast<int>(i)).killed);
    auto store = sqlite_dag_store::open(dir, open_intent::read_write);
    const std::string node = store->get_job(1)->node_key;
    const bool committed = i == points.size();
    EXPECT_EQ(store->artifacts_for(node).size(), committed ? 2u : 0u);
    EXPECT_EQ(list_dir(dir / "artifacts").size(), committed ? 2u : 0u);
    EXPECT_EQ(store->read_slot("s").state, committed ? freshness::current : freshness::missing);
  }
  for (const char* failing : {"commit.row", "commit.inserted", "commit.before_commit"}) {
    SCOPED_TRACE(failing);
    const fs::path dir = prepare("rv16e");
    auto store = sqlite_dag_store::open(dir, open_intent::read_write);
    int hits = 0;
    store->set_fault_injector([&](std::string_view p) { return p == failing ? (++hits, EIO) : 0; });
    const job_record job = store->start_job("s");
    const auto img = bytes_of("e image");
    const auto rep = bytes_of("e report");
    EXPECT_STOP(store->commit_job(job.id, store->write_files({artifact_content{img, "bin", "image", std::nullopt, "primary"},
                                                               artifact_content{rep, "json", "report", std::nullopt, "report"}})),
                error_category::io, "store.syscall_failed");
    EXPECT_EQ(hits, 1);
    EXPECT_TRUE(store->artifacts_for(job.node_key).empty());
    EXPECT_EQ(store->get_job(job.id)->state, job_state::running);
    EXPECT_EQ(store->read_slot("s").state, freshness::missing);
  }
}

TEST(Review, M8_manifest_must_declare_versions_and_a_closed_acyclic_chain) {
  auto store = make_project("rv8");
  store->apply_recipe(diamond(2).resolve(), diamond_slots());
  run_and_commit(*store, "sd", "payload d");
  const fs::path good = fs::path(store->root()).parent_path() / "rv8_export";
  store->export_slots(good, {"sd"});
  const export_manifest m = verify_export(good);
  EXPECT_EQ(m.nodes.size(), 4u);
  const std::string text = read_text(good / "manifest.json");
  auto bundle = [&](const std::string& name, const std::string& manifest) {
    const fs::path d = good.parent_path() / name;
    fs::remove_all(d);
    fs::create_directories(d);
    write_text(d / "manifest.json", manifest);
    write_text(d / "COMMIT", sha256_hex(bytes_of(manifest)));
    write_text(d / "sd.bin", "payload d");
    return d;
  };
  std::string no_versions = text;
  const auto v = no_versions.find("\"versions\":{");
  ASSERT_NE(v, std::string::npos);
  no_versions.replace(v, no_versions.find('}', v) - v + 1, "\"versions\":{}");
  EXPECT_STOP(verify_export(bundle("rv8_nov", no_versions)), error_category::invalid_input, "export.manifest_malformed");

  json doc = parse_json(text);
  auto rebuilt = [&](const std::function<bool(const json&)>& keep_node, bool duplicate_first) {
    json out = json::object();
    for (std::size_t i = 0; i < doc.keys().size(); ++i) {
      if (doc.keys()[i] != "nodes") { out.set(doc.keys()[i], doc.items()[i]); continue; }
      json nodes = json::array();
      for (const auto& n : doc.items()[i].items()) if (keep_node(n)) nodes.push(n);
      if (duplicate_first) nodes.push(nodes.items().front());
      out.set("nodes", std::move(nodes));
    }
    return canonical_json(out);
  };
  const std::string only_d = keys_of(diamond(2).resolve()).at("d");
  EXPECT_STOP(verify_export(bundle("rv8_chain", rebuilt([&](const json& n) { return n.find("node_key")->as_string() == only_d; }, false))),
              error_category::invalid_input, "export.manifest_malformed");
  EXPECT_STOP(verify_export(bundle("rv8_dup", rebuilt([](const json&) { return true; }, true))), error_category::invalid_input,
              "export.manifest_malformed");
}

TEST(Review, M10_verify_stops_when_a_file_is_replaced_or_modified_during_the_check) {
  auto store = make_project("rv10");
  store->apply_recipe(diamond(2).resolve(), diamond_slots());
  run_and_commit(*store, "sd", "payload d");
  const fs::path dir = store->root().parent_path() / "rv10_export";
  store->export_slots(dir, {"sd"});
  EXPECT_NO_THROW(verify_export(dir));
  EXPECT_STOP(verify_export(dir, [&](std::string_view) {
                write_text(dir / "sd.new", "payload d");
                fs::rename(dir / "sd.new", dir / "sd.bin");
                return 0;
              }),
              error_category::integrity, "export.changed_during_verify");
  fs::remove_all(dir);
  store->export_slots(dir, {"sd"});
  EXPECT_STOP(verify_export(dir, [&](std::string_view) {
                std::ofstream out(dir / "sd.bin", std::ios::binary | std::ios::in | std::ios::out);
                out.write("P", 1);
                return 0;
              }),
              error_category::integrity, "export.changed_during_verify");
}

TEST(Review, M6_verify_stops_when_a_file_is_added_or_the_root_is_replaced_during_the_check) {
  auto store = make_project("rv6v");
  store->apply_recipe(diamond(2).resolve(), diamond_slots());
  run_and_commit(*store, "sd", "payload d");
  const fs::path dir = store->root().parent_path() / "rv6v_export";
  store->export_slots(dir, {"sd"});
  EXPECT_STOP(verify_export(dir, [&](std::string_view) {
                write_text(dir / "late.txt", "x");
                return 0;
              }),
              error_category::integrity, "export.changed_during_verify");
  fs::remove(dir / "late.txt");
  const fs::path moved = store->root().parent_path() / "rv6v_moved";
  fs::remove_all(moved);
  EXPECT_STOP(verify_export(dir, [&](std::string_view) {
                fs::rename(dir, moved);
                fs::create_directory(dir);
                fs::copy(moved, dir, fs::copy_options::recursive);
                return 0;
              }),
              error_category::integrity, "export.changed_during_verify");
}

TEST(Review, M11_export_destination_is_resolved_through_the_verified_parent) {
  auto store = make_project("rv11");
  store->apply_recipe(diamond(2).resolve(), diamond_slots());
  run_and_commit(*store, "sd", "payload d");
  const fs::path link = store->root().parent_path() / "rv11_link";
  fs::create_directory_symlink(store->root(), link);
  EXPECT_STOP(store->export_slots(link / "exp", {"sd"}), error_category::security, "export.target_forbidden");
  EXPECT_STOP(store->export_slots(store->root() / "artifacts" / ".." / "exp", {"sd"}), error_category::security, "export.target_forbidden");
  EXPECT_STOP(store->export_slots(store->root().parent_path() / "..", {"sd"}), error_category::invalid_input, "export.target_invalid");
  EXPECT_FALSE(fs::exists(store->root() / "exp"));
}

}  // namespace
