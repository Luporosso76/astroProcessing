#pragma once

// FIX-SYN-CAL-08 (C-02.b §b.9, C-02.b-flat §f.9): generatore deterministico dei frame di calibrazione dei test dei
// master di M3.b, con la verità esatta di ciò che genera. Solo libreria standard, `exact_rational.hpp` e
// FIX-SYN-DNG-01 (`syn_dng.hpp`): nessun codice di astrocore, nessun operatore del master.
//
// Vincoli del protocollo preregistrato (docs/evidence/M3b-protocollo.md §1):
// - gaussiane: un `std::mt19937_64` per flusso, `U = (w >> 11) · 2⁻⁵³`, metodo polare di Marsaglia; le `z` si consumano
//   in sequenza e la `z₂` avanzata passa al campione successivo, anche oltre la fine di una realizzazione;
// - ordine del flusso: realizzazione, frame per indice di generazione, pixel in ordine di riga; consumano estrazioni
//   solo i campioni con varianza > 0;
// - campione `x = μ + σ z`: prima `σ · z`, poi `+ μ`, ciascuno arrotondato in f64 (la build compila con
//   `-ffp-contract=off`: nessuna FMA).
// Vincoli della fixture:
// - media e varianza di ogni campione sono razionali rappresentabili esattamente in f64; un parametro che non lo è
//   ferma la costruzione con `std::invalid_argument`, senza arrotondare. `σ` è la `sqrt` IEEE della varianza (errore
//   relativo ≤ 2⁻⁵³): scarto dal modello dichiarato come limite, fuori dal margine, come l'arrotondamento dei campioni;
// - un outlier sostituisce il campione dopo l'estrazione, che resta consumata: gli altri campioni non cambiano;
// - canali: mono 0; CFA 2 (y mod 2) + (x mod 2), le quattro posizioni del motivo con G1 e G2 separati (C-02.b §b.2),
//   contate dal pixel (0, 0) del frame;
// - flat: `x_f(p) = offset + β(p) + c_(f,c) g(p) + ν_f(p)`, `σ_f² = c_(f,c) g / G + R²` (C-02.b-flat §f.1, §f.9). La
//   regione di livello di ogni canale sono i primi `L_c` pixel del canale in ordine di riga, con `2 L_c > P_c`, `g = 1`,
//   `β = 0` e nessun rumore: la mediana di `x_f − offset` sul canale vale `c_(f,c)` in ogni realizzazione, quindi
//   `m_(f,c) = c_(f,c)` e `c_H,c = m_H,c`.

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "exact_rational.hpp"
#include "syn_dng.hpp"

namespace astro_fixture::syn_cal {

using exact::rat;

[[noreturn]] inline void refuse(const std::string& what) { throw std::invalid_argument("FIX-SYN-CAL-08: " + what); }

inline rat q(std::int64_t num, std::uint64_t den = 1) {
  if (den == 0) refuse("denominatore nullo");
  const bool neg = num < 0;
  const std::uint64_t mag = neg ? ~static_cast<std::uint64_t>(num) + 1 : static_cast<std::uint64_t>(num);
  return rat{exact::from_u64(mag, neg), exact::from_u64(den)};
}

inline bool eq(const rat& a, const rat& b) { return exact::cmp(a, b) == 0; }

// Valore approssimato di un razionale non ridotto: le tre parole alte di numeratore e denominatore, con l'esponente a
// parte, così numeratori e denominatori oltre il campo di long double non danno inf/inf. Errore relativo ≤ 2⁻⁶³.
inline long double approx(const rat& v) {
  const auto top = [](const exact::big& b, int& e) {
    const std::size_t k = std::min<std::size_t>(3, b.m.size());
    long double m = 0;
    for (std::size_t i = b.m.size(); i-- > b.m.size() - k;) m = m * 4294967296.0L + b.m[i];
    e = static_cast<int>(32 * (b.m.size() - k));
    return m;
  };
  if (v.n.zero()) return 0;
  int en = 0, ed = 0;
  const long double mn = top(v.n, en);
  const long double md = top(v.d, ed);
  const long double r = std::ldexp(mn / md, en - ed);
  return v.n.neg ? -r : r;
}

inline double exact_double(const rat& v, const std::string& what) {
  const double d = static_cast<double>(approx(v));
  if (!std::isfinite(d) || !eq(exact::from_double(d), v)) refuse(what + " non rappresentabile esattamente in f64");
  return d;
}

// ---------------------------------------------------------------- generatore (protocollo §1)

inline double uniform53(std::mt19937_64& e) { return static_cast<double>(e() >> 11) * 0x1p-53; }

class gauss_stream {
 public:
  explicit gauss_stream(std::uint64_t seed) : engine_(seed) {}

  double next() {
    ++count_;
    if (has_spare_) {
      has_spare_ = false;
      return spare_;
    }
    for (;;) {
      const double v1 = 2.0 * uniform53(engine_) - 1.0;
      const double v2 = 2.0 * uniform53(engine_) - 1.0;
      const double s = v1 * v1 + v2 * v2;
      if (s >= 1.0 || s == 0.0) continue;
      const double f = std::sqrt(-2.0 * std::log(s) / s);
      spare_ = v2 * f;
      has_spare_ = true;
      return v1 * f;
    }
  }

  std::uint64_t count() const { return count_; }

