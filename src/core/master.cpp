#include "astro/master.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <limits>
#include <sstream>
#include <type_traits>

#include "astro/calibration_analysis.hpp"
#include "astro/error.hpp"
#include "astro/operations.hpp"
#include "astro/raw_decode.hpp"
#include "astro/scheduler.hpp"
#include "calibration_internal.hpp"
#include "cpu_kernels.hpp"

namespace astro {

using namespace calib_detail;

namespace {

[[noreturn]] void stop(error_category category, std::string code, std::string message,
                       stop_error::context_list context = {}) {
  throw stop_error(category, std::move(code), std::move(message), std::move(context));
}

[[noreturn]] void bad_param(const char* name, const char* why) {
  stop(error_category::invalid_input, std::string("master.param.") + name, "parametro assente, non finito o fuori dominio",
       {{"parametro", name}, {"motivo", why}});
}

// `canale`: solo per un controllo per canale su un flat CFA.
[[noreturn]] void outside_variance_domain(const master_recipe& r, const char* motivo, const char* canale = nullptr) {
  stop_error::context_list context = {{"combine", r.combine ? combine_name(*r.combine) : "assente"},
                                      {"rejection", r.rejection ? rejection_name(*r.rejection) : "assente"},
                                      {"n", std::to_string(r.frames.size())},
                                      {"role", role_name(r.role)},
                                      {"motivo", motivo}};
  if (canale != nullptr) context.emplace_back("canale", canale);
  stop(error_category::unsupported, "master.variance_domain",
       "metodo, numero di frame o ruolo fuori dal dominio della varianza derivata (C-02.b §b.1, §b.2)", std::move(context));
}

const char* flat_norm_name(flat_normalization f) {
  switch (f) {
    case flat_normalization::none: return "none";
    case flat_normalization::median_ratio: return "median_ratio";
  }
  ASTRO_ASSERT(false);
  return "";
}
const char* frame_norm_name(frame_normalization_mode m) {
  return m == frame_normalization_mode::none ? "none" : "median_ratio";
}
const char* type_name(sample_type t) { return t == sample_type::f32 ? "f32" : "f64"; }
const char* policy_name(temperature_policy p) {
  return p == temperature_policy::compare ? "compare" : "not_applicable_declared";
}

std::string frame_name(std::size_t k) {
  char b[32];
  std::snprintf(b, sizeof b, "frame_%03zu", k);
  return b;
}

// Ordine canonico (C-02.a §8): digest SHA-256 crescente; esadecimale minuscolo, quindi ordine dei byte.
std::vector<std::size_t> canonical_order(const master_recipe& r) {
  std::vector<std::size_t> order(r.frames.size());
  for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::sort(order.begin(), order.end(),
            [&](std::size_t a, std::size_t b) { return r.frames[a].digest.sha256 < r.frames[b].digest.sha256; });
  return order;
}

void check_finite(const std::optional<double>& v, const char* name) {
  if (!v) bad_param(name, "assente");
  if (!std::isfinite(*v)) bad_param(name, "non finito");
}

void check_integer(const std::optional<double>& v, const char* name, double minimum) {
  check_finite(v, name);
  if (*v != std::floor(*v)) bad_param(name, "non intero");
  if (!(*v >= minimum)) bad_param(name, "sotto il minimo");
}

void forbid(const std::optional<double>& v, const char* name) {
  if (v) bad_param(name, "di un metodo non scelto");
}

enum class method_kind { single, mean, weighted_mean, median, winsorized, sigma_clip };

// Tabella delle coppie (`combine`, `rejection`) di C-02.b §b.1.
std::optional<method_kind> method_of(combine_method c, master_rejection j) {
  if (j == master_rejection::sigma_clip) return c == combine_method::mean ? std::optional(method_kind::sigma_clip) : std::nullopt;
  switch (c) {
    case combine_method::single: return method_kind::single;
    case combine_method::mean: return method_kind::mean;
    case combine_method::weighted_mean: return method_kind::weighted_mean;
    case combine_method::median: return method_kind::median;
    case combine_method::winsorized: return method_kind::winsorized;
    case combine_method::other: return std::nullopt;
  }
  return std::nullopt;
}

// Somma esatta di f64 come espansione non sovrapposta (componenti per modulo crescente). Un overflow intermedio rende la
// somma non rappresentabile: lo segnala `ok = false`.
struct exact_sum {
  std::vector<double> parts;
  bool ok = true;

  void add(double b) {
    double q = b;
    std::vector<double> next;
    next.reserve(parts.size() + 1);
    for (const double e : parts) {
      const double s = q + e;
      const double bv = s - q;
      const double err = (q - (s - bv)) + (e - bv);
      if (!std::isfinite(s)) ok = false;
      if (err != 0.0) next.push_back(err);
      q = s;
    }
    if (q != 0.0 || next.empty()) next.push_back(q);
    parts = std::move(next);
  }

  int sign() const {
    for (std::size_t i = parts.size(); i-- > 0;) {
      if (parts[i] != 0.0) return parts[i] > 0 ? 1 : -1;
    }
    return 0;
  }
};

__extension__ typedef unsigned __int128 u128;

// Intero senza segno a precisione arbitraria (cifre a 32 bit dalla meno significativa, senza zeri in testa): prodotti,
// moltiplicazioni per potenze di 2 e confronto, per i confronti esatti sui valori f64 dichiarati.
struct wide_uint {
  std::vector<std::uint32_t> limbs;

  static wide_uint of(u128 v) {
    wide_uint r;
    for (; v != 0; v >>= 32) r.limbs.push_back(static_cast<std::uint32_t>(v));
    return r;
  }

  friend wide_uint operator*(const wide_uint& a, const wide_uint& b) {
    wide_uint r;
    if (a.limbs.empty() || b.limbs.empty()) return r;
    r.limbs.assign(a.limbs.size() + b.limbs.size(), 0);
    for (std::size_t i = 0; i < a.limbs.size(); ++i) {
      std::uint64_t carry = 0;
      for (std::size_t k = 0; k < b.limbs.size(); ++k) {
        const std::uint64_t t = static_cast<std::uint64_t>(a.limbs[i]) * b.limbs[k] + r.limbs[i + k] + carry;
        r.limbs[i + k] = static_cast<std::uint32_t>(t);
        carry = t >> 32;
      }
      r.limbs[i + b.limbs.size()] = static_cast<std::uint32_t>(carry);
    }
    while (!r.limbs.empty() && r.limbs.back() == 0) r.limbs.pop_back();
    return r;
  }

  friend wide_uint operator+(const wide_uint& a, const wide_uint& b) {
    wide_uint r;
    const std::size_t size = std::max(a.limbs.size(), b.limbs.size());
    r.limbs.reserve(size + 1);
    std::uint64_t carry = 0;
    for (std::size_t i = 0; i < size; ++i) {
      const std::uint64_t t = carry + (i < a.limbs.size() ? a.limbs[i] : 0u) + (i < b.limbs.size() ? b.limbs[i] : 0u);
      r.limbs.push_back(static_cast<std::uint32_t>(t));
      carry = t >> 32;
    }
    if (carry != 0) r.limbs.push_back(static_cast<std::uint32_t>(carry));
    return r;
  }

  // this · 2^bits
  wide_uint shifted(std::size_t bits) const {
    wide_uint r;
    if (limbs.empty()) return r;
    const std::size_t whole = bits / 32, part = bits % 32;
    r.limbs.assign(whole, 0);
    std::uint32_t carry = 0;
    for (const std::uint32_t l : limbs) {
      r.limbs.push_back(part == 0 ? l : (l << part) | carry);
      carry = part == 0 ? 0 : l >> (32 - part);
    }
    if (carry != 0) r.limbs.push_back(carry);
    return r;
  }

  friend int compare(const wide_uint& a, const wide_uint& b) {
    if (a.limbs.size() != b.limbs.size()) return a.limbs.size() < b.limbs.size() ? -1 : 1;
    for (std::size_t i = a.limbs.size(); i-- > 0;) {
      if (a.limbs[i] != b.limbs[i]) return a.limbs[i] < b.limbs[i] ? -1 : 1;
    }
    return 0;
  }
};

// Valore diadico esatto `m · 2^e` di uno o più f64 finiti ≥ 0: somme e prodotti senza arrotondamento.
struct dyadic {
  wide_uint m;
  long long e = 0;

  // Mantissa scalata di 2^53 da frexp: intera ed esatta anche per i subnormali.
  static dyadic of(double v) {
    int exp2 = 0;
    const double fraction = std::frexp(v, &exp2);
    return {wide_uint::of(static_cast<std::uint64_t>(std::ldexp(fraction, 53))), static_cast<long long>(exp2) - 53};
  }

  friend dyadic operator*(const dyadic& a, const dyadic& b) { return {a.m * b.m, a.e + b.e}; }

  // Mantisse portate all'esponente minore dei due.
  static std::pair<wide_uint, wide_uint> aligned(const dyadic& a, const dyadic& b) {
    if (a.e >= b.e) return {a.m.shifted(static_cast<std::size_t>(a.e - b.e)), b.m};
    return {a.m, b.m.shifted(static_cast<std::size_t>(b.e - a.e))};
  }

  friend dyadic operator+(const dyadic& a, const dyadic& b) {
    const auto [x, y] = aligned(a, b);
    return {x + y, std::min(a.e, b.e)};
  }

