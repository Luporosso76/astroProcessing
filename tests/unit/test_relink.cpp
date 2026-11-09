// U9 (M3.a), F-IO-08: ricollegamento di un originale (RL-01, RL-04, RL-05, RL-08). Casi: T-M3A-08 a, b, c, d.
// Contratto: docs/contracts/F-IO-08-ricollegamento.md §5, §7, §10. Verità: file e digest del generatore, identità con
// stat, righe con SQL diretto.
#include <sys/stat.h>
#include <unistd.h>

#include "relink_support.hpp"

namespace {

using namespace rl;

constexpr std::size_t default_work = 65536;

struct outcome {
  std::optional<stop_error> stop;
  std::optional<input_record> record;
  int reads = 0;
  std::vector<std::uint32_t> flags;  // flag di apertura del candidato osservati alle letture
  std::vector<std::string> formats;  // formato dichiarato a ogni apertura (punto input.open_format)
};

struct attempt_options {
  std::vector<fs::path> watch;  // file osservati (e le loro directory): invariati dopo la chiamata (T-M3A-08c)
  probe_scope::handler extra;   // si esegue a ogni punto del confine, dopo l'osservazione dei flag
  std::size_t work_size = default_work;
  std::function<void()> cancel = no_cancel();
  std::vector<fs::path> watch_dirs;  // directory osservate da sole (candidato assente)
  const bool* intervened = nullptr;  // vero quando l'handler `extra` ha modificato `touched`
  fs::path touched;                  // file modificato dal test: impronta ripresa (con la sua directory) una volta, subito dopo
};

// Una chiamata di relink_input con osservazione: flag di apertura del candidato (RL-08: O_RDONLY), letture del contenuto,
// invarianza dei file e delle directory indicati.
outcome attempt(sqlite_dag_store& store, std::uint64_t id, const std::string& path, const attempt_options& o = {}) {
  watcher w;
  for (const fs::path& p : o.watch) w.add_file(p);
  for (const fs::path& d : o.watch_dirs) w.add_dir(d);
  outcome out;
  std::vector<std::byte> work(o.work_size);
  {
    probe_scope probe([&](std::string_view point, std::string_view detail) {
      if (point == "input.read") {
        for (const std::uint32_t f : open_flags_of(path)) out.flags.push_back(f);
      }
      if (o.extra) {
        const bool was = o.intervened != nullptr && *o.intervened;
        o.extra(point, detail);
        if (o.intervened != nullptr && *o.intervened && !was) w.rebase_path(o.touched);
      }
    });
    try {
      out.record = store.relink_input(id, path, std::span<std::byte>(work), o.cancel);
    } catch (const stop_error& e) {
      out.stop = e;
    }
    out.reads = probe.reads;
    out.formats = probe.open_formats;
  }
  if (!o.watch.empty() || !o.watch_dirs.empty()) w.expect_unchanged("relink " + path);
  for (const std::uint32_t f : out.flags) EXPECT_EQ(f & O_ACCMODE, static_cast<std::uint32_t>(O_RDONLY)) << path;
  return out;
}

void expect_stop_of(const outcome& o, error_category cat, const std::string& code, const std::string& what) {
  ASSERT_TRUE(o.stop.has_value()) << what << ": stop_error atteso, ricollegamento accettato";
  EXPECT_EQ(o.stop->category(), cat) << what << ": " << o.stop->what();
  EXPECT_EQ(o.stop->code(), code) << what << ": " << o.stop->what();
}

std::string input_row(const fs::path& project, std::uint64_t id) {
  return query(project,
               "SELECT id, path, size_bytes, sha256, role, dev, ino, format FROM inputs WHERE id = " + std::to_string(id), true);
}

std::string stable_columns(const fs::path& project, std::uint64_t id) {
  return query(project, "SELECT id, size_bytes, sha256, role, format FROM inputs WHERE id = " + std::to_string(id), true);
}

std::string position_columns(const fs::path& project, std::uint64_t id) {
  return query(project, "SELECT path, dev, ino FROM inputs WHERE id = " + std::to_string(id), true);
}

std::string relink_rows(const fs::path& project, std::uint64_t id) {
  return query(project,
               "SELECT input_id, revision, old_path, old_dev, old_ino, new_path, new_dev, new_ino, previous_status, "
               "record_version FROM input_relinks WHERE input_id = " + std::to_string(id) + " ORDER BY revision",
               true);
}

std::string relink_line(std::uint64_t id, int revision, const std::string& old_path, std::uint64_t old_dev,
                        std::uint64_t old_ino, const std::string& new_path, const stat_of& now, const std::string& previous) {
  std::ostringstream o;
  o << id << "|" << revision << "|" << old_path << "|" << old_dev << "|" << old_ino << "|" << new_path << "|" << now.dev()
    << "|" << now.ino() << "|" << previous << "|1|\n";
  return o.str();
}

// Tutte le tabelle tranne `inputs` e `input_relinks` (RL-06: il ricollegamento non le tocca).
std::string others(const fs::path& project) { return dump_all(project, true, {"inputs", "input_relinks"}); }

// T-M3A-08 a: ricollegamenti accettati (RL-04, RL-05): FITS spostato, RAW spostato, stesso percorso con file nuovo, input
// presente verso una copia, percorso con symlink seguito da `..`; una seconda revisione sullo stesso input.
TEST(Relink, T_M3A_08a_accepted_relinks_update_only_position_and_append_history) {
  const world w = make_world("08a");
  const orig f1 = make_orig(w.base / "orig" / "f1.fits", light_bytes(0));
  const orig raw = make_orig(w.base / "orig" / "r.dng", dng_bytes(), "light", "raw");
  const orig f4 = make_orig(w.base / "orig" / "f4.fits", light_bytes(2));
  const orig f5 = make_orig(w.base / "orig" / "f5.fits", light_bytes(3));
  const orig f6 = make_orig(w.base / "orig" / "f6.fits", light_bytes(4));
  auto store = sqlite_dag_store::create(w.proj);
  const input_record r1 = register_input(*store, f1), rr = register_input(*store, raw), r4 = register_input(*store, f4),
                     r5 = register_input(*store, f5), r6 = register_input(*store, f6);
  const std::pair<std::uint64_t, std::uint64_t> id1{stat_of(f1.path).dev(), stat_of(f1.path).ino()};
  const std::pair<std::uint64_t, std::uint64_t> idr{stat_of(raw.path).dev(), stat_of(raw.path).ino()};
  const std::pair<std::uint64_t, std::uint64_t> id4{stat_of(f4.path).dev(), stat_of(f4.path).ino()};
  const std::pair<std::uint64_t, std::uint64_t> id5{stat_of(f5.path).dev(), stat_of(f5.path).ino()};
  const std::pair<std::uint64_t, std::uint64_t> id6{stat_of(f6.path).dev(), stat_of(f6.path).ino()};

  // Stati precedenti (07a): 1, raw e 6 spostati o cancellati; 4 sostituito con inode nuovo; 5 presente.
  const fs::path f1_new = w.base / "nuovo" / "f1.fits";
  const fs::path raw_new = w.base / "nuovo" / "r.dng";
  write_new_file(f1_new, f1.content);
  write_new_file(raw_new, raw.content);
  fs::remove(f1.path);
  fs::remove(raw.path);
  replace_with_new_inode(f4.path, f4.content);
  const fs::path f5_copy = w.base / "copie" / "f5.fits";
  write_new_file(f5_copy, f5.content);
  // Percorso con symlink seguito da `..`: base/a/link -> base/b/sub; base/b/f6.fits ha il contenuto di f6; base/a/f6.fits è
  // un altro file.
  fs::create_directories(w.base / "b" / "sub");
  fs::create_directories(w.base / "a");
  fs::create_symlink(w.base / "b" / "sub", w.base / "a" / "link");
  write_new_file(w.base / "b" / "f6.fits", f6.content);
  write_new_file(w.base / "a" / "f6.fits", light_bytes(9));
  const std::string dotdot = (w.base / "a" / "link" / ".." / "f6.fits").string();
  fs::remove(f6.path);
  ASSERT_NE(stat_of(dotdot).ino(), stat_of(w.base / "a" / "f6.fits").ino());

  const std::string others_before = others(w.proj);

  struct step {
    const input_record* rec;
    fs::path candidate;
    std::string previous;
    std::string format;
    std::pair<std::uint64_t, std::uint64_t> old_id;
    std::string old_path;
  };
  const std::vector<step> steps{
      {&r1, f1_new, "input.missing", "fits", id1, f1.path.string()},
      {&rr, raw_new, "input.missing", "raw", idr, raw.path.string()},
      {&r4, f4.path, "input.modified", "fits", id4, f4.path.string()},
      {&r5, f5_copy, "present", "fits", id5, f5.path.string()},
      {&r6, fs::path(dotdot), "input.missing", "fits", id6, f6.path.string()},
  };
  for (const step& s : steps) {
    SCOPED_TRACE(s.candidate.string());
    const std::string stable_before = stable_columns(w.proj, s.rec->id);
    attempt_options opt;
    opt.watch = {s.candidate};
    if (fs::exists(s.old_path)) opt.watch.push_back(s.old_path);
    const outcome out = attempt(*store, s.rec->id, s.candidate.string(), opt);
    ASSERT_FALSE(out.stop.has_value()) << (out.stop ? out.stop->what() : "");
    ASSERT_TRUE(out.record.has_value());
    const stat_of now(s.candidate);
    // Registrato esattamente com'è dato, byte per byte; identità del file verificato.
    EXPECT_EQ(out.record->path, s.candidate.string());
    EXPECT_EQ(out.record->dev, now.dev());
    EXPECT_EQ(out.record->ino, now.ino());
    EXPECT_EQ(position_columns(w.proj, s.rec->id), s.candidate.string() + "|" + std::to_string(now.dev()) + "|" +
                                                        std::to_string(now.ino()) + "|\n");
    EXPECT_EQ(stable_columns(w.proj, s.rec->id), stable_before) << "id, sha256, size_bytes, role, format invariati";
    EXPECT_EQ(relink_rows(w.proj, s.rec->id),
              relink_line(s.rec->id, 1, s.old_path, s.old_id.first, s.old_id.second, s.candidate.string(), now, s.previous));
    EXPECT_FALSE(out.flags.empty()) << "il candidato è stato letto: flag di apertura osservati";
    ASSERT_FALSE(out.formats.empty()) << "nessuna apertura con dichiarazione di formato";
    for (const std::string& f : out.formats) EXPECT_EQ(f, s.format) << "RL-04: il candidato si apre col formato di inputs.format";
  }
  EXPECT_EQ(query(w.proj, "SELECT format FROM inputs WHERE id = " + std::to_string(rr.id), true), "raw|\n");
  EXPECT_EQ(others(w.proj), others_before) << "RL-06: nessuna altra tabella cambia";
  {
    const auto states = store->input_status();
    for (const input_record* r : {&r1, &rr, &r4, &r5, &r6}) {
      EXPECT_EQ(state_of(states, r->id).status, "present") << r->id;
      EXPECT_EQ(state_of(states, r->id).reason, "") << r->id;
    }
  }

  // Seconda revisione: l'input 1 si ricollega a un'altra copia; la storia aggiunge la riga 2 con il percorso della 1.
  const fs::path f1_again = w.base / "ancora" / "f1.fits";
  write_new_file(f1_again, f1.content);
  const stat_of before(f1_new);
  const outcome again = attempt(*store, r1.id, f1_again.string(), attempt_options{{f1_again, f1_new}, {}, default_work, no_cancel(), {}, nullptr, {}});
  ASSERT_FALSE(again.stop.has_value()) << (again.stop ? again.stop->what() : "");
  const stat_of now(f1_again);
  EXPECT_EQ(relink_rows(w.proj, r1.id),
            relink_line(r1.id, 1, f1.path.string(), id1.first, id1.second, f1_new.string(), before, "input.missing") +
                relink_line(r1.id, 2, f1_new.string(), before.dev(), before.ino(), f1_again.string(), now, "present"));
  EXPECT_EQ(others(w.proj), others_before);
}

struct reject_env {
  world w;
  orig x;                  // input registrato, presente
  std::uint64_t id = 0;    // id di x
  std::string inputs_before;
};

reject_env make_reject_env(const std::string& name) {
  reject_env e;
  e.w = make_world(name);
  e.x = make_orig(e.w.base / "orig" / "x.fits", light_bytes(0));
  auto store = sqlite_dag_store::create(e.w.proj);
  e.id = register_input(*store, e.x).id;
  return e;
}

// T-M3A-08 b: rifiuti (RL-04, RL-05). In ogni rifiuto tutte le tabelle restano invariate e il candidato e il file alla
// posizione precedente non cambiano (T-M3A-08c).
// `PRAGMA data_version` di una seconda connessione, aperta prima della chiamata, è invariato: nessuna scrittura (§10, RL-05).
TEST(Relink, T_M3A_08b_rejections_leave_everything_unchanged) {
  const world w = make_world("08b");
  const orig x = make_orig(w.base / "orig" / "x.fits", light_bytes(0));
  auto store = sqlite_dag_store::create(w.proj);
  const std::uint64_t id = register_input(*store, x).id;
  const orig rawx = make_orig(w.base / "orig" / "r.dng", dng_bytes(), "light", "raw");
  const std::uint64_t raw_id = register_input(*store, rawx).id;
  // Un artefatto con lo stesso contenuto dell'input, per il caso del collegamento fisico.
  store->apply_recipe(st::diamond(2).resolve(), st::diamond_slots());
  const artifact_record artifact = st::run_and_commit(*store, "sa", std::string(x.content.begin(), x.content.end()));
  const fs::path artifact_file = store->artifact_path(artifact.rel_path);
  fs::create_directories(w.proj / "cache" / "thumbnails");
  st::write_text(w.proj / "cache" / "thumbnails" / "voce.bin", "contenuto qualsiasi");

  const auto expect_rejected = [&](const std::string& what, const std::string& path, error_category cat,
                                   const std::string& code, attempt_options opt = {}, std::uint64_t input_id = 0,
                                   bool zero_reads = false) -> outcome {
    SCOPED_TRACE(what);
    const std::string tables_before = dump_all(w.proj, true);
    write_witness witness(w.proj);
    opt.watch.push_back(x.path);
    const outcome out = attempt(*store, input_id == 0 ? id : input_id, path, opt);
    expect_stop_of(out, cat, code, what);
    witness.expect_no_writes(what);
    EXPECT_FALSE(out.record.has_value()) << what;
    EXPECT_EQ(dump_all(w.proj, true), tables_before) << what << ": tabelle (SQL) invariate";
    if (zero_reads) {
      EXPECT_EQ(out.reads, 0) << what << ": nessuna lettura del contenuto";
    }
    return out;
  };

  // Candidati a verifica: stessa dimensione con un byte diverso; un byte in più; un byte in meno.
  bytes flipped = x.content;
  flipped[3003] ^= 0x01;
  const fs::path c_flip = w.base / "c" / "flip.fits";
  write_new_file(c_flip, flipped);
  const outcome digest_out = expect_rejected("digest diverso", c_flip.string(), error_category::integrity, "input.digest_mismatch", {{c_flip}, {}, default_work, no_cancel(), {}, nullptr, {}});
  EXPECT_FALSE(digest_out.flags.empty()) << "il candidato è stato letto: flag di apertura osservati";

  bytes longer = x.content;
  longer.push_back(0);
  const fs::path c_plus = w.base / "c" / "plus.fits";
  write_new_file(c_plus, longer);
  expect_rejected("un byte in più", c_plus.string(), error_category::integrity, "input.size_changed", {{c_plus}, {}, default_work, no_cancel(), {}, nullptr, {}}, 0, true);

  bytes shorter = x.content;
  shorter.pop_back();
  const fs::path c_minus = w.base / "c" / "minus.fits";
  write_new_file(c_minus, shorter);
  expect_rejected("un byte in meno", c_minus.string(), error_category::integrity, "input.size_changed", {{c_minus}, {}, default_work, no_cancel(), {}, nullptr, {}}, 0, true);

  {
    attempt_options opt;
    opt.watch_dirs = {w.base / "c"};
    expect_rejected("assente", (w.base / "c" / "assente.fits").string(), error_category::io, "input.missing", opt);
  }

  const fs::path c_dir = w.base / "c" / "dir.fits";
  fs::create_directories(c_dir);
  expect_rejected("directory", c_dir.string(), error_category::invalid_input, "fits.not_regular", {{c_dir}, {}, default_work, no_cancel(), {}, nullptr, {}});
  // RL-04: il formato è quello dell'input; per un input RAW la stessa directory dà `raw.not_regular`.
  expect_rejected("directory per un input RAW", c_dir.string(), error_category::invalid_input, "raw.not_regular", {{c_dir}, {}, default_work, no_cancel(), {}, nullptr, {}}, raw_id);

  expect_rejected("percorso relativo", "c/relativo.fits", error_category::invalid_input, "relink.path_not_absolute");

  // NUL nel percorso: nessun troncamento alla prima occorrenza (il prefisso è un file esistente e valido).
  expect_rejected("percorso con NUL", x.path.string() + std::string(1, '\0') + "coda.fits", error_category::invalid_input,
                  "relink.path_not_absolute", {}, 0, true);

  // Dentro il progetto, direttamente e tramite un symlink esterno che punta dentro.
  const fs::path inside = w.proj / "copia.fits";
  write_new_file(inside, x.content);
  expect_rejected("dentro il progetto", inside.string(), error_category::security, "relink.target_forbidden", {{inside}, {}, default_work, no_cancel(), {}, nullptr, {}}, 0, true);
  const fs::path ext_link = w.base / "c" / "verso_dentro.fits";
  fs::create_directories(ext_link.parent_path());
  fs::create_symlink(inside, ext_link);
  expect_rejected("symlink esterno che punta dentro", ext_link.string(), error_category::security, "relink.target_forbidden", {{inside}, {}, default_work, no_cancel(), {}, nullptr, {}}, 0, true);

  // Collegamenti fisici esterni di file gestiti dal progetto: stop prima di leggere un byte del contenuto.
  const std::vector<std::pair<std::string, fs::path>> managed{
      {"artefatto con lo stesso contenuto dell'input", artifact_file},
      {"project.lock", w.proj / "project.lock"},
      {"project.db", w.proj / "project.db"},
      {"project.db-wal", w.proj / "project.db-wal"},
      {"project.db-shm", w.proj / "project.db-shm"},
      {"voce di cache/thumbnails", w.proj / "cache" / "thumbnails" / "voce.bin"}};
  for (const auto& [what, target] : managed) {
    ASSERT_TRUE(fs::exists(target)) << what;
    const fs::path link = w.base / "hl" / (target.filename().string());
    fs::create_directories(link.parent_path());
    ASSERT_EQ(::link(target.c_str(), link.c_str()), 0) << what;
    // project.db, -wal e -shm cambiano per conto proprio con le letture dello store: si osservano gli altri.
    attempt_options opt;
    if (target.filename().string().rfind("project.db", 0) != 0) opt.watch = {link};
    expect_rejected("collegamento fisico di " + what, link.string(), error_category::security, "fs.original_inode", opt, 0, true);
    ASSERT_EQ(::unlink(link.c_str()), 0) << what;
  }

  {
    attempt_options opt;
    opt.watch = {c_flip};
    expect_rejected("input_id sconosciuto", c_flip.string(), error_category::invalid_input, "relink.unknown_input", opt, 987654);
  }
  // RL-05, ordine dei passi 2-4: l'id sconosciuto precede il controllo del percorso.
  expect_rejected("input_id sconosciuto e percorso relativo", "relativo.fits", error_category::invalid_input, "relink.unknown_input", {}, 987654);

  // Candidato sostituito fra lo stat e la verifica: stessi byte, inode diverso (punto `input.open`).
  {
    const fs::path swap = w.base / "c" / "scambio.fits";
    write_new_file(swap, x.content);
    const std::uint64_t ino_before = stat_of(swap).ino();
    bool done = false;
    attempt_options opt;
    opt.watch = {swap};
    opt.intervened = &done;
    opt.touched = swap;
    opt.extra = [&](std::string_view point, std::string_view) {
      if (point == "input.open" && !done) {
        done = true;
        replace_with_new_inode(swap, x.content);
      }
    };
    const outcome swap_out = expect_rejected("candidato sostituito fra stat e verifica", swap.string(), error_category::integrity, "relink.identity_changed", opt);
    EXPECT_FALSE(swap_out.flags.empty());
    EXPECT_TRUE(done);
    EXPECT_NE(stat_of(swap).ino(), ino_before);
  }

  // File allungato durante la lettura (punto `input.read`).
  {
    const fs::path grow = w.base / "c" / "cresce.fits";
    write_new_file(grow, x.content);
    bool done = false;
    attempt_options opt;
    opt.watch = {grow, x.path};
    opt.intervened = &done;
    opt.touched = grow;
    opt.extra = [&](std::string_view point, std::string_view) {
      if (point == "input.read" && !done) {
        done = true;
        append_byte(grow, 0);
      }
    };
    const std::string tables_before = dump_all(w.proj, true);
    write_witness witness(w.proj);
    const outcome out = attempt(*store, id, grow.string(), opt);
    witness.expect_no_writes("file allungato durante la lettura");
    ASSERT_TRUE(out.stop.has_value());
    EXPECT_EQ(out.stop->category(), error_category::integrity) << out.stop->what();
    EXPECT_TRUE(out.stop->code() == "input.modified" || out.stop->code() == "input.size_changed") << out.stop->code();
    EXPECT_EQ(dump_all(w.proj, true), tables_before);
    EXPECT_TRUE(done);
    EXPECT_FALSE(out.flags.empty());
  }

  // Cancel durante il digest: il file coincide, il buffer è piccolo (molti pezzi), il cancel scatta al terzo controllo.
  {
    const fs::path cand = w.base / "c" / "annulla.fits";
    write_new_file(cand, x.content);
    int calls = 0;
    attempt_options opt;
    opt.work_size = 1024;
    opt.cancel = [&calls] {
      if (++calls == 3) throw stop_error(error_category::cancelled, "sched.cancelled", "annullato dal test");
    };
    opt.watch = {cand};
    const outcome out = expect_rejected("cancel durante il digest", cand.string(), error_category::cancelled, "sched.cancelled", opt);
    EXPECT_EQ(calls, 3);
    EXPECT_GT(out.reads, 0) << "almeno una lettura prima dello stop";
    EXPECT_FALSE(out.flags.empty());
  }

  // add_input: formato vuoto e fuori elenco.
  for (const char* bad : {"", "tiff"}) {
    SCOPED_TRACE(std::string("formato «") + bad + "»");
    const std::string tables_before = dump_all(w.proj, true);
    input_record r;
    r.path = x.path.string();
    r.size_bytes = x.size;
    r.sha256 = x.sha;
    r.role = "light";
    r.format = bad;
    write_witness witness(w.proj);
    EXPECT_STOP(store->add_input(r), error_category::invalid_input, "store.bad_input");
    witness.expect_no_writes("add_input con formato non ammesso");
    EXPECT_EQ(dump_all(w.proj, true), tables_before);
  }

  // x non è mai cambiato e nessuna riga di storia è nata.
  EXPECT_EQ(count_rows(w.proj, "input_relinks", true), 0u);
  EXPECT_EQ(input_row(w.proj, id), std::to_string(id) + "|" + x.path.string() + "|" + std::to_string(x.size) + "|" + x.sha +
                                       "|light|" + std::to_string(stat_of(x.path).dev()) + "|" + std::to_string(stat_of(x.path).ino()) +
                                       "|fits|\n");
}

// T-M3A-08 b (add_input, NUL): un percorso con NUL non si tronca al prefisso esistente: store.bad_input, nessuna scrittura.
TEST(Relink, T_M3A_08b_add_input_rejects_nul_in_path) {
  const world w = make_world("08bnul");
  const orig x = make_orig(w.base / "orig" / "x.fits", light_bytes(0));
  auto store = sqlite_dag_store::create(w.proj);
  const std::string tables_before = dump_all(w.proj, true);
  input_record r;
  r.path = x.path.string() + std::string(1, '\0') + "coda.fits";
  r.size_bytes = x.size;
  r.sha256 = x.sha;
  r.role = "light";
  r.format = "fits";
  write_witness witness(w.proj);
  EXPECT_STOP(store->add_input(r), error_category::invalid_input, "store.bad_input");
  witness.expect_no_writes("add_input con NUL nel percorso");
  EXPECT_EQ(dump_all(w.proj, true), tables_before);
  EXPECT_EQ(count_rows(w.proj, "inputs", true), 0u);
}

// T-M3A-08 b (store in sola lettura): store.read_only prima di ogni altra verifica (RL-05, passo 1).
TEST(Relink, T_M3A_08b_read_only_store_rejects_relink) {
  const reject_env e = make_reject_env("08ro");
  const fs::path moved = e.w.base / "nuovo" / "x.fits";
  write_new_file(moved, e.x.content);
  fs::remove(e.x.path);
  const std::string tables_before = dump_all(e.w.proj);
  {
    auto store = sqlite_dag_store::open(e.w.proj, open_intent::read_only);
    const outcome ok_path = attempt(*store, e.id, moved.string(), attempt_options{{moved}, {}, default_work, no_cancel(), {}, nullptr, {}});
    expect_stop_of(ok_path, error_category::schema, "store.read_only", "percorso valido");
    EXPECT_EQ(ok_path.reads, 0);
    // Il passo 1 precede la verifica dell'id e del percorso.
    expect_stop_of(attempt(*store, 987654, "relativo.fits"), error_category::schema, "store.read_only", "id e percorso non validi");
  }
  EXPECT_EQ(dump_all(e.w.proj), tables_before);
}

// T-M3A-08 c (RL-08): la sola storia del ricollegamento accettato non lascia scritture sugli originali; il controllo vale
// per ogni caso di 08a e 08b (invarianza di file, directory e flag O_RDONLY, in `attempt`). Qui il caso a sé: il file alla
// posizione precedente, se esiste (input `present` ricollegato a una copia), resta identico.
TEST(Relink, T_M3A_08c_previous_location_and_candidate_untouched_on_success) {
  const world w = make_world("08c");
  const orig x = make_orig(w.base / "orig" / "x.fits", light_bytes(0));
  auto store = sqlite_dag_store::create(w.proj);
  const std::uint64_t id = register_input(*store, x).id;
  const fs::path copy = w.base / "copia" / "x.fits";
  write_new_file(copy, x.content);
  watcher both;
  both.add_file(x.path);
  both.add_file(copy);
  const outcome out = attempt(*store, id, copy.string(), attempt_options{{x.path, copy}, {}, default_work, no_cancel(), {}, nullptr, {}});
  ASSERT_FALSE(out.stop.has_value()) << (out.stop ? out.stop->what() : "");
  both.expect_unchanged("posizione precedente e candidato");
  EXPECT_FALSE(out.flags.empty());
  for (const std::uint32_t f : out.flags) EXPECT_EQ(f & O_ACCMODE, static_cast<std::uint32_t>(O_RDONLY));
  // Nessun file nuovo fuori dal progetto.
  EXPECT_EQ(st::list_dir(w.base), (std::vector<std::string>{"copia", "orig"}));
  EXPECT_EQ(st::list_dir(w.base / "copia"), (std::vector<std::string>{"x.fits"}));
  EXPECT_EQ(st::list_dir(w.base / "orig"), (std::vector<std::string>{"x.fits"}));
}

struct kill_project {
  world w;
  orig x;
  std::uint64_t id = 0;
  fs::path moved;
};

kill_project make_kill_project(const std::string& name) {
  kill_project p;
  p.w = make_world(name);
  p.x = make_orig(p.w.base / "orig" / "x.fits", light_bytes(0));
  p.moved = p.w.base / "nuovo" / "x.fits";
  {
    auto store = sqlite_dag_store::create(p.w.proj);
    p.id = register_input(*store, p.x).id;
  }
  write_new_file(p.moved, p.x.content);
  fs::remove(p.x.path);
  return p;
}

void relink_in_child(const kill_project& p, const fault_injector& injector) {
  open_options options;
  options.fault = injector;
  auto store = sqlite_dag_store::open(p.w.proj, open_intent::read_write, options);
  std::vector<std::byte> work(default_work);
  (void)store->relink_input(p.id, p.moved.string(), std::span<std::byte>(work), no_cancel());
}

// T-M3A-08 d: guasto al punto `relink.commit`, SIGKILL del figlio ai punti `relink.inserted`, `relink.before_commit` e
// `relink.after_commit`, trigger SQL (RL-01, RL-05, RL-06). Un kill dentro l'esecuzione di COMMIT non è raggiungibile da un
// punto del prodotto: lì vale l'atomicità di SQLite in WAL (limite dichiarato nel contratto, §10).
TEST(Relink, T_M3A_08d_fault_at_commit_leaves_previous_record_and_history) {
  const kill_project p = make_kill_project("08dfault");
  const std::string before = dump_all(p.w.proj);
  {
    auto store = sqlite_dag_store::open(p.w.proj, open_intent::read_write);
    store->set_fault_injector([](std::string_view point) { return point == "relink.commit" ? EIO : 0; });
    std::vector<std::byte> work(default_work);
    try {
      (void)store->relink_input(p.id, p.moved.string(), std::span<std::byte>(work), no_cancel());
      ADD_FAILURE() << "stop_error atteso al punto relink.commit";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.category(), error_category::io) << e.what();
    }
    EXPECT_EQ(dump_all(p.w.proj, true), before) << "record e storia precedenti";
  }
  EXPECT_EQ(dump_all(p.w.proj), before);
  EXPECT_EQ(count_rows(p.w.proj, "input_relinks"), 0u);
}

