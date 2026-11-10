// M3.b, unità B3: tipi, ricetta, domini, fase e linearità del master (C-02.b §b.1, §b.2, §b.7, §b.10, §b.13 punti 1 e 3;
// C-02.b-flat §f.2, §f.7, §f.13). Test: T-M3B-C02-12, T-M3B-C02-09 (domini), T-M3B-C02-03(c), T-M3B-DSC-01,
// T-M3B-FLAT-06 (parte parametri), T-M3B-C02-15 (solo `master.param.<nome>`).
//
// Nessun valore atteso viene dal prodotto: codici, categorie e valori del descrittore sono scritti qui dai paragrafi
// citati. Gli stop di ricetta sono controllati su `validate_master_recipe` e su `make_master_node` (nessun nodo).
// Ordine di controllo assunto: coppia (`combine`, `rejection`), `n` e ruolo prima dei parametri del metodo (le prove sui
// parametri usano sempre una coppia e un `n` ammessi).
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "astro/batch_classification.hpp"
#include "master_support.hpp"
#include "store_test_support.hpp"

namespace {

using namespace mst;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// ---------------------------------------------------------------- ricette senza file

content_digest fake_digest(std::size_t k) {
  const std::vector<std::uint8_t> b = {'B', '3', static_cast<std::uint8_t>(k & 0xff), static_cast<std::uint8_t>(k >> 8)};
  return digest_of(b);
}

// Ricetta valida per il ruolo con `n` frame distinti, senza file (le prove di dominio non toccano i campioni).
master_recipe bare(master_role role, std::size_t n) {
  master_recipe r;
  r.role = role;
  for (std::size_t k = 0; k < n; ++k) {
    master_frame_spec s;
    s.digest = fake_digest(k);
    r.frames.push_back(s);
  }
  r.combine = combine_method::mean;
  r.rejection = master_rejection::none;
  if (role == master_role::flat) {
    r.flat_norm = flat_normalization::none;
    r.flat_level_rel_tol = 0.25;
    r.flat_min_signal_adu = 1.0;
  }
  r.exposure_rel_tol = 1e-9;
  r.temperature_abs_tol_c = 0.5;
  r.temp_policy = temperature_policy::compare;
  r.simd = simd_test::requested();
  r.output_type = sample_type::f64;
  return r;
}

void use_single(master_recipe& r) {
  r.combine = combine_method::single;
  r.rejection = master_rejection::none;
  r.offset_adu = 10;
  r.conversion_gain_e_per_adu = 1.5;
  r.read_noise_adu = 2;
}
void use_weighted(master_recipe& r, const std::vector<double>& w) {
  r.combine = combine_method::weighted_mean;
  r.rejection = master_rejection::none;
  for (std::size_t k = 0; k < w.size(); ++k) r.frames[k].weight = w[k];
}
void use_median(master_recipe& r) {
  r.combine = combine_method::median;
  r.rejection = master_rejection::none;
}
void use_winsorized(master_recipe& r, double g) {
  r.combine = combine_method::winsorized;
  r.rejection = master_rejection::none;
  r.winsor_g = g;
}
void use_sigma(master_recipe& r, double lo = 3, double hi = 3, double iter = 1, double kept = 2) {
  r.combine = combine_method::mean;
  r.rejection = master_rejection::sigma_clip;
  r.clip_kappa_low = lo;
  r.clip_kappa_high = hi;
  r.clip_max_iter = iter;
  r.clip_min_kept = kept;
}

#define EXPECT_RECIPE_STOP(recipe, cat, code)             \
  do {                                                    \
    EXPECT_STOP(validate_master_recipe(recipe), cat, code); \
    EXPECT_STOP(make_master_node(recipe), cat, code);       \
  } while (0)

// ---------------------------------------------------------------- T-M3B-C02-09: domini per metodo, n e ruolo

struct domain_case {
  std::string name;
  master_role role;
  std::size_t n;
  std::function<void(master_recipe&)> setup;
  bool admitted;
};

std::vector<domain_case> domain_cases(master_role role) {
  using R = master_recipe;
  const std::vector<double> eq3(3, 1.0);
  std::vector<domain_case> c = {
      // C-02.b §b.1, colonna `n`
      {"single n=1", role, 1, [](R& r) { use_single(r); }, true},
      {"single n=2", role, 2, [](R& r) { use_single(r); }, false},
      {"single n=3", role, 3, [](R& r) { use_single(r); }, false},
      {"mean n=1", role, 1, [](R&) {}, false},
      {"mean n=2", role, 2, [](R&) {}, true},
      {"mean n=1025", role, 1025, [](R&) {}, true},
      {"weighted n=2", role, 2, [](R& r) { use_weighted(r, {1, 1}); }, false},
      {"weighted n=3", role, 3, [eq3](R& r) { use_weighted(r, eq3); }, true},
      {"weighted n=1024", role, 1024, [](R& r) { use_weighted(r, std::vector<double>(1024, 1.0)); }, true},
      {"median n=1", role, 1, [](R& r) { use_median(r); }, false},
      {"median n=2", role, 2, [](R& r) { use_median(r); }, false},
      {"median n=3", role, 3, [](R& r) { use_median(r); }, true},
      {"median n=4", role, 4, [](R& r) { use_median(r); }, true},
      {"median n=1024", role, 1024, [](R& r) { use_median(r); }, true},
      {"median n=1025", role, 1025, [](R& r) { use_median(r); }, false},
      {"winsorized g=1 n=2", role, 2, [](R& r) { use_winsorized(r, 1); }, false},
      {"winsorized g=1 n=3", role, 3, [](R& r) { use_winsorized(r, 1); }, true},
      {"winsorized g=2 n=4", role, 4, [](R& r) { use_winsorized(r, 2); }, false},
      {"winsorized g=2 n=5", role, 5, [](R& r) { use_winsorized(r, 2); }, true},
      {"winsorized g=511 n=1023", role, 1023, [](R& r) { use_winsorized(r, 511); }, true},
      {"winsorized g=1 n=1024", role, 1024, [](R& r) { use_winsorized(r, 1); }, true},
      {"winsorized g=1 n=1025", role, 1025, [](R& r) { use_winsorized(r, 1); }, false},
      {"sigma_clip n=2", role, 2, [](R& r) { use_sigma(r); }, false},
      {"sigma_clip n=11 kappa 3", role, 11, [](R& r) { use_sigma(r, 3, 3, 1, 2); }, true},
      {"sigma_clip n=11 asymmetric", role, 11, [](R& r) { use_sigma(r, 3, 4, 5, 11); }, true},
      // coppie fuori tabella (§b.1): nessun parametro del rejection, la coppia viene prima
      {"single + sigma_clip", role, 1, [](R& r) { use_single(r); r.rejection = master_rejection::sigma_clip; }, false},
      {"median + sigma_clip", role, 5, [](R& r) { use_median(r); r.rejection = master_rejection::sigma_clip; }, false},
      {"winsorized + sigma_clip", role, 5,
       [](R& r) { use_winsorized(r, 1); r.rejection = master_rejection::sigma_clip; }, false},
      {"weighted + sigma_clip", role, 3,
       [eq3](R& r) { use_weighted(r, eq3); r.rejection = master_rejection::sigma_clip; }, false},
      {"other + none", role, 3, [](R& r) { r.combine = combine_method::other; }, false},
  };
  return c;
}

TEST(MasterB3, T_M3B_C02_09_domains_of_method_n_and_role_bias_dark_dark_flat) {
  for (master_role role : {master_role::bias, master_role::dark, master_role::dark_flat}) {
    for (const domain_case& c : domain_cases(role)) {
      SCOPED_TRACE(c.name + " / " + imagetyp_of(role));
      master_recipe r = bare(c.role, c.n);
      c.setup(r);
      if (c.admitted) {
        EXPECT_NO_THROW(validate_master_recipe(r));
        EXPECT_NO_THROW((void)make_master_node(r));
      } else {
        EXPECT_RECIPE_STOP(r, error_category::unsupported, "master.variance_domain");
      }
    }
  }
}

TEST(MasterB3, T_M3B_C02_09_domain_stop_carries_method_n_and_role_in_the_context) {
  master_recipe r = bare(master_role::dark, 2);
  use_median(r);
  try {
    validate_master_recipe(r);
    ADD_FAILURE() << "stop attesto";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.code(), "master.variance_domain");
    std::string flat;  // §b.2: metodo, n e ruolo nel contesto
    for (const auto& kv : e.context()) flat += kv.first + "=" + kv.second + ";";
    EXPECT_NE(flat.find("median"), std::string::npos) << flat;
    EXPECT_NE(flat.find("2"), std::string::npos) << flat;
    EXPECT_NE(flat.find("dark"), std::string::npos) << flat;
  }
}

