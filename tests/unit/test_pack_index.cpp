// U11, G-PACK parte 2 (plans/M3a.md U11; docs/contracts/ADR-08-decoder-pack.md §3.1 file dati e indice, §3.3 punti 5
// e 7, §3.4, §3.6): T-M3A-PACK-06, T-M3A-PACK-08, T-M3A-PACK-09 (lettore dell'indice e delle righe) e i casi di §3.6
// fissati dai test di U11 (directory relativa nella ricetta, `audit.evidence_root` vuoto, `flag_bits: []`).
//
// Il pack sintetico lo scrive tests/fixtures/syn_pack.hpp, la copertura HEALPix la giudica l'oracolo indipendente di
// tests/fixtures/healpix_oracle.hpp (Górski et al. 2005). Codici e categorie attesi sono quelli della tabella di §3.4
// e delle scelte di §3.6, scritti a mano.
//
// Interfaccia (astro/pack.hpp):
//
//   // Regione del campo nota nel preflight (§3.3 punto 7): calotta ICRS di centro (ra_deg, dec_deg) e raggio
//   // radius_deg, in gradi.
//   struct sky_cap { double ra_deg = 0, dec_deg = 0, radius_deg = 0; };
//
//   // §3.3 punto 7 sulla `coverage` del manifest verificato: se una cella che interseca la calotta allargata del
//   // raggio angolare massimo di una cella all'ordine di coverage non è in coverage, stop incompatible
//   // `pack.out_of_coverage`. Regione non valida (campo non finito, raggio negativo, |dec| > 90): stop invalid_input
//   // `pack.region_invalid` (§3.6).
//   void verify_pack_coverage(const verified_pack& pack, const sky_cap& region);
//
//   // Una riga letta: values[c] è il valore della colonna c di manifest.columns convertito in double senza perdita
//   // (f64 identico, f32 allargato, u16 e u32 esatti); un'assenza è NaN.
//   struct pack_row { std::vector<double> values; };
//
//   // Righe della cella `cell` di coverage, lette dal data_file del pack della ricetta attraverso l'index_file.
//   // Ogni riga letta è controllata secondo §3.1 (null_bit, NaN, infiniti, bit non dichiarati): riga incoerente,
//   // stop integrity `pack.row_invalid`. Indice e dimensione del file dati sono riverificati su ciò che si legge.
//   std::vector<pack_row> read_pack_cell(const pack_recipe& recipe, const verified_pack& pack, std::uint64_t cell);
//
//   verify_pack esteso con §3.3 punto 5: conteggi incoerenti `pack.count_mismatch`, indice incoerente
//   `pack.index_invalid` (integrity). Da §3.6: la somma delle righe dell'indice diversa dal count è
//   `pack.count_mismatch`; dimensione dell'indice, celle, ordine, contiguità e partenza dalla riga 0 sono
//   `pack.index_invalid`.
//
// Codici di §3.6 fissati dai test di U11:
//   - directory della ricetta non assoluta (anche vuota) e `audit.evidence_root` vuoto o relativo: invalid_input
//     `pack.path_not_absolute`, prima di ogni apertura;
//   - regione di copertura non valida: invalid_input `pack.region_invalid`;
//   - `flag_bits: []`: `pack.manifest_invalid`.
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "astro/error.hpp"
#include "astro/pack.hpp"
#include "healpix_oracle.hpp"
#include "open_probe.hpp"
#include "store_test_support.hpp"
#include "syn_pack.hpp"

// Guasto fra il controllo e la lettura: `fstat` è interposta dall'eseguibile (come le aperture di open_probe.hpp) e,
// mentre un `stat_hook::arm` è vivo, esegue una sola volta la sua azione subito dopo la prima fstat riuscita su un
// descrittore aperto sul file indicato. Fuori da quella finestra la chiamata passa invariata alla libc.
namespace stat_hook {

inline std::atomic<bool> armed{false};
inline std::mutex mu;
inline std::string target;
inline std::function<void()> action;
inline bool fired = false;
inline thread_local bool busy = false;

inline std::string fd_path(int fd) {
  char buf[4096];
  const ssize_t n = ::readlink(("/proc/self/fd/" + std::to_string(fd)).c_str(), buf, sizeof buf - 1);
  return n > 0 ? std::string(buf, static_cast<std::size_t>(n)) : std::string();
}

inline void after(int fd) {
  if (!armed.load() || busy) return;
  busy = true;
  std::function<void()> act;
  {
    std::lock_guard<std::mutex> lock(mu);
    if (action && fd_path(fd) == target) {
      act = std::move(action);
      action = nullptr;
      fired = true;
    }
  }
  if (act) act();
  busy = false;
}

class arm {
 public:
  arm(const std::filesystem::path& path, std::function<void()> act) {
    std::lock_guard<std::mutex> lock(mu);
    target = std::filesystem::canonical(path).string();
    action = std::move(act);
    fired = false;
    armed = true;
  }
  ~arm() {
    std::lock_guard<std::mutex> lock(mu);
    armed = false;
    action = nullptr;
    target.clear();
  }
  arm(const arm&) = delete;
  arm& operator=(const arm&) = delete;
  bool fired_once() const {
    std::lock_guard<std::mutex> lock(mu);
    return fired;
  }
};

}  // namespace stat_hook

extern "C" {

int fstat(int fd, struct stat* st) {
  static auto real = open_probe::next_symbol<int (*)(int, struct stat*)>("fstat");
  const int rc = real(fd, st);
  if (rc == 0) stat_hook::after(fd);
  return rc;
}

int fstat64(int fd, struct stat64* st) {
  static auto real = open_probe::next_symbol<int (*)(int, struct stat64*)>("fstat64");
  const int rc = real(fd, st);
  if (rc == 0) stat_hook::after(fd);
  return rc;
}

}  // extern "C"