  friend int compare(const dyadic& a, const dyadic& b) {
    const auto [x, y] = aligned(a, b);
    return compare(x, y);
  }
};

std::string u128_text(u128 v) {
  std::string t;
  do {
    t.insert(t.begin(), static_cast<char>('0' + static_cast<int>(v % 10)));
    v /= 10;
  } while (v != 0);
  return t;
}

// (n − 1)² > κ² · n in aritmetica esatta (C-02.b §b.1, Samuelson), con κ f64 finito > 0. κ = M · 2^e con M intero
// (frexp, mantissa scalata di 2^53, esatta anche per i subnormali): κ² n = M² n · 2^(2e), confrontato con (n − 1)²
// dopo aver portato entrambi i membri a interi con lo stesso fattore 2^|2e|. Nessun prodotto arrotondato.
bool samuelson_admits(const wide_uint& limit, std::size_t n, double kappa) {
  int exp2 = 0;
  const double fraction = std::frexp(kappa, &exp2);
  const auto mantissa = static_cast<std::uint64_t>(std::ldexp(fraction, 53));
  const wide_uint m = wide_uint::of(mantissa);
  const wide_uint rhs = m * m * wide_uint::of(n);
  const long long shift = 2 * (static_cast<long long>(exp2) - 53);
  if (shift >= 0) return compare(limit, rhs.shifted(static_cast<std::size_t>(shift))) > 0;
  return compare(limit.shifted(static_cast<std::size_t>(-shift)), rhs) > 0;
}

// Ricetta inerte (C-02.b §b.1): nessuna delle due soglie può rifiutare alla prima iterazione. Contesto: n, i due κ in
// forma che rilegge lo stesso f64, `limite` = (n − 1)² intero esatto (M3b-lacune, «Decisioni successive (B6)»).
void require_sigma_clip_not_inert(std::size_t n, double kappa_low, double kappa_high) {
  const u128 limit = static_cast<u128>(n - 1) * (n - 1);
  const wide_uint wide_limit = wide_uint::of(limit);
  if (samuelson_admits(wide_limit, n, kappa_low) || samuelson_admits(wide_limit, n, kappa_high)) return;
  stop(error_category::invalid_input, "master.param.sigma_clip_inert",
       "sigma_clip non può rifiutare nessun campione: (n - 1)^2 <= kappa^2 n per entrambe le soglie (Samuelson)",
       {{"n", std::to_string(n)},
        {"clip_kappa_low", num_text(kappa_low)},
        {"clip_kappa_high", num_text(kappa_high)},
        {"limite", u128_text(limit)}});
}

// Pesi di `weighted_mean` (C-02.b §b.1, §b.10): finiti e > 0, uno per frame; `2 max w < W` confrontato in aritmetica
// esatta sui valori f64, cioè `max w < somma degli altri`.
void validate_weights(const master_recipe& r) {
  double largest = 0;
  std::size_t at = 0;
  for (std::size_t k = 0; k < r.frames.size(); ++k) {
    const std::optional<double>& w = r.frames[k].weight;
    if (!w) bad_param("weights", "peso assente");
    if (!std::isfinite(*w)) bad_param("weights", "peso non finito");
    if (!(*w > 0)) bad_param("weights", "peso non positivo");
    if (*w > largest) {
      largest = *w;
      at = k;
    }
  }
  exact_sum rest;
  for (std::size_t k = 0; k < r.frames.size(); ++k) {
    if (k != at) rest.add(*r.frames[k].weight);
  }
  if (!rest.ok) bad_param("weights", "somma dei pesi non rappresentabile");
  rest.add(-largest);
  if (!rest.ok) bad_param("weights", "somma dei pesi non rappresentabile");
  if (rest.sign() <= 0) outside_variance_domain(r, "max a_f >= 1/2");
}

void validate_params(const master_recipe& r) {
  if (!r.combine) bad_param("combine", "assente");
  if (!r.rejection) bad_param("rejection", "assente");
  const std::optional<method_kind> found = method_of(*r.combine, *r.rejection);
  if (!found) outside_variance_domain(r, "coppia fuori tabella");
  const method_kind method = *found;
  const std::size_t n = r.frames.size();
  const bool n_ok = [&] {
    switch (method) {
      case method_kind::single: return n == 1;
      case method_kind::mean: return n >= 2;
      case method_kind::weighted_mean:
      case method_kind::sigma_clip: return n >= 3;
      case method_kind::median:
      case method_kind::winsorized: return n >= 3 && n <= 1024;
    }
    return false;
  }();
  if (!n_ok) outside_variance_domain(r, "n fuori dominio");
  const bool plain = method == method_kind::single || method == method_kind::mean;
  if (r.role == master_role::flat) {
    if (!r.flat_norm) bad_param("flat_normalization", "assente");
    if (*r.flat_norm != flat_normalization::none && *r.flat_norm != flat_normalization::median_ratio) {
      bad_param("flat_normalization", "fuori dominio");
    }
    // C-02.b-flat §f.2: tabella dei metodi per `flat_normalization`, poi i parametri ammessi o vietati.
    if (*r.flat_norm == flat_normalization::median_ratio) {
      if (method == method_kind::single) outside_variance_domain(r, "median_ratio con single");
      if (r.flat_level_rel_tol) bad_param("flat_level_rel_tol", "vietato con median_ratio");
      check_finite(r.flat_additive_bound_adu, "flat_additive_bound_adu");
      if (!(*r.flat_additive_bound_adu >= 0)) bad_param("flat_additive_bound_adu", "deve essere >= 0");
      if (method == method_kind::mean) {
        if (r.flat_norm_rel_budget) bad_param("flat_norm_rel_budget", "vietato con mean");
      } else {
        check_finite(r.flat_norm_rel_budget, "flat_norm_rel_budget");
        if (!(*r.flat_norm_rel_budget > 0 && *r.flat_norm_rel_budget < 1)) {
          bad_param("flat_norm_rel_budget", "deve stare in (0, 1)");
        }
      }
    } else {
      if (!plain) outside_variance_domain(r, "metodo non ammesso per il flat");
      if (r.flat_additive_bound_adu) bad_param("flat_additive_bound_adu", "vietato con none");
      if (r.flat_norm_rel_budget) bad_param("flat_norm_rel_budget", "vietato con none");
      check_finite(r.flat_level_rel_tol, "flat_level_rel_tol");
      if (!(*r.flat_level_rel_tol > 0 && *r.flat_level_rel_tol < 1)) bad_param("flat_level_rel_tol", "deve stare in (0, 1)");
    }
    check_finite(r.flat_min_signal_adu, "flat_min_signal_adu");
    if (!(*r.flat_min_signal_adu > 0)) bad_param("flat_min_signal_adu", "deve essere > 0");
  } else {
    if (r.flat_norm) bad_param("flat_normalization", "solo per il ruolo flat");
    if (r.flat_level_rel_tol) bad_param("flat_level_rel_tol", "solo per il ruolo flat");
    if (r.flat_min_signal_adu) bad_param("flat_min_signal_adu", "solo per il ruolo flat");
    if (r.flat_additive_bound_adu) bad_param("flat_additive_bound_adu", "solo per il ruolo flat");
    if (r.flat_norm_rel_budget) bad_param("flat_norm_rel_budget", "solo per il ruolo flat");
  }
  if (method == method_kind::winsorized) {
    check_integer(r.winsor_g, "winsor_g", 1);
  } else {
    forbid(r.winsor_g, "winsor_g");
  }
  if (method == method_kind::sigma_clip) {
    check_finite(r.clip_kappa_low, "clip_kappa_low");
    if (!(*r.clip_kappa_low > 0)) bad_param("clip_kappa_low", "deve essere > 0");
    check_finite(r.clip_kappa_high, "clip_kappa_high");
    if (!(*r.clip_kappa_high > 0)) bad_param("clip_kappa_high", "deve essere > 0");
    check_integer(r.clip_max_iter, "clip_max_iter", 1);
    check_integer(r.clip_min_kept, "clip_min_kept", 2);
    if (*r.clip_min_kept > static_cast<double>(n)) bad_param("clip_min_kept", "oltre n");
    require_sigma_clip_not_inert(n, *r.clip_kappa_low, *r.clip_kappa_high);
    // C-02.b-flat §f.2: sul flat (solo `median_ratio` arriva qui) la procedura deve essere dispari.
    if (r.role == master_role::flat && *r.clip_kappa_low != *r.clip_kappa_high) {
      outside_variance_domain(r, "sigma_clip sul flat con soglie diverse");
    }
  } else {
    forbid(r.clip_kappa_low, "clip_kappa_low");
    forbid(r.clip_kappa_high, "clip_kappa_high");
    forbid(r.clip_max_iter, "clip_max_iter");
    forbid(r.clip_min_kept, "clip_min_kept");
  }
  // Frame RAW (L9, C-02.b §b.2 «RAW»): `offset_adu` è dichiarato nella ricetta, serve solo a `single` e al flat; senza
  // frame RAW resta un parametro del solo `single`. Il controllo di frame RAW e FITS insieme precede questo.
  const bool raw = std::any_of(r.frames.begin(), r.frames.end(), [](const master_frame_spec& f) { return f.decoder.has_value(); });
  const auto raw_offset_missing = [&] {
    stop(error_category::metadata_missing, "master.metadata.offset_adu", "offset_adu non dichiarato con frame RAW",
         {{"parametro", "offset_adu"}, {"role", role_name(r.role)}, {"combine", combine_name(*r.combine)}});
  };
  if (method == method_kind::single) {
    // Modello di rumore (ADR-17.a): assente = metadata_missing, non finito o fuori dominio = var.param.<nome>.
    // `offset_adu` solo finito (anche negativo), `conversion_gain_e_per_adu` > 0, `read_noise_adu` >= 0.
    if (raw && !r.offset_adu) raw_offset_missing();
    const auto need_model = [&](const std::optional<double>& v, const char* name, int domain) {
      if (!v) {
        stop(error_category::metadata_missing, std::string("var.source.") + name, "parametro del modello di rumore assente",
             {{"parametro", name}});
      }
      if (!std::isfinite(*v) || (domain > 0 && !(*v > 0)) || (domain < 0 && !(*v >= 0))) {
        stop(error_category::invalid_input, std::string("var.param.") + name, "parametro del modello di rumore fuori dominio",
             {{"parametro", name}});
      }
    };
    need_model(r.offset_adu, "offset_adu", 0);
    need_model(r.conversion_gain_e_per_adu, "conversion_gain_e_per_adu", 1);
    need_model(r.read_noise_adu, "read_noise_adu", -1);
  } else {
    if (raw && r.role == master_role::flat) {
      if (!r.offset_adu) raw_offset_missing();
      if (!std::isfinite(*r.offset_adu)) bad_param("offset_adu", "non finito");
    } else {
      forbid(r.offset_adu, "offset_adu");
    }
    forbid(r.conversion_gain_e_per_adu, "conversion_gain_e_per_adu");
    forbid(r.read_noise_adu, "read_noise_adu");
  }
  // ADR-16.b §b.7 «Parametro»: obbligatoria con frame RAW, vietata altrimenti.
  if (raw && !r.decoder_black_abs_tol_adu) bad_param("decoder_black_abs_tol_adu", "assente con frame RAW");
  if (!raw && r.decoder_black_abs_tol_adu) bad_param("decoder_black_abs_tol_adu", "dichiarata senza frame RAW");
  if (method == method_kind::weighted_mean) {
    validate_weights(r);
  } else {
    for (const master_frame_spec& f : r.frames) {
      if (f.weight) bad_param("weights", "pesi con un metodo diverso da weighted_mean");
    }
  }
  if (method == method_kind::winsorized && !(2 * *r.winsor_g + 1 <= static_cast<double>(n))) {
    outside_variance_domain(r, "n < 2g + 1");
  }
  if (r.analysis.type() != json::kind::null && r.analysis.type() != json::kind::object) {
    bad_param("analysis", "deve essere un oggetto oppure null");
  }
  if (r.saturation_adu) {
    if (!std::isfinite(*r.saturation_adu)) bad_param("saturation_adu", "non finito");
    check_finite(r.saturated_fraction_max, "saturated_fraction_max");
    if (!(*r.saturated_fraction_max >= 0 && *r.saturated_fraction_max <= 1)) {
      bad_param("saturated_fraction_max", "deve stare in [0, 1]");
    }
  } else if (r.saturated_fraction_max) {
    bad_param("saturated_fraction_max", "dichiarato senza saturation_adu");
  }
  check_finite(r.exposure_rel_tol, "exposure_rel_tol");
  if (*r.exposure_rel_tol < 0) bad_param("exposure_rel_tol", "deve essere >= 0");
  check_finite(r.temperature_abs_tol_c, "temperature_abs_tol_c");
  if (*r.temperature_abs_tol_c < 0) bad_param("temperature_abs_tol_c", "deve essere >= 0");
  if (!r.temp_policy) bad_param("temperature_policy", "assente");
  if (!r.simd) bad_param("simd", "assente");
}

json fields_json(const declared_image_fields& f) {
  json o = json::object();
  o.set("binning_x", f.binning_x ? json::uinteger(*f.binning_x) : json::null());
  o.set("binning_y", f.binning_y ? json::uinteger(*f.binning_y) : json::null());
  o.set("unit", f.unit ? json::string(*f.unit == pixel_unit::adu ? "adu" : *f.unit == pixel_unit::electron ? "electron" : "dimensionless")
                       : json::null());
  o.set("linearity", f.lin ? json::string(*f.lin == linearity::linear ? "linear" : "nonlinear") : json::null());
  return o;
}

json read_options_json(const fits_read_options& o) {
  json r = json::object();
  r.set("declared_bayer_x_offset", o.declared_bayer_x_offset ? json::uinteger(*o.declared_bayer_x_offset) : json::null());
  r.set("declared_bayer_y_offset", o.declared_bayer_y_offset ? json::uinteger(*o.declared_bayer_y_offset) : json::null());
  r.set("declared_phase", o.declared_phase ? json::string(to_string(*o.declared_phase)) : json::null());
  r.set("declared_linearity", o.declared_linearity ? json::string(to_string(*o.declared_linearity)) : json::null());
  return r;
}

json descriptor_json(const master_descriptor& d) {
  json o = json::object();
  o.set("role", json::string(role_name(d.role)));
  o.set("additive", d.additive ? json::string(additive_name(*d.additive)) : json::null());
  o.set("scaling", d.scaling ? json::string(scaling_name(*d.scaling)) : json::null());
  o.set("flat", d.flat ? json::string(flat_name(*d.flat)) : json::null());
  o.set("combine_count", json::uinteger(*d.combine_count));
  o.set("combine_method", json::string(combine_name(*d.combine)));
  o.set("rejection", json::string(rejection_name(*d.rejection)));
  o.set("variance_estimator", json::string(estimator_name(*d.estimator)));
  if (d.frame_normalization) o.set("frame_normalization", json::string(frame_norm_name(*d.frame_normalization)));
  return o;
}

// ---------------------------------------------------------------- preflight (C-02.a §2)

struct frame_state {
  std::string name;
  const master_frame_spec* spec = nullptr;
  image header;  // geometria e metadati; nessun campione
  field_origins origins;
  std::uint64_t saturated = 0;
  double saturated_fraction = 0;
  std::array<double, 4> level{};  // m_(f,c) nell'ordine dei canali, solo flat; il mono ha il solo canale 0
  std::array<double, 4> scale{};  // r_(f,c), solo flat con `median_ratio`
  std::uint64_t rejected = 0;  // campioni rifiutati da sigma_clip, somma dei parziali dei task
};

image header_of(const image& im) {
  image h;
  h.geometry = im.geometry;
  h.layout = im.layout;
  h.type = im.type;
  h.unit = im.unit;
  h.lin = im.lin;
  h.lin_origin = im.lin_origin;
  h.phase = im.phase;
  h.measure = im.measure;
  h.cfa = im.cfa;
  h.metadata = im.metadata;
  h.uninterpreted_keywords = im.uninterpreted_keywords;
  return h;
}

struct master_plan {
  master_recipe recipe;
  std::vector<std::size_t> order;
  std::vector<frame_state> frames;  // ordine canonico
  std::size_t n = 0;
  std::uint32_t w = 0, h = 0;
  bool flat = false, has_sat = false;
  double sat = 0, offset = 0, eps = 0;
  std::array<double, 4> level_mean{};  // m̄_c, solo flat con `none`
  std::array<double, 4> level_harmonic{};  // m_H,c, solo flat con `median_ratio`
  std::uint8_t cfa_x = 0, cfa_y = 0;
  bool per_channel = false;
  std::array<const char*, 4> labels = {"all", "", "", ""};
  cpu_path path{};  // risolto nel preflight dalla richiesta `simd` (ADR-07)
  image out;
  std::vector<double> lbe, ratio;
  std::vector<std::uint8_t> nonpositive;
  std::vector<std::string> warnings, hypotheses;
  json compat = json::array();
  json master_metadata = json::object();
  json not_common = json::array();
  master_descriptor descriptor;
  method_kind method = method_kind::mean;
  std::optional<kappa_result> kappa;  // metodi robusti: κ del preflight (C-02.b §b.6)
  // `weighted_mean`: costanti del nodo nell'ordine canonico, calcolate nel preflight (C-02.b §b.6).
  double weight_sum = 0;
  std::vector<double> weights, residual_coeffs;  // w_f, c_f

