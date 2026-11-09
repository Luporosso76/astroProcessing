#include "astro/master_kappa.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "astro/error.hpp"
#include "calibration_internal.hpp"

namespace astro {

// Calcolo di κ_T(n, g) = E[T(Z)²] (T è dispari e Z simmetrica, quindi E[T] = 0), in long double.
//
// Rappresentazione (C-02.b §b.6, David e Nagaraja):
// - Mediana a conteggio dispari e winsorized con n = 2g + 1 (che è la mediana): κ = E[a²], a = Z_(k), k = (n + 1)/2.
// - Altrimenti, con a = Z_(g+1), b = Z_(n−g), m = n − 2g − 2 ≥ 0: n·T = (g + 1)(a + b) + S, S somma degli m campioni
//   interni, iid gaussiani troncati ad (a, b) dato (a, b). κ = E[H(a, b)] / n², H = E[(n·T)² | a, b] dai momenti
//   troncati in forma chiusa. Con p = Φ(a) ~ Beta(g + 1, n − g) e q = (Φ(b) − Φ(a)) / Φ(−a) ~ Beta(m + 1, g + 1),
//   indipendenti (fattorizzazione di Dirichlet di (Φ(a), Φ(b) − Φ(a), 1 − Φ(b))).
//
// Quadratura: regola dei trapezi su tutta la retta nelle coordinate probit delle due Beta, a = Φ⁻¹(p) e t = Φ⁻¹(q).
// In queste coordinate le densità sono intere e log-concave con code gaussiane, e la regola dei trapezi converge in modo
// esponenziale nel passo. Il passo parte dalla scala di curvatura al modo (σ) e si dimezza: la stima d'errore è lo scarto
// relativo fra i due ultimi livelli più lo scarto della massa della misura da 1 (κ è normalizzato con la massa
// calcolata). Il dominio si tronca dove la log-densità scende di kTailDrop sotto il modo: la massa esclusa è sotto
// e^−kTailDrop per ogni lato e resta sotto l'arrotondamento anche moltiplicata per H.
// Φ(b) − Φ(a) = Φ(−a)·Φ(t) e 1 − Φ(b) = Φ(−a)·Φ(−t) si ottengono senza cancellazione.

namespace {

using ld = long double;

constexpr ld kSqrt2 = 1.414213562373095048801688724209698079L;
constexpr ld kInvSqrt2Pi = 0.398942280401432677939946059934381868L;
constexpr ld kLogSqrt2Pi = 0.918938533204672741780329736405617639L;

constexpr ld kTailDrop = 64.0L;      // caduta della log-densità ai bordi del dominio troncato
constexpr ld kLevelTarget = 1e-13L;  // scarto fra livelli a cui il dimezzamento si ferma
constexpr int kMinLevel = 2;         // almeno due dimezzamenti dopo il passo σ
constexpr int kMaxLevel = 6;         // passo minimo σ/64; oltre, la stima riportata resta quella dell'ultimo livello

// Iterazione interna che non converge: la stima d'errore diventa infinita e check_kappa_accuracy ferma il job.
struct not_converged {};

ld phi(ld x) { return kInvSqrt2Pi * std::exp(-0.5L * x * x); }
// Φ(x) con erfc: accurata in senso relativo per x ≤ 0; per la coda alta si usa Phi(−x).
ld Phi(ld x) { return 0.5L * std::erfc(-x / kSqrt2); }
ld log_Phi(ld x) { return x <= 0 ? std::log(Phi(x)) : std::log1p(-Phi(-x)); }

// Punto iniziale di Wichura (AS 241, PPND16) in double per p ∈ (0, 1).
double wichura(double p) {
  const double q = p - 0.5;
  if (std::fabs(q) <= 0.425) {
    const double r = 0.180625 - q * q;
    return q *
           (((((((2.5090809287301226727e+3 * r + 3.3430575583588128105e+4) * r + 6.7265770927008700853e+4) * r +
                4.5921953931549871457e+4) * r + 1.3731693765509461125e+4) * r + 1.9715909503065514427e+3) * r +
             1.3314166789178437745e+2) * r + 3.3871328727963666080e+0) /
           (((((((5.2264952788528545610e+3 * r + 2.8729085735721942674e+4) * r + 3.9307895800092710610e+4) * r +
                2.1213794301586595867e+4) * r + 5.3941960214247511077e+3) * r + 6.8718700749205790830e+2) * r +
             4.2313330701600911252e+1) * r + 1.0);
  }
  double r = std::sqrt(-std::log(q < 0 ? p : 1.0 - p));
  double x;
  if (r <= 5.0) {
    r -= 1.6;
    x = (((((((7.74545014278341407640e-4 * r + 2.27238449892691845833e-2) * r + 2.41780725177450611770e-1) * r +
             1.27045825245236838258e+0) * r + 3.64784832476320460504e+0) * r + 5.76949722146069140550e+0) * r +
          4.63033784615654529590e+0) * r + 1.42343711074968357734e+0) /
        (((((((1.05075007164441684324e-9 * r + 5.47593808499534494600e-4) * r + 1.51986665636164571966e-2) * r +
             1.48103976427480074590e-1) * r + 6.89767334985100004550e-1) * r + 1.67638483018380384940e+0) * r +
          2.05319162663775882187e+0) * r + 1.0);
  } else {
    r -= 5.0;
    x = (((((((2.01033439929228813265e-7 * r + 2.71155556874348757815e-5) * r + 1.24266094738807843860e-3) * r +
             2.65321895265761230930e-2) * r + 2.96560571828504891230e-1) * r + 1.78482653991729133580e+0) * r +
          5.46378491116411436990e+0) * r + 6.65790464350110377720e+0) /
        (((((((2.04426310338993978564e-15 * r + 1.42151175831644588870e-7) * r + 1.84631831751005468180e-5) * r +
             7.86869131145613259100e-4) * r + 1.48753612908506148525e-2) * r + 1.36929880922735805310e-1) * r +
          5.99832206555887937690e-1) * r + 1.0);
  }
  return q < 0 ? -x : x;
}

// x con Φ(x) = P, P ∈ (0, 1/2]. Newton su log Φ, concava e crescente: da sinistra della radice le iterate crescono in
// modo monotono, da destra il primo passo porta a sinistra. Ci si ferma quando il passo è sotto 1e-11 (relativo): la
// convergenza quadratica lascia un errore sotto l'arrotondamento del long double.
ld inv_Phi_lower(ld P) {
  ld x = P > 1e-300L ? static_cast<ld>(wichura(static_cast<double>(P))) : -std::sqrt(-2.0L * std::log(P));
  for (int it = 0; it < 64; ++it) {
    const ld F = Phi(x);
    const ld step = std::log1p((F - P) / P) * F / phi(x);
    x -= step;
    if (!std::isfinite(x)) throw not_converged{};
    if (std::fabs(step) <= 1e-11L * std::max(1.0L, std::fabs(x))) return x;
  }
  throw not_converged{};
}

// Somma compensata di Neumaier: l'ordine degli addendi è fisso, quindi il risultato è identico a ogni chiamata.
struct compensated {
  ld s = 0, c = 0;
  void add(ld x) {
    const ld t = s + x;
    c += std::fabs(s) >= std::fabs(x) ? (s - t) + x : (x - t) + s;
    s = t;
  }
  ld value() const { return s + c; }
};

// p ~ Beta(α, β) nella coordinata t = Φ⁻¹(p): densità φ(t) Φ(t)^(α−1) Φ(−t)^(β−1) / B(α, β), log-concava per α, β ≥ 1.
struct probit_beta {
  ld a1, b1, log_norm;
  ld mode = 0, sigma = 0, lo = 0, hi = 0;

