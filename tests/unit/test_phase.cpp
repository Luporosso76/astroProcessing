// R1: fase e linearita' nella calibrazione (ADR-03, aggiunte di R1). T-M2R1-05 (calibrazione), T-M2R1-07 (calibrazione).
#include <gtest/gtest.h>

#include <limits>

#include "astro/hash.hpp"
#include "astro/operations.hpp"
#include "calib_support.hpp"

namespace {

using namespace cal;

std::string ctx(const stop_error& e, const std::string& key) {
  for (const auto& [k, v] : e.context()) {
    if (k == key) return v;
  }
  return "";
}

frame_def* frame_named(set_def& s, const std::string& name) {
  if (name == "light") return &s.light;
  if (name == "dark") return &s.dark;
  if (name == "flat") return &s.flat;
  return &s.dark_flat;
}

TEST(PhaseCalib, T_M2R1_05_calibration_stops_on_a_wrong_phase_before_computing) {
  for (const char* name : {"light", "dark", "flat", "dark_flat"}) {
    for (const char* phase : {"calibrated", "prestacked"}) {
      SCOPED_TRACE(std::string(name) + "/" + phase);
      set_def s = synth_set(8, 4, -64);
      frame_named(s, name)->hdr.phase = phase;
      try {
        (void)calibrate(recipe_of(s), images_of(s));
        ADD_FAILURE() << "stop atteso";
      } catch (const stop_error& e) {
        EXPECT_EQ(e.category(), error_category::incompatible);
        EXPECT_EQ(e.code(), "op.phase_blocked");
        EXPECT_EQ(ctx(e, "input"), name);
        EXPECT_EQ(ctx(e, "fase"), phase);
      }
    }
  }
  // Fase assente: nessun valore di comodo.
  for (const char* name : {"light", "dark", "flat", "dark_flat"}) {
    SCOPED_TRACE(name);
    set_def s = synth_set(8, 4, -64);
    frame_named(s, name)->hdr.phase.clear();
    try {
      (void)calibrate(recipe_of(s), images_of(s));
      ADD_FAILURE() << "stop atteso";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.category(), error_category::metadata_missing);
      EXPECT_EQ(e.code(), "calib.metadata.phase");
      EXPECT_EQ(ctx(e, "input"), name);
    }
  }
  // La fase si controlla prima della linearita'; la linearita' non nota o NL ferma con i codici propri di M2.e.
  {
    set_def s = synth_set(8, 4, -64);
    s.light.hdr.phase = "calibrated";
    s.light.hdr.linear_value = false;
    EXPECT_STOP(calibrate(recipe_of(s), images_of(s)), error_category::incompatible, "op.phase_blocked");
  }
  {
    set_def s = synth_set(8, 4, -64);
    s.dark.hdr.linear_value = false;
    EXPECT_STOP(calibrate(recipe_of(s), images_of(s)), error_category::unsupported, "calib.linearity");
  }
  {
    set_def s = synth_set(8, 4, -64);
    s.flat.hdr.with_linear = false;
    EXPECT_STOP(calibrate(recipe_of(s), images_of(s)), error_category::metadata_missing, "calib.metadata.linearity");
  }
}

TEST(PhaseCalib, T_M2R1_05_declared_phase_conflicts_with_the_header_and_the_output_is_calibrated) {
  set_def s = synth_set(8, 4, -64);
  s.light.hdr.phase.clear();
  fits_read_options declared;
  declared.declared_phase = process_phase::raw;
  calibration_images im = images_of(s);
  im.light = decode(s.light, s.w, s.h, declared);
  calibration_recipe rec = recipe_of(s);
  rec.light.read_options = declared;
  const calibration_result r = calibrate(rec, std::move(im));
  const json& light = at(r.report, {"inputs", "light", "image_fields"});
  EXPECT_EQ(at(light, {"phase", "value"}).as_string(), "raw");
  EXPECT_EQ(at(light, {"phase", "origin"}).as_string(), "declared");
  EXPECT_EQ(at(r.report, {"inputs", "dark", "image_fields", "phase", "origin"}).as_string(), "header");
  ASSERT_TRUE(r.output.phase.has_value());
  EXPECT_EQ(r.output.phase->value, process_phase::calibrated);
  EXPECT_EQ(r.output.lin, linearity::linear);
  const image back = decode_fits(encode_fits(r.output), big_budget);
  ASSERT_TRUE(back.phase.has_value());
  EXPECT_EQ(back.phase->value, process_phase::calibrated);
  EXPECT_EQ(back.phase->source.origin, metadata_origin::header);
  EXPECT_NE(make_calibration_node(rec).key(), make_calibration_node(recipe_of(s)).key());

  fits_read_options conflict;
  conflict.declared_phase = process_phase::calibrated;
  set_def tagged = synth_set(8, 4, -64);
  EXPECT_STOP(decode(tagged.light, tagged.w, tagged.h, conflict), error_category::incompatible, "metadata.declared_conflict");
}

