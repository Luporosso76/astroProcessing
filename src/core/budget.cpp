#include "astro/budget.hpp"

#include <fcntl.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <vector>

#include "astro/sqlite_dag_store.hpp"
#include "sqlite_internal.hpp"

namespace astro {

using namespace sqlint;
namespace fs = std::filesystem;

namespace {

constexpr std::uint64_t mib = std::uint64_t{1} << 20;
constexpr unsigned normal_shift = 1;
constexpr unsigned pressure_shift = 2;
static_assert(budget_fraction_normal == 1.0 / (1 << normal_shift));
static_assert(budget_fraction_pressure == 1.0 / (1 << pressure_shift));
constexpr std::uint64_t max_source_bytes = std::uint64_t{1} << 16;

[[noreturn]] void syntax(std::string_view text) {
  throw stop_error(error_category::invalid_input, "budget.syntax",
                   "budget manuale: attesi <intero>MiB, <intero>GiB o <intero>%:<denominatore>",
                   {{"valore", std::string(text)}});
}

[[noreturn]] void out_of_range(const char* why, stop_error::context_list context = {}) {
  throw stop_error(error_category::invalid_input, "budget.out_of_range", why, std::move(context));
}

bool all_digits(std::string_view s) {
  return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}

// false se il valore non entra in 64 bit.
bool to_u64(std::string_view digits, std::uint64_t& out) {
  out = 0;
  for (const char c : digits) {
    const std::uint64_t d = static_cast<std::uint64_t>(c - '0');
    if (out > (UINT64_MAX - d) / 10) return false;
    out = out * 10 + d;
  }
  return true;
}

const char* denominator_name(budget_denominator d) {
  switch (d) {
    case budget_denominator::mem_total: return "mem_total";
    case budget_denominator::mem_available: return "mem_available";
    case budget_denominator::cgroup_max: return "cgroup_max";
  }
  std::abort();
}

std::optional<std::string> read_optional_file(const fs::path& path, const char* code) {
  unique_fd fd(::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOCTTY));
  if (!fd) {
    if (errno == ENOENT || errno == ENOTDIR) return std::nullopt;
    throw_errno(error_category::io, code, "apertura fallita", errno, {{"file", path.string()}});
  }
  const std::vector<std::uint8_t> bytes = read_all(fd.get(), max_source_bytes, code, path.string());
  return std::string(bytes.begin(), bytes.end());
}

struct meminfo {
  std::uint64_t total = 0;
  std::uint64_t available = 0;
};

[[noreturn]] void meminfo_error(const char* why, const fs::path& path) {
  throw stop_error(error_category::io, "budget.meminfo", why, {{"file", path.string()}});
}

meminfo read_meminfo(const budget_sources& sources) {
  const fs::path path = sources.proc_root / "meminfo";
  const std::optional<std::string> text = read_optional_file(path, "budget.meminfo");
  if (!text) meminfo_error("meminfo assente", path);
  std::optional<std::uint64_t> total, available;
  std::istringstream lines(*text);
  std::string line;
  while (std::getline(lines, line)) {
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    const std::string key = line.substr(0, colon);
    if (key != "MemTotal" && key != "MemAvailable") continue;
    std::istringstream rest(line.substr(colon + 1));
    std::string digits, unit, extra;
    rest >> digits >> unit;
    std::uint64_t kib = 0;
    if (!all_digits(digits) || unit != "kB" || (rest >> extra) || !to_u64(digits, kib) || kib > UINT64_MAX / 1024) {
      meminfo_error("riga di meminfo non interpretabile", path);
    }
    (key == "MemTotal" ? total : available) = kib * 1024;
  }
  if (!total) meminfo_error("MemTotal assente", path);
  if (!available) meminfo_error("MemAvailable assente", path);
  return meminfo{*total, *available};
}

struct cgroup_limit {
  std::uint64_t max = 0;
  std::uint64_t current = 0;
};

[[noreturn]] void cgroup_parse(const char* why, const fs::path& path) {
  throw stop_error(error_category::invalid_input, "budget.cgroup_parse", why, {{"file", path.string()}});
}

std::string strip_newline(std::string text) {
  if (!text.empty() && text.back() == '\n') text.pop_back();
  return text;
}

// Livelli del cgroup v2 del processo, dal proprio verso la radice. Vuoto se /proc/self/cgroup non indica un cgroup v2.
std::vector<fs::path> cgroup_levels(const budget_sources& sources) {
  const fs::path self = sources.proc_root / "self" / "cgroup";
  const std::optional<std::string> text = read_optional_file(self, "budget.cgroup_read");
  if (!text) return {};
  std::istringstream lines(*text);
  std::string line;
  std::optional<std::string> v2_path;
  while (std::getline(lines, line)) {
    if (line.empty()) continue;
    const std::size_t first = line.find(':');
    const std::size_t second = first == std::string::npos ? first : line.find(':', first + 1);
    if (second == std::string::npos) cgroup_parse("riga di /proc/self/cgroup non interpretabile", self);
    const std::string_view id = std::string_view(line).substr(0, first);
    const std::string_view controllers = std::string_view(line).substr(first + 1, second - first - 1);
    const std::string path = line.substr(second + 1);
    if (!all_digits(id) || path.empty() || path[0] != '/') {
      cgroup_parse("riga di /proc/self/cgroup non interpretabile", self);
    }
    if (id == "0") {
      if (!controllers.empty()) cgroup_parse("gerarchia 0 con lista di controller non vuota", self);
      if (v2_path) cgroup_parse("piu' righe cgroup v2", self);
      v2_path = path;
    } else if (controllers.empty() || id.front() == '0') {
      cgroup_parse("riga cgroup v1 con id o controller non validi", self);
    }
  }
  if (v2_path) {
    std::vector<std::string> parts;
    std::istringstream path(*v2_path);
    std::string part;
    while (std::getline(path, part, '/')) {
      if (part.empty()) continue;
      if (part == "." || part == "..") cgroup_parse("componente di percorso non ammesso", self);
      parts.push_back(part);
    }
    std::vector<fs::path> levels;
    for (std::size_t depth = parts.size() + 1; depth-- > 0;) {
      fs::path level = sources.cgroup_root;
      for (std::size_t i = 0; i < depth; ++i) level /= parts[i];
      levels.push_back(std::move(level));
    }
    return levels;
  }
  return {};
}

std::uint64_t parse_cgroup_number(const std::string& text, const fs::path& path) {
  std::uint64_t value = 0;
  if (!all_digits(text) || !to_u64(text, value)) cgroup_parse("valore non interpretabile", path);
  return value;
}

std::optional<cgroup_limit> read_cgroup(const budget_sources& sources) {
  std::optional<cgroup_limit> tightest;
  for (const fs::path& level : cgroup_levels(sources)) {
    const fs::path max_path = level / "memory.max";
    const std::optional<std::string> max_text = read_optional_file(max_path, "budget.cgroup_read");
    if (!max_text) continue;
    const std::string max_value = strip_newline(*max_text);
    if (max_value == "max") continue;
    const std::uint64_t max = parse_cgroup_number(max_value, max_path);
    const fs::path current_path = level / "memory.current";
    const std::optional<std::string> current_text = read_optional_file(current_path, "budget.cgroup_read");
    if (!current_text) cgroup_parse("memory.current assente", current_path);
    const std::uint64_t current = parse_cgroup_number(strip_newline(*current_text), current_path);
    if (current >= max) {
      throw stop_error(error_category::budget, "budget.cgroup_over_limit",
                       "il cgroup ha gia' raggiunto il proprio limite",
                       {{"cgroup_max", std::to_string(max)}, {"cgroup_current", std::to_string(current)},
                        {"cgroup", level.string()}});
    }
    if (!tightest || max - current < tightest->max - tightest->current) tightest = cgroup_limit{max, current};
  }
  return tightest;
}

std::optional<double> read_psi(const budget_sources& sources) {
  const fs::path path = sources.proc_root / "pressure" / "memory";
  const std::optional<std::string> text = read_optional_file(path, "budget.psi_parse");
  if (!text) return std::nullopt;
  const auto bad = [&](const char* why) {
    return stop_error(error_category::invalid_input, "budget.psi_parse", why, {{"file", path.string()}});
  };
  std::istringstream lines(*text);
  std::string line;
  while (std::getline(lines, line)) {
    if (line.rfind("some ", 0) != 0) continue;
    std::istringstream fields(line.substr(5));
    std::string field;
    while (fields >> field) {
      if (field.rfind("avg10=", 0) != 0) continue;
      const std::string number = field.substr(6);
      char* end = nullptr;
      errno = 0;
      const double value = std::strtod(number.c_str(), &end);
      if (number.empty() || end != number.c_str() + number.size() || errno != 0 || !std::isfinite(value) || value < 0) {
        throw bad("avg10 non interpretabile");
      }
      return value;
    }
    throw bad("avg10 assente");
  }
  throw bad("riga `some` assente");
}

budget_snapshot gather(const budget_sources& sources) {
  const meminfo mem = read_meminfo(sources);
  const std::optional<cgroup_limit> cgroup = read_cgroup(sources);
  budget_snapshot s;
  s.mem_total_bytes = mem.total;
  s.mem_available_bytes = mem.available;
  if (cgroup) {
    s.cgroup_max_bytes = cgroup->max;
    s.cgroup_current_bytes = cgroup->current;
  }
  s.psi_some_avg10 = read_psi(sources);
  return s;
}

std::uint64_t floor_mib(std::uint64_t bytes) { return bytes / mib * mib; }

}  // namespace

