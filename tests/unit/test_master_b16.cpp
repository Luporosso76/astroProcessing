// M3.b, unità B16: job `master.combine` generale, anche da RAW. C-02.b §b.2 («Selezione ed esecuzione», «RAW»), §b.8,
// §b.10; ADR-16.b §b.7 (P-LOST del master); decisioni L1 (registro: due voci per lo stesso operatore, una per formato),
// L8 (parametro `analysis` del nodo), L10 (frame RAW: lettura verificata, decodifica in `prepare` uno alla volta, spill
// dichiarato del job, fasce lette dallo spill, riverifica in `finalize`; senza spill stop prima di ogni lettura).
// Test:
// - T-M3B-JOB-01: registro dei tipi di job (voci, varianti, nodi) e job minimo di ogni variante contro il master
//   calcolato a mano; la conformità T-M3BASE-xx gira per ogni voce e variante in `test_conformance.cpp` (fixture
//   `conformance_inputs.hpp` estesa ai ruoli `frame<K>` RAW e alle varianti flat);
// - T-M3A-CAL-09 (parte master) e T-M3B-C02-15 sul percorso del job: stato o identità del decoder diversi alla nuova
//   decodifica, frame modificato prima del job e dopo la lettura completa, pixel `support_incomplete` senza cambio di
//   metodo;
// - T-M3B-FLAT-06 sul percorso del job: stop di ricetta, mancanza di spill e RAW con FITS insieme prima di ogni lettura,
//   con il contatore del `fault_injector` dello store uguale a quello di uno stop di riferimento;
// - T-M3B-C02-10 e T-M3B-FLAT-08, parte riapertura e ricostruzione del nodo (RAW), invarianza del job RAW a blocchi e
//   worker, quota dello spill n·S·6, candidati permutati con parametro `analysis` (digest dell'artefatto).
// Fuori da questo file: lettore RAW delle statistiche e master CFA in memoria (B15), report (B11), proposta (B14).
// - controllo termico `flat.bias` con il master eseguito (CL-06 (c), decisione B16 «Controllo termico di `flat.bias`
//   dopo il master»): `analyze_batch` con il candidato proposto, job del master, nuovo `analyze_batch` con lo slot `current`;
// - lettore RAW di B15 con i fixture esistenti: esiti di file diversi da `raw.max_pixels` (gli stop del lettore sono provati in B15) (decisione B16, «Lettore
//   RAW»).
//
// Nessun valore atteso viene dal prodotto. I master attesi sono calcolati qui dalle formule dei generatori (DNG di
// FIX-SYN-DNG-01 e frame di FIX-SYN-FITS-01): per le prove sui RAW i valori sono interi e i divisori potenze di due, quindi
// la media e la mediana di quattro frame sono esatte in f64. Dove il riferimento è `combine_master` (invarianza a blocchi,
// worker, ricostruzione) lo dice il commento del test; la correttezza dei valori è di T-M3B-C02-01…07.
//
// Convenzioni (decisioni B16 in docs/decisions/M3b-lacune.md):
// - nomi delle varianti: voce FITS `unica`, `dark_mediana`, `flat_media`; voce RAW `raw_dark_mediana`, che passa lo
//   scratch (`scratch = true`) e lo scheduler;
// - contesto di `master.decoder_state_changed`: `frame` = nome canonico (frame_000 = digest minimo), `campo` = membro
//   diverso (per l'identità `decoder_identity.<membro>`) e `predicate` = P-LOST (ADR-16.b §b.13);
// - `spill.quota_exceeded` (categoria budget) con quota n·S·6 − 1; la quota n·S·6 basta (L10).
#include <gtest/gtest.h>

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "astro/batch_classification.hpp"
#include "astro/calibration.hpp"
#include "astro/calibration_analysis.hpp"
#include "astro/cancel.hpp"
#include "astro/error.hpp"
#include "astro/fits.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "astro/input_format.hpp"
#include "astro/job_probe.hpp"
#include "astro/job_types.hpp"
#include "astro/json.hpp"
#include "astro/master.hpp"
#include "astro/master_analysis.hpp"
#include "astro/memory_pool.hpp"
#include "astro/node.hpp"
#include "astro/raw_decode.hpp"
#include "astro/resources.hpp"
#include "astro/sqlite_dag_store.hpp"
#include "conformance_inputs.hpp"
#include "master_support.hpp"
#include "simd_support.hpp"
#include "syn_dng.hpp"

