#pragma once

// Oracolo HEALPix NESTED dei test di T-M3A-PACK-06 (plans/M3a.md, U11), indipendente dal prodotto: non include
// intestazioni di astro e non ne condivide il codice.
//
// Geometria (Górski et al. 2005, ApJ 622, 759, §4): 12 celle di base di area uguale; calotte polari |z| > 2/3 e fascia
// equatoriale |z| <= 2/3; bordi delle celle nella fascia sulle rette z = 2/3 - 4k/(3N) ± 8φ/(3π), nelle calotte sulle
// curve 1 - |z| = k²/(3N²) (π/(2φ_t))² e 1 - |z| = k²/(3N²) (π/(2φ_t - π))², con φ_t = φ mod π/2. Nella proiezione
// HEALPix (unità π/4: X = φ/(π/4), Y = 3z/2 nella fascia; nelle calotte σ = sqrt(3(1 - |z|)), settore k = ⌊φ/(π/2)⌋,
// X = (2k+1) + (φ/(π/4) - (2k+1)) σ, Y = ±(2 - σ)) quelle curve diventano i lati di quadrati ruotati di 45°: ogni
// cella di base è il quadrato max(|ΔX + ΔY|, |ΔX - ΔY|) <= 1 attorno al suo centro, con centri (2f+1, 1) a nord,
// (2f, 0) all'equatore, (2f+1, -1) a sud, f = 0..3. Dentro la cella di base, la coordinata u = ΔX + ΔY cresce verso
// il vertice est e v = ΔY - ΔX verso il vertice ovest; ix = ⌊N(u+1)/2⌋, iy = ⌊N(v+1)/2⌋. Numerazione annidata della
// Fig. 4: figli 0 sud, 1 est, 2 ovest, 3 nord, cioè ix nei bit pari e iy nei bit dispari dell'indice dentro la cella.
// Centri delle celle (§4, anelli): calotta nord, anello i = 1..N-1: z = 1 - i²/(3N²), φ = π/(2i)(j - 1/2), j = 1..4i;
// fascia, i = N..3N: z = 4/3 - 2i/(3N), φ = π/(2N)(j - s/2), s = (i - N + 1) mod 2, j = 1..4N; calotta sud
// simmetrica.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

