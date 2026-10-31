// Suite di conformita' della base comune (docs/contracts/BASE-confini.md §3): ogni caso T-M3BASE-NN gira per ogni voce
// del registro dei tipi di job (registered_job_types) e per ogni variante dichiarata. Il `work` della suite avvolge il
// `work` dell'operatore (job_probe::set_work_wrapper), conta le proprie invocazioni e ospita le iniezioni «dentro
// work»; i punti del confine (job_probe::set_observer) contano costruzioni del nodo, aperture e letture degli input,
// task, blocchi elaborati e finalize, e ospitano le iniezioni «al confine».
// Verifiche comuni a ogni arresto di un job (§3.2): (a) record del job, (b) provenance del nodo con la stessa causa,
// (c) slot non pubblicato e nessun artefatto nuovo oltre la provenance, (d) codice di uscita, (e) input invariati.

#include <gtest/gtest.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xmmintrin.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>

#include "astro/calibration.hpp"
#include "astro/checkpoint.hpp"
#include "astro/cpu_dispatch.hpp"
#include "astro/fits.hpp"
#include "astro/image.hpp"
#include "astro/job_probe.hpp"
#include "astro/job_types.hpp"
#include "astro/workflow.hpp"
#include "conformance_inputs.hpp"
#include "no_crash_report.hpp"

namespace {

using namespace astro;
namespace fs = std::filesystem;
namespace cf = conformance_fixture;

constexpr std::uint64_t MiB = std::uint64_t{1} << 20;
constexpr std::uint64_t pool_bytes = 256 * MiB;
constexpr unsigned workers = 2;
constexpr const char* slot = "s";

constexpr unsigned mxcsr_daz = 1u << 6;
constexpr unsigned mxcsr_dm = 1u << 8;
constexpr unsigned mxcsr_rc_down = 1u << 13;
constexpr unsigned mxcsr_ftz = 1u << 15;
// I quattro modi vietati di require_fp_environment: FZ, DAZ, arrotondamento non al piu' vicino, una maschera spenta.
struct fp_mode {
  const char* name;
  unsigned bits;
  bool clear;  // il bit si spegne (maschera) invece di accendersi
};
constexpr fp_mode bad_fp_modes[] = {
    {"ftz", mxcsr_ftz, false}, {"daz", mxcsr_daz, false}, {"rc", mxcsr_rc_down, false}, {"dm_off", mxcsr_dm, true}};

unsigned with_mode(unsigned csr, const fp_mode& m) { return m.clear ? (csr & ~m.bits) : (csr | m.bits); }

using failures = std::vector<std::string>;

std::string joined(const failures& f) {
  std::string out;
  for (const std::string& s : f) out += "\n  - " + s;
  return out;
}

// ---------------------------------------------------------------- sonda del confine

struct event {
  std::string point;
  std::string detail;
};

// Osservatore e involucro del `work` installati per la durata di un caso. `action` e i due ganci di `work` sono le
// iniezioni; l'osservatore registra l'evento prima di chiamare `action`.
class probe {
 public:
  probe() {
    job_probe::set_observer([this](std::string_view p, std::string_view d) {
      {
        std::lock_guard lock(m_);
        events_.push_back({std::string(p), std::string(d)});
      }
      if (action) action(p, d);
    });
    job_probe::set_work_wrapper([this](const job_record& job, const job_probe::work_fn& work) {
      ++work_calls;
      if (before_work) before_work(job);
      job_files_result r = work(job);
      if (after_work) after_work(job, r);
      return r;
    });
  }
  ~probe() {
    job_probe::set_observer(nullptr);
    job_probe::set_work_wrapper(nullptr);
    cpu_test::set_task_injector(nullptr);
    cpu_test::set_capabilities(std::nullopt);
  }
  probe(const probe&) = delete;
  probe& operator=(const probe&) = delete;

  std::size_t mark() const {
    std::lock_guard lock(m_);
    return events_.size();
  }
  std::size_t count(std::string_view point, std::size_t from = 0) const {
    std::lock_guard lock(m_);
    std::size_t n = 0;
    for (std::size_t i = from; i < events_.size(); ++i) n += events_[i].point == point ? 1 : 0;
    return n;
  }
  std::size_t count_details(std::string_view point, const std::set<std::string>& details) const {
    std::lock_guard lock(m_);
    std::size_t n = 0;
    for (const event& e : events_) n += e.point == point && details.count(e.detail) != 0 ? 1 : 0;
    return n;
  }
  std::optional<std::string> first_detail(std::string_view point) const {
    std::lock_guard lock(m_);
    for (const event& e : events_) {
      if (e.point == point) return e.detail;
    }
    return std::nullopt;
  }

  std::function<void(std::string_view, std::string_view)> action;
  std::function<void(const job_record&)> before_work;
  std::function<void(const job_record&, job_files_result&)> after_work;
  std::atomic<int> work_calls{0};

