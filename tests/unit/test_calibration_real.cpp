// T-M2E-13 e T-M2R2-09: dato reale pubblico PUB-LFC-01 (Palomar LFC, CC BY 4.0, docs/datasets/PUB-LFC-01.md).
// Lungo: label `slow`. I file non sono versionati: se mancano o hanno un SHA-256 diverso da quello dichiarato il test
// FALLISCE (nessuno skip). Il lettore dei file grezzi e l'oracolo sono del test (fixture independent writer/parser).
//
// Verita' indipendente: maschera attesa, N e campioni su ogni pixel sono ricalcolati dal test con la stessa formula in
// aritmetica double sui campioni memorizzati dei master. Tolleranza sui campioni validi (uscita f32):
// mezzo ulp f32 + 2 gamma_6 |C| (il prodotto e il test differiscono ciascuno di al piu' gamma_6 |C| dal valore esatto).
// Le proprieta' fisiche (appiattimento del fondo, scarto overscan) si misurano e si registrano, senza soglie tarate
// sull'uscita: l'asserzione e' solo la diminuzione della disuniformita' a grande scala dopo il flat.
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>

#include "astro/hash.hpp"
#include "astro/master.hpp"
#include "astro/scheduler.hpp"
#include "calib_support.hpp"

namespace {

using namespace cal;

constexpr const char* kRoot = ASTRO_SOURCE_DIR "/data/public/zenodo-3254683/raw/example-cryo-LFC/";

// SHA-256 dichiarati in docs/datasets/PUB-LFC-01.md.
const std::map<std::string, std::string> kSha = {
    {"ccd.001.0.fits", "be73dedce9727aa708a5f9427328b6711d697e9d7935053bbd6a1bbd093bf8b9"},
    {"ccd.002.0.fits", "f00343b4bf78bbf84361720e0ffeeda7d936cc025408949e043dc601b60de0e1"},
    {"ccd.014.0.fits", "b88697d8343b0910a23e108a8fcf7cbf63ad3872fcc0edee4fe12447ad4ddc07"},
    {"ccd.015.0.fits", "74b6b0cb4d39e7ec6722bb78baa988590eae5876adb711a10f6f44fe5b7d73c8"},
    {"ccd.016.0.fits", "67ed4f32553413c6835a07ee2bf40cd1ee6cf169285f0b4ac2373c41879ee20b"},
    {"ccd.017.0.fits", "8f730c5ac91708a9fd5ba23ea7e7fb55aef769d54057f884b8fe826cdc798aae"},
    {"ccd.018.0.fits", "c308de11075112f3fbc705a00758bff74ae2a6e055e07637219f35aa5e7eadc6"},
    {"ccd.019.0.fits", "db6662c74f97490dcbb552145f759372272497f76ea99e88f0860032484c17e8"},
    {"ccd.037.0.fits", "5d0e92f0c6a5749e2a264129d1821d29f8941b09adb36ce11a8e7cf5067296c3"},
    {"ccd.043.0.fits", "d0322c84eeecb7f35485d0ef36b0cb87f2c2db5423bcec67f44f6d43048a41ab"},
    {"darks/ccd.013.0.fits", "f62a979edeab66206be26247951e6711f3e65fd73469a72a858025375255e198"},
    {"darks/ccd.014.0.fits", "0817828607ce8bd435ce3a1e66b4f69cb0394c3d3afb7a1bf341188631cb4782"},
    {"darks/ccd.015.0.fits", "e8cf7d754819d91ddb7f66856ad4faa09498d27d50ba1ceab0fc292e328a70d4"},
    {"darks/ccd.017.0.fits", "6a50cd57544b9fd2f5f3d5163724789af306874afe85a9e10edaf75bdebb7f76"},
    {"darks/ccd.018.0.fits", "fc9f1bf817a62d89446743880165340c8cd1624b83c5c7540bb40b5d2d56d364"},
    {"darks/ccd.019.0.fits", "8f25b570f4f59fdd48f5a3bdf57dd7904801093e5beb77c8f30924858facd42b"},
    {"darks/ccd.023.0.fits", "cc6dfea9a17641065910bf6b4288e2d31fc3dc36e57b71c37c65d659ade45bb2"},
    {"darks/ccd.024.0.fits", "7ce339d1aa9fce9409bb9fb2b2e8277946dcb7a4d72e7fdbc4ae93ff5e0def46"},
    {"darks/ccd.025.0.fits", "a253adca4a93ef09dd6b92a970f6881bca847f44b97ad21415cd1a39d64a84c7"},
};

struct raw_frame {
  std::vector<double> v;
  std::uint32_t w = 0, h = 0;
  double exposure = 0;
  fx::bytes file;
};

raw_frame load_raw(const std::string& rel) {
  const std::string path = std::string(kRoot) + rel;
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("dato reale mancante: " + path + " (data/public non e' versionato: vedi docs/datasets/PUB-LFC-01.md)");
  raw_frame f;
  f.file.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  if (sha256_hex(f.file) != kSha.at(rel)) throw std::runtime_error("SHA-256 diverso da quello dichiarato: " + path);
  const fx::raw_primary_view hdu = fx::parse_hdu(f.file);
  if (hdu.bitpix != 16 || hdu.axes.size() != 2) throw std::runtime_error("formato inatteso: " + path);
  f.w = static_cast<std::uint32_t>(hdu.axes[0]);
  f.h = static_cast<std::uint32_t>(hdu.axes[1]);
  for (const std::string& card : hdu.cards) {
    if (card.compare(0, 8, "EXPTIME ") == 0) f.exposure = std::stod(card.substr(10, 20));
  }
  f.v.resize(static_cast<std::size_t>(f.w) * f.h);
  for (std::size_t i = 0; i < f.v.size(); ++i) {
    const auto raw = static_cast<std::int16_t>((hdu.data[2 * i] << 8) | hdu.data[2 * i + 1]);
    f.v[i] = static_cast<double>(raw) + 32768.0;
  }
  return f;
}

struct master {
  frame_def def;
  std::uint32_t w = 0, h = 0;
};

master mean_of(const std::vector<std::string>& rels, const char* imagetyp, const char* filter, double offset) {
  std::vector<raw_frame> frames;
  for (const std::string& r : rels) frames.push_back(load_raw(r));
  master m;
  m.w = frames[0].w;
  m.h = frames[0].h;
  m.def.bitpix = -32;
  m.def.hdr = header_for(imagetyp, 0, 1.1);
  m.def.hdr.gain = 1.1;
  m.def.hdr.offset = offset;
  m.def.hdr.filter = filter;
  m.def.hdr.camera = "LFC";
  double exposure = 0;
  for (const raw_frame& f : frames) exposure += f.exposure / static_cast<double>(frames.size());
  m.def.hdr.exposure = exposure;
  m.def.v.resize(frames[0].v.size());
  for (std::size_t i = 0; i < m.def.v.size(); ++i) {
    double sum = 0;
    for (const raw_frame& f : frames) sum += f.v[i];
    m.def.v[i] = static_cast<double>(static_cast<float>(sum / static_cast<double>(frames.size())));
  }
  return m;
}

double median_copy(std::vector<double> v) {
  const std::size_t mid = v.size() / 2;
  std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid), v.end());
  const double hi = v[mid];
  if (v.size() % 2 == 1) return hi;
  return (*std::max_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid)) + hi) * 0.5;
}

