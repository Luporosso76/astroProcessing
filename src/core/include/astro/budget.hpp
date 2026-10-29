#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "astro/json.hpp"

namespace astro {

class sqlite_dag_store;

enum class budget_mode { automatic, manual };
enum class budget_unit { mib, gib, percent };
enum class budget_denominator { mem_total, mem_available, cgroup_max };

// F-RES-01: con 0.375 una crescita concorrente di 0.40·MemAvailable dopo il preflight resta entro 16 MiB di swap
// oltre il fondo (misura e perimetro in docs/evidence/M2-gate.md).
constexpr double budget_fraction_normal = 0.375;
constexpr double budget_fraction_pressure = 0.25;
constexpr double budget_psi_threshold = 10.0;

// Rapporto intero esatto: il budget automatico non usa mai virgola mobile per le frazioni.
struct budget_ratio {
  std::uint64_t num = 1, den = 1;
  friend bool operator==(const budget_ratio&, const budget_ratio&) = default;
};

// Frazioni e soglia PSI del budget automatico; i default sono le costanti sopra.
struct budget_tuning {
  budget_ratio normal{3, 8};
  budget_ratio pressure{1, 4};
  double psi_threshold = budget_psi_threshold;
};

// Grammatica esatta: `<intero>MiB`, `<intero>GiB`, `<intero>%:<denominatore>`.
struct manual_budget {
  budget_unit unit = budget_unit::mib;
  std::uint64_t value = 0;
  std::optional<budget_denominator> of;  // solo per `percent`
  friend bool operator==(const manual_budget&, const manual_budget&) = default;
};

// Radici iniettabili di /proc e /sys/fs/cgroup.
struct budget_sources {
  std::filesystem::path proc_root = "/proc";
  std::filesystem::path cgroup_root = "/sys/fs/cgroup";
};

struct budget_snapshot {
  budget_mode mode = budget_mode::automatic;
  std::uint64_t budget_bytes = 0;  // multiplo di 1 MiB, per difetto
  std::uint64_t mem_total_bytes = 0;
  std::uint64_t mem_available_bytes = 0;
  std::optional<std::uint64_t> cgroup_max_bytes;
  std::optional<std::uint64_t> cgroup_current_bytes;
  std::optional<double> psi_some_avg10;
  double fraction = 0;  // solo automatic
  std::optional<manual_budget> manual;
  // Configurazione delle risorse con default e origini (resource_config_json), se il chiamante la passa.
  std::optional<json> resources;
};

// Sintassi: invalid_input budget.syntax. Zero, percentuale fuori 1..100 o valore non rappresentabile:
// invalid_input budget.out_of_range.
manual_budget parse_manual_budget(std::string_view text);
std::string to_string(const manual_budget& budget);

// Errori: io budget.meminfo / budget.cgroup_read; budget budget.cgroup_over_limit; invalid_input budget.cgroup_parse /
// budget.psi_parse. Il cgroup effettivo e' il livello, dal proprio verso la radice, con meno margine.
budget_snapshot compute_automatic_budget(const budget_sources& sources = {}, const budget_tuning& tuning = {});
// Come sopra, piu': metadata_missing budget.no_cgroup_limit; invalid_input budget.out_of_range (zero o oltre mem_total).
budget_snapshot compute_manual_budget(const manual_budget& manual, const budget_sources& sources = {});

json budget_json(const budget_snapshot& snapshot);

constexpr const char* manual_budget_setting_key = "budget.manual";
// Convalida prima di scrivere in `settings` (JSON canonico).
void save_manual_budget(sqlite_dag_store& store, const manual_budget& budget);
// Rivalida a ogni lettura; `nullopt` se non e' mai stato salvato. Valore illeggibile: invalid_input
// budget.setting_malformed o gli errori della grammatica.
std::optional<manual_budget> load_manual_budget(const sqlite_dag_store& store);

}  // namespace astro
