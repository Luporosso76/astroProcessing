#pragma once
// Oracle di κ_T(n, g) = Var(T(Z_1, …, Z_n)), Z iid N(0, 1), per T-M3B-C02-05 (C-02.b §b.6, §b.12). Scritto da chi non
// scrive il prodotto; nessun valore viene dal prodotto.
//
// Metodo, in long double:
// - Mediana a conteggio dispari (e winsorized con n = 2g + 1): κ = E[Z_(k)²], k = (n + 1)/2, integrale semplice.
// - Altrimenti (mediana pari = winsorized con n = 2g + 2; winsorized con n ≥ 2g + 2; g = 0 è la media, usata solo come
//   controllo): con a = Z_(g+1), b = Z_(n−g) e m = n − 2g − 2, n·T = (g + 1)(a + b) + S, dove S è la somma degli m
//   campioni interni, iid gaussiani troncati ad (a, b) dato (a, b) (David e Nagaraja). Con u = Φ(a), v = Φ(b) le
//   variabili (u, v − u, 1 − v) sono Dirichlet(g + 1, m + 1, g + 1), quindi p = u ~ Beta(g + 1, n − g) e
//   q = (v − u)/(1 − u) ~ Beta(m + 1, g + 1) sono indipendenti: κ = E_p E_q[H(a, b)] / n², con
//   H = ((g + 1)(a + b))² + 2(g + 1)(a + b) m μ1 + m μ2 + m(m − 1) μ1², μ1, μ2 momenti della gaussiana troncata.
//   Φ(b) − Φ(a) = (1 − p) q e 1 − Φ(b) = (1 − p)(1 − q) si leggono esatti dalle coordinate, senza cancellazione.
// - Quadratura: Kronrod a 21 punti (K21, QUADPACK qk21) composta, prodotto tensoriale, nelle coordinate logit
//   x = log(p/(1 − p)) e y = log(q/(1 − q)), dove la densità Beta è log-concava. Pannelli costruiti dalla log-densità
//   (curvatura locale e caduta per pannello), troncati dove la log-densità scende di kTailDrop sotto il modo.
// - Stima d'errore (C-02 §b.12: dimezzamento del passo e confronto di due regole): K21 sui pannelli di passo h e h/2;
//   Gauss–Legendre a 20 punti (G20, nodi diversi da K21) sui pannelli h. Stima = max(|K_{h/2} − K_h|, |K_h − G_h|) /
//   |K_{h/2}|. Valore riportato: K_{h/2}.
// - Controlli propri, indipendenti dalla stima: massa della misura (deve valere 1) e covarianza con la media,
//   E[(nT)(n Z̄)] = n per ogni T equivariante per posizione (Basu: T − Z̄ è indipendente da Z̄), calcolata sulla stessa
//   griglia con un altro integrando.
// - Inversa della normale: punto iniziale di Abramowitz e Stegun 26.2.23, poi iterazioni di Halley su erfcl fino a
//   convergenza; nessun limite di iterazioni superato in silenzio.
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace kappa_oracle {

using ld = long double;

constexpr ld kTailDrop = 50.0L;        // troncamento: caduta della log-densità rispetto al modo
constexpr ld kPanelCurvature = 6.0L;   // larghezza massima del pannello in scale di curvatura locali
constexpr ld kPanelDrop = 24.0L;       // caduta massima della log-densità attraverso un pannello

// QUADPACK qk21: nodi di Kronrod su [0, 1) in ordine decrescente e pesi K21. Le cifre si verificano nel test con
// l'esattezza sui monomi fino al grado 31.
inline constexpr std::array<ld, 11> kXgk = {
    0.995657163025808080735527280689003L, 0.973906528517171720077964012084452L, 0.930157491355708226001207180059508L,
    0.865063366688984510732096688423493L, 0.780817726586416897063717578345042L, 0.679409568299024406234327365114874L,
    0.562757134668604683339000099272694L, 0.433395394129247190799265943165784L, 0.294392862701460198131126603103866L,
    0.148874338981631210884826001129720L, 0.000000000000000000000000000000000L};
inline constexpr std::array<ld, 11> kWgk = {
    0.011694638867371874278064396062192L, 0.032558162307964727478818972459390L, 0.054755896574351996031381300244580L,
    0.075039674810919952767043140916190L, 0.093125454583697605535065465083366L, 0.109387158802297641899210590325805L,
    0.123491976262065851077208707155775L, 0.134709217311473325928054001771707L, 0.142775938577060080797094273138717L,
    0.147739104901338491374841515972068L, 0.149445554002916905664936468389821L};

// Regola su [-1, 1].
struct quad_rule {
  std::vector<ld> t, w;
};

inline quad_rule make_k21() {
  quad_rule r{std::vector<ld>(21), std::vector<ld>(21)};
  for (int i = 0; i < 11; ++i) {
    r.t[i] = -kXgk[i];
    r.t[20 - i] = kXgk[i];
    r.w[i] = r.w[20 - i] = kWgk[i];
  }
  return r;
}

// Gauss–Legendre a n punti: radici di P_n per Newton in long double, pesi 2 / ((1 − x²) P_n'(x)²).
inline quad_rule make_gauss_legendre(int n) {
  constexpr ld kPi = 3.141592653589793238462643383279502884L;
  quad_rule r{std::vector<ld>(n), std::vector<ld>(n)};
  for (int i = 0; i < n; ++i) {
    ld x = std::cos(kPi * (i + 0.75L) / (n + 0.5L)), dp = 0;
    // Newton fino a |dx| ≤ 1e-15, poi un passo in più: la convergenza quadratica porta all'ultima cifra.
    bool last = false;
    for (int it = 0;; ++it) {
      if (it == 100) throw std::runtime_error("gauss_legendre: Newton non converge");
      ld p0 = 1, p1 = x;
      for (int k = 2; k <= n; ++k) {
        const ld p2 = ((2 * k - 1) * x * p1 - (k - 1) * p0) / k;
        p0 = p1;
        p1 = p2;
      }
      dp = n * (x * p1 - p0) / (x * x - 1);
      const ld dx = p1 / dp;
      x -= dx;
      if (last) break;
      last = std::fabs(dx) <= 1e-15L;
    }
    r.t[n - 1 - i] = x;
    r.w[n - 1 - i] = 2 / ((1 - x * x) * dp * dp);
  }
  return r;
}

inline const quad_rule& k21() {
  static const quad_rule r = make_k21();
  return r;
}
inline const quad_rule& g20() {
  static const quad_rule r = make_gauss_legendre(20);
  return r;
}

// ---------------------------------------------------------------- normale

constexpr ld kSqrt2 = 1.414213562373095048801688724209698L;
constexpr ld kInvSqrt2Pi = 0.398942280401432677939946059934382L;

inline ld phi(ld x) { return kInvSqrt2Pi * std::exp(-0.5L * x * x); }
// Φ(x), accurata in senso relativo per x ≤ 0 (erfc di argomento non negativo).
inline ld Phi_lower(ld x) { return 0.5L * std::erfc(-x / kSqrt2); }

struct normal_point {
  ld x, phi;  // x e φ(x)
};

// x ≤ 0 con Φ(x) = P, P in (0, 1/2], e φ(x).
inline normal_point inv_Phi_lower(ld P) {
  if (!(P > 0.0L && P <= 0.5L)) throw std::domain_error("inv_Phi_lower: P fuori da (0, 1/2]");
  if (P == 0.5L) return {0.0L, kInvSqrt2Pi};
  // Punto iniziale (errore ≤ 4.5e-4) e un passo di Halley in double; poi Halley in long double fino a convergenza.
  const double pd = static_cast<double>(P);
  const double t = std::sqrt(-2.0 * std::log(pd));
  double xd = -(t - (2.515517 + t * (0.802853 + t * 0.010328)) / (1.0 + t * (1.432788 + t * (0.189269 + t * 0.001308))));
  const double ud = (0.5 * std::erfc(-xd / 1.4142135623730951) - pd) / (0.3989422804014327 * std::exp(-0.5 * xd * xd));
  xd -= ud / (1.0 + 0.5 * xd * ud);
  ld x = xd;
  for (int it = 0; it < 12; ++it) {
    const ld ph = phi(x);
    const ld u = (Phi_lower(x) - P) / ph;
    const ld dx = u / (1.0L + 0.5L * x * u);
    const ld xn = x - dx;
    // Halley converge con costante (x² + 2)/12: dopo un passo ≤ 1e-8 l'errore residuo è sotto 1e-21.
    if (std::fabs(dx) <= 1e-8L * (1.0L + std::fabs(xn))) {
      // φ(xn) = φ(x) · exp(s), s = dx (x + xn)/2; per |s| ≤ 1e-6 la serie al terzo ordine ha errore < 1e-25.
      const ld s = 0.5L * dx * (x + xn);
      const ld f = std::fabs(s) <= 1e-6L ? 1.0L + s * (1.0L + s * (0.5L + s / 6.0L)) : std::exp(s);
      return {xn, ph * f};
    }
    x = xn;
  }
  throw std::runtime_error("inv_Phi_lower: Halley non converge, P = " + std::to_string(pd));
}

// Φ⁻¹ di una probabilità data come coda inferiore `lo` e coda superiore `hi` = 1 − lo, entrambe esatte.
inline normal_point inv_Phi(ld lo, ld hi) {
  if (lo <= 0.5L) return inv_Phi_lower(lo);
  const normal_point r = inv_Phi_lower(hi);
  return {-r.x, r.phi};
}

// ---------------------------------------------------------------- Beta in coordinata logit

inline ld sigmoid(ld x) {
  if (x >= 0) return 1.0L / (1.0L + std::exp(-x));
  const ld e = std::exp(x);
  return e / (1.0L + e);
}
inline ld log_sigmoid(ld x) { return x >= 0 ? -std::log1p(std::exp(-x)) : x - std::log1p(std::exp(x)); }

// Densità di p ~ Beta(α, β) nella coordinata x = logit(p): p^α (1 − p)^β / B(α, β).
struct beta_logit {
  ld alpha, beta, log_norm;
  beta_logit(ld a, ld b) : alpha(a), beta(b), log_norm(std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b)) {}
  ld log_density(ld x) const { return alpha * log_sigmoid(x) + beta * log_sigmoid(-x) - log_norm; }
  ld curvature(ld x) const { return (alpha + beta) * sigmoid(x) * sigmoid(-x); }  // −d²/dx² del log
  ld mode() const { return std::log(alpha / beta); }
};