namespace healpix_oracle {

constexpr double pi = 3.14159265358979323846;
constexpr double deg = pi / 180.0;

struct vec3 {
  double x = 0, y = 0, z = 0;
};

inline vec3 operator*(double s, const vec3& a) { return {s * a.x, s * a.y, s * a.z}; }
inline vec3 operator+(const vec3& a, const vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline double dot(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline vec3 cross(const vec3& a, const vec3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double norm(const vec3& a) { return std::sqrt(dot(a, a)); }
inline vec3 unit(const vec3& a) { return (1.0 / norm(a)) * a; }

inline vec3 from_z_phi(double z, double phi) {
  const double s = std::sqrt(std::max(0.0, 1.0 - z * z));
  return {s * std::cos(phi), s * std::sin(phi), z};
}
inline vec3 from_radec_deg(double ra_deg, double dec_deg) { return from_z_phi(std::sin(dec_deg * deg), ra_deg * deg); }

// Angolo fra due direzioni con atan2: preciso anche per angoli piccoli e vicini a π.
inline double angle(const vec3& a, const vec3& b) { return std::atan2(norm(cross(a, b)), dot(a, b)); }

inline double z_of(const vec3& v) { return std::clamp(v.z / norm(v), -1.0, 1.0); }
inline double phi_of(const vec3& v) {
  double p = std::atan2(v.y, v.x);
  if (p < 0) p += 2 * pi;
  if (p >= 2 * pi) p -= 2 * pi;
  return p;
}

inline std::uint64_t nside(int order) { return std::uint64_t{1} << order; }
inline std::uint64_t cells(int order) { return std::uint64_t{12} << (2 * order); }

inline std::uint64_t interleave(std::uint64_t ix, std::uint64_t iy) {
  std::uint64_t out = 0;
  for (int b = 0; b < 32; ++b) {
    out |= ((ix >> b) & 1u) << (2 * b);
    out |= ((iy >> b) & 1u) << (2 * b + 1);
  }
  return out;
}

inline std::int64_t mod4(double v) {
  const auto i = static_cast<std::int64_t>(v);
  return ((i % 4) + 4) % 4;
}

inline std::uint64_t local_index(double n, double c) {
  const double i = std::floor(n * (c + 1.0) / 2.0);
  return static_cast<std::uint64_t>(std::clamp(i, 0.0, n - 1.0));
}

// Cella NESTED che contiene la direzione (z, φ), φ in [0, 2π).
inline std::uint64_t ang2pix_nest(int order, double z, double phi) {
  const double n = static_cast<double>(nside(order));
  const double t = phi / (pi / 4);
  std::int64_t face = 0;
  double u = 0, v = 0;
  if (std::fabs(z) <= 2.0 / 3.0) {
    const double X = t, Y = 1.5 * z;
    const double p = X + Y, m = X - Y;
    const double pc = 2 * std::floor((p + 1) / 2), mc = 2 * std::floor((m + 1) / 2);
    if (pc == mc + 2) {
      face = mod4(mc / 2);
    } else if (pc == mc) {
      face = 4 + mod4(pc / 2);
    } else if (mc == pc + 2) {
      face = 8 + mod4(pc / 2);
    } else {
      throw std::logic_error("oracolo HEALPix: punto della fascia fuori da ogni cella di base");
    }
    u = p - pc;
    v = mc - m;
  } else {
    const double sigma = std::sqrt(3.0 * (1.0 - std::fabs(z)));
    const double k = std::min(3.0, std::floor(t / 2));
    const double c = 2 * k + 1;
    const double X = c + (t - c) * sigma;
    const double Y = z > 0 ? 2 - sigma : sigma - 2;
    const double yc = z > 0 ? 1.0 : -1.0;
    face = static_cast<std::int64_t>(k) + (z > 0 ? 0 : 8);
    u = (X - c) + (Y - yc);
    v = (Y - yc) - (X - c);
  }
  const std::uint64_t ix = local_index(n, u), iy = local_index(n, v);
  return static_cast<std::uint64_t>(face) * nside(order) * nside(order) + interleave(ix, iy);
}

inline std::uint64_t ang2pix_nest(int order, const vec3& v) { return ang2pix_nest(order, z_of(v), phi_of(v)); }

// Centri di tutte le celle dagli anelli di Górski et al. 2005 §4, nell'ordine degli anelli (non NESTED).
inline std::vector<vec3> ring_centers(int order) {
  const auto N = static_cast<std::int64_t>(nside(order));
  const double n = static_cast<double>(N);
  std::vector<vec3> out;
  for (std::int64_t i = 1; i < N; ++i) {
    const double z = 1.0 - static_cast<double>(i * i) / (3.0 * n * n);
    for (std::int64_t j = 1; j <= 4 * i; ++j) {
      const double phi = pi / (2.0 * static_cast<double>(i)) * (static_cast<double>(j) - 0.5);
      out.push_back(from_z_phi(z, phi));
      out.push_back(from_z_phi(-z, phi));
    }
  }
  for (std::int64_t i = N; i <= 3 * N; ++i) {
    const double z = 4.0 / 3.0 - 2.0 * static_cast<double>(i) / (3.0 * n);
    const double s = static_cast<double>((i - N + 1) % 2);
    for (std::int64_t j = 1; j <= 4 * N; ++j) out.push_back(from_z_phi(z, pi / (2.0 * n) * (static_cast<double>(j) - s / 2.0)));
  }
  return out;
}

// Centro di ogni cella NESTED: il centro d'anello che l'oracolo assegna a quella cella. Fallisce se due centri cadono
// nella stessa cella o una cella resta senza centro (biiezione fra centri d'anello e celle NESTED).
inline std::vector<vec3> nest_centers(int order) {
  std::vector<vec3> out(cells(order));
  std::vector<bool> seen(cells(order), false);
  for (const vec3& c : ring_centers(order)) {
    const std::uint64_t pix = ang2pix_nest(order, c);
    if (pix >= cells(order) || seen[pix]) throw std::logic_error("oracolo HEALPix: centri d'anello non biiettivi");
    seen[pix] = true;
    out[pix] = c;
  }
  if (std::find(seen.begin(), seen.end(), false) != seen.end()) throw std::logic_error("oracolo HEALPix: cella senza centro");
  return out;
}

// Raggio massimo di una cella all'ordine dato (r_upper): distanza fra il centro della cella sull'anello z = 2/3 (i = N,
// j = 1: φ = π/(4N)) e il suo vertice nord (z = 1 - (N-1)²/(3N²), φ = 0). Ipotesi dichiarata, non dimostrata qui:
// che sia la distanza centro-punto più grande di tutta la sfera (è la scelta della funzione max_pixrad della libreria
// HEALPix). Il test la controlla solo sui campioni: nessuna distanza campionata la supera e la più grande è entro 1°.
// Le classi che ne dipendono hanno margine: la classe `inside` usa 2 r_upper dove ne basterebbe 1 più il raggio di
// una cella (vedi `region_probe`).
inline double max_cell_radius(int order) {
  const double n = static_cast<double>(nside(order));
  const vec3 center = from_z_phi(2.0 / 3.0, pi / (4.0 * n));
  const vec3 vertex = from_z_phi(1.0 - (n - 1) * (n - 1) / (3.0 * n * n), 0.0);
  return angle(center, vertex);
}

// Punti deterministici quasi uniformi sulla sfera (reticolo di Fibonacci).
inline std::vector<vec3> fibonacci_sphere(std::size_t count) {
  std::vector<vec3> out;
  out.reserve(count);
  const double golden = pi * (3.0 - std::sqrt(5.0));
  for (std::size_t i = 0; i < count; ++i) {
    const double z = 1.0 - (2.0 * static_cast<double>(i) + 1.0) / static_cast<double>(count);
    out.push_back(from_z_phi(z, std::fmod(golden * static_cast<double>(i), 2 * pi)));
  }
  return out;
}

// Massima distanza osservata fra un campione e il centro della sua cella: limite inferiore del raggio massimo vero.
inline double max_center_distance(int order, const std::vector<vec3>& samples) {
  const std::vector<vec3> centers = nest_centers(order);
  double worst = 0;
  for (const vec3& s : samples) worst = std::max(worst, angle(s, centers[ang2pix_nest(order, s)]));
  return worst;
}

// Visita i campioni della corona d0 < d <= d1 attorno a `center` (d0 < 0: compreso il centro), con passo `step` in
// raggio e lungo ogni cerchio; d1 è sempre campionato. Il visitatore ritorna false per fermare la visita.
inline bool visit_annulus(const vec3& center, double d0, double d1, double step,
                          const std::function<bool(const vec3&)>& visit) {
  const vec3 c = unit(center);
  const vec3 ref = std::fabs(c.z) < 0.9 ? vec3{0, 0, 1} : vec3{1, 0, 0};
  const vec3 e1 = unit(cross(c, ref));
  const vec3 e2 = cross(c, e1);
  d1 = std::min(d1, pi);
  std::vector<double> radii;
  if (d0 < 0) radii.push_back(0.0);
  const double first = d0 < 0 ? step : d0 + step;
  for (double d = first; d < d1; d += step) radii.push_back(d);
  if (d1 > std::max(d0, 0.0) || d0 < 0) radii.push_back(d1);
  for (const double d : radii) {
    const double ring = 2 * pi * std::sin(d);
    const auto around = std::max<std::size_t>(d == 0.0 ? 1 : 8, static_cast<std::size_t>(std::ceil(ring / step)));
    for (std::size_t k = 0; k < around; ++k) {
      const double b = 2 * pi * static_cast<double>(k) / static_cast<double>(around);
      const vec3 p = std::cos(d) * c + std::sin(d) * (std::cos(b) * e1 + std::sin(b) * e2);
      if (!visit(p)) return false;
    }
  }
  return true;
}

// Classi della regione di T-M3A-PACK-06 rispetto a una copertura (criterio fissato in plans/M3a.md, U11):
// - outside: almeno un campione della calotta in una cella fuori copertura (esterna se tutti, a cavallo se alcuni);
// - edge: calotta tutta coperta, ma un campione entro R + r_lower in una cella fuori copertura: quella cella interseca
//   la regione allargata del raggio massimo (r_lower <= raggio massimo vero), quindi §3.3 punto 7 impone lo stop;
// - inside: calotta allargata di due raggi massimi (2 r_upper) tutta in copertura;
// - undecided: nessuna delle precedenti, il contratto ammette entrambi gli esiti.
// Giustificazione e limiti. Le classi di stop sono esatte rispetto all'oracolo: un campione trovato fuori copertura è
// un punto della regione (allargata di r_lower, limite inferiore campionato del raggio massimo vero) dentro una cella
// non coperta. La classe `inside` è una verifica discreta (passo `step`): una cella non coperta che interseca la
// regione allargata del raggio massimo r ha il centro entro R + 2r, quindi una parte della cella attorno al centro
// cade entro R + 2 r_upper; l'ipotesi è che quella parte sia più larga del passo (celle di ordine 1–2, lati di
// 7°–30°, passo 0,5°), e che r_upper non sia minore del raggio massimo vero (`max_cell_radius`).
enum class region_class { exterior, straddling, edge, inside, undecided };

struct region_probe {
  int order = 0;
  std::vector<bool> covered;  // per cella NESTED
  double r_lower = 0, r_upper = 0, step = 0;

  bool in(const vec3& p) const { return covered[ang2pix_nest(order, p)]; }

  region_class classify(const vec3& center, double radius) const {
    std::size_t inside_count = 0, outside_count = 0;
    visit_annulus(center, -1, radius, step, [&](const vec3& p) {
      (in(p) ? inside_count : outside_count) += 1;
      return true;
    });
    if (outside_count != 0) return inside_count == 0 ? region_class::exterior : region_class::straddling;
    const auto all_in = [&](double d0, double d1) {
      return visit_annulus(center, d0, d1, step, [&](const vec3& p) { return in(p); });
    };
    if (!all_in(radius, radius + r_lower)) return region_class::edge;
    if (!all_in(radius + r_lower, radius + 2 * r_upper)) return region_class::undecided;
    return region_class::inside;
  }
};

}  // namespace healpix_oracle