 private:
  mutable std::mutex m_;
  std::vector<event> events_;
};

// Valore di un campo `chiave=valore` del dettaglio di sched.plan.
std::uint64_t plan_field(const std::string& detail, const std::string& key) {
  const std::string needle = key + "=";
  std::size_t at = detail.rfind(";" + needle, std::string::npos);
  at = detail.rfind(needle, 0) == 0 ? 0 : (at == std::string::npos ? at : at + 1);
  if (at == std::string::npos) throw std::runtime_error("campo del piano assente: " + key);
  return std::stoull(detail.substr(at + needle.size()));
}

// ---------------------------------------------------------------- file e stato degli input

struct file_state {
  std::string sha256;
  std::uint64_t size = 0, inode = 0, nlink = 0;
  std::int64_t mtime_ns = 0;
  unsigned mode = 0;
  bool exists = false;
  friend bool operator==(const file_state&, const file_state&) = default;
};

std::vector<std::uint8_t> read_bytes(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::string read_text(const fs::path& p) {
  const auto b = read_bytes(p);
  return std::string(b.begin(), b.end());
}

// Riscrive il contenuto nello stesso inode.
void overwrite(const fs::path& p, const std::vector<std::uint8_t>& b) {
  std::ofstream out(p, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
  out.close();
  if (!out) throw std::runtime_error("riscrittura non riuscita: " + p.string());
}

file_state state_of(const fs::path& p) {
  struct stat sb {};
  if (::stat(p.c_str(), &sb) != 0) return {};
  const auto b = read_bytes(p);
  return {sha256_hex(b), static_cast<std::uint64_t>(sb.st_size), static_cast<std::uint64_t>(sb.st_ino),
          static_cast<std::uint64_t>(sb.st_nlink), std::int64_t{sb.st_mtim.tv_sec} * 1000000000 + sb.st_mtim.tv_nsec,
          static_cast<unsigned>(sb.st_mode & 07777), true};
}

// Identita' di un input per i dettagli di input.samples: inode del file e digest dei contenuti indicati.
std::set<std::string> sample_ids(const fs::path& p, const std::vector<std::vector<std::uint8_t>>& contents) {
  std::set<std::string> ids{"ino:" + std::to_string(state_of(p).inode)};
  for (const auto& b : contents) ids.insert("sha256:" + sha256_hex(b));
  return ids;
}

// Offset dei dati della HDU primaria: card dopo END arrotondate al blocco FITS.
std::size_t data_offset(const std::vector<std::uint8_t>& b) {
  for (std::size_t at = 0; at + 80 <= b.size(); at += 80) {
    if (std::string(b.begin() + static_cast<long>(at), b.begin() + static_cast<long>(at + 8)) == "END     ") {
      return (at / 2880 + 1) * 2880;
    }
  }
  throw std::runtime_error("header senza END");
}

// Sostituisce il valore di una card intera (colonne 11-30, allineato a destra) senza cambiare la dimensione del file.
void set_card(std::vector<std::uint8_t>& b, const std::string& key, long long value) {
  std::string k = key;
  k.resize(8, ' ');
  for (std::size_t at = 0; at + 80 <= b.size(); at += 80) {
    if (std::string(b.begin() + static_cast<long>(at), b.begin() + static_cast<long>(at + 8)) != k) continue;
    std::string v = std::to_string(value);
    v.insert(0, 20 - v.size(), ' ');
    for (std::size_t i = 0; i < 20; ++i) b[at + 10 + i] = static_cast<std::uint8_t>(v[i]);
    return;
  }
  throw std::runtime_error("card assente: " + key);
}

// ---------------------------------------------------------------- caso per tipo

struct type_param {
  std::size_t entry;
  std::size_t variant;
};

std::vector<type_param> params_where(const std::function<bool(const job_type_entry&, const job_type_variant&)>& keep) {
  std::vector<type_param> out;
  const auto& types = registered_job_types();
  for (std::size_t e = 0; e < types.size(); ++e) {
    for (std::size_t v = 0; v < types[e].variants.size(); ++v) {
      if (keep(types[e], types[e].variants[v])) out.push_back({e, v});
    }
  }
  return out;
}

std::vector<type_param> all_types() {
  return params_where([](const job_type_entry&, const job_type_variant&) { return true; });
}

std::string name_of(const type_param& p) {
  const job_type_entry& e = registered_job_types()[p.entry];
  std::string n = std::string(e.operator_id) + "_" + e.variants[p.variant].name;
  for (char& c : n) {
    if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
  }
  return n;
}

std::string param_name(const testing::TestParamInfo<type_param>& info) { return name_of(info.param); }

std::string current_test() {
  const testing::TestInfo* t = testing::UnitTest::GetInstance()->current_test_info();
  std::string n = std::string(t->test_suite_name()) + "_" + t->name();
  for (char& c : n) {
    if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
  }
  return n;
}

fs::path fresh(const std::string& name) {
  const fs::path dir = fs::absolute(fs::path(ASTRO_TEST_TMP) / "conformance" / name);
  fs::remove_all(dir);
  fs::create_directories(dir);
  return dir;
}

struct ran {
  std::optional<job_outcome> outcome;
  std::optional<stop_error> thrown;
  std::string other;  // eccezione non stop_error
};

struct type_case {
  const job_type_entry& entry;
  const job_type_variant& variant;
  fs::path dir;
  job_type_inputs inputs;
  std::unique_ptr<sqlite_dag_store> store;
  std::optional<minimal_job> job;
  std::map<std::string, file_state> expected;  // stato atteso degli input dopo il job (e)
  std::set<std::uint64_t> artifacts_before;
  std::optional<std::uint64_t> slot_artifact_before;

  type_case(const type_param& p, const std::string& tag = "")
      : entry(registered_job_types()[p.entry]), variant(entry.variants[p.variant]),
        dir(fresh(current_test() + tag)) {
    inputs = cf::write_inputs(entry, dir / "inputs");
    fs::create_directories(dir / "scratch");
    store = sqlite_dag_store::create(dir / "store");
    job.emplace(entry.make(inputs, variant.name, store->root()));
    store->put_node(job->node);
    store->create_slot(slot, job->node.key());
    snapshot();
  }

  // Stato di riferimento prima di un'esecuzione: input, artefatti del nodo, artefatto pubblicato dallo slot.
  void snapshot() {
    for (const auto& [role, in] : inputs) expected[role] = state_of(in.path);
    artifacts_before.clear();
    for (const artifact_record& a : store->artifacts_for(job->node.key())) artifacts_before.insert(a.id);
    slot_artifact_before = store->get_slot(slot)->artifact_id;
  }
  void refresh(const std::string& role) { expected[role] = state_of(inputs.at(role).path); }

  resource_config resources() const {
    resource_config r;
    r.scratch = dir_quota{dir / "scratch", 64 * MiB};
    r.worker_count = workers;
    r.tile_rows = 1;
    return r;
  }

  job_type_env env(memory_pool& pool, const cancel_token& cancel, std::optional<budget_snapshot> budget = {}) const {
    return job_type_env{*store, slot, pool, cancel, std::move(budget), resources(), "ckpt", false};
  }

  ran run(const job_type_env& e, bool quiet = true) {
    ran r;
    if (quiet) testing::internal::CaptureStderr();
    try {
      r.outcome = job->run(e);
    } catch (const stop_error& ex) {
      r.thrown = ex;
    } catch (const std::exception& ex) {
      r.other = ex.what();
    }
    if (quiet) testing::internal::GetCapturedStderr();
    return r;
  }

  ran run_default(std::optional<budget_snapshot> budget = {}) {
    memory_pool pool(pool_bytes);
    cancel_token cancel;
    return run(env(pool, cancel, std::move(budget)));
  }

  std::vector<job_record> jobs() const {
    std::vector<job_record> out;
    for (std::uint64_t id = 1; const std::optional<job_record> j = store->get_job(id); ++id) {
      if (j->slot_name == slot) out.push_back(*j);
    }
    return out;
  }

  std::vector<artifact_record> new_artifacts() const {
    std::vector<artifact_record> out;
    for (const artifact_record& a : store->artifacts_for(job->node.key())) {
      if (artifacts_before.count(a.id) == 0) out.push_back(a);
    }
    return out;
  }

  json provenance_doc(const artifact_record& a) const { return parse_json(read_text(store->artifact_path(a.rel_path))); }
};

const stop_error* stop_of(const ran& r) {
  if (r.thrown) return &*r.thrown;
  if (r.outcome && r.outcome->stop) return &*r.outcome->stop;
  return nullptr;
}

std::string describe(const ran& r) {
  if (r.thrown) return std::string("eccezione ") + r.thrown->what();
  if (!r.other.empty()) return "eccezione non stop_error: " + r.other;
  if (r.outcome && r.outcome->stop) return std::string("esito fermato ") + r.outcome->stop->what();
  if (r.outcome) return "esito riuscito (uscita " + std::to_string(r.outcome->exit_code) + ")";
  return "nessun esito";
}

const json* member(const json& o, const char* key) { return o.type() == json::kind::object ? o.find(key) : nullptr; }

std::string text_at(const json& o, const char* key) {
  const json* v = member(o, key);
  return v != nullptr && v->type() == json::kind::string ? v->as_string() : std::string("<assente>");
}

struct expected_stop {
  std::optional<error_category> category;  // vuoto: quella osservata, purche' coerente in (a), (b) e (d)
  std::string code;            // esatto; vuoto = quello osservato, purche' coerente in (a), (b) e (d)
  std::string code_suffix;     // alternativa a `code`: il codice deve terminare cosi'
  std::set<std::string> codes; // alternativa: uno dei codici
  bool before_work = false;
};

bool code_matches(const expected_stop& e, const std::string& code) {
  if (!e.codes.empty()) return e.codes.count(code) == 1;
  if (!e.code_suffix.empty()) {
    return code.size() >= e.code_suffix.size() && code.compare(code.size() - e.code_suffix.size(), e.code_suffix.size(), e.code_suffix) == 0;
  }
  return e.code.empty() || code == e.code;
}

// Verifiche comuni (a)-(e) di un arresto del job.
failures check_common(const type_case& c, const ran& r, const expected_stop& e) {
  failures f;
  const stop_error* s = stop_of(r);
  if (s == nullptr) {
    f.push_back("nessun arresto: " + describe(r));
    return f;
  }
  if ((e.category && s->category() != *e.category) || !code_matches(e, s->code())) {
    f.push_back(std::string("arresto diverso: ") + s->what());
  }
  if (e.before_work && !r.thrown) f.push_back("arresto prima del lavoro non propagato come eccezione: " + describe(r));
  // (d)
  const int exit = r.outcome ? r.outcome->exit_code : exit_code_for(s->category());
  if (exit != exit_code_for(s->category())) f.push_back("codice di uscita " + std::to_string(exit));
  // (a)
  const std::vector<job_record> jobs = c.jobs();
  if (jobs.empty()) {
    f.push_back("(a) nessun job registrato sullo slot");
  } else {
    const job_record& j = jobs.back();
    const job_state want = s->category() == error_category::cancelled ? job_state::cancelled : job_state::failed;
    if (j.state != want || j.stop_category != s->category() || j.stop_code != s->code()) {
      f.push_back("(a) record del job diverso dall'arresto: codice registrato " + j.stop_code);
    }
  }
  // (b) e (c)
  const std::vector<artifact_record> fresh_rows = c.new_artifacts();
  std::size_t provenance_rows = 0;
  for (const artifact_record& a : fresh_rows) {
    if (a.output_role != "provenance") {
      f.push_back("(c) artefatto nuovo oltre la provenance: ruolo " + a.output_role);
      continue;
    }
    ++provenance_rows;
    const json doc = c.provenance_doc(a);
    const json* stop = member(doc, "stop");
    if (stop == nullptr || text_at(*stop, "category") != to_string(s->category()) || text_at(*stop, "code") != s->code()) {
      f.push_back("(b) provenance con una causa diversa");
    }
    const json* sbw = member(doc, "stop_before_work");
    if (e.before_work && (sbw == nullptr || sbw->type() != json::kind::boolean || !sbw->as_bool())) {
      f.push_back("(b) provenance senza stop_before_work: true");
    }
  }
  if (provenance_rows != 1) f.push_back("(b) provenance nuove del nodo: " + std::to_string(provenance_rows));
  const std::optional<slot_record> sr = c.store->get_slot(slot);
  if (!sr || sr->artifact_id != c.slot_artifact_before) f.push_back("(c) slot pubblicato dal job fermato");
  // (e)
  for (const auto& [role, in] : c.inputs) {
    if (!(state_of(in.path) == c.expected.at(role))) f.push_back("(e) input cambiato: " + role);
  }
  return f;
}

// Stop prodotto dal punto d'iniezione dello store con errno EIO (sqlint::inject, throw_errno), costruito qui in modo
// indipendente dal codice sotto prova.
stop_error injected_store_stop(const std::string& point) {
  return stop_error(error_category::io, "store.syscall_failed", "chiamata di sistema fallita (iniezione)",
                    {{"punto", point}, {"errno", std::to_string(EIO)}, {"errno_text", std::strerror(EIO)}});
}

// Stessa categoria, codice, messaggio e contesto completo.
failures same_stop(const stop_error& got, const stop_error& want, const char* where) {
  failures f;
  if (got.category() != want.category() || got.code() != want.code() || got.message() != want.message() ||
      got.context() != want.context()) {
    f.push_back(std::string(where) + ": arresto diverso da quello dell'iniezione: " + got.what());
  }
  return f;
}

// La provenance nuova del nodo registra lo stesso messaggio e lo stesso contesto di `e`.
failures persisted_stop_matches(const type_case& c, const stop_error& e) {
  failures f;
  for (const artifact_record& a : c.new_artifacts()) {
    if (a.output_role != "provenance") continue;
    const json doc = c.provenance_doc(a);
    const json* stop = member(doc, "stop");
    if (stop == nullptr) {
      f.push_back("provenance senza stop");
      continue;
    }
    stop_error::context_list persisted;
    if (const json* ctx = member(*stop, "context")) {
      for (const json& item : ctx->items()) persisted.emplace_back(text_at(item, "key"), text_at(item, "value"));
    }
    if (text_at(*stop, "message") != e.message()) f.push_back("messaggio persistito diverso: " + text_at(*stop, "message"));
    if (persisted != e.context()) f.push_back("contesto persistito diverso da quello dell'arresto");
  }
  return f;
}

#define EXPECT_NO_FAILURES(expr)                      \
  do {                                                \
    const failures failures_ = (expr);                \
    EXPECT_TRUE(failures_.empty()) << joined(failures_); \
  } while (0)

// `body` in un processo figlio con SIGFPE e SIGABRT convertiti in uscite (136, 134, nessun core); uscita 0 se
// nessuna verifica fallisce, altrimenti le verifiche fallite su stderr e uscita 1.
[[noreturn]] void run_in_child(const std::function<failures()>& body) {
  struct sigaction action {};
  action.sa_handler = [](int) { _exit(136); };
  sigemptyset(&action.sa_mask);
  if (sigaction(SIGFPE, &action, nullptr) != 0) _exit(2);
  exit_instead_of_core_dump();
  failures f;
  try {
    f = body();
  } catch (const std::exception& e) {
    std::fprintf(stderr, "eccezione inattesa: %s\n", e.what());
    _exit(3);
  }
  if (!f.empty()) std::fprintf(stderr, "%s\n", joined(f).c_str());
  _exit(f.empty() ? 0 : 1);
}

class Conformance : public testing::TestWithParam<type_param> {};
class ConformanceScheduler : public testing::TestWithParam<type_param> {};
class ConformanceCheckpoint : public testing::TestWithParam<type_param> {};

}  // namespace

// ---------------------------------------------------------------- T-M3BASE-01 (OB-01)

// Eccezione dal `prepare` di prepare_job_start: il prepare del runner e' il controllo dell'ambiente FP, che con FZ acceso
// lancia numeric cpu.fp_mode. Nodo dello slot assente: graph dag.missing_node. In entrambi: arresto prima del lavoro
// registrato, eccezione propagata con categoria, codice e contesto del record.
TEST_P(Conformance, T_M3BASE_01_stop_before_work_is_recorded_and_propagated) {
  {
    type_case c(GetParam(), "_prepare");
    probe pr;
    const unsigned saved = _mm_getcsr();
    _mm_setcsr(saved | mxcsr_ftz);
    const ran r = c.run_default();
    _mm_setcsr(saved);
    EXPECT_NO_FAILURES(check_common(c, r, {error_category::numeric, "cpu.fp_mode", "", {}, true}));
    EXPECT_EQ(pr.work_calls.load(), 0);
    const std::vector<artifact_record> rows = c.new_artifacts();
    if (r.thrown && rows.size() == 1) {
      const json doc = c.provenance_doc(rows[0]);
      const json* stop = member(doc, "stop");
      ASSERT_NE(stop, nullptr);
      stop_error::context_list persisted;
      if (const json* ctx = member(*stop, "context")) {
        for (const json& item : ctx->items()) persisted.emplace_back(text_at(item, "key"), text_at(item, "value"));
      }
      EXPECT_EQ(persisted, r.thrown->context()) << "contesto propagato diverso da quello registrato";
      EXPECT_EQ(text_at(*stop, "message"), r.thrown->message());
    }
  }
  // Nodo dello slot assente: sullo store durevole la chiave esterna jobs.node_key -> nodes rifiuta start_job, il cui
  // errore e' il limite dichiarato di OB-01 (nessun job da registrare). Il ramo del runner si esercita con il nodo
  // presente ma illeggibile (parametri non analizzabili): node_of fallisce dopo start_job, come per un nodo assente.
  {
    type_case c(GetParam(), "_unreadable_node");
    probe pr;
    {
      sqlite3* db = nullptr;
      ASSERT_EQ(sqlite3_open_v2((c.store->root() / "project.db").c_str(), &db, SQLITE_OPEN_READWRITE, nullptr), SQLITE_OK);
      // Guasto dello storage: i trigger che rendono i nodi immutabili si tolgono solo in questo progetto di prova.
      std::string sql;
      sqlite3_stmt* st = nullptr;
      sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master WHERE type='trigger' AND tbl_name='nodes'", -1, &st, nullptr);
      while (sqlite3_step(st) == SQLITE_ROW) {
        sql += " DROP TRIGGER \"" + std::string(reinterpret_cast<const char*>(sqlite3_column_text(st, 0))) + "\";";
      }
      sqlite3_finalize(st);
      sql += " UPDATE nodes SET params_canonical='{' WHERE node_key='" + c.job->node.key() + "';";
      EXPECT_EQ(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr), SQLITE_OK) << sqlite3_errmsg(db);
      sqlite3_close(db);
    }
    const ran r = c.run_default();
    EXPECT_NO_FAILURES(check_common(c, r, {std::nullopt, "", "", {}, true}));
    EXPECT_EQ(pr.work_calls.load(), 0);
    if (r.thrown) {
      const std::vector<artifact_record> rows = c.new_artifacts();
      ASSERT_EQ(rows.size(), 1u);
      const json doc = c.provenance_doc(rows[0]);
      const json* stop = member(doc, "stop");
      ASSERT_NE(stop, nullptr);
      stop_error::context_list persisted;
      if (const json* ctx = member(*stop, "context")) {
        for (const json& item : ctx->items()) persisted.emplace_back(text_at(item, "key"), text_at(item, "value"));
      }
      EXPECT_EQ(persisted, r.thrown->context()) << "contesto propagato diverso da quello registrato";
    }
  }
}

// ---------------------------------------------------------------- T-M3BASE-02 e T-M3BASE-13 (OB-02, OB-13)

namespace {

// Le tre iniezioni di T-M3BASE-02, riusate da T-M3BASE-13.
enum class work_fault { store_write, foreign_exception, after_commit };

struct fault_run {
  ran result;
  int work_calls = 0;
};

fault_run run_with_fault(type_case& c, work_fault fault) {
  probe pr;
  bool fired = false;
  if (fault == work_fault::store_write) {
    c.store->set_fault_injector([&fired](std::string_view point) {
      if (fired || point != "artifact.create") return 0;
      fired = true;
      return EIO;
    });
  } else if (fault == work_fault::after_commit) {
    c.store->set_fault_injector([](std::string_view point) { return point == "commit.after_commit" ? EIO : 0; });
  } else {
    pr.after_work = [](const job_record&, job_files_result&) { throw std::runtime_error("eccezione estranea nel work"); };
  }
  fault_run out;
  out.result = c.run_default();
  c.store->set_fault_injector(nullptr);
  out.work_calls = pr.work_calls.load();
  return out;
}

}  // namespace

TEST_P(Conformance, T_M3BASE_02_stop_in_work_or_publication_is_recorded) {
  {
    type_case c(GetParam(), "_write");
    const fault_run fr = run_with_fault(c, work_fault::store_write);
    EXPECT_NO_FAILURES(check_common(c, fr.result, {error_category::io, "store.syscall_failed", "", {}, false}));
    ASSERT_TRUE(fr.result.outcome.has_value() && fr.result.outcome->stop.has_value()) << describe(fr.result);
    const stop_error want = injected_store_stop("artifact.create");
    EXPECT_NO_FAILURES(same_stop(*fr.result.outcome->stop, want, "esito"));
    EXPECT_NO_FAILURES(persisted_stop_matches(c, want));
  }
  {
    type_case c(GetParam(), "_foreign");
    const fault_run fr = run_with_fault(c, work_fault::foreign_exception);
    EXPECT_EQ(fr.result.other, "eccezione estranea nel work") << "eccezione non rilanciata invariata: " << describe(fr.result);
    const std::vector<job_record> jobs = c.jobs();
    ASSERT_FALSE(jobs.empty());
    EXPECT_EQ(jobs.back().state, job_state::failed);
    EXPECT_EQ(jobs.back().stop_category, error_category::integrity);
    EXPECT_EQ(jobs.back().stop_code, "job.unexpected_exception");
    const std::vector<artifact_record> rows = c.new_artifacts();
    ASSERT_EQ(rows.size(), 1u) << "solo la provenance come artefatto nuovo";
    EXPECT_EQ(rows[0].output_role, "provenance");
    const json doc = c.provenance_doc(rows[0]);
    const json* stop = member(doc, "stop");
    ASSERT_NE(stop, nullptr);
    EXPECT_EQ(text_at(*stop, "code"), "job.unexpected_exception");
    EXPECT_EQ(c.store->get_slot(slot)->artifact_id, c.slot_artifact_before);
    for (const auto& [role, in] : c.inputs) EXPECT_EQ(state_of(in.path), c.expected.at(role)) << role;
  }
  {
    type_case c(GetParam(), "_after_commit");
    const fault_run fr = run_with_fault(c, work_fault::after_commit);
    ASSERT_TRUE(fr.result.thrown.has_value()) << "errore dopo il commit non propagato: " << describe(fr.result);
    EXPECT_NO_FAILURES(same_stop(*fr.result.thrown, injected_store_stop("commit.after_commit"), "eccezione propagata"));
    const std::vector<job_record> jobs = c.jobs();
    ASSERT_EQ(jobs.size(), 1u);
    EXPECT_EQ(jobs.back().state, job_state::completed);
    EXPECT_EQ(c.store->read_slot(slot).state, freshness::current) << "output non pubblicati";
  }
}

TEST_P(Conformance, T_M3BASE_13_one_execution_per_job) {
  int k = 0;
  for (const work_fault fault : {work_fault::store_write, work_fault::foreign_exception, work_fault::after_commit}) {
    type_case c(GetParam(), "_" + std::to_string(k++));
    const fault_run fr = run_with_fault(c, fault);
    EXPECT_EQ(c.jobs().size(), 1u) << "un solo record di job per lo slot";
    EXPECT_EQ(fr.work_calls, 1) << "work chiamato una volta";
    bool found = false;
    for (const artifact_record& a : c.store->artifacts_for(c.job->node.key())) {
      if (a.output_role != "provenance") continue;
      const json doc = c.provenance_doc(a);
      const json* spec = member(doc, "node_spec");
      ASSERT_NE(spec, nullptr);
      EXPECT_EQ(text_at(*spec, "backend"), c.job->node.backend());
      found = true;
    }
    EXPECT_TRUE(found) << "provenance assente";
  }
}

// ---------------------------------------------------------------- T-M3BASE-03 (OB-03)

namespace {

struct db_row {
  std::string table;
  std::vector<int> types;
  std::vector<std::string> values;
};

std::vector<db_row> all_rows(const fs::path& db_path) {
  sqlite3* db = nullptr;
  std::vector<db_row> out;
  if (sqlite3_open_v2(("file:" + db_path.string() + "?mode=ro").c_str(), &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI,
                      nullptr) != SQLITE_OK) {
    sqlite3_close(db);
    throw std::runtime_error("apertura del database non riuscita");
  }
  std::vector<std::string> tables;
  sqlite3_stmt* st = nullptr;
  sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name", -1, &st, nullptr);
  while (sqlite3_step(st) == SQLITE_ROW) tables.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(st, 0)));
  sqlite3_finalize(st);
  for (const std::string& t : tables) {
    sqlite3_prepare_v2(db, ("SELECT * FROM \"" + t + "\"").c_str(), -1, &st, nullptr);
    while (sqlite3_step(st) == SQLITE_ROW) {
      db_row row{t, {}, {}};
      for (int i = 0; i < sqlite3_column_count(st); ++i) {
        row.types.push_back(sqlite3_column_type(st, i));
        const auto* v = sqlite3_column_text(st, i);
        row.values.emplace_back(v != nullptr ? reinterpret_cast<const char*>(v) : "");
      }
      out.push_back(std::move(row));
    }
    sqlite3_finalize(st);
  }
  sqlite3_close(db);
  return out;
}

