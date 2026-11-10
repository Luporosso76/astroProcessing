#pragma once
// Oracle dei test di B6 di M3.b (C-02.b §b.1, §b.6, §b.7): `mean` + `sigma_clip`. Scritto da chi non scrive il
// prodotto; nessun valore atteso viene dal prodotto.
//
// Strumenti (riusa catena esatta e regola I di `b5_oracle.hpp`):
// - `inert`: regola di Samuelson di C-02.b §b.1, confronti `(n − 1)² > κ² n` in razionali esatti sui κ f64 dichiarati.
// - `select_exact`: la procedura di §b.1 in razionali esatti, con la sequenza di §b.6. Vale come previsione delle
//   decisioni del prodotto solo se ogni operazione dà un valore rappresentabile in f64 (`exact`), radice compresa:
//   allora i valori del prodotto sono quelli esatti e ogni confronto ha l'esito esatto, uguaglianze comprese.
// - `select_interval`: la stessa procedura con la regola I (ADR-17.b §b.10) in long double arrotondato verso l'esterno,
//   come in `b5_oracle.hpp` (protocollo §1, versione 3). Un confronto è deciso se gli intervalli sono separati:
//   `a > b` vero se `a.lo > b.hi`, falso se `a.hi ≤ b.lo`; altrimenti il pixel è indeterminato ed esce dalla misura
//   (C-02.b §b.6, «Errore d'arrotondamento»). Un «oppure» con un ramo vero è deciso anche se l'altro non lo è.
// - `kept_mean_plane`: M = (Σ_{f∈K} x_f)/m e V = (Σ_{f∈K} (x_f − M)² / (m − 1)) / m nell'ordine canonico, esatti e
//   con la regola I.
// La radice in long double è `sqrtl` (IEEE, arrotondamento corretto a 64 bit): l'allargamento (u + 2⁻⁶⁰)|·| copre
// anche questo passo, come gli altri arrotondamenti in long double della regola I.
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include "b5_oracle.hpp"

