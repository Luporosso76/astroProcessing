// U6 di M3.a, G-CAL parte 1: stati accettati e compatibilità (ADR-16.b §b.2, §b.3, §b.7; C-01.b §2, §7; ADR-03.b §b.14).
// T-M3A-CAL-08 (compatibilità del decoder), T-M3A-TYP-18 (tabella delle operazioni), T-M3A-C01-06 (dominio),
// T-M3A-C01-07 parte di analisi (coverage). Uscita dell'unità: zero calcoli negli stop, contati col fault_injector.
//
// Simboli del prodotto che questo file usa e che U6 introduce (nomi del contratto, concordati coi test di U7):
// - `astro/calibration_analysis.hpp`: `analyze_calibration(input, fault)`, `calibration_analysis_input`,
//   `analysis_candidate`, `calibration_declarations`, `calibration_analysis::candidates`, `candidate_analysis`,
//   `chain_verdict`, `candidate_status`;
// - in `master_descriptor`: `flat_normalization_declared` (§b.2) e `frame_decoder_states` (§b.7), elementi
//   `frame_decoder_state { decoder_state state; decoder_identity identity; }`;
// - in `image`: `coverage` (`std::optional<coverage_plane>`, ADR-03.b §b.5);
// - in `operation_kind`: `calibrate_light`, `calibrate_master_raw`, `calibrate_master_prepared`, e
//   `require_operation_allowed(kind, state, ruolo)` col ruolo dell'ingresso nel contesto (ADR-03.b §b.14).
//
// Convenzione del contatore: `analyze_calibration` chiama `fault` con "calib.analysis.pair" prima di valutare una coppia
// e con "calib.analysis.samples" prima di leggere campioni. Uno stop dell'analisi avviene prima di ogni calcolo: zero
// chiamate. Un'analisi completata con almeno un master ne fa almeno una (controllo positivo del contatore).
//
// Tutti gli esiti attesi sono ricodificati a mano dalle tabelle dei contratti, mai calcolati col codice sotto test.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "astro/calibration.hpp"
#include "astro/calibration_analysis.hpp"
#include "astro/fault_injector.hpp"
#include "astro/fits.hpp"
#include "astro/fits_blocks.hpp"
#include "astro/image.hpp"
#include "astro/operations.hpp"
#include "expect_stop.hpp"

namespace {

using namespace astro;

constexpr std::uint32_t kW = 4;
constexpr std::uint32_t kH = 4;
constexpr std::uint32_t kTol = 10;

// ---------------------------------------------------------------- contatore del fault_injector

struct call_counter {
  std::shared_ptr<int> calls = std::make_shared<int>(0);
  fault_injector injector() const {
    auto c = calls;
    return [c](std::string_view) {
      ++*c;
      return 0;
    };
  }
  int count() const { return *calls; }
};

// ---------------------------------------------------------------- immagini e candidati

metadata_value<double> hd(double v, const char* key) { return {v, {metadata_origin::header, key}}; }
metadata_value<std::string> hs(const std::string& v, const char* key) { return {v, {metadata_origin::header, key}}; }

// Immagine di un ingresso nella regola di ogni riga di ADR-16.b §b.2: fase `raw`, `linear`, `adu`, binning 1×1,
// metadati di confronto uguali a quelli del light salvo esposizione (coppie L–D, F–DF entro `exposure_rel_tol`).
image frame_image(double exposure_s, plane_layout layout = plane_layout::mono, std::uint32_t planes = 1) {
  image_geometry g(kW, kH, planes);
  g.binning_x = 1;
  g.binning_y = 1;
  std::optional<cfa_layout> cfa;
  if (layout == plane_layout::cfa) cfa = cfa_layout{cfa_pattern::rggb, 0, 0};
  image im = make_image(g, sample_type::f64, layout, cfa);
  double* p = im.data<double>();
  for (std::uint64_t i = 0; i < g.sample_count(); ++i) p[i] = 1000.0;
  im.unit = pixel_unit::adu;
  im.lin = linearity::linear;
  im.lin_origin.value = metadata_origin::header;
  im.phase = metadata_value<process_phase>{process_phase::raw, {metadata_origin::header, "ASTRPHAS"}};
  im.metadata.exposure_s = hd(exposure_s, "EXPTIME");
  im.metadata.gain = hd(100.0, "GAIN");
  im.metadata.offset_adu = hd(10.0, "OFFSET");
  im.metadata.temperature_c = hd(-10.0, "CCD-TEMP");
  im.metadata.filter = hs("L", "FILTER");
  im.metadata.camera = hs("cam-a", "INSTRUME");
  return im;
}

master_descriptor bias_desc() {
  master_descriptor d;
  d.role = master_role::bias;
  return d;
}
master_descriptor dark_desc(master_role role, additive_state add) {
  master_descriptor d;
  d.role = role;
  d.additive = add;
  d.scaling = scaling_state::unscaled;
  return d;
}
master_descriptor flat_desc(flat_state s) {
  master_descriptor d;
  d.role = master_role::flat;
  d.flat = s;
  if (s == flat_state::calibrated) d.flat_normalization_declared = normalization_mode::global_median;
  return d;
}

content_digest fake_digest(char c) { return content_digest{std::string(64, c), 2880}; }

double exposure_of(master_role role) {
  switch (role) {
    case master_role::bias: return 0.001;
    case master_role::dark: return 300.0;
    case master_role::dark_flat: return 2.0;
    case master_role::flat: return 2.0;
  }
  return 0;
}

analysis_candidate candidate(master_descriptor d, plane_layout layout = plane_layout::mono) {
  analysis_candidate c;
  c.img = frame_image(exposure_of(d.role), layout);
  if (d.role == master_role::flat && d.flat == flat_state::calibrated) c.img.unit = pixel_unit::dimensionless;
  const char tag = d.role == master_role::bias ? 'b' : d.role == master_role::dark ? 'd' : d.role == master_role::dark_flat ? 'e' : 'f';
  c.descriptor = std::move(d);
  c.digest = fake_digest(tag);
  return c;
}

calibration_declarations fits_declarations() {
  calibration_declarations decl;
  decl.exposure_rel_tol = 0.01;
  decl.temperature_abs_tol_c = 1.0;
  decl.temp_policy = temperature_policy::compare;
  return decl;
}

// Light FITS e un master FITS per ruolo, tutti nella regola: D `bias_included`, dark-flat INC, flat `raw`.
calibration_analysis_input fits_input() {
  calibration_analysis_input in;
  in.light = frame_image(300.0);
  in.light_digest = fake_digest('a');
  in.bias = candidate(bias_desc());
  in.dark = candidate(dark_desc(master_role::dark, additive_state::bias_included));
  in.dark_flat = candidate(dark_desc(master_role::dark_flat, additive_state::bias_included));
  in.flat = candidate(flat_desc(flat_state::raw));
  in.declarations = fits_declarations();
  return in;
}

// ---------------------------------------------------------------- stato del decoder

// Visibile 4×4 in un grezzo 8×8, origine (2, 2); RGGB con FC = 0, 1, 3, 2 sui siti (0,0), (0,1), (1,0), (1,1).
decoder_state base_state() {
  decoder_state s;
  s.raw_width = 8;
  s.raw_height = 8;
  s.window = decoder_window{2, 2, kW, kH};
  s.black = 512;
  s.cfa_color_index = {0, 1, 3, 2};
  s.black_per_color = {0, 0, 0, 0};
  s.black_pattern_rows = 2;
  s.black_pattern_cols = 2;
  s.black_pattern = {0, 0, 0, 0};
  s.white = 16383;
  s.linearization = decoder_linearization::identity;
  s.source = metadata_source{metadata_origin::decoder, "color.black"};
  return s;
}

decoder_identity identity_a() { return decoder_identity{"0.22.3-Release", 0x001603, 0xc0, std::string(64, '1')}; }
decoder_identity identity_b() { return decoder_identity{"0.22.2-Release", 0x001602, 0x80, std::string(64, '2')}; }

image raw_light(const decoder_state& s) {
  image im = frame_image(300.0, plane_layout::cfa);
  im.phase = metadata_value<process_phase>{process_phase::raw, {metadata_origin::decoder, "decoder"}};
  im.metadata.offset_adu.reset();
  im.decoder_state = s;
  return im;
}

void make_raw_master(analysis_candidate& c, const std::vector<decoder_state>& states) {
  c.img = frame_image(exposure_of(c.descriptor.role), plane_layout::cfa);
  c.img.metadata.offset_adu.reset();
  if (c.descriptor.role == master_role::flat && c.descriptor.flat == flat_state::calibrated) {
    c.img.unit = pixel_unit::dimensionless;
  }
  std::vector<frame_decoder_state> frames;
  for (const decoder_state& s : states) frames.push_back(frame_decoder_state{s, identity_a()});
  c.descriptor.frame_decoder_states = std::move(frames);
}

// Light RAW e master da RAW (un frame ciascuno) con lo stesso stato: D, dark-flat INC, bias, flat `raw`.
calibration_analysis_input raw_input(const decoder_state& s = base_state()) {
  calibration_analysis_input in = fits_input();
  in.light = raw_light(s);
  for (std::optional<analysis_candidate>* c : {&in.bias, &in.dark, &in.dark_flat, &in.flat}) make_raw_master(**c, {s});
  in.declarations.decoder_black_abs_tol_adu = kTol;
  return in;
}

// ---------------------------------------------------------------- lettura dell'esito

const char* role_name(master_role r) {
  switch (r) {
    case master_role::bias: return "bias";
    case master_role::dark: return "dark";
    case master_role::dark_flat: return "dark_flat";
    case master_role::flat: return "flat";
  }
  return "?";
}

const candidate_analysis* find_candidate(const calibration_analysis& a, master_role r) {
  for (const candidate_analysis& c : a.candidates) {
    if (c.role == r) return &c;
  }
  return nullptr;
}

// Catene di ADR-16.b §b.5 che usano un candidato, dal suo ruolo e dal suo stato dichiarato (ricodifica a mano).
std::vector<std::string> chains_of(const master_descriptor& d) {
  switch (d.role) {
    case master_role::bias: return {"light.bias", "light.bias_dark_sub", "flat.bias_dark_flat_sub", "flat.bias"};
    case master_role::dark:
      if (d.additive == additive_state::bias_subtracted) return {"light.bias_dark_sub", "light.dark_sub_partial"};
      return {"light.dark"};
    case master_role::dark_flat:
      if (d.additive == additive_state::bias_subtracted) return {"flat.bias_dark_flat_sub"};
      return {"flat.dark_flat"};
    case master_role::flat:
      if (d.flat == flat_state::corrected_unnormalized) return {"flat.normalize"};
      if (d.flat == flat_state::calibrated) return {"flat.given_q"};
      return {"flat.dark_flat", "flat.bias_dark_flat_sub", "flat.bias"};
  }
  return {};
}

std::string verdict_text(const chain_verdict& v) {
  return v.chain + "=" + (v.status == candidate_status::usable ? "usable" : "excluded:" + v.reason.value_or("<nessun motivo>"));
}

// Insieme dei nomi delle catene del candidato uguale a quello di §b.5 per il suo ruolo e stato, senza catene estranee
// né ripetute.
void expect_chain_set(const calibration_analysis& a, master_role r, const master_descriptor& d) {
  SCOPED_TRACE(std::string("catene di ") + role_name(r));
  const candidate_analysis* c = find_candidate(a, r);
  ASSERT_NE(c, nullptr) << "candidato assente dall'esito dell'analisi";
  std::vector<std::string> got;
  for (const chain_verdict& v : c->chains) got.push_back(v.chain);
  std::vector<std::string> want = chains_of(d);
  std::sort(got.begin(), got.end());
  std::sort(want.begin(), want.end());
  EXPECT_EQ(got, want);
}

// Esito di una catena: `reason` vuoto = `usable` senza motivo; altrimenti `excluded` con quel motivo.
void expect_chain(const calibration_analysis& a, master_role r, const std::string& chain, const std::string& reason) {
  SCOPED_TRACE(std::string(role_name(r)) + " in " + chain);
  const candidate_analysis* c = find_candidate(a, r);
  ASSERT_NE(c, nullptr) << "candidato assente dall'esito dell'analisi";
  const chain_verdict* found = nullptr;
  for (const chain_verdict& v : c->chains) {
    if (v.chain == chain) found = &v;
  }
  ASSERT_NE(found, nullptr) << "catena assente dall'esito del candidato";
  if (reason.empty()) {
    EXPECT_EQ(found->status, candidate_status::usable) << verdict_text(*found);
    EXPECT_FALSE(found->reason.has_value()) << verdict_text(*found);
  } else {
    EXPECT_EQ(found->status, candidate_status::excluded) << verdict_text(*found);
    EXPECT_EQ(found->reason.value_or(""), reason) << verdict_text(*found);
  }
}

// Tutte le catene del candidato (ricodificate da chains_of) con lo stesso esito; stato complessivo coerente
// (`usable` se lo è in almeno una catena, §b.1).
void expect_all_chains(const calibration_analysis& a, master_role r, const master_descriptor& d, const std::string& reason) {
  expect_chain_set(a, r, d);
  for (const std::string& chain : chains_of(d)) expect_chain(a, r, chain, reason);
  const candidate_analysis* c = find_candidate(a, r);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->status, reason.empty() ? candidate_status::usable : candidate_status::excluded) << role_name(r);
}

