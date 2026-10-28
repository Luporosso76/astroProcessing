#pragma once

#include <optional>
#include <string>
#include <vector>

#include "astro/dag_store.hpp"
#include "astro/error.hpp"
#include "astro/hash.hpp"
#include "astro/json.hpp"
#include "astro/node.hpp"

namespace astro {

struct library_versions {
  std::string product;
  std::string source_revision;
  std::optional<bool> source_dirty;
  std::string cfitsio;
  std::string sqlite;
  std::string openssl;
};

library_versions current_library_versions();

// Provenance di un job terminato. `completed`: nessun arresto e output dichiarati; `failed`/`cancelled`: arresto
// presente e nessun output (altrimenti invalid_input dag.bad_provenance).
struct provenance_record {
  node_spec spec;
  library_versions versions;
  std::uint64_t generation = 0;
  job_state state = job_state::completed;
  std::vector<std::string> warnings;
  std::vector<content_digest> output_digests;
  std::optional<stop_error> stop;
};

json provenance_json(const provenance_record& record);
std::string provenance_canonical(const provenance_record& record);

}  // namespace astro