manual_budget parse_manual_budget(std::string_view text) {
  std::size_t digits_end = 0;
  while (digits_end < text.size() && text[digits_end] >= '0' && text[digits_end] <= '9') ++digits_end;
  const std::string_view digits = text.substr(0, digits_end);
  const std::string_view suffix = text.substr(digits_end);
  if (digits.empty()) syntax(text);
  manual_budget budget;
  if (suffix == "MiB") {
    budget.unit = budget_unit::mib;
  } else if (suffix == "GiB") {
    budget.unit = budget_unit::gib;
  } else if (suffix.rfind("%:", 0) == 0) {
    budget.unit = budget_unit::percent;
    const std::string_view denominator = suffix.substr(2);
    if (denominator == "mem_total") {
      budget.of = budget_denominator::mem_total;
    } else if (denominator == "mem_available") {
      budget.of = budget_denominator::mem_available;
    } else if (denominator == "cgroup_max") {
      budget.of = budget_denominator::cgroup_max;
    } else {
      syntax(text);
    }
  } else {
    syntax(text);
  }
  if (!to_u64(digits, budget.value)) out_of_range("valore oltre 64 bit", {{"valore", std::string(text)}});
  if (budget.value == 0) out_of_range("valore zero", {{"valore", std::string(text)}});
  if (budget.unit == budget_unit::percent && budget.value > 100) {
    out_of_range("percentuale oltre 100", {{"valore", std::string(text)}});
  }
  const unsigned shift = budget.unit == budget_unit::gib ? 30 : 20;
  if (budget.unit != budget_unit::percent && budget.value > (UINT64_MAX >> shift)) {
    out_of_range("valore non rappresentabile in byte", {{"valore", std::string(text)}});
  }
  return budget;
}