// Come expect_all_chains con un candidato escluso, ma il motivo è uno qualsiasi di `reasons`, lo stesso in ogni catena:
// per due campi che il contratto obbliga a cambiare insieme e fra i quali §b.7 non fissa una precedenza.
void expect_all_chains_one_of(const calibration_analysis& a, master_role r, const master_descriptor& d,
                              const std::vector<std::string>& reasons) {
  expect_chain_set(a, r, d);
  const candidate_analysis* c = find_candidate(a, r);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->status, candidate_status::excluded) << role_name(r);
  ASSERT_FALSE(c->chains.empty());
  const std::string first = c->chains.front().reason.value_or("");
  EXPECT_NE(std::find(reasons.begin(), reasons.end(), first), reasons.end()) << verdict_text(c->chains.front());
  for (const chain_verdict& v : c->chains) {
    EXPECT_EQ(v.status, candidate_status::excluded) << verdict_text(v);
    EXPECT_EQ(v.reason.value_or(""), first) << verdict_text(v);
  }
}

const master_descriptor& desc(const std::optional<analysis_candidate>& c) { return c->descriptor; }

// Bias `usable` nelle catene del light ed escluso con `reason` in quelle del flat (§b.3, esclusione per catena).
void expect_bias_excluded_only_in_flat_chains(const calibration_analysis& a, const master_descriptor& d,
                                              const std::string& reason) {
  expect_chain_set(a, master_role::bias, d);
  expect_chain(a, master_role::bias, "light.bias", "");
  expect_chain(a, master_role::bias, "light.bias_dark_sub", "");
  expect_chain(a, master_role::bias, "flat.bias_dark_flat_sub", reason);
  expect_chain(a, master_role::bias, "flat.bias", reason);
  const candidate_analysis* b = find_candidate(a, master_role::bias);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->status, candidate_status::usable);
}

void expect_all_usable(const calibration_analysis& a, const calibration_analysis_input& in) {
  if (in.bias) expect_all_chains(a, master_role::bias, desc(in.bias), "");
  if (in.dark) expect_all_chains(a, master_role::dark, desc(in.dark), "");
  if (in.dark_flat) expect_all_chains(a, master_role::dark_flat, desc(in.dark_flat), "");
  if (in.flat) expect_all_chains(a, master_role::flat, desc(in.flat), "");
}

std::optional<analysis_candidate>& slot(calibration_analysis_input& in, master_role r) {
  switch (r) {
    case master_role::bias: return in.bias;
    case master_role::dark: return in.dark;
    case master_role::dark_flat: return in.dark_flat;
    case master_role::flat: return in.flat;
  }
  return in.bias;
}

// Analisi che deve completarsi: nessuno stop, almeno un calcolo contato.
calibration_analysis analyze_ok(const calibration_analysis_input& in) {
  call_counter counter;
  calibration_analysis a = analyze_calibration(in, counter.injector());
  EXPECT_GT(counter.count(), 0) << "analisi completata senza alcuna chiamata al fault_injector: contatore non collegato";
  return a;
}

// Stop dell'analisi: categoria e codice, nessun esito, zero calcoli.
void expect_analysis_stop(const calibration_analysis_input& in, error_category cat, const char* code) {
  call_counter counter;
  EXPECT_STOP(analyze_calibration(in, counter.injector()), cat, code);
  EXPECT_EQ(counter.count(), 0) << "calcoli prima dello stop " << code;
}

bool context_has_value(const stop_error& e, const std::string& value) {
  for (const auto& [k, v] : e.context()) {
    if (v == value) return true;
  }
  return false;
}

// ================================================================ controllo positivo

TEST(CalibrationAnalysisU6, T_M3A_C01_06_inputs_in_rule_are_usable_in_every_chain_fits_and_raw) {
  {
    SCOPED_TRACE("FITS");
    const calibration_analysis_input in = fits_input();
    expect_all_usable(analyze_ok(in), in);
  }
  {
    SCOPED_TRACE("RAW");
    const calibration_analysis_input in = raw_input();
    expect_all_usable(analyze_ok(in), in);
  }
}

// ================================================================ T-M3A-C01-06: dominio del light (C-01.b §2)

TEST(CalibrationAnalysisU6, T_M3A_C01_06_light_out_of_domain_stops_the_analysis_before_any_computation) {
  struct light_case {
    const char* name;
    void (*mutate)(image&);
    error_category cat;
    const char* code;
  };
  const light_case cases[] = {
      {"fase calibrated", [](image& l) { l.phase->value = process_phase::calibrated; }, error_category::incompatible,
       "op.phase_blocked"},
      {"fase prestacked", [](image& l) { l.phase->value = process_phase::prestacked; }, error_category::incompatible,
       "op.phase_blocked"},
      {"linearità nonlinear", [](image& l) { l.lin = linearity::nonlinear; }, error_category::incompatible,
       "op.linearity_blocked"},
      {"linearità assente",
       [](image& l) {
         l.lin.reset();
         l.lin_origin.value.reset();
       },
       error_category::incompatible, "op.linearity_blocked"},
      {"unità electron", [](image& l) { l.unit = pixel_unit::electron; }, error_category::unsupported, "calib.unit"},
      {"unità dimensionless", [](image& l) { l.unit = pixel_unit::dimensionless; }, error_category::unsupported,
       "calib.unit"},
      {"fase e linearità entrambe fuori",
       [](image& l) {
         l.phase->value = process_phase::calibrated;
         l.lin = linearity::nonlinear;
       },
       error_category::incompatible, "op.phase_blocked"},
      {"fase prestacked e linearità assente",
       [](image& l) {
         l.phase->value = process_phase::prestacked;
         l.lin.reset();
         l.lin_origin.value.reset();
       },
       error_category::incompatible, "op.phase_blocked"},
      {"linearità nonlinear e unità electron",
       [](image& l) {
         l.lin = linearity::nonlinear;
         l.unit = pixel_unit::electron;
       },
       error_category::incompatible, "op.linearity_blocked"},
  };
  for (const light_case& c : cases) {
    SCOPED_TRACE(c.name);
    calibration_analysis_input in = fits_input();
    c.mutate(in.light);
    expect_analysis_stop(in, c.cat, c.code);
  }
}

TEST(CalibrationAnalysisU6, T_M3A_C01_06_light_with_rgb_or_multi_plane_layout_stops_with_plane_layout) {
  for (const auto& [layout, planes, name] :
       {std::tuple{plane_layout::rgb, 3u, "rgb"}, std::tuple{plane_layout::multi_plane, 2u, "multi_plane"}}) {
    SCOPED_TRACE(name);
    calibration_analysis_input in = fits_input();
    in.light = frame_image(300.0, layout, planes);
    expect_analysis_stop(in, error_category::unsupported, "calib.plane_layout");
  }
  // Unità e layout entrambi fuori: l'unità si controlla prima del layout (C-01.b §2).
  calibration_analysis_input in = fits_input();
  in.light = frame_image(300.0, plane_layout::rgb, 3);
  in.light.unit = pixel_unit::electron;
  expect_analysis_stop(in, error_category::unsupported, "calib.unit");
}

// ================================================================ T-M3A-C01-06: dominio dei master (ADR-16.b §b.2)

struct master_case {
  const char* name;
  master_role role;
  master_descriptor descriptor;
  plane_layout layout;
  void (*mutate)(image&);
  const char* reason;  // vuoto = controllo positivo, `usable`
};

void run_master_cases(const std::vector<master_case>& cases) {
  for (const master_case& c : cases) {
    SCOPED_TRACE(c.name);
    calibration_analysis_input in = fits_input();
    analysis_candidate cand = candidate(c.descriptor);
    if (c.layout == plane_layout::rgb) cand.img = frame_image(exposure_of(c.role), plane_layout::rgb, 3);
    if (c.mutate) c.mutate(cand.img);
    slot(in, c.role) = std::move(cand);
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, c.role, c.descriptor, c.reason);
    // Gli altri ruoli restano `usable`: l'esclusione non ferma l'analisi e non tocca gli altri candidati.
    for (master_role other : {master_role::bias, master_role::dark, master_role::dark_flat, master_role::flat}) {
      if (other == c.role) continue;
      // Un flat fuori dominio non esclude il dark-flat e il bias (le coppie F–DF e F–B si valutano sul flat candidato
      // anche se escluso, ADR-16.b §b.3): i suoi metadati di confronto restano quelli della regola.
      expect_all_chains(a, other, desc(slot(in, other)), "");
    }
  }
}

