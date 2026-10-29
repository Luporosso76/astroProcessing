#pragma once

// Quantili chi-quadro esatti per i test statistici (C-02.a §9), solo libreria standard.
// F(x; k) = P(k/2, x/2), gamma incompleta regolarizzata. Abramowitz e Stegun (1964): serie 6.5.29 per la coda
// inferiore P con x < a + 1, frazione continua 6.5.31 (valutata con il metodo di Lentz) per la coda superiore Q con
// x >= a + 1, presa direttamente e mai come 1 - P. Arresto con errore relativo del termine <= 1e-15 sulla coda
// (richiesto <= 1e-12) e bisezione fino a un intervallo relativo <= 1e-13 sul quantile (richiesto <= 1e-10).

#include <cmath>
#include <stdexcept>

namespace chi2x {

inline long double log_prefactor(long double a, long double x) { return -x + a * std::log(x); }

// P(a, x) per x < a + 1 (serie 6.5.29).
inline long double lower_regularized(long double a, long double x) {
  if (!(x >= 0) || !(x < a + 1)) throw std::domain_error("serie 6.5.29 fuori dal dominio x < a + 1");
  if (x == 0) return 0;
  long double term = 1, sum = 1;
  for (int n = 1; n < 100000; ++n) {
    term *= x / (a + n);
    sum += term;
    if (term < sum * 1e-15L) {
      return sum * std::exp(log_prefactor(a, x) - std::lgamma(a + 1));
    }
  }
  throw std::runtime_error("serie 6.5.29 non convergente");
}

// Q(a, x) = 1 - P(a, x) per x >= a + 1 (frazione continua 6.5.31, metodo di Lentz).
inline long double upper_regularized(long double a, long double x) {
  if (!(x >= a + 1)) throw std::domain_error("frazione continua 6.5.31 fuori dal dominio x >= a + 1");
  constexpr long double tiny = 1e-4000L;
  long double b = x + 1 - a, c = 1 / tiny, d = 1 / b, h = d;
  for (int i = 1; i < 100000; ++i) {
    const long double an = -static_cast<long double>(i) * (i - a);
    b += 2;
    d = an * d + b;
    if (std::fabs(d) < tiny) d = tiny;
    c = b + an / c;
    if (std::fabs(c) < tiny) c = tiny;
    d = 1 / d;
    const long double del = d * c;
    h *= del;
    if (std::fabs(del - 1) < 1e-15L) return h * std::exp(log_prefactor(a, x) - std::lgamma(a));
  }
  throw std::runtime_error("frazione continua 6.5.31 non convergente");
}

// Quantile inferiore: x con P(k/2, x/2) = p (coda inferiore p, x/2 < k/2 + 1).
inline double lower_quantile(double k, double p) {
  const long double a = k / 2.0L;
  long double lo = 0, hi = 2 * (a + 1);
  if (lower_regularized(a, hi / 2 * 0.999999999999L) < p) throw std::domain_error("quantile inferiore oltre a + 1");
  for (int it = 0; it < 400 && (hi - lo) > 1e-13L * hi; ++it) {
    const long double mid = (lo + hi) / 2;
    (lower_regularized(a, mid / 2) < p ? lo : hi) = mid;
  }
  return static_cast<double>((lo + hi) / 2);
}

// Quantile superiore: x con Q(k/2, x/2) = p (coda superiore p, x/2 >= k/2 + 1).
inline double upper_quantile(double k, double p) {
  const long double a = k / 2.0L;
  long double lo = 2 * (a + 1), hi = lo;
  if (upper_regularized(a, lo / 2) < p) throw std::domain_error("quantile superiore sotto a + 1");
  while (upper_regularized(a, hi / 2) > p) hi *= 2;
  for (int it = 0; it < 400 && (hi - lo) > 1e-13L * hi; ++it) {
    const long double mid = (lo + hi) / 2;
    (upper_regularized(a, mid / 2) > p ? lo : hi) = mid;
  }
  return static_cast<double>((lo + hi) / 2);
}

}  // namespace chi2x
