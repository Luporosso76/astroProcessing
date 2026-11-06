// U8a (M3.a), F-IO-03 CL-01…CL-04: proposta di classificazione, revisioni persistite, import batch.
// Prove: T-M3A-05 a, b, c (parte di persistenza), T-M3A-06 b (solo la riapertura del progetto dopo `import_batch`).
// Il resto di T-M3A-04, 05c, 05d e 06 (analisi, gruppi, candidati, `verify_selection`) è di U8b.
//
// Il file usa simboli che U8a deve ancora introdurre: compila solo con -DASTRO_U8A_TESTS=ON. Interfaccia attesa (nomi di
// funzione e di tabella dal contratto; i campi delle strutture sono la forma fissata da questi test):
//   astro/batch_classification.hpp
//     struct classification_proposal { role, rule_id, reason, json evidence };   // reason vuoto se role != unclassified
//     classification_proposal propose_classification(const json& fits_import_report);        // regola imagetyp.v1
//     classification_proposal propose_classification_raw();                                   // regola raw_no_type.v1
//     classification_proposal propose_classification_stopped(error_category, const std::string& code);  // import_stopped.v1
//     struct batch_import_result { input_record input; std::string slot_name; std::string job_type; branch_state state;
//                                  std::optional<stop_error> stop; classification_record classification; };
//     std::vector<batch_import_result> import_batch(sqlite_dag_store&, const std::vector<std::string>& absolute_paths,
//                                                   memory_pool&, const cancel_token&);
//   astro/sqlite_dag_store.hpp
//     struct classification_record { input_id, revision, role, source, optional<string> rule_id, evidence_canonical,
//                                    declarations_canonical };
//     std::vector<classification_record> sqlite_dag_store::append_classifications(const std::vector<classification_record>&);
//     std::vector<classification_record> sqlite_dag_store::classifications(std::uint64_t input_id) const;  // storia per revisione
// Evidenza (JSON canonico): imagetyp.v1 → {"imagetyp": <valore>}; keyword assente → {}; import_stopped.v1 →
// {"category": to_string(categoria), "code": <codice>}; raw_no_type.v1 → {}. Con role = unclassified l'evidenza porta
// anche "reason" (il contratto non ha una colonna del motivo); un `unclassified` dell'utente senza `reason = user` è `classify.role`.
// Scelte dei test non scritte nel contratto: dichiarazioni JSON valide ma non canoniche (spazi, chiavi fuori ordine) sono
// `classify.declaration`, senza canonicalizzare; `master_descriptor` e `frame_metadata` sono chiavi di primo livello del
// documento `astro.input_declarations.v1`; il punto di guasto `classify.<passo>` è uno per riga ed è chiamato subito prima
// dell'INSERT di quella riga (dopo la validazione dell'intero elenco): il guasto sulla terza riga prova così il rollback.
#include <gtest/gtest.h>

#include <cerrno>
#include <cstdlib>
#include <map>
#include <set>
#include <stdexcept>

#include "astro/batch_classification.hpp"
#include "astro/hash.hpp"
#include "astro/job.hpp"
#include "astro/json.hpp"
#include "astro/memory_pool.hpp"
#include "astro/scheduler.hpp"
#include "astro/sqlite_dag_store.hpp"
#include "import_support.hpp"
#include "pub_raw_01.hpp"
#include "store_test_support.hpp"
#include "syn_dng.hpp"
#include "syn_fits.hpp"

namespace {

using namespace astro;
namespace fs = std::filesystem;
namespace fx = astro_fixture;

constexpr const char* kDeclFormat = "astro.input_declarations.v1";

fs::path fresh(const std::string& name) {
  const fs::path dir = fs::path(ASTRO_TEST_TMP) / "u8a" / name;
  fs::remove_all(dir);
  fs::create_directories(dir);
  return dir;
}

const cancel_token& never() {
  static const cancel_token token;
  return token;
}

// FITS 8x8 con header di calibrazione; `imagetyp` assente se nullopt.
std::string write_fits(const fs::path& path, const std::optional<std::string>& imagetyp, double exposure = 60,
                       const std::string& phase = "raw") {
  fx::calib_header h;
  h.exposure = exposure;
  h.phase = phase;
  std::vector<std::string> cards;
  for (const std::string& c : fx::calib_cards(h)) {
    if (c.rfind("IMAGETYP", 0) == 0) continue;
    cards.push_back(c);
  }
  if (imagetyp) cards.push_back(fx::card_str("IMAGETYP", *imagetyp));
  imp::gen g;
  g.bitpix = 16;
  g.w = 8;
  g.h = 8;
  g.cards = cards;
  g.value = [](std::uint32_t, std::uint32_t x, std::uint32_t y) { return 100.0 + x + 8 * y; };
  imp::write_file(path, imp::make_fits(g));
  return path.string();
}

std::string write_dng(const fs::path& path) {
  fx::dng_spec s;
  s.width = 64;
  s.height = 48;
  s.active_area = fx::dng_active_area{0, 0, 32, 40};
  s.white = 4000;
  std::vector<std::uint16_t> v(64 * 48);
  for (std::uint32_t r = 0; r < 48; ++r)
    for (std::uint32_t c = 0; c < 64; ++c) v[r * 64 + c] = static_cast<std::uint16_t>(100 + (r * 37 + c * 11) % 3500);
  s.pixels = fx::pixels_u16(v);
  imp::write_file(path, fx::make_dng(s));
  return path.string();
}

// Percorso di un campione di PUB-RAW-01, con hash verificato contro il record; mai un salto.
std::string pub_raw_path(std::size_t index) {
  std::string name, error;
  std::vector<std::uint8_t> bytes;
  const bool ok = pub_raw::load(index, name, bytes, error);
  EXPECT_TRUE(ok) << error;
  if (!ok) throw std::runtime_error(error);
  return (fs::path(std::getenv("ASTRO_RAW_PIXLS_DIR")) / pub_raw::kSeeds[index].path).string();
}

std::unique_ptr<sqlite_dag_store> make_store(const fs::path& dir) { return sqlite_dag_store::create(dir / "project"); }

std::string node_operator(const sqlite_dag_store& store, const std::string& slot) {
  const auto s = store.get_slot(slot);
  EXPECT_TRUE(s.has_value()) << slot;
  if (!s) return "";
  const auto n = store.get_node(s->target_node_key);
  EXPECT_TRUE(n.has_value()) << slot;
  return n ? n->operator_id() : "";
}

json parse(const std::string& text) { return parse_json(text); }

std::string str_of(const json& obj, const std::string& key) {
  const json* v = obj.find(key);
  EXPECT_TRUE(v != nullptr) << key;
  return v ? v->as_string() : "<assente>";
}

std::string declarations_doc(const std::map<std::string, json>& declared) {
  json fields = json::object();
  for (const auto& [k, v] : declared) fields.set(k, v);
  json doc = json::object();
  doc.set("format", json::string(kDeclFormat));
  doc.set("declared_image_fields", std::move(fields));
  return canonical_json(doc);
}

classification_record user_record(std::uint64_t input_id, const std::string& role, const std::string& declarations = "") {
  classification_record r;
  r.input_id = input_id;
  r.role = role;
  r.source = "user";
  r.rule_id = std::nullopt;
  json ev = json::object();
  if (role == "unclassified") ev.set("reason", json::string("user"));
  r.evidence_canonical = canonical_json(ev);
  r.declarations_canonical = declarations.empty() ? declarations_doc({}) : declarations;
  return r;
}

// Righe di input_classifications lette con SQL diretto (progetto chiuso), nell'ordine (input_id, revision).
std::string rows_sql(const fs::path& project) {
  store_test::raw_db db(project, true);
  return db.query(
      "SELECT input_id, revision, role, source, rule_id, evidence_canonical, declarations_canonical, record_version "
      "FROM input_classifications ORDER BY input_id, revision");
}

std::int64_t row_count(const fs::path& project) {
  store_test::raw_db db(project, true);
  return std::stoll(db.query("SELECT COUNT(*) FROM input_classifications"));
}

struct imagetyp_case {
  const char* value;
  const char* role;
};
// Tabella scritta a mano da ADR-16.a: undici valori riconosciuti, confronto esatto dopo il passaggio a minuscole ASCII.
const imagetyp_case kRecognized[] = {
    {"light", "light"},       {"light frame", "light"}, {"dark", "dark"},      {"dark frame", "dark"},
    {"flat", "flat"},         {"flat field", "flat"},   {"flat frame", "flat"}, {"bias", "bias"},
    {"bias frame", "bias"},   {"dark flat", "dark_flat"}, {"flat dark", "dark_flat"}};

std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}
std::string mixed(const std::string& s) {
  std::string out = s;
  bool up = true;
  for (char& c : out) {
    if (c == ' ') {
      up = true;
      continue;
    }
    c = static_cast<char>(up ? std::toupper(static_cast<unsigned char>(c)) : std::tolower(static_cast<unsigned char>(c)));
    up = false;
  }
  return out;
}