TEST(CalibrationAnalysisU6, T_M3A_C01_06_master_out_of_domain_is_excluded_and_the_analysis_completes) {
  const auto set_phase = [](process_phase p) {
    return p == process_phase::calibrated ? +[](image& m) { m.phase->value = process_phase::calibrated; }
                                          : +[](image& m) { m.phase->value = process_phase::prestacked; };
  };
  const std::vector<master_case> cases = {
      {"bias fase calibrated", master_role::bias, bias_desc(), plane_layout::mono, set_phase(process_phase::calibrated),
       "phase"},
      {"D fase calibrated", master_role::dark, dark_desc(master_role::dark, additive_state::bias_included),
       plane_layout::mono, set_phase(process_phase::calibrated), "phase"},
      {"D' fase prestacked", master_role::dark, dark_desc(master_role::dark, additive_state::bias_subtracted),
       plane_layout::mono, set_phase(process_phase::prestacked), "phase"},
      {"flat calibrated fase prestacked", master_role::flat, flat_desc(flat_state::calibrated), plane_layout::mono,
       set_phase(process_phase::prestacked), "phase"},
      {"D linearità nonlinear", master_role::dark, dark_desc(master_role::dark, additive_state::bias_included),
       plane_layout::mono, [](image& m) { m.lin = linearity::nonlinear; }, "linearity"},
      {"D unità dimensionless", master_role::dark, dark_desc(master_role::dark, additive_state::bias_included),
       plane_layout::mono, [](image& m) { m.unit = pixel_unit::dimensionless; }, "unit"},
      {"flat raw unità electron", master_role::flat, flat_desc(flat_state::raw), plane_layout::mono,
       [](image& m) { m.unit = pixel_unit::electron; }, "unit"},
      {"flat calibrated unità adu", master_role::flat, flat_desc(flat_state::calibrated), plane_layout::mono,
       [](image& m) { m.unit = pixel_unit::adu; }, "unit"},
      {"D layout rgb", master_role::dark, dark_desc(master_role::dark, additive_state::bias_included),
       plane_layout::rgb, nullptr, "layout"},
      // Controlli positivi.
      {"D' fase calibrated", master_role::dark, dark_desc(master_role::dark, additive_state::bias_subtracted),
       plane_layout::mono, set_phase(process_phase::calibrated), ""},
      {"flat calibrated unità dimensionless", master_role::flat, flat_desc(flat_state::calibrated), plane_layout::mono,
       nullptr, ""},
  };
  run_master_cases(cases);
}

TEST(CalibrationAnalysisU6, T_M3A_C01_06_master_domain_checks_follow_phase_linearity_unit_layout_order) {
  // Il primo violato nell'ordine fase, linearità, unità, layout (C-01.b §2).
  const std::vector<master_case> cases = {
      {"D fase calibrated e unità electron", master_role::dark,
       dark_desc(master_role::dark, additive_state::bias_included), plane_layout::mono,
       [](image& m) {
         m.phase->value = process_phase::calibrated;
         m.unit = pixel_unit::electron;
       },
       "phase"},
      {"D linearità nonlinear e unità electron", master_role::dark,
       dark_desc(master_role::dark, additive_state::bias_included), plane_layout::mono,
       [](image& m) {
         m.lin = linearity::nonlinear;
         m.unit = pixel_unit::electron;
       },
       "linearity"},
      {"D layout rgb e unità dimensionless", master_role::dark,
       dark_desc(master_role::dark, additive_state::bias_included), plane_layout::rgb,
       [](image& m) { m.unit = pixel_unit::dimensionless; }, "unit"},
  };
  run_master_cases(cases);
}

// Fase e unità assenti: il prodotto non le indovina (ADR-03, «Aggiunte di R1»: la calibrazione richiede la fase). Campo
// richiesto assente: `metadata_missing`, `calib.metadata.<campo>` (ADR-16.a §2); nell'analisi il master è escluso con
// `metadata_missing.<campo>` (ADR-16.b §b.3).
TEST(CalibrationAnalysisU6, T_M3A_C01_06_absent_phase_or_unit_is_never_assumed) {
  {
    SCOPED_TRACE("light senza fase");
    calibration_analysis_input in = fits_input();
    in.light.phase.reset();
    expect_analysis_stop(in, error_category::metadata_missing, "calib.metadata.phase");
  }
  {
    SCOPED_TRACE("light senza unità");
    calibration_analysis_input in = fits_input();
    in.light.unit.reset();
    expect_analysis_stop(in, error_category::metadata_missing, "calib.metadata.unit");
  }
  const std::vector<master_case> cases = {
      {"D senza fase", master_role::dark, dark_desc(master_role::dark, additive_state::bias_included), plane_layout::mono,
       [](image& m) { m.phase.reset(); }, "metadata_missing.phase"},
      {"flat raw senza unità", master_role::flat, flat_desc(flat_state::raw), plane_layout::mono,
       [](image& m) { m.unit.reset(); }, "metadata_missing.unit"},
  };
  run_master_cases(cases);
}

// ================================================================ T-M3A-TYP-18: tabella delle operazioni (ADR-03.b §b.14)

TEST(CalibrationAnalysisU6, T_M3A_TYP_18_three_rows_by_phase_by_linearity) {
  struct row {
    operation_kind kind;
    const char* name;
    bool calibrated_admitted;
  };
  // Ricodifica a mano di §b.14: `calibrate_master_prepared` ammette anche `calibrated`; nessuna riga ammette `prestacked`;
  // la sola linearità ammessa è `linear`.
  const row rows[] = {{operation_kind::calibrate_light, "calibrate_light", false},
                      {operation_kind::calibrate_master_raw, "calibrate_master_raw", false},
                      {operation_kind::calibrate_master_prepared, "calibrate_master_prepared", true}};
  const process_phase phases[] = {process_phase::raw, process_phase::calibrated, process_phase::prestacked};
  const linearity_class lins[] = {linearity_class::linear, linearity_class::nonlinear, linearity_class::unknown};
  int admitted = 0;
  for (const row& r : rows) {
    EXPECT_STREQ(to_string(r.kind), r.name);
    for (const process_phase ph : phases) {
      for (const linearity_class ln : lins) {
        SCOPED_TRACE(std::string(r.name) + "/" + to_string(ph) + "/" + to_string(ln));
        const process_state st{ph, ln};
        const bool phase_ok = ph == process_phase::raw || (ph == process_phase::calibrated && r.calibrated_admitted);
        const char* expected = !phase_ok ? "op.phase_blocked" : ln != linearity_class::linear ? "op.linearity_blocked" : "";
        EXPECT_EQ(blocked_code(r.kind, st), expected);
        if (*expected == '\0') {
          ++admitted;
          EXPECT_NO_THROW(require_operation_allowed(r.kind, st, "dark"));
          continue;
        }
        try {
          require_operation_allowed(r.kind, st, "dark_flat");
          ADD_FAILURE() << "atteso stop " << expected;
        } catch (const stop_error& e) {
          EXPECT_EQ(e.category(), error_category::incompatible) << e.what();
          EXPECT_EQ(e.code(), expected) << e.what();
          EXPECT_TRUE(context_has_value(e, "dark_flat")) << "ruolo dell'ingresso assente dal contesto: " << e.what();
        }
      }
    }
  }
  // Celle ammesse: (raw, linear) nelle tre righe più (calibrated, linear) in `calibrate_master_prepared`.
  EXPECT_EQ(admitted, 4);
}

TEST(CalibrationAnalysisU6, T_M3A_TYP_18_analysis_excludes_a_master_out_of_its_row_and_never_stops) {
  const std::vector<master_case> cases = {
      // calibrate_master_raw: bias, D, DF, flat raw.
      {"bias fase prestacked", master_role::bias, bias_desc(), plane_layout::mono,
       [](image& m) { m.phase->value = process_phase::prestacked; }, "phase"},
      {"D linearità assente", master_role::dark, dark_desc(master_role::dark, additive_state::bias_included),
       plane_layout::mono,
       [](image& m) {
         m.lin.reset();
         m.lin_origin.value.reset();
       },
       "linearity"},
      {"DF fase calibrated", master_role::dark_flat, dark_desc(master_role::dark_flat, additive_state::bias_included),
       plane_layout::mono, [](image& m) { m.phase->value = process_phase::calibrated; }, "phase"},
      {"flat raw fase calibrated", master_role::flat, flat_desc(flat_state::raw), plane_layout::mono,
       [](image& m) { m.phase->value = process_phase::calibrated; }, "phase"},
      // calibrate_master_prepared: D', DF', flat corrected_unnormalized e calibrated.
      {"DF' fase prestacked", master_role::dark_flat, dark_desc(master_role::dark_flat, additive_state::bias_subtracted),
       plane_layout::mono, [](image& m) { m.phase->value = process_phase::prestacked; }, "phase"},
      {"flat corrected_unnormalized linearità nonlinear", master_role::flat, flat_desc(flat_state::corrected_unnormalized),
       plane_layout::mono, [](image& m) { m.lin = linearity::nonlinear; }, "linearity"},
      // Fase prima della linearità.
      {"D' fase prestacked e linearità nonlinear", master_role::dark,
       dark_desc(master_role::dark, additive_state::bias_subtracted), plane_layout::mono,
       [](image& m) {
         m.phase->value = process_phase::prestacked;
         m.lin = linearity::nonlinear;
       },
       "phase"},
      {"flat calibrated fase prestacked e linearità assente", master_role::flat, flat_desc(flat_state::calibrated),
       plane_layout::mono,
       [](image& m) {
         m.phase->value = process_phase::prestacked;
         m.lin.reset();
         m.lin_origin.value.reset();
       },
       "phase"},
      // Celle ammesse della riga `calibrate_master_prepared` per i master preparati.
      {"DF' fase calibrated", master_role::dark_flat, dark_desc(master_role::dark_flat, additive_state::bias_subtracted),
       plane_layout::mono, [](image& m) { m.phase->value = process_phase::calibrated; }, ""},
      {"flat corrected_unnormalized fase calibrated", master_role::flat, flat_desc(flat_state::corrected_unnormalized),
       plane_layout::mono, [](image& m) { m.phase->value = process_phase::calibrated; }, ""},
  };
  run_master_cases(cases);
}

TEST(CalibrationAnalysisU6, T_M3A_TYP_18_state_is_checked_before_the_phase) {
  // Un master di stato `unknown` o `scaled` non ha riga: escluso per stato prima del controllo di fase (§b.14).
  struct state_case {
    const char* name;
    master_role role;
    master_descriptor descriptor;
    const char* reason;
  };
  master_descriptor d_unknown = dark_desc(master_role::dark, additive_state::bias_included);
  d_unknown.scaling = scaling_state::unknown;
  master_descriptor d_scaled = dark_desc(master_role::dark, additive_state::bias_included);
  d_scaled.scaling = scaling_state::scaled;
  master_descriptor f_incomplete;
  f_incomplete.role = master_role::flat;
  f_incomplete.flat = flat_state::calibrated;  // senza flat_normalization_declared: `state_incomplete`
  master_descriptor d_additive_unknown = dark_desc(master_role::dark, additive_state::unknown);
  master_descriptor df_scaled = dark_desc(master_role::dark_flat, additive_state::bias_included);
  df_scaled.scaling = scaling_state::scaled;
  master_descriptor df_unknown = dark_desc(master_role::dark_flat, additive_state::unknown);
  master_descriptor f_unknown = flat_desc(flat_state::unknown);
  master_descriptor b_incomplete = bias_desc();
  b_incomplete.additive = additive_state::bias_included;  // bias con un campo di stato presente: `state_incomplete`
  const state_case cases[] = {{"D scaling unknown", master_role::dark, d_unknown, "state_unknown"},
                              {"D additive unknown", master_role::dark, d_additive_unknown, "state_unknown"},
                              {"D scaled", master_role::dark, d_scaled, "state_scaled"},
                              {"DF scaled (P-DF-BAD)", master_role::dark_flat, df_scaled, "state_scaled"},
                              {"DF additive unknown (P-DF-BAD)", master_role::dark_flat, df_unknown, "state_unknown"},
                              {"flat unknown (P-FLAT-UNKNOWN)", master_role::flat, f_unknown, "state_unknown"},
                              {"flat calibrated senza normalizzazione dichiarata", master_role::flat, f_incomplete,
                               "state_incomplete"},
                              {"bias con additive presente", master_role::bias, b_incomplete, "state_incomplete"}};
  for (const state_case& c : cases) {
    SCOPED_TRACE(c.name);
    calibration_analysis_input in = fits_input();
    analysis_candidate cand = candidate(c.descriptor);
    cand.img.phase->value = process_phase::prestacked;
    cand.img.lin = linearity::nonlinear;
    slot(in, c.role) = std::move(cand);
    const calibration_analysis a = analyze_ok(in);
    const candidate_analysis* ca = find_candidate(a, c.role);
    ASSERT_NE(ca, nullptr);
    EXPECT_EQ(ca->status, candidate_status::excluded);
    ASSERT_FALSE(ca->chains.empty()) << "candidato escluso senza alcun esito con motivo";
    for (const chain_verdict& v : ca->chains) {
      EXPECT_EQ(v.status, candidate_status::excluded) << verdict_text(v);
      EXPECT_EQ(v.reason.value_or(""), c.reason) << verdict_text(v);
    }
  }
}