struct band {
  const char* name;
  const char* light;
  std::vector<std::string> flat, dark_flat;
  const char* filter;
};

// Disuniformita' a grande scala: coefficiente di variazione delle mediane di blocchi 64x64 della regione dati.
double block_cv(const std::vector<double>& img, std::uint32_t w, const std::vector<std::uint8_t>& valid) {
  std::vector<double> meds;
  for (std::uint32_t by = 0; by + 64 <= 4128; by += 64) {
    for (std::uint32_t bx = 0; bx + 64 <= 2048; bx += 64) {
      std::vector<double> vals;
      for (std::uint32_t y = by; y < by + 64; ++y) {
        for (std::uint32_t x = bx; x < bx + 64; ++x) {
          const std::size_t i = static_cast<std::size_t>(y) * w + x;
          if (valid[i]) vals.push_back(img[i]);
        }
      }
      if (vals.size() > 2048) meds.push_back(median_copy(vals));
    }
  }
  double mean = 0;
  for (double m : meds) mean += m / static_cast<double>(meds.size());
  double m2 = 0;
  for (double m : meds) m2 += (m - mean) * (m - mean);
  return std::sqrt(m2 / static_cast<double>(meds.size() - 1)) / std::fabs(mean);
}

// Master usato dalla calibrazione: campioni, piano VARIANCE (vuoto con noise_model) e bit come li legge la calibrazione.
struct band_master {
  std::vector<double> v, var;
  std::vector<std::uint16_t> bits;
  calibration_input_spec in;
};