// C-02.b §b.2, tabella dei domini; C-02.b-flat §f.2: sul flat con `none` solo `single` e `mean`.
TEST(MasterB3, T_M3B_C02_09_flat_role_admits_only_single_and_mean_with_none) {
  using R = master_recipe;
  struct fc {
    const char* name;
    std::size_t n;
    std::function<void(R&)> setup;
    bool admitted;
  };
  const std::vector<fc> cases = {
      {"single", 1, [](R& r) { use_single(r); }, true},
      {"mean", 3, [](R&) {}, true},
      {"weighted", 3, [](R& r) { use_weighted(r, {1, 1, 1}); }, false},
      {"median", 3, [](R& r) { use_median(r); }, false},
      {"winsorized", 3, [](R& r) { use_winsorized(r, 1); }, false},
      {"sigma_clip", 11, [](R& r) { use_sigma(r, 3, 3, 1, 2); }, false},
      // §f.2: `sigma_clip` sul flat con soglie diverse; con `none` il metodo è già fuori dominio
      {"sigma_clip asymmetric", 11, [](R& r) { use_sigma(r, 2, 4, 1, 2); }, false},
      {"single n=2", 2, [](R& r) { use_single(r); }, false},
      // §f.2: `median_ratio` con `single` è fuori dominio
      {"single + median_ratio", 1,
       [](R& r) {
         use_single(r);
         r.flat_norm = flat_normalization::median_ratio;
         r.flat_level_rel_tol.reset();
         set_flat_additive_bound(r, 0.0);  // parametri di §f.2 validi: l'unica violazione è la casella della tabella
       },
       false},
  };
  for (const fc& c : cases) {
    SCOPED_TRACE(c.name);
    master_recipe r = bare(master_role::flat, c.n);
    c.setup(r);
    if (c.admitted) {
      EXPECT_NO_THROW(validate_master_recipe(r));
      EXPECT_NO_THROW((void)make_master_node(r));
    } else {
      EXPECT_RECIPE_STOP(r, error_category::unsupported, "master.variance_domain");
    }
  }
}

// ---------------------------------------------------------------- T-M3B-C02-03(c): domini della media pesata

TEST(MasterB3, T_M3B_C02_03_c_weighted_mean_domain_n2_and_max_weight_boundary) {
  const auto stops_domain = [](std::vector<double> w) {
    master_recipe r = bare(master_role::dark, w.size());
    use_weighted(r, w);
    EXPECT_RECIPE_STOP(r, error_category::unsupported, "master.variance_domain");
  };
  const auto admitted = [](std::vector<double> w) {
    master_recipe r = bare(master_role::dark, w.size());
    use_weighted(r, w);
    EXPECT_NO_THROW(validate_master_recipe(r));
    EXPECT_NO_THROW((void)make_master_node(r));
  };
  stops_domain({1, 1});         // n = 2: impossibile (§b.1)
  stops_domain({1, 3});         // n = 2
  stops_domain({1, 1, 2});      // 2 max w = W: a_f = 1/2, escluso
  stops_domain({1, 1, 3});      // 2 max w > W
  stops_domain({4, 1, 1, 1, 1});  // 2 max w = W = 8 con n = 5
  admitted({1, 1, 1});
  admitted({1, 2, 2});
  admitted({2, 1, 1, 1, 1});  // W = 6, 2 max w = 4 < W
  admitted({2, 1, 1, 1});       // 2 max w = 4 < W = 5
  // Confronto in aritmetica esatta sui f64 (§b.6): W = 1 + 2^-60 si arrotonda a 1 in f64, ma 2 max w = 1 < W esatto.
  admitted({0.5, 0.25, 0.25, std::ldexp(1.0, -60)});
}

TEST(MasterB3, T_M3B_C02_03_c_weights_absent_null_negative_nonfinite_or_with_other_methods) {
  const auto weights_stop = [](master_recipe& r) {
    EXPECT_RECIPE_STOP(r, error_category::invalid_input, "master.param.weights");
  };
  for (double bad : {0.0, -1.0, -0.0, kNaN, kInf, -kInf}) {
    SCOPED_TRACE(bad);
    for (std::size_t k = 0; k < 3; ++k) {
      master_recipe r = bare(master_role::dark, 3);
      use_weighted(r, {1, 1, 1});
      r.frames[k].weight = bad;
      weights_stop(r);
    }
  }
  for (std::size_t k = 0; k < 3; ++k) {  // un solo frame senza peso
    master_recipe r = bare(master_role::dark, 3);
    use_weighted(r, {1, 1, 1});
    r.frames[k].weight.reset();
    weights_stop(r);
  }
  {  // nessun peso
    master_recipe r = bare(master_role::dark, 3);
    use_weighted(r, {1, 1, 1});
    for (master_frame_spec& f : r.frames) f.weight.reset();
    weights_stop(r);
  }
  // pesi con un metodo diverso da `weighted_mean` (§b.10), anche uno solo
  {
    master_recipe r = bare(master_role::dark, 3);
    r.frames[1].weight = 1;
    weights_stop(r);  // mean
  }
  {
    master_recipe r = bare(master_role::dark, 3);
    use_median(r);
    r.frames[0].weight = 2;
    weights_stop(r);
  }
  {
    master_recipe r = bare(master_role::dark, 3);
    use_winsorized(r, 1);
    for (master_frame_spec& f : r.frames) f.weight = 1;
    weights_stop(r);
  }
  {
    master_recipe r = bare(master_role::dark, 11);
    use_sigma(r);
    r.frames[4].weight = 1;
    weights_stop(r);
  }
  {
    master_recipe r = bare(master_role::dark, 1);
    use_single(r);
    r.frames[0].weight = 1;
    weights_stop(r);
  }
}

// ---------------------------------------------------------------- T-M3B-C02-12: fase e linearità

// C-02.b §b.13 punto 1: tabella delle operazioni di ADR-03 R1, `master_create`; la fase prima della linearità.
struct state_case {
  const char* name;
  std::function<void(fx::calib_header&)> mutate;
  error_category category;
  const char* code;
};

