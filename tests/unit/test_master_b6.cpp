// M3.b, unità B6: `mean` + `sigma_clip` (C-02.b §b.1, §b.6, §b.7, §b.10, §b.12; protocollo preregistrato
// docs/evidence/M3b-protocollo.md, versione 3, SHA-256 e21760c7f68590d71bccee78088f1577ea0a1bd7e337bc758c3a4aa7f3e1ffc7).
// Test: T-M3B-C02-01 (parte sigma), 02, 06; T-M3B-C02-15 (pixel `support_incomplete` a livello di combinazione).
//
// Oracle in tests/unit/b6_oracle.hpp (catena esatta e regola I di b5_oracle.hpp); frame da FIX-SYN-CAL-08
// (tests/fixtures/syn_cal.hpp). Nessun valore atteso viene dal prodotto. `measure` atteso = `merge_measure` dei frame più
// `variance.fixed_weight_approximation` (C-02.b §b.7).
//
// Tolleranze, fissate prima della misura:
// - T-M3B-C02-01, 02 (classi E ed E con intervalli): decisioni dai razionali esatti se ogni operazione della procedura
//   è rappresentabile, altrimenti dagli intervalli della regola I; un pixel con intervalli sovrapposti (o fuori dalle
//   condizioni della regola I) esce dalla misura ed è contato e stampato. M e V: 0 dove l'oracle verifica esatta la
//   sequenza, altrimenti l'ampiezza della regola I, più mezzo ulp f32 con uscita f32.
// - Appartenenza (C02-02): il prodotto non espone l'insieme rifiutato per pixel; la si osserva da M e V del pixel
//   entro la tolleranza dell'insieme dell'oracle (ogni campione tolto o aggiunto sposta M di ordine σ/m ≫ τ) e dai
//   conteggi per frame `rejected_samples` del report, uguali all'oracle: i campioni di ogni R applicato, compreso
//   l'ultimo che porta un pixel a `support_incomplete` (M3b-lacune, «Decisioni successive (B6)»). Con pixel esclusi
//   il conteggio del prodotto sta fra quello dei pixel decisi e quello più il numero dei pixel esclusi. Limite
//   dichiarato: l'insieme rifiutato per pixel non si osserva direttamente.
// - T-M3B-C02-06 (classe C): nessuna soglia sul rapporto; stima e errore Monte Carlo stampati (protocollo §3.3).
//   Binario: capability, `variance_estimator`, avviso.
//
// Parametri di `sigma_clip` dei test E (protocollo §4: «parametri nel codice», fissati qui prima della misura):
// C02-02 configurazioni A…E sotto (`c02_02_configs`), C02-01 per n in `c02_01_clip`.
//
// Bit `support_incomplete`: `invalid_reason::support_incomplete`, bit 9 di ADR-03.b §b.6 (T-M3B-C02-15 controlla il valore).
#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "b6_oracle.hpp"
#include "master_support.hpp"
#include "store_test_support.hpp"
#include "syn_cal.hpp"