 private:
  std::mt19937_64 engine_;
  double spare_ = 0.0;
  bool has_spare_ = false;
  std::uint64_t count_ = 0;
};

// Intero uniforme in [lo, hi] dai bit alti della parola successiva, con rifiuto (nessuna distorsione di modulo);
// con lo = hi nessuna parola consumata.
inline std::int64_t uniform_int(std::mt19937_64& e, std::int64_t lo, std::int64_t hi) {
  if (hi < lo) refuse("intervallo intero vuoto");
  const auto span = static_cast<std::uint64_t>(hi - lo);
  if (span == 0) return lo;
  const int bits = std::bit_width(span);
  for (;;) {
    const std::uint64_t v = e() >> (64 - bits);
    if (v <= span) return lo + static_cast<std::int64_t>(v);
  }
}

// ---------------------------------------------------------------- insiemi di frame

struct outlier {
  std::size_t frame = 0;
  std::size_t pixel = 0;
  double value = 0.0;
};

struct frame_set {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::vector<std::vector<double>> mean;   // [frame][pixel], valore esatto
  std::vector<std::vector<double>> var;    // [frame][pixel], valore esatto; 0 = nessun rumore, nessuna estrazione
  std::vector<std::vector<double>> sigma;  // sqrt(var)
  std::vector<outlier> outliers;           // ordinati per (frame, pixel), al più uno per campione

  std::size_t frames() const { return mean.size(); }
  std::size_t pixels() const { return std::size_t{width} * height; }
  rat mean_exact(std::size_t f, std::size_t p) const { return exact::from_double(mean[f][p]); }
  rat var_exact(std::size_t f, std::size_t p) const { return exact::from_double(var[f][p]); }
};

using frames = std::vector<std::vector<double>>;  // [frame][pixel]

inline frame_set empty_set(std::uint32_t w, std::uint32_t h, std::size_t n) {
  if (w == 0 || h == 0 || n == 0) refuse("dimensioni o numero di frame nulli");
  frame_set s;
  s.width = w;
  s.height = h;
  s.mean.assign(n, std::vector<double>(s.pixels(), 0.0));
  s.var = s.mean;
  s.sigma = s.mean;
  return s;
}

inline void set_law(frame_set& s, std::size_t f, std::size_t p, const rat& mean, const rat& var) {
  if (exact::cmp(var, q(0)) < 0) refuse("varianza negativa");
  s.mean[f][p] = exact_double(mean, "media");
  s.var[f][p] = exact_double(var, "varianza");
  s.sigma[f][p] = std::sqrt(s.var[f][p]);
}

inline void sort_outliers(frame_set& s) {
  const auto key = [](const outlier& o) { return std::pair{o.frame, o.pixel}; };
  std::sort(s.outliers.begin(), s.outliers.end(), [&](const outlier& a, const outlier& b) { return key(a) < key(b); });
  for (std::size_t i = 1; i < s.outliers.size(); ++i) {
    if (key(s.outliers[i]) == key(s.outliers[i - 1])) refuse("due outlier sullo stesso campione");
  }
}

inline void add_outlier(frame_set& s, std::size_t f, std::size_t p, const rat& value) {
  if (f >= s.frames() || p >= s.pixels()) refuse("outlier fuori dall'insieme");
  s.outliers.push_back(outlier{f, p, exact_double(value, "valore dell'outlier")});
  sort_outliers(s);
}

inline std::uint64_t draws_per_realization(const frame_set& s) {
  std::uint64_t k = 0;
  for (const auto& vf : s.var) k += static_cast<std::uint64_t>(std::count_if(vf.begin(), vf.end(), [](double v) { return v > 0; }));
  return k;
}

inline void realize_into(const frame_set& s, gauss_stream& g, frames& out) {
  out.resize(s.frames());
  for (std::size_t f = 0; f < s.frames(); ++f) {
    out[f].resize(s.pixels());
    for (std::size_t p = 0; p < s.pixels(); ++p) {
      if (s.var[f][p] > 0) {
        const double t = s.sigma[f][p] * g.next();
        out[f][p] = t + s.mean[f][p];
      } else {
        out[f][p] = s.mean[f][p];
      }
    }
  }
  for (const auto& o : s.outliers) out[o.frame][o.pixel] = o.value;
}

inline frames realize(const frame_set& s, gauss_stream& g) {
  frames out;
  realize_into(s, g, out);
  return out;
}

// ---------------------------------------------------------------- verità dei momenti

struct moments {
  rat mean;
  rat var;
};

using matrix = std::vector<std::vector<rat>>;

// y gaussiano di media mu e covarianza diag(var), Q simmetrica: E[yᵀQy] = tr(QΣ) + μᵀQμ,
// Var(yᵀQy) = 2 tr((QΣ)²) + 4 μᵀQΣQμ.
inline moments quadratic_moments(const matrix& Q, const std::vector<rat>& mu, const std::vector<rat>& var) {
  const std::size_t n = mu.size();
  if (Q.size() != n || var.size() != n) refuse("dimensioni della forma quadratica");
  for (std::size_t i = 0; i < n; ++i) {
    if (Q[i].size() != n) refuse("forma quadratica non quadrata");
    for (std::size_t j = 0; j < i; ++j) {
      if (!eq(Q[i][j], Q[j][i])) refuse("forma quadratica non simmetrica");
    }
  }
  rat tr = q(0), tr2 = q(0), mqm = q(0), quad = q(0);
  for (std::size_t i = 0; i < n; ++i) {
    tr = tr + Q[i][i] * var[i];
    rat qmu = q(0);
    for (std::size_t j = 0; j < n; ++j) {
      tr2 = tr2 + Q[i][j] * var[j] * Q[j][i] * var[i];
      qmu = qmu + Q[i][j] * mu[j];
    }
    mqm = mqm + mu[i] * qmu;
    quad = quad + qmu * qmu * var[i];
  }
  return {tr + mqm, q(2) * tr2 + q(4) * quad};
}

// Piano di `mean`, V = s²/n = yᵀ H y / (n (n − 1)), H = I − 11ᵀ/n.
inline matrix mean_plane_form(std::size_t n) {
  if (n < 2) refuse("piano di mean con n < 2");
  const auto nn = static_cast<std::int64_t>(n);
  matrix Q(n, std::vector<rat>(n));
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < n; ++j) Q[i][j] = ((i == j ? q(1) : q(0)) - q(1, n)) / q(nn * (nn - 1));
  }
  return Q;
}