TEST(CalibrationAnalysisU6, T_M3A_TYP_18_light_calibrated_stops_with_phase_blocked) {
  calibration_analysis_input in = fits_input();
  in.light.phase->value = process_phase::calibrated;
  call_counter counter;
  try {
    (void)analyze_calibration(in, counter.injector());
    ADD_FAILURE() << "atteso op.phase_blocked";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::incompatible) << e.what();
    EXPECT_EQ(e.code(), "op.phase_blocked") << e.what();
    EXPECT_TRUE(context_has_value(e, "light")) << "ruolo dell'ingresso assente dal contesto: " << e.what();
  }
  EXPECT_EQ(counter.count(), 0);
}

// ================================================================ T-M3A-C01-07, parte di analisi: coverage (C-01.b §7)

void add_coverage(image& im, double value) {
  const std::uint64_t n = im.geometry.sample_count();
  coverage_plane cov;
  cov.samples = aligned_buffer(static_cast<std::size_t>(n * sizeof(double)));
  double* p = cov.samples.as<double>();
  for (std::uint64_t i = 0; i < n; ++i) p[i] = value;
  im.coverage = std::move(cov);
}

TEST(CalibrationAnalysisU6, T_M3A_C01_07_light_with_coverage_stops_the_analysis_even_when_it_is_all_one) {
  for (const double value : {0.5, 1.0}) {
    SCOPED_TRACE(value);
    calibration_analysis_input in = fits_input();
    add_coverage(in.light, value);
    expect_analysis_stop(in, error_category::unsupported, "calib.coverage_input");
  }
}

TEST(CalibrationAnalysisU6, T_M3A_C01_07_master_with_coverage_is_excluded_in_every_chain_even_when_it_is_all_one) {
  for (const double value : {0.5, 1.0}) {
    for (master_role r : {master_role::bias, master_role::dark, master_role::dark_flat, master_role::flat}) {
      SCOPED_TRACE(std::string(role_name(r)) + " coverage " + std::to_string(value));
      calibration_analysis_input in = fits_input();
      add_coverage(slot(in, r)->img, value);
      const calibration_analysis a = analyze_ok(in);
      expect_all_chains(a, r, desc(slot(in, r)), "coverage");
      for (master_role other : {master_role::bias, master_role::dark, master_role::dark_flat, master_role::flat}) {
        if (other != r) expect_all_chains(a, other, desc(slot(in, other)), "");
      }
    }
  }
}

TEST(CalibrationAnalysisU6, T_M3A_C01_07_inputs_without_coverage_are_usable) {
  const calibration_analysis_input in = fits_input();
  ASSERT_FALSE(in.light.coverage.has_value());
  expect_all_usable(analyze_ok(in), in);
}

// ================================================================ T-M3A-CAL-08: compatibilità del decoder (ADR-16.b §b.7)

struct structural_case {
  const char* field;
  void (*mutate)(decoder_state&);
  const char* alternative = nullptr;  // altro motivo ammesso quando il contratto obbliga due campi a cambiare insieme
};

const std::vector<structural_case>& structural_cases() {
  static const std::vector<structural_case> cases = {
      {"raw_width", [](decoder_state& s) { s.raw_width = 10; }},
      {"raw_height", [](decoder_state& s) { s.raw_height = 10; }},
      {"window", [](decoder_state& s) { s.window.x0 = 0; }},
      {"window", [](decoder_state& s) { s.window.y0 = 4; }},
      {"cfa_color_index", [](decoder_state& s) { s.cfa_color_index = {0, 3, 1, 2}; }},
      {"black_pattern_rows",
       [](decoder_state& s) {
         s.black_pattern_rows = 1;
         s.black_pattern = {0, 0};
       }},
      {"black_pattern_cols",
       [](decoder_state& s) {
         s.black_pattern_cols = 1;
         s.black_pattern = {0, 0};
       }},
      // `curve_sha256` è presente se e solo se `linearization = decoder_curve` (ADR-03.b §b.2): i due campi cambiano
      // insieme e §b.7 non fissa una precedenza, quindi vale l'uno o l'altro motivo.
      {"linearization",
       [](decoder_state& s) {
         s.linearization = decoder_linearization::decoder_curve;
         s.curve_sha256 = std::array<std::uint8_t, 32>{};
       },
       "curve_sha256"},
  };
  return cases;
}

TEST(CalibrationAnalysisU6, T_M3A_CAL_08_structural_field_differs_one_at_a_time_on_light_dark) {
  for (const structural_case& c : structural_cases()) {
    SCOPED_TRACE(c.field);
    calibration_analysis_input in = raw_input();
    decoder_state s = base_state();
    c.mutate(s);
    make_raw_master(*in.dark, {s});
    const calibration_analysis a = analyze_ok(in);
    if (c.alternative) {
      expect_all_chains_one_of(a, master_role::dark, desc(in.dark),
                               {std::string("decoder_state_mismatch.") + c.field,
                                std::string("decoder_state_mismatch.") + c.alternative});
    } else {
      expect_all_chains(a, master_role::dark, desc(in.dark), std::string("decoder_state_mismatch.") + c.field);
    }
    expect_all_chains(a, master_role::bias, desc(in.bias), "");
    expect_all_chains(a, master_role::dark_flat, desc(in.dark_flat), "");
    expect_all_chains(a, master_role::flat, desc(in.flat), "");
  }
}

TEST(CalibrationAnalysisU6, T_M3A_CAL_08_curve_sha256_differs_with_both_curves_present) {
  decoder_state s = base_state();
  s.linearization = decoder_linearization::decoder_curve;
  std::array<std::uint8_t, 32> h{};
  h.fill(0x11);
  s.curve_sha256 = h;
  calibration_analysis_input in = raw_input(s);
  decoder_state other = s;
  h.fill(0x22);
  other.curve_sha256 = h;
  make_raw_master(*in.dark, {other});
  const calibration_analysis a = analyze_ok(in);
  expect_all_chains(a, master_role::dark, desc(in.dark), "decoder_state_mismatch.curve_sha256");
  expect_all_chains(a, master_role::bias, desc(in.bias), "");
}

TEST(CalibrationAnalysisU6, T_M3A_CAL_08_structural_mismatch_on_every_pair_of_section_b3) {
  const decoder_state changed = [] {
    decoder_state s = base_state();
    s.raw_width = 10;
    return s;
  }();
  const char* reason = "decoder_state_mismatch.raw_width";
  {
    SCOPED_TRACE("L–D'");
    calibration_analysis_input in = raw_input();
    in.dark->descriptor = dark_desc(master_role::dark, additive_state::bias_subtracted);
    make_raw_master(*in.dark, {changed});
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::dark, desc(in.dark), reason);
    expect_all_chains(a, master_role::bias, desc(in.bias), "");
  }
  {
    SCOPED_TRACE("L–B: escluso nelle catene del light; F–B: il flat ha lo stato del light, escluso anche in quelle del flat");
    calibration_analysis_input in = raw_input();
    make_raw_master(*in.bias, {changed});
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::bias, desc(in.bias), reason);
    expect_all_chains(a, master_role::dark, desc(in.dark), "");
    expect_all_chains(a, master_role::flat, desc(in.flat), "");
  }
  {
    SCOPED_TRACE("F–DF: il dark-flat si confronta col flat, non col light");
    calibration_analysis_input in = raw_input();
    make_raw_master(*in.dark_flat, {changed});
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::dark_flat, desc(in.dark_flat), reason);
    expect_all_chains(a, master_role::flat, desc(in.flat), "");
    expect_all_chains(a, master_role::dark, desc(in.dark), "");
  }
  {
    SCOPED_TRACE("F–DF'");
    calibration_analysis_input in = raw_input();
    in.dark_flat->descriptor = dark_desc(master_role::dark_flat, additive_state::bias_subtracted);
    make_raw_master(*in.dark_flat, {changed});
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::dark_flat, desc(in.dark_flat), reason);
    expect_all_chains(a, master_role::flat, desc(in.flat), "");
  }
  {
    SCOPED_TRACE("L–F: flat escluso; F–DF e F–B escludono dark-flat e bias nelle sole catene del flat");
    calibration_analysis_input in = raw_input();
    make_raw_master(*in.flat, {changed});
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::flat, desc(in.flat), reason);
    expect_all_chains(a, master_role::dark_flat, desc(in.dark_flat), reason);
    expect_bias_excluded_only_in_flat_chains(a, desc(in.bias), reason);
    expect_all_chains(a, master_role::dark, desc(in.dark), "");
  }
}

TEST(CalibrationAnalysisU6, T_M3A_CAL_08_every_frame_of_the_master_is_compared) {
  decoder_state structural = base_state();
  structural.cfa_color_index = {0, 3, 1, 2};
  decoder_state black_off = base_state();
  black_off.black = 512 + kTol + 1;
  const struct {
    const decoder_state* odd;
    const char* reason;
  } kinds[] = {{&structural, "decoder_state_mismatch.cfa_color_index"}, {&black_off, "decoder_black"}};
  // Tre frame, il diverso in prima, seconda e terza posizione.
  for (const auto& k : kinds) {
    for (std::size_t pos = 0; pos < 3; ++pos) {
      SCOPED_TRACE(std::string(k.reason) + " nel frame " + std::to_string(pos));
      std::vector<decoder_state> frames(3, base_state());
      frames[pos] = *k.odd;
      calibration_analysis_input in = raw_input();
      make_raw_master(*in.dark, frames);
      expect_all_chains(analyze_ok(in), master_role::dark, desc(in.dark), k.reason);
    }
  }
}

// Nero effettivo b(r, c) = black + black_per_color[FC(r, c)] + P(r, c) (H3), max |b_L − b_X| contro la tolleranza.
struct black_case {
  const char* name;
  decoder_state light;
  decoder_state master;
  std::uint32_t tol;
  bool excluded;
};

