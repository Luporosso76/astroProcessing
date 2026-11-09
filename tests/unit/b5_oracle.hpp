#pragma once
// Oracle dei test di B5 di M3.b (C-02.b §b.6, §b.8, §b.12): `single`, `weighted_mean`, `median`, `winsorized`.
// Scritto da chi non scrive il prodotto; nessun valore atteso viene dal prodotto, salvo `κ̂` (`robust_kappa`), che la
// regola I prende come ingresso puntuale dei piani `gaussian_reference` (C-02.b §b.6, «Errore d'arrotondamento»).
//
// Tre strumenti:
// - `chain`: la sequenza di C-02.b §b.6 in aritmetica razionale esatta, con il controllo che ogni operazione dia un
//   valore rappresentabile in f64 (casi dyadic: tolleranza 0) e con la regola di rappresentabilità di C-02.a §6.3
//   (prodotto o quoziente di operandi non nulli che dà zero o un subnormale: pixel `result_nonfinite`).
// - `ivl`: la regola I (ADR-17.b §b.10) sulla stessa sequenza. Vincolo: gli estremi sono long double (64 bit di
//   mantissa) allargati di (u + 2⁻⁶⁰)|·| per operazione invece di u|·|; il margine 2⁻⁶⁰ copre i tre arrotondamenti in
//   long double di ogni passo (estremo, prodotto, sottrazione: ≤ 3·2⁻⁶⁴ relativo), quindi l'intervallo calcolato
//   contiene quello razionale della regola I e la sua ampiezza lo supera al più di 2⁻⁶⁰/2⁻⁵³ = 1/128 per operazione. I razionali esatti con il fattore
//   [1 − u, 1 + u] a ogni passo non sono praticabili sulle 10⁸ realizzazioni-pixel di T-M3B-C02-04.
//   Il prodotto di un valore per sé stesso (`d · d`, `a_f · a_f`) è il quadrato dell'intervallo: i due operandi sono
//   lo stesso valore f64.
// - `moment4`: μ_4 = E[T(Z)⁴] per T-M3B-C02-04(b), stessa costruzione e stesse griglie di `kappa_oracle.hpp`.
// - `discrete_median`: distribuzione esatta della mediana di gaussiane arrotondate all'intero (T-M3B-C02-07).
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "exact_rational.hpp"
#include "kappa_oracle.hpp"

namespace b5o {

using ld = long double;
using exact::rat;
static_assert(std::numeric_limits<ld>::digits == 64, "la regola I in long double richiede la mantissa a 64 bit");

constexpr double kMinNormal = 0x1p-1022;

// ---------------------------------------------------------------- ordine totale per pixel (C-02.b §b.8)

// z ordinati su (valore, indice canonico); `x` è già nell'ordine canonico. Nessun NaN ammesso.
inline std::vector<double> sorted_total(const std::vector<double>& x) {
  std::vector<std::pair<double, std::size_t>> v(x.size());
  for (std::size_t i = 0; i < x.size(); ++i) {
    if (std::isnan(x[i])) throw std::invalid_argument("NaN nell'ordinamento");
    v[i] = {x[i], i};
  }
  std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) {
    if (a.first < b.first) return true;
    if (b.first < a.first) return false;
    return a.second < b.second;  // −0 == +0: il frame canonico minore prima
  });
  std::vector<double> z(x.size());
  for (std::size_t i = 0; i < x.size(); ++i) z[i] = v[i].first;
  return z;
}

// v_i di winsorized, i = 1 … n (C-02.b §b.1), dai z ordinati.
inline std::vector<double> winsor_values(const std::vector<double>& z, std::size_t g) {
  const std::size_t n = z.size();
  if (g < 1 || n < 2 * g + 1) throw std::invalid_argument("winsorized fuori dominio");
  std::vector<double> v(n);
  for (std::size_t i = 1; i <= n; ++i) {
    if (i <= g) v[i - 1] = z[g];
    else if (i > n - g) v[i - 1] = z[n - g - 1];
    else v[i - 1] = z[i - 1];
  }
  return v;
}