std::vector<state_case> state_cases() {
  return {
      {"calibrated", [](fx::calib_header& h) { h.phase = "calibrated"; }, error_category::incompatible, "op.phase_blocked"},
      {"prestacked", [](fx::calib_header& h) { h.phase = "prestacked"; }, error_category::incompatible, "op.phase_blocked"},
      {"nonlinear", [](fx::calib_header& h) { h.linear_value = false; }, error_category::incompatible,
       "op.linearity_blocked"},
      {"calibrated and nonlinear",
       [](fx::calib_header& h) {
         h.phase = "calibrated";
         h.linear_value = false;
       },
       error_category::incompatible, "op.phase_blocked"},
      {"prestacked and nonlinear",
       [](fx::calib_header& h) {
         h.phase = "prestacked";
         h.linear_value = false;
       },
       error_category::incompatible, "op.phase_blocked"},
      {"phase missing", [](fx::calib_header& h) { h.phase.clear(); }, error_category::metadata_missing,
       "master.metadata.phase"},
      {"linearity missing", [](fx::calib_header& h) { h.with_linear = false; }, error_category::metadata_missing,
       "master.metadata.linearity"},
  };
}

group group_for_role(master_role role) {
  group g = make_group(role, 8, 4, 3);
  for (std::size_t k = 0; k < g.frames.size(); ++k) {
    for (std::size_t i = 0; i < g.frames[k].v.size(); ++i)
      g.frames[k].v[i] = 500 + static_cast<double>((i + k) % 7);
  }
  return g;
}

const char* role_text(master_role r) {
  switch (r) {
    case master_role::bias: return "bias";
    case master_role::dark: return "dark";
    case master_role::dark_flat: return "dark_flat";
    case master_role::flat: return "flat";
  }
  return "";
}

TEST(MasterB3, T_M3B_C02_12_phase_and_linearity_stops_use_the_operation_table_for_every_role) {
  for (master_role role : {master_role::bias, master_role::dark, master_role::dark_flat, master_role::flat}) {
    for (const state_case& c : state_cases()) {
      SCOPED_TRACE(std::string(c.name) + " / " + role_text(role));
      group g = group_for_role(role);
      c.mutate(g.frames[1].hdr);
      master_recipe r = recipe_of(g);
      r.exposure_rel_tol = 1e-9;
      try {
        (void)combine_master(r, images_of(g));
        ADD_FAILURE() << "stop attesto";
      } catch (const stop_error& e) {
        EXPECT_EQ(e.category(), c.category) << e.what();
        EXPECT_EQ(e.code(), std::string(c.code)) << e.what();
        if (c.category == error_category::incompatible) {
          // ADR-03.b §b.14: il ruolo dell'ingresso nel contesto (chiave `role`)
          std::string seen;
          for (const auto& kv : e.context())
            if (kv.first == "role") seen = kv.second;
          EXPECT_EQ(seen, role_text(role)) << e.what();
        }
      }
    }
  }
}

TEST(MasterB3, T_M3B_C02_12_unit_other_than_adu_is_unsupported_input_state_and_phase_is_checked_first) {
  group g = group_for_role(master_role::dark);
  g.frames[1].hdr.with_unit_and_linear = false;  // BUNIT e LINEAR a mano: omit_keys toglierebbe anche l'extra
  g.frames[1].hdr.extra.push_back(fx::card_str("BUNIT", "electron"));
  g.frames[1].hdr.extra.push_back(fx::card_bool("LINEAR", true));
  master_recipe r = recipe_of(g);
  EXPECT_STOP(combine_master(r, images_of(g)), error_category::unsupported, "master.input_state");
  // fase non ammessa e unità diversa: la fase viene prima
  g.frames[1].hdr.phase = "calibrated";
  r = recipe_of(g);
  EXPECT_STOP(combine_master(r, images_of(g)), error_category::incompatible, "op.phase_blocked");
}

TEST(MasterB3, T_M3B_C02_12_job_header_peek_gives_the_same_codes_as_the_full_preflight) {
  for (const state_case& c : state_cases()) {
    SCOPED_TRACE(c.name);
    group g = group_for_role(master_role::dark);
    c.mutate(g.frames[2].hdr);
    const fs::path dir = fresh_dir(std::string("b3-t12-job"));
    master_recipe r = recipe_of(g);
    write_frames(g, r, dir, "frame");
    auto store = sqlite_dag_store::create(dir / "store");
    // Il nodo non legge gli header: lo si costruisce da una ricetta con gli stessi digest.
    const node_spec node = make_master_node(r);
    store->put_node(node);
    store->create_slot("master", node.key());
    memory_pool pool(256 * (std::uint64_t{1} << 20));
    const job_outcome out = run_master_quiet(*store, "master", r, pool);
    EXPECT_EQ(out.exit_code, exit_code_stop);
    ASSERT_TRUE(out.stop);
    EXPECT_EQ(out.stop->category(), c.category);
    EXPECT_EQ(out.stop->code(), std::string(c.code));
    EXPECT_EQ(by_role(*store, node.key()).count("primary"), 0u);
  }
}

// ---------------------------------------------------------------- T-M3B-DSC-01: nodo, chiave e descrittore

struct descriptor_case {
  const char* name;
  master_role role;
  std::size_t n;
  std::function<void(master_recipe&)> setup;
  combine_method method;
  master_rejection rejection;
  variance_estimator estimator;
};

// C-02.b §b.6 (tabella `variance_estimator`) e §b.7 (valori del descrittore).
std::vector<descriptor_case> descriptor_cases(master_role role) {
  using R = master_recipe;
  std::vector<descriptor_case> c = {
      {"single", role, 1, [](R& r) { use_single(r); }, combine_method::single, master_rejection::none,
       variance_estimator::noise_model},
      {"mean", role, 4, [](R&) {}, combine_method::mean, master_rejection::none, variance_estimator::empirical},
  };
  if (role != master_role::flat) {
    c.push_back({"weighted_mean", role, 4, [](R& r) { use_weighted(r, {1, 2, 2, 1}); }, combine_method::weighted_mean,
                 master_rejection::none, variance_estimator::empirical_weighted});
    c.push_back({"median", role, 5, [](R& r) { use_median(r); }, combine_method::median, master_rejection::none,
                 variance_estimator::gaussian_reference});
    c.push_back({"winsorized", role, 5, [](R& r) { use_winsorized(r, 1); }, combine_method::winsorized,
                 master_rejection::none, variance_estimator::gaussian_reference});
    c.push_back({"sigma_clip", role, 11, [](R& r) { use_sigma(r, 3, 3, 2, 5); }, combine_method::mean,
                 master_rejection::sigma_clip, variance_estimator::frozen_selection});
  }
  return c;
}

const char* method_text(combine_method m) {
  switch (m) {
    case combine_method::single: return "single";
    case combine_method::mean: return "mean";
    case combine_method::weighted_mean: return "weighted_mean";
    case combine_method::median: return "median";
    case combine_method::winsorized: return "winsorized";
    case combine_method::other: return "other";
  }
  return "";
}

