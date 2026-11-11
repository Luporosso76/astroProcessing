#pragma once
// Oracle dei test di B9 di M3.b (C-02.b-flat §f.1, §f.6, §f.12): `median_ratio`, valori e piano. Scritto da chi non
// scrive il prodotto; nessun valore atteso viene dal prodotto, salvo `κ̂` (`robust_kappa`), ingresso puntuale dei piani
// `gaussian_reference` come in B5.
//
// Sequenza di §f.6, nell'ordine canonico dei frame e per canale:
//   i_f = 1 / m_f; S = Σ_f i_f; m_H = n / S; r_f = m_H / m_f; per pixel d_f = x_f − offset, t_f = r_f · d_f,
//   y_f = offset + t_f; poi il metodo di C-02.b §b.6 sugli y_f.
// Strumenti:
// - `scales_f64`: le costanti in f64 rieseguite qui con la sequenza fissata (premesse dei casi dyadic: se coincidono
//   con i razionali esatti, ogni r_f del prodotto è esatto);
// - `scales_exact`: costanti in razionali esatti (verità della fixture);
// - `scale_intervals`, `y_interval`: la regola I (ADR-17.b §b.10) di costanti e y_f dagli ingressi esatti (livelli e
//   campioni f64), con gli intervalli in long double di `b5_oracle.hpp`;
// - `method_interval`: la sequenza del metodo sugli intervalli degli y_f. Le statistiche d'ordine sono monotone in ogni
//   argomento, quindi la k-esima ha estremi [k-esimo dei lo, k-esimo degli hi]. `sigma_clip`: la procedura di §b.1 con
//   i confronti decisi solo se gli intervalli sono separati, come `b6_oracle.hpp`; indeterminato altrimenti;
// - `any_subset_mean`: per `sigma_clip` con campioni normalizzati uguali in aritmetica esatta (T-M3B-FLAT-02(b),
//   ν = 0, β ≡ 0) la selezione dipende solo dagli arrotondamenti; M è la media di un sottoinsieme di almeno
//   `clip_min_kept` campioni, quindi sta nell'unione, su m, delle medie di m valori presi nell'inviluppo degli y_f;
// - `method_exact`: la sequenza in razionali esatti sugli y_f f64 (riusa le catene di B5 e B6), per i casi dyadic.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

#include "b5_oracle.hpp"
#include "b6_oracle.hpp"

