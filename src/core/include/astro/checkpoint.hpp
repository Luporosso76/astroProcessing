#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "astro/fault_injector.hpp"
#include "astro/node.hpp"
#include "astro/job_plan.hpp"

namespace astro {

constexpr const char* checkpoint_format = "astro.checkpoint.v1";

struct checkpoint_record {
  std::string node_key;
  std::string recipe_digest;
  std::string backend;
  std::uint64_t task_count = 0;
  std::vector<std::uint64_t> completed;       // indici crescenti
  std::vector<std::string> partial_digests;   // SHA-256 esadecimale, paralleli a `completed`
};

// SHA-256 del JCS di { node_spec, tile_rows, worker_count, spill, checkpoint_every }.
std::string recipe_digest(const node_spec& spec, const job_plan& plan);

// Scrittura atomica (ADR-04, passi 1-5) di <name>.ckpt.json in `dir`, che deve esistere e non essere un symlink
// (io sched.checkpoint_dir). `name` identifica il job ([A-Za-z0-9_-]{1,64}, altrimenti invalid_input
// sched.checkpoint_name). Sostituisce il checkpoint precedente con lo stesso nome. `injector` (solo test) riceve i
// punti `checkpoint.<passo>` del protocollo atomico; un valore diverso da zero e' l'errno del passo (io).
void write_checkpoint(const std::filesystem::path& dir, const std::string& name, const checkpoint_record& record,
                      const fault_injector* injector = nullptr);
// nullopt se non esiste. File non interpretabile: integrity sched.checkpoint_malformed.
std::optional<checkpoint_record> read_checkpoint(const std::filesystem::path& dir, const std::string& name);
void remove_checkpoint(const std::filesystem::path& dir, const std::string& name);

}  // namespace astro