TEST(MasterB3, T_M3B_DSC_01_descriptor_values_for_every_admitted_method_and_role) {
  for (master_role role : {master_role::bias, master_role::dark, master_role::dark_flat, master_role::flat}) {
    for (const descriptor_case& c : descriptor_cases(role)) {
      SCOPED_TRACE(std::string(c.name) + " / " + role_text(role));
      master_recipe r = bare(c.role, c.n);
      c.setup(r);
      const node_spec node = make_master_node(r);
      EXPECT_EQ(node.operator_id(), master_operator_id);
      const master_descriptor d = master_descriptor_from_node(node);
      EXPECT_EQ(d.role, role);
      EXPECT_EQ(d.combine_count, static_cast<std::uint32_t>(c.n));
      EXPECT_EQ(d.combine, c.method);
      EXPECT_EQ(d.rejection, c.rejection);
      EXPECT_EQ(d.estimator, c.estimator);
      // Stati come in R2 (§b.7): dark e dark-flat `bias_included`, `unscaled`; flat `raw`; bias senza stati
      if (role == master_role::dark || role == master_role::dark_flat) {
        EXPECT_EQ(d.additive, additive_state::bias_included);
        EXPECT_EQ(d.scaling, scaling_state::unscaled);
        EXPECT_FALSE(d.flat.has_value());
      } else if (role == master_role::flat) {
        EXPECT_EQ(d.flat, flat_state::raw);
        EXPECT_FALSE(d.additive.has_value());
      } else {
        EXPECT_FALSE(d.additive.has_value());
        EXPECT_FALSE(d.flat.has_value());
      }
      // C-02.b-flat §f.7 e §f.13 punto 7: `frame_normalization` per il flat (= `flat_normalization`), assente altrove
      if (role == master_role::flat) {
        EXPECT_EQ(d.frame_normalization, frame_normalization_mode::none);
      } else {
        EXPECT_FALSE(d.frame_normalization.has_value());
      }
      EXPECT_FALSE(d.frame_decoder_states.has_value());
      EXPECT_FALSE(d.flat_normalization_declared.has_value());
      // I parametri canonici nominano il metodo scelto (§b.7: `combine_method`, `rejection`)
      const json* combine = node.params().find("combine");
      ASSERT_NE(combine, nullptr);
      EXPECT_EQ(combine->as_string(), method_text(c.method));
      const json* rejection = node.params().find("rejection");
      ASSERT_NE(rejection, nullptr);
      EXPECT_EQ(rejection->as_string(), c.rejection == master_rejection::none ? "none" : "sigma_clip");
    }
  }
}

TEST(MasterB3, T_M3B_DSC_01_algorithm_version_is_raised_above_the_dot_a_version) {
  EXPECT_GT(master_algorithm_version, 1u);  // B3: i master `.a` esistenti diventano `stale`
  const node_spec node = make_master_node(bare(master_role::dark, 3));
  EXPECT_EQ(node.algorithm_version(), master_algorithm_version);
}

json params_replacing(const node_spec& node, const std::string& key, json value) {
  json out = json::object();
  const std::vector<std::string>& keys = node.params().keys();
  const std::vector<json>& items = node.params().items();
  for (std::size_t i = 0; i < keys.size(); ++i) out.set(keys[i], keys[i] == key ? value : items[i]);
  return out;
}

node_spec with_params(const node_spec& node, json params) {
  return node_spec(node.operator_id(), node.algorithm_version(), node.serialization_version(), std::move(params),
                   node.inputs(), node.precision(), node.backend(), node.seed());
}

TEST(MasterB3, T_M3B_DSC_01_descriptor_from_node_stops_on_values_outside_the_tables) {
  const node_spec dark = make_master_node(bare(master_role::dark, 5));
  const node_spec flat = make_master_node(bare(master_role::flat, 3));
  const auto not_master = [](const node_spec& n) {
    EXPECT_STOP(master_descriptor_from_node(n), error_category::invalid_input, "master.not_master_node");
  };
  not_master(with_params(dark, params_replacing(dark, "combine", json::string("bogus"))));
  not_master(with_params(dark, params_replacing(dark, "combine", json::string("other"))));  // `other` è dei master esterni
  not_master(with_params(dark, params_replacing(dark, "rejection", json::string("bogus"))));
  // coppie fuori tabella (§b.1)
  {
    json p = params_replacing(dark, "combine", json::string("median"));
    not_master(with_params(dark, params_replacing(with_params(dark, p), "rejection", json::string("sigma_clip"))));
  }
  {
    json p = params_replacing(dark, "combine", json::string("single"));
    not_master(with_params(dark, params_replacing(with_params(dark, p), "rejection", json::string("sigma_clip"))));
  }
  // L11: valore sconosciuto di flat_normalization letto da un nodo
  not_master(with_params(flat, params_replacing(flat, "flat_normalization", json::string("bogus"))));
}

// Sostituzione di un parametro sul nodo: la chiave cambia (L5, L8, §b.10).
TEST(MasterB3, T_M3B_DSC_01_every_method_parameter_enters_the_key) {
  using R = master_recipe;
  struct kc {
    const char* name;
    std::size_t n;
    std::function<void(R&)> base;
    std::function<void(R&)> change;
  };
  const std::vector<kc> cases = {
      {"winsor_g", 7, [](R& r) { use_winsorized(r, 1); }, [](R& r) { r.winsor_g = 2; }},
      {"clip_kappa_low", 11, [](R& r) { use_sigma(r); }, [](R& r) { r.clip_kappa_low = 3.5; }},
      {"clip_kappa_high", 11, [](R& r) { use_sigma(r); }, [](R& r) { r.clip_kappa_high = 3.5; }},
      {"clip_max_iter", 11, [](R& r) { use_sigma(r); }, [](R& r) { r.clip_max_iter = 2; }},
      {"clip_min_kept", 11, [](R& r) { use_sigma(r); }, [](R& r) { r.clip_min_kept = 3; }},
      {"offset_adu", 1, [](R& r) { use_single(r); }, [](R& r) { r.offset_adu = 11; }},
      {"conversion_gain_e_per_adu", 1, [](R& r) { use_single(r); }, [](R& r) { r.conversion_gain_e_per_adu = 1.25; }},
      {"read_noise_adu", 1, [](R& r) { use_single(r); }, [](R& r) { r.read_noise_adu = 2.5; }},
      {"weight", 4, [](R& r) { use_weighted(r, {1, 2, 2, 1}); }, [](R& r) { r.frames[2].weight = 3; }},
  };
  for (const kc& c : cases) {
    SCOPED_TRACE(c.name);
    master_recipe a = bare(master_role::dark, c.n);
    c.base(a);
    master_recipe b = a;
    c.change(b);
    EXPECT_NE(make_master_node(a).key(), make_master_node(b).key());
    // il parametro compare nel nodo con lo stesso nome del campo di ricetta (voce del frame per `weight`)
    if (std::string(c.name) != "weight") {
      EXPECT_NE(make_master_node(a).params().find(c.name), nullptr);
    }
  }
}

// L5: il peso è un campo `weight` della voce del frame, legato al digest; permutare i frame non cambia la chiave.
TEST(MasterB3, T_M3B_DSC_01_weights_are_bound_to_the_frame_digest_not_to_the_position) {
  master_recipe a = bare(master_role::dark, 4);
  use_weighted(a, {1, 2, 2, 1});
  master_recipe permuted = a;
  std::reverse(permuted.frames.begin(), permuted.frames.end());
  const node_spec na = make_master_node(a), np = make_master_node(permuted);
  EXPECT_EQ(na.key(), np.key());
  // scambiare i pesi di due frame (digest invariati) cambia la chiave
  master_recipe swapped = a;
  std::swap(swapped.frames[0].weight, swapped.frames[1].weight);
  EXPECT_NE(make_master_node(swapped).key(), na.key());
  // le voci `frames[k]` nell'ordine canonico portano `weight`, e `null` per un metodo senza pesi
  const json* frames = na.params().find("frames");
  ASSERT_NE(frames, nullptr);
  ASSERT_EQ(frames->items().size(), 4u);
  std::map<std::string, double> by_digest;
  for (const master_frame_spec& f : a.frames) by_digest[f.digest.sha256] = *f.weight;
  for (std::size_t k = 0; k < 4; ++k) {
    const json* w = frames->items()[k].find("weight");
    ASSERT_NE(w, nullptr) << k;
    ASSERT_EQ(w->type() == json::kind::number || w->type() == json::kind::integer, true) << k;
    EXPECT_EQ(w->as_number(), by_digest.at(na.inputs()[k].file_digest().sha256)) << k;
  }
  const node_spec mean = make_master_node(bare(master_role::dark, 3));
  for (const json& f : mean.params().find("frames")->items()) {
    const json* w = f.find("weight");
    ASSERT_NE(w, nullptr);
    EXPECT_EQ(w->type(), json::kind::null);
  }
}

