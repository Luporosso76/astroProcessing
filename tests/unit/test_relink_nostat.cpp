// U9 (M3.a), F-IO-08: osservazione delle chiamate di sistema di input_status e blocked_slots (RL-02, RL-03, RL-08).
// Casi: T-M3A-09 f (blocked_slots su stati costruiti a mano, zero stat) e la parte «nessuna ricerca» di T-M3A-07 (un
// solo stat per input, nessuna apertura né listing di directory). Eseguibile a parte: fs_probe.hpp interpone stat e open.
#include "fs_probe.hpp"
#include "relink_support.hpp"

namespace {

using namespace rl;

std::size_t count_kind(const std::vector<std::string>& events, const std::string& kind, const std::string& under = "") {
  std::size_t n = 0;
  for (const std::string& e : events) {
    if (e.rfind(kind + " ", 0) != 0) continue;
    if (under.empty() || e.find(under) != std::string::npos) ++n;
  }
  return n;
}

// T-M3A-07 (RL-02): un solo stat sul percorso registrato di ogni input; nessuna apertura di file, nessun listing.
TEST(RelinkNoStat, T_M3A_07_status_is_one_stat_per_registered_path_and_no_search) {
  const world w = make_world("07_nostat");
  std::vector<orig> origs;
  std::vector<input_record> recs;
  auto store = sqlite_dag_store::create(w.proj);
  for (std::size_t k = 0; k < 4; ++k) {
    origs.push_back(make_orig(w.base / "orig" / ("l" + std::to_string(k) + ".fits"), light_bytes(k)));
    recs.push_back(register_input(*store, origs.back()));
  }
  fs::remove(origs[1].path);  // esca: stesso nome e stesso contenuto in una directory sorella
  write_bytes(w.base / "sorella" / "l1.fits", origs[1].content);
  fs::rename(origs[2].path, w.base / "orig" / "l2_rinominato.fits");

  std::vector<std::string> events;
  std::vector<input_state> states;
  {
    fs_probe::window window;
    states = store->input_status();
    events = window.seen();
  }
  ASSERT_EQ(states.size(), 4u);
  const std::string under = w.base.string();
  EXPECT_EQ(count_kind(events, "open", under), 0u) << "nessun file degli originali si apre";
  EXPECT_EQ(count_kind(events, "opendir"), 0u) << "nessuna directory si elenca";
  std::map<std::string, std::size_t> stats;
  for (const std::string& e : events) {
    if (e.rfind("stat ", 0) == 0 && e.find(under) != std::string::npos) ++stats[e.substr(5)];
  }
  std::map<std::string, std::size_t> want;
  for (const orig& o : origs) want[o.path.string()] = 1;
  EXPECT_EQ(stats, want) << "un solo stat per ogni percorso registrato, e solo su quelli";
}

struct level_project {
  world w;
  std::vector<orig> files;  // F1, F2, F3
  std::vector<input_record> recs;
  std::unique_ptr<sqlite_dag_store> store;
};

recipe_graph three_levels(const std::vector<orig>& f, bool with_extra) {
  recipe_graph g;
  g.add("n1", st::entry("op.a", 1, {recipe_input{"file", content_digest{f[0].sha, f[0].size}}}));
  g.add("n2", st::entry("op.b", 2, {recipe_input{"in", std::string("n1")}, recipe_input{"file", content_digest{f[1].sha, f[1].size}}}));
  g.add("n3", st::entry("op.c", 3, {recipe_input{"in", std::string("n2")}}));
  if (with_extra) g.add("x", st::entry("op.x", 4, {recipe_input{"file", content_digest{f[2].sha, f[2].size}}}));
  return g;
}

// T-M3A-09 f: catena di tre livelli con uno slot `detached`, stati scritti a mano, zero stat (RL-03).
TEST(RelinkNoStat, T_M3A_09f_blocked_slots_is_pure_over_hand_built_states) {
  level_project p;
  p.w = make_world("09f");
  for (std::size_t k = 0; k < 3; ++k) {
    p.files.push_back(make_orig(p.w.base / "orig" / ("f" + std::to_string(k) + ".fits"), light_bytes(k + 10)));
  }
  p.store = sqlite_dag_store::create(p.w.proj);
  for (const orig& o : p.files) p.recs.push_back(register_input(*p.store, o));
  p.store->apply_recipe(three_levels(p.files, false).resolve(), {{"s1", "n1"}, {"s2", "n2"}, {"s3", "n3"}});
  p.store->apply_recipe(three_levels(p.files, true).resolve(), {{"s1", "n1"}, {"s2", "n2"}, {"s3", "n3"}, {"sx", "x"}});
  p.store->undo();  // sx esce dalla ricetta corrente: detached
  ASSERT_TRUE(p.store->get_slot("sx").has_value());
  ASSERT_TRUE(p.store->get_slot("sx")->detached);

  // Stati scritti a mano, senza toccare il filesystem: F1 presente, F2 mancante, F3 cambiato (esaminato solo se `sx` lo
  // fosse).
  const auto make_state = [&](std::size_t k, const std::string& status, const std::string& reason) {
    input_state s;
    s.input_id = p.recs[k].id;
    s.path = p.files[k].path.string();
    s.sha256 = p.files[k].sha;
    s.size_bytes = p.files[k].size;
    s.dev = p.recs[k].dev;
    s.ino = p.recs[k].ino;
    s.status = status;
    s.reason = reason;
    return s;
  };
  const std::vector<input_state> states{make_state(0, "present", ""), make_state(1, "missing", "input.missing"),
                                        make_state(2, "changed", "input.modified")};

  std::vector<slot_block> blocks;
  std::vector<std::string> events;
  {
    fs_probe::window window;
    blocks = p.store->blocked_slots(states);
    events = window.seen();
  }
  // Conta ogni stat del processo nella finestra, anche quelli interni di SQLite sul progetto: un rosso causato da
  // project.db* è un limite di questo osservatore, non una violazione di RL-03 (la finestra non apre lo store).
  EXPECT_EQ(count_kind(events, "stat"), 0u) << "blocked_slots non chiama stat";
  EXPECT_EQ(count_kind(events, "open", p.w.base.string()), 0u);

  // Tabella attesa, scritta a mano: archi in ordine di digest e dimensione; slot in ordine di nome; `sx` assente.
  std::vector<std::pair<std::string, std::string>> edges1{{p.files[0].sha, "avail"}};
  std::vector<std::pair<std::string, std::string>> edges2{{p.files[0].sha, "avail"}, {p.files[1].sha, "blocked"}};
  const auto sort_edges = [](std::vector<std::pair<std::string, std::string>>& e) { std::sort(e.begin(), e.end()); };
  sort_edges(edges1);
  sort_edges(edges2);
  const auto render = [&](const std::string& slot, const std::vector<std::pair<std::string, std::string>>& edges, bool blocked) {
    std::ostringstream o;
    o << slot << " blocked=" << blocked << "\n";
    for (const auto& [sha, kind] : edges) {
      const std::size_t k = sha == p.files[0].sha ? 0 : 1;
      o << "  " << sha << ":" << p.files[k].size << " available=" << (kind == "avail") << " ids=";
      if (kind == "avail") o << p.recs[k].id << ",";
      o << " blockers=";
      if (kind != "avail") o << p.recs[k].id << "/" << p.files[k].path.string() << "/input.missing,";
      o << "\n";
    }
    return o.str();
  };
  EXPECT_EQ(blocks_shape(blocks), render("s1", edges1, false) + render("s2", edges2, true) + render("s3", edges2, true));
  EXPECT_EQ(block_of(blocks, "sx"), nullptr) << "gli slot detached non si esaminano";
  for (const auto& b : blocks) EXPECT_FALSE(b.node_key.empty()) << b.slot;

  // JSON canonico astro.input_status.v1: forma e ordine dei campi non sono ancora fissati dal contratto, si controlla
  // solo il contenuto (limite dichiarato): contiene il formato, le voci di RL-02 e i blocchi.
  const std::string json = input_status_canonical(states, blocks);
  EXPECT_NE(json.find("astro.input_status.v1"), std::string::npos);
  EXPECT_NE(json.find("input.missing"), std::string::npos);
  EXPECT_NE(json.find(p.files[1].path.string()), std::string::npos);
}

}  // namespace