TEST(PhaseCalib, T_M2R1_07_calibration_report_lists_the_uninterpreted_keywords_of_each_input) {
  set_def s = synth_set(8, 4, -64);
  s.light.hdr.extra = {fx::card_str("OBSERVER", "Rossi"), fx::card_int("NOTE_A", 1), fx::card_int("NOTE_A", 2)};
  s.flat.hdr.extra = {fx::card_real("SITELAT", 45.5)};
  const calibration_result r = calibrate(recipe_of(s), images_of(s));
  const auto list = [&](const char* input) {
    std::vector<std::string> out;
    for (const json& k : at(r.report, {"inputs", input, "uninterpreted_keywords"}).items()) out.push_back(k.as_string());
    return out;
  };
  EXPECT_EQ(list("light"), (std::vector<std::string>{"OBSERVER", "NOTE_A"}));
  EXPECT_EQ(list("flat"), (std::vector<std::string>{"SITELAT"}));
  EXPECT_TRUE(list("dark").empty());
  EXPECT_TRUE(list("dark_flat").empty());
  EXPECT_NE(r.report_text.find("uninterpreted_keywords"), std::string::npos);
  EXPECT_NE(r.report_text.find("OBSERVER"), std::string::npos);
}

TEST(PhaseCalib, T_M2R1_07_calibration_job_provenance_lists_the_observed_inputs) {
  set_def s = synth_set(8, 4, -64);
  s.light.hdr.extra = {fx::card_str("OBSERVER", "Rossi")};
  s.dark.hdr.with_linear = false;
  const fs::path dir = fresh_dir("r1_phase_job");
  calibration_recipe recipe = recipe_of(s);
  recipe.dark.read_options.declared_linearity = linearity_class::linear;
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
  memory_pool pool(256 * (std::uint64_t{1} << 20));
  const cancel_token cancel;
  testing::internal::CaptureStderr();
  const job_outcome out = run_calibration_job(*store, "cal", recipe, pool, {}, cancel);
  testing::internal::GetCapturedStderr();
  ASSERT_EQ(out.exit_code, 0);
  ASSERT_TRUE(out.provenance.details.has_value());
  const json& observed = *out.provenance.details->find("observed_inputs");
  ASSERT_EQ(observed.items().size(), 4u);
  const char* names[4] = {"light", "dark", "flat", "dark_flat"};
  for (std::size_t i = 0; i < 4; ++i) {
    const json& o = observed.items()[i];
    EXPECT_EQ(o.find("input_name")->as_string(), names[i]);
    EXPECT_EQ(o.find("sha256")->as_string(), specs[i]->digest.sha256);
    EXPECT_EQ(o.find("phase")->find("value")->as_string(), "raw");
    EXPECT_EQ(o.find("linearity")->find("value")->as_string(), "linear");
    EXPECT_EQ(o.find("linearity")->find("origin")->as_string(), i == 1 ? "declared" : "header");
    EXPECT_EQ(o.find("uninterpreted_keywords")->items().size(), i == 0 ? 1u : 0u);
  }
  const json& light_keywords = *observed.items()[0].find("uninterpreted_keywords");
  ASSERT_EQ(light_keywords.items().size(), 1u);
  EXPECT_EQ(light_keywords.items()[0].as_string(), "OBSERVER");
  // La stessa osservazione e' nella provenance persistita.
  json persisted;
  for (const artifact_record& a : store->artifacts_for(node.key())) {
    if (a.output_role == "provenance") persisted = parse_json(read_text(store->artifact_path(a.rel_path)));
  }
  ASSERT_NE(persisted.find("details"), nullptr);
  EXPECT_EQ(canonical_json(*persisted.find("details")->find("observed_inputs")), canonical_json(observed));
  const json& persisted_observed = *persisted.find("details")->find("observed_inputs");
  EXPECT_EQ(persisted_observed.items()[1].find("linearity")->find("origin")->as_string(), "declared");
  EXPECT_EQ(persisted_observed.items()[0].find("linearity")->find("origin")->as_string(), "header");
}


TEST(PhaseCalib, T_M2R1_05_declared_linearity_keeps_its_origin_in_report_and_provenance) {
  set_def s = synth_set(8, 4, -64);
  s.dark.hdr.with_linear = false;
  fits_read_options declared;
  declared.declared_linearity = linearity_class::linear;
  calibration_images im = images_of(s);
  im.dark = decode(s.dark, s.w, s.h, declared);
  calibration_recipe rec = recipe_of(s);
  rec.dark.read_options = declared;
  const calibration_result r = calibrate(rec, std::move(im));
  EXPECT_EQ(at(r.report, {"inputs", "dark", "image_fields", "linearity", "origin"}).as_string(), "declared");
  EXPECT_EQ(at(r.report, {"inputs", "light", "image_fields", "linearity", "origin"}).as_string(), "header");
  EXPECT_EQ(at(r.report, {"inputs", "dark", "image_fields", "linearity", "value"}).as_string(), "linear");
}

