// R5 (NFR-SIMD-01, ADR-07.a): percorsi cpu_scalar e cpu_avx2, dispatch nel preflight, ambiente FP.
// Tolleranze di docs/contracts/ADR-07-cpu-simd.md, fissate prima della misura: 0 ulp sui kernel elemento per
// elemento e sulle accumulazioni per pixel; bound (b) di Higham con i termini di underflow sulle riduzioni.

#include <gtest/gtest.h>
#include <xmmintrin.h>

#include <csignal>
#include <unistd.h>

#include <cfloat>
#include <cmath>
#include <cstring>
#include <limits>
#include <random>

#include "astro/calibration.hpp"
#include "astro/cpu_dispatch.hpp"
#include "astro/import_job.hpp"
#include "astro/job.hpp"
#include "astro/master.hpp"
#include "astro/resources.hpp"
#include "astro/scheduler.hpp"
#include "astro/workflow.hpp"
#include "calibration_internal.hpp"
#include "master_support.hpp"
#include "no_crash_report.hpp"

namespace {

using namespace astro;
using namespace mst;
using cal::frame_def;
using cal::set_def;
namespace fs = std::filesystem;

constexpr std::uint64_t MiB = std::uint64_t{1} << 20;
constexpr unsigned mxcsr_daz = 1u << 6;
constexpr unsigned mxcsr_ftz = 1u << 15;
constexpr unsigned mxcsr_rc_mask = 3u << 13;
constexpr unsigned mxcsr_masks = 0x3fu << 7;  // maschere delle eccezioni IM, DM, ZM, OM, UM, PM
constexpr unsigned mxcsr_controlled = mxcsr_ftz | mxcsr_daz | mxcsr_rc_mask | mxcsr_masks;

const cancel_token& no_cancel() {
  static const cancel_token token;
  return token;
}

// Il seam e' globale al processo: ogni test parte e finisce con capacita' reali, nessun iniettore, contatori a zero.
struct seam_guard {
  seam_guard() { clear(); }
  ~seam_guard() { clear(); }
  static void clear() {
    cpu_test::set_capabilities(std::nullopt);
    cpu_test::set_task_injector(nullptr);
    cpu_test::reset_kernel_calls();
  }
};

struct mxcsr_guard {
  unsigned saved = _mm_getcsr();
  ~mxcsr_guard() { _mm_setcsr(saved); }
};

// I due percorsi si confrontano sulla CPU reale: senza AVX2 il secondo percorso non e' eseguibile e il test fallisce.
bool hardware_avx2() {
  __builtin_cpu_init();
  return __builtin_cpu_supports("avx2");
}

std::uint64_t calls(cpu_path p) { return cpu_test::kernel_calls(p); }

bool same_buffer(const aligned_buffer& a, const aligned_buffer& b) {
  return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size()) == 0;
}

::testing::AssertionResult bit_identical(const image& a, const image& b) {
  if (a.type != b.type) return ::testing::AssertionFailure() << "tipo diverso";
  if (!same_buffer(a.samples, b.samples)) return ::testing::AssertionFailure() << "campioni diversi";
  if (a.invalid.bits != b.invalid.bits) return ::testing::AssertionFailure() << "maschere diverse";
  if (a.variance.has_value() != b.variance.has_value()) return ::testing::AssertionFailure() << "piano di varianza";
  if (a.variance && !same_buffer(a.variance->samples, b.variance->samples)) {
    return ::testing::AssertionFailure() << "piani di varianza diversi";
  }
  return ::testing::AssertionSuccess();
}

// Campo di un oggetto JSON: assente o di tipo diverso e' un errore del test, mai una dereferenziazione nulla.
const json& field(const json& o, const char* key, json::kind kind) {
  const json* v = o.type() == json::kind::object ? o.find(key) : nullptr;
  if (v == nullptr || v->type() != kind) throw std::runtime_error(std::string("campo assente o di tipo diverso: ") + key);
  return *v;
}
const std::string& text(const json& o, const char* key) { return field(o, key, json::kind::string).as_string(); }
bool flag(const json& o, const char* key) { return field(o, key, json::kind::boolean).as_bool(); }

std::string context_of(const stop_error& e, const char* key) {
  for (const auto& [k, v] : e.context()) {
    if (k == key) return v;
  }
  return "";
}

// Bit di modo di un valore di MXCSR diversi da quelli richiesti (maschere accese, FZ, DAZ e RC spenti); i flag di
// stato non contano.
unsigned fp_violation(unsigned csr) { return (csr ^ mxcsr_masks) & mxcsr_controlled; }

// fp_violation del valore di `mxcsr` nel contesto dello stop (esadecimale "0x...").
unsigned fp_mode_bits(const stop_error& e) {
  const std::string v = context_of(e, "mxcsr");
  if (v.rfind("0x", 0) != 0 || v.size() < 3) throw std::runtime_error("contesto mxcsr assente o non esadecimale: " + v);
  return fp_violation(static_cast<unsigned>(std::stoul(v.substr(2), nullptr, 16)));
}

// Stop atteso con categoria e codice; restituisce l'errore per i controlli sul contesto.
stop_error expect_stop_of(const std::function<void()>& f, error_category category, const char* code) {
  try {
    f();
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), category) << e.what();
    EXPECT_EQ(e.code(), code) << e.what();
    return e;
  }
  ADD_FAILURE() << "stop_error atteso: " << code;
  return stop_error(error_category::invalid_input, "test.no_stop", "nessuno stop");
}

// ---------------------------------------------------------------- generatori

double uniform(std::mt19937_64& g, double lo, double hi) {
  const double u = static_cast<double>(g() >> 11) * 0x1p-53;
  return lo + (hi - lo) * u;
}

// Insieme di calibrazione con un caso speciale per pixel (indice del caso: (7 i + seed) mod 16).
set_def cal_set(std::uint32_t w, std::uint32_t h, int light_bp, int master_bp, bool plane, bool cfa, bool sat,
                std::uint64_t seed) {
  set_def s = cal::empty_set(w, h, light_bp);
  for (frame_def* f : {&s.dark, &s.flat, &s.dark_flat}) f->bitpix = master_bp;
  if (cfa) {
    for (frame_def* f : {&s.light, &s.dark, &s.flat, &s.dark_flat}) f->hdr.bayer = "RGGB";
  }
  if (sat) {
    s.light.hdr.saturation = 60000;
    s.flat.hdr.saturation = 60000;
  }
  const std::size_t n = static_cast<std::size_t>(w) * h;
  if (plane) {
    for (frame_def* f : {&s.light, &s.dark, &s.flat, &s.dark_flat}) f->variance = std::vector<double>(n, 1.0);
  }
  const double light_tiny = light_bp == -32 ? 0x1p-149 : 0x1p-1074 * 3;
  const double light_huge = light_bp == -32 ? 3e38 : 1e300;
  const double var_tiny = light_bp == -32 ? 0x1p-149 : 0x1p-1074;
  // Varianze grandi ma finite nel tipo del piano: un piano non finito su un pixel valido e' un errore d'ingresso.
  const double var_huge_l = light_bp == -32 ? 3e38 : 1e300, var_huge_d = master_bp == -32 ? 3e38 : 1e300;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  std::mt19937_64 g(seed);
  for (std::size_t i = 0; i < n; ++i) {
    double df = 100 + uniform(g, 0, 5), f = df + 1000 + uniform(g, -50, 50);
    double d = 300 + uniform(g, 0, 3), l = d + uniform(g, -10, 500);
    double vl = uniform(g, 0.5, 2), vd = uniform(g, 0.5, 2);
    switch ((7 * i + seed) % 16) {
      case 0: l = d; break;                                // Lc = 0 esatto
      case 1: f = df; break;                               // Fc = 0: q_nonpositive
      case 2: f = df - 5; break;                           // Q < 0
      case 3: f = df + 1; break;                           // Q piccolo: q_below_min, q_unreliable
      case 4: l = nan; break;                              // source_nonfinite del light
      case 5: d = nan; break;                              // master_invalid
      case 6: l = light_huge; d = 0; break;                // Lc^2 oltre f64 o C oltre f32
      case 7: l = light_tiny; d = 0; break;                // Lc subnormale: C non normale
      case 8: l = light_tiny; d = light_tiny; break;       // differenza esatta zero: pixel valido
      case 9: if (sat) f = 61000; break;                   // flat saturo
      case 10: if (sat) l = 61000; break;                  // light saturo
      case 11: f = nan; break;                             // flat mascherato, escluso da N
      case 12: f = df + 0x1p-30; break;                    // Fc piccolo normale
      case 13: vl = var_huge_l; vd = var_huge_d; break;    // varianze grandi: Var(C) oltre f64 o f32 (piano)
      case 14: vl = var_tiny; vd = var_tiny; break;        // varianze subnormali (piano)
      default: break;
    }
    s.light.v[i] = l;
    s.dark.v[i] = d;
    s.flat.v[i] = f;
    s.dark_flat.v[i] = df;
    if (plane) {
      (*s.light.variance)[i] = vl;
      (*s.dark.variance)[i] = vd;
      (*s.flat.variance)[i] = uniform(g, 0.5, 2);
      (*s.dark_flat.variance)[i] = uniform(g, 0.5, 2);
    }
  }
  return s;
}

