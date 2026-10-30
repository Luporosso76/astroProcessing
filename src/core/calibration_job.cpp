#include <algorithm>
#include <array>

#include "astro/calibration.hpp"
#include "astro/error.hpp"
#include "astro/master.hpp"
#include "astro/fits_blocks.hpp"
#include "astro/operations.hpp"
#include "astro/scheduler.hpp"
#include "calibration_internal.hpp"

namespace astro {

namespace {

const char* name_of(normalization_mode m) {
  return m == normalization_mode::global_median ? "global_median" : "cfa_channel_median";
}

const char* name_of(temperature_policy p) {
  return p == temperature_policy::compare ? "compare" : "not_applicable_declared";
}

const char* role_text(master_role r) {
  switch (r) {
    case master_role::bias: return "bias";
    case master_role::dark: return "dark";
    case master_role::dark_flat: return "dark_flat";
    case master_role::flat: return "flat";
  }
  ASTRO_ASSERT(false);
  return "";
}

json descriptor_json(const std::optional<master_descriptor>& d) {
  if (!d) return json::null();
  json o = json::object();
  o.set("role", json::string(role_text(d->role)));
  const auto text = [](auto v, auto fn) { return v ? json::string(fn(*v)) : json::null(); };
  o.set("additive", text(d->additive, [](additive_state s) {
          return s == additive_state::bias_included ? "bias_included"
                 : s == additive_state::bias_subtracted ? "bias_subtracted" : "unknown";
        }));
  o.set("scaling", text(d->scaling, [](scaling_state s) {
          return s == scaling_state::unscaled ? "unscaled" : s == scaling_state::scaled ? "scaled" : "unknown";
        }));
  o.set("flat", text(d->flat, [](flat_state s) {
          return s == flat_state::raw ? "raw" : s == flat_state::corrected_unnormalized ? "corrected_unnormalized"
                 : s == flat_state::calibrated ? "calibrated" : "unknown";
        }));
  o.set("combine_count", d->combine_count ? json::uinteger(*d->combine_count) : json::null());
  o.set("combine_method", text(d->combine, [](combine_method m) {
          return m == combine_method::mean ? "mean" : m == combine_method::median ? "median" : "other";
        }));
  // Campi dei master del prodotto: nei parametri solo se presenti, cosi' le chiavi dei nodi esistenti non cambiano.
  if (d->rejection) o.set("rejection", json::string(*d->rejection == master_rejection::none ? "none" : "sigma_clip"));
  if (d->estimator) o.set("variance_estimator", json::string("empirical"));
  return o;
}

json input_params(const calibration_input_spec& s) {
  json o = json::object();
  o.set("descriptor", descriptor_json(s.descriptor));
  o.set("declared_metadata", declared_metadata_json(s.declared));
  json fields = json::object();
  fields.set("binning_x", s.declared_fields.binning_x ? json::uinteger(*s.declared_fields.binning_x) : json::null());
  fields.set("binning_y", s.declared_fields.binning_y ? json::uinteger(*s.declared_fields.binning_y) : json::null());
  fields.set("unit", s.declared_fields.unit
                         ? json::string(*s.declared_fields.unit == pixel_unit::adu ? "adu"
                                        : *s.declared_fields.unit == pixel_unit::electron ? "electron" : "dimensionless")
                         : json::null());
  fields.set("linearity", s.declared_fields.lin
                              ? json::string(*s.declared_fields.lin == linearity::linear ? "linear" : "nonlinear")
                              : json::null());
  o.set("declared_fields", std::move(fields));
  json read = json::object();
  read.set("declared_bayer_x_offset",
           s.read_options.declared_bayer_x_offset ? json::uinteger(*s.read_options.declared_bayer_x_offset) : json::null());
  read.set("declared_bayer_y_offset",
           s.read_options.declared_bayer_y_offset ? json::uinteger(*s.read_options.declared_bayer_y_offset) : json::null());
  read.set("declared_phase", s.read_options.declared_phase ? json::string(to_string(*s.read_options.declared_phase)) : json::null());
  read.set("declared_linearity",
           s.read_options.declared_linearity ? json::string(to_string(*s.read_options.declared_linearity)) : json::null());
  o.set("fits_read_options", std::move(read));
  json var = json::object();
  var.set("source", json::string(*s.variance.source == variance_source::plane ? "plane" : "noise_model"));
  var.set("read_noise_adu", s.variance.read_noise_adu ? json::number(*s.variance.read_noise_adu) : json::null());
  o.set("variance", std::move(var));
  return o;
}

[[noreturn]] void fail(error_category c, const char* code, const char* message,
                       stop_error::context_list ctx = {}) {
  throw stop_error(c, code, message, std::move(ctx));
}

}  // namespace

node_spec make_calibration_node(const calibration_recipe& recipe) {
  require_fp_environment();  // prima dei confronti sui parametri (ADR-07, D23)
  validate_calibration_recipe(recipe);
  json params = json::object();
  params.set("normalization", json::string(name_of(*recipe.normalization)));
  params.set("q_min", json::number(*recipe.q_min));
  params.set("q_rel_sigma_max", json::number(*recipe.q_rel_sigma_max));
  params.set("exposure_rel_tol", json::number(*recipe.exposure_rel_tol));
  params.set("temperature_abs_tol_c", json::number(*recipe.temperature_abs_tol_c));
  params.set("temperature_policy", json::string(name_of(*recipe.temp_policy)));
  params.set("simd", json::string(simd_request_name(*recipe.simd)));
  json inputs = json::object();
  inputs.set("light", input_params(recipe.light));
  inputs.set("dark", input_params(recipe.dark));
  inputs.set("flat", input_params(recipe.flat));
  inputs.set("dark_flat", input_params(recipe.dark_flat));
  params.set("inputs", std::move(inputs));
  // Un master del prodotto entra dal suo nodo (ADR-16, aggiunte di R2): la calibrazione e' sua discendente nel DAG.
  const auto input_of = [](const char* name, const calibration_input_spec& s) {
    return s.master_node_key ? node_input::from_node(name, *s.master_node_key) : node_input::from_file(name, s.digest);
  };
  std::vector<node_input> node_inputs;
  node_inputs.push_back(input_of("light", recipe.light));
  node_inputs.push_back(input_of("dark", recipe.dark));
  node_inputs.push_back(input_of("flat", recipe.flat));
  node_inputs.push_back(input_of("dark_flat", recipe.dark_flat));
  return node_spec(calibration_operator_id, calibration_algorithm_version, calibration_serialization_version,
                   std::move(params), std::move(node_inputs), sample_type::f64,
                   backend_name(resolve_cpu_path(*recipe.simd, read_cpu_capabilities())), std::nullopt);
}

namespace {

constexpr std::uint64_t report_margin_bytes = std::uint64_t{1} << 20;
// Header di uscita: al piu' tre HDU con max_header_cards card ciascuno, piu' il riempimento dei blocchi.
constexpr std::uint64_t output_header_bound =
    3 * (std::uint64_t{fits_limits::max_header_cards} * 80 + 2 * 2880);

}  // namespace

job_outcome run_calibration_job(sqlite_dag_store& store, const std::string& slot_name,
                                const calibration_recipe& recipe, memory_pool& pool,
                                const calibration_job_options& options, const cancel_token& cancel,
                                const std::optional<budget_snapshot>& budget) {
  // Nodo della ricetta: lo costruisce e lo confronta con lo slot run_job_files, prima di `work`.
  std::optional<node_spec> requested;
  job_request request;
  request.node = [&] { return requested.emplace(make_calibration_node(recipe)); };
  request.mismatch_code = "calib.node_mismatch";
  if (options.resources) request.resources = &*options.resources;
  const auto work = [&](const job_record&) -> job_files_result {
    const node_spec& node = *requested;
    const std::array<const calibration_input_spec*, 4> specs = {&recipe.light, &recipe.dark, &recipe.flat,
                                                                &recipe.dark_flat};
    constexpr const char* names[4] = {"light", "dark", "flat", "dark_flat"};
    // Master del prodotto: descrittore derivato dal nodo e file uguale a un artefatto `primary` del nodo.
    for (std::size_t i = 0; i < specs.size(); ++i) {
      const calibration_input_spec& s = *specs[i];
      if (!s.master_node_key) continue;
      const std::optional<node_spec> master_node = store.get_node(*s.master_node_key);
      if (!master_node) {
        fail(error_category::graph, "dag.missing_node", "nodo del master assente dallo store",
             {{"input", names[i]}, {"nodo", *s.master_node_key}});
      }
      if (!(master_descriptor_from_node(*master_node) == *s.descriptor)) {
        fail(error_category::invalid_input, "calib.descriptor_mismatch",
             "il descrittore della ricetta differisce da quello derivato dal nodo del master",
             {{"input", names[i]}, {"nodo", *s.master_node_key}});
      }
      bool published = false;
      for (const artifact_record& a : store.artifacts_for(*s.master_node_key)) {
        published = published || (a.present && a.output_role == master_image_role && a.sha256 == s.digest.sha256 &&
                                  a.size_bytes == s.digest.size_bytes);
      }
      if (!published) {
        fail(error_category::integrity, "calib.master_artifact_mismatch",
             "il file dell'ingresso non e' un artefatto del nodo del master",
             {{"input", names[i]}, {"nodo", *s.master_node_key}, {"sha256", s.digest.sha256}});
      }
    }
    if (options.checkpoint && (!options.resources || !options.resources->scratch)) {
      fail(error_category::invalid_input, "calib.checkpoint_scratch",
           "il checkpoint del job richiede lo scratch nella configurazione delle risorse");
    }

    std::array<fits_plan, 4> plans;
    std::uint64_t input_peaks = 0;
    std::uint64_t block_raw = 0, block_band = 0, block_regions = 0;
    for (std::size_t i = 0; i < specs.size(); ++i) {
      fits_input input = open_fits_input(specs[i]->path);
      // Fase e linearita' dai soli header e dalla ricetta, prima di ogni lettura di campioni (ADR-03, R1).
      const header_peek peek = peek_header(input, specs[i]->read_options);
      require_calibration_phase(names[i], peek.phase);
      require_calibration_linearity(names[i], peek.lin, specs[i]->declared_fields.lin);
      plans[i] = plan_fits(input);
      input_peaks += plans[i].peak_bytes;
      if (options.resources) {
        const std::uint64_t e = sample_size(plans[i].type);
        const bool plane = *specs[i]->variance.source == variance_source::plane && plans[i].has_variance;
        block_raw += std::uint64_t{plans[i].width} *
                     (primary_sample_bytes(input) + (plans[i].has_invalid ? 2 : 0) + (plane ? e : 0));
        block_band += std::uint64_t{plans[i].width} * (e + 2 + (plane ? e : 0));
        block_regions += 1 + (plans[i].has_invalid ? 1 : 0) + (plane ? 1 : 0);
      }
    }
    if (options.hook) options.hook("plan.done");
    const fits_plan& lp = plans[0];
    if (lp.planes != 1) {
      fail(error_category::unsupported, "calib.plane_layout", "la ricetta M2 accetta solo mono e CFA",
           {{"input", "light"}, {"planes", std::to_string(lp.planes)}});
    }
    const std::uint64_t samples = lp.samples;
    const std::uint64_t out_elem = sample_size(lp.type);
    const std::uint64_t out_buffers = samples * (2 * out_elem + sizeof(std::uint16_t));

    job_plan plan;
    std::optional<resource_plan> chosen_plan;
    if (options.resources) {
      const std::size_t cpus = options.resources->cpu_set ? options.resources->cpu_set->size() : cpu_count();
      planner_input in{pool.budget_bytes(), cpus, lp.width, lp.height, out_elem, input_peaks, output_header_bound};
      in.block_raw_bytes_per_row = block_raw;
      in.block_band_bytes_per_row = block_band;
      in.block_regions = block_regions;
      in.normalization_channels = *recipe.normalization == normalization_mode::cfa_channel_median ? 4 : 1;
      in.checkpoint = options.checkpoint.has_value();
      chosen_plan = plan_resources(in, *options.resources);
      const resource_plan& chosen = *chosen_plan;
      if (chosen.spill) {
        job_files_result blocks = calib_detail::run_block_calibration(
            store, pool, calib_detail::block_job_request{recipe, node, chosen, *options.resources, plans, options}, cancel);
        blocks.plan = std::make_shared<const resource_plan>(chosen);
        return blocks;
      }
      plan.shared_bytes = chosen.shared_bytes;
      plan.per_task_bytes = chosen.per_task_bytes;
      plan.worker_count = chosen.worker_count;
      plan.queue_capacity = chosen.task_queue_capacity;
      plan.tile_rows = chosen.tile_rows;
    } else {
      plan.shared_bytes = input_peaks + out_buffers + (out_buffers + output_header_bound) +
                          samples * sizeof(double) + report_margin_bytes;
      plan.per_task_bytes = std::uint64_t{lp.width} * (2 * sizeof(double) + sizeof(std::uint16_t));
      plan.worker_count = options.worker_count;
      plan.queue_capacity = options.queue_capacity;
      plan.tile_rows = options.tile_rows;
    }

    std::optional<prepared_calibration> prepared;
    json observed = json::array();
    std::vector<stored_file> files;
    std::vector<std::string> warnings;
    const std::uint64_t tile_rows = plan.tile_rows == 0 ? 1 : plan.tile_rows;
    const std::uint64_t tasks = (std::uint64_t{lp.height} + tile_rows - 1) / tile_rows;

    job_spec spec(node, tasks, [&](task_context& ctx) -> std::string {
      pool_buffer scratch(ctx.memory, prepared->row_scratch_bytes());
      const std::uint32_t y0 = static_cast<std::uint32_t>(ctx.index * tile_rows);
      const std::uint32_t y1 = static_cast<std::uint32_t>(std::min<std::uint64_t>(lp.height, y0 + tile_rows));
      for (std::uint32_t y = y0; y < y1; ++y) {
        ctx.check_cancel();
        prepared->calibrate_rows(y, y + 1, scratch.data());
      }
      return prepared->rows_digest(y0, y1);
    });

    spec.prepare = [&](job_context& jc) {
      const std::uint64_t budget_bytes = jc.shared.bytes();
      std::array<image, 4> images;
      for (std::size_t i = 0; i < specs.size(); ++i) {
        jc.check_cancel();
        const std::vector<std::uint8_t> bytes = read_verified_fits(specs[i]->path, specs[i]->digest, budget_bytes);
        images[i] = decode_fits(bytes, budget_bytes, specs[i]->read_options);
        const fits_plan& planned = plans[i];
        if (bytes.size() != planned.file_bytes) {
          fail(error_category::integrity, "input.size_changed", "il file riletto ha dimensione diversa da quella pianificata",
               {{"input", names[i]}, {"pianificata", std::to_string(planned.file_bytes)}, {"riletta", std::to_string(bytes.size())}});
        }
        const image_geometry& g = images[i].geometry;
        if (g.width != planned.width || g.height != planned.height || g.planes != planned.planes) {
          fail(error_category::integrity, "input.geometry_changed", "la geometria riletta differisce da quella del piano",
               {{"input", names[i]}, {"pianificata", std::to_string(planned.width) + "x" + std::to_string(planned.height)},
                {"riletta", std::to_string(g.width) + "x" + std::to_string(g.height)}});
        }
      }
      prepared.emplace(prepare_calibration(
          recipe, calibration_images{std::move(images[0]), std::move(images[1]), std::move(images[2]),
                                     std::move(images[3])}));
      observed = prepared->observed_inputs();
      if (options.hook) options.hook("preflight.done");
      for (std::size_t i = 0; i < specs.size(); ++i) {
        jc.check_cancel();
        try {
          (void)read_verified_fits(specs[i]->path, specs[i]->digest, budget_bytes);
        } catch (stop_error& e) {
          e.add_context("fase", "riverifica dopo il preflight");
          e.add_context("input", names[i]);
          throw;
        }
      }
    };

    spec.finalize = [&](job_context&) {
      warnings = prepared->warnings();
      calibration_result result = finish_calibration(std::move(*prepared));
      prepared.reset();
      const std::vector<std::uint8_t> fits = encode_fits(result.output);
      const content_digest fits_digest = digest_of(fits);
      json image_out = json::object();
      image_out.set("sha256", json::string(fits_digest.sha256));
      image_out.set("size_bytes", json::uinteger(fits_digest.size_bytes));
      image_out.set("unit", json::string("adu"));
      json outputs = json::object();
      outputs.set("calibrated_image", std::move(image_out));
      result.report.set("outputs", std::move(outputs));
      const std::string report_json = canonical_json(result.report);
      const std::string report_text = calibration_report_text(result.report);
      const auto span = [](const std::string& s) {
        return std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
      };
      files = store.write_files(
          {artifact_content{fits, "fits", "image", std::nullopt, calibration_image_role},
           artifact_content{span(report_json), "json", "calibration_report", std::nullopt, calibration_report_role},
           artifact_content{span(report_text), "txt", "calibration_report_text", std::nullopt,
                            calibration_report_text_role}});
    };

    if (options.on_job_plan) options.on_job_plan(plan);
    scheduler_tuning tuning = options.resources ? scheduler_tuning_of(*options.resources) : scheduler_tuning{};
    tuning.pressure = options.pressure;
    job_scheduler scheduler(pool, system_affinity, std::move(tuning));
    const job_report report = scheduler.run(plan, spec, cancel);
    json details = json::object();
    details.set("observed_inputs", std::move(observed));
    if (chosen_plan) details.set("scheduler", calib_detail::scheduler_details(report));
    if (options.pressure) details.set("admission", calib_detail::admission_details(report));
    job_files_result result{std::move(files), std::move(warnings), std::move(details)};
    if (chosen_plan) result.plan = std::make_shared<const resource_plan>(*chosen_plan);
    return result;
  };
  job_outcome outcome = run_job_files(store, slot_name, work, budget, &cancel, request);
  // Checkpoint e staging restano dopo stop e annullamento (stato del resume); dopo la pubblicazione si rimuovono.
  if (outcome.exit_code == 0 && options.checkpoint) {
    remove_checkpoint(options.resources->scratch->dir, options.checkpoint->name);
    remove_named_spill_file(options.resources->scratch->dir, options.checkpoint->name + ".stage");
  }
  return outcome;
}

}  // namespace astro