  probit_beta(ld alpha, ld beta)
      : a1(alpha - 1), b1(beta - 1), log_norm(std::lgamma(alpha) + std::lgamma(beta) - std::lgamma(alpha + beta)) {
    // Modo: zero della derivata della log-densità, decrescente; il segno a ±40 è positivo a sinistra e negativo a destra.
    ld l = -40.0L, r = 40.0L;
    for (int i = 0; i < 256; ++i) {
      const ld mid = 0.5L * (l + r);
      if (mid == l || mid == r) break;
      (slope(mid) > 0 ? l : r) = mid;
    }
    mode = 0.5L * (l + r);
    sigma = 1.0L / std::sqrt(curvature(mode));
    lo = bound(-1.0L);
    hi = bound(1.0L);
  }

  ld log_density(ld t) const { return a1 * log_Phi(t) + b1 * log_Phi(-t) - 0.5L * t * t - kLogSqrt2Pi - log_norm; }
  ld slope(ld t) const {
    const ld f = phi(t);
    return a1 * (f / Phi(t)) - b1 * (f / Phi(-t)) - t;
  }
  ld curvature(ld t) const {  // −d²/dt² della log-densità
    const ld f = phi(t), r = f / Phi(t), s = f / Phi(-t);
    return a1 * r * (r + t) + b1 * s * (s - t) + 1.0L;
  }
  // Estremo dal lato `dir` dove la log-densità è scesa di kTailDrop sotto il modo.
  ld bound(ld dir) const {
    const ld top = log_density(mode);
    ld step = sigma;
    while (top - log_density(mode + dir * step) < kTailDrop) {
      step *= 2.0L;
      if (step > 64.0L) throw not_converged{};
    }
    ld l = step / 2, r = step;
    for (int i = 0; i < 128; ++i) {
      const ld mid = 0.5L * (l + r);
      if (mid == l || mid == r) break;
      (top - log_density(mode + dir * mid) < kTailDrop ? l : r) = mid;
    }
    return mode + dir * r;
  }
};

struct node {
  ld t, w;  // coordinata e peso del trapezio per la densità
};

// Nodi del livello: t = modo + j·h, h = σ / 2^level, entro [lo, hi]; i nodi di un livello contengono quelli del
// precedente.
std::vector<node> nodes_of(const probit_beta& b, int level) {
  const ld h = std::ldexp(b.sigma, -level);
  const auto jlo = static_cast<long long>(std::ceil((b.lo - b.mode) / h));
  const auto jhi = static_cast<long long>(std::floor((b.hi - b.mode) / h));
  std::vector<node> out;
  out.reserve(static_cast<std::size_t>(jhi - jlo + 1));
  for (long long j = jlo; j <= jhi; ++j) {
    const ld t = b.mode + static_cast<ld>(j) * h;
    out.push_back({t, h * std::exp(b.log_density(t))});
  }
  return out;
}

struct level_value {
  ld kappa;     // normalizzato con la massa calcolata
  ld mass_dev;  // |massa − 1| sommata sulle coordinate
};

// κ = E[a²], a = Z_(k) con p ~ Beta(k, k).
struct odd_median {
  probit_beta pb;
  explicit odd_median(std::uint32_t n) : pb((n + 1) / 2, (n + 1) / 2) {}
  level_value at(int level) const {
    compensated mass, sum;
    for (const node& nd : nodes_of(pb, level)) {
      mass.add(nd.w);
      sum.add(nd.w * nd.t * nd.t);
    }
    return {sum.value() / mass.value(), std::fabs(mass.value() - 1.0L)};
  }
};

// κ = E[H(a, b)] / n² con la fattorizzazione di Dirichlet, n ≥ 2g + 2.
struct order_pair {
  ld n, g1, m;  // n, g + 1, m = n − 2g − 2
  probit_beta pa, pt;
  order_pair(std::uint32_t n_, std::uint32_t g)
      : n(n_), g1(static_cast<ld>(g) + 1), m(static_cast<ld>(n_) - 2.0L * g - 2.0L),
        pa(static_cast<ld>(g) + 1, static_cast<ld>(n_) - g), pt(m + 1, static_cast<ld>(g) + 1) {}