namespace {

using namespace mst;
namespace sc = astro_fixture::syn_cal;
using b6o::ld;

constexpr std::uint16_t kSupportIncomplete = invalid_reason::support_incomplete;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// ---------------------------------------------------------------- ricetta

struct clip {
  double kl = 3, kh = 3;
  std::size_t iter = 1, min_kept = 2;
  std::string name() const {
    std::ostringstream o;
    o << std::setprecision(17) << "κl=" << kl << " κh=" << kh << " iter=" << iter << " min_kept=" << min_kept;
    return o.str();
  }
  b6o::params p() const { return {kl, kh, iter, min_kept}; }
};

void apply_clip(const clip& c, master_recipe& r) {
  r.combine = combine_method::mean;
  r.rejection = master_rejection::sigma_clip;
  r.clip_kappa_low = c.kl;
  r.clip_kappa_high = c.kh;
  r.clip_max_iter = static_cast<double>(c.iter);
  r.clip_min_kept = static_cast<double>(c.min_kept);
}

// Posizione canonica → indice del frame nella ricetta (C-02.a §8: digest SHA-256 crescente).
std::vector<std::size_t> canonical(const master_recipe& r) {
  std::vector<std::size_t> order(r.frames.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::sort(order.begin(), order.end(),
            [&](std::size_t a, std::size_t b) { return r.frames[a].digest.sha256 < r.frames[b].digest.sha256; });
  return order;
}

std::optional<master_result> combine_or_fail(const master_recipe& r, std::vector<image> frames, const std::string& what) {
  try {
    return combine_master(r, std::move(frames));
  } catch (const stop_error& e) {
    ADD_FAILURE() << what << ": stop " << e.code() << " — " << e.what();
    return std::nullopt;
  }
}

// px[i][f]: campione del frame f (indice di generazione) al pixel i.
group group_of(const std::vector<std::vector<double>>& px, std::uint32_t w, std::uint32_t h) {
  const std::size_t n = px.at(0).size();
  group g = make_group(master_role::dark, w, h, n);
  for (std::size_t i = 0; i < px.size(); ++i) {
    if (px[i].size() != n) ADD_FAILURE() << "pixel " << i << " con un numero di frame diverso";
    for (std::size_t f = 0; f < n; ++f) g.frames[f].v[i] = px[i][f];
  }
  return g;
}

group group_of_frames(const sc::frames& x, std::uint32_t w, std::uint32_t h) {
  group g = make_group(master_role::dark, w, h, x.size());
  for (std::size_t f = 0; f < x.size(); ++f) g.frames[f].v = x[f];
  return g;
}

std::vector<double> canonical_samples(const group& g, const std::vector<std::size_t>& order, std::size_t i) {
  std::vector<double> x;
  for (std::size_t c : order) x.push_back(stored(g.frames[c], i));
  return x;
}

// Permutazioni cicliche dei valori sui frame: la selezione non dipende dalla posizione del frame.
std::vector<std::vector<double>> rotations(const std::vector<std::vector<double>>& base) {
  std::vector<std::vector<double>> out;
  for (const auto& v : base) {
    for (std::size_t s = 0; s < v.size(); ++s) {
      std::vector<double> r(v.size());
      for (std::size_t f = 0; f < v.size(); ++f) r[f] = v[(f + s) % v.size()];
      out.push_back(r);
    }
  }
  return out;
}

bool has_string(const json& arr, const std::string& s) {
  if (arr.type() != json::kind::array) return false;
  for (const json& v : arr.items())
    if (v.type() == json::kind::string && v.as_string() == s) return true;
  return false;
}

// ---------------------------------------------------------------- confronto di un pixel con l'oracle

// |got − esatto| ≤ tolleranza: 0 se la sequenza è esatta, altrimenti l'ampiezza della regola I (+ mezzo ulp f32).
::testing::AssertionResult close_to(double got, const exact::rat& value, bool exact_seq, const b6o::ivl& i, bool f32) {
  if (!std::isfinite(got)) return ::testing::AssertionFailure() << "valore non finito " << got;
  exact::rat tol = exact::from_u(0);
  if (!exact_seq) tol = b5o::from_ld(i.hi - i.lo);
  if (f32) tol = tol + half_ulp_out(value, got, true);
  const exact::rat diff = exact::abs(exact::from_double(got) - value);
  if (diff <= tol) return ::testing::AssertionSuccess();
  return ::testing::AssertionFailure() << "scarto " << b5o::approx(diff) << " oltre " << b5o::approx(tol) << " (esatto "
                                       << std::setprecision(17) << b5o::approx(value) << ", ottenuto " << got
                                       << (exact_seq ? ", sequenza esatta" : ", regola I") << ")";
}

// Pixel deciso dall'oracle: support_incomplete → solo quel bit, M e V NaN; altrimenti nessun bit (il rifiuto non è
// un'invalidità) e M, V dell'insieme tenuto.
void expect_clip_pixel(const master_result& res, std::size_t i, const b6o::pixel_oracle& o, const std::string& where) {
  const bool f32 = res.output.type == sample_type::f32;
  const double m = sample(res.output, i), v = plane(res.output, i);
  const std::uint16_t bits = res.output.invalid.bits[i];
  if (o.sel.support_incomplete) {
    EXPECT_EQ(bits, kSupportIncomplete) << where;
    EXPECT_TRUE(std::isnan(m) && std::isnan(v)) << where << ": campione " << m << ", piano " << v;
    return;
  }
  EXPECT_EQ(bits, 0) << where;
  EXPECT_TRUE(close_to(m, o.val.ex.m, o.val.ex.exact_m, o.val.m, f32)) << where << ": M";
  EXPECT_TRUE(close_to(v, o.val.ex.v, o.val.ex.exact_v, o.val.v, f32)) << where << ": V";
}

struct run_stats {
  std::size_t pixels = 0, undetermined = 0, rule_i_excluded = 0, incomplete = 0, with_rejection = 0, exact_decisions = 0;
  std::vector<std::uint64_t> rejected;  // per posizione canonica, sui pixel decisi: ogni R applicato, compreso l'ultimo
};

struct clip_run {
  std::optional<master_result> res;
  std::vector<std::size_t> order;
  std::vector<b6o::pixel_oracle> oracle;
  run_stats st;
};

// Oracle per ogni pixel prima della combinazione, poi la combinazione del prodotto.
clip_run run_clip(const group& g, const clip& c, sample_type out = sample_type::f64) {
  clip_run cr;
  master_recipe r = recipe_of(g);
  r.output_type = out;
  apply_clip(c, r);
  cr.order = canonical(r);
  const std::size_t n = g.frames.size(), P = static_cast<std::size_t>(g.w) * g.h;
  cr.st.rejected.assign(n, 0);
  for (std::size_t i = 0; i < P; ++i) {
    const b6o::pixel_oracle o = b6o::oracle_pixel(canonical_samples(g, cr.order, i), c.p());
    if (o.nonfinite) ADD_FAILURE() << "pixel " << i << ": la fixture produce un risultato zero o subnormale";
    ++cr.st.pixels;
    if (o.sel.v == b6o::verdict::undetermined) ++cr.st.undetermined;
    if (o.sel.v == b6o::verdict::rule_i_excluded) ++cr.st.rule_i_excluded;
    if (o.sel.v == b6o::verdict::determined) {
      if (o.from_exact) ++cr.st.exact_decisions;
      if (o.sel.support_incomplete) ++cr.st.incomplete;
      const bool any = std::any_of(o.sel.rejected.begin(), o.sel.rejected.end(), [](char b) { return b != 0; });
      if (any && !o.sel.support_incomplete) ++cr.st.with_rejection;
      for (std::size_t f = 0; f < n; ++f) cr.st.rejected[f] += static_cast<std::uint64_t>(o.sel.rejected[f]);
    }
    cr.oracle.push_back(o);
  }
  cr.res = combine_or_fail(r, images_of(g), c.name());
  return cr;
}

void check_run(const clip_run& cr, const std::string& what) {
  if (!cr.res) return;
  for (std::size_t i = 0; i < cr.oracle.size(); ++i) {
    if (cr.oracle[i].sel.v != b6o::verdict::determined) continue;
    expect_clip_pixel(*cr.res, i, cr.oracle[i], what + " pixel " + std::to_string(i));
  }
}

// `rejected_samples` del report per posizione canonica (frame del report riconosciuti dal digest).
std::optional<std::vector<std::uint64_t>> report_rejected(const master_result& res, const master_recipe& r,
                                                          const std::vector<std::size_t>& order) {
  const json* frames = res.report.find("frames");
  if (frames == nullptr || frames->type() != json::kind::array) return std::nullopt;
  std::vector<std::uint64_t> out(order.size(), 0);
  std::vector<char> seen(order.size(), 0);
  for (const json& f : frames->items()) {
    const json* d = f.find("digest_sha256");
    const json* k = f.find("rejected_samples");
    if (d == nullptr || k == nullptr || d->type() != json::kind::string || k->type() != json::kind::integer) return std::nullopt;
    for (std::size_t c = 0; c < order.size(); ++c) {
      if (r.frames[order[c]].digest.sha256 == d->as_string()) {
        out[c] = static_cast<std::uint64_t>(k->as_integer());
        seen[c] = 1;
      }
    }
  }
  if (std::count(seen.begin(), seen.end(), 1) != static_cast<std::ptrdiff_t>(order.size())) return std::nullopt;
  return out;
}

void print_stats(const std::string& what, const run_stats& s) {
  std::cout << what << ": pixel " << s.pixels << ", decisi coi razionali esatti " << s.exact_decisions
            << ", esclusi per intervalli sovrapposti " << s.undetermined << ", esclusi per la regola I " << s.rule_i_excluded
            << ", support_incomplete " << s.incomplete << ", validi con rifiuti " << s.with_rejection << "\n";
}

// ---------------------------------------------------------------- T-M3B-C02-01 (parte sigma): casi dyadic

struct dyadic_case {
  std::string name;
  clip c;
  std::vector<std::vector<double>> px;
  bool v_exact_required;  // dichiarato: V esatto su ogni pixel
};

// Valori scelti perché M = (Σ_K x)/m e V = (Σ_K (x − M)²/(m − 1))/m siano esatti in f64 (V = s_K²/m, pesi 1/m sui
// tenuti), con una e più iterazioni, soglie uguali e asimmetriche, `clip_max_iter` che tronca, s = 0 e confronti al
// confine (x − μ = κ s esatto: disuguaglianza stretta, nessun rifiuto).
std::vector<dyadic_case> dyadic_cases() {
  std::vector<dyadic_case> c;
  c.push_back({"n=4 κ=1 una iterazione", {1, 1, 1, 2},
               rotations({{1001, 1008, 1000, 992}, {1032, 1000, 1032, 1032}, {1000, 1000, 1000, 1000}}), true});
  c.push_back({"n=4 κ=1 due iterazioni", {1, 1, 2, 2}, rotations({{1001, 1016, 1001, 1004}, {1000, 984, 1032, 1002}}), true});
  c.push_back({"n=5 κ=1 fino a tre iterazioni", {1, 1, 5, 2},
               rotations({{1001, 984, 1008, 1000, 1004}, {1002, 992, 992, 1000, 1004}, {1001, 1004, 1008, 1000, 992}}), true});
  c.push_back({"n=5 κl=1 κh=2", {1, 2, 5, 2},
               rotations({{1000, 1000, 1000, 992, 1004}, {1002, 1008, 1002, 984, 992}, {992, 1002, 1008, 1002, 992}}), true});
  c.push_back({"n=5 κl=1.5 κh=1 iter=2", {1.5, 1, 2, 2},
               rotations({{1016, 992, 1002, 984, 1002},
                          {1008, 1032, 1032, 1008, 1002},
                          {1008, 1016, 1002, 1002, 1002},
                          {1000, 1032, 1002, 1016, 1032}}),
               true});
  c.push_back({"confine n=3 κ=1", {1, 1, 5, 2}, rotations({{1000, 1002, 1004}, {1008, 1003, 998}}), false});
  c.push_back({"confine n=6 κ=1.5", {1.5, 1.5, 5, 2}, rotations({{1004, 1004, 998, 998, 1000, 1008}}), false});
  return c;
}

TEST(MasterB6, T_M3B_C02_01_sigma_dyadic_cases_give_the_exact_master_and_frozen_selection_plane) {
  for (const dyadic_case& dc : dyadic_cases()) {
    SCOPED_TRACE(dc.name + " " + dc.c.name());
    const group g = group_of(dc.px, static_cast<std::uint32_t>(dc.px.size()), 1);
    const clip_run cr = run_clip(g, dc.c);
    // Precondizioni: ogni pixel deciso, M esatto (e V dove dichiarato), almeno un pixel con rifiuti dove la riga lo
    // dichiara.
    bool all_m = true, all_v = true;
    for (const b6o::pixel_oracle& o : cr.oracle) {
      ASSERT_EQ(o.sel.v, b6o::verdict::determined) << "caso non deciso dall'oracle";
      ASSERT_FALSE(o.sel.support_incomplete);
      all_m = all_m && o.val.ex.exact_m && !o.val.ex.nonfinite;
      all_v = all_v && o.val.ex.exact_v;
    }
    ASSERT_TRUE(all_m) << "caso non dyadic per M";
    if (dc.v_exact_required) {
      EXPECT_TRUE(all_v) << "V dichiarato esatto non lo è";
    }
    if (dc.name.rfind("confine", 0) == 0) {
      EXPECT_EQ(cr.st.with_rejection, 0u) << "al confine nessun campione è rifiutato";
      EXPECT_EQ(cr.st.exact_decisions, cr.st.pixels) << "il confine richiede decisioni esatte";
    } else {
      EXPECT_GT(cr.st.with_rejection, 0u) << "il caso non rifiuta nulla";
    }
    check_run(cr, dc.name);
  }
}

// Il valore del pixel con rifiuti non è la media di tutti i campioni: lo stesso gruppo con `mean` senza rifiuto dà
// valori diversi sui pixel in cui l'oracle rifiuta (controllo che la procedura sia quella di §b.1).
TEST(MasterB6, T_M3B_C02_01_sigma_master_differs_from_the_plain_mean_where_samples_are_rejected) {
  const dyadic_case dc = dyadic_cases()[0];
  const group g = group_of(dc.px, static_cast<std::uint32_t>(dc.px.size()), 1);
  const clip_run cr = run_clip(g, dc.c);
  master_recipe plain = recipe_of(g);
  plain.output_type = sample_type::f64;
  const std::optional<master_result> rm = combine_or_fail(plain, images_of(g), "mean");
  if (!cr.res || !rm) return;
  std::size_t differing = 0;
  for (std::size_t i = 0; i < cr.oracle.size(); ++i) {
    const auto& rej = cr.oracle[i].sel.rejected;
    if (std::none_of(rej.begin(), rej.end(), [](char b) { return b != 0; })) continue;
    if (sample(cr.res->output, i) != sample(rm->output, i)) ++differing;
  }
  EXPECT_EQ(differing, cr.st.with_rejection);
}

// ---------------------------------------------------------------- T-M3B-C02-01 (parte sigma): casi non dyadic

// Parametri per n sugli insiemi di C02-01 (seed 2026110961), tutti non inerti.
clip c02_01_clip(std::size_t n) {
  switch (n) {
    case 3: return {1, 1, 5, 2};
    case 4: return {1, 1, 5, 2};
    case 5: return {1.5, 1.5, 5, 2};
    case 9: return {1.5, 2, 5, 2};
    case 10: return {2, 2, 5, 2};
    case 11: return {3, 2, 5, 2};
    case 101: return {2.5, 2.5, 5, 2};
    default: return {};
  }
}

std::vector<std::pair<std::size_t, group>> c02_01_groups() {
  const sc::gaussian_cases cases = sc::c02_01_cases();
  const std::vector<sc::frames> x = sc::realize_cases(cases);
  std::vector<std::pair<std::size_t, group>> out;
  for (std::size_t c = 0; c < cases.sets.size(); ++c)
    out.emplace_back(cases.n[c], group_of_frames(x[c], cases.sets[c].width, cases.sets[c].height));
  return out;
}

TEST(MasterB6, T_M3B_C02_01_sigma_non_dyadic_cases_within_rule_i_f64_and_f32) {
  for (const auto& [n, g] : c02_01_groups()) {
    const clip c = c02_01_clip(n);
    ASSERT_FALSE(b6o::inert(n, c.kl, c.kh)) << "n=" << n;
    for (sample_type out : {sample_type::f64, sample_type::f32}) {
      const std::string what = "C02-01 n=" + std::to_string(n) + (out == sample_type::f32 ? " f32" : " f64");
      SCOPED_TRACE(what + " " + c.name());
      const clip_run cr = run_clip(g, c, out);
      print_stats(what, cr.st);
      EXPECT_GT(cr.st.with_rejection, 0u) << "nessun rifiuto: caso senza potere";
      check_run(cr, what);
    }
  }
}

// Un solo arrotondamento al sample_type: l'uscita f32 è float() dell'uscita f64, bit per bit.
TEST(MasterB6, T_M3B_C02_01_sigma_f32_output_is_the_single_rounding_of_the_f64_result) {
  for (const auto& [n, g] : c02_01_groups()) {
    SCOPED_TRACE("n=" + std::to_string(n));
    master_recipe r = recipe_of(g);
    apply_clip(c02_01_clip(n), r);
    r.output_type = sample_type::f64;
    const std::optional<master_result> r64 = combine_or_fail(r, images_of(g), "f64");
    r.output_type = sample_type::f32;
    const std::optional<master_result> r32 = combine_or_fail(r, images_of(g), "f32");
    if (!r64 || !r32) continue;
    ASSERT_EQ(r32->output.type, sample_type::f32);
    for (std::size_t i = 0; i < r64->output.invalid.bits.size(); ++i) {
      ASSERT_EQ(r32->output.invalid.bits[i], r64->output.invalid.bits[i]) << i;
      if (r64->output.invalid.bits[i] != 0) continue;
      EXPECT_EQ(std::bit_cast<std::uint32_t>(r32->output.data<float>()[i]),
                std::bit_cast<std::uint32_t>(static_cast<float>(r64->output.data<double>()[i])))
          << "M al pixel " << i;
      EXPECT_EQ(std::bit_cast<std::uint32_t>(static_cast<float>(plane(r32->output, i))),
                std::bit_cast<std::uint32_t>(static_cast<float>(plane(r64->output, i))))
          << "V al pixel " << i;
    }
  }
}

// §b.7: un campione invalido o saturo rende invalido il pixel con i bit della causa, senza `support_incomplete`; un
// pixel valido usa tutti gli n frame prima del rifiuto.
TEST(MasterB6, T_M3B_C02_01_sigma_masks_invalid_and_saturated_samples_invalidate_the_pixel) {
  constexpr std::uint16_t nf = invalid_reason::source_nonfinite, fl = invalid_reason::source_flagged,
                          sat = invalid_reason::saturated;
  group g = make_group(master_role::dark, 4, 2, 3, 16);
  g.frames[0].bitpix = -32;
  // Pixel validi 0, 1, 4, 6, 7: un rifiuto e due tenuti diversi (0, 4, 6, 7), oppure il confine x − μ = κ s esatto
  // senza rifiuti (1). I tenuti diversi lasciano s > 0, così ogni confronto resta deciso dalla regola I.
  const double base[8][3] = {{100, 104, 132}, {100, 102, 104}, {101, 105, 109}, {100, 116, 104},
                             {120, 104, 100}, {108, 104, 100}, {100, 101, 132}, {104, 100, 132}};
  for (std::size_t f = 0; f < 3; ++f)
    for (std::size_t i = 0; i < 8; ++i) g.frames[f].v[i] = base[i][f];
  std::map<std::size_t, std::uint16_t> want;
  g.frames[0].v[2] = kNaN;
  want[2] = nf;
  g.frames[0].v[5] = 900;
  want[5] = sat;
  g.frames[1].invalid = std::vector<std::uint16_t>(8, 0);
  g.frames[0].v[3] = kNaN;
  (*g.frames[1].invalid)[3] = fl;
  g.frames[2].v[3] = 950;
  want[3] = nf | fl | sat;
  const clip c{1, 1, 5, 2};
  master_recipe r = recipe_of(g);
  r.saturation_adu = 900;
  r.saturated_fraction_max = 0.5;
  apply_clip(c, r);
  const std::vector<std::size_t> order = canonical(r);
  const std::optional<master_result> res = combine_or_fail(r, images_of(g), "sigma_clip");
  if (!res) return;
  for (std::size_t i = 0; i < 8; ++i) {
    if (want.count(i)) {
      EXPECT_EQ(res->output.invalid.bits[i], want[i]) << i;
      EXPECT_TRUE(std::isnan(sample(res->output, i)) && std::isnan(plane(res->output, i))) << i;
      continue;
    }
    const b6o::pixel_oracle o = b6o::oracle_pixel(canonical_samples(g, order, i), c.p());
    ASSERT_EQ(o.sel.v, b6o::verdict::determined) << i;
    expect_clip_pixel(*res, i, o, "pixel " + std::to_string(i));
  }
  EXPECT_TRUE(operator_output_invariants_hold(res->output));
}

// §b.6, «Rappresentabilità»: (x − μ)·(x − μ) con operandi non nulli che dà un subnormale rende il pixel
// `result_nonfinite`, campione e piano NaN; gli altri pixel restano quelli di `sigma_clip`.
TEST(MasterB6, T_M3B_C02_01_sigma_subnormal_square_of_a_deviation_gives_result_nonfinite) {
  const clip c{1, 1, 5, 2};
  const std::vector<std::vector<double>> px = {
      {-0x1p-520, 0, 0x1p-520}, {1000, 1002, 1004}, {1001, 1016, 1000}, {1008, 1003, 998}};
  const group g = group_of(px, 4, 1);
  master_recipe r = recipe_of(g);
  apply_clip(c, r);
  const std::vector<std::size_t> order = canonical(r);
  // Precondizione: per l'oracle esatto il solo pixel 0 incontra il risultato subnormale.
  for (std::size_t i = 0; i < px.size(); ++i)
    ASSERT_EQ(b6o::select_exact(canonical_samples(g, order, i), c.p()).nonfinite, i == 0) << i;
  const std::optional<master_result> res = combine_or_fail(r, images_of(g), "sigma_clip");
  if (!res) return;
  EXPECT_EQ(res->output.invalid.bits[0], invalid_reason::result_nonfinite);
  EXPECT_TRUE(std::isnan(sample(res->output, 0)) && std::isnan(plane(res->output, 0)));
  for (std::size_t i = 1; i < px.size(); ++i) {
    const b6o::pixel_oracle o = b6o::oracle_pixel(canonical_samples(g, order, i), c.p());
    ASSERT_EQ(o.sel.v, b6o::verdict::determined) << i;
    expect_clip_pixel(*res, i, o, "pixel " + std::to_string(i));
  }
}

// ---------------------------------------------------------------- T-M3B-C02-02

// Configurazioni sui tre insiemi di C02-02 (n ∈ {11, 15, 31}, outlier standard, seed 2026110962).
struct c02_02_config {
  const char* tag;
  clip c;  // min_kept = 0: ⌈2n/3⌉
  bool support_incomplete_expected;
};

std::vector<c02_02_config> c02_02_configs() {
  return {
      {"A una iterazione", {3, 3, 1, 2}, false},
      {"B più iterazioni", {3, 3, 5, 2}, false},
      {"C asimmetrica κl=2.5 κh=4", {2.5, 4, 5, 2}, false},
      {"D asimmetrica κl=4 κh=2.5", {4, 2.5, 3, 2}, false},
      {"E clip_min_kept raggiunto", {1.5, 1.5, 5, 0}, true},
  };
}

struct c02_02_set {
  std::size_t n;
  group g;
};

std::vector<c02_02_set> c02_02_sets() {
  const sc::gaussian_cases cases = sc::c02_02_cases();
  const std::vector<sc::frames> x = sc::realize_cases(cases);
  std::vector<c02_02_set> out;
  for (std::size_t c = 0; c < cases.sets.size(); ++c)
    out.push_back({cases.n[c], group_of_frames(x[c], cases.sets[c].width, cases.sets[c].height)});
  return out;
}

// Appartenenza osservata da M (proprietà dell'oracle, indipendente dal prodotto): per ogni pixel deciso e valido,
// l'insieme K con il campione più vicino alla soglia (sui μ e s di K) aggiunto o tolto ha l'intervallo di M della regola
// I disgiunto da quello di K. Un prodotto che sbagliasse quel campione cadrebbe quindi fuori dalla tolleranza.
void expect_membership_identifiable(const group& g, const clip_run& cr, const clip& c, const std::string& what) {
  std::size_t checked = 0, overlapping = 0;
  for (std::size_t i = 0; i < cr.oracle.size(); ++i) {
    const b6o::pixel_oracle& o = cr.oracle[i];
    if (o.sel.v != b6o::verdict::determined || o.sel.support_incomplete) continue;
    const std::vector<double> x = canonical_samples(g, cr.order, i);
    ld sum = 0, ss = 0;
    std::size_t m = 0;
    for (std::size_t f = 0; f < x.size(); ++f)
      if (!o.sel.rejected[f]) {
        sum += x[f];
        ++m;
      }
    const ld mu = sum / static_cast<ld>(m);
    for (std::size_t f = 0; f < x.size(); ++f)
      if (!o.sel.rejected[f]) ss += (x[f] - mu) * (x[f] - mu);
    const ld s = std::sqrt(ss / static_cast<ld>(m - 1));
    std::size_t near = 0;
    ld best = std::numeric_limits<ld>::infinity();
    for (std::size_t f = 0; f < x.size(); ++f) {
      const ld d = std::min(std::fabs((x[f] - mu) - c.kh * s), std::fabs((mu - x[f]) - c.kl * s));
      if (d < best) {
        best = d;
        near = f;
      }
    }
    std::vector<char> toggled = o.sel.rejected;
    toggled[near] = toggled[near] ? 0 : 1;
    if (std::all_of(toggled.begin(), toggled.end(), [](char b) { return b != 0; })) continue;
    bool ex_k = false, ex_t = false;
    const b6o::ivl a = b6o::kept_mean_interval(x, o.sel.rejected, ex_k);
    const b6o::ivl b = b6o::kept_mean_interval(x, toggled, ex_t);
    ++checked;
    if (ex_k || ex_t || !(b.hi < a.lo || b.lo > a.hi)) ++overlapping;
  }
  std::cout << what << ": appartenenza identificabile su " << checked << " pixel, intervalli non separati " << overlapping << "\n";
  EXPECT_EQ(overlapping, 0u) << what;
}

// Con n = 11 e κ = 3 (A, B) l'oracle non rifiuta nulla: per Samuelson |x − μ| ≤ 3.015 s, e un outlier di 12σ fra
// dieci campioni gaussiani alza s quanto basta a restare sotto 3s. La precondizione «pixel con rifiuti» vale quindi
// sui tre insiemi insieme, non per n.
void run_c02_02(const c02_02_config& cfg) {
  std::size_t with_rejection = 0;
  for (const c02_02_set& s : c02_02_sets()) {
    clip c = cfg.c;
    if (c.min_kept == 0) c.min_kept = (2 * s.n + 2) / 3;
    ASSERT_FALSE(b6o::inert(s.n, c.kl, c.kh));
    const std::string what = std::string("C02-02 ") + cfg.tag + " n=" + std::to_string(s.n);
    SCOPED_TRACE(what + " " + c.name());
    const clip_run cr = run_clip(s.g, c);
    print_stats(what, cr.st);
    // Precondizioni sull'oracle (fixture e parametri, indipendenti dal prodotto).
    with_rejection += cr.st.with_rejection;
    if (cfg.support_incomplete_expected) {
      EXPECT_GT(cr.st.incomplete, 0u) << "clip_min_kept mai raggiunto";
      EXPECT_LT(cr.st.incomplete, cr.st.pixels - cr.st.undetermined - cr.st.rule_i_excluded) << "nessun pixel valido";
    } else {
      ASSERT_EQ(cr.st.incomplete, 0u) << "support_incomplete inatteso nella configurazione";
    }
    check_run(cr, what);
    expect_membership_identifiable(s.g, cr, c, what);
    if (!cr.res) continue;
    master_recipe r = recipe_of(s.g);
    apply_clip(c, r);
    const std::optional<std::vector<std::uint64_t>> got = report_rejected(*cr.res, r, cr.order);
    ASSERT_TRUE(got.has_value()) << "rejected_samples assente o non intero nei frame del report";
    const std::uint64_t slack = cr.st.undetermined + cr.st.rule_i_excluded;
    for (std::size_t f = 0; f < s.n; ++f) {
      if (slack == 0) {
        EXPECT_EQ((*got)[f], cr.st.rejected[f]) << "rejected_samples del frame canonico " << f;
      } else {
        EXPECT_GE((*got)[f], cr.st.rejected[f]) << "rejected_samples del frame canonico " << f;
        EXPECT_LE((*got)[f], cr.st.rejected[f] + slack) << "rejected_samples del frame canonico " << f;
      }
    }
  }
  EXPECT_GT(with_rejection, 0u) << "nessun pixel valido con rifiuti";
}

TEST(MasterB6, T_M3B_C02_02_A_one_iteration_rejected_sets_and_counts_per_frame) { run_c02_02(c02_02_configs()[0]); }
TEST(MasterB6, T_M3B_C02_02_B_several_iterations_rejected_sets_and_counts_per_frame) { run_c02_02(c02_02_configs()[1]); }
TEST(MasterB6, T_M3B_C02_02_C_asymmetric_low_threshold_rejected_sets_and_counts_per_frame) { run_c02_02(c02_02_configs()[2]); }
TEST(MasterB6, T_M3B_C02_02_D_asymmetric_high_threshold_rejected_sets_and_counts_per_frame) { run_c02_02(c02_02_configs()[3]); }
TEST(MasterB6, T_M3B_C02_02_E_clip_min_kept_reached_sets_support_incomplete_exactly_on_oracle_pixels) {
  run_c02_02(c02_02_configs()[4]);
}

// ---------------------------------------------------------------- T-M3B-C02-02: clip_max_iter senza tetto

// M3b-lacune, «Correzioni della review di B6 (giro 1)»: ogni iterazione che non chiude toglie almeno un campione, quindi
// dopo n iterazioni la procedura è finita; un `clip_max_iter` dichiarato oltre n (anche 1e20 o 2⁶⁴, interi f64 ammessi
// dalla validazione) dà il risultato dell'oracle con n.
TEST(MasterB6, T_M3B_C02_02_clip_max_iter_beyond_n_gives_the_result_with_n_iterations) {
  struct source {
    std::string name;
    group g;
    clip c;
  };
  std::vector<source> sources;
  {
    const dyadic_case dc = dyadic_cases()[2];  // n = 5, κ = 1, fino a tre iterazioni
    sources.push_back({dc.name, group_of(dc.px, static_cast<std::uint32_t>(dc.px.size()), 1), {1, 1, 5, 2}});
  }
  for (const c02_02_set& s : c02_02_sets())
    if (s.n == 31) sources.push_back({"C02-02 n=31 κ=1.5", s.g, {1.5, 1.5, 31, 2}});
  for (const source& src : sources) {
    const std::size_t n = src.g.frames.size();
    ASSERT_EQ(src.c.iter, n);
    master_recipe base = recipe_of(src.g);
    apply_clip(src.c, base);
    const std::vector<std::size_t> order = canonical(base);
    std::vector<b6o::pixel_oracle> oracle;
    std::size_t most_iterations = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(src.g.w) * src.g.h; ++i) {
      oracle.push_back(b6o::oracle_pixel(canonical_samples(src.g, order, i), src.c.p()));
      if (oracle.back().sel.v == b6o::verdict::determined) most_iterations = std::max(most_iterations, oracle.back().sel.iterations);
    }
    ASSERT_GE(most_iterations, 3u) << src.name << ": caso senza più iterazioni effettive";
    for (const double max_iter : {static_cast<double>(n), static_cast<double>(n + 3), 1e20, 0x1p64}) {
      SCOPED_TRACE(src.name + " clip_max_iter=" + std::to_string(max_iter));
      master_recipe r = base;
      r.clip_max_iter = max_iter;
      const std::optional<master_result> res = combine_or_fail(r, images_of(src.g), "sigma_clip");
      if (!res) continue;
      for (std::size_t i = 0; i < oracle.size(); ++i) {
        if (oracle[i].sel.v != b6o::verdict::determined) continue;
        expect_clip_pixel(*res, i, oracle[i], "pixel " + std::to_string(i));
      }
    }
  }
}

// ---------------------------------------------------------------- T-M3B-C02-02: conteggi e result_nonfinite

// M3b-lacune, «Correzioni della review di B6 (giro 1)»: gli R di un pixel si contano solo se la procedura dà un esito
// (valore valido o support_incomplete); un pixel che diventa result_nonfinite, per un'operazione di §b.6 o per la
// conversione al tipo di uscita, non contribuisce a `rejected_samples`.
std::vector<std::uint64_t> total_rejected(const master_result& res, const master_recipe& r, const std::vector<std::size_t>& order) {
  const std::optional<std::vector<std::uint64_t>> got = report_rejected(res, r, order);
  if (!got) {
    ADD_FAILURE() << "rejected_samples assente o non intero nei frame del report";
    return {};
  }
  return *got;
}

// (a) Pixel 0: alla prima iterazione 1 è rifiutato; sui tenuti {0, 2⁻⁵²⁰, −2⁻⁵²⁰} μ = 0 e (x − μ)·(x − μ) = 2⁻¹⁰⁴⁰ è
// subnormale: result_nonfinite dopo un R applicato. Pixel 1 e 2: casi dyadic decisi, i soli che contano.
TEST(MasterB6, T_M3B_C02_02_rejections_of_a_pixel_made_result_nonfinite_by_the_kept_statistics_are_not_counted) {
  const clip c{1, 1, 2, 2};
  const std::vector<std::vector<double>> px = {{0, 0x1p-520, -0x1p-520, 1}, {1001, 1016, 1001, 1004}, {1000, 984, 1032, 1002}};
  const group g = group_of(px, 3, 1);
  master_recipe r = recipe_of(g);
  apply_clip(c, r);
  const std::vector<std::size_t> order = canonical(r);
  // Precondizioni: il pixel 0 rifiuta il campione 1 alla prima iterazione (decisione degli intervalli) e poi la catena
  // esatta incontra il subnormale; i pixel 1 e 2 sono decisi e validi.
  {
    b6o::params one = c.p();
    one.max_iter = 1;
    const std::vector<double> x0 = canonical_samples(g, order, 0);
    const b6o::selection s1 = b6o::select_interval(x0, one);
    ASSERT_EQ(s1.v, b6o::verdict::determined);
    ASSERT_EQ(std::count(s1.rejected.begin(), s1.rejected.end(), 1), 1);
    ASSERT_TRUE(b6o::kept_mean_plane(x0, s1.rejected).ex.nonfinite);
  }
  std::vector<std::uint64_t> want(4, 0);
  std::vector<b6o::pixel_oracle> oracle(3);
  for (std::size_t i = 1; i < 3; ++i) {
    oracle[i] = b6o::oracle_pixel(canonical_samples(g, order, i), c.p());
    ASSERT_EQ(oracle[i].sel.v, b6o::verdict::determined) << i;
    ASSERT_FALSE(oracle[i].sel.support_incomplete) << i;
    for (std::size_t f = 0; f < 4; ++f) want[f] += static_cast<std::uint64_t>(oracle[i].sel.rejected[f]);
  }
  const std::optional<master_result> res = combine_or_fail(r, images_of(g), "sigma_clip");
  if (!res) return;
  EXPECT_EQ(res->output.invalid.bits[0], invalid_reason::result_nonfinite);
  EXPECT_TRUE(std::isnan(sample(res->output, 0)) && std::isnan(plane(res->output, 0)));
  for (std::size_t i = 1; i < 3; ++i) expect_clip_pixel(*res, i, oracle[i], "pixel " + std::to_string(i));
  EXPECT_EQ(total_rejected(*res, r, order), want);
}

// (b) Pixel 0 = pixel 1 · 2¹²⁰: stessa selezione (1000 rifiutato), M = 1032 · 2¹²⁰ ≈ 1.37e39 valido in f64 e oltre il
// massimo di f32. Con uscita f64 contano entrambi; con uscita f32 il pixel 0 è result_nonfinite e non conta.
TEST(MasterB6, T_M3B_C02_02_rejections_of_a_pixel_made_result_nonfinite_by_the_f32_conversion_are_not_counted) {
  const clip c{1, 1, 1, 2};
  const double k = 0x1p120;
  const std::vector<std::vector<double>> px = {{1032 * k, 1000 * k, 1032 * k, 1032 * k}, {1032, 1000, 1032, 1032}};
  const group g = group_of(px, 2, 1);
  master_recipe r = recipe_of(g);
  apply_clip(c, r);
  const std::vector<std::size_t> order = canonical(r);
  std::vector<std::uint64_t> one_pixel(4, 0);
  std::vector<b6o::pixel_oracle> oracle;
  for (std::size_t i = 0; i < 2; ++i) {
    oracle.push_back(b6o::oracle_pixel(canonical_samples(g, order, i), c.p()));
    ASSERT_EQ(oracle[i].sel.v, b6o::verdict::determined) << i;
    ASSERT_TRUE(oracle[i].has_values) << i;
  }
  ASSERT_EQ(oracle[0].sel.rejected, oracle[1].sel.rejected);
  ASSERT_GT(b5o::approx(oracle[0].val.ex.m), static_cast<ld>(std::numeric_limits<float>::max()));
  for (std::size_t f = 0; f < 4; ++f) one_pixel[f] = static_cast<std::uint64_t>(oracle[1].sel.rejected[f]);
  std::vector<std::uint64_t> two_pixels(4, 0);
  for (std::size_t f = 0; f < 4; ++f) two_pixels[f] = 2 * one_pixel[f];

  r.output_type = sample_type::f64;
  const std::optional<master_result> r64 = combine_or_fail(r, images_of(g), "f64");
  r.output_type = sample_type::f32;
  const std::optional<master_result> r32 = combine_or_fail(r, images_of(g), "f32");
  if (!r64 || !r32) return;
  for (std::size_t i = 0; i < 2; ++i) expect_clip_pixel(*r64, i, oracle[i], "f64 pixel " + std::to_string(i));
  EXPECT_EQ(total_rejected(*r64, r, order), two_pixels) << "f64";
  EXPECT_EQ(r32->output.invalid.bits[0], invalid_reason::result_nonfinite);
  EXPECT_TRUE(std::isnan(sample(r32->output, 0)) && std::isnan(plane(r32->output, 0)));
  expect_clip_pixel(*r32, 1, oracle[1], "f32 pixel 1");
  EXPECT_EQ(total_rejected(*r32, r, order), one_pixel) << "f32: il pixel result_nonfinite non conta";
}

// ---------------------------------------------------------------- T-M3B-C02-02: Samuelson

struct inert_case {
  std::string name;
  std::size_t n;
  double kl, kh;
};

// (n − 1)² ≤ κ² n per entrambe le soglie, in aritmetica esatta sui f64: inerte.
std::vector<inert_case> inert_cases() {
  return {
      {"n=10 κ=3", 10, 3, 3},
      {"n=4 κ=1.5 (uguaglianza di Samuelson)", 4, 1.5, 1.5},
      {"n=10 κl=2.9 κh=3", 10, 2.9, 3},
      {"n=9 κ = successore f64 di 8/3", 9, 0x1.5555555555556p+1, 0x1.5555555555556p+1},
  };
}

// Almeno una disuguaglianza vale in aritmetica esatta: ammesso. Le ultime tre righe sono inerti se il confronto è
// fatto con prodotti arrotondati in f64 (fl(fl(κ·κ)·n) ≥ (n − 1)²): il controllo deve essere esatto.
std::vector<inert_case> admitted_cases() {
  return {
      {"n=11 κ=3", 11, 3, 3},
      {"n=10 κl=2.8 κh=3", 10, 2.8, 3},
      {"n=10 κl=3 κh=2.8", 10, 3, 2.8},
      {"n=4 κl = predecessore f64 di 1.5, κh=1.5", 4, 0x1.7ffffffffffffp+0, 1.5},
      {"n=9 κ = predecessore f64 di 8/3", 9, 0x1.5555555555555p+1, 0x1.5555555555555p+1},
      {"n=9 κl = pred. di 8/3, κh = succ. di 8/3", 9, 0x1.5555555555555p+1, 0x1.5555555555556p+1},
      {"n=25 κ = f64 di 4.8 (sotto 24/5)", 25, 4.8, 4.8},
  };
}

group small_group(std::size_t n) {
  std::vector<std::vector<double>> px(8, std::vector<double>(n));
  for (std::size_t i = 0; i < 8; ++i)
    for (std::size_t f = 0; f < n; ++f) px[i][f] = 1000 + static_cast<double>((3 * i + 7 * f) % 13);
  px[1][0] = 1200;  // un outlier
  return group_of(px, 4, 2);
}

std::optional<double> number_of(const std::string& v) {
  char* end = nullptr;
  const double d = std::strtod(v.c_str(), &end);
  if (v.empty() || end == nullptr || *end != '\0') return std::nullopt;
  return d;
}

// Contesto (M3b-lacune, «Decisioni successive (B6)»): `n`, `clip_kappa_low`, `clip_kappa_high` (testo che rilegge lo
// stesso f64) e `limite` = (n − 1)², intero esatto.
std::optional<std::string> context_of(const stop_error& e, const std::string& key) {
  for (const auto& [k, v] : e.context())
    if (k == key) return v;
  return std::nullopt;
}

void expect_inert_context(const stop_error& e, const inert_case& ic) {
  EXPECT_EQ(context_of(e, "n"), std::optional<std::string>(std::to_string(ic.n))) << e.what();
  EXPECT_EQ(context_of(e, "limite"), std::optional<std::string>(std::to_string((ic.n - 1) * (ic.n - 1)))) << e.what();
  for (const auto& [key, want] : {std::pair<const char*, double>{"clip_kappa_low", ic.kl}, {"clip_kappa_high", ic.kh}}) {
    const std::optional<std::string> v = context_of(e, key);
    ASSERT_TRUE(v.has_value()) << key << " assente dal contesto: " << e.what();
    const std::optional<double> d = number_of(*v);
    ASSERT_TRUE(d.has_value()) << key << " non numerico: " << *v;
    EXPECT_EQ(std::bit_cast<std::uint64_t>(*d), std::bit_cast<std::uint64_t>(want)) << key << " = " << *v;
  }
}

TEST(MasterB6, T_M3B_C02_02_oracle_samuelson_rule_on_the_declared_cases) {
  for (const inert_case& ic : inert_cases()) EXPECT_TRUE(b6o::inert(ic.n, ic.kl, ic.kh)) << ic.name;
  for (const inert_case& ic : admitted_cases()) EXPECT_FALSE(b6o::inert(ic.n, ic.kl, ic.kh)) << ic.name;
  // Le righe di confine sbagliano con i prodotti arrotondati: il caso discrimina il confronto esatto.
  for (std::size_t k = 4; k < 7; ++k) {
    const inert_case ic = admitted_cases()[k];
    const double nd = static_cast<double>(ic.n), lhs = (nd - 1) * (nd - 1);
    EXPECT_FALSE(lhs > ic.kl * ic.kl * nd || lhs > ic.kh * ic.kh * nd) << ic.name;
  }
}

TEST(MasterB6, T_M3B_C02_02_inert_recipes_stop_with_sigma_clip_inert_before_any_read) {
  for (const inert_case& ic : inert_cases()) {
    SCOPED_TRACE(ic.name);
    const group g = small_group(ic.n);
    master_recipe r = recipe_of(g);
    apply_clip({ic.kl, ic.kh, 5, 2}, r);
    // Validazione della ricetta: parametri, n e digest, nessun accesso ai file.
    try {
      validate_master_recipe(r);
      ADD_FAILURE() << "validate_master_recipe: nessuno stop";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.category(), error_category::invalid_input) << e.what();
      EXPECT_EQ(e.code(), "master.param.sigma_clip_inert") << e.what();
      expect_inert_context(e, ic);
    }
    EXPECT_STOP(make_master_node(r), error_category::invalid_input, "master.param.sigma_clip_inert");
    EXPECT_STOP(combine_master(r, images_of(g)), error_category::invalid_input, "master.param.sigma_clip_inert");
  }
}