  std::size_t channel_of(std::uint32_t x, std::uint32_t y) const noexcept {
    return per_channel ? static_cast<std::size_t>(((y + cfa_y) & 1u) * 2 + ((x + cfa_x) & 1u)) : 0;
  }
  std::size_t channel_count() const noexcept { return per_channel ? 4 : 1; }
  // Contesto di uno stop del flat: le chiavi di `.a`, più `canale` per un frame CFA.
  stop_error::context_list flat_context(stop_error::context_list context, std::size_t c) const {
    if (per_channel) context.emplace_back("canale", labels[c]);
    return context;
  }
};

// κ dei metodi robusti (C-02.b §b.6) sulla ricetta validata: calcolato una volta, controllato contro la propria stima
// d'errore prima di ogni lettura di campioni. `mean` e gli altri metodi non hanno κ.
std::optional<kappa_result> preflight_kappa(const master_recipe& r, const std::function<void(kappa_result&)>& on_kappa) {
  const method_kind method = *method_of(*r.combine, *r.rejection);
  if (method != method_kind::median && method != method_kind::winsorized) return std::nullopt;
  const kappa_estimator estimator = method == method_kind::median ? kappa_estimator::median : kappa_estimator::winsorized;
  const auto n = static_cast<std::uint32_t>(r.frames.size());
  const auto g = method == method_kind::winsorized ? static_cast<std::uint32_t>(*r.winsor_g) : 0u;
  kappa_result k = robust_kappa(estimator, n, g);
  if (on_kappa) on_kappa(k);
  check_kappa_accuracy(k, estimator, n, g);
  return k;
}

// Costanti di `weighted_mean` (C-02.b §b.6, «Ordine delle operazioni»), nell'ordine canonico: W, a_f = w_f / W,
// T = Σ (a_f · a_f) / (1 − 2 · a_f), c_f = (a_f · a_f) / ((1 − 2 · a_f) · (1 + T)). Un a_f, T o c_f non normale, non
// finito o ≤ 0, oppure un 1 − 2 · a_f ≤ 0, ferma con master.param.weights prima di ogni lettura di campioni: con W
// arrotondato un a_f in f64 può valere 1/2 o più anche se il dominio esatto 2 max w < W vale. Così ogni c_f è > 0 e
// il piano è una somma di termini non negativi.
void weight_constants(master_plan& p) {
  const auto require_normal = [&](double v, const char* constant, const std::string& frame) {
    if (!std::isfinite(v) || !is_normal(v) || !(v > 0)) {
      stop(error_category::invalid_input, "master.param.weights", "costante dei pesi non rappresentabile",
           {{"costante", constant}, {"frame", frame}, {"valore", num_text(v)}});
    }
  };
  p.weights.resize(p.n);
  for (std::size_t k = 0; k < p.n; ++k) p.weights[k] = *p.frames[k].spec->weight;
  double W = p.weights[0];
  for (std::size_t k = 1; k < p.n; ++k) W += p.weights[k];
  std::vector<double> sq(p.n), gap(p.n);
  double T = 0;
  for (std::size_t k = 0; k < p.n; ++k) {
    const double a = p.weights[k] / W;
    require_normal(a, "a_f", p.frames[k].name);
    sq[k] = a * a;
    gap[k] = 1 - 2 * a;
    if (!(gap[k] > 0)) {
      stop(error_category::invalid_input, "master.param.weights", "costante dei pesi non rappresentabile",
           {{"costante", "1 - 2 a_f"}, {"frame", p.frames[k].name}, {"valore", num_text(gap[k])}, {"a_f", num_text(a)}});
    }
    const double term = sq[k] / gap[k];
    T = k == 0 ? term : T + term;
  }
  require_normal(T, "T", "tutti");
  const double one_plus_t = 1 + T;
  p.residual_coeffs.resize(p.n);
  for (std::size_t k = 0; k < p.n; ++k) {
    p.residual_coeffs[k] = sq[k] / (gap[k] * one_plus_t);
    require_normal(p.residual_coeffs[k], "c_f", p.frames[k].name);
  }
  p.weight_sum = W;
}

master_plan make_plan(const master_recipe& r, const std::function<void(kappa_result&)>& on_kappa = {}) {
  // Prima della validazione: i confronti sui parametri sono già operazioni in virgola mobile (ADR-07, D23).
  require_fp_environment();
  validate_master_recipe(r);
  const std::optional<kappa_result> kappa = preflight_kappa(r, on_kappa);
  const method_kind method = *method_of(*r.combine, *r.rejection);
  master_plan p;
  p.method = method;
  p.kappa = kappa;
  p.path = resolve_cpu_path(*r.simd, read_cpu_capabilities());
  p.recipe = r;
  p.order = canonical_order(r);
  p.n = r.frames.size();
  p.flat = r.role == master_role::flat;
  p.has_sat = r.saturation_adu.has_value();
  if (p.has_sat) p.sat = *r.saturation_adu;
  if (p.flat && *r.flat_norm == flat_normalization::none) p.eps = *r.flat_level_rel_tol;
  p.frames.resize(p.n);
  for (std::size_t k = 0; k < p.n; ++k) {
    p.frames[k].name = frame_name(k);
    p.frames[k].spec = &p.recipe.frames[p.order[k]];
  }
  if (method == method_kind::weighted_mean) weight_constants(p);
  p.descriptor = master_descriptor_from_node(make_master_node(r));
  return p;
}

// Fase e linearità dai metadati, prima di ogni accesso ai campioni: la regola è la tabella delle operazioni di ADR-03
// (`master_create`, la fase prima della linearità); il contesto porta il ruolo del master e il frame.
void require_allowed_state(const std::string& name, master_role role, process_state state) {
  try {
    require_operation_allowed(operation_kind::master_create, state, role_name(role));
  } catch (stop_error& e) {
    e.add_context("frame", name);
    throw;
  }
}

void require_phase(const std::string& name, master_role role, const std::optional<metadata_value<process_phase>>& phase) {
  if (!phase) stop(error_category::metadata_missing, "master.metadata.phase", "fase di processo non dichiarata", {{"frame", name}});
  require_allowed_state(name, role, process_state{phase->value, linearity_class::linear});
}

void require_linear(const std::string& name, master_role role, const std::optional<linearity>& lin) {
  if (!lin) stop(error_category::metadata_missing, "master.metadata.linearity", "linearità non dichiarata", {{"frame", name}});
  require_allowed_state(name, role, process_state{process_phase::raw, classify_linearity(lin)});
}

// Linearità dall'header e dalla dichiarazione della ricetta: conflitto = incompatible metadata.declared_conflict.
std::optional<linearity> merged_linearity(const std::string& name, const std::optional<linearity>& header,
                                          const std::optional<linearity>& declared) {
  if (header && declared && *header != *declared) {
    stop(error_category::incompatible, "metadata.declared_conflict", "valore dichiarato diverso da quello presente",
         {{"input", name}, {"campo", "linearity"}});
  }
  return declared ? declared : header;
}

template <class T>
const T& need(const std::optional<metadata_value<T>>& field, const std::string& frame, const char* name) {
  if (!field) {
    stop(error_category::metadata_missing, std::string("master.metadata.") + name,
         "metadato richiesto assente (dichiarabile nella ricetta)", {{"frame", frame}, {"campo", name}});
  }
  return field->value;
}

template <class T>
bool all_finite_or_flagged(const image& im) {
  const T* s = im.data<T>();
  const std::size_t n = im.samples.size() / sizeof(T);
  for (std::size_t i = 0; i < n; ++i) {
    if (!std::isfinite(s[i]) && im.invalid.bits[i] == 0) return false;
  }
  return true;
}

double value_at(const image& im, std::size_t i) {
  return im.type == sample_type::f32 ? static_cast<double>(im.data<float>()[i]) : im.data<double>()[i];
}

[[noreturn]] void mismatch(const std::string& pair, const char* field, const std::string& left, const std::string& right,
                           const std::string& tolerance) {
  stop(error_category::incompatible, std::string("master.mismatch.") + field, "frame del gruppo incompatibili",
       {{"coppia", pair}, {"campo", field}, {"sinistra", left}, {"destra", right}, {"tolleranza", tolerance}});
}

// Controlli del singolo frame, statistiche sull'intera immagine (frazione satura, m_f) e copia dei metadati;
// per il flat, motivo CFA confrontato col frame 0 prima dei livelli per canale.
void inspect_frame(master_plan& p, std::size_t k, image& im) {
  frame_state& fs = p.frames[k];
  const std::string& name = fs.name;
  if (im.layout != plane_layout::mono && im.layout != plane_layout::cfa) {
    stop(error_category::unsupported, "calib.plane_layout", "un master accetta solo frame mono e CFA",
         {{"frame", name}, {"layout", layout_name(im.layout)}});
  }
  require_phase(name, p.recipe.role, im.phase);
  fs.origins = apply_declared(im, fs.spec->declared, fs.spec->declared_fields, name.c_str());
  require_linear(name, p.recipe.role, im.lin);
  if (!im.unit) stop(error_category::metadata_missing, "master.metadata.unit", "unità non dichiarata", {{"frame", name}});
  if (*im.unit != pixel_unit::adu) {
    stop(error_category::unsupported, "master.input_state", "un master si combina da frame in ADU",
         {{"frame", name}, {"campo", "unit"}});
  }
  if (im.metadata.pedestal_adu && im.metadata.pedestal_adu->value != 0.0) {
    stop(error_category::unsupported, "calib.pedestal", "pedestal diverso da zero non supportato",
         {{"frame", name}, {"pedestal_adu", num_text(im.metadata.pedestal_adu->value)}});
  }
  if (im.variance) {
    stop(error_category::unsupported, "master.input_variance",
         "piano VARIANCE in un frame: la varianza del master è empirica e il piano non avrebbe uso", {{"frame", name}});
  }
  validate_image(im);
  const bool finite_ok = im.type == sample_type::f32 ? all_finite_or_flagged<float>(im) : all_finite_or_flagged<double>(im);
  if (!finite_ok) {
    stop(error_category::invalid_input, "calib.nonfinite_unflagged", "campione non finito senza bit di invalidità",
         {{"frame", name}});
  }
  const master_recipe& r = p.recipe;
  if (im.metadata.saturation_adu) {
    if (!r.saturation_adu) {
      stop(error_category::invalid_input, "master.param.saturation_adu",
           "soglia di saturazione nell'header del frame e assente dalla ricetta",
           {{"frame", name}, {"valore", num_text(im.metadata.saturation_adu->value)}});
    }
    if (im.metadata.saturation_adu->value != *r.saturation_adu) {
      stop(error_category::incompatible, "metadata.declared_conflict", "valore dichiarato diverso da quello presente",
           {{"frame", name}, {"campo", "saturation_adu"}, {"header", num_text(im.metadata.saturation_adu->value)},
            {"ricetta", num_text(*r.saturation_adu)}});
    }
  }
  if (const auto& t = im.metadata.imagetyp) {
    const std::optional<std::string> role = imagetyp_role(t->value);
    const std::string expected = role_name(r.role);
    if (!role) {
      p.warnings.push_back("imagetyp_unrecognized:" + name + ":" + t->value);
    } else if (!(*role == expected || (expected == "dark_flat" && *role == "dark"))) {
      stop(error_category::incompatible, "calib.imagetyp_conflict", "IMAGETYP indica un altro ruolo",
           {{"frame", name}, {"imagetyp", t->value}, {"ruolo_atteso", expected}});
    }
  }
  (void)need(im.metadata.camera, name, "camera");
  (void)need(im.metadata.gain, name, "gain");
  const double offset = need(im.metadata.offset_adu, name, "offset_adu");
  (void)need(im.metadata.exposure_s, name, "exposure_s");
  if (*r.temp_policy == temperature_policy::compare) (void)need(im.metadata.temperature_c, name, "temperature_c");
  if (p.flat) (void)need(im.metadata.filter, name, "filter");
  for (const auto& [field, v] : {std::pair{"binning_x", &im.geometry.binning_x}, std::pair{"binning_y", &im.geometry.binning_y}}) {
    if (!*v) {
      stop(error_category::metadata_missing, std::string("master.metadata.") + field,
           "metadato richiesto assente (dichiarabile nella ricetta)", {{"frame", name}, {"campo", field}});
    }
  }

  // Canali dal frame di riferimento canonico, prima dei livelli m_(f,c); un frame k > 0 con un altro motivo (o mono) si
  // ferma con lo stop CFA del confronto col riferimento prima che i suoi livelli si calcolino col motivo del frame 0.
  if (k == 0 && im.cfa) {
    p.per_channel = true;
    p.cfa_x = im.cfa->x_offset;
    p.cfa_y = im.cfa->y_offset;
    p.labels = channel_labels(im.cfa->pattern);
  }
  const std::size_t px = static_cast<std::size_t>(im.geometry.width) * im.geometry.height;
  if (p.has_sat) {
    for (std::size_t i = 0; i < px; ++i) {
      if (im.invalid.bits[i] == 0 && value_at(im, i) >= p.sat) ++fs.saturated;
    }
    fs.saturated_fraction = static_cast<double>(fs.saturated) / static_cast<double>(px);
    if (fs.saturated_fraction > *r.saturated_fraction_max) {
      stop(error_category::invalid_input, p.flat ? "master.flat_saturated" : "master.saturated",
           "frazione di campioni saturi oltre saturated_fraction_max",
           {{"frame", name}, {"frazione", num_text(fs.saturated_fraction)}, {"massimo", num_text(*r.saturated_fraction_max)}});
    }
  }
  if (p.flat) {
    if (k > 0 && !(p.frames[0].header.cfa == im.cfa)) {
      mismatch(p.frames[0].name + "-" + name, "cfa", cfa_text(p.frames[0].header.cfa), cfa_text(im.cfa), "esatta");
    }
    // m_(f,c) per canale (C-02.b §b.6): mediana dei campioni validi del canale, nessuna somma.
    const std::size_t channels = p.channel_count();
    std::array<std::vector<double>, 4> z;
    for (std::size_t c = 0; c < channels; ++c) z[c].reserve(px / channels + 1);
    const std::uint32_t w = im.geometry.width;
    for (std::uint32_t y = 0; y < im.geometry.height; ++y) {
      for (std::uint32_t x = 0; x < w; ++x) {
        const std::size_t i = static_cast<std::size_t>(y) * w + x;
        if (im.invalid.bits[i] == 0) z[p.channel_of(x, y)].push_back(value_at(im, i) - offset);
      }
    }
    for (std::size_t c = 0; c < channels; ++c) {
      if (z[c].empty()) {
        stop(error_category::invalid_input, "master.flat_low_signal", "nessun pixel valido per il livello del flat",
             p.flat_context({{"frame", name}}, c));
      }
      const double level = median_of(z[c]);
      if (!std::isfinite(level) || level < *r.flat_min_signal_adu) {
        stop(error_category::invalid_input, "master.flat_low_signal", "livello del flat sotto flat_min_signal_adu o non finito",
             p.flat_context({{"frame", name}, {"m_f", num_text(level)}, {"minimo", num_text(*r.flat_min_signal_adu)}}, c));
      }
      fs.level[c] = level;
    }
  }
  fs.header = header_of(im);
}

json record_of(const std::string& pair, const char* field, json left, json right, const char* rule, json tolerance,
               const char* result = "pass") {
  json r = json::object();
  r.set("pair", json::string(pair));
  r.set("field", json::string(field));
  r.set("left", std::move(left));
  r.set("right", std::move(right));
  r.set("rule", json::string(rule));
  r.set("tolerance", std::move(tolerance));
  r.set("result", json::string(result));
  return r;
}

// Compatibilità del frame k con il frame di riferimento canonico (C-02.a §2).
void compare_with_reference(master_plan& p, std::size_t k) {
  if (k == 0) return;
  const image& a = p.frames[0].header;
  const image& b = p.frames[k].header;
  const std::string pair = p.frames[0].name + "-" + p.frames[k].name;
  const auto exact_u = [&](const char* field, std::uint32_t x, std::uint32_t y) {
    if (x != y) mismatch(pair, field, std::to_string(x), std::to_string(y), "esatta");
    p.compat.push(record_of(pair, field, json::uinteger(x), json::uinteger(y), "exact", json::null()));
  };
  exact_u("width", a.geometry.width, b.geometry.width);
  exact_u("height", a.geometry.height, b.geometry.height);
  exact_u("binning_x", *a.geometry.binning_x, *b.geometry.binning_x);
  exact_u("binning_y", *a.geometry.binning_y, *b.geometry.binning_y);
  if (!(a.cfa == b.cfa)) mismatch(pair, "cfa", cfa_text(a.cfa), cfa_text(b.cfa), "esatta");
  p.compat.push(record_of(pair, "cfa", json::string(cfa_text(a.cfa)), json::string(cfa_text(b.cfa)), "exact", json::null()));
  const frame_metadata &ma = a.metadata, &mb = b.metadata;
  if (ma.camera->value != mb.camera->value) mismatch(pair, "camera", ma.camera->value, mb.camera->value, "esatta");
  p.compat.push(record_of(pair, "camera", json::string(ma.camera->value), json::string(mb.camera->value), "exact", json::null()));
  for (const auto& [field, fa, fb] : {std::tuple{"gain", &ma.gain, &mb.gain}, std::tuple{"offset_adu", &ma.offset_adu, &mb.offset_adu}}) {
    const double x = (*fa)->value, y = (*fb)->value;
    if (x != y) mismatch(pair, field, num_text(x), num_text(y), "esatta");
    p.compat.push(record_of(pair, field, json::number(x), json::number(y), "exact", json::null()));
  }
  {
    const double x = ma.exposure_s->value, y = mb.exposure_s->value, tol = *p.recipe.exposure_rel_tol;
    if (!(std::fabs(x - y) <= tol * std::max(std::fabs(x), std::fabs(y)))) {
      mismatch(pair, "exposure_s", num_text(x), num_text(y), "relativa " + num_text(tol));
    }
    p.compat.push(record_of(pair, "exposure_s", json::number(x), json::number(y), "relative_to_max", json::number(tol)));
  }
  if (*p.recipe.temp_policy == temperature_policy::compare) {
    const double x = ma.temperature_c->value, y = mb.temperature_c->value, tol = *p.recipe.temperature_abs_tol_c;
    if (!(std::fabs(x - y) <= tol)) mismatch(pair, "temperature_c", num_text(x), num_text(y), "assoluta " + num_text(tol));
    p.compat.push(record_of(pair, "temperature_c", json::number(x), json::number(y), "absolute", json::number(tol)));
  } else {
    p.compat.push(record_of(pair, "temperature_c", json::null(), json::null(), "skipped_not_applicable_declared", json::null(),
                            "skipped"));
  }
  if (p.flat) {
    if (ma.filter->value != mb.filter->value) mismatch(pair, "filter", ma.filter->value, mb.filter->value, "esatta");
    p.compat.push(record_of(pair, "filter", json::string(ma.filter->value), json::string(mb.filter->value), "exact", json::null()));
  }
}

// Campi opzionali (roi_x, roi_y, readout_mode, row_order): ogni valore presente si confronta con il primo frame
// canonico che lo porta, anche quando il frame di riferimento non lo ha; ogni assenza resta un'ipotesi del report.
void compare_optional_fields(master_plan& p, std::size_t k) {
  const auto field = [&](const char* name, auto member, auto to_text, auto to_json) {
    const auto& mine = p.frames[k].header.metadata.*member;
    if (!mine) {
      p.hypotheses.push_back(std::string("not_verified:") + name + ":absent_in=" + p.frames[k].name);
      return;
    }
    for (std::size_t j = 0; j < k; ++j) {
      const auto& first = p.frames[j].header.metadata.*member;
      if (!first) continue;
      const std::string pair = p.frames[j].name + "-" + p.frames[k].name;
      if (!(first->value == mine->value)) mismatch(pair, name, to_text(first->value), to_text(mine->value), "esatta");
      p.compat.push(record_of(pair, name, to_json(first->value), to_json(mine->value), "exact", json::null()));
      return;
    }
  };
  const auto int_text = [](std::int64_t v) { return std::to_string(v); };
  const auto int_json = [](std::int64_t v) { return json::integer(v); };
  const auto str_text = [](const std::string& v) { return v; };
  const auto str_json = [](const std::string& v) { return json::string(v); };
  field("roi_x", &frame_metadata::roi_x, int_text, int_json);
  field("roi_y", &frame_metadata::roi_y, int_text, int_json);
  field("readout_mode", &frame_metadata::readout_mode, str_text, str_json);
  field("row_order", &frame_metadata::row_order, str_text, str_json);
}

// Metadati del master (C-02.a, aggiunte di R2): comuni, dal frame di riferimento (esposizione e temperatura), omessi.
frame_metadata master_metadata_of(master_plan& p) {
  frame_metadata m;
  const auto field = [&](auto member, const char* name, auto to_json) {
    const auto& ref = p.frames[0].header.metadata.*member;
    bool common = true;
    for (const frame_state& f : p.frames) common = common && f.header.metadata.*member == ref;
    const bool tolerant = std::string(name) == "exposure_s" || std::string(name) == "temperature_c";
    if (!common && !tolerant) {
      p.not_common.push(json::string(name));
      return;
    }
    if (!ref) return;
    m.*member = ref;
    json e = json::object();
    e.set("value", to_json(ref->value));
    e.set("source", json::string(common ? "common" : "reference_frame"));
    p.master_metadata.set(name, std::move(e));
  };
  const auto d = [](double v) { return json::number(v); };
  const auto s = [](const std::string& v) { return json::string(v); };
  const auto i = [](std::int64_t v) { return json::integer(v); };
  field(&frame_metadata::exposure_s, "exposure_s", d);
  field(&frame_metadata::gain, "gain", d);
  field(&frame_metadata::conversion_gain_e_per_adu, "conversion_gain_e_per_adu", d);
  field(&frame_metadata::offset_adu, "offset_adu", d);
  field(&frame_metadata::temperature_c, "temperature_c", d);
  field(&frame_metadata::filter, "filter", s);
  field(&frame_metadata::camera, "camera", s);
  field(&frame_metadata::imagetyp, "imagetyp", s);
  field(&frame_metadata::pedestal_adu, "pedestal_adu", d);
  field(&frame_metadata::roi_x, "roi_x", i);
  field(&frame_metadata::roi_y, "roi_y", i);
  field(&frame_metadata::readout_mode, "readout_mode", s);
  field(&frame_metadata::row_order, "row_order", s);
  m.saturation_adu.reset();
  if (p.has_sat) {
    m.saturation_adu = metadata_value<double>{p.sat, {metadata_origin::declared, {}}};
    json e = json::object();
    e.set("value", json::number(p.sat));
    e.set("source", json::string("recipe"));
    p.master_metadata.set("saturation_adu", std::move(e));
  }
  return m;
}

// `median_ratio` (C-02.b-flat §f.1 regola, §f.2 controlli 2-4), per canale e con i frame nell'ordine canonico:
// 1/m_(f,c), la loro somma da sinistra, m_H,c = n / Σ, r_(f,c) = m_H,c / m_(f,c). Ogni controllo per tutti i canali
// prima del successivo. Confronti dei controlli 3 e 4 in aritmetica esatta sui valori f64.
void flat_scale_constants(master_plan& p) {
  const master_recipe& r = p.recipe;
  const std::size_t channels = p.channel_count();
  const auto require_normal = [&](double v, stop_error::context_list context, std::size_t c) {
    if (!std::isfinite(v) || !is_normal(v)) {
      stop(error_category::numeric, "master.flat_scale_nonfinite", "costante delle scale del flat non finita o non normale",
           p.flat_context(std::move(context), c));
    }
  };
  for (std::size_t c = 0; c < channels; ++c) {
    double sum = 0;
    for (const frame_state& f : p.frames) {
      const double inverse = 1 / f.level[c];
      require_normal(inverse, {{"costante", "1/m_f"}, {"frame", f.name}, {"m_f", num_text(f.level[c])}}, c);
      sum += inverse;
    }
    require_normal(sum, {{"costante", "somma"}, {"valore", num_text(sum)}}, c);
    const double harmonic = static_cast<double>(p.n) / sum;
    require_normal(harmonic, {{"costante", "m_H"}, {"valore", num_text(harmonic)}}, c);
    p.level_harmonic[c] = harmonic;
    for (frame_state& f : p.frames) {
      f.scale[c] = harmonic / f.level[c];
      require_normal(f.scale[c], {{"costante", "r_f"}, {"frame", f.name}, {"m_f", num_text(f.level[c])}}, c);
    }
  }
  if (p.method == method_kind::median || p.method == method_kind::winsorized || p.method == method_kind::sigma_clip) {
    // 4 · min m è esatto in f64 (potenza di 2; un overflow a +inf lascia invariato l'esito del confronto).
    for (std::size_t c = 0; c < channels; ++c) {
      double lo = p.frames[0].level[c], hi = lo;
      for (const frame_state& f : p.frames) {
        lo = std::min(lo, f.level[c]);
        hi = std::max(hi, f.level[c]);
      }
      if (!(hi <= 4 * lo)) outside_variance_domain(r, "max m > 4 min m", p.per_channel ? p.labels[c] : nullptr);
    }
  }
  if (p.method == method_kind::mean) return;
  // A · |r − 1| ≤ budget · m_H per ogni frame, cioè A · ρ ≤ budget · m_H, senza sottrazione: con r ≥ 1
  // A · r ≤ A + budget · m_H, con r < 1 A ≤ A · r + budget · m_H.
  const double bound = *r.flat_additive_bound_adu, budget = *r.flat_norm_rel_budget;
  const dyadic a = dyadic::of(bound), b = dyadic::of(budget);
  for (std::size_t c = 0; c < channels; ++c) {
    const dyadic allowed = b * dyadic::of(p.level_harmonic[c]);
    double rho = 0;
    bool over = false;
    for (const frame_state& f : p.frames) {
      const double s = f.scale[c];
      rho = std::max(rho, std::fabs(s - 1));
      const dyadic as = a * dyadic::of(s);
      over = over || (s >= 1 ? compare(as, a + allowed) > 0 : compare(a, as + allowed) > 0);
    }
    if (over) {
      stop(error_category::unsupported, "master.flat_norm_budget", "A · rho oltre flat_norm_rel_budget · m_H",
           p.flat_context({{"A", num_text(bound)},
                           {"rho", num_text(rho)},
                           {"m_H", num_text(p.level_harmonic[c])},
                           {"budget", num_text(budget)}},
                          c));
    }
  }
}

// Dopo l'ispezione di tutti i frame: livelli del flat, uscita allocata, metadati del master.
void finish_preflight(master_plan& p) {
  const image& ref = p.frames[0].header;
  p.w = ref.geometry.width;
  p.h = ref.geometry.height;
  p.offset = ref.metadata.offset_adu->value;
  const bool ratio = p.flat && *p.recipe.flat_norm == flat_normalization::median_ratio;
  if (ratio) flat_scale_constants(p);
  // m̄_c serve alla tolleranza dei livelli, che con `median_ratio` non esiste (C-02.b-flat §f.2), e ai campi dei
  // livelli nel report; con `median_ratio` quei campi si decidono in B9/B11.
  if (p.flat && !ratio) {
    // m̄_c in ordine canonico; poi la tolleranza per frame canonico e, nel frame, per canale: la prima violazione.
    const std::size_t channels = p.channel_count();
    for (std::size_t c = 0; c < channels; ++c) {
      double sum = p.frames[0].level[c];
      for (std::size_t k = 1; k < p.n; ++k) sum += p.frames[k].level[c];
      p.level_mean[c] = sum / static_cast<double>(p.n);
      if (!std::isfinite(p.level_mean[c])) {
        stop(error_category::numeric, "master.report_nonfinite", "media dei livelli del flat non finita", p.flat_context({}, c));
      }
    }
    for (const frame_state& f : p.frames) {
      for (std::size_t c = 0; c < channels; ++c) {
        const double rel = f.level[c] / p.level_mean[c] - 1;
        if (std::fabs(rel) > p.eps) {
          stop(error_category::unsupported, "master.variance_domain", "livello del flat oltre flat_level_rel_tol",
               p.flat_context({{"frame", f.name},
                               {"m_f", num_text(f.level[c])},
                               {"m_media", num_text(p.level_mean[c])},
                               {"tolleranza", num_text(p.eps)}},
                              c));
        }
      }
    }
  }
  // `median_ratio` ammesso ma non ancora calcolato: combinare gli x_f senza normalizzarli darebbe un master sbagliato.
  // Stop dopo il preflight, prima dei blocchi e di ogni artefatto (M3b-lacune, «Decisioni successive (B8)»).
  if (ratio) {
    stop(error_category::unsupported, "master.method_not_computable", "flat_normalization median_ratio non ancora calcolata",
         {{"flat_normalization", "median_ratio"}});
  }
  if (!p.has_sat) p.warnings.push_back("saturation_not_evaluated");
  if (*p.recipe.temp_policy == temperature_policy::not_applicable_declared && p.n > 1) {
    p.warnings.push_back("temperature_not_compared");
  }
  if (p.method == method_kind::sigma_clip) p.warnings.push_back("frozen_selection_approximation");
  image_geometry g(p.w, p.h);
  g.binning_x = ref.geometry.binning_x;
  g.binning_y = ref.geometry.binning_y;
  p.out = make_image(g, p.recipe.output_type, ref.layout, ref.cfa);
  p.out.unit = pixel_unit::adu;
  p.out.lin = linearity::linear;
  p.out.lin_origin.value = metadata_origin::header;
  p.out.phase = metadata_value<process_phase>{process_phase::raw, {metadata_origin::header, "ASTRPHAS"}};
  p.out.metadata = master_metadata_of(p);
  std::vector<const image*> frame_headers;
  for (const frame_state& f : p.frames) frame_headers.push_back(&f.header);
  // C-02.b §b.7: con `sigma_clip` il piano a selezione congelata è la riga a pesi congelati di ADR-17.b §b.1.
  std::vector<measure_limit> own;
  if (p.method == method_kind::sigma_clip) own.push_back(measure_limit::variance_fixed_weight_approximation);
  p.out.measure = merge_measure(frame_headers, own);
  p.out.variance = variance_plane{aligned_buffer(p.out.samples.size())};
  const std::size_t px = static_cast<std::size_t>(p.w) * p.h;
  if (p.flat) {
    p.lbe.assign(px, std::numeric_limits<double>::quiet_NaN());
    p.ratio.assign(px, std::numeric_limits<double>::quiet_NaN());
    p.nonpositive.assign(px, 0);
  }
}

// ---------------------------------------------------------------- combinazione per pixel (C-02.a §6, §6.3, §7)

// Elemento dell'ordinamento per pixel (C-02.b §b.8): valore e indice canonico del frame.
struct sort_slot {
  double value;
  std::uint32_t frame;
};

// Esito f64 di un pixel valido: `ok` falso se un'operazione viola la regola di rappresentabilità (C-02.a §6.3).
struct pixel_value {
  double m = 0, v = 0;
  bool ok = true;
};

// s² di C-02.a §6 sugli n campioni nell'ordine canonico: Σ x, / n, (x − x̄)², somma, / (n − 1).
double sample_variance(const double* const* vals, std::size_t j, std::size_t n, bool& ok) {
  double s = vals[0][j];
  for (std::size_t f = 1; f < n; ++f) s += vals[f][j];
  const double mean = s / static_cast<double>(n);
  ok = ok && std::isfinite(s) && div_ok(s, mean);
  double ss = 0;
  for (std::size_t f = 0; ok && f < n; ++f) {
    const double d = vals[f][j] - mean;
    const double sq = d * d;
    ok = std::isfinite(d) && mul_ok(d, d, sq);
    if (!ok) break;
    if (f == 0) {
      ss = sq;
    } else {
      const double t = ss + sq;
      ok = add_ok(ss, sq, t);
      ss = t;
    }
  }
  const double s2 = ss / static_cast<double>(n - 1);
  ok = ok && div_ok(ss, s2);
  return s2;
}

// `single`: x_1 − offset_adu, max(·, 0), / conversion_gain_e_per_adu, read_noise_adu², somma (C-02.b §b.6). I
// parametri sono quelli della ricetta; i valori dell'header non entrano.
pixel_value single_value(const master_recipe& r, double x) {
  pixel_value out;
  out.m = x;
  const double t = x - *r.offset_adu;
  const double pos = t > 0 ? t : 0.0;
  const double gain = *r.conversion_gain_e_per_adu, rn = *r.read_noise_adu;
  const double shot = pos / gain;
  const double r2 = rn * rn;
  out.v = r2 + shot;
  out.ok = std::isfinite(t) && div_ok(pos, shot) && mul_ok(rn, rn, r2) && add_ok(r2, shot, out.v);
  return out;
}

// `weighted_mean`: M = (Σ w_f · x_f) / W, V = Σ c_f · ((x_f − M) · (x_f − M)), somme nell'ordine canonico. I c_f sono
// > 0 (weight_constants), quindi V è una somma di termini non negativi e add_ok vale.
pixel_value weighted_value(const master_plan& p, const double* const* vals, std::size_t j) {
  pixel_value out;
  double acc = 0;
  for (std::size_t f = 0; f < p.n; ++f) {
    const double wx = p.weights[f] * vals[f][j];
    out.ok = out.ok && mul_ok(p.weights[f], vals[f][j], wx);
    acc = f == 0 ? wx : acc + wx;
  }
  out.m = acc / p.weight_sum;
  out.ok = out.ok && std::isfinite(acc) && div_ok(acc, out.m);
  for (std::size_t f = 0; out.ok && f < p.n; ++f) {
    const double d = vals[f][j] - out.m;
    const double q = d * d;
    const double t = p.residual_coeffs[f] * q;
    out.ok = std::isfinite(d) && mul_ok(d, d, q) && mul_ok(p.residual_coeffs[f], q, t);
    if (f == 0) {
      out.v = t;
    } else {
      const double sum = out.v + t;
      out.ok = out.ok && add_ok(out.v, t, sum);
      out.v = sum;
    }
  }
  return out;
}

// `median` e `winsorized`: z ordinati su (valore, indice canonico); a parità di valore, compreso −0 == +0, prima il
// frame canonico minore. Mediana pari: (z_k + z_(k+1)) / 2. Winsorized: Σ v_i per i crescente, poi / n. Piano κ · s².
pixel_value robust_value(const master_plan& p, const double* const* vals, std::size_t j, sort_slot* z, std::size_t g) {
  const std::size_t n = p.n;
  for (std::size_t f = 0; f < n; ++f) z[f] = {vals[f][j], static_cast<std::uint32_t>(f)};
  std::sort(z, z + n, [](const sort_slot& a, const sort_slot& b) {
    if (a.value < b.value) return true;
    if (b.value < a.value) return false;
    return a.frame < b.frame;
  });
  pixel_value out;
  if (p.method == method_kind::median) {
    if (n % 2 == 1) {
      out.m = z[n / 2].value;
    } else {
      const double s = z[n / 2 - 1].value + z[n / 2].value;
      out.m = s / 2.0;
      out.ok = std::isfinite(s) && div_ok(s, out.m);
    }
  } else {
    double s = z[g].value;
    for (std::size_t i = 2; i <= n; ++i) s += i <= g ? z[g].value : i > n - g ? z[n - g - 1].value : z[i - 1].value;
    out.m = s / static_cast<double>(n);
    out.ok = std::isfinite(s) && div_ok(s, out.m);
  }
  const double s2 = sample_variance(vals, j, n, out.ok);
  const double kappa = p.kappa->value;
  out.v = kappa * s2;
  out.ok = out.ok && mul_ok(kappa, s2, out.v);
  return out;
}

// `mean` + `sigma_clip` (C-02.b §b.1, §b.6): per t = 1 … clip_max_iter, μ e s sui frame tenuti S nell'ordine
// canonico (Σ x, / m; Σ (x − μ)², / (m − 1), sqrt); R = frame di S con x − μ > κ_high · s oppure μ − x > κ_low · s,
// disuguaglianze strette; R vuoto o s = 0 chiude; altrimenti S ← S \ R e |S| < clip_min_kept rende il pixel
// support_incomplete. Master μ dell'insieme finale K, piano (Σ_K (x − μ)² / (m − 1)) / m. Ogni operazione segue la
// regola di rappresentabilità di §b.6; il rifiuto non è un'invalidità. `kept[f]`: 1 tenuto, 0 rifiutato, 2 in R
// durante il confronto.
struct clip_outcome {
  pixel_value pv;
  bool incomplete = false;
};

clip_outcome clip_value(const master_plan& p, const double* const* vals, std::size_t j, std::uint8_t* kept) {
  const std::size_t n = p.n;
  const double kl = *p.recipe.clip_kappa_low, kh = *p.recipe.clip_kappa_high;
  // clip_max_iter non ha un massimo (C-02.b §b.1): ogni iterazione che non chiude toglie almeno un campione, quindi
  // min(clip_max_iter, n) dà la stessa procedura; il minimo è in f64, prima della conversione in intero.
  const auto max_iter = static_cast<std::size_t>(std::min(*p.recipe.clip_max_iter, static_cast<double>(n)));
  const auto min_kept = static_cast<std::size_t>(*p.recipe.clip_min_kept);
  std::fill(kept, kept + n, std::uint8_t{1});
  std::size_t m = n;
  clip_outcome out;
  pixel_value& pv = out.pv;
  double s2 = 0;
  const auto kept_stats = [&] {
    double sum = 0;
    bool first = true;
    for (std::size_t f = 0; f < n; ++f) {
      if (kept[f] == 0) continue;
      sum = first ? vals[f][j] : sum + vals[f][j];
      first = false;
    }
    pv.m = sum / static_cast<double>(m);
    pv.ok = std::isfinite(sum) && div_ok(sum, pv.m);
    double ss = 0;
    first = true;
    for (std::size_t f = 0; pv.ok && f < n; ++f) {
      if (kept[f] == 0) continue;
      const double d = vals[f][j] - pv.m;
      const double q = d * d;
      pv.ok = std::isfinite(d) && mul_ok(d, d, q);
      if (!pv.ok) break;
      if (first) {
        ss = q;
      } else {
        const double t = ss + q;
        pv.ok = add_ok(ss, q, t);
        ss = t;
      }
      first = false;
    }
    s2 = ss / static_cast<double>(m - 1);
    pv.ok = pv.ok && div_ok(ss, s2);
  };
  kept_stats();
  for (std::size_t t = 1; pv.ok && t <= max_iter; ++t) {
    const double s = std::sqrt(s2);
    if (s == 0) break;
    const double th = kh * s, tl = kl * s;
    pv.ok = mul_ok(kh, s, th) && mul_ok(kl, s, tl);
    if (!pv.ok) break;
    std::size_t rejected = 0;
    for (std::size_t f = 0; f < n; ++f) {
      if (kept[f] == 0) continue;
      const double x = vals[f][j];
      if (x - pv.m > th || pv.m - x > tl) {
        kept[f] = 2;
        ++rejected;
      }
    }
    if (rejected == 0) break;
    for (std::size_t f = 0; f < n; ++f) {
      if (kept[f] == 2) kept[f] = 0;
    }
    m -= rejected;
    if (m < min_kept) {
      out.incomplete = true;
      pv.ok = false;
      return out;
    }
    kept_stats();
  }
  if (pv.ok) {
    pv.v = s2 / static_cast<double>(m);
    pv.ok = div_ok(s2, pv.v);
  }
  return out;
}

// Buffer per task del modello di memoria (ADR-06, estensione R2.r): ordinamento (`median`, `winsorized`), indicatori
// dei frame tenuti e parziali di `rejected_samples` per frame (`sigma_clip`), n elementi ciascuno.
struct task_scratch {
  sort_slot* sort;
  std::uint8_t* kept;
  std::uint64_t* rejected;
};

// Conversione al sample_type del master (C-02.a §6.3). Con uscita f64 è un'identità: un valore subnormale non vi
// diventa subnormale, e arriva qui solo come campione d'ingresso scelto senza operazione (`single`, mediana dispari),
// perché ogni prodotto, quoziente e somma di termini non negativi è già controllato; resta valido. Con uscita f32 vale
// convert_finite: un valore non nullo che diventa zero o subnormale non è rappresentabile.
template <class T>
bool to_output(double v, T& out) {
  if constexpr (std::is_same_v<T, double>) {
    out = v;
    return std::isfinite(v);
  } else {
    return convert_finite<float>(v, out);
  }
}

// Banda dei metodi di C-02.b diversi da `mean`: maschere come C-02.a §7, accumulo f64 e un solo arrotondamento al
// sample_type (to_output). Lo stesso codice per i due percorsi di ADR-07: ordinamento e selezione per pixel
// sono operazioni IEEE identiche, quindi bit-identiche fra i percorsi. Il flat ammette qui solo `single` (§b.2): il
// suo level_bias_estimate non è definito con n = 1 (§b.7, `single_frame`) e resta NaN.
template <class T>
void method_band(master_plan& p, std::uint32_t y0, std::uint32_t rows, const double* const* vals,
                 const std::uint16_t* const* bits, const task_scratch& scratch) {
  T* out = reinterpret_cast<T*>(p.out.samples.data());
  T* outv = reinterpret_cast<T*>(p.out.variance->samples.data());
  constexpr T nan = std::numeric_limits<T>::quiet_NaN();
  const std::size_t g = p.method == method_kind::winsorized ? static_cast<std::size_t>(*p.recipe.winsor_g) : 0;
  const bool clip = p.method == method_kind::sigma_clip;
  if (clip) std::fill(scratch.rejected, scratch.rejected + p.n, std::uint64_t{0});
  for (std::uint32_t r = 0; r < rows; ++r) {
    for (std::uint32_t c = 0; c < p.w; ++c) {
      const std::size_t j = static_cast<std::size_t>(r) * p.w + c;
      const std::size_t i = static_cast<std::size_t>(y0 + r) * p.w + c;
      std::uint16_t m = 0;
      bool saturated = false;
      for (std::size_t f = 0; f < p.n; ++f) {
        m = static_cast<std::uint16_t>(m | bits[f][j]);
        if (p.has_sat && vals[f][j] >= p.sat) saturated = true;
      }
      if (saturated) m = static_cast<std::uint16_t>(m | invalid_reason::saturated);
      if (m == 0) {
        pixel_value pv;
        std::uint16_t failure = invalid_reason::result_nonfinite;
        switch (p.method) {
          case method_kind::single: pv = single_value(p.recipe, vals[0][j]); break;
          case method_kind::weighted_mean: pv = weighted_value(p, vals, j); break;
          case method_kind::median:
          case method_kind::winsorized: pv = robust_value(p, vals, j, scratch.sort, g); break;
          case method_kind::sigma_clip: {
            const clip_outcome co = clip_value(p, vals, j, scratch.kept);
            pv = co.pv;
            if (co.incomplete) failure = invalid_reason::support_incomplete;
            break;
          }
          case method_kind::mean: ASTRO_ASSERT(false); break;
        }
        // rejected_samples (M3b-lacune, «Correzioni della review di B6 (giro 1)»): gli R di un pixel si contano,
        // compreso l'ultimo che lo porta a support_incomplete, solo dopo l'esito finale: valore valido, conversione al
        // tipo d'uscita compresa, oppure support_incomplete. Un pixel result_nonfinite in qualunque punto non conta.
        const auto count_rejected = [&] {
          for (std::size_t f = 0; f < p.n; ++f) scratch.rejected[f] += scratch.kept[f] == 0 ? 1 : 0;
        };
        T ms{}, vs{};
        if (pv.ok && to_output<T>(pv.m, ms) && to_output<T>(pv.v, vs)) {
          out[i] = ms;
          outv[i] = vs;
          p.out.invalid.bits[i] = 0;
          if (p.flat) p.nonpositive[i] = pv.m - p.offset <= 0 ? 1 : 0;
          if (clip) count_rejected();
          continue;
        }
        if (clip && failure == invalid_reason::support_incomplete) count_rejected();
        m = failure;
      }
      out[i] = nan;
      outv[i] = nan;
      p.out.invalid.bits[i] = m;
    }
  }
  // C-02.b §b.11 somma i conteggi per frame in `finalize`: qui i parziali del task si sommano ai totali con interi
  // atomici, e una somma di interi non dipende dall'ordine, quindi il risultato è lo stesso.
  if (clip) {
    for (std::size_t f = 0; f < p.n; ++f) {
      if (scratch.rejected[f] != 0) {
        std::atomic_ref<std::uint64_t>(p.frames[f].rejected).fetch_add(scratch.rejected[f], std::memory_order_relaxed);
      }
    }
  }
}

// `vals[f]` e `bits[f]`: righe [y0, y0 + rows) del frame canonico f; `scratch`: buffer del task, n elementi ciascuno.
// `mean` usa il kernel del percorso risolto nel preflight (ADR-07); ogni banda è l'avvio di un task per il controllo
// dell'ambiente FP. Righe disgiunte: thread-safe.
void combine_rows(master_plan& p, std::uint32_t y0, std::uint32_t rows, const double* const* vals,
                  const std::uint16_t* const* bits, const task_scratch& scratch) {
  if (p.method != method_kind::mean) {
    begin_kernel_task();
    if (p.out.type == sample_type::f32) method_band<float>(p, y0, rows, vals, bits, scratch);
    else method_band<double>(p, y0, rows, vals, bits, scratch);
    return;
  }
  kernels::master_band b;
  b.n = p.n;
  b.w = p.w;
  b.y0 = y0;
  b.rows = rows;
  b.vals = vals;
  b.bits = bits;
  b.has_sat = p.has_sat;
  b.sat = p.sat;
  b.f32_out = p.out.type == sample_type::f32;
  b.out = p.out.samples.data();
  b.outv = p.out.variance->samples.data();
  b.mask = p.out.invalid.bits.data();
  b.flat = p.flat;
  b.offset = p.offset;
  b.eps = p.eps;
  b.lbe = p.lbe.data();
  b.ratio = p.ratio.data();
  b.nonpositive = p.nonpositive.data();
  begin_kernel_task();
  if (p.path == cpu_path::avx2) kernels::master_band_avx2(b);
  else kernels::master_band_scalar(b);
}

std::string rows_digest(const image& out, std::uint32_t y0, std::uint32_t y1) {
  const std::size_t elem = sample_size(out.type);
  const std::size_t first = static_cast<std::size_t>(y0) * out.geometry.width;
  const std::size_t count = static_cast<std::size_t>(y1 - y0) * out.geometry.width;
  const auto bytes = [](const void* q, std::size_t n) {
    return std::span<const std::uint8_t>(static_cast<const std::uint8_t*>(q), n);
  };
  sha256_stream s;
  s.update(bytes(out.samples.data() + first * elem, count * elem));
  s.update(bytes(out.invalid.bits.data() + first, count * sizeof(std::uint16_t)));
  s.update(bytes(out.variance->samples.data() + first * elem, count * elem));
  return s.finish().sha256;
}

// ---------------------------------------------------------------- report (C-02.a §7)

json number_or_null(const std::optional<double>& v) { return v ? json::number(*v) : json::null(); }

json frame_report(const master_plan& p, const frame_state& f) {
  json o = json::object();
  o.set("name", json::string(f.name));
  o.set("digest_sha256", json::string(f.spec->digest.sha256));
  o.set("size_bytes", json::uinteger(f.spec->digest.size_bytes));
  o.set("role", json::string(role_name(p.recipe.role)));
  o.set("used", json::boolean(true));
  o.set("rejected_samples", json::uinteger(f.rejected));
  o.set("saturated_samples", p.has_sat ? json::uinteger(f.saturated) : json::null());
  o.set("saturated_fraction", p.has_sat ? json::number(f.saturated_fraction) : json::null());
  if (p.flat && p.per_channel) {
    json levels = json::array();
    for (std::size_t c = 0; c < 4; ++c) {
      json e = json::object();
      e.set("label", json::string(p.labels[c]));
      e.set("level", json::number(f.level[c]));
      e.set("rel_dev", json::number(f.level[c] / p.level_mean[c] - 1));
      levels.push(std::move(e));
    }
    o.set("flat_levels", std::move(levels));
  } else if (p.flat) {
    o.set("flat_level", json::number(f.level[0]));
    o.set("flat_level_rel_dev", json::number(f.level[0] / p.level_mean[0] - 1));
  }
  const image& im = f.header;
  json fields = json::object();
  const auto field = [](json value, bool declared) {
    json e = json::object();
    e.set("value", std::move(value));
    e.set("origin", json::string(declared ? "declared" : "header"));
    return e;
  };
  fields.set("binning_x", field(json::uinteger(*im.geometry.binning_x), f.origins.binning_x));
  fields.set("binning_y", field(json::uinteger(*im.geometry.binning_y), f.origins.binning_y));
  fields.set("unit", field(json::string("adu"), f.origins.unit));
  fields.set("linearity", field(json::string("linear"), f.origins.lin));
  fields.set("phase", field(json::string(to_string(im.phase->value)), im.phase->source.origin == metadata_origin::declared));
  o.set("image_fields", std::move(fields));
  o.set("layout", json::string(layout_name(im.layout)));
  o.set("cfa", json::string(cfa_text(im.cfa)));
  o.set("sample_type", json::string(type_name(im.type)));
  o.set("metadata", metadata_report(im));
  json keywords = json::array();
  for (const std::string& k : im.uninterpreted_keywords) keywords.push(json::string(k));
  o.set("uninterpreted_keywords", std::move(keywords));
  return o;
}

json recipe_report(const master_recipe& r) {
  json o = json::object();
  std::string name = std::string("master_") + combine_name(*r.combine);
  if (*r.rejection != master_rejection::none) name += std::string("_") + rejection_name(*r.rejection);
  o.set("name", json::string(name));
  o.set("role", json::string(role_name(r.role)));
  o.set("combine", json::string(combine_name(*r.combine)));
  o.set("rejection", json::string(rejection_name(*r.rejection)));
  o.set("winsor_g", number_or_null(r.winsor_g));
  o.set("clip_kappa_low", number_or_null(r.clip_kappa_low));
  o.set("clip_kappa_high", number_or_null(r.clip_kappa_high));
  o.set("clip_max_iter", number_or_null(r.clip_max_iter));
  o.set("clip_min_kept", number_or_null(r.clip_min_kept));
  o.set("offset_adu", number_or_null(r.offset_adu));
  o.set("conversion_gain_e_per_adu", number_or_null(r.conversion_gain_e_per_adu));
  o.set("read_noise_adu", number_or_null(r.read_noise_adu));
  o.set("decoder_black_abs_tol_adu",
        r.decoder_black_abs_tol_adu ? json::integer(*r.decoder_black_abs_tol_adu) : json::null());
  o.set("analysis", r.analysis);
  o.set("flat_normalization", r.flat_norm ? json::string(flat_norm_name(*r.flat_norm)) : json::null());
  o.set("flat_level_rel_tol", number_or_null(r.flat_level_rel_tol));
  o.set("flat_min_signal_adu", number_or_null(r.flat_min_signal_adu));
  o.set("flat_additive_bound_adu", number_or_null(r.flat_additive_bound_adu));
  o.set("flat_norm_rel_budget", number_or_null(r.flat_norm_rel_budget));
  o.set("saturation_adu", number_or_null(r.saturation_adu));
  o.set("saturated_fraction_max", number_or_null(r.saturated_fraction_max));
  o.set("exposure_rel_tol", json::number(*r.exposure_rel_tol));
  o.set("temperature_abs_tol_c", json::number(*r.temperature_abs_tol_c));
  o.set("temperature_policy", json::string(policy_name(*r.temp_policy)));
  o.set("output_type", json::string(type_name(r.output_type)));
  o.set("simd", json::string(simd_request_name(*r.simd)));
  return o;
}

// La mediana di un numero pari di valori finiti può andare in overflow nella media dei due centrali.
json median_or_null(std::vector<double>& v) {
  if (v.empty()) return json::null();
  const double m = median_of(v);
  if (!std::isfinite(m)) stop(error_category::numeric, "master.report_nonfinite", "mediana di una statistica del report non finita");
  return json::number(m);
}
json max_or_null(const std::vector<double>& v) {
  return v.empty() ? json::null() : json::number(*std::max_element(v.begin(), v.end()));
}

json channel_report(const master_plan& p) {
  const std::size_t channels = p.per_channel ? 4 : 1;
  std::vector<std::vector<double>> planes(channels), lbe(channels), ratio(channels);
  std::vector<std::uint64_t> ratio_null(channels, 0), nonpositive(channels, 0), valid(channels, 0);
  for (std::uint32_t y = 0; y < p.h; ++y) {
    for (std::uint32_t x = 0; x < p.w; ++x) {
      const std::size_t i = static_cast<std::size_t>(y) * p.w + x;
      if (p.out.invalid.bits[i] != 0) continue;
      const std::size_t c = p.channel_of(x, y);
      ++valid[c];
      const double v = p.out.type == sample_type::f32 ? static_cast<double>(p.out.variance_data<float>()[i])
                                                      : p.out.variance_data<double>()[i];
      planes[c].push_back(v);
      if (!p.flat) continue;
      lbe[c].push_back(p.lbe[i]);
      if (std::isnan(p.ratio[i])) ++ratio_null[c];
      else ratio[c].push_back(p.ratio[i]);
      nonpositive[c] += p.nonpositive[i];
    }
  }
  json list = json::array();
  for (std::size_t c = 0; c < channels; ++c) {
    json o = json::object();
    o.set("label", json::string(p.labels[c]));
    if (p.flat && p.per_channel) o.set("flat_level_mean", json::number(p.level_mean[c]));
    o.set("valid_pixels", json::uinteger(valid[c]));
    o.set("variance_plane_median", median_or_null(planes[c]));
    if (p.flat && p.method == method_kind::single) {
      // C-02.b §b.7: con un solo frame la stima non è definita (nessuna divisione per n − 1).
      json lb = json::object();
      for (const char* k : {"median", "max", "ratio_median", "ratio_max", "level_bias_ratio_null"}) lb.set(k, json::null());
      lb.set("null_reason", json::string("single_frame"));
      o.set("level_bias_estimate", std::move(lb));
      o.set("pixels_nonpositive_signal", json::uinteger(nonpositive[c]));
    } else if (p.flat) {
      json lb = json::object();
      lb.set("median", median_or_null(lbe[c]));
      lb.set("max", max_or_null(lbe[c]));
      lb.set("ratio_median", median_or_null(ratio[c]));
      lb.set("ratio_max", max_or_null(ratio[c]));
      lb.set("level_bias_ratio_null", json::uinteger(ratio_null[c]));
      lb.set("ratio_null_reason", json::string("variance_zero"));
      o.set("level_bias_estimate", std::move(lb));
      o.set("pixels_nonpositive_signal", json::uinteger(nonpositive[c]));
    }
    list.push(std::move(o));
  }
  return list;
}

json build_report(master_plan& p) {
  json report = json::object();
  report.set("recipe", recipe_report(p.recipe));
  json frames = json::array();
  for (const frame_state& f : p.frames) frames.push(frame_report(p, f));
  report.set("frames", std::move(frames));
  report.set("frames_used", json::uinteger(p.n));
  report.set("frames_discarded", json::uinteger(0));
  report.set("rejection", json::string("none"));
  report.set("reference_frame", json::string(p.frames[0].spec->digest.sha256));
  report.set("compatibility", p.compat);
  if (p.flat && !p.per_channel) report.set("flat_level_mean", json::number(p.level_mean[0]));
  report.set("master_metadata", p.master_metadata);
  report.set("metadata_not_common", p.not_common);
  // Piano e coefficiente di variazione per metodo (C-02.b §b.6, §b.7): sqrt(2/(n − 1)) per `mean`, `median` e
  // `winsorized`; null con motivo per `single` (`single_frame`), `weighted_mean` e `sigma_clip` (`not_derived`).
  json var = json::object();
  var.set("variance_estimator", json::string(estimator_name(*p.descriptor.estimator)));
  var.set("variance_source", json::string("plane"));
  const char* plane = "s2_over_n";
  const char* cv_null = nullptr;
  switch (p.method) {
    case method_kind::single: plane = "read_noise2_plus_signal_over_gain"; cv_null = "single_frame"; break;
    case method_kind::weighted_mean: plane = "weighted_residuals"; cv_null = "not_derived"; break;
    case method_kind::median:
    case method_kind::winsorized: plane = "kappa_s2"; break;
    case method_kind::sigma_clip: plane = "kept_s2_over_m"; cv_null = "not_derived"; break;
    case method_kind::mean: break;
  }
  var.set("plane", json::string(plane));
  if (cv_null) {
    var.set("variance_cv_gaussian_homoscedastic", json::null());
    var.set("variance_cv_null_reason", json::string(cv_null));
  } else {
    var.set("variance_cv_gaussian_homoscedastic", json::number(std::sqrt(2.0 / static_cast<double>(p.n - 1))));
  }
  report.set("variance", std::move(var));
  report.set("channels", channel_report(p));
  invalid_reason::bit_counts counts{};
  std::uint64_t invalid = 0;
  for (const std::uint16_t b : p.out.invalid.bits) {
    if (b != 0) ++invalid;
    for (std::size_t k = 0; k < counts.size(); ++k) counts[k] += (b >> k) & 1u;
  }
  json bitcounts = json::object();
  for (std::size_t k = 0; k < counts.size(); ++k) {
    bitcounts.set(invalid_reason::bit_names[k], json::uinteger(counts[k]));
  }
  report.set("invalid_counts", std::move(bitcounts));
  report.set("invalid_pixels", json::uinteger(invalid));
  json hyp = json::array();
  for (const char* h : {"frames_independent", "outliers_sparse"}) hyp.push(json::string(h));
  if (p.flat) {
    for (const char* h : {"illumination_stable", "same_illumination_shape", "level_proportional", "additive_nonnegative",
                          "nonnegative_signal"}) {
      hyp.push(json::string(h));
    }
  } else {
    hyp.push(json::string("stationary_mean"));
  }
  for (const std::string& h : p.hypotheses) hyp.push(json::string(h));
  report.set("assumptions_not_verified", std::move(hyp));
  json warnings = json::array();
  for (const std::string& w : p.warnings) warnings.push(json::string(w));
  report.set("warnings", std::move(warnings));
  report.set("descriptor", descriptor_json(p.descriptor));
  return report;
}

master_result finish_result(master_plan& p) {
  master_result r;
  r.report = build_report(p);
  r.report_text = master_report_text(r.report);
  r.warnings = p.warnings;
  r.level_bias_estimate = std::move(p.lbe);
  r.output = std::move(p.out);
  return r;
}

json observed_inputs(const master_plan& p) {
  json list = json::array();
  for (const frame_state& f : p.frames) {
    json fr = frame_report(p, f);
    const json& fields = *fr.find("image_fields");
    json o = json::object();
    o.set("input_name", json::string(f.name));
    o.set("sha256", json::string(f.spec->digest.sha256));
    o.set("size_bytes", json::uinteger(f.spec->digest.size_bytes));
    o.set("phase", *fields.find("phase"));
    o.set("linearity", *fields.find("linearity"));
    o.set("uninterpreted_keywords", *fr.find("uninterpreted_keywords"));
    list.push(std::move(o));
  }
  return list;
}

void render(std::ostringstream& out, const json& value, int depth, const std::string& key) {
  const std::string pad(static_cast<std::size_t>(depth) * 2, ' ');
  switch (value.type()) {
    case json::kind::object: {
      if (!key.empty()) out << pad << key << ":\n";
      std::vector<std::size_t> order(value.keys().size());
      for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
      std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return value.keys()[a] < value.keys()[b]; });
      for (const std::size_t i : order) render(out, value.items()[i], key.empty() ? depth : depth + 1, value.keys()[i]);
      return;
    }
    case json::kind::array:
      out << pad << key << ":" << (value.items().empty() ? " []" : "") << "\n";
      for (const json& item : value.items()) {
        if (item.type() == json::kind::object || item.type() == json::kind::array) {
          out << pad << "  -\n";
          render(out, item, depth + 2, "");
        } else {
          out << pad << "  - " << canonical_json(item) << "\n";
        }
      }
      return;
    default:
      out << pad << key << ": " << canonical_json(value) << "\n";
  }
}