// `weighted_mean` (C-02.b §b.6): a_f = w_f / W, T = Σ a²/(1 − 2a), c_f = a²/((1 − 2a)(1 + T)); var_m = Var(M) =
// Σ a² σ²; var_v = Var(V) = 2 tr((AᵀCAΣ)²), A = I − 1aᵀ (T-M3B-C02-03(a)). Fatto di §b.6: E[V] = var_m.
struct weighted_truth {
  std::vector<rat> a, c;
  rat T, var_m, var_v;
  matrix form;  // AᵀCA
};

inline weighted_truth weighted_truth_of(const std::vector<rat>& w, const std::vector<rat>& var) {
  const std::size_t n = w.size();
  if (var.size() != n || n < 3) refuse("weighted_mean richiede n ≥ 3 e una varianza per peso");
  weighted_truth t;
  rat W = q(0);
  for (const auto& x : w) {
    // Un razionale con denominatore nullo è il peso non finito: rifiutato come quello non positivo (C-02.b §b.1).
    if (x.d.zero() || x.d.neg || !(q(0) < x)) refuse("peso non positivo o non finito");
    W = W + x;
  }
  t.T = q(0);
  for (const auto& x : w) {
    t.a.push_back(x / W);
    if (!(q(0) < q(1) - q(2) * t.a.back())) refuse("peso fuori dal dominio 2 max w < W");
    t.T = t.T + t.a.back() * t.a.back() / (q(1) - q(2) * t.a.back());
  }
  t.var_m = q(0);
  for (std::size_t f = 0; f < n; ++f) {
    t.c.push_back(t.a[f] * t.a[f] / ((q(1) - q(2) * t.a[f]) * (q(1) + t.T)));
    t.var_m = t.var_m + t.a[f] * t.a[f] * var[f];
  }
  t.form.assign(n, std::vector<rat>(n, q(0)));
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < n; ++j) {
      for (std::size_t k = 0; k < n; ++k) {
        const rat aki = (k == i ? q(1) : q(0)) - t.a[i];
        const rat akj = (k == j ? q(1) : q(0)) - t.a[j];
        t.form[i][j] = t.form[i][j] + aki * t.c[k] * akj;
      }
    }
  }
  t.var_v = quadratic_moments(t.form, std::vector<rat>(n, q(0)), var).var;
  return t;
}

// ---------------------------------------------------------------- insiemi gaussiani di C-02.b

inline constexpr std::size_t standard_period = 37;

// μ_p = 1000 + (p mod 37) ADU, p in ordine di riga (protocollo §3.1).
inline rat standard_mean(std::size_t p) { return q(static_cast<std::int64_t>(1000 + p % standard_period)); }

inline frame_set gaussian_set(std::uint32_t w, std::uint32_t h, const std::vector<rat>& var_per_frame) {
  auto s = empty_set(w, h, var_per_frame.size());
  std::vector<double> mu(standard_period);
  for (std::size_t k = 0; k < standard_period; ++k) mu[k] = exact_double(standard_mean(k), "media");
  for (std::size_t f = 0; f < s.frames(); ++f) {
    if (exact::cmp(var_per_frame[f], q(0)) < 0) refuse("varianza negativa");
    const double v = exact_double(var_per_frame[f], "varianza");
    const double sd = std::sqrt(v);
    for (std::size_t p = 0; p < s.pixels(); ++p) {
      s.mean[f][p] = mu[p % standard_period];
      s.var[f][p] = v;
      s.sigma[f][p] = sd;
    }
  }
  return s;
}

inline frame_set gaussian_set(std::uint32_t w, std::uint32_t h, std::size_t n, const rat& var) {
  return gaussian_set(w, h, std::vector<rat>(n, var));
}

// Disposizione degli outlier (B2, per T-M3B-C02-02, 10, 11): classe k = p mod 8, j = p / 8, frame f_i = (j + i) mod n,
// valore = media del campione + passi · step. Passi per classe: 0 nessuno; 1 {+12}; 2 {−12}; 3 {+12, −12};
// 4 {+12, +12, +12}; 5 {+25, +6}; 6 {+4}; 7 {−6, +6}.
inline const std::vector<std::vector<std::int64_t>>& standard_outlier_steps() {
  static const std::vector<std::vector<std::int64_t>> steps = {{}, {12}, {-12}, {12, -12}, {12, 12, 12}, {25, 6}, {4}, {-6, 6}};
  return steps;
}