  // E[(nT)² | a, b]: c = (g + 1)(a + b), momenti μ1, μ2 della gaussiana troncata ad (a, b).
  ld h_of(ld a, ld phia, ld Qa, ld b, ld q) const {
    const ld c = g1 * (a + b);
    if (m == 0) return c * c;
    const ld phib = phi(b);
    const ld d = Qa * q;                                                  // Φ(b) − Φ(a)
    const ld e = 0.5L * (a - b) * (a + b);                                // log φ(b) − log φ(a)
    const ld num1 = std::fabs(e) < 0.5L ? -phia * std::expm1(e) : phia - phib;  // φ(a) − φ(b)
    const ld num2 = (a - b) * phib + a * num1;                            // aφ(a) − bφ(b)
    const ld mu1 = num1 / d, mu2 = 1.0L + num2 / d;
    return c * c + 2.0L * c * m * mu1 + m * mu2 + m * (m - 1.0L) * mu1 * mu1;
  }

  level_value at(int level) const {
    const std::vector<node> xs = nodes_of(pa, level), ys = nodes_of(pt, level);
    std::vector<ld> q(ys.size()), qc(ys.size());
    compensated mass_t;
    for (std::size_t j = 0; j < ys.size(); ++j) {
      q[j] = Phi(ys[j].t);
      qc[j] = Phi(-ys[j].t);
      mass_t.add(ys[j].w);
    }
    compensated mass_a, sum;
    for (const node& x : xs) {
      const ld a = x.t, phia = phi(a), Pa = Phi(a), Qa = Phi(-a);
      compensated inner;
      for (std::size_t j = 0; j < ys.size(); ++j) {
        const ld lo = Pa + Qa * q[j], hi = Qa * qc[j];  // Φ(b) e 1 − Φ(b)
        const ld b = lo <= hi ? inv_Phi_lower(lo) : -inv_Phi_lower(hi);
        inner.add(ys[j].w * h_of(a, phia, Qa, b, q[j]));
      }
      mass_a.add(x.w);
      sum.add(x.w * inner.value());
    }
    const ld ma = mass_a.value(), mt = mass_t.value();
    return {sum.value() / (ma * mt * n * n), std::fabs(ma - 1.0L) + std::fabs(mt - 1.0L)};
  }
};

template <class Integral>
kappa_result integrate(const Integral& integral) {
  level_value prev = integral.at(0);
  for (int level = 1;; ++level) {
    const level_value cur = integral.at(level);
    const ld change = std::fabs(cur.kappa - prev.kappa) / std::fabs(cur.kappa);
    const bool done = level >= kMinLevel && change <= kLevelTarget;
    if (done || level == kMaxLevel || !std::isfinite(change)) {
      const ld estimate = change + cur.mass_dev;
      return {static_cast<double>(cur.kappa),
              std::isfinite(estimate) ? static_cast<double>(estimate) : std::numeric_limits<double>::infinity()};
    }
    prev = cur;
  }
}

const char* estimator_name(kappa_estimator e) { return e == kappa_estimator::median ? "median" : "winsorized"; }

stop_error::context_list cell_context(kappa_estimator e, std::uint32_t n, std::uint32_t g) {
  return {{"metodo", estimator_name(e)}, {"n", std::to_string(n)}, {"g", std::to_string(g)}};
}

}  // namespace

kappa_result robust_kappa(kappa_estimator estimator, std::uint32_t n, std::uint32_t g) {
  const std::uint64_t n64 = n, g64 = g;
  const bool in_domain = estimator == kappa_estimator::median
                             ? g == 0 && n >= 2 && n <= kappa_n_max
                             : g >= 1 && 2 * g64 + 1 <= n64 && n <= kappa_n_max;
  if (!in_domain) {
    throw stop_error(error_category::unsupported, "master.variance_domain",
                     "κ fuori dal dominio verificato (C-02.b §b.2, §b.6)", cell_context(estimator, n, g));
  }
  try {
    if (estimator == kappa_estimator::median) {
      return n % 2 == 1 ? integrate(odd_median(n)) : integrate(order_pair(n, n / 2 - 1));
    }
    return n == 2 * g + 1 ? integrate(odd_median(n)) : integrate(order_pair(n, g));
  } catch (const not_converged&) {
    return {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()};
  }
}

void check_kappa_accuracy(const kappa_result& kappa, kappa_estimator estimator, std::uint32_t n, std::uint32_t g) {
  const double est = kappa.rel_error_estimate;
  if (std::isfinite(est) && est >= 0 && est <= kappa_rel_error_max) return;
  stop_error::context_list ctx = cell_context(estimator, n, g);
  ctx.emplace_back("stima", calib_detail::num_text(est));
  ctx.emplace_back("soglia", calib_detail::num_text(kappa_rel_error_max));
  throw stop_error(error_category::numeric, "master.kappa_accuracy",
                   "κ non calcolato con l'accuratezza richiesta secondo la propria stima d'errore (C-02.b §b.6)",
                   std::move(ctx));
}

}  // namespace astro