band_master test_master(const master& m, const fs::path& path, master_role role, double read_noise) {
  band_master out;
  out.v = m.def.v;
  out.bits.assign(m.def.v.size(), 0);
  write_bytes(path, file_of(m.def, m.w, m.h));
  out.in.path = path.string();
  const std::string text = read_text(out.in.path);
  out.in.digest = digest_of(std::vector<std::uint8_t>(text.begin(), text.end()));
  out.in.descriptor = descriptor(role, 3);
  out.in.variance = noise(read_noise);
  return out;
}

// T-M2R2-09: master del prodotto dai frame grezzi, uscita f64. Verita' indipendente in long double dai campioni interi:
// somma esatta S (interi < 2^53), media S/n e piano s^2/n = (n sum x^2 - S^2)/(n^2 (n-1)), numeratore intero esatto; una
// sola divisione long double (errore relativo <= 2^-64). Tolleranze di C-02.a §6.3 con uscita f64 (nessun mezzo ulp),
// piu' l'errore dell'oracle 2^-63 |valore|.
band_master product_master(sqlite_dag_store& store, const std::string& slot, master_role role,
                           const std::vector<std::string>& rels, const char* filter, double offset, memory_pool& pool,
                           std::ostream& log) {
  const auto decl = [](double v) { return metadata_value<double>{v, {metadata_origin::declared, {}}}; };
  master_recipe r;
  r.role = role;
  std::vector<raw_frame> raw;
  for (const std::string& rel : rels) {
    raw.push_back(load_raw(rel));
    master_frame_spec f;
    f.path = std::string(kRoot) + rel;
    f.digest = digest_of(raw.back().file);
    f.declared.offset_adu = decl(offset);
    f.declared_fields.binning_x = 1;
    f.declared_fields.binning_y = 1;
    f.declared_fields.unit = pixel_unit::adu;
    f.declared_fields.lin = linearity::linear;
    f.read_options.declared_phase = process_phase::raw;
    r.frames.push_back(f);
  }
  r.combine = combine_method::mean;
  r.rejection = master_rejection::none;
  if (role == master_role::flat) {
    r.flat_norm = flat_normalization::none;
    r.flat_level_rel_tol = 0.2;
    r.flat_min_signal_adu = 1000;
  }
  r.saturation_adu = 65535;
  r.saturated_fraction_max = 0.05;
  r.exposure_rel_tol = 1e-3;
  r.temperature_abs_tol_c = 1.0;
  r.temp_policy = temperature_policy::not_applicable_declared;
  r.simd = simd_test::requested();
  r.output_type = sample_type::f64;
  const node_spec node = make_master_node(r);
  store.put_node(node);
  store.create_slot(slot, node.key());
  master_job_options o;
  o.worker_count = 4;
  o.tile_rows = 128;
  cancel_token cancel;
  const job_outcome out = run_master_job(store, slot, r, pool, o, cancel);
  if (out.exit_code != 0) throw std::runtime_error("master " + slot + ": " + (out.stop ? out.stop->what() : "uscita non nulla"));
  const slot_view view = store.read_slot(slot);
  const std::string text = read_text(store.artifact_path(view.artifact->rel_path));
  const image m = decode_fits(std::vector<std::uint8_t>(text.begin(), text.end()), big_budget);
  if (m.type != sample_type::f64 || !m.variance) throw std::runtime_error("master " + slot + ": tipo o piano inatteso");
  if (m.metadata.filter->value != filter) throw std::runtime_error("master " + slot + ": filtro inatteso");

  const std::size_t px = raw[0].v.size();
  const long double nn = static_cast<long double>(rels.size());
  const rat g_mean = exact::gamma(static_cast<unsigned>(rels.size()));
  const rat g_plane = exact::gamma(static_cast<unsigned>(rels.size() + 4));
  const long double gm = exact::to_ld(g_mean), gp = exact::to_ld(g_plane), oracle_rel = std::ldexp(1.0L, -63);
  std::uint64_t wrong = 0, saturated = 0;
  long double worst_mean = 0, worst_plane = 0;
  band_master b;
  b.v.resize(px);
  b.var.resize(px);
  b.bits.assign(m.invalid.bits.begin(), m.invalid.bits.end());
  for (std::size_t i = 0; i < px; ++i) {
    std::int64_t s = 0, s2 = 0;
    bool sat = false;
    for (const raw_frame& f : raw) {
      const auto x = static_cast<std::int64_t>(f.v[i]);
      s += x;
      s2 += x * x;
      sat = sat || f.v[i] >= 65535;
    }
    b.v[i] = m.data<double>()[i];
    b.var[i] = m.variance_data<double>()[i];
    if (sat) {
      ++saturated;
      if (m.invalid.bits[i] != invalid_reason::saturated || !std::isnan(b.v[i]) || !std::isnan(b.var[i])) ++wrong;
      continue;
    }
    if (m.invalid.bits[i] != 0) {
      ++wrong;
      continue;
    }
    const long double mean = static_cast<long double>(s) / nn;
    const long double num = static_cast<long double>(static_cast<std::int64_t>(rels.size()) * s2 - s * s);
    const long double plane = num / (nn * nn * (nn - 1));
    const long double sabs = static_cast<long double>(s);  // campioni non negativi
    const long double delta = gm * sabs / nn;
    const long double tol_mean = delta + oracle_rel * std::fabs(mean);
    const long double tol_plane = gp * plane + (1 + gp) * delta * delta / (nn - 1) + oracle_rel * plane;
    const long double em = std::fabs(static_cast<long double>(b.v[i]) - mean);
    const long double ep = std::fabs(static_cast<long double>(b.var[i]) - plane);
    worst_mean = std::max(worst_mean, em / tol_mean);
    worst_plane = std::max(worst_plane, tol_plane > 0 ? ep / tol_plane : (ep > 0 ? 1e30L : 0));
    if (em > tol_mean || ep > tol_plane) ++wrong;
  }
  EXPECT_EQ(wrong, 0u) << slot;
  log << "  master " << slot << ": " << rels.size() << " frame, " << saturated << " pixel saturi, scarto/tolleranza massimo media="
      << static_cast<double>(worst_mean) << " piano=" << static_cast<double>(worst_plane) << "\n";
  b.in = product_master_input(store, slot);
  return b;
}