// Header di uscita: al più tre HDU con max_header_cards card ciascuno, più il riempimento dei blocchi.
constexpr std::uint64_t output_header_bound = 3 * (std::uint64_t{fits_limits::max_header_cards} * 80 + 2 * 2880);

[[noreturn]] void not_master_node(const node_spec& node, const char* why) {
  stop(error_category::invalid_input, "master.not_master_node", "il nodo non è un master del prodotto",
       {{"operatore", node.operator_id()}, {"motivo", why}});
}

// ---------------------------------------------------------------- frame RAW (ADR-16.b §b.7, C-02.b §b.2)

// Prima di ogni altro controllo sui parametri: con frame RAW e FITS insieme le regole dei due casi non si compongono.
void require_one_frame_kind(const master_recipe& r, const std::vector<std::size_t>& order) {
  if (order.empty()) return;
  const bool reference_raw = r.frames[order[0]].decoder.has_value();
  for (std::size_t k = 1; k < order.size(); ++k) {
    if (r.frames[order[k]].decoder.has_value() != reference_raw) {
      stop(error_category::incompatible, "master.decoder_state_mixed", "frame RAW e frame FITS nello stesso master",
           {{"riferimento", frame_name(0)}, {"riferimento_raw", reference_raw ? "true" : "false"}, {"frame", frame_name(k)}});
    }
  }
}

