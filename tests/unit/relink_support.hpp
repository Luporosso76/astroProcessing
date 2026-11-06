#pragma once

// Supporto dei test di U9 (F-IO-08, docs/contracts/F-IO-08-ricollegamento.md). Verità di riferimento indipendenti dal
// prodotto: file e digest scritti dal generatore (SHA-256 con OpenSSL, mai con astro::sha256_hex), identità e permessi
// letti con stat, tabelle lette con SQL diretto.
//
// Simboli di prodotto che il contratto dichiara e che oggi non esistono (nomi e forme sono proposte dell'autore dei
// test, RL-01…RL-07; il proprietario del codice li adotta o li rinomina qui, in un solo punto):
//
//   input_record::format                       std::string, «fits» o «raw» (RL-01)
//   struct input_state { input_id, path, sha256, size_bytes, dev, ino, status, reason, os_errno, found_dev, found_ino }
//   std::vector<input_state> sqlite_dag_store::input_status() const                                   (RL-02)
//   struct blocked_input { input_id, path, reason }
//   struct blocked_edge  { sha256, size_bytes, available, available_ids, blockers }
//   struct slot_block    { slot, node_key, blocked, edges }
//   std::vector<slot_block> sqlite_dag_store::blocked_slots(const std::vector<input_state>&) const    (RL-03)
//   std::string input_status_canonical(const std::vector<input_state>&, const std::vector<slot_block>&)
//   input_record sqlite_dag_store::relink_input(std::uint64_t input_id, const std::string& path,
//                                               std::span<std::byte> work, const std::function<void()>& check_cancel)
//                                                                                                     (RL-05)
//   std::string input_status_canonical(...) è dichiarata in astro/sqlite_dag_store.hpp (RL-02)
//   punti del fault_injector dello store (§10): «relink.commit» (guasto), «relink.inserted» (dopo INSERT, prima di UPDATE),
//   «relink.before_commit» (prima del COMMIT), «relink.after_commit» (dopo il COMMIT)

#include <fcntl.h>
#include <openssl/evp.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <set>

#include "astro/calibration.hpp"
#include "astro/import_job.hpp"
#include "astro/job_probe.hpp"
#include "astro/memory_pool.hpp"
#include "astro/workflow.hpp"
#include "calib_support.hpp"
#include "store_test_support.hpp"
#include "syn_dng.hpp"