enum class master_source { test_mean, product };

void run_band(const band& b, const std::map<std::string, double>& env, master_source source) {
  const double offset = env.at("offset"), read_noise = env.at("read_noise");
  const std::vector<std::string> dark_rels = {"darks/ccd.013.0.fits", "darks/ccd.014.0.fits", "darks/ccd.015.0.fits"};
  const raw_frame light = load_raw(b.light);
  const std::uint32_t w = light.w, h = light.h;
  const bool product = source == master_source::product;
  const fs::path dir = fresh_dir(std::string(product ? "real_product_" : "real_") + b.name);
  write_bytes(dir / "light.fits", light.file);
  auto store = sqlite_dag_store::create(dir / "store");
  memory_pool pool(4096 * (std::uint64_t{1} << 20));
  std::ostringstream os;

  band_master dark, flat, dflat;
  if (product) {
    dark = product_master(*store, "master_dark", master_role::dark, dark_rels, "r", offset, pool, os);
    flat = product_master(*store, "master_flat", master_role::flat, b.flat, b.filter, offset, pool, os);
    dflat = product_master(*store, "master_dark_flat", master_role::dark_flat, b.dark_flat, "r", offset, pool, os);
  } else {
    const master md = mean_of(dark_rels, "Dark Frame", "r", offset);
    const master mf = mean_of(b.flat, "Flat Field", b.filter, offset);
    const master mdf = mean_of(b.dark_flat, "Dark Flat", "r", offset);
    ASSERT_EQ(md.w, w);
    ASSERT_EQ(mf.w, w);
    dark = test_master(md, dir / "dark.fits", master_role::dark, read_noise);
    flat = test_master(mf, dir / "flat.fits", master_role::flat, read_noise);
    dflat = test_master(mdf, dir / "dark_flat.fits", master_role::dark_flat, read_noise);
  }

  const auto decl = [](double v) { return metadata_value<double>{v, {metadata_origin::declared, {}}}; };
  calibration_recipe r;
  r.light.path = (dir / "light.fits").string();
  {
    const std::string text = read_text(r.light.path);
    r.light.digest = digest_of(std::vector<std::uint8_t>(text.begin(), text.end()));
  }
  r.light.variance = noise(read_noise);
  r.dark = dark.in;
  r.flat = flat.in;
  r.dark_flat = dflat.in;
  // Il light grezzo non porta questi metadati: la ricetta li dichiara (valori dalla documentazione del dataset).
  r.light.declared.offset_adu = decl(offset);
  r.light.declared.conversion_gain_e_per_adu = decl(1.1);
  r.light.declared.saturation_adu = decl(65535);
  if (!product) r.flat.declared.saturation_adu = decl(65535);
  for (calibration_input_spec* in : {&r.light}) {
    in->declared_fields.binning_x = 1;
    in->declared_fields.binning_y = 1;
    in->declared_fields.unit = pixel_unit::adu;
    in->declared_fields.lin = linearity::linear;
    in->read_options.declared_phase = process_phase::raw;
  }
  r.normalization = normalization_mode::global_median;
  r.q_min = 0.05;
  r.q_rel_sigma_max = 0.01;
  r.exposure_rel_tol = 1e-3;
  r.temperature_abs_tol_c = 1.0;
  r.temp_policy = temperature_policy::not_applicable_declared;
  r.simd = simd_test::requested();

  const node_spec node = make_calibration_node(r);
  store->put_node(node);
  store->create_slot("cal", node.key());
  calibration_job_options o;
  o.worker_count = 4;
  o.tile_rows = 128;
  cancel_token cancel;
  const job_outcome out = run_calibration_job(*store, "cal", r, pool, o, cancel);
  ASSERT_EQ(out.exit_code, 0) << (out.stop ? out.stop->what() : "");
  std::map<std::string, artifact_record> roles;
  for (const artifact_record& a : store->artifacts_for(node.key())) roles[a.output_role] = a;
  const std::string text = read_text(store->artifact_path(roles.at("primary").rel_path));
  const image result = decode_fits(std::vector<std::uint8_t>(text.begin(), text.end()), big_budget);
  ASSERT_EQ(result.geometry.width, w);
  ASSERT_EQ(result.type, sample_type::f32);
  const json report = parse_json(read_text(store->artifact_path(roles.at("report").rel_path)));

  // Verita' indipendente sul pixel, con le stesse regole di maschera della calibrazione (ADR-16 §6, §7).
  const std::size_t n = static_cast<std::size_t>(w) * h;
  const double sat = 65535;
  const auto f_saturated = [&](std::size_t i) { return flat.v[i] >= sat; };
  std::vector<double> fc_valid;
  fc_valid.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    if ((flat.bits[i] | dflat.bits[i]) != 0 || f_saturated(i)) continue;
    fc_valid.push_back(flat.v[i] - dflat.v[i]);
  }
  const double big_n = median_copy(fc_valid);
  const double reported_n = as_double(*at(report, {"normalization"}).items()[0].find("N"));
  EXPECT_EQ(reported_n, big_n);
  EXPECT_EQ(static_cast<std::uint64_t>(at(report, {"normalization"}).items()[0].find("n_valid")->as_integer()), fc_valid.size());

  const auto var_model_d = [&](double x, double count) {
    const double above = x - offset;
    return (read_noise * read_noise + (above > 0 ? above : 0.0) / 1.1) / count;
  };
  const auto master_var = [&](const band_master& m, std::size_t i) { return product ? m.var[i] : var_model_d(m.v[i], 3); };
  std::map<std::string, std::uint64_t> bit_counts;
  std::uint64_t bits_wrong = 0, values_wrong = 0, valid = 0;
  std::vector<double> lc_img(n, 0.0), c_img(n, 0.0);
  std::vector<std::uint8_t> valid_mask(n, 0);
  const std::array<const char*, 9> names = {"source_nonfinite", "source_flagged", "saturated", "master_invalid", "q_nonpositive",
                                            "q_nonfinite", "q_below_min", "q_unreliable", "result_nonfinite"};
  double var_sum = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const double l = light.v[i], d = dark.v[i], f = flat.v[i], df = dflat.v[i];
    const std::uint16_t master_bits = static_cast<std::uint16_t>(dark.bits[i] | flat.bits[i] | dflat.bits[i]);
    std::uint16_t expect = master_bits;
    if (master_bits != 0) expect |= invalid_reason::master_invalid;
    if (l >= sat || f_saturated(i)) expect |= invalid_reason::saturated;
    const double lc = l - d;
    const double q = (f - df) / big_n;
    double vq = 0;
    if ((flat.bits[i] | dflat.bits[i]) == 0 && !f_saturated(i)) {
      if (!std::isfinite(q)) expect |= invalid_reason::q_nonfinite;
      else if (q <= 0) expect |= invalid_reason::q_nonpositive;
      else {
        vq = (master_var(flat, i) + master_var(dflat, i)) / (big_n * big_n);
        if (q < 0.05) expect |= invalid_reason::q_below_min;
        if (std::sqrt(vq) / q > 0.01) expect |= invalid_reason::q_unreliable;
      }
    }
    const std::uint16_t got = result.invalid.bits[i];
    if (got != expect) ++bits_wrong;
    for (std::size_t bit = 0; bit < 9; ++bit) {
      if ((got >> bit) & 1u) ++bit_counts[names[bit]];
    }
    lc_img[i] = lc;
    if (got != 0) {
      if (!std::isnan(result.data<float>()[i]) || !std::isnan(result.variance_data<float>()[i])) ++values_wrong;
      continue;
    }
    ++valid;
    valid_mask[i] = 1;
    const double c = lc / q;
    c_img[i] = result.data<float>()[i];
    const double pred_var = (var_model_d(l, 1) + master_var(dark, i)) / (q * q) + (lc * lc * vq) / std::pow(q, 4);
    const double got_c = result.data<float>()[i], got_v = result.variance_data<float>()[i];
    const rat exact_c = exact::from_double(c);
    if (!close_to(exact_c, got_c, 12, true)) ++values_wrong;
    if (!(got_v > 0) || !std::isfinite(got_v) || std::fabs(got_v - pred_var) > 1e-4 * pred_var) ++values_wrong;
    var_sum += got_v;
  }
  EXPECT_EQ(bits_wrong, 0u) << b.name;
  EXPECT_EQ(values_wrong, 0u) << b.name;
  EXPECT_TRUE(operator_output_invariants_hold(result));

  std::vector<std::uint8_t> region(n, 0);
  for (std::uint32_t y = 0; y < h; ++y) {
    for (std::uint32_t x = 0; x < 2048; ++x) region[static_cast<std::size_t>(y) * w + x] = valid_mask[static_cast<std::size_t>(y) * w + x];
  }
  const double cv_before = block_cv(lc_img, w, region);
  const double cv_after = block_cv(c_img, w, region);
  EXPECT_LT(cv_after, cv_before) << "il flat non riduce la disuniformita' a grande scala";

  std::vector<double> overscan_lc, data_c;
  for (std::uint32_t y = 0; y < h; ++y) {
    for (std::uint32_t x = 2048; x < w; ++x) overscan_lc.push_back(lc_img[static_cast<std::size_t>(y) * w + x]);
    for (std::uint32_t x = 0; x < 2048; x += 8) {
      if (valid_mask[static_cast<std::size_t>(y) * w + x]) data_c.push_back(c_img[static_cast<std::size_t>(y) * w + x]);
    }
  }
  os << "PUB-LFC-01 banda " << b.name << (product ? " (master del prodotto)" : "") << ": " << w << "x" << h
     << " pixel, N=" << big_n << " n_valid=" << fc_valid.size()
     << ", offset_dichiarato=" << offset << " read_noise_adu=" << read_noise << "\n"
     << "  pixel validi=" << valid << " (" << 100.0 * static_cast<double>(valid) / static_cast<double>(n) << " %), invalidi="
     << n - valid << "\n";
  for (const auto& [name, count] : bit_counts) os << "  bit " << name << ": " << count << "\n";
  os << "  varianza media sui validi=" << var_sum / static_cast<double>(valid) << " ADU^2, mediana C regione dati=" << median_copy(data_c)
     << " ADU\n  disuniformita' a grande scala (CV blocchi 64x64): prima=" << cv_before << " dopo=" << cv_after
     << "\n  mediana di L-D nell'overscan=" << median_copy(overscan_lc) << " ADU (registrata, nessuna soglia)\n"
     << "  rel_var_estimate di N=" << as_double(*at(report, {"normalization"}).items()[0].find("normalization_rel_var_estimate"))
     << " iid_assumption=" << at(report, {"normalization"}).items()[0].find("iid_assumption")->as_string() << "\n  avvisi:";
  for (const json& wn : at(report, {"correction_ledger", "warnings"}).items()) os << " " << wn.as_string();
  os << "\n  Fc<=0 entrati in N=" << at(report, {"normalization"}).items()[0].find("fc_nonpositive_in_N")->as_integer() << "\n";
  std::cout << os.str() << std::flush;
  fs::create_directories(fs::path(ASTRO_REAL_OUT));
  std::ofstream(fs::path(ASTRO_REAL_OUT) / (std::string(product ? "real-product-" : "real-") + b.name + ".txt")) << os.str();
  EXPECT_GT(valid, n / 2);
}