[[noreturn]] void bad_decoder_state_param(const std::string& frame, const std::string& field, const std::string& why) {
  stop(error_category::invalid_input, "master.decoder_state_param", "stato del decoder di un frame non valido",
       {{"frame", frame}, {"campo", field}, {"motivo", why}});
}

// Elemento di `frame_decoder_states`: chiavi del record del decoder per lo stato (ADR-08.b §2.9) e per l'identità.
json frame_decoder_state_json(const frame_decoder_state& f) {
  json o = decoder_state_record(f.state);
  const json identity = decoder_identity_record(f.identity);
  for (std::size_t i = 0; i < identity.keys().size(); ++i) o.set(identity.keys()[i], identity.items()[i]);
  return o;
}

// Inverso di `frame_decoder_state_json`. L'elemento deve essere esattamente la forma canonica dello stato letto, così
// una chiave in più o un valore non canonico non passano in silenzio.
frame_decoder_state frame_decoder_state_of_json(const json& item, const std::string& frame) {
  if (item.type() != json::kind::object) bad_decoder_state_param(frame, "frame_decoder_states", "elemento non oggetto");
  frame_decoder_state f;
  try {
    f.state = decoder_state_of_record(item);
    f.identity = decoder_identity_of_record(item);
  } catch (const stop_error& e) {
    // Stesso arresto con il codice e il contesto della lettura del record.
    stop_error::context_list context = {{"frame", frame}, {"campo", "frame_decoder_states"}, {"codice_lettura", e.code()}};
    for (const auto& entry : e.context()) context.push_back(entry);
    stop(error_category::invalid_input, "master.decoder_state_param", "stato del decoder di un frame non leggibile",
         std::move(context));
  }
  if (const char* field = decoder_state_violation(f.state)) bad_decoder_state_param(frame, field, "invariante di ADR-03.b §b.2");
  if (canonical_json(frame_decoder_state_json(f)) != canonical_json(item)) {
    bad_decoder_state_param(frame, "frame_decoder_states", "elemento con chiavi o valori fuori dalla forma canonica");
  }
  return f;
}