// Riduzione ai minimi termini (MCD binario e divisione esatta): senza, numeratori e denominatori della regola I
// letterale crescono in modo esponenziale con la profondità della sequenza.
namespace red {
using limbs = std::vector<std::uint32_t>;
inline void trim(limbs& a) {
  while (!a.empty() && a.back() == 0) a.pop_back();
}
inline int cmp(const limbs& a, const limbs& b) {
  if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
  for (std::size_t i = a.size(); i-- > 0;) {
    if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
  }
  return 0;
}
inline void sub(limbs& a, const limbs& b) {  // a ≥ b
  std::int64_t borrow = 0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    std::int64_t d = static_cast<std::int64_t>(a[i]) - (i < b.size() ? b[i] : 0) - borrow;
    borrow = d < 0 ? 1 : 0;
    if (borrow) d += std::int64_t{1} << 32;
    a[i] = static_cast<std::uint32_t>(d);
  }
  trim(a);
}
inline void shl1(limbs& a) {
  std::uint32_t carry = 0;
  for (std::uint32_t& x : a) {
    const std::uint32_t next = x >> 31;
    x = (x << 1) | carry;
    carry = next;
  }
  if (carry) a.push_back(carry);
}
inline std::size_t ctz(const limbs& a) {
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i]) return i * 32 + static_cast<std::size_t>(std::countr_zero(a[i]));
  }
  return 0;
}
inline void shr(limbs& a, std::size_t bits) {
  const std::size_t w = bits / 32, b = bits % 32;
  if (w >= a.size()) {
    a.clear();
    return;
  }
  a.erase(a.begin(), a.begin() + static_cast<std::ptrdiff_t>(w));
  if (b) {
    for (std::size_t i = 0; i < a.size(); ++i) a[i] = (a[i] >> b) | (i + 1 < a.size() ? a[i + 1] << (32 - b) : 0);
  }
  trim(a);
}
inline limbs gcd(limbs a, limbs b) {
  if (a.empty()) return b;
  if (b.empty()) return a;
  const std::size_t za = ctz(a), zb = ctz(b), k = std::min(za, zb);
  shr(a, za);
  shr(b, zb);
  for (;;) {
    const int c = cmp(a, b);
    if (c == 0) break;
    if (c > 0) std::swap(a, b);
    sub(b, a);
    shr(b, ctz(b));
  }
  exact::big g;
  g.m = a;
  g = exact::shl(g, static_cast<unsigned>(k));
  return g.m;
}
inline limbs div_exact(const limbs& a, const limbs& g) {
  limbs r, q(a.size(), 0);
  for (std::size_t i = a.size() * 32; i-- > 0;) {
    shl1(r);
    if ((a[i / 32] >> (i % 32)) & 1u) {
      if (r.empty()) r.push_back(1);
      else r[0] |= 1u;
    }
    if (cmp(r, g) >= 0) {
      sub(r, g);
      q[i / 32] |= 1u << (i % 32);
    }
  }
  trim(q);
  return q;
}
inline rat reduce(rat x) {
  if (x.n.zero()) return exact::from_u(0);
  const limbs g = gcd(x.n.m, x.d.m);
  if (!(g.size() == 1 && g[0] == 1)) {
    x.n.m = div_exact(x.n.m, g);
    x.d.m = div_exact(x.d.m, g);
  }
  return x;
}
}  // namespace red

// ---------------------------------------------------------------- catena esatta (casi dyadic)

// Valore approssimato di un razionale con numeratore e denominatore oltre il campo di long double: le tre parole alte
// di ciascuno, l'esponente a parte. Errore relativo ≤ 2⁻⁶³.
inline ld approx(const rat& v) {
  const auto top = [](const exact::big& b, int& e) {
    const std::size_t k = std::min<std::size_t>(3, b.m.size());
    ld m = 0;
    for (std::size_t i = b.m.size(); i-- > b.m.size() - k;) m = m * 4294967296.0L + b.m[i];
    e = static_cast<int>(32 * (b.m.size() - k));
    return m;
  };
  if (v.n.zero()) return 0;
  int en = 0, ed = 0;
  const ld mn = top(v.n, en);
  const ld md = top(v.d, ed);
  const ld r = std::ldexp(mn / md, en - ed);
  return v.n.neg ? -r : r;
}

// Valore esatto di un long double finito.
inline rat from_ld(ld x) {
  if (x == 0) return exact::from_u(0);
  int e = 0;
  const ld frac = std::frexp(x, &e);
  const ld mag = std::ldexp(std::fabs(frac), 64);
  const auto mant = static_cast<std::uint64_t>(mag);
  rat r{exact::from_u64(mant, x < 0), exact::from_u64(1)};
  const int shift = e - 64;
  if (shift >= 0) r.n = exact::shl(r.n, static_cast<unsigned>(shift));
  else r.d = exact::shl(r.d, static_cast<unsigned>(-shift));
  return r;
}

// Valore f64 uguale al razionale, se esiste.
inline std::optional<double> as_double(const rat& r) {
  if (exact::is_zero(r)) return 0.0;
  const auto d = static_cast<double>(approx(r));
  if (!std::isfinite(d)) return std::nullopt;
  if (exact::cmp(exact::from_double(d), r) != 0) return std::nullopt;
  return d;
}

inline bool below_normal(const rat& r) {
  return exact::cmp(exact::abs(r), exact::from_double(kMinNormal)) < 0;
}

struct chain {
  bool exact = true;       // ogni operazione ha dato un valore rappresentabile
  bool nonfinite = false;  // regola di rappresentabilità violata (pixel result_nonfinite)