// L8: parametro `analysis` del nodo; `null` esplicito per un nodo non proposto; entra nella chiave.
TEST(MasterB3, T_M3B_DSC_01_analysis_parameter_enters_the_key_and_is_null_for_a_node_not_proposed) {
  master_recipe base = bare(master_role::dark, 3);
  const node_spec plain = make_master_node(base);
  const json* analysis = plain.params().find("analysis");
  ASSERT_NE(analysis, nullptr);
  EXPECT_EQ(analysis->type(), json::kind::null);

  master_recipe with_obj = base;
  with_obj.analysis = json::object();
  with_obj.analysis.set("group", json::string("g1"));
  with_obj.analysis.set("followed_proposal", json::boolean(true));
  const node_spec proposed = make_master_node(with_obj);
  EXPECT_NE(proposed.key(), plain.key());
  const json* got = proposed.params().find("analysis");
  ASSERT_NE(got, nullptr);
  EXPECT_EQ(got->type(), json::kind::object);

  master_recipe changed = with_obj;
  changed.analysis = json::object();
  changed.analysis.set("group", json::string("g2"));
  changed.analysis.set("followed_proposal", json::boolean(true));
  EXPECT_NE(make_master_node(changed).key(), proposed.key());

  // JSON canonico: l'ordine di inserimento delle chiavi non conta
  master_recipe reordered = with_obj;
  reordered.analysis = json::object();
  reordered.analysis.set("followed_proposal", json::boolean(true));
  reordered.analysis.set("group", json::string("g1"));
  EXPECT_EQ(make_master_node(reordered).key(), proposed.key());

  // validato solo come oggetto o null (L8)
  for (json bad : {json::string("x"), json::number(1), json::boolean(true), json::array()}) {
    master_recipe r = base;
    r.analysis = bad;
    EXPECT_RECIPE_STOP(r, error_category::invalid_input, "master.param.analysis");
  }
}

TEST(MasterB3, T_M3B_DSC_01_flat_frame_normalization_is_in_the_node_and_changes_nothing_for_other_roles) {
  const node_spec flat = make_master_node(bare(master_role::flat, 3));
  const master_descriptor d = master_descriptor_from_node(flat);
  EXPECT_EQ(d.frame_normalization, frame_normalization_mode::none);
  const json* fn = flat.params().find("flat_normalization");
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->as_string(), "none");
  // bias, dark e dark-flat: nel descrittore `null` (campo assente)
  for (master_role role : {master_role::bias, master_role::dark, master_role::dark_flat}) {
    EXPECT_FALSE(master_descriptor_from_node(make_master_node(bare(role, 3))).frame_normalization.has_value());
  }
}

// ---------------------------------------------------------------- tabella dei parametri (FLAT-06 parte parametri, C02-15)

struct param_case {
  std::string name;
  bool flat;
  std::function<void(master_recipe&)> mutate;
  std::string code;  // sempre invalid_input
};

std::vector<param_case> param_cases() {
  using R = master_recipe;
  std::vector<param_case> c;
  const auto add = [&](std::string name, bool flat, std::function<void(R&)> m, std::string code) {
    c.push_back({std::move(name), flat, std::move(m), std::move(code)});
  };
  // winsor_g: assente, non intero, < 1, non finito (§b.10)
  add("winsor_g missing", false, [](R& r) { use_winsorized(r, 1); r.winsor_g.reset(); }, "master.param.winsor_g");
  for (double bad : {0.0, -1.0, 0.5, 1.5, kNaN, kInf}) {
    add("winsor_g " + std::to_string(bad), false, [bad](R& r) { use_winsorized(r, 1); r.winsor_g = bad; },
        "master.param.winsor_g");
  }
  // parametri di sigma_clip: assenti o fuori dominio
  for (const char* name : {"clip_kappa_low", "clip_kappa_high", "clip_max_iter", "clip_min_kept"}) {
    const std::string n = name;
    const auto field = [n](R& r) -> std::optional<double>& {
      if (n == "clip_kappa_low") return r.clip_kappa_low;
      if (n == "clip_kappa_high") return r.clip_kappa_high;
      if (n == "clip_max_iter") return r.clip_max_iter;
      return r.clip_min_kept;
    };
    add(n + " missing", false, [field](R& r) { use_sigma(r); field(r).reset(); }, "master.param." + n);
    std::vector<double> bad = {kNaN, kInf, -1.0, 0.0};
    if (n == "clip_max_iter") bad = {kNaN, kInf, -1.0, 0.0, 1.5};
    if (n == "clip_min_kept") bad = {kNaN, kInf, -1.0, 0.0, 1.0, 2.5, 4.0};  // n = 3: l'intervallo è [2, 3]
    for (double b : bad) {
      add(n + " " + std::to_string(b), false, [field, b](R& r) { use_sigma(r); field(r) = b; }, "master.param." + n);
    }
  }
  // parametri di un metodo non scelto presenti (§b.10): uno alla volta
  for (const char* chosen : {"mean", "median", "winsorized", "weighted_mean", "single"}) {
    const std::string m = chosen;
    const auto choose = [m](R& r) {
      if (m == "median") use_median(r);
      if (m == "winsorized") use_winsorized(r, 1);
      if (m == "weighted_mean") use_weighted(r, {1, 1, 1});
      if (m == "single") {
        use_single(r);
        r.frames.resize(1);
      }
    };
    if (m != "winsorized") {
      add("winsor_g with " + m, false, [choose](R& r) { choose(r); r.winsor_g = 1; }, "master.param.winsor_g");
    }
    add("clip_kappa_low with " + m, false, [choose](R& r) { choose(r); r.clip_kappa_low = 3; }, "master.param.clip_kappa_low");
    add("clip_kappa_high with " + m, false, [choose](R& r) { choose(r); r.clip_kappa_high = 3; }, "master.param.clip_kappa_high");
    add("clip_max_iter with " + m, false, [choose](R& r) { choose(r); r.clip_max_iter = 1; }, "master.param.clip_max_iter");
    add("clip_min_kept with " + m, false, [choose](R& r) { choose(r); r.clip_min_kept = 2; }, "master.param.clip_min_kept");
  }
  add("winsor_g with sigma_clip", false, [](R& r) { use_sigma(r, 1, 1, 1, 2); r.winsor_g = 1; }, "master.param.winsor_g");
  // pesi (§b.10)
  add("weights missing on one frame", false, [](R& r) { use_weighted(r, {1, 1, 1}); r.frames[1].weight.reset(); },
      "master.param.weights");
  add("weights with mean", false, [](R& r) { r.frames[0].weight = 1; }, "master.param.weights");
  add("weight zero", false, [](R& r) { use_weighted(r, {1, 1, 1}); r.frames[2].weight = 0; }, "master.param.weights");
  add("weight NaN", false, [](R& r) { use_weighted(r, {1, 1, 1}); r.frames[0].weight = kNaN; }, "master.param.weights");
  // analysis (L8)
  add("analysis string", false, [](R& r) { r.analysis = json::string("x"); }, "master.param.analysis");
  // flat: C-02.b-flat §f.2, L11
  add("flat_normalization out of domain", true,
      [](R& r) { r.flat_norm = static_cast<flat_normalization>(7); }, "master.param.flat_normalization");
  add("flat_normalization missing", true, [](R& r) { r.flat_norm.reset(); }, "master.param.flat_normalization");
  add("flat_normalization on dark", false, [](R& r) { r.flat_norm = flat_normalization::none; },
      "master.param.flat_normalization");
  for (double bad : {0.0, 1.0, -0.5, kNaN, kInf}) {
    add("flat_level_rel_tol " + std::to_string(bad), true, [bad](R& r) { r.flat_level_rel_tol = bad; },
        "master.param.flat_level_rel_tol");
  }
  add("flat_level_rel_tol missing with none", true, [](R& r) { r.flat_level_rel_tol.reset(); },
      "master.param.flat_level_rel_tol");
  add("flat_level_rel_tol on dark", false, [](R& r) { r.flat_level_rel_tol = 0.1; }, "master.param.flat_level_rel_tol");
  for (double bad : {0.0, -1.0, kNaN, kInf}) {
    add("flat_min_signal_adu " + std::to_string(bad), true, [bad](R& r) { r.flat_min_signal_adu = bad; },
        "master.param.flat_min_signal_adu");
  }
  add("flat_min_signal_adu missing", true, [](R& r) { r.flat_min_signal_adu.reset(); }, "master.param.flat_min_signal_adu");
  add("flat_min_signal_adu on dark", false, [](R& r) { r.flat_min_signal_adu = 1; }, "master.param.flat_min_signal_adu");
  // parametri del metodo su un flat con `mean`
  add("winsor_g with mean on flat", true, [](R& r) { r.winsor_g = 1; }, "master.param.winsor_g");
  add("clip_kappa_high with mean on flat", true, [](R& r) { r.clip_kappa_high = 3; }, "master.param.clip_kappa_high");
  add("weights with mean on flat", true, [](R& r) { r.frames[0].weight = 1; }, "master.param.weights");
  return c;
}