// Valore di un campo strutturale di ADR-16.b §b.7 nella forma JSON canonica del record del decoder.
std::string structural_text(const decoder_state& s, std::string_view field) {
  const json record = decoder_state_record(s);
  const json& black = *record.find("black");
  if (field == "raw_width") return canonical_json(*record.find("raw_width"));
  if (field == "raw_height") return canonical_json(*record.find("raw_height"));
  if (field == "window") return canonical_json(*record.find("crop"));
  if (field == "cfa_color_index") return canonical_json(*record.find("cfa_color_index"));
  if (field == "black_pattern_rows") return canonical_json(*black.find("pattern_rows"));
  if (field == "black_pattern_cols") return canonical_json(*black.find("pattern_cols"));
  if (field == "linearization") return canonical_json(*record.find("linearization"));
  if (field == "curve_sha256") return canonical_json(*record.find("linearization_curve_sha256"));
  ASTRO_ASSERT(false);  // decoder_state_difference restituisce solo i campi di §b.7
  return {};
}

// Preflight del nodo per i frame RAW (C-02.b §b.2 «Selezione ed esecuzione»): invarianti di ogni stato, poi campi
// strutturali per uguaglianza e nero effettivo entro la tolleranza col riferimento canonico. Il nero non si sottrae.
void validate_raw_frames(const master_recipe& r, const std::vector<std::size_t>& order) {
  if (order.empty() || !r.frames[order[0]].decoder) return;
  // Invarianti prima della scrittura dell'elemento, poi rilettura: ricetta e descrittore accettano gli stessi stati.
  for (std::size_t k = 0; k < order.size(); ++k) {
    const frame_decoder_state& f = *r.frames[order[k]].decoder;
    if (const char* field = decoder_state_violation(f.state)) {
      bad_decoder_state_param(frame_name(k), field, "invariante di ADR-03.b §b.2");
    }
    (void)frame_decoder_state_of_json(frame_decoder_state_json(f), frame_name(k));
  }
  const std::uint64_t tol = *r.decoder_black_abs_tol_adu;
  const decoder_state& ref = r.frames[order[0]].decoder->state;
  const decoder_black_period ref_black = decoder_black_period_of(ref);
  for (std::size_t k = 1; k < order.size(); ++k) {
    const decoder_state& s = r.frames[order[k]].decoder->state;
    const decoder_black_period black = decoder_black_period_of(s);
    const char* field = decoder_state_difference(s, black, ref, ref_black, true, tol);
    if (field == nullptr) continue;
    stop_error::context_list context = {{"coppia", frame_name(0) + "-" + frame_name(k)}, {"campo", field}};
    if (std::string_view(field) != "decoder_black") {
      context.emplace_back("sinistra", structural_text(ref, field));
      context.emplace_back("destra", structural_text(s, field));
      context.emplace_back("tolleranza", "esatta");
    } else {
      std::size_t at = 0;
      std::uint64_t largest = 0;
      for (std::size_t i = 0; i < black.values.size(); ++i) {
        const std::uint64_t u = black.values[i], v = ref_black.values[i];
        if (const std::uint64_t d = u > v ? u - v : v - u; d > largest) {
          largest = d;
          at = i;
        }
      }
      context.emplace_back("differenza_massima_adu", std::to_string(largest));
      context.emplace_back("riga_periodo", std::to_string(at / black.cols));
      context.emplace_back("colonna_periodo", std::to_string(at % black.cols));
      context.emplace_back("tolleranza_adu", std::to_string(tol));
    }
    stop(error_category::incompatible, std::string("master.mismatch.") + field,
         "frame RAW diverso dal riferimento canonico nello stato del decoder", std::move(context));
  }
}