calibration_recipe cal_recipe(const set_def& s, bool plane, bool cfa) {
  calibration_recipe r = cal::recipe_of(s);
  if (plane) {
    for (calibration_input_spec* in : {&r.light, &r.dark, &r.flat, &r.dark_flat}) {
      in->variance = variance_spec{variance_source::plane, std::nullopt};
    }
  }
  if (cfa) r.normalization = normalization_mode::cfa_channel_median;
  r.q_min = 0.01;
  r.q_rel_sigma_max = 0.05;
  return r;
}

calibration_result calibrate_with(calibration_recipe r, simd_request simd, const set_def& s) {
  r.simd = simd;
  return calibrate(r, cal::images_of(s));
}

// Gruppo di frame per il master con un caso speciale per pixel (indice: (5 i + seed) mod 12).
group master_group(master_role role, std::uint32_t w, std::uint32_t h, std::size_t n, int bp, std::uint64_t seed) {
  group g = make_group(role, w, h, n, bp);
  const bool flat = role == master_role::flat;
  const double tiny = bp == -32 ? 0x1p-149 : 0x1p-1074;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  std::mt19937_64 rng(seed);
  for (std::size_t i = 0; i < static_cast<std::size_t>(w) * h; ++i) {
    const std::size_t kind = (5 * i + seed) % 12;
    for (std::size_t k = 0; k < n; ++k) {
      double v = (flat ? 20000 : 1000) + uniform(rng, -30, 30);
      switch (kind) {
        case 0: if (k == 0) v = nan; break;                                      // bit del frame
        case 1: if (k == 1) v = 70000; break;                                    // saturo se dichiarato
        case 2: if (!flat) v = (k % 2 ? -1.0 : 1.0) * 1e16 + static_cast<double>(k); break;  // cancellazione
        case 3: if (!flat) v = bp == -32 ? 3e38 : 1e308; break;                  // somma oltre il formato
        case 4: v = tiny * static_cast<double>(k + 1); break;                    // subnormali
        case 5: v = (k % 2 ? -1.0 : 1.0) * tiny; break;                          // somma con segno: esatta
        case 6: v = 1e-40; break;                                                 // media sotto FLT_MIN
        case 7: v = flat ? 20000 : 1000; break;                                   // varianza zero
        default: break;
      }
      g.frames[k].v[i] = v;
    }
  }
  return g;
}

master_result combine_with(master_recipe r, simd_request simd, const group& g) {
  r.simd = simd;
  return combine_master(r, images_of(g));
}

// ---------------------------------------------------------------- aritmetica esatta per le riduzioni

// x * 2^1074 come intero esatto con segno.
exact::big scaled(double x) {
  if (x == 0) return exact::big{};
  const bool neg = std::signbit(x);
  const double a = std::fabs(x);
  if (std::fpclassify(a) == FP_SUBNORMAL) return exact::from_u64(static_cast<std::uint64_t>(std::ldexp(a, 1074)), neg);
  int e = 0;
  const double fr = std::frexp(a, &e);
  const auto m = static_cast<std::uint64_t>(std::ldexp(fr, 53));
  return exact::shl(exact::from_u64(m, neg), static_cast<unsigned>(e - 53 + 1074));
}

exact::big magnitude(exact::big b) {
  b.neg = false;
  return b;
}

exact::big u64(std::uint64_t v) { return exact::from_u64(v); }

struct exact_sum {
  exact::big s, abs_sum;  // in unita' di 2^-1074
};

exact_sum exact_of(const std::vector<double>& x) {
  exact_sum r;
  for (const double v : x) {
    const exact::big b = scaled(v);
    r.s = exact::add(r.s, b);
    r.abs_sum = exact::add(r.abs_sum, magnitude(b));
  }
  return r;
}

std::uint64_t h_scalar(std::size_t n) { return n - 1; }
std::uint64_t h_avx2(std::size_t n) { return (n + 3) / 4 + 1; }

// |s_hat - s| <= gamma_k A + h (1 + gamma_k) eta/2, con gamma_k = k/D, D = 2^53 - k, eta/2 = 1/2 in unita' di
// 2^-1074. Moltiplicato per 2D: 2D |S_hat - S| <= 2k A + h 2^53. Esatto.
bool within_oracle(double got, const exact_sum& e, std::uint64_t k) {
  const exact::big d = u64((std::uint64_t{1} << 53) - k);
  const exact::big lhs = exact::mul(u64(2), exact::mul(d, magnitude(exact::sub(scaled(got), e.s))));
  const exact::big rhs = exact::add(exact::mul(u64(2 * k), e.abs_sum), exact::mul(u64(k), u64(std::uint64_t{1} << 53)));
  return exact::cmp(lhs, rhs) <= 0;
}

// Bound congiunto di ADR-07 (b), con h = k per ciascun percorso:
// 2 Da Db |Sa - Sb| <= 2 (ka Db + kb Da) A + 2^53 (ka Db + kb Da).
bool within_joint(double a, double b, const exact_sum& e, std::uint64_t ka, std::uint64_t kb) {
  const exact::big da = u64((std::uint64_t{1} << 53) - ka), db = u64((std::uint64_t{1} << 53) - kb);
  const exact::big lhs = exact::mul(u64(2), exact::mul(exact::mul(da, db), magnitude(exact::sub(scaled(a), scaled(b)))));
  const exact::big mix = exact::add(exact::mul(u64(ka), db), exact::mul(u64(kb), da));
  const exact::big rhs = exact::add(exact::mul(u64(2), exact::mul(mix, e.abs_sum)), exact::mul(u64(std::uint64_t{1} << 53), mix));
  return exact::cmp(lhs, rhs) <= 0;
}

// Schemi dichiarati in ADR-07 (b), scritti qui in modo indipendente.
double reference_scalar(const std::vector<double>& x) {
  double s = x[0];
  for (std::size_t i = 1; i < x.size(); ++i) s += x[i];
  return s;
}

double reference_lanes(const std::vector<double>& x) {
  if (x.size() < 4) return reference_scalar(x);
  double c[4] = {x[0], x[1], x[2], x[3]};
  for (std::size_t i = 4; i < x.size(); ++i) c[i % 4] += x[i];
  return (c[0] + c[1]) + (c[2] + c[3]);
}

std::vector<std::vector<double>> sum_cases() {
  std::vector<std::vector<double>> out;
  std::mt19937_64 g(20261029);
  for (const std::size_t n : {1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16, 17, 63, 64, 65, 1000, 1001, 1002, 1003}) {
    std::vector<double> mixed(n), cancel(n), under(n), tenth(n, 0.1);
    for (std::size_t i = 0; i < n; ++i) {
      mixed[i] = (g() % 2 ? -1.0 : 1.0) * std::ldexp(uniform(g, 1, 2), static_cast<int>(g() % 81) - 40);
      cancel[i] = (i % 2 ? -1.0 : 1.0) * 0x1p40 * (1 + uniform(g, 0, 1e-6)) + uniform(g, -1, 1);
      const double j = static_cast<double>(g() % 8);
      under[i] = (g() % 2 ? -1.0 : 1.0) * (i % 3 == 0 ? j * 0x1p-1074 : 0x1p-1022 + (i % 3 == 1 ? -j : j) * 0x1p-1074);
    }
    for (auto* v : {&mixed, &cancel, &under, &tenth}) out.push_back(*v);
  }
  return out;
}

// ---------------------------------------------------------------- dischi e store

struct cal_disk {
  fs::path dir;
  set_def s;
  calibration_recipe recipe;
};

cal_disk write_cal(const std::string& name, simd_request simd) {
  cal_disk c{cal::fresh_dir("simd_" + name), cal::synth_set(13, 6, -32), {}};
  c.recipe = cal::recipe_of(c.s);
  c.recipe.simd = simd;
  const std::pair<const char*, const frame_def*> frames[4] = {
      {"light.fits", &c.s.light}, {"dark.fits", &c.s.dark}, {"flat.fits", &c.s.flat}, {"dark_flat.fits", &c.s.dark_flat}};
  calibration_input_spec* specs[4] = {&c.recipe.light, &c.recipe.dark, &c.recipe.flat, &c.recipe.dark_flat};
  for (int i = 0; i < 4; ++i) {
    cal::write_bytes(c.dir / frames[i].first, cal::file_of(*frames[i].second, c.s.w, c.s.h));
    specs[i]->path = (c.dir / frames[i].first).string();
  }
  return c;
}

struct cal_store {
  std::unique_ptr<sqlite_dag_store> store;
  node_spec node;
};

cal_store cal_project(const cal_disk& c, const std::string& name) {
  cal_store p{sqlite_dag_store::create(c.dir / name), make_calibration_node(c.recipe)};
  p.store->put_node(p.node);
  p.store->create_slot("cal", p.node.key());
  return p;
}

job_outcome run_cal(cal_store& p, const calibration_recipe& r, const calibration_job_options& o = {}) {
  memory_pool pool(256 * MiB);
  testing::internal::CaptureStderr();
  job_outcome out = run_calibration_job(*p.store, "cal", r, pool, o, no_cancel());
  testing::internal::GetCapturedStderr();
  return out;
}

std::size_t artifacts_of(const cal_store& p) { return p.store->artifacts_for(p.node.key()).size(); }