inline void add_standard_outliers(frame_set& s, const rat& step) {
  const std::size_t n = s.frames();
  if (n < 3) refuse("outlier standard con meno di tre frame");
  const auto& steps = standard_outlier_steps();
  for (std::size_t p = 0; p < s.pixels(); ++p) {
    const auto& ks = steps[p % 8];
    for (std::size_t i = 0; i < ks.size(); ++i) {
      const std::size_t f = (p / 8 + i) % n;
      s.outliers.push_back(outlier{f, p, exact_double(s.mean_exact(f, p) + q(ks[i]) * step, "valore dell'outlier")});
    }
  }
  sort_outliers(s);
}

// T-M3B-C02-03 (protocollo §3.1): pesi e σ_f (ADU) di W1 e W2, 64 × 64.
struct weighted_config {
  std::vector<std::int64_t> w;
  std::vector<std::int64_t> sigma;
  std::uint64_t seed = 0;
};

inline weighted_config c02_03_config(int which) {
  if (which == 1) return {{1, 2, 3, 4, 6}, {1, 2, 3, 5, 8}, 2026110901};
  if (which == 2) return {{7, 3, 3, 3}, {2, 1, 3, 4}, 2026110902};
  refuse("configurazione di T-M3B-C02-03 inesistente");
}

inline frame_set c02_03_set(int which) {
  std::vector<rat> var;
  for (auto s : c02_03_config(which).sigma) var.push_back(q(s * s));
  return gaussian_set(64, 64, var);
}

// T-M3B-C02-04 (protocollo §3.2): σ = 4 ADU, 64 × 64; K e seed per n, K_a = 256 per (a).
inline constexpr std::size_t c02_04_K_a = 256;

inline std::pair<std::uint64_t, std::size_t> c02_04_seed_K(std::size_t n) {
  switch (n) {
    case 3: return {2026110911, 2250};
    case 4: return {2026110912, 2000};
    case 9: return {2026110913, 24300};
    case 10: return {2026110914, 3400};
    case 101: return {2026110915, 256};
    default: refuse("n fuori da T-M3B-C02-04");
  }
}

inline frame_set c02_04_set(std::size_t n) {
  c02_04_seed_K(n);
  return gaussian_set(64, 64, n, q(16));
}

// T-M3B-C02-06 (protocollo §3.3): σ = 4 ADU, 64 × 64, K = 2000.
inline constexpr std::size_t c02_06_K = 2000;

inline std::uint64_t c02_06_seed(std::size_t n) {
  switch (n) {
    case 11: return 2026110921;
    case 21: return 2026110922;
    case 51: return 2026110923;
    default: refuse("n fuori da T-M3B-C02-06");
  }
}

inline frame_set c02_06_set(std::size_t n) {
  c02_06_seed(n);
  return gaussian_set(64, 64, n, q(16));
}

// ---------------------------------------------------------------- flat (C-02.b-flat §f.9)

inline std::size_t channel_of(bool cfa, std::uint32_t x, std::uint32_t y) { return cfa ? 2 * (y % 2) + (x % 2) : 0; }

inline std::vector<std::size_t> pixels_per_channel(std::uint32_t w, std::uint32_t h, bool cfa) {
  std::vector<std::size_t> count(cfa ? 4 : 1, 0);
  for (std::uint32_t y = 0; y < h; ++y) {
    for (std::uint32_t x = 0; x < w; ++x) ++count[channel_of(cfa, x, y)];
  }
  return count;
}

// L_c = ⌊P_c / 2⌋ + 1, il minimo con 2 L_c > P_c.
inline std::vector<std::size_t> majority_level_pixels(std::uint32_t w, std::uint32_t h, bool cfa) {
  auto count = pixels_per_channel(w, h, cfa);
  for (auto& c : count) c = c / 2 + 1;
  return count;
}

struct noise_params {
  rat gain;        // G, e⁻/ADU
  rat read_noise;  // R, ADU
};

struct flat_pixel {
  rat g;
  rat beta;
};

struct flat_set {
  frame_set set;
  bool cfa = false;
  rat offset;
  std::vector<std::vector<rat>> levels;  // [canale][frame] c_(f,c)
  std::vector<std::uint8_t> channel;     // per pixel
  std::vector<std::int32_t> region;      // per pixel: −1 regione di livello, altrimenti classe di misura
  std::vector<rat> g, beta;              // per pixel
  std::optional<noise_params> noise;

  std::size_t channels() const { return levels.size(); }
  std::size_t frames() const { return set.frames(); }
};

// Legge del j-esimo pixel di misura del canale c (ordine di riga): {classe ≥ 0, {g > 0, β}}.
using measure_fn = std::function<std::pair<std::int32_t, flat_pixel>(std::size_t c, std::size_t j, std::uint32_t x, std::uint32_t y)>;

