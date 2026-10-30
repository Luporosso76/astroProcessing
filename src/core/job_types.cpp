#include "astro/job_types.hpp"

#include "astro/calibration.hpp"
#include "astro/import_job.hpp"
#include "astro/master.hpp"
#include "astro/thumbnail_cache.hpp"

namespace astro {

namespace {

constexpr std::uint64_t minimal_cache_quota = std::uint64_t{64} << 20;
const preview_request minimal_preview{2, 2};

job_type_input input_of(const job_type_inputs& inputs, const std::string& role) {
  const auto it = inputs.find(role);
  if (it == inputs.end()) {
    throw stop_error(error_category::invalid_input, "job_types.input_missing", "input del ruolo assente",
                     {{"ruolo", role}});
  }
  return it->second;
}

[[noreturn]] void unknown_variant(const char* op, const std::string& variant) {
  throw stop_error(error_category::invalid_input, "job_types.variant", "variante non dichiarata",
                   {{"operatore", op}, {"variante", variant}});
}

void require_single(const char* op, const std::string& variant) {
  if (variant != "unica") unknown_variant(op, variant);
}

// Risorse dei job di I/O: la variante unica non dichiara scratch.
resource_config io_resources(const job_type_env& env) {
  resource_config r = env.resources;
  r.scratch.reset();
  return r;
}

minimal_job make_import(const job_type_inputs& inputs, const std::string& variant, const std::filesystem::path&) {
  require_single(import_operator_id, variant);
  const job_type_input in = input_of(inputs, "file");
  const import_request request{in.path, in.digest, {}, nullptr};
  return {make_import_node(request), [request](const job_type_env& env) {
            const resource_config resources = io_resources(env);
            return run_import_job(env.store, env.slot_name, request, env.pool, env.cancel, env.budget, &resources);
          }};
}

minimal_job make_thumbnail(const job_type_inputs& inputs, const std::string& variant, const std::filesystem::path&) {
  require_single(thumbnail_operator_id, variant);
  const job_type_input in = input_of(inputs, "file");
  return {make_thumbnail_node(in.digest, minimal_preview), [in](const job_type_env& env) {
            thumbnail_cache cache(env.store.root(), cache_options{minimal_cache_quota, {}});
            const resource_config resources = io_resources(env);
            return run_thumbnail_job(env.store, env.slot_name, in.path, in.digest, minimal_preview, cache, env.pool,
                                     env.cancel, env.budget, {}, &resources);
          }};
}

minimal_job make_cache_rebuild(const job_type_inputs& inputs, const std::string& variant,
                               const std::filesystem::path& project_root) {
  require_single(cache_rebuild_operator_id, variant);
  const job_type_input in = input_of(inputs, "file");
  {  // Precondizione del tipo: la voce da ricostruire esiste nella cache del progetto.
    memory_pool pool(minimal_cache_quota);
    thumbnail_cache cache(project_root, cache_options{minimal_cache_quota, {}});
    const fits_input input = open_fits_input(in.path);
    (void)cache.get(input, in.digest, minimal_preview, pool);
  }
  return {make_cache_rebuild_node(in.digest, minimal_preview, cache_rebuild_cause_user), [in](const job_type_env& env) {
            thumbnail_cache cache(env.store.root(), cache_options{minimal_cache_quota, {}});
            const resource_config resources = io_resources(env);
            return run_cache_rebuild_job(env.store, env.slot_name, in.path, in.digest, minimal_preview,
                                         cache_rebuild_cause_user, cache, env.pool, env.cancel, env.budget, {},
                                         &resources);
          }};
}

master_descriptor minimal_descriptor(master_role role) {
  master_descriptor d;
  d.role = role;
  if (role == master_role::flat) {
    d.flat = flat_state::raw;
  } else {
    d.additive = additive_state::bias_included;
    d.scaling = scaling_state::unscaled;
  }
  d.combine_count = 4;
  d.combine = combine_method::mean;
  return d;
}

calibration_input_spec calibration_input(const job_type_input& in, std::optional<master_role> role) {
  calibration_input_spec s;
  s.path = in.path;
  s.digest = in.digest;
  if (role) s.descriptor = minimal_descriptor(*role);
  s.variance.source = variance_source::noise_model;
  s.variance.read_noise_adu = 2.0;
  return s;
}

minimal_job make_calibration(const job_type_inputs& inputs, const std::string& variant, const std::filesystem::path&) {
  const bool blocks = variant == "blocchi";
  if (!blocks && variant != "intera") unknown_variant(calibration_operator_id, variant);
  calibration_recipe r;
  r.light = calibration_input(input_of(inputs, "light"), std::nullopt);
  r.dark = calibration_input(input_of(inputs, "dark"), master_role::dark);
  r.flat = calibration_input(input_of(inputs, "flat"), master_role::flat);
  r.dark_flat = calibration_input(input_of(inputs, "dark_flat"), master_role::dark_flat);
  r.normalization = normalization_mode::global_median;
  r.q_min = 0.01;
  r.q_rel_sigma_max = 10.0;
  r.exposure_rel_tol = 1e-9;
  r.temperature_abs_tol_c = 0.5;
  r.temp_policy = temperature_policy::compare;
  r.simd = simd_request::scalar;
  return {make_calibration_node(r), [r, blocks](const job_type_env& env) {
            calibration_job_options o;
            o.resources = env.resources;
            // Residenza dichiarata prima dell'esecuzione: il checkpoint implica i blocchi; senza scratch plan_resources
            // non puo' scegliere i blocchi e la residenza e' intera.
            if (blocks) {
              o.checkpoint = calibration_job_options::checkpoint_options{env.checkpoint_name, 1, env.resume};
            } else {
              o.resources->scratch.reset();
            }
            return run_calibration_job(env.store, env.slot_name, r, env.pool, o, env.cancel, env.budget);
          }};
}

minimal_job make_master(const job_type_inputs& inputs, const std::string& variant, const std::filesystem::path&) {
  require_single(master_operator_id, variant);
  master_recipe r;
  r.role = master_role::dark;
  for (const char* role : {"frame0", "frame1", "frame2"}) {
    const job_type_input in = input_of(inputs, role);
    master_frame_spec f;
    f.path = in.path;
    f.digest = in.digest;
    r.frames.push_back(f);
  }
  r.combine = combine_method::mean;
  r.rejection = master_rejection::none;
  r.exposure_rel_tol = 1e-9;
  r.temperature_abs_tol_c = 0.5;
  r.temp_policy = temperature_policy::compare;
  r.simd = simd_request::scalar;
  return {make_master_node(r), [r](const job_type_env& env) {
            master_job_options o;
            o.resources = env.resources;
            return run_master_job(env.store, env.slot_name, r, env.pool, o, env.cancel, env.budget);
          }};
}

}  // namespace

const std::vector<job_type_entry>& registered_job_types() {
  static const std::vector<job_type_entry> entries{
      {import_operator_id, {"file"}, {{"unica", false, false}}, false, make_import},
      {thumbnail_operator_id, {"file"}, {{"unica", false, false}}, false, make_thumbnail},
      {cache_rebuild_operator_id, {"file"}, {{"unica", false, false}}, false, make_cache_rebuild},
      {calibration_operator_id,
       {"light", "dark", "flat", "dark_flat"},
       {{"intera", false, false}, {"blocchi", true, true}},
       true,
       make_calibration},
      {master_operator_id, {"frame0", "frame1", "frame2"}, {{"unica", false, true}}, true, make_master},
  };
  return entries;
}

}  // namespace astro
