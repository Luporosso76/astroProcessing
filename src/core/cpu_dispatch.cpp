#include "astro/cpu_dispatch.hpp"

#include <xmmintrin.h>

#include <atomic>
#include <cstdio>
#include <string>

#include "astro/error.hpp"
#include "cpu_kernels.hpp"

namespace astro {

namespace {

constexpr unsigned mxcsr_daz = 1u << 6;
constexpr unsigned mxcsr_ftz = 1u << 15;
constexpr unsigned mxcsr_rc = 3u << 13;
// Maschere delle sei eccezioni (IM, DM, ZM, OM, UM, PM, bit 7-12): tutte attive, cosi' nessuna operazione dei kernel,
// nemmeno sulle corsie scartate, genera un segnale.
constexpr unsigned mxcsr_masks = 0x3fu << 7;

// Seam dei test: impostato prima del job, letto dai worker senza modifiche concorrenti.
std::optional<cpu_capabilities> injected_capabilities;
fault_injector task_injector;
std::atomic<std::uint64_t> scalar_calls{0};
std::atomic<std::uint64_t> avx2_calls{0};

}  // namespace

const char* simd_request_name(simd_request request) noexcept {
  switch (request) {
    case simd_request::auto_select: return "auto";
    case simd_request::scalar: return "scalar";
    case simd_request::avx2: return "avx2";
  }
  ASTRO_ASSERT(false);
  return "";
}

const char* backend_name(cpu_path path) noexcept {
  switch (path) {
    case cpu_path::scalar: return "cpu_scalar";
    case cpu_path::avx2: return "cpu_avx2";
  }
  ASTRO_ASSERT(false);
  return "";
}

cpu_capabilities read_cpu_capabilities() {
  if (injected_capabilities) return *injected_capabilities;
  // Il builtin di GCC riporta AVX2 solo se anche il sistema operativo abilita lo stato esteso (XGETBV).
  __builtin_cpu_init();
  return cpu_capabilities{__builtin_cpu_supports("avx2") != 0};
}

cpu_path resolve_cpu_path(simd_request request, const cpu_capabilities& caps) {
  switch (request) {
    case simd_request::scalar: return cpu_path::scalar;
    case simd_request::avx2:
      if (!caps.avx2) {
        throw stop_error(error_category::unsupported, "cpu.avx2_unavailable",
                         "percorso avx2 richiesto ma la CPU o il sistema non offrono AVX2",
                         {{"simd", "avx2"}, {"avx2", "false"}});
      }
      return cpu_path::avx2;
    case simd_request::auto_select: return caps.avx2 ? cpu_path::avx2 : cpu_path::scalar;
  }
  ASTRO_ASSERT(false);
  return cpu_path::scalar;
}

void require_fp_environment() {
  const unsigned csr = _mm_getcsr();
  if ((csr & (mxcsr_ftz | mxcsr_daz | mxcsr_rc)) == 0 && (csr & mxcsr_masks) == mxcsr_masks) return;
  char hex[16];
  std::snprintf(hex, sizeof hex, "0x%x", csr);
  const unsigned rc = (csr & mxcsr_rc) >> 13;
  throw stop_error(error_category::numeric, "cpu.fp_mode",
                   "ambiente in virgola mobile non ammesso: FZ, DAZ, arrotondamento diverso dal piu' vicino o eccezioni "
                   "non mascherate",
                   {{"mxcsr", hex},
                    {"fz", (csr & mxcsr_ftz) ? "1" : "0"},
                    {"daz", (csr & mxcsr_daz) ? "1" : "0"},
                    {"rc", std::to_string(rc)},
                    {"maschere", std::to_string((csr & mxcsr_masks) >> 7)}});
}

void begin_kernel_task() {
  if (task_injector) {
    const int err = task_injector("cpu.task_start");
    if (err != 0) {
      throw stop_error(error_category::io, "cpu.task_start", "errore all'avvio del task del kernel",
                       {{"errno", std::to_string(err)}});
    }
  }
  require_fp_environment();
}

double pixel_sum(cpu_path path, const double* x, std::size_t n) {
  if (n == 0) throw stop_error(error_category::invalid_input, "cpu.empty_sum", "somma su pixel senza termini");
  return path == cpu_path::avx2 ? kernels::sum_avx2(x, n) : kernels::sum_scalar(x, n);
}

json cpu_dispatch_json(simd_request request, const cpu_capabilities& caps, cpu_path path) {
  json capabilities = json::object();
  capabilities.set("avx2", json::boolean(caps.avx2));
  json o = json::object();
  o.set("simd", json::string(simd_request_name(request)));
  o.set("backend", json::string(backend_name(path)));
  o.set("capabilities", std::move(capabilities));
  return o;
}

namespace cpu_detail {

void count_kernel_call(cpu_path path) noexcept {
  (path == cpu_path::avx2 ? avx2_calls : scalar_calls).fetch_add(1, std::memory_order_relaxed);
}

}  // namespace cpu_detail

namespace cpu_test {

void set_capabilities(std::optional<cpu_capabilities> caps) { injected_capabilities = caps; }
void set_task_injector(fault_injector injector) { task_injector = std::move(injector); }
std::uint64_t kernel_calls(cpu_path path) {
  return (path == cpu_path::avx2 ? avx2_calls : scalar_calls).load(std::memory_order_relaxed);
}
void reset_kernel_calls() {
  scalar_calls.store(0, std::memory_order_relaxed);
  avx2_calls.store(0, std::memory_order_relaxed);
}

}  // namespace cpu_test

}  // namespace astro