namespace rl {

namespace st = store_test;
namespace fx = astro_fixture;
using namespace astro;
namespace fs = std::filesystem;
using bytes = std::vector<std::uint8_t>;

// ---------------------------------------------------------------------------------------------- verità indipendenti

inline std::string sha256_ref(const bytes& data) {
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int n = 0;
  const bool ok = ctx != nullptr && EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1 &&
                  EVP_DigestUpdate(ctx, data.data(), data.size()) == 1 && EVP_DigestFinal_ex(ctx, md, &n) == 1;
  EVP_MD_CTX_free(ctx);
  if (!ok) throw std::runtime_error("OpenSSL: SHA-256 fallito");
  static const char* hex = "0123456789abcdef";
  std::string out;
  for (unsigned int i = 0; i < n; ++i) {
    out += hex[md[i] >> 4];
    out += hex[md[i] & 15];
  }
  return out;
}

inline bytes read_bytes(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return bytes(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

inline void write_bytes(const fs::path& p, const bytes& b) {
  fs::create_directories(p.parent_path());
  std::ofstream out(p, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
  if (!out) throw std::runtime_error("scrittura fallita: " + p.string());
}

// Sostituisce `p` con un file nuovo (stesso contenuto o altro): scrive accanto e rinomina, quindi inode nuovo.
inline void replace_with_new_inode(const fs::path& p, const bytes& b) {
  const fs::path tmp = p.parent_path() / (p.filename().string() + ".nuovo");
  write_bytes(tmp, b);
  fs::rename(tmp, p);
}

inline void append_byte(const fs::path& p, std::uint8_t v) {
  std::ofstream out(p, std::ios::binary | std::ios::app);
  out.put(static_cast<char>(v));
  out.flush();
  if (!out) throw std::runtime_error("append fallito: " + p.string());
}

inline void overwrite_byte(const fs::path& p, std::uint64_t offset, std::uint8_t v) {
  std::fstream f(p, std::ios::binary | std::ios::in | std::ios::out);
  f.seekp(static_cast<std::streamoff>(offset));
  f.put(static_cast<char>(v));
  f.flush();
  if (!f) throw std::runtime_error("scrittura di un byte fallita: " + p.string());
}

struct stat_of {
  struct stat s {};
  explicit stat_of(const fs::path& p) {
    if (::stat(p.c_str(), &s) != 0) throw std::runtime_error("stat fallita: " + p.string());
  }
  std::uint64_t dev() const { return static_cast<std::uint64_t>(s.st_dev); }
  std::uint64_t ino() const { return static_cast<std::uint64_t>(s.st_ino); }
};

// Impronta di un file: esistenza, tipo, identità, nlink, dimensione, permessi, mtime, ctime e digest del contenuto.
inline std::string fingerprint_file(const fs::path& p) {
  struct stat s {};
  if (::stat(p.c_str(), &s) != 0) return "assente";
  std::ostringstream o;
  o << "dev=" << s.st_dev << " ino=" << s.st_ino << " nlink=" << s.st_nlink << " size=" << s.st_size << " mode=" << std::oct
    << s.st_mode << std::dec << " mtime=" << s.st_mtim.tv_sec << "." << s.st_mtim.tv_nsec << " ctime=" << s.st_ctim.tv_sec
    << "." << s.st_ctim.tv_nsec;
  if (S_ISREG(s.st_mode)) o << " sha=" << sha256_ref(read_bytes(p));
  return o.str();
}

// Impronta di una directory: elenco dei nomi, mtime e ctime.
inline std::string fingerprint_dir(const fs::path& d) {
  struct stat s {};
  if (::stat(d.c_str(), &s) != 0) return "assente";
  std::ostringstream o;
  o << "mtime=" << s.st_mtim.tv_sec << "." << s.st_mtim.tv_nsec << " ctime=" << s.st_ctim.tv_sec << "." << s.st_ctim.tv_nsec;
  for (const std::string& n : st::list_dir(d)) o << " " << n;
  return o.str();
}

// Osserva file e directory attorno a una chiamata: `unchanged()` confronta con le impronte prese alla costruzione.
class watcher {
 public:
  void add_file(const fs::path& p) {
    files_[p] = fingerprint_file(p);
    if (p.has_parent_path()) dirs_[p.parent_path()] = fingerprint_dir(p.parent_path());
  }
  void add_dir(const fs::path& d) { dirs_[d] = fingerprint_dir(d); }
  // Riprende le impronte (dopo un intervento del test stesso).
  void rebase_path(const fs::path& p) {
    if (files_.count(p) == 0 || (p.has_parent_path() && dirs_.count(p.parent_path()) == 0)) {
      ADD_FAILURE() << "rebase_path: " << p << " non è fra i file osservati";
      return;
    }
    files_.at(p) = fingerprint_file(p);
    if (p.has_parent_path()) dirs_.at(p.parent_path()) = fingerprint_dir(p.parent_path());
  }
  void rebase() {
    for (auto& [p, f] : files_) f = fingerprint_file(p);
    for (auto& [d, f] : dirs_) f = fingerprint_dir(d);
  }
  void expect_unchanged(const std::string& what) const {
    for (const auto& [p, f] : files_) EXPECT_EQ(fingerprint_file(p), f) << what << ": file " << p;
    for (const auto& [d, f] : dirs_) EXPECT_EQ(fingerprint_dir(d), f) << what << ": directory " << d;
  }

 private:
  std::map<fs::path, std::string> files_;
  std::map<fs::path, std::string> dirs_;
};

// ---------------------------------------------------------------------------------------------- stato del database

// Assenza di scritture: `PRAGMA data_version` di una seconda connessione, aperta prima della chiamata, non cambia se
// nessun'altra connessione conclude una transazione (una transazione annullata non lo cambia).
class write_witness {
 public:
  explicit write_witness(const fs::path& project) : db_(project, false), before_(version()) {}
  void expect_no_writes(const std::string& what) { EXPECT_EQ(version(), before_) << what << ": data_version cambiato"; }

 private:
  std::string version() { return db_.query("PRAGMA data_version"); }
  st::raw_db db_;
  std::string before_;
};

inline std::vector<std::string> table_names(st::raw_db& db) {
  std::vector<std::string> out;
  const std::string text = db.query("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name");
  std::istringstream in(text);
  for (std::string line; std::getline(in, line);) out.push_back(line.substr(0, line.size() - 1));
  return out;
}

// Tutte le righe di tutte le tabelle, in ordine di rowid. Su un progetto chiuso (`live` falso) la lettura è immutabile;
// su uno store aperto si legge con una seconda connessione, che vede anche il WAL.
inline std::string dump_all(const fs::path& project, bool live = false, const std::set<std::string>& skip = {}) {
  st::raw_db db(project, !live);
  std::string out;
  for (const std::string& t : table_names(db)) {
    if (skip.count(t) != 0) continue;
    out += "## " + t + "\n" + db.query("SELECT * FROM " + t + " ORDER BY rowid") + "\n";
  }
  return out;
}

// Un sottoinsieme delle tabelle, per nome.
inline std::string dump_tables(const fs::path& project, const std::vector<std::string>& tables, bool live = false) {
  st::raw_db db(project, !live);
  std::string out;
  for (const std::string& t : tables) out += "## " + t + "\n" + db.query("SELECT * FROM " + t + " ORDER BY rowid") + "\n";
  return out;
}

inline std::string query(const fs::path& project, const std::string& sql, bool live = false) {
  st::raw_db db(project, !live);
  return db.query(sql);
}

// Elenco di `artifacts/` con il digest di ogni file.
inline std::string artifacts_tree(const fs::path& project) {
  std::string out;
  const fs::path dir = project / "artifacts";
  if (!fs::exists(dir)) return out;
  for (const std::string& n : st::list_dir(dir)) {
    const fs::path p = dir / n;
    out += n + " " + (fs::is_regular_file(p) ? sha256_ref(read_bytes(p)) : std::string("<dir>")) + "\n";
  }
  return out;
}

inline std::string file_bytes_text(const fs::path& p) { return st::read_text(p); }

// ---------------------------------------------------------------------------------------------- originali

struct orig {
  fs::path path;
  bytes content;
  std::string sha;  // dal generatore
  std::uint64_t size = 0;
  std::string role = "light";
  std::string format = "fits";
};

inline orig make_orig(const fs::path& path, const bytes& content, const std::string& role = "light",
                      const std::string& format = "fits") {
  write_bytes(path, content);
  return orig{path, content, sha256_ref(content), content.size(), role, format};
}

// Light k-esimo del modello FIX-SYN-FITS-01 (16x12, f32, header di calibrazione completi).
inline bytes light_bytes(std::size_t k) {
  cal::set_def s = cal::synth_set(16, 12, -32);
  for (std::size_t i = 0; i < s.light.v.size(); ++i) {
    s.light.v[i] = s.dark.v[i] + (s.light.v[i] - s.dark.v[i]) * (1.0 + 0.25 * static_cast<double>(k));
  }
  return cal::file_of(s.light, s.w, s.h);
}

// FIX-SYN-DNG-01: DNG non compresso 64x48.
inline bytes dng_bytes() {
  fx::dng_spec s;
  s.width = 64;
  s.height = 48;
  s.white = 4000;
  std::vector<std::uint16_t> v(static_cast<std::size_t>(64) * 48);
  for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<std::uint16_t>((i * 7) % 4000);
  s.pixels = fx::pixels_u16(v);
  return fx::make_dng(s);
}

inline input_record register_input(sqlite_dag_store& store, const orig& o) {
  input_record r;
  r.path = o.path.string();
  r.size_bytes = o.size;
  r.sha256 = o.sha;
  r.role = o.role;
  r.format = o.format;
  return store.add_input(r);
}

// ---------------------------------------------------------------------------------------------- stati e blocchi

inline const input_state& state_of(const std::vector<input_state>& states, std::uint64_t input_id) {
  for (const auto& s : states) {
    if (s.input_id == input_id) return s;
  }
  throw std::runtime_error("stato dell'input assente: " + std::to_string(input_id));
}

inline const slot_block* block_of(const std::vector<slot_block>& blocks, const std::string& slot) {
  for (const auto& b : blocks) {
    if (b.slot == slot) return &b;
  }
  return nullptr;
}

inline std::set<std::string> blocked_names(const std::vector<slot_block>& blocks) {
  std::set<std::string> out;
  for (const auto& b : blocks) {
    if (b.blocked) out.insert(b.slot);
  }
  return out;
}

// Proiezione di blocked_slots senza node_key (che cambia con undo e redo): slot, blocco, archi con esito e motivi.
inline std::string blocks_shape(const std::vector<slot_block>& blocks) {
  std::ostringstream o;
  for (const auto& b : blocks) {
    o << b.slot << " blocked=" << b.blocked << "\n";
    for (const auto& e : b.edges) {
      o << "  " << e.sha256 << ":" << e.size_bytes << " available=" << e.available << " ids=";
      for (const auto id : e.available_ids) o << id << ",";
      o << " blockers=";
      for (const auto& x : e.blockers) o << x.input_id << "/" << x.path << "/" << x.reason << ",";
      o << "\n";
    }
  }
  return o.str();
}

// ---------------------------------------------------------------------------------------------- osservatore dei job

// Imposta job_probe per la durata dello scope; `handler` è chiamato per ogni punto del confine.
class probe_scope {
 public:
  using handler = std::function<void(std::string_view point, std::string_view detail)>;
  explicit probe_scope(handler h) {
    job_probe::set_observer([h = std::move(h), this](std::string_view point, std::string_view detail) {
      if (point == "input.read") ++reads;
      if (point == "input.open") opens.emplace_back(detail);
      if (point == "input.open_format") open_formats.emplace_back(detail.substr(0, detail.find('\t')));
      if (h) h(point, detail);
    });
  }
  ~probe_scope() { job_probe::set_observer({}); }
  probe_scope(const probe_scope&) = delete;
  probe_scope& operator=(const probe_scope&) = delete;
  int reads = 0;
  std::vector<std::string> opens;
  std::vector<std::string> open_formats;  // prefisso del formato dichiarato a ogni apertura (punto input.open_format)
};

// Flag di apertura (da /proc/self/fdinfo) dei descrittori aperti sul file `path`.
inline std::vector<std::uint32_t> open_flags_of(const fs::path& path) {
  std::vector<std::uint32_t> flags;
  const std::string want = fs::weakly_canonical(path).string();
  for (const auto& e : fs::directory_iterator("/proc/self/fd")) {
    std::error_code ec;
    const fs::path target = fs::read_symlink(e.path(), ec);
    if (ec || target.string() != want) continue;
    std::ifstream in("/proc/self/fdinfo/" + e.path().filename().string());
    for (std::string line; std::getline(in, line);) {
      if (line.rfind("flags:", 0) == 0) flags.push_back(static_cast<std::uint32_t>(std::stoul(line.substr(6), nullptr, 8)));
    }
  }
  return flags;
}

// ---------------------------------------------------------------------------------------------- workflow di calibrazione

struct chain {
  fs::path dir;
  std::vector<orig> lights;     // light_0..light_{K-1}
  std::vector<orig> masters;    // dark, flat, dark_flat
  std::vector<calibration_recipe> recipes;
};

inline std::string import_slot(std::size_t k) { return "imp" + std::to_string(k); }
inline std::string cal_slot(std::size_t k) { return "cal" + std::to_string(k); }

// K light + tre master. Le ricette puntano ai file `lights[k]`, `masters`; i digest delle ricette sono quelli del prodotto
// (cal::recipe_of), controllati qui contro quelli del generatore.
inline chain make_chain(const fs::path& dir, std::size_t k_count) {
  chain c;
  c.dir = dir;
  fs::create_directories(dir);
  const cal::set_def base = cal::synth_set(16, 12, -32);
  const std::pair<const char*, const cal::frame_def*> masters[3] = {
      {"dark", &base.dark}, {"flat", &base.flat}, {"dark_flat", &base.dark_flat}};
  for (const auto& [role, def] : masters) {
    c.masters.push_back(make_orig(dir / (std::string(role) + ".fits"), cal::file_of(*def, base.w, base.h), role));
  }
  for (std::size_t k = 0; k < k_count; ++k) {
    c.lights.push_back(make_orig(dir / ("light_" + std::to_string(k) + ".fits"), light_bytes(k), "light"));
    cal::set_def s = base;
    for (std::size_t i = 0; i < s.light.v.size(); ++i) {
      s.light.v[i] = base.dark.v[i] + (base.light.v[i] - base.dark.v[i]) * (1.0 + 0.25 * static_cast<double>(k));
    }
    calibration_recipe r = cal::recipe_of(s);
    r.light.path = c.lights[k].path.string();
    r.dark.path = c.masters[0].path.string();
    r.flat.path = c.masters[1].path.string();
    r.dark_flat.path = c.masters[2].path.string();
    EXPECT_EQ(r.light.digest.sha256, c.lights[k].sha);
    EXPECT_EQ(r.dark.digest.sha256, c.masters[0].sha);
    c.recipes.push_back(std::move(r));
  }
  return c;
}

inline import_request import_request_of(const orig& o) {
  import_request r;
  r.path = o.path.string();
  r.digest = content_digest{o.sha, o.size};
  return r;
}

struct runner_env {
  memory_pool pool{std::uint64_t{256} << 20};
  cancel_token cancel;
};

// Registra gli originali, crea nodi e slot (imp<k> sui primi `imports` light, cal<k> per ricetta). Restituisce gli
// `input_record` nell'ordine: light, poi dark, flat, dark_flat.
inline std::vector<input_record> setup_chain_project(sqlite_dag_store& store, const chain& c, std::size_t imports) {
  std::vector<input_record> ids;
  for (const orig& o : c.lights) ids.push_back(register_input(store, o));
  for (const orig& o : c.masters) ids.push_back(register_input(store, o));
  for (std::size_t k = 0; k < imports; ++k) {
    const node_spec node = make_import_node(import_request_of(c.lights[k]));
    store.put_node(node);
    store.create_slot(import_slot(k), node.key());
  }
  for (std::size_t k = 0; k < c.recipes.size(); ++k) {
    const node_spec node = make_calibration_node(c.recipes[k]);
    store.put_node(node);
    store.create_slot(cal_slot(k), node.key());
  }
  return ids;
}

// Workflow (imp0..imp{imports-1}, cal0..cal{K-1}). `light_path(k)` dà il percorso corrente del light k-esimo.
inline workflow make_workflow(sqlite_dag_store& store, const chain& c, runner_env& env, std::size_t imports,
                              const std::function<std::string(std::size_t)>& light_path) {
  workflow wf;
  wf.name = "relink";
  for (std::size_t k = 0; k < imports; ++k) {
    import_request req = import_request_of(c.lights[k]);
    req.path = light_path(k);
    wf.steps.push_back(workflow_step{import_slot(k), make_import_node(req), [&store, &env, req, k] {
                                       return run_import_job(store, import_slot(k), req, env.pool, env.cancel);
                                     }});
  }
  for (std::size_t k = 0; k < c.recipes.size(); ++k) {
    calibration_recipe recipe = c.recipes[k];
    recipe.light.path = light_path(k);
    calibration_job_options opt;
    opt.worker_count = 2;
    opt.tile_rows = 4;
    wf.steps.push_back(workflow_step{cal_slot(k), make_calibration_node(recipe), [&store, &env, recipe, opt, k] {
                                       return run_calibration_job(store, cal_slot(k), recipe, env.pool, opt, env.cancel);
                                     }});
  }
  return wf;
}

inline std::size_t job_count(const fs::path& project, bool live = false) {
  return std::stoul(query(project, "SELECT count(*) FROM jobs", live));
}

// Digest e nome del file primario pubblicato dallo slot (`primary`), letti da SQL.
inline std::string primary_artifact_row(const fs::path& project, const std::string& slot, bool live = false) {
  return query(project,
               "SELECT a.rel_path, a.sha256 FROM slots s JOIN artifacts a ON a.id = s.artifact_id WHERE s.slot_name = '" + slot + "'",
               live);
}

inline std::string slot_rows(const fs::path& project, bool live = false) {
  return query(project, "SELECT * FROM slots ORDER BY slot_name", live);
}

inline std::size_t count_rows(const fs::path& project, const std::string& table, bool live = false) {
  return std::stoul(query(project, "SELECT count(*) FROM " + table, live));
}

// Cartella dei file di un test: progetto in `<name>`, originali in `<name>_w`.
struct world {
  fs::path proj;
  fs::path base;
};

inline world make_world(const std::string& name) {
  world w;
  w.proj = st::fresh_dir("rl_" + name);
  w.base = w.proj.parent_path() / ("rl_" + name + "_w");
  fs::create_directories(w.base);
  return w;
}

inline std::function<void()> no_cancel() {
  return [] {};
}

}  // namespace rl