  rat check(const rat& v) {
    rat r = red::reduce(v);
    if (!as_double(r)) exact = false;
    return r;
  }
  rat add(const rat& a, const rat& b) { return check(a + b); }
  rat sub(const rat& a, const rat& b) { return check(a - b); }
  // Prodotto o quoziente: con operandi non nulli, zero o subnormale è un underflow.
  rat mul(const rat& a, const rat& b) {
    const rat r = a * b;
    if (!exact::is_zero(a) && !exact::is_zero(b) && (exact::is_zero(r) || below_normal(r))) nonfinite = true;
    return check(r);
  }
  rat div(const rat& a, const rat& b) {
    if (exact::is_zero(b)) {
      nonfinite = true;
      return a;
    }
    const rat r = a / b;
    if (!exact::is_zero(a) && (exact::is_zero(r) || below_normal(r))) nonfinite = true;
    return check(r);
  }
};

inline rat R(double x) { return exact::from_double(x); }
inline rat RU(std::uint64_t x) { return exact::from_u(x); }

// Valori esatti della formula sui campioni realizzati; `exact_m`, `exact_v`: ogni operazione della sequenza che porta
// a M (a V) dà un valore rappresentabile, quindi il valore del prodotto è quello esatto (tolleranza 0).
struct exact_pixel {
  bool exact_m = true, exact_v = true;
  bool nonfinite = false;
  rat m, v;
};

// s² di C-02.a §6 (due passaggi, ordine canonico).
inline rat exact_s2(chain& c, const std::vector<double>& x) {
  const std::size_t n = x.size();
  rat s = R(x[0]);
  for (std::size_t f = 1; f < n; ++f) s = c.add(s, R(x[f]));
  const rat mean = c.div(s, RU(n));
  rat ss;
  for (std::size_t f = 0; f < n; ++f) {
    const rat d = c.sub(R(x[f]), mean);
    const rat q = c.mul(d, d);
    ss = f == 0 ? q : c.add(ss, q);
  }
  return c.div(ss, RU(n - 1));
}

inline exact_pixel exact_median(const std::vector<double>& x, double kappa) {
  chain cm, cv;
  const std::vector<double> z = sorted_total(x);
  const std::size_t n = z.size();
  exact_pixel p;
  p.m = n % 2 == 1 ? R(z[n / 2]) : cm.div(cm.add(R(z[n / 2 - 1]), R(z[n / 2])), RU(2));
  p.v = cv.mul(R(kappa), exact_s2(cv, x));
  p.exact_m = cm.exact;
  p.exact_v = cv.exact;
  p.nonfinite = cm.nonfinite || cv.nonfinite;
  return p;
}

inline exact_pixel exact_winsorized(const std::vector<double>& x, std::size_t g, double kappa) {
  chain cm, cv;
  const std::vector<double> v = winsor_values(sorted_total(x), g);
  rat s = R(v[0]);
  for (std::size_t i = 1; i < v.size(); ++i) s = cm.add(s, R(v[i]));
  exact_pixel p;
  p.m = cm.div(s, RU(v.size()));
  p.v = cv.mul(R(kappa), exact_s2(cv, x));
  p.exact_m = cm.exact;
  p.exact_v = cv.exact;
  p.nonfinite = cm.nonfinite || cv.nonfinite;
  return p;
}

// Costanti del nodo di `weighted_mean` nell'ordine canonico (C-02.b §b.6).
struct exact_weights {
  bool exact_W = true;  // la sola somma W (entra in M)
  bool exact = true;
  bool nonfinite = false;
  rat W, T;
  std::vector<rat> a, c;
};

inline exact_weights exact_weight_constants(const std::vector<double>& w) {
  chain cw, ch;
  exact_weights k;
  k.W = R(w[0]);
  for (std::size_t f = 1; f < w.size(); ++f) k.W = cw.add(k.W, R(w[f]));
  k.exact_W = cw.exact;
  std::vector<rat> p(w.size()), u(w.size());
  for (std::size_t f = 0; f < w.size(); ++f) {
    k.a.push_back(ch.div(R(w[f]), k.W));
    p[f] = ch.mul(k.a[f], k.a[f]);
    u[f] = ch.sub(RU(1), ch.mul(RU(2), k.a[f]));
    const rat term = ch.div(p[f], u[f]);
    k.T = f == 0 ? term : ch.add(k.T, term);
  }
  const rat onep = ch.add(RU(1), k.T);
  for (std::size_t f = 0; f < w.size(); ++f) k.c.push_back(ch.div(p[f], ch.mul(u[f], onep)));
  k.exact = cw.exact && ch.exact;
  k.nonfinite = cw.nonfinite || ch.nonfinite;
  return k;
}