inline flat_set make_flat(std::uint32_t w, std::uint32_t h, bool cfa, const rat& offset,
                          const std::vector<std::vector<rat>>& levels, const std::vector<std::size_t>& level_pixels,
                          const std::optional<noise_params>& noise, const measure_fn& measure) {
  const std::size_t channels = cfa ? 4 : 1;
  if (levels.size() != channels || level_pixels.size() != channels) refuse("livelli o regioni di livello non per canale");
  const std::size_t n = levels[0].size();
  for (const auto& lc : levels) {
    if (lc.size() != n) refuse("numero di livelli diverso fra canali");
    for (const auto& c : lc) {
      if (!(q(0) < c)) refuse("livello non positivo");
    }
  }
  if (noise && !(q(0) < noise->gain)) refuse("guadagno non positivo");
  const auto count = pixels_per_channel(w, h, cfa);
  for (std::size_t c = 0; c < channels; ++c) {
    if (2 * level_pixels[c] <= count[c] || level_pixels[c] > count[c]) refuse("regione di livello non oltre la metà del canale");
  }
  flat_set s;
  s.set = empty_set(w, h, n);
  s.cfa = cfa;
  s.offset = offset;
  s.levels = levels;
  s.noise = noise;
  const std::size_t P = s.set.pixels();
  s.channel.resize(P);
  s.region.resize(P);
  s.g.resize(P);
  s.beta.resize(P);
  std::vector<std::size_t> seen(channels, 0);
  for (std::uint32_t y = 0; y < h; ++y) {
    for (std::uint32_t x = 0; x < w; ++x) {
      const std::size_t p = std::size_t{y} * w + x;
      const std::size_t c = channel_of(cfa, x, y);
      const std::size_t idx = seen[c]++;
      s.channel[p] = static_cast<std::uint8_t>(c);
      if (idx < level_pixels[c]) {
        s.region[p] = -1;
        s.g[p] = q(1);
        s.beta[p] = q(0);
      } else {
        auto [cls, px] = measure(c, idx - level_pixels[c], x, y);
        if (cls < 0 || !(q(0) < px.g)) refuse("pixel di misura con classe negativa o g non positivo");
        s.region[p] = cls;
        s.g[p] = px.g;
        s.beta[p] = px.beta;
      }
      for (std::size_t f = 0; f < n; ++f) {
        const rat signal = levels[c][f] * s.g[p];
        rat var = q(0);
        if (noise && s.region[p] >= 0) var = signal / noise->gain + noise->read_noise * noise->read_noise;
        set_law(s.set, f, p, offset + s.beta[p] + signal, var);
      }
    }
  }
  return s;
}

// Classi a blocchi contigui di `per_class` pixel di misura, nell'ordine dato.
inline measure_fn class_measure(std::vector<flat_pixel> classes, std::size_t per_class) {
  return [classes = std::move(classes), per_class](std::size_t, std::size_t j, std::uint32_t, std::uint32_t) {
    const std::size_t k = j / per_class;
    if (k >= classes.size()) refuse("pixel di misura oltre le classi");
    return std::pair<std::int32_t, flat_pixel>{static_cast<std::int32_t>(k), classes[k]};
  };
}

// g ∈ {1/2, 3/2} × β ∈ {0, +A, −A}, in quest'ordine (protocollo §3.4, §3.5).
inline std::vector<flat_pixel> six_classes(const rat& A) {
  std::vector<flat_pixel> out;
  for (const rat& g : {q(1, 2), q(3, 2)}) {
    for (const rat& b : {q(0), A, q(0) - A}) out.push_back({g, b});
  }
  return out;
}

// c_f = 1000 + ⌊span · f / (n − 1)⌋, f = 0 … n − 1.
inline std::vector<rat> ramp_levels(std::size_t n, std::int64_t span) {
  if (n < 2) refuse("rampa di livelli con n < 2");
  std::vector<rat> out;
  for (std::size_t f = 0; f < n; ++f) out.push_back(q(1000 + span * static_cast<std::int64_t>(f) / static_cast<std::int64_t>(n - 1)));
  return out;
}

// Scale dei canali CFA (B2): s = (1, 5/4, 3/2, 2) per R, G1, G2, B; G1 e G2 a livelli diversi.
inline constexpr std::int64_t channel_scale_quarters[4] = {4, 5, 6, 8};

inline std::vector<std::vector<rat>> channel_levels(const std::vector<rat>& base, bool cfa) {
  std::vector<std::vector<rat>> out;
  for (std::size_t c = 0; c < (cfa ? 4u : 1u); ++c) {
    std::vector<rat> lc;
    for (const auto& b : base) lc.push_back(b * q(channel_scale_quarters[c], 4));
    out.push_back(lc);
  }
  return out;
}

struct flat_truth {
  std::vector<std::vector<rat>> level;  // m_(f,c) = c_(f,c)
  std::vector<std::vector<rat>> r;      // m_H,c / m_(f,c)
  std::vector<rat> m_h;                 // n / Σ_f 1/m_(f,c), uguale a c_H,c
  std::vector<rat> rho;                 // max_f |r_(f,c) − 1|
  std::vector<rat> lambda;              // (1/n) Σ_f r_(f,c) c_(f,c)
};

inline flat_truth truth_of_levels(const std::vector<std::vector<rat>>& levels) {
  flat_truth t;
  t.level = levels;
  for (const auto& lc : levels) {
    const auto n = static_cast<std::int64_t>(lc.size());
    rat inv = q(0);
    for (const auto& m : lc) inv = inv + q(1) / m;
    const rat m_h = q(n) / inv;
    std::vector<rat> r;
    rat rho = q(0), sum_rc = q(0);
    for (const auto& m : lc) {
      r.push_back(m_h / m);
      const rat dev = exact::abs(r.back() - q(1));
      if (rho < dev) rho = dev;
      sum_rc = sum_rc + r.back() * m;
    }
    t.r.push_back(r);
    t.m_h.push_back(m_h);
    t.rho.push_back(rho);
    t.lambda.push_back(sum_rc / q(n));
  }
  return t;
}