const json& cpu_details(const job_outcome& out) {
  if (!out.provenance.details) throw std::runtime_error("provenance senza details");
  return field(*out.provenance.details, "cpu", json::kind::object);
}

// details.cpu completo (ADR-07, aggiunte di R5): richiesta, percorso risolto, capacita' lette.
void expect_cpu_details(const job_outcome& out, const char* simd, const char* backend, bool avx2) {
  const json& cpu = cpu_details(out);
  EXPECT_EQ(text(cpu, "simd"), simd);
  EXPECT_EQ(text(cpu, "backend"), backend);
  EXPECT_EQ(flag(field(cpu, "capabilities", json::kind::object), "avx2"), avx2);
}

std::string primary_of(const sqlite_dag_store& store, const std::string& key) {
  for (const artifact_record& a : store.artifacts_for(key)) {
    if (a.output_role == "primary") return a.sha256;
  }
  throw std::runtime_error("artefatto primary assente");
}

struct master_disk {
  fs::path dir;
  group g;
  master_recipe recipe;
  std::unique_ptr<sqlite_dag_store> store;
  node_spec node;
};

master_disk master_project(const std::string& name, simd_request simd) {
  const fs::path dir = cal::fresh_dir("simd_" + name);
  group g = make_group(master_role::dark, 9, 5, 3);
  master_recipe r = recipe_of(g);
  r.simd = simd;
  write_frames(g, r, dir, "frame");
  auto store = sqlite_dag_store::create(dir / "store");
  const node_spec node = make_master_node(r);
  store->put_node(node);
  store->create_slot("master", node.key());
  return master_disk{dir, std::move(g), std::move(r), std::move(store), node};
}

job_outcome run_master(master_disk& m, const master_job_options& o = {}) {
  memory_pool pool(256 * MiB);
  testing::internal::CaptureStderr();
  job_outcome out = run_master_job(*m.store, "master", m.recipe, pool, o, no_cancel());
  testing::internal::GetCapturedStderr();
  return out;
}

}  // namespace

// ---------------------------------------------------------------- T-M2R5-03: tabella del dispatch e stop

TEST(Simd, T_M2R5_03_dispatch_table_of_the_contract) {
  const cpu_capabilities with{true}, without{false};
  EXPECT_EQ(resolve_cpu_path(simd_request::scalar, with), cpu_path::scalar);
  EXPECT_EQ(resolve_cpu_path(simd_request::scalar, without), cpu_path::scalar);
  EXPECT_EQ(resolve_cpu_path(simd_request::avx2, with), cpu_path::avx2);
  EXPECT_STOP(resolve_cpu_path(simd_request::avx2, without), error_category::unsupported, "cpu.avx2_unavailable");
  EXPECT_EQ(resolve_cpu_path(simd_request::auto_select, with), cpu_path::avx2);
  EXPECT_EQ(resolve_cpu_path(simd_request::auto_select, without), cpu_path::scalar);
  EXPECT_STREQ(backend_name(cpu_path::scalar), "cpu_scalar");
  EXPECT_STREQ(backend_name(cpu_path::avx2), "cpu_avx2");
  EXPECT_STREQ(simd_request_name(simd_request::auto_select), "auto");
  EXPECT_STREQ(simd_request_name(simd_request::scalar), "scalar");
  EXPECT_STREQ(simd_request_name(simd_request::avx2), "avx2");
}

TEST(Simd, T_M2R5_03_capabilities_come_from_the_cpu_unless_injected) {
  seam_guard seam;
  EXPECT_EQ(read_cpu_capabilities().avx2, hardware_avx2());
  cpu_test::set_capabilities(cpu_capabilities{false});
  EXPECT_FALSE(read_cpu_capabilities().avx2);
  cpu_test::set_capabilities(cpu_capabilities{true});
  EXPECT_TRUE(read_cpu_capabilities().avx2);
}

TEST(Simd, T_M2R5_03_recipe_without_simd_is_refused) {
  calibration_recipe r = cal::recipe_of(cal::synth_set(8, 4, -64));
  r.simd.reset();
  EXPECT_STOP(make_calibration_node(r), error_category::invalid_input, "calib.param.simd");
  EXPECT_STOP(calibrate(r, cal::images_of(cal::synth_set(8, 4, -64))), error_category::invalid_input, "calib.param.simd");
  const group g = make_group(master_role::dark, 5, 3, 3);
  master_recipe m = recipe_of(g);
  m.simd.reset();
  EXPECT_STOP(make_master_node(m), error_category::invalid_input, "master.param.simd");
  EXPECT_STOP(combine_master(m, images_of(g)), error_category::invalid_input, "master.param.simd");
}

TEST(Simd, T_M2R5_03_avx2_requested_without_avx2_stops_and_runs_no_path) {
  seam_guard seam;
  cpu_test::set_capabilities(cpu_capabilities{false});
  const set_def s = cal::synth_set(8, 4, -64);
  calibration_recipe r = cal::recipe_of(s);
  r.simd = simd_request::avx2;
  EXPECT_STOP(make_calibration_node(r), error_category::unsupported, "cpu.avx2_unavailable");
  EXPECT_STOP(calibrate(r, cal::images_of(s)), error_category::unsupported, "cpu.avx2_unavailable");
  const group g = make_group(master_role::dark, 5, 3, 3);
  master_recipe m = recipe_of(g);
  m.simd = simd_request::avx2;
  EXPECT_STOP(make_master_node(m), error_category::unsupported, "cpu.avx2_unavailable");
  EXPECT_STOP(combine_master(m, images_of(g)), error_category::unsupported, "cpu.avx2_unavailable");
  EXPECT_EQ(calls(cpu_path::scalar), 0u);
  EXPECT_EQ(calls(cpu_path::avx2), 0u);
}

TEST(Simd, T_M2R5_03_job_whose_slot_needs_avx2_stops_on_a_cpu_without_it) {
  seam_guard seam;
  cpu_test::set_capabilities(cpu_capabilities{true});
  const cal_disk c = write_cal("no_avx2_job", simd_request::avx2);
  cal_store p = cal_project(c, "store");
  ASSERT_EQ(p.node.backend(), "cpu_avx2");
  cpu_test::set_capabilities(cpu_capabilities{false});
  const job_outcome out = run_cal(p, c.recipe);
  EXPECT_EQ(out.exit_code, exit_code_stop);
  ASSERT_TRUE(out.stop.has_value());
  EXPECT_EQ(out.stop->category(), error_category::unsupported);
  EXPECT_EQ(out.stop->code(), "cpu.avx2_unavailable");
  EXPECT_EQ(artifacts_of(p), 1u);  // solo la provenance dell'arresto
  EXPECT_EQ(calls(cpu_path::scalar), 0u);
  EXPECT_EQ(calls(cpu_path::avx2), 0u);
}

TEST(Simd, T_M2R5_03_auto_without_avx2_is_the_declared_scalar_path) {
  seam_guard seam;
  cpu_test::set_capabilities(cpu_capabilities{false});
  const cal_disk c = write_cal("auto_scalar", simd_request::auto_select);
  cal_store p = cal_project(c, "store");
  EXPECT_EQ(p.node.backend(), "cpu_scalar");
  const job_outcome out = run_cal(p, c.recipe);
  ASSERT_EQ(out.exit_code, 0);
  EXPECT_GT(calls(cpu_path::scalar), 0u);
  EXPECT_EQ(calls(cpu_path::avx2), 0u);
  expect_cpu_details(out, "auto", "cpu_scalar", false);
}

// Con `auto` il percorso risolto entra nella chiave: se le capacita' cambiano dopo la creazione dello slot, la
// chiave ricostruita dal job differisce e il job si ferma senza eseguire alcun percorso (D19).
TEST(Simd, T_M2R5_03_auto_resolved_differently_after_the_slot_is_node_mismatch) {
  seam_guard seam;
  cpu_test::set_capabilities(cpu_capabilities{true});
  const cal_disk c = write_cal("auto_changed", simd_request::auto_select);
  cal_store p = cal_project(c, "store");
  ASSERT_EQ(p.node.backend(), "cpu_avx2");
  master_disk m = master_project("auto_changed_master", simd_request::auto_select);
  ASSERT_EQ(m.node.backend(), "cpu_avx2");
  cpu_test::set_capabilities(cpu_capabilities{false});
  const job_outcome out = run_cal(p, c.recipe);
  ASSERT_TRUE(out.stop.has_value());
  EXPECT_EQ(out.stop->category(), error_category::invalid_input);
  EXPECT_EQ(out.stop->code(), "calib.node_mismatch");
  EXPECT_EQ(artifacts_of(p), 1u);
  const job_outcome mo = run_master(m);
  ASSERT_TRUE(mo.stop.has_value());
  EXPECT_EQ(mo.stop->category(), error_category::invalid_input);
  EXPECT_EQ(mo.stop->code(), "master.node_mismatch");
  EXPECT_EQ(m.store->artifacts_for(m.node.key()).size(), 1u);
  EXPECT_EQ(calls(cpu_path::scalar), 0u);
  EXPECT_EQ(calls(cpu_path::avx2), 0u);
}

// ---------------------------------------------------------------- T-M2R5-04: backend, chiave, provenance, resume