// Estremo dove la log-densità è scesa di `drop` dal modo, dal lato `dir` (±1). La log-densità è concava.
inline ld tail_bound(const beta_logit& b, ld dir, ld drop) {
  const ld x0 = b.mode(), top = b.log_density(x0);
  ld step = 1.0L;
  while (top - b.log_density(x0 + dir * step) < drop) {
    step *= 2.0L;
    if (step > 1e6L) throw std::runtime_error("tail_bound: coda non trovata");
  }
  ld lo = 0.0L, hi = step;
  for (int i = 0; i < 200 && hi - lo > 1e-12L; ++i) {
    const ld mid = 0.5L * (lo + hi);
    (top - b.log_density(x0 + dir * mid) < drop ? lo : hi) = mid;
  }
  return x0 + dir * hi;
}

// Pannelli del passo h: dal modo verso le code, ciascuno largo al più kPanelCurvature scale di curvatura (massima
// curvatura sul pannello) e con caduta della log-densità al più kPanelDrop.
inline std::vector<ld> panels(const beta_logit& b) {
  const ld x0 = b.mode();
  const ld lo = tail_bound(b, -1.0L, kTailDrop), hi = tail_bound(b, 1.0L, kTailDrop);
  auto ok = [&](ld a, ld c) {
    ld curv = std::max(b.curvature(a), b.curvature(c));
    if ((a < 0) != (c < 0)) curv = std::max(curv, b.curvature(0.0L));
    const bool wide_ok = std::fabs(c - a) * std::sqrt(curv) <= kPanelCurvature;
    const bool drop_ok = std::fabs(b.log_density(c) - b.log_density(a)) <= kPanelDrop;
    return wide_ok && drop_ok;
  };
  std::vector<ld> right{x0}, left{x0};
  for (ld dir : {1.0L, -1.0L}) {
    std::vector<ld>& out = dir > 0 ? right : left;
    const ld end = dir > 0 ? hi : lo;
    ld a = x0;
    while (dir * (end - a) > 0) {
      ld w = std::fabs(end - a);
      while (!ok(a, a + dir * w)) w *= 0.8L;
      a = std::fabs(end - a) <= w ? end : a + dir * w;
      out.push_back(a);
      if (out.size() > 10000) throw std::runtime_error("panels: troppi pannelli");
    }
  }
  std::vector<ld> br(left.rbegin(), left.rend());
  br.insert(br.end(), right.begin() + 1, right.end());
  return br;
}