inline flat_truth truth_of(const flat_set& s) { return truth_of_levels(s.levels); }

// y_f = offset + r_f (x_f − offset): E[y_f | r] = offset + r_f (β + c_f g), Var(y_f | r) = r_f² σ_f².
inline moments normalized_law(const flat_set& s, const flat_truth& t, std::size_t f, std::size_t p) {
  const std::size_t c = s.channel[p];
  const rat& r = t.r[c][f];
  return {s.offset + r * (s.beta[p] + s.levels[c][f] * s.g[p]), r * r * s.set.var_exact(f, p)};
}

// b + λ_c g (C-02.b-flat §f.1, fatto 1), con b = offset + β.
inline rat global_center(const flat_set& s, const flat_truth& t, std::size_t p) {
  return s.offset + s.beta[p] + t.lambda[s.channel[p]] * s.g[p];
}

// T-M3B-FLAT-03 (protocollo §3.4): mono 128 × 128, regione di livello di 8194 pixel, 6 classi da 1365, n = 4.
inline constexpr std::uint64_t flat_03_seed = 2026110931;
inline constexpr std::size_t flat_03_K = 4000;

inline flat_set flat_03_set() {
  const std::size_t per = 1365;
  return make_flat(128, 128, false, q(512), {{q(1000), q(1500), q(2000), q(3000)}}, {128 * 128 - 6 * per},
                   noise_params{q(1), q(5)}, class_measure(six_classes(q(32)), per));
}

// T-M3B-FLAT-04 (protocollo §3.5): mono 64 × 64, regione di livello di 2050 pixel, 6 classi da 341, K = 2000.
inline constexpr std::size_t flat_04_K = 2000;

inline std::uint64_t flat_04_seed(std::size_t n) {
  switch (n) {
    case 3: return 2026110941;
    case 9: return 2026110942;
    case 11: return 2026110943;
    case 31: return 2026110944;
    default: refuse("n fuori da T-M3B-FLAT-04");
  }
}

inline std::vector<rat> flat_04_levels(std::size_t n) {
  flat_04_seed(n);
  if (n == 3) return {q(1000), q(2000), q(4000)};
  return ramp_levels(n, 3000);
}

inline flat_set flat_04_set(std::size_t n) {
  const std::size_t per = 341;
  return make_flat(64, 64, false, q(512), {flat_04_levels(n)}, {64 * 64 - 6 * per}, noise_params{q(1), q(5)},
                   class_measure(six_classes(q(32)), per));
}

// T-M3B-FLAT-05 (protocollo §3.6): mono 64 × 64, regione di livello di 2050 pixel, classi g ∈ {1/2, 3/2} da 1023,
// β = 0; ϱ in decimi (10ϱ ∈ {10, 15, 20, 30, 40}); regime 0 fotonico (G = 1, R = 2), 1 di lettura (G = 16, R = 50).
inline constexpr std::size_t flat_05_K = 256;

inline void flat_05_check(std::size_t n, std::int64_t rho_tenths, int regime) {
  const bool n_ok = n == 3 || n == 9 || n == 31;
  const bool r_ok = rho_tenths == 10 || rho_tenths == 15 || rho_tenths == 20 || rho_tenths == 30 || rho_tenths == 40;
  if (!n_ok || !r_ok || (regime != 0 && regime != 1)) refuse("cella fuori da T-M3B-FLAT-05");
}

inline std::uint64_t flat_05_seed(std::size_t n, std::int64_t rho_tenths, int regime) {
  flat_05_check(n, rho_tenths, regime);
  return 2026110950000ULL + 1000 * n + 10 * static_cast<std::uint64_t>(rho_tenths) + static_cast<std::uint64_t>(regime);
}

inline std::vector<rat> flat_05_levels(std::size_t n, std::int64_t rho_tenths) {
  flat_05_check(n, rho_tenths, 0);
  return ramp_levels(n, (rho_tenths - 10) * 100);
}

inline flat_set flat_05_set(std::size_t n, std::int64_t rho_tenths, int regime) {
  flat_05_check(n, rho_tenths, regime);
  const std::size_t per = 1023;
  const noise_params noise = regime == 0 ? noise_params{q(1), q(2)} : noise_params{q(16), q(50)};
  return make_flat(64, 64, false, q(512), {flat_05_levels(n, rho_tenths)}, {64 * 64 - 2 * per}, noise,
                   class_measure({{q(1, 2), q(0)}, {q(3, 2), q(0)}}, per));
}

// ---------------------------------------------------------------- casi dei test E e B (protocollo §4, derivati in B2)
// Un flusso per test, dal seed del protocollo; i casi si realizzano una volta ciascuno, nell'ordine dichiarato.

struct gaussian_cases {
  std::uint64_t seed = 0;
  std::vector<std::size_t> n;
  std::vector<frame_set> sets;
};

struct flat_cases {
  std::uint64_t seed = 0;
  std::vector<std::string> names;
  std::vector<flat_set> sets;
};

inline gaussian_cases standard_cases(std::uint64_t seed, std::uint32_t w, std::uint32_t h, std::vector<std::size_t> ns, bool outliers) {
  gaussian_cases c{seed, std::move(ns), {}};
  for (std::size_t n : c.n) {
    c.sets.push_back(gaussian_set(w, h, n, q(16)));
    if (outliers) add_standard_outliers(c.sets.back(), q(4));
  }
  return c;
}