master_recipe param_base(bool flat) { return bare(flat ? master_role::flat : master_role::dark, flat ? 4 : 3); }

TEST(MasterB3, T_M3B_FLAT_06_parameters_missing_forbidden_or_out_of_domain_are_invalid_input) {
  const std::vector<param_case> cases = param_cases();
  EXPECT_GT(cases.size(), 60u);
  for (const param_case& c : cases) {
    SCOPED_TRACE(c.name);
    master_recipe r = param_base(c.flat);
    c.mutate(r);
    EXPECT_RECIPE_STOP(r, error_category::invalid_input, c.code.c_str());
  }
}

// C-02.b-flat §f.2, §f.13 punto 2: con `none` `flat_level_rel_tol` è obbligatorio; il valore al bordo (0,1) è accettato.
TEST(MasterB3, T_M3B_FLAT_06_flat_none_requires_level_tolerance_inside_the_open_interval) {
  master_recipe r = bare(master_role::flat, 3);
  for (double ok : {1e-9, 0.25, 0.999999}) {
    r.flat_level_rel_tol = ok;
    EXPECT_NO_THROW(validate_master_recipe(r)) << ok;
  }
}

// ---------------------------------------------------------------- T-M3B-C02-15: nessun lavoro dopo uno stop di parametro

struct job_probe {
  std::optional<std::string> code;
  std::optional<error_category> category;
  int faults = 0;
  bool frames_read = false;
  bool primary = false;
};

// Progetto con il nodo della ricetta valida `node_recipe`; il job gira con `job_recipe`. Il contatore del fault_injector
// del repository conta ogni punto di protocollo dello store dopo l'allestimento.
job_probe probe_job(const fs::path& root, const master_recipe& node_recipe, const master_recipe& job_recipe) {
  job_probe out;
  auto store = sqlite_dag_store::create(root);
  const node_spec node = make_master_node(node_recipe);
  store->put_node(node);
  store->create_slot("master", node.key());
  int faults = 0;
  store->set_fault_injector([&faults](std::string_view) {
    ++faults;
    return 0;
  });
  master_job_options o;
  o.hook = [&out](std::string_view point) {
    if (point == "frames.read") out.frames_read = true;
  };
  memory_pool pool(256 * (std::uint64_t{1} << 20));
  try {
    const job_outcome res = run_master_quiet(*store, "master", job_recipe, pool, o);
    if (res.stop) {
      out.code = res.stop->code();
      out.category = res.stop->category();
    }
  } catch (const stop_error& e) {
    out.code = e.code();
    out.category = e.category();
  }
  out.faults = faults;
  store->set_fault_injector({});
  out.primary = by_role(*store, node.key()).count("primary") != 0;
  return out;
}

TEST(MasterB3, T_M3B_C02_15_parameter_stops_leave_no_node_no_read_and_no_more_store_work_than_the_baseline) {
  const fs::path dir = fresh_dir("b3-t15");
  group dark = group_for_role(master_role::dark);
  master_recipe dark_r = recipe_of(dark);
  write_frames(dark, dark_r, dir, "dark");
  group flat = make_group(master_role::flat, 8, 4, 4);
  for (std::size_t k = 0; k < 4; ++k)
    for (std::size_t i = 0; i < flat.frames[k].v.size(); ++i) flat.frames[k].v[i] = 2010 + static_cast<double>(i % 5) + k;
  master_recipe flat_r = recipe_of(flat, 0.01);
  write_frames(flat, flat_r, dir, "flat");

  // Riferimento: stop di parametro già presente (combine assente), prima di ogni lettura.
  master_recipe base_stop = dark_r;
  base_stop.combine.reset();
  const job_probe baseline = probe_job(dir / "store-baseline", dark_r, base_stop);
  ASSERT_EQ(baseline.code, std::optional<std::string>("master.param.combine"));
  EXPECT_FALSE(baseline.frames_read);

  // Controllo positivo: il contatore e il gancio sono collegati (un job valido legge i frame e lavora sullo store).
  const job_probe control = probe_job(dir / "store-control", dark_r, dark_r);
  EXPECT_FALSE(control.code.has_value());
  EXPECT_TRUE(control.frames_read);
  EXPECT_TRUE(control.primary);
  EXPECT_GT(control.faults, baseline.faults) << "contatore del fault_injector non collegato";

  std::size_t k = 0;
  for (const param_case& c : param_cases()) {
    SCOPED_TRACE(c.name);
    master_recipe node_r = c.flat ? flat_r : dark_r;
    master_recipe job_r = node_r;
    c.mutate(job_r);
    const job_probe p = probe_job(dir / ("store-" + std::to_string(k++)), node_r, job_r);
    EXPECT_EQ(p.code, std::optional<std::string>(c.code));
    EXPECT_EQ(p.category, std::optional<error_category>(error_category::invalid_input));
    EXPECT_FALSE(p.frames_read);
    EXPECT_FALSE(p.primary);
    EXPECT_EQ(p.faults, baseline.faults);
  }
}

// ---------------------------------------------------------------- parametri del modello di rumore di `single`