void expect_rule_row(const classification_record& c, const std::string& role, const std::string& rule_id,
                     const std::string& reason, const std::string& context) {
  EXPECT_EQ(c.revision, 1u) << context;
  EXPECT_EQ(c.role, role) << context;
  EXPECT_EQ(c.source, "rule") << context;
  ASSERT_TRUE(c.rule_id.has_value()) << context;
  EXPECT_EQ(*c.rule_id, rule_id) << context;
  const json ev = parse(c.evidence_canonical);
  EXPECT_EQ(canonical_json(ev), c.evidence_canonical) << context;
  if (role == "unclassified") {
    EXPECT_EQ(str_of(ev, "reason"), reason) << context;
  } else {
    EXPECT_EQ(ev.find("reason"), nullptr) << context;
  }
  const json decl = parse(c.declarations_canonical);
  EXPECT_EQ(canonical_json(decl), c.declarations_canonical) << context;
  EXPECT_EQ(str_of(decl, "format"), kDeclFormat) << context;
  const json* fields = decl.find("declared_image_fields");
  if (fields != nullptr) {
    EXPECT_TRUE(fields->items().empty()) << context;
  }
  const json* fm = decl.find("frame_metadata");
  if (fm != nullptr) {
    EXPECT_TRUE(fm->items().empty()) << context;
  }
}

}  // namespace

// ------------------------------------------------------------------ T-M3A-05a: import_batch e proposta (CL-01, CL-04)

TEST(BatchClassification, T_M3A_05a_recognized_imagetyp_in_any_case) {
  const fs::path dir = fresh("t05a_values");
  auto store = make_store(dir);
  std::vector<std::string> paths;
  std::vector<std::pair<std::string, std::string>> expected;  // valore scritto, ruolo
  int n = 0;
  for (const imagetyp_case& c : kRecognized) {
    for (const std::string& value : {std::string(c.value), upper(c.value), mixed(c.value)}) {
      paths.push_back(write_fits(dir / ("f" + std::to_string(n++) + ".fits"), value));
      expected.emplace_back(value, c.role);
    }
  }
  memory_pool pool(64 * imp::MiB);
  const auto results = import_batch(*store, paths, pool, never());
  ASSERT_EQ(results.size(), paths.size());
  for (std::size_t i = 0; i < results.size(); ++i) {
    const std::string ctx = "IMAGETYP '" + expected[i].first + "'";
    EXPECT_EQ(results[i].input.path, paths[i]) << ctx;
    EXPECT_EQ(results[i].job_type, "io.import") << ctx;
    EXPECT_EQ(results[i].state, branch_state::completed) << ctx;
    EXPECT_FALSE(results[i].stop.has_value()) << ctx;
    EXPECT_EQ(node_operator(*store, results[i].slot_name), "io.import") << ctx;
    expect_rule_row(results[i].classification, expected[i].second, "imagetyp.v1", "", ctx);
    EXPECT_EQ(str_of(parse(results[i].classification.evidence_canonical), "imagetyp"), expected[i].first) << ctx;
    const auto hist = store->classifications(results[i].input.id);
    ASSERT_EQ(hist.size(), 1u) << ctx;
    EXPECT_EQ(hist[0].role, expected[i].second) << ctx;
  }
}

TEST(BatchClassification, T_M3A_05a_unrecognized_and_absent_imagetyp_are_unclassified_with_reason) {
  const fs::path dir = fresh("t05a_unrecognized");
  auto store = make_store(dir);
  const std::vector<std::string> bad = {"Master Dark", "darkflat", "Dark_Frame", " light", ""};
  std::vector<std::string> paths;
  for (std::size_t i = 0; i < bad.size(); ++i) paths.push_back(write_fits(dir / ("u" + std::to_string(i) + ".fits"), bad[i]));
  paths.push_back(write_fits(dir / "absent.fits", std::nullopt));
  memory_pool pool(64 * imp::MiB);
  const auto results = import_batch(*store, paths, pool, never());
  ASSERT_EQ(results.size(), paths.size());
  for (std::size_t i = 0; i < bad.size(); ++i) {
    const std::string ctx = "IMAGETYP '" + bad[i] + "'";
    expect_rule_row(results[i].classification, "unclassified", "imagetyp.v1", "imagetyp_unrecognized", ctx);
    EXPECT_EQ(str_of(parse(results[i].classification.evidence_canonical), "imagetyp"), bad[i]) << ctx;
  }
  const auto& absent = results.back().classification;
  expect_rule_row(absent, "unclassified", "imagetyp.v1", "imagetyp_absent", "keyword assente");
  EXPECT_EQ(parse(absent.evidence_canonical).find("imagetyp"), nullptr);
}

TEST(BatchClassification, T_M3A_05a_trailing_spaces_of_imagetyp_are_not_significant_H_IT1) {
  const fs::path dir = fresh("t05a_hit1");
  auto store = make_store(dir);
  memory_pool pool(64 * imp::MiB);
  const auto results = import_batch(*store, {write_fits(dir / "d.fits", "Dark Frame   ")}, pool, never());
  ASSERT_EQ(results.size(), 1u);
  expect_rule_row(results[0].classification, "dark", "imagetyp.v1", "", "H-IT1");
  EXPECT_EQ(str_of(parse(results[0].classification.evidence_canonical), "imagetyp"), "Dark Frame");
}