std::string definition_digest(const workflow& wf) {
  json steps = json::array();
  for (const auto& s : wf.steps) {
    json o = json::object();
    o.set("slot_name", json::string(s.slot_name));
    o.set("node_key", json::string(s.node.key()));
    steps.push(std::move(o));
  }
  json doc = json::object();
  doc.set("format", json::string(workflow_format));
  doc.set("name", json::string(wf.name));
  doc.set("steps", std::move(steps));
  const std::string c = canonical_json(doc);
  return sha256_hex({reinterpret_cast<const std::uint8_t*>(c.data()), c.size()});
}

// Righe nuove dello store dopo il rifiuto: un record di arresto del workflow con la sua identita' (digest della
// definizione), categoria e codice, solo stringhe e interi; nessun job nuovo.
failures check_workflow_stop_record(const std::vector<db_row>& before, const std::vector<db_row>& after,
                                    const std::vector<std::string>& digests, const stop_error& e) {
  failures f;
  std::multiset<std::string> seen;
  for (const db_row& r : before) {
    std::string k = r.table;
    for (const std::string& v : r.values) k += "|" + v;
    seen.insert(k);
  }
  bool record = false;
  for (const db_row& r : after) {
    std::string k = r.table;
    for (const std::string& v : r.values) k += "|" + v;
    if (seen.count(k) != 0) {
      seen.erase(seen.find(k));
      continue;
    }
    if (r.table == "jobs") f.push_back("job avviato dal workflow rifiutato");
    bool has_digest = false, has_code = false, has_category = false, only_text_int = true;
    for (std::size_t i = 0; i < r.values.size(); ++i) {
      for (const std::string& d : digests) has_digest = has_digest || r.values[i].find(d) != std::string::npos;
      has_code = has_code || r.values[i].find(e.code()) != std::string::npos;
      has_category = has_category || r.values[i].find(to_string(e.category())) != std::string::npos;
      only_text_int = only_text_int && (r.types[i] == SQLITE_TEXT || r.types[i] == SQLITE_INTEGER || r.types[i] == SQLITE_NULL);
    }
    if (has_digest && has_code && has_category) {
      record = true;
      if (!only_text_int) f.push_back("record di arresto del workflow con valori non stringa o intero");
    }
  }
  if (!record) {
    f.push_back("nessun record di arresto del workflow nello store con identita' del workflow, categoria e codice (" +
                std::string(e.what()) + ")");
  }
  return f;
}

}  // namespace

