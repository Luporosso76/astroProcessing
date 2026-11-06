#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "astro/image.hpp"

namespace astro {

// Operazioni sottoposte alla tabella di compatibilità fase × linearità (ADR-03, R1), più le tre righe della
// calibrazione `.b` (ADR-03.b §b.14), assegnate a ogni ingresso dal suo ruolo e stato: non dipendono dal solo stato e
// non entrano in `preflight_table`.
enum class operation_kind {
  inventory,
  preview,
  export_bundle,
  master_create,
  calibrate,
  linear_ancestor_required,
  calibrate_light,
  calibrate_master_raw,
  calibrate_master_prepared,
};

// Operazioni di R1, le voci di `preflight_table`.
constexpr std::size_t operation_count = 6;

struct process_state {
  process_phase phase = process_phase::raw;
  linearity_class lin = linearity_class::unknown;
};

const char* to_string(operation_kind kind) noexcept;
const char* to_string(process_phase phase) noexcept;
const char* to_string(linearity_class lin) noexcept;

// `lin` assente = `unknown`.
linearity_class classify_linearity(const std::optional<linearity>& lin) noexcept;

// Codice dello stop per l'operazione non ammessa nello stato; vuoto se ammessa.
std::string blocked_code(operation_kind kind, const process_state& state);

// Ammessa: niente. Altrimenti incompatible, op.phase_blocked / op.linearity_blocked / op.linear_ancestor_missing.
void require_operation_allowed(operation_kind kind, const process_state& state);
// Come sopra, con il ruolo dell'ingresso nel contesto (chiave `role`, ADR-03.b §b.14).
void require_operation_allowed(operation_kind kind, const process_state& state, std::string_view role);

// Diagnostica del preflight leggibile dai report: una voce per operazione non ammessa, più il limite C-14.
struct preflight_entry {
  operation_kind operation;
  bool allowed = false;
  std::string code;
};
std::vector<preflight_entry> preflight_table(const process_state& state);

}  // namespace astro