namespace {

using namespace astro;
namespace ho = healpix_oracle;
namespace sp = syn_pack;
namespace fs = std::filesystem;
using store_test::fresh_dir;

constexpr int fuzz_iterations = 5000;

std::optional<error_category> category_of(const std::string& code) {
  static const std::map<std::string, error_category> table{
      {"pack.missing", error_category::metadata_missing},
      {"pack.manifest_invalid", error_category::integrity},
      {"pack.format", error_category::unsupported},
      {"pack.digest_mismatch", error_category::incompatible},
      {"pack.id_mismatch", error_category::incompatible},
      {"pack.file_missing", error_category::integrity},
      {"pack.size_mismatch", error_category::integrity},
      {"pack.hash_mismatch", error_category::integrity},
      {"pack.unlisted_file", error_category::integrity},
      {"pack.count_mismatch", error_category::integrity},
      {"pack.index_invalid", error_category::integrity},
      {"pack.license_not_allowed", error_category::incompatible},
      {"pack.license_file", error_category::incompatible},
      {"pack.license_evidence", error_category::incompatible},
      {"pack.purpose", error_category::incompatible},
      {"pack.band_missing", error_category::incompatible},
      {"pack.out_of_coverage", error_category::incompatible},
      {"pack.row_invalid", error_category::integrity},
      {"pack.io", error_category::io},
      {"pack.path_not_absolute", error_category::invalid_input},
      {"pack.region_invalid", error_category::invalid_input},
  };
  const auto it = table.find(code);
  if (it == table.end()) return std::nullopt;
  return it->second;
}

pack_use astrometry_use() {
  pack_use u;
  u.purpose = "astrometry";
  u.bands = {"J"};
  return u;
}

pack_recipe recipe_of(const sp::synth& p) {
  pack_recipe r;
  r.pack_id = p.id;
  r.pack_version = p.version;
  r.pack_digest = p.digest;
  r.directory = p.dir.string();
  return r;
}

pack_audit audit_of(const sp::synth& p) {
  pack_audit a;
  a.evidence_root = p.evidence_root.string();
  return a;
}

// Esito di G-PACK seguito dalla lettura delle celle: uno stop dell'una o dell'altra chiamata. Il contratto dice che
// le righe si verificano «sulle righe che il lettore legge»; un prodotto che le verificasse già in G-PACK darebbe lo
// stesso codice, quindi il test accetta lo stop da entrambe le chiamate.
struct outcome {
  bool ok = false;
  error_category category = error_category::invalid_input;
  std::string code, what;
  stop_error::context_list context;
  std::vector<std::vector<pack_row>> rows;
};

outcome verify_and_read(const std::vector<std::uint64_t>& cells, const pack_recipe& r, const pack_audit& a) {
  outcome o;
  try {
    const verified_pack v = verify_pack(r, astrometry_use(), a);
    for (const std::uint64_t cell : cells) o.rows.push_back(read_pack_cell(r, v, cell));
    o.ok = true;
  } catch (const stop_error& e) {
    o.category = e.category();
    o.code = e.code();
    o.what = e.what();
  }
  return o;
}

outcome verify_and_read(const sp::synth& p, const std::vector<std::uint64_t>& cells) {
  return verify_and_read(cells, recipe_of(p), audit_of(p));
}

outcome verify_only(const pack_recipe& r, const pack_audit& a) {
  outcome o;
  try {
    (void)verify_pack(r, astrometry_use(), a);
    o.ok = true;
  } catch (const stop_error& e) {
    o.category = e.category();
    o.code = e.code();
    o.what = e.what();
  }
  return o;
}

void expect_stop(const outcome& o, const std::string& code, const std::string& label) {
  SCOPED_TRACE(label);
  ASSERT_FALSE(o.ok) << "stop " << code << " atteso, nessuno stop";
  EXPECT_EQ(o.code, code) << o.what;
  const std::optional<error_category> cat = category_of(code);
  ASSERT_TRUE(cat.has_value()) << "codice fuori dalla tabella del test: " << code;
  EXPECT_EQ(o.category, *cat) << o.what;
}

void expect_stop_among(const outcome& o, const std::set<std::string>& codes, const std::string& label) {
  SCOPED_TRACE(label);
  ASSERT_FALSE(o.ok) << "stop atteso, nessuno stop";
  EXPECT_EQ(codes.count(o.code), 1u) << o.what;
  const std::optional<error_category> cat = category_of(o.code);
  ASSERT_TRUE(cat.has_value()) << "codice fuori dalla tabella del test: " << o.code;
  EXPECT_EQ(o.category, *cat) << o.what;
}

void expect_rows(const std::vector<pack_row>& got, const std::vector<sp::row>& want, const std::string& label,
                 const sp::schema& cols = sp::default_schema()) {
  SCOPED_TRACE(label);
  ASSERT_EQ(got.size(), want.size());
  for (std::size_t r = 0; r < want.size(); ++r) {
    ASSERT_EQ(got[r].values.size(), cols.size()) << "riga " << r;
    for (std::size_t i = 0; i < cols.size(); ++i) {
      const std::size_t c = cols[i];
      const double w = sp::value(want[r], c);
      const double g = got[r].values[i];
      if (std::isnan(w)) {
        EXPECT_TRUE(std::isnan(g)) << "riga " << r << " colonna " << sp::columns()[c].name;
      } else {
        EXPECT_EQ(sp::bits64(g), sp::bits64(w)) << "riga " << r << " colonna " << sp::columns()[c].name << ": " << g
                                                << " invece di " << w;
      }
    }
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Oracolo HEALPix di T-M3A-PACK-06, controllato prima dell'uso (plans/M3a.md, U11): centri delle 12 celle di base,
// annidamento dei 4 figli, biiezione con i centri d'anello di Górski et al. 2005 §4, raggio massimo.
// ---------------------------------------------------------------------------------------------------------------------

TEST(PackIndex, T_M3A_PACK_06_oracle_puts_the_twelve_analytic_base_centers_in_their_base_cells) {
  for (std::uint64_t f = 0; f < 4; ++f) {
    const double fd = static_cast<double>(f);
    EXPECT_EQ(ho::ang2pix_nest(0, 2.0 / 3.0, (2 * fd + 1) * ho::pi / 4), f) << "nord " << f;
    EXPECT_EQ(ho::ang2pix_nest(0, 0.0, fd * ho::pi / 2), 4 + f) << "equatore " << f;
    EXPECT_EQ(ho::ang2pix_nest(0, -2.0 / 3.0, (2 * fd + 1) * ho::pi / 4), 8 + f) << "sud " << f;
  }
  EXPECT_EQ(ho::ang2pix_nest(0, 1.0, 0.3), 0u);
  EXPECT_EQ(ho::ang2pix_nest(0, -1.0, 0.3), 8u);
}

// Fig. 4 di Górski et al. 2005: dentro una cella, figlio 0 a sud, 1 a est, 2 a ovest, 3 a nord. Al centro di una
// cella di base le direzioni sud/nord (meridiano) ed est/ovest (parallelo) puntano ai quattro vertici.
TEST(PackIndex, T_M3A_PACK_06_oracle_numbers_the_four_children_south_east_west_north) {
  for (std::uint64_t b = 0; b < 12; ++b) {
    const double f = static_cast<double>(b % 4);
    const double z = b < 4 ? 2.0 / 3.0 : (b < 8 ? 0.0 : -2.0 / 3.0);
    const double phi = (b < 4 || b >= 8) ? (2 * f + 1) * ho::pi / 4 : f * ho::pi / 2;
    const double theta = std::acos(z), e = 2 * ho::deg;
    const auto at = [](double th, double ph) { return ho::ang2pix_nest(1, std::cos(th), std::fmod(ph + 2 * ho::pi, 2 * ho::pi)); };
    EXPECT_EQ(at(theta + e, phi), 4 * b + 0) << "sud della cella " << b;
    EXPECT_EQ(at(theta, phi + e), 4 * b + 1) << "est della cella " << b;
    EXPECT_EQ(at(theta, phi - e), 4 * b + 2) << "ovest della cella " << b;
    EXPECT_EQ(at(theta - e, phi), 4 * b + 3) << "nord della cella " << b;
  }
}

TEST(PackIndex, T_M3A_PACK_06_oracle_child_cells_nest_in_their_parent_at_every_order) {
  const std::vector<ho::vec3> points = ho::fibonacci_sphere(100000);
  int wrong = 0;
  for (int order = 0; order < 6; ++order) {
    for (const ho::vec3& p : points) {
      if (ho::ang2pix_nest(order + 1, p) >> 2 != ho::ang2pix_nest(order, p)) ++wrong;
    }
  }
  EXPECT_EQ(wrong, 0);
}

TEST(PackIndex, T_M3A_PACK_06_oracle_maps_the_ring_centers_one_to_one_onto_the_nested_cells) {
  for (int order = 0; order <= 4; ++order) {
    SCOPED_TRACE(order);
    EXPECT_EQ(ho::ring_centers(order).size(), ho::cells(order));
    EXPECT_NO_THROW((void)ho::nest_centers(order));
  }
}

// Il raggio massimo dell'oracolo è un limite superiore di ogni distanza osservata fra un punto e il centro della sua
// cella; il campionamento (100 000 punti, spaziatura ~0,64°) arriva entro 1° dal vertice più lontano.
TEST(PackIndex, T_M3A_PACK_06_oracle_max_cell_radius_bounds_every_sampled_distance) {
  EXPECT_NEAR(ho::max_cell_radius(0), std::acos(2.0 / 3.0), 1e-12);  // centro (z = 2/3, φ = π/4) e polo nord
  const std::vector<ho::vec3> points = ho::fibonacci_sphere(100000);
  for (int order = 0; order <= 2; ++order) {
    SCOPED_TRACE(order);
    const double sampled = ho::max_center_distance(order, points);
    EXPECT_LE(sampled, ho::max_cell_radius(order) + 1e-12);
    EXPECT_GE(sampled, ho::max_cell_radius(order) - 1.0 * ho::deg);
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// T-M3A-PACK-08: pack valido, conteggi e indice (§3.3 punto 5).
// ---------------------------------------------------------------------------------------------------------------------

TEST(PackIndex, T_M3A_PACK_08_valid_pack_passes_and_every_cell_reads_back_the_written_rows) {
  const sp::synth p = sp::make_pack(fresh_dir("u11valid"));
  const outcome o = verify_and_read(p, p.coverage());
  ASSERT_TRUE(o.ok) << o.what;
  ASSERT_EQ(o.rows.size(), p.coverage().size());
  for (std::size_t i = 0; i < o.rows.size(); ++i) {
    expect_rows(o.rows[i], p.rows_of(p.coverage()[i]), "cella " + std::to_string(p.coverage()[i]));
  }
}

// Lettura dal §3.1: una cella coperta senza righe (voce con zero righe, intervallo vuoto contiguo) e più componenti
// la cui somma è il count sono validi.
TEST(PackIndex, T_M3A_PACK_08_a_covered_cell_without_rows_and_two_components_are_valid) {
  const fs::path base = fresh_dir("u11validvariants");
  sp::layout lay = sp::default_layout();
  lay.cells[1].second = 0;
  const sp::synth empty = sp::make_pack(base / "empty", lay);
  const outcome o = verify_and_read(empty, empty.coverage());
  ASSERT_TRUE(o.ok) << o.what;
  EXPECT_TRUE(o.rows[1].empty());
  expect_rows(o.rows[3], empty.rows_of(3), "cella 3");

  sp::synth two = sp::make_pack(base / "two");
  sp::add_component(two, "synthetic2", 6);
  two.set_counts(15, 9);
  const outcome t = verify_and_read(two, two.coverage());
  EXPECT_TRUE(t.ok) << t.what;
}

// Lo schema viene da manifest.columns: colonne permutate, senza nobs e pm_mas, record di 28 byte invece di 38.
TEST(PackIndex, T_M3A_PACK_08_columns_in_another_order_and_record_length_are_read_from_the_manifest) {
  sp::layout lay = sp::default_layout();
  lay.cols = sp::permuted_schema();
  ASSERT_EQ(sp::record_size(lay.cols), 28u);
  const sp::synth p = sp::make_pack(fresh_dir("u11permuted"), lay);
  ASSERT_EQ(sp::read_bytes(p.dir / sp::data_name).size(), 28u * p.rows.size());
  const outcome o = verify_and_read(p, p.coverage());
  ASSERT_TRUE(o.ok) << o.what;
  ASSERT_EQ(o.rows.size(), p.coverage().size());
  for (std::size_t i = 0; i < o.rows.size(); ++i) {
    expect_rows(o.rows[i], p.rows_of(p.coverage()[i]), "cella " + std::to_string(p.coverage()[i]), lay.cols);
  }
}

struct pack_case {
  std::string label;
  std::function<void(sp::synth&)> apply;
};

TEST(PackIndex, T_M3A_PACK_08_inconsistent_counts_stop_as_count_mismatch) {
  const fs::path base = fresh_dir("u11counts");
  const std::vector<pack_case> cases{
      {"count del componente 14, manifest, dati e indice 15", [](sp::synth& p) { p.set_counts(15, 14); }},
      {"count del manifest 16, componente 16, dati e indice 15", [](sp::synth& p) { p.set_counts(16, 16); }},
      {"solo il count del manifest 16, componente, dati e indice 15", [](sp::synth& p) { p.set_counts(16, 15); }},
      {"solo il count del manifest 14, componente, dati e indice 15", [](sp::synth& p) { p.set_counts(14, 15); }},
      {"count del manifest e del componente 14, dati e indice 15", [](sp::synth& p) { p.set_counts(14, 14); }},
      {"una riga valida in più nel file dati",
       [](sp::synth& p) {
         std::vector<sp::row> rows = p.rows;
         const sp::row extra = p.rows.back();
         rows.push_back(extra);
         p.rewrite_rows(rows);
       }},
      {"file dati con 5 byte in più (resto non nullo)",
       [](sp::synth& p) { p.rewrite(sp::data_name, sp::data_bytes(p.rows) + std::string(5, '\0')); }},
      {"file dati troncato di un byte",
       [](sp::synth& p) {
         const std::string d = sp::data_bytes(p.rows);
         p.rewrite(sp::data_name, d.substr(0, d.size() - 1));
       }},
      {"file dati vuoto", [](sp::synth& p) { p.rewrite(sp::data_name, ""); }},
      {"somma delle righe dell'indice 14, intervalli contigui",
       [](sp::synth& p) {
         std::vector<sp::index_entry> e = p.index;
         e.back().rows = 1;
         p.rewrite_index(e);
       }},
      {"due componenti 9 + 5 contro 15",
       [](sp::synth& p) {
         sp::add_component(p, "synthetic2", 5);
         p.set_counts(15, 9);
       }},
  };
  for (std::size_t i = 0; i < cases.size(); ++i) {
    sp::synth p = sp::make_pack(base / std::to_string(i));
    cases[i].apply(p);
    expect_stop(verify_only(recipe_of(p), audit_of(p)), "pack.count_mismatch", cases[i].label);
  }
}

TEST(PackIndex, T_M3A_PACK_08_inconsistent_index_stops_as_index_invalid) {
  const fs::path base = fresh_dir("u11index");
  const auto with_index = [](std::function<void(std::vector<sp::index_entry>&)> edit) {
    return [edit](sp::synth& p) {
      std::vector<sp::index_entry> e = p.index;
      edit(e);
      p.rewrite_index(e);
    };
  };
  const std::vector<pack_case> cases{
      {"indice con un byte in più",
       [](sp::synth& p) { p.rewrite(sp::index_name, sp::index_bytes(p.index) + std::string(1, '\0')); }},
      {"indice con un byte in meno",
       [](sp::synth& p) {
         const std::string i = sp::index_bytes(p.index);
         p.rewrite(sp::index_name, i.substr(0, i.size() - 1));
       }},
      {"voce in più per una cella fuori da coverage", with_index([](auto& e) { e.push_back({47, 15, 0}); })},
      {"voce dell'ultima cella mancante, dati e conteggi di 13 righe",
       [](sp::synth& p) {
         std::vector<sp::index_entry> e = p.index;
         e.pop_back();
         std::vector<sp::row> rows(p.rows.begin(), p.rows.end() - 2);
         p.rewrite_index(e);
         p.rewrite_rows(rows);
         p.set_counts(13, 13);
       }},
      {"cella 21 scritta come 22", with_index([](auto& e) { e[4].cell = 22; })},
      {"celle 2 e 3 scambiate, intervalli invariati", with_index([](auto& e) { std::swap(e[2].cell, e[3].cell); })},
      {"cella ripetuta (1 al posto di 2)", with_index([](auto& e) { e[2].cell = 1; })},
      {"intervalli sovrapposti: la cella 1 parte dalla riga 2", with_index([](auto& e) { e[1].first = 2; })},
      {"intervalli contigui che partono dalla riga 1",
       with_index([](auto& e) {
         for (sp::index_entry& x : e) x.first += 1;
       })},
  };
  for (std::size_t i = 0; i < cases.size(); ++i) {
    sp::synth p = sp::make_pack(base / std::to_string(i));
    cases[i].apply(p);
    expect_stop(verify_only(recipe_of(p), audit_of(p)), "pack.index_invalid", cases[i].label);
  }
}

// Righe dell'indice che, sommate in aritmetica modulo 2^64, danno il count giusto e intervalli «contigui»: un lettore
// che non controlla il traboccamento accetterebbe celle di 2^63 righe. Il contratto non dice quale dei due controlli
// lo vede per primo: uno dei due codici dei conteggi e dell'indice.
TEST(PackIndex, T_M3A_PACK_08_row_counts_that_overflow_to_the_right_sum_never_pass) {
  sp::synth p = sp::make_pack(fresh_dir("u11overflow"));
  std::vector<sp::index_entry> e = p.index;
  const std::uint64_t half = std::uint64_t{1} << 63;
  ASSERT_EQ(e[4].cell, 21u);
  ASSERT_EQ(e[4].first, 10u);
  e[4].rows = half + 3;
  e[5].first = 10 + half + 3;
  e[5].rows = half + 2;
  std::uint64_t sum = 0;
  for (const sp::index_entry& x : e) sum += x.rows;
  ASSERT_EQ(sum, 15u);  // modulo 2^64
  p.rewrite_index(e);
  expect_stop_among(verify_only(recipe_of(p), audit_of(p)), {"pack.count_mismatch", "pack.index_invalid"},
                    "righe di 2^63 + 3 e 2^63 + 2");
}

// Un intervallo dell'indice di 2^64 - 1 righe riporta, modulo 2^64, la riga successiva a 9: la voce seguente parte
// da 9 e la somma torna 15. Solo il controllo «intervallo entro il file dati» della voce lo vede; il contratto non dice
// se sia contiguità o somma: uno dei due codici.
TEST(PackIndex, T_M3A_PACK_08_an_index_interval_that_wraps_around_never_passes) {
  sp::synth p = sp::make_pack(fresh_dir("u11wrap"));
  std::vector<sp::index_entry> e = p.index;
  ASSERT_EQ(e[4].cell, 21u);
  ASSERT_EQ(e[4].first, 10u);
  e[4].rows = std::numeric_limits<std::uint64_t>::max();
  e[5].first = e[4].first + e[4].rows;  // modulo 2^64
  ASSERT_EQ(e[5].first, 9u);
  e[5].rows = 6;
  ASSERT_EQ(e[5].first + e[5].rows, p.rows.size());
  p.rewrite_index(e);
  expect_stop_among(verify_only(recipe_of(p), audit_of(p)), {"pack.count_mismatch", "pack.index_invalid"},
                    "voce di 2^64 - 1 righe seguita da una voce che parte dalla riga 9");
}

// Count dei componenti la cui somma trabocca u64 e torna, modulo 2^64, al count del manifest e alle righe del file:
// il componente di base con 15 righe e 2048 componenti di 2^53 (l'intero più grande del JSON canonico): somma
// 2^64 + 15. §3.3 punto 5 vuole la somma vera uguale al count. I componenti aggiunti condividono licenza e record.
TEST(PackIndex, T_M3A_PACK_08_component_counts_that_overflow_to_the_right_sum_stop_as_count_mismatch) {
  sp::synth p = sp::make_pack(fresh_dir("u11compoverflow"));
  constexpr int extra = 2048;
  std::vector<sp::jv>& comps = p.manifest.at("components").items;
  for (int i = 0; i < extra; ++i) {
    comps.push_back(sp::component("synthetic-" + std::to_string(i), "licenses/synthetic.txt", "docs/datasets/PUB-SYN-01.md", 0));
  }
  p.write_manifest();
  const outcome control = verify_only(recipe_of(p), audit_of(p));
  ASSERT_TRUE(control.ok) << "componenti 15 + 2048 x 0: " << control.what;
  const std::int64_t two53 = std::int64_t{1} << 53;
  std::uint64_t sum = p.rows.size();
  for (int i = 1; i <= extra; ++i) {
    comps[static_cast<std::size_t>(i)].set("count", sp::jv::num(two53));
    sum += static_cast<std::uint64_t>(two53);
  }
  p.write_manifest();
  ASSERT_EQ(sum, p.rows.size());  // modulo 2^64
  expect_stop(verify_only(recipe_of(p), audit_of(p)), "pack.count_mismatch", "count dei componenti 15 + 2048 x 2^53");
}

// ---------------------------------------------------------------------------------------------------------------------
// T-M3A-PACK-08: righe lette (§3.1, valore assente), sulle righe che il lettore legge.
// ---------------------------------------------------------------------------------------------------------------------

constexpr std::size_t first_of_cell3 = 6;   // prima riga della cella 3 nel layout di base
constexpr std::size_t last_row = 14;        // ultima riga del file (cella 46)

TEST(PackIndex, T_M3A_PACK_08_absence_with_any_nan_payload_and_a_declared_quality_bit_is_accepted) {
  sp::synth p = sp::make_pack(fresh_dir("u11nanpayload"));
  std::vector<sp::row> rows = p.rows;
  ASSERT_EQ(p.rows_of(3).size(), 4u);
  sp::row& a = rows[first_of_cell3];
  a = sp::clean_row(a);
  a.j = 0x7FC00001u;  // NaN quieto con payload
  a.flags = sp::j_null_bit;
  sp::row& b = rows[first_of_cell3 + 1];
  b = sp::clean_row(b);
  b.h = 0xFFC00000u;  // NaN negativo
  b.flags = sp::h_null_bit;
  sp::row& c = rows[first_of_cell3 + 2];
  c = sp::clean_row(c);
  c.pm = 0x7FF0000000000001ULL;  // NaN segnalante f64
  c.flags = sp::pm_null_bit | sp::saturated_bit;
  sp::row& d = rows[first_of_cell3 + 3];
  d = sp::clean_row(d);
  d.j = sp::bits32(-0.0f);
  d.pm = sp::bits64(-1e300);
  d.nobs = 65535;
  p.rewrite_rows(rows);
  p.rows = rows;
  const outcome o = verify_and_read(p, {3});
  ASSERT_TRUE(o.ok) << o.what;
  expect_rows(o.rows[0], p.rows_of(3), "cella 3");
}

TEST(PackIndex, T_M3A_PACK_08_incoherent_rows_stop_as_row_invalid_when_read) {
  sp::synth p = sp::make_pack(fresh_dir("u11rows"));
  const std::vector<sp::row> baseline = p.rows;
  const float finf = std::numeric_limits<float>::infinity();
  const double dinf = std::numeric_limits<double>::infinity();
  struct row_case {
    std::string label;
    std::function<void(sp::row&)> apply;
  };
  const std::vector<row_case> cases{
      {"j_mag finito con il suo bit a 1", [](sp::row& r) { r.flags |= sp::j_null_bit; }},
      {"j_mag NaN con il bit a 0", [](sp::row& r) { r.j = sp::f32_quiet_nan; }},
      {"h_mag finito con il suo bit a 1", [](sp::row& r) { r.flags |= sp::h_null_bit; }},
      {"h_mag NaN con il bit a 0", [](sp::row& r) { r.h = sp::f32_quiet_nan; }},
      {"pm_mas (f64) finito con il suo bit a 1", [](sp::row& r) { r.flags |= sp::pm_null_bit; }},
      {"pm_mas (f64) NaN con il bit a 0", [](sp::row& r) { r.pm = sp::f64_quiet_nan; }},
      {"pm_mas +inf con il bit a 1 (l'assenza è NaN)",
       [dinf](sp::row& r) {
         r.pm = sp::bits64(dinf);
         r.flags |= sp::pm_null_bit;
       }},
      {"j_mag -inf con il bit a 1 (l'assenza è NaN)",
       [finf](sp::row& r) {
         r.j = sp::bits32(-finf);
         r.flags |= sp::j_null_bit;
       }},
      {"ra_deg NaN (non nullabile)", [](sp::row& r) { r.ra = sp::f64_quiet_nan; }},
      {"dec_deg NaN (non nullabile)", [](sp::row& r) { r.dec = sp::f64_quiet_nan; }},
      {"ra_deg +inf (non nullabile)", [dinf](sp::row& r) { r.ra = sp::bits64(dinf); }},
      {"dec_deg -inf (non nullabile)", [dinf](sp::row& r) { r.dec = sp::bits64(-dinf); }},
      {"bit 4 non dichiarato a 1", [](sp::row& r) { r.flags |= 1u << 4; }},
      {"bit 7 non dichiarato a 1", [](sp::row& r) { r.flags |= 1u << 7; }},
      {"bit 31 non dichiarato a 1", [](sp::row& r) { r.flags |= 1u << 31; }},
  };
  for (const auto& [target, cell] : {std::pair{first_of_cell3, std::uint64_t{3}}, std::pair{last_row, std::uint64_t{46}}}) {
    for (const row_case& c : cases) {
      std::vector<sp::row> rows = baseline;
      rows[target] = sp::clean_row(rows[target]);
      c.apply(rows[target]);
      p.rewrite_rows(rows);
      expect_stop(verify_and_read(p, {cell}), "pack.row_invalid", c.label + ", cella " + std::to_string(cell));
    }
  }
  p.rewrite_rows(baseline);
  const outcome control = verify_and_read(p, {3, 46});
  EXPECT_TRUE(control.ok) << control.what;
}

// Decisione del coordinatore (30 ottobre 2026): un valore non finito è ammesso solo come assenza, NaN con il bit di
// assenza a 1; un infinito, positivo o negativo, con il bit a 0 o a 1, in colonna nullabile o no, è pack.row_invalid.
TEST(PackIndex, T_M3A_PACK_08_an_infinity_is_never_an_absence_nor_a_value) {
  sp::synth p = sp::make_pack(fresh_dir("u11infinity"));
  const std::vector<sp::row> baseline = p.rows;
  for (const double sign : {1.0, -1.0}) {
    const double dinf = sign * std::numeric_limits<double>::infinity();
    const float finf = static_cast<float>(dinf);
    const std::string s = sign > 0 ? "+inf" : "-inf";
    const std::vector<std::pair<std::string, std::function<void(sp::row&)>>> cases{
        {"j_mag (f32 nullabile) " + s + ", bit a 0", [finf](sp::row& r) { r.j = sp::bits32(finf); }},
        {"j_mag (f32 nullabile) " + s + ", bit a 1",
         [finf](sp::row& r) {
           r.j = sp::bits32(finf);
           r.flags |= sp::j_null_bit;
         }},
        {"pm_mas (f64 nullabile) " + s + ", bit a 0", [dinf](sp::row& r) { r.pm = sp::bits64(dinf); }},
        {"pm_mas (f64 nullabile) " + s + ", bit a 1",
         [dinf](sp::row& r) {
           r.pm = sp::bits64(dinf);
           r.flags |= sp::pm_null_bit;
         }},
        {"ra_deg (f64 non nullabile) " + s, [dinf](sp::row& r) { r.ra = sp::bits64(dinf); }},
        {"dec_deg (f64 non nullabile) " + s, [dinf](sp::row& r) { r.dec = sp::bits64(dinf); }},
    };
    for (const auto& [label, apply] : cases) {
      std::vector<sp::row> rows = baseline;
      rows[first_of_cell3] = sp::clean_row(rows[first_of_cell3]);
      apply(rows[first_of_cell3]);
      p.rewrite_rows(rows);
      expect_stop(verify_and_read(p, {3}), "pack.row_invalid", label);
    }
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// T-M3A-PACK-08: rilettura di read_pack_cell su un pack cambiato DOPO il preflight. L'interfaccia dichiara che indice e
// dimensione del file dati sono riverificati su ciò che si legge; i codici sono quelli di §3.4 e §3.6 per lo stesso
// guasto trovato da G-PACK.
// ---------------------------------------------------------------------------------------------------------------------

outcome read_only(const pack_recipe& r, const verified_pack& v, std::uint64_t cell) {
  outcome o;
  try {
    o.rows.push_back(read_pack_cell(r, v, cell));
    o.ok = true;
  } catch (const stop_error& e) {
    o.category = e.category();
    o.code = e.code();
    o.what = e.what();
    o.context = e.context();
  }
  return o;
}

// Pack valido e il suo verified_pack: il preflight è passato prima di ogni guasto iniettato dal caso.
struct verified_synth {
  sp::synth p;
  pack_recipe recipe;
  verified_pack v;
};

verified_synth verified_at(const fs::path& base, const sp::layout& lay = sp::default_layout()) {
  verified_synth s{sp::make_pack(base, lay), {}, {}};
  s.recipe = recipe_of(s.p);
  s.v = verify_pack(s.recipe, astrometry_use(), audit_of(s.p));
  return s;
}

pack_file& listed_in(verified_pack& v, const std::string& rel) {
  for (pack_file& f : v.manifest.files) {
    if (f.path == rel) return f;
  }
  throw std::logic_error("file assente dal manifest verificato: " + rel);
}

std::string first_rows_bytes(const sp::synth& p, std::size_t n) { return sp::data_bytes({p.rows.begin(), p.rows.begin() + static_cast<std::ptrdiff_t>(n)}); }

void replace_with_file(const fs::path& path) {
  fs::remove_all(path);
  sp::write_bytes(path, "non una directory\n");
}

void replace_with_directory(const fs::path& path) {
  fs::remove(path);
  fs::create_directories(path);
}

struct read_case {
  std::string label;
  std::uint64_t cell;
  std::function<void(verified_synth&)> fault;
  std::string code;
};

void run_read_cases(const std::string& name, const std::vector<read_case>& cases, const sp::layout& lay = sp::default_layout()) {
  const fs::path base = fresh_dir(name);
  for (std::size_t i = 0; i < cases.size(); ++i) {
    verified_synth s = verified_at(base / std::to_string(i), lay);
    const outcome before = read_only(s.recipe, s.v, cases[i].cell);
    ASSERT_TRUE(before.ok) << cases[i].label << ": lettura prima del guasto fermata: " << before.what;
    cases[i].fault(s);
    expect_stop(read_only(s.recipe, s.v, cases[i].cell), cases[i].code, cases[i].label);
  }
}

TEST(PackIndex, T_M3A_PACK_08_reader_detects_files_changed_after_the_preflight) {
  const auto raw_index = [](verified_synth& s, std::vector<sp::index_entry> e) {
    sp::write_bytes(s.p.dir / sp::index_name, sp::index_bytes(e));
  };
  const std::vector<read_case> cases{
      {"indice riscritto con la stessa dimensione: voce della cella 3 di 3 righe invece di 4", 3,
       [raw_index](verified_synth& s) {
         std::vector<sp::index_entry> e = s.p.index;
         e[3].rows = 3;
         raw_index(s, e);
       },
       "pack.hash_mismatch"},
      {"indice riscritto con la stessa dimensione: voci delle celle 0 e 1 scambiate", 0,
       [raw_index](verified_synth& s) {
         std::vector<sp::index_entry> e = s.p.index;
         std::swap(e[0], e[1]);
         raw_index(s, e);
       },
       "pack.hash_mismatch"},
      {"indice troncato di una voce", 0,
       [](verified_synth& s) {
         const std::string i = sp::index_bytes(s.p.index);
         sp::write_bytes(s.p.dir / sp::index_name, i.substr(0, i.size() - 24));
       },
       "pack.size_mismatch"},
      {"indice con 24 byte in più", 0,
       [](verified_synth& s) { sp::write_bytes(s.p.dir / sp::index_name, sp::index_bytes(s.p.index) + std::string(24, '\0')); },
       "pack.size_mismatch"},
      {"indice rimosso", 0, [](verified_synth& s) { fs::remove(s.p.dir / sp::index_name); }, "pack.file_missing"},
      {"file dati con una riga valida in più, lettura della cella 0", 0,
       [](verified_synth& s) { sp::write_bytes(s.p.dir / sp::data_name, sp::data_bytes(s.p.rows) + sp::encode(s.p.rows.back())); },
       "pack.size_mismatch"},
      {"file dati troncato di una riga, lettura della cella 0", 0,
       [](verified_synth& s) { sp::write_bytes(s.p.dir / sp::data_name, first_rows_bytes(s.p, s.p.rows.size() - 1)); },
       "pack.size_mismatch"},
      {"file dati troncato di una riga, lettura dell'ultima cella", 46,
       [](verified_synth& s) { sp::write_bytes(s.p.dir / sp::data_name, first_rows_bytes(s.p, s.p.rows.size() - 1)); },
       "pack.size_mismatch"},
      {"file dati rimosso", 0, [](verified_synth& s) { fs::remove(s.p.dir / sp::data_name); }, "pack.file_missing"},
      {"directory del pack rimossa", 0, [](verified_synth& s) { fs::remove_all(s.p.dir); }, "pack.missing"},
      {"directory del pack sostituita da un file", 0, [](verified_synth& s) { replace_with_file(s.p.dir); }, "pack.missing"},
      {"ricetta con un altro pack_digest", 0,
       [](verified_synth& s) { s.recipe.pack_digest = sp::sha256_of("un altro manifest"); },
       "pack.digest_mismatch"},
      {"NUL e suffisso dopo la directory della ricetta", 0,
       [](verified_synth& s) { s.recipe.directory = s.p.dir.string() + std::string(1, '\0') + "/assente"; },
       "pack.missing"},
      {"directory della ricetta relativa", 0, [](verified_synth& s) { s.recipe.directory = "pack"; }, "pack.path_not_absolute"},
      {"directory della ricetta vuota", 0, [](verified_synth& s) { s.recipe.directory.clear(); }, "pack.path_not_absolute"},
  };
  run_read_cases("u11reread", cases);
}

// Celle non in coverage, anche minori dell'ultima coperta (4, 20, 22, 45) o fra due coperte: pack.out_of_coverage, mai
// le righe della cella coperta successiva.
TEST(PackIndex, T_M3A_PACK_08_reading_a_cell_outside_coverage_stops_as_out_of_coverage) {
  verified_synth s = verified_at(fresh_dir("u11readcov"));
  for (const std::uint64_t cell : {std::uint64_t{4}, std::uint64_t{20}, std::uint64_t{22}, std::uint64_t{45},
                                   std::uint64_t{47}, std::uint64_t{48}, std::numeric_limits<std::uint64_t>::max()}) {
    expect_stop(read_only(s.recipe, s.v, cell), "pack.out_of_coverage", "cella " + std::to_string(cell));
  }
  const outcome control = read_only(s.recipe, s.v, 21);
  ASSERT_TRUE(control.ok) << control.what;
  expect_rows(control.rows[0], s.p.rows_of(21), "cella 21");
}

// verified_pack incoerente con l'indice e col file dati sul disco (alterato dal chiamante dopo il preflight): l'indice
// riletto ha l'hash del manifest verificato, ma la sua voce non nomina la cella chiesta o esce dal file dati che il
// verified_pack dichiara. Voce incoerente col file dati: pack.index_invalid (§3.4, «indice incoerente»).
TEST(PackIndex, T_M3A_PACK_08_reader_rejects_an_index_entry_inconsistent_with_the_verified_pack) {
  const std::size_t record = sp::record_size(sp::default_schema());
  const auto shrink_data = [record](verified_synth& s, std::size_t rows) {
    sp::write_bytes(s.p.dir / sp::data_name, first_rows_bytes(s.p, rows));
    listed_in(s.v, sp::data_name).size_bytes = rows * record;
  };
  const std::vector<read_case> cases{
      {"coverage del verified_pack con 22 al posto di 21, indice invariato", 22,
       [](verified_synth& s) {
         std::vector<std::uint64_t>& c = s.v.manifest.coverage_cells;
         ASSERT_EQ(c[4], 21u);
         c[4] = 22;
       },
       "pack.index_invalid"},
      {"file dati di 14 righe nel verified_pack e sul disco, voce della cella 46 sulle righe 13-14", 46,
       [shrink_data](verified_synth& s) { shrink_data(s, 14); }, "pack.index_invalid"},
      {"file dati di 12 righe nel verified_pack e sul disco, voce della cella 46 dalla riga 13", 46,
       [shrink_data](verified_synth& s) { shrink_data(s, 12); }, "pack.index_invalid"},
  };
  const fs::path base = fresh_dir("u11forged");
  for (std::size_t i = 0; i < cases.size(); ++i) {
    verified_synth s = verified_at(base / std::to_string(i));
    // La cella 22 non è coperta prima del guasto: il controllo di partenza legge la cella 21.
    const outcome before = read_only(s.recipe, s.v, cases[i].cell == 22 ? 21 : cases[i].cell);
    ASSERT_TRUE(before.ok) << cases[i].label << ": " << before.what;
    cases[i].fault(s);
    expect_stop(read_only(s.recipe, s.v, cases[i].cell), cases[i].code, cases[i].label);
  }
}

// File cambiato fra la fstat del lettore e la lettura: il file dati si accorcia di una riga dopo la fstat (la
// dimensione vista era quella del manifest), l'indice cresce di una voce dopo la prima fstat. Nessuna riga inventata:
// pack.size_mismatch, con la causa della conversione conservata nel contesto per l'indice.
TEST(PackIndex, T_M3A_PACK_08_reader_detects_a_file_changed_between_fstat_and_read) {
  const fs::path base = fresh_dir("u11race");
  {
    verified_synth s = verified_at(base / "dati");
    const fs::path data = s.p.dir / sp::data_name;
    const auto shorter = static_cast<off_t>(sp::record_size(sp::default_schema()) * (s.p.rows.size() - 1));
    bool truncated = false;
    outcome o;
    {
      const stat_hook::arm hook(data, [&] { truncated = ::truncate(data.c_str(), shorter) == 0; });
      o = read_only(s.recipe, s.v, 46);
      ASSERT_TRUE(hook.fired_once()) << "nessuna fstat sul file dati";
    }
    ASSERT_TRUE(truncated) << "troncamento del file dati fallito";
    expect_stop(o, "pack.size_mismatch", "file dati accorciato dopo la fstat, lettura dell'ultima cella");
  }
  {
    verified_synth s = verified_at(base / "indice");
    const fs::path index = s.p.dir / sp::index_name;
    const auto longer = static_cast<off_t>(24 * (s.p.index.size() + 1));
    bool grown = false;
    outcome o;
    {
      const stat_hook::arm hook(index, [&] { grown = ::truncate(index.c_str(), longer) == 0; });
      o = read_only(s.recipe, s.v, 0);
      ASSERT_TRUE(hook.fired_once()) << "nessuna fstat sull'indice";
    }
    ASSERT_TRUE(grown) << "allungamento dell'indice fallito";
    expect_stop(o, "pack.size_mismatch", "indice cresciuto dopo la prima fstat");
    bool cause = false;
    for (const auto& [key, value] : o.context) cause = cause || (key == "causa" && !value.empty());
    EXPECT_TRUE(cause) << "causa della conversione assente dal contesto: " << o.what;
  }
}

// §3.6: un file elencato sostituito da una directory è pack.file_missing, anche quando il cambio avviene dopo il
// preflight e lo trova il lettore.
TEST(PackIndex, T_M3A_PACK_08_reader_listed_file_replaced_by_a_directory_is_file_missing) {
  const std::vector<read_case> cases{
      {"indice sostituito da una directory", 0, [](verified_synth& s) { replace_with_directory(s.p.dir / sp::index_name); },
       "pack.file_missing"},
      {"file dati sostituito da una directory", 0, [](verified_synth& s) { replace_with_directory(s.p.dir / sp::data_name); },
       "pack.file_missing"},
  };
  run_read_cases("u11readdir", cases);
}

// Indice e file dati in una sottodirectory del pack; dopo il preflight la sottodirectory diventa un file regolare:
// il file elencato non esiste più (ENOTDIR dal sistema, un'assenza, non un errore di I/O): pack.file_missing (§3.4).
void move_listed(sp::synth& p, const std::string& from, const std::string& to) {
  fs::create_directories((p.dir / to).parent_path());
  fs::rename(p.dir / from, p.dir / to);
  std::vector<sp::jv>& files = p.manifest.at("files").items;
  for (sp::jv& f : files) {
    if (f.at("path").s == from) f.set("path", sp::jv::str(to));
  }
  std::sort(files.begin(), files.end(), [](const sp::jv& a, const sp::jv& b) { return a.at("path").s < b.at("path").s; });
  for (const char* key : {"data_file", "index_file"}) {
    if (p.manifest.at(key).s == from) p.manifest.set(key, sp::jv::str(to));
  }
  p.write_manifest();
}

TEST(PackIndex, T_M3A_PACK_08_reader_listed_file_under_a_directory_replaced_by_a_file_is_file_missing) {
  const fs::path base = fresh_dir("u11readnotdir");
  for (const auto& [file, sub] : {std::pair{std::string(sp::index_name), std::string("idx")},
                                  std::pair{std::string(sp::data_name), std::string("dat")}}) {
    SCOPED_TRACE(sub + "/" + file);
    sp::synth p = sp::make_pack(base / sub);
    move_listed(p, file, sub + "/" + file);
    const pack_recipe r = recipe_of(p);
    const verified_pack v = verify_pack(r, astrometry_use(), audit_of(p));
    const outcome before = read_only(r, v, 3);
    ASSERT_TRUE(before.ok) << before.what;
    expect_rows(before.rows[0], p.rows_of(3), "cella 3 prima del guasto");
    replace_with_file(p.dir / sub);
    expect_stop(read_only(r, v, 3), "pack.file_missing", "sottodirectory " + sub + " sostituita da un file");
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// T-M3A-PACK-06: copertura (§3.3 punto 7). Oracolo fissato in plans/M3a.md prima della misura.
// ---------------------------------------------------------------------------------------------------------------------

sky_cap cap(double ra_deg, double dec_deg, double radius_deg) {
  sky_cap c;
  c.ra_deg = ra_deg;
  c.dec_deg = dec_deg;
  c.radius_deg = radius_deg;
  return c;
}

verified_pack covered_pack(int order, const std::vector<std::uint64_t>& cells) {
  verified_pack v;
  v.manifest.coverage_order = static_cast<std::uint32_t>(order);
  v.manifest.index_order = static_cast<std::uint32_t>(order);
  v.manifest.coverage_cells = cells;
  return v;
}

std::vector<std::uint64_t> all_but(int order, const std::set<std::uint64_t>& holes) {
  std::vector<std::uint64_t> out;
  for (std::uint64_t c = 0; c < ho::cells(order); ++c) {
    if (holes.count(c) == 0) out.push_back(c);
  }
  return out;
}

ho::region_probe probe_of(int order, const std::vector<std::uint64_t>& cells) {
  static const std::vector<ho::vec3> samples = ho::fibonacci_sphere(100000);
  ho::region_probe pr;
  pr.order = order;
  pr.covered.assign(ho::cells(order), false);
  for (const std::uint64_t c : cells) pr.covered[c] = true;
  pr.r_lower = ho::max_center_distance(order, samples);
  pr.r_upper = ho::max_cell_radius(order);
  pr.step = 0.5 * ho::deg;
  return pr;
}

const char* class_name(ho::region_class c) {
  switch (c) {
    case ho::region_class::exterior: return "esterna";
    case ho::region_class::straddling: return "a cavallo";
    case ho::region_class::edge: return "bordo allargato";
    case ho::region_class::inside: return "interna";
    case ho::region_class::undecided: return "indecisa";
  }
  return "?";
}

// Esito binario atteso per classe: interna passa; esterna, a cavallo e bordo allargato: pack.out_of_coverage.
void expect_coverage_outcome(const verified_pack& v, const sky_cap& region, ho::region_class cls,
                             const std::string& label) {
  SCOPED_TRACE(label + " (" + class_name(cls) + ")");
  ASSERT_NE(cls, ho::region_class::undecided);
  if (cls == ho::region_class::inside) {
    EXPECT_NO_THROW(verify_pack_coverage(v, region));
    return;
  }
  try {
    verify_pack_coverage(v, region);
    ADD_FAILURE() << "falso accetto: regione " << class_name(cls) << " accettata";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::incompatible) << e.what();
    EXPECT_EQ(e.code(), std::string("pack.out_of_coverage")) << e.what();
  }
}

// Ordine 1, tutto il cielo tranne le celle di base 10 e 11 (celle 40-47) e la cella 20 (figlio sud della cella di
// base equatoriale 5): ogni caso ha la classe scritta a mano e confermata dall'oracolo.
TEST(PackIndex, T_M3A_PACK_06_named_regions_inside_straddling_outside_and_at_the_enlarged_edge) {
  const std::vector<std::uint64_t> cells = all_but(1, {20, 40, 41, 42, 43, 44, 45, 46, 47});
  const verified_pack v = covered_pack(1, cells);
  const ho::region_probe pr = probe_of(1, cells);
  struct named {
    std::string label;
    sky_cap region;
    ho::region_class expected;
  };
  const std::vector<named> cases{
      {"polo nord, raggio 10°", cap(0, 90, 10), ho::region_class::inside},
      {"(300°, -70°), raggio 3°, dentro la cella di base 11", cap(300, -70, 3), ho::region_class::exterior},
      {"(300°, -70°), raggio 0", cap(300, -70, 0), ho::region_class::exterior},
      {"(292.5°, -19.47°), raggio 5°, sul lato fra le celle di base 7 e 11", cap(292.5, -19.47, 5),
       ho::region_class::straddling},
      {"polo nord, raggio 120°", cap(0, 90, 120), ho::region_class::straddling},
      {"(90°, 15°), raggio 5°, coperta, cella 20 entro un raggio massimo", cap(90, 15, 5), ho::region_class::edge},
      {"(315°, 20°), raggio 5°, coperta, cella di base 11 entro un raggio massimo", cap(315, 20, 5),
       ho::region_class::edge},
  };
  for (const named& c : cases) {
    const ho::region_class cls =
        pr.classify(ho::from_radec_deg(c.region.ra_deg, c.region.dec_deg), c.region.radius_deg * ho::deg);
    EXPECT_EQ(cls, c.expected) << c.label << ": l'oracolo dà " << class_name(cls);
    expect_coverage_outcome(v, c.region, c.expected, c.label);
  }
}

// Calotte con seed fisso a ordine 1 e 2, ciascuna classificata dall'oracolo; criterio binario per classe, nessun falso
// accetto. Le indecise non hanno esito atteso e si contano soltanto.
TEST(PackIndex, T_M3A_PACK_06_sampled_regions_at_orders_one_and_two_follow_the_oracle) {
  struct coverage_case {
    int order;
    std::set<std::uint64_t> holes;
    std::uint64_t seed;
  };
  std::set<std::uint64_t> holes2{5, 6, 77, 100, 101, 102, 103};
  for (std::uint64_t c = 144; c < 176; ++c) holes2.insert(c);
  const std::vector<coverage_case> setups{
      {1, {20, 40, 41, 42, 43, 44, 45, 46, 47}, 0xC0FE0007ULL},
      {2, holes2, 0xC0FE0008ULL},
  };
  constexpr int regions = 240;
  constexpr int at_least = 8;
  for (const coverage_case& s : setups) {
    SCOPED_TRACE("ordine " + std::to_string(s.order));
    const std::vector<std::uint64_t> cells = all_but(s.order, s.holes);
    const verified_pack v = covered_pack(s.order, cells);
    const ho::region_probe pr = probe_of(s.order, cells);
    sp::splitmix g(s.seed);
    std::map<ho::region_class, int> seen;
    for (int i = 0; i < regions; ++i) {
      const double z = 2 * g.unit() - 1, phi = 2 * ho::pi * g.unit();
      const double radius = 0.5 + 19.5 * g.unit();
      const ho::vec3 center = ho::from_z_phi(z, phi);
      const sky_cap region = cap(phi / ho::deg, std::asin(z) / ho::deg, radius);
      const ho::region_class cls = pr.classify(center, radius * ho::deg);
      ++seen[cls];
      if (cls != ho::region_class::undecided) expect_coverage_outcome(v, region, cls, "regione " + std::to_string(i));
    }
    for (const ho::region_class c : {ho::region_class::exterior, ho::region_class::straddling, ho::region_class::edge,
                                     ho::region_class::inside}) {
      EXPECT_GE(seen[c], at_least) << "classe " << class_name(c) << " poco rappresentata";
    }
  }
}

TEST(PackIndex, T_M3A_PACK_06_full_coverage_accepts_any_region_up_to_the_whole_sphere) {
  for (const int order : {0, 1}) {
    SCOPED_TRACE(order);
    const verified_pack v = covered_pack(order, all_but(order, {}));
    for (const sky_cap& r : {cap(0, 90, 0), cap(123, -45, 90), cap(10, 10, 180), cap(359.9, -89.9, 30)}) {
      EXPECT_NO_THROW(verify_pack_coverage(v, r)) << r.ra_deg << " " << r.dec_deg << " " << r.radius_deg;
    }
  }
}

// Regione non valida: mai un accetto, nemmeno con copertura totale: invalid_input pack.region_invalid (§3.6).
TEST(PackIndex, T_M3A_PACK_06_invalid_region_never_passes) {
  const verified_pack v = covered_pack(1, all_but(1, {}));
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  for (const sky_cap& r : {cap(nan, 0, 1), cap(0, nan, 1), cap(0, 0, nan), cap(inf, 0, 1), cap(0, 0, inf),
                           cap(0, 0, -1), cap(0, 90.5, 1), cap(0, -91, 1)}) {
    SCOPED_TRACE(std::to_string(r.ra_deg) + " " + std::to_string(r.dec_deg) + " " + std::to_string(r.radius_deg));
    try {
      verify_pack_coverage(v, r);
      ADD_FAILURE() << "regione non valida accettata";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.category(), error_category::invalid_input) << e.what();
      EXPECT_EQ(e.code(), std::string("pack.region_invalid")) << e.what();
    }
  }
}

// Dal pack generato al controllo di copertura: la `coverage` letta da verify_pack è quella che il controllo usa.
TEST(PackIndex, T_M3A_PACK_06_coverage_of_a_generated_pack_after_verify_pack) {
  sp::layout lay;
  lay.order = 1;
  for (std::uint64_t c = 0; c < 40; ++c) lay.cells.push_back({c, 1});
  const sp::synth p = sp::make_pack(fresh_dir("u11coverage"), lay);
  const verified_pack v = verify_pack(recipe_of(p), astrometry_use(), audit_of(p));
  const ho::region_probe pr = probe_of(1, p.coverage());
  const auto check = [&](const sky_cap& r, ho::region_class expected, const std::string& label) {
    EXPECT_EQ(pr.classify(ho::from_radec_deg(r.ra_deg, r.dec_deg), r.radius_deg * ho::deg), expected) << label;
    expect_coverage_outcome(v, r, expected, label);
  };
  check(cap(0, 90, 10), ho::region_class::inside, "polo nord, raggio 10°");
  check(cap(300, -70, 3), ho::region_class::exterior, "(300°, -70°), raggio 3°");
  check(cap(315, 20, 5), ho::region_class::edge, "(315°, 20°), raggio 5°");
}

// Celle NESTED dei primi `rings` anelli attorno a un polo (Górski et al. 2005 §4): nella cella di base polare f
// l'anello i contato dal polo è ix + iy = 2N - 1 - i al nord (f = 0..3) e ix + iy = i - 1 al sud (f = 8..11), con i
// celle per cella di base; indice NESTED f N² + ix nei bit pari + iy nei bit dispari.
std::vector<std::uint64_t> polar_rings(int order, std::uint64_t rings, bool north) {
  const std::uint64_t n = ho::nside(order);
  std::vector<std::uint64_t> out;
  for (std::uint64_t f = 0; f < 4; ++f) {
    const std::uint64_t face = north ? f : 8 + f;
    for (std::uint64_t i = 1; i <= rings; ++i) {
      for (std::uint64_t k = 0; k < i; ++k) {
        const std::uint64_t ix = north ? n - i + k : k;
        const std::uint64_t iy = north ? 2 * n - 1 - i - ix : i - 1 - ix;
        out.push_back(face * n * n + ho::interleave(ix, iy));
      }
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

// Il generatore dei primi anelli polari coincide con le celle che l'oracolo assegna ai centri d'anello, dove
// l'oracolo è preciso (ordine 3, tutti gli anelli della calotta).
TEST(PackIndex, T_M3A_PACK_06_polar_ring_generator_matches_the_oracle_at_order_three) {
  constexpr int order = 3;
  const double n = static_cast<double>(ho::nside(order));
  for (const bool north : {true, false}) {
    for (std::uint64_t rings = 1; rings < ho::nside(order); ++rings) {
      SCOPED_TRACE(std::string(north ? "nord" : "sud") + ", anelli " + std::to_string(rings));
      std::set<std::uint64_t> want;
      for (std::uint64_t i = 1; i <= rings; ++i) {
        const double di = static_cast<double>(i);
        const double z = 1.0 - di * di / (3.0 * n * n);
        for (std::uint64_t j = 1; j <= 4 * i; ++j) {
          want.insert(ho::ang2pix_nest(order, north ? z : -z, ho::pi / (2.0 * di) * (static_cast<double>(j) - 0.5)));
        }
      }
      const std::vector<std::uint64_t> got = polar_rings(order, rings, north);
      EXPECT_EQ(std::set<std::uint64_t>(got.begin(), got.end()), want);
      EXPECT_EQ(got.size(), 2 * rings * (rings + 1));
    }
  }
}

// Regressione della precisione polare all'ordine massimo 29: in z = cos(colatitudine) i primi sei anelli cadono sul
// polo (1 - z < 2^-54). Coperti solo i sei anelli attorno al polo (84 celle), la regione di raggio 0 sta a 3/4 di
// raggio massimo dal centro di una cella del settimo anello, non coperta: quella cella interseca la regione allargata
// del raggio massimo, stop. Controllo: dal polo il primo anello scoperto (il tredicesimo) dista più di 4 raggi
// massimi e la regione passa. Colatitudine del centro dell'anello i: 1 - cos θ = i²/(3N²), cioè sin(θ/2) = i/(√6 N).
TEST(PackIndex, T_M3A_PACK_06_polar_regions_at_order_29_keep_their_precision) {
  constexpr int order = 29;
  const double n = static_cast<double>(ho::nside(order));
  const double r_max = ho::max_cell_radius(order);
  const auto theta = [n](double i) { return 2.0 * std::asin(i / (std::sqrt(6.0) * n)); };
  for (const bool north : {true, false}) {
    SCOPED_TRACE(north ? "polo nord" : "polo sud");
    const double sign = north ? 1.0 : -1.0;
    const auto at = [sign](double colat, double phi) {
      return ho::vec3{std::sin(colat) * std::cos(phi), std::sin(colat) * std::sin(phi), sign * std::cos(colat)};
    };
    const double phi = ho::pi / 28.0;  // anello 7, j = 1
    const double colat = theta(7) - 0.75 * r_max;
    const double ra = phi / ho::deg, dec = sign * (90.0 - colat / ho::deg);
    const ho::vec3 region{std::cos(dec * ho::deg) * std::cos(ra * ho::deg), std::cos(dec * ho::deg) * std::sin(ra * ho::deg),
                          std::sin(dec * ho::deg)};
    ASSERT_LT(ho::angle(region, at(theta(7), phi)), 0.8 * r_max);
    ASSERT_GT(ho::angle(region, at(theta(7), phi)), 0.7 * r_max);
    const verified_pack six = covered_pack(order, polar_rings(order, 6, north));
    ASSERT_EQ(six.manifest.coverage_cells.size(), 84u);
    expect_coverage_outcome(six, cap(ra, dec, 0), ho::region_class::edge, "3/4 di raggio massimo dal settimo anello");

    ASSERT_GT(theta(13), 4 * r_max);
    const verified_pack twelve = covered_pack(order, polar_rings(order, 12, north));
    expect_coverage_outcome(twelve, cap(0, sign * 90, 0), ho::region_class::inside, "polo, dodici anelli coperti");
  }
}

// Regressione della periodicità della RA: RA finite grandi (k giri più l'angolo, somma esatta in double) danno lo
// stesso esito dell'angolo ridotto, classificato dall'oracolo. Copertura di ordine 1 senza la cella di base 4 (16-19).
TEST(PackIndex, T_M3A_PACK_06_large_finite_right_ascensions_are_periodic) {
  const std::vector<std::uint64_t> cells = all_but(1, {16, 17, 18, 19});
  const verified_pack v = covered_pack(1, cells);
  const ho::region_probe pr = probe_of(1, cells);
  const std::vector<std::array<double, 3>> regions{{0, 0, 0},     {350, 5, 1},  {10, 30, 2},       {30, -10, 4},
                                                   {315, 20, 5},  {180, 0, 0},  {292.5, -19.47, 5}, {200, -40, 10},
                                                   {120, 60, 5},  {240, 10, 8}, {160, -60, 5}};
  const std::vector<double> turns{1e6, -1e6, 1073741824.0, -1073741824.0, 68719476736.0};
  std::set<bool> stops_seen;
  for (const auto& [ra, dec, radius] : regions) {
    const std::string label = "(" + std::to_string(ra) + ", " + std::to_string(dec) + "), raggio " + std::to_string(radius);
    const ho::region_class cls = pr.classify(ho::from_radec_deg(ra, dec), radius * ho::deg);
    ASSERT_NE(cls, ho::region_class::undecided) << label;
    stops_seen.insert(cls != ho::region_class::inside);
    expect_coverage_outcome(v, cap(ra, dec, radius), cls, label);
    for (const double k : turns) {
      const double big = ra + k * 360.0;
      ASSERT_EQ(big - k * 360.0, ra) << label << ": somma non esatta per " << k << " giri";
      expect_coverage_outcome(v, cap(big, dec, radius), cls, label + " più " + std::to_string(k) + " giri");
    }
  }
  EXPECT_EQ(stops_seen.size(), 2u) << "servono regioni che passano e regioni che si fermano";
  // Giri oltre 2^53 gradi: RA esattamente multiple di 360 (o di 360 più 180), angolo ridotto esatto con fmod.
  const double two53 = 9007199254740992.0;
  for (const auto& [big, reduced] : {std::pair{360.0 * two53, 0.0}, std::pair{-360.0 * two53 * 128.0, 0.0},
                                     std::pair{360.0 * 35184372088832.0 + 180.0, 180.0}}) {
    ASSERT_EQ(std::fabs(std::fmod(big, 360.0)), reduced) << big;
    const ho::region_class cls = pr.classify(ho::from_radec_deg(reduced, 0), 0);
    ASSERT_NE(cls, ho::region_class::undecided);
    expect_coverage_outcome(v, cap(big, 0, 0), cls, "RA " + std::to_string(big) + " (ridotta " + std::to_string(reduced) + ")");
  }
}

// Una sola cella scoperta all'ordine 6, il cui centro sta a R + r/2 dal centro della regione (r raggio massimo): il
// centro è un punto della cella dentro la regione allargata di r, quindi stop. La ricerca gerarchica non deve
// scartare il ramo che porta a quella cella vicino al bordo della sua portata.
TEST(PackIndex, T_M3A_PACK_06_a_single_uncovered_cell_near_the_reach_at_order_six_stops) {
  constexpr int order = 6;
  const std::vector<ho::vec3> centers = ho::nest_centers(order);
  const double r_max = ho::max_cell_radius(order);
  for (const auto& [z, phi] : {std::pair{0.3, 1.0}, std::pair{0.8, 4.0}, std::pair{-0.5, 2.5}}) {
    const std::uint64_t hole = ho::ang2pix_nest(order, z, phi);
    SCOPED_TRACE("cella scoperta " + std::to_string(hole));
    const std::vector<std::uint64_t> cells = all_but(order, {hole});
    const verified_pack v = covered_pack(order, cells);
    ho::region_probe pr = probe_of(order, cells);
    pr.step = 0.05 * ho::deg;
    const ho::vec3 c = ho::unit(centers[hole]);
    const ho::vec3 east = ho::unit(ho::cross(ho::vec3{0, 0, 1}, c));
    const double radius = 3 * ho::deg;
    const double d = radius + 0.5 * r_max;
    ASSERT_LT(0.5 * r_max, pr.r_lower);
    const ho::vec3 p = std::cos(d) * c + std::sin(d) * east;
    const double ra = ho::phi_of(p) / ho::deg, dec = std::asin(ho::z_of(p)) / ho::deg;
    const ho::region_class cls = pr.classify(ho::from_radec_deg(ra, dec), radius);
    ASSERT_TRUE(cls == ho::region_class::edge || cls == ho::region_class::straddling) << class_name(cls);
    expect_coverage_outcome(v, cap(ra, dec, radius / ho::deg), cls, "centro della cella a R + r/2");
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Casi aperti di §3.6.
// ---------------------------------------------------------------------------------------------------------------------

class cwd_guard {
 public:
  explicit cwd_guard(const fs::path& to) : saved_(fs::current_path()) { fs::current_path(to); }
  ~cwd_guard() {
    std::error_code ec;
    fs::current_path(saved_, ec);
    if (ec) ADD_FAILURE() << "ripristino della directory corrente fallito: " << ec.message();
  }
  cwd_guard(const cwd_guard&) = delete;
  cwd_guard& operator=(const cwd_guard&) = delete;

 private:
  fs::path saved_;
};

// Esito di verify_pack con le aperture osservate durante la chiamata.
outcome verify_observed(const pack_recipe& r, const pack_audit& a, std::vector<std::string>& opened) {
  open_probe::window w;
  outcome o = verify_only(r, a);
  opened = w.seen();
  return o;
}

std::string listed(const std::vector<std::string>& opened) {
  std::string out;
  for (const std::string& o : opened) out += "\n  " + o;
  return out;
}

// §3.2: la ricetta contiene un solo percorso assoluto. Uno relativo si ferma prima di ogni apertura (nessuna apertura
// osservata durante la chiamata): con la directory corrente sopra un pack valido non passa, sopra una directory vuota
// non diventa pack.missing.
TEST(PackIndex, T_M3A_PACK_02_relative_recipe_directory_stops_before_opening_anything) {
  const fs::path base = fresh_dir("u11relative");
  const sp::synth p = sp::make_pack(base);
  const fs::path empty = base / "vuota";
  fs::create_directories(empty);
  pack_recipe r = recipe_of(p);
  {
    cwd_guard at(base);
    for (const std::string& dir : {std::string("pack"), std::string("./pack"), std::string("pack/"),
                                  "../" + base.filename().string() + "/pack"}) {
      r.directory = dir;
      std::vector<std::string> opened;
      expect_stop(verify_observed(r, audit_of(p), opened), "pack.path_not_absolute", "ricetta '" + dir + "' su un pack valido");
      EXPECT_TRUE(opened.empty()) << "ricetta '" << dir << "', aperture:" << listed(opened);
    }
  }
  {
    cwd_guard at(empty);
    r.directory = "pack";
    std::vector<std::string> opened;
    expect_stop(verify_observed(r, audit_of(p), opened), "pack.path_not_absolute", "ricetta 'pack' su una directory vuota");
    EXPECT_TRUE(opened.empty()) << "aperture:" << listed(opened);
  }
  r.directory = "";
  std::vector<std::string> opened;
  expect_stop(verify_observed(r, audit_of(p), opened), "pack.path_not_absolute", "ricetta con percorso vuoto");
  EXPECT_TRUE(opened.empty()) << "aperture:" << listed(opened);
  r.directory = p.dir.string();
  const outcome control = verify_observed(r, audit_of(p), opened);
  EXPECT_TRUE(control.ok) << control.what;
  EXPECT_FALSE(opened.empty()) << "l'osservatore non vede le aperture del pack valido";
}

// `audit.evidence_root` vuoto o relativo (§3.6): la radice delle evidenze è un percorso assoluto; una vuota non deve
// diventare la radice del filesystem ("/docs/datasets/…") né una relativa la directory corrente. Lo stop viene prima
// di ogni apertura: durante la chiamata nessuna apertura osservata, nemmeno dentro il pack.
TEST(PackIndex, T_M3A_LIC_01_empty_or_relative_evidence_root_stops_as_path_not_absolute) {
  const fs::path base = fresh_dir("u11evroot");
  const sp::synth p = sp::make_pack(base);
  pack_audit a;
  a.evidence_root = "";
  std::vector<std::string> opened;
  expect_stop(verify_observed(recipe_of(p), a, opened), "pack.path_not_absolute", "radice delle evidenze vuota");
  EXPECT_TRUE(opened.empty()) << "radice vuota, aperture:" << listed(opened);
  {
    cwd_guard at(base);
    a.evidence_root = "ev";
    expect_stop(verify_observed(recipe_of(p), a, opened), "pack.path_not_absolute", "radice 'ev' relativa a un record valido");
    EXPECT_TRUE(opened.empty()) << "radice 'ev', aperture:" << listed(opened);
  }
  const outcome control = verify_only(recipe_of(p), audit_of(p));
  EXPECT_TRUE(control.ok) << control.what;
}

TEST(PackIndex, T_M3A_PACK_04_empty_flag_bits_stops_as_manifest_invalid) {
  sp::synth p = sp::make_pack(fresh_dir("u11flagbits"));
  p.manifest.set("flag_bits", sp::jv::arr());
  p.write_manifest();
  expect_stop(verify_only(recipe_of(p), audit_of(p)), "pack.manifest_invalid", "flag_bits: []");
}

// ---------------------------------------------------------------------------------------------------------------------
// T-M3A-PACK-09: fuzz con seed fisso del lettore dell'indice e del lettore delle righe, 5 000 mutazioni per file seme.
// L'esito atteso di ogni mutazione lo calcola il test dai byte mutati (oracoli sotto, indipendenti dal prodotto): pack
// valido letto per intero con le righe attese, oppure stop con il codice atteso; ASan/UBSan nella build dev.
// ---------------------------------------------------------------------------------------------------------------------

std::string mutate(const std::string& seed, sp::splitmix& g) {
  std::string s = seed;
  const std::uint64_t steps = 1 + g.below(3);
  for (std::uint64_t k = 0; k < steps; ++k) {
    switch (g.below(6)) {
      case 0:
        if (!s.empty()) {
          const std::size_t at = g.below(s.size());
          s[at] = static_cast<char>(s[at] ^ (1u << g.below(8)));
        }
        break;
      case 1:
        if (!s.empty()) s[g.below(s.size())] = static_cast<char>(g.below(256));
        break;
      case 2:
        if (!s.empty()) s.erase(g.below(s.size()), 1 + g.below(8));
        break;
      case 3: {
        std::string ins;
        for (std::uint64_t n = 0, c = 1 + g.below(8); n < c; ++n) ins += static_cast<char>(g.below(256));
        s.insert(s.empty() ? 0 : g.below(s.size() + 1), ins);
        break;
      }
      case 4:
        if (!s.empty()) {
          const std::size_t at = g.below(s.size());
          s.insert(at, s.substr(at, 1 + g.below(24)));
        }
        break;
      default:
        s.resize(g.below(s.size() + 1));
        break;
    }
  }
  return s;
}

struct fuzz_tally {
  int accepted = 0;
  std::map<std::string, int> stops;
  int stopped() const {
    int n = 0;
    for (const auto& [code, k] : stops) n += k;
    return n;
  }
};

// Esito atteso di una mutazione: righe attese per cella se il pack è valido, altrimenti i codici ammessi; nessuna
// attesa se il contratto non decide il caso.
struct expectation {
  bool decided = true;
  std::set<std::string> codes;             // vuoto: pack valido
  std::vector<std::vector<sp::row>> rows;  // per cella di coverage, se valido
};

std::uint64_t u64_at(const std::string& b, std::size_t at) {
  std::uint64_t v = 0;
  for (std::size_t i = 8; i-- > 0;) v = (v << 8) | static_cast<unsigned char>(b[at + i]);
  return v;
}

// §3.3.5 sull'indice mutato (dati invariati): 24 byte per cella, celle di coverage in ordine, intervalli contigui
// dalla riga 0 entro le righe del file, ultima riga uguale al count. Un indice non valido può fermarsi su uno dei due
// codici: il contratto non fissa l'ordine dei controlli.
expectation index_oracle(const sp::synth& p, const std::string& index) {
  expectation e;
  const std::vector<std::uint64_t> cells = p.coverage();
  const std::uint64_t total = p.rows.size();
  const expectation invalid{true, {"pack.index_invalid", "pack.count_mismatch"}, {}};
  if (index.size() != 24 * cells.size()) return invalid;
  std::uint64_t next = 0;
  for (std::size_t i = 0; i < cells.size(); ++i) {
    const std::uint64_t cell = u64_at(index, 24 * i), first = u64_at(index, 24 * i + 8), n = u64_at(index, 24 * i + 16);
    if (cell != cells[i] || first != next || n > total - first) return invalid;
    e.rows.emplace_back(p.rows.begin() + static_cast<std::ptrdiff_t>(first),
                        p.rows.begin() + static_cast<std::ptrdiff_t>(first + n));
    next = first + n;
  }
  if (next != total) return invalid;
  return e;
}

enum class coherence { valid, invalid };

// §3.1 su una riga dello schema di base: bit non dichiarati (dichiarati 0, 1, 2, 3), ra e dec finiti, assenza se e
// solo se NaN; un infinito è incoerente in ogni colonna (decisione del coordinatore, 30 ottobre 2026).
coherence row_coherence(const sp::row& r) {
  const std::uint32_t declared = sp::j_null_bit | sp::h_null_bit | sp::saturated_bit | sp::pm_null_bit;
  if ((r.flags & ~declared) != 0) return coherence::invalid;
  if (!std::isfinite(sp::from64(r.ra)) || !std::isfinite(sp::from64(r.dec))) return coherence::invalid;
  for (const auto& [v, bit] : {std::pair{sp::from32(r.j), sp::j_null_bit}, std::pair{sp::from32(r.h), sp::h_null_bit},
                               std::pair{sp::from64(r.pm), sp::pm_null_bit}}) {
    const bool absent = (r.flags & bit) != 0;
    if (absent != std::isnan(v) || std::isinf(v)) return coherence::invalid;
  }
  return coherence::valid;
}

// File dati mutato (indice e conteggi invariati): dimensione diversa: count_mismatch; una riga incoerente:
// row_invalid (ogni cella è letta, quindi ogni riga); altrimenti righe decodificate dai byte mutati.
expectation rows_oracle(const sp::synth& p, const std::string& data) {
  const std::size_t rec = sp::record_size(p.lay.cols);
  if (data.size() != rec * p.rows.size()) return expectation{true, {"pack.count_mismatch"}, {}};
  std::vector<sp::row> rows;
  for (std::size_t i = 0; i < p.rows.size(); ++i) {
    rows.push_back(sp::decode(data, i * rec, p.lay.cols));
    if (row_coherence(rows.back()) == coherence::invalid) return expectation{true, {"pack.row_invalid"}, {}};
  }
  expectation e;
  for (const sp::index_entry& x : p.index) {
    e.rows.emplace_back(rows.begin() + static_cast<std::ptrdiff_t>(x.first),
                        rows.begin() + static_cast<std::ptrdiff_t>(x.first + x.rows));
  }
  return e;
}

void fuzz_file(sp::synth& p, const char* file, std::uint64_t seed, fuzz_tally& tally,
               const std::function<expectation(const sp::synth&, const std::string&)>& oracle) {
  const std::string original = sp::read_bytes(p.dir / file);
  sp::splitmix g(seed);
  for (int i = 0; i < fuzz_iterations; ++i) {
    const std::string mutated = mutate(original, g);
    p.rewrite(file, mutated);
    const expectation want = oracle(p, mutated);
    const outcome o = verify_and_read(p, p.coverage());
    if (o.ok) {
      ++tally.accepted;
    } else {
      ++tally.stops[o.code];
    }
    if (!want.decided) continue;
    std::string label = "mutazione " + std::to_string(i) + ", attesi:";
    for (const std::string& c : want.codes) label += " " + c;
    if (want.codes.empty()) {
      ASSERT_TRUE(o.ok) << label << ": pack valido fermato: " << o.what;
      for (std::size_t c = 0; c < want.rows.size(); ++c) expect_rows(o.rows[c], want.rows[c], label + ", cella " + std::to_string(c));
    } else {
      expect_stop_among(o, want.codes, label);
    }
  }
}

TEST(PackIndex, T_M3A_PACK_09_fuzz_of_the_index_reader_stops_or_reads_every_cell) {
  sp::synth p = sp::make_pack(fresh_dir("u11fuzzindex"));
  fuzz_tally tally;
  fuzz_file(p, sp::index_name, 0xF0221109ULL, tally, index_oracle);
  EXPECT_EQ(tally.accepted + tally.stopped(), fuzz_iterations);
  EXPECT_GT(tally.stops["pack.index_invalid"], 0) << "il fuzz non esercita il lettore dell'indice";
}

TEST(PackIndex, T_M3A_PACK_09_fuzz_of_the_row_reader_stops_or_reads_every_cell) {
  sp::synth p = sp::make_pack(fresh_dir("u11fuzzrows"));
  fuzz_tally tally;
  fuzz_file(p, sp::data_name, 0xF0221110ULL, tally, rows_oracle);
  EXPECT_EQ(tally.accepted + tally.stopped(), fuzz_iterations);
  EXPECT_GT(tally.stops["pack.row_invalid"], 0) << "il fuzz non esercita il controllo delle righe";
  EXPECT_GT(tally.stops["pack.count_mismatch"], 0) << "il fuzz non esercita il conteggio delle righe";
}

}  // namespace