TEST(BatchClassification, T_M3A_05a_name_extension_and_exposure_never_classify) {
  const fs::path dir = fresh("t05a_nohints");
  auto store = make_store(dir);
  const std::vector<std::string> paths = {
      write_fits(dir / "dark_001.fits", "Light"),            // il nome dice dark, IMAGETYP light
      write_fits(dir / "flat_dark_bias.fits", std::nullopt),  // nome pieno di ruoli, IMAGETYP assente
      write_fits(dir / "dark_at_flat_exposure.fits", "Dark", 2.5),  // esposizione da flat: resta dark
      write_fits(dir / "looks_like_raw.cr2", "Flat"),         // FITS con estensione RAW
      write_dng(dir / "looks_like_fits.fits"),                // RAW con estensione FITS
  };
  memory_pool pool(64 * imp::MiB);
  const auto results = import_batch(*store, paths, pool, never());
  ASSERT_EQ(results.size(), paths.size());
  expect_rule_row(results[0].classification, "light", "imagetyp.v1", "", "dark_001.fits");
  expect_rule_row(results[1].classification, "unclassified", "imagetyp.v1", "imagetyp_absent", "nome da ruolo");
  expect_rule_row(results[2].classification, "dark", "imagetyp.v1", "", "dark all'esposizione di un flat");
  EXPECT_NE(results[2].classification.role, "dark_flat");
  expect_rule_row(results[3].classification, "flat", "imagetyp.v1", "", "FITS .cr2");
  EXPECT_EQ(results[3].job_type, "io.import");
  EXPECT_EQ(node_operator(*store, results[3].slot_name), "io.import");
  expect_rule_row(results[4].classification, "unclassified", "raw_no_type.v1", "raw_no_type_metadata", "RAW .fits");
  EXPECT_EQ(results[4].job_type, "io.raw_import");
  EXPECT_EQ(node_operator(*store, results[4].slot_name), "io.raw_import");
  // Contenuto, non estensione, decide il tipo di job.
  EXPECT_EQ(results[0].job_type, "io.import");
}

TEST(BatchClassification, T_M3A_05a_raw_files_are_unclassified_and_xtrans_stops_only_its_branch) {
  const fs::path dir = fresh("t05a_raw");
  auto store = make_store(dir);
  const std::vector<std::string> paths = {write_dng(dir / "syn.dng"), pub_raw_path(0), pub_raw_path(1), pub_raw_path(7),
                                          write_fits(dir / "after.fits", "Bias")};
  memory_pool pool(512 * imp::MiB);
  const auto results = import_batch(*store, paths, pool, never());
  ASSERT_EQ(results.size(), paths.size());
  for (std::size_t i = 0; i < 3; ++i) {
    const std::string ctx = paths[i];
    EXPECT_EQ(results[i].job_type, "io.raw_import") << ctx;
    EXPECT_EQ(results[i].state, branch_state::completed) << ctx;
    expect_rule_row(results[i].classification, "unclassified", "raw_no_type.v1", "raw_no_type_metadata", ctx);
  }
  const auto& xt = results[3];
  EXPECT_EQ(xt.job_type, "io.raw_import");
  EXPECT_EQ(xt.state, branch_state::failed);
  ASSERT_TRUE(xt.stop.has_value());
  EXPECT_EQ(xt.stop->code(), "raw.xtrans");
  expect_rule_row(xt.classification, "unclassified", "import_stopped.v1", "import_stopped", "X-Trans");
  const json ev = parse(xt.classification.evidence_canonical);
  EXPECT_EQ(str_of(ev, "code"), "raw.xtrans");
  EXPECT_EQ(str_of(ev, "category"), to_string(xt.stop->category()));
  EXPECT_EQ(results[4].state, branch_state::completed);  // gli altri rami completano
  expect_rule_row(results[4].classification, "bias", "imagetyp.v1", "", "ramo dopo lo stop");
  // L'input fermato è comunque registrato e ha la sua revisione 1.
  EXPECT_EQ(store->inputs().size(), paths.size());
  EXPECT_EQ(store->classifications(xt.input.id).size(), 1u);
}

TEST(BatchClassification, T_M3A_05a_import_batch_registers_inputs_with_digest_and_constant_role) {
  const fs::path dir = fresh("t05a_inputs");
  auto store = make_store(dir);
  const std::vector<std::string> paths = {write_fits(dir / "a.fits", "Light"), write_fits(dir / "b.fits", "Dark", 120)};
  memory_pool pool(64 * imp::MiB);
  const auto results = import_batch(*store, paths, pool, never());
  ASSERT_EQ(results.size(), 2u);
  const auto inputs = store->inputs();
  ASSERT_EQ(inputs.size(), 2u);
  for (std::size_t i = 0; i < 2; ++i) {
    const auto bytes = imp::read_file(paths[i]);
    EXPECT_EQ(inputs[i].path, paths[i]);  // nell'ordine dei percorsi dati
    EXPECT_EQ(inputs[i].sha256, sha256_hex(bytes));
    EXPECT_EQ(inputs[i].size_bytes, bytes.size());
    EXPECT_EQ(inputs[i].role, "batch");  // costante: inputs.role non è una fonte di classificazione (CL-03)
    EXPECT_EQ(results[i].input.id, inputs[i].id);
    EXPECT_EQ(results[i].classification.input_id, inputs[i].id);
  }
}

TEST(BatchClassification, T_M3A_05a_import_batch_rejects_relative_paths_and_writes_nothing) {
  const fs::path dir = fresh("t05a_relative");
  auto store = make_store(dir);
  write_fits(dir / "rel.fits", "Light");
  const fs::path cwd = fs::current_path();
  fs::current_path(dir);
  memory_pool pool(64 * imp::MiB);
  EXPECT_STOP(import_batch(*store, {"rel.fits"}, pool, never()), error_category::invalid_input, "store.bad_input");
  fs::current_path(cwd);
  EXPECT_TRUE(store->inputs().empty());
}

TEST(BatchClassification, T_M3A_05a_registration_fault_leaves_inputs_without_any_revision) {
  const fs::path dir = fresh("t05a_fault");
  const std::vector<std::string> paths = {write_fits(dir / "a.fits", "Light"), write_fits(dir / "b.fits", "Dark"),
                                          write_fits(dir / "c.fits", "Flat")};
  memory_pool pool(64 * imp::MiB);
  // Prova a secco su un altro progetto: un punto `classify.*` per riga, quindi tre.
  int dry = 0;
  {
    auto probe = sqlite_dag_store::create(dir / "probe");
    probe->set_fault_injector([&](std::string_view point) { return point.rfind("classify.", 0) == 0 ? (++dry, 0) : 0; });
    (void)import_batch(*probe, paths, pool, never());
  }
  EXPECT_EQ(dry, 3);
  // Guasto sull'ultima riga, dopo due scritture riuscite: la transazione le annulla.
  auto store = make_store(dir);
  int seen = 0;
  store->set_fault_injector([&](std::string_view point) { return point.rfind("classify.", 0) == 0 && ++seen == 3 ? EIO : 0; });
  testing::internal::CaptureStderr();
  try {
    (void)import_batch(*store, paths, pool, never());
    ADD_FAILURE() << "stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::io) << e.what();
  }
  testing::internal::GetCapturedStderr();
  EXPECT_EQ(seen, 3);
  EXPECT_EQ(store->inputs().size(), 3u);  // le revisioni entrano in una sola chiamata: o tutte o nessuna
  for (const input_record& in : store->inputs()) EXPECT_TRUE(store->classifications(in.id).empty());
  store.reset();
  EXPECT_EQ(row_count(dir / "project"), 0);
}

