#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "astro/spill.hpp"

namespace astro {

// Parametri del job dichiarati prima dell'esecuzione (ADR-06).
struct job_plan {
  std::uint64_t shared_bytes = 0;    // master, buffer comuni, tutto il batch di frame o candidati
  std::uint64_t per_task_bytes = 0;  // picco di un task
  unsigned worker_count = 1;         // fra 1 e cpu_count
  std::size_t queue_capacity = 1;    // >= 1
  std::uint32_t tile_rows = 1;       // altezza del blocco di righe per task
  std::optional<spill_config> spill;
  std::uint32_t checkpoint_every = 0;  // task completati fra due checkpoint; 0 nessun checkpoint
};

}  // namespace astro
