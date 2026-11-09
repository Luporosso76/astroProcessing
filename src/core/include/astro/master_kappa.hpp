#pragma once

#include <cstdint>

namespace astro {

// κ dei metodi robusti del master (C-02.b §b.6, «Metodi robusti»): κ_T(n, g) = Var(T(Z_1, …, Z_n)) con Z iid N(0, 1)
// e T la mediana o winsorized con parametro g. Funzione pura e deterministica, calcolata una volta nel preflight; il
// valore entra nel report. Verificata da T-M3B-C02-05 contro un oracle di quadratura del test: il prodotto usa una
// quadratura diversa da quella dell'oracle (Kronrod a 21 punti nelle coordinate logit della fattorizzazione di
// Dirichlet, tests/unit/kappa_oracle.hpp).

enum class kappa_estimator { median, winsorized };

constexpr std::uint32_t kappa_n_max = 1024;     // dominio entro cui κ è verificato (C-02.b §b.2)
constexpr double kappa_rel_error_max = 1e-9;    // regola sul calcolo di κ (C-02.b §b.6)

struct kappa_result {
  double value = 0;               // κ arrotondato a f64
  double rel_error_estimate = 0;  // stima dell'errore relativo dal controllo d'errore dell'implementazione
};

// Dominio: median con g = 0 e 2 ≤ n ≤ kappa_n_max; winsorized con g ≥ 1 e 2g + 1 ≤ n ≤ kappa_n_max.
// Fuori dominio: unsupported, master.variance_domain (metodo, n e g nel contesto).
// Non confronta la stima con kappa_rel_error_max: lo fa check_kappa_accuracy.
kappa_result robust_kappa(kappa_estimator estimator, std::uint32_t n, std::uint32_t g);

// Stop numeric, master.kappa_accuracy (metodo, n, g, stima e soglia nel contesto) se la stima non è finita o supera
// kappa_rel_error_max; il valore uguale alla soglia è ammesso. Nel job si chiama prima di ogni lettura di campioni.
void check_kappa_accuracy(const kappa_result& kappa, kappa_estimator estimator, std::uint32_t n, std::uint32_t g);

}  // namespace astro
