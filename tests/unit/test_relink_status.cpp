// U9 (M3.a), F-IO-08: rilevamento degli originali mancanti, spostati o cambiati (RL-02) e blocco con copie dello stesso
// digest (RL-03). Casi: T-M3A-07 a, b, c; T-M3A-09 e. Contratto: docs/contracts/F-IO-08-ricollegamento.md.
#include <sys/stat.h>

#include "relink_support.hpp"

namespace {

using namespace rl;

struct seven {
  world w;
  std::vector<orig> origs;
  std::vector<input_record> records;
  std::vector<std::pair<std::uint64_t, std::uint64_t>> identity;  // (dev, ino) letti dal test prima di ogni modifica
};

// Progetto con sette input registrati e sette import completati.
seven build_seven(const std::string& name) {
  seven s;
  s.w = make_world(name);
  const fs::path dir = s.w.base / "orig";
  for (std::size_t k = 0; k < 7; ++k) {
    s.origs.push_back(make_orig(dir / ("light_" + std::to_string(k) + ".fits"), light_bytes(k)));
    const stat_of id(s.origs.back().path);
    s.identity.emplace_back(id.dev(), id.ino());
  }
  auto store = sqlite_dag_store::create(s.w.proj);
  for (const orig& o : s.origs) s.records.push_back(register_input(*store, o));
  runner_env env;
  for (std::size_t k = 0; k < 7; ++k) {
    const import_request req = import_request_of(s.origs[k]);
    const node_spec node = make_import_node(req);
    store->put_node(node);
    store->create_slot(import_slot(k), node.key());
    const job_outcome out = run_import_job(*store, import_slot(k), req, env.pool, env.cancel);
    EXPECT_EQ(out.exit_code, 0) << import_slot(k);
  }
  return s;
}

void expect_state(const std::vector<input_state>& states, const seven& s, std::size_t k, const std::string& status,
                  const std::string& reason) {
  const input_state& st_k = state_of(states, s.records[k].id);
  EXPECT_EQ(st_k.status, status) << "input " << k + 1;
  EXPECT_EQ(st_k.reason, reason) << "input " << k + 1;
  EXPECT_EQ(st_k.path, s.origs[k].path.string()) << "input " << k + 1;
  EXPECT_EQ(st_k.sha256, s.origs[k].sha) << "input " << k + 1;
  EXPECT_EQ(st_k.size_bytes, s.origs[k].size) << "input " << k + 1;
  EXPECT_EQ(st_k.dev, s.identity[k].first) << "input " << k + 1;
  EXPECT_EQ(st_k.ino, s.identity[k].second) << "input " << k + 1;
}

// T-M3A-07 a: sette input, sette stati diversi, progetto invariato e riapribile (RL-02).
TEST(RelinkStatus, T_M3A_07a_moved_deleted_replaced_detected_project_untouched) {
  seven s = build_seven("07a");
  std::string report_before;
  {
    auto store = sqlite_dag_store::open(s.w.proj, open_intent::read_write);
    report_before = recovery_report_canonical(store->recovery());
  }
  const std::string tables_before = dump_all(s.w.proj);
  const std::string artifacts_before = artifacts_tree(s.w.proj);
  ASSERT_FALSE(artifacts_before.empty());

  // 1 rinominato nella stessa directory; 2 spostato in una sottodirectory con lo stesso nome; 3 cancellato con un'esca
  // (stesso nome e stesso contenuto in una directory sorella); 4 sostituito da una copia byte-identica (inode nuovo);
  // 5 con un byte in più; 6 sostituito da una directory; 7 intatto.
  fs::rename(s.origs[0].path, s.origs[0].path.parent_path() / "light_0_rinominato.fits");
  fs::create_directories(s.origs[1].path.parent_path() / "sub");
  fs::rename(s.origs[1].path, s.origs[1].path.parent_path() / "sub" / s.origs[1].path.filename());
  fs::remove(s.origs[2].path);
  write_bytes(s.w.base / "sorella" / s.origs[2].path.filename(), s.origs[2].content);
  replace_with_new_inode(s.origs[3].path, s.origs[3].content);
  const stat_of new_copy(s.origs[3].path);
  EXPECT_NE(new_copy.ino(), s.identity[3].second);
  append_byte(s.origs[4].path, 0x5a);
  fs::remove(s.origs[5].path);
  fs::create_directory(s.origs[5].path);

  for (const open_intent intent : {open_intent::read_write, open_intent::read_only}) {
    SCOPED_TRACE(intent == open_intent::read_write ? "lettura-scrittura" : "sola lettura");
    {
      auto store = sqlite_dag_store::open(s.w.proj, intent);
      ASSERT_NE(store, nullptr);
      EXPECT_EQ(query(s.w.proj, "PRAGMA quick_check", true), "ok|\n");
      if (intent == open_intent::read_write) {
        EXPECT_EQ(recovery_report_canonical(store->recovery()), report_before);
      }
      const std::vector<input_state> states = store->input_status();
      ASSERT_EQ(states.size(), 7u);
      for (std::size_t k = 0; k < 7; ++k) EXPECT_EQ(states[k].input_id, s.records[k].id) << "ordine di id";
      expect_state(states, s, 0, "missing", "input.missing");
      expect_state(states, s, 1, "missing", "input.missing");
      expect_state(states, s, 2, "missing", "input.missing");
      expect_state(states, s, 3, "changed", "input.modified");
      expect_state(states, s, 4, "changed", "input.size_changed");
      expect_state(states, s, 5, "changed", "input.not_regular");
      expect_state(states, s, 6, "present", "");
      // Identità trovata: letta dal test con stat.
      EXPECT_FALSE(state_of(states, s.records[0].id).found_ino.has_value());
      EXPECT_TRUE(state_of(states, s.records[3].id).found_ino.has_value());
      EXPECT_EQ(state_of(states, s.records[3].id).found_ino.value_or(0), new_copy.ino());
      EXPECT_EQ(state_of(states, s.records[3].id).found_dev.value_or(0), new_copy.dev());
      EXPECT_EQ(state_of(states, s.records[4].id).found_ino.value_or(0), s.identity[4].second);
      EXPECT_TRUE(state_of(states, s.records[5].id).found_ino.has_value());
      EXPECT_EQ(state_of(states, s.records[6].id).found_ino.value_or(0), s.identity[6].second);
      EXPECT_EQ(state_of(states, s.records[6].id).found_dev.value_or(0), s.identity[6].first);
    }
    EXPECT_EQ(dump_all(s.w.proj), tables_before) << "tutte le tabelle (SQL) invariate";
    EXPECT_EQ(artifacts_tree(s.w.proj), artifacts_before) << "artifacts/ invariato";
  }
  // Nessun ricollegamento: né per nome identico nella stessa directory, né per l'esca, né per i file spostati.
  EXPECT_EQ(count_rows(s.w.proj, "input_relinks"), 0u);
  EXPECT_EQ(query(s.w.proj, "SELECT path FROM inputs ORDER BY id"), [&] {
    std::string t;
    for (const orig& o : s.origs) t += o.path.string() + "|\n";
    return t;
  }());
}

// T-M3A-07 b: input_status non scrive niente (RL-02, RL-08): `PRAGMA data_version` di una seconda connessione, aperta prima
// della chiamata, invariato; byte, mtime e ctime di project.db e project.db-wal, e tutte le tabelle, identici.
TEST(RelinkStatus, T_M3A_07b_status_writes_nothing) {
  seven s = build_seven("07b");
  fs::remove(s.origs[1].path);
  fs::rename(s.origs[2].path, s.w.base / "altrove.fits");
  const fs::path db = s.w.proj / "project.db";
  const fs::path wal = s.w.proj / "project.db-wal";

  {  // sola lettura: project.db e WAL identici byte per byte
    auto store = sqlite_dag_store::open(s.w.proj, open_intent::read_only);
    write_witness witness(s.w.proj);
    const std::string tables_before = dump_all(s.w.proj, true);
    const std::string db_before = fingerprint_file(db), wal_before = fingerprint_file(wal);
    const auto first = store->input_status();
    const auto second = store->input_status();
    witness.expect_no_writes("input_status in sola lettura");
    EXPECT_EQ(state_of(first, s.records[1].id).reason, "input.missing");
    EXPECT_EQ(first.size(), second.size());
    EXPECT_EQ(fingerprint_file(db), db_before);
    EXPECT_EQ(fingerprint_file(wal), wal_before);
    EXPECT_EQ(dump_all(s.w.proj, true), tables_before);
  }
  {  // lettura-scrittura: nessuna riga scritta, nessun byte in più nel WAL
    auto store = sqlite_dag_store::open(s.w.proj, open_intent::read_write);
    write_witness witness(s.w.proj);
    const std::string tables_before = dump_all(s.w.proj, true);
    const std::string db_before = fingerprint_file(db), wal_before = fingerprint_file(wal);
    (void)store->input_status();
    witness.expect_no_writes("input_status in lettura-scrittura");
    EXPECT_EQ(fingerprint_file(db), db_before);
    EXPECT_EQ(fingerprint_file(wal), wal_before);
    EXPECT_EQ(dump_all(s.w.proj, true), tables_before);
    EXPECT_EQ(count_rows(s.w.proj, "input_relinks", true), 0u);
  }
}

struct mode_guard {
  fs::path dir;
  explicit mode_guard(fs::path d) : dir(std::move(d)) {}
  ~mode_guard() { ::chmod(dir.c_str(), 0700); }
};

// T-M3A-07 c: directory padre senza permesso di esecuzione: quell'input è `inaccessible` con errno 13, gli altri hanno il
// loro stato e la funzione non si ferma (RL-02).
TEST(RelinkStatus, T_M3A_07c_unreadable_parent_is_a_state_not_a_stop) {
  if (::geteuid() == 0) GTEST_SKIP() << "il caso richiede un utente non root (EACCES non si produce da root)";
  const world w = make_world("07c");
  const orig locked = make_orig(w.base / "locked" / "a.fits", light_bytes(0));
  const orig present = make_orig(w.base / "b.fits", light_bytes(1));
  const orig gone = make_orig(w.base / "c.fits", light_bytes(2));
  auto store = sqlite_dag_store::create(w.proj);
  const input_record ra = register_input(*store, locked);
  const input_record rb = register_input(*store, present);
  const input_record rc = register_input(*store, gone);
  fs::remove(gone.path);
  mode_guard guard(w.base / "locked");
  ASSERT_EQ(::chmod((w.base / "locked").c_str(), 0600), 0);
  ASSERT_NE(::access(locked.path.c_str(), R_OK), 0) << "la directory deve essere davvero inaccessibile";
  std::vector<input_state> states;
  ASSERT_NO_THROW(states = store->input_status());
  ASSERT_EQ(states.size(), 3u);
  EXPECT_EQ(state_of(states, ra.id).status, "inaccessible");
  EXPECT_EQ(state_of(states, ra.id).reason, "input.stat_failed");
  EXPECT_EQ(state_of(states, ra.id).os_errno.value_or(0), 13);
  EXPECT_EQ(state_of(states, rb.id).status, "present");
  EXPECT_EQ(state_of(states, rc.id).status, "missing");
  EXPECT_EQ(state_of(states, rc.id).reason, "input.missing");
}

content_digest digest_of_orig(const orig& o) { return content_digest{o.sha, o.size}; }

recipe_graph graph_with_file(const content_digest& d, const content_digest* other = nullptr) {
  recipe_graph g;
  g.add("a", st::entry("op.a", 1, {recipe_input{"file", d}}));
  g.add("b", st::entry("op.b", 2, {recipe_input{"in", std::string("a")}}));
  if (other != nullptr) g.add("u", st::entry("op.u", 9, {recipe_input{"file", *other}}));
  return g;
}

// T-M3A-09 e: due copie registrate dello stesso digest; un arco verso un digest mai registrato (RL-03).
TEST(RelinkBlocked, T_M3A_09e_copies_with_same_digest_and_unregistered_digest) {
  const world w = make_world("09e");
  const bytes content = light_bytes(3);
  const orig c1 = make_orig(w.base / "c1.fits", content);
  const orig c2 = make_orig(w.base / "c2.fits", content);
  const orig other = make_orig(w.base / "altro.fits", light_bytes(5));
  const content_digest never{std::string(64, 'e'), 10};
  auto store = sqlite_dag_store::create(w.proj);
  const input_record r1 = register_input(*store, c1);
  const input_record r2 = register_input(*store, c2);
  register_input(*store, other);
  store->apply_recipe(graph_with_file(digest_of_orig(c1), &never).resolve(), {{"sa", "a"}, {"sb", "b"}, {"su", "u"}});

  const auto edge_for = [](const slot_block* b, const std::string& sha) -> const blocked_edge* {
    for (const auto& e : b->edges) {
      if (e.sha256 == sha) return &e;
    }
    return nullptr;
  };

  // Entrambe presenti: arco disponibile, ids ordinati.
  {
    const auto blocks = store->blocked_slots(store->input_status());
    const slot_block* sa = block_of(blocks, "sa");
    ASSERT_NE(sa, nullptr);
    EXPECT_FALSE(sa->blocked);
    ASSERT_NE(edge_for(sa, c1.sha), nullptr);
    EXPECT_TRUE(edge_for(sa, c1.sha)->available);
    EXPECT_EQ(edge_for(sa, c1.sha)->available_ids, (std::vector<std::uint64_t>{r1.id, r2.id}));
  }
  // Una mancante: l'arco resta disponibile con l'id dell'input presente; nessun record cambia.
  const std::string inputs_before = query(w.proj, "SELECT * FROM inputs ORDER BY id", true);
  fs::remove(c1.path);
  {
    const auto blocks = store->blocked_slots(store->input_status());
    for (const char* slot : {"sa", "sb"}) {
      const slot_block* b = block_of(blocks, slot);
      ASSERT_NE(b, nullptr) << slot;
      EXPECT_FALSE(b->blocked) << slot;
      ASSERT_NE(edge_for(b, c1.sha), nullptr) << slot;
      EXPECT_TRUE(edge_for(b, c1.sha)->available) << slot;
      EXPECT_EQ(edge_for(b, c1.sha)->available_ids, (std::vector<std::uint64_t>{r2.id})) << slot;
    }
  }
  EXPECT_EQ(query(w.proj, "SELECT * FROM inputs ORDER BY id", true), inputs_before);
  // Entrambe mancanti: bloccato con i due motivi.
  fs::remove(c2.path);
  {
    const auto blocks = store->blocked_slots(store->input_status());
    for (const char* slot : {"sa", "sb"}) {
      const slot_block* b = block_of(blocks, slot);
      ASSERT_NE(b, nullptr) << slot;
      EXPECT_TRUE(b->blocked) << slot;
      const blocked_edge* e = edge_for(b, c1.sha);
      ASSERT_NE(e, nullptr) << slot;
      EXPECT_FALSE(e->available) << slot;
      std::set<std::string> got;
      for (const auto& x : e->blockers) got.insert(std::to_string(x.input_id) + "/" + x.path + "/" + x.reason);
      EXPECT_EQ(got, (std::set<std::string>{std::to_string(r1.id) + "/" + c1.path.string() + "/input.missing",
                                            std::to_string(r2.id) + "/" + c2.path.string() + "/input.missing"}))
          << slot;
    }
  }
  // Digest mai registrato: bloccato, motivo `input.unregistered`, indipendente dallo stato degli altri input.
  {
    const auto blocks = store->blocked_slots(store->input_status());
    const slot_block* su = block_of(blocks, "su");
    ASSERT_NE(su, nullptr);
    EXPECT_TRUE(su->blocked);
    const blocked_edge* e = edge_for(su, never.sha256);
    ASSERT_NE(e, nullptr);
    EXPECT_FALSE(e->available);
    ASSERT_EQ(e->blockers.size(), 1u);
    EXPECT_EQ(e->blockers.front().reason, "input.unregistered");
    EXPECT_EQ(e->blockers.front().input_id, 0u);
  }
}

}  // namespace