std::vector<black_case> black_cases() {
  std::vector<black_case> out;
  const decoder_state b = base_state();
  const auto with = [&](auto f) {
    decoder_state s = b;
    f(s);
    return s;
  };
  // Nero scalare: differenza uguale alla tolleranza passa, tolleranza + 1 esclude, in entrambe le direzioni.
  out.push_back({"scalare +tol", b, with([](decoder_state& s) { s.black = 512 + kTol; }), kTol, false});
  out.push_back({"scalare +tol+1", b, with([](decoder_state& s) { s.black = 512 + kTol + 1; }), kTol, true});
  out.push_back({"scalare -tol", b, with([](decoder_state& s) { s.black = 512 - kTol; }), kTol, false});
  out.push_back({"scalare -(tol+1)", b, with([](decoder_state& s) { s.black = 512 - kTol - 1; }), kTol, true});
  // Per colore: indice 2 (sito blu (1, 1)).
  out.push_back({"per colore tol", b, with([](decoder_state& s) { s.black_per_color = {0, 0, kTol, 0}; }), kTol, false});
  out.push_back({"per colore tol+1", b, with([](decoder_state& s) { s.black_per_color = {0, 0, kTol + 1, 0}; }), kTol, true});
  // Per colore con un indice che non compare nella cella (verdi entrambi con indice 1): la differenza non conta.
  const decoder_state g11 = with([](decoder_state& s) { s.cfa_color_index = {0, 1, 1, 2}; });
  decoder_state g11_off = g11;
  g11_off.black_per_color = {0, 0, 0, 1000};
  out.push_back({"per colore su indice assente dalla cella", g11, g11_off, kTol, false});
  // Motivo 2×3: elemento (1, 1).
  const decoder_state p23 = with([](decoder_state& s) {
    s.black_pattern_rows = 2;
    s.black_pattern_cols = 3;
    s.black_pattern = {0, 0, 0, 0, 0, 0};
  });
  decoder_state p23_tol = p23;
  p23_tol.black_pattern[4] = kTol;
  decoder_state p23_off = p23;
  p23_off.black_pattern[4] = kTol + 1;
  out.push_back({"motivo 2x3 tol", p23, p23_tol, kTol, false});
  out.push_back({"motivo 2x3 tol+1", p23, p23_off, kTol, true});
  // Motivo 1×6 su un'area visibile larga 4: le colonne 4 e 5 del motivo non cadono nell'area visibile.
  const decoder_state p16 = with([](decoder_state& s) {
    s.black_pattern_rows = 1;
    s.black_pattern_cols = 6;
    s.black_pattern = {0, 0, 0, 0, 0, 0};
  });
  decoder_state p16_out = p16;
  p16_out.black_pattern[5] = 1000;
  p16_out.black_pattern[4] = 1000;
  decoder_state p16_in = p16;
  p16_in.black_pattern[3] = kTol + 1;
  out.push_back({"motivo 1x6, colonne fuori dall'area visibile", p16, p16_out, kTol, false});
  out.push_back({"motivo 1x6, colonna 3 tol+1", p16, p16_in, kTol, true});
  // Si confronta il nero effettivo, non i campi: scomposizioni diverse dello stesso nero sono compatibili a tolleranza 0.
  decoder_state split_pattern = b;
  split_pattern.black = 502;
  split_pattern.black_pattern = {10, 10, 10, 10};
  out.push_back({"stesso nero, scalare contro motivo", b, split_pattern, 0, false});
  decoder_state split_color = b;
  split_color.black = 0;
  split_color.black_per_color = {512, 512, 512, 512};
  out.push_back({"stesso nero, scalare contro per colore", b, split_color, 0, false});
  decoder_state split_off = split_pattern;
  split_off.black_pattern[3] = 11;
  out.push_back({"motivo con un elemento diverso a tolleranza 0", b, split_off, 0, true});
  // Aritmetica intera a 64 bit: somma di tre uint32_t e differenze oltre 2^32.
  const decoder_state p11 = with([](decoder_state& s) {
    s.black_pattern_rows = 1;
    s.black_pattern_cols = 1;
    s.black_pattern = {0};
  });
  constexpr std::uint32_t kMax = std::numeric_limits<std::uint32_t>::max();
  decoder_state light_max = p11;
  light_max.black = kMax;  // b = 2^32 − 1
  decoder_state wide = p11;
  wide.black = 0;
  wide.black_per_color = {kMax, kMax, kMax, kMax};
  wide.black_pattern = {1};  // b = 2^32
  out.push_back({"64 bit: differenza 1 oltre 2^32, tolleranza 1", light_max, wide, 1, false});
  out.push_back({"64 bit: differenza 1 oltre 2^32, tolleranza 0", light_max, wide, 0, true});
  decoder_state zero = p11;
  zero.black = 0;
  decoder_state two32 = p11;
  two32.black = kMax;
  two32.black_pattern = {1};  // b = 2^32
  out.push_back({"64 bit: differenza 2^32 con tolleranza 2^32 − 1", zero, two32, kMax, true});
  decoder_state three = p11;
  three.black = kMax;
  three.black_per_color = {kMax, kMax, kMax, kMax};
  three.black_pattern = {kMax};  // b = 3 · (2^32 − 1)
  decoder_state three_less = three;
  three_less.black_pattern = {kMax - 5};
  out.push_back({"64 bit: somma di tre massimi, differenza 5, tolleranza 5", three, three_less, 5, false});
  out.push_back({"64 bit: somma di tre massimi, differenza 5, tolleranza 4", three, three_less, 4, true});
  return out;
}

TEST(CalibrationAnalysisU6, T_M3A_CAL_08_effective_black_against_decoder_black_abs_tol_adu_on_light_dark) {
  for (const black_case& c : black_cases()) {
    SCOPED_TRACE(c.name);
    calibration_analysis_input in = raw_input(c.light);
    in.declarations.decoder_black_abs_tol_adu = c.tol;
    make_raw_master(*in.dark, {c.master});
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::dark, desc(in.dark), c.excluded ? "decoder_black" : "");
    expect_all_chains(a, master_role::bias, desc(in.bias), "");
  }
}

TEST(CalibrationAnalysisU6, T_M3A_CAL_08_effective_black_on_every_additive_pair_and_never_on_light_flat) {
  decoder_state off = base_state();
  off.black = 512 + kTol + 1;
  {
    SCOPED_TRACE("L–D'");
    calibration_analysis_input in = raw_input();
    in.dark->descriptor = dark_desc(master_role::dark, additive_state::bias_subtracted);
    make_raw_master(*in.dark, {off});
    expect_all_chains(analyze_ok(in), master_role::dark, desc(in.dark), "decoder_black");
  }
  {
    SCOPED_TRACE("L–B e F–B (flat con il nero del light)");
    calibration_analysis_input in = raw_input();
    make_raw_master(*in.bias, {off});
    expect_all_chains(analyze_ok(in), master_role::bias, desc(in.bias), "decoder_black");
  }
  {
    SCOPED_TRACE("F–DF");
    calibration_analysis_input in = raw_input();
    make_raw_master(*in.dark_flat, {off});
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::dark_flat, desc(in.dark_flat), "decoder_black");
    expect_all_chains(a, master_role::flat, desc(in.flat), "");
  }
  {
    SCOPED_TRACE("F–DF'");
    calibration_analysis_input in = raw_input();
    in.dark_flat->descriptor = dark_desc(master_role::dark_flat, additive_state::bias_subtracted);
    make_raw_master(*in.dark_flat, {off});
    expect_all_chains(analyze_ok(in), master_role::dark_flat, desc(in.dark_flat), "decoder_black");
  }
  {
    // L–F non è additiva: il nero del flat diverso da quello del light non esclude il flat. Dark-flat e bias hanno il
    // nero del light, quindi F–DF e F–B escludono il dark-flat e il bias nelle sole catene del flat (§b.3).
    SCOPED_TRACE("L–F non confronta il nero; esclusione per catena del bias");
    calibration_analysis_input in = raw_input();
    decoder_state flat_black = base_state();
    flat_black.black = 512 + 1000;
    make_raw_master(*in.flat, {flat_black});
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::flat, desc(in.flat), "");
    expect_all_chains(a, master_role::dark_flat, desc(in.dark_flat), "decoder_black");
    expect_bias_excluded_only_in_flat_chains(a, desc(in.bias), "decoder_black");
    expect_all_chains(a, master_role::dark, desc(in.dark), "");
  }
  {
    // Dark-flat e flat col nero del flat, lontano da quello del light: F–DF compatibile, il dark-flat non si confronta
    // col light.
    SCOPED_TRACE("F–DF col nero del flat");
    calibration_analysis_input in = raw_input();
    decoder_state flat_black = base_state();
    flat_black.black = 512 + 1000;
    make_raw_master(*in.flat, {flat_black});
    make_raw_master(*in.dark_flat, {flat_black});
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::dark_flat, desc(in.dark_flat), "");
    expect_all_chains(a, master_role::flat, desc(in.flat), "");
  }
}

// L'ingresso dell'analisi non porta l'identità del decoder del light: qui si provano identità diverse fra i frame del
// master; il confronto light–master con versioni diverse e stato uguale è il controllo di T-M3A-CAL-09.
TEST(CalibrationAnalysisU6, T_M3A_CAL_08_white_data_maximum_linear_max_and_identity_are_not_compared) {
  decoder_state s = base_state();
  s.white = 4095;
  s.data_maximum = 3000;
  s.linear_max = std::array<std::int64_t, 4>{1, 2, 3, 4};
  calibration_analysis_input in = raw_input();
  make_raw_master(*in.dark, {s, base_state()});
  (*in.dark->descriptor.frame_decoder_states)[1].identity = identity_b();
  // offset_adu non si confronta fra ingressi con stato del decoder (§b.7): valori diversi presenti.
  in.light.metadata.offset_adu = hd(10.0, "OFFSET");
  in.dark->img.metadata.offset_adu = hd(99.0, "OFFSET");
  const calibration_analysis a = analyze_ok(in);
  expect_all_usable(a, in);
}

TEST(CalibrationAnalysisU6, T_M3A_CAL_08_decoder_state_mixed_excludes_the_master_and_not_the_others) {
  {
    SCOPED_TRACE("light RAW con un master esterno");
    calibration_analysis_input in = raw_input();
    in.dark = candidate(dark_desc(master_role::dark, additive_state::bias_included), plane_layout::cfa);
    in.dark->img.metadata.offset_adu.reset();
    ASSERT_FALSE(in.dark->descriptor.frame_decoder_states.has_value());
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::dark, desc(in.dark), "decoder_state_mixed");
    expect_all_chains(a, master_role::bias, desc(in.bias), "");
    expect_all_chains(a, master_role::flat, desc(in.flat), "");
  }
  {
    SCOPED_TRACE("light FITS con un master da RAW");
    calibration_analysis_input in = fits_input();
    in.light = frame_image(300.0, plane_layout::cfa);
    for (std::optional<analysis_candidate>* c : {&in.bias, &in.dark_flat, &in.flat}) {
      (*c)->img = frame_image(exposure_of((*c)->descriptor.role), plane_layout::cfa);
    }
    make_raw_master(*in.dark, {base_state()});
    in.dark->img.metadata.offset_adu = hd(10.0, "OFFSET");
    in.declarations.decoder_black_abs_tol_adu = kTol;  // obbligatorio: un ingresso ha stato del decoder
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::dark, desc(in.dark), "decoder_state_mixed");
    expect_all_chains(a, master_role::bias, desc(in.bias), "");
    expect_all_chains(a, master_role::dark_flat, desc(in.dark_flat), "");
    expect_all_chains(a, master_role::flat, desc(in.flat), "");
  }
}