// M esatto e V con le costanti esatte (V non è rappresentabile in nessun caso dyadic noto: i c_f non lo sono).
inline exact_pixel exact_weighted(const std::vector<double>& x, const std::vector<double>& w) {
  const exact_weights k = exact_weight_constants(w);
  chain cm, cv;
  rat acc = cm.mul(R(w[0]), R(x[0]));
  for (std::size_t f = 1; f < x.size(); ++f) acc = cm.add(acc, cm.mul(R(w[f]), R(x[f])));
  exact_pixel p;
  p.m = cm.div(acc, k.W);
  rat v;
  for (std::size_t f = 0; f < x.size(); ++f) {
    const rat d = cv.sub(R(x[f]), p.m);
    const rat t = cv.mul(k.c[f], cv.mul(d, d));
    v = f == 0 ? t : cv.add(v, t);
  }
  p.v = v;
  p.exact_m = cm.exact && k.exact_W;
  p.exact_v = cv.exact && cm.exact && k.exact;
  p.nonfinite = cm.nonfinite || cv.nonfinite || k.nonfinite;
  return p;
}

// `single`: x_1 − offset, max(·, 0), / G, R², somma (C-02.b §b.6).
inline exact_pixel exact_single(double x, double offset, double gain, double read_noise) {
  chain c;
  exact_pixel p;
  p.m = R(x);
  const rat t = c.sub(R(x), R(offset));
  const rat pos = exact::cmp(t, RU(0)) > 0 ? t : RU(0);
  const rat shot = c.div(pos, R(gain));
  const rat r2 = c.mul(R(read_noise), R(read_noise));
  p.v = c.add(r2, shot);
  p.exact_v = c.exact;
  p.nonfinite = c.nonfinite;
  return p;
}

// ---------------------------------------------------------------- regola I (intervalli in long double)

constexpr ld kU = 0x1p-53L;
constexpr ld kSlack = 0x1p-60L;
constexpr ld kWiden = kU + kSlack;  // un solo passo: L − (u + 2⁻⁶⁰)|L| ≤ L − u|L| anche con i tre arrotondamenti
constexpr ld kTiny = static_cast<ld>(0x1p-1022);

struct ivl {
  ld lo = 0, hi = 0;
};

// Stato della sequenza: `excluded` se una condizione della regola I non vale (denominatore che contiene 0, estremo
// non nullo nel campo dei subnormali, estremo non finito); la realizzazione esce prima della misura.
struct rule_i {
  bool excluded = false;

  static ivl pt(double x) { return {x, x}; }

  ivl widen(ld L, ld H) {
    const ld aL = std::fabs(L), aH = std::fabs(H);
    if (!(aL <= std::numeric_limits<ld>::max() && aH <= std::numeric_limits<ld>::max()) || (L != 0 && aL < kTiny) ||
        (H != 0 && aH < kTiny)) {
      excluded = true;
    }
    return {L - kWiden * aL, H + kWiden * aH};
  }
  ivl add(const ivl& a, const ivl& b) { return widen(a.lo + b.lo, a.hi + b.hi); }
  ivl sub(const ivl& a, const ivl& b) { return widen(a.lo - b.hi, a.hi - b.lo); }
  ivl mul(const ivl& a, const ivl& b) {
    const ld p[4] = {a.lo * b.lo, a.lo * b.hi, a.hi * b.lo, a.hi * b.hi};
    return widen(*std::min_element(p, p + 4), *std::max_element(p, p + 4));
  }
  ivl sqr(const ivl& a) {
    const ld l2 = a.lo * a.lo, h2 = a.hi * a.hi;
    if (a.lo >= 0) return widen(l2, h2);
    if (a.hi <= 0) return widen(h2, l2);
    return widen(0, std::max(l2, h2));
  }
  ivl div(const ivl& a, const ivl& b) {
    if (b.lo <= 0 && b.hi >= 0) {
      excluded = true;
      return {0, 0};
    }
    const ld q[4] = {a.lo / b.lo, a.lo / b.hi, a.hi / b.lo, a.hi / b.hi};
    return widen(*std::min_element(q, q + 4), *std::max_element(q, q + 4));
  }
  // max(·, 0) è esatto in IEEE: nessun allargamento.
  static ivl max0(const ivl& a) { return {std::max(a.lo, ld{0}), std::max(a.hi, ld{0})}; }
};

// Distanza massima fra l'intervallo e il valore del prodotto; nullopt se il valore è fuori dall'intervallo.
inline std::optional<ld> tau_of(const ivl& i, double v) {
  const ld x = v;
  if (!(x >= i.lo && x <= i.hi)) return std::nullopt;
  return std::max(x - i.lo, i.hi - x);
}

inline ivl s2_interval(rule_i& r, const double* x, std::size_t n) {
  ivl s = rule_i::pt(x[0]);
  for (std::size_t f = 1; f < n; ++f) s = r.add(s, rule_i::pt(x[f]));
  const ivl mean = r.div(s, rule_i::pt(static_cast<double>(n)));
  ivl ss;
  for (std::size_t f = 0; f < n; ++f) {
    const ivl q = r.sqr(r.sub(rule_i::pt(x[f]), mean));
    ss = f == 0 ? q : r.add(ss, q);
  }
  return r.div(ss, rule_i::pt(static_cast<double>(n - 1)));
}