inline std::vector<ld> halve(const std::vector<ld>& br) {
  std::vector<ld> out{br.front()};
  for (std::size_t i = 1; i < br.size(); ++i) {
    out.push_back(0.5L * (br[i - 1] + br[i]));
    out.push_back(br[i]);
  }
  return out;
}

// Nodi di una dimensione: coordinata e peso della regola moltiplicato per la densità Beta.
struct node1 {
  ld x, w;
};

inline std::vector<node1> nodes_of(const beta_logit& b, const std::vector<ld>& br, const quad_rule& r) {
  std::vector<node1> out;
  for (std::size_t i = 1; i < br.size(); ++i) {
    const ld c = 0.5L * (br[i - 1] + br[i]), h = 0.5L * (br[i] - br[i - 1]);
    for (std::size_t j = 0; j < r.t.size(); ++j) {
      const ld x = c + h * r.t[j];
      out.push_back({x, h * r.w[j] * std::exp(b.log_density(x))});
    }
  }
  return out;
}

// ---------------------------------------------------------------- κ

struct result {
  ld kappa = 0;          // K_{h/2}
  ld est_rel = 0;        // stima dell'errore relativo: max(est_halving, est_rules)
  ld est_halving = 0;    // |K_{h/2} − K_h| / |K_{h/2}|
  ld est_rules = 0;      // |K_h − G_h| / |K_{h/2}|
  ld mass_dev = 0;       // |∫ densità − 1| con K_{h/2}
  ld cross_dev = 0;      // |E[(nT)(n Z̄)]/n − 1| con K_{h/2}
  std::size_t nodes = 0; // valutazioni dell'integrando
};