// T-M3B-C02-01: 16 × 16, σ = 4, senza outlier.
inline gaussian_cases c02_01_cases() { return standard_cases(2026110961, 16, 16, {3, 4, 5, 9, 10, 11, 101}, false); }
// T-M3B-C02-02: 32 × 32, σ = 4, outlier standard con step = σ.
inline gaussian_cases c02_02_cases() { return standard_cases(2026110962, 32, 32, {11, 15, 31}, true); }
// T-M3B-C02-10: 37 × 71 (tile_rows 7 e 64 lasciano un tassello parziale), outlier standard.
inline gaussian_cases c02_10_cases() { return standard_cases(2026110963, 37, 71, {3, 4, 11}, true); }

// T-M3B-C02-11: 16 × 16, outlier standard; i pixel con p mod 16 = 8 (classe senza outlier) non hanno rumore: s = 0.
inline gaussian_cases c02_11_cases() {
  auto c = standard_cases(2026110964, 16, 16, {3, 4, 11}, true);
  for (auto& s : c.sets) {
    for (std::size_t f = 0; f < s.frames(); ++f) {
      for (std::size_t p = 8; p < s.pixels(); p += 16) set_law(s, f, p, standard_mean(p), q(0));
    }
  }
  return c;
}

// Regione di misura dei flat di T-M3B-FLAT-01, 07, 08: g = 1/2 + ((x + 2y) mod 9)/8, β = ((3x + 5y) mod 65) − 32 ADU,
// classe 0.
inline measure_fn pattern_measure() {
  return [](std::size_t, std::size_t, std::uint32_t x, std::uint32_t y) {
    return std::pair<std::int32_t, flat_pixel>{
        0, {q(4 + (x + 2 * y) % 9, 8), q(static_cast<std::int64_t>((3 * x + 5 * y) % 65) - 32)}};
  };
}

// Livelli c_(f,c) = s_c (1000 + ⌊3000 f/(n − 1)⌋), offset 512, G = 1, R = 5.
inline flat_set pattern_flat(std::uint32_t w, std::uint32_t h, bool cfa, std::size_t n) {
  return make_flat(w, h, cfa, q(512), channel_levels(ramp_levels(n, 3000), cfa), majority_level_pixels(w, h, cfa),
                   noise_params{q(1), q(5)}, pattern_measure());
}

inline flat_cases pattern_cases(std::uint64_t seed, std::uint32_t w, std::uint32_t h, const std::vector<std::size_t>& ns) {
  flat_cases c{seed, {}, {}};
  for (std::size_t n : ns) {
    for (bool cfa : {false, true}) {
      c.names.push_back("n=" + std::to_string(n) + (cfa ? "/cfa" : "/mono"));
      c.sets.push_back(pattern_flat(w, h, cfa, n));
    }
  }
  return c;
}

inline flat_cases flat_01_cases() { return pattern_cases(2026110965, 16, 16, {3, 4, 9, 11}); }
inline flat_cases flat_07_cases() { return pattern_cases(2026110967, 16, 16, {3, 4, 11}); }
inline flat_cases flat_08_cases() { return pattern_cases(2026110968, 37, 71, {3, 4, 11}); }

// T-M3B-FLAT-02 (ν = 0), 16 × 16, mono e CFA. Parametri estratti da un `std::mt19937_64` col seed del protocollo con
// `uniform_int`, nell'ordine: casi; per caso livelli (canale, frame), poi pixel di misura in ordine di riga (g, poi β).
// Livelli: c_(0,c) = lo_c, c_(1,c) = ratio · lo_c, gli altri interi uniformi in [lo_c, ratio · lo_c], lo_c = 1000 s_c.
// g = 1/2 + k/1024, k in [0, 1024]; β = j/16, j in [−512, 512] (|β| ≤ 32). (a) ratio 16, n ∈ {2, 3, 8}, β estratto;
// (b) ratio 4, n ∈ {3, 9, 11}, β ≡ 0. Campioni esatti in f64: le operazioni non esatte sono quelle delle costanti.
inline flat_cases flat_02_cases() {
  flat_cases c{2026110966, {}, {}};
  std::mt19937_64 e(c.seed);
  struct part {
    const char* name;
    std::int64_t ratio;
    std::vector<std::size_t> ns;
    bool beta;
  };
  for (const part& pt : {part{"a", 16, {2, 3, 8}, true}, part{"b", 4, {3, 9, 11}, false}}) {
    for (std::size_t n : pt.ns) {
      for (bool cfa : {false, true}) {
        std::vector<std::vector<rat>> levels;
        for (std::size_t ch = 0; ch < (cfa ? 4u : 1u); ++ch) {
          const std::int64_t lo = 250 * channel_scale_quarters[ch];
          std::vector<rat> lc{q(lo), q(pt.ratio * lo)};
          for (std::size_t f = 2; f < n; ++f) lc.push_back(q(uniform_int(e, lo, pt.ratio * lo)));
          levels.push_back(lc);
        }
        const bool draw_beta = pt.beta;
        measure_fn m = [&e, draw_beta](std::size_t, std::size_t, std::uint32_t, std::uint32_t) {
          const rat g = q(512 + uniform_int(e, 0, 1024), 1024);
          const rat b = draw_beta ? q(uniform_int(e, -512, 512), 16) : q(0);
          return std::pair<std::int32_t, flat_pixel>{0, {g, b}};
        };
        c.names.push_back(std::string(pt.name) + "/n=" + std::to_string(n) + (cfa ? "/cfa" : "/mono"));
        c.sets.push_back(make_flat(16, 16, cfa, q(512), levels, majority_level_pixels(16, 16, cfa), std::nullopt, m));
      }
    }
  }
  return c;
}