TEST(PhaseCalib, T_M2R1_05_the_job_checks_the_phase_from_the_headers_before_any_sample_is_read) {
  set_def s = synth_set(8, 4, -64);
  s.dark.hdr.phase = "prestacked";
  const fs::path dir = fresh_dir("r1_phase_job_bad");
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
  memory_pool pool(256 * (std::uint64_t{1} << 20));
  const cancel_token cancel;
  std::vector<std::string> points;
  calibration_job_options options;
  options.hook = [&](std::string_view p) { points.emplace_back(p); };
  const std::uint64_t allocated = allocated_sample_bytes();
  testing::internal::CaptureStderr();
  const job_outcome out = run_calibration_job(*store, "cal", recipe, pool, options, cancel);
  testing::internal::GetCapturedStderr();
  EXPECT_EQ(out.exit_code, 2);
  ASSERT_TRUE(out.stop);
  EXPECT_EQ(out.stop->category(), error_category::incompatible);
  EXPECT_EQ(out.stop->code(), "op.phase_blocked");
  EXPECT_EQ(ctx(*out.stop, "input"), "dark");
  EXPECT_TRUE(points.empty()) << "nessun punto del job raggiunto";
  EXPECT_EQ(allocated_sample_bytes(), allocated) << "nessun campione allocato o decodificato";
}


TEST(PhaseCalib, T_M2R1_05_the_job_checks_the_linearity_from_headers_and_recipe_before_any_sample_is_read) {
  struct variant {
    const char* name;
    std::function<void(set_def&, calibration_recipe&)> arrange;
    error_category category;
    const char* code;
  };
  const std::vector<variant> variants = {
      {"nonlinear_dark", [](set_def& s, calibration_recipe&) { s.dark.hdr.linear_value = false; }, error_category::unsupported, "calib.linearity"},
      {"unknown_flat", [](set_def& s, calibration_recipe&) { s.flat.hdr.with_linear = false; }, error_category::metadata_missing, "calib.metadata.linearity"},
      {"declared_nonlinear_over_linear_header", [](set_def&, calibration_recipe& r) { r.dark_flat.declared_fields.lin = linearity::nonlinear; }, error_category::incompatible, "metadata.declared_conflict"},
      {"declared_nonlinear_on_unknown_light", [](set_def& s, calibration_recipe& r) { s.light.hdr.with_linear = false; r.light.declared_fields.lin = linearity::nonlinear; }, error_category::unsupported, "calib.linearity"},
  };
  for (const variant& v : variants) {
    SCOPED_TRACE(v.name);
    set_def s = synth_set(8, 4, -64);
    calibration_recipe recipe = recipe_of(s);
    v.arrange(s, recipe);
    const fs::path dir = fresh_dir(std::string("r1_lin_job_") + v.name);
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
    memory_pool pool(256 * (std::uint64_t{1} << 20));
    const cancel_token cancel;
    std::vector<std::string> points;
    calibration_job_options options;
    options.hook = [&](std::string_view p) { points.emplace_back(p); };
    const std::uint64_t allocated = allocated_sample_bytes();
    testing::internal::CaptureStderr();
    const job_outcome out = run_calibration_job(*store, "cal", recipe, pool, options, cancel);
    testing::internal::GetCapturedStderr();
    ASSERT_TRUE(out.stop);
    EXPECT_EQ(out.stop->category(), v.category);
    EXPECT_EQ(out.stop->code(), v.code);
    EXPECT_TRUE(points.empty());
    EXPECT_EQ(allocated_sample_bytes(), allocated);
  }
}


TEST(PhaseCalib, T_M2R1_05_prepare_calibration_checks_the_linearity_before_it_visits_the_samples) {
  // Campione non finito senza bit di invalidita': check_image lo rifiuterebbe con calib.nonfinite_unflagged.
  const auto unflagged_nan = [](image& im) { im.data<double>()[3] = std::numeric_limits<double>::quiet_NaN(); };
  {
    set_def s = synth_set(8, 4, -64);
    s.dark.hdr.linear_value = false;
    calibration_images im = images_of(s);
    unflagged_nan(im.light);
    EXPECT_STOP(prepare_calibration(recipe_of(s), std::move(im)), error_category::unsupported, "calib.linearity");
  }
  {
    set_def s = synth_set(8, 4, -64);
    s.flat.hdr.with_linear = false;
    calibration_images im = images_of(s);
    unflagged_nan(im.light);
    EXPECT_STOP(prepare_calibration(recipe_of(s), std::move(im)), error_category::metadata_missing, "calib.metadata.linearity");
  }
  {
    // Controllo negativo: con linearita' valida lo stesso campione ferma check_image.
    set_def s = synth_set(8, 4, -64);
    calibration_images im = images_of(s);
    unflagged_nan(im.light);
    EXPECT_STOP(prepare_calibration(recipe_of(s), std::move(im)), error_category::invalid_input, "calib.nonfinite_unflagged");
  }
}

}  // namespace
