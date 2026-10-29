// Workflow M2.e completo su dati sintetici, eseguibile da riga di comando: scrive i frame, esegue il job di
// calibrazione su uno store durevole con pool e scheduler, rilegge i risultati. Usato da T-M2F-02 sotto strace.
// Uso: m2e_workflow <directory di lavoro nuova>. Uscita 0 solo se ogni verifica passa.
#include <cstdio>
#include <cstdlib>

#include "calib_support.hpp"

namespace {

using namespace cal;

int fail(const char* what) {
  std::fprintf(stderr, "m2e_workflow: %s\n", what);
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return fail("uso: m2e_workflow <directory di lavoro>");
  try {
    const fs::path dir = argv[1];
    if (fs::exists(dir)) return fail("la directory di lavoro esiste gia'");
    fs::create_directories(dir);
    const set_def s = synth_set(64, 48, -32);
    calibration_recipe recipe = recipe_of(s);
    const std::pair<const char*, const frame_def*> frames[4] = {
        {"light.fits", &s.light}, {"dark.fits", &s.dark}, {"flat.fits", &s.flat}, {"dark_flat.fits", &s.dark_flat}};
    calibration_input_spec* specs[4] = {&recipe.light, &recipe.dark, &recipe.flat, &recipe.dark_flat};
    for (int i = 0; i < 4; ++i) {
      write_bytes(dir / frames[i].first, file_of(*frames[i].second, s.w, s.h));
      specs[i]->path = (dir / frames[i].first).string();
    }

    auto store = sqlite_dag_store::create(dir / "store");
    const node_spec node = make_calibration_node(recipe);
    store->put_node(node);
    store->create_slot("cal", node.key());
    memory_pool pool(std::uint64_t{256} << 20);
    calibration_job_options options;
    options.worker_count = 2;
    options.tile_rows = 5;
    cancel_token cancel;
    const job_outcome out = run_calibration_job(*store, "cal", recipe, pool, options, cancel);
    if (out.exit_code != 0) return fail("job di calibrazione fermato");
    if (store->artifacts_for(node.key()).size() != 3) return fail("artefatti attesi: immagine, report, report testuale");
    const slot_view view = store->read_slot("cal");
    if (view.state != freshness::current || !view.artifact) return fail("slot non corrente");
    const std::string text = read_text(store->artifact_path(view.artifact->rel_path));
    const image decoded = decode_fits(std::vector<std::uint8_t>(text.begin(), text.end()), big_budget);
    const calibration_result memory = calibrate(recipe, images_of(s));
    if (!equivalent_content(decoded, memory.output)) return fail("l'immagine pubblicata differisce dal calcolo in memoria");
    if (!operator_output_invariants_hold(decoded)) return fail("invarianti dell'uscita violate");
    std::puts("m2e_workflow ok");
    return 0;
  } catch (const astro::stop_error& e) {
    std::fprintf(stderr, "m2e_workflow: %s\n", e.what());
    return 2;
  }
}