inline ivl s2_interval(rule_i& r, const std::vector<double>& x) { return s2_interval(r, x.data(), x.size()); }

// `z` ordinati secondo §b.8.
inline ivl median_interval(rule_i& r, const double* z, std::size_t n) {
  if (n % 2 == 1) return rule_i::pt(z[n / 2]);
  return r.div(r.add(rule_i::pt(z[n / 2 - 1]), rule_i::pt(z[n / 2])), rule_i::pt(2.0));
}

// Σ v_i per i crescente, poi / n, senza costruire i v_i.
inline ivl winsorized_interval(rule_i& r, const double* z, std::size_t n, std::size_t g) {
  if (g < 1 || n < 2 * g + 1) throw std::invalid_argument("winsorized fuori dominio");
  const auto v = [&](std::size_t i) {  // i = 1 … n
    if (i <= g) return z[g];
    if (i > n - g) return z[n - g - 1];
    return z[i - 1];
  };
  ivl s = rule_i::pt(v(1));
  for (std::size_t i = 2; i <= n; ++i) s = r.add(s, rule_i::pt(v(i)));
  return r.div(s, rule_i::pt(static_cast<double>(n)));
}

// Ordinamento su (valore, indice canonico) in buffer riusati; nessun NaN.
inline void sort_total(const double* x, std::size_t n, std::vector<std::pair<double, std::size_t>>& buf, double* z) {
  buf.resize(n);
  for (std::size_t i = 0; i < n; ++i) buf[i] = {x[i], i};
  std::sort(buf.begin(), buf.end(), [](const auto& a, const auto& b) {
    if (a.first < b.first) return true;
    if (b.first < a.first) return false;
    return a.second < b.second;
  });
  for (std::size_t i = 0; i < n; ++i) z[i] = buf[i].first;
}

inline ivl kappa_plane(rule_i& r, double kappa, const ivl& s2) { return r.mul(rule_i::pt(kappa), s2); }

struct weight_intervals {
  bool excluded = false;
  ivl W;
  std::vector<ivl> c;
};

inline weight_intervals weight_constant_intervals(const std::vector<double>& w) {
  rule_i r;
  weight_intervals k;
  k.W = rule_i::pt(w[0]);
  for (std::size_t f = 1; f < w.size(); ++f) k.W = r.add(k.W, rule_i::pt(w[f]));
  std::vector<ivl> p(w.size()), u(w.size());
  ivl T;
  for (std::size_t f = 0; f < w.size(); ++f) {
    const ivl a = r.div(rule_i::pt(w[f]), k.W);
    p[f] = r.sqr(a);
    u[f] = r.sub(rule_i::pt(1.0), r.mul(rule_i::pt(2.0), a));
    const ivl term = r.div(p[f], u[f]);
    T = f == 0 ? term : r.add(T, term);
  }
  const ivl onep = r.add(rule_i::pt(1.0), T);
  for (std::size_t f = 0; f < w.size(); ++f) k.c.push_back(r.div(p[f], r.mul(u[f], onep)));
  k.excluded = r.excluded;
  return k;
}

struct pixel_intervals {
  ivl m, v;
};

inline pixel_intervals weighted_intervals(rule_i& r, const double* x, std::size_t n, const std::vector<double>& w,
                                          const weight_intervals& k) {
  ivl acc = r.mul(rule_i::pt(w[0]), rule_i::pt(x[0]));
  for (std::size_t f = 1; f < n; ++f) acc = r.add(acc, r.mul(rule_i::pt(w[f]), rule_i::pt(x[f])));
  pixel_intervals p;
  p.m = r.div(acc, k.W);
  for (std::size_t f = 0; f < n; ++f) {
    const ivl t = r.mul(k.c[f], r.sqr(r.sub(rule_i::pt(x[f]), p.m)));
    p.v = f == 0 ? t : r.add(p.v, t);
  }
  if (k.excluded) r.excluded = true;
  return p;
}

inline ivl single_interval(rule_i& r, double x, double offset, double gain, double read_noise) {
  const ivl shot = r.div(rule_i::max0(r.sub(rule_i::pt(x), rule_i::pt(offset))), rule_i::pt(gain));
  const ivl r2 = r.sqr(rule_i::pt(read_noise));
  return r.add(r2, shot);
}

// ---------------------------------------------------------------- regola I in razionali esatti (costanti dei pesi)


// ρ_c di T-M3B-C02-03(a) (protocollo §3.1, §5): ampiezza relativa massima degli intervalli dei c_f secondo la regola I
// letterale (razionali, fattore [1 − u, 1 + u] a ogni operazione), rispetto ai c_f esatti.
struct rat_ivl {
  rat lo, hi;
};

inline rat rat_u() { return rat{exact::from_u64(1), exact::shl(exact::from_u64(1), 53)}; }