TEST(Simd, T_M2R5_04_path_is_in_backend_key_and_provenance) {
  ASSERT_TRUE(hardware_avx2());
  seam_guard seam;
  const cal_disk cs = write_cal("prov_scalar", simd_request::scalar);
  const cal_disk ca = write_cal("prov_avx2", simd_request::avx2);
  cal_store ps = cal_project(cs, "store");
  cal_store pa = cal_project(ca, "store");
  EXPECT_EQ(ps.node.backend(), "cpu_scalar");
  EXPECT_EQ(pa.node.backend(), "cpu_avx2");
  EXPECT_NE(ps.node.key(), pa.node.key());
  EXPECT_EQ(text(ps.node.params(), "simd"), "scalar");
  EXPECT_EQ(text(pa.node.params(), "simd"), "avx2");
  const job_outcome os = run_cal(ps, cs.recipe);
  const job_outcome oa = run_cal(pa, ca.recipe);
  ASSERT_EQ(os.exit_code, 0);
  ASSERT_EQ(oa.exit_code, 0);
  EXPECT_EQ(os.provenance.spec.backend(), "cpu_scalar");
  EXPECT_EQ(oa.provenance.spec.backend(), "cpu_avx2");
  expect_cpu_details(os, "scalar", "cpu_scalar", true);
  expect_cpu_details(oa, "avx2", "cpu_avx2", true);
  // Stesso calcolo elemento per elemento: l'immagine calibrata e' identica byte per byte (0 ulp, ADR-07 (a)).
  EXPECT_EQ(primary_of(*ps.store, ps.node.key()), primary_of(*pa.store, pa.node.key()));

  // Master: stesso percorso nel nodo, nella chiave e nella provenance, sui due percorsi.
  master_disk ms = master_project("master_prov_scalar", simd_request::scalar);
  master_disk ma = master_project("master_prov_avx2", simd_request::avx2);
  EXPECT_EQ(ms.node.backend(), "cpu_scalar");
  EXPECT_EQ(ma.node.backend(), "cpu_avx2");
  EXPECT_NE(ms.node.key(), ma.node.key());
  EXPECT_EQ(text(ms.node.params(), "simd"), "scalar");
  EXPECT_EQ(text(ma.node.params(), "simd"), "avx2");
  const job_outcome mos = run_master(ms), moa = run_master(ma);
  ASSERT_EQ(mos.exit_code, 0);
  ASSERT_EQ(moa.exit_code, 0);
  EXPECT_EQ(mos.provenance.spec.backend(), "cpu_scalar");
  EXPECT_EQ(moa.provenance.spec.backend(), "cpu_avx2");
  expect_cpu_details(mos, "scalar", "cpu_scalar", true);
  expect_cpu_details(moa, "avx2", "cpu_avx2", true);
  EXPECT_EQ(primary_of(*ms.store, ms.node.key()), primary_of(*ma.store, ma.node.key()));
}

TEST(Simd, T_M2R5_04_resume_with_a_different_path_is_resume_mismatch_and_runs_nothing) {
  ASSERT_TRUE(hardware_avx2());
  seam_guard seam;
  const cal_disk c = write_cal("resume", simd_request::scalar);
  cal_store p = cal_project(c, "store");
  memory_pool pool(256 * MiB);
  const auto step = [&](const calibration_recipe& r) {
    return workflow_step{"cal", make_calibration_node(r), [&, r]() {
                           return run_calibration_job(*p.store, "cal", r, pool, calibration_job_options{}, no_cancel());
                         }};
  };
  workflow wf{"simd", {step(c.recipe)}, nullptr};
  testing::internal::CaptureStderr();
  const workflow_result first = run_workflow(*p.store, wf);
  testing::internal::GetCapturedStderr();
  ASSERT_EQ(first.exit_code, 0);
  const std::size_t before = artifacts_of(p);
  calibration_recipe other = c.recipe;
  other.simd = simd_request::avx2;
  workflow changed{"simd", {step(other)}, nullptr};
  changed.steps[0].run = []() -> job_outcome {
    ADD_FAILURE() << "un passo e' stato eseguito da un resume rifiutato";
    throw std::runtime_error("passo eseguito");
  };
  cpu_test::reset_kernel_calls();
  EXPECT_STOP(resume_workflow(*p.store, changed), error_category::resume_mismatch, "resume.recipe");
  EXPECT_EQ(artifacts_of(p), before);
  EXPECT_EQ(calls(cpu_path::scalar), 0u);
  EXPECT_EQ(calls(cpu_path::avx2), 0u);
}

// ---------------------------------------------------------------- T-M2R5-02: scarto fra i percorsi

TEST(Simd, T_M2R5_02_calibration_kernels_are_bit_identical_between_paths) {
  ASSERT_TRUE(hardware_avx2());
  seam_guard seam;
  std::size_t computed = 0;
  std::uint64_t seed = 1;
  for (const std::uint32_t w : {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 17u}) {
    std::uint32_t h = std::max<std::uint32_t>(4, (40 + w - 1) / w);
    h += h % 2;
    for (const auto& bp : {std::pair{-64, -64}, std::pair{-32, -32}, std::pair{-32, -64}}) {
      for (const bool plane : {false, true}) {
        for (const bool cfa : {false, true}) {
          if (cfa && w < 2) continue;  // la normalizzazione per canale richiede i quattro canali CFA
          for (const bool sat : {false, true}) {
            SCOPED_TRACE(testing::Message() << "w=" << w << " h=" << h << " bitpix=" << bp.first << "/" << bp.second
                                            << " plane=" << plane << " cfa=" << cfa << " sat=" << sat);
            const set_def s = cal_set(w, h, bp.first, bp.second, plane, cfa, sat, seed++);
            const calibration_recipe r = cal_recipe(s, plane, cfa);
            cpu_test::reset_kernel_calls();
            const calibration_result a = calibrate_with(r, simd_request::scalar, s);
            EXPECT_GT(calls(cpu_path::scalar), 0u);
            EXPECT_EQ(calls(cpu_path::avx2), 0u);
            cpu_test::reset_kernel_calls();
            const calibration_result b = calibrate_with(r, simd_request::avx2, s);
            EXPECT_EQ(calls(cpu_path::scalar), 0u);
            EXPECT_GT(calls(cpu_path::avx2), 0u);
            EXPECT_TRUE(bit_identical(a.output, b.output));
            // Mediane e varianza di N: stesso codice nei due percorsi, bit-identiche.
            const auto& na = field(a.report, "normalization", json::kind::array).items();
            const auto& nb = field(b.report, "normalization", json::kind::array).items();
            ASSERT_EQ(na.size(), nb.size());
            ASSERT_EQ(na.size(), cfa ? 4u : 1u);
            for (std::size_t c = 0; c < na.size(); ++c) {
              for (const char* key : {"N", "normalization_rel_var_estimate"}) {
                const double x = field(na[c], key, json::kind::number).as_number();
                const double y = field(nb[c], key, json::kind::number).as_number();
                EXPECT_EQ(std::memcmp(&x, &y, sizeof x), 0) << key;
              }
              EXPECT_EQ(field(na[c], "n_valid", json::kind::integer).as_integer(),
                        field(nb[c], "n_valid", json::kind::integer).as_integer());
            }
            ++computed;
          }
        }
      }
    }
  }
  EXPECT_EQ(computed, 9u * 3u * 2u * 2u * 2u + 3u * 2u * 2u);  // w = 1 solo mono
}

TEST(Simd, T_M2R5_02_master_accumulators_are_bit_identical_between_paths) {
  ASSERT_TRUE(hardware_avx2());
  seam_guard seam;
  std::size_t computed = 0;
  std::uint64_t seed = 100;
  for (const master_role role : {master_role::dark, master_role::flat}) {
    for (const std::size_t n : {2u, 3u, 5u, 8u}) {
      for (const std::uint32_t w : {1u, 3u, 4u, 5u, 9u}) {
        for (const int bp : {-64, -32}) {
          for (const sample_type out : {sample_type::f32, sample_type::f64}) {
            for (const bool sat : {false, true}) {
              SCOPED_TRACE(testing::Message() << "ruolo=" << static_cast<int>(role) << " n=" << n << " w=" << w
                                              << " bitpix=" << bp << " out=" << static_cast<int>(out) << " sat=" << sat);
              const group g = master_group(role, w, 13, n, bp, seed++);
              master_recipe r = recipe_of(g, 0.5);
              r.output_type = out;
              if (sat) {
                r.saturation_adu = 65000;
                r.saturated_fraction_max = 1.0;
              }
              cpu_test::reset_kernel_calls();
              const master_result a = combine_with(r, simd_request::scalar, g);
              EXPECT_GT(calls(cpu_path::scalar), 0u);
              EXPECT_EQ(calls(cpu_path::avx2), 0u);
              cpu_test::reset_kernel_calls();
              const master_result b = combine_with(r, simd_request::avx2, g);
              EXPECT_EQ(calls(cpu_path::scalar), 0u);
              EXPECT_GT(calls(cpu_path::avx2), 0u);
              EXPECT_TRUE(bit_identical(a.output, b.output));
              ASSERT_EQ(a.level_bias_estimate.size(), b.level_bias_estimate.size());
              ASSERT_EQ(a.level_bias_estimate.empty(), role != master_role::flat);
              if (!a.level_bias_estimate.empty()) {
                EXPECT_EQ(std::memcmp(a.level_bias_estimate.data(), b.level_bias_estimate.data(),
                                      a.level_bias_estimate.size() * sizeof(double)), 0);
              }
              ++computed;
            }
          }
        }
      }
    }
  }
  EXPECT_EQ(computed, 2u * 4u * 5u * 2u * 2u * 2u);
}