std::string to_string(const manual_budget& budget) {
  switch (budget.unit) {
    case budget_unit::mib: return std::to_string(budget.value) + "MiB";
    case budget_unit::gib: return std::to_string(budget.value) + "GiB";
    case budget_unit::percent:
      if (!budget.of) syntax("percentuale senza denominatore");
      return std::to_string(budget.value) + "%:" + denominator_name(*budget.of);
  }
  std::abort();
}

budget_snapshot compute_automatic_budget(const budget_sources& sources) {
  budget_snapshot s = gather(sources);
  s.mode = budget_mode::automatic;
  std::uint64_t avail = s.mem_available_bytes;
  if (s.cgroup_max_bytes) avail = std::min(avail, *s.cgroup_max_bytes - *s.cgroup_current_bytes);
  const bool pressure = s.psi_some_avg10 && *s.psi_some_avg10 >= budget_psi_threshold;
  s.fraction = pressure ? budget_fraction_pressure : budget_fraction_normal;
  s.budget_bytes = floor_mib(avail >> (pressure ? pressure_shift : normal_shift));
  return s;
}

budget_snapshot compute_manual_budget(const manual_budget& manual, const budget_sources& sources) {
  const manual_budget checked = parse_manual_budget(to_string(manual));
  budget_snapshot s = gather(sources);
  s.mode = budget_mode::manual;
  s.manual = checked;
  std::uint64_t bytes = 0;
  switch (checked.unit) {
    case budget_unit::mib: bytes = checked.value << 20; break;
    case budget_unit::gib: bytes = checked.value << 30; break;
    case budget_unit::percent: {
      std::uint64_t base = 0;
      switch (*checked.of) {
        case budget_denominator::mem_total: base = s.mem_total_bytes; break;
        case budget_denominator::mem_available: base = s.mem_available_bytes; break;
        case budget_denominator::cgroup_max:
          if (!s.cgroup_max_bytes) {
            throw stop_error(error_category::metadata_missing, "budget.no_cgroup_limit",
                             "nessun limite cgroup v2 per il denominatore cgroup_max");
          }
          base = *s.cgroup_max_bytes;
          break;
      }
      bytes = base / 100 * checked.value + base % 100 * checked.value / 100;
      break;
    }
  }
  s.budget_bytes = floor_mib(bytes);
  if (s.budget_bytes == 0 || s.budget_bytes > s.mem_total_bytes) {
    out_of_range("il budget manuale e' zero o supera mem_total",
                 {{"budget_bytes", std::to_string(s.budget_bytes)}, {"mem_total", std::to_string(s.mem_total_bytes)},
                  {"valore", to_string(checked)}});
  }
  return s;
}