TEST(MasterB6, T_M3B_C02_02_admitted_recipes_are_combined) {
  for (const inert_case& ic : admitted_cases()) {
    SCOPED_TRACE(ic.name);
    const group g = small_group(ic.n);
    master_recipe r = recipe_of(g);
    apply_clip({ic.kl, ic.kh, 5, 2}, r);
    const std::optional<master_result> res = combine_or_fail(r, images_of(g), ic.name);
    if (!res) continue;
    const std::vector<std::size_t> order = canonical(r);
    for (std::size_t i = 0; i < 8; ++i) {
      const b6o::pixel_oracle o = b6o::oracle_pixel(canonical_samples(g, order, i), {ic.kl, ic.kh, 5, 2});
      if (o.sel.v != b6o::verdict::determined) continue;
      expect_clip_pixel(*res, i, o, "pixel " + std::to_string(i));
    }
  }
}

// ---------------------------------------------------------------- percorso del job: inerte prima di ogni lettura

struct job_probe {
  std::optional<std::string> setup_error;
  std::optional<std::string> code;
  std::optional<error_category> category;
  int faults = 0;
  bool frames_read = false;
  bool primary = false;
};

job_probe probe_job(const fs::path& root, const master_recipe& node_recipe, const master_recipe& job_recipe) {
  job_probe out;
  auto store = sqlite_dag_store::create(root);
  std::optional<node_spec> node;
  try {
    node = make_master_node(node_recipe);
  } catch (const stop_error& e) {
    out.setup_error = e.what();
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
  memory_pool pool(256 * (std::uint64_t{1} << 20));
  try {
    const job_outcome res = run_master_quiet(*store, "master", job_recipe, pool, o);
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
  out.primary = by_role(*store, node->key()).count("primary") != 0;
  return out;
}

// Il nodo nasce da una ricetta ammessa (n = 10, κ = 2); il job riceve la stessa ricetta con κ = 3, inerte: stop
// prima di ogni lettura, nessun artefatto, nessun lavoro sullo store oltre lo stop di parametro di riferimento.
TEST(MasterB6, T_M3B_C02_02_inert_stop_in_the_job_reads_nothing_and_does_no_extra_store_work) {
  const fs::path dir = fresh_dir("b6-inert-job");
  const group g = small_group(10);
  master_recipe good = recipe_of(g);
  write_frames(g, good, dir, "f");
  apply_clip({2, 2, 5, 2}, good);

  master_recipe base_stop = good;
  base_stop.combine.reset();
  const job_probe baseline = probe_job(dir / "store-baseline", good, base_stop);
  ASSERT_FALSE(baseline.setup_error.has_value()) << *baseline.setup_error;
  ASSERT_EQ(baseline.code, std::optional<std::string>("master.param.combine"));
  EXPECT_FALSE(baseline.frames_read);

  const job_probe control = probe_job(dir / "store-control", good, good);
  ASSERT_FALSE(control.setup_error.has_value()) << *control.setup_error;
  EXPECT_FALSE(control.code.has_value()) << control.code.value_or("");
  EXPECT_TRUE(control.frames_read);
  EXPECT_TRUE(control.primary);
  EXPECT_GT(control.faults, baseline.faults) << "contatore del fault_injector non collegato";

  master_recipe inert = good;
  inert.clip_kappa_low = 3;
  inert.clip_kappa_high = 3;
  const job_probe p = probe_job(dir / "store-inert", good, inert);
  ASSERT_FALSE(p.setup_error.has_value()) << *p.setup_error;
  EXPECT_EQ(p.code, std::optional<std::string>("master.param.sigma_clip_inert"));
  EXPECT_EQ(p.category, std::optional<error_category>(error_category::invalid_input));
  EXPECT_FALSE(p.frames_read);
  EXPECT_FALSE(p.primary);
  EXPECT_EQ(p.faults, baseline.faults);
}

// Percorso del job a blocchi con più task (C-02.b §b.8, §b.11): con worker e tile_rows diversi master, piano, maschera
// (compreso support_incomplete) e rejected_samples sono quelli del calcolo in memoria e dell'oracle.
TEST(MasterB6, T_M3B_C02_02_job_with_several_workers_and_tile_rows_matches_memory_and_oracle) {
  const fs::path dir = fresh_dir("b6-job-tiles");
  c02_02_set src{0, {}};
  for (const c02_02_set& s : c02_02_sets())
    if (s.n == 15) src = s;
  ASSERT_EQ(src.n, 15u);
  const clip c{1.5, 1.5, 5, 10};
  const clip_run cr = run_clip(src.g, c);
  ASSERT_GT(cr.st.incomplete, 0u);
  ASSERT_GT(cr.st.with_rejection, 0u);
  ASSERT_EQ(cr.st.undetermined + cr.st.rule_i_excluded, 0u);
  if (!cr.res) return;
  check_run(cr, "memoria");
  master_recipe r = recipe_of(src.g);
  write_frames(src.g, r, dir, "f");
  apply_clip(c, r);
  const std::vector<std::uint64_t> memory_counts = total_rejected(*cr.res, r, cr.order);
  EXPECT_EQ(memory_counts, cr.st.rejected);
  struct variant {
    unsigned workers;
    std::uint32_t rows;
  };
  for (const variant v : {variant{1, 32}, variant{4, 1}, variant{4, 7}, variant{2, 5}}) {
    SCOPED_TRACE(std::to_string(v.workers) + " worker, " + std::to_string(v.rows) + " righe");
    const std::string tag = std::to_string(v.workers) + "-" + std::to_string(v.rows);
    auto store = sqlite_dag_store::create(dir / ("store-" + tag));
    const node_spec node = make_master_node(r);
    store->put_node(node);
    store->create_slot("master", node.key());
    master_job_options o;
    o.resources.worker_count = v.workers;
    o.resources.tile_rows = v.rows;
    memory_pool pool(256 * (std::uint64_t{1} << 20));
    const job_outcome out = run_master_quiet(*store, "master", r, pool, o);
    ASSERT_EQ(out.exit_code, 0) << (out.stop ? out.stop->what() : "");
    const auto roles = by_role(*store, node.key());
    ASSERT_TRUE(roles.count("primary") && roles.count("report"));
    const image decoded = decode_fits(read_artifact(*store, roles.at("primary")), cal::big_budget);
    EXPECT_TRUE(same_bits(decoded, cr.res->output));
    const std::vector<std::uint8_t> rep_bytes = read_artifact(*store, roles.at("report"));
    master_result from_job;
    from_job.report = parse_json(std::string(rep_bytes.begin(), rep_bytes.end()));
    EXPECT_EQ(total_rejected(from_job, r, cr.order), cr.st.rejected);
  }
}

// ---------------------------------------------------------------- T-M3B-C02-15: support_incomplete

// Pixel con meno di `clip_min_kept` tenuti: bit `support_incomplete`, campione e piano NaN; gli altri pixel restano
// quelli di `sigma_clip` (nessun cambio di metodo, nessuno stop, descrittore invariato).
TEST(MasterB6, T_M3B_C02_15_support_incomplete_pixel_without_method_change) {
  EXPECT_EQ(invalid_reason::support_incomplete, 1u << 9);  // ADR-03.b §b.6
  const clip c{1, 1, 1, 4};
  const std::vector<std::vector<double>> px =
      rotations({{984, 1002, 1008, 1008, 1032}, {1032, 1008, 1004, 1002, 1002}, {1000, 1000, 1000, 1000, 1000}});
  const group g = group_of(px, static_cast<std::uint32_t>(px.size()), 1);
  const clip_run cr = run_clip(g, c);
  ASSERT_GT(cr.st.incomplete, 0u);
  ASSERT_GT(cr.st.with_rejection, 0u);
  ASSERT_EQ(cr.st.undetermined + cr.st.rule_i_excluded, 0u);
  if (!cr.res) return;
  check_run(cr, "C02-15");
  // rejected_samples: i campioni dell'ultimo R, che porta |S| sotto clip_min_kept, si contano (M3b-lacune, B6).
  master_recipe r = recipe_of(g);
  apply_clip(c, r);
  const std::optional<std::vector<std::uint64_t>> got = report_rejected(*cr.res, r, cr.order);
  ASSERT_TRUE(got.has_value()) << "rejected_samples assente o non intero nei frame del report";
  EXPECT_EQ(*got, cr.st.rejected);
  const json& rep = cr.res->report;
  const json& inc = cal::at(rep, {"invalid_counts", "support_incomplete"});
  ASSERT_EQ(inc.type(), json::kind::integer) << "invalid_counts.support_incomplete assente o non intero";
  EXPECT_EQ(static_cast<std::size_t>(inc.as_integer()), cr.st.incomplete);
  EXPECT_EQ(cal::at(rep, {"descriptor", "combine_method"}).as_string(), "mean");
  EXPECT_EQ(cal::at(rep, {"descriptor", "rejection"}).as_string(), "sigma_clip");
  EXPECT_EQ(cal::at(rep, {"descriptor", "variance_estimator"}).as_string(), "frozen_selection");
  EXPECT_TRUE(operator_output_invariants_hold(cr.res->output));
}

// ---------------------------------------------------------------- T-M3B-C02-06: parte binaria su una pila piccola

TEST(MasterB6, T_M3B_C02_06_binary_capability_estimator_and_warning) {
  using ml = measure_limit;
  const std::size_t n = 4;
  group g = make_group(master_role::dark, 4, 2, n);
  for (std::size_t f = 0; f < n; ++f)
    for (std::size_t i = 0; i < 8; ++i) g.frames[f].v[i] = 200 + static_cast<double>((f * 5 + i) % 7);
  std::vector<image> frames = images_of(g);
  const std::vector<std::vector<ml>> codes = {{ml::calib_offset_unknown, ml::samples_interpolated},
                                              {ml::calib_omitted_bias_subtraction},
                                              {ml::calib_offset_unknown, ml::calib_assumption_decoder_black},
                                              {}};
  std::vector<const image*> ptrs;
  for (std::size_t f = 0; f < n; ++f) {
    frames[f].measure = measure_capability{codes[f]};
    ptrs.push_back(&frames[f]);
  }
  const measure_capability expected = merge_measure(ptrs, {ml::variance_fixed_weight_approximation});
  master_recipe r = recipe_of(g);
  apply_clip({1, 1, 5, 2}, r);
  const std::optional<master_result> res = combine_or_fail(r, frames, "sigma_clip");
  if (!res) return;
  ASSERT_TRUE(res->output.measure.has_value());
  EXPECT_EQ(*res->output.measure, expected);
  const json& var = cal::at(res->report, {"variance"});
  EXPECT_EQ(cal::at(var, {"variance_estimator"}).as_string(), "frozen_selection");
  EXPECT_EQ(cal::at(res->report, {"descriptor", "variance_estimator"}).as_string(), "frozen_selection");
  EXPECT_NE(var.find("plane"), nullptr) << "campo plane assente";
  EXPECT_EQ(cal::at(var, {"variance_cv_gaussian_homoscedastic"}).type(), json::kind::null);
  EXPECT_EQ(cal::at(var, {"variance_cv_null_reason"}).as_string(), "not_derived");
  EXPECT_NE(std::find(res->warnings.begin(), res->warnings.end(), "frozen_selection_approximation"), res->warnings.end());
  EXPECT_TRUE(has_string(cal::at(res->report, {"warnings"}), "frozen_selection_approximation"));
}

// ---------------------------------------------------------------- T-M3B-C02-06 (protocollo §3.3): caratterizzazione

double seconds_since(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// Il più grande divisore di K con n · R ≤ 1000 (memoria per chiamata ≤ circa 32 MB di campioni).
std::size_t batch_of(std::size_t K, std::size_t n) {
  for (std::size_t r = std::min<std::size_t>(K, 1000 / n); r >= 1; --r)
    if (K % r == 0) return r;
  return 1;
}

// R realizzazioni consecutive del flusso stanno una sotto l'altra in un'immagine 64 × 64R; il pixel p della
// realizzazione k è il pixel p dell'immagine 64 × 64 del protocollo. Il calcolo del master è per pixel (§b.8).
void run_c02_06(std::size_t n, std::size_t iter) {
  const auto t0 = std::chrono::steady_clock::now();
  const std::size_t K = sc::c02_06_K, P = 4096, lot = 100;
  ASSERT_EQ(K % lot, 0u);
  const sc::frame_set set = sc::c02_06_set(n);
  ASSERT_EQ(set.pixels(), P);
  const clip c{3, 3, iter, 2};
  ASSERT_FALSE(b6o::inert(n, c.kl, c.kh));
  const std::size_t R = batch_of(K, n);
  group tpl_group = make_group(master_role::dark, 64, static_cast<std::uint32_t>(64 * R), n);
  const std::vector<image> tpl = images_of(tpl_group);
  master_recipe recipe = recipe_of(tpl_group);
  recipe.output_type = sample_type::f64;
  apply_clip(c, recipe);
  std::vector<std::vector<double>> samples(n, std::vector<double>(R * P));
  sc::gauss_stream gs(sc::c02_06_seed(n));
  sc::frames buf;
  // Accumulatori: piano su tutte le realizzazioni; scarti dei master per pixel, totali e per lotto di 100.
  ld sumV = 0;
  std::size_t NV = 0, incomplete = 0, other_invalid = 0;
  std::vector<ld> sy(P, 0), syy(P, 0), ly(P, 0), lyy(P, 0);
  std::vector<std::size_t> kp(P, 0), lk(P, 0);
  ld lot_sumV = 0;
  std::size_t lot_NV = 0;
  std::vector<double> lot_ratio;
  bool binary_checked = false;
  double t_draw = 0, t_combine = 0;
  for (std::size_t b = 0; b < K / R; ++b) {
    const auto td = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < R; ++r) {
      sc::realize_into(set, gs, buf);
      for (std::size_t f = 0; f < n; ++f) std::copy(buf[f].begin(), buf[f].end(), samples[f].begin() + static_cast<std::ptrdiff_t>(r * P));
    }
    t_draw += seconds_since(td);
    std::vector<image> imgs = tpl;
    for (std::size_t f = 0; f < n; ++f) std::memcpy(imgs[f].data<double>(), samples[f].data(), samples[f].size() * sizeof(double));
    const auto tc = std::chrono::steady_clock::now();
    std::optional<master_result> res;
    try {
      res = combine_master(recipe, std::move(imgs));
    } catch (const stop_error& e) {
      std::cout << "T-M3B-C02-06 n=" << n << " iter=" << iter << ": prima pila realizzata in " << t_draw << " s, stima delle sole estrazioni per "
                << K << " realizzazioni " << t_draw * static_cast<double>(K / R) << " s\n";
      FAIL() << "stop " << e.code() << " — " << e.what();
    }
    t_combine += seconds_since(tc);
    if (!binary_checked) {
      binary_checked = true;
      ASSERT_TRUE(res->output.measure.has_value());
      const auto& lim = res->output.measure->limits;
      EXPECT_EQ(std::count(lim.begin(), lim.end(), measure_limit::variance_fixed_weight_approximation), 1);
      EXPECT_EQ(cal::at(res->report, {"descriptor", "variance_estimator"}).as_string(), "frozen_selection");
      EXPECT_NE(std::find(res->warnings.begin(), res->warnings.end(), "frozen_selection_approximation"), res->warnings.end());
    }
    for (std::size_t r = 0; r < R; ++r) {
      for (std::size_t p = 0; p < P; ++p) {
        const std::size_t i = r * P + p;
        const std::uint16_t bits = res->output.invalid.bits[i];
        if (bits == kSupportIncomplete) {
          ++incomplete;
          continue;
        }
        if (bits != 0) {
          ++other_invalid;
          continue;
        }
        const double M = sample(res->output, i), V = plane(res->output, i);
        sumV += V;
        ++NV;
        lot_sumV += V;
        ++lot_NV;
        const ld y = static_cast<ld>(M) - static_cast<ld>(set.mean[0][p]);
        sy[p] += y;
        syy[p] += y * y;
        ++kp[p];
        ly[p] += y;
        lyy[p] += y * y;
        ++lk[p];
      }
      // Fine di un lotto di 100 realizzazioni consecutive: rapporto del lotto.
      if ((b * R + r + 1) % lot == 0) {
        ld ss = 0;
        std::size_t nu = 0;
        for (std::size_t p = 0; p < P; ++p) {
          if (lk[p] >= 2) {
            ss += lyy[p] - ly[p] * ly[p] / static_cast<ld>(lk[p]);
            nu += lk[p] - 1;
          }
          ly[p] = lyy[p] = 0;
          lk[p] = 0;
        }
        lot_ratio.push_back(static_cast<double>((lot_sumV / static_cast<ld>(lot_NV)) / (ss / static_cast<ld>(nu))));
        lot_sumV = 0;
        lot_NV = 0;
      }
    }
  }
  ld ss = 0;
  std::size_t nu = 0;
  for (std::size_t p = 0; p < P; ++p) {
    if (kp[p] < 2) continue;
    ss += syy[p] - sy[p] * sy[p] / static_cast<ld>(kp[p]);
    nu += kp[p] - 1;
  }
  const ld meanV = sumV / static_cast<ld>(NV), s2pool = ss / static_cast<ld>(nu);
  const double ratio = static_cast<double>(meanV / s2pool);
  double lm = 0;
  for (double x : lot_ratio) lm += x;
  lm /= static_cast<double>(lot_ratio.size());
  double lv = 0;
  for (double x : lot_ratio) lv += (x - lm) * (x - lm);
  const double se = std::sqrt(lv / static_cast<double>(lot_ratio.size() - 1) / static_cast<double>(lot_ratio.size()));
  std::cout << std::setprecision(8) << "T-M3B-C02-06 n=" << n << " clip_max_iter=" << iter << " R " << R << ": E[V]/Var(M) stimato "
            << ratio << " (media dei piani " << static_cast<double>(meanV) << ", varianza raggruppata dei master "
            << static_cast<double>(s2pool) << ", nu " << nu << "), errore standard su " << lot_ratio.size() << " lotti "
            << se << "; support_incomplete " << incomplete << ", altri invalidi " << other_invalid << "; estrazioni "
            << t_draw << " s, combinazione " << t_combine << " s, durata " << seconds_since(t0) << " s\n";
  EXPECT_EQ(other_invalid, 0u);
  EXPECT_EQ(lot_ratio.size(), K / lot);
}

TEST(MasterB6Slow, T_M3B_C02_06_n11_iter1) { run_c02_06(11, 1); }
TEST(MasterB6Slow, T_M3B_C02_06_n11_iter5) { run_c02_06(11, 5); }
TEST(MasterB6Slow, T_M3B_C02_06_n21_iter1) { run_c02_06(21, 1); }
TEST(MasterB6Slow, T_M3B_C02_06_n21_iter5) { run_c02_06(21, 5); }
TEST(MasterB6Slow, T_M3B_C02_06_n51_iter1) { run_c02_06(51, 1); }
TEST(MasterB6Slow, T_M3B_C02_06_n51_iter5) { run_c02_06(51, 5); }

}  // namespace