TEST(ConformanceWorkflow, T_M3BASE_03_workflow_refusal_leaves_a_stop_record) {
  const type_param first{0, 0};
  type_case c(first);
  memory_pool pool(pool_bytes);
  cancel_token cancel;
  const job_type_env e = c.env(pool, cancel);
  minimal_job& job = *c.job;
  workflow wf{"conf", {workflow_step{slot, job.node, [&] { return job.run(e); }}}, nullptr};
  const fs::path db = c.store->root() / "project.db";
  {  // run_workflow con slot che punta a un altro nodo: il passo dichiara il nodo di una richiesta diversa
    job_type_inputs other_inputs = cf::write_inputs(c.entry, c.dir / "other");
    for (auto& [role, in] : other_inputs) {
      std::vector<std::uint8_t> b = read_bytes(in.path);
      set_card(b, "FRAMENO", 7);
      overwrite(in.path, b);
      in.digest = digest_of(b);
    }
    const minimal_job other_job = c.entry.make(other_inputs, c.variant.name, c.store->root());
    c.store->put_node(other_job.node);
    workflow wrong_target = wf;
    wrong_target.steps[0].node = other_job.node;
    const std::vector<db_row> before = all_rows(db);
    std::optional<stop_error> refused;
    try {
      (void)run_workflow(*c.store, wrong_target);
    } catch (const stop_error& ex) {
      refused = ex;
    }
    ASSERT_TRUE(refused.has_value());
    EXPECT_EQ(refused->code(), "workflow.slot_target");
    EXPECT_NO_FAILURES(check_workflow_stop_record(before, all_rows(db), {definition_digest(wrong_target)}, *refused));
  }
  {  // resume_workflow con definizione diversa
    testing::internal::CaptureStderr();
    const workflow_result done = run_workflow(*c.store, wf);
    testing::internal::GetCapturedStderr();
    ASSERT_EQ(done.exit_code, 0);
    workflow changed = wf;
    c.store->create_slot("secondo", job.node.key());
    changed.steps.push_back(workflow_step{"secondo", job.node, [&] { return job.run(e); }});
    const std::vector<db_row> before = all_rows(db);
    std::optional<stop_error> refused;
    try {
      (void)resume_workflow(*c.store, changed);
    } catch (const stop_error& ex) {
      refused = ex;
    }
    ASSERT_TRUE(refused.has_value());
    EXPECT_EQ(refused->code(), "resume.recipe");
    EXPECT_NO_FAILURES(check_workflow_stop_record(before, all_rows(db), {definition_digest(changed), definition_digest(wf)},
                                                  *refused));
  }
}

// ---------------------------------------------------------------- T-M3BASE-04 (OB-04)

// In un processo figlio per ognuno dei quattro modi: arresto numeric cpu.fp_mode prima di leggere il nodo, nessuna
// costruzione di node_spec e nessun `work` dopo l'impostazione del modo, verifiche (a)-(e) con MXCSR ripristinato.
TEST_P(Conformance, T_M3BASE_04_fp_environment_at_the_job_boundary) {
  for (const fp_mode& m : bad_fp_modes) {
    SCOPED_TRACE(m.name);
    type_case c(GetParam(), std::string("_") + m.name);
    const auto child = [&] {
      run_in_child([&] {
        probe pr;
        memory_pool pool(pool_bytes);
        cancel_token cancel;
        const job_type_env e = c.env(pool, cancel);
        const unsigned saved = _mm_getcsr();
        ran r;
        _mm_setcsr(with_mode(saved, m));
        try {
          r.outcome = c.job->run(e);
        } catch (const stop_error& ex) {
          r.thrown = ex;
        } catch (const std::exception& ex) {
          r.other = ex.what();
        }
        _mm_setcsr(saved);
        failures f = check_common(c, r, {error_category::numeric, "cpu.fp_mode", "", {}, true});
        if (pr.count("node.build") != 0) {
          f.push_back("costruttore del nodo chiamato " + std::to_string(pr.count("node.build")) + " volte con MXCSR vietato");
        }
        if (pr.work_calls.load() != 0) f.push_back("work chiamato");
        return f;
      });
    };
    EXPECT_EXIT(child(), testing::ExitedWithCode(0), "");
  }
}

// ---------------------------------------------------------------- T-M3BASE-05 (OB-05)

// Tipi con scheduler: il punto cpu.task_start imposta un MXCSR vietato nel thread del worker. Il controllo del worker
// deve precedere run_task: nessuna invocazione di run_task, nessun kernel.
TEST_P(ConformanceScheduler, T_M3BASE_05_fp_environment_in_the_worker) {
  for (const fp_mode& m : bad_fp_modes) {
    SCOPED_TRACE(m.name);
    type_case c(GetParam(), std::string("_") + m.name);
    const auto child = [&] {
      run_in_child([&] {
        probe pr;
        cpu_test::reset_kernel_calls();
        cpu_test::set_task_injector([m](std::string_view point) {
          if (point == "cpu.task_start") _mm_setcsr(with_mode(_mm_getcsr(), m));
          return 0;
        });
        memory_pool pool(pool_bytes);
        cancel_token cancel;
        const ran r = c.run(c.env(pool, cancel), false);
        failures f = check_common(c, r, {error_category::numeric, "cpu.fp_mode", "", {}, false});
        if (pr.count("sched.task") != 0) {
          f.push_back("run_task invocato " + std::to_string(pr.count("sched.task")) + " volte con MXCSR vietato nel worker");
        }
        if (cpu_test::kernel_calls(cpu_path::scalar) + cpu_test::kernel_calls(cpu_path::avx2) != 0) f.push_back("kernel eseguiti");
        return f;
      });
    };
    EXPECT_EXIT(child(), testing::ExitedWithCode(0), "");
  }
}

// ---------------------------------------------------------------- T-M3BASE-06 (OB-06)

TEST_P(Conformance, T_M3BASE_06_cancel_at_the_publication_boundary) {
  type_case c(GetParam());
  probe pr;
  memory_pool pool(pool_bytes);
  cancel_token cancel;
  pr.after_work = [&](const job_record&, job_files_result&) { cancel.request(); };
  const ran r = c.run(c.env(pool, cancel));
  EXPECT_NO_FAILURES(check_common(c, r, {error_category::cancelled, "sched.cancelled", "", {}, false}));
  if (r.outcome) {
    EXPECT_EQ(r.outcome->exit_code, exit_code_cancelled);
  }
}

// ---------------------------------------------------------------- T-M3BASE-07 (OB-07)