json budget_json(const budget_snapshot& s) {
  const auto opt_u64 = [](const std::optional<std::uint64_t>& v) { return v ? json::uinteger(*v) : json::null(); };
  json doc = json::object();
  doc.set("mode", json::string(s.mode == budget_mode::automatic ? "automatic" : "manual"));
  doc.set("budget_bytes", json::uinteger(s.budget_bytes));
  doc.set("mem_total_bytes", json::uinteger(s.mem_total_bytes));
  doc.set("mem_available_bytes", json::uinteger(s.mem_available_bytes));
  doc.set("cgroup_max_bytes", opt_u64(s.cgroup_max_bytes));
  doc.set("cgroup_current_bytes", opt_u64(s.cgroup_current_bytes));
  doc.set("psi_some_avg10", s.psi_some_avg10 ? json::number(*s.psi_some_avg10) : json::null());
  if (s.mode == budget_mode::automatic) doc.set("fraction", json::number(s.fraction));
  if (s.manual) doc.set("manual", json::string(to_string(*s.manual)));
  return doc;
}

void save_manual_budget(sqlite_dag_store& store, const manual_budget& budget) {
  const std::string spec = to_string(parse_manual_budget(to_string(budget)));
  json doc = json::object();
  doc.set("spec", json::string(spec));
  store.put_setting(manual_budget_setting_key, canonical_json(doc));
}

std::optional<manual_budget> load_manual_budget(const sqlite_dag_store& store) {
  const std::optional<std::string> stored = store.get_setting(manual_budget_setting_key);
  if (!stored) return std::nullopt;
  const json doc = parse_json(*stored);
  const json* spec = doc.find("spec");
  if (doc.type() != json::kind::object || doc.keys().size() != 1 || spec == nullptr ||
      spec->type() != json::kind::string) {
    throw stop_error(error_category::invalid_input, "budget.setting_malformed", "impostazione del budget manuale non valida",
                     {{"valore", *stored}});
  }
  return parse_manual_budget(spec->as_string());
}

}  // namespace astro