TEST(BatchClassification, T_M3A_05a_mixed_absolute_and_relative_list_is_rejected_without_revisions) {
  const fs::path dir = fresh("t05a_mixed");
  auto store = make_store(dir);
  const std::string absolute = write_fits(dir / "abs.fits", "Light");
  write_fits(dir / "rel.fits", "Dark");
  const fs::path cwd = fs::current_path();
  fs::current_path(dir);
  memory_pool pool(64 * imp::MiB);
  EXPECT_STOP(import_batch(*store, {absolute, "rel.fits"}, pool, never()), error_category::invalid_input, "store.bad_input");
  fs::current_path(cwd);
  for (const input_record& in : store->inputs()) EXPECT_TRUE(store->classifications(in.id).empty());
  store.reset();
  EXPECT_EQ(row_count(dir / "project"), 0);
}

// Annullamento di `import_batch` (CL-04: `run_branches`, un ramo per file, in una sola chiamata). Dopo un annullamento
// durante il ramo k i rami successivi restano `not_started` (scheduler.hpp, commento di `run_branches`): nessun job
// parte, quindi nessun record di job, nessuna provenance e generazione dello slot ancora 0 (valore di creazione).
// Il gancio è l'involucro del `work` (job_probe): al primo job richiede l'annullamento durante il suo lavoro.
struct cancel_on_first_work {
  explicit cancel_on_first_work(cancel_token& token) {
    job_probe::set_work_wrapper([&token, this](const job_record& job, const job_probe::work_fn& work) {
      ++calls;
      if (calls == 1) {
        first_slot = job.slot_name;
        token.request();
      }
      return work(job);
    });
  }
  ~cancel_on_first_work() { job_probe::set_work_wrapper(nullptr); }
  cancel_on_first_work(const cancel_on_first_work&) = delete;
  cancel_on_first_work& operator=(const cancel_on_first_work&) = delete;
  int calls = 0;
  std::string first_slot;
};

TEST(BatchClassification, T_M3A_05a_cancel_during_first_branch_leaves_later_branches_not_started) {
  const fs::path dir = fresh("t05a_cancel_mid");
  const std::vector<std::string> paths = {write_fits(dir / "a.fits", "Light"), write_fits(dir / "b.fits", "Dark"),
                                          write_fits(dir / "c.fits", "Flat")};
  memory_pool pool(64 * imp::MiB);
  std::vector<input_record> inputs;
  std::string first_slot;
  int work_calls = 0;
  {
    auto store = make_store(dir);
    cancel_token cancel;
    cancel_on_first_work hook(cancel);
    testing::internal::CaptureStderr();
    try {
      (void)import_batch(*store, paths, pool, cancel);
      ADD_FAILURE() << "stop atteso";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.category(), error_category::cancelled) << e.what();
      EXPECT_EQ(e.code(), "sched.cancelled");
    }
    testing::internal::GetCapturedStderr();
    work_calls = hook.calls;
    first_slot = hook.first_slot;
    inputs = store->inputs();
  }
  EXPECT_EQ(work_calls, 1);  // il lavoro dei rami 2 e 3 non parte
  ASSERT_EQ(inputs.size(), 3u);
  EXPECT_EQ(first_slot, "batch.input." + std::to_string(inputs[0].id));
  store_test::raw_db db(dir / "project", true);
  // Un solo record di job: quello del primo ramo, chiuso `cancelled`.
  EXPECT_EQ(db.query("SELECT slot_name, state, stop_code FROM jobs"), first_slot + "|cancelled|sched.cancelled|\n");
  // Una sola provenance in tutto lo store (la provenance è un artefatto del nodo): quella del nodo del primo slot.
  EXPECT_EQ(db.query("SELECT COUNT(*) FROM artifacts"), "1|\n");
  EXPECT_EQ(db.query("SELECT a.output_role FROM artifacts a JOIN slots s ON s.target_node_key = a.node_key WHERE s.slot_name='" +
                     first_slot + "'"),
            "provenance|\n");
  EXPECT_EQ(db.query("SELECT COUNT(*) FROM slots WHERE artifact_id IS NOT NULL"), "0|\n");
  // Slot 2 e 3: generazione ancora 0 e nessun job.
  for (std::size_t i = 1; i < 3; ++i) {
    const std::string slot = "batch.input." + std::to_string(inputs[i].id);
    EXPECT_EQ(db.query("SELECT current_generation FROM slots WHERE slot_name='" + slot + "'"), "0|\n") << slot;
    EXPECT_EQ(db.query("SELECT COUNT(*) FROM jobs WHERE slot_name='" + slot + "'"), "0|\n") << slot;
  }
  // Nessuna revisione di classificazione, nemmeno per l'input del ramo annullato.
  EXPECT_EQ(db.query("SELECT COUNT(*) FROM input_classifications"), "0|\n");
}

struct count_work_calls {
  count_work_calls() {
    job_probe::set_work_wrapper([this](const job_record& job, const job_probe::work_fn& work) {
      ++calls;
      return work(job);
    });
  }
  ~count_work_calls() { job_probe::set_work_wrapper(nullptr); }
  count_work_calls(const count_work_calls&) = delete;
  count_work_calls& operator=(const count_work_calls&) = delete;
  int calls = 0;
};

TEST(BatchClassification, T_M3A_05a_cancel_requested_before_the_call_opens_no_job) {
  const fs::path dir = fresh("t05a_cancel_before");
  const std::vector<std::string> paths = {write_fits(dir / "a.fits", "Light"), write_fits(dir / "b.fits", "Dark"),
                                          write_fits(dir / "c.fits", "Flat")};
  memory_pool pool(64 * imp::MiB);
  {
    auto store = make_store(dir);
    cancel_token cancel;
    cancel.request();
    count_work_calls probe;
    testing::internal::CaptureStderr();
    try {
      (void)import_batch(*store, paths, pool, cancel);
      ADD_FAILURE() << "stop atteso";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.category(), error_category::cancelled) << e.what();
      EXPECT_EQ(e.code(), "sched.cancelled");
    }
    testing::internal::GetCapturedStderr();
    EXPECT_EQ(probe.calls, 0);
    // Con annullamento già richiesto l'arresto esce dal calcolo del digest (`digest_of_file`), prima di `add_input` e
    // di `run_branches`: nessun input registrato.
    EXPECT_TRUE(store->inputs().empty());
  }
  store_test::raw_db db(dir / "project", true);
  EXPECT_EQ(db.query("SELECT COUNT(*) FROM jobs"), "0|\n");
  EXPECT_EQ(db.query("SELECT COUNT(*) FROM artifacts"), "0|\n");
  EXPECT_EQ(db.query("SELECT COUNT(*) FROM slots WHERE current_generation <> 0"), "0|\n");
  EXPECT_EQ(db.query("SELECT COUNT(*) FROM input_classifications"), "0|\n");
}

