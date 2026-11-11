// M3.b, unità B9: `median_ratio`, parte 2 — valori e piano (C-02.b-flat §f.1 fatti 1-4, §f.6, §f.7 maschere e
// capability, §f.9, §f.12).
// Test:
// - T-M3B-FLAT-06, parte confini ammessi: rapporto fra i livelli uguale a 4 e `A · ρ_c = budget · m_H,c` con la
//   combinazione eseguita; prodotto `r_f (x_f − offset_adu)` subnormale con operandi normali → `result_nonfinite`;
// - T-M3B-FLAT-01: master e piano per ogni metodo con `median_ratio`, mono e CFA, casi dyadic (tolleranza 0) e non
//   dyadic (regola I);
// - T-M3B-FLAT-02: fattore globale senza rumore, (a) `mean`, β qualunque, rapporto fino a 16; (b) ogni metodo, β ≡ 0,
//   rapporto fino a 4;
// - invalidità e saturazione valutate sugli `x_f` (§f.7);
// - capability `variance.fixed_weight_approximation` per ogni metodo con `median_ratio` (§f.7; parte binaria di
//   T-M3B-FLAT-05);
// - T-M3B-FLAT-03, 04 (criteri S) e 05 (caratterizzazione): suite *Slow, protocollo docs/evidence/M3b-protocollo.md
//   §3.4-3.6, un test ctest per cella o per n; esecuzione seriale: `ctest -L slow`.
// Fuori da questo file: report e provenance delle costanti, ipotesi e avviso `flat_norm_scales_estimated` (B11,
// T-M3B-FLAT-07); determinismo a blocchi (B10, T-M3B-FLAT-08).
//
// Oracle in `b9_oracle.hpp`; nessun valore atteso dal prodotto, salvo κ̂ di `robust_kappa` (ingresso puntuale della
// regola I per `gaussian_reference`, come B5). Ogni frame ha la regione di livello oltre la metà di ogni canale
// (FIX-SYN-CAL-08, §f.9), quindi m_(f,c) è il livello scritto.
//
// Vincolo del report con `median_ratio` (docs/decisions/M3b-lacune.md, «Decisioni successive (B9)»): m̄_c non si
// calcola, quindi `flat_level_rel_dev` (mono), `flat_levels[].rel_dev` (CFA), `flat_level_mean` (mono) e
// `channels[c].flat_level_mean` (CFA) sono JSON null; `flat_level` e `flat_levels[].level` restano m_(f,c);
// `level_bias_estimate` è null con motivo `normalized` e `pixels_nonpositive_signal` resta.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "b9_oracle.hpp"
#include "chi2_exact.hpp"
#include "master_support.hpp"
#include "store_test_support.hpp"
#include "syn_cal.hpp"

