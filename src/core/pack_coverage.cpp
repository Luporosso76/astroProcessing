#include <algorithm>
#include <cmath>

#include "astro/error.hpp"
#include "astro/pack.hpp"

namespace astro {

namespace {

constexpr double pi = 3.14159265358979323846;
constexpr double deg = pi / 180.0;
// Margine sull'arrotondamento in virgola mobile, sempre nel verso che rifiuta. Ipotesi non derivata: gli angoli si
// calcolano con atan2 sul prodotto vettoriale e la componente trasversale delle direzioni non si ricava da z, quindi
// l'errore atteso e' di pochi ulp (ordine 1e-15 rad) anche ai poli; non c'e' un limite d'errore dimostrato per
// max_pixrad e per le conversioni ang-vec a ordine 29, dove una cella e' larga circa 3e-9 rad e il margine ne e' una
// frazione non trascurabile.
constexpr double rounding_margin_rad = 1e-9;

struct vec3 {
  double x, y, z;
};

// Direzione da z = cos(colatitudine), s = sin(colatitudine) e longitudine; s e' passata a parte perche' vicino ai poli
// 1 - z*z perde la componente trasversale.
vec3 from_z_s_phi(double z, double s, double phi) { return {s * std::cos(phi), s * std::sin(phi), z}; }

// Per una calotta polare con q = 1 - |z| (esatto nel prodotto del chiamante): s = sqrt(q (2 - q)).
double polar_s(double q) { return std::sqrt(q * (2.0 - q)); }

double angle_between(const vec3& a, const vec3& b) {
  const vec3 c{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
  return std::atan2(std::sqrt(c.x * c.x + c.y * c.y + c.z * c.z), a.x * b.x + a.y * b.y + a.z * b.z);
}

// Distanza fra il centro della cella sull'anello z = 2/3 (primo pixel dell'anello) e il suo vertice nord: il valore di
// max_pixrad della libreria HEALPix.
double max_cell_radius(std::uint32_t order) {
  const double n = static_cast<double>(std::uint64_t{1} << order);
  const double q = (n - 1) * (n - 1) / (3.0 * n * n);
  return angle_between(from_z_s_phi(2.0 / 3.0, std::sqrt(5.0) / 3.0, pi / (4.0 * n)), from_z_s_phi(1.0 - q, polar_s(q), 0.0));
}

// Centro di una cella NESTED (Górski et al. 2005, §4; pix2loc di HEALPix).
vec3 nested_center(std::uint32_t order, std::uint64_t pix) {
  static const int jrll[12] = {2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4};
  static const int jpll[12] = {1, 3, 5, 7, 0, 2, 4, 6, 1, 3, 5, 7};
  const std::int64_t n = std::int64_t{1} << order;
  const std::uint64_t face = pix >> (2 * order), inner = pix & ((std::uint64_t{1} << (2 * order)) - 1);
  std::int64_t ix = 0, iy = 0;
  for (std::uint32_t b = 0; b < order; ++b) {
    ix |= static_cast<std::int64_t>((inner >> (2 * b)) & 1u) << b;
    iy |= static_cast<std::int64_t>((inner >> (2 * b + 1)) & 1u) << b;
  }
  const std::int64_t jr = (std::int64_t{jrll[face]} << order) - ix - iy - 1;
  const double fact2 = 4.0 / (12.0 * static_cast<double>(n) * static_cast<double>(n));
  std::int64_t nr = n, kshift = 0;
  double z = 0, s = 0;
  if (jr < n || jr > 3 * n) {
    nr = jr < n ? jr : 4 * n - jr;
    const double q = static_cast<double>(nr * nr) * fact2;
    z = jr < n ? 1.0 - q : q - 1.0;
    s = polar_s(q);
  } else {
    z = static_cast<double>(2 * n - jr) * (2.0 * static_cast<double>(n) * fact2);
    s = std::sqrt((1.0 - z) * (1.0 + z));
    kshift = (jr - n) & 1;
  }
  std::int64_t jp = (std::int64_t{jpll[face]} * nr + ix - iy + 1 + kshift) / 2;
  if (jp > 4 * n) jp -= 4 * n;
  if (jp < 1) jp += 4 * n;
  return from_z_s_phi(z, s, (static_cast<double>(jp) - (static_cast<double>(kshift) + 1.0) * 0.5) * (pi / 2.0) / static_cast<double>(nr));
}

struct search {
  const std::vector<std::uint64_t>& covered;
  std::uint32_t leaf_order;
  vec3 center;
  double reach;  // R + 2 r_max alle foglie
  std::vector<double> radius;

  // Cerca una cella fuori da coverage il cui centro cada entro `reach`: una cella che interseca la calotta allargata
  // di r_max ha il centro entro R + 2 r_max. Un nodo ha tutti i centri dei figli entro il proprio raggio massimo.
  bool hole_near(std::uint32_t order, std::uint64_t pix) const {
    const std::uint32_t depth = leaf_order - order;
    const std::uint64_t first = pix << (2 * depth), last = (pix + 1) << (2 * depth);
    const auto count = static_cast<std::uint64_t>(std::lower_bound(covered.begin(), covered.end(), last) -
                                                  std::lower_bound(covered.begin(), covered.end(), first));
    if (count == last - first) return false;
    const double d = angle_between(center, nested_center(order, pix));
    if (d - radius[order] > reach) return false;
    if (depth == 0) return d <= reach;
    for (std::uint64_t k = 0; k < 4; ++k) {
      if (hole_near(order + 1, 4 * pix + k)) return true;
    }
    return false;
  }
};

}  // namespace

void verify_pack_coverage(const verified_pack& pack, const sky_cap& region) {
  if (!std::isfinite(region.ra_deg) || !std::isfinite(region.dec_deg) || !std::isfinite(region.radius_deg) ||
      region.radius_deg < 0 || std::fabs(region.dec_deg) > 90.0) {
    throw stop_error(error_category::invalid_input, "pack.region_invalid",
                     "regione con un campo non finito, raggio negativo o |dec| > 90");
  }
  const std::uint32_t order = pack.manifest.coverage_order;
  const double dec = region.dec_deg * deg;
  const vec3 center = from_z_s_phi(std::sin(dec), std::cos(dec), std::fmod(region.ra_deg, 360.0) * deg);
  search s{pack.manifest.coverage_cells, order, center, 0, {}};
  for (std::uint32_t o = 0; o <= order; ++o) s.radius.push_back(max_cell_radius(o));
  s.reach = region.radius_deg * deg + 2.0 * s.radius[order] + rounding_margin_rad;
  for (std::uint64_t base = 0; base < 12; ++base) {
    if (s.hole_near(0, base)) {
      throw stop_error(error_category::incompatible, "pack.out_of_coverage", "regione fuori dalla copertura del pack",
                       {{"ra_deg", std::to_string(region.ra_deg)},
                        {"dec_deg", std::to_string(region.dec_deg)},
                        {"raggio_deg", std::to_string(region.radius_deg)}});
    }
  }
}

}  // namespace astro