namespace {

using namespace mst;
namespace cf = conformance_fixture;

constexpr std::uint64_t kMiB = std::uint64_t{1} << 20;

// ---------------------------------------------------------------- DNG sintetici (stessa famiglia di B15)

constexpr std::uint32_t kW = 64;
constexpr std::uint32_t kH = 48;
constexpr std::uint16_t kBlack = 64;
constexpr double kWhite = 4000.0;
constexpr double kOffset = 32.0;
constexpr std::size_t kN = 4;  // frame per master: media e mediana (media dei due centrali) esatte in f64

struct site_values {
  std::uint16_t r, g1, g2, b;
};

// Campione del pixel (y, x): base del sito RGGB (0,0) R, (0,1) G1, (1,0) G2, (1,1) B più 100 sul terzo blocco 2 x 2 di
// ogni tre. Il termine in più è uguale in tutti i frame: l'ordine dei frame in un pixel è quello delle basi.
std::uint16_t sample_of(site_values v, std::uint32_t y, std::uint32_t x) {
  const std::uint16_t site[4] = {v.r, v.g1, v.g2, v.b};
  const std::uint32_t j = (y / 2) * (kW / 2) + x / 2;
  return static_cast<std::uint16_t>(site[(y % 2) * 2 + x % 2] + (j % 3 == 2 ? 100 : 0));
}

std::vector<std::uint8_t> dng_of(site_values v, std::uint16_t black = kBlack, double white = kWhite) {
  fx::dng_spec s;
  s.width = kW;
  s.height = kH;
  s.white = static_cast<std::uint32_t>(white);
  s.black = {black};
  s.exposure_time = std::pair<std::uint32_t, std::uint32_t>{30, 1};
  s.iso = 800;
  std::vector<std::uint16_t> px(static_cast<std::size_t>(kW) * kH);
  for (std::uint32_t y = 0; y < kH; ++y)
    for (std::uint32_t x = 0; x < kW; ++x) px[static_cast<std::size_t>(y) * kW + x] = sample_of(v, y, x);
  s.pixels = fx::pixels_u16(px);
  return fx::make_dng(s);
}

// Basi crescenti con k per ogni sito: mediana di quattro = media dei frame 1 e 2. Livelli del flat (base − offset 32): G2
// 968…974, scarto relativo massimo da m̄ = 971 di 3/971 < 0.01.
site_values dark_site(std::size_t k) {
  const auto d = static_cast<std::uint16_t>(k);
  return {static_cast<std::uint16_t>(1000 + 3 * d), static_cast<std::uint16_t>(800 + 5 * d),
          static_cast<std::uint16_t>(700 + 2 * d), static_cast<std::uint16_t>(900 + 4 * d)};
}
site_values flat_site(std::size_t k) {
  const auto d = static_cast<std::uint16_t>(k);
  return {static_cast<std::uint16_t>(2000 + 2 * d), static_cast<std::uint16_t>(1500 + 2 * d),
          static_cast<std::uint16_t>(1000 + 2 * d), static_cast<std::uint16_t>(1200 + 2 * d)};
}
site_values site_of(master_role role, std::size_t k) { return role == master_role::flat ? flat_site(k) : dark_site(k); }

// Precondizioni della fixture sulla decodifica (come B15): motivo, campioni uguali al file (nero non sottratto), soglia di
// saturazione = WhiteLevel, stato del decoder presente.
image decoded(const std::vector<std::uint8_t>& bytes, site_values v) {
  image im = decode_raw_buffer(bytes);
  EXPECT_TRUE(im.decoder_state.has_value());
  EXPECT_TRUE(im.cfa.has_value() && im.cfa->pattern == cfa_pattern::rggb);
  EXPECT_EQ(im.geometry.width, kW);
  EXPECT_EQ(im.geometry.height, kH);
  EXPECT_TRUE(im.metadata.saturation_adu.has_value() && im.metadata.saturation_adu->value == kWhite);
  const float* p = im.data<float>();
  for (std::uint32_t y = 0; y < kH; ++y)
    for (std::uint32_t x = 0; x < kW; ++x) {
      if (p[static_cast<std::size_t>(y) * kW + x] != static_cast<float>(sample_of(v, y, x))) {
        ADD_FAILURE() << "fixture: campione decodificato diverso dal file in (" << y << ", " << x << ")";
        return im;
      }
    }
  return im;
}

struct raw_set {
  master_role role = master_role::dark;
  std::vector<std::vector<std::uint8_t>> bytes;
  std::vector<image> images;
  std::vector<std::string> paths;
};

void write_file(const std::string& path, const std::vector<std::uint8_t>& bytes) { cal::write_bytes(path, bytes); }

raw_set make_raw_set(const fs::path& dir, master_role role) {
  fs::create_directories(dir);
  raw_set s;
  s.role = role;
  for (std::size_t k = 0; k < kN; ++k) {
    s.bytes.push_back(dng_of(site_of(role, k)));
    s.images.push_back(decoded(s.bytes.back(), site_of(role, k)));
    s.paths.push_back((dir / ("frame" + std::to_string(k) + ".dng")).string());
    write_file(s.paths.back(), s.bytes.back());
  }
  return s;
}

// Ricetta del job sui frame RAW scritti: stato del decoder e identità dalla decodifica di questo processo (quelli che
// l'analisi ha osservato), filtro e binning dichiarati, tolleranza del nero 0, `offset_adu` solo per il flat (L9).
master_recipe raw_recipe(const raw_set& s) {
  master_recipe r;
  r.role = s.role;
  for (std::size_t k = 0; k < s.bytes.size(); ++k) {
    master_frame_spec f;
    f.path = s.paths[k];
    f.digest = digest_of(s.bytes[k]);
    if (s.role == master_role::flat) f.declared.filter = metadata_value<std::string>{"L", {metadata_origin::declared, {}}};
    f.declared_fields.binning_x = 1;
    f.declared_fields.binning_y = 1;
    f.decoder = frame_decoder_state{*s.images[k].decoder_state, current_decoder_identity()};
    r.frames.push_back(std::move(f));
  }
  r.combine = combine_method::mean;
  r.rejection = master_rejection::none;
  if (s.role == master_role::flat) {
    r.flat_norm = flat_normalization::none;
    r.flat_level_rel_tol = 0.01;
    r.flat_min_signal_adu = 900.0;
    r.offset_adu = kOffset;
  }
  r.saturation_adu = kWhite;
  r.saturated_fraction_max = 0.5;
  r.exposure_rel_tol = 1e-9;
  r.temperature_abs_tol_c = 0.5;
  r.temp_policy = temperature_policy::not_applicable_declared;
  r.decoder_black_abs_tol_adu = 0;
  r.simd = simd_test::requested();
  r.output_type = sample_type::f64;
  return r;
}

// Master atteso a mano: media (somma di interi / 4) o mediana di quattro (media dei frame 1 e 2, ordinati per base).
double expected_sample(master_role role, combine_method m, std::uint32_t y, std::uint32_t x) {
  double v[kN];
  for (std::size_t k = 0; k < kN; ++k) v[k] = sample_of(site_of(role, k), y, x);
  if (m == combine_method::median) return (v[1] + v[2]) / 2;
  return (v[0] + v[1] + v[2] + v[3]) / 4;
}

// ---------------------------------------------------------------- store, probe, esecuzione

struct project {
  std::unique_ptr<sqlite_dag_store> store;
  node_spec node;
};

project make_project(const fs::path& root, const master_recipe& node_recipe) {
  project p{sqlite_dag_store::create(root), make_master_node(node_recipe)};
  p.store->put_node(p.node);
  p.store->create_slot("master", p.node.key());
  return p;
}

resource_config raw_resources(const fs::path& scratch, std::optional<std::uint64_t> quota, std::optional<unsigned> workers = 2,
                              std::optional<std::uint32_t> tile_rows = 7) {
  resource_config rc;
  if (quota) {
    fs::create_directories(scratch);
    rc.scratch = dir_quota{fs::absolute(scratch), *quota};
  }
  rc.worker_count = workers;
  rc.tile_rows = tile_rows;
  return rc;
}

constexpr std::uint64_t kSpillBytes = std::uint64_t{kN} * kW * kH * 6;  // f32 + maschera uint16 per pixel (L10)

// Osservatore dei punti del confine: aperture degli input e task eseguiti.
struct probe_scope {
  std::atomic<int> opens{0}, tasks{0}, plans{0};
  probe_scope() {
    astro::job_probe::set_observer([this](std::string_view point, std::string_view) {
      if (point == "input.open") ++opens;
      if (point == "sched.task") ++tasks;
      if (point == "sched.plan") ++plans;
    });
  }
  ~probe_scope() { astro::job_probe::set_observer({}); }
  probe_scope(const probe_scope&) = delete;
  probe_scope& operator=(const probe_scope&) = delete;
};

struct run_info {
  std::optional<stop_error> stop;
  int exit_code = -1;
  bool frames_read = false;
  int faults = 0;
  int opens = 0, tasks = 0, plans = 0;
  bool primary = false;
};

run_info run_job(project& p, const master_recipe& job_recipe, const resource_config& rc,
                 const std::function<void()>& on_frames_read = {}) {
  run_info out;
  int faults = 0;
  p.store->set_fault_injector([&faults](std::string_view) {
    ++faults;
    return 0;
  });
  probe_scope probe;
  master_job_options o;
  o.resources = rc;
  o.hook = [&](std::string_view point) {
    if (point != "frames.read") return;
    out.frames_read = true;
    if (on_frames_read) on_frames_read();
  };
  memory_pool pool(256 * kMiB);
  try {
    const job_outcome res = run_master_quiet(*p.store, "master", job_recipe, pool, o);
    out.exit_code = res.exit_code;
    out.stop = res.stop;
  } catch (const stop_error& e) {
    out.stop = e;
  }
  p.store->set_fault_injector({});
  out.faults = faults;
  out.opens = probe.opens;
  out.tasks = probe.tasks;
  out.plans = probe.plans;
  out.primary = by_role(*p.store, p.node.key()).count("primary") != 0;
  return out;
}

// Stop di riferimento per il contatore: ricetta senza `combine` (master.param.combine, B6).
int baseline_faults(const fs::path& dir, const master_recipe& good, const resource_config& rc) {
  master_recipe stop_recipe = good;
  stop_recipe.combine.reset();
  project p = make_project(dir, good);
  const run_info run = run_job(p, stop_recipe, rc);
  EXPECT_TRUE(run.stop.has_value() && run.stop->code() == "master.param.combine");
  return run.faults;
}

image primary_of(const project& p) {
  return decode_fits(read_artifact(*p.store, by_role(*p.store, p.node.key()).at("primary")), cal::big_budget);
}

std::optional<std::string> context_of(const stop_error& e, const std::string& key) {
  for (const auto& [k, v] : e.context())
    if (k == key) return v;
  return std::nullopt;
}

// Nome canonico del frame k della ricetta: frame_NNN con NNN = posizione per digest crescente.
std::string canonical_name(const master_recipe& r, std::size_t k) {
  std::size_t rank = 0;
  for (const master_frame_spec& f : r.frames)
    if (f.digest.sha256 < r.frames[k].digest.sha256) ++rank;
  char buf[16];
  std::snprintf(buf, sizeof buf, "frame_%03zu", rank);
  return buf;
}

void flip_last_byte(const std::string& path) {
  std::string text = cal::read_text(path);
  text.back() = static_cast<char>(text.back() ^ 0x01);
  cal::write_bytes(path, std::vector<std::uint8_t>(text.begin(), text.end()));
}

// Confronto bit per bit dei campioni, del piano e della maschera con `combine_master` sugli stessi frame.
::testing::AssertionResult equals_in_memory(const image& got, const master_recipe& r, const raw_set& s) {
  std::vector<image> ims = s.images;
  const master_result ref = combine_master(r, std::move(ims));
  if (same_bits(got, ref.output)) return ::testing::AssertionSuccess();
  return ::testing::AssertionFailure() << "il master del job differisce da combine_master (campioni, piano o maschera)";
}

// ================================================================ T-M3B-JOB-01: registro dei tipi di job

std::vector<const job_type_entry*> master_entries() {
  std::vector<const job_type_entry*> out;
  for (const job_type_entry& e : registered_job_types())
    if (std::string(e.operator_id) == master_operator_id) out.push_back(&e);
  return out;
}

bool all_roles_have_format(const job_type_entry& e, const input_format& f) {
  for (const std::string& role : e.input_roles) {
    const auto it = e.role_formats.find(role);
    if (it == e.role_formats.end() || it->second != &f) return false;
  }
  return !e.input_roles.empty();
}

const job_type_entry* master_entry(bool raw) {
  for (const job_type_entry* e : master_entries())
    if (all_roles_have_format(*e, raw ? raw_input_format() : fits_input_format())) return e;
  return nullptr;
}

const job_type_variant* variant_named(const job_type_entry& e, const std::string& name) {
  for (const job_type_variant& v : e.variants)
    if (v.name == name) return &v;
  return nullptr;
}

TEST(MasterB16, T_M3B_JOB_01_registry_has_one_master_combine_entry_per_input_format) {
  // L1 opzione B: stesso operatore, una voce con i ruoli FITS e una con i ruoli RAW, tre frame ciascuna.
  EXPECT_EQ(master_entries().size(), 2u);
  for (const bool raw : {false, true}) {
    SCOPED_TRACE(raw ? "raw" : "fits");
    const job_type_entry* e = master_entry(raw);
    ASSERT_NE(e, nullptr) << "voce master.combine con ruoli " << (raw ? "RAW" : "FITS") << " assente";
    EXPECT_EQ(e->input_roles, (std::vector<std::string>{"frame0", "frame1", "frame2"}));
    EXPECT_TRUE(static_cast<bool>(e->make));
    EXPECT_TRUE(e->scheduler);
  }
}

TEST(MasterB16, T_M3B_JOB_01_fits_entry_declares_the_variants_unica_dark_mediana_and_flat_media) {
  const job_type_entry* e = master_entry(false);
  ASSERT_NE(e, nullptr);
  for (const char* name : {"unica", "dark_mediana", "flat_media"}) {
    EXPECT_NE(variant_named(*e, name), nullptr) << "variante assente: " << name;
  }
  EXPECT_EQ(e->variants.size(), 3u);
}

TEST(MasterB16, T_M3B_JOB_01_raw_entry_declares_the_variant_raw_dark_mediana) {
  const job_type_entry* e = master_entry(true);
  ASSERT_NE(e, nullptr);
  EXPECT_NE(variant_named(*e, "raw_dark_mediana"), nullptr);
  EXPECT_EQ(e->variants.size(), 1u);
}

TEST(MasterB16, T_M3B_JOB_01_variant_names_are_distinct_across_the_entries_of_the_operator) {
  // Il nome di un caso della suite è l'operatore più la variante (L1): due voci non possono ripeterlo.
  std::vector<std::string> names;
  for (const job_type_entry* e : master_entries())
    for (const job_type_variant& v : e->variants) names.push_back(v.name);
  std::sort(names.begin(), names.end());
  EXPECT_EQ(std::adjacent_find(names.begin(), names.end()), names.end());
}

TEST(MasterB16, T_M3B_JOB_01_raw_variant_passes_the_declared_scratch_to_the_job) {
  const job_type_entry* e = master_entry(true);
  ASSERT_NE(e, nullptr);
  const job_type_variant* v = variant_named(*e, "raw_dark_mediana");
  ASSERT_NE(v, nullptr);
  EXPECT_TRUE(v->scratch) << "L10: i frame RAW passano dallo spill dichiarato nelle risorse del job";
  EXPECT_FALSE(v->checkpoint);
}

struct variant_case {
  bool raw;
  const char* name;
  master_role role;
  combine_method method;
};
const variant_case kVariantCases[] = {
    {false, "unica", master_role::dark, combine_method::mean},
    {false, "dark_mediana", master_role::dark, combine_method::median},
    {false, "flat_media", master_role::flat, combine_method::mean},
    {true, "raw_dark_mediana", master_role::dark, combine_method::median},
};

TEST(MasterB16, T_M3B_JOB_01_every_master_variant_builds_the_node_of_its_role_and_method) {
  const fs::path dir = fresh_dir("b16-variant-nodes");
  for (const variant_case& c : kVariantCases) {
    SCOPED_TRACE(c.name);
    const job_type_entry* e = master_entry(c.raw);
    ASSERT_NE(e, nullptr);
    ASSERT_NE(variant_named(*e, c.name), nullptr);
    const job_type_inputs in = cf::write_inputs(*e, dir / c.name, c.name);
    const minimal_job job = e->make(in, c.name, dir / (std::string(c.name) + "-store"));
    EXPECT_EQ(job.node.operator_id(), std::string(master_operator_id));
    const master_descriptor d = master_descriptor_from_node(job.node);
    EXPECT_EQ(d.role, c.role);
    ASSERT_TRUE(d.combine.has_value());
    EXPECT_EQ(*d.combine, c.method);
    EXPECT_EQ(d.combine_count, std::optional<std::uint32_t>(3));
  }
}

TEST(MasterB16, T_M3B_JOB_01_raw_variant_node_carries_one_decoder_state_per_frame_and_fits_variants_none) {
  const fs::path dir = fresh_dir("b16-variant-decoder-states");
  for (const variant_case& c : kVariantCases) {
    SCOPED_TRACE(c.name);
    const job_type_entry* e = master_entry(c.raw);
    ASSERT_NE(e, nullptr);
    ASSERT_NE(variant_named(*e, c.name), nullptr);
    const job_type_inputs in = cf::write_inputs(*e, dir / c.name, c.name);
    const minimal_job job = e->make(in, c.name, dir / (std::string(c.name) + "-store"));
    const master_descriptor d = master_descriptor_from_node(job.node);
    if (c.raw) {
      ASSERT_TRUE(d.frame_decoder_states.has_value());
      EXPECT_EQ(d.frame_decoder_states->size(), 3u);
    } else {
      EXPECT_FALSE(d.frame_decoder_states.has_value());
    }
  }
}

// Master atteso dei frame di `conformance_inputs.hpp` (verità analitica dalle funzioni di FIX-SYN-FITS-01, valori
// memorizzati f32): dark = b + 60 d + K, flat = b + 5 d + q. Tolleranza: mezzo ulp f32 dell'arrotondamento d'uscita.
double fits_value(master_role role, std::uint32_t k, std::uint32_t x, std::uint32_t y) {
  const double b = fx::synth_bias(x, y), d = fx::synth_dark_rate(x, y), q = fx::synth_flat_response(x, y);
  const double v = role == master_role::flat ? b + 5 * d + q : b + 60 * d + k;
  return static_cast<double>(static_cast<float>(v));
}

double half_ulp_f32_of(double v) {
  return std::ldexp(1.0, std::ilogb(std::max(std::fabs(v), std::numeric_limits<double>::min())) - 24) * (1 + 1e-6);
}

constexpr std::uint16_t kThirdOffset = 5;

TEST(MasterB16, T_M3B_JOB_01_every_master_variant_publishes_the_hand_computed_master) {
  const fs::path dir = fresh_dir("b16-variant-run");
  for (const variant_case& c : kVariantCases) {
    SCOPED_TRACE(c.name);
    const job_type_entry* e = master_entry(c.raw);
    ASSERT_NE(e, nullptr);
    ASSERT_NE(variant_named(*e, c.name), nullptr);
    const fs::path root = dir / c.name;
    job_type_inputs in = cf::write_inputs(*e, root / "inputs", c.name);
    if (c.role == master_role::dark) {
      // Terzo frame con scarto asimmetrico (K = 5 invece di 2): media e mediana dei tre differiscono.
      const fx::bytes third = c.raw ? cf::dng_frame(kThirdOffset) : cf::frame(cf::kind::dark, kThirdOffset);
      write_file(in.at("frame2").path, third);
      in.at("frame2").digest = digest_of(third);
    }
    auto store = sqlite_dag_store::create(root / "store");
    const minimal_job job = e->make(in, c.name, store->root());
    store->put_node(job.node);
    store->create_slot("s", job.node.key());
    resource_config rc;
    rc.scratch = dir_quota{fs::absolute(root / "scratch"), 64 * kMiB};
    rc.worker_count = 2u;
    rc.tile_rows = 1u;
    fs::create_directories(root / "scratch");
    memory_pool pool(256 * kMiB);
    const cancel_token cancel;
    std::optional<job_outcome> result;
    testing::internal::CaptureStderr();
    try {
      result = job.run(job_type_env{*store, "s", pool, cancel, std::nullopt, rc, "ckpt", false});
    } catch (const stop_error& ex) {
      testing::internal::GetCapturedStderr();
      FAIL() << "stop inatteso: " << ex.what();
    }
    testing::internal::GetCapturedStderr();
    ASSERT_TRUE(result.has_value());
    const job_outcome& out = *result;
    ASSERT_EQ(out.exit_code, 0) << (out.stop ? out.stop->what() : "");
    const auto roles = by_role(*store, job.node.key());
    ASSERT_EQ(roles.count("primary"), 1u);
    const image m = decode_fits(read_artifact(*store, roles.at("primary")), cal::big_budget);
    if (c.raw) {
      ASSERT_EQ(m.geometry.width, cf::dng_width);
      ASSERT_EQ(m.geometry.height, cf::dng_height);
      // Frame k: base (100 + (r·37 + c·11) mod 3500), primo campione + k, il terzo + 5 (nero scalare 0, non sottratto).
      // Mediana di tre: il frame 1.
      std::size_t wrong = 0;
      for (std::uint32_t r = 0; r < cf::dng_height; ++r)
        for (std::uint32_t col = 0; col < cf::dng_width; ++col) {
          double want = 100 + (r * 37 + col * 11) % 3500;
          if (r == 0 && col == 0) want += 1;
          if (sample(m, static_cast<std::size_t>(r) * cf::dng_width + col) != want) ++wrong;
        }
      EXPECT_EQ(wrong, 0u);
    } else {
      ASSERT_EQ(m.geometry.width, cf::width);
      ASSERT_EQ(m.geometry.height, cf::height);
      std::size_t wrong = 0;
      for (std::uint32_t y = 0; y < cf::height; ++y)
        for (std::uint32_t x = 0; x < cf::width; ++x) {
          const std::uint32_t k2 = c.role == master_role::dark ? kThirdOffset : 2;
          const double a = fits_value(c.role, 0, x, y), b = fits_value(c.role, 1, x, y), d = fits_value(c.role, k2, x, y);
          const double want = c.method == combine_method::median ? b : (a + b + d) / 3;
          const double got = sample(m, static_cast<std::size_t>(y) * cf::width + x);
          if (std::fabs(got - want) > half_ulp_f32_of(want)) ++wrong;
        }
      EXPECT_EQ(wrong, 0u);
    }
  }
}

TEST(MasterB16, T_M3B_JOB_01_raw_variant_without_declared_scratch_stops_before_reading_any_input) {
  const job_type_entry* e = master_entry(true);
  ASSERT_NE(e, nullptr);
  ASSERT_NE(variant_named(*e, "raw_dark_mediana"), nullptr);
  const fs::path root = fresh_dir("b16-variant-no-scratch");
  const job_type_inputs in = cf::write_inputs(*e, root / "inputs", "raw_dark_mediana");
  auto store = sqlite_dag_store::create(root / "store");
  const minimal_job job = e->make(in, "raw_dark_mediana", store->root());
  store->put_node(job.node);
  store->create_slot("s", job.node.key());
  resource_config rc;  // nessuno scratch dichiarato
  rc.worker_count = 2u;
  rc.tile_rows = 1u;
  memory_pool pool(256 * kMiB);
  const cancel_token cancel;
  probe_scope probe;
  std::optional<stop_error> stop;
  testing::internal::CaptureStderr();
  try {
    const job_outcome out = job.run(job_type_env{*store, "s", pool, cancel, std::nullopt, rc, "ckpt", false});
    stop = out.stop;
  } catch (const stop_error& ex) {
    stop = ex;
  }
  testing::internal::GetCapturedStderr();
  ASSERT_TRUE(stop.has_value()) << "nessuno spill dichiarato: stop atteso (L10, nessun percorso residente alternativo)";
  EXPECT_EQ(stop->category(), error_category::invalid_input) << stop->what();
  EXPECT_EQ(stop->code(), "master.raw_requires_spill") << stop->what();
  EXPECT_EQ(context_of(*stop, "frame"), std::optional<std::string>("frame_000")) << stop->what();
  EXPECT_EQ(context_of(*stop, "n"), std::optional<std::string>("3")) << stop->what();
  EXPECT_EQ(probe.opens.load(), 0) << "nessun input aperto prima dello stop";
  EXPECT_EQ(by_role(*store, job.node.key()).count("primary"), 0u);
}

TEST(MasterB16, T_M3B_JOB_01_raw_variant_with_different_saturation_thresholds_between_frames_stops_before_building_the_node) {
  const job_type_entry* e = master_entry(true);
  ASSERT_NE(e, nullptr);
  const fs::path root = fresh_dir("b16-variant-saturation");
  job_type_inputs in = cf::write_inputs(*e, root / "inputs", "raw_dark_mediana");
  const std::vector<std::uint8_t> other_white = dng_of(dark_site(1), kBlack, kWhite - 1);
  write_file(in.at("frame1").path, other_white);
  in.at("frame1").digest = digest_of(other_white);
  try {
    (void)e->make(in, "raw_dark_mediana", root / "store");
    ADD_FAILURE() << "soglia di saturazione diversa fra i frame: stop atteso";
  } catch (const stop_error& ex) {
    EXPECT_EQ(ex.category(), error_category::invalid_input) << ex.what();
    EXPECT_EQ(ex.code(), "job_types.saturation_adu") << ex.what();
    EXPECT_EQ(context_of(ex, "ruolo"), std::optional<std::string>("frame1")) << ex.what();
    EXPECT_EQ(context_of(ex, "valore"), std::optional<std::string>(std::to_string(kWhite - 1))) << ex.what();
  }
}

// ================================================================ T-M3A-CAL-09 (parte master)

TEST(MasterB16, T_M3A_CAL_09_job_with_the_recipe_states_equal_to_the_decoded_ones_publishes_the_master) {
  // Controllo delle prove che seguono: stato e identità osservati dall'analisi uguali a quelli della decodifica.
  const fs::path dir = fresh_dir("b16-cal09-control");
  const raw_set s = make_raw_set(dir / "frames", master_role::dark);
  const master_recipe r = raw_recipe(s);
  project p = make_project(dir / "store", r);
  const run_info run = run_job(p, r, raw_resources(dir / "scratch", 64 * kMiB));
  ASSERT_FALSE(run.stop.has_value()) << run.stop->what();
  EXPECT_EQ(run.exit_code, 0);
  EXPECT_TRUE(run.primary);
}

TEST(MasterB16, T_M3A_CAL_09_job_frame_decoded_with_a_different_black_stops_with_decoder_state_changed_on_black) {
  // Il frame 2 è stato sostituito da un DNG con BlackLevel 65: la ricetta porta lo stato che l'analisi ha osservato
  // (nero 64) con il digest del file di oggi; i campioni sono uguali, cambia solo il nero dichiarato dal decoder.
  const fs::path dir = fresh_dir("b16-cal09-black");
  raw_set s = make_raw_set(dir / "frames", master_role::dark);
  master_recipe r = raw_recipe(s);
  const std::vector<std::uint8_t> changed = dng_of(dark_site(2), kBlack + 1);
  write_file(s.paths[2], changed);
  r.frames[2].digest = digest_of(changed);
  const resource_config rc = raw_resources(dir / "scratch", 64 * kMiB);
  const int baseline = baseline_faults(dir / "store-baseline", r, rc);
  project p = make_project(dir / "store", r);
  const run_info run = run_job(p, r, rc);
  ASSERT_TRUE(run.stop.has_value()) << "nero diverso alla nuova decodifica: stop atteso";
  EXPECT_EQ(run.stop->category(), error_category::integrity) << run.stop->what();
  EXPECT_EQ(run.stop->code(), "master.decoder_state_changed") << run.stop->what();
  EXPECT_EQ(context_of(*run.stop, "campo"), std::optional<std::string>("black")) << run.stop->what();
  EXPECT_EQ(context_of(*run.stop, "frame"), std::optional<std::string>(canonical_name(r, 2))) << run.stop->what();
  EXPECT_EQ(context_of(*run.stop, "predicate"), std::optional<std::string>("P-LOST")) << run.stop->what();
  EXPECT_FALSE(run.primary);
  EXPECT_EQ(run.tasks, 0) << "nessun calcolo della combinazione dopo lo stop";
  EXPECT_EQ(run.faults, baseline) << "nessun altro nodo né lavoro sullo store oltre lo stop di riferimento";
}

TEST(MasterB16, T_M3A_CAL_09_job_frame_decoded_with_a_different_identity_and_equal_state_stops_with_decoder_state_changed) {
  // `decoder_identity` della ricetta diversa da quella di questo processo (SHA-256 del tarball), stato uguale: l'identità
  // si confronta fra analisi ed esecuzione dello stesso ingresso (ADR-16.b §b.7).
  const fs::path dir = fresh_dir("b16-cal09-identity");
  const raw_set s = make_raw_set(dir / "frames", master_role::dark);
  master_recipe r = raw_recipe(s);
  std::string other(64, 'a');
  if (r.frames[1].decoder->identity.libraw_tarball_sha256 == other) other.assign(64, 'b');
  r.frames[1].decoder->identity.libraw_tarball_sha256 = other;
  const resource_config rc = raw_resources(dir / "scratch", 64 * kMiB);
  const int baseline = baseline_faults(dir / "store-baseline", r, rc);
  project p = make_project(dir / "store", r);
  const run_info run = run_job(p, r, rc);
  ASSERT_TRUE(run.stop.has_value()) << "identità del decoder diversa alla nuova decodifica: stop atteso";
  EXPECT_EQ(run.stop->category(), error_category::integrity) << run.stop->what();
  EXPECT_EQ(run.stop->code(), "master.decoder_state_changed") << run.stop->what();
  EXPECT_EQ(context_of(*run.stop, "campo"), std::optional<std::string>("decoder_identity.libraw_tarball_sha256"))
      << run.stop->what();
  EXPECT_EQ(context_of(*run.stop, "frame"), std::optional<std::string>(canonical_name(r, 1))) << run.stop->what();
  EXPECT_EQ(context_of(*run.stop, "predicate"), std::optional<std::string>("P-LOST")) << run.stop->what();
  EXPECT_FALSE(run.primary);
  EXPECT_EQ(run.tasks, 0);
  EXPECT_EQ(run.faults, baseline);
}

TEST(MasterB16, T_M3A_CAL_09_job_frame_with_a_different_libraw_version_and_equal_state_stops_with_decoder_state_changed) {
  // Versione di LibRaw della ricetta diversa da quella di questo processo, stato e SHA-256 uguali: primo membro
  // dell'identità nell'ordine del record.
  const fs::path dir = fresh_dir("b16-cal09-version");
  const raw_set s = make_raw_set(dir / "frames", master_role::dark);
  master_recipe r = raw_recipe(s);
  r.frames[0].decoder->identity.libraw_version += "-altra";
  const resource_config rc = raw_resources(dir / "scratch", 64 * kMiB);
  const int baseline = baseline_faults(dir / "store-baseline", r, rc);
  project p = make_project(dir / "store", r);
  const run_info run = run_job(p, r, rc);
  ASSERT_TRUE(run.stop.has_value()) << "versione del decoder diversa alla nuova decodifica: stop atteso";
  EXPECT_EQ(run.stop->category(), error_category::integrity) << run.stop->what();
  EXPECT_EQ(run.stop->code(), "master.decoder_state_changed") << run.stop->what();
  EXPECT_EQ(context_of(*run.stop, "campo"), std::optional<std::string>("decoder_identity.libraw_version")) << run.stop->what();
  EXPECT_EQ(context_of(*run.stop, "frame"), std::optional<std::string>(canonical_name(r, 0))) << run.stop->what();
  EXPECT_EQ(context_of(*run.stop, "predicate"), std::optional<std::string>("P-LOST")) << run.stop->what();
  EXPECT_FALSE(run.primary);
  EXPECT_EQ(run.tasks, 0);
  EXPECT_EQ(run.faults, baseline);
}

TEST(MasterB16, T_M3A_CAL_09_job_frame_with_black_and_identity_both_different_reports_the_identity_first) {
  // Decisione B16: il job confronta prima l'identità, poi lo stato.
  const fs::path dir = fresh_dir("b16-cal09-both");
  raw_set s = make_raw_set(dir / "frames", master_role::dark);
  master_recipe r = raw_recipe(s);
  const std::vector<std::uint8_t> changed = dng_of(dark_site(2), kBlack + 1);
  write_file(s.paths[2], changed);
  r.frames[2].digest = digest_of(changed);
  std::string other(64, 'a');
  if (r.frames[2].decoder->identity.libraw_tarball_sha256 == other) other.assign(64, 'b');
  r.frames[2].decoder->identity.libraw_tarball_sha256 = other;
  project p = make_project(dir / "store", r);
  const run_info run = run_job(p, r, raw_resources(dir / "scratch", 64 * kMiB));
  ASSERT_TRUE(run.stop.has_value());
  EXPECT_EQ(run.stop->code(), "master.decoder_state_changed") << run.stop->what();
  EXPECT_EQ(context_of(*run.stop, "predicate"), std::optional<std::string>("P-LOST")) << run.stop->what();
  EXPECT_EQ(context_of(*run.stop, "campo"), std::optional<std::string>("decoder_identity.libraw_tarball_sha256"))
      << run.stop->what();
  EXPECT_EQ(context_of(*run.stop, "frame"), std::optional<std::string>(canonical_name(r, 2))) << run.stop->what();
}

// ================================================================ T-M3B-C02-15 sul percorso del job

TEST(MasterB16, T_M3B_C02_15_job_raw_frame_modified_before_the_run_stops_with_digest_mismatch_before_any_task) {
  const fs::path dir = fresh_dir("b16-c15-before");
  const raw_set s = make_raw_set(dir / "frames", master_role::dark);
  const master_recipe r = raw_recipe(s);
  const resource_config rc = raw_resources(dir / "scratch", 64 * kMiB);
  const int baseline = baseline_faults(dir / "store-baseline", r, rc);
  project p = make_project(dir / "store", r);
  flip_last_byte(s.paths[2]);  // stessa dimensione, byte cambiato dopo la selezione
  const run_info run = run_job(p, r, rc);
  ASSERT_TRUE(run.stop.has_value()) << "frame cambiato dopo la selezione: stop atteso";
  EXPECT_EQ(run.stop->category(), error_category::integrity) << run.stop->what();
  EXPECT_EQ(run.stop->code(), "input.digest_mismatch") << run.stop->what();
  EXPECT_FALSE(run.primary);
  EXPECT_EQ(run.tasks, 0);
  EXPECT_EQ(run.faults, baseline) << "nessun altro nodo né lavoro sullo store oltre lo stop di riferimento";
  EXPECT_EQ(p.store->read_slot("master").state, freshness::missing) << "nessun master pubblicato";
}

TEST(MasterB16, T_M3B_C02_15_job_raw_frame_modified_after_the_full_read_stops_in_finalize_without_a_master) {
  // Riverifica in `finalize` (L10): il file cambia dopo la lettura completa e prima dei blocchi.
  const fs::path dir = fresh_dir("b16-c15-after");
  const raw_set s = make_raw_set(dir / "frames", master_role::dark);
  const master_recipe r = raw_recipe(s);
  project p = make_project(dir / "store", r);
  const run_info run = run_job(p, r, raw_resources(dir / "scratch", 64 * kMiB), [&] { flip_last_byte(s.paths[1]); });
  ASSERT_TRUE(run.frames_read) << "il gancio frames.read deve scattare anche con frame RAW";
  ASSERT_TRUE(run.stop.has_value()) << "file cambiato dopo la lettura: stop atteso";
  EXPECT_EQ(run.stop->category(), error_category::integrity) << run.stop->what();
  EXPECT_EQ(run.stop->code(), "input.digest_mismatch") << run.stop->what();
  EXPECT_EQ(context_of(*run.stop, "fase"), std::optional<std::string>("riverifica prima della pubblicazione")) << run.stop->what();
  EXPECT_EQ(context_of(*run.stop, "frame"), std::optional<std::string>(canonical_name(r, 1))) << run.stop->what();
  EXPECT_FALSE(run.primary);
  EXPECT_EQ(p.store->read_slot("master").state, freshness::missing);
}

TEST(MasterB16, T_M3B_C02_15_job_sigma_clip_pixel_below_min_kept_is_support_incomplete_and_the_method_is_unchanged) {
  // Pixel 0: {984, 1002, 1008, 1008, 1032}, media 1006.8, s = 17.18: κ = 1 scarta 984 e 1032 (scarti 22.8 e 25.2), restano
  // 3 < clip_min_kept 4. Gli altri pixel valgono 1000 in ogni frame (s = 0, nessun rifiuto, M = 1000).
  const fs::path dir = fresh_dir("b16-c15-support");
  group g = make_group(master_role::dark, 4, 2, 5);
  const double px0[5] = {984, 1002, 1008, 1008, 1032};
  for (std::size_t f = 0; f < 5; ++f) {
    std::fill(g.frames[f].v.begin(), g.frames[f].v.end(), 1000.0);
    g.frames[f].v[0] = px0[f];
  }
  master_recipe r = recipe_of(g);
  r.rejection = master_rejection::sigma_clip;
  r.clip_kappa_low = r.clip_kappa_high = 1.0;
  r.clip_max_iter = 1.0;
  r.clip_min_kept = 4.0;
  write_frames(g, r, dir, "f");
  project p = make_project(dir / "store", r);
  const run_info run = run_job(p, r, resource_config{});
  ASSERT_FALSE(run.stop.has_value()) << run.stop->what() << ": nessuno stop per un pixel support_incomplete";
  EXPECT_EQ(run.exit_code, 0);
  EXPECT_EQ(run.plans, 1) << "un solo piano eseguito: nessun altro metodo né nodo";
  const image m = primary_of(p);
  EXPECT_EQ(m.invalid.bits[0], invalid_reason::support_incomplete) << "solo il bit support_incomplete sul pixel 0";
  EXPECT_TRUE(std::isnan(sample(m, 0)));
  EXPECT_TRUE(std::isnan(plane(m, 0)));
  for (std::size_t i = 1; i < 8; ++i) {
    EXPECT_EQ(m.invalid.bits[i], 0u) << i;
    EXPECT_EQ(sample(m, i), 1000.0) << i;
  }
  const json rep = parse_json(cal::read_text(p.store->artifact_path(by_role(*p.store, p.node.key()).at("report").rel_path)));
  EXPECT_EQ(cal::at(rep, {"descriptor", "combine_method"}).as_string(), "mean");
  EXPECT_EQ(cal::at(rep, {"descriptor", "rejection"}).as_string(), "sigma_clip");
}

// ================================================================ T-M3B-FLAT-06 sul percorso del job

TEST(MasterB16, T_M3B_FLAT_06_job_raw_recipe_stop_reads_decodes_and_computes_nothing) {
  // Il nodo nasce da una ricetta ammessa; il job riceve un flat con `median_ratio` e `flat_level_rel_tol` ancora presente
  // (vietato con `median_ratio`). Il codice atteso è quello di `validate_master_recipe` sulla stessa ricetta.
  const fs::path dir = fresh_dir("b16-flat06-recipe");
  const raw_set s = make_raw_set(dir / "frames", master_role::flat);
  const master_recipe good = raw_recipe(s);
  master_recipe bad = good;
  bad.flat_norm = flat_normalization::median_ratio;
  std::optional<std::string> want;
  try {
    validate_master_recipe(bad);
  } catch (const stop_error& e) {
    want = e.code();
  }
  ASSERT_TRUE(want.has_value()) << "premessa: la ricetta del job viola un dominio di §f.2";
  const resource_config rc = raw_resources(dir / "scratch", 64 * kMiB);
  const int baseline = baseline_faults(dir / "store-baseline", good, rc);
  project p = make_project(dir / "store", good);
  const run_info run = run_job(p, bad, rc);
  ASSERT_TRUE(run.stop.has_value());
  EXPECT_EQ(run.stop->code(), *want) << run.stop->what();
  EXPECT_EQ(run.faults, baseline) << "nessun lavoro sullo store oltre lo stop di parametro di riferimento";
  EXPECT_EQ(run.opens, 0) << "nessun input RAW aperto";
  EXPECT_EQ(run.tasks, 0);
  EXPECT_FALSE(run.frames_read);
  EXPECT_FALSE(run.primary);
}

TEST(MasterB16, T_M3B_FLAT_06_job_raw_without_declared_scratch_stops_before_any_read_and_does_no_extra_store_work) {
  const fs::path dir = fresh_dir("b16-flat06-spill");
  const raw_set s = make_raw_set(dir / "frames", master_role::dark);
  const master_recipe good = raw_recipe(s);
  const int baseline = baseline_faults(dir / "store-baseline", good, raw_resources(dir / "scratch", 64 * kMiB));
  project p = make_project(dir / "store", good);
  const run_info run = run_job(p, good, raw_resources(dir / "none", std::nullopt));
  ASSERT_TRUE(run.stop.has_value()) << "senza spill dichiarato: stop atteso, nessun percorso residente";
  EXPECT_EQ(run.stop->category(), error_category::invalid_input) << run.stop->what();
  EXPECT_EQ(run.stop->code(), "master.raw_requires_spill") << run.stop->what();
  EXPECT_EQ(context_of(*run.stop, "frame"), std::optional<std::string>("frame_000")) << run.stop->what();
  EXPECT_EQ(context_of(*run.stop, "n"), std::optional<std::string>(std::to_string(kN))) << run.stop->what();
  EXPECT_EQ(run.faults, baseline);
  EXPECT_EQ(run.opens, 0);
  EXPECT_EQ(run.tasks, 0);
  EXPECT_FALSE(run.frames_read);
  EXPECT_FALSE(run.primary);
}

TEST(MasterB16, T_M3B_FLAT_06_job_with_raw_and_fits_frames_together_stops_with_decoder_state_mixed_before_any_read) {
  const fs::path dir = fresh_dir("b16-flat06-mixed");
  const raw_set s = make_raw_set(dir / "frames", master_role::dark);
  const master_recipe good = raw_recipe(s);
  master_recipe mixed = good;
  mixed.frames[3].decoder.reset();  // un frame FITS fra frame RAW
  const resource_config rc = raw_resources(dir / "scratch", 64 * kMiB);
  const int baseline = baseline_faults(dir / "store-baseline", good, rc);
  project p = make_project(dir / "store", good);
  const run_info run = run_job(p, mixed, rc);
  ASSERT_TRUE(run.stop.has_value());
  EXPECT_EQ(run.stop->category(), error_category::incompatible) << run.stop->what();
  EXPECT_EQ(run.stop->code(), "master.decoder_state_mixed") << run.stop->what();
  EXPECT_EQ(run.faults, baseline);
  EXPECT_EQ(run.opens, 0);
  EXPECT_FALSE(run.frames_read);
  EXPECT_FALSE(run.primary);
}

// ================================================================ T-M3B-C02-10 e T-M3B-FLAT-08: job RAW

TEST(MasterB16, T_M3B_C02_10_hand_computed_samples_agree_with_combine_master_on_the_decoded_frames) {
  // Precondizione dei valori attesi dei test che seguono: la formula del test e il riferimento in memoria coincidono, quindi
  // un divario nel job RAW è del job e non della fixture.
  const fs::path dir = fresh_dir("b16-oracle");
  const struct {
    master_role role;
    combine_method method;
  } cases[] = {{master_role::dark, combine_method::mean},
               {master_role::dark, combine_method::median},
               {master_role::flat, combine_method::mean}};
  for (const auto& c : cases) {
    SCOPED_TRACE(std::string(c.role == master_role::flat ? "flat " : "dark ") + (c.method == combine_method::mean ? "mean" : "median"));
    const raw_set s = make_raw_set(dir / (c.role == master_role::flat ? "flat" : "dark"), c.role);
    master_recipe r = raw_recipe(s);
    r.combine = c.method;
    master_result res;
    ASSERT_NO_THROW(res = combine_master(r, s.images));
    std::size_t wrong = 0;
    for (std::uint32_t y = 0; y < kH; ++y)
      for (std::uint32_t x = 0; x < kW; ++x)
        if (res.output.data<double>()[static_cast<std::size_t>(y) * kW + x] != expected_sample(c.role, c.method, y, x)) ++wrong;
    EXPECT_EQ(wrong, 0u);
  }
}

TEST(MasterB16, T_M3B_C02_10_job_raw_dark_mean_equals_the_hand_computed_master) {
  const fs::path dir = fresh_dir("b16-c10-mean");
  const raw_set s = make_raw_set(dir / "frames", master_role::dark);
  const master_recipe r = raw_recipe(s);
  project p = make_project(dir / "store", r);
  const run_info run = run_job(p, r, raw_resources(dir / "scratch", kSpillBytes));
  ASSERT_FALSE(run.stop.has_value()) << run.stop->what();
  const image m = primary_of(p);
  ASSERT_EQ(m.geometry.width, kW);
  ASSERT_EQ(m.geometry.height, kH);
  std::size_t wrong = 0;
  for (std::uint32_t y = 0; y < kH; ++y)
    for (std::uint32_t x = 0; x < kW; ++x)
      if (sample(m, static_cast<std::size_t>(y) * kW + x) != expected_sample(master_role::dark, combine_method::mean, y, x)) ++wrong;
  EXPECT_EQ(wrong, 0u) << "media dei campioni decodificati, nero non sottratto";
}

TEST(MasterB16, T_M3B_C02_10_job_raw_dark_median_equals_the_hand_computed_master) {
  const fs::path dir = fresh_dir("b16-c10-median");
  const raw_set s = make_raw_set(dir / "frames", master_role::dark);
  master_recipe r = raw_recipe(s);
  r.combine = combine_method::median;
  project p = make_project(dir / "store", r);
  const run_info run = run_job(p, r, raw_resources(dir / "scratch", kSpillBytes));
  ASSERT_FALSE(run.stop.has_value()) << run.stop->what();
  const image m = primary_of(p);
  std::size_t wrong = 0;
  for (std::uint32_t y = 0; y < kH; ++y)
    for (std::uint32_t x = 0; x < kW; ++x)
      if (sample(m, static_cast<std::size_t>(y) * kW + x) != expected_sample(master_role::dark, combine_method::median, y, x)) ++wrong;
  EXPECT_EQ(wrong, 0u);
}

TEST(MasterB16, T_M3B_FLAT_08_job_raw_flat_mean_equals_the_hand_computed_master) {
  const fs::path dir = fresh_dir("b16-flat08-mean");
  const raw_set s = make_raw_set(dir / "frames", master_role::flat);
  const master_recipe r = raw_recipe(s);
  project p = make_project(dir / "store", r);
  const run_info run = run_job(p, r, raw_resources(dir / "scratch", kSpillBytes));
  ASSERT_FALSE(run.stop.has_value()) << run.stop->what();
  const image m = primary_of(p);
  std::size_t wrong = 0;
  for (std::uint32_t y = 0; y < kH; ++y)
    for (std::uint32_t x = 0; x < kW; ++x)
      if (sample(m, static_cast<std::size_t>(y) * kW + x) != expected_sample(master_role::flat, combine_method::mean, y, x)) ++wrong;
  EXPECT_EQ(wrong, 0u);
}

struct block_config {
  unsigned workers;
  std::uint32_t tile_rows;
};
constexpr block_config kBlockConfigs[] = {{1, 1}, {1, 7}, {1, 48}, {4, 1}, {4, 7}, {4, 48}};

void expect_job_invariant_to_blocks_and_workers(master_role role, const char* name) {
  const fs::path dir = fresh_dir(std::string("b16-blocks-") + name);
  const raw_set s = make_raw_set(dir / "frames", role);
  const master_recipe r = raw_recipe(s);
  std::optional<std::string> first_sha;
  for (const block_config& c : kBlockConfigs) {
    SCOPED_TRACE("worker " + std::to_string(c.workers) + " tile_rows " + std::to_string(c.tile_rows));
    const std::string tag = std::to_string(c.workers) + "-" + std::to_string(c.tile_rows);
    project p = make_project(dir / ("store-" + tag), r);
    const run_info run = run_job(p, r, raw_resources(dir / ("scratch-" + tag), kSpillBytes, c.workers, c.tile_rows));
    ASSERT_FALSE(run.stop.has_value()) << run.stop->what();
    EXPECT_TRUE(equals_in_memory(primary_of(p), r, s));
    const std::string sha = by_role(*p.store, p.node.key()).at("primary").sha256;
    if (!first_sha) first_sha = sha;
    EXPECT_EQ(sha, *first_sha) << "digest dell'artefatto diverso fra le configurazioni";
  }
}

TEST(MasterB16, T_M3B_C02_10_job_raw_dark_is_bit_identical_to_combine_master_for_every_tile_rows_and_worker_count) {
  expect_job_invariant_to_blocks_and_workers(master_role::dark, "dark");
}

TEST(MasterB16, T_M3B_FLAT_08_job_raw_flat_is_bit_identical_to_combine_master_for_every_tile_rows_and_worker_count) {
  expect_job_invariant_to_blocks_and_workers(master_role::flat, "flat");
}

void expect_reopen_and_rebuild_give_the_same_key_and_digests(master_role role, const char* name) {
  const fs::path dir = fresh_dir(std::string("b16-reopen-") + name);
  const raw_set s = make_raw_set(dir / "frames", role);
  const master_recipe r = raw_recipe(s);
  std::string key, image_sha, report_sha;
  {
    project p = make_project(dir / "store", r);
    const run_info run = run_job(p, r, raw_resources(dir / "scratch-a", kSpillBytes));
    ASSERT_FALSE(run.stop.has_value()) << run.stop->what();
    key = p.node.key();
    const auto roles = by_role(*p.store, key);
    image_sha = roles.at("primary").sha256;
    report_sha = roles.at("report").sha256;
  }
  auto reopened = sqlite_dag_store::open(dir / "store", open_intent::read_write);
  const std::optional<node_spec> stored = reopened->get_node(key);
  ASSERT_TRUE(stored.has_value());
  EXPECT_EQ(stored->key(), make_master_node(r).key()) << "il nodo riaperto non è quello della ricetta";
  EXPECT_EQ(node_from_json(stored->to_json()).key(), key);
  const slot_view view = reopened->read_slot("master");
  ASSERT_EQ(view.state, freshness::current);
  EXPECT_EQ(view.artifact->sha256, image_sha);

  // Ricostruzione in un progetto nuovo con i frame in ordine inverso (candidati permutati): stessa chiave, stessi digest.
  master_recipe reversed = r;
  std::reverse(reversed.frames.begin(), reversed.frames.end());
  project q = make_project(dir / "rebuild", reversed);
  EXPECT_EQ(q.node.key(), key);
  const run_info run = run_job(q, reversed, raw_resources(dir / "scratch-b", kSpillBytes));
  ASSERT_FALSE(run.stop.has_value()) << run.stop->what();
  const auto roles = by_role(*q.store, key);
  EXPECT_EQ(roles.at("primary").sha256, image_sha);
  EXPECT_EQ(roles.at("report").sha256, report_sha);
}

TEST(MasterB16, T_M3B_C02_10_job_raw_dark_reopened_project_and_rebuilt_node_give_the_same_key_and_digests) {
  expect_reopen_and_rebuild_give_the_same_key_and_digests(master_role::dark, "dark");
}

TEST(MasterB16, T_M3B_FLAT_08_job_raw_flat_reopened_project_and_rebuilt_node_give_the_same_key_and_digests) {
  expect_reopen_and_rebuild_give_the_same_key_and_digests(master_role::flat, "flat");
}

TEST(MasterB16, T_M3B_C02_10_job_raw_with_the_spill_quota_one_byte_short_stops_with_quota_exceeded_and_no_master) {
  // L10: quota necessaria n·S·6 byte (f32 più maschera uint16 per pixel), controllata prima della prima decodifica.
  const fs::path dir = fresh_dir("b16-quota-short");
  const raw_set s = make_raw_set(dir / "frames", master_role::dark);
  const master_recipe r = raw_recipe(s);
  project p = make_project(dir / "store", r);
  const run_info run = run_job(p, r, raw_resources(dir / "scratch", kSpillBytes - 1));
  ASSERT_TRUE(run.stop.has_value()) << "quota dello spill insufficiente: stop atteso";
  EXPECT_EQ(run.stop->category(), error_category::budget) << run.stop->what();
  EXPECT_EQ(run.stop->code(), "spill.quota_exceeded") << run.stop->what();
  EXPECT_FALSE(run.primary);
  EXPECT_EQ(run.tasks, 0);
}

TEST(MasterB16, T_M3B_C02_10_job_raw_with_the_spill_quota_exactly_n_s_6_completes) {
  const fs::path dir = fresh_dir("b16-quota-exact");
  const raw_set s = make_raw_set(dir / "frames", master_role::dark);
  const master_recipe r = raw_recipe(s);
  project p = make_project(dir / "store", r);
  const run_info run = run_job(p, r, raw_resources(dir / "scratch", kSpillBytes));
  ASSERT_FALSE(run.stop.has_value()) << run.stop->what();
  EXPECT_TRUE(run.primary);
}

// ---------------------------------------------------------------- candidati permutati con parametro `analysis` (L8)

std::string declarations() {
  json doc = json::object();
  doc.set("format", json::string("astro.input_declarations.v1"));
  json fields = json::object();
  fields.set("binning_x", json::uinteger(1));
  fields.set("binning_y", json::uinteger(1));
  doc.set("declared_image_fields", std::move(fields));
  return canonical_json(doc);
}

// Proposta per tre dark 8 x 4 scritti su disco, con i candidati nell'ordine `order` e gli `input_id` `ids`.
master_proposal propose_for(const group& g, const std::vector<image>& images, const std::vector<std::size_t>& order,
                            const std::vector<std::uint64_t>& ids) {
  master_proposal_request rq;
  rq.role = master_role::dark;
  for (std::size_t i = 0; i < order.size(); ++i) {
    const std::size_t f = order[i];
    master_proposal_frame pf;
    pf.analysis.input_id = ids[i];
    pf.analysis.digest = digest_of(file_of(g.frames[f], g.w, g.h));
    pf.analysis.observation = make_observation(images[f], declarations());
    pf.spec.path = "da-assegnare";
    pf.spec.digest = pf.analysis.digest;
    rq.frames.push_back(pf);
  }
  rq.params.exposure_rel_tol = 1e-9;
  rq.params.temperature_abs_tol_c = 0.5;
  rq.params.temp_policy = temperature_policy::compare;
  rq.simd = simd_test::requested();
  return propose_masters(rq);
}

TEST(MasterB16, T_M3B_C02_10_job_with_the_proposed_node_gives_the_same_artifact_digests_for_permuted_candidates) {
  const fs::path dir = fresh_dir("b16-permuted");
  group g = make_group(master_role::dark, 8, 4, 3);
  for (std::size_t f = 0; f < 3; ++f)
    for (std::size_t i = 0; i < g.frames[f].v.size(); ++i) g.frames[f].v[i] = 1000.0 + static_cast<double>(f + i % 3);
  std::vector<std::string> paths;
  std::vector<image> images;
  for (std::size_t f = 0; f < 3; ++f) {
    paths.push_back((dir / ("f" + std::to_string(f) + ".fits")).string());
    cal::write_bytes(paths.back(), file_of(g.frames[f], g.w, g.h));
    images.push_back(decode(g.frames[f], g.w, g.h));
  }
  const master_proposal a = propose_for(g, images, {0, 1, 2}, {10, 11, 12});
  const master_proposal b = propose_for(g, images, {2, 0, 1}, {77, 5, 31});
  ASSERT_EQ(a.groups.size(), 1u);
  ASSERT_EQ(b.groups.size(), 1u);
  ASSERT_TRUE(a.groups[0].recipe.has_value() && b.groups[0].recipe.has_value()) << "premessa: proposta `median` per n = 3";
  master_recipe ra = *a.groups[0].recipe, rb = *b.groups[0].recipe;
  ASSERT_NE(ra.analysis.type(), json::kind::null) << "il nodo proposto porta il parametro `analysis` (L8)";
  const auto assign_paths = [&](master_recipe& r) {
    for (master_frame_spec& s : r.frames) {
      for (std::size_t f = 0; f < 3; ++f)
        if (digest_of(file_of(g.frames[f], g.w, g.h)).sha256 == s.digest.sha256) s.path = paths[f];
    }
  };
  assign_paths(ra);
  assign_paths(rb);
  EXPECT_EQ(canonical_json(ra.analysis), canonical_json(rb.analysis));
  project pa = make_project(dir / "store-a", ra);
  project pb = make_project(dir / "store-b", rb);
  ASSERT_EQ(pa.node.key(), pb.node.key());
  ASSERT_FALSE(run_job(pa, ra, resource_config{}).stop.has_value());
  ASSERT_FALSE(run_job(pb, rb, resource_config{}).stop.has_value());
  const auto roles_a = by_role(*pa.store, pa.node.key());
  const auto roles_b = by_role(*pb.store, pb.node.key());
  EXPECT_EQ(roles_a.at("primary").sha256, roles_b.at("primary").sha256);
  EXPECT_EQ(roles_a.at("report").sha256, roles_b.at("report").sha256);
}

// ---------------------------------------------------------------- controllo termico di `flat.bias` dopo il master (CL-06 (c))

constexpr std::uint32_t kTw = 8, kTh = 4;
constexpr std::size_t kBiasFrames = 4;

image fits_image(const char* imagetyp, double exposure, double temperature, const char* filter, double value,
                 content_digest* digest) {
  fx::calib_header h;
  h.imagetyp = imagetyp;
  h.exposure = exposure;
  h.temperature = temperature;
  h.filter = filter;
  h.bayer = "RGGB";
  const fx::bytes bytes = fx::float_frame(kTw, kTh, -64, std::vector<double>(kTw * kTh, value), fx::calib_cards(h));
  if (digest) *digest = digest_of(bytes);
  return decode_fits(bytes, cal::big_budget);
}

// Light, flat esterno (descrittore `raw`) e quattro frame bias senza descrittore, scritti su disco: gli ingressi di
// `analyze_batch` prima del master.
struct thermal_flow {
  image light, flat;
  content_digest light_digest, flat_digest;
  group bias;
  std::vector<image> bias_images;
  master_recipe bias_recipe;  // frame e percorsi; la ricetta proposta li prende dal digest
};

thermal_flow make_thermal_flow(const fs::path& dir) {
  thermal_flow t;
  t.light = fits_image("Light Frame", 60, -10, "L", 500, &t.light_digest);
  t.flat = fits_image("Flat Field", 5, -6, "L", 1124, &t.flat_digest);
  t.bias = make_group(master_role::bias, kTw, kTh, kBiasFrames);
  for (mframe& f : t.bias.frames) {
    std::fill(f.v.begin(), f.v.end(), 100.0);
    f.hdr.exposure = 0.001;
    f.hdr.temperature = -8;
    f.hdr.filter = "L";
    f.hdr.bayer = "RGGB";
  }
  t.bias_recipe = recipe_of(t.bias);
  write_frames(t.bias, t.bias_recipe, dir, "bias");
  t.bias_images = images_of(t.bias);
  return t;
}

batch_input thermal_input(std::uint64_t id, const content_digest& digest, const char* role, const image& img,
                          std::optional<master_descriptor> descriptor = std::nullopt) {
  batch_input in;
  in.input_id = id;
  in.digest = digest;
  in.role = role;
  in.declarations_canonical = declarations();
  in.descriptor = std::move(descriptor);
  in.observation = make_observation(img, in.declarations_canonical);
  return in;
}

// Proposta del master bias dai quattro frame, nodo con i percorsi dei file: il candidato (c) di CL-06.
struct proposed_bias {
  master_proposal proposal;
  master_recipe recipe;
};

proposed_bias propose_bias(const thermal_flow& t) {
  master_proposal_request rq;
  rq.role = master_role::bias;
  for (std::size_t k = 0; k < kBiasFrames; ++k) {
    master_proposal_frame pf;
    pf.analysis.input_id = 10 + k;
    pf.analysis.digest = t.bias_recipe.frames[k].digest;
    pf.analysis.observation = make_observation(t.bias_images[k], declarations());
    pf.spec.path = t.bias_recipe.frames[k].path;
    pf.spec.digest = pf.analysis.digest;
    rq.frames.push_back(std::move(pf));
  }
  rq.params.exposure_rel_tol = 1e-9;
  rq.params.temperature_abs_tol_c = 0.5;
  rq.params.temp_policy = temperature_policy::compare;
  rq.simd = simd_test::requested();
  proposed_bias out;
  out.proposal = propose_masters(rq);
  EXPECT_EQ(out.proposal.groups.size(), 1u);
  if (out.proposal.groups.size() != 1 || !out.proposal.groups[0].recipe) {
    ADD_FAILURE() << "premessa: proposta del master bias per n = 4";
    return out;
  }
  out.recipe = *out.proposal.groups[0].recipe;
  for (master_frame_spec& f : out.recipe.frames)
    for (const master_frame_spec& orig : t.bias_recipe.frames)
      if (f.digest.sha256 == orig.digest.sha256) f.path = orig.path;
  return out;
}

group_proposal analyze_thermal_group(const thermal_flow& t, const std::vector<slot_candidate>& slots, double bound) {
  std::vector<batch_input> in;
  in.push_back(thermal_input(1, t.light_digest, "light", t.light));
  master_descriptor flat_descriptor;
  flat_descriptor.role = master_role::flat;
  flat_descriptor.flat = flat_state::raw;
  flat_descriptor.combine_count = 4;
  flat_descriptor.combine = combine_method::mean;
  batch_input flat = thermal_input(2, t.flat_digest, "flat", t.flat, flat_descriptor);
  flat.load_samples = [img = t.flat] { return img; };
  in.push_back(std::move(flat));
  for (std::size_t k = 0; k < kBiasFrames; ++k) {
    in.push_back(thermal_input(10 + k, t.bias_recipe.frames[k].digest, "bias", t.bias_images[k]));
  }
  batch_analysis_request req;
  calibration_declarations& d = req.params;
  d.exposure_rel_tol = 0.0;
  d.temperature_abs_tol_c = 3.0;
  d.temp_policy = temperature_policy::compare;
  d.normalization = normalization_mode::global_median;
  d.q_min = 0.5;
  d.q_rel_sigma_max = 10.0;
  d.flat_thermal_bound_adu = bound;
  d.flat_thermal_rel_budget = 0.25;
  const batch_proposal proposal = analyze_batch(in, slots, req);
  EXPECT_EQ(proposal.groups.size(), 1u);
  if (proposal.groups.empty()) throw std::runtime_error("nessun gruppo");
  return proposal.groups[0];
}

// Slot `current` del master eseguito, come lo costruisce `analyze_batch` sullo store: campioni letti dall'artefatto.
slot_candidate executed_slot_candidate(const project& p) {
  const image m = primary_of(p);
  slot_candidate c;
  c.node_key = p.node.key();
  c.role = master_role::bias;
  c.descriptor = master_descriptor_from_node(p.node);
  c.observation = make_observation(m, declarations());
  c.load_samples = [m] { return m; };
  return c;
}

const chain_recipe* flat_bias_recipe(const group_proposal& g) {
  for (const chain_recipe& r : g.recipes)
    if (r.flat == flat_chain::bias) return &r;
  return nullptr;
}

// Fixture: F = 1124, B = 100, Fc = 1024 = N, q_min · N = 512, budget 0,25 (soglia 128): bound 64 nel budget (β = 1/8),
// bound 256 fuori.
TEST(MasterB16, T_M3B_B16_thermal_check_with_the_proposed_bias_is_declared_pending_and_the_selection_cannot_run) {
  const thermal_flow t = make_thermal_flow(fresh_dir("b16-thermal-pending"));
  const proposed_bias pb = propose_bias(t);
  const group_proposal g = analyze_thermal_group(t, proposed_slot_candidates(pb.proposal), 64.0);
  ASSERT_EQ(g.outcome, group_outcome::ready);
  ASSERT_TRUE(g.analysis.has_value());
  EXPECT_TRUE(g.analysis->flat_thermal_pending_master);
  EXPECT_FALSE(g.analysis->flat_thermal_within_budget);
  const chain_recipe* recipe = flat_bias_recipe(g);
  ASSERT_NE(recipe, nullptr) << "la catena flat.bias resta ammissibile finché il master non è eseguito";
  const calibration_selection sel = select_recipe(g, *recipe);
  const std::vector<observed_proposed_node> nodes = {
      observed_proposed_node{pb.proposal.groups[0].node_key, pb.proposal.groups[0].frames}};
  try {
    verify_selection(sel, {}, nodes);
    ADD_FAILURE() << "selezione fatta con il controllo termico in attesa: stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::integrity) << e.what();
    EXPECT_EQ(e.code(), "calib.thermal_check_pending") << e.what();
    EXPECT_EQ(context_of(e, "node_key"), std::optional<std::string>(pb.proposal.groups[0].node_key));
    EXPECT_EQ(context_of(e, "role"), std::optional<std::string>("bias"));
  }
}

TEST(MasterB16, T_M3B_B16_thermal_check_after_the_bias_job_runs_is_computed_from_the_current_slot_and_within_budget) {
  const fs::path dir = fresh_dir("b16-thermal-within");
  const thermal_flow t = make_thermal_flow(dir);
  const proposed_bias pb = propose_bias(t);
  ASSERT_TRUE(analyze_thermal_group(t, proposed_slot_candidates(pb.proposal), 64.0).analysis->flat_thermal_pending_master);
  project p = make_project(dir / "store", pb.recipe);
  ASSERT_EQ(p.node.key(), pb.proposal.groups[0].node_key) << "il nodo eseguito è quello proposto";
  const run_info run = run_job(p, pb.recipe, resource_config{});
  ASSERT_FALSE(run.stop.has_value()) << run.stop->what();
  const group_proposal g = analyze_thermal_group(t, {executed_slot_candidate(p)}, 64.0);
  ASSERT_EQ(g.outcome, group_outcome::ready);
  ASSERT_TRUE(g.analysis.has_value());
  EXPECT_FALSE(g.analysis->flat_thermal_pending_master) << "il flag sparisce con il master eseguito";
  EXPECT_TRUE(g.analysis->flat_thermal_within_budget);
  const chain_recipe* recipe = flat_bias_recipe(g);
  ASSERT_NE(recipe, nullptr);
  const calibration_selection sel = select_recipe(g, *recipe);
  for (const selected_candidate& c : sel.candidates) EXPECT_FALSE(c.proposed_frames.has_value());
  EXPECT_NO_THROW(verify_selection(sel, {}));
}

TEST(MasterB16, T_M3B_B16_thermal_check_after_the_bias_job_runs_out_of_budget_excludes_the_flat_bias_chain) {
  const fs::path dir = fresh_dir("b16-thermal-out");
  const thermal_flow t = make_thermal_flow(dir);
  const proposed_bias pb = propose_bias(t);
  ASSERT_NE(flat_bias_recipe(analyze_thermal_group(t, proposed_slot_candidates(pb.proposal), 256.0)), nullptr)
      << "in attesa la catena non è esclusa, anche con un bound fuori budget";
  project p = make_project(dir / "store", pb.recipe);
  const run_info run = run_job(p, pb.recipe, resource_config{});
  ASSERT_FALSE(run.stop.has_value()) << run.stop->what();
  const group_proposal g = analyze_thermal_group(t, {executed_slot_candidate(p)}, 256.0);
  ASSERT_TRUE(g.analysis.has_value());
  EXPECT_FALSE(g.analysis->flat_thermal_pending_master);
  EXPECT_FALSE(g.analysis->flat_thermal_within_budget);
  EXPECT_EQ(flat_bias_recipe(g), nullptr);
}

// ---------------------------------------------------------------- aggancio del lettore RAW all'analisi (decisione B16)

// Statistiche RAW (sovraccarico di B15) -> proposta (screening e gruppi inclusi) -> nodo -> job -> master uguale a
// `combine_master`.
void raw_analysis_pipeline(memory_pool& pool, const cancel_token& cancel, const fs::path& dir) {
  const raw_set s = make_raw_set(dir / "frames", master_role::dark);
  master_proposal_request rq;
  rq.role = master_role::dark;
  rq.params.exposure_rel_tol = 1e-9;
  rq.params.temperature_abs_tol_c = 0.5;
  rq.params.temp_policy = temperature_policy::not_applicable_declared;
  rq.params.decoder_black_abs_tol_adu = 0;
  rq.params.saturation_adu = kWhite;
  rq.params.saturated_fraction_max = 0.5;
  rq.simd = simd_test::requested();
  master_stats_request req;
  req.saturation_adu = kWhite;
  for (std::size_t k = 0; k < kN; ++k) {
    master_proposal_frame pf;
    pf.analysis.input_id = 100 + k;
    pf.analysis.digest = digest_of(s.bytes[k]);
    pf.analysis.observation = make_observation(s.images[k], declarations(), current_decoder_identity());
    pf.analysis.stats = read_master_frame_stats(s.paths[k], pf.analysis.digest, raw_input_format(), req, pool, cancel);
    ASSERT_FALSE(pf.analysis.stats->decode_failure.has_value());
    EXPECT_EQ(pf.analysis.stats->pixel_count, std::uint64_t{kW} * kH);
    EXPECT_EQ(pf.analysis.stats->saturated_samples, 0u);
    pf.spec.path = s.paths[k];
    pf.spec.digest = pf.analysis.digest;
    pf.spec.declared_fields.binning_x = 1;
    pf.spec.declared_fields.binning_y = 1;
    rq.frames.push_back(std::move(pf));
  }
  std::vector<master_analysis_frame> analysis_frames;
  for (const master_proposal_frame& pf : rq.frames) analysis_frames.push_back(pf.analysis);
  const master_screening screening = screen_master_frames(master_role::dark, analysis_frames, rq.params);
  ASSERT_TRUE(screening.excluded.empty());
  ASSERT_EQ(screening.groups.size(), 1u);
  EXPECT_EQ(screening.groups[0].members.size(), kN);
  const master_proposal proposal = propose_masters(rq);
  ASSERT_EQ(proposal.groups.size(), 1u);
  ASSERT_TRUE(proposal.groups[0].recipe.has_value()) << "premessa: proposta per n = 4";
  master_recipe r = *proposal.groups[0].recipe;
  ASSERT_EQ(r.frames.size(), kN);
  for (master_frame_spec& f : r.frames) {
    EXPECT_TRUE(f.decoder.has_value()) << "stato del decoder dall'osservazione RAW";
    for (std::size_t k = 0; k < kN; ++k)
      if (f.digest.sha256 == digest_of(s.bytes[k]).sha256) f.path = s.paths[k];
  }
  std::vector<image> ims;
  for (const master_frame_spec& f : r.frames)
    for (std::size_t k = 0; k < kN; ++k)
      if (f.digest.sha256 == digest_of(s.bytes[k]).sha256) ims.push_back(s.images[k]);
  ASSERT_EQ(ims.size(), kN);
  project p = make_project(dir / "store", r);
  const run_info run = run_job(p, r, raw_resources(dir / "scratch", kSpillBytes));
  ASSERT_FALSE(run.stop.has_value()) << run.stop->what();
  const master_result ref = combine_master(r, std::move(ims));
  EXPECT_TRUE(same_bits(primary_of(p), ref.output)) << "il master del job differisce da combine_master";
}

// Lettore RAW di B15 con i fixture esistenti (decisione B16, «Lettore RAW»): esiti di file diversi da `raw.max_pixels`.
master_frame_stats read_raw_bytes(const std::string& name, const std::vector<std::uint8_t>& bytes) {
  const fs::path dir = fresh_dir("b16-reader-" + name);
  const std::string path = (dir / "frame.dng").string();
  write_file(path, bytes);
  memory_pool pool(256 * kMiB);
  const cancel_token cancel;
  return read_master_frame_stats(path, digest_of(bytes), raw_input_format(), master_stats_request{}, pool, cancel);
}

TEST(MasterB16, T_M3B_C02_08_L2_raw_reader_truncated_dng_with_its_own_digest_is_a_data_error_outcome) {
  std::vector<std::uint8_t> bytes = dng_of(dark_site(0));
  bytes.resize(bytes.size() / 2);
  master_frame_stats st;
  ASSERT_NO_THROW(st = read_raw_bytes("truncated", bytes));
  ASSERT_TRUE(st.decode_failure.has_value());
  EXPECT_EQ(st.decode_failure->code, "raw.data_error");
  EXPECT_FALSE(st.decode_failure->detail.empty());
}

TEST(MasterB16, T_M3B_C02_08_L2_raw_reader_bytes_that_are_not_a_raw_file_are_a_file_unsupported_outcome) {
  master_frame_stats st;
  ASSERT_NO_THROW(st = read_raw_bytes("garbage", std::vector<std::uint8_t>(5000, 0x5a)));
  ASSERT_TRUE(st.decode_failure.has_value());
  EXPECT_EQ(st.decode_failure->code, "raw.file_unsupported");
}

TEST(MasterB16, T_M3B_C02_08_L2_raw_reader_dng_with_an_unsupported_compression_is_a_file_unsupported_outcome) {
  fx::dng_spec s;
  s.width = kW;
  s.height = kH;
  s.white = static_cast<std::uint32_t>(kWhite);
  s.compression = 9;
  s.pixels.assign(static_cast<std::size_t>(kW) * kH * 2, 0);
  master_frame_stats st;
  ASSERT_NO_THROW(st = read_raw_bytes("vc5", fx::make_dng(s)));
  ASSERT_TRUE(st.decode_failure.has_value());
  EXPECT_EQ(st.decode_failure->code, "raw.file_unsupported");
}

TEST(MasterB16, T_M3B_ANA_01_raw_group_statistics_to_proposal_to_node_to_job_equals_combine_master) {
  memory_pool pool(256 * kMiB);
  const cancel_token cancel;
  raw_analysis_pipeline(pool, cancel, fresh_dir("b16-raw-pipeline"));
}

}  // namespace