TEST(BatchClassification, T_M3A_06b_reopened_project_keeps_inputs_and_revisions_after_import_batch) {
  const fs::path dir = fresh("t06b_reopen");
  const std::vector<std::string> paths = {write_fits(dir / "a.fits", "Light"), write_dng(dir / "b.dng"), pub_raw_path(7),
                                          write_fits(dir / "c.fits", std::nullopt)};
  std::string before_logical, before_rows;
  std::vector<std::vector<classification_record>> before_hist;
  {
    auto store = make_store(dir);
    memory_pool pool(512 * imp::MiB);
    (void)import_batch(*store, paths, pool, never());
    for (const input_record& in : store->inputs()) before_hist.push_back(store->classifications(in.id));
  }
  before_logical = store_test::dump_logical(dir / "project");
  before_rows = rows_sql(dir / "project");
  EXPECT_EQ(row_count(dir / "project"), 4);
  auto reopened = sqlite_dag_store::open(dir / "project", open_intent::read_write);
  const auto inputs = reopened->inputs();
  ASSERT_EQ(inputs.size(), before_hist.size());
  for (std::size_t i = 0; i < inputs.size(); ++i) {
    const auto hist = reopened->classifications(inputs[i].id);
    ASSERT_EQ(hist.size(), before_hist[i].size());
    for (std::size_t r = 0; r < hist.size(); ++r) {
      EXPECT_EQ(hist[r].role, before_hist[i][r].role);
      EXPECT_EQ(hist[r].evidence_canonical, before_hist[i][r].evidence_canonical);
      EXPECT_EQ(hist[r].declarations_canonical, before_hist[i][r].declarations_canonical);
    }
  }
  reopened.reset();
  EXPECT_EQ(store_test::dump_logical(dir / "project"), before_logical);
  EXPECT_EQ(rows_sql(dir / "project"), before_rows);
}

// ------------------------------------------------------------------ T-M3A-05a: funzione pura di proposta

TEST(BatchClassification, T_M3A_05a_propose_classification_stopped_import_carries_category_and_code) {
  const classification_proposal p = propose_classification_stopped(error_category::incompatible, "raw.xtrans");
  EXPECT_EQ(p.role, "unclassified");
  EXPECT_EQ(p.rule_id, "import_stopped.v1");
  EXPECT_EQ(p.reason, "import_stopped");
  EXPECT_EQ(str_of(p.evidence, "category"), to_string(error_category::incompatible));
  EXPECT_EQ(str_of(p.evidence, "code"), "raw.xtrans");
}

TEST(BatchClassification, T_M3A_05a_propose_classification_raw_never_proposes_a_role) {
  const classification_proposal p = propose_classification_raw();
  EXPECT_EQ(p.role, "unclassified");
  EXPECT_EQ(p.rule_id, "raw_no_type.v1");
  EXPECT_EQ(p.reason, "raw_no_type_metadata");
}

// ------------------------------------------------------------------ T-M3A-05b: persistenza e correzione (CL-02, CL-03)

namespace {

struct registered {
  std::unique_ptr<sqlite_dag_store> store;
  fs::path project;
  std::vector<std::uint64_t> ids;
};

// Tre input registrati con la sola revisione 1 delle regole (light, dark, light).
registered register_three(const std::string& name) {
  const fs::path dir = fresh(name);
  registered r;
  r.project = dir / "project";
  r.store = make_store(dir);
  memory_pool pool(64 * imp::MiB);
  const auto res = import_batch(*r.store, {write_fits(dir / "a.fits", "Light"), write_fits(dir / "b.fits", "Dark", 90),
                                          write_fits(dir / "c.fits", "Light", 61)},
                                pool, never());
  for (const auto& x : res) r.ids.push_back(x.input.id);
  return r;
}

}  // namespace

TEST(BatchClassification, T_M3A_05b_user_corrections_are_new_revisions_and_history_is_intact) {
  registered r = register_three("t05b_user");
  const auto before = r.store->classifications(r.ids[0]);
  ASSERT_EQ(before.size(), 1u);
  const auto out = r.store->append_classifications(
      {user_record(r.ids[0], "flat"), user_record(r.ids[1], "dark_flat"), user_record(r.ids[2], "unclassified")});
  ASSERT_EQ(out.size(), 3u);
  for (const auto& c : out) {
    EXPECT_EQ(c.revision, 2u);
    EXPECT_EQ(c.source, "user");
    EXPECT_FALSE(c.rule_id.has_value());
  }
  const std::map<std::uint64_t, std::string> role = {{r.ids[0], "flat"}, {r.ids[1], "dark_flat"}, {r.ids[2], "unclassified"}};
  for (const std::uint64_t id : r.ids) {
    const auto hist = r.store->classifications(id);
    ASSERT_EQ(hist.size(), 2u);
    EXPECT_EQ(hist[0].revision, 1u);
    EXPECT_EQ(hist[0].source, "rule");  // la storia resta leggibile con origine e regola
    EXPECT_EQ(hist[0].rule_id.value_or(""), "imagetyp.v1");
    EXPECT_EQ(hist[1].revision, 2u);
    EXPECT_EQ(hist.back().role, role.at(id));  // effettiva = revisione di numero massimo
  }
  EXPECT_EQ(r.store->classifications(r.ids[0])[0].evidence_canonical, before[0].evidence_canonical);
  // Ogni correzione è una nuova revisione completa: una terza riporta un ruolo precedente senza alterare le prime due.
  const auto third = r.store->append_classifications({user_record(r.ids[0], "light")});
  EXPECT_EQ(third.front().revision, 3u);
  EXPECT_EQ(r.store->classifications(r.ids[0]).size(), 3u);
  EXPECT_EQ(r.store->classifications(r.ids[1]).size(), 2u);
  r.store.reset();
  const std::string rows = rows_sql(r.project);
  EXPECT_NE(rows.find("|user|<null>|"), std::string::npos) << rows;  // rule_id nullo per source = user
  EXPECT_EQ(std::count(rows.begin(), rows.end(), '\n'), 7);
  store_test::raw_db db(r.project, true);
  EXPECT_EQ(db.query("SELECT COUNT(*) FROM input_classifications WHERE record_version <> 1"), "0|\n");
  EXPECT_EQ(db.query("SELECT COUNT(*) FROM input_classifications WHERE source = 'rule' AND revision <> 1"), "0|\n");
}

TEST(BatchClassification, T_M3A_05b_declarations_are_stored_byte_for_byte_with_the_revision) {
  registered r = register_three("t05b_decl");
  const std::string decl = declarations_doc({{"binning_x", json::uinteger(2)}, {"binning_y", json::uinteger(2)}});
  (void)r.store->append_classifications({user_record(r.ids[0], "light", decl)});
  const auto hist = r.store->classifications(r.ids[0]);
  EXPECT_EQ(hist.back().declarations_canonical, decl);
  // Una revisione è completa e non porta un delta: la 1 resta senza dichiarazioni.
  EXPECT_EQ(hist.front().declarations_canonical, declarations_doc({}));
  r.store.reset();
  store_test::raw_db db(r.project, true);
  EXPECT_NE(db.query("SELECT declarations_canonical FROM input_classifications WHERE input_id = " +
                     std::to_string(r.ids[0]) + " AND revision = 2")
                .find(decl),
            std::string::npos);
}