TEST(Simd, T_M2R5_02_pixel_sum_follows_the_declared_schemes_within_the_joint_bound) {
  ASSERT_TRUE(hardware_avx2());
  seam_guard seam;
  for (const std::vector<double>& x : sum_cases()) {
    const std::size_t n = x.size();
    SCOPED_TRACE(testing::Message() << "n=" << n << " x0=" << x[0]);
    const double sc = pixel_sum(cpu_path::scalar, x.data(), n);
    const double av = pixel_sum(cpu_path::avx2, x.data(), n);
    // Gli schemi sono quelli su cui e' derivato il bound: ricorsivo per indice, 4 corsie con riduzione a coppie.
    EXPECT_EQ(sc, reference_scalar(x));
    EXPECT_EQ(av, reference_lanes(x));
    const exact_sum e = exact_of(x);
    EXPECT_TRUE(within_oracle(sc, e, h_scalar(n)));
    EXPECT_TRUE(within_oracle(av, e, h_avx2(n)));
    EXPECT_TRUE(within_joint(sc, av, e, h_scalar(n), h_avx2(n)));
  }
  // Discriminante: 1 + 4 * 2^-53 vale 1 nell'ordine degli indici e 1 + 2^-52 sulle 4 corsie.
  const std::vector<double> x = {1.0, 0x1p-53, 0x1p-53, 0x1p-53, 0x1p-53};
  EXPECT_EQ(pixel_sum(cpu_path::scalar, x.data(), x.size()), 1.0);
  EXPECT_EQ(pixel_sum(cpu_path::avx2, x.data(), x.size()), 1.0 + 0x1p-52);
  EXPECT_STOP(pixel_sum(cpu_path::scalar, x.data(), 0), error_category::invalid_input, "cpu.empty_sum");
  EXPECT_STOP(pixel_sum(cpu_path::avx2, x.data(), 0), error_category::invalid_input, "cpu.empty_sum");
}

// ---------------------------------------------------------------- T-M2R5-06: subnormali

TEST(Simd, T_M2R5_06_subnormal_inputs_results_and_conversions) {
  ASSERT_TRUE(hardware_avx2());
  seam_guard seam;
  constexpr double tiny32 = 0x1p-149, top32 = 0x1p-126 - 0x1p-149;
  // Calibrazione f32: ingressi subnormali validi; Lc = 0 esatto valido; Lc subnormale da' C non normale.
  set_def s = cal::synth_set(8, 4, -32);
  s.light.v[0] = tiny32;
  s.dark.v[0] = tiny32;  // Lc = 0
  s.light.v[1] = top32;
  s.dark.v[1] = top32 - 2 * tiny32;  // Lc = 2^-148: C = Lc / Q sotto FLT_MIN
  s.light.v[2] = tiny32;
  s.dark.v[2] = 0;
  const calibration_images in = cal::images_of(s);
  for (const std::size_t i : {0u, 1u, 2u}) EXPECT_EQ(in.light.invalid.bits[i], 0) << i;
  EXPECT_EQ(static_cast<double>(in.light.data<float>()[1]), top32);
  const calibration_recipe r = cal::recipe_of(s);
  const calibration_result a = calibrate_with(r, simd_request::scalar, s);
  const calibration_result b = calibrate_with(r, simd_request::avx2, s);
  EXPECT_TRUE(bit_identical(a.output, b.output));
  for (const calibration_result* res : {&a, &b}) {
    EXPECT_EQ(res->output.invalid.bits[0], 0);
    EXPECT_EQ(cal::sample_at(res->output, 0), 0.0);
    EXPECT_EQ(res->output.invalid.bits[1], invalid_reason::result_nonfinite);
    EXPECT_EQ(res->output.invalid.bits[2], invalid_reason::result_nonfinite);
  }

  // Master: somma con segno che da' zero (esatta, valida); media subnormale f64; media 2^-150 e 2^-150 + 2^-201
  // convertite in f32 a 0 e 2^-149 (non nulle prima della conversione): result_nonfinite in entrambi i percorsi.
  group g = make_group(master_role::bias, 5, 1, 2);
  g.frames[0].v = {-1.0, 0x1p-1074, 0x1p-150, 0x1p-150 + 0x1p-201, 1000};
  g.frames[1].v = {1.0, 0x1p-1074, 0x1p-150, 0x1p-150 + 0x1p-201, 1000};
  master_recipe m = recipe_of(g);
  for (const sample_type out : {sample_type::f64, sample_type::f32}) {
    m.output_type = out;
    const master_result ma = combine_with(m, simd_request::scalar, g);
    const master_result mb = combine_with(m, simd_request::avx2, g);
    EXPECT_TRUE(bit_identical(ma.output, mb.output));
    for (const master_result* res : {&ma, &mb}) {
      const auto& bits = res->output.invalid.bits;
      EXPECT_EQ(bits[0], 0);
      EXPECT_EQ(cal::sample_at(res->output, 0), 0.0);
      EXPECT_EQ(bits[1], invalid_reason::result_nonfinite);  // media 2^-1074 da somma 2^-1073: quoziente subnormale
      EXPECT_EQ(bits[2], out == sample_type::f32 ? invalid_reason::result_nonfinite : 0);
      EXPECT_EQ(bits[3], out == sample_type::f32 ? invalid_reason::result_nonfinite : 0);
      EXPECT_EQ(bits[4], 0);
    }
  }

  // Bound di conversione dopo una riduzione (D20): |fl(a) - fl(b)| <= |a - b| + u_32 (|a| + |b|) + eta_32.
  const double ca = 0x1p-150 - 0x1p-201, cb = 0x1p-150 + 0x1p-201;
  EXPECT_EQ(static_cast<float>(ca), 0.0f);
  EXPECT_EQ(static_cast<float>(cb), static_cast<float>(tiny32));
  const exact::rat lhs = exact::abs(exact::from_double(static_cast<double>(static_cast<float>(ca))) -
                                    exact::from_double(static_cast<double>(static_cast<float>(cb))));
  const exact::rat rhs = exact::abs(exact::from_double(ca) - exact::from_double(cb)) +
                         exact::unit_roundoff_f32() * (exact::from_double(ca) + exact::from_double(cb)) +
                         exact::from_double(tiny32);
  EXPECT_TRUE(lhs <= rhs);
  // Divisione per N e conversione dopo le somme dei due percorsi sui casi vicini all'underflow f64.
  for (const std::vector<double>& x : sum_cases()) {
    const std::size_t n = x.size();
    const double sa = pixel_sum(cpu_path::scalar, x.data(), n), sb = pixel_sum(cpu_path::avx2, x.data(), n);
    const double qa = sa / static_cast<double>(n), qb = sb / static_cast<double>(n);
    const exact::rat dn = exact::from_u(n);
    const exact::rat ea = exact::from_double(sa) / dn, eb = exact::from_double(sb) / dn;
    const exact::rat u64r{exact::from_u64(1), exact::shl(exact::from_u64(1), 53)};
    const exact::rat bound = exact::abs(ea - eb) + u64r * (exact::abs(ea) + exact::abs(eb)) + exact::from_double(0x1p-1074);
    EXPECT_TRUE(exact::abs(exact::from_double(qa) - exact::from_double(qb)) <= bound) << "n=" << n;
  }
}

// ---------------------------------------------------------------- T-M2R5-07: ambiente FP

constexpr unsigned bad_fp_modes[] = {mxcsr_ftz, mxcsr_daz, 1u << 13, 2u << 13, 3u << 13};
// Anche ciascuna maschera di eccezione spenta (bit da invertire rispetto al valore richiesto). Solo nei controlli dello
// stesso thread, ripristinato subito dopo lo stop: in un worker un'eccezione smascherata colpirebbe il resto del job.
constexpr unsigned bad_fp_modes_with_masks[] = {mxcsr_ftz, mxcsr_daz, 1u << 13, 2u << 13, 3u << 13, 1u << 7, 1u << 8,
                                                1u << 9, 1u << 10, 1u << 11, 1u << 12};