// Somme di una griglia: integrando di κ (n²κ nel caso a due coordinate), massa, covarianza con la media.
struct sums {
  ld h = 0, mass = 0, cross = 0;
};

struct pnode {  // valori per nodo della prima coordinata
  ld p, pc, a, phia;
};

inline pnode p_values(ld x) {
  const ld p = sigmoid(x), pc = sigmoid(-x);
  const normal_point a = inv_Phi(p, pc);
  return {p, pc, a.x, a.phi};
}

inline result finish(const sums& kh, const sums& gh, const sums& kh2, ld scale, ld cross_target, std::size_t nodes) {
  result r;
  r.kappa = kh2.h / scale;
  r.est_halving = std::fabs(kh2.h - kh.h) / std::fabs(kh2.h);
  r.est_rules = std::fabs(kh.h - gh.h) / std::fabs(kh2.h);
  r.est_rel = std::max(r.est_halving, r.est_rules);
  r.mass_dev = std::fabs(kh2.mass - 1.0L);
  r.cross_dev = std::fabs(kh2.cross / cross_target - 1.0L);
  r.nodes = nodes;
  return r;
}

// Caso dispari: κ = E[Z_(k)²], Z_(k) = Φ⁻¹(p), p ~ Beta(k, k); controllo E[Z_(k) · n Z̄] = 1.
inline result odd_median(std::uint32_t n) {
  const ld k = (n + 1) / 2;
  const beta_logit bp(k, k);
  const std::vector<ld> b1 = panels(bp), b2 = halve(b1);
  auto run = [&](const std::vector<node1>& nodes) {
    sums s;
    for (const node1& nd : nodes) {
      const pnode v = p_values(nd.x);
      const ld el = (k - 1) * (-v.phia / v.p), eu = (k - 1) * (v.phia / v.pc);
      s.h += nd.w * v.a * v.a;
      s.mass += nd.w;
      s.cross += nd.w * v.a * (v.a + el + eu);
    }
    return s;
  };
  const std::vector<node1> kn1 = nodes_of(bp, b1, k21()), gn1 = nodes_of(bp, b1, g20()), kn2 = nodes_of(bp, b2, k21());
  return finish(run(kn1), run(gn1), run(kn2), 1.0L, 1.0L, kn1.size() + gn1.size() + kn2.size());
}

