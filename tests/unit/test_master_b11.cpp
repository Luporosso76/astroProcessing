// M3.b, unità B11: report del master (C-02.b §b.7 «Report del master» e sezione «Analisi», §b.10 avvisi; C-02.b-flat §f.7,
// §f.10, §f.13).
// Test:
// - T-M3B-C02-11, parte master: per `single`, `mean`, `weighted_mean`, `median`, `winsorized`, `mean` + `sigma_clip`,
//   campo per campo: rumore (`residual_mean_adu`, `residual_rms_adu` per frame e canale), artefatti (`invalid_samples` per
//   frame; per canale conteggi per bit e `pixels_with_rejection`), stato (κ, `a_f`, `c_f`), ipotesi e avvisi; costanti
//   nei `details` del job; casi dyadic scritti a mano e casi preregistrati `c02_11_cases` (seed 2026110964);
// - T-M3B-C02-11, parte analisi: sezione «Analisi» del report costruita dal parametro `analysis` del nodo (B14), con
//   chiave del gruppo, metodo scelto e `proposal_not_followed`;
// - T-M3B-FLAT-07: descrittore, `measure` e report per ogni metodo con `median_ratio` e con `none`: `frame_normalization`,
//   `variance_estimator`, codice di capability, ipotesi e avvisi esatti; `m`, `r`, `m_H`, `ρ`, valore del controllo,
//   `norm_level_bias_bound` e rapporti col piano contro l'oracle; `level_bias_estimate` null con motivo `normalized`;
// - T-M3B-C02-10 e T-M3B-FLAT-08, parte report: report del job con `tile_rows` ∈ {1, 7, 64} e worker ∈ {1, 4} uguale
//   byte per byte al report in memoria, per ogni metodo;
// - T-M3B-C02-15, solo `master.report_nonfinite`: statistica di rumore non rappresentabile, stop senza artefatti e senza
//   lavoro sullo store oltre uno stop di riferimento allo stesso punto (contatore del fault_injector);
// - `master_algorithm_version` = 4.
//
// Convenzioni. Nessun valore atteso dal prodotto: nei casi dyadic master, residui e costanti sono calcolati a mano in
// aritmetica esatta (la sola operazione arrotondata dell'oracle è quella che il contratto fissa: una divisione per `P`,
// una radice); κ dal valore chiuso 1 − √3/π (mediana di 3, winsorized di 3 con g = 1) con la tolleranza di
// `kappa_rel_error_max`. Nei casi preregistrati l'oracle dei residui riesegue in f64 la sequenza fissata da C-02.b §b.7
// (r = x_f − M, somma per riga in ordine di colonna, poi righe in ordine crescente) sul master pubblicato e sulla sua
// maschera, che T-M3B-C02-01…07 verificano a parte.
//
// FORMA DEL REPORT SOTTO PROVA (decisioni del coordinatore in docs/decisions/M3b-lacune.md, «Decisioni successive (B11)»
// e «Correzioni della review di B11 (giro 1)»):
//   frames[k]                       ordine canonico, come oggi
//     .invalid_samples              campioni del frame con almeno un bit di invalidità, su tutti i pixel
//     .residuals[c]                 {label, residual_mean_adu, residual_rms_adu} nell'ordine di `channels`
//                                   (null nei due valori per un canale senza pixel validi)
//   channels[c]
//     .invalid_counts               {nome del bit: conteggio} dei pixel del canale (accanto al totale `invalid_counts`)
//     .pixels_with_rejection        pixel con almeno un campione rifiutato (0 senza `sigma_clip`)
//     .flat_normalization           solo flat: null con `none`; con `median_ratio`
//                                   {level_ratio, additive_bound_adu, rel_budget e check_value (null con `mean`),
//                                    norm_level_bias_bound, bound_ratio_median, bound_ratio_max, bound_ratio_null,
//                                    bound_ratio_null_reason = "variance_zero"}
//   constants                       {kappa, a_f, c_f, flat_scales}: κ (metodi robusti) o null; a_f e c_f array
//                                   nell'ordine di `frames` (`weighted_mean`) o null; flat_scales con `median_ratio`
//                                   [{label, m:[…], r:[…], m_H, rho}] per canale, frame nell'ordine di `frames`, o null.
//                                   Lo stesso oggetto è `details.master_constants` della provenance del job.
//   analysis                        null se il parametro `analysis` del nodo è null; altrimenti
//                                   {role, candidates (come nel parametro), group {reference, members, status, key,
//                                    proposal, proposal_absent_reason, alternatives}, chosen {combine, rejection, flat_normalization},
//                                    proposal_followed}; `key` = chiave esatta di C-02.b §b.2 punto 2, prodotta da
//                                   `propose_masters` nel gruppo del parametro `analysis`
//   assumptions_not_verified        insieme dei nomi di §b.3 e §f.3 per il metodo
//   warnings                        insieme esatto: quelli di `.a` più §b.10 e §f.10
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "astro/batch_classification.hpp"
#include "astro/master_analysis.hpp"
#include "astro/master_kappa.hpp"
#include "master_support.hpp"
#include "store_test_support.hpp"
#include "syn_cal.hpp"

