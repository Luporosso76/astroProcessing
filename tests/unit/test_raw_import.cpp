// U4 (M3.a): job `io.raw_import` e record del decoder. T-M3A-RAW-07 (parte sul buffer), 09, 11, 12 (determinismo),
// 13 e T-M3A-02 (export F-IO-04) sul tipo registrato in `registered_job_types()`, senza simboli di prodotto nuovi: il
// tipo, il suo costruttore minimo e la provenance bastano. Chiavi del record: tests/unit/raw_record_keys.hpp.
// Valori attesi: dalla fixture (DNG scritto da tests/fixtures/syn_dng.hpp, indipendente da LibRaw e dal prodotto) o dal
// contratto (docs/contracts/ADR-08-decoder-pack.md §2.3, §2.4, §2.6, §2.9, §6; U12-third-party.md §1). Nessun valore
// calcolato chiamando il codice sotto test.
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>

#include "astro/fits.hpp"
#include "astro/hash.hpp"
#include "astro/job_probe.hpp"
#include "astro/job_types.hpp"
#include "astro/inventory.hpp"
#include "astro/json.hpp"
#include "astro/raw_decode.hpp"
#include "astro/raw_import_job.hpp"
#include "conformance_inputs.hpp"
#include "pub_raw_01.hpp"
#include "raw_record_keys.hpp"
#include "record_roundtrip.hpp"