// Caso con (a, b) = (Z_(g+1), Z_(n−g)), n ≥ 2g + 2.
inline result dirichlet(std::uint32_t n, std::uint32_t g) {
  if (n < 2 * g + 2) throw std::domain_error("dirichlet: n < 2g + 2");
  const ld G = g, M = n - 2 * g - 2, N = n;
  const beta_logit bp(G + 1, N - G), bq(M + 1, G + 1);
  const std::vector<ld> bp1 = panels(bp), bp2 = halve(bp1), bq1 = panels(bq), bq2 = halve(bq1);

  auto run = [&](const std::vector<node1>& xs, const std::vector<node1>& ys) {
    sums s;
    std::vector<ld> qv(ys.size()), qcv(ys.size());
    for (std::size_t j = 0; j < ys.size(); ++j) {
      qv[j] = sigmoid(ys[j].x);
      qcv[j] = sigmoid(-ys[j].x);
    }
    for (const node1& xn : xs) {
      const pnode P = p_values(xn.x);
      const ld el = g > 0 ? -G * P.phia / P.p : 0.0L;
      for (std::size_t j = 0; j < ys.size(); ++j) {
        const ld q = qv[j], qc = qcv[j];
        const ld lo = P.p + P.pc * q, hi = P.pc * qc;
        const normal_point B = inv_Phi(lo, hi);
        const ld b = B.x;
        const ld c = (G + 1) * (P.a + b);
        ld es = 0, es2 = 0;
        if (M > 0) {
          const ld d = P.pc * q;                                                 // Φ(b) − Φ(a)
          // φ(a) − φ(b), con expm1 quando i due valori sono vicini (nessuna cancellazione).
          const ld e = -0.5L * (b - P.a) * (b + P.a);
          const ld num1 = std::fabs(e) < 0.5L ? -P.phia * std::expm1(e) : P.phia - B.phi;
          const ld num2 = (P.a - b) * B.phi + P.a * num1;                       // aφ(a) − bφ(b)
          const ld mu1 = num1 / d, mu2 = 1.0L + num2 / d;
          es = M * mu1;
          es2 = M * mu2 + M * (M - 1) * mu1 * mu1;
        }
        const ld h = c * c + 2 * c * es + es2;
        const ld eu = g > 0 ? G * B.phi / hi : 0.0L;
        const ld rest = el + eu + P.a + b;
        const ld cross = c * (rest + es) + es * rest + es2;
        const ld w = xn.w * ys[j].w;
        s.h += w * h;
        s.mass += w;
        s.cross += w * cross;
      }
    }
    return s;
  };
  const std::vector<node1> kx1 = nodes_of(bp, bp1, k21()), gx1 = nodes_of(bp, bp1, g20()), kx2 = nodes_of(bp, bp2, k21());
  const std::vector<node1> ky1 = nodes_of(bq, bq1, k21()), gy1 = nodes_of(bq, bq1, g20()), ky2 = nodes_of(bq, bq2, k21());
  const std::size_t nodes = kx1.size() * ky1.size() + gx1.size() * gy1.size() + kx2.size() * ky2.size();
  return finish(run(kx1, ky1), run(gx1, gy1), run(kx2, ky2), N * N, N, nodes);
}

// Mediana: 2 ≤ n. Winsorized: g ≥ 1, n ≥ 2g + 1. Media (solo controllo): g = 0 su `winsorized_or_mean`.
inline result median(std::uint32_t n) {
  if (n < 2) throw std::domain_error("median: n < 2");
  return n % 2 == 1 ? odd_median(n) : dirichlet(n, n / 2 - 1);
}
inline result winsorized_or_mean(std::uint32_t n, std::uint32_t g) {
  if (n < 2 * g + 1 || n < 2) throw std::domain_error("winsorized: n < 2g + 1");
  return n == 2 * g + 1 ? odd_median(n) : dirichlet(n, g);
}

}  // namespace kappa_oracle