TEST(Relink, T_M3A_08d_sigkill_leaves_old_or_new_never_one_alone) {
  // Punti toccati dal ricollegamento (in un progetto di prova, che poi si butta).
  const kill_project probe = make_kill_project("08dkp");
  const std::vector<std::string> points = st::record_points([&](const fault_injector& inj) { relink_in_child(probe, inj); });
  const auto index_of = [&](const char* name) {
    return static_cast<std::size_t>(std::find(points.begin(), points.end(), name) - points.begin());
  };
  const std::size_t inserted = index_of("relink.inserted"), before_commit = index_of("relink.before_commit"),
                    after_commit = index_of("relink.after_commit");
  ASSERT_LT(inserted, points.size()) << "punto non toccato: relink.inserted";
  ASSERT_LT(before_commit, points.size()) << "punto non toccato: relink.before_commit";
  ASSERT_LT(after_commit, points.size()) << "punto non toccato: relink.after_commit";
  EXPECT_LT(inserted, before_commit);
  EXPECT_LT(before_commit, after_commit);

  // Kill alla chiamata k (1-based) dell'iniettore = al punto points[k - 1], prima della sua azione.
  for (const std::size_t at : {inserted, before_commit, after_commit}) {
    const std::size_t k = at + 1;
    SCOPED_TRACE("kill al punto " + points[at]);
    const kill_project p = make_kill_project("08dk" + std::to_string(k));
    const auto child = st::run_child([&](const fault_injector& inj) { relink_in_child(p, inj); }, static_cast<int>(k));
    EXPECT_TRUE(child.killed);
    {
      auto store = sqlite_dag_store::open(p.w.proj, open_intent::read_write);
      EXPECT_EQ(query(p.w.proj, "PRAGMA quick_check", true), "ok|\n");
      const bool moved_now = query(p.w.proj, "SELECT path FROM inputs WHERE id = " + std::to_string(p.id), true) ==
                             p.moved.string() + "|\n";
      const std::size_t history = count_rows(p.w.proj, "input_relinks", true);
      EXPECT_EQ(moved_now, history == 1u) << "record e storia precedenti o entrambi nuovi, mai uno solo";
      EXPECT_LE(history, 1u);
      // Fino a before_commit resta lo stato precedente; dopo il COMMIT (after_commit) entrambi sono nuovi.
      const bool committed = at == after_commit;
      EXPECT_EQ(moved_now, committed);
      EXPECT_EQ(history, committed ? 1u : 0u);
    }
  }
}