TEST(CalibrationAnalysisU6, T_M3A_CAL_08_decoder_black_abs_tol_adu_is_mandatory_with_decoder_state_and_forbidden_without) {
  {
    SCOPED_TRACE("light RAW senza tolleranza");
    calibration_analysis_input in = raw_input();
    in.declarations.decoder_black_abs_tol_adu.reset();
    expect_analysis_stop(in, error_category::invalid_input, "calib.param.decoder_black_abs_tol_adu");
  }
  {
    SCOPED_TRACE("light FITS e un solo master da RAW, senza tolleranza");
    calibration_analysis_input in = fits_input();
    make_raw_master(*in.dark, {base_state()});
    in.declarations.decoder_black_abs_tol_adu.reset();
    expect_analysis_stop(in, error_category::invalid_input, "calib.param.decoder_black_abs_tol_adu");
  }
  {
    SCOPED_TRACE("tutto FITS con tolleranza");
    calibration_analysis_input in = fits_input();
    in.declarations.decoder_black_abs_tol_adu = kTol;
    expect_analysis_stop(in, error_category::invalid_input, "calib.param_not_applicable.decoder_black_abs_tol_adu");
  }
  {
    SCOPED_TRACE("tolleranza 0 ammessa");
    calibration_analysis_input in = raw_input();
    in.declarations.decoder_black_abs_tol_adu = 0;
    expect_all_usable(analyze_ok(in), in);
  }
}

// ================================================================ rami della dichiarazione, dei ruoli e delle coppie
// (ADR-16.b §b.2, §b.3, §b.7 con le decisioni del coordinatore; C-01.b §2, §7)

constexpr master_role kRoles[] = {master_role::bias, master_role::dark, master_role::dark_flat, master_role::flat};

master_descriptor descriptor_for(master_role r) {
  switch (r) {
    case master_role::bias: return bias_desc();
    case master_role::dark:
    case master_role::dark_flat: return dark_desc(r, additive_state::bias_included);
    case master_role::flat: return flat_desc(flat_state::raw);
  }
  std::abort();
}

std::string context_value(const stop_error& e, const std::string& key) {
  for (const auto& [k, v] : e.context()) {
    if (k == key) return v;
  }
  return "<chiave assente>";
}

using context_pairs = std::vector<std::pair<std::string, std::string>>;

// Stop dell'analisi con categoria, codice e chiavi di contesto esatti; zero calcoli.
void expect_analysis_stop_with(const calibration_analysis_input& in, error_category cat, const std::string& code,
                               const context_pairs& want) {
  call_counter counter;
  try {
    (void)analyze_calibration(in, counter.injector());
    ADD_FAILURE() << "stop_error atteso, nessuna eccezione: " << code;
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), cat) << e.what();
    EXPECT_EQ(e.code(), code) << e.what();
    for (const auto& [k, v] : want) EXPECT_EQ(context_value(e, k), v) << "chiave di contesto " << k;
  }
  EXPECT_EQ(counter.count(), 0) << "calcoli prima dello stop " << code;
}

// Come fits_input, ma con light e master in layout CFA: i candidati da RAW (cfa) si confrontano con i FITS.
calibration_analysis_input fits_cfa_input() {
  calibration_analysis_input in = fits_input();
  in.light = frame_image(300.0, plane_layout::cfa);
  for (const master_role r : kRoles) slot(in, r)->img = frame_image(exposure_of(r), plane_layout::cfa);
  return in;
}

// fits_input con i soli ruoli indicati.
calibration_analysis_input fits_input_with(const std::vector<master_role>& keep) {
  calibration_analysis_input in = fits_input();
  for (const master_role r : kRoles) {
    if (std::find(keep.begin(), keep.end(), r) == keep.end()) slot(in, r).reset();
  }
  return in;
}

// Ogni ruolo presente, tranne `except`, è `usable` in ogni catena.
void expect_usable_except(const calibration_analysis& a, calibration_analysis_input& in,
                          std::initializer_list<master_role> except) {
  for (const master_role r : kRoles) {
    if (!slot(in, r)) continue;
    if (std::find(except.begin(), except.end(), r) != except.end()) continue;
    expect_all_chains(a, r, desc(slot(in, r)), "");
  }
}

// ---------------------------------------------------------------- calib.role_slot

TEST(CalibrationAnalysisU6, T_M3A_C01_06_master_in_the_slot_of_another_role_stops_with_role_slot) {
  for (const master_role r : kRoles) {
    for (const master_role w : kRoles) {
      if (w == r) continue;
      SCOPED_TRACE(std::string("slot ") + role_name(r) + ", descrittore " + role_name(w));
      calibration_analysis_input in = fits_input();
      slot(in, r)->descriptor = descriptor_for(w);
      expect_analysis_stop_with(in, error_category::invalid_input, "calib.role_slot",
                                {{"slot", role_name(r)}, {"ruolo_dichiarato", role_name(w)}});
    }
  }
}

// ---------------------------------------------------------------- calib.param_not_applicable.flat_normalization_declared

TEST(CalibrationAnalysisU6, T_M3A_C01_06_flat_normalization_declared_on_a_master_that_is_not_a_calibrated_flat_stops) {
  for (const normalization_mode mode : {normalization_mode::global_median, normalization_mode::cfa_channel_median}) {
    for (const master_role r : {master_role::bias, master_role::dark, master_role::dark_flat}) {
      SCOPED_TRACE(std::string("dichiarata su ") + role_name(r));
      calibration_analysis_input in = fits_input();
      slot(in, r)->descriptor.flat_normalization_declared = mode;
      expect_analysis_stop_with(in, error_category::invalid_input, "calib.param_not_applicable.flat_normalization_declared",
                                {{"role", role_name(r)}});
    }
    for (const flat_state s : {flat_state::raw, flat_state::corrected_unnormalized}) {
      SCOPED_TRACE(std::string("flat in stato ") + (s == flat_state::raw ? "raw" : "corrected_unnormalized"));
      calibration_analysis_input in = fits_input();
      in.flat = candidate(flat_desc(s));
      in.flat->descriptor.flat_normalization_declared = mode;
      expect_analysis_stop_with(in, error_category::invalid_input, "calib.param_not_applicable.flat_normalization_declared",
                                {{"role", "flat"}});
    }
  }
  {
    SCOPED_TRACE("controllo positivo: flat calibrated con la dichiarazione");
    calibration_analysis_input in = fits_input();
    in.flat = candidate(flat_desc(flat_state::calibrated));
    ASSERT_TRUE(in.flat->descriptor.flat_normalization_declared.has_value());
    expect_all_chains(analyze_ok(in), master_role::flat, desc(in.flat), "");
  }
}

// ---------------------------------------------------------------- frame_decoder_states vuoto

TEST(CalibrationAnalysisU6, T_M3A_CAL_08_raw_master_with_empty_frame_decoder_states_is_excluded_in_every_chain) {
  const std::string reason = "metadata_missing.frame_decoder_states";
  for (const master_role r : kRoles) {
    SCOPED_TRACE(role_name(r));
    calibration_analysis_input in = raw_input();
    make_raw_master(*slot(in, r), {});
    ASSERT_TRUE(desc(slot(in, r)).frame_decoder_states.has_value());
    ASSERT_TRUE(desc(slot(in, r)).frame_decoder_states->empty());
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, r, desc(slot(in, r)), reason);
    if (r == master_role::flat) {
      // F–DF e F–B: il flat di riferimento non ha stato da confrontare; il bias resta usabile nelle catene del light.
      expect_all_chains(a, master_role::dark_flat, desc(in.dark_flat), reason);
      expect_bias_excluded_only_in_flat_chains(a, desc(in.bias), reason);
      expect_all_chains(a, master_role::dark, desc(in.dark), "");
    } else {
      expect_usable_except(a, in, {r});
    }
  }
}

// ADR-16.b §b.7: l'elenco vuoto si controlla prima del confronto col light, quindi vale anche quando il light è FITS
// (altrimenti il motivo sarebbe `decoder_state_mixed`).
TEST(CalibrationAnalysisU6, T_M3A_CAL_08_empty_frame_decoder_states_is_checked_before_decoder_state_mixed) {
  for (const master_role r : kRoles) {
    SCOPED_TRACE(std::string("light FITS, ") + role_name(r) + " da RAW senza stati");
    calibration_analysis_input in = fits_cfa_input();
    make_raw_master(*slot(in, r), {});
    in.declarations.decoder_black_abs_tol_adu = kTol;
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, r, desc(slot(in, r)), "metadata_missing.frame_decoder_states");
    if (r != master_role::flat) expect_usable_except(a, in, {r});
  }
}

// ---------------------------------------------------------------- flat senza stato del decoder nelle coppie F–X

TEST(CalibrationAnalysisU6, T_M3A_CAL_08_flat_without_decoder_state_makes_the_flat_pairs_decoder_state_mixed) {
  const std::string reason = "decoder_state_mixed";
  {
    SCOPED_TRACE("light RAW, flat esterno: F–DF e F–B escludono dark-flat e bias nelle sole catene del flat");
    calibration_analysis_input in = raw_input();
    in.flat = candidate(flat_desc(flat_state::raw), plane_layout::cfa);
    in.flat->img.metadata.offset_adu.reset();
    ASSERT_FALSE(in.flat->descriptor.frame_decoder_states.has_value());
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::flat, desc(in.flat), reason);
    expect_all_chains(a, master_role::dark_flat, desc(in.dark_flat), reason);
    expect_bias_excluded_only_in_flat_chains(a, desc(in.bias), reason);
    expect_all_chains(a, master_role::dark, desc(in.dark), "");
  }
  {
    SCOPED_TRACE("light FITS, flat da RAW con stati: dark-flat e bias FITS, `offset_adu` del flat assente non conta");
    calibration_analysis_input in = fits_cfa_input();
    make_raw_master(*in.flat, {base_state()});
    in.declarations.decoder_black_abs_tol_adu = kTol;
    ASSERT_FALSE(in.flat->img.metadata.offset_adu.has_value());
    ASSERT_TRUE(in.dark_flat->img.metadata.offset_adu.has_value());
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::flat, desc(in.flat), reason);
    // Con un ingresso RAW nella coppia §b.7 sostituisce il confronto di `offset_adu`: il motivo è il decoder, non
    // `metadata_missing.offset_adu`.
    expect_all_chains(a, master_role::dark_flat, desc(in.dark_flat), reason);
    expect_bias_excluded_only_in_flat_chains(a, desc(in.bias), reason);
    expect_all_chains(a, master_role::dark, desc(in.dark), "");
  }
}

// ---------------------------------------------------------------- offset_adu coppia per coppia