inline rat_ivl rwiden(const rat& L, const rat& H) {
  const rat u = rat_u();
  return {red::reduce(L - u * exact::abs(L)), red::reduce(H + u * exact::abs(H))};
}
inline const rat& rmin(const rat& a, const rat& b) { return b < a ? b : a; }
inline const rat& rmax(const rat& a, const rat& b) { return a < b ? b : a; }
inline rat_ivl radd(const rat_ivl& a, const rat_ivl& b) { return rwiden(a.lo + b.lo, a.hi + b.hi); }
inline rat_ivl rsub(const rat_ivl& a, const rat_ivl& b) { return rwiden(a.lo - b.hi, a.hi - b.lo); }
inline rat_ivl rmul(const rat_ivl& a, const rat_ivl& b) {
  const rat p0 = a.lo * b.lo, p1 = a.lo * b.hi, p2 = a.hi * b.lo, p3 = a.hi * b.hi;
  return rwiden(rmin(rmin(p0, p1), rmin(p2, p3)), rmax(rmax(p0, p1), rmax(p2, p3)));
}
inline rat_ivl rdiv(const rat_ivl& a, const rat_ivl& b) {
  if (!(exact::cmp(b.lo, RU(0)) > 0) && !(exact::cmp(b.hi, RU(0)) < 0)) throw std::domain_error("denominatore con 0");
  const rat q0 = a.lo / b.lo, q1 = a.lo / b.hi, q2 = a.hi / b.lo, q3 = a.hi / b.hi;
  return rwiden(rmin(rmin(q0, q1), rmin(q2, q3)), rmax(rmax(q0, q1), rmax(q2, q3)));
}
inline rat_ivl rsqr(const rat_ivl& a) {
  const rat l2 = a.lo * a.lo, h2 = a.hi * a.hi;
  if (exact::cmp(a.lo, RU(0)) >= 0) return rwiden(l2, h2);
  if (exact::cmp(a.hi, RU(0)) <= 0) return rwiden(h2, l2);
  return rwiden(RU(0), rmax(l2, h2));
}

struct rho_c_result {
  std::vector<rat_ivl> c;
  std::vector<rat> c_exact;
  long double rho = 0;
};

// `w` nell'ordine canonico.
inline rho_c_result rho_c_of(const std::vector<double>& w) {
  rho_c_result out;
  const auto P = [](double x) { return rat_ivl{R(x), R(x)}; };
  rat_ivl W = P(w[0]);
  rat We = R(w[0]);
  for (std::size_t f = 1; f < w.size(); ++f) {
    W = radd(W, P(w[f]));
    We = We + R(w[f]);
  }
  std::vector<rat_ivl> p(w.size()), u(w.size());
  std::vector<rat> pe(w.size()), ue(w.size());
  rat_ivl T;
  rat Te;
  for (std::size_t f = 0; f < w.size(); ++f) {
    const rat_ivl a = rdiv(P(w[f]), W);
    const rat ae = red::reduce(R(w[f]) / We);
    p[f] = rsqr(a);
    pe[f] = red::reduce(ae * ae);
    u[f] = rsub(P(1.0), rmul(P(2.0), a));
    ue[f] = red::reduce(RU(1) - RU(2) * ae);
    const rat_ivl term = rdiv(p[f], u[f]);
    T = f == 0 ? term : radd(T, term);
    Te = red::reduce(f == 0 ? pe[f] / ue[f] : Te + pe[f] / ue[f]);
  }
  const rat_ivl onep = radd(P(1.0), T);
  for (std::size_t f = 0; f < w.size(); ++f) {
    out.c.push_back(rdiv(p[f], rmul(u[f], onep)));
    out.c_exact.push_back(red::reduce(pe[f] / (ue[f] * (RU(1) + Te))));
    const long double rel = approx((out.c[f].hi - out.c[f].lo) / out.c_exact[f]);
    out.rho = std::max(out.rho, rel);
  }
  return out;
}

// ---------------------------------------------------------------- μ_4 = E[T(Z)⁴] (T-M3B-C02-04(b))

// Stessa fattorizzazione, stessi pannelli, stesse regole e stessa stima d'errore di kappa_oracle (dimezzamento del
// passo, K21 contro G20). Momenti grezzi della gaussiana troncata ad (a, b) dalla ricorrenza
// m_k = (k − 1) m_(k−2) + (a^(k−1) φ(a) − b^(k−1) φ(b)) / (Φ(b) − Φ(a)), con le differenze scritte senza cancellazione:
// a^(k−1) φ(a) − b^(k−1) φ(b) = a^(k−1) (φ(a) − φ(b)) + (a^(k−1) − b^(k−1)) φ(b).
struct moment_result {
  ld kappa = 0, mu4 = 0;
  ld est_kappa = 0, est_mu4 = 0;  // stime dell'errore relativo
};

struct moment_sums {
  ld h2 = 0, h4 = 0;
};