TEST(Relink, T_M3A_08d_sql_triggers_abort_forbidden_updates_and_deletes) {
  const world w = make_world("08dsql");
  const orig a = make_orig(w.base / "orig" / "a.fits", light_bytes(0));
  const orig b = make_orig(w.base / "orig" / "b.fits", light_bytes(1));
  std::uint64_t ida = 0, idb = 0;
  {
    auto store = sqlite_dag_store::create(w.proj);
    ida = register_input(*store, a).id;
    idb = register_input(*store, b).id;
    const fs::path moved = w.base / "nuovo" / "a.fits";
    write_new_file(moved, a.content);
    fs::remove(a.path);
    const outcome out = attempt(*store, ida, moved.string());
    ASSERT_FALSE(out.stop.has_value()) << (out.stop ? out.stop->what() : "");
  }
  ASSERT_EQ(count_rows(w.proj, "input_relinks"), 1u);
  const std::string before = dump_all(w.proj);
  {
    st::raw_db db(w.proj);
    sqlite3_extended_result_codes(db.db, 1);
    const std::string ia = std::to_string(ida), ib = std::to_string(idb);
    const std::vector<std::string> forbidden{
        "UPDATE inputs SET sha256 = '" + std::string(64, '0') + "' WHERE id = " + ia,
        "UPDATE inputs SET size_bytes = size_bytes + 1 WHERE id = " + ia,
        "UPDATE inputs SET role = 'dark' WHERE id = " + ia,
        "UPDATE inputs SET format = 'raw' WHERE id = " + ia,
        "UPDATE inputs SET id = 9999 WHERE id = " + ia,
        "UPDATE inputs SET ino = ino + 1 WHERE id = " + ia,
        "UPDATE inputs SET dev = dev + 1 WHERE id = " + ia,
        "UPDATE inputs SET path = '/senza/storia.fits' WHERE id = " + ib,
        "UPDATE inputs SET path = '/diverso/dall/ultimo.fits' WHERE id = " + ia,
        "DELETE FROM inputs WHERE id = " + ia,
        "DELETE FROM inputs WHERE id = " + ib,
        "UPDATE input_relinks SET old_path = '/x' WHERE input_id = " + ia,
        "UPDATE input_relinks SET previous_status = 'present' WHERE input_id = " + ia,
        "DELETE FROM input_relinks WHERE input_id = " + ia,
    };
    for (const std::string& sql : forbidden) {
      EXPECT_EQ(db.exec(sql), SQLITE_CONSTRAINT_TRIGGER) << "abortita dal trigger (RAISE(ABORT)): " << sql;
    }
  }
  EXPECT_EQ(dump_all(w.proj), before) << "nessuna istruzione ha cambiato lo stato";
}

}  // namespace