namespace {

using namespace astro;
namespace fs = std::filesystem;
namespace fx = astro_fixture;
namespace rk = raw_record;

constexpr std::uint64_t MiB = std::uint64_t{1} << 20;

const job_type_entry* raw_entry() {
  for (const job_type_entry& e : registered_job_types()) {
    if (std::string(e.operator_id) == "io.raw_import") return &e;
  }
  return nullptr;
}

fs::path fresh(const std::string& name) {
  const fs::path dir = fs::absolute(fs::path(ASTRO_TEST_TMP) / "raw_import" / name);
  fs::remove_all(dir);
  fs::create_directories(dir);
  return dir;
}

std::string text_of(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::ostringstream o;
  o << in.rdbuf();
  return o.str();
}

// Osservatore dei punti del confine, ripristinato all'uscita.
struct probe {
  std::mutex m;
  std::vector<std::pair<std::string, std::string>> events;
  probe() {
    job_probe::set_observer([this](std::string_view p, std::string_view d) {
      std::lock_guard lock(m);
      events.emplace_back(std::string(p), std::string(d));
    });
  }
  ~probe() { job_probe::set_observer({}); }
  std::vector<std::string> details(const std::string& point) {
    std::lock_guard lock(m);
    std::vector<std::string> out;
    for (const auto& [p, d] : events) {
      if (p == point) out.push_back(d);
    }
    return out;
  }
};

struct raw_run {
  std::optional<job_outcome> outcome;
  std::optional<stop_error> thrown;
  std::string node_key;
  std::optional<json> provenance;
  std::optional<image> published;
  std::unique_ptr<sqlite_dag_store> store;
  const stop_error* stop() const {
    if (thrown) return &*thrown;
    if (outcome && outcome->stop) return &*outcome->stop;
    return nullptr;
  }
};

// Esegue il job minimo di `io.raw_import` sul file e legge la provenance e l'immagine pubblicata.
// Immagine pubblicata dal job: si legge con decode_fits, che notifica `input.samples` a sua volta; chi osserva gli eventi del
// job la legge dopo aver chiuso l'osservatore.
void load_published(raw_run& r) {
  for (const artifact_record& a : r.store->artifacts_for(r.node_key)) {
    if (a.output_role == "primary" && a.kind == "image") {
      const std::string text = text_of(r.store->artifact_path(a.rel_path));
      const std::vector<std::uint8_t> bytes(text.begin(), text.end());
      r.published = decode_fits(bytes, std::uint64_t{1} << 40);
    }
  }
}

raw_run run_raw(const fs::path& file, const content_digest& digest, const std::string& tag, bool read_published = true) {
  raw_run r;
  const job_type_entry* e = raw_entry();
  if (e == nullptr) {
    ADD_FAILURE() << "tipo io.raw_import assente dal registro dei tipi di job";
    return r;
  }
  const fs::path dir = fresh(tag);
  fs::create_directories(dir / "scratch");
  r.store = sqlite_dag_store::create(dir / "store");
  const job_type_inputs inputs{{"file", job_type_input{file.string(), digest}}};
  const minimal_job job = e->make(inputs, e->variants.at(0).name, r.store->root());
  r.node_key = job.node.key();
  r.store->put_node(job.node);
  r.store->create_slot("s", job.node.key());
  resource_config res;
  res.scratch = dir_quota{dir / "scratch", 64 * MiB};
  res.worker_count = 2;
  res.tile_rows = 1;
  memory_pool pool(256 * MiB);
  cancel_token cancel;
  testing::internal::CaptureStderr();
  try {
    r.outcome = job.run(job_type_env{*r.store, "s", pool, cancel, std::nullopt, res, "ckpt", false});
  } catch (const stop_error& ex) {
    r.thrown = ex;
  }
  testing::internal::GetCapturedStderr();
  for (const artifact_record& a : r.store->artifacts_for(job.node.key())) {
    if (a.output_role == "provenance") r.provenance = parse_json(text_of(r.store->artifact_path(a.rel_path)));
  }
  if (read_published) load_published(r);
  return r;
}

const json* member(const json& o, std::string_view key) {
  return o.type() == json::kind::object ? o.find(std::string(key)) : nullptr;
}

// Record del decoder: `details.decoder` della provenance del job riuscito.
const json* decoder_record(const raw_run& r) {
  if (!r.provenance) return nullptr;
  const json* details = member(*r.provenance, "details");
  return details != nullptr ? member(*details, "decoder") : nullptr;
}

#define ASSERT_RECORD(run, name)                                                                         \
  ASSERT_TRUE((run).outcome.has_value() && (run).outcome->exit_code == 0)                                 \
      << ((run).stop() != nullptr ? (run).stop()->what() : "nessun esito");                              \
  const json* name = decoder_record(run);                                                                 \
  ASSERT_NE(name, nullptr) << "provenance senza details.decoder"

std::string str(const json* o, std::string_view key) {
  const json* v = o != nullptr ? member(*o, key) : nullptr;
  return v != nullptr && v->type() == json::kind::string ? v->as_string() : "<assente o non stringa>";
}

bool is_null(const json* o, std::string_view key) {
  const json* v = o != nullptr ? member(*o, key) : nullptr;
  return v != nullptr && v->type() == json::kind::null;
}

std::int64_t num(const json* o, std::string_view key) {
  const json* v = o != nullptr ? member(*o, key) : nullptr;
  if (v == nullptr || v->type() != json::kind::integer) {
    ADD_FAILURE() << "chiave " << key << " assente o non intera";
    return -1;
  }
  return v->as_integer();
}

std::set<std::string> names_in(const json* o, std::string_view key) {
  std::set<std::string> out;
  const json* v = o != nullptr ? member(*o, key) : nullptr;
  if (v == nullptr || v->type() != json::kind::array) {
    ADD_FAILURE() << "chiave " << key << " assente o non array";
    return out;
  }
  for (const json& item : v->items()) out.insert(item.as_string());
  return out;
}

std::vector<std::int64_t> ints_in(const json* o, std::string_view key) {
  std::vector<std::int64_t> out;
  const json* v = o != nullptr ? member(*o, key) : nullptr;
  if (v == nullptr || v->type() != json::kind::array) {
    ADD_FAILURE() << "chiave " << key << " assente o non array";
    return out;
  }
  for (const json& item : v->items()) out.push_back(item.as_integer());
  return out;
}

// ---------------------------------------------------------------- fixture DNG (verità scritta dal test)

constexpr std::uint32_t raw_w = 64, raw_h = 48, area_w = 40, area_h = 32;

std::uint16_t written_sample(std::uint32_t r, std::uint32_t c) {
  return static_cast<std::uint16_t>(100 + (r * 37 + c * 11) % 3500);
}

struct dng_case {
  std::string name;
  fx::dng_spec spec;
  // Attesi dalla fixture.
  std::uint32_t x0, y0, w, h;       // crop nel buffer grezzo (§2.3)
  std::string pattern;              // cella 2x2 all'origine dell'area visibile
  std::string linearization;        // identity | decoder_curve
  std::set<std::string> present;    // campi interpretati presenti (§2.6)
};

std::string pattern_name(const std::vector<std::uint8_t>& cfa, std::uint32_t top, std::uint32_t left, std::uint32_t mt,
                         std::uint32_t ml) {
  // Colore del campione grezzo (r, c): cfa[((r - top) % 2) * 2 + (c - left) % 2] (DNG 1.6.0.0 p. 21).
  const auto color = [&](std::uint32_t r, std::uint32_t c) { return cfa[((r - top) % 2) * 2 + (c - left) % 2]; };
  const std::uint8_t cell[4] = {color(mt, ml), color(mt, ml + 1), color(mt + 1, ml), color(mt + 1, ml + 1)};
  const auto is = [&](std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d) {
    return cell[0] == a && cell[1] == b && cell[2] == c && cell[3] == d;
  };
  if (is(0, 1, 1, 2)) return "rggb";
  if (is(2, 1, 1, 0)) return "bggr";
  if (is(1, 0, 2, 1)) return "grbg";
  if (is(1, 2, 0, 1)) return "gbrg";
  return "invalid";
}

dng_case make_case(const std::string& name, const std::vector<std::uint8_t>& cfa, std::uint32_t ox, std::uint32_t oy) {
  dng_case c;
  c.name = name;
  fx::dng_spec& s = c.spec;
  s.width = raw_w;
  s.height = raw_h;
  s.cfa = cfa;
  s.active_area = fx::dng_active_area{oy, ox, oy + area_h, ox + area_w};
  s.white = 4000;
  std::vector<std::uint16_t> v(raw_w * raw_h);
  for (std::uint32_t r = 0; r < raw_h; ++r)
    for (std::uint32_t c2 = 0; c2 < raw_w; ++c2) v[r * raw_w + c2] = written_sample(r, c2);
  s.pixels = fx::pixels_u16(v);
  // ADR-08.b §2.3: su un asse con origine dispari il margine passa al pari successivo, la dimensione scende di 1, il
  // bordo opposto resta fermo.
  c.x0 = ox + ox % 2;
  c.y0 = oy + oy % 2;
  c.w = area_w - ox % 2;
  c.h = area_h - oy % 2;
  c.pattern = pattern_name(cfa, oy, ox, c.y0, c.x0);
  c.linearization = "identity";
  c.present = {"camera", "saturation_adu"};
  return c;
}

void with_metadata(dng_case& c) {
  c.spec.exposure_time = std::make_pair(std::uint32_t{60}, std::uint32_t{1});
  c.spec.iso = 100;
  c.spec.datetime_original = "2026:10:30 12:34:56";
  c.present = {"exposure_s", "gain", "capture_time_libraw", "camera", "saturation_adu"};
}

constexpr std::int64_t time_20261030_123456 = 1793363696;  // timegm dei campi scritti, fatto di ADR-08.b §2.6

std::vector<std::uint16_t> triple_table() {
  std::vector<std::uint16_t> t(4096);
  for (std::uint32_t i = 0; i < t.size(); ++i) t[i] = static_cast<std::uint16_t>(3 * i + 5);
  return t;
}

// SHA-256 della curva estesa a 65 536 voci con l'ultima voce, little-endian (ADR-08.b §2.4, regola «byte del digest»).
std::string expected_curve_digest(const std::vector<std::uint16_t>& table) {
  std::vector<std::uint8_t> bytes;
  for (std::uint32_t i = 0; i < 65536; ++i) {
    const std::uint16_t v = table[std::min<std::size_t>(i, table.size() - 1)];
    bytes.push_back(static_cast<std::uint8_t>(v & 0xff));
    bytes.push_back(static_cast<std::uint8_t>(v >> 8));
  }
  return sha256_hex(bytes);
}

std::vector<dng_case> cases() {
  std::vector<dng_case> out;
  {  // T-M3A-RAW-01: pattern e area attiva
    dng_case a = make_case("rggb_origin_0_0", {0, 1, 1, 2}, 0, 0);
    with_metadata(a);
    out.push_back(a);
    out.push_back(make_case("bggr_origin_1_1_no_metadata", {2, 1, 1, 0}, 1, 1));
    out.push_back(make_case("grbg_origin_2_2", {1, 0, 2, 1}, 2, 2));
    out.push_back(make_case("gbrg_origin_1_0", {1, 2, 0, 1}, 1, 0));
  }
  {  // T-M3A-RAW-02: nero per canale 2x2
    dng_case b = make_case("black_2x2", {0, 1, 1, 2}, 0, 0);
    b.spec.black_dim = std::make_pair<std::uint16_t, std::uint16_t>(2, 2);
    b.spec.black = {64, 72, 80, 96};
    out.push_back(b);
  }
  {  // T-M3A-RAW-04: LinearizationTable non identica
    dng_case d = make_case("linearization_triple", {0, 1, 1, 2}, 0, 0);
    d.spec.active_area.reset();
    std::vector<std::uint16_t> idx(raw_w * raw_h);
    for (std::uint32_t i = 0; i < idx.size(); ++i) idx[i] = static_cast<std::uint16_t>((i * 7) % 4096);
    idx[5] = 5000;
    d.spec.pixels = fx::pixels_u16(idx);
    d.spec.linearization = triple_table();
    d.spec.white = 12290;
    d.x0 = 0, d.y0 = 0, d.w = raw_w, d.h = raw_h;
    d.pattern = "rggb";
    d.linearization = "decoder_curve";
    out.push_back(d);
  }
  return out;
}

struct written_case {
  fs::path file;
  content_digest digest;
};

written_case write_case(const dng_case& c, const std::string& dir_name) {
  const fs::path dir = fresh("files_" + dir_name);
  const std::vector<std::uint8_t> bytes = fx::make_dng(c.spec);
  const fs::path p = dir / (c.name + ".dng");
  std::ofstream(p, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return {p, digest_of(bytes)};
}

// RAW-12 (chiave per campo) e RAW-13 (iniezione) stanno in test_raw_import_record.cpp, dietro ASTRO_U4_RECORD_TESTS: finché
// il header di U4 manca questo caso è rosso, così il gate non chiude senza quei casi.
TEST(RawImport, T_M3A_RAW_12_13_record_tests_target_is_buildable) {
#if defined(__has_include)
#if __has_include("astro/raw_import_job.hpp")
#if defined(ASTRO_U4_RECORD_TESTS_ENABLED) && ASTRO_U4_RECORD_TESTS_ENABLED
  SUCCEED();
#else
  FAIL() << "opzione ASTRO_U4_RECORD_TESTS spenta: test_raw_import_record (RAW-12 per campo, RAW-13) fuori dalla suite";
#endif
#else
  FAIL() << "astro/raw_import_job.hpp assente: test_raw_import_record.cpp (RAW-12 per campo, RAW-13 per iniezione) non compila";
#endif
#else
  FAIL() << "__has_include non disponibile";
#endif
}

// ---------------------------------------------------------------- T-M3A-RAW-07 (parte sul buffer)

// Lo SHA-256 identità dell'input è calcolato sullo stesso buffer che arriva alla decodifica (ADR-08.b §2.1): il punto
// `input.samples` della decodifica da memoria porta `sha256:<digest dei byte passati a open_buffer>`, uguale a quello
// del file; ogni apertura dichiara il formato `raw`.
TEST(RawImport, T_M3A_RAW_07_decoded_buffer_is_the_hashed_buffer) {
  const written_case w = write_case(cases().front(), "raw07");
  raw_run r;
  std::vector<std::string> samples, formats;
  {
    probe pr;
    r = run_raw(w.file, w.digest, "raw07", false);  // l'osservatore vede job e identificazione; `input.samples` lo notifica solo il job
    samples = pr.details("input.samples");
    formats = pr.details("input.open_format");
  }
  ASSERT_TRUE(r.outcome.has_value() && r.outcome->exit_code == 0) << (r.stop() ? r.stop()->what() : "nessun esito");
  ASSERT_EQ(samples.size(), 1u) << "la decodifica da memoria deve segnalare il proprio buffer una sola volta";
  EXPECT_EQ(samples[0], "sha256:" + w.digest.sha256);
  ASSERT_FALSE(formats.empty());
  for (const std::string& f : formats) EXPECT_EQ(f.rfind("raw\t", 0), 0u) << f;
  // Il buffer decodificato è quello di cui si è calcolato l'hash: l'immagine pubblicata coincide campione per campione
  // con la decodifica dei byte del file, che hanno quel digest.
  std::ifstream in(w.file, std::ios::binary);
  const std::vector<std::uint8_t> file_bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  ASSERT_EQ(digest_of(file_bytes).sha256, w.digest.sha256);
  load_published(r);
  ASSERT_TRUE(r.published.has_value());
  const image expected = decode_raw_buffer(file_bytes);
  EXPECT_EQ(r.published->geometry.width, expected.geometry.width);
  EXPECT_EQ(r.published->geometry.height, expected.geometry.height);
  ASSERT_EQ(r.published->samples.size(), expected.samples.size());
  EXPECT_EQ(std::memcmp(r.published->samples.data(), expected.samples.data(), expected.samples.size()), 0)
      << "i campioni pubblicati non sono quelli della decodifica dei byte con il digest del file";
}

// ---------------------------------------------------------------- T-M3A-RAW-11

// Insieme esatto del record di LibRaw: `all_keys` più l'instradamento, nessuna chiave del decoder del progetto (U13 §8).
void expect_libraw_record_keys(const json& rec) {
  std::vector<std::string_view> keys(rk::all_keys.begin(), rk::all_keys.end());
  keys.insert(keys.end(), rk::libraw_route_keys.begin(), rk::libraw_route_keys.end());
  record_roundtrip::expect_exact_keys(rec, keys);
}

TEST(RawImport, T_M3A_RAW_11_decoder_record_complete_for_synthetic_inputs) {
  for (const dng_case& c : cases()) {
    SCOPED_TRACE(c.name);
    const written_case w = write_case(c, "raw11_" + c.name);
    const raw_run r = run_raw(w.file, w.digest, "raw11_" + c.name);
    ASSERT_RECORD(r, rec);
    expect_libraw_record_keys(*rec);
    // Libreria (U12-third-party.md §1, §2, obblighi 5 e 6).
    EXPECT_EQ(str(rec, rk::libraw_version), rk::expected_libraw_version);
    EXPECT_EQ(num(rec, rk::libraw_version_number), rk::expected_libraw_version_number);
    EXPECT_EQ(num(rec, rk::libraw_capabilities), rk::expected_libraw_capabilities);
    EXPECT_EQ(str(rec, rk::libraw_tarball_sha256), rk::expected_libraw_tarball_sha256);
    // Nero e scala non applicati dal decoder; stato del dato.
    EXPECT_EQ(str(rec, rk::black_applied), "none");
    EXPECT_EQ(str(rec, rk::scale_applied), "none");
    EXPECT_EQ(str(rec, rk::data_state), "raw");
    EXPECT_EQ(str(rec, rk::camera_processing), "unknown");
    EXPECT_EQ(num(rec, rk::process_warnings), 0);
    // Geometria (§2.3).
    const json* crop = member(*rec, rk::crop);
    ASSERT_NE(crop, nullptr);
    EXPECT_EQ(num(crop, "x0"), c.x0);
    EXPECT_EQ(num(crop, "y0"), c.y0);
    EXPECT_EQ(num(crop, "w"), c.w);
    EXPECT_EQ(num(crop, "h"), c.h);
    EXPECT_EQ(ints_in(rec, rk::margin_parity), (std::vector<std::int64_t>{0, 0})) << "i margini dei DNG sono pari (§2.3)";
    EXPECT_EQ(str(rec, rk::cfa_pattern), c.pattern);
    // Campi strutturali di decoder_state (§2.9, emendamento U8b): dimensioni del buffer grezzo e indice colore dei siti.
    EXPECT_EQ(num(rec, rk::raw_width), c.spec.width);
    EXPECT_EQ(num(rec, rk::raw_height), c.spec.height);
    {
      const std::vector<std::int64_t> idx = ints_in(rec, rk::cfa_color_index);
      ASSERT_EQ(idx.size(), 4u);
      std::string letters;
      for (std::int64_t i : idx) {
        ASSERT_TRUE(i >= 0 && i <= 3) << "indice colore fuori da 0..3: " << i;
        letters += i == 0 ? 'r' : (i == 2 ? 'b' : 'g');  // 1 e 3 sono i due verdi (image.cpp: invariante di decoder_state)
      }
      EXPECT_EQ(letters, c.pattern) << "l'indice colore dei siti deve riprodurre il pattern dichiarato";
    }
    // Livelli (§2.4).
    EXPECT_EQ(num(member(*rec, rk::black) , "scalar"), 0);
    EXPECT_EQ(num(rec, rk::white), c.spec.white.value());
    EXPECT_EQ(str(rec, rk::linearization), c.linearization);
    if (c.linearization == "decoder_curve") {
      EXPECT_EQ(str(rec, rk::curve_sha256), expected_curve_digest(c.spec.linearization));
      // Digest scritto nel contratto (ADR-08.b §2.4) per la tabella `3i + 5` di 4096 voci.
      EXPECT_EQ(str(rec, rk::curve_sha256), "a3089d2ad8be7b57af4ad35ff19bd2aa94fca3ac23a0f676cff7f47a826b6662");
    } else {
      EXPECT_TRUE(is_null(rec, rk::curve_sha256));
    }
    if (c.spec.black_dim) {
      const json* black = member(*rec, rk::black);
      EXPECT_EQ(num(black, "pattern_rows"), 2);
      EXPECT_EQ(num(black, "pattern_cols"), 2);
      EXPECT_EQ(ints_in(black, "pattern"), (std::vector<std::int64_t>{64, 72, 80, 96}));
      EXPECT_EQ(ints_in(black, "per_color"), (std::vector<std::int64_t>{0, 0, 0, 0}));
    }
    // Metadati (§2.6): presenti solo quelli scritti.
    EXPECT_EQ(names_in(rec, rk::metadata_present), c.present);
    // Ora (§2.6, §2.9).
    EXPECT_EQ(str(rec, rk::capture_time_clock), "camera_no_zone");
    EXPECT_EQ(str(rec, rk::process_tz), "UTC0");
    if (c.present.count("capture_time_libraw") == 1) {
      EXPECT_EQ(num(rec, rk::capture_time_libraw), time_20261030_123456);
    } else {
      EXPECT_TRUE(is_null(rec, rk::capture_time_libraw)) << "senza DateTimeOriginal l'ora è null, non un valore di comodo";
    }
  }
}

// Andata e ritorno del record LibRaw (ADR-08.b §2.9, U13 §8, F-IO-03 CL-12): sui DNG sintetici di RAW-11 (pattern,
// origini pari e dispari, nero a motivo, LinearizationTable) e su due file non DNG di PUB-RAW-01 (Canon EOS 6D CR2,
// `tiff_not_dng`; Fujifilm X-A1 RAF, `not_tiff`, con avviso del parser e `raw_inset_crops` non nulli) lo stato
// ricostruito dal record è quello della decodifica; senza una chiave alla volta: raw.record_malformed con la chiave.
TEST(RawImport, T_M3A_RAW_11_record_rebuilds_the_libraw_decoder_state) {
  std::vector<std::pair<std::string, std::vector<std::uint8_t>>> files;
  for (const dng_case& c : cases()) files.emplace_back(c.name, fx::make_dng(c.spec));
  for (const std::size_t index : {std::size_t{0}, std::size_t{3}}) {
    std::string name, error;
    std::vector<std::uint8_t> bytes;
    ASSERT_TRUE(pub_raw::load(index, name, bytes, error)) << error;
    files.emplace_back(name, std::move(bytes));
  }
  bool saw_warning = false, saw_inset = false;
  for (const auto& [name, bytes] : files) {
    SCOPED_TRACE(name);
    const image img = decode_raw_buffer(bytes);
    ASSERT_TRUE(img.decoder_state.has_value());
    EXPECT_EQ(img.decoder_state->route.decoder, raw_decoder_kind::libraw);
    EXPECT_FALSE(img.decoder_state->dng.has_value());
    saw_warning = saw_warning || img.decoder_state->process_warnings != 0;
    saw_inset = saw_inset || img.decoder_state->raw_inset_crops != std::array<std::array<std::uint32_t, 4>, 2>{};
    record_roundtrip::expect_state_round_trip(img);
    const json rec = raw_decoder_record(img);
    for (std::string_view key : rk::state_common_keys) {
      SCOPED_TRACE(std::string(key));
      ASSERT_NE(member(rec, key), nullptr);
      record_roundtrip::expect_record_malformed(record_roundtrip::without_key(rec, key), key);
    }
    for (std::string_view key : rk::state_libraw_keys) {
      SCOPED_TRACE(std::string(key));
      ASSERT_NE(member(rec, key), nullptr);
      record_roundtrip::expect_record_malformed(record_roundtrip::without_key(rec, key), key);
    }
    expect_libraw_record_keys(rec);
    // Un valore fuori dominio per ramo: la chiave del contesto è quella del valore letto.
    using record_roundtrip::with_key;
    record_roundtrip::expect_record_malformed(with_key(rec, rk::decoder, json::string("dcraw")), "decoder");
    record_roundtrip::expect_record_malformed(with_key(rec, rk::decoder, json::string("astro_dng")), "dng_spec");
    for (const char* reason : {"unknown", "", "TIFF_NOT_DNG"}) {
      SCOPED_TRACE(std::string("route_reason = ") + reason);
      record_roundtrip::expect_record_malformed(with_key(rec, rk::route_reason, json::string(reason)), "route_reason");
    }
  }
  EXPECT_TRUE(saw_warning) << "nessun file con process_warnings non nullo: il membro non è attraversato";
  EXPECT_TRUE(saw_inset) << "nessun file con raw_inset_crops non nulli: il membro non è attraversato";
}

// ---------------------------------------------------------------- T-M3A-RAW-12 (determinismo; la chiave per campo è in test_raw_import_record)

TEST(RawImport, T_M3A_RAW_12_same_file_gives_same_record_and_key) {
  const written_case w = write_case(cases().front(), "raw12");
  const raw_run a = run_raw(w.file, w.digest, "raw12_a");
  const raw_run b = run_raw(w.file, w.digest, "raw12_b");
  ASSERT_RECORD(a, ra);
  ASSERT_RECORD(b, rb);
  EXPECT_EQ(canonical_json(*ra), canonical_json(*rb));
  EXPECT_EQ(a.node_key, b.node_key);
  ASSERT_TRUE(a.provenance.has_value());
  EXPECT_EQ(str(&*a.provenance, "node_key"), a.node_key) << "la chiave in provenance è quella del nodo costruito";
  // Un file con un solo campione diverso ha un altro digest: stessa libreria, chiave diversa.
  fx::dng_spec other = cases().front().spec;
  other.pixels[0] ^= 0x01;
  dng_case oc = cases().front();
  oc.spec = other;
  oc.name = "changed";
  const written_case w2 = write_case(oc, "raw12_changed");
  const raw_run c = run_raw(w2.file, w2.digest, "raw12_c");
  ASSERT_TRUE(c.outcome.has_value());
  EXPECT_NE(c.node_key, a.node_key);
}

// ---------------------------------------------------------------- T-M3A-RAW-13 e T-M3A-02

TEST(RawImport, T_M3A_RAW_13_absent_fields_are_null_with_reason) {
  for (const dng_case& c : cases()) {
    SCOPED_TRACE(c.name);
    const written_case w = write_case(c, "raw13_" + c.name);
    const raw_run r = run_raw(w.file, w.digest, "raw13_" + c.name);
    ASSERT_RECORD(r, rec);
    for (std::string_view key : {rk::sensor, rk::pixel_size_um, rk::pixel_scale_arcsec, rk::wcs}) {
      EXPECT_NE(member(*rec, key), nullptr) << "chiave assente: " << key;
      EXPECT_TRUE(is_null(rec, key)) << key << " deve essere JSON null (ADR-08.b §2.6, emendamento L4)";
    }
    EXPECT_EQ(str(rec, rk::absent_reason), "not_provided_by_decoder");
  }
}

// T-M3A-02 (F-IO-04, plans/M3a.md U4): export della provenance. Hash della sorgente, offset applicato `none`, campi
// assenti che restano assenti (record e immagine pubblicata), campi presenti con il valore scritto nella fixture.
TEST(RawImport, T_M3A_02_export_has_source_hash_black_none_and_absent_stays_absent) {
  const std::vector<dng_case> all = cases();
  dng_case with = all.front();            // con esposizione, ISO e ora
  dng_case without = all.at(1);           // senza
  for (const dng_case* c : {&with, &without}) {
    SCOPED_TRACE(c->name);
    const written_case w = write_case(*c, "t02_" + c->name);
    const raw_run r = run_raw(w.file, w.digest, "t02_" + c->name);
    ASSERT_RECORD(r, rec);
    // Hash della sorgente: nel record di provenance e nel nodo.
    const json* digests = member(*r.provenance, "input_digests");
    ASSERT_TRUE(digests != nullptr && digests->type() == json::kind::array && digests->items().size() == 1);
    EXPECT_EQ(str(&digests->items()[0], "sha256"), w.digest.sha256);
    EXPECT_EQ(num(&digests->items()[0], "size_bytes"), static_cast<std::int64_t>(w.digest.size_bytes));
    const json* inputs = member(*member(*r.provenance, "node_spec"), "inputs");
    ASSERT_TRUE(inputs != nullptr && inputs->items().size() == 1);
    EXPECT_EQ(str(&inputs->items()[0], "ref"), "input:" + w.digest.sha256 + ":" + std::to_string(w.digest.size_bytes));
    EXPECT_EQ(str(rec, rk::black_applied), "none");
    ASSERT_TRUE(r.published.has_value()) << "immagine pubblicata assente";
    const frame_metadata& m = r.published->metadata;
    EXPECT_FALSE(m.temperature_c.has_value()) << "il DNG sintetico non porta la temperatura: resta assente";
    EXPECT_TRUE(m.camera.has_value());
    if (c == &with) {
      EXPECT_EQ(names_in(rec, rk::metadata_present), c->present);
      ASSERT_TRUE(m.exposure_s.has_value());
      EXPECT_EQ(m.exposure_s->value, 60.0);
      ASSERT_TRUE(m.gain.has_value());
      EXPECT_EQ(m.gain->value, 100.0);
      ASSERT_TRUE(m.capture_time_libraw.has_value());
      EXPECT_EQ(m.capture_time_libraw->value, time_20261030_123456);
    } else {
      EXPECT_EQ(names_in(rec, rk::metadata_present), c->present);
      EXPECT_FALSE(m.exposure_s.has_value()) << "esposizione non scritta: resta assente, nessun valore di comodo";
      EXPECT_FALSE(m.gain.has_value());
      EXPECT_FALSE(m.capture_time_libraw.has_value());
      EXPECT_TRUE(is_null(rec, rk::capture_time_libraw));
    }
  }
}

// ---------------------------------------------------------------- T-M3A-RAW-09 (PUB-RAW-01, solo coerenza)

// Indici di tests/unit/pub_raw_01.hpp: 0 Canon, 1 Nikon, 2 Sony, 3 Fujifilm X-A1, 4 Olympus, 5 Pentax, 6 Panasonic,
// 7 Fujifilm X-E1 (X-Trans).
struct real_case {
  std::size_t index;
  const char* brand;
  bool temperature;   // `temperature_c` presente: solo Olympus E-M10 (ADR-08.b §6, «Copertura dei DNG sintetici»)
  bool linear_max;    // `linear_max` non nullo: Canon, Nikon, Sony, Olympus, Panasonic
  bool black_per_color;  // `cblack[0..3]` non nulli: Pentax K-x
  unsigned warnings;  // process_warnings: X-A1 0x10000 (PARSEFUJI_PROCESSED, §2.5)
};

const real_case real_cases[] = {
    {0, "canon", false, true, false, 0},   {1, "nikon", false, true, false, 0},  {2, "sony", false, true, false, 0},
    {3, "fujifilm_xa1", false, false, false, 0x10000}, {4, "olympus", true, true, false, 0},
    {5, "pentax", false, false, true, 0},  {6, "panasonic", false, true, false, 0},
};

fs::path real_path(std::size_t index) {
  const char* dir = std::getenv("ASTRO_RAW_PIXLS_DIR");
  if (dir == nullptr || *dir == '\0') {
    ADD_FAILURE() << "ASTRO_RAW_PIXLS_DIR assente: i campioni di PUB-RAW-01 sono richiesti, il test non salta";
    return {};
  }
  return fs::path(dir) / pub_raw::kSeeds[index].path;
}

TEST(RawImport, T_M3A_RAW_09_pub_raw_01_bayer_samples_import) {
  for (const real_case& c : real_cases) {
    SCOPED_TRACE(c.brand);
    std::string name, error;
    std::vector<std::uint8_t> bytes;
    ASSERT_TRUE(pub_raw::load(c.index, name, bytes, error)) << error;
    const raw_run r = run_raw(real_path(c.index), digest_of(bytes), std::string("raw09_") + c.brand);
    ASSERT_RECORD(r, rec);
    expect_libraw_record_keys(*rec);
    for (std::string_view key : {rk::sensor, rk::pixel_size_um, rk::pixel_scale_arcsec, rk::wcs}) {
      EXPECT_TRUE(is_null(rec, key)) << key << " deve essere JSON null (L4)";
    }
    EXPECT_EQ(str(rec, rk::absent_reason), "not_provided_by_decoder");
    {  // «metadati con origine», stato raw e layout cfa: sull'immagine del decoder
      const image dec = decode_raw_buffer(bytes);
      ASSERT_TRUE(dec.phase.has_value());
      EXPECT_EQ(dec.phase->value, process_phase::raw);
      EXPECT_TRUE(dec.cfa.has_value());
      ASSERT_TRUE(dec.metadata.camera.has_value());
      EXPECT_EQ(dec.metadata.camera->source.origin, metadata_origin::decoder);
    }
    const std::string pattern = str(rec, rk::cfa_pattern);
    EXPECT_TRUE(pattern == "rggb" || pattern == "bggr" || pattern == "grbg" || pattern == "gbrg") << pattern;
    EXPECT_EQ(str(rec, rk::data_state), "raw");
    EXPECT_EQ(str(rec, rk::black_applied), "none");
    EXPECT_GT(num(rec, rk::white), 0);
    EXPECT_EQ(num(rec, rk::process_warnings), static_cast<std::int64_t>(c.warnings));
    EXPECT_EQ(names_in(rec, rk::metadata_present).count("temperature_c") == 1, c.temperature);
    EXPECT_TRUE(names_in(rec, rk::metadata_present).count("camera") == 1);
    EXPECT_EQ(!is_null(rec, rk::linear_max), c.linear_max);
    if (!is_null(rec, rk::linear_max)) {
      const std::vector<std::int64_t> lm = ints_in(rec, rk::linear_max);
      ASSERT_EQ(lm.size(), 4u);
      for (std::int64_t v : lm) EXPECT_GT(v, 0);
    }
    const std::vector<std::int64_t> per_color = ints_in(member(*rec, rk::black), "per_color");
    ASSERT_EQ(per_color.size(), 4u);
    EXPECT_EQ(std::any_of(per_color.begin(), per_color.end(), [](std::int64_t v) { return v != 0; }), c.black_per_color);
    // Solo coerenza dei campioni: nell'intervallo di uint16, dimensioni uguali al crop del record.
    ASSERT_TRUE(r.published.has_value());
    ASSERT_TRUE(r.published->phase.has_value());
    EXPECT_EQ(r.published->phase->value, process_phase::raw);
    EXPECT_TRUE(r.published->cfa.has_value());
    const json* crop = member(*rec, rk::crop);
    EXPECT_EQ(r.published->geometry.width, static_cast<std::uint32_t>(num(crop, "w")));
    EXPECT_EQ(r.published->geometry.height, static_cast<std::uint32_t>(num(crop, "h")));
    const float* p = r.published->data<float>();
    const std::size_t n = static_cast<std::size_t>(r.published->geometry.width) * r.published->geometry.height;
    float lo = p[0], hi = p[0];
    for (std::size_t i = 0; i < n; ++i) lo = std::min(lo, p[i]), hi = std::max(hi, p[i]);
    EXPECT_GE(lo, 0.0f);
    EXPECT_LE(hi, 65535.0f);
  }
}

// X-E1 (X-Trans): stop controllato `unsupported`/`raw.xtrans`, nessuna immagine.
TEST(RawImport, T_M3A_RAW_09_xtrans_stops_with_raw_xtrans) {
  std::string name, error;
  std::vector<std::uint8_t> bytes;
  ASSERT_TRUE(pub_raw::load(7, name, bytes, error)) << error;
  const raw_run r = run_raw(real_path(7), digest_of(bytes), "raw09_xtrans");
  const stop_error* s = r.stop();
  ASSERT_NE(s, nullptr) << "X-Trans importato: atteso lo stop raw.xtrans";
  EXPECT_EQ(s->category(), error_category::unsupported);
  EXPECT_EQ(s->code(), "raw.xtrans");
  EXPECT_FALSE(r.published.has_value());
  if (r.outcome) {
    EXPECT_EQ(r.outcome->exit_code, exit_code_for(error_category::unsupported));
  }
}

// ---------------------------------------------------------------- job diretto: budget, identificazione, picco (OB-08, OB-09)

// Esegue `run_raw_import_job` con un pool di `budget` byte, nel thread di un future con tempo massimo: un blocco senza fine
// è un guasto del test, non un'attesa. Il cancel parte solo allo scadere del tempo, per liberare il thread.
struct direct_run {
  std::optional<job_outcome> outcome;
  std::optional<stop_error> request_stop;     // identification_stop della richiesta
  std::uint64_t pool_peak = 0;
  bool timed_out = false;
  std::vector<std::string> opens;             // eventi `input.open` visti dal solo job
  std::unique_ptr<sqlite_dag_store> store;
  std::string node_key;
};

direct_run run_direct(const fs::path& file, const content_digest& digest, std::uint64_t budget, const std::string& tag) {
  direct_run d;
  const raw_import_request request = make_raw_import_request(file.string(), digest);  // fuori dall'osservatore
  d.request_stop = request.identification_stop;
  d.node_key = request.node.key();
  const fs::path dir = fresh(tag);
  d.store = sqlite_dag_store::create(dir / "store");
  d.store->put_node(request.node);
  d.store->create_slot("s", d.node_key);
  memory_pool pool(budget);
  cancel_token cancel;
  probe pr;
  testing::internal::CaptureStderr();
  auto done = std::async(std::launch::async, [&] { return run_raw_import_job(*d.store, "s", request, pool, cancel); });
  if (done.wait_for(std::chrono::seconds(30)) == std::future_status::timeout) {
    d.timed_out = true;
    cancel.request();
  }
  try {
    d.outcome = done.get();
  } catch (const stop_error&) {
    if (!d.timed_out) throw;
  }
  testing::internal::GetCapturedStderr();
  d.opens = pr.details("input.open");
  d.pool_peak = pool.peak_reserved_bytes();
  return d;
}

// Byte che il decoder prenota nel pool (§2.8) per questo file: letti dal piano subito prima di `unpack`.
std::uint64_t decoder_reservation(const std::vector<std::uint8_t>& bytes) {
  memory_pool pool(256 * MiB);
  std::uint64_t reserved = 0;
  raw_decode_hooks hooks;
  hooks.before_unpack_plan = [&reserved](const raw_unpack_plan& p) { reserved = p.reserved_bytes; };
  const raw_decode_context ctx{&pool, nullptr, raw_limits::max_pixels, bytes.size(), std::nullopt};
  (void)decode_raw_buffer(bytes, hooks, ctx);
  return reserved;
}

// OB-08, ADR-08.b §2.8: i byte tenuti dal job (condiviso del digest + file) più la prenotazione del decoder sono il minimo
// del job. Un solo byte sotto: stop `budget` `sched.job_exceeds_budget` dal decoder, prima di `unpack`, senza attesa; al
// minimo il job riesce. Se il decoder ignorasse i byte del job, la prenotazione resterebbe in attesa nel pool, che non può
// liberarla.
TEST(RawImport, T_M3A_RAW_07_job_budget_boundary_is_shared_plus_file_plus_decoder) {
  const dng_case c = cases().front();
  const written_case w = write_case(c, "budget");
  const std::vector<std::uint8_t> bytes = fx::make_dng(c.spec);
  const std::uint64_t held = digest_reserve_bytes(bytes.size()) + bytes.size();
  const std::uint64_t needed = decoder_reservation(bytes);
  ASSERT_GT(needed, 0u);
  const std::uint64_t minimal = held + needed;

  const direct_run below = run_direct(w.file, w.digest, minimal - 1, "budget_below");
  ASSERT_FALSE(below.timed_out) << "il job è rimasto in attesa di una prenotazione che non può entrare nel pool";
  ASSERT_TRUE(below.outcome.has_value());
  ASSERT_TRUE(below.outcome->stop.has_value()) << "job riuscito sotto il minimo";
  const stop_error& s = *below.outcome->stop;
  EXPECT_EQ(s.category(), error_category::budget);
  EXPECT_EQ(s.code(), "sched.job_exceeds_budget");
  std::map<std::string, std::string> ctx(s.context().begin(), s.context().end());
  EXPECT_EQ(ctx["budget"], std::to_string(minimal - 1));
  EXPECT_EQ(ctx["minimo"], std::to_string(minimal)) << "il decoder dichiara il minimo (job + decoder), non il solo file";
  for (const artifact_record& a : below.store->artifacts_for(below.node_key)) EXPECT_NE(a.output_role, "primary");

  const direct_run at = run_direct(w.file, w.digest, minimal, "budget_at");
  ASSERT_FALSE(at.timed_out);
  ASSERT_TRUE(at.outcome.has_value());
  EXPECT_FALSE(at.outcome->stop.has_value()) << (at.outcome->stop ? at.outcome->stop->what() : "");
  EXPECT_EQ(at.outcome->exit_code, 0);
}

// Stesso confine nel decoder da solo: con `held_bytes` il decoder si ferma prima di `unpack` un byte sotto il minimo;
// al minimo decodifica (il caso senza `held_bytes` è T_M3A_RAW_06, test_raw_robust.cpp).
TEST(RawImport, T_M3A_RAW_07_decoder_counts_bytes_held_by_the_caller_before_unpack) {
  const dng_case c = cases().front();
  const std::vector<std::uint8_t> bytes = fx::make_dng(c.spec);
  const std::uint64_t needed = decoder_reservation(bytes);
  const std::uint64_t held = digest_reserve_bytes(bytes.size()) + bytes.size();
  ASSERT_GT(held, bytes.size());
  bool unpacked = false;
  raw_decode_hooks hooks;
  hooks.before_unpack = [&unpacked] { unpacked = true; };
  {
    memory_pool pool(held + needed - 1);
    const raw_decode_context ctx{&pool, nullptr, raw_limits::max_pixels, held, std::nullopt};
    try {
      (void)decode_raw_buffer(bytes, hooks, ctx);
      ADD_FAILURE() << "decodifica riuscita con un budget sotto job + decoder";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.category(), error_category::budget);
      EXPECT_EQ(e.code(), "sched.job_exceeds_budget");
    }
    EXPECT_FALSE(unpacked) << "unpack raggiunto prima dello stop";
  }
  {
    memory_pool pool(held + needed);
    const raw_decode_context ctx{&pool, nullptr, raw_limits::max_pixels, held, std::nullopt};
    EXPECT_NO_THROW((void)decode_raw_buffer(bytes, hooks, ctx));
    EXPECT_TRUE(unpacked);
  }
}

// Identificazione fallita alla costruzione della richiesta (file illeggibile, file diverso dal digest): il job si ferma con
// la stessa categoria, lo stesso codice e lo stesso contesto, senza aprire il file.
TEST(RawImport, T_M3A_RAW_07_identification_stop_is_reraised_without_opening_the_file) {
  const written_case w = write_case(cases().front(), "ident");
  const fs::path missing = w.file.parent_path() / "assente.dng";
  const fs::path altered = w.file.parent_path() / "alterato.dng";
  {
    std::ifstream in(w.file, std::ios::binary);
    std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    data[data.size() / 2] = static_cast<char>(data[data.size() / 2] ^ 0x5a);
    std::ofstream(altered, std::ios::binary).write(data.data(), static_cast<std::streamsize>(data.size()));
  }
  for (const fs::path& file : {missing, altered}) {
    SCOPED_TRACE(file.filename().string());
    const direct_run d = run_direct(file, w.digest, 256 * MiB, "ident_" + file.stem().string());
    ASSERT_TRUE(d.request_stop.has_value()) << "la richiesta deve registrare l'arresto di identificazione";
    ASSERT_FALSE(d.timed_out);
    ASSERT_TRUE(d.outcome.has_value());
    ASSERT_TRUE(d.outcome->stop.has_value());
    const stop_error& s = *d.outcome->stop;
    EXPECT_EQ(s.category(), d.request_stop->category());
    EXPECT_EQ(s.code(), d.request_stop->code());
    EXPECT_EQ(s.context(), d.request_stop->context());
    EXPECT_EQ(d.outcome->exit_code, exit_code_for(d.request_stop->category()));
    EXPECT_TRUE(d.opens.empty()) << "il job ha aperto il file: " << d.opens.size() << " aperture";
  }
}

// OB-09: il `resource_plan` registrato è il picco misurato nel pool dal job. Il codice lo compone da quantità misurate
// (condiviso + file + prenotazione del decoder, poi l'uscita da sola): deve coincidere con `peak_reserved_bytes`.
TEST(RawImport, T_M3A_RAW_07_recorded_peak_equals_pool_peak) {
  const written_case w = write_case(cases().front(), "peak");
  const direct_run d = run_direct(w.file, w.digest, 256 * MiB, "peak");
  ASSERT_FALSE(d.timed_out);
  ASSERT_TRUE(d.outcome.has_value() && d.outcome->exit_code == 0);
  ASSERT_TRUE(d.outcome->provenance.details.has_value());
  const json* plan = member(*d.outcome->provenance.details, "resource_plan");
  ASSERT_NE(plan, nullptr);
  EXPECT_EQ(num(plan, "peak_bytes"), static_cast<std::int64_t>(d.pool_peak));
}

}  // namespace