inline moment_result finish_moments(const moment_sums& kh, const moment_sums& gh, const moment_sums& kh2, ld n) {
  moment_result r;
  r.kappa = kh2.h2 / (n * n);
  r.mu4 = kh2.h4 / (n * n * n * n);
  r.est_kappa = std::max(std::fabs(kh2.h2 - kh.h2), std::fabs(kh.h2 - gh.h2)) / std::fabs(kh2.h2);
  r.est_mu4 = std::max(std::fabs(kh2.h4 - kh.h4), std::fabs(kh.h4 - gh.h4)) / std::fabs(kh2.h4);
  return r;
}

inline moment_result moments_odd_median(std::uint32_t n) {
  using namespace kappa_oracle;
  const ld k = (n + 1) / 2;
  const beta_logit bp(k, k);
  const std::vector<ld> b1 = panels(bp), b2 = halve(b1);
  auto run = [&](const std::vector<node1>& nodes) {
    moment_sums s;
    for (const node1& nd : nodes) {
      const pnode v = p_values(nd.x);
      const ld a2 = v.a * v.a;
      s.h2 += nd.w * a2;
      s.h4 += nd.w * a2 * a2;
    }
    return s;
  };
  return finish_moments(run(nodes_of(bp, b1, k21())), run(nodes_of(bp, b1, g20())), run(nodes_of(bp, b2, k21())), 1.0L);
}

// n ≥ 2g + 2; g = 0 è la media (solo controllo: κ = 1/n, μ_4 = 3/n²).
inline moment_result moments_dirichlet(std::uint32_t n, std::uint32_t g) {
  using namespace kappa_oracle;
  if (n < 2 * g + 2) throw std::domain_error("moments_dirichlet: n < 2g + 2");
  const ld G = g, M = n - 2 * g - 2, N = n;
  const beta_logit bp(G + 1, N - G), bq(M + 1, G + 1);
  const std::vector<ld> bp1 = panels(bp), bp2 = halve(bp1), bq1 = panels(bq), bq2 = halve(bq1);
  auto run = [&](const std::vector<node1>& xs, const std::vector<node1>& ys) {
    moment_sums s;
    std::vector<ld> qv(ys.size()), qcv(ys.size());
    for (std::size_t j = 0; j < ys.size(); ++j) {
      qv[j] = sigmoid(ys[j].x);
      qcv[j] = sigmoid(-ys[j].x);
    }
    for (const node1& xn : xs) {
      const pnode P = p_values(xn.x);
      for (std::size_t j = 0; j < ys.size(); ++j) {
        const ld lo = P.p + P.pc * qv[j], hi = P.pc * qcv[j];
        const normal_point B = inv_Phi(lo, hi);
        const ld a = P.a, b = B.x;
        const ld c = (G + 1) * (a + b);
        ld e1 = 0, e2 = 0, e3 = 0, e4 = 0;  // momenti grezzi di S
        if (M > 0) {
          const ld d = P.pc * qv[j];
          const ld e = -0.5L * (b - a) * (b + a);
          const ld n1 = std::fabs(e) < 0.5L ? -P.phia * std::expm1(e) : P.phia - B.phi;
          const ld n2 = a * n1 + (a - b) * B.phi;
          const ld n3 = a * a * n1 + (a - b) * (a + b) * B.phi;
          const ld n4 = a * a * a * n1 + (a - b) * (a * a + a * b + b * b) * B.phi;
          const ld m1 = n1 / d, m2 = 1.0L + n2 / d, m3 = 2.0L * m1 + n3 / d, m4 = 3.0L * m2 + n4 / d;
          const ld M1 = M - 1, M2 = M - 2, M3 = M - 3;
          e1 = M * m1;
          e2 = M * m2 + M * M1 * m1 * m1;
          e3 = M * m3 + 3 * M * M1 * m2 * m1 + M * M1 * M2 * m1 * m1 * m1;
          e4 = M * m4 + 4 * M * M1 * m3 * m1 + 3 * M * M1 * m2 * m2 + 6 * M * M1 * M2 * m2 * m1 * m1 +
               M * M1 * M2 * M3 * m1 * m1 * m1 * m1;
        }
        const ld c2 = c * c;
        const ld h2 = c2 + 2 * c * e1 + e2;
        const ld h4 = c2 * c2 + 4 * c2 * c * e1 + 6 * c2 * e2 + 4 * c * e3 + e4;
        const ld w = xn.w * ys[j].w;
        s.h2 += w * h2;
        s.h4 += w * h4;
      }
    }
    return s;
  };
  return finish_moments(run(nodes_of(bp, bp1, k21()), nodes_of(bq, bq1, k21())),
                        run(nodes_of(bp, bp1, g20()), nodes_of(bq, bq1, g20())),
                        run(nodes_of(bp, bp2, k21()), nodes_of(bq, bq2, k21())), N);
}

inline moment_result moments_median(std::uint32_t n) { return n % 2 == 1 ? moments_odd_median(n) : moments_dirichlet(n, n / 2 - 1); }
inline moment_result moments_winsorized(std::uint32_t n, std::uint32_t g) {
  return n == 2 * g + 1 ? moments_odd_median(n) : moments_dirichlet(n, g);
}