// Cancel nel `prepare` (punto sched.prepare) e nel primo blocco di righe (primo sched.task). Dopo la richiesta: blocchi
// letti nel prepare al piu' uno (il prepare gira in un solo thread), blocchi elaborati (sched.task_done) al piu' uno
// per worker, task avviati dopo la richiesta al piu' quelli gia' ammessi dagli altri worker, nessun finalize.
TEST_P(Conformance, T_M3BASE_07_cancel_during_the_work) {
  {
    type_case c(GetParam(), "_prepare");
    probe pr;
    memory_pool pool(pool_bytes);
    cancel_token cancel;
    std::atomic<std::size_t> mark{0};
    pr.action = [&](std::string_view p, std::string_view) {
      if (p == "sched.prepare" && !cancel.requested()) {
        mark = pr.mark();
        cancel.request();
      }
    };
    const ran r = c.run(c.env(pool, cancel));
    EXPECT_TRUE(cancel.requested()) << "punto di iniezione sched.prepare non raggiunto: il tipo non passa dallo scheduler";
    EXPECT_NO_FAILURES(check_common(c, r, {error_category::cancelled, "sched.cancelled", "", {}, false}));
    if (cancel.requested()) {
      EXPECT_LE(pr.count("input.read", mark), 1u) << "blocchi letti nel prepare dopo la richiesta";
      EXPECT_EQ(pr.count("sched.task", mark), 0u) << "task avviati dopo la richiesta";
      EXPECT_EQ(pr.count("sched.finalize"), 0u) << "finalize eseguito";
    }
  }
  {
    type_case c(GetParam(), "_rows");
    probe pr;
    memory_pool pool(pool_bytes);
    cancel_token cancel;
    std::atomic<std::size_t> mark{0};
    pr.action = [&](std::string_view p, std::string_view) {
      if (p == "sched.task" && !cancel.requested()) {
        mark = pr.mark();
        cancel.request();
      }
    };
    const ran r = c.run(c.env(pool, cancel));
    EXPECT_TRUE(cancel.requested()) << "punto di iniezione sched.task non raggiunto: il tipo non passa dallo scheduler";
    EXPECT_NO_FAILURES(check_common(c, r, {error_category::cancelled, "sched.cancelled", "", {}, false}));
    if (cancel.requested()) {
      const std::optional<std::string> plan = pr.first_detail("sched.plan");
      ASSERT_TRUE(plan.has_value());
      const std::uint64_t w = plan_field(*plan, "worker_count");
      EXPECT_GT(cf::height / plan_field(*plan, "tile_rows"), w + 1) << "blocchi ancora da elaborare all'iniezione";
      EXPECT_LE(pr.count("sched.task_done", mark), w) << "blocchi elaborati dopo la richiesta";
      EXPECT_LE(pr.count("sched.task", mark), w - 1) << "task avviati dopo la richiesta oltre quelli gia' ammessi";
      EXPECT_EQ(pr.count("sched.finalize"), 0u) << "finalize eseguito";
    }
  }
}

// ---------------------------------------------------------------- T-M3BASE-08 (OB-08)

