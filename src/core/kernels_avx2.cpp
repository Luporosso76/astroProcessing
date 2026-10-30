// Percorso cpu_avx2 (ADR-07): 4 pixel per vettore in f64. Ogni funzione che usa i registri ymm ha target("avx2"); il
// resto dell'unita' resta nell'ISA di base, cosi' le funzioni inline condivise non portano istruzioni AVX altrove.
// Stesse operazioni IEEE, nello stesso ordine per pixel, del kernel scalare: niente FMA (target senza "fma",
// -ffp-contract=off) e nessuna istruzione approssimata.

#include <immintrin.h>

#include <cfloat>
#include <cmath>
#include <limits>
#include <string>

#include "cpu_kernels.hpp"

#define ASTRO_AVX2 __attribute__((target("avx2")))

namespace astro::kernels {

namespace {

using calib_detail::input_view;
using calib_detail::kernel_io;

// ---------------------------------------------------------------- predicati per corsia (maschere a 64 bit)

ASTRO_AVX2 inline __m256d all_ones() { return _mm256_castsi256_pd(_mm256_set1_epi64x(-1)); }
ASTRO_AVX2 inline __m256d mask_not(__m256d m) { return _mm256_xor_pd(m, all_ones()); }
ASTRO_AVX2 inline __m256d mask_and(__m256d a, __m256d b) { return _mm256_and_pd(a, b); }
ASTRO_AVX2 inline __m256d mask_or(__m256d a, __m256d b) { return _mm256_or_pd(a, b); }
// m ? a : b per corsia.
ASTRO_AVX2 inline __m256d select(__m256d m, __m256d a, __m256d b) { return _mm256_blendv_pd(b, a, m); }

ASTRO_AVX2 inline __m256d abs4(__m256d x) { return _mm256_andnot_pd(_mm256_set1_pd(-0.0), x); }
// fpclassify == FP_NORMAL: DBL_MIN <= |r| <= DBL_MAX (confronti ordinati: NaN falso).
ASTRO_AVX2 inline __m256d is_normal4(__m256d r) {
  const __m256d a = abs4(r);
  return mask_and(_mm256_cmp_pd(a, _mm256_set1_pd(DBL_MIN), _CMP_GE_OQ), _mm256_cmp_pd(a, _mm256_set1_pd(DBL_MAX), _CMP_LE_OQ));
}
ASTRO_AVX2 inline __m256d is_finite4(__m256d r) { return _mm256_cmp_pd(abs4(r), _mm256_set1_pd(DBL_MAX), _CMP_LE_OQ); }
ASTRO_AVX2 inline __m256d is_zero4(__m256d r) { return _mm256_cmp_pd(r, _mm256_setzero_pd(), _CMP_EQ_OQ); }
ASTRO_AVX2 inline __m256d is_nan4(__m256d r) { return _mm256_cmp_pd(r, r, _CMP_UNORD_Q); }

// mul_ok, div_ok, add_ok di calibration_internal.hpp, per corsia.
ASTRO_AVX2 inline __m256d mul_ok4(__m256d a, __m256d b, __m256d r) {
  return select(mask_or(is_zero4(a), is_zero4(b)), is_zero4(r), is_normal4(r));
}
ASTRO_AVX2 inline __m256d div_ok4(__m256d a, __m256d r) { return select(is_zero4(a), is_zero4(r), is_normal4(r)); }
ASTRO_AVX2 inline __m256d add_ok4(__m256d a, __m256d b, __m256d r) {
  return select(mask_and(is_zero4(a), is_zero4(b)), is_zero4(r), is_normal4(r));
}

// ---------------------------------------------------------------- bit della maschera (uint16 in corsie a 64 bit)

ASTRO_AVX2 inline __m256i load_bits(const std::uint16_t* m, std::size_t i, unsigned cnt) {
  std::uint16_t t[4] = {0, 0, 0, 0};
  for (unsigned k = 0; k < cnt; ++k) t[k] = m[i + k];
  return _mm256_cvtepu16_epi64(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(t)));
}
ASTRO_AVX2 inline __m256i bit_if(__m256d m, std::uint16_t bit) {
  return _mm256_and_si256(_mm256_castpd_si256(m), _mm256_set1_epi64x(bit));
}
ASTRO_AVX2 inline __m256d bits_zero(__m256i b) { return _mm256_castsi256_pd(_mm256_cmpeq_epi64(b, _mm256_setzero_si256())); }
ASTRO_AVX2 inline __m256i select_bits(__m256d m, __m256i a, __m256i b) {
  return _mm256_castpd_si256(select(m, _mm256_castsi256_pd(a), _mm256_castsi256_pd(b)));
}

// ---------------------------------------------------------------- campioni (f32 convertiti in f64 in modo esatto)

ASTRO_AVX2 inline __m256d load_values(const void* base, bool f32, std::size_t i, unsigned cnt) {
  if (cnt == 4) {
    if (f32) return _mm256_cvtps_pd(_mm_loadu_ps(static_cast<const float*>(base) + i));
    return _mm256_loadu_pd(static_cast<const double*>(base) + i);
  }
  double t[4] = {0, 0, 0, 0};
  for (unsigned k = 0; k < cnt; ++k) {
    t[k] = f32 ? static_cast<double>(static_cast<const float*>(base)[i + k]) : static_cast<const double*>(base)[i + k];
  }
  return _mm256_loadu_pd(t);
}

ASTRO_AVX2 inline __m256d sample4(const input_view& v, std::size_t i, unsigned cnt) {
  return load_values(v.samples, v.f32, i, cnt);
}

// input_view::variance per corsia: piano, oppure modello di rumore con NaN su un passo non rappresentabile.
ASTRO_AVX2 inline __m256d variance4(const input_view& v, std::size_t i, unsigned cnt, __m256d x) {
  if (v.plane) return load_values(v.variance_plane, v.f32, i, cnt);
  const __m256d zero = _mm256_setzero_pd();
  const __m256d above = _mm256_sub_pd(x, _mm256_set1_pd(v.offset));
  const __m256d a = select(_mm256_cmp_pd(above, zero, _CMP_GT_OQ), above, zero);
  const __m256d poisson = _mm256_div_pd(a, _mm256_set1_pd(v.gain));
  const __m256d rn = _mm256_set1_pd(v.read_noise_sq);
  const __m256d sum = _mm256_add_pd(rn, poisson);
  const __m256d var = _mm256_div_pd(sum, _mm256_set1_pd(v.n));
  const __m256d ok = mask_and(mask_and(div_ok4(a, poisson), add_ok4(rn, poisson, sum)),
                              mask_and(div_ok4(sum, var), is_finite4(var)));
  return select(ok, var, _mm256_set1_pd(std::numeric_limits<double>::quiet_NaN()));
}

// ---------------------------------------------------------------- conversione al tipo di uscita (convert_finite)

template <class T>
struct converted;

template <>
struct converted<double> {
  double v[4];
  __m256d ok;
};

template <>
struct converted<float> {
  float v[4];
  __m256d ok;
};

template <class T>
ASTRO_AVX2 inline converted<T> convert4(__m256d x) {
  converted<T> r;
  const __m256d finite = is_finite4(x);
  if constexpr (std::is_same_v<T, double>) {
    _mm256_storeu_pd(r.v, x);
    r.ok = mask_and(finite, mask_or(is_zero4(x), is_normal4(x)));
  } else {
    const __m256d in_range = _mm256_cmp_pd(abs4(x), _mm256_set1_pd(static_cast<double>(FLT_MAX)), _CMP_LE_OQ);
    const __m128 f = _mm256_cvtpd_ps(x);
    _mm_storeu_ps(r.v, f);
    const __m128 af = _mm_andnot_ps(_mm_set1_ps(-0.0f), f);
    const __m128 normal = _mm_and_ps(_mm_cmp_ps(af, _mm_set1_ps(FLT_MIN), _CMP_GE_OQ), _mm_cmp_ps(af, _mm_set1_ps(FLT_MAX), _CMP_LE_OQ));
    const __m256d normal64 = _mm256_castsi256_pd(_mm256_cvtepi32_epi64(_mm_castps_si128(normal)));
    r.ok = mask_and(mask_and(finite, in_range), mask_or(is_zero4(x), normal64));
  }
  return r;
}

// ---------------------------------------------------------------- calibrazione

template <class T>
ASTRO_AVX2 void rows_kernel(const prepared_calibration::impl& p, const kernel_io& io, std::uint32_t y0, std::uint32_t y1) {
  using namespace invalid_reason;
  const std::uint32_t w = p.w;
  T* out = static_cast<T*>(io.out);
  T* outv = static_cast<T*>(io.out_variance);
  std::uint16_t* out_mask = io.out_mask;
  const input_view &L = *io.light, &D = *io.dark, &F = *io.flat, &DF = *io.dark_flat;
  constexpr T nan = std::numeric_limits<T>::quiet_NaN();
  const __m256d zero = _mm256_setzero_pd();
  const __m256d q_min = _mm256_set1_pd(p.q_min), q_rel_max = _mm256_set1_pd(p.q_rel_sigma_max);
  const __m256d f_sat = _mm256_set1_pd(F.saturation), l_sat = _mm256_set1_pd(L.saturation);
  for (std::uint32_t y = y0; y < y1; ++y) {
    // Indice a 64 bit: con w vicino a 2^32 l'incremento di 4 non torna a zero dopo l'ultima coda.
    for (std::uint64_t x0 = 0; x0 < w; x0 += 4) {
      const unsigned cnt = w - x0 < 4 ? static_cast<unsigned>(w - x0) : 4;
      const std::size_t i = static_cast<std::size_t>(y - io.row0) * w + x0;
      const __m256i lm = load_bits(L.mask, i, cnt), dm = load_bits(D.mask, i, cnt);
      const __m256i fm = load_bits(F.mask, i, cnt), dfm = load_bits(DF.mask, i, cnt);
      const __m256d f = sample4(F, i, cnt), df = sample4(DF, i, cnt);
      const __m256d f_saturated = F.has_saturation ? _mm256_cmp_pd(f, f_sat, _CMP_GE_OQ) : zero;
      const __m256d skip = mask_or(mask_not(bits_zero(_mm256_or_si256(fm, dfm))), f_saturated);
      double nl[4], nsl[4];
      for (unsigned k = 0; k < 4; ++k) {
        const std::size_t c = p.channel_of(static_cast<std::uint32_t>(x0 + k), y);  // conta solo la parita
        nl[k] = p.norm[c];
        nsl[k] = p.norm_sq[c];
      }
      const __m256d n = _mm256_loadu_pd(nl), n_sq = _mm256_loadu_pd(nsl);

      // Q, Var(Q) e bit di Q.
      const __m256d fc = _mm256_sub_pd(f, df);
      const __m256d q = _mm256_div_pd(fc, n);
      const __m256d q_finite = is_finite4(q);
      const __m256d c1 = mask_and(mask_not(div_ok4(fc, q)), q_finite);
      const __m256d c2 = mask_not(q_finite);
      const __m256d c3 = mask_and(mask_and(mask_not(c1), q_finite), _mm256_cmp_pd(q, zero, _CMP_LE_OQ));
      const __m256d branch = mask_not(mask_or(mask_or(c1, c2), c3));
      const __m256d vf = variance4(F, i, cnt, f), vdf = variance4(DF, i, cnt, df);
      const __m256d vfc = _mm256_add_pd(vf, vdf);
      const __m256d vq = _mm256_div_pd(vfc, n_sq);
      const __m256d model_bad = mask_or(mask_or(is_nan4(vf), is_nan4(vdf)),
                                        mask_or(mask_not(add_ok4(vf, vdf, vfc)), mask_not(div_ok4(vfc, vq))));
      const __m256d c4 = mask_and(branch, model_bad);
      const __m256d thresholds = mask_and(branch, mask_not(model_bad));
      const __m256d below = mask_and(thresholds, _mm256_cmp_pd(q, q_min, _CMP_LT_OQ));
      const __m256d unreliable = mask_and(thresholds, _mm256_cmp_pd(_mm256_div_pd(_mm256_sqrt_pd(vq), q), q_rel_max, _CMP_GT_OQ));
      __m256i qb = _mm256_or_si256(_mm256_or_si256(bit_if(c1, result_nonfinite), bit_if(mask_and(mask_not(c1), c2), q_nonfinite)),
                                   _mm256_or_si256(bit_if(c3, q_nonpositive), bit_if(c4, result_nonfinite)));
      qb = _mm256_or_si256(qb, _mm256_or_si256(bit_if(below, q_below_min), bit_if(unreliable, q_unreliable)));
      qb = select_bits(skip, _mm256_setzero_si256(), qb);

      // Bit del pixel.
      const __m256i master_bits = _mm256_or_si256(dm, _mm256_or_si256(fm, dfm));
      __m256i bits = _mm256_or_si256(lm, _mm256_or_si256(master_bits, qb));
      bits = _mm256_or_si256(bits, bit_if(mask_not(bits_zero(master_bits)), master_invalid));
      const __m256d l = sample4(L, i, cnt), d = sample4(D, i, cnt);
      if (L.has_saturation) bits = _mm256_or_si256(bits, bit_if(_mm256_cmp_pd(l, l_sat, _CMP_GE_OQ), saturated));
      if (F.has_saturation) bits = _mm256_or_si256(bits, bit_if(f_saturated, saturated));
      const __m256d clean = bits_zero(bits);

      // C e Var(C), con la regola di rappresentabilita' su ogni passo.
      const __m256d lc = _mm256_sub_pd(l, d);
      const __m256d c = _mm256_div_pd(lc, q);
      const __m256d vl = variance4(L, i, cnt, l), vd = variance4(D, i, cnt, d);
      const __m256d vlc = _mm256_add_pd(vl, vd);
      const __m256d q2 = _mm256_mul_pd(q, q);
      const __m256d q4 = _mm256_mul_pd(q2, q2);
      const __m256d lc2 = _mm256_mul_pd(lc, lc);
      const __m256d term_q = _mm256_mul_pd(lc2, vq);
      const __m256d t1 = _mm256_div_pd(vlc, q2);
      const __m256d t2 = _mm256_div_pd(term_q, q4);
      const __m256d var_c = _mm256_add_pd(t1, t2);
      __m256d ok = mask_and(mask_and(mask_not(is_nan4(vl)), mask_not(is_nan4(vd))), mask_and(add_ok4(vl, vd, vlc), is_finite4(vlc)));
      ok = mask_and(ok, mask_and(div_ok4(lc, c), mask_and(mul_ok4(q, q, q2), mul_ok4(q2, q2, q4))));
      ok = mask_and(ok, mask_and(_mm256_cmp_pd(q2, zero, _CMP_GT_OQ), _mm256_cmp_pd(q4, zero, _CMP_GT_OQ)));
      ok = mask_and(ok, mask_and(mul_ok4(lc, lc, lc2), mul_ok4(lc2, vq, term_q)));
      ok = mask_and(ok, mask_and(mask_and(div_ok4(vlc, t1), div_ok4(term_q, t2)), add_ok4(t1, t2, var_c)));
      const converted<T> cs = convert4<T>(c), vs = convert4<T>(var_c);
      const __m256d good = mask_and(mask_and(clean, ok), mask_and(cs.ok, vs.ok));
      bits = select_bits(clean, _mm256_set1_epi64x(result_nonfinite), bits);

      const int good_lanes = _mm256_movemask_pd(good);
      alignas(32) std::uint64_t b[4];
      _mm256_store_si256(reinterpret_cast<__m256i*>(b), bits);
      for (unsigned k = 0; k < cnt; ++k) {
        if (good_lanes & (1 << k)) {
          out[i + k] = cs.v[k];
          outv[i + k] = vs.v[k];
        } else {
          out[i + k] = nan;
          outv[i + k] = nan;
          out_mask[i + k] = static_cast<std::uint16_t>(b[k]);
        }
      }
    }
  }
}

// ---------------------------------------------------------------- master

template <class T>
ASTRO_AVX2 void band_kernel(const master_band& b) {
  const std::size_t n = b.n;
  const __m256d dn = _mm256_set1_pd(static_cast<double>(n)), dn1 = _mm256_set1_pd(static_cast<double>(n - 1));
  const __m256d zero = _mm256_setzero_pd(), sat = _mm256_set1_pd(b.sat);
  const __m256d eps = _mm256_set1_pd(b.eps), offset = _mm256_set1_pd(b.offset);
  T* out = static_cast<T*>(b.out);
  T* outv = static_cast<T*>(b.outv);
  constexpr T nan = std::numeric_limits<T>::quiet_NaN();
  const std::size_t total = static_cast<std::size_t>(b.rows) * b.w;
  const std::size_t first = static_cast<std::size_t>(b.y0) * b.w;
  for (std::size_t j = 0; j < total; j += 4) {
    const unsigned cnt = total - j < 4 ? static_cast<unsigned>(total - j) : 4;
    __m256i m = _mm256_setzero_si256();
    __m256d saturated = zero;
    for (std::size_t f = 0; f < n; ++f) {
      m = _mm256_or_si256(m, load_bits(b.bits[f], j, cnt));
      if (b.has_sat) saturated = mask_or(saturated, _mm256_cmp_pd(load_values(b.vals[f], false, j, cnt), sat, _CMP_GE_OQ));
    }
    m = _mm256_or_si256(m, bit_if(saturated, invalid_reason::saturated));
    const __m256d clean = bits_zero(m);

    // Somma nell'ordine canonico dei frame, accumulatore inizializzato col primo termine.
    __m256d s = load_values(b.vals[0], false, j, cnt);
    for (std::size_t f = 1; f < n; ++f) s = _mm256_add_pd(s, load_values(b.vals[f], false, j, cnt));
    const __m256d mean = _mm256_div_pd(s, dn);
    __m256d ok = mask_and(is_finite4(s), div_ok4(s, mean));
    __m256d ss = zero;
    for (std::size_t f = 0; f < n; ++f) {
      const __m256d d = _mm256_sub_pd(load_values(b.vals[f], false, j, cnt), mean);
      const __m256d sq = _mm256_mul_pd(d, d);
      ok = mask_and(ok, mask_and(is_finite4(d), mul_ok4(d, d, sq)));
      if (f == 0) {
        ss = sq;
      } else {
        const __m256d t = _mm256_add_pd(ss, sq);
        ok = mask_and(ok, add_ok4(ss, sq, t));
        ss = t;
      }
    }
    const __m256d s2 = _mm256_div_pd(ss, dn1);
    const __m256d var = _mm256_div_pd(s2, dn);
    ok = mask_and(ok, mask_and(div_ok4(ss, s2), div_ok4(s2, var)));
    const converted<T> ms = convert4<T>(mean), vs = convert4<T>(var);
    const int good_lanes = _mm256_movemask_pd(mask_and(mask_and(clean, ok), mask_and(ms.ok, vs.ok)));
    const int clean_lanes = _mm256_movemask_pd(clean);

    // Statistiche del flat (§6.2), stessa regola di rappresentabilita'.
    alignas(32) double a[4], lbe[4], ratio[4], v[4];
    int lbe_ok = 0, ratio_ok = 0;
    if (b.flat) {
      const __m256d av = _mm256_sub_pd(mean, offset);
      const __m256d ac = select(_mm256_cmp_pd(av, zero, _CMP_GT_OQ), av, zero);
      const __m256d e2 = _mm256_mul_pd(eps, eps);
      const __m256d e2a = _mm256_mul_pd(e2, ac);
      const __m256d t = _mm256_mul_pd(e2a, ac);
      const __m256d l = _mm256_div_pd(t, dn1);
      const __m256d q = _mm256_div_pd(l, var);
      lbe_ok = _mm256_movemask_pd(mask_and(mask_and(mul_ok4(eps, eps, e2), mul_ok4(e2, ac, e2a)),
                                           mask_and(mul_ok4(e2a, ac, t), div_ok4(t, l))));
      ratio_ok = _mm256_movemask_pd(div_ok4(l, q));
      _mm256_store_pd(a, av);
      _mm256_store_pd(lbe, l);
      _mm256_store_pd(ratio, q);
      _mm256_store_pd(v, var);
    }
    alignas(32) std::uint64_t mb[4];
    _mm256_store_si256(reinterpret_cast<__m256i*>(mb), m);
    for (unsigned k = 0; k < cnt; ++k) {
      const std::size_t i = first + j + k;
      if (good_lanes & (1 << k)) {
        out[i] = ms.v[k];
        outv[i] = vs.v[k];
        b.mask[i] = 0;
        if (b.flat) {
          const std::size_t x = (j + k) % b.w, y = b.y0 + (j + k) / b.w;
          if (!(lbe_ok & (1 << k))) {
            throw stop_error(error_category::numeric, "master.report_nonfinite", "level_bias_estimate non rappresentabile",
                             {{"x", std::to_string(x)}, {"y", std::to_string(y)}});
          }
          b.lbe[i] = lbe[k];
          b.nonpositive[i] = a[k] <= 0 ? 1 : 0;
          if (v[k] > 0) {
            if (!(ratio_ok & (1 << k))) {
              throw stop_error(error_category::numeric, "master.report_nonfinite",
                               "rapporto level_bias_estimate / piano non finito",
                               {{"x", std::to_string(x)}, {"y", std::to_string(y)}});
            }
            b.ratio[i] = ratio[k];
          }
        }
        continue;
      }
      out[i] = nan;
      outv[i] = nan;
      b.mask[i] = (clean_lanes & (1 << k)) ? invalid_reason::result_nonfinite : static_cast<std::uint16_t>(mb[k]);
    }
  }
}

// 4 corsie, termine i nella corsia i mod 4, ciascuna inizializzata col primo termine; riduzione (c0 + c1) + (c2 + c3).
// Con meno di 4 termini le corsie vuote non entrano: la somma e' quella nell'ordine degli indici.
ASTRO_AVX2 double lanes_sum(const double* x, std::size_t n) {
  if (n < 4) {
    double s = x[0];
    for (std::size_t i = 1; i < n; ++i) s += x[i];
    return s;
  }
  __m256d acc = _mm256_loadu_pd(x);
  std::size_t i = 4;
  for (; i + 4 <= n; i += 4) acc = _mm256_add_pd(acc, _mm256_loadu_pd(x + i));
  const std::size_t r = n - i;
  if (r != 0) {
    const __m256i lanes = _mm256_set_epi64x(0, r > 2 ? -1 : 0, r > 1 ? -1 : 0, -1);
    const __m256d tail = _mm256_maskload_pd(x + i, lanes);
    acc = _mm256_blendv_pd(acc, _mm256_add_pd(acc, tail), _mm256_castsi256_pd(lanes));
  }
  const __m128d pairs = _mm_hadd_pd(_mm256_castpd256_pd128(acc), _mm256_extractf128_pd(acc, 1));
  return _mm_cvtsd_f64(_mm_add_sd(pairs, _mm_unpackhi_pd(pairs, pairs)));
}

}  // namespace

// Punti d'ingresso senza target: chiamano le funzioni target("avx2") con soli puntatori e riferimenti. Ogni punto
// d'ingresso conta la propria chiamata (cpu_test::kernel_calls): il contatore dice quale kernel ha girato.
void calibrate_rows_avx2(const prepared_calibration::impl& p, const kernel_io& io, std::uint32_t y0, std::uint32_t y1,
                         void*) {
  cpu_detail::count_kernel_call(cpu_path::avx2);
  if (p.out.type == sample_type::f32) rows_kernel<float>(p, io, y0, y1);
  else rows_kernel<double>(p, io, y0, y1);
}

void master_band_avx2(const master_band& b) {
  cpu_detail::count_kernel_call(cpu_path::avx2);
  if (b.f32_out) band_kernel<float>(b);
  else band_kernel<double>(b);
}

double sum_avx2(const double* x, std::size_t n) {
  cpu_detail::count_kernel_call(cpu_path::avx2);
  return lanes_sum(x, n);
}

}  // namespace astro::kernels