// ---------------------------------------------------------------- T-M3B-C02-07: mediana di gaussiane arrotondate

// Campione intero j con probabilità Φ((j + 1/2 − μ)/σ) − Φ((j − 1/2 − μ)/σ); mediana a n dispari dalla distribuzione
// esatta della statistica d'ordine discreta; supporto |j − μ| ≤ L. Tutto nel tipo F (long double o double). Momenti
// centrati su μ, senza cancellazione fra valori dell'ordine di 1000.
template <class F>
struct discrete_result {
  F var_median = 0;  // Var(M)
  F var_sample = 0;  // Var(campione intero)
};

template <class F>
F upper_normal(F t) { return F(0.5) * std::erfc(t / std::sqrt(F(2))); }  // P(Z > t)

template <class F>
discrete_result<F> discrete_median(F sigma, std::uint32_t n, F frac, std::int64_t L) {
  if (n % 2 == 0) throw std::domain_error("discrete_median: n pari");
  // μ = 1000 + φ; j − 1000 in [lo, hi], con |j − μ| ≤ L.
  const auto lo = static_cast<std::int64_t>(std::ceil(static_cast<double>(frac) - static_cast<double>(L)));
  const auto hi = static_cast<std::int64_t>(std::floor(static_cast<double>(frac) + static_cast<double>(L)));
  const std::size_t J = static_cast<std::size_t>(hi - lo + 1);
  // Code della cumulata nel punto j + 1/2: F_j = P(X ≤ j), S_j = P(X > j), ciascuna dalla coda diretta.
  std::vector<F> cdf(J), sf(J), p(J);
  for (std::size_t i = 0; i < J; ++i) {
    const F t = (F(lo + static_cast<std::int64_t>(i)) + F(0.5) - frac) / sigma;
    sf[i] = upper_normal(t);
    cdf[i] = upper_normal(-t);
  }
  for (std::size_t i = 0; i < J; ++i) {
    // p_j = F_j − F_(j−1) dalla coda più piccola.
    const F prev_cdf = i == 0 ? upper_normal(-(F(lo) - F(0.5) - frac) / sigma) : cdf[i - 1];
    const F prev_sf = i == 0 ? upper_normal((F(lo) - F(0.5) - frac) / sigma) : sf[i - 1];
    p[i] = cdf[i] <= F(0.5) ? cdf[i] - prev_cdf : prev_sf - sf[i];
  }
  // log C(n, i) per i = 0 … n.
  std::vector<F> logc(n + 1);
  for (std::uint32_t i = 0; i <= n; ++i) logc[i] = std::lgamma(F(n + 1)) - std::lgamma(F(i + 1)) - std::lgamma(F(n - i + 1));
  const std::uint32_t k = (n + 1) / 2;
  // G_j = P(M ≤ j) = Σ_{i ≥ k} C(n,i) F^i S^(n−i); H_j = P(M > j) = Σ_{i < k} C(n,i) F^i S^(n−i).
  // Un esponente nullo non moltiplica il logaritmo: con una coda nulla in double 0 · log(0) darebbe NaN.
  const auto term = [&](std::uint32_t i, F f, F s) {
    const F lf = i == 0 ? F(0) : F(i) * std::log(f);
    const F ls = i == n ? F(0) : F(n - i) * std::log(s);
    return std::exp(logc[i] + lf + ls);
  };
  auto tail = [&](F f, F s, bool upper) {
    F sum = 0;
    if (upper) {
      for (std::uint32_t i = 0; i < k; ++i) sum += term(i, f, s);
    } else {
      for (std::uint32_t i = k; i <= n; ++i) sum += term(i, f, s);
    }
    return sum;
  };
  std::vector<F> G(J), H(J);
  for (std::size_t i = 0; i < J; ++i) {
    G[i] = tail(cdf[i], sf[i], false);
    H[i] = tail(cdf[i], sf[i], true);
  }
  // Varianze a due passaggi (prima la media, poi gli scarti): nessuna cancellazione fra E[X²] ed E[X]².
  std::vector<F> pm(J);
  for (std::size_t i = 0; i < J; ++i) {
    if (i == 0) pm[i] = G[0];
    else pm[i] = G[i] <= F(0.5) ? G[i] - G[i - 1] : H[i - 1] - H[i];
  }
  const auto variance = [&](const std::vector<F>& prob) {
    F m1 = 0;
    for (std::size_t i = 0; i < J; ++i) m1 += (F(lo + static_cast<std::int64_t>(i)) - frac) * prob[i];
    F v = 0;
    for (std::size_t i = 0; i < J; ++i) {
      const F d = F(lo + static_cast<std::int64_t>(i)) - frac - m1;
      v += d * d * prob[i];
    }
    return v;
  };
  discrete_result<F> out;
  out.var_median = variance(pm);
  out.var_sample = variance(p);
  return out;
}

}  // namespace b5o