TEST(BatchClassification, T_M3A_05b_store_fault_after_two_rows_writes_nothing) {
  registered r = register_three("t05b_fault");
  const auto three = [&](const char* role) {
    return std::vector<classification_record>{user_record(r.ids[0], role), user_record(r.ids[1], role), user_record(r.ids[2], role)};
  };
  // Prova a secco: un punto `classify.*` per riga, quindi tre.
  int dry = 0;
  r.store->set_fault_injector([&](std::string_view p) { return p.rfind("classify.", 0) == 0 ? (++dry, 0) : 0; });
  (void)r.store->append_classifications(three("flat"));
  EXPECT_EQ(dry, 3);
  r.store.reset();
  const std::string before = rows_sql(r.project);
  r.store = sqlite_dag_store::open(r.project, open_intent::read_write);
  // Guasto sulla terza riga, dopo due scritture riuscite: la transazione le annulla.
  int hit = 0;
  r.store->set_fault_injector([&](std::string_view p) { return p.rfind("classify.", 0) == 0 && ++hit == 3 ? EIO : 0; });
  testing::internal::CaptureStderr();
  try {
    (void)r.store->append_classifications(three("dark"));
    ADD_FAILURE() << "stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::io) << e.what();
  }
  testing::internal::GetCapturedStderr();
  EXPECT_EQ(hit, 3);
  r.store.reset();
  EXPECT_EQ(rows_sql(r.project), before);
}

TEST(BatchClassification, T_M3A_05b_update_and_delete_on_revisions_abort) {
  registered r = register_three("t05b_triggers");
  r.store.reset();
  const std::string before = rows_sql(r.project);
  store_test::raw_db db(r.project);
  EXPECT_NE(db.exec("UPDATE input_classifications SET role = 'flat'"), SQLITE_OK);
  EXPECT_NE(db.exec("UPDATE input_classifications SET revision = revision + 10"), SQLITE_OK);
  EXPECT_NE(db.exec("DELETE FROM input_classifications"), SQLITE_OK);
  EXPECT_NE(db.exec("DELETE FROM input_classifications WHERE input_id = " + std::to_string(r.ids[0])), SQLITE_OK);
  EXPECT_EQ(rows_sql(r.project), before);
}

TEST(BatchClassification, T_M3A_05b_reopen_keeps_identical_history_also_read_only) {
  registered r = register_three("t05b_reopen");
  (void)r.store->append_classifications({user_record(r.ids[1], "flat")});
  std::vector<std::vector<classification_record>> live;
  for (const std::uint64_t id : r.ids) live.push_back(r.store->classifications(id));
  r.store.reset();
  const std::string rows = rows_sql(r.project);
  for (const open_intent intent : {open_intent::read_write, open_intent::read_only}) {
    auto reopened = sqlite_dag_store::open(r.project, intent);
    for (std::size_t i = 0; i < r.ids.size(); ++i) {
      const auto hist = reopened->classifications(r.ids[i]);
      ASSERT_EQ(hist.size(), live[i].size());
      for (std::size_t k = 0; k < hist.size(); ++k) {
        EXPECT_EQ(hist[k].revision, live[i][k].revision);
        EXPECT_EQ(hist[k].role, live[i][k].role);
        EXPECT_EQ(hist[k].source, live[i][k].source);
        EXPECT_EQ(hist[k].rule_id, live[i][k].rule_id);
        EXPECT_EQ(hist[k].evidence_canonical, live[i][k].evidence_canonical);
        EXPECT_EQ(hist[k].declarations_canonical, live[i][k].declarations_canonical);
      }
    }
    reopened.reset();
    EXPECT_EQ(rows_sql(r.project), rows);
  }
}

TEST(BatchClassification, T_M3A_05b_read_only_store_refuses_to_append) {
  registered r = register_three("t05b_readonly");
  r.store.reset();
  auto ro = sqlite_dag_store::open(r.project, open_intent::read_only);
  EXPECT_STOP(ro->append_classifications({user_record(r.ids[0], "flat")}), error_category::schema, "store.read_only");
  EXPECT_EQ(ro->classifications(r.ids[0]).size(), 1u);
}

TEST(BatchClassification, T_M3A_05b_invalid_revisions_are_rejected_with_their_code_and_write_nothing) {
  registered r = register_three("t05b_errors");
  const auto expect_rejected = [&](std::vector<classification_record> list, const char* code, const char* what) {
    int before = 0;
    for (const std::uint64_t id : r.ids) before += static_cast<int>(r.store->classifications(id).size());
    SCOPED_TRACE(what);
    EXPECT_STOP(r.store->append_classifications(list), error_category::invalid_input, code);
    int after = 0;
    for (const std::uint64_t id : r.ids) after += static_cast<int>(r.store->classifications(id).size());
    EXPECT_EQ(before, after) << what;
  };
  // input_id sconosciuto: anche se la prima riga è valida non si scrive nulla.
  classification_record unknown = user_record(9999, "flat");
  expect_rejected({user_record(r.ids[0], "flat"), unknown}, "classify.unknown_input", "input sconosciuto");
  // Ruolo fuori elenco; rule_id con source user; rule_id assente con source rule.
  expect_rejected({user_record(r.ids[0], "sky")}, "classify.role", "ruolo fuori elenco");
  expect_rejected({user_record(r.ids[0], "")}, "classify.role", "ruolo vuoto");
  expect_rejected({user_record(r.ids[0], "Flat")}, "classify.role", "ruolo con maiuscola");
  classification_record user_with_rule = user_record(r.ids[0], "flat");
  user_with_rule.rule_id = "imagetyp.v1";
  expect_rejected({user_with_rule}, "classify.role", "source user con rule_id");
  classification_record rule_without_id = user_record(r.ids[0], "flat");
  rule_without_id.source = "rule";
  expect_rejected({rule_without_id}, "classify.role", "source rule senza rule_id");
  // Dichiarazioni: nessuna coercizione di tipo, formato errato, JSON non valido o non canonico.
  expect_rejected({user_record(r.ids[0], "light", declarations_doc({{"binning_x", json::string("2")}}))}, "classify.declaration",
                  "binning come stringa");
  expect_rejected({user_record(r.ids[0], "light", declarations_doc({{"binning_x", json::number(2.5)}}))}, "classify.declaration",
                  "binning frazionario");
  expect_rejected({user_record(r.ids[0], "light", declarations_doc({{"binning_x", json::integer(-1)}}))}, "classify.declaration",
                  "binning negativo");
  expect_rejected({user_record(r.ids[0], "light", declarations_doc({{"unit", json::integer(3)}}))}, "classify.declaration",
                  "unità non testuale");
  expect_rejected({user_record(r.ids[0], "light", "{\"declared_image_fields\":{},\"format\":\"astro.other.v1\"}")},
                  "classify.declaration", "formato errato");
  expect_rejected({user_record(r.ids[0], "light", "not json")}, "classify.declaration", "JSON non valido");
  // JSON valido ma non canonico (spazi, chiavi fuori ordine, numero scritto 2.0): `declarations_canonical` è JCS.
  expect_rejected({user_record(r.ids[0], "light", std::string("{ \"declared_image_fields\": {}, \"format\": \"") + kDeclFormat + "\" }")},
                  "classify.declaration", "con spazi");
  expect_rejected({user_record(r.ids[0], "light", std::string("{\"format\":\"") + kDeclFormat + "\",\"declared_image_fields\":{}}")},
                  "classify.declaration", "chiavi fuori ordine");
  expect_rejected({user_record(r.ids[0], "light", std::string("{\"declared_image_fields\":{\"binning_x\":2.0},\"format\":\"") + kDeclFormat + "\"}")},
                  "classify.declaration", "numero 2.0 (controllo di tipo, non di canonicità)");
  // Sezioni opzionali di tipo sbagliato per qualunque forma. Il descrittore sta su un ruolo di calibrazione (`dark`), dove CL-02 lo ammette:
  // l'unico motivo di rifiuto è la forma.
  expect_rejected({user_record(r.ids[0], "dark", std::string("{\"declared_image_fields\":{},\"format\":\"") + kDeclFormat + "\",\"master_descriptor\":\"x\"}")},
                  "classify.declaration", "master_descriptor non oggetto");
  expect_rejected({user_record(r.ids[0], "dark", std::string("{\"declared_image_fields\":{},\"format\":\"") + kDeclFormat + "\",\"master_descriptor\":{}}")},
                  "classify.declaration", "master_descriptor senza ruolo");
  expect_rejected({user_record(r.ids[0], "light", std::string("{\"declared_image_fields\":{},\"format\":\"") + kDeclFormat + "\",\"frame_metadata\":5}")},
                  "classify.declaration", "frame_metadata non oggetto");
  // Un unclassified dell'utente porta il motivo `user` (CL-02): senza, o con un altro motivo, è un ruolo non valido.
  classification_record no_reason = user_record(r.ids[0], "unclassified");
  no_reason.evidence_canonical = "{}";
  expect_rejected({no_reason}, "classify.role", "unclassified utente senza motivo");
  classification_record other_reason = user_record(r.ids[0], "unclassified");
  other_reason.evidence_canonical = "{\"reason\":\"imagetyp_absent\"}";
  expect_rejected({other_reason}, "classify.role", "unclassified utente con altro motivo");
  EXPECT_EQ(r.store->classifications(r.ids[0]).size(), 1u);
}