// ADR-17.a, C-02.a §1: assenti `metadata_missing` `var.source.<campo>`; non finiti o fuori dominio `invalid_input`
// `var.param.<campo>`. `offset_adu` è solo finito (anche negativo); `read_noise_adu >= 0`; `conversion_gain > 0`.
TEST(MasterB3, T_M3B_C02_13_single_noise_model_parameters_missing_or_out_of_domain) {
  using R = master_recipe;
  struct field {
    const char* name;
    std::function<std::optional<double>&(R&)> ref;
  };
  const std::vector<field> fields = {
      {"offset_adu", [](R& r) -> std::optional<double>& { return r.offset_adu; }},
      {"conversion_gain_e_per_adu", [](R& r) -> std::optional<double>& { return r.conversion_gain_e_per_adu; }},
      {"read_noise_adu", [](R& r) -> std::optional<double>& { return r.read_noise_adu; }},
  };
  for (const field& f : fields) {
    SCOPED_TRACE(f.name);
    master_recipe r = bare(master_role::dark, 1);
    use_single(r);
    f.ref(r).reset();
    EXPECT_RECIPE_STOP(r, error_category::metadata_missing, (std::string("var.source.") + f.name).c_str());
    std::vector<double> bad = {kNaN, kInf, -kInf};
    if (std::string(f.name) == "conversion_gain_e_per_adu") {
      bad.push_back(0.0);
      bad.push_back(-1.0);
    }
    if (std::string(f.name) == "read_noise_adu") bad.push_back(-0.5);
    for (double b : bad) {
      SCOPED_TRACE(b);
      master_recipe q = bare(master_role::dark, 1);
      use_single(q);
      f.ref(q) = b;
      EXPECT_RECIPE_STOP(q, error_category::invalid_input, (std::string("var.param.") + f.name).c_str());
    }
  }
  for (double ok_offset : {-5.0, -1e-3, 0.0, 10.0}) {  // offset negativo e finito ammesso
    master_recipe r = bare(master_role::dark, 1);
    use_single(r);
    r.offset_adu = ok_offset;
    EXPECT_NO_THROW(validate_master_recipe(r)) << ok_offset;
    EXPECT_NO_THROW((void)make_master_node(r)) << ok_offset;
  }
  master_recipe zero_noise = bare(master_role::dark, 1);
  use_single(zero_noise);
  zero_noise.read_noise_adu = 0;
  EXPECT_NO_THROW(validate_master_recipe(zero_noise));
}

TEST(MasterB3, T_M3B_C02_13_noise_model_parameters_are_forbidden_with_a_method_other_than_single) {
  using R = master_recipe;
  const std::vector<std::pair<const char*, std::function<void(R&)>>> methods = {
      {"mean", [](R&) {}},
      {"median", [](R& r) { use_median(r); }},
      {"winsorized", [](R& r) { use_winsorized(r, 1); }},
      {"weighted_mean", [](R& r) { use_weighted(r, {1, 1, 1}); }},
      {"sigma_clip", [](R& r) { use_sigma(r, 1, 1, 1, 2); }},
  };
  for (const auto& [name, choose] : methods) {
    for (const char* field : {"offset_adu", "conversion_gain_e_per_adu", "read_noise_adu"}) {
      SCOPED_TRACE(std::string(name) + " / " + field);
      master_recipe r = bare(master_role::dark, 3);
      choose(r);
      if (std::string(field) == "offset_adu") r.offset_adu = 10;
      if (std::string(field) == "conversion_gain_e_per_adu") r.conversion_gain_e_per_adu = 1.5;
      if (std::string(field) == "read_noise_adu") r.read_noise_adu = 2;
      EXPECT_RECIPE_STOP(r, error_category::invalid_input, (std::string("master.param.") + field).c_str());
    }
  }
}

// ---------------------------------------------------------------- master_descriptor_from_node: flat e normalizzazione

json params_with(const node_spec& node, std::vector<std::pair<std::string, json>> changes) {
  json out = json::object();
  const std::vector<std::string>& keys = node.params().keys();
  const std::vector<json>& items = node.params().items();
  for (std::size_t i = 0; i < keys.size(); ++i) {
    json v = items[i];
    for (const auto& [k, nv] : changes)
      if (k == keys[i]) v = nv;
    out.set(keys[i], std::move(v));
  }
  return out;
}

// C-02.b-flat §f.2: con `median_ratio` sono ammessi `mean`, `weighted_mean`, `median`, `winsorized`, `mean` + `sigma_clip`;
// `single` no. Con `none` solo `single` e `mean` (C-02.b §b.2). Il descrittore non è per bias, dark e dark-flat.
TEST(MasterB3, T_M3B_DSC_01_descriptor_from_node_accepts_median_ratio_only_on_flat_with_the_pairs_of_f2) {
  const node_spec flat = make_master_node(bare(master_role::flat, 3));
  struct pc {
    const char* flat_norm;
    const char* combine;
    const char* rejection;
    bool admitted;
    variance_estimator estimator;
  };
  const std::vector<pc> cases = {
      {"median_ratio", "mean", "none", true, variance_estimator::empirical},
      {"median_ratio", "weighted_mean", "none", true, variance_estimator::empirical_weighted},
      {"median_ratio", "median", "none", true, variance_estimator::gaussian_reference},
      {"median_ratio", "winsorized", "none", true, variance_estimator::gaussian_reference},
      {"median_ratio", "mean", "sigma_clip", true, variance_estimator::frozen_selection},
      {"median_ratio", "single", "none", false, variance_estimator::noise_model},
      {"none", "single", "none", true, variance_estimator::noise_model},
      {"none", "mean", "none", true, variance_estimator::empirical},
      {"none", "weighted_mean", "none", false, variance_estimator::empirical_weighted},
      {"none", "median", "none", false, variance_estimator::gaussian_reference},
      {"none", "winsorized", "none", false, variance_estimator::gaussian_reference},
      {"none", "mean", "sigma_clip", false, variance_estimator::frozen_selection},
  };
  for (const pc& c : cases) {
    SCOPED_TRACE(std::string(c.flat_norm) + " / " + c.combine + " / " + c.rejection);
    const node_spec n = with_params(flat, params_with(flat, {{"flat_normalization", json::string(c.flat_norm)},
                                                             {"combine", json::string(c.combine)},
                                                             {"rejection", json::string(c.rejection)}}));
    if (c.admitted) {
      const master_descriptor d = master_descriptor_from_node(n);
      EXPECT_EQ(d.frame_normalization, std::string(c.flat_norm) == "none" ? frame_normalization_mode::none
                                                                          : frame_normalization_mode::median_ratio);
      EXPECT_EQ(d.estimator, c.estimator);
      EXPECT_EQ(d.flat, flat_state::raw);
    } else {
      EXPECT_STOP(master_descriptor_from_node(n), error_category::invalid_input, "master.not_master_node");
    }
  }
  // bias, dark e dark-flat: `flat_normalization` deve essere null nel nodo
  for (master_role role : {master_role::bias, master_role::dark, master_role::dark_flat}) {
    const node_spec base = make_master_node(bare(role, 3));
    for (const char* v : {"none", "median_ratio"}) {
      SCOPED_TRACE(std::string(role_text(role)) + " / " + v);
      const node_spec n = with_params(base, params_with(base, {{"flat_normalization", json::string(v)}}));
      EXPECT_STOP(master_descriptor_from_node(n), error_category::invalid_input, "master.not_master_node");
    }
  }
}