TEST_P(Conformance, T_M3BASE_08_job_peak_reserved_before_the_first_task) {
  std::optional<std::string> executed;
  std::optional<json> recorded;
  std::optional<std::uint64_t> reserved_at_first_task;
  {  // piano eseguito, picco prenotato al primo task, piano registrato nella provenance
    type_case c(GetParam(), "_plan");
    probe pr;
    memory_pool pool(pool_bytes);
    cancel_token cancel;
    std::mutex m;
    pr.action = [&](std::string_view p, std::string_view) {
      std::lock_guard lock(m);
      if (p == "sched.task" && !reserved_at_first_task) reserved_at_first_task = pool.reserved_bytes();
    };
    const ran r = c.run(c.env(pool, cancel, budget_snapshot{}));
    EXPECT_TRUE(r.outcome.has_value() && r.outcome->exit_code == 0) << describe(r);
    executed = pr.first_detail("sched.plan");
    for (const artifact_record& a : c.store->artifacts_for(c.job->node.key())) {
      if (a.output_role != "provenance") continue;
      const json doc = c.provenance_doc(a);
      if (const json* d = member(doc, "details")) {
        if (const json* p = member(*d, "resource_plan")) recorded = *p;
      }
    }
  }
  std::optional<std::uint64_t> peak;
  if (executed) {
    peak = plan_field(*executed, "shared_bytes") +
           plan_field(*executed, "worker_count") * plan_field(*executed, "per_task_bytes");
  }
  {  // pool con spazio libero pari al picco meno un byte: attesa FIFO senza task, poi esecuzione dopo il rilascio
    type_case c(GetParam(), "_busy");
    probe pr;
    memory_pool pool(pool_bytes);
    cancel_token cancel;
    reservation held = pool.reserve(peak ? pool_bytes - *peak + 1 : pool_bytes, "occupante");
    const job_type_env e = c.env(pool, cancel);
    ran r;
    std::atomic<bool> finished{false};
    std::thread t([&] {
      r = c.run(e, false);
      finished = true;
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (pool.waiting_count() == 0 && !finished && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    const bool waited = pool.waiting_count() == 1;
    EXPECT_TRUE(waited) << "il job non e' in attesa FIFO del pool con spazio libero sotto il picco";
    EXPECT_EQ(pr.count("sched.task"), 0u) << "task avviati senza il picco prenotato";
    held.release();
    t.join();
    EXPECT_TRUE(r.outcome.has_value() && r.outcome->exit_code == 0) << "dopo il rilascio il job deve eseguire: " << describe(r);
  }
  ASSERT_TRUE(executed.has_value()) << "il tipo non dichiara un job_plan allo scheduler";
  ASSERT_TRUE(recorded.has_value()) << "job_plan non registrato nella provenance";
  for (const char* k : {"worker_count", "tile_rows", "shared_bytes", "per_task_bytes"}) {
    const json* v = member(*recorded, k);
    ASSERT_NE(v, nullptr) << k;
    EXPECT_EQ(static_cast<std::uint64_t>(v->as_integer()), plan_field(*executed, k)) << k;
  }
  ASSERT_TRUE(reserved_at_first_task.has_value()) << "nessun task eseguito";
  EXPECT_GE(*reserved_at_first_task, *peak) << "picco non prenotato per intero prima del primo task";
  {  // budget = picco - 1
    type_case c(GetParam(), "_budget");
    probe pr;
    memory_pool pool(*peak - 1);
    cancel_token cancel;
    const ran r = c.run(c.env(pool, cancel));
    EXPECT_NO_FAILURES(check_common(c, r, {error_category::budget, "sched.job_exceeds_budget", "", {}, false}));
    EXPECT_EQ(pr.count("sched.task"), 0u) << "task avviati oltre il budget";
  }
}

// ---------------------------------------------------------------- T-M3BASE-09 (OB-09)

namespace {

const json* provenance_of_state(const type_case& c, const char* state, json& holder) {
  for (const artifact_record& a : c.new_artifacts()) {
    if (a.output_role != "provenance") continue;
    holder = c.provenance_doc(a);
    if (text_at(holder, "state") == state) return &holder;
  }
  return nullptr;
}

}  // namespace

namespace {

constexpr std::uint64_t GiB = std::uint64_t{1} << 30;

budget_snapshot recognizable_snapshot() {
  budget_snapshot b;
  b.budget_bytes = 192 * MiB;
  b.mem_total_bytes = 7 * GiB + 12345;
  b.mem_available_bytes = 3 * GiB + 678;
  b.fraction = 0.25;
  return b;
}

// Budget persistito uguale allo snapshot passato, con la configurazione delle risorse che il job ha ricevuto.
failures budget_matches(const json& doc, const type_case& c) {
  failures f;
  const json* budget = member(doc, "budget");
  if (budget == nullptr || budget->type() != json::kind::object) return {"budget assente"};
  const budget_snapshot want = recognizable_snapshot();
  const auto u64 = [&](const char* k) {
    const json* v = member(*budget, k);
    return v != nullptr && v->type() == json::kind::integer ? static_cast<std::uint64_t>(v->as_integer()) : ~std::uint64_t{0};
  };
  if (u64("budget_bytes") != want.budget_bytes || u64("mem_total_bytes") != want.mem_total_bytes ||
      u64("mem_available_bytes") != want.mem_available_bytes) {
    f.push_back("budget persistito diverso dallo snapshot passato");
  }
  const json* fraction = member(*budget, "fraction");
  if (text_at(*budget, "mode") != "automatic" || fraction == nullptr || fraction->type() != json::kind::number ||
      fraction->as_number() != want.fraction) {
    f.push_back("modo o fraction del budget persistito diversi dallo snapshot passato");
  }
  resource_config used = c.resources();
  if (!c.variant.scratch) used.scratch.reset();
  const json* resources = member(*budget, "resources");
  if (resources == nullptr || canonical_json(*resources) != canonical_json(resource_config_json(used))) {
    f.push_back("configurazione delle risorse assente o diversa da quella ricevuta dal job");
  }
  return f;
}

}  // namespace

TEST_P(Conformance, T_M3BASE_09_common_observations_in_the_provenance) {
  {  // riuscito, con snapshot e risorse
    type_case c(GetParam(), "_ok");
    probe pr;
    const ran r = c.run_default(recognizable_snapshot());
    ASSERT_TRUE(r.outcome.has_value() && r.outcome->exit_code == 0) << describe(r);
    json doc;
    ASSERT_NE(provenance_of_state(c, "completed", doc), nullptr);
    EXPECT_NO_FAILURES(budget_matches(doc, c));
    const json* details = member(doc, "details");
    if (c.job->node.backend().rfind("cpu_", 0) == 0) {
      const json* cpu = details != nullptr ? member(*details, "cpu") : nullptr;
      EXPECT_TRUE(cpu != nullptr && cpu->type() == json::kind::object) << "details.cpu assente";
      if (cpu != nullptr && cpu->type() == json::kind::object) {
        EXPECT_EQ(text_at(*cpu, "backend"), c.job->node.backend()) << "details.cpu.backend diverso dal nodo";
        if (const json* simd = member(c.job->node.params(), "simd")) {
          EXPECT_EQ(text_at(*cpu, "simd"), simd->as_string()) << "details.cpu.simd diverso dalla richiesta";
        }
        const json* caps = member(*cpu, "capabilities");
        const json* avx2 = caps != nullptr ? member(*caps, "avx2") : nullptr;
        ASSERT_TRUE(avx2 != nullptr && avx2->type() == json::kind::boolean) << "details.cpu.capabilities.avx2 assente";
        __builtin_cpu_init();
        EXPECT_EQ(avx2->as_bool(), static_cast<bool>(__builtin_cpu_supports("avx2")))
            << "details.cpu.capabilities.avx2 diverso dalla CPU";
      }
    }
    const std::optional<std::string> plan = pr.first_detail("sched.plan");
    EXPECT_TRUE(plan.has_value()) << "il tipo non dichiara un job_plan: details.resource_plan non confrontabile";
    const json* rp = details != nullptr ? member(*details, "resource_plan") : nullptr;
    EXPECT_NE(rp, nullptr) << "details.resource_plan assente";
    if (plan && rp != nullptr) {
      for (const char* k : {"worker_count", "tile_rows", "shared_bytes", "per_task_bytes"}) {
        const json* v = member(*rp, k);
        ASSERT_NE(v, nullptr) << k;
        EXPECT_EQ(static_cast<std::uint64_t>(v->as_integer()), plan_field(*plan, k)) << k;
      }
    }
  }
  {  // fermato dentro work, con snapshot e risorse
    type_case c(GetParam(), "_stopped");
    probe pr;
    pr.after_work = [](const job_record&, job_files_result&) {
      throw stop_error(error_category::io, "test.injected", "arresto iniettato dal work della suite");
    };
    const ran r = c.run_default(recognizable_snapshot());
    ASSERT_TRUE(r.outcome.has_value()) << describe(r);
    json doc;
    ASSERT_NE(provenance_of_state(c, "failed", doc), nullptr);
    EXPECT_NO_FAILURES(budget_matches(doc, c));
  }
  {  // senza snapshot
    type_case c(GetParam(), "_nosnapshot");
    const ran r = c.run_default();
    ASSERT_TRUE(r.outcome.has_value() && r.outcome->exit_code == 0) << describe(r);
    json doc;
    ASSERT_NE(provenance_of_state(c, "completed", doc), nullptr);
    EXPECT_EQ(member(doc, "budget"), nullptr) << "budget registrato senza snapshot";
  }
}

// ---------------------------------------------------------------- T-M3BASE-10 (OB-10)

// Slot sul nodo della richiesta A, job lanciato con la richiesta B: il confronto delle chiavi chiude l'obbligo prima di
// `work` (BASE-confini, OB-10, «Si estingue»), quindi nessun `work` e nessuna apertura di input.
TEST_P(Conformance, T_M3BASE_10_node_of_the_job_equals_the_request) {
  type_case c(GetParam());
  job_type_inputs other = cf::write_inputs(c.entry, c.dir / "other");
  for (auto& [role, in] : other) {
    std::vector<std::uint8_t> b = read_bytes(in.path);
    set_card(b, "FRAMENO", 7);
    overwrite(in.path, b);
    in.digest = digest_of(b);
  }
  const minimal_job b_job = c.entry.make(other, c.variant.name, c.store->root());
  ASSERT_NE(b_job.node.key(), c.job->node.key());
  std::map<std::string, file_state> other_before;
  for (const auto& [role, in] : other) other_before[role] = state_of(in.path);
  probe pr;
  memory_pool pool(pool_bytes);
  cancel_token cancel;
  ran r;
  testing::internal::CaptureStderr();
  try {
    r.outcome = b_job.run(c.env(pool, cancel));
  } catch (const stop_error& ex) {
    r.thrown = ex;
  } catch (const std::exception& ex) {
    r.other = ex.what();
  }
  testing::internal::GetCapturedStderr();
  EXPECT_NO_FAILURES(check_common(c, r, {error_category::invalid_input, "", ".node_mismatch", {}, false}));
  EXPECT_EQ(pr.count("input.open"), 0u) << "input aperti";
  EXPECT_EQ(pr.work_calls.load(), 0) << "work chiamato: il confronto non avviene prima del lavoro";
  if (const stop_error* s = stop_of(r)) {
    std::set<std::string> values;
    for (const auto& [k, v] : s->context()) values.insert(v);
    EXPECT_EQ(values.count(c.job->node.key()), 1u) << "chiave del nodo dello slot assente dal contesto";
    EXPECT_EQ(values.count(b_job.node.key()), 1u) << "chiave del nodo della richiesta assente dal contesto";
  }
  for (const auto& [role, in] : other) EXPECT_EQ(state_of(in.path), other_before.at(role)) << role;
}

// ---------------------------------------------------------------- T-M3BASE-11 (OB-11)

TEST_P(Conformance, T_M3BASE_11_inputs_verified_before_use) {
  for (const std::string& role : registered_job_types()[GetParam().entry].input_roles) {
    SCOPED_TRACE(role);
    {  // file assente
      type_case c(GetParam(), "_missing_" + role);
      fs::remove(c.inputs.at(role).path);
      c.refresh(role);
      const ran r = c.run_default();
      EXPECT_NO_FAILURES(check_common(c, r, {error_category::io, "input.missing", "", {}, false}));
    }
    {  // un byte cambiato: lo stop precede ogni uso dei campioni
      type_case c(GetParam(), "_byte_" + role);
      std::vector<std::uint8_t> b = read_bytes(c.inputs.at(role).path);
      b[data_offset(b)] ^= 0x01;
      overwrite(c.inputs.at(role).path, b);
      c.refresh(role);
      const std::set<std::string> ids = sample_ids(c.inputs.at(role).path, {b});
      probe pr;
      const ran r = c.run_default();
      EXPECT_NO_FAILURES(check_common(c, r, {error_category::integrity, "input.digest_mismatch", "", {}, false}));
      EXPECT_EQ(pr.count_details("input.samples", ids), 0u) << "campioni dell'input alterato usati prima del digest";
    }
    {  // file cambiato dopo la verifica, durante i task e prima della pubblicazione
      type_case c(GetParam(), "_after_" + role);
      probe pr;
      std::atomic<bool> changed{false};
      std::mutex m;
      pr.action = [&](std::string_view p, std::string_view) {
        if (p != "sched.task" || changed.exchange(true)) return;
        std::lock_guard lock(m);
        std::vector<std::uint8_t> b = read_bytes(c.inputs.at(role).path);
        b[data_offset(b)] ^= 0x01;
        overwrite(c.inputs.at(role).path, b);
        c.refresh(role);
      };
      memory_pool pool(pool_bytes);
      cancel_token cancel;
      const ran r = c.run(c.env(pool, cancel));
      EXPECT_TRUE(changed.load()) << "punto di iniezione sched.task non raggiunto: il tipo non passa dallo scheduler";
      std::lock_guard lock(m);
      EXPECT_NO_FAILURES(check_common(c, r,
                                      {error_category::integrity, "", "", {"input.modified", "input.size_changed",
                                                                             "input.digest_mismatch", "input.consumed_digest"},
                                       false}));
    }
    // header cambiato fra il piano e la verifica: il file letto per il piano ha la card mutata, quello verificato e'
    // l'originale (ripristinato all'ingresso dello scheduler, sched.plan); stop integrity prima di usare un pixel.
    for (const std::pair<const char*, long long> mutation :
         {std::pair<const char*, long long>{"NAXIS1", cf::width - 1}, {"NAXIS2", cf::height - 1}, {"BITPIX", -cf::bitpix}}) {
      SCOPED_TRACE(mutation.first);
      type_case c(GetParam(), std::string("_header_") + mutation.first + "_" + role);
      const std::string path = c.inputs.at(role).path;
      const std::vector<std::uint8_t> original = read_bytes(path);
      std::vector<std::uint8_t> mutated = original;
      set_card(mutated, mutation.first, mutation.second);
      ASSERT_EQ(mutated.size(), original.size());
      overwrite(path, mutated);
      c.refresh(role);
      probe pr;
      std::atomic<bool> restored{false};
      pr.action = [&](std::string_view p, std::string_view) {
        if (p != "sched.plan" || restored.exchange(true)) return;
        overwrite(path, original);
        c.refresh(role);
      };
      cpu_test::reset_kernel_calls();
      const ran r = c.run_default();
      const stop_error* s = stop_of(r);
      EXPECT_TRUE(s != nullptr && s->category() == error_category::integrity) << describe(r);
      EXPECT_NO_FAILURES(check_common(c, r, {error_category::integrity, "", "", {}, false}));
      EXPECT_EQ(pr.count("sched.task"), 0u) << "task eseguiti con il piano di un header diverso";
      EXPECT_EQ(pr.count_details("input.samples", sample_ids(path, {original, mutated})), 0u)
          << "campioni dell'input usati prima del confronto dell'header";
      EXPECT_EQ(cpu_test::kernel_calls(cpu_path::scalar) + cpu_test::kernel_calls(cpu_path::avx2), 0u) << "pixel usati";
    }
  }
}

// ---------------------------------------------------------------- T-M3BASE-12 (OB-12)

// Input in sola lettura (0444): un job riuscito di ogni tipo non apre nulla in scrittura e lascia digest, dimensione,
// inode, nlink, mtime e permessi invariati. La verifica (e) gira anche dopo ogni arresto degli altri casi.
TEST_P(Conformance, T_M3BASE_12_originals_read_only) {
  type_case c(GetParam());
  for (const auto& [role, in] : c.inputs) fs::permissions(in.path, fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read);
  for (const auto& [role, in] : c.inputs) c.refresh(role);
  const ran r = c.run_default();
  EXPECT_TRUE(r.outcome.has_value() && r.outcome->exit_code == 0) << describe(r);
  for (const auto& [role, in] : c.inputs) EXPECT_EQ(state_of(in.path), c.expected.at(role)) << role;
}

// ---------------------------------------------------------------- T-M3BASE-14 (OB-14)

TEST_P(Conformance, T_M3BASE_14_nothing_published_on_error) {
  {  // arresto dopo un write_files riuscito dentro work; orfani rimossi alla riapertura
    type_case c(GetParam(), "_after_write");
    probe pr;
    std::vector<std::string> written;
    pr.after_work = [&](const job_record&, job_files_result& r) {
      for (const stored_file& f : r.files) written.push_back(f.rel_path());
      throw stop_error(error_category::io, "test.after_write", "arresto dopo la scrittura degli output");
    };
    const ran r = c.run_default();
    EXPECT_FALSE(written.empty()) << "il work dell'operatore non ha scritto output";
    EXPECT_NO_FAILURES(check_common(c, r, {error_category::io, "test.after_write", "", {}, false}));
    const fs::path root = c.store->root();
    c.store.reset();
    auto reopened = sqlite_dag_store::open(root, open_intent::read_write);
    for (const std::string& rel : written) {
      EXPECT_FALSE(fs::exists(root / "artifacts" / rel)) << "orfano non rimosso alla riapertura: " << rel;
    }
  }
  {  // generazione dello slot cambiata durante work
    type_case c(GetParam(), "_stale");
    probe pr;
    pr.after_work = [&](const job_record&, job_files_result&) { (void)c.store->retarget_slot(slot, c.job->node.key()); };
    const ran r = c.run_default();
    const std::vector<job_record> jobs = c.jobs();
    ASSERT_EQ(jobs.size(), 1u) << "un solo record di job";
    EXPECT_EQ(jobs[0].state, job_state::failed);
    EXPECT_EQ(jobs[0].stop_code, "dag.stale_publish");
    EXPECT_NE(c.store->read_slot(slot).state, freshness::current) << "slot pubblicato con generazione superata";
    EXPECT_EQ(c.store->get_slot(slot)->artifact_id, c.slot_artifact_before);
    bool outputs_as_cache = false;
    for (const artifact_record& a : c.new_artifacts()) {
      if (a.output_role != "provenance") {
        outputs_as_cache = true;
        continue;
      }
      EXPECT_NE(text_at(c.provenance_doc(a), "state"), "completed") << "riga provenance di un job completed";
    }
    EXPECT_TRUE(outputs_as_cache) << "output non registrati come cache del nodo";
    const stop_error* s = stop_of(r);
    ASSERT_NE(s, nullptr) << "nessun arresto con generazione superata: " << describe(r);
    EXPECT_EQ(s->category(), error_category::stale_generation);
    EXPECT_EQ(s->code(), "dag.stale_publish");
    const int exit = r.outcome ? r.outcome->exit_code : exit_code_for(s->category());
    EXPECT_EQ(exit, exit_code_stop);
  }
}

// ---------------------------------------------------------------- T-M3BASE-15 (OB-15)

namespace {

// Stop a meta' dei task (cancel al terzo blocco elaborato) con checkpoint a ogni task.
void stop_halfway(type_case& c) {
  probe pr;
  memory_pool pool(pool_bytes);
  cancel_token cancel;
  std::atomic<int> done{0};
  pr.action = [&](std::string_view p, std::string_view) {
    if (p == "sched.task_done" && ++done == 3) cancel.request();
  };
  const ran r = c.run(c.env(pool, cancel));
  const stop_error* s = stop_of(r);
  ASSERT_TRUE(s != nullptr && s->code() == "sched.cancelled") << describe(r);
  ASSERT_TRUE(read_checkpoint(c.dir / "scratch", "ckpt").has_value()) << "checkpoint assente dopo lo stop";
  c.snapshot();
}

struct resume_run {
  ran result;
  std::size_t tasks = 0;
};

resume_run resume(type_case& c, std::uint32_t tile_rows = 1) {
  probe pr;
  memory_pool pool(pool_bytes);
  cancel_token cancel;
  job_type_env e = c.env(pool, cancel);
  e.resume = true;
  e.resources.tile_rows = tile_rows;
  resume_run out;
  out.result = c.run(e);
  out.tasks = pr.count("sched.task");
  return out;
}

std::string primary_sha(const type_case& c) {
  const slot_view v = c.store->read_slot(slot);
  return v.artifact ? v.artifact->sha256 : std::string();
}

}  // namespace

TEST_P(ConformanceCheckpoint, T_M3BASE_15_task_resume_only_with_identical_recipe) {
  {
    type_case c(GetParam(), "_tile");
    stop_halfway(c);
    const resume_run r = resume(c, 2);
    EXPECT_NO_FAILURES(check_common(c, r.result, {error_category::resume_mismatch, "sched.resume_recipe", "", {}, false}));
    EXPECT_EQ(r.tasks, 0u);
  }
  {
    type_case c(GetParam(), "_backend");
    stop_halfway(c);
    checkpoint_record rec = *read_checkpoint(c.dir / "scratch", "ckpt");
    rec.backend = rec.backend == "cpu_scalar" ? "cpu_avx2" : "cpu_scalar";
    write_checkpoint(c.dir / "scratch", "ckpt", rec);
    const resume_run r = resume(c);
    EXPECT_NO_FAILURES(check_common(c, r.result, {error_category::resume_mismatch, "sched.resume_recipe", "", {}, false}));
    EXPECT_EQ(r.tasks, 0u);
  }
  {
    type_case c(GetParam(), "_partial");
    stop_halfway(c);
    const fs::path stage = c.dir / "scratch" / "ckpt.stage";
    ASSERT_TRUE(fs::exists(stage));
    std::vector<std::uint8_t> b = read_bytes(stage);
    for (std::uint8_t& x : b) x ^= 0x5a;
    overwrite(stage, b);
    const resume_run r = resume(c);
    EXPECT_NO_FAILURES(check_common(c, r.result, {error_category::integrity, "sched.checkpoint_digest", "", {}, false}));
    EXPECT_EQ(r.tasks, 0u);
  }
  {
    type_case c(GetParam(), "_nockpt");
    stop_halfway(c);
    remove_checkpoint(c.dir / "scratch", "ckpt");
    const resume_run r = resume(c);
    EXPECT_NO_FAILURES(check_common(c, r.result, {error_category::metadata_missing, "sched.checkpoint_missing", "", {}, false}));
    EXPECT_EQ(r.tasks, 0u);
  }
  {
    type_case uninterrupted(GetParam(), "_reference");
    const ran ref = uninterrupted.run_default();
    ASSERT_TRUE(ref.outcome.has_value() && ref.outcome->exit_code == 0) << describe(ref);
    type_case c(GetParam(), "_identical");
    stop_halfway(c);
    const std::size_t saved = read_checkpoint(c.dir / "scratch", "ckpt")->completed.size();
    const resume_run r = resume(c);
    ASSERT_TRUE(r.result.outcome.has_value() && r.result.outcome->exit_code == 0) << describe(r.result);
    EXPECT_EQ(r.tasks, cf::height - saved) << "task rieseguiti";
    const json* details = r.result.outcome->provenance.details ? &*r.result.outcome->provenance.details : nullptr;
    const json* sched = details != nullptr ? member(*details, "scheduler") : nullptr;
    ASSERT_NE(sched, nullptr) << "details.scheduler assente";
    EXPECT_EQ(static_cast<std::size_t>(member(*sched, "tasks_resumed")->as_integer()), saved);
    EXPECT_EQ(primary_sha(c), primary_sha(uninterrupted)) << "output diverso dall'esecuzione senza interruzioni";
  }
}

// ---------------------------------------------------------------- T-M3BASE-16 (OB-16)

namespace {

// Workflow con un passo per tipo registrato (prima variante), in un solo progetto.
struct multi_workflow {
  fs::path dir;
  std::unique_ptr<sqlite_dag_store> store;
  std::vector<minimal_job> jobs;
  std::vector<std::string> slots;
  memory_pool pool{pool_bytes};
  cancel_token cancel;
  std::string stop_slot;  // slot il cui work si ferma (vuoto: nessuno)

  explicit multi_workflow(const std::string& name) : dir(fresh(name)) {
    store = sqlite_dag_store::create(dir / "store");
    fs::create_directories(dir / "scratch");
    const auto& types = registered_job_types();
    for (std::size_t i = 0; i < types.size(); ++i) {
      const job_type_inputs in = cf::write_inputs(types[i], dir / ("inputs" + std::to_string(i)));
      jobs.push_back(types[i].make(in, types[i].variants[0].name, store->root()));
      slots.push_back("p" + std::to_string(i));
      store->put_node(jobs.back().node);
      store->create_slot(slots.back(), jobs.back().node.key());
    }
  }
  void reopen() {
    const fs::path root = store->root();
    store.reset();
    store = sqlite_dag_store::open(root, open_intent::read_write);
  }
  workflow make(const std::string& name = "tutti") {
    workflow wf{name, {}, nullptr};
    for (std::size_t i = 0; i < jobs.size(); ++i) {
      wf.steps.push_back(workflow_step{slots[i], jobs[i].node, [this, i] {
                                         resource_config r;
                                         r.scratch = dir_quota{dir / "scratch", 64 * MiB};
                                         r.worker_count = workers;
                                         r.tile_rows = 1;
                                         return jobs[i].run(job_type_env{*store, slots[i], pool, cancel, std::nullopt, r,
                                                                         "w" + std::to_string(i), false});
                                       }});
    }
    return wf;
  }
  // Primo passo eseguito, secondo fermato.
  void run_stopped_at_second(probe& pr) {
    stop_slot = slots[1];
    pr.before_work = [this](const job_record& job) {
      if (job.slot_name == stop_slot) throw stop_error(error_category::io, "test.step_stop", "arresto del passo");
    };
    testing::internal::CaptureStderr();
    const workflow_result r = run_workflow(*store, make());
    testing::internal::GetCapturedStderr();
    pr.before_work = nullptr;
    ASSERT_EQ(r.steps[0].action, step_action::executed);
    ASSERT_EQ(r.steps[1].action, step_action::executed);
    ASSERT_NE(r.exit_code, 0);
  }
  workflow_result resume_quiet(const workflow& wf) {
    testing::internal::CaptureStderr();
    try {
      workflow_result r = resume_workflow(*store, wf);
      testing::internal::GetCapturedStderr();
      return r;
    } catch (...) {
      testing::internal::GetCapturedStderr();
      throw;
    }
  }
  // Output presenti del primo passo (tutti i ruoli, provenance compresa), in ordine di id.
  std::vector<fs::path> first_outputs() const {
    std::vector<fs::path> out;
    for (const artifact_record& a : store->artifacts_for(jobs[0].node.key())) {
      if (a.present) out.push_back(store->artifact_path(a.rel_path));
    }
    if (out.empty()) throw std::runtime_error("primo passo senza output");
    return out;
  }
  std::uint64_t job_count() const {
    std::uint64_t n = 0;
    while (store->get_job(n + 1)) ++n;
    return n;
  }
};

}  // namespace

TEST(ConformanceWorkflow, T_M3BASE_16_workflow_resume_only_with_identical_definition) {
  {  // resume dopo l'arresto del secondo passo
    multi_workflow m("T16_resume");
    probe pr;
    m.run_stopped_at_second(pr);
    const workflow_result r = m.resume_quiet(m.make());
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_EQ(r.steps[0].action, step_action::reused);
    for (std::size_t i = 1; i < r.steps.size(); ++i) {
      ASSERT_EQ(r.steps[i].action, step_action::executed) << i;
      EXPECT_TRUE(r.steps[i].outcome->provenance.resume.has_value()) << "contesto di resume assente nel passo " << i;
    }
  }
  std::size_t outputs = 0;
  {
    multi_workflow m("T16_count");
    probe pr;
    m.run_stopped_at_second(pr);
    outputs = m.first_outputs().size();
  }
  ASSERT_GE(outputs, 2u);
  for (std::size_t k = 0; k < outputs; ++k) {
    SCOPED_TRACE("output " + std::to_string(k));
    {  // output k del primo passo rimosso prima della recovery: rieseguito
      multi_workflow m("T16_removed_before_" + std::to_string(k));
      probe pr;
      m.run_stopped_at_second(pr);
      const fs::path out = m.first_outputs().at(k);
      m.store.reset();
      fs::remove(out);
      m.store = sqlite_dag_store::open(m.dir / "store", open_intent::read_write);
      const workflow_result r = m.resume_quiet(m.make());
      EXPECT_EQ(r.steps[0].action, step_action::executed) << "primo passo non rieseguito";
    }
    {  // output k rimosso dopo la recovery: io resume.artifact_missing, nessun job nuovo
      multi_workflow m("T16_removed_after_" + std::to_string(k));
      probe pr;
      m.run_stopped_at_second(pr);
      const std::uint64_t jobs = m.job_count();
      fs::remove(m.first_outputs().at(k));
      try {
        (void)m.resume_quiet(m.make());
        ADD_FAILURE() << "resume.artifact_missing atteso";
      } catch (const stop_error& e) {
        EXPECT_EQ(e.category(), error_category::io);
        EXPECT_EQ(e.code(), "resume.artifact_missing");
      }
      EXPECT_EQ(m.job_count(), jobs) << "job nuovi dopo il rifiuto";
    }
    {  // output k alterato
      multi_workflow m("T16_altered_" + std::to_string(k));
      probe pr;
      m.run_stopped_at_second(pr);
      const std::uint64_t jobs = m.job_count();
      const fs::path out = m.first_outputs().at(k);
      std::vector<std::uint8_t> b = read_bytes(out);
      b[b.size() / 2] ^= 0x01;
      fs::permissions(out, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::add);
      overwrite(out, b);
      try {
        (void)m.resume_quiet(m.make());
        ADD_FAILURE() << "resume.artifact_digest atteso";
      } catch (const stop_error& e) {
        EXPECT_EQ(e.category(), error_category::integrity);
        EXPECT_EQ(e.code(), "resume.artifact_digest");
      }
      EXPECT_EQ(m.job_count(), jobs);
    }
  }
  {  // workflow non registrato
    multi_workflow m("T16_unknown");
    try {
      (void)m.resume_quiet(m.make("mai_eseguito"));
      ADD_FAILURE() << "resume.unknown_workflow atteso";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.category(), error_category::resume_mismatch);
      EXPECT_EQ(e.code(), "resume.unknown_workflow");
    }
    EXPECT_EQ(m.job_count(), 0u);
  }
}

// ---------------------------------------------------------------- T-M3BASE-17 (OB-17)

namespace {

struct env_guard {
  std::string name;
  std::optional<std::string> saved;
  env_guard(const char* n, const fs::path& value) : name(n) {
    if (const char* v = std::getenv(n)) saved = v;
    ::setenv(n, value.c_str(), 1);
  }
  ~env_guard() {
    if (saved) ::setenv(name.c_str(), saved->c_str(), 1);
    else ::unsetenv(name.c_str());
  }
};

}  // namespace

TEST_P(Conformance, T_M3BASE_17_temporaries_only_in_declared_directories) {
  type_case c(GetParam());
  const fs::path tmpdir = c.dir / "tmpdir", home = c.dir / "home";
  fs::create_directories(tmpdir);
  fs::create_directories(home);
  {
    const env_guard t("TMPDIR", tmpdir), h("HOME", home);
    probe pr;
    memory_pool pool(pool_bytes);
    cancel_token cancel;
    const ran ok = c.run(c.env(pool, cancel), false);
    EXPECT_TRUE(ok.outcome.has_value() && ok.outcome->exit_code == 0) << describe(ok);
    pr.after_work = [](const job_record&, job_files_result&) {
      throw stop_error(error_category::io, "test.injected", "arresto iniettato");
    };
    const ran stopped = c.run(c.env(pool, cancel), false);
    EXPECT_TRUE(stopped.outcome.has_value()) << describe(stopped);
  }
  EXPECT_TRUE(fs::is_empty(tmpdir)) << "file nella TMPDIR di sistema";
  EXPECT_TRUE(fs::is_empty(home)) << "file nella HOME";
}

// Regola sul sorgente: nessuna directory temporanea di sistema implicita in src/.
TEST(ConformanceSource, T_M3BASE_17_no_implicit_system_temporary_directory_in_src) {
  const fs::path src = fs::path(ASTRO_SOURCE_DIR) / "src";
  std::size_t files = 0;
  for (const auto& e : fs::recursive_directory_iterator(src)) {
    if (!e.is_regular_file()) continue;
    ++files;
    const std::string text = read_text(e.path());
    for (const char* banned : {"/tmp", "temp_directory_path", "TMPDIR", "mkstemp", "tmpfile"}) {
      EXPECT_EQ(text.find(banned), std::string::npos) << e.path() << " contiene " << banned;
    }
  }
  EXPECT_GT(files, 10u);
}

// ---------------------------------------------------------------- T-M3BASE-19 (OB-19)

TEST_P(ConformanceCheckpoint, T_M3BASE_19_checkpoint_removed_only_after_publication) {
  const auto present = [](const type_case& c) {
    return fs::exists(c.dir / "scratch" / "ckpt.ckpt.json") && fs::exists(c.dir / "scratch" / "ckpt.stage");
  };
  {  // stop a meta'
    type_case c(GetParam(), "_stop");
    probe pr;
    std::atomic<int> started{0};
    pr.action = [&](std::string_view p, std::string_view) {
      if (p == "sched.task" && ++started == 4) throw stop_error(error_category::io, "test.injected", "arresto a meta'");
    };
    const ran r = c.run_default();
    ASSERT_NE(stop_of(r), nullptr) << describe(r);
    EXPECT_TRUE(present(c)) << "checkpoint o staging rimossi dopo lo stop";
  }
  {  // cancel a meta'
    type_case c(GetParam(), "_cancel");
    stop_halfway(c);
    EXPECT_TRUE(present(c)) << "checkpoint o staging rimossi dopo il cancel";
  }
  {  // commit fallito prima della transazione: nulla pubblicato, checkpoint e staging restano
    type_case c(GetParam(), "_commit_fail");
    c.store->set_fault_injector([](std::string_view point) { return point == "commit.before_commit" ? EIO : 0; });
    const ran r = c.run_default();
    c.store->set_fault_injector(nullptr);
    ASSERT_NE(stop_of(r), nullptr) << describe(r);
    EXPECT_EQ(c.store->get_slot(slot)->artifact_id, c.slot_artifact_before) << "pubblicato nonostante il commit fallito";
    EXPECT_TRUE(present(c)) << "checkpoint o staging rimossi dopo un commit fallito";
  }
  {  // cancel al ritorno di work: nulla pubblicato, checkpoint e staging restano
    type_case c(GetParam(), "_cancel_after_work");
    probe pr;
    memory_pool pool(pool_bytes);
    cancel_token cancel;
    pr.after_work = [&](const job_record&, job_files_result&) { cancel.request(); };
    const ran r = c.run(c.env(pool, cancel));
    const stop_error* s = stop_of(r);
    ASSERT_TRUE(s != nullptr && s->code() == "sched.cancelled") << describe(r);
    EXPECT_TRUE(present(c)) << "checkpoint o staging rimossi dopo il cancel al ritorno di work";
  }
  {  // pubblicazione riuscita
    type_case c(GetParam(), "_publish");
    const ran r = c.run_default();
    ASSERT_TRUE(r.outcome.has_value() && r.outcome->exit_code == 0) << describe(r);
    EXPECT_FALSE(fs::exists(c.dir / "scratch" / "ckpt.ckpt.json")) << "checkpoint rimasto dopo la pubblicazione";
    EXPECT_FALSE(fs::exists(c.dir / "scratch" / "ckpt.stage")) << "staging rimasto dopo la pubblicazione";
  }
}

// OB-20: ogni immagine pubblicata da un job di ogni tipo registrato porta `measure` e rispetta le invarianti. I tipi che
// non pubblicano un'immagine (artefatto `image`) non hanno uscita da controllare.
TEST_P(Conformance, T_M3BASE_23_OB_20_published_images_carry_measure) {
  type_case c(GetParam());
  const ran r = c.run_default();
  ASSERT_TRUE(r.outcome.has_value() && r.outcome->exit_code == 0) << describe(r);
  for (const artifact_record& a : c.new_artifacts()) {
    if (a.kind != "image") continue;
    const std::string text = read_text(c.store->artifact_path(a.rel_path));
    const std::vector<std::uint8_t> bytes(text.begin(), text.end());
    const image out = decode_fits(bytes, std::uint64_t{1} << 40);
    EXPECT_TRUE(out.measure.has_value()) << "measure assente nell'uscita " << a.rel_path;
    EXPECT_TRUE(operator_output_invariants_hold(out)) << "invarianti violate dall'uscita " << a.rel_path;
  }
}

INSTANTIATE_TEST_SUITE_P(Types, Conformance, testing::ValuesIn(all_types()), param_name);
INSTANTIATE_TEST_SUITE_P(Types, ConformanceScheduler,
                         testing::ValuesIn(params_where([](const job_type_entry& e, const job_type_variant&) {
                           return e.scheduler;
                         })),
                         param_name);
INSTANTIATE_TEST_SUITE_P(Types, ConformanceCheckpoint,
                         testing::ValuesIn(params_where([](const job_type_entry&, const job_type_variant& v) {
                           return v.checkpoint;
                         })),
                         param_name);