namespace {

using namespace mst;
namespace sc = astro_fixture::syn_cal;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr std::uint64_t kMiB = std::uint64_t{1} << 20;
const long double kPi = 3.141592653589793238462643383279502884L;

// ---------------------------------------------------------------- lettura sicura del report

const json* find_path(const json& doc, std::initializer_list<const char*> path) {
  const json* cur = &doc;
  for (const char* k : path) {
    if (cur->type() != json::kind::object) return nullptr;
    cur = cur->find(k);
    if (cur == nullptr) return nullptr;
  }
  return cur;
}

std::optional<double> num_of(const json* j) {
  if (j == nullptr) return std::nullopt;
  if (j->type() == json::kind::integer) return static_cast<double>(j->as_integer());
  if (j->type() == json::kind::number) return j->as_number();
  return std::nullopt;
}
std::optional<double> num_at(const json& doc, std::initializer_list<const char*> path) { return num_of(find_path(doc, path)); }

std::optional<std::string> str_of(const json* j) {
  if (j == nullptr || j->type() != json::kind::string) return std::nullopt;
  return j->as_string();
}
std::optional<std::string> str_at(const json& doc, std::initializer_list<const char*> path) { return str_of(find_path(doc, path)); }

bool null_at(const json& doc, std::initializer_list<const char*> path) {
  const json* j = find_path(doc, path);
  return j != nullptr && j->type() == json::kind::null;
}

const std::vector<json>& items_at(const json& doc, std::initializer_list<const char*> path) {
  static const std::vector<json> none;
  const json* j = find_path(doc, path);
  if (j == nullptr || j->type() != json::kind::array) return none;
  return j->items();
}

std::set<std::string> strings_at(const json& doc, std::initializer_list<const char*> path) {
  std::set<std::string> out;
  for (const json& e : items_at(doc, path)) {
    if (e.type() == json::kind::string) out.insert(e.as_string());
  }
  return out;
}

// Oggetto del frame con quel digest fra `frames` del report.
const json* frame_by_digest(const json& report, const std::string& sha) {
  for (const json& f : items_at(report, {"frames"})) {
    if (str_at(f, {"digest_sha256"}) == sha) return &f;
  }
  return nullptr;
}

// Posizione del frame nell'array `frames` del report (ordine canonico).
std::optional<std::size_t> frame_index(const json& report, const std::string& sha) {
  const std::vector<json>& fr = items_at(report, {"frames"});
  for (std::size_t k = 0; k < fr.size(); ++k) {
    if (str_at(fr[k], {"digest_sha256"}) == sha) return k;
  }
  return std::nullopt;
}

std::optional<master_result> combine_or_fail(const master_recipe& r, std::vector<image> frames, const std::string& what) {
  try {
    return combine_master(r, std::move(frames));
  } catch (const stop_error& e) {
    ADD_FAILURE() << what << ": stop " << e.code() << " — " << e.what();
    return std::nullopt;
  }
}

::testing::AssertionResult rel_close(std::optional<double> got, long double want, long double rel, const char* what) {
  if (!got) return ::testing::AssertionFailure() << what << ": campo assente o non numerico";
  const long double d = std::fabs(static_cast<long double>(*got) - want);
  if (d <= rel * std::fabs(want)) return ::testing::AssertionSuccess();
  return ::testing::AssertionFailure() << what << ": " << std::setprecision(20) << *got << " invece di " << want
                                       << " (scarto relativo " << d / std::fabs(want) << ")";
}

// Tolleranza delle diagnostiche con poche operazioni f64 arrotondate (regola I, limite di γ_8 relativo).
constexpr long double kFewOps = 8.0L * 0x1p-53L / (1.0L - 8.0L * 0x1p-53L);

// ---------------------------------------------------------------- metodi

enum class kind { single, mean, weighted, median, winsorized, sigma_clip };

const char* name_of(kind k) {
  switch (k) {
    case kind::single: return "single";
    case kind::mean: return "mean";
    case kind::weighted: return "weighted_mean";
    case kind::median: return "median";
    case kind::winsorized: return "winsorized";
    case kind::sigma_clip: return "sigma_clip";
  }
  return "";
}

const char* plane_of(kind k) {
  switch (k) {
    case kind::single: return "read_noise2_plus_signal_over_gain";
    case kind::mean: return "s2_over_n";
    case kind::weighted: return "weighted_residuals";
    case kind::median:
    case kind::winsorized: return "kappa_s2";
    case kind::sigma_clip: return "kept_s2_over_m";
  }
  return "";
}

const char* estimator_of(kind k) {
  switch (k) {
    case kind::single: return "noise_model";
    case kind::mean: return "empirical";
    case kind::weighted: return "empirical_weighted";
    case kind::median:
    case kind::winsorized: return "gaussian_reference";
    case kind::sigma_clip: return "frozen_selection";
  }
  return "";
}

bool robust(kind k) { return k == kind::median || k == kind::winsorized; }

// Pesi per indice di frame del gruppo: n = 4 (3, 3, 1, 1), W = 8; n = 3 (2, 3, 3), W = 8; altrimenti 2, 3, 2, 3, …
double weight_of(std::size_t n, std::size_t f) {
  if (n == 4) return f < 2 ? 3.0 : 1.0;
  if (n == 3) return f == 0 ? 2.0 : 3.0;
  return 2.0 + static_cast<double>(f % 2);
}

// κ = 1 per n ∈ {3, 4} ((n − 1)² > κ² n), κ = 2 per n = 11; tre iterazioni, almeno 2 tenuti; g = 1 (n ≤ 4) o 2.
void apply(kind k, master_recipe& r) {
  const std::size_t n = r.frames.size();
  r.combine = combine_method::mean;
  r.rejection = master_rejection::none;
  switch (k) {
    case kind::single:
      r.combine = combine_method::single;
      r.offset_adu = 10;
      r.conversion_gain_e_per_adu = 1.5;
      r.read_noise_adu = 2;
      break;
    case kind::mean: break;
    case kind::weighted:
      r.combine = combine_method::weighted_mean;
      for (std::size_t f = 0; f < n; ++f) r.frames[f].weight = weight_of(n, f);
      break;
    case kind::median: r.combine = combine_method::median; break;
    case kind::winsorized:
      r.combine = combine_method::winsorized;
      r.winsor_g = n >= 11 ? 2.0 : 1.0;
      break;
    case kind::sigma_clip:
      r.rejection = master_rejection::sigma_clip;
      r.clip_kappa_low = r.clip_kappa_high = n >= 11 ? 2.0 : 1.0;
      r.clip_max_iter = 3;
      r.clip_min_kept = 2;
      break;
  }
}

master_recipe dark_recipe(const group& g, kind k) {
  master_recipe r = recipe_of(g);
  apply(k, r);
  r.output_type = sample_type::f64;
  return r;
}

constexpr double kA = 32;         // flat_additive_bound_adu
constexpr double kBudget = 0.5;   // flat_norm_rel_budget (vietato con `mean`)

master_recipe ratio_recipe(const group& g, kind k, double a = kA, double budget = kBudget) {
  master_recipe r = recipe_of(g);
  r.flat_norm = flat_normalization::median_ratio;
  r.flat_level_rel_tol.reset();
  apply(k, r);
  set_flat_additive_bound(r, a);
  set_flat_norm_budget(r, k == kind::mean ? std::optional<double>{} : std::optional<double>{budget});
  r.output_type = sample_type::f64;
  return r;
}

const std::array<kind, 6> kAll = {kind::single, kind::mean, kind::weighted, kind::median, kind::winsorized, kind::sigma_clip};
const std::array<kind, 5> kRatio = {kind::mean, kind::weighted, kind::median, kind::winsorized, kind::sigma_clip};

// ---------------------------------------------------------------- oracle esatto del master su pochi valori

rat sum_of(const std::vector<rat>& v) {
  rat s = exact::from_u(0);
  for (const rat& x : v) s = s + x;
  return s;
}

rat mean_of(const std::vector<rat>& v) { return sum_of(v) / exact::from_u(v.size()); }

std::vector<rat> sorted(std::vector<rat> v) {
  std::sort(v.begin(), v.end(), [](const rat& a, const rat& b) { return a < b; });
  return v;
}

// Media a k-sigma con soglie uguali κ: s² campionaria (n − 1), rifiuto se (x − m)² > κ² s², fino a `iters` passi.
// Premessa dei dati del test: le decisioni non dipendono dalla scelta n o n − 1 né dal confine (nessun valore sulla soglia).
rat clip_mean(const std::vector<rat>& v, long kappa, int iters) {
  std::vector<rat> kept = v;
  for (int it = 0; it < iters; ++it) {
    const rat m = mean_of(kept);
    rat ss = exact::from_u(0);
    for (const rat& x : kept) ss = ss + (x - m) * (x - m);
    const rat s2 = ss / exact::from_u(kept.size() - 1);
    const rat k2 = exact::from_u(static_cast<std::uint64_t>(kappa * kappa));
    std::vector<rat> next;
    for (const rat& x : kept) {
      if (!(k2 * s2 < (x - m) * (x - m))) next.push_back(x);
    }
    if (next.size() == kept.size()) break;
    kept = next;
  }
  return mean_of(kept);
}

// Master esatto del metodo sui valori `v` (indice del frame del gruppo) con i pesi `w`.
rat master_of(kind k, const std::vector<rat>& v, const std::vector<double>& w) {
  const std::size_t n = v.size();
  switch (k) {
    case kind::single: return v[0];
    case kind::mean: return mean_of(v);
    case kind::weighted: {
      rat s = exact::from_u(0), W = exact::from_u(0);
      for (std::size_t f = 0; f < n; ++f) {
        s = s + exact::from_double(w[f]) * v[f];
        W = W + exact::from_double(w[f]);
      }
      return s / W;
    }
    case kind::median: {
      const std::vector<rat> s = sorted(v);
      return n % 2 ? s[n / 2] : (s[n / 2 - 1] + s[n / 2]) / exact::from_u(2);
    }
    case kind::winsorized: {
      std::vector<rat> s = sorted(v);
      const std::size_t g = 1;
      for (std::size_t i = 0; i < g; ++i) {
        s[i] = s[g];
        s[n - 1 - i] = s[n - 1 - g];
      }
      return mean_of(s);
    }
    case kind::sigma_clip: return clip_mean(v, 1, 3);
  }
  return exact::from_u(0);
}

// ---------------------------------------------------------------- casi dyadic dei dark

// 8 × 4 mono (8 × 5 con la riga NaN) o 8 × 8 RGGB. Base 1000 + 2 (i mod 7); un picco di +64 nel frame (i / 4) mod n
// sui pixel con i mod 4 = 1. Riga NaN (ultima): campione NaN nel frame x mod n, pixel invalido nel master.
struct dyadic_case {
  group g;
  std::size_t valid_rows = 0;
};

dyadic_case dyadic_dark(std::size_t n, bool cfa, bool nan_row) {
  const std::uint32_t w = 8, valid_h = cfa ? 8 : 4, h = valid_h + (nan_row ? 1 : 0);
  dyadic_case c;
  c.g = make_group(master_role::dark, w, h, n);
  c.valid_rows = valid_h;
  for (std::size_t f = 0; f < n; ++f) {
    if (cfa) c.g.frames[f].hdr.bayer = "RGGB";
    for (std::size_t i = 0; i < static_cast<std::size_t>(w) * valid_h; ++i) {
      double v = 1000.0 + 2.0 * static_cast<double>(i % 7);
      if (i % 4 == 1 && (i / 4) % n == f) v += 64.0;
      c.g.frames[f].v[i] = v;
    }
    if (nan_row) {
      for (std::uint32_t x = 0; x < w; ++x) {
        const std::size_t i = static_cast<std::size_t>(valid_h) * w + x;
        c.g.frames[f].v[i] = x % n == f ? kNaN : 1000.0;
      }
    }
  }
  return c;
}

std::size_t channel_at(bool cfa, std::uint32_t x, std::uint32_t y) { return cfa ? (y % 2) * 2 + (x % 2) : 0; }

struct resid_want {
  double mean = 0, rms = 0;
  std::size_t pixels = 0;
};

// Residui esatti per frame (indice del gruppo) e canale: somme esatte di r = x_f − M sui pixel validi, poi una divisione
// per P e una radice in f64, come C-02.b §b.7.
std::vector<std::vector<resid_want>> dyadic_residuals(const dyadic_case& c, kind k, const std::vector<double>& w) {
  const group& g = c.g;
  const bool cfa = !g.frames[0].hdr.bayer.empty();
  const std::size_t n = g.frames.size(), channels = cfa ? 4 : 1;
  std::vector<std::vector<rat>> s1(n, std::vector<rat>(channels, exact::from_u(0))), s2 = s1;
  std::vector<std::size_t> P(channels, 0);
  for (std::uint32_t y = 0; y < c.valid_rows; ++y) {
    for (std::uint32_t x = 0; x < g.w; ++x) {
      const std::size_t i = static_cast<std::size_t>(y) * g.w + x, ch = channel_at(cfa, x, y);
      std::vector<rat> v;
      for (const mframe& f : g.frames) v.push_back(exact::from_double(f.v[i]));
      const rat m = master_of(k, v, w);
      ++P[ch];
      for (std::size_t f = 0; f < n; ++f) {
        const rat r = v[f] - m;
        s1[f][ch] = s1[f][ch] + r;
        s2[f][ch] = s2[f][ch] + r * r;
      }
    }
  }
  std::vector<std::vector<resid_want>> out(n, std::vector<resid_want>(channels));
  for (std::size_t f = 0; f < n; ++f) {
    for (std::size_t ch = 0; ch < channels; ++ch) {
      const double a = sc::exact_double(s1[f][ch], "somma dei residui");
      const double b = sc::exact_double(s2[f][ch], "somma dei quadrati");
      const double p = static_cast<double>(P[ch]);
      out[f][ch] = {a / p, std::sqrt(b / p), P[ch]};
    }
  }
  return out;
}

std::vector<double> weights_of(const master_recipe& r) {
  std::vector<double> w;
  for (const master_frame_spec& s : r.frames) w.push_back(s.weight.value_or(1.0));
  return w;
}

void expect_residuals(const json& report, const master_recipe& r, const std::vector<std::vector<resid_want>>& want,
                      const std::vector<std::string>& labels) {
  for (std::size_t f = 0; f < want.size(); ++f) {
    SCOPED_TRACE("frame del gruppo " + std::to_string(f));
    const json* fr = frame_by_digest(report, r.frames[f].digest.sha256);
    ASSERT_NE(fr, nullptr) << "frame assente dal report";
    const std::vector<json>& res = items_at(*fr, {"residuals"});
    ASSERT_EQ(res.size(), want[f].size()) << "frames[k].residuals: un elemento per canale";
    for (std::size_t c = 0; c < want[f].size(); ++c) {
      SCOPED_TRACE("canale " + labels[c]);
      EXPECT_EQ(str_at(res[c], {"label"}), std::optional<std::string>(labels[c]));
      EXPECT_EQ(num_at(res[c], {"residual_mean_adu"}), std::optional<double>(want[f][c].mean)) << std::setprecision(17);
      EXPECT_EQ(num_at(res[c], {"residual_rms_adu"}), std::optional<double>(want[f][c].rms)) << std::setprecision(17);
    }
  }
}

// ================================================================ versione dell'algoritmo

// B11 completa il report di `median_ratio` (avviso, ipotesi, provenance delle costanti): versione 4 (decisione di B9).
TEST(MasterB11, master_algorithm_version_is_4_and_enters_the_node) {
  EXPECT_EQ(master_algorithm_version, 4u);
  const dyadic_case c = dyadic_dark(4, false, false);
  EXPECT_EQ(make_master_node(dark_recipe(c.g, kind::mean)).algorithm_version(), 4u);
}

// ================================================================ T-M3B-C02-11, parte master: rumore

TEST(MasterB11, T_M3B_C02_11_residual_mean_and_rms_per_frame_are_exact_for_every_method_mono) {
  for (const kind k : kAll) {
    SCOPED_TRACE(name_of(k));
    const dyadic_case c = dyadic_dark(k == kind::single ? 1 : 4, false, true);
    const master_recipe r = dark_recipe(c.g, k);
    const std::optional<master_result> res = combine_or_fail(r, images_of(c.g), name_of(k));
    if (!res) continue;
    const auto want = dyadic_residuals(c, k, weights_of(r));
    ASSERT_EQ(want[0][0].pixels, 32u) << "premessa: i pixel della riga NaN non entrano in P";
    expect_residuals(res->report, r, want, {"all"});
  }
}

TEST(MasterB11, T_M3B_C02_11_residuals_are_per_channel_for_a_cfa_master) {
  for (const kind k : {kind::mean, kind::weighted, kind::median, kind::sigma_clip}) {
    SCOPED_TRACE(name_of(k));
    const dyadic_case c = dyadic_dark(4, true, false);
    const master_recipe r = dark_recipe(c.g, k);
    const std::optional<master_result> res = combine_or_fail(r, images_of(c.g), name_of(k));
    if (!res) continue;
    const auto want = dyadic_residuals(c, k, weights_of(r));
    expect_residuals(res->report, r, want, {"R", "G1", "G2", "B"});
  }
}

// Casi preregistrati (c02_11_cases): l'oracle riesegue in f64 la sequenza di C-02.b §b.7 sul master pubblicato e sulla
// sua maschera; uguaglianza bit a bit.
TEST(MasterB11, T_M3B_C02_11_residuals_follow_the_fixed_summation_order_on_the_preregistered_cases) {
  const sc::gaussian_cases cases = sc::c02_11_cases();
  const std::vector<sc::frames> x = sc::realize_cases(cases);
  for (std::size_t ci = 0; ci < cases.sets.size(); ++ci) {
    for (const kind k : kAll) {
      if (k == kind::single && ci != 0) continue;
      for (const bool cfa : {false, true}) {
        const std::size_t n = k == kind::single ? 1 : cases.n[ci];
        SCOPED_TRACE(std::string(name_of(k)) + " n=" + std::to_string(n) + (cfa ? " cfa" : " mono"));
        group g = make_group(master_role::dark, cases.sets[ci].width, cases.sets[ci].height, n);
        for (std::size_t f = 0; f < n; ++f) {
          g.frames[f].v = x[ci][f];
          if (cfa) g.frames[f].hdr.bayer = "RGGB";
        }
        const master_recipe r = dark_recipe(g, k);
        const std::optional<master_result> res = combine_or_fail(r, images_of(g), name_of(k));
        if (!res) continue;
        const std::size_t channels = cfa ? 4 : 1;
        const std::vector<std::string> labels = cfa ? std::vector<std::string>{"R", "G1", "G2", "B"}
                                                    : std::vector<std::string>{"all"};
        std::vector<std::vector<resid_want>> want(n, std::vector<resid_want>(channels));
        for (std::size_t f = 0; f < n; ++f) {
          for (std::size_t ch = 0; ch < channels; ++ch) {
            double total = 0, total2 = 0;
            std::size_t P = 0;
            for (std::uint32_t y = 0; y < g.h; ++y) {
              double row = 0, row2 = 0;
              for (std::uint32_t xx = 0; xx < g.w; ++xx) {
                const std::size_t i = static_cast<std::size_t>(y) * g.w + xx;
                if (channel_at(cfa, xx, y) != ch || res->output.invalid.bits[i] != 0) continue;
                const double rr = g.frames[f].v[i] - sample(res->output, i);
                row += rr;
                row2 += rr * rr;
                ++P;
              }
              total += row;
              total2 += row2;
            }
            want[f][ch] = {total / static_cast<double>(P), std::sqrt(total2 / static_cast<double>(P)), P};
          }
        }
        expect_residuals(res->report, r, want, labels);
      }
    }
  }
}

// ================================================================ T-M3B-C02-11, parte master: artefatti

TEST(MasterB11, T_M3B_C02_11_artifact_counts_per_frame_and_per_channel) {
  for (const kind k : kAll) {
    SCOPED_TRACE(name_of(k));
    const std::size_t n = k == kind::single ? 1 : 4;
    const dyadic_case c = dyadic_dark(n, false, true);
    const master_recipe r = dark_recipe(c.g, k);
    const std::optional<master_result> res = combine_or_fail(r, images_of(c.g), name_of(k));
    if (!res) continue;
    // Riga NaN: 8 campioni, nel frame x mod n. Picchi: pixel i mod 4 = 1 nelle righe valide, 8 pixel, 2 per frame (n = 4);
    // `sigma_clip` (κ = 1) li rifiuta tutti, gli altri metodi non rifiutano.
    for (std::size_t f = 0; f < n; ++f) {
      const json* fr = frame_by_digest(res->report, r.frames[f].digest.sha256);
      ASSERT_NE(fr, nullptr);
      EXPECT_EQ(num_at(*fr, {"invalid_samples"}), std::optional<double>(8.0 / static_cast<double>(n))) << "frame " << f;
      EXPECT_EQ(num_at(*fr, {"rejected_samples"}), std::optional<double>(k == kind::sigma_clip ? 2.0 : 0.0)) << "frame " << f;
    }
    const std::vector<json>& ch = items_at(res->report, {"channels"});
    ASSERT_EQ(ch.size(), 1u);
    EXPECT_EQ(num_at(ch[0], {"invalid_counts", "source_nonfinite"}), std::optional<double>(8.0));
    EXPECT_EQ(num_at(ch[0], {"invalid_counts", "support_incomplete"}), std::optional<double>(0.0));
    EXPECT_EQ(num_at(ch[0], {"pixels_with_rejection"}), std::optional<double>(k == kind::sigma_clip ? 8.0 : 0.0));
  }
}

// Conteggi per bit per canale: un campione NaN in (0, 0) del frame 0 (R) e uno in (1, 1) del frame 1 (B).
TEST(MasterB11, T_M3B_C02_11_bit_counts_are_per_channel_for_a_cfa_master) {
  dyadic_case c = dyadic_dark(4, true, false);
  c.g.frames[0].v[0] = kNaN;
  c.g.frames[1].v[1 * c.g.w + 1] = kNaN;
  const master_recipe r = dark_recipe(c.g, kind::mean);
  const std::optional<master_result> res = combine_or_fail(r, images_of(c.g), "mean cfa");
  ASSERT_TRUE(res.has_value());
  const std::vector<json>& ch = items_at(res->report, {"channels"});
  ASSERT_EQ(ch.size(), 4u);
  const std::array<double, 4> want = {1, 0, 0, 1};
  for (std::size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(num_at(ch[i], {"invalid_counts", "source_nonfinite"}), std::optional<double>(want[i])) << i;
  }
}

// ================================================================ T-M3B-C02-11, parte master: stato

// κ dal valore chiuso: Var(mediana di 3 N(0, 1)) = 1 − √3/π; winsorized di 3 con g = 1 coincide con la mediana.
TEST(MasterB11, T_M3B_C02_11_state_reports_kappa_for_the_robust_methods) {
  const long double k3 = 1.0L - std::sqrt(3.0L) / kPi;
  for (const kind k : kAll) {
    SCOPED_TRACE(name_of(k));
    const dyadic_case c = dyadic_dark(k == kind::single ? 1 : 3, false, false);
    const master_recipe r = dark_recipe(c.g, k);
    const std::optional<master_result> res = combine_or_fail(r, images_of(c.g), name_of(k));
    if (!res) continue;
    if (robust(k)) {
      EXPECT_TRUE(rel_close(num_at(res->report, {"constants", "kappa"}), k3,
                            static_cast<long double>(kappa_rel_error_max) + kFewOps, "kappa"));
    } else {
      EXPECT_TRUE(null_at(res->report, {"constants", "kappa"})) << "constants.kappa null fuori dai metodi robusti";
    }
  }
}

// Pesi (2, 3, 3), W = 8: a = (1/4, 3/8, 3/8), T = 5/4, c = (1/18, 1/4, 1/4). Ogni operazione è esatta tranne il quoziente
// di c per a = 1/4, che è un solo arrotondamento di 1/18.
TEST(MasterB11, T_M3B_C02_11_state_reports_a_f_and_c_f_for_weighted_mean_in_canonical_order) {
  for (const kind k : kAll) {
    SCOPED_TRACE(name_of(k));
    const dyadic_case c = dyadic_dark(k == kind::single ? 1 : 3, false, false);
    const master_recipe r = dark_recipe(c.g, k);
    const std::optional<master_result> res = combine_or_fail(r, images_of(c.g), name_of(k));
    if (!res) continue;
    if (k != kind::weighted) {
      EXPECT_TRUE(null_at(res->report, {"constants", "a_f"}));
      EXPECT_TRUE(null_at(res->report, {"constants", "c_f"}));
      continue;
    }
    const std::vector<json>& a = items_at(res->report, {"constants", "a_f"});
    const std::vector<json>& cf = items_at(res->report, {"constants", "c_f"});
    ASSERT_EQ(a.size(), 3u);
    ASSERT_EQ(cf.size(), 3u);
    for (std::size_t f = 0; f < 3; ++f) {
      const std::optional<std::size_t> pos = frame_index(res->report, r.frames[f].digest.sha256);
      ASSERT_TRUE(pos.has_value());
      const double w = weight_of(3, f);
      EXPECT_EQ(num_of(&a[*pos]), std::optional<double>(w / 8.0)) << "a_f del frame " << f;
      EXPECT_EQ(num_of(&cf[*pos]), std::optional<double>(w == 2.0 ? 1.0 / 18.0 : 0.25)) << "c_f del frame " << f;
    }
  }
}

TEST(MasterB11, T_M3B_C02_11_constants_are_null_where_the_method_has_none) {
  const dyadic_case c = dyadic_dark(4, false, false);
  const std::optional<master_result> res = combine_or_fail(dark_recipe(c.g, kind::mean), images_of(c.g), "mean");
  ASSERT_TRUE(res.has_value());
  const json* cst = find_path(res->report, {"constants"});
  ASSERT_NE(cst, nullptr) << "sezione constants assente";
  ASSERT_EQ(cst->type(), json::kind::object);
  for (const char* key : {"kappa", "a_f", "c_f", "flat_scales"}) EXPECT_TRUE(null_at(*cst, {key})) << key;
}

// ================================================================ T-M3B-C02-11, parte master: ipotesi, avvisi, piano

std::set<std::string> dark_warnings(kind k) {
  std::set<std::string> w = {"saturation_not_evaluated"};
  if (robust(k)) w.insert("gaussian_reference_not_verified");
  if (k == kind::sigma_clip) w.insert("frozen_selection_approximation");
  if (k == kind::single) w.insert("noise_model_variance");
  return w;
}

TEST(MasterB11, T_M3B_C02_11_warnings_are_exactly_those_of_the_method) {
  for (const kind k : kAll) {
    SCOPED_TRACE(name_of(k));
    const dyadic_case c = dyadic_dark(k == kind::single ? 1 : 4, false, false);
    const master_recipe r = dark_recipe(c.g, k);
    const std::optional<master_result> res = combine_or_fail(r, images_of(c.g), name_of(k));
    if (!res) continue;
    EXPECT_EQ(strings_at(res->report, {"warnings"}), dark_warnings(k));
    const std::set<std::string> in_result(res->warnings.begin(), res->warnings.end());
    EXPECT_EQ(in_result, dark_warnings(k)) << "master_result::warnings come il report";
  }
}

// Oracle dell'insieme esatto delle ipotesi non verificate, scritto dal contratto e dalle decisioni (non dal prodotto):
// - C-02.a §3: `frames_independent`, `outliers_sparse`; bias, dark, dark-flat `stationary_mean`; flat `none` le ipotesi
//   del §6.2 e del §7 (`illumination_stable` dal §3, `same_illumination_shape`, `level_proportional`,
//   `additive_nonnegative`, `nonnegative_signal`);
// - C-02.b §b.3: mediana e winsorized `iid_gaussian`, `continuous_samples`; `single`, `weighted_mean`, `sigma_clip`
//   nessun nome in più (decisione B11 per `single`; per `sigma_clip` la selezione congelata è un avviso);
// - C-02.b-flat §f.3, §f.7 con le decisioni B11: `median_ratio` sostituisce le ipotesi del flat di `.a` con
//   `scale_fixed`, `same_illumination_shape`, `additive_common`; `level_proportional`, `additive_bounded` per i metodi
//   diversi da `mean`; `symmetric_noise`, `gaussian_reference_heteroscedastic` per mediana, winsorized e `sigma_clip`;
//   `same_illumination_color` per il CFA;
// - C-02.a, «Aggiunte di R2» (campi opzionali): ogni frame senza `roi_x`, `roi_y`, `readout_mode`, `row_order` è elencato,
//   `not_verified:<campo>:absent_in=<nome canonico del frame>`; i frame della fixture non li hanno.
std::set<std::string> expected_assumptions(master_role role, bool normalized, kind k, bool cfa, std::size_t n) {
  std::set<std::string> h = {"frames_independent", "outliers_sparse"};
  if (role != master_role::flat) {
    h.insert("stationary_mean");
  } else if (!normalized) {
    h.insert({"illumination_stable", "same_illumination_shape", "level_proportional", "additive_nonnegative",
              "nonnegative_signal"});
  } else {
    h.insert({"scale_fixed", "same_illumination_shape", "additive_common"});
    if (k != kind::mean) h.insert({"level_proportional", "additive_bounded"});
    if (robust(k) || k == kind::sigma_clip) h.insert({"symmetric_noise", "gaussian_reference_heteroscedastic"});
    if (cfa) h.insert("same_illumination_color");
  }
  if (robust(k)) h.insert({"iid_gaussian", "continuous_samples"});
  for (std::size_t f = 0; f < n; ++f) {
    char name[32];
    std::snprintf(name, sizeof name, "frame_%03zu", f);
    for (const char* field : {"roi_x", "roi_y", "readout_mode", "row_order"}) {
      h.insert(std::string("not_verified:") + field + ":absent_in=" + name);
    }
  }
  return h;
}

// Insieme esatto e nessun nome ripetuto.
void expect_assumptions(const json& report, const std::set<std::string>& want) {
  EXPECT_EQ(strings_at(report, {"assumptions_not_verified"}), want);
  EXPECT_EQ(items_at(report, {"assumptions_not_verified"}).size(), want.size()) << "nomi ripetuti o non testuali";
}

TEST(MasterB11, T_M3B_C02_11_assumptions_are_exactly_those_of_section_b3_for_every_method_mono_and_cfa) {
  for (const bool cfa : {false, true}) {
    for (const kind k : kAll) {
      SCOPED_TRACE(std::string(name_of(k)) + (cfa ? " cfa" : " mono"));
      const std::size_t n = k == kind::single ? 1 : 4;
      const dyadic_case c = dyadic_dark(n, cfa, false);
      const master_recipe r = dark_recipe(c.g, k);
      const std::optional<master_result> res = combine_or_fail(r, images_of(c.g), name_of(k));
      if (!res) continue;
      expect_assumptions(res->report, expected_assumptions(master_role::dark, false, k, cfa, n));
    }
  }
}

TEST(MasterB11, T_M3B_C02_11_variance_block_names_the_plane_and_the_estimator_of_the_method) {
  for (const kind k : kAll) {
    SCOPED_TRACE(name_of(k));
    const dyadic_case c = dyadic_dark(k == kind::single ? 1 : 4, false, false);
    const master_recipe r = dark_recipe(c.g, k);
    const std::optional<master_result> res = combine_or_fail(r, images_of(c.g), name_of(k));
    if (!res) continue;
    EXPECT_EQ(str_at(res->report, {"variance", "plane"}), std::optional<std::string>(plane_of(k)));
    EXPECT_EQ(str_at(res->report, {"variance", "variance_estimator"}), std::optional<std::string>(estimator_of(k)));
    EXPECT_EQ(str_at(res->report, {"descriptor", "variance_estimator"}), std::optional<std::string>(estimator_of(k)));
    EXPECT_TRUE(null_at(res->report, {"analysis"})) << "nodo senza parametro `analysis`: sezione null";
  }
}

// ================================================================ costanti nella provenance del job (§b.10, §f.10)

struct job_result {
  json report;
  std::optional<json> details;
};

std::optional<job_result> run_small_job(const fs::path& root, const group& g, master_recipe r) {
  fs::create_directories(root);
  write_frames(g, r, root, "frame");
  auto store = sqlite_dag_store::create(root / "store");
  const node_spec node = make_master_node(r);
  store->put_node(node);
  store->create_slot("master", node.key());
  memory_pool pool(256 * kMiB);
  const job_outcome out = run_master_quiet(*store, "master", r, pool);
  if (out.exit_code != 0) {
    ADD_FAILURE() << "job fallito: " << (out.stop ? out.stop->code() + " — " + out.stop->what() : "senza stop");
    return std::nullopt;
  }
  const auto roles = by_role(*store, node.key());
  if (!roles.count("report")) {
    ADD_FAILURE() << "report non pubblicato";
    return std::nullopt;
  }
  const std::vector<std::uint8_t> bytes = read_artifact(*store, roles.at("report"));
  job_result jr;
  jr.report = parse_json(std::string(bytes.begin(), bytes.end()));
  jr.details = out.provenance.details;
  return jr;
}

group level_flat(const std::vector<double>& levels, double offset, std::uint32_t w, std::uint32_t h, bool cfa) {
  group g = make_group(master_role::flat, w, h, levels.size());
  for (std::size_t f = 0; f < levels.size(); ++f) {
    g.frames[f].hdr.offset = offset;
    if (cfa) g.frames[f].hdr.bayer = "RGGB";
    for (double& v : g.frames[f].v) v = offset + levels[f];
  }
  return g;
}

// Test di identità: `details.master_constants` della provenance del job è lo stesso oggetto di `constants` del report
// (decisione B11); i valori delle costanti li verificano i test di stato e T-M3B-FLAT-07.
TEST(MasterB11, T_M3B_C02_11_job_details_carry_the_preflight_constants_of_the_report) {
  const fs::path dir = fresh_dir("b11-details");
  struct job_case {
    std::string tag;
    group g;
    master_recipe r;
  };
  std::vector<job_case> cases;
  {
    const dyadic_case c = dyadic_dark(3, false, false);
    cases.push_back({"median", c.g, dark_recipe(c.g, kind::median)});
    cases.push_back({"weighted", c.g, dark_recipe(c.g, kind::weighted)});
  }
  {
    const group g = level_flat({256, 512, 512}, 10, 8, 4, false);
    cases.push_back({"median_ratio", g, ratio_recipe(g, kind::median)});
  }
  for (const job_case& c : cases) {
    SCOPED_TRACE(c.tag);
    const std::optional<job_result> jr = run_small_job(dir / c.tag, c.g, c.r);
    if (!jr) continue;
    const json* cst = find_path(jr->report, {"constants"});
    ASSERT_NE(cst, nullptr) << "constants assente dal report del job";
    ASSERT_TRUE(jr->details.has_value());
    const json* det = jr->details->find("master_constants");
    ASSERT_NE(det, nullptr) << "details.master_constants assente";
    EXPECT_EQ(canonical_json(*det), canonical_json(*cst));
    if (c.tag == "median") {
      EXPECT_EQ(num_at(*det, {"kappa"}).has_value(), true);
    }
    if (c.tag == "weighted") {
      EXPECT_EQ(items_at(*det, {"c_f"}).size(), 3u);
    }
    if (c.tag == "median_ratio") {
      EXPECT_EQ(items_at(*det, {"flat_scales"}).size(), 1u);
    }
  }
}

// ================================================================ T-M3B-FLAT-07: flat con `median_ratio` e con `none`

// Mono 8 × 4, n = 3, offset 10, livelli (256, 512, 512) nell'ordine del gruppo: S = 1/128, m_H = 384,
// r = (3/2, 3/4, 3/4), ρ = 1/2, max m / min m = 2, Var_f(r) = Σ (r − 1)² / (n − 1) = 3/16. A = 32, budget 1/2.
// Valore del controllo A ρ / m_H = 1/24 (un arrotondamento). Pixel di misura 5, 14, 27: frame 0 con + 2t, t = 2, 4, 8.
// Sugli y: (3t, 0, 0) sopra offset + m_H, quindi piano t² (`mean`), 9t²/16 (`weighted_mean`, pesi 2, 3, 3), 3 κ t²
// (mediana, winsorized), 0 (`sigma_clip` κ = 1: rifiuta 3t, tiene due valori uguali); piano nullo sugli altri 29.
// norm_level_bias_bound (§f.6): `mean` A² Var_f(r) / n = 64; `weighted_mean` A² Σ c_f (r_f − Σ a_g r_g)² = 36;
// mediana e winsorized κ A² Var_f(r) = 192 κ; `sigma_clip` A² (max r − min r)² / (4 (clip_min_kept − 1)) = 144.
// Rapporti col piano sui pixel di misura: 64 / t² = 16, 4, 1 (mediana 4, massimo 16) per i primi quattro metodi.
constexpr std::array<std::size_t, 3> kMeasurePixels = {5, 14, 27};
constexpr std::array<double, 3> kT = {2, 4, 8};

group flat07_group(bool cfa = false) {
  group g = level_flat({256, 512, 512}, 10, 8, cfa ? 8 : 4, cfa);
  if (!cfa) {
    for (std::size_t j = 0; j < kMeasurePixels.size(); ++j) g.frames[0].v[kMeasurePixels[j]] += 2 * kT[j];
  }
  return g;
}

long double bound_of(kind k) {
  const long double k3 = 1.0L - std::sqrt(3.0L) / kPi;
  switch (k) {
    case kind::mean: return 64;
    case kind::weighted: return 36;
    case kind::median:
    case kind::winsorized: return 192 * k3;
    case kind::sigma_clip: return 144;
    case kind::single: break;
  }
  return 0;
}

TEST(MasterB11, T_M3B_FLAT_07_descriptor_and_measure_for_every_method_with_median_ratio_and_none) {
  const group g = flat07_group();
  for (const kind k : kRatio) {
    SCOPED_TRACE(name_of(k));
    const master_recipe r = ratio_recipe(g, k);
    const master_descriptor d = master_descriptor_from_node(make_master_node(r));
    EXPECT_EQ(d.frame_normalization, std::optional<frame_normalization_mode>(frame_normalization_mode::median_ratio));
    const std::optional<master_result> res = combine_or_fail(r, images_of(g), name_of(k));
    if (!res) continue;
    EXPECT_EQ(str_at(res->report, {"descriptor", "frame_normalization"}), std::optional<std::string>("median_ratio"));
    EXPECT_EQ(str_at(res->report, {"descriptor", "variance_estimator"}), std::optional<std::string>(estimator_of(k)));
    ASSERT_TRUE(res->output.measure.has_value());
    const auto& lim = res->output.measure->limits;
    EXPECT_EQ(std::count(lim.begin(), lim.end(), measure_limit::variance_fixed_weight_approximation), 1);
  }
  for (const kind k : {kind::mean, kind::single}) {
    SCOPED_TRACE(std::string("none ") + name_of(k));
    group gn = k == kind::single ? level_flat({256}, 10, 8, 4, false) : g;
    master_recipe r = recipe_of(gn, 0.9);
    apply(k, r);
    const master_descriptor d = master_descriptor_from_node(make_master_node(r));
    EXPECT_EQ(d.frame_normalization, std::optional<frame_normalization_mode>(frame_normalization_mode::none));
    const std::optional<master_result> res = combine_or_fail(r, images_of(gn), name_of(k));
    if (!res) continue;
    EXPECT_EQ(str_at(res->report, {"descriptor", "frame_normalization"}), std::optional<std::string>("none"));
    ASSERT_TRUE(res->output.measure.has_value());
    const auto& lim = res->output.measure->limits;
    EXPECT_EQ(std::count(lim.begin(), lim.end(), measure_limit::variance_fixed_weight_approximation), 0);
  }
}

TEST(MasterB11, T_M3B_FLAT_07_scale_constants_m_r_m_h_and_rho_are_exact) {
  const group g = flat07_group();
  const std::array<double, 3> m = {256, 512, 512}, rr = {1.5, 0.75, 0.75};
  for (const kind k : kRatio) {
    SCOPED_TRACE(name_of(k));
    const master_recipe r = ratio_recipe(g, k);
    const std::optional<master_result> res = combine_or_fail(r, images_of(g), name_of(k));
    if (!res) continue;
    const std::vector<json>& fs_ = items_at(res->report, {"constants", "flat_scales"});
    ASSERT_EQ(fs_.size(), 1u) << "constants.flat_scales: un elemento per canale";
    const json& c0 = fs_[0];
    EXPECT_EQ(str_at(c0, {"label"}), std::optional<std::string>("all"));
    EXPECT_EQ(num_at(c0, {"m_H"}), std::optional<double>(384.0));
    EXPECT_EQ(num_at(c0, {"rho"}), std::optional<double>(0.5));
    const std::vector<json>& ms = items_at(c0, {"m"});
    const std::vector<json>& rs = items_at(c0, {"r"});
    ASSERT_EQ(ms.size(), 3u);
    ASSERT_EQ(rs.size(), 3u);
    for (std::size_t f = 0; f < 3; ++f) {
      const std::optional<std::size_t> pos = frame_index(res->report, r.frames[f].digest.sha256);
      ASSERT_TRUE(pos.has_value());
      EXPECT_EQ(num_of(&ms[*pos]), std::optional<double>(m[f])) << "m del frame " << f;
      EXPECT_EQ(num_of(&rs[*pos]), std::optional<double>(rr[f])) << "r del frame " << f;
    }
  }
}

TEST(MasterB11, T_M3B_FLAT_07_channel_diagnostics_bound_check_value_and_ratios_against_the_oracle) {
  const group g = flat07_group();
  for (const kind k : kRatio) {
    SCOPED_TRACE(name_of(k));
    const master_recipe r = ratio_recipe(g, k);
    const std::optional<master_result> res = combine_or_fail(r, images_of(g), name_of(k));
    if (!res) continue;
    const std::vector<json>& ch = items_at(res->report, {"channels"});
    ASSERT_EQ(ch.size(), 1u);
    const json* fn = find_path(ch[0], {"flat_normalization"});
    ASSERT_NE(fn, nullptr) << "channels[c].flat_normalization assente";
    ASSERT_EQ(fn->type(), json::kind::object);
    EXPECT_EQ(num_at(*fn, {"level_ratio"}), std::optional<double>(2.0));
    EXPECT_EQ(num_at(*fn, {"additive_bound_adu"}), std::optional<double>(kA));
    if (k == kind::mean) {
      EXPECT_TRUE(null_at(*fn, {"rel_budget"})) << "budget vietato con mean";
    } else {
      EXPECT_EQ(num_at(*fn, {"rel_budget"}), std::optional<double>(kBudget));
    }
    // Decisione B11: con `mean` il controllo non si applica (budget vietato): chiave presente, valore null.
    if (k == kind::mean) {
      EXPECT_TRUE(null_at(*fn, {"check_value"})) << "check_value null con mean";
    } else {
      EXPECT_EQ(num_at(*fn, {"check_value"}), std::optional<double>(1.0 / 24.0));
    }
    const long double rel = robust(k) ? static_cast<long double>(kappa_rel_error_max) + kFewOps : kFewOps;
    EXPECT_TRUE(rel_close(num_at(*fn, {"norm_level_bias_bound"}), bound_of(k), rel, "norm_level_bias_bound"));
    EXPECT_EQ(str_at(*fn, {"bound_ratio_null_reason"}), std::optional<std::string>("variance_zero"));
    if (k == kind::sigma_clip) {
      EXPECT_EQ(num_at(*fn, {"bound_ratio_null"}), std::optional<double>(32.0));
      EXPECT_TRUE(null_at(*fn, {"bound_ratio_median"}));
      EXPECT_TRUE(null_at(*fn, {"bound_ratio_max"}));
    } else {
      EXPECT_EQ(num_at(*fn, {"bound_ratio_null"}), std::optional<double>(29.0));
      EXPECT_TRUE(rel_close(num_at(*fn, {"bound_ratio_median"}), 4, kFewOps, "bound_ratio_median"));
      EXPECT_TRUE(rel_close(num_at(*fn, {"bound_ratio_max"}), 16, kFewOps, "bound_ratio_max"));
    }
    const json* lb = find_path(ch[0], {"level_bias_estimate"});
    ASSERT_NE(lb, nullptr);
    EXPECT_EQ(str_at(*lb, {"null_reason"}), std::optional<std::string>("normalized"));
  }
}

// Con `median_ratio` r = y_f − M sui campioni normalizzati, quelli che il metodo combina (decisione B11). `mean`: sugli y
// (3t, 0, 0) − t, quindi Σ r = 2 · 14 nel frame 0 e −14 negli altri; Σ r² = 4 · 84 e 84; P = 32.
TEST(MasterB11, T_M3B_FLAT_07_residuals_use_the_normalized_samples) {
  const group g = flat07_group();
  const master_recipe r = ratio_recipe(g, kind::mean);
  const std::optional<master_result> res = combine_or_fail(r, images_of(g), "mean");
  ASSERT_TRUE(res.has_value());
  std::vector<std::vector<resid_want>> want = {{{28.0 / 32.0, std::sqrt(336.0 / 32.0), 32}},
                                               {{-14.0 / 32.0, std::sqrt(84.0 / 32.0), 32}},
                                               {{-14.0 / 32.0, std::sqrt(84.0 / 32.0), 32}}};
  expect_residuals(res->report, r, want, {"all"});
}

std::set<std::string> ratio_warnings(kind k) {
  std::set<std::string> w = {"saturation_not_evaluated", "flat_norm_scales_estimated"};
  if (robust(k)) w.insert("gaussian_reference_not_verified");
  if (k == kind::sigma_clip) w.insert("frozen_selection_approximation");
  return w;
}

TEST(MasterB11, T_M3B_FLAT_07_assumptions_and_warnings_are_exact_with_median_ratio) {
  for (const bool cfa : {false, true}) {
    const group g = flat07_group(cfa);
    for (const kind k : kRatio) {
      SCOPED_TRACE(std::string(name_of(k)) + (cfa ? " cfa" : " mono"));
      const master_recipe r = ratio_recipe(g, k);
      const std::optional<master_result> res = combine_or_fail(r, images_of(g), name_of(k));
      if (!res) continue;
      EXPECT_EQ(strings_at(res->report, {"warnings"}), ratio_warnings(k));
      expect_assumptions(res->report, expected_assumptions(master_role::flat, true, k, cfa, 3));
      if (cfa) {
        EXPECT_EQ(items_at(res->report, {"constants", "flat_scales"}).size(), 4u);
      }
    }
  }
}

TEST(MasterB11, T_M3B_FLAT_07_flat_with_none_has_the_exact_assumptions_of_a_mono_and_cfa) {
  for (const bool cfa : {false, true}) {
    for (const kind k : {kind::mean, kind::single}) {
      SCOPED_TRACE(std::string(name_of(k)) + (cfa ? " cfa" : " mono"));
      const std::size_t n = k == kind::single ? 1 : 3;
      const group g = level_flat(k == kind::single ? std::vector<double>{256} : std::vector<double>{256, 512, 512}, 10, 8,
                                 cfa ? 8 : 4, cfa);
      master_recipe r = recipe_of(g, 0.9);
      apply(k, r);
      const std::optional<master_result> res = combine_or_fail(r, images_of(g), name_of(k));
      if (!res) continue;
      expect_assumptions(res->report, expected_assumptions(master_role::flat, false, k, cfa, n));
    }
  }
}

// LOW del giro 2 di B11, chiuso in B15: ogni metodo di `kAll` sul flat con `none`. C-02.b §b.2 e C-02.b-flat §f.2: con
// `none` il flat ammette solo `single` e `mean`, con le ipotesi esatte di `.a`; ogni altro metodo si ferma con
// unsupported `master.variance_domain`, quindi non ha un report di cui confrontare le ipotesi.
TEST(MasterB11, T_M3B_FLAT_07_flat_with_none_has_the_exact_assumptions_for_every_admitted_method_and_stops_for_the_others) {
  for (const bool cfa : {false, true}) {
    for (const kind k : kAll) {
      SCOPED_TRACE(std::string(name_of(k)) + (cfa ? " cfa" : " mono"));
      const bool admitted = k == kind::single || k == kind::mean;
      const std::size_t n = k == kind::single ? 1 : 3;
      const group g = level_flat(k == kind::single ? std::vector<double>{256} : std::vector<double>{256, 512, 512}, 10, 8,
                                 cfa ? 8 : 4, cfa);
      master_recipe r = recipe_of(g, 0.9);
      apply(k, r);
      if (!admitted) {
        EXPECT_STOP(combine_master(r, images_of(g)), error_category::unsupported, "master.variance_domain");
        continue;
      }
      const std::optional<master_result> res = combine_or_fail(r, images_of(g), name_of(k));
      if (!res) continue;
      expect_assumptions(res->report, expected_assumptions(master_role::flat, false, k, cfa, n));
    }
  }
}

TEST(MasterB11, T_M3B_FLAT_07_flat_with_none_has_no_scale_constants_nor_normalization_warning) {
  for (const kind k : {kind::mean, kind::single}) {
    SCOPED_TRACE(name_of(k));
    const group g = k == kind::single ? level_flat({256}, 10, 8, 4, false) : flat07_group();
    master_recipe r = recipe_of(g, 0.9);
    apply(k, r);
    const std::optional<master_result> res = combine_or_fail(r, images_of(g), name_of(k));
    if (!res) continue;
    std::set<std::string> want = {"saturation_not_evaluated"};
    if (k == kind::single) want.insert("noise_model_variance");
    EXPECT_EQ(strings_at(res->report, {"warnings"}), want);
    EXPECT_TRUE(null_at(res->report, {"constants", "flat_scales"}));
    const std::vector<json>& ch = items_at(res->report, {"channels"});
    ASSERT_EQ(ch.size(), 1u);
    EXPECT_TRUE(null_at(ch[0], {"flat_normalization"})) << "channels[c].flat_normalization null con none";
    EXPECT_FALSE(strings_at(res->report, {"assumptions_not_verified"}).count("scale_fixed"));
  }
}

// ================================================================ T-M3B-C02-11, parte analisi

std::string declarations() {
  json doc = json::object();
  doc.set("format", json::string("astro.input_declarations.v1"));
  doc.set("declared_image_fields", json::object());
  return canonical_json(doc);
}

const std::string kExcludedSha(64, 'f');

struct analysis_case {
  group g;
  std::vector<image> images;     // indice del gruppo
  master_proposal proposal;
  std::vector<std::string> used;  // digest dei tre frame usati, ordine canonico
};

// Tre dark 8 × 4 dal writer della fixture (osservazione dalla decodifica) più un candidato escluso per `layout` con
// digest "ff…f", l'ultimo dell'ordine canonico. Proposta attesa: un gruppo, `median` (n = 3, §b.4).
analysis_case analysis_dark() {
  analysis_case c;
  c.g = make_group(master_role::dark, 8, 4, 3);
  for (std::size_t f = 0; f < 3; ++f) {
    for (std::size_t i = 0; i < c.g.frames[f].v.size(); ++i) c.g.frames[f].v[i] = 1000.0 + static_cast<double>(f + i % 3);
  }
  c.images = images_of(c.g);
  master_proposal_request rq;
  rq.role = master_role::dark;
  for (std::size_t f = 0; f < 3; ++f) {
    master_proposal_frame pf;
    pf.analysis.input_id = 10 + f;
    pf.analysis.digest = digest_of(file_of(c.g.frames[f], c.g.w, c.g.h));
    pf.analysis.observation = make_observation(c.images[f], declarations());
    pf.spec.path = "inesistente/frame" + std::to_string(f) + ".fits";
    pf.spec.digest = pf.analysis.digest;
    rq.frames.push_back(pf);
    c.used.push_back(pf.analysis.digest.sha256);
  }
  {
    image rgb = c.images[0];
    rgb.layout = plane_layout::rgb;
    master_proposal_frame pf;
    pf.analysis.input_id = 99;
    pf.analysis.digest = content_digest{kExcludedSha, 2880};
    pf.analysis.observation = make_observation(rgb, declarations());
    pf.spec.path = "inesistente/rgb.fits";
    pf.spec.digest = pf.analysis.digest;
    rq.frames.push_back(pf);
  }
  std::sort(c.used.begin(), c.used.end());
  rq.params.exposure_rel_tol = 1e-9;
  rq.params.temperature_abs_tol_c = 0.5;
  rq.params.temp_policy = temperature_policy::compare;
  rq.simd = simd_test::requested();
  c.proposal = propose_masters(rq);
  return c;
}

// Immagini nell'ordine dei frame della ricetta (canonico per la ricetta proposta).
std::vector<image> images_for(const analysis_case& c, const master_recipe& r) {
  std::vector<image> out;
  for (const master_frame_spec& s : r.frames) {
    for (std::size_t f = 0; f < 3; ++f) {
      if (digest_of(file_of(c.g.frames[f], c.g.w, c.g.h)).sha256 == s.digest.sha256) out.push_back(c.images[f]);
    }
  }
  return out;
}

TEST(MasterB11, T_M3B_C02_11_analysis_section_is_built_from_the_analysis_parameter_of_the_node) {
  const analysis_case c = analysis_dark();
  ASSERT_EQ(c.proposal.groups.size(), 1u);
  ASSERT_TRUE(c.proposal.groups[0].recipe.has_value()) << "premessa: proposta `median` per n = 3";
  const master_recipe& r = *c.proposal.groups[0].recipe;
  const std::optional<master_result> res = combine_or_fail(r, images_for(c, r), "proposto");
  ASSERT_TRUE(res.has_value());
  const json* a = find_path(res->report, {"analysis"});
  ASSERT_NE(a, nullptr) << "sezione analysis assente";
  ASSERT_EQ(a->type(), json::kind::object);
  EXPECT_EQ(str_at(*a, {"role"}), std::optional<std::string>("dark"));
  const std::vector<json>& cands = items_at(*a, {"candidates"});
  ASSERT_EQ(cands.size(), 4u) << "tutti i candidati del ruolo";
  for (std::size_t i = 0; i < 3; ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(str_at(cands[i], {"digest"}), std::optional<std::string>(c.used[i]));
    EXPECT_EQ(str_at(cands[i], {"status"}), std::optional<std::string>("used"));
    EXPECT_EQ(str_at(cands[i], {"group"}), std::optional<std::string>(c.used[0]));
  }
  EXPECT_EQ(str_at(cands[3], {"digest"}), std::optional<std::string>(kExcludedSha));
  EXPECT_EQ(str_at(cands[3], {"status"}), std::optional<std::string>("excluded"));
  EXPECT_EQ(str_at(cands[3], {"reason"}), std::optional<std::string>("layout"));
  EXPECT_EQ(str_at(cands[3], {"values", "layout"}), std::optional<std::string>("rgb"));
  const json* grp = find_path(*a, {"group"});
  ASSERT_NE(grp, nullptr);
  EXPECT_EQ(str_at(*grp, {"reference"}), std::optional<std::string>(c.used[0]));
  const std::vector<json>& mem = items_at(*grp, {"members"});
  ASSERT_EQ(mem.size(), 3u);
  for (std::size_t i = 0; i < 3; ++i) EXPECT_EQ(str_of(&mem[i]), std::optional<std::string>(c.used[i]));
  EXPECT_EQ(str_at(*grp, {"status"}), std::optional<std::string>("ok"));
  EXPECT_EQ(str_at(*grp, {"proposal", "combine"}), std::optional<std::string>("median"));
  EXPECT_EQ(str_at(*grp, {"proposal", "rejection"}), std::optional<std::string>("none"));
  EXPECT_TRUE(null_at(*grp, {"proposal_absent_reason"}));
  EXPECT_TRUE(items_at(*grp, {"alternatives"}).empty());
  EXPECT_EQ(str_at(*a, {"chosen", "combine"}), std::optional<std::string>("median"));
  EXPECT_EQ(str_at(*a, {"chosen", "rejection"}), std::optional<std::string>("none"));
  const json* followed = find_path(*a, {"proposal_followed"});
  ASSERT_NE(followed, nullptr);
  ASSERT_EQ(followed->type(), json::kind::boolean);
  EXPECT_TRUE(followed->as_bool());
  EXPECT_FALSE(strings_at(res->report, {"warnings"}).count("proposal_not_followed"));
}

// Chiave esatta del gruppo (C-02.b §b.2 punto 2) nel parametro `analysis` prodotto da `propose_masters` e nella sezione
// del report: valori dall'header della fixture (8 × 4, binning 1, SYN-CAM, GAIN 100, OFFSET 10); filtro solo per il flat;
// campi opzionali assenti null; nessuno stato del decoder.
TEST(MasterB11, T_M3B_C02_11_group_key_is_in_the_analysis_parameter_and_in_the_report) {
  const analysis_case c = analysis_dark();
  ASSERT_EQ(c.proposal.groups.size(), 1u);
  ASSERT_TRUE(c.proposal.groups[0].recipe.has_value());
  const master_recipe& r = *c.proposal.groups[0].recipe;
  const std::optional<master_result> res = combine_or_fail(r, images_for(c, r), "proposto");
  ASSERT_TRUE(res.has_value());
  const json* from_param = find_path(c.proposal.groups[0].analysis, {"group", "key"});
  ASSERT_NE(from_param, nullptr) << "analysis.group.key assente nel parametro del nodo";
  const json* key = find_path(res->report, {"analysis", "group", "key"});
  ASSERT_NE(key, nullptr) << "analysis.group.key assente nel report";
  EXPECT_EQ(canonical_json(*key), canonical_json(*from_param));
  for (const char* k : {"width", "height", "binning_x", "binning_y", "cfa", "camera", "gain", "filter", "roi_x", "roi_y",
                        "readout_mode", "row_order", "decoder_state", "offset_adu"}) {
    EXPECT_NE(key->find(k), nullptr) << "campo della chiave assente: " << k;
  }
  EXPECT_EQ(num_at(*key, {"width"}), std::optional<double>(8));
  EXPECT_EQ(num_at(*key, {"height"}), std::optional<double>(4));
  EXPECT_EQ(num_at(*key, {"binning_x"}), std::optional<double>(1));
  EXPECT_EQ(num_at(*key, {"binning_y"}), std::optional<double>(1));
  EXPECT_EQ(str_at(*key, {"camera"}), std::optional<std::string>("SYN-CAM"));
  EXPECT_EQ(num_at(*key, {"gain"}), std::optional<double>(100));
  EXPECT_EQ(num_at(*key, {"offset_adu"}), std::optional<double>(10));
  EXPECT_EQ(str_at(*key, {"cfa"}), std::optional<std::string>("none")) << "mono: `cfa` come nel report dei campi (cfa_text)";
  EXPECT_TRUE(null_at(*key, {"filter"})) << "filtro solo nella chiave del flat";
  for (const char* k : {"roi_x", "roi_y", "readout_mode", "row_order"}) EXPECT_TRUE(null_at(*key, {k})) << k;
  const json* dec = key->find("decoder_state");
  ASSERT_NE(dec, nullptr);
  ASSERT_EQ(dec->type(), json::kind::boolean);
  EXPECT_FALSE(dec->as_bool());
}

TEST(MasterB11, T_M3B_C02_11_method_different_from_the_proposal_gives_proposal_not_followed) {
  const analysis_case c = analysis_dark();
  ASSERT_EQ(c.proposal.groups.size(), 1u);
  ASSERT_TRUE(c.proposal.groups[0].recipe.has_value());
  master_recipe r = *c.proposal.groups[0].recipe;
  r.combine = combine_method::mean;  // scelta dell'utente diversa dalla proposta `median`
  const std::optional<master_result> res = combine_or_fail(r, images_for(c, r), "mean scelto");
  ASSERT_TRUE(res.has_value());
  EXPECT_EQ(str_at(res->report, {"analysis", "chosen", "combine"}), std::optional<std::string>("mean"));
  EXPECT_EQ(str_at(res->report, {"analysis", "group", "proposal", "combine"}), std::optional<std::string>("median"));
  const json* followed = find_path(res->report, {"analysis", "proposal_followed"});
  ASSERT_NE(followed, nullptr);
  ASSERT_EQ(followed->type(), json::kind::boolean);
  EXPECT_FALSE(followed->as_bool());
  EXPECT_TRUE(strings_at(res->report, {"warnings"}).count("proposal_not_followed"));
  const std::set<std::string> in_result(res->warnings.begin(), res->warnings.end());
  EXPECT_TRUE(in_result.count("proposal_not_followed"));
}

// Flat, tre frame (livelli 256, 300, 320 sopra offset 10, entro `flat_level_rel_tol` 0.25): proposta `mean` con `none`.
// La ricetta scelta cambia solo la normalizzazione (`median_ratio`, A = 32, budget vietato con `mean`): stesso metodo,
// normalizzazione diversa dalla proposta → `proposal_not_followed` (decisione B11). Controllo: la ricetta proposta
// invariata segue la proposta. `chosen` porta anche `flat_normalization` (decisione B11 su `proposal_not_followed`).
TEST(MasterB11, T_M3B_C02_11_only_the_normalization_different_from_the_proposal_gives_proposal_not_followed) {
  const group g = level_flat({256, 300, 320}, 10, 8, 4, false);
  const std::vector<image> images = images_of(g);
  master_proposal_request rq;
  rq.role = master_role::flat;
  for (std::size_t f = 0; f < 3; ++f) {
    master_proposal_frame pf;
    pf.analysis.input_id = 20 + f;
    pf.analysis.digest = digest_of(file_of(g.frames[f], g.w, g.h));
    pf.analysis.observation = make_observation(images[f], declarations());
    master_frame_stats st;
    st.pixel_count = static_cast<std::uint64_t>(g.w) * g.h;
    st.levels = {std::optional<double>(std::array<double, 3>{256, 300, 320}[f])};
    pf.analysis.stats = st;
    pf.spec.path = "inesistente/flat" + std::to_string(f) + ".fits";
    pf.spec.digest = pf.analysis.digest;
    rq.frames.push_back(pf);
  }
  rq.params.exposure_rel_tol = 1e-9;
  rq.params.temperature_abs_tol_c = 0.5;
  rq.params.temp_policy = temperature_policy::compare;
  rq.params.flat_min_signal_adu = 1.0;
  rq.params.flat_level_rel_tol = 0.25;
  rq.simd = simd_test::requested();
  const master_proposal p = propose_masters(rq);
  ASSERT_EQ(p.groups.size(), 1u);
  ASSERT_TRUE(p.groups[0].recipe.has_value()) << "premessa: proposta `mean` per tre flat entro tolleranza";
  const master_recipe& proposed = *p.groups[0].recipe;
  ASSERT_EQ(proposed.combine, std::optional<combine_method>(combine_method::mean));
  const auto images_in_order = [&](const master_recipe& r) {
    std::vector<image> out;
    for (const master_frame_spec& s : r.frames) {
      for (std::size_t f = 0; f < 3; ++f) {
        if (rq.frames[f].spec.digest.sha256 == s.digest.sha256) out.push_back(images[f]);
      }
    }
    return out;
  };
  {
    SCOPED_TRACE("controllo: ricetta proposta");
    const std::optional<master_result> res = combine_or_fail(proposed, images_in_order(proposed), "proposta");
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(str_at(res->report, {"analysis", "chosen", "flat_normalization"}), std::optional<std::string>("none"));
    const json* followed = find_path(res->report, {"analysis", "proposal_followed"});
    ASSERT_NE(followed, nullptr);
    ASSERT_EQ(followed->type(), json::kind::boolean);
    EXPECT_TRUE(followed->as_bool());
    EXPECT_FALSE(strings_at(res->report, {"warnings"}).count("proposal_not_followed"));
  }
  master_recipe r = proposed;
  r.flat_norm = flat_normalization::median_ratio;
  r.flat_level_rel_tol.reset();
  set_flat_additive_bound(r, kA);
  const std::optional<master_result> res = combine_or_fail(r, images_in_order(r), "median_ratio scelto");
  ASSERT_TRUE(res.has_value());
  EXPECT_EQ(str_at(res->report, {"analysis", "chosen", "combine"}), std::optional<std::string>("mean"));
  EXPECT_EQ(str_at(res->report, {"analysis", "chosen", "rejection"}), std::optional<std::string>("none"));
  EXPECT_EQ(str_at(res->report, {"analysis", "chosen", "flat_normalization"}), std::optional<std::string>("median_ratio"));
  const json* followed = find_path(res->report, {"analysis", "proposal_followed"});
  ASSERT_NE(followed, nullptr);
  ASSERT_EQ(followed->type(), json::kind::boolean);
  EXPECT_FALSE(followed->as_bool());
  EXPECT_TRUE(strings_at(res->report, {"warnings"}).count("proposal_not_followed"));
}

// Decisione B11: i membri del gruppo in `analysis` devono essere i frame del master (come insieme di digest). Ricetta con
// due dei tre frame (`mean`, valida per n = 2) e il parametro `analysis` del gruppo di tre: invalid_input `master.param.analysis`, nel contesto
// `parametro` = "analysis" e `motivo` (forma di `bad_param`), già in `validate_master_recipe` (nessun
// accesso ai file). Controllo: gli stessi tre frame in ordine diverso nella ricetta non fermano.
TEST(MasterB11, T_M3B_C02_11_analysis_members_different_from_the_master_frames_stop_with_param_analysis) {
  const analysis_case c = analysis_dark();
  ASSERT_EQ(c.proposal.groups.size(), 1u);
  ASSERT_TRUE(c.proposal.groups[0].recipe.has_value());
  const master_recipe& proposed = *c.proposal.groups[0].recipe;
  ASSERT_EQ(proposed.frames.size(), 3u);
  {
    SCOPED_TRACE("controllo: stessi frame, ordine inverso");
    master_recipe r = proposed;
    std::reverse(r.frames.begin(), r.frames.end());
    EXPECT_NO_THROW(validate_master_recipe(r));
    EXPECT_TRUE(combine_or_fail(r, images_for(c, r), "ordine inverso").has_value());
  }
  master_recipe r = proposed;
  r.frames.pop_back();
  r.combine = combine_method::mean;  // n = 2: `median` fermerebbe prima con master.variance_domain
  EXPECT_STOP(validate_master_recipe(r), error_category::invalid_input, "master.param.analysis");
  EXPECT_STOP(combine_master(r, images_for(c, r)), error_category::invalid_input, "master.param.analysis");
  try {
    validate_master_recipe(r);
  } catch (const stop_error& e) {
    std::map<std::string, std::string> ctx;
    for (const auto& [k, v] : e.context()) ctx[k] = v;
    EXPECT_EQ(ctx["parametro"], "analysis");
    EXPECT_TRUE(ctx.count("motivo")) << "motivo nel contesto";
  }
}

TEST(MasterB11, T_M3B_C02_11_node_without_analysis_has_a_null_section_and_no_proposal_warning) {
  const analysis_case c = analysis_dark();
  ASSERT_TRUE(c.proposal.groups[0].recipe.has_value());
  master_recipe r = *c.proposal.groups[0].recipe;
  r.analysis = json::null();
  r.combine = combine_method::mean;
  const std::optional<master_result> res = combine_or_fail(r, images_for(c, r), "senza analisi");
  ASSERT_TRUE(res.has_value());
  EXPECT_TRUE(null_at(res->report, {"analysis"}));
  EXPECT_FALSE(strings_at(res->report, {"warnings"}).count("proposal_not_followed"));
}

// ================================================================ T-M3B-C02-10 e T-M3B-FLAT-08, parte report

struct run_cfg {
  unsigned workers;
  std::uint32_t rows;
};
constexpr std::array<run_cfg, 6> kConfigs = {run_cfg{1, 1}, run_cfg{4, 1}, run_cfg{1, 7},
                                             run_cfg{4, 7}, run_cfg{1, 64}, run_cfg{4, 64}};

void poke_nonfinite(group& g) {
  const std::array<std::size_t, 7> rows = {0, 6, 7, 8, 63, 64, 70};
  for (std::size_t k = 0; k < rows.size(); ++k) {
    g.frames[k % g.frames.size()].v[rows[k] * g.w + (3 + 5 * k) % g.w] = kNaN;
  }
}

// Oggetto senza il membro `key` (il report del job aggiunge `outputs`, i digest degli artefatti, che la memoria non ha).
json without(const json& o, const std::string& key) {
  json out = json::object();
  for (std::size_t i = 0; i < o.keys().size(); ++i) {
    if (o.keys()[i] != key) out.set(o.keys()[i], o.items()[i]);
  }
  return out;
}

// Report del job byte per byte uguale fra le sei configurazioni e, tolto `outputs`, uguale al JSON canonico del report in
// memoria. Premessa: il report contiene i residui per frame (campo di B11), così il confronto copre le somme per riga.
void expect_report_invariant(const std::string& tag, const group& g, master_recipe r) {
  SCOPED_TRACE(tag);
  const std::optional<master_result> memory = combine_or_fail(r, images_of(g), tag);
  if (!memory) return;
  const std::vector<json>& frames = items_at(memory->report, {"frames"});
  ASSERT_FALSE(frames.empty());
  ASSERT_FALSE(items_at(frames[0], {"residuals"}).empty()) << "premessa: residui per frame nel report (B11)";
  const std::string want = canonical_json(memory->report);
  const fs::path dir = fresh_dir("b11-inv-" + tag);
  write_frames(g, r, dir, "frame");
  std::set<std::string> reports;
  for (const run_cfg& c : kConfigs) {
    SCOPED_TRACE(std::to_string(c.workers) + " worker, tile_rows " + std::to_string(c.rows));
    auto store = sqlite_dag_store::create(dir / ("w" + std::to_string(c.workers) + "-t" + std::to_string(c.rows)));
    const node_spec node = make_master_node(r);
    store->put_node(node);
    store->create_slot("master", node.key());
    memory_pool pool(256 * kMiB);
    master_job_options o;
    o.resources.worker_count = c.workers;
    o.resources.tile_rows = c.rows;
    const job_outcome out = run_master_quiet(*store, "master", r, pool, o);
    ASSERT_EQ(out.exit_code, 0) << (out.stop ? out.stop->code() + " — " + out.stop->what() : "");
    const auto roles = by_role(*store, node.key());
    ASSERT_TRUE(roles.count("report"));
    const std::vector<std::uint8_t> bytes = read_artifact(*store, roles.at("report"));
    const std::string text(bytes.begin(), bytes.end());
    reports.insert(text);
    const json parsed = parse_json(text);
    ASSERT_EQ(parsed.type(), json::kind::object);
    EXPECT_EQ(canonical_json(without(parsed, "outputs")), want);
  }
  EXPECT_EQ(reports.size(), 1u) << "report del job diverso fra le configurazioni";
}

TEST(MasterB11, T_M3B_C02_10_report_is_byte_identical_across_tile_rows_workers_blocks_and_memory) {
  const sc::gaussian_cases cases = sc::c02_10_cases();
  const std::vector<sc::frames> x = sc::realize_cases(cases);
  const std::size_t ci = cases.sets.size() - 1;  // n = 11
  for (const kind k : kAll) {
    for (const bool cfa : {false, true}) {
      const std::size_t n = k == kind::single ? 1 : cases.n[ci];
      group g = make_group(master_role::dark, cases.sets[ci].width, cases.sets[ci].height, n);
      for (std::size_t f = 0; f < n; ++f) {
        g.frames[f].v = x[ci][f];
        if (cfa) g.frames[f].hdr.bayer = "RGGB";
      }
      poke_nonfinite(g);
      expect_report_invariant(std::string("dark-") + name_of(k) + (cfa ? "-cfa" : "-mono"), g, dark_recipe(g, k));
    }
  }
}

TEST(MasterB11, T_M3B_FLAT_08_report_is_byte_identical_across_tile_rows_workers_blocks_and_memory) {
  const sc::flat_cases cases = sc::flat_08_cases();
  const std::vector<sc::frames> x = sc::realize_cases(cases);
  for (std::size_t ci = 0; ci < cases.sets.size(); ++ci) {
    if (cases.names[ci].rfind("n=11/", 0) != 0) continue;
    const sc::flat_set& s = cases.sets[ci];
    group g = make_group(master_role::flat, s.set.width, s.set.height, s.frames());
    for (std::size_t f = 0; f < s.frames(); ++f) {
      g.frames[f].hdr.offset = sc::exact_double(s.offset, "offset");
      if (s.cfa) g.frames[f].hdr.bayer = "RGGB";
      g.frames[f].v = x[ci][f];
    }
    poke_nonfinite(g);
    for (const kind k : kRatio) {
      // A = 32 e budget 0.02 come T-M3B-FLAT-08 di B10 (stessa famiglia di flat).
      expect_report_invariant(std::string("flat-") + name_of(k) + (s.cfa ? "-cfa" : "-mono"), g, ratio_recipe(g, k, 32, 0.02));
    }
  }
}

// ================================================================ T-M3B-C02-15: master.report_nonfinite

struct job_probe {
  std::optional<std::string> setup_error;
  std::optional<std::string> code;
  std::optional<error_category> category;
  int faults = 0;
  bool frames_read = false;
  bool primary = false;
  bool report = false;
};

// Progetto col nodo della ricetta; il contatore del fault_injector conta ogni punto di protocollo dello store dopo
// l'allestimento.
job_probe probe_job(const fs::path& root, const group& g, master_recipe r) {
  job_probe out;
  fs::create_directories(root);
  write_frames(g, r, root, "frame");
  auto store = sqlite_dag_store::create(root / "store");
  std::optional<node_spec> node;
  try {
    node = make_master_node(r);
  } catch (const stop_error& e) {
    out.setup_error = e.code() + " — " + e.what();
    return out;
  }
  store->put_node(*node);
  store->create_slot("master", node->key());
  int faults = 0;
  store->set_fault_injector([&faults](std::string_view) {
    ++faults;
    return 0;
  });
  master_job_options o;
  o.hook = [&out](std::string_view point) {
    if (point == "frames.read") out.frames_read = true;
  };
  memory_pool pool(256 * kMiB);
  try {
    const job_outcome res = run_master_quiet(*store, "master", r, pool, o);
    if (res.stop) {
      out.code = res.stop->code();
      out.category = res.stop->category();
    }
  } catch (const stop_error& e) {
    out.code = e.code();
    out.category = e.category();
  }
  out.faults = faults;
  store->set_fault_injector({});
  const auto roles = by_role(*store, node->key());
  out.primary = roles.count("primary") != 0;
  out.report = roles.count("report") != 0;
  return out;
}

// Dark `mean`, n = 2, 8 × 4, uscita f64: x = 3 · 2^511 e 2^511, M = 2^512, r = ±2^511, r² = 2^1022 e piano 2^1022
// finiti; Σ r² per frame su 32 pixel = 2^1027, non rappresentabile: `residual_rms_adu` non finito. Riferimento allo
// stesso punto (statistica del report dopo la combinazione): `level_bias_estimate` in overflow di `.a` (flat 1e200).
// Controllo: gli stessi frame scalati (2^20) pubblicano il master.
TEST(MasterB11, T_M3B_C02_15_unrepresentable_noise_statistic_stops_with_report_nonfinite_and_no_extra_store_work) {
  const fs::path dir = fresh_dir("b11-nonfinite");
  const auto dark_of = [](double hi, double lo) {
    group g = make_group(master_role::dark, 8, 4, 2);
    std::fill(g.frames[0].v.begin(), g.frames[0].v.end(), hi);
    std::fill(g.frames[1].v.begin(), g.frames[1].v.end(), lo);
    return g;
  };
  const group bad = dark_of(3 * std::ldexp(1.0, 511), std::ldexp(1.0, 511));
  const group ok = dark_of(3 * std::ldexp(1.0, 20), std::ldexp(1.0, 20));
  group ref = make_group(master_role::flat, 4, 2, 3);
  for (mframe& f : ref.frames) std::fill(f.v.begin(), f.v.end(), 1e200);

  EXPECT_STOP(combine_master(dark_recipe(bad, kind::mean), images_of(bad)), error_category::numeric, "master.report_nonfinite");

  const job_probe control = probe_job(dir / "control", ok, dark_recipe(ok, kind::mean));
  const job_probe base = probe_job(dir / "reference", ref, recipe_of(ref, 0.01));
  const job_probe stop = probe_job(dir / "stop", bad, dark_recipe(bad, kind::mean));
  ASSERT_FALSE(control.setup_error.has_value()) << *control.setup_error;
  ASSERT_FALSE(base.setup_error.has_value()) << *base.setup_error;
  ASSERT_FALSE(stop.setup_error.has_value()) << *stop.setup_error;
  EXPECT_FALSE(control.code.has_value()) << control.code.value_or("");
  EXPECT_TRUE(control.primary);
  ASSERT_EQ(base.code, std::optional<std::string>("master.report_nonfinite")) << "premessa: stop di riferimento";
  EXPECT_EQ(stop.code, std::optional<std::string>("master.report_nonfinite"));
  EXPECT_EQ(stop.category, std::optional<error_category>(error_category::numeric));
  EXPECT_TRUE(stop.frames_read) << "lo stop è dopo la combinazione";
  EXPECT_FALSE(stop.primary);
  EXPECT_FALSE(stop.report);
  EXPECT_GT(control.faults, base.faults) << "contatore del fault_injector non collegato";
  EXPECT_EQ(stop.faults, base.faults) << "nessun lavoro sullo store oltre lo stop di riferimento";
}

// ================================================================ T-M3B-C02-11, parte analisi: forma e proposta

// Copia dell'oggetto `o` con il membro `key` sostituito da `value` (aggiunto in coda se assente) o tolto (nullopt).
json with_member(const json& o, const std::string& key, const std::optional<json>& value) {
  json out = json::object();
  bool seen = false;
  for (std::size_t i = 0; i < o.keys().size(); ++i) {
    if (o.keys()[i] != key) {
      out.set(o.keys()[i], o.items()[i]);
    } else if (value) {
      out.set(key, *value);
      seen = true;
    }
  }
  if (value && !seen) out.set(key, *value);
  return out;
}

json with_group_member(const json& analysis, const std::string& key, const std::optional<json>& value) {
  return with_member(analysis, "group", with_member(*analysis.find("group"), key, value));
}

// Gruppo con i frame nell'ordine della ricetta: `write_frames` e `probe_job` legano il frame k a `r.frames[k]`.
group in_recipe_order(const analysis_case& c, const master_recipe& r) {
  group g = c.g;
  g.frames.clear();
  for (const master_frame_spec& s : r.frames) {
    for (const mframe& f : c.g.frames) {
      if (digest_of(file_of(f, c.g.w, c.g.h)).sha256 == s.digest.sha256) g.frames.push_back(f);
    }
  }
  return g;
}

// Decisioni B11 e correzioni del giro 1: il nodo ammette ogni oggetto `analysis` (L8); all'esecuzione `make_plan` richiede
// la forma scritta da `propose_masters` (ruolo uguale, `candidates`, `group` con `members`, `proposal` oggetto o null,
// `key`, `reference`, `status`, `proposal_absent_reason`, `alternatives` col loro tipo) e si ferma con invalid_input
// `master.param.analysis` prima di ogni lettura: `combine_master` fermo, job fermo senza `frames.read` e senza artefatti.
TEST(MasterB11, T_M3B_C02_11_malformed_analysis_stops_before_any_read_in_memory_and_in_the_job) {
  const analysis_case c = analysis_dark();
  ASSERT_EQ(c.proposal.groups.size(), 1u);
  ASSERT_TRUE(c.proposal.groups[0].recipe.has_value());
  const master_recipe& proposed = *c.proposal.groups[0].recipe;
  const json& a = proposed.analysis;
  ASSERT_EQ(a.type(), json::kind::object);
  struct bad_case {
    std::string name;
    json analysis;
  };
  std::vector<bad_case> cases = {
      {"role_diverso", with_member(a, "role", json::string("flat"))},
      {"members_assente", with_group_member(a, "members", std::nullopt)},
      {"proposal_assente", with_group_member(a, "proposal", std::nullopt)},
      {"proposal_testo", with_group_member(a, "proposal", json::string("median"))},
      {"key_assente", with_group_member(a, "key", std::nullopt)},
      {"key_testo", with_group_member(a, "key", json::string("chiave"))},
      {"reference_assente", with_group_member(a, "reference", std::nullopt)},
      {"reference_numero", with_group_member(a, "reference", json::uinteger(1))},
      {"status_assente", with_group_member(a, "status", std::nullopt)},
      {"status_numero", with_group_member(a, "status", json::uinteger(0))},
      {"proposal_absent_reason_assente", with_group_member(a, "proposal_absent_reason", std::nullopt)},
      {"proposal_absent_reason_numero", with_group_member(a, "proposal_absent_reason", json::uinteger(0))},
      {"alternatives_assente", with_group_member(a, "alternatives", std::nullopt)},
      {"alternatives_oggetto", with_group_member(a, "alternatives", json::object())},
      // Casi di forma controllati da `make_plan` dal giro 1 di B11 (LOW del giro 2, chiusi in B15).
      {"role_assente", with_member(a, "role", std::nullopt)},
      {"role_numero", with_member(a, "role", json::uinteger(1))},
      {"candidates_assente", with_member(a, "candidates", std::nullopt)},
      {"group_testo", with_member(a, "group", json::string("gruppo"))},
  };
  {
    const json* group = a.find("group");
    ASSERT_NE(group, nullptr);
    const json* proposal = group->find("proposal");
    ASSERT_TRUE(proposal != nullptr && proposal->type() == json::kind::object) << "premessa: proposta `median` presente";
    cases.push_back({"proposal_combine_assente", with_group_member(a, "proposal", with_member(*proposal, "combine", std::nullopt))});
    cases.push_back({"proposal_rejection_assente", with_group_member(a, "proposal", with_member(*proposal, "rejection", std::nullopt))});
  }
  const fs::path dir = fresh_dir("b11-analysis-shape");
  for (const bad_case& bc : cases) {
    SCOPED_TRACE(bc.name);
    master_recipe r = proposed;
    r.analysis = bc.analysis;
    EXPECT_NO_THROW((void)make_master_node(r)) << "il nodo ammette ogni oggetto `analysis` (L8)";
    EXPECT_STOP(combine_master(r, images_for(c, r)), error_category::invalid_input, "master.param.analysis");
    const job_probe jp = probe_job(dir / bc.name, in_recipe_order(c, r), r);
    ASSERT_FALSE(jp.setup_error.has_value()) << *jp.setup_error;
    EXPECT_EQ(jp.code, std::optional<std::string>("master.param.analysis"));
    EXPECT_EQ(jp.category, std::optional<error_category>(error_category::invalid_input));
    EXPECT_FALSE(jp.frames_read) << "stop prima di ogni lettura";
    EXPECT_FALSE(jp.primary);
    EXPECT_FALSE(jp.report);
  }
  {
    // `group.members` presente ma non elenco: `validate_master_recipe` confronta i membri quando il campo esiste
    // (decisione B11), quindi lo stop, con lo stesso codice, arriva già alla creazione del nodo, prima di ogni lettura.
    SCOPED_TRACE("members_oggetto");
    master_recipe r = proposed;
    r.analysis = with_group_member(a, "members", json::object());
    EXPECT_STOP(make_master_node(r), error_category::invalid_input, "master.param.analysis");
    EXPECT_STOP(combine_master(r, images_for(c, r)), error_category::invalid_input, "master.param.analysis");
  }
  {
    SCOPED_TRACE("controllo: analisi della proposta");
    const job_probe jp = probe_job(dir / "controllo", in_recipe_order(c, proposed), proposed);
    ASSERT_FALSE(jp.setup_error.has_value()) << *jp.setup_error;
    EXPECT_FALSE(jp.code.has_value()) << jp.code.value_or("");
    EXPECT_TRUE(jp.frames_read);
    EXPECT_TRUE(jp.primary);
  }
}

bool followed_is(const json& report, bool want) {
  const json* f = find_path(report, {"analysis", "proposal_followed"});
  return f != nullptr && f->type() == json::kind::boolean && f->as_bool() == want;
}

// Gruppo senza proposta (`group.proposal` null): nessun metodo scelto coincide con la proposta, quindi
// `proposal_followed` = false e avviso `proposal_not_followed` (decisione B11).
TEST(MasterB11, T_M3B_C02_11_group_without_proposal_is_not_followed) {
  const analysis_case c = analysis_dark();
  ASSERT_TRUE(c.proposal.groups[0].recipe.has_value());
  master_recipe r = *c.proposal.groups[0].recipe;
  json a = with_group_member(r.analysis, "proposal", json::null());
  r.analysis = with_member(a, "group", with_member(*a.find("group"), "proposal_absent_reason", json::string("noise_model_not_declared")));
  const std::optional<master_result> res = combine_or_fail(r, images_for(c, r), "senza proposta");
  ASSERT_TRUE(res.has_value());
  EXPECT_TRUE(null_at(res->report, {"analysis", "group", "proposal"}));
  EXPECT_TRUE(followed_is(res->report, false)) << "proposal_followed = false";
  EXPECT_TRUE(strings_at(res->report, {"warnings"}).count("proposal_not_followed"));
}

// Proposta `mean`/`none` nel parametro; la ricetta sceglie `mean` + `sigma_clip` (κ = 1, n = 3, non inerte): solo la
// `rejection` è diversa → non seguita, con avviso. Controllo: `mean`/`none` segue la proposta.
TEST(MasterB11, T_M3B_C02_11_only_the_rejection_different_from_the_proposal_gives_proposal_not_followed) {
  const analysis_case c = analysis_dark();
  ASSERT_TRUE(c.proposal.groups[0].recipe.has_value());
  master_recipe base = *c.proposal.groups[0].recipe;
  json proposal = json::object();
  proposal.set("combine", json::string("mean"));
  proposal.set("rejection", json::string("none"));
  base.analysis = with_group_member(base.analysis, "proposal", proposal);
  base.combine = combine_method::mean;
  {
    SCOPED_TRACE("controllo: mean/none");
    const std::optional<master_result> res = combine_or_fail(base, images_for(c, base), "mean");
    ASSERT_TRUE(res.has_value());
    EXPECT_TRUE(followed_is(res->report, true));
    EXPECT_FALSE(strings_at(res->report, {"warnings"}).count("proposal_not_followed"));
  }
  master_recipe r = base;
  r.rejection = master_rejection::sigma_clip;
  r.clip_kappa_low = r.clip_kappa_high = 1.0;
  r.clip_max_iter = 3;
  r.clip_min_kept = 2;
  const std::optional<master_result> res = combine_or_fail(r, images_for(c, r), "mean + sigma_clip");
  ASSERT_TRUE(res.has_value());
  EXPECT_EQ(str_at(res->report, {"analysis", "chosen", "combine"}), std::optional<std::string>("mean"));
  EXPECT_EQ(str_at(res->report, {"analysis", "chosen", "rejection"}), std::optional<std::string>("sigma_clip"));
  EXPECT_TRUE(followed_is(res->report, false)) << "proposal_followed = false";
  EXPECT_TRUE(strings_at(res->report, {"warnings"}).count("proposal_not_followed"));
}

// ================================================================ T-M3B-C02-15: report_nonfinite delle diagnostiche di median_ratio

// Correzioni del giro 1: le diagnostiche della normalizzazione seguono la regola §6.3 di C-02.a (prodotto o quoziente
// di operandi non nulli che dà zero o subnormale, o risultato non finito → numeric `master.report_nonfinite`).
// Flat mono 8 × 4, livelli (256, 512, 512), offset 10, `mean` + `median_ratio` (Var_f(r) = 3/16, n = 3):
// - bound: A = 2^-540, A² Var_f(r) / n = 2^-1084, subnormale in qualunque ordine delle operazioni;
// - rapporto: A = 2^-490, bound = 2^-984 normale; un pixel di misura con +2t nel frame 0, t = 2^20: piano t² = 2^40,
//   rapporto bound / v = 2^-1024 subnormale.
// Controllo: A = 32 pubblica. Riferimento allo stesso punto: `level_bias_estimate` in overflow di `.a`.
TEST(MasterB11, T_M3B_C02_15_subnormal_normalization_diagnostics_stop_with_report_nonfinite_and_no_extra_store_work) {
  const fs::path dir = fresh_dir("b11-norm-nonfinite");
  group spike = level_flat({256, 512, 512}, 10, 8, 4, false);
  spike.frames[0].v[5] += 2 * std::ldexp(1.0, 20);
  const group plain = level_flat({256, 512, 512}, 10, 8, 4, false);
  struct nf_case {
    std::string name;
    group g;
    double a;
  };
  const std::vector<nf_case> cases = {{"bound", plain, std::ldexp(1.0, -540)}, {"ratio", spike, std::ldexp(1.0, -490)}};
  group ref = make_group(master_role::flat, 4, 2, 3);
  for (mframe& f : ref.frames) std::fill(f.v.begin(), f.v.end(), 1e200);
  const job_probe base = probe_job(dir / "reference", ref, recipe_of(ref, 0.01));
  const job_probe control = probe_job(dir / "control", spike, ratio_recipe(spike, kind::mean, kA));
  ASSERT_FALSE(base.setup_error.has_value()) << *base.setup_error;
  ASSERT_FALSE(control.setup_error.has_value()) << *control.setup_error;
  ASSERT_EQ(base.code, std::optional<std::string>("master.report_nonfinite")) << "premessa: stop di riferimento";
  EXPECT_FALSE(control.code.has_value()) << control.code.value_or("");
  EXPECT_TRUE(control.primary);
  EXPECT_GT(control.faults, base.faults) << "contatore del fault_injector non collegato";
  for (const nf_case& nc : cases) {
    SCOPED_TRACE(nc.name);
    const master_recipe r = ratio_recipe(nc.g, kind::mean, nc.a);
    EXPECT_STOP(combine_master(r, images_of(nc.g)), error_category::numeric, "master.report_nonfinite");
    if (nc.name == "ratio") {
      // Statistica per pixel (decisione B11, LOW del giro 2): il contesto porta `x`, `y` del pixel col picco del piano,
      // indice 5 della fixture 8 × 4, cioè x = 5, y = 0.
      std::map<std::string, std::string> ctx;
      try {
        (void)combine_master(r, images_of(nc.g));
      } catch (const stop_error& e) {
        for (const auto& [k, v] : e.context()) ctx[k] = v;
      }
      EXPECT_EQ(ctx["statistica"], "bound_ratio");
      EXPECT_EQ(ctx["x"], "5");
      EXPECT_EQ(ctx["y"], "0");
    }
    const job_probe stop = probe_job(dir / nc.name, nc.g, r);
    ASSERT_FALSE(stop.setup_error.has_value()) << *stop.setup_error;
    EXPECT_EQ(stop.code, std::optional<std::string>("master.report_nonfinite"));
    EXPECT_EQ(stop.category, std::optional<error_category>(error_category::numeric));
    EXPECT_TRUE(stop.frames_read) << "lo stop è dopo la combinazione";
    EXPECT_FALSE(stop.primary);
    EXPECT_FALSE(stop.report);
    EXPECT_EQ(stop.faults, base.faults) << "nessun lavoro sullo store oltre lo stop di riferimento";
  }
}

// LOW del giro 2 di B11, chiuso in B15: `master.report_nonfinite` attraverso `check_value` = (A · ρ) / m_H (C-02.b-flat
// §f.7, regola §6.3 di C-02.a). Flat mono 8 × 4, `median` (n = 3) + `median_ratio`, livelli (2^520, 2^521, 2^521)
// (l'offset 10 sparisce nell'arrotondamento: ulp(2^520) = 2^468), A = 2^-505, budget 1/2:
// - m_H = 3 / (2^-520 + 2 · 2^-521) = 3 · 2^519; r = (3/2, 3/4, 3/4); ρ = 1/2;
// - controlli di §f.2: max m = 2 min m ≤ 4 min m; ρ A = 2^-506 ≤ budget · m_H;
// - bound = κ_med(3) · A² · Var_f(r) con A² = 2^-1010, Var_f(r) = (1/4 + 1/16 + 1/16) / 2 = 3/16: normale (≈ 2^-1013.6);
// - campioni normalizzati uguali (livelli proporzionali, nessun rumore): piano 0, rapporti nulli, non uno stop;
// - check_value: A · ρ = 2^-506 normale, poi / (3 · 2^519) = 2^-1025 / 3, subnormale → stop sul quoziente.
TEST(MasterB11, T_M3B_C02_15_subnormal_check_value_stops_with_report_nonfinite_on_the_check_value) {
  const group g = level_flat({std::ldexp(1.0, 520), std::ldexp(1.0, 521), std::ldexp(1.0, 521)}, 10, 8, 4, false);
  const master_recipe r = ratio_recipe(g, kind::median, std::ldexp(1.0, -505), 0.5);
  std::optional<stop_error> e;
  try {
    (void)combine_master(r, images_of(g));
  } catch (const stop_error& s) {
    e = s;
  }
  ASSERT_TRUE(e.has_value()) << "check_value subnormale: stop atteso";
  EXPECT_EQ(e->category(), error_category::numeric) << e->what();
  EXPECT_EQ(e->code(), "master.report_nonfinite") << e->what();
  std::map<std::string, std::string> ctx;
  for (const auto& [k, v] : e->context()) ctx[k] = v;
  EXPECT_EQ(ctx["statistica"], "check_value") << e->what();
  EXPECT_EQ(ctx["operazione"], "quoziente") << e->what();
  EXPECT_EQ(ctx.count("x"), 0u) << "statistica del canale, non di un pixel";
  const master_recipe control = ratio_recipe(g, kind::median, std::ldexp(1.0, -400), 0.5);
  EXPECT_NO_THROW((void)combine_master(control, images_of(g))) << "controllo: A = 2^-400, check_value = 2^-920 / 3 normale";
}

// `level_ratio` = max m / min m in overflow con `mean` + `median_ratio`, che non
// ha il controllo 3 di §f.2 (max m ≤ 4 min m, solo per i metodi robusti). Flat mono 8 × 4, offset 0, n = 5, livelli
// (2^-523, 2^501 × 4), `flat_min_signal_adu` = 2^-600:
// - Σ 1/m = 2^523 + 4 · 2^-501 = 2^523 in f64, m_H = 5 · 2^-523; r = (5, 5 · 2^-1024 × 4), 5 · 2^-1024 normale;
// - campioni normalizzati r · x = 5 · 2^-523 in ogni frame: piano 0, rapporti nulli; bound di `mean` normale (A = 32);
// - level_ratio = 2^501 / 2^-523 = 2^1024, oltre il massimo f64 → stop sul quoziente (regola §6.3 di C-02.a).
TEST(MasterB11, T_M3B_C02_15_level_ratio_overflow_with_mean_and_median_ratio_stops_with_report_nonfinite) {
  const double lo = std::ldexp(1.0, -523), hi = std::ldexp(1.0, 501);
  const group g = level_flat({lo, hi, hi, hi, hi}, 0, 8, 4, false);
  master_recipe r = ratio_recipe(g, kind::mean);
  r.flat_min_signal_adu = std::ldexp(1.0, -600);
  std::optional<stop_error> e;
  try {
    (void)combine_master(r, images_of(g));
  } catch (const stop_error& s) {
    e = s;
  }
  ASSERT_TRUE(e.has_value()) << "level_ratio in overflow: stop atteso";
  EXPECT_EQ(e->category(), error_category::numeric) << e->what();
  EXPECT_EQ(e->code(), "master.report_nonfinite") << e->what();
  std::map<std::string, std::string> ctx;
  for (const auto& [k, v] : e->context()) ctx[k] = v;
  EXPECT_EQ(ctx["statistica"], "level_ratio") << e->what();
  EXPECT_EQ(ctx["operazione"], "quoziente") << e->what();
  EXPECT_EQ(ctx.count("x"), 0u) << "statistica del canale, non di un pixel";
  const group control = level_flat({std::ldexp(1.0, -520), hi, hi, hi, hi}, 0, 8, 4, false);
  master_recipe rc = ratio_recipe(control, kind::mean);
  rc.flat_min_signal_adu = std::ldexp(1.0, -600);
  EXPECT_NO_THROW((void)combine_master(rc, images_of(control))) << "controllo: level_ratio = 2^1021, finito";
}

}  // namespace