TEST(Simd, T_M2R5_07_ftz_daz_or_rounding_before_the_preflight_stop_without_kernels) {
  seam_guard seam;
  const set_def s = cal::synth_set(8, 4, -64);
  const calibration_recipe r = cal::recipe_of(s);
  const calibration_images images = cal::images_of(s);
  const group g = make_group(master_role::dark, 5, 3, 3);
  const master_recipe m = recipe_of(g);
  const std::vector<image> frames = images_of(g);
  // Ingressi che il preflight fermerebbe per un altro motivo (esposizioni incompatibili): il controllo di MXCSR viene
  // prima di ogni altro passo del preflight, quindi lo stop resta cpu.fp_mode.
  set_def s_bad = s;
  s_bad.dark.hdr.exposure = 61;
  const calibration_images images_bad = cal::images_of(s_bad);
  group g_bad = g;
  g_bad.frames[1].hdr.exposure = 61;
  const std::vector<image> frames_bad = images_of(g_bad);
  const master_recipe m_bad = recipe_of(g_bad);
  (void)expect_stop_of([&] { (void)calibrate(r, images_bad); }, error_category::incompatible, "calib.mismatch.exposure_s");
  (void)expect_stop_of([&] { (void)combine_master(m_bad, frames_bad); }, error_category::incompatible,
                       "master.mismatch.exposure_s");
  const unsigned base = _mm_getcsr();
  ASSERT_EQ(fp_violation(base), 0u);
  for (const unsigned bad : bad_fp_modes_with_masks) {
    SCOPED_TRACE(testing::Message() << std::hex << "bit=" << bad);
    cpu_test::reset_kernel_calls();
    unsigned after = 0;
    std::optional<stop_error> cal_stop, master_stop;
    {
      mxcsr_guard restore;
      _mm_setcsr(base ^ bad);
      cal_stop = expect_stop_of([&] { (void)calibrate(r, images); }, error_category::numeric, "cpu.fp_mode");
      master_stop = expect_stop_of([&] { (void)combine_master(m, frames); }, error_category::numeric, "cpu.fp_mode");
      (void)expect_stop_of([&] { (void)calibrate(r, images_bad); }, error_category::numeric, "cpu.fp_mode");
      (void)expect_stop_of([&] { (void)combine_master(m_bad, frames_bad); }, error_category::numeric, "cpu.fp_mode");
      after = _mm_getcsr();
    }
    EXPECT_EQ(fp_mode_bits(*cal_stop), bad);
    EXPECT_EQ(fp_mode_bits(*master_stop), bad);
    EXPECT_EQ(fp_violation(after), bad);  // nessun ripristino automatico del registro
    EXPECT_EQ(calls(cpu_path::scalar), 0u);
    EXPECT_EQ(calls(cpu_path::avx2), 0u);
    // Al primo modo non fermato si esce: con una maschera spenta e senza controllo il caso dopo terminerebbe il
    // processo con un segnale, nascondendo il fallimento gia' registrato.
    if (HasFailure()) return;
  }
  EXPECT_NO_THROW(require_fp_environment());
}

// Con DM spenta un confronto su un parametro valido ma subnormale genererebbe un segnale: il controllo di MXCSR viene
// prima della validazione della ricetta nei punti d'ingresso. I confini dei job e l'avvio dei task hanno i loro test
// (T_M2R5_07_denormal_mask_off_* piu' sotto).
TEST(Simd, T_M2R5_07_denormal_mask_off_with_subnormal_parameters_stops_before_validation) {
  seam_guard seam;
  const set_def s = cal::synth_set(8, 4, -64);
  calibration_recipe r = cal::recipe_of(s);
  r.q_min = 0x1p-1074;
  const calibration_images images = cal::images_of(s);
  const group g = make_group(master_role::dark, 5, 3, 3);
  master_recipe m = recipe_of(g);
  m.exposure_rel_tol = 0x1p-1074;
  const std::vector<image> frames = images_of(g);
  // Con MXCSR regolare i parametri subnormali sono validi.
  EXPECT_NO_THROW((void)make_calibration_node(r));
  EXPECT_NO_THROW((void)calibrate(r, images));
  EXPECT_NO_THROW((void)make_master_node(m));
  EXPECT_NO_THROW((void)combine_master(m, frames));
  const unsigned base = _mm_getcsr();
  ASSERT_EQ(fp_violation(base), 0u);
  constexpr unsigned dm = 1u << 8;
  ASSERT_EQ((base ^ dm) & mxcsr_controlled, 0x1e80u);  // i flag di stato (bit 0-5) non contano
  // In un processo figlio: se il controllo venisse dopo la validazione, il SIGFPE terminerebbe il figlio (uscita 136
  // dal gestore, niente core) e il test fallirebbe senza abbattere il processo dei test.
  cpu_test::reset_kernel_calls();
  const auto child = [&] {
    struct sigaction action {};
    action.sa_handler = [](int) { _exit(136); };
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGFPE, &action, nullptr) != 0) _exit(2);
    _mm_setcsr(base ^ dm);
    const std::function<void()> calls_under_test[] = {
        [&] { (void)make_calibration_node(r); }, [&] { (void)calibrate(r, images); },
        [&] { (void)make_master_node(m); }, [&] { (void)combine_master(m, frames); }};
    int stopped = 0;
    for (const auto& f : calls_under_test) {
      try {
        f();
      } catch (const stop_error& e) {
        const bool fp = e.category() == error_category::numeric && e.code() == "cpu.fp_mode" &&
                        fp_mode_bits(e) == dm;
        stopped += fp ? 1 : 0;
      }
    }
    const bool no_kernel = cpu_test::kernel_calls(cpu_path::scalar) == 0 && cpu_test::kernel_calls(cpu_path::avx2) == 0;
    _exit(stopped == 4 && no_kernel ? 0 : 1);
  };
  EXPECT_EXIT(child(), testing::ExitedWithCode(0), "");
}

// Il confine del job controlla MXCSR prima di avviare il job: lo stop si propaga, nessun job parte (generazione dello
// slot invariata), nessun artefatto, nessun kernel.
TEST(Simd, T_M2R5_07_job_with_ftz_stops_before_any_kernel_and_publishes_nothing) {
  seam_guard seam;
  const cal_disk c = write_cal("ftz_job", simd_test::requested());
  cal_store p = cal_project(c, "store");
  master_disk m = master_project("ftz_job_master", simd_test::requested());
  std::optional<stop_error> cal_stop, master_stop;
  {
    memory_pool pool(256 * MiB);
    mxcsr_guard restore;
    _mm_setcsr(_mm_getcsr() | mxcsr_ftz);
    cal_stop = expect_stop_of([&] { (void)run_calibration_job(*p.store, "cal", c.recipe, pool, {}, no_cancel()); },
                              error_category::numeric, "cpu.fp_mode");
    master_stop = expect_stop_of([&] { (void)run_master_job(*m.store, "master", m.recipe, pool, {}, no_cancel()); },
                                 error_category::numeric, "cpu.fp_mode");
  }
  EXPECT_EQ(fp_mode_bits(*cal_stop), mxcsr_ftz);
  EXPECT_EQ(fp_mode_bits(*master_stop), mxcsr_ftz);
  EXPECT_EQ(p.store->get_slot("cal")->current_generation, 0u);
  EXPECT_EQ(m.store->get_slot("master")->current_generation, 0u);
  EXPECT_EQ(artifacts_of(p), 0u);
  EXPECT_EQ(m.store->artifacts_for(m.node.key()).size(), 0u);
  EXPECT_EQ(calls(cpu_path::scalar), 0u);
  EXPECT_EQ(calls(cpu_path::avx2), 0u);
}

// Il fault_injector del task altera MXCSR nel thread del worker: ogni modo vietato ferma il job prima del kernel.
TEST(Simd, T_M2R5_07_fault_injected_in_a_worker_stops_every_task_before_the_kernel) {
  ASSERT_TRUE(hardware_avx2());
  for (const simd_request simd : {simd_request::scalar, simd_request::avx2}) {
    for (const unsigned bad : bad_fp_modes) {
      seam_guard seam;
      SCOPED_TRACE(testing::Message() << simd_request_name(simd) << std::hex << " bit=" << bad);
      const std::string tag = std::string(simd_request_name(simd)) + "_" + std::to_string(bad);
      const cal_disk c = write_cal("ftz_worker_" + tag, simd);
      cal_store p = cal_project(c, "store");
      master_disk m = master_project("ftz_worker_master_" + tag, simd);
      std::uint64_t at_preflight[2] = {0, 0};
      calibration_job_options o;
      o.worker_count = 2;
      o.tile_rows = 2;
      o.hook = [&](std::string_view point) {
        if (point != "preflight.done") return;
        at_preflight[0] = calls(cpu_path::scalar);
        at_preflight[1] = calls(cpu_path::avx2);
      };
      master_job_options mo;
      mo.resources.worker_count = 2;
      mo.resources.tile_rows = 2;
      bool master_frames_read = false;
      mo.hook = [&](std::string_view point) { master_frames_read = master_frames_read || point == "frames.read"; };
      cpu_test::set_task_injector([bad](std::string_view point) {
        if (point == "cpu.task_start") _mm_setcsr(_mm_getcsr() | bad);
        return 0;
      });
      const job_outcome out = run_cal(p, c.recipe, o);
      const std::uint64_t after_cal[2] = {calls(cpu_path::scalar), calls(cpu_path::avx2)};
      const job_outcome mout = run_master(m, mo);
      EXPECT_TRUE(master_frames_read);
      for (const job_outcome* r : {&out, &mout}) {
        EXPECT_EQ(r->exit_code, exit_code_stop);
        ASSERT_TRUE(r->stop.has_value());
        EXPECT_EQ(r->stop->category(), error_category::numeric);
        EXPECT_EQ(r->stop->code(), "cpu.fp_mode");
        EXPECT_EQ(fp_mode_bits(*r->stop), bad);
      }
      EXPECT_EQ(artifacts_of(p), 1u);
      EXPECT_EQ(m.store->artifacts_for(m.node.key()).size(), 1u);
      // Nessun kernel dopo il preflight della calibrazione; il master non ne esegue nel preflight.
      EXPECT_EQ(after_cal[0], at_preflight[0]);
      EXPECT_EQ(after_cal[1], at_preflight[1]);
      EXPECT_EQ(calls(cpu_path::scalar), after_cal[0]);
      EXPECT_EQ(calls(cpu_path::avx2), after_cal[1]);
    }
  }
}