namespace b9o {

using b5o::ivl;
using b5o::ld;
using b5o::rule_i;
using exact::rat;

// ---------------------------------------------------------------- costanti delle scale

struct scales_f64 {
  std::vector<double> inv, r;
  double S = 0, m_h = 0;
};

// `m` nell'ordine canonico.
inline scales_f64 f64_scales(const std::vector<double>& m) {
  scales_f64 s;
  for (double v : m) s.inv.push_back(1.0 / v);
  s.S = s.inv[0];
  for (std::size_t f = 1; f < m.size(); ++f) s.S = s.S + s.inv[f];
  s.m_h = static_cast<double>(m.size()) / s.S;
  for (double v : m) s.r.push_back(s.m_h / v);
  return s;
}

struct scales_exact {
  std::vector<rat> r;
  rat m_h;
};

inline scales_exact exact_scales(const std::vector<double>& m) {
  scales_exact s;
  rat inv = exact::from_u(0);
  for (double v : m) inv = inv + exact::from_u(1) / exact::from_double(v);
  s.m_h = b5o::red::reduce(exact::from_u(m.size()) / inv);
  for (double v : m) s.r.push_back(b5o::red::reduce(s.m_h / exact::from_double(v)));
  return s;
}

// Le costanti f64 coincidono con i razionali esatti (premessa dei casi dyadic).
inline bool scales_are_exact(const scales_f64& a, const scales_exact& b) {
  if (exact::cmp(exact::from_double(a.m_h), b.m_h) != 0) return false;
  for (std::size_t f = 0; f < a.r.size(); ++f)
    if (exact::cmp(exact::from_double(a.r[f]), b.r[f]) != 0) return false;
  return true;
}

struct scales_ivl {
  bool excluded = false;
  ivl S, m_h;
  std::vector<ivl> r;
};

inline scales_ivl scale_intervals(const std::vector<double>& m) {
  rule_i ri;
  scales_ivl s;
  std::vector<ivl> inv;
  for (double v : m) inv.push_back(ri.div(rule_i::pt(1.0), rule_i::pt(v)));
  s.S = inv[0];
  for (std::size_t f = 1; f < m.size(); ++f) s.S = ri.add(s.S, inv[f]);
  s.m_h = ri.div(rule_i::pt(static_cast<double>(m.size())), s.S);
  for (double v : m) s.r.push_back(ri.div(s.m_h, rule_i::pt(v)));
  s.excluded = ri.excluded;
  return s;
}

// ---------------------------------------------------------------- campioni normalizzati

inline double y_f64(double x, double offset, double r) {
  const double d = x - offset;
  const double t = r * d;
  return offset + t;
}

inline ivl y_interval(rule_i& ri, double x, double offset, const ivl& r) {
  const ivl d = ri.sub(rule_i::pt(x), rule_i::pt(offset));
  const ivl t = ri.mul(r, d);
  return ri.add(rule_i::pt(offset), t);
}

// y_f in razionali esatti con r esatto; `underflow`: t_f = r_f · d_f con operandi non nulli dà zero o un subnormale
// (§f.6, pixel `result_nonfinite`).
struct y_exact {
  rat y;
  bool underflow = false;
};

inline y_exact exact_y(double x, double offset, const rat& r) {
  const rat d = exact::from_double(x) - exact::from_double(offset);
  const rat t = r * d;
  y_exact out;
  out.underflow = !exact::is_zero(d) && !exact::is_zero(r) && (exact::is_zero(t) || b5o::below_normal(t));
  out.y = b5o::red::reduce(exact::from_double(offset) + t);
  return out;
}

// ---------------------------------------------------------------- metodi sugli intervalli degli y_f

enum class method_kind { mean, weighted, median, winsorized, sigma_clip };

struct method_spec {
  method_kind k = method_kind::mean;
  std::vector<double> w;  // canonici, solo `weighted`
  std::size_t g = 0;      // solo `winsorized`
  b6o::params clip;       // solo `sigma_clip`
  double kappa = 0;       // κ̂ di `median` e `winsorized`
};

struct pix_ivl {
  bool excluded = false;            // condizione della regola I non soddisfatta
  bool undetermined = false;        // `sigma_clip`: confronto non deciso dagli intervalli
  bool support_incomplete = false;  // `sigma_clip`: meno di clip_min_kept tenuti
  ivl m, v;
};

inline ivl sum_of(rule_i& ri, const std::vector<ivl>& y, const std::vector<std::size_t>& idx) {
  ivl s = y[idx[0]];
  for (std::size_t j = 1; j < idx.size(); ++j) s = ri.add(s, y[idx[j]]);
  return s;
}

// M = Σ y / m e V = (Σ (y − M)² / (m − 1)) / m sugli indici dati, nell'ordine canonico.
inline void mean_and_plane(rule_i& ri, const std::vector<ivl>& y, const std::vector<std::size_t>& idx, ivl& m, ivl& v) {
  const double k = static_cast<double>(idx.size());
  m = ri.div(sum_of(ri, y, idx), rule_i::pt(k));
  ivl ss;
  for (std::size_t j = 0; j < idx.size(); ++j) {
    const ivl q = ri.sqr(ri.sub(y[idx[j]], m));
    ss = j == 0 ? q : ri.add(ss, q);
  }
  v = ri.div(ri.div(ss, rule_i::pt(k - 1)), rule_i::pt(k));
}

// s² di C-02.a §6 sugli intervalli (due passaggi, ordine canonico).
inline ivl s2_direct(rule_i& ri, const std::vector<ivl>& y) {
  std::vector<std::size_t> all(y.size());
  for (std::size_t f = 0; f < y.size(); ++f) all[f] = f;
  const ivl mean = ri.div(sum_of(ri, y, all), rule_i::pt(static_cast<double>(y.size())));
  ivl ss;
  for (std::size_t f = 0; f < y.size(); ++f) {
    const ivl q = ri.sqr(ri.sub(y[f], mean));
    ss = f == 0 ? q : ri.add(ss, q);
  }
  return ri.div(ss, rule_i::pt(static_cast<double>(y.size() - 1)));
}

// k-esima statistica d'ordine (0-based) degli intervalli: [k-esimo dei lo, k-esimo degli hi].
struct order_stats {
  std::vector<ld> lo, hi;
  explicit order_stats(const std::vector<ivl>& y) {
    for (const ivl& v : y) {
      lo.push_back(v.lo);
      hi.push_back(v.hi);
    }
    std::sort(lo.begin(), lo.end());
    std::sort(hi.begin(), hi.end());
  }
  ivl at(std::size_t k) const { return {lo[k], hi[k]}; }
};

inline int greater(const ivl& a, const ivl& b) { return b6o::greater(a, b); }

inline pix_ivl clip_interval(const std::vector<ivl>& y, const b6o::params& p) {
  rule_i ri;
  pix_ivl out;
  const std::size_t n = y.size();
  std::vector<std::size_t> S(n);
  for (std::size_t f = 0; f < n; ++f) S[f] = f;
  for (std::size_t t = 1; t <= p.max_iter; ++t) {
    const std::size_t m = S.size();
    const ivl mu = ri.div(sum_of(ri, y, S), rule_i::pt(static_cast<double>(m)));
    ivl ss;
    for (std::size_t j = 0; j < m; ++j) {
      const ivl q = ri.sqr(ri.sub(y[S[j]], mu));
      ss = j == 0 ? q : ri.add(ss, q);
    }
    const ivl s = b6o::sqrt_interval(ri, ri.div(ss, rule_i::pt(static_cast<double>(m - 1))));
    const ivl th = ri.mul(rule_i::pt(p.kh), s), tl = ri.mul(rule_i::pt(p.kl), s);
    std::vector<std::size_t> keep;
    bool any = false;
    for (std::size_t f : S) {
      const int up = greater(ri.sub(y[f], mu), th);
      const int dn = greater(ri.sub(mu, y[f]), tl);
      if (up == 1 || dn == 1) {
        any = true;
      } else if (up == 0 && dn == 0) {
        keep.push_back(f);
      } else {
        out.undetermined = true;
        return out;
      }
    }
    if (ri.excluded) {
      out.excluded = true;
      return out;
    }
    if (!any) break;
    S = keep;
    if (S.size() < p.min_kept) {
      out.support_incomplete = true;
      return out;
    }
  }
  mean_and_plane(ri, y, S, out.m, out.v);
  out.excluded = ri.excluded;
  return out;
}

// `y` nell'ordine canonico.
inline pix_ivl method_interval(const method_spec& ms, const std::vector<ivl>& y) {
  pix_ivl out;
  rule_i ri;
  const std::size_t n = y.size();
  switch (ms.k) {
    case method_kind::mean: {
      std::vector<std::size_t> all(n);
      for (std::size_t f = 0; f < n; ++f) all[f] = f;
      mean_and_plane(ri, y, all, out.m, out.v);
      break;
    }
    case method_kind::weighted: {
      const b5o::weight_intervals k = b5o::weight_constant_intervals(ms.w);
      ivl acc = ri.mul(rule_i::pt(ms.w[0]), y[0]);
      for (std::size_t f = 1; f < n; ++f) acc = ri.add(acc, ri.mul(rule_i::pt(ms.w[f]), y[f]));
      out.m = ri.div(acc, k.W);
      for (std::size_t f = 0; f < n; ++f) {
        const ivl t = ri.mul(k.c[f], ri.sqr(ri.sub(y[f], out.m)));
        out.v = f == 0 ? t : ri.add(out.v, t);
      }
      if (k.excluded) ri.excluded = true;
      break;
    }
    case method_kind::median:
    case method_kind::winsorized: {
      const order_stats z(y);
      if (ms.k == method_kind::median) {
        out.m = n % 2 == 1 ? z.at(n / 2) : ri.div(ri.add(z.at(n / 2 - 1), z.at(n / 2)), rule_i::pt(2.0));
      } else {
        const std::size_t g = ms.g;
        if (g < 1 || n < 2 * g + 1) throw std::invalid_argument("winsorized fuori dominio");
        const auto v = [&](std::size_t i) {  // i = 1 … n
          if (i <= g) return z.at(g);
          if (i > n - g) return z.at(n - g - 1);
          return z.at(i - 1);
        };
        ivl s = v(1);
        for (std::size_t i = 2; i <= n; ++i) s = ri.add(s, v(i));
        out.m = ri.div(s, rule_i::pt(static_cast<double>(n)));
      }
      out.v = ri.mul(rule_i::pt(ms.kappa), s2_direct(ri, y));
      break;
    }
    case method_kind::sigma_clip: return clip_interval(y, ms.clip);
  }
  out.excluded = ri.excluded;
  return out;
}

// Unione, su m in [min_kept, n], delle medie di m valori presi nell'inviluppo degli y_f (vedi intestazione).
inline ivl any_subset_mean(const std::vector<ivl>& y, std::size_t min_kept, bool& excluded) {
  ivl hull = y[0];
  for (const ivl& v : y) {
    hull.lo = std::min(hull.lo, v.lo);
    hull.hi = std::max(hull.hi, v.hi);
  }
  rule_i ri;
  ivl out{0, 0};
  bool first = true;
  ivl s = hull;
  for (std::size_t m = 1; m <= y.size(); ++m) {
    if (m > 1) s = ri.add(s, hull);
    if (m < min_kept) continue;
    const ivl mean = ri.div(s, rule_i::pt(static_cast<double>(m)));
    if (first) out = mean;
    out.lo = std::min(out.lo, mean.lo);
    out.hi = std::max(out.hi, mean.hi);
    first = false;
  }
  excluded = ri.excluded;
  return out;
}

// Come `any_subset_mean`, anche per il piano: per ogni m, M_m nell'inviluppo H, V = (Σ (y − M_m)² / (m − 1)) / m con
// ogni y in H; unione su m.
inline pix_ivl any_subset_mean_plane(const std::vector<ivl>& y, std::size_t min_kept) {
  ivl hull = y[0];
  for (const ivl& v : y) {
    hull.lo = std::min(hull.lo, v.lo);
    hull.hi = std::max(hull.hi, v.hi);
  }
  rule_i ri;
  pix_ivl out;
  bool first = true;
  ivl s = hull;
  for (std::size_t m = 1; m <= y.size(); ++m) {
    if (m > 1) s = ri.add(s, hull);
    if (m < min_kept || m < 2) continue;
    const ivl mean = ri.div(s, rule_i::pt(static_cast<double>(m)));
    const ivl q = ri.sqr(ri.sub(hull, mean));
    ivl ss = q;
    for (std::size_t j = 1; j < m; ++j) ss = ri.add(ss, q);
    const ivl v = ri.div(ri.div(ss, rule_i::pt(static_cast<double>(m - 1))), rule_i::pt(static_cast<double>(m)));
    if (first) {
      out.m = mean;
      out.v = v;
    }
    out.m = {std::min(out.m.lo, mean.lo), std::max(out.m.hi, mean.hi)};
    out.v = {std::min(out.v.lo, v.lo), std::max(out.v.hi, v.hi)};
    first = false;
  }
  out.excluded = ri.excluded;
  return out;
}

// ---------------------------------------------------------------- catena esatta sugli y_f (casi dyadic)

struct pix_exact {
  bool determined = true;  // `sigma_clip`: decisioni previste dai razionali esatti
  bool support_incomplete = false;
  b5o::exact_pixel ex;
};

// `y` sono gli y_f f64 nell'ordine canonico, uguali ai razionali esatti (premessa del chiamante).
inline pix_exact method_exact(const method_spec& ms, const std::vector<double>& y) {
  pix_exact out;
  switch (ms.k) {
    case method_kind::mean: {
      b5o::chain cm, cv;
      rat s = b5o::R(y[0]);
      for (std::size_t f = 1; f < y.size(); ++f) s = cm.add(s, b5o::R(y[f]));
      out.ex.m = cm.div(s, b5o::RU(y.size()));
      out.ex.v = cv.div(b5o::exact_s2(cv, y), b5o::RU(y.size()));
      out.ex.exact_m = cm.exact;
      out.ex.exact_v = cv.exact;
      out.ex.nonfinite = cm.nonfinite || cv.nonfinite;
      break;
    }
    case method_kind::weighted: out.ex = b5o::exact_weighted(y, ms.w); break;
    case method_kind::median: out.ex = b5o::exact_median(y, ms.kappa); break;
    case method_kind::winsorized: out.ex = b5o::exact_winsorized(y, ms.g, ms.kappa); break;
    case method_kind::sigma_clip: {
      const b6o::pixel_oracle po = b6o::oracle_pixel(y, ms.clip);
      out.determined = po.from_exact;
      out.support_incomplete = po.sel.support_incomplete;
      out.ex = po.val.ex;
      out.ex.nonfinite = po.nonfinite;
      if (!po.has_values) out.ex.exact_m = out.ex.exact_v = false;
      break;
    }
  }
  return out;
}

// ---------------------------------------------------------------- quantili (C-02.b-flat §f.9)

// P(Z > t) per la normale standard.
inline ld upper_normal(ld t) { return 0.5L * std::erfc(t / std::sqrt(2.0L)); }

// z con P(Z > z) = p, per bisezione su erfc (errore relativo sulla coda ≤ 10⁻¹⁰).
inline ld normal_upper_quantile(ld p) {
  ld lo = 0, hi = 40;
  for (int i = 0; i < 400; ++i) {
    const ld mid = (lo + hi) / 2;
    if (upper_normal(mid) > p) lo = mid;
    else hi = mid;
  }
  return hi;
}

// P(X ≥ k), X ~ Bin(N, 1/2), somma esatta dei termini in long double dal termine k verso la coda.
inline ld binomial_upper_tail(std::uint64_t N, std::uint64_t k) {
  if (k == 0) return 1;
  if (k > N) return 0;
  const ld n = static_cast<ld>(N), kk = static_cast<ld>(k);
  ld term = std::exp(std::lgamma(n + 1) - std::lgamma(kk + 1) - std::lgamma(n - kk + 1) - n * std::log(2.0L));
  ld sum = 0;
  for (std::uint64_t j = k; j <= N; ++j) {
    sum += term;
    if (term < sum * 1e-22L) break;
    term = term * static_cast<ld>(N - j) / static_cast<ld>(j + 1);
  }
  return sum;
}

// Il più piccolo q con P(X > q) ≤ p, X ~ Bin(N, 1/2).
inline std::uint64_t binomial_upper_quantile(std::uint64_t N, ld p) {
  std::uint64_t lo = N / 2, hi = N;  // P(X > N) = 0 ≤ p
  while (lo < hi) {
    const std::uint64_t mid = lo + (hi - lo) / 2;
    if (binomial_upper_tail(N, mid + 1) <= p) hi = mid;
    else lo = mid + 1;
  }
  return lo;
}

}  // namespace b9o