[[noreturn]] void bad_node_states(const std::string& why, stop_error::context_list extra = {}) {
  extra.insert(extra.begin(), {{"campo", "frame_decoder_states"}, {"motivo", why}});
  stop(error_category::invalid_input, "master.decoder_state_param", "frame_decoder_states del nodo non valido",
       std::move(extra));
}

// ADR-16.b §b.7 «Descrittore»: `null` (nessun frame RAW) oppure un elemento valido per ogni ingresso frame.
std::optional<std::vector<frame_decoder_state>> frame_decoder_states_of_node(const node_spec& node) {
  const json* states = node.params().find("frame_decoder_states");
  if (states == nullptr) bad_node_states("parametro assente dal nodo");
  if (states->type() == json::kind::null) return std::nullopt;
  if (states->type() != json::kind::array) bad_node_states("non è un array né null");
  if (states->items().size() != node.inputs().size()) {
    bad_node_states("numero di elementi diverso dagli ingressi frame",
                    {{"elementi", std::to_string(states->items().size())}, {"ingressi", std::to_string(node.inputs().size())}});
  }
  std::vector<frame_decoder_state> out;
  for (std::size_t k = 0; k < states->items().size(); ++k) {
    out.push_back(frame_decoder_state_of_json(states->items()[k], frame_name(k)));
  }
  return out;
}

}  // namespace

// ---------------------------------------------------------------- interfaccia

void validate_master_recipe(const master_recipe& r) {
  const std::vector<std::size_t> order = canonical_order(r);
  require_one_frame_kind(r, order);
  validate_params(r);
  for (std::size_t k = 1; k < order.size(); ++k) {
    const master_frame_spec &a = r.frames[order[k - 1]], &b = r.frames[order[k]];
    if (a.digest.sha256 == b.digest.sha256) {
      stop(error_category::unsupported, "calib.shared_input", "due frame con lo stesso digest",
           {{"primo", std::to_string(order[k - 1])}, {"secondo", std::to_string(order[k])}, {"sha256", a.digest.sha256}});
    }
  }
  validate_raw_frames(r, order);
}

node_spec make_master_node(const master_recipe& r) {
  require_fp_environment();  // prima dei confronti sui parametri (ADR-07, D23)
  validate_master_recipe(r);
  const std::vector<std::size_t> order = canonical_order(r);
  json params = recipe_report(r);
  json frames = json::array();
  // ADR-16.b §b.7: un elemento per ingresso frame nello stesso ordine canonico; null senza frame RAW.
  json decoder_states = r.frames[order[0]].decoder ? json::array() : json::null();
  std::vector<node_input> inputs;
  for (std::size_t k = 0; k < order.size(); ++k) {
    const master_frame_spec& f = r.frames[order[k]];
    if (f.decoder) decoder_states.push(frame_decoder_state_json(*f.decoder));
    json o = json::object();
    o.set("declared_metadata", declared_metadata_json(f.declared));
    o.set("declared_fields", fields_json(f.declared_fields));
    o.set("fits_read_options", read_options_json(f.read_options));
    o.set("weight", number_or_null(f.weight));
    frames.push(std::move(o));
    inputs.push_back(node_input::from_file(frame_name(k), f.digest));
  }
  params.set("frames", std::move(frames));
  params.set("frame_decoder_states", std::move(decoder_states));
  return node_spec(master_operator_id, master_algorithm_version, master_serialization_version, std::move(params),
                   std::move(inputs), sample_type::f64, backend_name(resolve_cpu_path(*r.simd, read_cpu_capabilities())),
                   std::nullopt);
}

master_descriptor master_descriptor_from_node(const node_spec& node) {
  const auto bad = [&](const char* why) { not_master_node(node, why); };
  if (node.operator_id() != master_operator_id) bad("operatore diverso");
  const json* role = node.params().find("role");
  const json* combine = node.params().find("combine");
  const json* rejection = node.params().find("rejection");
  const auto is_text = [](const json* j, const char* v) { return j && j->type() == json::kind::string && j->as_string() == v; };
  const auto parse_combine = [&]() -> combine_method {
    for (const combine_method m : {combine_method::single, combine_method::mean, combine_method::weighted_mean,
                                   combine_method::median, combine_method::winsorized}) {
      if (is_text(combine, combine_name(m))) return m;
    }
    not_master_node(node, "combine assente o non riconosciuto");
  };
  const auto parse_rejection = [&]() -> master_rejection {
    for (const master_rejection j : {master_rejection::none, master_rejection::sigma_clip}) {
      if (is_text(rejection, rejection_name(j))) return j;
    }
    not_master_node(node, "rejection assente o non riconosciuto");
  };
  master_descriptor d;
  d.combine = parse_combine();
  d.rejection = parse_rejection();
  const std::optional<method_kind> method = method_of(*d.combine, *d.rejection);
  if (!method) bad("coppia combine e rejection fuori tabella");
  switch (*method) {
    case method_kind::single: d.estimator = variance_estimator::noise_model; break;
    case method_kind::mean: d.estimator = variance_estimator::empirical; break;
    case method_kind::weighted_mean: d.estimator = variance_estimator::empirical_weighted; break;
    case method_kind::median:
    case method_kind::winsorized: d.estimator = variance_estimator::gaussian_reference; break;
    case method_kind::sigma_clip: d.estimator = variance_estimator::frozen_selection; break;
  }
  if (is_text(role, "bias")) d.role = master_role::bias;
  else if (is_text(role, "dark")) d.role = master_role::dark;
  else if (is_text(role, "dark_flat")) d.role = master_role::dark_flat;
  else if (is_text(role, "flat")) d.role = master_role::flat;
  else bad("ruolo assente o non riconosciuto");
  if (d.role == master_role::flat) {
    d.flat = flat_state::raw;
    const json* norm = node.params().find("flat_normalization");
    if (is_text(norm, "none")) d.frame_normalization = frame_normalization_mode::none;
    else if (is_text(norm, "median_ratio")) d.frame_normalization = frame_normalization_mode::median_ratio;
    else bad("flat_normalization assente o non riconosciuta");
    // C-02.b §b.2, C-02.b-flat §f.2: con `none` solo `single` e `mean`; `single` non ammette `median_ratio`.
    const bool plain = *method == method_kind::single || *method == method_kind::mean;
    if (*d.frame_normalization == frame_normalization_mode::none ? !plain : *method == method_kind::single) {
      bad("coppia metodo e flat_normalization fuori tabella");
    }
  } else {
    const json* norm = node.params().find("flat_normalization");
    if (norm != nullptr && norm->type() != json::kind::null) bad("flat_normalization su un ruolo diverso da flat");
  }
  if (d.role != master_role::flat && d.role != master_role::bias) {
    d.additive = additive_state::bias_included;
    d.scaling = scaling_state::unscaled;
  }
  d.combine_count = static_cast<std::uint32_t>(node.inputs().size());
  d.frame_decoder_states = frame_decoder_states_of_node(node);
  return d;
}