TEST(Simd, T_M2R5_07_task_start_check_on_rows_and_bands_and_injected_errno) {
  seam_guard seam;
  const set_def s = cal::synth_set(8, 4, -64);
  const calibration_recipe r = cal::recipe_of(s);
  prepared_calibration prepared = prepare_calibration(r, cal::images_of(s));
  std::vector<std::byte> scratch(prepared.row_scratch_bytes());
  const std::uint64_t before = calls(cpu_path::scalar) + calls(cpu_path::avx2);
  const group g = make_group(master_role::dark, 5, 3, 3);
  const std::vector<image> frames = images_of(g);
  for (const unsigned bad : bad_fp_modes_with_masks) {
    SCOPED_TRACE(testing::Message() << std::hex << "bit=" << bad);
    mxcsr_guard restore;
    cpu_test::set_task_injector([bad](std::string_view point) {
      if (point == "cpu.task_start") _mm_setcsr(_mm_getcsr() ^ bad);
      return 0;
    });
    const stop_error rows = expect_stop_of([&] { prepared.calibrate_rows(0, 1, scratch.data()); }, error_category::numeric,
                                           "cpu.fp_mode");
    const unsigned after = _mm_getcsr();
    _mm_setcsr(restore.saved);
    EXPECT_EQ(fp_mode_bits(rows), bad);
    EXPECT_EQ(fp_violation(after), bad);
    const stop_error band = expect_stop_of([&] { (void)combine_master(recipe_of(g), frames); }, error_category::numeric,
                                           "cpu.fp_mode");
    _mm_setcsr(restore.saved);
    EXPECT_EQ(fp_mode_bits(band), bad);
    if (HasFailure()) return;  // come sopra: nessun segnale dopo un fallimento gia' registrato
  }
  EXPECT_EQ(calls(cpu_path::scalar) + calls(cpu_path::avx2), before);
  cpu_test::set_task_injector([](std::string_view point) { return point == "cpu.task_start" ? 5 : 0; });
  const stop_error io = expect_stop_of([&] { prepared.calibrate_rows(0, 1, scratch.data()); }, error_category::io,
                                       "cpu.task_start");
  EXPECT_EQ(context_of(io, "errno"), "5");
  const stop_error io_band = expect_stop_of([&] { (void)combine_master(recipe_of(g), frames); }, error_category::io,
                                            "cpu.task_start");
  EXPECT_EQ(context_of(io_band, "errno"), "5");
  EXPECT_EQ(calls(cpu_path::scalar) + calls(cpu_path::avx2), before);
  cpu_test::set_task_injector(nullptr);
  EXPECT_NO_THROW(prepared.calibrate_rows(0, 1, scratch.data()));
  EXPECT_GT(calls(cpu_path::scalar) + calls(cpu_path::avx2), before);
}

// ---------------------------------------------------------------- T-M2R5-07: confini dei job e avvio dei task

namespace {

constexpr unsigned mxcsr_dm = 1u << 8;

// `body` in un processo figlio, con SIGFPE e SIGABRT convertiti in uscite (136, 134, nessun core): senza il controllo
// il segnale termina il figlio e il test fallisce senza abbattere il processo dei test. Uscita 0 se `body` da' true.
[[noreturn]] void run_in_fp_child(const std::function<bool()>& body) {
  struct sigaction action {};
  action.sa_handler = [](int) { _exit(136); };
  sigemptyset(&action.sa_mask);
  if (sigaction(SIGFPE, &action, nullptr) != 0) _exit(2);
  exit_instead_of_core_dump();
  bool ok = false;
  try {
    ok = body();
  } catch (const std::exception& e) {
    std::fprintf(stderr, "eccezione inattesa: %s\n", e.what());
    _exit(3);
  }
  _exit(ok ? 0 : 1);
}

// `f` con DM spenta deve fermarsi con numeric cpu.fp_mode e DM come unico bit di modo nel contesto. Il registro si
// ripristina dopo lo stop solo per i controlli successivi del test.
bool dm_off_stops(const char* what, const std::function<void()>& f) {
  const unsigned base = _mm_getcsr();
  _mm_setcsr(base ^ mxcsr_dm);
  try {
    f();
  } catch (const stop_error& e) {
    const unsigned seen = _mm_getcsr();
    _mm_setcsr(base);
    const bool ok = e.category() == error_category::numeric && e.code() == "cpu.fp_mode" && fp_mode_bits(e) == mxcsr_dm &&
                    fp_violation(seen) == mxcsr_dm;
    if (!ok) std::fprintf(stderr, "%s: stop diverso da cpu.fp_mode con DM spenta: %s\n", what, e.what());
    return ok;
  }
  _mm_setcsr(base);
  std::fprintf(stderr, "%s: nessuno stop\n", what);
  return false;
}

bool generation_is_zero(const dag_store& store, const char* slot) {
  const std::optional<slot_record> s = store.get_slot(slot);
  if (s && s->current_generation == 0) return true;
  std::fprintf(stderr, "slot %s: job avviato (generazione diversa da 0)\n", slot);
  return false;
}

// File di calibrazione di `s` su disco, con la ricetta `r` completata dai percorsi e lo scratch per i blocchi.
cal_disk write_cal_set(const std::string& name, const set_def& s, calibration_recipe r) {
  cal_disk c{cal::fresh_dir("simd_" + name), s, std::move(r)};
  const std::pair<const char*, const frame_def*> frames[4] = {
      {"light.fits", &c.s.light}, {"dark.fits", &c.s.dark}, {"flat.fits", &c.s.flat}, {"dark_flat.fits", &c.s.dark_flat}};
  calibration_input_spec* specs[4] = {&c.recipe.light, &c.recipe.dark, &c.recipe.flat, &c.recipe.dark_flat};
  for (int i = 0; i < 4; ++i) {
    cal::write_bytes(c.dir / frames[i].first, cal::file_of(*frames[i].second, c.s.w, c.s.h));
    specs[i]->path = (c.dir / frames[i].first).string();
  }
  fs::create_directories(c.dir / "scratch");
  return c;
}

// Esito di un job fermato da cpu.fp_mode con DM spenta, con la sola provenance dell'arresto e nessun kernel dopo
// `kernels_before`.
bool dm_off_job_stop(const char* what, const job_outcome& out, const sqlite_dag_store& store, const node_spec& node,
                     std::uint64_t kernels_before) {
  const bool fp = out.exit_code == exit_code_stop && out.stop && out.stop->category() == error_category::numeric &&
                  out.stop->code() == "cpu.fp_mode" && fp_mode_bits(*out.stop) == mxcsr_dm;
  if (!fp) std::fprintf(stderr, "%s: stop diverso: %s\n", what, out.stop ? out.stop->what() : "nessuno");
  const bool only_provenance = store.artifacts_for(node.key()).size() == 1;
  if (!only_provenance) std::fprintf(stderr, "%s: artefatti oltre la provenance\n", what);
  const bool no_kernel = calls(cpu_path::scalar) + calls(cpu_path::avx2) == kernels_before;
  if (!no_kernel) std::fprintf(stderr, "%s: kernel eseguiti\n", what);
  return fp && only_provenance && no_kernel;
}

}  // namespace

