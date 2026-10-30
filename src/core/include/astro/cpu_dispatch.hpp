#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "astro/fault_injector.hpp"
#include "astro/json.hpp"

namespace astro {

// Percorsi CPU e dispatch nel preflight (ADR-07.a, NFR-SIMD-01).

enum class simd_request { auto_select, scalar, avx2 };
enum class cpu_path { scalar, avx2 };

struct cpu_capabilities {
  bool avx2 = false;
};

const char* simd_request_name(simd_request request) noexcept;  // "auto", "scalar", "avx2"
const char* backend_name(cpu_path path) noexcept;              // "cpu_scalar", "cpu_avx2"

// __builtin_cpu_supports("avx2"), oppure le capacita' iniettate da cpu_test::set_capabilities.
cpu_capabilities read_cpu_capabilities();

// Tabella di ADR-07: `avx2` senza AVX2 e' unsupported cpu.avx2_unavailable; `auto` senza AVX2 e' cpu_scalar.
cpu_path resolve_cpu_path(simd_request request, const cpu_capabilities& caps);

// MXCSR con FZ (bit 15) o DAZ (bit 6) accesi, RC (bit 13-14) diverso da «al piu' vicino» o una maschera di eccezione
// (bit 7-12) spenta: numeric cpu.fp_mode, con `mxcsr` nel contesto. Il registro non viene modificato.
void require_fp_environment();

// Avvio di un task dello scheduler nel thread del worker, prima che il task legga i propri dati (OB-05):
// fault_injector del seam al punto cpu.task_start (un valore non nullo e' io cpu.task_start con l'errno), poi
// require_fp_environment.
void begin_worker_task();

// Avvio di un kernel: require_fp_environment, preceduto dal seam cpu.task_start solo fuori dai worker dello scheduler
// (chiamate di libreria), dove il seam e' gia' scattato in begin_worker_task.
void begin_kernel_task();

// Somma ricorsiva su pixel (ADR-07 (b)): cpu_scalar nell'ordine degli indici, cpu_avx2 su 4 corsie con riduzione
// finale (c0 + c1) + (c2 + c3). Entrambe inizializzano l'accumulatore col primo termine. n = 0: invalid_input
// cpu.empty_sum.
double pixel_sum(cpu_path path, const double* x, std::size_t n);

// `{"simd": richiesta, "backend": percorso, "capabilities": {"avx2": bool}}` per la provenance, con richiesta e percorso
// del nodo del job; `simd` nullo: il nodo non porta la richiesta e la chiave manca.
json cpu_dispatch_json(const json* simd, const std::string& backend, const cpu_capabilities& caps);

// Seam dei test (ADR-07, aggiunte di R5): si imposta prima del job e non cambia durante l'esecuzione.
namespace cpu_test {
void set_capabilities(std::optional<cpu_capabilities> caps);
void set_task_injector(fault_injector injector);
std::uint64_t kernel_calls(cpu_path path);
void reset_kernel_calls();
}  // namespace cpu_test

}  // namespace astro