TEST(CalibrationAnalysisU6, T_M3A_CAL_08_offset_adu_is_compared_pair_by_pair_without_decoder_state) {
  const std::string differs = "incompatible.offset_adu";
  const std::string missing = "metadata_missing.offset_adu";
  {
    SCOPED_TRACE("L–D");
    calibration_analysis_input in = fits_input();
    in.dark->img.metadata.offset_adu = hd(99.0, "OFFSET");
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::dark, desc(in.dark), differs);
    expect_usable_except(a, in, {master_role::dark});
  }
  {
    SCOPED_TRACE("F–DF");
    calibration_analysis_input in = fits_input();
    in.dark_flat->img.metadata.offset_adu = hd(99.0, "OFFSET");
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::dark_flat, desc(in.dark_flat), differs);
    expect_usable_except(a, in, {master_role::dark_flat});
  }
  {
    SCOPED_TRACE("L–B e F–B");
    calibration_analysis_input in = fits_input();
    in.bias->img.metadata.offset_adu = hd(99.0, "OFFSET");
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::bias, desc(in.bias), differs);
    expect_usable_except(a, in, {master_role::bias});
  }
  {
    SCOPED_TRACE("F–B differisce, L–B no: il bias è escluso nelle sole catene del flat");
    calibration_analysis_input in = fits_input();
    in.flat->img.metadata.offset_adu = hd(99.0, "OFFSET");
    in.dark_flat->img.metadata.offset_adu = hd(99.0, "OFFSET");  // F–DF uguale
    const calibration_analysis a = analyze_ok(in);
    expect_bias_excluded_only_in_flat_chains(a, desc(in.bias), differs);
    expect_usable_except(a, in, {master_role::bias});
  }
  {
    SCOPED_TRACE("L–F non confronta `offset_adu`");
    calibration_analysis_input in = fits_input_with({master_role::flat});
    in.flat->img.metadata.offset_adu = hd(99.0, "OFFSET");
    expect_all_chains(analyze_ok(in), master_role::flat, desc(in.flat), "");
  }
  {
    SCOPED_TRACE("assente nel light: L–D e L–B");
    calibration_analysis_input in = fits_input();
    in.light.metadata.offset_adu.reset();
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::dark, desc(in.dark), missing);
    expect_chain_set(a, master_role::bias, desc(in.bias));
    expect_chain(a, master_role::bias, "light.bias", missing);
    expect_chain(a, master_role::bias, "light.bias_dark_sub", missing);
    expect_chain(a, master_role::bias, "flat.bias_dark_flat_sub", "");
    expect_chain(a, master_role::bias, "flat.bias", "");
    expect_usable_except(a, in, {master_role::dark, master_role::bias});
  }
  {
    SCOPED_TRACE("assente nel dark-flat: F–DF");
    calibration_analysis_input in = fits_input();
    in.dark_flat->img.metadata.offset_adu.reset();
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::dark_flat, desc(in.dark_flat), missing);
    expect_usable_except(a, in, {master_role::dark_flat});
  }
}

// ---------------------------------------------------------------- IMAGETYP, ingresso condiviso

TEST(CalibrationAnalysisU6, T_M3A_C01_06_master_imagetyp_of_another_role_is_excluded_in_every_chain) {
  const struct {
    master_role role;
    const char* imagetyp;
  } conflicts[] = {{master_role::bias, "Dark Frame"},
                   {master_role::dark, "Flat Field"},
                   {master_role::dark, "Flat Dark"},
                   {master_role::dark_flat, "Light Frame"},
                   {master_role::dark_flat, "Bias Frame"},
                   {master_role::flat, "Bias Frame"}};
  for (const auto& c : conflicts) {
    SCOPED_TRACE(std::string(role_name(c.role)) + " con IMAGETYP " + c.imagetyp);
    calibration_analysis_input in = fits_input();
    slot(in, c.role)->img.metadata.imagetyp = hs(c.imagetyp, "IMAGETYP");
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, c.role, desc(slot(in, c.role)), "imagetyp_conflict");
    expect_usable_except(a, in, {c.role});
  }
  const struct {
    master_role role;
    const char* imagetyp;
  } fine[] = {{master_role::bias, "Bias Frame"},   {master_role::dark, "dark"},        {master_role::dark_flat, "Dark Frame"},
              {master_role::dark_flat, "Flat Dark"}, {master_role::flat, "Flat Field"}};
  for (const auto& c : fine) {
    SCOPED_TRACE(std::string("controllo positivo: ") + role_name(c.role) + " con IMAGETYP " + c.imagetyp);
    calibration_analysis_input in = fits_input();
    slot(in, c.role)->img.metadata.imagetyp = hs(c.imagetyp, "IMAGETYP");
    const calibration_analysis a = analyze_ok(in);
    expect_usable_except(a, in, {});
    EXPECT_TRUE(a.warnings.empty());
  }
}

TEST(CalibrationAnalysisU6, T_M3A_C01_06_unrecognized_imagetyp_is_a_warning_and_never_excludes) {
  calibration_analysis_input in = fits_input();
  in.dark->img.metadata.imagetyp = hs("Mystery", "IMAGETYP");
  in.flat->img.metadata.imagetyp = hs("Other", "IMAGETYP");
  const calibration_analysis a = analyze_ok(in);
  expect_usable_except(a, in, {});
  EXPECT_NE(std::find(a.warnings.begin(), a.warnings.end(), "imagetyp_unrecognized:dark:Mystery"), a.warnings.end());
  EXPECT_NE(std::find(a.warnings.begin(), a.warnings.end(), "imagetyp_unrecognized:flat:Other"), a.warnings.end());
}

TEST(CalibrationAnalysisU6, T_M3A_C01_06_light_imagetyp_of_another_role_stops_and_an_unrecognized_one_warns) {
  {
    calibration_analysis_input in = fits_input();
    in.light.metadata.imagetyp = hs("Dark Frame", "IMAGETYP");
    expect_analysis_stop_with(in, error_category::incompatible, "calib.imagetyp_conflict",
                              {{"input", "light"}, {"imagetyp", "Dark Frame"}, {"ruolo_atteso", "light"}});
  }
  {
    calibration_analysis_input in = fits_input();
    in.light.metadata.imagetyp = hs("Mystery", "IMAGETYP");
    const calibration_analysis a = analyze_ok(in);
    expect_usable_except(a, in, {});
    EXPECT_NE(std::find(a.warnings.begin(), a.warnings.end(), "imagetyp_unrecognized:light:Mystery"), a.warnings.end());
  }
}

TEST(CalibrationAnalysisU6, T_M3A_CAL_08_master_with_the_digest_of_the_light_or_of_another_master_is_excluded_as_shared) {
  for (const master_role r : kRoles) {
    SCOPED_TRACE(std::string(role_name(r)) + " con il digest del light");
    calibration_analysis_input in = fits_input();
    slot(in, r)->digest = in.light_digest;
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, r, desc(slot(in, r)), "shared_input");
    expect_usable_except(a, in, {r});
  }
  for (std::size_t i = 0; i < 4; ++i) {
    for (std::size_t j = i + 1; j < 4; ++j) {
      SCOPED_TRACE(std::string(role_name(kRoles[i])) + " e " + role_name(kRoles[j]) + " con lo stesso digest");
      calibration_analysis_input in = fits_input();
      slot(in, kRoles[j])->digest = slot(in, kRoles[i])->digest;
      const calibration_analysis a = analyze_ok(in);
      expect_all_chains(a, kRoles[i], desc(slot(in, kRoles[i])), "shared_input");
      expect_all_chains(a, kRoles[j], desc(slot(in, kRoles[j])), "shared_input");
      expect_usable_except(a, in, {kRoles[i], kRoles[j]});
    }
  }
}

// ---------------------------------------------------------------- metadata_missing.<campo> di coppia

struct field_edit {
  const char* field;
  void (*drop)(image&);
};

const std::vector<field_edit>& additive_field_edits() {
  static const std::vector<field_edit> edits = {
      {"binning_x", [](image& m) { m.geometry.binning_x.reset(); }},
      {"binning_y", [](image& m) { m.geometry.binning_y.reset(); }},
      {"camera", [](image& m) { m.metadata.camera.reset(); }},
      {"gain", [](image& m) { m.metadata.gain.reset(); }},
      {"offset_adu", [](image& m) { m.metadata.offset_adu.reset(); }},
      {"exposure_s", [](image& m) { m.metadata.exposure_s.reset(); }},
      {"temperature_c", [](image& m) { m.metadata.temperature_c.reset(); }},
  };
  return edits;
}

TEST(CalibrationAnalysisU6, T_M3A_CAL_08_missing_field_of_a_master_excludes_it_with_metadata_missing_of_that_field) {
  for (const master_role r : {master_role::bias, master_role::dark, master_role::dark_flat}) {
    for (const field_edit& e : additive_field_edits()) {
      const std::string field = e.field;
      SCOPED_TRACE(std::string(role_name(r)) + " senza " + field);
      calibration_analysis_input in = fits_input();
      e.drop(slot(in, r)->img);
      const calibration_analysis a = analyze_ok(in);
      if (r == master_role::bias && field == "exposure_s") {
        // Le coppie L–B e F–B non confrontano l'esposizione.
        expect_usable_except(a, in, {});
        continue;
      }
      expect_all_chains(a, r, desc(slot(in, r)), "metadata_missing." + field);
      expect_usable_except(a, in, {r});
    }
  }
}

TEST(CalibrationAnalysisU6, T_M3A_CAL_08_missing_field_of_the_light_excludes_the_masters_paired_with_it) {
  {
    SCOPED_TRACE("light senza gain: L–D e L–B");
    calibration_analysis_input in = fits_input();
    in.light.metadata.gain.reset();
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::dark, desc(in.dark), "metadata_missing.gain");
    expect_chain_set(a, master_role::bias, desc(in.bias));
    expect_chain(a, master_role::bias, "light.bias", "metadata_missing.gain");
    expect_chain(a, master_role::bias, "light.bias_dark_sub", "metadata_missing.gain");
    expect_chain(a, master_role::bias, "flat.bias_dark_flat_sub", "");
    expect_chain(a, master_role::bias, "flat.bias", "");
    // L–F non è additiva: niente gain.
    expect_usable_except(a, in, {master_role::dark, master_role::bias});
  }
  {
    SCOPED_TRACE("light senza camera: anche L–F");
    calibration_analysis_input in = fits_input();
    in.light.metadata.camera.reset();
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::dark, desc(in.dark), "metadata_missing.camera");
    expect_all_chains(a, master_role::flat, desc(in.flat), "metadata_missing.camera");
    expect_chain(a, master_role::bias, "light.bias", "metadata_missing.camera");
    expect_chain(a, master_role::bias, "light.bias_dark_sub", "metadata_missing.camera");
    expect_chain(a, master_role::bias, "flat.bias_dark_flat_sub", "");
    expect_chain(a, master_role::bias, "flat.bias", "");
    expect_all_chains(a, master_role::dark_flat, desc(in.dark_flat), "");
  }
}

TEST(CalibrationAnalysisU6, T_M3A_CAL_08_light_flat_pair_compares_the_filter_and_no_additive_field) {
  {
    SCOPED_TRACE("filter assente nel flat");
    calibration_analysis_input in = fits_input();
    in.flat->img.metadata.filter.reset();
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::flat, desc(in.flat), "metadata_missing.filter");
    expect_usable_except(a, in, {master_role::flat});
  }
  {
    SCOPED_TRACE("filter diverso");
    calibration_analysis_input in = fits_input();
    in.flat->img.metadata.filter = hs("R", "FILTER");
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::flat, desc(in.flat), "incompatible.filter");
    expect_usable_except(a, in, {master_role::flat});
  }
  {
    SCOPED_TRACE("camera assente nel flat: anche le coppie F–DF e F–B che lo usano come riferimento");
    calibration_analysis_input in = fits_input();
    in.flat->img.metadata.camera.reset();
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::flat, desc(in.flat), "metadata_missing.camera");
    expect_all_chains(a, master_role::dark_flat, desc(in.dark_flat), "metadata_missing.camera");
    expect_bias_excluded_only_in_flat_chains(a, desc(in.bias), "metadata_missing.camera");
    expect_all_chains(a, master_role::dark, desc(in.dark), "");
  }
  {
    SCOPED_TRACE("gain, offset, esposizione e temperatura del flat non si confrontano con il light");
    calibration_analysis_input in = fits_input_with({master_role::flat});
    in.flat->img.metadata.gain = hd(1.0, "GAIN");
    in.flat->img.metadata.offset_adu.reset();
    in.flat->img.metadata.exposure_s.reset();
    in.flat->img.metadata.temperature_c = hd(30.0, "CCD-TEMP");
    expect_all_chains(analyze_ok(in), master_role::flat, desc(in.flat), "");
  }
}