// Con DM spenta il confine run_job_files si ferma prima di avviare il job: nessuna generazione dello slot, nessun nodo
// letto (l'analisi del parametro subnormale nel JSON del nodo genererebbe SIGFPE), nessun artefatto, nessun kernel.
// Stesso stop per i job di calibrazione, master, import e per run_branches sullo store durevole.
TEST(Simd, T_M2R5_07_denormal_mask_off_stops_run_job_files_before_the_job_and_the_node) {
  seam_guard seam;
  ASSERT_EQ(fp_violation(_mm_getcsr()), 0u);
  cal_disk c = write_cal("dm_job_files", simd_test::requested());
  c.recipe.q_min = 0x1p-1074;
  const fs::path mdir = cal::fresh_dir("simd_dm_job_files_master");
  const group g = make_group(master_role::dark, 9, 5, 3);
  master_recipe m = recipe_of(g);
  m.simd = simd_test::requested();
  m.exposure_rel_tol = 0x1p-1074;
  write_frames(g, m, mdir, "frame");
  const import_request imp{c.recipe.light.path, c.recipe.light.digest, c.recipe.light.read_options};
  const auto child = [&] {
    run_in_fp_child([&] {
      memory_pool pool(256 * MiB);
      auto store = sqlite_dag_store::create(c.dir / "store");
      const node_spec cal_node = make_calibration_node(c.recipe);
      const node_spec imp_node = make_import_node(imp);
      store->put_node(cal_node);
      store->put_node(imp_node);
      store->create_slot("cal", cal_node.key());
      store->create_slot("branch", cal_node.key());
      store->create_slot("import", imp_node.key());
      auto mstore = sqlite_dag_store::create(mdir / "store");
      const node_spec m_node = make_master_node(m);
      mstore->put_node(m_node);
      mstore->create_slot("master", m_node.key());
      // Con il budget la configurazione delle risorse si serializza prima del confine: soglia PSI subnormale.
      resource_config tiny_psi;
      tiny_psi.psi_threshold.value = 0x1p-1074;
      calibration_job_options with_resources;
      with_resources.resources = tiny_psi;
      master_job_options mo;
      mo.resources = tiny_psi;
      const budget_snapshot budget{};
      bool ok = dm_off_stops("calibrazione con budget", [&] {
        (void)run_calibration_job(*store, "cal", c.recipe, pool, with_resources, no_cancel(), budget);
      });
      ok = dm_off_stops("master con budget", [&] {
        (void)run_master_job(*mstore, "master", m, pool, mo, no_cancel(), budget);
      }) && ok;
      ok = dm_off_stops("calibrazione", [&] {
        (void)run_calibration_job(*store, "cal", c.recipe, pool, {}, no_cancel());
      }) && ok;
      ok = dm_off_stops("master", [&] { (void)run_master_job(*mstore, "master", m, pool, {}, no_cancel()); }) && ok;
      ok = dm_off_stops("import", [&] { (void)run_import_job(*store, "import", imp, pool, no_cancel()); }) && ok;
      ok = dm_off_stops("run_branches", [&] {
        std::vector<branch_files_job> jobs;
        jobs.push_back(branch_files_job{"branch", [](const job_record&) { return job_files_result{}; }});
        (void)run_branches(*store, std::move(jobs));
      }) && ok;
      for (const char* slot : {"cal", "branch", "import"}) ok = generation_is_zero(*store, slot) && ok;
      ok = generation_is_zero(*mstore, "master") && ok;
      const bool no_artifacts = store->artifacts_for(cal_node.key()).empty() &&
                                store->artifacts_for(imp_node.key()).empty() &&
                                mstore->artifacts_for(m_node.key()).empty();
      if (!no_artifacts) std::fprintf(stderr, "artefatti scritti\n");
      const bool no_kernel = calls(cpu_path::scalar) == 0 && calls(cpu_path::avx2) == 0;
      if (!no_kernel) std::fprintf(stderr, "kernel eseguiti\n");
      return ok && no_artifacts && no_kernel;
    });
  };
  EXPECT_EXIT(child(), testing::ExitedWithCode(0), "");
}

// Stesso confine in memoria: run_job e run_branches si fermano prima di avviare il job e prima del lavoro, che con un
// operando subnormale genererebbe SIGFPE.
TEST(Simd, T_M2R5_07_denormal_mask_off_stops_run_job_before_the_job_and_the_work) {
  seam_guard seam;
  ASSERT_EQ(fp_violation(_mm_getcsr()), 0u);
  const auto child = [&] {
    run_in_fp_child([&] {
      memory_dag_store store;
      json params = json::object();
      params.set("x", json::number(0x1p-1074));
      const node_spec node("test.subnormal", 1, 1, std::move(params), {}, sample_type::f64, "cpu_scalar", std::nullopt);
      store.put_node(node);
      store.create_slot("s", node.key());
      store.create_slot("b", node.key());
      bool worked = false;
      const auto work = [&](const job_record& job) {
        volatile double tiny = 0x1p-1074;
        volatile double twice = tiny * 2;
        worked = twice != 0;
        artifact_record a;
        a.node_key = job.node_key;
        a.generation = job.generation;
        a.sha256 = std::string(64, '5');
        a.size_bytes = 8;
        a.kind = "image";
        return job_result{a, {}};
      };
      bool ok = dm_off_stops("run_job", [&] { (void)run_job(store, "s", work); });
      ok = dm_off_stops("run_branches", [&] {
        std::vector<branch_job> jobs;
        jobs.push_back(branch_job{"b", work});
        (void)run_branches(store, std::move(jobs));
      }) && ok;
      ok = generation_is_zero(store, "s") && generation_is_zero(store, "b") && ok;
      if (worked) std::fprintf(stderr, "lavoro del job eseguito\n");
      return ok && !worked;
    });
  };
  EXPECT_EXIT(child(), testing::ExitedWithCode(0), "");
}

// DM spenta nel thread del job dopo il preflight (hook): i worker la ereditano alla creazione. Ogni task della
// residenza a blocchi si ferma con cpu.fp_mode prima di leggere e decodificare il proprio blocco, che contiene campioni
// f32 subnormali (la conversione a f64 genererebbe SIGFPE). Con MXCSR regolare lo stesso job pubblica (controllo).
TEST(Simd, T_M2R5_07_denormal_mask_off_in_the_workers_stops_each_block_before_decoding) {
  seam_guard seam;
  ASSERT_EQ(fp_violation(_mm_getcsr()), 0u);
  const set_def s = cal_set(13, 6, -32, -32, false, false, false, 0);
  calibration_recipe r = cal_recipe(s, false, false);
  r.simd = simd_test::requested();
  const cal_disk c = write_cal_set("dm_workers", s, r);
  const auto child = [&] {
    run_in_fp_child([&] {
      bool ok = true;
      for (const bool dm_off : {false, true}) {
        const std::string tag = dm_off ? "dm" : "ok";
        memory_pool pool(256 * MiB);
        auto store = sqlite_dag_store::create(c.dir / ("store_" + tag));
        const node_spec node = make_calibration_node(c.recipe);
        store->put_node(node);
        store->create_slot("cal", node.key());
        calibration_job_options o;
        o.resources = resource_config{};
        o.resources->scratch = dir_quota{fs::absolute(c.dir / "scratch"), std::uint64_t{1} << 32};
        o.resources->tile_rows = 2;
        o.resources->worker_count = 2;
        o.checkpoint = calibration_job_options::checkpoint_options{"dm_workers_" + tag, 1000, false};
        std::optional<job_plan> seen;
        o.on_job_plan = [&](const job_plan& jp) { seen = jp; };
        std::uint64_t at_preflight = 0;
        o.hook = [&](std::string_view point) {
          if (point != "preflight.done") return;
          at_preflight = calls(cpu_path::scalar) + calls(cpu_path::avx2);
          if (dm_off) _mm_setcsr(_mm_getcsr() ^ mxcsr_dm);
        };
        const unsigned base = _mm_getcsr();
        const job_outcome out = run_calibration_job(*store, "cal", c.recipe, pool, o, no_cancel());
        _mm_setcsr(base);
        if (!seen || !seen->spill) {
          std::fprintf(stderr, "calibrazione %s: residenza diversa dai blocchi\n", tag.c_str());
          ok = false;
        }
        if (!dm_off && out.exit_code != 0) {
          std::fprintf(stderr, "calibrazione di controllo fermata: %s\n", out.stop ? out.stop->what() : "");
          ok = false;
        }
        if (dm_off) ok = dm_off_job_stop("calibrazione", out, *store, node, at_preflight) && ok;
      }
      return ok;
    });
  };
  EXPECT_EXIT(child(), testing::ExitedWithCode(0), "");
}

// Come sopra per il master: DM spenta dopo la lettura dei frame del preflight; ogni task si ferma prima di leggere i
// blocchi di righe dei frame, con campioni f32 subnormali.
TEST(Simd, T_M2R5_07_denormal_mask_off_in_the_workers_stops_each_master_band_before_reading) {
  seam_guard seam;
  ASSERT_EQ(fp_violation(_mm_getcsr()), 0u);
  const fs::path mdir = cal::fresh_dir("simd_dm_workers_master");
  const group g = master_group(master_role::dark, 9, 5, 3, -32, 0);
  master_recipe m = recipe_of(g);
  m.simd = simd_test::requested();
  write_frames(g, m, mdir, "frame");
  const auto child = [&] {
    run_in_fp_child([&] {
      bool ok = true;
      for (const bool dm_off : {false, true}) {
        const std::string tag = dm_off ? "dm" : "ok";
        memory_pool pool(256 * MiB);
        auto store = sqlite_dag_store::create(mdir / ("store_" + tag));
        const node_spec node = make_master_node(m);
        store->put_node(node);
        store->create_slot("master", node.key());
        master_job_options mo;
        mo.resources.worker_count = 2;
        mo.resources.tile_rows = 2;
        bool frames_read = false;
        mo.hook = [&](std::string_view point) {
          if (point != "frames.read") return;
          frames_read = true;
          if (dm_off) _mm_setcsr(_mm_getcsr() ^ mxcsr_dm);
        };
        const std::uint64_t before = calls(cpu_path::scalar) + calls(cpu_path::avx2);
        const unsigned base = _mm_getcsr();
        const job_outcome out = run_master_job(*store, "master", m, pool, mo, no_cancel());
        _mm_setcsr(base);
        if (!frames_read) {
          std::fprintf(stderr, "master %s: frame non letti\n", tag.c_str());
          ok = false;
        }
        if (!dm_off && out.exit_code != 0) {
          std::fprintf(stderr, "master di controllo fermato: %s\n", out.stop ? out.stop->what() : "");
          ok = false;
        }
        if (dm_off) ok = dm_off_job_stop("master", out, *store, node, before) && ok;
      }
      return ok;
    });
  };
  EXPECT_EXIT(child(), testing::ExitedWithCode(0), "");
}