// Documento `astro.input_declarations.v1` con `declared_image_fields` e, se `section` non è vuota, una sezione opzionale.
std::string declarations_with(const char* section, json value, json fields = json::object()) {
  json doc = json::object();
  doc.set("format", json::string(kDeclFormat));
  doc.set("declared_image_fields", std::move(fields));
  if (section != nullptr) doc.set(section, std::move(value));
  return canonical_json(doc);
}

// Descrittore di un master esterno (ADR-16.a): `role` più i campi opzionali; i valori sono quelli di `descriptor_json`.
json master_descriptor(const std::string& role, const std::map<std::string, json>& extra = {}) {
  json d = json::object();
  d.set("role", json::string(role));
  for (const auto& [k, v] : extra) d.set(k, v);
  return d;
}

json full_master_descriptor(const std::string& role) {
  return master_descriptor(role, {{"additive", json::string("bias_included")},
                                  {"scaling", json::string("unscaled")},
                                  {"flat", json::null()},
                                  {"combine_count", json::uinteger(20)},
                                  {"combine_method", json::string("mean")},
                                  {"rejection", json::string("none")},
                                  {"variance_estimator", json::string("empirical")}});
}

TEST(BatchClassification, T_M3A_05b_valid_declarations_are_accepted_and_stored) {
  registered r = register_three("t05b_valid_decl");
  json fields = json::object();
  fields.set("binning_x", json::uinteger(2));
  fields.set("binning_y", json::uinteger(2));
  fields.set("unit", json::string("adu"));
  fields.set("linearity", json::string("linear"));
  json meta = json::object();
  meta.set("exposure_s", json::number(60.5));
  meta.set("gain", json::integer(100));
  meta.set("roi_x", json::integer(0));
  meta.set("filter", json::string("L"));
  meta.set("temperature_c", json::number(-10));
  meta.set("row_order", json::string("TOP-DOWN"));
  struct accepted {
    const char* what;
    std::string role;
    std::string declarations;
  };
  const std::vector<accepted> cases = {
      {"descrittore completo su dark", "dark", declarations_with("master_descriptor", full_master_descriptor("dark"), fields)},
      {"descrittore minimo su dark", "dark", declarations_with("master_descriptor", master_descriptor("dark"))},
      {"descrittore flat con valore di flat", "flat",
       declarations_with("master_descriptor", master_descriptor("flat", {{"flat", json::string("raw")}}))},
      {"descrittore bias", "bias", declarations_with("master_descriptor", master_descriptor("bias"))},
      {"descrittore dark_flat", "dark_flat", declarations_with("master_descriptor", master_descriptor("dark_flat"))},
      {"combine_method median e rejection sigma_clip", "dark",
       declarations_with("master_descriptor", master_descriptor("dark", {{"combine_method", json::string("median")},
                                                                         {"rejection", json::string("sigma_clip")}}))},
      {"combine_count al massimo uint32", "dark",
       declarations_with("master_descriptor", master_descriptor("dark", {{"combine_count", json::uinteger(4294967295ULL)}}))},
      {"frame_metadata completo su light", "light", declarations_with("frame_metadata", meta, fields)},
  };
  std::uint64_t expected = 1;
  for (const accepted& c : cases) {
    SCOPED_TRACE(c.what);
    classification_record rec = user_record(r.ids[0], c.role, c.declarations);
    std::vector<classification_record> written;
    ASSERT_NO_THROW(written = r.store->append_classifications({rec}));
    ASSERT_EQ(written.size(), 1u);
    EXPECT_EQ(written.front().revision, ++expected);
    EXPECT_EQ(r.store->classifications(r.ids[0]).back().declarations_canonical, c.declarations);
  }
}

TEST(BatchClassification, T_M3A_05b_master_descriptor_and_frame_metadata_shape_is_rejected_with_classify_declaration) {
  registered r = register_three("t05b_decl_shape");
  const auto rejected = [&](const char* what, const std::string& role, const std::string& decl) {
    SCOPED_TRACE(what);
    const std::size_t before = r.store->classifications(r.ids[0]).size();
    EXPECT_STOP(r.store->append_classifications({user_record(r.ids[0], role, decl)}), error_category::invalid_input,
                "classify.declaration");
    EXPECT_EQ(r.store->classifications(r.ids[0]).size(), before);
  };
  rejected("ruolo del descrittore diverso da quello della revisione", "dark",
           declarations_with("master_descriptor", master_descriptor("flat")));
  rejected("ruolo del descrittore non testuale", "dark",
           declarations_with("master_descriptor", [] { json d = json::object(); d.set("role", json::integer(1)); return d; }()));
  rejected("descrittore su light", "light", declarations_with("master_descriptor", master_descriptor("light")));
  rejected("descrittore su unclassified", "unclassified", declarations_with("master_descriptor", master_descriptor("unclassified")));
  rejected("chiave sconosciuta nel descrittore", "dark",
           declarations_with("master_descriptor", master_descriptor("dark", {{"bogus", json::string("x")}})));
  rejected("sezione di primo livello sconosciuta", "light", declarations_with("frame_meta", json::object()));
  rejected("roi_x frazionario", "light", declarations_with("frame_metadata", [] { json m = json::object(); m.set("roi_x", json::number(1.5)); return m; }()));
  rejected("roi_y frazionario", "light", declarations_with("frame_metadata", [] { json m = json::object(); m.set("roi_y", json::number(0.5)); return m; }()));
  rejected("chiave sconosciuta in frame_metadata", "light",
           declarations_with("frame_metadata", [] { json m = json::object(); m.set("bogus", json::integer(1)); return m; }()));
  rejected("campo numerico di frame_metadata come stringa", "light",
           declarations_with("frame_metadata", [] { json m = json::object(); m.set("exposure_s", json::string("60")); return m; }()));
  rejected("campo testuale di frame_metadata come numero", "light",
           declarations_with("frame_metadata", [] { json m = json::object(); m.set("filter", json::integer(1)); return m; }()));
  rejected("chiave sconosciuta in declared_image_fields", "light",
           declarations_with(nullptr, json::null(), [] { json f = json::object(); f.set("bogus", json::integer(1)); return f; }()));
}