// ---------------------------------------------------------------- descrittore dichiarato di un master esterno nello store

// C-02.b §b.13 punto 3: per un master esterno restano i valori dichiarati di ADR-16.a; i valori dei master del prodotto
// sono rifiutati in scrittura (`classify.declaration`) e quelli ammessi tornano identici alla rilettura.
TEST(MasterB3, T_M3B_DSC_01_external_declared_descriptor_roundtrips_adr16a_values_and_rejects_product_values) {
  const fs::path dir = fresh_dir("b3-store-descriptor");
  group g = group_for_role(master_role::dark);
  const fs::path file = dir / "d.fits";
  cal::write_bytes(file, file_of(g.frames[0], g.w, g.h));
  auto store = sqlite_dag_store::create(dir / "project");
  memory_pool pool(64 * (std::uint64_t{1} << 20));
  const cancel_token cancel;
  const auto imported = import_batch(*store, {file.string()}, pool, cancel);
  ASSERT_EQ(imported.size(), 1u);
  const std::uint64_t id = imported[0].input.id;

  const auto doc_of = [](const std::string& role, const std::map<std::string, json>& extra) {
    json d = json::object();
    d.set("role", json::string(role));
    for (const auto& [k, v] : extra) d.set(k, v);
    json fields = json::object();
    json doc = json::object();
    doc.set("format", json::string("astro.input_declarations.v1"));
    doc.set("declared_image_fields", std::move(fields));
    doc.set("master_descriptor", std::move(d));
    return canonical_json(doc);
  };
  const auto record = [&](const std::string& role, const std::string& declarations) {
    classification_record r;
    r.input_id = id;
    r.role = role;
    r.source = "user";
    r.evidence_canonical = "{}";
    r.declarations_canonical = declarations;
    return r;
  };

  // valori di ADR-16.a: scritti e riletti identici, campo per campo
  const std::vector<std::map<std::string, json>> accepted = {
      {{"combine_method", json::string("mean")}, {"rejection", json::string("none")},
       {"variance_estimator", json::string("empirical")}, {"combine_count", json::uinteger(20)}},
      {{"combine_method", json::string("median")}, {"rejection", json::string("sigma_clip")}},
      {{"combine_method", json::string("other")}},
  };
  for (const auto& extra : accepted) {
    const std::string decl = doc_of("dark", extra);
    ASSERT_NO_THROW(store->append_classifications({record("dark", decl)}));
    const auto hist = store->classifications(id);
    EXPECT_EQ(hist.back().declarations_canonical, decl);
    const json back = parse_json(hist.back().declarations_canonical);
    const json* d = back.find("master_descriptor");
    ASSERT_NE(d, nullptr);
    for (const auto& [k, v] : extra) {
      const json* got = d->find(k);
      ASSERT_NE(got, nullptr) << k;
      EXPECT_EQ(canonical_json(*got), canonical_json(v)) << k;
    }
  }

  // valori dei master del prodotto o `frame_normalization`: malformati in scrittura, storia intatta
  struct bad {
    const char* what;
    const char* role;
    std::map<std::string, json> extra;
  };
  const std::vector<bad> rejected = {
      {"weighted_mean", "dark", {{"combine_method", json::string("weighted_mean")}}},
      {"single", "dark", {{"combine_method", json::string("single")}}},
      {"winsorized", "dark", {{"combine_method", json::string("winsorized")}}},
      {"noise_model", "dark", {{"variance_estimator", json::string("noise_model")}}},
      {"empirical_weighted", "dark", {{"variance_estimator", json::string("empirical_weighted")}}},
      {"gaussian_reference", "dark", {{"variance_estimator", json::string("gaussian_reference")}}},
      {"frozen_selection", "dark", {{"variance_estimator", json::string("frozen_selection")}}},
      {"frame_normalization none su dark", "dark", {{"frame_normalization", json::string("none")}}},
      {"frame_normalization su flat", "flat", {{"frame_normalization", json::string("median_ratio")}}},
  };
  for (const bad& b : rejected) {
    SCOPED_TRACE(b.what);
    const std::size_t before = store->classifications(id).size();
    EXPECT_STOP(store->append_classifications({record(b.role, doc_of(b.role, b.extra))}), error_category::invalid_input,
                "classify.declaration");
    EXPECT_EQ(store->classifications(id).size(), before);
  }
}

// Progetto con il database manomesso: un descrittore dichiarato che lo store in scrittura non avrebbe accettato. Il lettore
// (`analyze_batch` sullo store) si ferma con `batch.report_malformed`; nessuna conversione a `other` o `empirical` (NC-21).
TEST(MasterB3, T_M3B_DSC_01_tampered_project_with_unknown_declared_descriptor_value_stops_the_reader) {
  struct tamper {
    const char* what;
    const char* key;
    const char* value;
  };
  const std::vector<tamper> cases = {
      {"combine_method sconosciuto", "combine_method", "bogus"},
      {"combine_method del prodotto", "combine_method", "weighted_mean"},
      {"variance_estimator sconosciuto", "variance_estimator", "bogus"},
      {"variance_estimator del prodotto", "variance_estimator", "noise_model"},
  };
  for (const tamper& t : cases) {
    SCOPED_TRACE(t.what);
    const fs::path dir = fresh_dir("b3-tampered");
    group g = group_for_role(master_role::dark);
    const fs::path file = dir / "d.fits";
    cal::write_bytes(file, file_of(g.frames[0], g.w, g.h));
    const fs::path project = dir / "project";
    std::uint64_t id = 0;
    memory_pool pool(64 * (std::uint64_t{1} << 20));
    const cancel_token cancel;
    {
      auto store = sqlite_dag_store::create(project);
      const auto imported = import_batch(*store, {file.string()}, pool, cancel);
      ASSERT_EQ(imported.size(), 1u);
      id = imported[0].input.id;
      json d = json::object();
      d.set("role", json::string("dark"));
      d.set("combine_method", json::string("mean"));
      json doc = json::object();
      doc.set("format", json::string("astro.input_declarations.v1"));
      doc.set("declared_image_fields", json::object());
      doc.set("master_descriptor", std::move(d));
      classification_record r;
      r.input_id = id;
      r.role = "dark";
      r.source = "user";
      r.evidence_canonical = "{}";
      r.declarations_canonical = canonical_json(doc);
      store->append_classifications({r});
      // controllo positivo: il record valido è letto
      EXPECT_NO_THROW((void)analyze_batch(*store, {}, pool, cancel));
    }
    {
      json d = json::object();
      d.set("role", json::string("dark"));
      d.set(t.key, json::string(t.value));
      json doc = json::object();
      doc.set("format", json::string("astro.input_declarations.v1"));
      doc.set("declared_image_fields", json::object());
      doc.set("master_descriptor", std::move(d));
      store_test::raw_db db(project);
      const std::string sql =
          "INSERT INTO input_classifications(input_id, revision, role, source, rule_id, evidence_canonical, "
          "declarations_canonical, record_version) SELECT input_id, max(revision) + 1, 'dark', 'user', NULL, '{}', '" +
          canonical_json(doc) + "', 1 FROM input_classifications WHERE input_id = " + std::to_string(id);
      ASSERT_EQ(db.exec(sql), SQLITE_OK);
    }
    auto reopened = sqlite_dag_store::open(project, open_intent::read_write);
    EXPECT_STOP(analyze_batch(*reopened, {}, pool, cancel), error_category::integrity, "batch.report_malformed");
  }
}

}  // namespace