inline std::vector<frames> realize_cases(const gaussian_cases& c) {
  gauss_stream g(c.seed);
  std::vector<frames> out;
  for (const auto& s : c.sets) out.push_back(realize(s, g));
  return out;
}

inline std::vector<frames> realize_cases(const flat_cases& c) {
  gauss_stream g(c.seed);
  std::vector<frames> out;
  for (const auto& s : c.sets) out.push_back(realize(s.set, g));
  return out;
}

// ---------------------------------------------------------------- B, D, DF, F con verità nota

// Bias `offset + β_s`, dark `offset + β_s + d t_dark`, dark-flat `offset + β_s + d t_flat` (= b del flat), con
// varianza R² (nessun rumore termico); flat `b + c_(f,c) g` con σ² = c g / G + R². Struttura (B2):
// β_s = ((7x + 13y) mod 17) − 8, d = ((3x + 5y) mod 8)/8 ADU/s, g = 1/2 + ((x + y) mod 32)/32,
// c_(f,c) = 8192 s_c (1 + (f mod 8)/64); β_s = d = 0 nella regione di livello del flat. Senza rumore ogni campione è
// intero (convertibile in RAW).
struct calibration_set {
  frame_set bias, dark, dark_flat;
  flat_set flat;
  std::vector<rat> structure;  // β_s per pixel
  std::vector<rat> dark_rate;  // d per pixel, ADU/s
  rat offset = q(512);
  rat t_dark = q(64);
  rat t_flat = q(8);
  rat gain = q(1);
  rat read_noise = q(5);
};

inline calibration_set make_calibration_set(std::uint32_t w, std::uint32_t h, bool cfa, std::size_t n, bool noise) {
  calibration_set cs;
  const std::size_t P = std::size_t{w} * h;
  cs.structure.assign(P, q(0));
  cs.dark_rate.assign(P, q(0));
  std::vector<rat> base;
  for (std::size_t f = 0; f < n; ++f) base.push_back(q(8192) * q(static_cast<std::int64_t>(64 + f % 8), 64));
  measure_fn m = [&cs, w](std::size_t, std::size_t, std::uint32_t x, std::uint32_t y) {
    const std::size_t p = std::size_t{y} * w + x;
    cs.structure[p] = q(static_cast<std::int64_t>((7 * x + 13 * y) % 17) - 8);
    cs.dark_rate[p] = q((3 * x + 5 * y) % 8, 8);
    return std::pair<std::int32_t, flat_pixel>{0, {q(16 + (x + y) % 32, 32), cs.structure[p] + cs.dark_rate[p] * cs.t_flat}};
  };
  std::optional<noise_params> np;
  if (noise) np = noise_params{cs.gain, cs.read_noise};
  cs.flat = make_flat(w, h, cfa, cs.offset, channel_levels(base, cfa), majority_level_pixels(w, h, cfa), np, m);
  const rat r2 = noise ? cs.read_noise * cs.read_noise : q(0);
  cs.bias = empty_set(w, h, n);
  cs.dark = cs.bias;
  cs.dark_flat = cs.bias;
  for (std::size_t p = 0; p < P; ++p) {
    const rat b = cs.offset + cs.structure[p];
    for (std::size_t f = 0; f < n; ++f) {
      set_law(cs.bias, f, p, b, r2);
      set_law(cs.dark, f, p, b + cs.dark_rate[p] * cs.t_dark, r2);
      set_law(cs.dark_flat, f, p, b + cs.dark_rate[p] * cs.t_flat, r2);
    }
  }
  return cs;
}

// ---------------------------------------------------------------- frame RAW (FIX-SYN-DNG-01)

// DNG CFA RGGB a 16 bit: la posizione 2 (y mod 2) + (x mod 2) del motivo è il canale dei flat CFA (R, G1, G2, B).
// Ogni campione deve essere un intero in [0, white]; altrimenti la conversione si ferma (nessun arrotondamento).
inline dng_spec raw_frame(const std::vector<double>& x, std::uint32_t w, std::uint32_t h, std::uint16_t black,
                          std::uint16_t white, std::pair<std::uint32_t, std::uint32_t> exposure, std::uint16_t iso) {
  if (x.size() != std::size_t{w} * h) refuse("frame RAW di dimensione diversa da w × h");
  std::vector<std::uint16_t> v;
  v.reserve(x.size());
  for (double s : x) {
    if (!(s >= 0 && s <= white) || s != std::floor(s)) refuse("campione RAW non intero in [0, white]");
    v.push_back(static_cast<std::uint16_t>(s));
  }
  dng_spec d;
  d.width = w;
  d.height = h;
  d.cfa = {0, 1, 1, 2};
  d.black = {black};
  d.white = white;
  d.exposure_time = exposure;
  d.iso = iso;
  d.pixels = pixels_u16(v);
  return d;
}

}  // namespace astro_fixture::syn_cal