std::map<std::string, double> real_environment() {
  // Rumore di lettura e livello di offset dai bias del dataset (ccd.001, ccd.002): sigma robusta della differenza / sqrt(2).
  const raw_frame b1 = load_raw("ccd.001.0.fits"), b2 = load_raw("ccd.002.0.fits");
  std::vector<double> diff, level;
  for (std::uint32_t y = 0; y < b1.h; ++y) {
    for (std::uint32_t x = 0; x < 2048; ++x) {
      const std::size_t i = static_cast<std::size_t>(y) * b1.w + x;
      diff.push_back(b1.v[i] - b2.v[i]);
      level.push_back(b1.v[i]);
    }
  }
  const double med = median_copy(diff);
  std::vector<double> dev;
  for (double d : diff) dev.push_back(std::fabs(d - med));
  const double read_noise = 1.4826 * median_copy(dev) / std::sqrt(2.0);
  const double offset = median_copy(level);
  EXPECT_GT(read_noise, 0.0);
  return {{"offset", offset}, {"read_noise", read_noise}};
}

const band band_g{"g", "ccd.037.0.fits", {"ccd.014.0.fits", "ccd.015.0.fits", "ccd.016.0.fits"},
                  {"darks/ccd.017.0.fits", "darks/ccd.018.0.fits", "darks/ccd.019.0.fits"}, "g"};
const band band_i{"i", "ccd.043.0.fits", {"ccd.017.0.fits", "ccd.018.0.fits", "ccd.019.0.fits"},
                  {"darks/ccd.023.0.fits", "darks/ccd.024.0.fits", "darks/ccd.025.0.fits"}, "i"};

TEST(CalibReal, T_M2E_13_palomar_lfc_bands_g_and_i) {
  const std::map<std::string, double> env = real_environment();
  run_band(band_g, env, master_source::test_mean);
  run_band(band_i, env, master_source::test_mean);
}

// T-M2R2-09: dark, flat e dark-flat grezzi di PUB-LFC-01 combinati dal prodotto (uscita f64), confrontati con la
// media del test; poi T-M2E-13 rieseguito su questi master, con piano VARIANCE empirico e descrittore dal nodo.
TEST(CalibReal, T_M2R2_09_product_masters_from_raw_palomar_frames) {
  const std::map<std::string, double> env = real_environment();
  run_band(band_g, env, master_source::product);
  run_band(band_i, env, master_source::product);
}

}  // namespace
