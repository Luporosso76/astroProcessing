#pragma once

// Aritmetica razionale esatta per gli oracle di M2.e: interi senza segno a precisione arbitraria (base 2^32),
// razionali p/q con q > 0, conversione esatta da double. Nessuna divisione fra interi grandi: il quoziente di
// razionali scambia numeratore e denominatore; il confronto moltiplica in croce. Indipendente da astrocore.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace exact {

struct big {
  bool neg = false;
  std::vector<std::uint32_t> m;  // little endian, senza zeri in testa; vuoto = 0

  bool zero() const { return m.empty(); }
};

inline void trim(big& a) {
  while (!a.m.empty() && a.m.back() == 0) a.m.pop_back();
  if (a.m.empty()) a.neg = false;
}

inline big from_u64(std::uint64_t v, bool neg = false) {
  big r;
  r.neg = neg;
  while (v) {
    r.m.push_back(static_cast<std::uint32_t>(v));
    v >>= 32;
  }
  trim(r);
  return r;
}

inline int cmp_mag(const big& a, const big& b) {
  if (a.m.size() != b.m.size()) return a.m.size() < b.m.size() ? -1 : 1;
  for (std::size_t i = a.m.size(); i-- > 0;) {
    if (a.m[i] != b.m[i]) return a.m[i] < b.m[i] ? -1 : 1;
  }
  return 0;
}

inline big add_mag(const big& a, const big& b) {
  big r;
  std::uint64_t carry = 0;
  for (std::size_t i = 0; i < std::max(a.m.size(), b.m.size()) || carry; ++i) {
    std::uint64_t s = carry + (i < a.m.size() ? a.m[i] : 0) + (i < b.m.size() ? b.m[i] : 0);
    r.m.push_back(static_cast<std::uint32_t>(s));
    carry = s >> 32;
  }
  trim(r);
  return r;
}

// |a| >= |b|
inline big sub_mag(const big& a, const big& b) {
  big r;
  std::int64_t borrow = 0;
  for (std::size_t i = 0; i < a.m.size(); ++i) {
    std::int64_t d = static_cast<std::int64_t>(a.m[i]) - borrow - (i < b.m.size() ? b.m[i] : 0);
    borrow = d < 0 ? 1 : 0;
    if (d < 0) d += (std::int64_t{1} << 32);
    r.m.push_back(static_cast<std::uint32_t>(d));
  }
  trim(r);
  return r;
}

inline big add(const big& a, const big& b) {
  big r;
  if (a.neg == b.neg) {
    r = add_mag(a, b);
    r.neg = a.neg;
  } else if (cmp_mag(a, b) >= 0) {
    r = sub_mag(a, b);
    r.neg = a.neg;
  } else {
    r = sub_mag(b, a);
    r.neg = b.neg;
  }
  trim(r);
  return r;
}

inline big negate(big a) {
  if (!a.zero()) a.neg = !a.neg;
  return a;
}

inline big sub(const big& a, const big& b) { return add(a, negate(b)); }

inline big mul(const big& a, const big& b) {
  big r;
  if (a.zero() || b.zero()) return r;
  r.m.assign(a.m.size() + b.m.size(), 0);
  for (std::size_t i = 0; i < a.m.size(); ++i) {
    std::uint64_t carry = 0;
    for (std::size_t j = 0; j < b.m.size() || carry; ++j) {
      std::uint64_t cur = r.m[i + j] + carry + (j < b.m.size() ? std::uint64_t{a.m[i]} * b.m[j] : 0);
      r.m[i + j] = static_cast<std::uint32_t>(cur);
      carry = cur >> 32;
    }
  }
  r.neg = a.neg != b.neg;
  trim(r);
  return r;
}

inline big shl(const big& a, unsigned bits) {
  big r = a;
  if (a.zero()) return r;
  r.m.insert(r.m.begin(), bits / 32, 0);
  const unsigned s = bits % 32;
  if (s) {
    std::uint32_t carry = 0;
    for (auto& w : r.m) {
      const std::uint32_t next = w >> (32 - s);
      w = (w << s) | carry;
      carry = next;
    }
    if (carry) r.m.push_back(carry);
  }
  return r;
}

inline int cmp(const big& a, const big& b) {
  if (a.neg != b.neg) return a.neg ? -1 : 1;
  const int c = cmp_mag(a, b);
  return a.neg ? -c : c;
}

inline long double to_long_double(const big& a) {
  long double r = 0;
  for (std::size_t i = a.m.size(); i-- > 0;) r = r * 4294967296.0L + a.m[i];
  return a.neg ? -r : r;
}

struct rat {
  big n, d;  // d > 0
};

inline rat from_u(std::uint64_t v) { return rat{from_u64(v), from_u64(1)}; }

// Valore esatto di un double finito.
inline rat from_double(double x) {
  int e = 0;
  const double frac = std::frexp(x, &e);
  const auto mant = static_cast<std::int64_t>(std::ldexp(frac, 53));
  e -= 53;
  rat r{from_u64(static_cast<std::uint64_t>(mant < 0 ? -mant : mant), mant < 0), from_u64(1)};
  if (e >= 0) r.n = shl(r.n, static_cast<unsigned>(e));
  else r.d = shl(r.d, static_cast<unsigned>(-e));
  return r;
}

inline rat operator+(const rat& a, const rat& b) { return {add(mul(a.n, b.d), mul(b.n, a.d)), mul(a.d, b.d)}; }
inline rat operator-(const rat& a, const rat& b) { return {sub(mul(a.n, b.d), mul(b.n, a.d)), mul(a.d, b.d)}; }
inline rat operator*(const rat& a, const rat& b) { return {mul(a.n, b.n), mul(a.d, b.d)}; }
inline rat operator/(const rat& a, const rat& b) {
  rat r{mul(a.n, b.d), mul(a.d, b.n)};
  if (r.d.neg) {
    r.d.neg = false;
    r.n = negate(r.n);
  }
  return r;
}
inline rat abs(rat a) {
  a.n.neg = false;
  return a;
}
inline int cmp(const rat& a, const rat& b) { return cmp(mul(a.n, b.d), mul(b.n, a.d)); }
inline bool operator<=(const rat& a, const rat& b) { return cmp(a, b) <= 0; }
inline bool operator<(const rat& a, const rat& b) { return cmp(a, b) < 0; }
inline bool is_zero(const rat& a) { return a.n.zero(); }
inline long double to_ld(const rat& a) { return to_long_double(a.n) / to_long_double(a.d); }

// gamma_m = m u / (1 - m u), u = 2^-53.
inline rat gamma(unsigned m) {
  const rat mu{from_u64(m), shl(from_u64(1), 53)};
  return mu / (from_u(1) - mu);
}

// u = 2^-24 per float.
inline rat unit_roundoff_f32() { return {from_u64(1), shl(from_u64(1), 24)}; }

// Mediana esatta: conteggio dispari = elemento centrale, pari = media dei due centrali.
inline rat median(std::vector<rat> v) {
  for (std::size_t i = 1; i < v.size(); ++i) {
    for (std::size_t j = i; j > 0 && v[j] < v[j - 1]; --j) std::swap(v[j], v[j - 1]);
  }
  const std::size_t n = v.size();
  if (n % 2 == 1) return v[n / 2];
  return (v[n / 2 - 1] + v[n / 2]) / from_u(2);
}

}  // namespace exact