master_result combine_master(const master_recipe& recipe, std::vector<image> frames) {
  master_plan p = make_plan(recipe);
  if (frames.size() != recipe.frames.size()) {
    stop(error_category::invalid_input, "master.param.frames", "serve un'immagine per ogni frame della ricetta",
         {{"immagini", std::to_string(frames.size())}, {"frame", std::to_string(recipe.frames.size())}});
  }
  for (std::size_t k = 0; k < p.n; ++k) {
    inspect_frame(p, k, frames[p.order[k]]);
    compare_with_reference(p, k);
    compare_optional_fields(p, k);
  }
  finish_preflight(p);
  constexpr std::uint32_t band_rows = 64;
  const std::size_t band = static_cast<std::size_t>(band_rows) * p.w;
  std::vector<std::vector<double>> vals(p.n, std::vector<double>(band));
  std::vector<std::vector<std::uint16_t>> bits(p.n, std::vector<std::uint16_t>(band));
  std::vector<const double*> vp(p.n);
  std::vector<const std::uint16_t*> bp(p.n);
  std::vector<sort_slot> sort(p.n);
  std::vector<std::uint8_t> kept(p.n);
  std::vector<std::uint64_t> rejected(p.n);
  const task_scratch scratch{sort.data(), kept.data(), rejected.data()};
  for (std::size_t f = 0; f < p.n; ++f) {
    vp[f] = vals[f].data();
    bp[f] = bits[f].data();
  }
  for (std::uint32_t y0 = 0; y0 < p.h; y0 += band_rows) {
    const std::uint32_t rows = std::min(band_rows, p.h - y0);
    for (std::size_t f = 0; f < p.n; ++f) {
      const image& im = frames[p.order[f]];
      const std::size_t first = static_cast<std::size_t>(y0) * p.w;
      for (std::size_t j = 0; j < static_cast<std::size_t>(rows) * p.w; ++j) {
        vals[f][j] = value_at(im, first + j);
        bits[f][j] = im.invalid.bits[first + j];
      }
    }
    combine_rows(p, y0, rows, vp.data(), bp.data(), scratch);
  }
  return finish_result(p);
}

std::string master_report_text(const json& report) {
  std::ostringstream out;
  out << "Rapporto del master\n";
  render(out, report, 0, "");
  return out.str();
}

std::optional<std::uint64_t> master_task_bytes(std::uint64_t frames, std::uint64_t rows, std::uint64_t width) {
  std::uint64_t pointers = 0, cells = 0, per_frame = 0, bands = 0, scratch = 0, sum = 0;
  if (__builtin_mul_overflow(frames, sizeof(double*) + sizeof(std::uint16_t*), &pointers) ||
      __builtin_mul_overflow(rows, width, &cells) ||
      __builtin_mul_overflow(cells, sizeof(double) + sizeof(std::uint16_t), &per_frame) ||
      __builtin_mul_overflow(frames, per_frame, &bands) || __builtin_mul_overflow(cells, sizeof(double), &scratch) ||
      __builtin_add_overflow(pointers, bands, &sum) || __builtin_add_overflow(sum, scratch, &sum)) {
    return std::nullopt;
  }
  return sum;
}

calibration_input_spec product_master_input(const sqlite_dag_store& store, const std::string& slot_name) {
  const std::optional<slot_record> slot = store.get_slot(slot_name);
  const slot_view view = store.read_slot(slot_name);
  if (!slot || view.state != freshness::current || !view.artifact) {
    const char* state = !slot ? "absent" : view.state == freshness::stale ? "stale" : view.state == freshness::missing ? "missing" : "current";
    stop(error_category::invalid_input, "calib.product_master_not_current", "lo slot del master non ha un artefatto corrente",
         {{"slot", slot_name}, {"stato", state}});
  }
  const std::optional<node_spec> node = store.get_node(slot->target_node_key);
  if (!node) {
    stop(error_category::graph, "dag.missing_node", "nodo del master assente dallo store",
         {{"slot", slot_name}, {"nodo", slot->target_node_key}});
  }
  calibration_input_spec in;
  in.path = store.artifact_path(view.artifact->rel_path).string();
  in.digest = content_digest{view.artifact->sha256, view.artifact->size_bytes};
  in.descriptor = master_descriptor_from_node(*node);
  in.variance.source = variance_source::plane;
  in.master_node_key = node->key();
  return in;
}

job_outcome run_master_job(sqlite_dag_store& store, const std::string& slot_name, const master_recipe& recipe,
                           memory_pool& pool, const master_job_options& options, const cancel_token& cancel,
                           const std::optional<budget_snapshot>& budget) {
  // Nodo della ricetta: lo costruisce e lo confronta con lo slot run_job_files, prima di `work`.
  std::optional<node_spec> requested;
  job_request request;
  request.node = [&] { return requested.emplace(make_master_node(recipe)); };
  request.mismatch_code = "master.node_mismatch";
  request.resources = &options.resources;
  const auto work = [&](const job_record&) -> job_files_result {
    master_plan p = make_plan(recipe, options.on_kappa);
    const node_spec& node = *requested;
    // Apertura, fase e linearità dai soli header, piano dei file: nessun campione letto.
    std::vector<fits_input> inputs;
    std::vector<fits_plan> plans;
    // Canali per riga del modello di memoria (Cr): dal motivo dell'header del frame canonico 0, lo stesso da cui il
    // preflight decide `per_channel`; il preflight lo riconferma prima di ogni task.
    bool header_cfa = false;
    inputs.reserve(p.n);
    for (std::size_t k = 0; k < p.n; ++k) {
      const master_frame_spec& f = *p.frames[k].spec;
      inputs.push_back(open_fits_input(f.path));
      const header_peek peek = peek_header(inputs.back(), f.read_options);
      if (k == 0) header_cfa = peek.cfa.has_value();
      require_phase(p.frames[k].name, p.recipe.role, peek.phase);
      require_linear(p.frames[k].name, p.recipe.role, merged_linearity(p.frames[k].name, peek.lin, f.declared_fields.lin));
      plans.push_back(plan_fits(inputs.back()));
      if (plans.back().planes != 1) {
        stop(error_category::unsupported, "calib.plane_layout", "un master accetta solo frame mono e CFA",
             {{"frame", p.frames[k].name}, {"planes", std::to_string(plans.back().planes)}});
      }
    }
    std::vector<fits_row_layout> layouts;
    for (const fits_input& in : inputs) layouts.push_back(plan_fits_rows(in));

    // Piano del job (ADR-06, estensione R2.r): un frame letto per intero alla volta, uscita, statistiche, codifica;
    // per task i blocchi di righe di tutti i frame, mai la pila. Il pianificatore ferma un piano oltre il budget o non
    // rappresentabile prima di ogni prenotazione.
    std::uint64_t frame_peak = 0;
    for (const fits_plan& fp : plans) frame_peak = std::max(frame_peak, fp.peak_bytes);
    const std::uint64_t w = plans[0].width, h = plans[0].height;
    const resource_config& config = options.resources;
    const std::size_t cpus = config.cpu_set ? config.cpu_set->size() : cpu_count();
    planner_input in{pool.budget_bytes(), cpus, plans[0].width, plans[0].height, sample_size(recipe.output_type),
                     frame_peak, output_header_bound};
    in.frame_count = p.n;
    in.master_row_channels = header_cfa ? 2 : 1;
    in.master_raw_frames = false;
    const resource_plan chosen = plan_resources(in, config);
    job_plan plan;
    plan.shared_bytes = chosen.shared_bytes;
    plan.per_task_bytes = chosen.per_task_bytes;
    plan.worker_count = chosen.worker_count;
    plan.queue_capacity = chosen.task_queue_capacity;
    plan.tile_rows = chosen.tile_rows;
    const std::uint64_t rows = chosen.tile_rows;
    const std::uint64_t tasks = chosen.task_count;
    const std::optional<std::uint64_t> tile_bytes = master_task_bytes(p.n, rows, w);
    ASTRO_ASSERT(tile_bytes.has_value());

    json observed = json::array();
    std::vector<stored_file> files;
    std::vector<std::string> warnings;
    std::optional<master_result> result;

    job_spec spec(node, tasks, [&](task_context& ctx) -> std::string {
      const std::uint32_t y0 = static_cast<std::uint32_t>(ctx.index * rows);
      const std::uint32_t cnt = static_cast<std::uint32_t>(std::min<std::uint64_t>(h, y0 + rows) - y0);
      pool_buffer buffer(ctx.memory, static_cast<std::size_t>(plan.per_task_bytes));
      auto** vp = reinterpret_cast<const double**>(buffer.data());
      auto** bp = reinterpret_cast<const std::uint16_t**>(vp + p.n);
      auto* vals = reinterpret_cast<double*>(bp + p.n);
      auto* bits = reinterpret_cast<std::uint16_t*>(vals + p.n * rows * w);
      auto* raw = reinterpret_cast<std::byte*>(bits + p.n * rows * w);
      // Dopo puntatori e blocchi, nei byte per task del modello (ADR-06 R2.r): ordinamento (16·n), indicatori (q(n)),
      // parziali di rejected_samples (8·n, dentro il termine dei contatori).
      const std::size_t sort_at = (static_cast<std::size_t>(*tile_bytes) + alignof(sort_slot) - 1) / alignof(sort_slot) *
                                  alignof(sort_slot);
      const std::size_t kept_at = sort_at + p.n * sizeof(sort_slot);
      const std::size_t rejected_at = kept_at + 8 * ((p.n + 7) / 8);
      static_assert(alignof(sort_slot) % alignof(std::uint64_t) == 0 &&
                    sizeof(sort_slot) % alignof(std::uint64_t) == 0);
      ASTRO_ASSERT(rejected_at + p.n * sizeof(std::uint64_t) <= buffer.size());
      const task_scratch scratch{reinterpret_cast<sort_slot*>(buffer.data() + sort_at),
                                 reinterpret_cast<std::uint8_t*>(buffer.data() + kept_at),
                                 reinterpret_cast<std::uint64_t*>(buffer.data() + rejected_at)};
      for (std::size_t f = 0; f < p.n; ++f) {
        ctx.check_cancel();
        double* v = vals + f * rows * w;
        std::uint16_t* b = bits + f * rows * w;
        read_fits_rows(inputs[f], layouts[f], y0, cnt, raw, v, b);
        vp[f] = v;
        bp[f] = b;
      }
      combine_rows(p, y0, cnt, vp, bp, scratch);
      return rows_digest(p.out, y0, y0 + cnt);
    });

    spec.prepare = [&](job_context& jc) {
      const std::uint64_t budget_bytes = jc.shared.bytes();
      for (std::size_t k = 0; k < p.n; ++k) {
        jc.check_cancel();
        const master_frame_spec& f = *p.frames[k].spec;
        image im;
        try {
          im = decode_fits(read_verified_input(inputs[k], f.path, f.digest, budget_bytes, &plans[k]), budget_bytes,
                           f.read_options);
        } catch (stop_error& e) {
          e.add_context("frame", p.frames[k].name);
          throw;
        }
        inspect_frame(p, k, im);
        compare_with_reference(p, k);
        compare_optional_fields(p, k);
      }
      finish_preflight(p);
      // Invariante: peek e decode leggono gli stessi byte con le stesse read_options e la stessa interpretazione
      // dell'header; un file modificato fra i due lo ferma già read_verified_input (integrity, input.modified).
      ASTRO_ASSERT(p.per_channel == header_cfa);
      observed = observed_inputs(p);
      if (options.hook) options.hook("frames.read");
    };

    spec.finalize = [&](job_context& jc) {
      // I blocchi letti dai task appartengono al file verificato: file aperto invariato e digest al percorso.
      pool_buffer chunk(jc.shared, static_cast<std::size_t>(master_digest_chunk_bytes));
      for (std::size_t k = 0; k < p.n; ++k) {
        try {
          jc.check_cancel();
          reverify_input(p.frames[k].spec->path, fits_input_format(), &inputs[k], p.frames[k].spec->digest,
                         {chunk.data(), chunk.size()}, [&jc] { jc.check_cancel(); });
        } catch (stop_error& e) {
          e.add_context("frame", p.frames[k].name);
          e.add_context("fase", "riverifica prima della pubblicazione");
          throw;
        }
      }
      warnings = p.warnings;
      result.emplace(finish_result(p));
      const std::vector<std::uint8_t> fits = encode_fits(result->output);
      const content_digest fits_digest = digest_of(fits);
      json image_out = json::object();
      image_out.set("sha256", json::string(fits_digest.sha256));
      image_out.set("size_bytes", json::uinteger(fits_digest.size_bytes));
      json outputs = json::object();
      outputs.set("master_image", std::move(image_out));
      result->report.set("outputs", std::move(outputs));
      const std::string report_json = canonical_json(result->report);
      const std::string report_text = master_report_text(result->report);
      const auto span = [](const std::string& s) {
        return std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
      };
      files = store.write_files(
          {artifact_content{fits, "fits", "image", std::nullopt, master_image_role},
           artifact_content{span(report_json), "json", "master_report", std::nullopt, master_report_role},
           artifact_content{span(report_text), "txt", "master_report_text", std::nullopt, master_report_text_role}});
    };

    if (options.on_job_plan) options.on_job_plan(plan);
    job_scheduler scheduler(pool, system_affinity, scheduler_tuning_of(config));
    scheduler.run(plan, spec, cancel);
    json details = json::object();
    details.set("observed_inputs", std::move(observed));
    job_files_result out{std::move(files), std::move(warnings), std::move(details)};
    out.plan = std::make_shared<const resource_plan>(chosen);
    return out;
  };
  return run_job_files(store, slot_name, work, budget, &cancel, request);
}

}  // namespace astro