TEST(BatchClassification, T_M3A_05b_closed_list_fields_reject_out_of_list_values_each_with_its_code) {
  registered r = register_three("t05b_decl_lists");
  const auto descriptor_rejected = [&](const char* key, json value) {
    SCOPED_TRACE(std::string("master_descriptor.") + key);
    const std::size_t before = r.store->classifications(r.ids[0]).size();
    EXPECT_STOP(r.store->append_classifications(
                    {user_record(r.ids[0], "dark", declarations_with("master_descriptor", master_descriptor("dark", {{key, std::move(value)}})))}),
                error_category::invalid_input, "classify.declaration");
    EXPECT_EQ(r.store->classifications(r.ids[0]).size(), before);
  };
  descriptor_rejected("additive", json::string("bias"));
  descriptor_rejected("additive", json::integer(1));
  descriptor_rejected("scaling", json::string("normalized"));
  descriptor_rejected("flat", json::string("normalized"));
  descriptor_rejected("flat", json::integer(1));
  descriptor_rejected("combine_method", json::string("sum"));
  descriptor_rejected("rejection", json::string("minmax"));
  descriptor_rejected("variance_estimator", json::string("model"));
  descriptor_rejected("variance_estimator", json::integer(1));
  descriptor_rejected("combine_count", json::integer(0));
  descriptor_rejected("combine_count", json::integer(-3));
  descriptor_rejected("combine_count", json::number(2.5));
  descriptor_rejected("combine_count", json::string("20"));
  descriptor_rejected("combine_count", json::uinteger(4294967296ULL));
  const auto field_rejected = [&](const char* key, json value) {
    SCOPED_TRACE(std::string("declared_image_fields.") + key);
    json fields = json::object();
    fields.set(key, std::move(value));
    EXPECT_STOP(r.store->append_classifications({user_record(r.ids[0], "light", declarations_with(nullptr, json::null(), fields))}),
                error_category::invalid_input, "classify.declaration");
  };
  field_rejected("unit", json::string("photon"));
  field_rejected("linearity", json::string("gamma"));
  field_rejected("binning_y", json::integer(0));
  field_rejected("binning_y", json::uinteger(4294967296ULL));
  EXPECT_EQ(r.store->classifications(r.ids[0]).size(), 1u);
}

TEST(BatchClassification, T_M3A_05b_undo_and_redo_do_not_touch_revisions) {
  const fs::path dir = fresh("t05b_undo");
  auto store = make_store(dir);
  store->apply_recipe(store_test::diamond(2).resolve(), store_test::diamond_slots());
  store->apply_recipe(store_test::diamond(5).resolve(), store_test::diamond_slots());
  memory_pool pool(64 * imp::MiB);
  const auto res = import_batch(*store, {write_fits(dir / "a.fits", "Dark")}, pool, never());
  const std::uint64_t id = res.front().input.id;
  (void)store->append_classifications({user_record(id, "flat")});
  const auto dump = [&] {
    std::string s;
    for (const auto& c : store->classifications(id)) {
      s += std::to_string(c.revision) + "|" + c.role + "|" + c.source + "|" + c.rule_id.value_or("<null>") + "|" + c.evidence_canonical +
           "|" + c.declarations_canonical + "\n";
    }
    return s;
  };
  const std::string before = dump();
  ASSERT_EQ(store->classifications(id).size(), 2u);
  EXPECT_FALSE(store->undo().empty());
  EXPECT_EQ(dump(), before);
  EXPECT_FALSE(store->redo().empty());
  EXPECT_EQ(dump(), before);
}

// ------------------------------------------------------------------ T-M3A-05c (parte di U8a): ruolo cambiato e conflitti

TEST(BatchClassification, T_M3A_05c_changed_role_is_the_effective_classification_and_rules_never_rewrite) {
  const fs::path dir = fresh("t05c_changed");
  auto store = make_store(dir);
  memory_pool pool(64 * imp::MiB);
  const auto first = import_batch(*store, {write_fits(dir / "dark.fits", "Dark"), write_fits(dir / "light.fits", "Light")}, pool, never());
  const std::uint64_t dark = first[0].input.id, light = first[1].input.id;
  (void)store->append_classifications({user_record(dark, "flat"), user_record(light, "unclassified")});
  EXPECT_EQ(store->classifications(dark).back().role, "flat");
  const auto light_hist = store->classifications(light);
  EXPECT_EQ(light_hist.back().role, "unclassified");
  EXPECT_EQ(str_of(parse(light_hist.back().evidence_canonical), "reason"), "user");
  // Nuovi input registrati dopo la correzione: le regole non toccano gli input esistenti.
  (void)import_batch(*store, {write_fits(dir / "more.fits", "Bias")}, pool, never());
  EXPECT_EQ(store->classifications(dark).size(), 2u);
  EXPECT_EQ(store->classifications(light).size(), 2u);
  EXPECT_EQ(store->classifications(dark).back().role, "flat");
  // Tornare a un ruolo precedente è una nuova revisione user.
  (void)store->append_classifications({user_record(dark, "dark")});
  const auto dark_hist = store->classifications(dark);
  ASSERT_EQ(dark_hist.size(), 3u);
  EXPECT_EQ(dark_hist.back().role, "dark");
  EXPECT_EQ(dark_hist.back().source, "user");
  EXPECT_EQ(dark_hist.back().revision, 3u);
}

TEST(BatchClassification, T_M3A_05c_correction_against_recognized_imagetyp_is_recorded_not_overridden) {
  const fs::path dir = fresh("t05c_conflict");
  auto store = make_store(dir);
  memory_pool pool(64 * imp::MiB);
  const auto res = import_batch(*store, {write_fits(dir / "master.fits", "Flat"), write_fits(dir / "light.fits", "Light"),
                                         write_fits(dir / "dark.fits", "Dark")},
                                pool, never());
  // Ruoli in conflitto con IMAGETYP (master e light) e dark_flat da IMAGETYP di dark: tutte registrate.
  const auto out = store->append_classifications(
      {user_record(res[0].input.id, "dark"), user_record(res[1].input.id, "flat"), user_record(res[2].input.id, "dark_flat")});
  ASSERT_EQ(out.size(), 3u);
  for (std::size_t i = 0; i < 3; ++i) {
    const auto hist = store->classifications(res[i].input.id);
    ASSERT_EQ(hist.size(), 2u);
    EXPECT_EQ(hist[0].role, res[i].classification.role);  // la proposta della regola resta in storia
    EXPECT_EQ(hist[1].source, "user");
  }
  EXPECT_EQ(store->classifications(res[0].input.id).back().role, "dark");
  EXPECT_EQ(store->classifications(res[1].input.id).back().role, "flat");
  EXPECT_EQ(store->classifications(res[2].input.id).back().role, "dark_flat");
  EXPECT_EQ(str_of(parse(store->classifications(res[0].input.id)[0].evidence_canonical), "imagetyp"), "Flat");
}