// ---------------------------------------------------------------- dichiarazioni richieste dalle coppie presenti

// Ruoli presenti, e se la coppia richiede esposizione e criterio di temperatura (ADR-16.a §2, §b.3).
struct required_case {
  std::vector<master_role> roles;
  bool exposure;
  bool temperature;
};

const std::vector<required_case> kRequired = {
    {{master_role::dark}, true, true},
    {{master_role::bias}, false, true},
    {{master_role::dark_flat}, false, false},
    {{master_role::flat}, false, false},
    {{master_role::dark_flat, master_role::flat}, true, true},
    {{master_role::bias, master_role::flat}, false, true},
    {{master_role::bias, master_role::dark_flat}, false, true},
};

std::string roles_text(const std::vector<master_role>& roles) {
  std::string out;
  for (const master_role r : roles) out += std::string(out.empty() ? "" : "+") + role_name(r);
  return out;
}

// Analisi che deve completarsi senza candidati assenti dall'esito; non richiede chiamate al contatore (con il solo
// dark-flat nessuna coppia si valuta).
void expect_completes_with_all_candidates(calibration_analysis_input& in) {
  calibration_analysis a;
  ASSERT_NO_THROW(a = analyze_calibration(in));
  for (const master_role r : kRoles) {
    if (!slot(in, r)) continue;
    EXPECT_NE(find_candidate(a, r), nullptr) << role_name(r);
  }
}

TEST(CalibrationAnalysisU6, T_M3A_C01_06_exposure_rel_tol_is_required_only_by_a_pair_that_compares_the_exposure) {
  for (const required_case& c : kRequired) {
    SCOPED_TRACE(roles_text(c.roles));
    calibration_analysis_input in = fits_input_with(c.roles);
    in.declarations.exposure_rel_tol.reset();
    if (c.exposure) {
      expect_analysis_stop_with(in, error_category::invalid_input, "calib.param.exposure_rel_tol",
                                {{"parametro", "exposure_rel_tol"}, {"motivo", "assente"}});
    } else {
      expect_completes_with_all_candidates(in);
    }
  }
}

TEST(CalibrationAnalysisU6, T_M3A_C01_06_exposure_rel_tol_must_be_finite_and_not_negative) {
  const struct {
    double value;
    const char* why;
  } bad[] = {{std::numeric_limits<double>::quiet_NaN(), "non finito"},
             {std::numeric_limits<double>::infinity(), "non finito"},
             {-0.5, "deve essere >= 0"}};
  for (const auto& b : bad) {
    SCOPED_TRACE(b.why);
    calibration_analysis_input in = fits_input();
    in.declarations.exposure_rel_tol = b.value;
    expect_analysis_stop_with(in, error_category::invalid_input, "calib.param.exposure_rel_tol",
                              {{"parametro", "exposure_rel_tol"}, {"motivo", b.why}});
  }
  {
    SCOPED_TRACE("zero ammesso: esposizioni uguali");
    calibration_analysis_input in = fits_input();
    in.declarations.exposure_rel_tol = 0.0;
    expect_usable_except(analyze_ok(in), in, {});
  }
}

TEST(CalibrationAnalysisU6, T_M3A_CAL_08_exposure_rel_tol_decides_the_exposure_pairs) {
  calibration_analysis_input in = fits_input();
  in.dark->img.metadata.exposure_s = hd(306.0, "EXPTIME");       // +2% rispetto al light (300 s)
  in.dark_flat->img.metadata.exposure_s = hd(2.04, "EXPTIME");   // +2% rispetto al flat (2 s)
  in.declarations.exposure_rel_tol = 0.01;
  {
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::dark, desc(in.dark), "incompatible.exposure_s");
    expect_all_chains(a, master_role::dark_flat, desc(in.dark_flat), "incompatible.exposure_s");
    expect_usable_except(a, in, {master_role::dark, master_role::dark_flat});
  }
  in.declarations.exposure_rel_tol = 0.05;
  expect_usable_except(analyze_ok(in), in, {});
  in.declarations.exposure_rel_tol = 0.01;
  in.dark->img.metadata.exposure_s.reset();
  expect_all_chains(analyze_ok(in), master_role::dark, desc(in.dark), "metadata_missing.exposure_s");
}

TEST(CalibrationAnalysisU6, T_M3A_C01_06_temperature_policy_is_required_by_every_additive_pair_with_a_candidate) {
  for (const required_case& c : kRequired) {
    SCOPED_TRACE(roles_text(c.roles));
    calibration_analysis_input in = fits_input_with(c.roles);
    in.declarations.temp_policy.reset();
    if (c.temperature) {
      expect_analysis_stop_with(in, error_category::invalid_input, "calib.param.temperature_policy",
                                {{"parametro", "temperature_policy"}, {"motivo", "assente"}});
    } else {
      expect_completes_with_all_candidates(in);
    }
  }
  {
    SCOPED_TRACE("senza esposizione dichiarata il primo stop è quello dell'esposizione");
    calibration_analysis_input in = fits_input();
    in.declarations.exposure_rel_tol.reset();
    in.declarations.temp_policy.reset();
    expect_analysis_stop_with(in, error_category::invalid_input, "calib.param.exposure_rel_tol",
                              {{"parametro", "exposure_rel_tol"}});
  }
}

TEST(CalibrationAnalysisU6, T_M3A_C01_06_temperature_abs_tol_c_is_required_finite_and_not_negative_with_compare) {
  const struct {
    std::optional<double> value;
    const char* why;
  } bad[] = {{std::nullopt, "assente"},
             {std::numeric_limits<double>::quiet_NaN(), "non finito"},
             {-std::numeric_limits<double>::infinity(), "non finito"},
             {-1.0, "deve essere >= 0"}};
  for (const auto& b : bad) {
    SCOPED_TRACE(b.why);
    calibration_analysis_input in = fits_input();
    in.declarations.temperature_abs_tol_c = b.value;
    expect_analysis_stop_with(in, error_category::invalid_input, "calib.param.temperature_abs_tol_c",
                              {{"parametro", "temperature_abs_tol_c"}, {"motivo", b.why}});
  }
  for (const required_case& c : kRequired) {
    if (!c.temperature) {
      SCOPED_TRACE(std::string("non richiesta con ") + roles_text(c.roles));
      calibration_analysis_input in = fits_input_with(c.roles);
      in.declarations.temperature_abs_tol_c.reset();
      expect_completes_with_all_candidates(in);
    }
  }
  {
    SCOPED_TRACE("zero ammesso");
    calibration_analysis_input in = fits_input();
    in.declarations.temperature_abs_tol_c = 0.0;
    expect_usable_except(analyze_ok(in), in, {});
  }
}

TEST(CalibrationAnalysisU6, T_M3A_CAL_08_temperature_policy_compare_or_skipped) {
  calibration_analysis_input in = fits_input();
  in.dark->img.metadata.temperature_c = hd(-7.0, "CCD-TEMP");  // 3 °C sopra il light
  in.declarations.temperature_abs_tol_c = 1.0;
  {
    SCOPED_TRACE("compare: oltre la tolleranza");
    const calibration_analysis a = analyze_ok(in);
    expect_all_chains(a, master_role::dark, desc(in.dark), "incompatible.temperature_c");
    expect_usable_except(a, in, {master_role::dark});
    EXPECT_TRUE(a.warnings.empty());
  }
  in.declarations.temperature_abs_tol_c = 3.0;
  {
    SCOPED_TRACE("compare: sul limite");
    expect_usable_except(analyze_ok(in), in, {});
  }
  in.declarations.temperature_abs_tol_c = 1.0;
  in.dark->img.metadata.temperature_c.reset();
  {
    SCOPED_TRACE("compare: temperatura assente");
    expect_all_chains(analyze_ok(in), master_role::dark, desc(in.dark), "metadata_missing.temperature_c");
  }
  {
    SCOPED_TRACE("temperatura non applicabile dichiarata: la coppia non confronta e il report lo dice");
    in.declarations.temp_policy = temperature_policy::not_applicable_declared;
    const calibration_analysis a = analyze_ok(in);
    expect_usable_except(a, in, {});
    for (const char* pair : {"L-B", "L-D", "F-DF", "F-B"}) {
      const std::string w = std::string("temperature_comparison_skipped:") + pair;
      EXPECT_NE(std::find(a.warnings.begin(), a.warnings.end(), w), a.warnings.end()) << w;
    }
    EXPECT_EQ(std::count(a.warnings.begin(), a.warnings.end(), "temperature_comparison_skipped:L-F"), 0);
  }
}

// ---------------------------------------------------------------- scrittore FITS e contenuto equivalente con coverage

// Il prodotto non scrive ancora l'HDU COVERAGE: la coverage non si perde in silenzio. Nome senza ID del registro: il
// contratto di scrittura è di T-M3A-TYP-07 (M3.g).
TEST(CalibrationAnalysisU6, fits_encoder_refuses_an_image_with_coverage) {
  for (const double value : {0.5, 1.0}) {
    SCOPED_TRACE(value);
    image with = frame_image(300.0);
    add_coverage(with, value);
    EXPECT_STOP(encode_fits(with), error_category::unsupported, "fits.coverage");
  }
  const image without = frame_image(300.0);
  EXPECT_NO_THROW(encode_fits(without));
}

TEST(CalibrationAnalysisU6, fits_stream_writer_refuses_an_image_with_coverage) {
  for (const bool with_invalid : {false, true}) {
    SCOPED_TRACE(with_invalid);
    image with = frame_image(300.0);
    add_coverage(with, 0.5);
    EXPECT_STOP(fits_stream_writer(with, with_invalid), error_category::unsupported, "fits.coverage");
    const image without = frame_image(300.0);
    EXPECT_NO_THROW(fits_stream_writer(without, with_invalid));
  }
}

TEST(CalibrationAnalysisU6, equivalent_content_distinguishes_the_coverage_plane) {
  const image base = frame_image(300.0);
  image half = base;
  add_coverage(half, 0.5);
  image half_again = base;
  add_coverage(half_again, 0.5);
  image one = base;
  add_coverage(one, 1.0);
  EXPECT_FALSE(equivalent_content(base, half));
  EXPECT_FALSE(equivalent_content(half, base));
  EXPECT_FALSE(equivalent_content(base, one));  // coverage tutta a 1 non equivale a coverage assente
  EXPECT_FALSE(equivalent_content(half, one));
  EXPECT_TRUE(equivalent_content(half, half_again));
  EXPECT_TRUE(equivalent_content(base, base));
  image edited = half_again;
  edited.coverage->samples.as<double>()[3] = 0.25;
  EXPECT_FALSE(equivalent_content(half, edited));
}

}  // namespace
