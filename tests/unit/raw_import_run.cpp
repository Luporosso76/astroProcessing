// T-M3A-RAW-07 (ADR-08.b §2.1, §5): esegue il job minimo di `io.raw_import` su un DNG sintetico, da riga di comando sotto
// strace (tests/u4/test_raw07_offline.py). Scrive l'input e un file fratello in <dir>/inputs, calcola il digest, poi apre
// un percorso-marcatore inesistente (un `openat` visibile a strace) e solo dopo lancia il job: gli eventi successivi al
// marcatore sono quelli del prodotto.
// Uso: raw_import_run <directory di lavoro nuova>. Uscita 0 solo se il job pubblica.
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>

#include "astro/job_types.hpp"
#include "conformance_inputs.hpp"

int main(int argc, char** argv) {
  namespace fs = std::filesystem;
  using namespace astro;
  // ADR-08.b §2.6: TZ=UTC0 dal main, prima di ogni thread (il decoder RAW lo verifica).
  if (setenv("TZ", "UTC0", 1) != 0) {
    std::fputs("setenv(TZ) fallita\n", stderr);
    return 2;
  }
  tzset();
  if (argc != 2) {
    std::fputs("uso: raw_import_run <directory di lavoro>\n", stderr);
    return 1;
  }
  try {
    const fs::path dir = fs::absolute(argv[1]);
    if (fs::exists(dir)) {
      std::fputs("raw_import_run: la directory di lavoro esiste già\n", stderr);
      return 1;
    }
    const job_type_entry* entry = nullptr;
    for (const job_type_entry& e : registered_job_types()) {
      if (std::string(e.operator_id) == "io.raw_import") entry = &e;
    }
    if (entry == nullptr) {
      std::fputs("raw_import_run: tipo io.raw_import assente dal registro\n", stderr);
      return 1;
    }
    fs::create_directories(dir / "scratch");
    const job_type_inputs inputs = conformance_fixture::write_inputs(*entry, dir / "inputs");
    std::ofstream(dir / "inputs" / "sibling.txt") << "fratello\n";
    auto store = sqlite_dag_store::create(dir / "store");
    const minimal_job job = entry->make(inputs, entry->variants.at(0).name, store->root());
    store->put_node(job.node);
    store->create_slot("s", job.node.key());
    { std::ifstream marker(dir / "MARK_PRODUCT_START"); }
    memory_pool pool(std::uint64_t{256} << 20);
    cancel_token cancel;
    resource_config r;
    r.scratch = dir_quota{dir / "scratch", std::uint64_t{64} << 20};
    r.worker_count = 2;
    r.tile_rows = 1;
    const job_outcome out = job.run(job_type_env{*store, "s", pool, cancel, std::nullopt, r, "c", false});
    if (out.exit_code != 0) {
      std::fputs("raw_import_run: job fermato\n", stderr);
      return 1;
    }
    std::puts("raw_import_run ok");
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "raw_import_run: %s\n", e.what());
    return 2;
  }
}