namespace {

using namespace mst;
namespace sc = astro_fixture::syn_cal;
using b5o::ivl;
using b5o::ld;
using b9o::method_kind;

constexpr double kAlpha = 1e-3;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kMinNormal = std::numeric_limits<double>::min();

// ---------------------------------------------------------------- metodi e ricetta

struct clip_cfg {
  double kl = 1, kh = 1;
  std::size_t iter = 1, min_kept = 2;
};

struct method {
  method_kind k = method_kind::mean;
  std::size_t g = 1;  // winsorized
  clip_cfg clip;      // sigma_clip
  std::string name() const {
    switch (k) {
      case method_kind::mean: return "mean";
      case method_kind::weighted: return "weighted_mean";
      case method_kind::median: return "median";
      case method_kind::winsorized: return "winsorized g=" + std::to_string(g);
      case method_kind::sigma_clip: return "mean + sigma_clip k=" + std::to_string(clip.kl);
    }
    return "";
  }
};

method mean_m() { return {method_kind::mean, 1, {}}; }
method weighted_m() { return {method_kind::weighted, 1, {}}; }
method median_m() { return {method_kind::median, 1, {}}; }
method winsor_m(std::size_t g) { return {method_kind::winsorized, g, {}}; }
method clip_m(clip_cfg c = {}) { return {method_kind::sigma_clip, 1, c}; }

// I cinque metodi ammessi con `median_ratio` (§f.2); n ≥ 3: pesi uguali (2 max w < W), g = 1, κ = 1 non inerte
// ((n − 1)² > n), una iterazione, almeno 2 tenuti.
std::vector<method> all_methods() { return {mean_m(), weighted_m(), median_m(), winsor_m(1), clip_m()}; }

void apply(const method& m, master_recipe& r) {
  r.combine = combine_method::mean;
  r.rejection = master_rejection::none;
  switch (m.k) {
    case method_kind::mean: break;
    case method_kind::weighted:
      r.combine = combine_method::weighted_mean;
      for (master_frame_spec& f : r.frames) f.weight = 1;
      break;
    case method_kind::median: r.combine = combine_method::median; break;
    case method_kind::winsorized:
      r.combine = combine_method::winsorized;
      r.winsor_g = static_cast<double>(m.g);
      break;
    case method_kind::sigma_clip:
      r.rejection = master_rejection::sigma_clip;
      r.clip_kappa_low = m.clip.kl;
      r.clip_kappa_high = m.clip.kh;
      r.clip_max_iter = static_cast<double>(m.clip.iter);
      r.clip_min_kept = static_cast<double>(m.clip.min_kept);
      break;
  }
}

// Ricetta `median_ratio` (§f.2): A dichiarato, budget solo per i metodi diversi da `mean`, nessuna tolleranza.
master_recipe ratio_of(master_recipe r, const method& m, double additive_bound, double budget) {
  r.flat_norm = flat_normalization::median_ratio;
  r.flat_level_rel_tol.reset();
  apply(m, r);
  set_flat_additive_bound(r, additive_bound);
  set_flat_norm_budget(r, m.k == method_kind::mean ? std::optional<double>{} : std::optional<double>{budget});
  r.output_type = sample_type::f64;
  return r;
}

master_recipe ratio_recipe(const group& g, const method& m, double additive_bound = 0, double budget = 0.5) {
  return ratio_of(recipe_of(g), m, additive_bound, budget);
}

std::vector<std::size_t> canonical(const master_recipe& r) {
  std::vector<std::size_t> order(r.frames.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::sort(order.begin(), order.end(),
            [&](std::size_t a, std::size_t b) { return r.frames[a].digest.sha256 < r.frames[b].digest.sha256; });
  return order;
}

double kappa_hat(const method& m, std::size_t n) {
  if (m.k == method_kind::median) return robust_kappa(kappa_estimator::median, static_cast<std::uint32_t>(n), 0).value;
  if (m.k == method_kind::winsorized) {
    return robust_kappa(kappa_estimator::winsorized, static_cast<std::uint32_t>(n), static_cast<std::uint32_t>(m.g)).value;
  }
  return 0;
}

b9o::method_spec spec_of(const method& m, std::size_t n) {
  b9o::method_spec s;
  s.k = m.k;
  if (m.k == method_kind::weighted) s.w.assign(n, 1.0);
  s.g = m.g;
  s.clip = b6o::params{m.clip.kl, m.clip.kh, std::min(m.clip.iter, n), m.clip.min_kept};
  s.kappa = kappa_hat(m, n);
  return s;
}

std::optional<master_result> combine_or_fail(const master_recipe& r, std::vector<image> frames, const std::string& what) {
  try {
    return combine_master(r, std::move(frames));
  } catch (const stop_error& e) {
    ADD_FAILURE() << what << ": stop " << e.code() << " — " << e.what();
    return std::nullopt;
  }
}

// ---------------------------------------------------------------- frame

std::size_t channel_at(bool cfa, std::uint32_t x, std::uint32_t y) { return cfa ? 2 * (y & 1u) + (x & 1u) : 0; }

// Flat mono: ogni pixel del frame f vale offset + levels[f].
group level_flat(const std::vector<double>& levels, double offset = 10, std::uint32_t w = 4, std::uint32_t h = 4) {
  group g = make_group(master_role::flat, w, h, levels.size());
  for (std::size_t f = 0; f < levels.size(); ++f) {
    g.frames[f].hdr.offset = offset;
    for (double& v : g.frames[f].v) v = offset + levels[f];
  }
  return g;
}

// Gruppo dai frame realizzati di un insieme della fixture (offset nell'header, RGGB se CFA).
group group_from(const sc::flat_set& s, const sc::frames& x) {
  group g = make_group(master_role::flat, s.set.width, s.set.height, s.frames());
  for (std::size_t f = 0; f < s.frames(); ++f) {
    g.frames[f].hdr.offset = sc::exact_double(s.offset, "offset");
    if (s.cfa) g.frames[f].hdr.bayer = "RGGB";
    g.frames[f].v = x[f];
  }
  return g;
}

std::vector<std::vector<double>> levels_of(const sc::flat_set& s) {
  std::vector<std::vector<double>> out;
  for (const auto& lc : s.levels) {
    std::vector<double> v;
    for (const exact::rat& c : lc) v.push_back(sc::exact_double(c, "livello"));
    out.push_back(v);
  }
  return out;
}

// ---------------------------------------------------------------- confronto di un master con l'oracle

// Conteggi per regione (0: regione di livello o pixel senza regione dichiarata; 1: regione di misura della fixture).
struct check_counts {
  std::size_t interval = 0, exact = 0, nonfinite = 0, skipped = 0;
  std::size_t exact_m = 0;  // pixel con M confrontato a tolleranza 0 (anche se V passa dalla regola I)
  std::array<std::size_t, 2> checked_in{0, 0}, skipped_in{0, 0};
  std::vector<bool> dyadic;                // per canale: costanti f64 uguali ai razionali esatti
  std::vector<std::size_t> nonpositive;    // per canale: pixel validi con M − offset ≤ 0, dall'oracle
  std::vector<std::size_t> nonpositive_undecided;  // per canale: segno di M − offset non deciso dall'intervallo
  std::string summary() const {
    return "livello: verificati " + std::to_string(checked_in[0]) + ", saltati " + std::to_string(skipped_in[0]) +
           "; misura: verificati " + std::to_string(checked_in[1]) + ", saltati " + std::to_string(skipped_in[1]);
  }
};

bool inside(const ivl& i, double v) {
  const ld x = v;
  return x >= i.lo && x <= i.hi;
}

// Mezzo ulp f32 di un valore di modulo ≤ a: 2^(e − 24) con 2^e ≤ a < 2^(e+1); 0 per l'uscita f64.
ld half_ulp_f32(ld a, bool f32) {
  if (!f32 || a == 0) return 0;
  return std::ldexp(1.0L, std::ilogb(a) - 24);
}

bool inside_out(const ivl& i, double v, bool f32) {
  const ld h = half_ulp_f32(std::max(std::fabs(i.lo), std::fabs(i.hi)), f32);
  const ld x = v;
  return x >= i.lo - h && x <= i.hi + h;
}

// Ogni pixel non in `skip`: y_f con la regola I (e in razionali esatti se le costanti f64 sono esatte); poi il metodo.
// Tolleranza 0 dove la catena esatta lo prova; altrimenti inclusione nell'intervallo della regola I (che contiene il
// valore esatto: |M̂ − M| ≤ τ). Uscita f32: più mezzo ulp f32. `region` (facoltativa): regione della fixture per pixel.
check_counts expect_flat_master(const group& g, const master_result& res, const master_recipe& r, const method& m,
                                const std::vector<std::vector<double>>& levels, double offset,
                                const std::set<std::size_t>& skip = {}, const std::vector<std::int32_t>* region = nullptr) {
  check_counts cc;
  const bool cfa = levels.size() == 4;
  const bool f32 = res.output.type == sample_type::f32;
  const std::vector<std::size_t> order = canonical(r);
  const std::size_t n = order.size();
  std::vector<b9o::scales_f64> f64;
  std::vector<b9o::scales_ivl> iv;
  std::vector<b9o::scales_exact> ex;
  for (const auto& lc : levels) {
    std::vector<double> canon(n);
    for (std::size_t j = 0; j < n; ++j) canon[j] = lc[order[j]];
    f64.push_back(b9o::f64_scales(canon));
    iv.push_back(b9o::scale_intervals(canon));
    ex.push_back(b9o::exact_scales(canon));
    cc.dyadic.push_back(b9o::scales_are_exact(f64.back(), ex.back()));
    if (iv.back().excluded) ADD_FAILURE() << "regola I non applicabile alle costanti (caso da togliere dalla fixture)";
  }
  cc.nonpositive.assign(levels.size(), 0);
  cc.nonpositive_undecided.assign(levels.size(), 0);
  const exact::rat off = exact::from_double(offset);
  const b9o::method_spec spec = spec_of(m, n);
  std::vector<double> x(n), yh(n);
  std::vector<ivl> yi(n);
  for (std::uint32_t py = 0; py < g.h; ++py) {
    for (std::uint32_t px = 0; px < g.w; ++px) {
      const std::size_t i = std::size_t{py} * g.w + px;
      if (skip.count(i)) continue;
      const std::size_t c = channel_at(cfa, px, py);
      const std::size_t reg = region && (*region)[i] >= 0 ? 1 : 0;
      const std::string where = m.name() + ", pixel " + std::to_string(i);
      bool t_underflow = false, y_exact = cc.dyadic[c], y_all_equal = true;
      std::optional<exact::rat> y_first;
      b5o::rule_i ri;
      for (std::size_t j = 0; j < n; ++j) {
        x[j] = stored(g.frames[order[j]], i);
        const double d = x[j] - offset, t = f64[c].r[j] * d;
        if (d != 0 && (t == 0 || std::fabs(t) < kMinNormal)) t_underflow = true;
        yh[j] = b9o::y_f64(x[j], offset, f64[c].r[j]);
        yi[j] = b9o::y_interval(ri, x[j], offset, iv[c].r[j]);
        const b9o::y_exact ye = b9o::exact_y(x[j], offset, ex[c].r[j]);
        if (ye.underflow || exact::cmp(exact::from_double(yh[j]), ye.y) != 0) y_exact = false;
        if (!y_first) y_first = ye.y;
        else if (exact::cmp(*y_first, ye.y) != 0) y_all_equal = false;
      }
      const std::uint16_t bits = res.output.invalid.bits[i];
      const double M = sample(res.output, i), V = plane(res.output, i);
      if (t_underflow) {
        ++cc.nonfinite;
        ++cc.checked_in[reg];
        EXPECT_EQ(bits, invalid_reason::result_nonfinite) << where << ": r_f (x_f − offset) zero o subnormale";
        EXPECT_TRUE(std::isnan(M) && std::isnan(V)) << where;
        continue;
      }
      std::optional<exact::rat> m_exact;
      if (y_exact) {
        const b9o::pix_exact pe = b9o::method_exact(spec, yh);
        if (pe.determined) {
          if (pe.ex.nonfinite) {
            ++cc.exact;
            ++cc.checked_in[reg];
            EXPECT_EQ(bits, invalid_reason::result_nonfinite) << where;
            continue;
          }
          if (pe.support_incomplete) {
            ++cc.exact;
            ++cc.checked_in[reg];
            EXPECT_EQ(bits, invalid_reason::support_incomplete) << where;
            continue;
          }
          EXPECT_EQ(bits, 0) << where;
          if (pe.ex.exact_m) {
            ++cc.exact_m;
            m_exact = pe.ex.m;
            EXPECT_TRUE(exact::abs(exact::from_double(M) - pe.ex.m) <= half_ulp_out(pe.ex.m, M, f32))
                << where << ": M " << std::setprecision(17) << M << ", esatto " << b5o::approx(pe.ex.m);
          }
          if (pe.ex.exact_v) {
            EXPECT_TRUE(exact::abs(exact::from_double(V) - pe.ex.v) <= half_ulp_out(pe.ex.v, V, f32))
                << where << ": V " << std::setprecision(17) << V << ", esatto " << b5o::approx(pe.ex.v);
          }
          if (pe.ex.exact_m && pe.ex.exact_v) {
            ++cc.exact;
            ++cc.checked_in[reg];
            if (exact::cmp(*m_exact, off) <= 0) ++cc.nonpositive[c];
            continue;
          }
        }
      }
      b9o::pix_ivl pi = b9o::method_interval(spec, yi);
      // `sigma_clip` con gli y_f uguali in aritmetica esatta (regione di livello): la selezione dipende solo dagli
      // arrotondamenti; M e V stanno nel limite su ogni sottoinsieme tenuto, e l'esito può anche essere
      // `support_incomplete`, perché una selezione decisa dagli arrotondamenti può scendere sotto `clip_min_kept`.
      const bool any_subset = m.k == method_kind::sigma_clip && pi.undetermined && y_all_equal;
      if (any_subset) pi = b9o::any_subset_mean_plane(yi, spec.clip.min_kept);
      if (ri.excluded || pi.excluded || pi.undetermined) {
        ++cc.skipped;
        ++cc.skipped_in[reg];
        continue;
      }
      ++cc.interval;
      ++cc.checked_in[reg];
      if (pi.support_incomplete || (any_subset && bits == invalid_reason::support_incomplete)) {
        EXPECT_EQ(bits, invalid_reason::support_incomplete) << where;
        continue;
      }
      EXPECT_EQ(bits, 0) << where;
      EXPECT_TRUE(inside_out(pi.m, M, f32)) << where << ": M " << std::setprecision(17) << M << " fuori da ["
                                            << static_cast<double>(pi.m.lo) << ", " << static_cast<double>(pi.m.hi) << "]";
      EXPECT_TRUE(inside_out(pi.v, V, f32)) << where << ": V " << std::setprecision(17) << V << " fuori da ["
                                            << static_cast<double>(pi.v.lo) << ", " << static_cast<double>(pi.v.hi) << "]";
      // Segno di M − offset (pixels_nonpositive_signal): dalla catena esatta se c'è, altrimenti dall'intervallo.
      if (m_exact) {
        if (exact::cmp(*m_exact, off) <= 0) ++cc.nonpositive[c];
      } else if (pi.m.hi <= static_cast<ld>(offset)) {
        ++cc.nonpositive[c];
      } else if (!(pi.m.lo > static_cast<ld>(offset))) {
        ++cc.nonpositive_undecided[c];
      }
    }
  }
  return cc;
}

// ================================================================ T-M3B-FLAT-06: confini ammessi

// §f.2 controllo 3 con uguaglianza esatta: livelli (1024, 2048, 4096), max m = 4 min m, ammesso per mediana,
// winsorized e `sigma_clip` (e senza controllo per `mean` e `weighted_mean`); A = 0. La combinazione è eseguita: ogni
// pixel valido, master e piano entro la regola I degli y_f = offset + r_f (x_f − offset).
TEST(MasterB9, T_M3B_FLAT_06_level_ratio_equal_to_four_is_combined_by_every_method_within_rule_i) {
  const std::vector<double> lv = {1024, 2048, 4096};
  ASSERT_EQ(lv[2], 4 * lv[0]);
  const group g = level_flat(lv);
  for (const method& m : all_methods()) {
    SCOPED_TRACE(m.name());
    const master_recipe r = ratio_recipe(g, m);
    const std::optional<master_result> res = combine_or_fail(r, images_of(g), m.name());
    if (!res) continue;
    const check_counts cc = expect_flat_master(g, *res, r, m, {lv}, 10);
    EXPECT_EQ(cc.interval + cc.exact, std::size_t{g.w} * g.h) << "pixel non verificati: " << cc.skipped;
    EXPECT_TRUE(operator_output_invariants_hold(res->output));
  }
}

// §f.2 controllo 4 con uguaglianza esatta: livelli (1, 3, 3/2, 3/2), m_H = 3/2 e r = (3/2, 1/2, 1, 1) esatti in f64 in
// ogni ordine canonico (premessa verificata qui), ρ = 1/2; A = 3/2, budget = 1/2: A · ρ = 3/4 = budget · m_H, ammesso.
// y_f = 512 + r_f · m_f = 512 + 3/2 in ogni frame e pixel: master 513.5 e piano 0 esatti per ogni metodo con il budget.
TEST(MasterB9, T_M3B_FLAT_06_budget_equal_to_a_rho_is_combined_with_exact_values) {
  const std::vector<double> lv = {1, 3, 1.5, 1.5};
  const group g = level_flat(lv, 512);
  for (const method& m : {weighted_m(), median_m(), winsor_m(1), clip_m()}) {
    SCOPED_TRACE(m.name());
    const master_recipe r = ratio_recipe(g, m, 1.5, 0.5);
    std::vector<double> canon;
    for (std::size_t c : canonical(r)) canon.push_back(lv[c]);
    const b9o::scales_f64 s = b9o::f64_scales(canon);
    ASSERT_EQ(s.m_h, 1.5) << "premessa: m_H f64 nell'ordine canonico";
    const std::optional<master_result> res = combine_or_fail(r, images_of(g), m.name());
    if (!res) continue;
    for (std::size_t i = 0; i < std::size_t{g.w} * g.h; ++i) {
      EXPECT_EQ(res->output.invalid.bits[i], 0) << i;
      EXPECT_EQ(sample(res->output, i), 513.5) << i;
      EXPECT_EQ(plane(res->output, i), 0.0) << i;
    }
  }
}

// §f.6 «Rappresentabilità»: offset 0, livelli (1, 3, 3/2, 3/2), r = (3/2, 1/2, 1, 1). Pixel 15: frame 1 (r = 1/2) a
// 2⁻¹⁰²² normale, t = 2⁻¹⁰²³ subnormale → `result_nonfinite` per ogni metodo, campione e piano NaN. Pixel 14 di
// controllo: frame 0 (r = 3/2) a 2⁻¹⁰²², t = 1.5 · 2⁻¹⁰²² normale → pixel valido entro l'oracle. Gli altri campioni
// dei due pixel restano al livello (la mediana dei frame non cambia). Pixel 13: ramo «prodotto zero con x ≠ offset»;
// un campione subnormale in ingresso è un valore finito valido (decisioni di B5), quindi frame 1 a 2⁻¹⁰⁷⁴:
// d = 2⁻¹⁰⁷⁴ ≠ 0, t = fl(2⁻¹⁰⁷⁵) = 0 → `result_nonfinite`.
TEST(MasterB9, T_M3B_FLAT_06_subnormal_normalized_product_with_normal_operands_marks_result_nonfinite) {
  const std::vector<double> lv = {1, 3, 1.5, 1.5};
  group g = level_flat(lv, 0.0);
  g.frames[1].v[15] = kMinNormal;
  g.frames[0].v[14] = kMinNormal;
  g.frames[1].v[13] = 0x1p-1074;
  ASSERT_EQ(0.5 * 0x1p-1074, 0.0);
  ASSERT_LT(0.5 * kMinNormal, kMinNormal);
  ASSERT_GE(1.5 * kMinNormal, kMinNormal);
  for (const method& m : all_methods()) {
    SCOPED_TRACE(m.name());
    const master_recipe r = ratio_recipe(g, m);
    const std::optional<master_result> res = combine_or_fail(r, images_of(g), m.name());
    if (!res) continue;
    EXPECT_EQ(res->output.invalid.bits[15], invalid_reason::result_nonfinite);
    EXPECT_TRUE(std::isnan(sample(res->output, 15)) && std::isnan(plane(res->output, 15)));
    EXPECT_EQ(res->output.invalid.bits[13], invalid_reason::result_nonfinite) << "prodotto zero con x ≠ offset";
    EXPECT_TRUE(std::isnan(sample(res->output, 13)) && std::isnan(plane(res->output, 13)));
    EXPECT_EQ(res->output.invalid.bits[14], 0);
    const check_counts cc = expect_flat_master(g, *res, r, m, {lv}, 0.0);
    EXPECT_EQ(cc.nonfinite, 2u);
  }
}

// ================================================================ §f.7: invalidità e saturazione sugli x_f

// Offset 0, livelli (1000, 3000, 1500, 1500): m_H = 1500, r = (3/2, 1/2, 1, 1); saturation_adu = 5000.
// - pixel 13: frame 1 a 6000 ≥ 5000, y = 3000 sotto la soglia → `saturated` (valutata su x);
// - pixel 14: frame 0 a 4000 < 5000, y = 6000 sopra la soglia → valido (y non si confronta con la soglia);
// - pixel 15: frame 2 NaN → `source_nonfinite`.
TEST(MasterB9, T_M3B_FLAT_01_invalidity_and_saturation_are_evaluated_on_x_before_the_normalization) {
  const std::vector<double> lv = {1000, 3000, 1500, 1500};
  group g = level_flat(lv, 0.0);
  g.frames[1].v[13] = 6000;
  g.frames[0].v[14] = 4000;
  g.frames[2].v[15] = kNaN;
  for (const method& m : all_methods()) {
    SCOPED_TRACE(m.name());
    master_recipe r = ratio_recipe(g, m);
    r.saturation_adu = 5000;
    r.saturated_fraction_max = 0.5;
    const std::optional<master_result> res = combine_or_fail(r, images_of(g), m.name());
    if (!res) continue;
    EXPECT_EQ(res->output.invalid.bits[13], invalid_reason::saturated);
    EXPECT_EQ(res->output.invalid.bits[15], invalid_reason::source_nonfinite);
    EXPECT_EQ(res->output.invalid.bits[14], 0) << "y sopra la soglia non è saturazione";
    const check_counts cc = expect_flat_master(g, *res, r, m, {lv}, 0.0, {13, 15});
    EXPECT_EQ(cc.interval + cc.exact, std::size_t{g.w} * g.h - 2) << "pixel non verificati: " << cc.skipped;
  }
}

// ================================================================ capability (§f.7; parte binaria di T-M3B-FLAT-05)

// Con `median_ratio` `measure` è l'unione di quella dei frame con `variance.fixed_weight_approximation`, una volta sola
// per ogni metodo (con `sigma_clip` c'era già). Controllo: `none` + `mean` sugli stessi frame non lo aggiunge.
TEST(MasterB9, T_M3B_FLAT_05_measure_has_fixed_weight_approximation_for_every_median_ratio_method) {
  using ml = measure_limit;
  group g = level_flat({1024, 1100, 1300, 1500}, 10);
  for (std::size_t f = 0; f < 4; ++f) g.frames[f].v[15] += static_cast<double>(f);
  const std::vector<std::vector<ml>> codes = {{ml::calib_offset_unknown, ml::samples_interpolated},
                                              {ml::calib_omitted_bias_subtraction},
                                              {ml::calib_offset_unknown, ml::calib_assumption_decoder_black},
                                              {}};
  const auto frames_with_codes = [&] {
    std::vector<image> frames = images_of(g);
    for (std::size_t f = 0; f < 4; ++f) frames[f].measure = measure_capability{codes[f]};
    return frames;
  };
  std::vector<image> ref = frames_with_codes();
  std::vector<const image*> ptrs;
  for (const image& im : ref) ptrs.push_back(&im);
  for (const method& m : all_methods()) {
    SCOPED_TRACE(m.name());
    const std::optional<master_result> res = combine_or_fail(ratio_recipe(g, m), frames_with_codes(), m.name());
    if (!res) continue;
    ASSERT_TRUE(res->output.measure.has_value());
    EXPECT_EQ(*res->output.measure, merge_measure(ptrs, {ml::variance_fixed_weight_approximation}));
    const auto& lim = res->output.measure->limits;
    EXPECT_EQ(std::count(lim.begin(), lim.end(), ml::variance_fixed_weight_approximation), 1);
  }
  {
    SCOPED_TRACE("controllo: none + mean");
    const std::optional<master_result> res = combine_or_fail(recipe_of(g, 0.5), frames_with_codes(), "none");
    if (res) {
      ASSERT_TRUE(res->output.measure.has_value());
      EXPECT_EQ(*res->output.measure, merge_measure(ptrs, {}));
    }
  }
}

// ================================================================ T-M3B-FLAT-01: valori

// Casi dyadic (§f.12): livelli (1, 3) con n = 2 (`mean`) e (1, 3, 3/2, 3/2) con n = 4 (ogni metodo); CFA con livelli
// del canale moltiplicati per (1, 2, 4, 8) (G1 e G2 diversi), stessi r. Regione di misura con g = (1 + (x + 2y) mod 5)/4
// e β = ((3x + y) mod 9) − 4: x_f, d_f, t_f e y_f esatti. Il test verifica la premessa (costanti f64 uguali ai razionali
// esatti nell'ordine canonico, y_f esatti) e poi la catena di B5/B6 sugli y_f: tolleranza 0 dove ogni operazione è
// esatta, altrimenti regola I. A = 1, budget 1/2: A ρ = 1/2 ≤ budget · m_H = 3/4 nel canale con i livelli minori (A è
// un limite dichiarato, il prodotto non lo confronta con i β della fixture).
sc::flat_set dyadic_flat(const std::vector<double>& base, bool cfa) {
  std::vector<std::vector<exact::rat>> levels;
  for (std::size_t c = 0; c < (cfa ? 4u : 1u); ++c) {
    std::vector<exact::rat> lc;
    for (double b : base) lc.push_back(exact::from_double(b * static_cast<double>(1u << c)));
    levels.push_back(lc);
  }
  const auto measure = [](std::size_t, std::size_t, std::uint32_t x, std::uint32_t y) {
    return std::pair<std::int32_t, sc::flat_pixel>{
        0, {sc::q(1 + (x + 2 * y) % 5, 4), sc::q(static_cast<std::int64_t>((3 * x + y) % 9) - 4)}};
  };
  const std::uint32_t w = 8, h = 8;
  return sc::make_flat(w, h, cfa, sc::q(512), levels, sc::majority_level_pixels(w, h, cfa), std::nullopt, measure);
}

TEST(MasterB9, T_M3B_FLAT_01_dyadic_levels_give_exact_master_and_plane_for_every_method_mono_and_cfa) {
  struct dcase {
    std::vector<double> base;
    std::vector<method> methods;
  };
  const std::vector<dcase> cases = {{{1, 3}, {mean_m()}}, {{1, 3, 1.5, 1.5}, all_methods()}};
  for (const dcase& dc : cases) {
    for (bool cfa : {false, true}) {
      const sc::flat_set s = dyadic_flat(dc.base, cfa);
      sc::gauss_stream gs(1);  // nessun rumore: nessuna estrazione
      const group g = group_from(s, sc::realize(s.set, gs));
      for (const method& m : dc.methods) {
        SCOPED_TRACE("n=" + std::to_string(dc.base.size()) + (cfa ? " cfa " : " mono ") + m.name());
        const master_recipe r = ratio_recipe(g, m, 1, 0.5);
        const std::optional<master_result> res = combine_or_fail(r, images_of(g), m.name());
        if (!res) continue;
        const check_counts cc = expect_flat_master(g, *res, r, m, levels_of(s), 512);
        for (std::size_t ch = 0; ch < cc.dyadic.size(); ++ch)
          ASSERT_TRUE(cc.dyadic[ch]) << "premessa: costanti f64 del canale " << ch << " diverse dai razionali esatti";
        EXPECT_GT(cc.exact_m, 0u) << "nessun pixel con la catena esatta";
        EXPECT_EQ(cc.skipped, 0u);
        EXPECT_TRUE(operator_output_invariants_hold(res->output));
      }
    }
  }
}

// Casi non dyadic (§f.12, protocollo §4, seed 2026110965): `flat_01_cases`, n ∈ {3, 4, 9, 11}, mono e CFA, rumore
// G = 1, R = 5, β ∈ [−32, 32], rapporto dei livelli 4. A = 32, budget 0.02 (controllo 4: A ρ ≤ 34.8 < 0.02 m_H, caso
// peggiore n = 11). Ogni metodo; inclusione nell'intervallo della regola I di master e piano.
TEST(MasterB9, T_M3B_FLAT_01_non_dyadic_cases_within_rule_i_for_every_method_mono_and_cfa) {
  const sc::flat_cases c = sc::flat_01_cases();
  const std::vector<sc::frames> x = sc::realize_cases(c);
  for (std::size_t k = 0; k < c.sets.size(); ++k) {
    const group g = group_from(c.sets[k], x[k]);
    for (const method& m : all_methods()) {
      SCOPED_TRACE(c.names[k] + " " + m.name());
      const master_recipe r = ratio_recipe(g, m, 32, 0.02);
      const std::optional<master_result> res = combine_or_fail(r, images_of(g), m.name());
      if (!res) continue;
      const check_counts cc = expect_flat_master(g, *res, r, m, levels_of(c.sets[k]), 512, {}, &c.sets[k].region);
      std::cout << "T-M3B-FLAT-01 " << c.names[k] << " " << m.name() << ": " << cc.summary() << "\n";
      EXPECT_EQ(cc.skipped_in[1], 0u) << "regione di misura non verificata: " << cc.summary();
      EXPECT_EQ(cc.checked_in[0] + cc.checked_in[1] + cc.skipped_in[0], std::size_t{g.w} * g.h);
      EXPECT_TRUE(operator_output_invariants_hold(res->output));
    }
  }
}

// Uscita f32 (§f.12: regola I più mezzo ulp f32): un caso dyadic (n = 4, mono, ogni metodo) e uno non dyadic
// (`flat_01_cases`, n = 4 mono, ogni metodo).
TEST(MasterB9, T_M3B_FLAT_01_f32_output_within_rule_i_plus_half_ulp_dyadic_and_non_dyadic) {
  {
    const sc::flat_set s = dyadic_flat({1, 3, 1.5, 1.5}, false);
    sc::gauss_stream gs(1);
    const group g = group_from(s, sc::realize(s.set, gs));
    for (const method& m : all_methods()) {
      SCOPED_TRACE("dyadic " + m.name());
      master_recipe r = ratio_recipe(g, m, 1, 0.5);
      r.output_type = sample_type::f32;
      const std::optional<master_result> res = combine_or_fail(r, images_of(g), m.name());
      if (!res) continue;
      ASSERT_EQ(res->output.type, sample_type::f32);
      const check_counts cc = expect_flat_master(g, *res, r, m, levels_of(s), 512);
      EXPECT_EQ(cc.skipped, 0u);
    }
  }
  {
    const sc::flat_cases c = sc::flat_01_cases();
    const std::vector<sc::frames> x = sc::realize_cases(c);
    std::size_t k = 0;
    while (k < c.names.size() && c.names[k] != "n=4/mono") ++k;
    ASSERT_LT(k, c.names.size());
    const group g = group_from(c.sets[k], x[k]);
    for (const method& m : all_methods()) {
      SCOPED_TRACE("non dyadic " + m.name());
      master_recipe r = ratio_recipe(g, m, 32, 0.02);
      r.output_type = sample_type::f32;
      const std::optional<master_result> res = combine_or_fail(r, images_of(g), m.name());
      if (!res) continue;
      ASSERT_EQ(res->output.type, sample_type::f32);
      const check_counts cc = expect_flat_master(g, *res, r, m, levels_of(c.sets[k]), 512, {}, &c.sets[k].region);
      EXPECT_EQ(cc.skipped_in[1], 0u) << cc.summary();
    }
  }
}

// Campi dei livelli nel report con `median_ratio`: vincolo dell'intestazione (decisioni di B9).
TEST(MasterB9, T_M3B_FLAT_01_report_keeps_the_levels_and_nulls_the_level_mean_terms_with_median_ratio) {
  for (bool cfa : {false, true}) {
    SCOPED_TRACE(cfa ? "cfa" : "mono");
    const sc::flat_set s = dyadic_flat({1, 3, 1.5, 1.5}, cfa);
    sc::gauss_stream gs(1);
    const group g = group_from(s, sc::realize(s.set, gs));
    const master_recipe r = ratio_recipe(g, median_m(), 1, 0.5);
    const std::optional<master_result> res = combine_or_fail(r, images_of(g), "median");
    if (!res) continue;
    const std::vector<std::size_t> order = canonical(r);
    const auto& frames = cal::at(res->report, {"frames"}).items();
    ASSERT_EQ(frames.size(), order.size());
    for (std::size_t j = 0; j < order.size(); ++j) {
      if (!cfa) {
        EXPECT_EQ(cal::as_double(cal::at(frames[j], {"flat_level"})), sc::exact_double(s.levels[0][order[j]], "livello"));
        EXPECT_EQ(cal::at(frames[j], {"flat_level_rel_dev"}).type(), json::kind::null);
        continue;
      }
      const auto& lv = cal::at(frames[j], {"flat_levels"}).items();
      ASSERT_EQ(lv.size(), 4u);
      for (std::size_t c = 0; c < 4; ++c) {
        EXPECT_EQ(cal::as_double(cal::at(lv[c], {"level"})), sc::exact_double(s.levels[c][order[j]], "livello"));
        EXPECT_EQ(cal::at(lv[c], {"rel_dev"}).type(), json::kind::null);
      }
    }
    if (!cfa) {
      EXPECT_EQ(cal::at(res->report, {"flat_level_mean"}).type(), json::kind::null);
    } else {
      for (const json& ch : cal::at(res->report, {"channels"}).items())
        EXPECT_EQ(cal::at(ch, {"flat_level_mean"}).type(), json::kind::null);
    }
  }
}

// ================================================================ T-M3B-FLAT-02: fattore globale senza rumore

// Casi del protocollo §4 (seed 2026110966), ν = 0: x_f = 512 + β + c_f g esatti.
// (a) `mean`, β qualunque (|β| ≤ 32), rapporto 16, n ∈ {2, 3, 8}: |M̂ − (b + λ̂ g)| ≤ τ, λ̂ = (1/n) Σ r̂_f c_f con r̂ le
//     costanti f64 della sequenza di §f.6, b = 512 + β, τ l'ampiezza dell'intervallo della regola I di M. A = 32.
// (b) ogni metodo, β ≡ 0, rapporto 4, n ∈ {3, 9, 11}: |M̂ − (512 + c_H g)| ≤ τ, con c_H = m_H esatto. A = 0. Per
//     `sigma_clip` i campioni normalizzati sono uguali in aritmetica esatta e la selezione dipende solo dagli
//     arrotondamenti: τ è l'ampiezza dell'unione delle medie di almeno clip_min_kept valori (b9_oracle).
// Premessa dell'oracle, prima del confronto: il valore di riferimento sta nell'intervallo.
void flat_02_case(const sc::flat_set& s, const sc::frames& x, const method& m, double A, bool part_a,
                  const std::string& name) {
  const group g = group_from(s, x);
  const master_recipe r = ratio_recipe(g, m, A, 0.5);
  const std::vector<std::size_t> order = canonical(r);
  const std::size_t n = order.size();
  const sc::flat_truth truth = sc::truth_of(s);
  const b9o::method_spec spec = spec_of(m, n);
  std::vector<b9o::scales_f64> f64;
  std::vector<b9o::scales_ivl> iv;
  std::vector<exact::rat> lambda_hat;
  for (std::size_t c = 0; c < s.channels(); ++c) {
    std::vector<double> canon(n);
    for (std::size_t j = 0; j < n; ++j) canon[j] = sc::exact_double(s.levels[c][order[j]], "livello");
    f64.push_back(b9o::f64_scales(canon));
    iv.push_back(b9o::scale_intervals(canon));
    ASSERT_FALSE(iv.back().excluded);
    exact::rat sum = exact::from_u(0);
    for (std::size_t j = 0; j < n; ++j) sum = sum + exact::from_double(f64.back().r[j]) * s.levels[c][order[j]];
    lambda_hat.push_back(sum / exact::from_u(n));
  }
  const std::size_t P = s.set.pixels();
  std::vector<ivl> want(P);
  std::vector<exact::rat> target(P);
  std::vector<double> xs(n);
  std::vector<ivl> yi(n);
  for (std::size_t p = 0; p < P; ++p) {
    const std::size_t c = s.channel[p];
    b5o::rule_i ri;
    for (std::size_t j = 0; j < n; ++j) {
      xs[j] = x[order[j]][p];
      yi[j] = b9o::y_interval(ri, xs[j], 512, iv[c].r[j]);
    }
    ASSERT_FALSE(ri.excluded) << name << " pixel " << p;
    bool excluded = false;
    if (m.k == method_kind::sigma_clip) {
      want[p] = b9o::any_subset_mean(yi, m.clip.min_kept, excluded);
    } else {
      const b9o::pix_ivl pi = b9o::method_interval(spec, yi);
      want[p] = pi.m;
      excluded = pi.excluded;
    }
    ASSERT_FALSE(excluded) << name << " pixel " << p;
    target[p] = part_a ? s.offset + s.beta[p] + lambda_hat[c] * s.g[p] : s.offset + truth.m_h[c] * s.g[p];
    const ld t = sc::approx(target[p]);
    ASSERT_TRUE(t >= want[p].lo && t <= want[p].hi) << name << ": premessa dell'oracle, riferimento fuori dall'intervallo, pixel " << p;
  }
  const std::optional<master_result> res = combine_or_fail(r, images_of(g), name);
  if (!res) return;
  for (std::size_t p = 0; p < P; ++p) {
    const double M = sample(res->output, p);
    EXPECT_EQ(res->output.invalid.bits[p], 0) << name << " pixel " << p;
    EXPECT_TRUE(inside(want[p], M)) << name << " pixel " << p << ": M " << std::setprecision(17) << M;
    const exact::rat tau = b5o::from_ld(want[p].hi - want[p].lo);
    EXPECT_TRUE(exact::abs(exact::from_double(M) - target[p]) <= tau)
        << name << " pixel " << p << ": |M − riferimento| = " << b5o::approx(exact::abs(exact::from_double(M) - target[p]))
        << " oltre tau " << b5o::approx(tau);
  }
}

TEST(MasterB9, T_M3B_FLAT_02_a_mean_global_factor_with_any_beta_and_level_ratio_up_to_16) {
  const sc::flat_cases c = sc::flat_02_cases();
  const std::vector<sc::frames> x = sc::realize_cases(c);
  std::size_t run = 0;
  for (std::size_t k = 0; k < c.sets.size(); ++k) {
    if (c.names[k].rfind("a/", 0) != 0) continue;
    SCOPED_TRACE(c.names[k]);
    flat_02_case(c.sets[k], x[k], mean_m(), 32, true, c.names[k]);
    ++run;
  }
  EXPECT_EQ(run, 6u);
}

TEST(MasterB9, T_M3B_FLAT_02_b_every_method_global_factor_with_zero_beta_and_level_ratio_up_to_4) {
  const sc::flat_cases c = sc::flat_02_cases();
  const std::vector<sc::frames> x = sc::realize_cases(c);
  std::size_t run = 0;
  for (std::size_t k = 0; k < c.sets.size(); ++k) {
    if (c.names[k].rfind("b/", 0) != 0) continue;
    for (const method& m : all_methods()) {
      SCOPED_TRACE(c.names[k] + " " + m.name());
      flat_02_case(c.sets[k], x[k], m, 0, false, c.names[k] + " " + m.name());
      ++run;
    }
  }
  EXPECT_EQ(run, 30u);
}

// ================================================================ ordine canonico bit a bit (§f.6, C-02.b §b.8)

// Sequenza di §f.6 in f64 sugli y_f nell'ordine dato: Σ y da sinistra, / n, Σ (y − M)² da sinistra, / (n − 1); `mean`:
// V = s² / n; `median`, `winsorized`: V = κ̂ · s², M dagli y ordinati (valore, indice canonico).
struct f64_mv {
  double m = 0, v = 0;
};

f64_mv f64_sequence(const method& m, const std::vector<double>& y, double kappa) {
  const std::size_t n = y.size();
  double s = y[0];
  for (std::size_t f = 1; f < n; ++f) s += y[f];
  const double mean = s / static_cast<double>(n);
  double ss = 0;
  for (std::size_t f = 0; f < n; ++f) {
    const double d = y[f] - mean;
    ss = f == 0 ? d * d : ss + d * d;
  }
  const double s2 = ss / static_cast<double>(n - 1);
  if (m.k == method_kind::mean) return {mean, s2 / static_cast<double>(n)};
  std::vector<double> z = y;
  std::sort(z.begin(), z.end());  // con i pareggi i valori ordinati non cambiano: per M l'indice canonico non serve
  f64_mv out;
  out.v = kappa * s2;
  if (m.k == method_kind::median) {
    out.m = z[n / 2];
  } else {
    const std::vector<double> v = b5o::winsor_values(z, m.g);
    double t = v[0];
    for (std::size_t i = 1; i < n; ++i) t += v[i];
    out.m = t / static_cast<double>(n);
  }
  return out;
}

// Livelli e campioni non dyadic, n = 5 (rapporto 3.1 ≤ 4), mono 8 × 8, offset 512: regione di livello sui primi 33 pixel,
// poi x_f(p) = 512 + m_f (1/2 + p/64) + (f + 1) sqrt(p)/10. Master e piano bit-identici alla sequenza f64 nell'ordine
// canonico; premessa verificata qui: l'ordine inverso dà un altro valore in almeno un pixel (M per `mean`, V per ogni
// metodo).
TEST(MasterB9, T_M3B_FLAT_01_master_and_plane_accumulate_in_canonical_order_bit_for_bit) {
  const std::vector<double> lv = {1000, 1300, 1700, 2300, 3100};
  const std::size_t n = lv.size();
  group g = make_group(master_role::flat, 8, 8, n);
  for (std::size_t f = 0; f < n; ++f) {
    g.frames[f].hdr.offset = 512;
    for (std::size_t p = 0; p < 64; ++p) {
      g.frames[f].v[p] = p < 33 ? 512 + lv[f]
                                : 512 + lv[f] * (0.5 + static_cast<double>(p) / 64) +
                                      static_cast<double>(f + 1) * std::sqrt(static_cast<double>(p)) / 10;
    }
  }
  for (const method& m : {mean_m(), median_m(), winsor_m(1)}) {
    SCOPED_TRACE(m.name());
    const master_recipe r = ratio_recipe(g, m);
    const std::vector<std::size_t> order = canonical(r);
    std::vector<double> canon;
    for (std::size_t c : order) canon.push_back(lv[c]);
    const b9o::scales_f64 sc64 = b9o::f64_scales(canon);
    const double kappa = kappa_hat(m, n);
    std::vector<f64_mv> want(64);
    std::size_t differs = 0;
    for (std::size_t p = 0; p < 64; ++p) {
      std::vector<double> y(n);
      for (std::size_t j = 0; j < n; ++j) y[j] = b9o::y_f64(stored(g.frames[order[j]], p), 512, sc64.r[j]);
      want[p] = f64_sequence(m, y, kappa);
      const f64_mv rev = f64_sequence(m, std::vector<double>(y.rbegin(), y.rend()), kappa);
      if (rev.v != want[p].v || (m.k == method_kind::mean && rev.m != want[p].m)) ++differs;
    }
    ASSERT_GT(differs, 0u) << "premessa: l'ordine inverso non cambia alcun valore";
    const std::optional<master_result> res = combine_or_fail(r, images_of(g), m.name());
    if (!res) continue;
    for (std::size_t p = 0; p < 64; ++p) {
      EXPECT_EQ(res->output.invalid.bits[p], 0) << p;
      EXPECT_EQ(sample(res->output, p), want[p].m) << "pixel " << p << std::setprecision(17) << ": M";
      EXPECT_EQ(plane(res->output, p), want[p].v) << "pixel " << p << std::setprecision(17) << ": V";
    }
  }
}

// ================================================================ level_bias_estimate con `median_ratio`

// C-02.b-flat §f.13 punto 3, decisioni di B9: per ogni metodo e canale `level_bias_estimate` ha i valori null con
// `null_reason` = `normalized`; `pixels_nonpositive_signal` resta, uguale per canale al conteggio dei pixel validi con
// M − offset ≤ 0 (catena esatta dell'oracle).
TEST(MasterB9, T_M3B_FLAT_01_level_bias_estimate_is_null_with_reason_normalized_and_nonpositive_count_stays) {
  for (bool cfa : {false, true}) {
    const sc::flat_set s = dyadic_flat({1, 3, 1.5, 1.5}, cfa);
    sc::gauss_stream gs(1);
    const group g = group_from(s, sc::realize(s.set, gs));
    for (const method& m : all_methods()) {
      SCOPED_TRACE(std::string(cfa ? "cfa " : "mono ") + m.name());
      const std::optional<master_result> res = combine_or_fail(ratio_recipe(g, m, 1, 0.5), images_of(g), m.name());
      if (!res) continue;
      const auto& channels = cal::at(res->report, {"channels"}).items();
      ASSERT_EQ(channels.size(), cfa ? 4u : 1u);
      // Conteggio atteso per canale: pixel validi con M − offset ≤ 0, dalla catena esatta (casi dyadic).
      const check_counts cc = expect_flat_master(g, *res, ratio_recipe(g, m, 1, 0.5), m, levels_of(s), 512);
      // Con `sigma_clip` la catena esatta non sempre decide la selezione e l'intervallo di M può contenere l'offset
      // (M − offset = 0 nella fixture): per quei canali il conteggio sta fra il deciso e il deciso più gli indecisi.
      std::size_t any = 0;
      for (std::size_t c = 0; c < channels.size(); ++c) {
        const auto got = static_cast<std::size_t>(cal::at(channels[c], {"pixels_nonpositive_signal"}).as_integer());
        if (cc.nonpositive_undecided[c] == 0) {
          EXPECT_EQ(got, cc.nonpositive[c]) << "canale " << c;
        } else {
          ASSERT_EQ(m.k, method_kind::sigma_clip) << "segno indeciso solo con sigma_clip, canale " << c;
          EXPECT_GE(got, cc.nonpositive[c]) << "canale " << c;
          EXPECT_LE(got, cc.nonpositive[c] + cc.nonpositive_undecided[c]) << "canale " << c;
        }
        any += cc.nonpositive[c];
      }
      if (!cfa) {
        EXPECT_GT(any, 0u) << "premessa: la fixture mono ha pixel con M − offset ≤ 0";
      }
      for (const json& ch : channels) {
        const json& lb = cal::at(ch, {"level_bias_estimate"});
        for (const char* k : {"median", "max", "ratio_median", "ratio_max", "level_bias_ratio_null"})
          EXPECT_EQ(cal::at(lb, {k}).type(), json::kind::null) << k;
        EXPECT_EQ(cal::at(lb, {"null_reason"}).as_string(), "normalized");
        EXPECT_EQ(cal::at(ch, {"pixels_nonpositive_signal"}).type(), json::kind::integer);
      }
    }
  }
}

// ================================================================ T-M3B-FLAT-03, 04, 05 (protocollo §3.4-3.6)

double seconds_since(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// Il più grande divisore di K con n · R · P ≤ 4 · 10⁶ campioni per chiamata.
std::size_t batch_of(std::size_t K, std::size_t n, std::size_t P) {
  for (std::size_t r = std::min<std::size_t>(K, std::max<std::size_t>(1, 4000000 / (n * P))); r >= 1; --r)
    if (K % r == 0) return r;
  return 1;
}

// R realizzazioni consecutive del flusso una sotto l'altra: immagine w × hR, regione di livello oltre la metà anche
// sull'immagine intera, quindi m_f = c_f. Il calcolo del master è per pixel (§f.8).
struct flat_batch {
  std::size_t n = 0, R = 0, P = 0;
  group tpl_group;
  std::vector<image> tpl;
  master_recipe base;
  std::vector<std::size_t> order;
  std::vector<std::vector<double>> samples;  // [frame][r · P + p], indice di generazione

  flat_batch(const sc::flat_set& s, std::size_t R_) : n(s.frames()), R(R_), P(s.set.pixels()) {
    tpl_group = make_group(master_role::flat, s.set.width, static_cast<std::uint32_t>(s.set.height * R), n);
    for (std::size_t f = 0; f < n; ++f) {
      tpl_group.frames[f].hdr.offset = sc::exact_double(s.offset, "offset");
      for (std::size_t i = 0; i < tpl_group.frames[f].v.size(); ++i) tpl_group.frames[f].v[i] = s.set.mean[f][i % P];
    }
    tpl = images_of(tpl_group);
    base = recipe_of(tpl_group);
    base.flat_min_signal_adu = 100;
    order = canonical(base);
    samples.assign(n, std::vector<double>(R * P));
  }

  void realize(const sc::flat_set& s, sc::gauss_stream& gs, sc::frames& buf) {
    for (std::size_t r = 0; r < R; ++r) {
      sc::realize_into(s.set, gs, buf);
      for (std::size_t f = 0; f < n; ++f) std::copy(buf[f].begin(), buf[f].end(), samples[f].begin() + static_cast<std::ptrdiff_t>(r * P));
    }
  }

  std::optional<master_result> run(const method& m, double A, double budget) {
    std::vector<image> imgs = tpl;
    for (std::size_t f = 0; f < n; ++f) std::memcpy(imgs[f].data<double>(), samples[f].data(), samples[f].size() * sizeof(double));
    return combine_or_fail(ratio_of(base, m, A, budget), std::move(imgs), m.name());
  }

  void canonical_at(std::size_t i, double* x) const {
    for (std::size_t c = 0; c < n; ++c) x[c] = samples[order[c]][i];
  }
};

b9o::pix_ivl excluded_pixel() {
  b9o::pix_ivl p;
  p.excluded = true;
  return p;
}

// Intervalli degli y_f di un pixel nell'ordine canonico; false se la regola I non si applica.
bool y_intervals(const double* x, std::size_t n, const b9o::scales_ivl& iv, double offset, std::vector<ivl>& y) {
  b5o::rule_i ri;
  y.resize(n);
  for (std::size_t j = 0; j < n; ++j) y[j] = b9o::y_interval(ri, x[j], offset, iv.r[j]);
  return !ri.excluded;
}

b9o::scales_ivl canonical_scale_intervals(const sc::flat_set& s, const std::vector<std::size_t>& order) {
  std::vector<double> canon;
  for (std::size_t c : order) canon.push_back(sc::exact_double(s.levels[0][c], "livello"));
  return b9o::scale_intervals(canon);
}

// ---------------------------------------------------------------- T-M3B-FLAT-03 (protocollo §3.4)

TEST(MasterB9Slow, T_M3B_FLAT_03_mean_median_ratio_plane_values_and_center) {
  const auto t0 = std::chrono::steady_clock::now();
  const sc::flat_set s = sc::flat_03_set();
  const sc::flat_truth truth = sc::truth_of(s);
  const std::size_t n = s.frames(), K = sc::flat_03_K, P = s.set.pixels(), m = 18, classes = 6;
  ASSERT_EQ(exact::cmp(truth.m_h[0], sc::q(1600)), 0) << "premessa del protocollo: m_H = 1600";
  const std::size_t R = batch_of(K, n, P);
  flat_batch fb(s, R);
  const b9o::scales_ivl iv = canonical_scale_intervals(s, fb.order);
  ASSERT_FALSE(iv.excluded);
  // Verità per classe: E, Var(X), Var(M | r), centro b + λ g, dal primo pixel della classe (stessi g e β).
  std::vector<ld> E(classes), varX(classes), varM(classes), center(classes);
  std::vector<bool> seen(classes, false);
  for (std::size_t p = 0; p < P; ++p) {
    const std::int32_t k = s.region[p];
    if (k < 0 || seen[static_cast<std::size_t>(k)]) continue;
    seen[static_cast<std::size_t>(k)] = true;
    std::vector<exact::rat> mu, var;
    exact::rat vm = sc::q(0);
    for (std::size_t f = 0; f < n; ++f) {
      const sc::moments lw = sc::normalized_law(s, truth, f, p);
      mu.push_back(lw.mean);
      var.push_back(lw.var);
      vm = vm + lw.var;
    }
    const sc::moments qm = sc::quadratic_moments(sc::mean_plane_form(n), mu, var);
    E[k] = sc::approx(qm.mean);
    varX[k] = sc::approx(qm.var);
    varM[k] = sc::approx(vm / sc::q(static_cast<std::int64_t>(n * n)));
    center[k] = sc::approx(sc::global_center(s, truth, p));
  }
  struct acc {
    ld sumV = 0, sumM = 0, tauV = 0, tauM = 0;
    std::size_t N = 0, outV = 0, outM = 0, excluded = 0;
  };
  std::vector<acc> a(classes);
  std::vector<ld> sy(P, 0), syy(P, 0);
  std::vector<std::size_t> kp(P, 0);
  sc::gauss_stream gs(sc::flat_03_seed);
  sc::frames buf;
  std::vector<double> x(n);
  std::vector<ivl> y;
  const b9o::method_spec spec = spec_of(mean_m(), n);
  for (std::size_t b = 0; b < K / R; ++b) {
    fb.realize(s, gs, buf);
    const std::optional<master_result> res = fb.run(mean_m(), 32, 0);
    ASSERT_TRUE(res.has_value());
    for (std::size_t i = 0; i < R * P; ++i) {
      const std::size_t p = i % P;
      if (s.region[p] < 0) continue;
      acc& c = a[static_cast<std::size_t>(s.region[p])];
      fb.canonical_at(i, x.data());
      const b9o::pix_ivl pi = y_intervals(x.data(), n, iv, 512, y) ? b9o::method_interval(spec, y) : excluded_pixel();
      if (pi.excluded) {
        ++c.excluded;
        continue;
      }
      ASSERT_EQ(res->output.invalid.bits[i], 0) << "pixel invalido " << i;
      const double M = sample(res->output, i), V = plane(res->output, i);
      c.tauV = std::max(c.tauV, pi.v.hi - pi.v.lo);
      c.tauM = std::max(c.tauM, pi.m.hi - pi.m.lo);
      if (!inside(pi.v, V)) ++c.outV;
      if (!inside(pi.m, M)) ++c.outM;
      const ld dev = static_cast<ld>(M) - center[static_cast<std::size_t>(s.region[p])];
      c.sumV += V;
      c.sumM += dev;
      ++c.N;
      sy[p] += dev;
      syy[p] += dev * dev;
      ++kp[p];
    }
  }
  const double tail = kAlpha / (2.0 * static_cast<double>(m));
  const ld z = b9o::normal_upper_quantile(static_cast<ld>(tail));
  for (std::size_t k = 0; k < classes; ++k) {
    const acc& c = a[k];
    ld ss = 0;
    std::size_t nu = 0;
    for (std::size_t p = 0; p < P; ++p) {
      if (s.region[p] != static_cast<std::int32_t>(k) || kp[p] < 2) continue;
      ss += syy[p] - sy[p] * sy[p] / static_cast<ld>(kp[p]);
      nu += kp[p] - 1;
    }
    const ld N = static_cast<ld>(c.N);
    const ld meanV = c.sumV / N;
    const ld half_a = std::sqrt(static_cast<ld>(m) * varX[k] / (kAlpha * N)) + c.tauV;
    const ld q_lo = chi2x::lower_quantile(static_cast<double>(nu), tail), q_hi = chi2x::upper_quantile(static_cast<double>(nu), tail);
    const ld sd = std::sqrt(ss / static_cast<ld>(nu));
    const ld delta = std::sqrt(static_cast<ld>(K) / static_cast<ld>(K - 1)) * c.tauM;
    const ld lo_b = std::sqrt(varM[k] * q_lo / static_cast<ld>(nu)) - delta, hi_b = std::sqrt(varM[k] * q_hi / static_cast<ld>(nu)) + delta;
    const ld meanC = c.sumM / N, half_c = z * std::sqrt(varM[k] / N) + c.tauM;
    std::cout << std::setprecision(10) << "T-M3B-FLAT-03 classe " << k << " R " << R << " N " << c.N << " esclusi " << c.excluded
              << " V fuori " << c.outV << ", M fuori " << c.outM << "\n  (a) media V " << static_cast<double>(meanV) << " E "
              << static_cast<double>(E[k]) << " semiampiezza " << static_cast<double>(half_a) << " (max tau "
              << static_cast<double>(c.tauV) << ")\n  (b) sd " << static_cast<double>(sd) << " in [" << static_cast<double>(lo_b)
              << ", " << static_cast<double>(hi_b) << "] nu " << nu << "\n  (c) media M − centro " << static_cast<double>(meanC)
              << " semiampiezza " << static_cast<double>(half_c) << " (max tau " << static_cast<double>(c.tauM) << ")\n";
    EXPECT_EQ(c.outV, 0u) << "classe " << k << ": valori di V fuori dall'intervallo";
    EXPECT_EQ(c.outM, 0u) << "classe " << k << ": valori di M fuori dall'intervallo";
    EXPECT_LE(std::fabs(meanV - E[k]), half_a) << "classe " << k << " (a)";
    EXPECT_GE(sd, lo_b) << "classe " << k << " (b)";
    EXPECT_LE(sd, hi_b) << "classe " << k << " (b)";
    EXPECT_LE(std::fabs(meanC), half_c) << "classe " << k << " (c)";
  }
  std::cout << "T-M3B-FLAT-03 durata " << seconds_since(t0) << " s\n";
}

// ---------------------------------------------------------------- T-M3B-FLAT-04 (protocollo §3.5)

// Cella: metodo e parte. (a) classi con β = 0 (0 e 3 di six_classes), (b) con β = ±A (1, 2, 4, 5).
struct flat04_cell {
  method meth;
  bool part_b = false;
  std::string name;
};

constexpr std::size_t kFlat04M = 8;

void run_flat_04(std::size_t n) {
  const auto t0 = std::chrono::steady_clock::now();
  const sc::flat_set s = sc::flat_04_set(n);
  const sc::flat_truth truth = sc::truth_of(s);
  const std::size_t K = sc::flat_04_K, P = s.set.pixels();
  const clip_cfg clip{3, 3, 5, 2};
  std::vector<flat04_cell> cells;
  if (n == 3 || n == 9) {
    cells.push_back({median_m(), false, "mediana (a)"});
    cells.push_back({median_m(), true, "mediana (b)"});
  }
  if (n == 9) {
    cells.push_back({winsor_m(1), false, "winsorized g=1 (a)"});
    cells.push_back({winsor_m(1), true, "winsorized g=1 (b)"});
  }
  if (n == 11 || n == 31) cells.push_back({clip_m(clip), false, "sigma_clip (a)"});
  ASSERT_FALSE(cells.empty());
  if (n == 11 || n == 31) {
    ASSERT_FALSE(b6o::inert(n, clip.kl, clip.kh));  // (n − 1)² > 9n
  }
  const std::size_t R = batch_of(K, n, P);
  flat_batch fb(s, R);
  const b9o::scales_ivl iv = canonical_scale_intervals(s, fb.order);
  ASSERT_FALSE(iv.excluded);
  // Soglie esatte per pixel: centro b + c_H g; spostamenti max_f, min_f di (r_f − 1) β (parte (b)).
  std::vector<ld> center(P), up_shift(P, 0), dn_shift(P, 0);
  for (std::size_t p = 0; p < P; ++p) {
    center[p] = sc::approx(s.offset + s.beta[p] + truth.m_h[0] * s.g[p]);
    for (std::size_t f = 0; f < n; ++f) {
      const ld sh = sc::approx((truth.r[0][f] - sc::q(1)) * s.beta[p]);
      up_shift[p] = f == 0 ? sh : std::max(up_shift[p], sh);
      dn_shift[p] = f == 0 ? sh : std::min(dn_shift[p], sh);
    }
  }
  const auto in_cell = [&](const flat04_cell& c, std::size_t p) {
    const std::int32_t k = s.region[p];
    if (k < 0) return false;
    const bool zero_beta = k == 0 || k == 3;
    return c.part_b ? !zero_beta : zero_beta;
  };
  struct counts {
    std::uint64_t N = 0, U = 0, D = 0, excluded = 0, undetermined = 0, incomplete = 0, outside = 0;
    ld tau_max = 0;
  };
  std::vector<counts> cnt(cells.size());
  sc::gauss_stream gs(sc::flat_04_seed(n));
  sc::frames buf;
  std::vector<double> x(n);
  std::vector<ivl> y;
  struct pend {
    ld M, lo_thr, hi_thr;
  };
  std::vector<pend> pending;
  bool binary_checked = false;
  for (std::size_t b = 0; b < K / R; ++b) {
    fb.realize(s, gs, buf);
    for (std::size_t ci = 0; ci < cells.size(); ++ci) {
      const flat04_cell& cell = cells[ci];
      // Le due parti dello stesso metodo usano la stessa combinazione: si calcola una volta per metodo.
      if (ci > 0 && cells[ci - 1].meth.k == cell.meth.k && cells[ci - 1].meth.g == cell.meth.g) continue;
      const std::optional<master_result> res = fb.run(cell.meth, 32, 0.02);
      ASSERT_TRUE(res.has_value());
      if (!binary_checked) {
        binary_checked = true;
        ASSERT_TRUE(res->output.measure.has_value());
        const auto& lim = res->output.measure->limits;
        EXPECT_EQ(std::count(lim.begin(), lim.end(), measure_limit::variance_fixed_weight_approximation), 1);
      }
      const b9o::method_spec spec = spec_of(cell.meth, n);
      for (std::size_t cj = ci; cj < cells.size() && cells[cj].meth.k == cell.meth.k && cells[cj].meth.g == cell.meth.g; ++cj) {
        counts& c = cnt[cj];
        for (std::size_t r = 0; r < R; ++r) {
          // τ_k: ampiezza massima degli intervalli dei valori confrontati nella realizzazione (protocollo §1).
          pending.clear();
          ld tau_k = 0;
          for (std::size_t p = 0; p < P; ++p) {
            if (!in_cell(cells[cj], p)) continue;
            const std::size_t i = r * P + p;
            fb.canonical_at(i, x.data());
            const std::uint16_t bits = res->output.invalid.bits[i];
            if (!y_intervals(x.data(), n, iv, 512, y)) {
              ++c.excluded;
              continue;
            }
            const b9o::pix_ivl pi = b9o::method_interval(spec, y);
            if (pi.excluded) {
              ++c.excluded;
              continue;
            }
            if (pi.undetermined) {
              ++c.undetermined;
              continue;
            }
            if (pi.support_incomplete) {
              ++c.incomplete;
              EXPECT_EQ(bits, invalid_reason::support_incomplete) << "pixel " << i;
              continue;
            }
            EXPECT_EQ(bits, 0) << "pixel " << i;
            const double M = sample(res->output, i);
            if (!inside(pi.m, M)) ++c.outside;
            tau_k = std::max(tau_k, pi.m.hi - pi.m.lo);
            const ld lo = center[p] + (cells[cj].part_b ? dn_shift[p] : 0);
            const ld hi = center[p] + (cells[cj].part_b ? up_shift[p] : 0);
            pending.push_back({static_cast<ld>(M), lo, hi});
          }
          c.tau_max = std::max(c.tau_max, tau_k);
          for (const pend& q : pending) {
            ++c.N;
            if (q.M > q.hi_thr + tau_k) ++c.U;
            if (q.M < q.lo_thr - tau_k) ++c.D;
          }
        }
      }
    }
  }
  const ld tail = static_cast<ld>(kAlpha) / (2 * static_cast<ld>(kFlat04M));
  for (std::size_t ci = 0; ci < cells.size(); ++ci) {
    const counts& c = cnt[ci];
    const std::uint64_t q = b9o::binomial_upper_quantile(c.N, tail);
    std::cout << "T-M3B-FLAT-04 n=" << n << " " << cells[ci].name << ": N " << c.N << " U " << c.U << " D " << c.D << " soglia " << q
              << " (esclusi " << c.excluded << ", indeterminati " << c.undetermined << ", support_incomplete " << c.incomplete
              << ", M fuori " << c.outside << ", max tau " << static_cast<double>(c.tau_max) << ")\n";
    EXPECT_GT(c.N, 0u) << cells[ci].name;
    EXPECT_EQ(c.outside, 0u) << cells[ci].name << ": valori di M fuori dall'intervallo";
    EXPECT_LE(c.U, q) << cells[ci].name << " U";
    EXPECT_LE(c.D, q) << cells[ci].name << " D";
  }
  std::cout << "T-M3B-FLAT-04 n=" << n << " durata " << seconds_since(t0) << " s\n";
}

TEST(MasterB9Slow, T_M3B_FLAT_04_n3_median) { run_flat_04(3); }
TEST(MasterB9Slow, T_M3B_FLAT_04_n9_median_and_winsorized) { run_flat_04(9); }
TEST(MasterB9Slow, T_M3B_FLAT_04_n11_sigma_clip) { run_flat_04(11); }
TEST(MasterB9Slow, T_M3B_FLAT_04_n31_sigma_clip) { run_flat_04(31); }

// Premessa del protocollo §3.5: soglia binomiale indicativa per N = 1 364 000 (684 240) e 2 728 000 (1 367 168).
TEST(MasterB9, T_M3B_FLAT_04_exact_binomial_threshold_matches_the_protocol_indication) {
  const ld tail = static_cast<ld>(kAlpha) / (2 * static_cast<ld>(kFlat04M));
  const std::uint64_t q1 = b9o::binomial_upper_quantile(1364000, tail), q2 = b9o::binomial_upper_quantile(2728000, tail);
  EXPECT_NEAR(static_cast<double>(q1), 684240.0, 2.0);
  EXPECT_NEAR(static_cast<double>(q2), 1367168.0, 2.0);
  EXPECT_LE(b9o::binomial_upper_tail(1364000, q1 + 1), tail);
  EXPECT_GT(b9o::binomial_upper_tail(1364000, q1), tail);
}

// ---------------------------------------------------------------- T-M3B-FLAT-05 (protocollo §3.6), caratterizzazione

void run_flat_05(std::size_t n) {
  const auto t0 = std::chrono::steady_clock::now();
  const double kappa = n == 3 ? 1 : n == 9 ? 2.5 : 3;
  std::vector<method> methods = {median_m(), winsor_m(1)};
  if ((n - 1) / 2 > 1) methods.push_back(winsor_m((n - 1) / 2));
  methods.push_back(clip_m({kappa, kappa, 5, 2}));
  ASSERT_FALSE(b6o::inert(n, kappa, kappa));
  for (std::int64_t rho : {10, 15, 20, 30, 40}) {
    for (int regime : {0, 1}) {
      const sc::flat_set s = sc::flat_05_set(n, rho, regime);
      const std::size_t K = sc::flat_05_K, P = s.set.pixels(), lot = 16;
      ASSERT_EQ(K % lot, 0u);
      const std::size_t R = batch_of(K, n, P);
      flat_batch fb(s, R);
      struct stat {
        ld sumV = 0;
        std::size_t NV = 0, incomplete = 0, other_invalid = 0;
        std::vector<ld> sy, syy, lsumV, lsy, lsyy;
        std::vector<std::size_t> kp, lN, lk;
      };
      std::vector<std::array<stat, 2>> st(methods.size());
      for (auto& sm : st)
        for (stat& c : sm) {
          c.sy.assign(P, 0);
          c.syy.assign(P, 0);
          c.kp.assign(P, 0);
          c.lsumV.assign(K / lot, 0);
          c.lsy.assign(P * (K / lot), 0);
          c.lsyy.assign(P * (K / lot), 0);
          c.lk.assign(P * (K / lot), 0);
          c.lN.assign(K / lot, 0);
        }
      sc::gauss_stream gs(sc::flat_05_seed(n, rho, regime));
      sc::frames buf;
      for (std::size_t b = 0; b < K / R; ++b) {
        fb.realize(s, gs, buf);
        for (std::size_t mi = 0; mi < methods.size(); ++mi) {
          const std::optional<master_result> res = fb.run(methods[mi], 32, 0.02);
          ASSERT_TRUE(res.has_value());
          if (b == 0) {
            ASSERT_TRUE(res->output.measure.has_value());
            const auto& lim = res->output.measure->limits;
            EXPECT_EQ(std::count(lim.begin(), lim.end(), measure_limit::variance_fixed_weight_approximation), 1) << methods[mi].name();
          }
          for (std::size_t i = 0; i < R * P; ++i) {
            const std::size_t p = i % P;
            if (s.region[p] < 0) continue;
            stat& c = st[mi][static_cast<std::size_t>(s.region[p])];
            const std::uint16_t bits = res->output.invalid.bits[i];
            if (bits == invalid_reason::support_incomplete) {
              ++c.incomplete;
              continue;
            }
            if (bits != 0) {
              ++c.other_invalid;
              continue;
            }
            const std::size_t real = b * R + i / P, l = real / lot;
            const ld M = sample(res->output, i), V = plane(res->output, i);
            const ld dev = M - static_cast<ld>(s.set.mean[0][p]);
            c.sumV += V;
            ++c.NV;
            c.sy[p] += dev;
            c.syy[p] += dev * dev;
            ++c.kp[p];
            c.lsumV[l] += V;
            ++c.lN[l];
            c.lsy[l * P + p] += dev;
            c.lsyy[l * P + p] += dev * dev;
            ++c.lk[l * P + p];
          }
        }
      }
      for (std::size_t mi = 0; mi < methods.size(); ++mi) {
        for (std::size_t k = 0; k < 2; ++k) {
          const stat& c = st[mi][k];
          const auto pooled = [&](const ld* sy, const ld* syy, const std::size_t* kp) {
            ld ss = 0;
            std::size_t nu = 0;
            for (std::size_t p = 0; p < P; ++p) {
              if (kp[p] < 2) continue;
              ss += syy[p] - sy[p] * sy[p] / static_cast<ld>(kp[p]);
              nu += kp[p] - 1;
            }
            return nu == 0 ? std::numeric_limits<ld>::quiet_NaN() : ss / static_cast<ld>(nu);
          };
          const ld ratio = (c.sumV / static_cast<ld>(c.NV)) / pooled(c.sy.data(), c.syy.data(), c.kp.data());
          std::vector<ld> lots;
          for (std::size_t l = 0; l < K / lot; ++l) {
            lots.push_back((c.lsumV[l] / static_cast<ld>(c.lN[l])) / pooled(&c.lsy[l * P], &c.lsyy[l * P], &c.lk[l * P]));
          }
          ld lm = 0, lv = 0;
          for (ld v : lots) lm += v;
          lm /= static_cast<ld>(lots.size());
          for (ld v : lots) lv += (v - lm) * (v - lm);
          const ld se = std::sqrt(lv / static_cast<ld>(lots.size() - 1) / static_cast<ld>(lots.size()));
          std::cout << std::setprecision(6) << "T-M3B-FLAT-05 n=" << n << " rapporto " << static_cast<double>(rho) / 10 << " regime "
                    << (regime == 0 ? "fotonico" : "lettura") << " " << methods[mi].name() << " classe g=" << (k == 0 ? "1/2" : "3/2")
                    << ": E[V]/Var(M|r) ~ " << static_cast<double>(ratio) << " +- " << static_cast<double>(se) << " (N " << c.NV
                    << ", support_incomplete " << c.incomplete << ", altri invalidi " << c.other_invalid << ")\n";
          EXPECT_EQ(c.other_invalid, 0u) << methods[mi].name();
          EXPECT_GT(c.NV, 0u) << methods[mi].name();
        }
      }
    }
  }
  std::cout << "T-M3B-FLAT-05 n=" << n << " durata " << seconds_since(t0) << " s\n";
}

TEST(MasterB9Slow, T_M3B_FLAT_05_n3_robust_plane_characterization) { run_flat_05(3); }
TEST(MasterB9Slow, T_M3B_FLAT_05_n9_robust_plane_characterization) { run_flat_05(9); }
TEST(MasterB9Slow, T_M3B_FLAT_05_n31_robust_plane_characterization) { run_flat_05(31); }

}  // namespace