namespace b6o {

using b5o::ivl;
using b5o::ld;
using b5o::rule_i;
using exact::rat;

struct params {
  double kl = 0, kh = 0;  // clip_kappa_low, clip_kappa_high
  std::size_t max_iter = 0, min_kept = 0;
};

// C-02.b §b.1: la ricetta è inerte se nessuna delle due disuguaglianze vale.
inline bool admits(std::size_t n, double kappa) {
  const rat lhs = exact::from_u(static_cast<std::uint64_t>((n - 1) * (n - 1)));
  const rat k = exact::from_double(kappa);
  return exact::cmp(lhs, k * k * exact::from_u(n)) > 0;
}
inline bool inert(std::size_t n, double kl, double kh) { return !admits(n, kl) && !admits(n, kh); }

enum class verdict { determined, undetermined, rule_i_excluded };

struct selection {
  verdict v = verdict::determined;
  bool support_incomplete = false;
  std::vector<char> rejected;  // per posizione canonica
  std::size_t iterations = 0;  // iterazioni eseguite
};

inline bool same_decisions(const selection& a, const selection& b) {
  return a.support_incomplete == b.support_incomplete && a.rejected == b.rejected && a.iterations == b.iterations;
}

// ---------------------------------------------------------------- razionali esatti

inline rat exact_sqrt(b5o::chain& c, const rat& v) {
  if (exact::is_zero(v)) return v;
  const auto d = static_cast<double>(std::sqrt(b5o::approx(v)));
  const rat r = exact::from_double(d);
  if (exact::cmp(r * r, v) != 0) c.exact = false;
  return r;
}

struct exact_selection {
  selection sel;
  bool exact = true;
  bool nonfinite = false;
};

inline exact_selection select_exact(const std::vector<double>& x, const params& p) {
  using b5o::R;
  using b5o::RU;
  b5o::chain c;
  exact_selection out;
  const std::size_t n = x.size();
  out.sel.rejected.assign(n, 0);
  std::vector<std::size_t> S(n);
  for (std::size_t f = 0; f < n; ++f) S[f] = f;
  for (std::size_t t = 1; t <= p.max_iter; ++t) {
    out.sel.iterations = t;
    const std::size_t m = S.size();
    rat sum = R(x[S[0]]);
    for (std::size_t j = 1; j < m; ++j) sum = c.add(sum, R(x[S[j]]));
    const rat mu = c.div(sum, RU(m));
    rat ss;
    for (std::size_t j = 0; j < m; ++j) {
      const rat d = c.sub(R(x[S[j]]), mu);
      const rat q = c.mul(d, d);
      ss = j == 0 ? q : c.add(ss, q);
    }
    const rat s = exact_sqrt(c, c.div(ss, RU(m - 1)));
    const rat th = c.mul(R(p.kh), s), tl = c.mul(R(p.kl), s);
    // Sequenza non esatta: le decisioni esatte non prevedono quelle del prodotto, e i razionali crescono a ogni passo.
    if (!c.exact) break;
    std::vector<std::size_t> keep;
    bool any = false;
    for (std::size_t f : S) {
      const rat up = c.sub(R(x[f]), mu), dn = c.sub(mu, R(x[f]));
      if (exact::cmp(up, th) > 0 || exact::cmp(dn, tl) > 0) {
        out.sel.rejected[f] = 1;
        any = true;
      } else {
        keep.push_back(f);
      }
    }
    if (!any) break;
    S = keep;
    if (S.size() < p.min_kept) {
      out.sel.support_incomplete = true;
      break;
    }
  }
  out.exact = c.exact;
  out.nonfinite = c.nonfinite;
  return out;
}

// ---------------------------------------------------------------- regola I

inline ivl sqrt_interval(rule_i& r, const ivl& a) {
  if (a.lo < 0) {
    r.excluded = true;
    return {0, 0};
  }
  return r.widen(std::sqrt(a.lo), std::sqrt(a.hi));
}

// `a > b`: 1 vero, 0 falso, −1 non determinato dagli intervalli.
inline int greater(const ivl& a, const ivl& b) {
  if (a.lo > b.hi) return 1;
  if (a.hi <= b.lo) return 0;
  return -1;
}

inline selection select_interval(const std::vector<double>& x, const params& p) {
  rule_i r;
  selection out;
  const std::size_t n = x.size();
  out.rejected.assign(n, 0);
  std::vector<std::size_t> S(n);
  for (std::size_t f = 0; f < n; ++f) S[f] = f;
  for (std::size_t t = 1; t <= p.max_iter; ++t) {
    out.iterations = t;
    const std::size_t m = S.size();
    ivl sum = rule_i::pt(x[S[0]]);
    for (std::size_t j = 1; j < m; ++j) sum = r.add(sum, rule_i::pt(x[S[j]]));
    const ivl mu = r.div(sum, rule_i::pt(static_cast<double>(m)));
    ivl ss;
    for (std::size_t j = 0; j < m; ++j) {
      const ivl q = r.sqr(r.sub(rule_i::pt(x[S[j]]), mu));
      ss = j == 0 ? q : r.add(ss, q);
    }
    const ivl s = sqrt_interval(r, r.div(ss, rule_i::pt(static_cast<double>(m - 1))));
    const ivl th = r.mul(rule_i::pt(p.kh), s), tl = r.mul(rule_i::pt(p.kl), s);
    std::vector<std::size_t> keep;
    bool any = false;
    for (std::size_t f : S) {
      const int up = greater(r.sub(rule_i::pt(x[f]), mu), th);
      const int dn = greater(r.sub(mu, rule_i::pt(x[f])), tl);
      if (up == 1 || dn == 1) {
        out.rejected[f] = 1;
        any = true;
      } else if (up == 0 && dn == 0) {
        keep.push_back(f);
      } else {
        out.v = verdict::undetermined;
        return out;
      }
    }
    if (r.excluded) {
      out.v = verdict::rule_i_excluded;
      return out;
    }
    if (!any) break;
    S = keep;
    if (S.size() < p.min_kept) {
      out.support_incomplete = true;
      break;
    }
  }
  if (r.excluded) out.v = verdict::rule_i_excluded;
  return out;
}

// ---------------------------------------------------------------- master e piano dei tenuti

struct kept_values {
  b5o::exact_pixel ex;  // valori esatti; exact_m, exact_v: sequenza esatta (tolleranza 0)
  ivl m, v;             // regola I
  bool excluded = false;
};

inline kept_values kept_mean_plane(const std::vector<double>& x, const std::vector<char>& rejected) {
  using b5o::R;
  using b5o::RU;
  std::vector<std::size_t> K;
  for (std::size_t f = 0; f < x.size(); ++f)
    if (!rejected[f]) K.push_back(f);
  const std::size_t m = K.size();
  if (m < 2) throw std::invalid_argument("meno di due campioni tenuti");
  b5o::chain cm, cv;
  rule_i r;
  kept_values out;
  rat sum = R(x[K[0]]);
  ivl isum = rule_i::pt(x[K[0]]);
  for (std::size_t j = 1; j < m; ++j) {
    sum = cm.add(sum, R(x[K[j]]));
    isum = r.add(isum, rule_i::pt(x[K[j]]));
  }
  out.ex.m = cm.div(sum, RU(m));
  out.m = r.div(isum, rule_i::pt(static_cast<double>(m)));
  rat ss;
  ivl iss;
  for (std::size_t j = 0; j < m; ++j) {
    const rat d = cv.sub(R(x[K[j]]), out.ex.m);
    const rat q = cv.mul(d, d);
    ss = j == 0 ? q : cv.add(ss, q);
    const ivl iq = r.sqr(r.sub(rule_i::pt(x[K[j]]), out.m));
    iss = j == 0 ? iq : r.add(iss, iq);
  }
  out.ex.v = cv.div(cv.div(ss, RU(m - 1)), RU(m));
  out.v = r.div(r.div(iss, rule_i::pt(static_cast<double>(m - 1))), rule_i::pt(static_cast<double>(m)));
  out.ex.exact_m = cm.exact;
  out.ex.exact_v = cm.exact && cv.exact;  // V usa M: esatto solo se lo è anche M
  out.ex.nonfinite = cm.nonfinite || cv.nonfinite;
  out.excluded = r.excluded;
  return out;
}

// Solo l'intervallo della regola I di M = (Σ_{f∈K} x_f)/m, per insiemi qualsiasi con almeno un campione.
inline ivl kept_mean_interval(const std::vector<double>& x, const std::vector<char>& rejected, bool& excluded) {
  rule_i r;
  ivl sum;
  std::size_t m = 0;
  for (std::size_t f = 0; f < x.size(); ++f) {
    if (rejected[f]) continue;
    sum = m == 0 ? rule_i::pt(x[f]) : r.add(sum, rule_i::pt(x[f]));
    ++m;
  }
  if (m == 0) throw std::invalid_argument("insieme vuoto");
  const ivl out = r.div(sum, rule_i::pt(static_cast<double>(m)));
  excluded = r.excluded;
  return out;
}

// ---------------------------------------------------------------- previsione per pixel

struct pixel_oracle {
  selection sel;
  bool from_exact = false;      // decisioni dai razionali esatti (sequenza esatta)
  bool nonfinite = false;       // la catena esatta ha incontrato un risultato zero o subnormale
  bool has_values = false;      // determinata e non support_incomplete
  kept_values val;
};

// Le due previsioni, quando entrambe valgono, devono coincidere: il contrario è un difetto dell'oracle.
inline pixel_oracle oracle_pixel(const std::vector<double>& x, const params& p) {
  const exact_selection es = select_exact(x, p);
  const selection is = select_interval(x, p);
  pixel_oracle out;
  out.nonfinite = es.nonfinite;
  if (es.exact && !es.nonfinite) {
    if (is.v == verdict::determined && !same_decisions(es.sel, is)) throw std::logic_error("oracle di sigma_clip incoerente");
    out.sel = es.sel;
    out.from_exact = true;
  } else {
    out.sel = is;
  }
  if (out.sel.v == verdict::determined && !out.sel.support_incomplete) {
    out.val = kept_mean_plane(x, out.sel.rejected);
    out.has_values = !out.val.excluded;
    if (out.val.excluded) out.sel.v = verdict::rule_i_excluded;
  }
  return out;
}

}  // namespace b6o
