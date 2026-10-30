// Percorso cpu_scalar (ADR-07): compilato con -fno-tree-vectorize -fno-tree-slp-vectorize (src/core/CMakeLists.txt).

#include <cmath>
#include <limits>
#include <string>

#include "cpu_kernels.hpp"

namespace astro::kernels {

namespace {

using namespace calib_detail;

template <class T>
void rows_kernel(const prepared_calibration::impl& p, const kernel_io& io, std::uint32_t y0, std::uint32_t y1,
                 double* qrow, double* varq, std::uint16_t* qbits) {
  const std::uint32_t w = p.w;
  T* out = static_cast<T*>(io.out);
  T* outv = static_cast<T*>(io.out_variance);
  std::uint16_t* out_mask = io.out_mask;
  const input_view &L = *io.light, &D = *io.dark, &F = *io.flat, &DF = *io.dark_flat;
  constexpr T nan = std::numeric_limits<T>::quiet_NaN();
  for (std::uint32_t y = y0; y < y1; ++y) {
    for (std::uint32_t x = 0; x < w; ++x) {
      const std::size_t i = static_cast<std::size_t>(y - io.row0) * w + x;
      qbits[x] = 0;
      qrow[x] = std::numeric_limits<double>::quiet_NaN();
      varq[x] = std::numeric_limits<double>::quiet_NaN();
      const bool f_saturated = F.has_saturation && F.value(i) >= F.saturation;
      if ((F.mask[i] | DF.mask[i]) != 0 || f_saturated) continue;
      const double f = F.value(i), df = DF.value(i);
      const double n = p.norm[p.channel_of(x, y)];
      const double fc = f - df;
      const double q = fc / n;
      qrow[x] = q;
      if (!div_ok(fc, q) && std::isfinite(q)) {
        qbits[x] = invalid_reason::result_nonfinite;  // Q zero o subnormale da Fc non nullo
      } else if (!std::isfinite(q)) {
        qbits[x] = invalid_reason::q_nonfinite;
      } else if (q <= 0) {
        qbits[x] = invalid_reason::q_nonpositive;
      } else {
        const double vf = F.variance(i, f), vdf = DF.variance(i, df);
        const double vfc = vf + vdf;
        const double vq = vfc / p.norm_sq[p.channel_of(x, y)];
        varq[x] = vq;
        if (std::isnan(vf) || std::isnan(vdf) || !add_ok(vf, vdf, vfc) || !div_ok(vfc, vq)) {
          qbits[x] = invalid_reason::result_nonfinite;  // passo del modello di varianza non rappresentabile
          continue;
        }
        if (q < p.q_min) qbits[x] |= invalid_reason::q_below_min;
        if (std::sqrt(vq) / q > p.q_rel_sigma_max) qbits[x] |= invalid_reason::q_unreliable;
      }
    }
    for (std::uint32_t x = 0; x < w; ++x) {
      const std::size_t i = static_cast<std::size_t>(y - io.row0) * w + x;
      const std::uint16_t master_bits = static_cast<std::uint16_t>(D.mask[i] | F.mask[i] | DF.mask[i]);
      std::uint16_t bits = static_cast<std::uint16_t>(L.mask[i] | master_bits | qbits[x]);
      if (master_bits != 0) bits |= invalid_reason::master_invalid;
      if (L.has_saturation && L.value(i) >= L.saturation) bits |= invalid_reason::saturated;
      if (F.has_saturation && F.value(i) >= F.saturation) bits |= invalid_reason::saturated;
      if (bits == 0) {
        const double l = L.value(i), d = D.value(i);
        const double lc = l - d;
        const double q = qrow[x];
        const double c = lc / q;
        const double vl = L.variance(i, l), vd = D.variance(i, d);
        const double vlc = vl + vd;
        const double q2 = q * q;
        const double q4 = q2 * q2;
        const double lc2 = lc * lc;
        const double term_q = lc2 * varq[x];
        const double t1 = vlc / q2;
        const double t2 = term_q / q4;
        const double var_c = t1 + t2;
        // Ogni prodotto, quoziente e somma deve essere finito e, con operandi non nulli, normale (ne' zero ne'
        // subnormale per underflow, ne' infinito): altrimenti il pixel e' result_nonfinite.
        const bool intermediates_ok = !std::isnan(vl) && !std::isnan(vd) && add_ok(vl, vd, vlc) && is_finite_ok(vlc) &&
                                      div_ok(lc, c) && mul_ok(q, q, q2) && mul_ok(q2, q2, q4) && q2 > 0 && q4 > 0 &&
                                      mul_ok(lc, lc, lc2) && mul_ok(lc2, varq[x], term_q) && div_ok(vlc, t1) &&
                                      div_ok(term_q, t2) && add_ok(t1, t2, var_c);
        T cs{}, vs{};
        if (intermediates_ok && convert_finite<T>(c, cs) && convert_finite<T>(var_c, vs)) {
          out[i] = cs;
          outv[i] = vs;
          continue;
        }
        bits = invalid_reason::result_nonfinite;
      }
      out[i] = nan;
      outv[i] = nan;
      out_mask[i] = bits;
    }
  }
}

template <class T>
void band_kernel(const master_band& b) {
  const std::size_t n = b.n;
  const double dn = static_cast<double>(n), dn1 = static_cast<double>(n - 1);
  T* out = static_cast<T*>(b.out);
  T* outv = static_cast<T*>(b.outv);
  constexpr T nan = std::numeric_limits<T>::quiet_NaN();
  for (std::uint32_t r = 0; r < b.rows; ++r) {
    for (std::uint32_t x = 0; x < b.w; ++x) {
      const std::size_t j = static_cast<std::size_t>(r) * b.w + x;
      const std::size_t i = static_cast<std::size_t>(b.y0 + r) * b.w + x;
      std::uint16_t m = 0;
      bool saturated = false;
      for (std::size_t f = 0; f < n; ++f) {
        m = static_cast<std::uint16_t>(m | b.bits[f][j]);
        if (b.has_sat && b.vals[f][j] >= b.sat) saturated = true;
      }
      if (saturated) m = static_cast<std::uint16_t>(m | invalid_reason::saturated);
      if (m == 0) {
        double s = b.vals[0][j];
        for (std::size_t f = 1; f < n; ++f) s += b.vals[f][j];
        const double mean = s / dn;
        bool ok = std::isfinite(s) && div_ok(s, mean);
        double ss = 0;
        for (std::size_t f = 0; ok && f < n; ++f) {
          const double d = b.vals[f][j] - mean;
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
        double var = 0;
        if (ok) {
          const double s2 = ss / dn1;
          var = s2 / dn;
          ok = div_ok(ss, s2) && div_ok(s2, var);
        }
        T ms{}, vs{};
        if (ok && convert_finite<T>(mean, ms) && convert_finite<T>(var, vs)) {
          out[i] = ms;
          outv[i] = vs;
          b.mask[i] = 0;
          if (b.flat) {
            const double a = mean - b.offset;
            const double ac = a > 0 ? a : 0.0;
            // Stessa regola di rappresentabilita' dei pixel (§6.3): prodotti e quozienti di operandi non nulli devono
            // restare normali; altrimenti la statistica non e' rappresentabile e il job si ferma.
            const double e2 = b.eps * b.eps;
            const double e2a = e2 * ac;
            const double t = e2a * ac;
            const double lbe = t / dn1;
            if (!mul_ok(b.eps, b.eps, e2) || !mul_ok(e2, ac, e2a) || !mul_ok(e2a, ac, t) || !div_ok(t, lbe)) {
              throw stop_error(error_category::numeric, "master.report_nonfinite", "level_bias_estimate non rappresentabile",
                               {{"x", std::to_string(x)}, {"y", std::to_string(b.y0 + r)}});
            }
            b.lbe[i] = lbe;
            b.nonpositive[i] = a <= 0 ? 1 : 0;
            if (var > 0) {
              const double q = lbe / var;
              if (!div_ok(lbe, q)) {
                throw stop_error(error_category::numeric, "master.report_nonfinite",
                                 "rapporto level_bias_estimate / piano non finito",
                                 {{"x", std::to_string(x)}, {"y", std::to_string(b.y0 + r)}});
              }
              b.ratio[i] = q;
            }
          }
          continue;
        }
        m = invalid_reason::result_nonfinite;
      }
      out[i] = nan;
      outv[i] = nan;
      b.mask[i] = m;
    }
  }
}

}  // namespace

void calibrate_rows_scalar(const prepared_calibration::impl& p, const kernel_io& io, std::uint32_t y0,
                           std::uint32_t y1, void* scratch) {
  cpu_detail::count_kernel_call(cpu_path::scalar);
  auto* qrow = static_cast<double*>(scratch);
  double* varq = qrow + p.w;
  auto* qbits = reinterpret_cast<std::uint16_t*>(varq + p.w);
  if (p.out.type == sample_type::f32) rows_kernel<float>(p, io, y0, y1, qrow, varq, qbits);
  else rows_kernel<double>(p, io, y0, y1, qrow, varq, qbits);
}

void master_band_scalar(const master_band& b) {
  cpu_detail::count_kernel_call(cpu_path::scalar);
  if (b.f32_out) band_kernel<float>(b);
  else band_kernel<double>(b);
}

double sum_scalar(const double* x, std::size_t n) {
  cpu_detail::count_kernel_call(cpu_path::scalar);
  double s = x[0];
  for (std::size_t i = 1; i < n; ++i) s += x[i];
  return s;
}

}  // namespace astro::kernels
