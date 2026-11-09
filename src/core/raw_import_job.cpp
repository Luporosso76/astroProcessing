#include "astro/raw_import_job.hpp"

#include <algorithm>
#include <memory>

#include "astro/fits.hpp"
#include "astro/input_format.hpp"
#include "astro/inventory.hpp"
#include "astro/job_probe.hpp"
#include "astro/raw_decode.hpp"
#include "astro/scheduler.hpp"

namespace astro {

namespace {

constexpr std::uint32_t raw_import_algorithm_version = 1;
constexpr std::uint32_t raw_import_serialization_version = 1;
constexpr const char* raw_import_backend = "cpu_scalar";
// Stima non misurata dell'header FITS dell'immagine pubblicata: 16 blocchi di 2880 byte, riservati con i campioni.
constexpr std::uint64_t fits_header_reserve_bytes = 16 * 2880;

// Byte del file letti da `input` e verificati contro `digest` (OB-11); nessun header si legge prima del digest.
std::vector<std::uint8_t> read_raw_bytes(const fits_input& input, const std::string& path, const content_digest& digest) {
  return read_verified_input(input, path, digest, input.size_bytes(), nullptr);
}

}  // namespace

node_spec make_raw_import_node(const content_digest& digest, const json& record) {
  if (record.type() != json::kind::object && record.type() != json::kind::null)
    throw stop_error(error_category::invalid_input, "dag.bad_spec", "record del decoder non valido");
  json params = json::object();
  params.set("decoder", record);
  params.set("report_version", json::uinteger(1));
  std::vector<node_input> inputs;
  inputs.push_back(node_input::from_file("file", digest));
  return node_spec(raw_import_operator_id, raw_import_algorithm_version, raw_import_serialization_version,
                   std::move(params), std::move(inputs), sample_type::f32, raw_import_backend, std::nullopt);
}

raw_import_request make_raw_import_request(std::string path, content_digest digest) {
  try {
    const fits_input input = open_input(path, raw_input_format());
    const std::vector<std::uint8_t> bytes = read_raw_bytes(input, path, digest);
    const json record = raw_decoder_record(decode_raw_buffer(bytes));
    node_spec node = make_raw_import_node(digest, record);
    return {std::move(path), std::move(digest), std::move(node), std::nullopt};
  } catch (const stop_error& stop) {
    // Nodo con record nullo: l'arresto non si perde, il job lo rilancia identico prima di aprire il file.
    node_spec node = make_raw_import_node(digest, json::null());
    return {std::move(path), std::move(digest), std::move(node), stop};
  }
}

branch_files_job make_raw_import_branch_job(sqlite_dag_store& store, const std::string& slot_name,
                                            const raw_import_request& request, memory_pool& pool,
                                            const cancel_token& cancel, const resource_config* resources) {
  const auto work = [&store, &request, &pool, &cancel, resources](const job_record&) -> job_files_result {
    if (request.identification_stop) throw *request.identification_stop;
    const fits_input input = open_input(request.path, raw_input_format());
    const std::uint64_t shared_bytes = digest_reserve_bytes(input.size_bytes());
    std::optional<image> decoded;
    std::uint64_t decoder_reserved = 0;
    job_plan plan;
    plan.shared_bytes = shared_bytes;
    plan.per_task_bytes = input.size_bytes();  // i byte del file; la decodifica prenota il resto dal pool (§2.8)
    plan.tile_rows = 1;
    job_spec spec(request.node, 1, [&](task_context& tc) {
      tc.check_cancel();
      std::vector<std::uint8_t> bytes = read_raw_bytes(input, request.path, request.digest);
      tc.check_cancel();
      if (job_probe::active()) job_probe::notify("input.samples", "sha256:" + sha256_hex(bytes));
      raw_decode_hooks hooks;
      hooks.before_unpack_plan = [&decoder_reserved](const raw_unpack_plan& p) { decoder_reserved = p.reserved_bytes; };
      raw_decode_context ctx{&pool, &tc.job.cancel, raw_limits::max_pixels, shared_bytes + plan.per_task_bytes};
      decoded.emplace(decode_raw_buffer(bytes, hooks, ctx));
      return std::string();
    });
    spec.prepare = [&](job_context& jc) {
      jc.check_cancel();
      require_input_digest(digest_fits_input(input, jc.shared, &jc.cancel), request.digest, request.path);
    };
    spec.finalize = [&](job_context& jc) {
      pool_buffer work_buffer(jc.shared, static_cast<std::size_t>(shared_bytes));
      reverify_input(request.path, raw_input_format(), &input, request.digest, {work_buffer.data(), work_buffer.size()},
                     [&jc] { jc.check_cancel(); });
    };
    job_scheduler scheduler(pool, system_affinity, resources ? scheduler_tuning_of(*resources) : scheduler_tuning{});
    (void)scheduler.run(plan, spec, cancel);

    image img = std::move(*decoded);
    const json record = raw_decoder_record(img);
    const node_spec built = make_raw_import_node(request.digest, record);
    if (built.key() != request.node.key()) {
      throw stop_error(error_category::invalid_input, "raw_import.node_mismatch",
                       "il record del decoder non è quello del nodo dello slot",
                       {{"nodo_slot", request.node.key()}, {"nodo_richiesta", built.key()}});
    }
    // Lo scrittore FITS non rappresenta `decoder_state` (ADR-03.b §b.2): il suo contenuto è nel record di provenance.
    img.decoder_state.reset();
    img.measure = merge_measure({}, {});
    const std::uint64_t sample_bytes = img.samples.size();
    const std::uint64_t output_bytes = 2 * sample_bytes + fits_header_reserve_bytes;
    reservation output = pool.reserve(output_bytes, "raw_import.output", &cancel);
    const std::vector<std::uint8_t> fits = encode_fits(img);

    job_files_result result;
    result.files = store.write_files({artifact_content{fits, "fits", "image", std::nullopt, raw_import_image_role}});
    json details = json::object();
    details.set("decoder", record);
    result.details = std::move(details);
    auto executed = std::make_shared<resource_plan>();
    executed->tile_rows = 1;
    executed->task_count = 1;
    executed->shared_bytes = shared_bytes;
    executed->per_task_bytes = plan.per_task_bytes;
    // Picco nel pool: job e decoder (§2.8) insieme, poi l'uscita da sola (si prenota dopo lo scheduler).
    executed->peak_bytes = std::max(shared_bytes + plan.per_task_bytes + decoder_reserved, output_bytes);
    executed->working_set_bytes = executed->peak_bytes;
    executed->minimum_feasible_bytes = executed->peak_bytes;
    result.plan = std::move(executed);
    return result;
  };
  job_request jr;
  jr.node = [&request] { return request.node; };
  jr.mismatch_code = "raw_import.node_mismatch";
  jr.resources = resources;
  return branch_files_job{slot_name, work, jr};
}

job_outcome run_raw_import_job(sqlite_dag_store& store, const std::string& slot_name, const raw_import_request& request,
                               memory_pool& pool, const cancel_token& cancel, const std::optional<budget_snapshot>& budget,
                               const resource_config* resources) {
  const branch_files_job job = make_raw_import_branch_job(store, slot_name, request, pool, cancel, resources);
  return run_job_files(store, slot_name, job.work, budget, &cancel, job.request);
}

}  // namespace astro
