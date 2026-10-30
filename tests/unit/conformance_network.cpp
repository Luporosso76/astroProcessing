// T-M3BASE-18: un job riuscito per ogni voce e variante del registro dei tipi di job, in un solo progetto, eseguibile
// da riga di comando sotto strace (tools/check_m2f02_network.sh).
// Uso: conformance_network <directory di lavoro nuova>. Uscita 0 solo se ogni job pubblica.
#include <cstdio>
#include <filesystem>

#include "astro/job_types.hpp"
#include "conformance_inputs.hpp"

int main(int argc, char** argv) {
  namespace fs = std::filesystem;
  using namespace astro;
  if (argc != 2) {
    std::fputs("uso: conformance_network <directory di lavoro>\n", stderr);
    return 1;
  }
  try {
    const fs::path dir = fs::absolute(argv[1]);
    if (fs::exists(dir)) {
      std::fputs("conformance_network: la directory di lavoro esiste gia'\n", stderr);
      return 1;
    }
    fs::create_directories(dir / "scratch");
    auto store = sqlite_dag_store::create(dir / "store");
    memory_pool pool(std::uint64_t{256} << 20);
    cancel_token cancel;
    const auto& types = registered_job_types();
    for (std::size_t i = 0; i < types.size(); ++i) {
      for (std::size_t v = 0; v < types[i].variants.size(); ++v) {
        const std::string tag = std::to_string(i) + "_" + std::to_string(v);
        const job_type_inputs in = conformance_fixture::write_inputs(types[i], dir / ("inputs" + tag));
        const minimal_job job = types[i].make(in, types[i].variants[v].name, store->root());
        store->put_node(job.node);
        store->create_slot("t" + tag, job.node.key());
        resource_config r;
        r.scratch = dir_quota{dir / "scratch", std::uint64_t{64} << 20};
        r.worker_count = 2;
        r.tile_rows = 1;
        const job_outcome out = job.run(job_type_env{*store, "t" + tag, pool, cancel, std::nullopt, r, "c" + tag, false});
        if (out.exit_code != 0) {
          std::fprintf(stderr, "conformance_network: job %s/%s fermato\n", types[i].operator_id,
                       types[i].variants[v].name.c_str());
          return 1;
        }
      }
    }
    std::puts("conformance_network ok");
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "conformance_network: %s\n", e.what());
    return 2;
  }
}
