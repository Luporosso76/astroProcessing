// R1: job di import, thumbnail e cache.rebuild su store SQLite durevole (ADR-04 e ADR-05, aggiunte di R1).
// T-M2R1-04, T-M2R1-06, T-M2R1-07 (import), T-M2R1-08, T-M2R1-11, T-M2R1-12.
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <set>

#include "astro/hash.hpp"
#include "astro/import_job.hpp"
#include "astro/json.hpp"
#include "astro/operations.hpp"
#include "import_support.hpp"
#include "store_test_support.hpp"

namespace {

using namespace imp;
using store_test::raw_db;

const cancel_token& never() {
  static const cancel_token token;
  return token;
}

struct project {
  fs::path dir;
  std::unique_ptr<sqlite_dag_store> store;
};

project make_project(const std::string& name) {
  project p;
  p.dir = fresh(name);
  p.store = sqlite_dag_store::create(p.dir / "project");
  return p;
}

void add_slot(project& p, const std::string& slot, const node_spec& node) {
  p.store->put_node(node);
  p.store->create_slot(slot, node.key());
}

struct source {
  fs::path path;
  fx::bytes bytes;
  content_digest digest;
};

source write_source(const fs::path& dir, const std::string& name, const gen& g, bool invalid = false) {
  source s;
  s.path = dir / name;
  s.bytes = make_fits(g, invalid);
  write_file(s.path, s.bytes);
  s.digest = digest_of(s.bytes);
  return s;
}

gen thumb_gen(std::vector<std::string> cards = {}) {
  gen g;
  g.bitpix = -32;
  g.w = 80;
  g.h = 40;
  g.cards = std::move(cards);
  g.value = [](std::uint32_t, std::uint32_t x, std::uint32_t y) { return ((x * 131 + y * 17) % 1013) / 7.0; };
  return g;
}

template <class F>
auto quiet(F&& f) {
  testing::internal::CaptureStderr();
  try {
    auto r = f();
    testing::internal::GetCapturedStderr();
    return r;
  } catch (...) {
    testing::internal::GetCapturedStderr();
    throw;
  }
}

std::string ctx(const stop_error& e, const std::string& key) {
  for (const auto& [k, v] : e.context()) {
    if (k == key) return v;
  }
  return "";
}

// Provenance persistita di un job: file di ruolo `provenance` del nodo, generazione del job.
json persisted_provenance(const project& p, const job_outcome& out) {
  for (const artifact_record& a : p.store->artifacts_for(out.job.node_key)) {
    if (a.output_role == "provenance" && a.generation == out.job.generation) {
      return parse_json(text_of(p.store->artifact_path(a.rel_path)));
    }
  }
  ADD_FAILURE() << "provenance non trovata";
  return json::null();
}

artifact_record role_of(const project& p, const job_outcome& out, const std::string& role) {
  for (const artifact_record& a : p.store->artifacts_for(out.job.node_key)) {
    if (a.output_role == role && a.generation == out.job.generation) return a;
  }
  ADD_FAILURE() << "artefatto " << role << " non trovato";
  return {};
}

const json& at(const json& j, const std::string& key) {
  const json* v = j.find(key);
  if (v == nullptr) {
    ADD_FAILURE() << "chiave assente: " << key;
    static const json none;
    return none;
  }
  return *v;
}

std::vector<std::string> strings_of(const json& array) {
  std::vector<std::string> out;
  for (const json& item : array.items()) out.push_back(item.as_string());
  return out;
}

std::string thumb_key(const content_digest& d, std::uint32_t bw, std::uint32_t bh) {
  json o = json::object();
  o.set("source_sha256", json::string(d.sha256));
  o.set("source_size", json::uinteger(d.size_bytes));
  o.set("block_width", json::uinteger(bw));
  o.set("block_height", json::uinteger(bh));
  o.set("algorithm_version", json::uinteger(1));
  const std::string text = "astro.cache.thumbnail.v1\n" + canonical_json(o);
  return sha256_hex({reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
}

std::vector<fs::path> entries_in(const fs::path& dir) {
  std::vector<fs::path> out;
  for (const auto& e : fs::directory_iterator(dir)) out.push_back(e.path());
  std::sort(out.begin(), out.end());
  return out;
}

fs::path thumbs_dir(const project& p) { return p.dir / "project" / "cache" / "thumbnails"; }

// ---------------------------------------------------------------- T-M2R1-12

TEST(Thumbnail, T_M2R1_12_absent_entry_is_computed_valid_entry_is_a_hit_and_the_cache_is_confined) {
  project p = make_project("t12");
  const source src = write_source(p.dir, "a.fits", thumb_gen());
  const preview_request req{8, 4};
  cache_options opt;
  opt.quota_bytes = 1 << 20;
  thumbnail_cache cache(p.dir / "project", opt);
  add_slot(p, "thumb", make_thumbnail_node(src.digest, req));
  memory_pool pool(64 * MiB);
  const auto run = [&] { return quiet([&] { return run_thumbnail_job(*p.store, "thumb", src.path.string(), src.digest, req, cache, pool, never()); }); };

  const job_outcome first = run();
  ASSERT_EQ(first.exit_code, 0);
  EXPECT_EQ(cache.computations(), 1u);
  const json prov1 = persisted_provenance(p, first);
  const json& c1 = at(at(prov1, "details"), "cache");
  EXPECT_EQ(at(c1, "outcome").as_string(), "computed");
  EXPECT_EQ(at(c1, "key").as_string(), thumb_key(src.digest, 8, 4));
  const std::string entry = at(c1, "entry").as_string();
  const std::vector<fs::path> files = entries_in(thumbs_dir(p));
  ASSERT_EQ(files.size(), 1u);
  EXPECT_EQ(files[0].filename().string(), entry);
  const fx::bytes entry_bytes = read_file(files[0]);
  EXPECT_EQ(entry, thumb_key(src.digest, 8, 4) + "." + digest_of(entry_bytes).sha256 + ".thm");
  EXPECT_EQ(entry_bytes.size(), 36u + 10u * 10u * 8u);
  EXPECT_EQ(at(prov1, "state").as_string(), "completed");
  const artifact_record ref = role_of(p, first, "primary");
  EXPECT_EQ(ref.kind, "thumbnail_ref");
  EXPECT_EQ(at(parse_json(text_of(p.store->artifact_path(ref.rel_path))), "entry").as_string(), entry);

  const job_outcome second = run();
  ASSERT_EQ(second.exit_code, 0);
  EXPECT_EQ(cache.computations(), 1u) << "una voce valida non si ricalcola";
  EXPECT_EQ(at(at(at(persisted_provenance(p, second), "details"), "cache"), "outcome").as_string(), "cache_hit");
  EXPECT_EQ(read_file(files[0]), entry_bytes);

  fs::remove(files[0]);
  const job_outcome third = run();
  ASSERT_EQ(third.exit_code, 0);
  EXPECT_EQ(cache.computations(), 2u);
  EXPECT_EQ(at(at(at(persisted_provenance(p, third), "details"), "cache"), "outcome").as_string(), "computed");
  EXPECT_EQ(read_file(files[0]), entry_bytes) << "la voce ricalcolata e' identica";

  // Quota: la nuova voce oltre la somma massima si ferma prima di scrivere; nessuna rimozione automatica.
  {
    project q = make_project("t12_quota");
    const source qs = write_source(q.dir, "a.fits", thumb_gen());
    cache_options exact;
    exact.quota_bytes = entry_bytes.size();
    thumbnail_cache qc(q.dir / "project", exact);
    add_slot(q, "t1", make_thumbnail_node(qs.digest, req));
    const preview_request other{4, 4};
    add_slot(q, "t2", make_thumbnail_node(qs.digest, other));
    EXPECT_EQ(quiet([&] { return run_thumbnail_job(*q.store, "t1", qs.path.string(), qs.digest, req, qc, pool, never()); }).exit_code, 0);
    const std::vector<fs::path> before = entries_in(thumbs_dir(q));
    const job_outcome over = quiet([&] { return run_thumbnail_job(*q.store, "t2", qs.path.string(), qs.digest, other, qc, pool, never()); });
    EXPECT_EQ(over.exit_code, 2);
    ASSERT_TRUE(over.stop);
    EXPECT_EQ(over.stop->category(), error_category::budget);
    EXPECT_EQ(over.stop->code(), "cache.quota_exceeded");
    EXPECT_EQ(entries_in(thumbs_dir(q)), before);

    project z = make_project("t12_quota0");
    cache_options tiny;
    tiny.quota_bytes = entry_bytes.size() - 1;
    thumbnail_cache zc(z.dir / "project", tiny);
    add_slot(z, "t1", make_thumbnail_node(src.digest, req));
    const job_outcome none = quiet([&] { return run_thumbnail_job(*z.store, "t1", src.path.string(), src.digest, req, zc, pool, never()); });
    ASSERT_TRUE(none.stop);
    EXPECT_EQ(none.stop->code(), "cache.quota_exceeded");
    EXPECT_TRUE(entries_in(thumbs_dir(z)).empty());
    cache_options zero;
    EXPECT_STOP(thumbnail_cache(z.dir / "project", zero), error_category::invalid_input, "cache.quota");
  }

  // Symlink, hardlink e directory sostituite: security, nessuna lettura seguita.
  {
    project s = make_project("t12_links");
    const source ss = write_source(s.dir, "a.fits", thumb_gen());
    thumbnail_cache sc(s.dir / "project", opt);
    add_slot(s, "t", make_thumbnail_node(ss.digest, req));
    ASSERT_EQ(quiet([&] { return run_thumbnail_job(*s.store, "t", ss.path.string(), ss.digest, req, sc, pool, never()); }).exit_code, 0);
    const fs::path e = entries_in(thumbs_dir(s)).front();
    const fs::path moved = s.dir / "moved.thm";
    fs::rename(e, moved);
    fs::create_symlink(moved, e);
    const job_outcome sym = quiet([&] { return run_thumbnail_job(*s.store, "t", ss.path.string(), ss.digest, req, sc, pool, never()); });
    ASSERT_TRUE(sym.stop);
    EXPECT_EQ(sym.stop->category(), error_category::security);
    EXPECT_EQ(sym.stop->code(), "cache.symlink");
    fs::remove(e);
    fs::rename(moved, e);
    fs::create_hard_link(e, s.dir / "alias.thm");
    const job_outcome hard = quiet([&] { return run_thumbnail_job(*s.store, "t", ss.path.string(), ss.digest, req, sc, pool, never()); });
    ASSERT_TRUE(hard.stop);
    EXPECT_EQ(hard.stop->category(), error_category::security);
    EXPECT_EQ(hard.stop->code(), "cache.hardlink");
    EXPECT_EQ(read_file(e), entry_bytes);
    fs::remove(s.dir / "alias.thm");

    // Cleanup confinato: un temporaneo symlink ferma tutto senza toccare niente.
    {
      write_file(thumbs_dir(s) / ".tmp-aa", fx::bytes{1, 2, 3});
      fs::create_symlink(moved, thumbs_dir(s) / ".tmp-bb");
      EXPECT_STOP(sc.cleanup_temporaries(), error_category::security, "cache.symlink");
      EXPECT_TRUE(fs::exists(thumbs_dir(s) / ".tmp-aa"));
      fs::remove(thumbs_dir(s) / ".tmp-bb");
      const std::vector<std::string> removed = sc.cleanup_temporaries();
      EXPECT_EQ(removed, (std::vector<std::string>{".tmp-aa"}));
      EXPECT_FALSE(fs::exists(thumbs_dir(s) / ".tmp-aa"));
      EXPECT_TRUE(fs::exists(e));
      write_file(thumbs_dir(s) / ".tmp-cc", fx::bytes{1});
      fs::create_hard_link(thumbs_dir(s) / ".tmp-cc", s.dir / "cc-alias");
      EXPECT_STOP(sc.cleanup_temporaries(), error_category::security, "cache.hardlink");
      EXPECT_TRUE(fs::exists(thumbs_dir(s) / ".tmp-cc"));
    }

    project d = make_project("t12_dirlink");
    fs::create_directories(d.dir / "project" / "cache");
    fs::create_directories(d.dir / "elsewhere");
    fs::create_directory_symlink(d.dir / "elsewhere", d.dir / "project" / "cache" / "thumbnails");
    EXPECT_STOP(thumbnail_cache(d.dir / "project", opt), error_category::security, "cache.symlink");
    EXPECT_TRUE(entries_in(d.dir / "elsewhere").empty());
    project d2 = make_project("t12_dirlink2");
    fs::create_directories(d2.dir / "elsewhere");
    fs::create_directory_symlink(d2.dir / "elsewhere", d2.dir / "project" / "cache");
    EXPECT_STOP(thumbnail_cache(d2.dir / "project", opt), error_category::security, "cache.symlink");
  }
}

// ---------------------------------------------------------------- T-M2R1-04 e T-M2R1-11

TEST(Thumbnail, T_M2R1_04_T_M2R1_11_a_corrupt_entry_stops_and_only_the_explicit_rebuild_replaces_it) {
  project p = make_project("t04");
  const source src = write_source(p.dir, "a.fits", thumb_gen());
  const preview_request req{8, 4};
  cache_options opt;
  opt.quota_bytes = 1 << 20;
  thumbnail_cache cache(p.dir / "project", opt);
  add_slot(p, "thumb", make_thumbnail_node(src.digest, req));
  memory_pool pool(64 * MiB);
  const auto thumb = [&] { return quiet([&] { return run_thumbnail_job(*p.store, "thumb", src.path.string(), src.digest, req, cache, pool, never()); }); };
  ASSERT_EQ(thumb().exit_code, 0);
  const fs::path entry = entries_in(thumbs_dir(p)).front();
  const fx::bytes original = read_file(entry);
  const std::string name = entry.filename().string();
  const std::string expected_digest = name.substr(65, 64);
  ASSERT_EQ(digest_of(original).sha256, expected_digest);

  fx::bytes altered = original;
  altered[40] ^= 0x01;
  write_file(entry, altered);
  const std::string found_digest = digest_of(altered).sha256;
  ASSERT_NE(found_digest, expected_digest);
  const std::size_t primaries_before = [&] {
    std::size_t n = 0;
    for (const artifact_record& a : p.store->artifacts_for(make_thumbnail_node(src.digest, req).key())) n += a.output_role == "primary";
    return n;
  }();

  const job_outcome bad = thumb();
  EXPECT_EQ(bad.exit_code, 2);
  ASSERT_TRUE(bad.stop);
  EXPECT_EQ(bad.stop->category(), error_category::integrity);
  EXPECT_EQ(bad.stop->code(), "cache.corrupt");
  EXPECT_EQ(ctx(*bad.stop, "atteso"), expected_digest);
  EXPECT_EQ(ctx(*bad.stop, "trovato"), found_digest);
  EXPECT_EQ(ctx(*bad.stop, "voce"), name);
  EXPECT_EQ(read_file(entry), altered) << "la voce corrotta non si tocca";
  EXPECT_EQ(entries_in(thumbs_dir(p)).size(), 1u);
  const json stop_prov = persisted_provenance(p, bad);
  EXPECT_EQ(at(stop_prov, "state").as_string(), "failed");
  EXPECT_EQ(at(at(stop_prov, "stop"), "code").as_string(), "cache.corrupt");
  EXPECT_EQ(stop_prov.find("details"), nullptr);
  std::size_t primaries_after = 0;
  for (const artifact_record& a : p.store->artifacts_for(bad.job.node_key)) primaries_after += a.output_role == "primary";
  EXPECT_EQ(primaries_after, primaries_before) << "nessun output";
  EXPECT_EQ(cache.computations(), 1u) << "nessuna ricostruzione";
  {
    raw_db db(p.dir / "project", true);
    EXPECT_EQ(db.query("SELECT COUNT(*) FROM nodes WHERE operator_id = 'cache.rebuild'"), "0|\n");
  }
  // Un secondo tentativo dello stesso job si ferma allo stesso modo: nessun percorso ricostruisce da solo.
  const job_outcome again = thumb();
  ASSERT_TRUE(again.stop);
  EXPECT_EQ(again.stop->code(), "cache.corrupt");
  EXPECT_EQ(read_file(entry), altered);

  // T-M2R1-11: job esplicito.
  const node_spec rebuild_node = make_cache_rebuild_node(src.digest, req, cache_rebuild_cause_corrupt);
  add_slot(p, "rebuild", rebuild_node);
  const job_outcome rb = quiet([&] {
    return run_cache_rebuild_job(*p.store, "rebuild", src.path.string(), src.digest, req, cache_rebuild_cause_corrupt, cache, pool, never());
  });
  ASSERT_EQ(rb.exit_code, 0);
  EXPECT_EQ(read_file(entry), original) << "la nuova voce e' identica byte per byte all'originale";
  EXPECT_EQ(entries_in(thumbs_dir(p)).size(), 1u);
  const json rb_prov = persisted_provenance(p, rb);
  const json& rc = at(at(rb_prov, "details"), "cache");
  EXPECT_EQ(at(rc, "action").as_string(), "rebuild");
  EXPECT_EQ(at(rc, "outcome").as_string(), "rebuilt");
  EXPECT_EQ(at(rc, "cause").as_string(), "cache.corrupt");
  EXPECT_EQ(at(at(rc, "replaced_entry"), "name").as_string(), name);
  EXPECT_EQ(at(at(rc, "replaced_entry"), "found_digest").as_string(), found_digest);
  EXPECT_EQ(at(at(rb_prov, "node_spec"), "operator_id").as_string(), "cache.rebuild");

  const job_outcome hit = thumb();
  ASSERT_EQ(hit.exit_code, 0);
  EXPECT_EQ(at(at(at(persisted_provenance(p, hit), "details"), "cache"), "outcome").as_string(), "cache_hit");

  // Rebuild senza voce, causa non valida e rebuild di una voce valida (richiesta dell'utente).
  const preview_request other{4, 4};
  const node_spec nothing = make_cache_rebuild_node(src.digest, other, cache_rebuild_cause_user);
  add_slot(p, "nothing", nothing);
  const job_outcome none = quiet([&] {
    return run_cache_rebuild_job(*p.store, "nothing", src.path.string(), src.digest, other, cache_rebuild_cause_user, cache, pool, never());
  });
  ASSERT_TRUE(none.stop);
  EXPECT_EQ(none.stop->category(), error_category::invalid_input);
  EXPECT_EQ(none.stop->code(), "cache.rebuild_nothing");
  EXPECT_STOP(make_cache_rebuild_node(src.digest, req, "perche_si"), error_category::invalid_input, "cache.rebuild_cause");
  // La causa non valida nel job e' un arresto prima del lavoro (R5.d): job `failed` registrato con la provenance
  // dell'arresto senza nodo, stop propagato invariato, nessuna ricostruzione.
  const std::uint64_t computations_before = cache.computations();
  const std::uint64_t generation_before = p.store->get_slot("nothing")->current_generation;
  testing::internal::CaptureStderr();
  EXPECT_STOP(run_cache_rebuild_job(*p.store, "nothing", src.path.string(), src.digest, other, "perche_si", cache, pool,
                                    never()),
              error_category::invalid_input, "cache.rebuild_cause");
  testing::internal::GetCapturedStderr();
  EXPECT_EQ(cache.computations(), computations_before);
  {
    const std::uint64_t generation = p.store->get_slot("nothing")->current_generation;
    EXPECT_EQ(generation, generation_before + 1) << "un job registra l'arresto";
    std::size_t found = 0;
    for (const artifact_record& a : p.store->artifacts_for(nothing.key())) {
      if (a.output_role != "provenance" || a.generation != generation_before + 1) continue;
      ++found;
      const json doc = parse_json(text_of(p.store->artifact_path(a.rel_path)));
      const json* marker = doc.find("stop_before_work");
      ASSERT_NE(marker, nullptr);
      EXPECT_EQ(marker->type(), json::kind::boolean);
      EXPECT_EQ(at(doc, "node_key").as_string(), nothing.key());
      EXPECT_EQ(at(doc, "state").as_string(), "failed");
      EXPECT_EQ(at(at(doc, "stop"), "code").as_string(), "cache.rebuild_cause");
      EXPECT_EQ(doc.find("node_spec"), nullptr);
    }
    EXPECT_EQ(found, 1u);
  }
  const node_spec user = make_cache_rebuild_node(src.digest, req, cache_rebuild_cause_user);
  add_slot(p, "user", user);
  const job_outcome valid = quiet([&] {
    return run_cache_rebuild_job(*p.store, "user", src.path.string(), src.digest, req, cache_rebuild_cause_user, cache, pool, never());
  });
  ASSERT_EQ(valid.exit_code, 0);
  EXPECT_EQ(read_file(entry), original);
  EXPECT_NE(rebuild_node.key(), user.key());
  EXPECT_NE(make_thumbnail_node(src.digest, req).key(), make_cache_rebuild_node(src.digest, req, cache_rebuild_cause_user).key());
}

// ---------------------------------------------------------------- T-M2R1-08, T-M2R1-06, T-M2R1-07

fits_inventory fresh_inventory(const source& s, const fits_read_options& o) {
  const fits_input in = open_fits_input(s.path.string());
  memory_pool pool(64 * MiB);
  return inventory_fits(in, o, pool);
}

TEST(Import, T_M2R1_08_the_report_after_reopening_matches_the_inventory) {
  project p = make_project("t08");
  gen g = thumb_gen({fx::card_str("OBSERVER", "tester"), fx::card_real("EXPTIME", 12.5), fx::card_bool("LINEAR", true), phase_card("raw")});
  const source src = write_source(p.dir, "a.fits", g, true);
  import_request req;
  req.path = src.path.string();
  req.digest = src.digest;
  const node_spec node = make_import_node(req);
  add_slot(p, "imp", node);
  memory_pool pool(64 * MiB);
  const job_outcome out = quiet([&] { return run_import_job(*p.store, "imp", req, pool, never()); });
  ASSERT_EQ(out.exit_code, 0);
  EXPECT_EQ(out.provenance.state, job_state::completed);
  const fs::path root = p.store->root();
  p.store.reset();
  auto reopened = sqlite_dag_store::open(root, open_intent::read_only);
  const slot_view view = reopened->read_slot("imp");
  ASSERT_EQ(view.state, freshness::current);
  EXPECT_EQ(view.artifact->kind, "import_report");
  const json report = parse_json(text_of(reopened->artifact_path(view.artifact->rel_path)));
  const fits_inventory inv = fresh_inventory(src, {});

  EXPECT_EQ(at(report, "format").as_string(), "astro.import_report.v1");
  EXPECT_EQ(at(at(report, "source"), "sha256").as_string(), src.digest.sha256);
  EXPECT_EQ(at(at(report, "source"), "size_bytes").as_integer(), static_cast<std::int64_t>(src.bytes.size()));
  EXPECT_EQ(at(report, "header_bytes_read").as_integer(), static_cast<std::int64_t>(inv.header_bytes_read));
  const json& hdus = at(report, "hdus");
  ASSERT_EQ(hdus.items().size(), inv.hdus.size());
  for (std::size_t i = 0; i < inv.hdus.size(); ++i) {
    EXPECT_EQ(at(hdus.items()[i], "bitpix").as_integer(), inv.hdus[i].bitpix);
    EXPECT_EQ(at(hdus.items()[i], "extname").as_string(), inv.hdus[i].extname);
    EXPECT_EQ(at(hdus.items()[i], "data_offset").as_integer(), static_cast<std::int64_t>(inv.hdus[i].data_offset));
    ASSERT_EQ(at(hdus.items()[i], "axes").items().size(), inv.hdus[i].axes.size());
    for (std::size_t k = 0; k < inv.hdus[i].axes.size(); ++k) {
      EXPECT_EQ(at(hdus.items()[i], "axes").items()[k].as_integer(), static_cast<std::int64_t>(inv.hdus[i].axes[k]));
    }
  }
  const json& image = at(report, "image");
  EXPECT_EQ(at(image, "width").as_integer(), 80);
  EXPECT_EQ(at(image, "height").as_integer(), 40);
  EXPECT_EQ(at(image, "layout").as_string(), "mono");
  EXPECT_EQ(at(image, "sample_type").as_string(), "f32");
  EXPECT_EQ(at(at(at(report, "state"), "phase"), "value").as_string(), "raw");
  EXPECT_EQ(at(at(at(report, "state"), "phase"), "origin").as_string(), "header");
  EXPECT_EQ(at(at(at(report, "state"), "linearity"), "value").as_string(), "linear");
  EXPECT_EQ(strings_of(at(report, "uninterpreted_keywords")), inv.uninterpreted_keywords);
  EXPECT_EQ(at(report, "header_cards").items().size(), inv.header_cards.size());
  EXPECT_EQ(at(at(at(report, "metadata"), "exposure_s"), "value").as_number(), 12.5);
  // Resa testuale dello stesso documento.
  std::string text;
  for (const artifact_record& a : reopened->artifacts_for(node.key())) {
    if (a.output_role == "report_text") text = text_of(reopened->artifact_path(a.rel_path));
  }
  EXPECT_NE(text.find(src.digest.sha256), std::string::npos);
  EXPECT_NE(text.find("OBSERVER"), std::string::npos);
}

TEST(Import, T_M2R1_07_uninterpreted_keywords_are_listed_in_report_and_provenance) {
  // Il test calcola l'insieme atteso dalle card generate, con il proprio elenco delle keyword interpretate.
  const std::vector<std::string> cards = {
      fx::card_str("OBSERVER", "Rossi"),  fx::card_real("EXPTIME", 30),         fx::card_str("PROGRAM", "P1"),
      fx::card_text("COMMENT", "una nota"), fx::card_text("HISTORY", "storia"),  fx::card_int("NOTE_A", 5),
      fx::card_bool("LINEAR", false),     phase_card("prestacked"),             fx::card_int("NOTE_A", 6),
      fx::card_str("BUNIT", "adu"),       fx::card_real("SITELAT", 45.5),       fx::card_text("", "senza keyword"),
      fx::card_str("FILTER", "L"),        fx::card_int("Z-ORDER", 1),           fx::card_int("TOTALEXP", 9)};
  const std::set<std::string> interpreted = {"EXPTIME", "GAIN", "EGAIN", "OFFSET", "CCD-TEMP", "SATURATE", "PEDESTAL", "FILTER",
                                              "INSTRUME", "IMAGETYP", "READOUTM", "ROWORDER", "XORGSUBF", "YORGSUBF", "BUNIT",
                                              "LINEAR", "XBINNING", "YBINNING", "BAYERPAT", "XBAYROFF", "YBAYROFF", "ASTRPHAS", ""};
  std::vector<std::string> expected;
  for (const std::string& c : cards) {
    std::string key = c.substr(0, 8);
    while (!key.empty() && key.back() == ' ') key.pop_back();
    if (interpreted.count(key) != 0) continue;
    if (std::find(expected.begin(), expected.end(), key) == expected.end()) expected.push_back(key);
  }
  ASSERT_EQ(expected, (std::vector<std::string>{"OBSERVER", "PROGRAM", "COMMENT", "HISTORY", "NOTE_A", "SITELAT", "Z-ORDER", "TOTALEXP"}));

  project p = make_project("t07");
  const source src = write_source(p.dir, "a.fits", thumb_gen(cards));
  EXPECT_EQ(decode_fits(src.bytes, 64 * MiB).uninterpreted_keywords, expected);
  EXPECT_EQ(fresh_inventory(src, {}).uninterpreted_keywords, expected);
  import_request req;
  req.path = src.path.string();
  req.digest = src.digest;
  add_slot(p, "imp", make_import_node(req));
  memory_pool pool(64 * MiB);
  const job_outcome out = quiet([&] { return run_import_job(*p.store, "imp", req, pool, never()); });
  ASSERT_EQ(out.exit_code, 0);
  const artifact_record primary = role_of(p, out, "primary");
  const json report = parse_json(text_of(p.store->artifact_path(primary.rel_path)));
  EXPECT_EQ(strings_of(at(report, "uninterpreted_keywords")), expected);
  const auto observed = [&](const json& provenance) {
    const json& list = at(at(provenance, "details"), "observed_inputs");
    EXPECT_EQ(list.items().size(), 1u);
    EXPECT_EQ(at(list.items()[0], "sha256").as_string(), src.digest.sha256);
    EXPECT_EQ(at(at(list.items()[0], "phase"), "value").as_string(), "prestacked");
    EXPECT_EQ(at(at(list.items()[0], "linearity"), "value").as_string(), "nonlinear");
    return strings_of(at(list.items()[0], "uninterpreted_keywords"));
  };
  EXPECT_EQ(observed(provenance_json(out.provenance)), expected);
  EXPECT_EQ(observed(persisted_provenance(p, out)), expected);
}

TEST(Import, T_M2R1_06_calibrated_and_prestacked_without_raw_are_inventoried_previewed_and_exported) {
  struct variant {
    const char* name;
    std::vector<std::string> cards;
    const char* phase;
    const char* lin;
  };
  const std::vector<variant> variants = {
      {"calibrated_unknown", {phase_card("calibrated")}, "calibrated", "unknown"},
      {"prestacked_nonlinear", {phase_card("prestacked"), fx::card_bool("LINEAR", false)}, "prestacked", "nonlinear"},
  };
  for (const variant& v : variants) {
    SCOPED_TRACE(v.name);
    project p = make_project(std::string("t06_") + v.name);
    const gen g = thumb_gen(v.cards);
    const source src = write_source(p.dir, "a.fits", g);
    import_request req;
    req.path = src.path.string();
    req.digest = src.digest;
    add_slot(p, "imp", make_import_node(req));
    memory_pool pool(64 * MiB);
    const job_outcome out = quiet([&] { return run_import_job(*p.store, "imp", req, pool, never()); });
    ASSERT_EQ(out.exit_code, 0);
    const json report = parse_json(text_of(p.store->artifact_path(role_of(p, out, "primary").rel_path)));
    EXPECT_EQ(at(at(at(report, "state"), "phase"), "value").as_string(), v.phase);
    EXPECT_EQ(at(at(at(report, "state"), "linearity"), "value").as_string(), v.lin);
    const json& ops = at(at(report, "preflight"), "operations");
    std::map<std::string, std::pair<bool, std::string>> by_name;
    for (const json& o : ops.items()) by_name[at(o, "operation").as_string()] = {at(o, "allowed").as_bool(), at(o, "code").as_string()};
    for (const char* free_op : {"inventory", "preview", "export_bundle"}) EXPECT_TRUE(by_name.at(free_op).first) << free_op;
    EXPECT_EQ(by_name.at("master_create"), (std::pair<bool, std::string>{false, "op.phase_blocked"}));
    EXPECT_EQ(by_name.at("calibrate"), (std::pair<bool, std::string>{false, "op.phase_blocked"}));
    EXPECT_EQ(by_name.at("linear_ancestor_required"), (std::pair<bool, std::string>{false, "op.linear_ancestor_missing"}));
    const json& diags = at(at(report, "preflight"), "diagnostics");
    ASSERT_EQ(diags.items().size(), 1u);
    EXPECT_EQ(at(diags.items()[0], "code").as_string(), "c14.linear_ancestor_missing");
    EXPECT_NE(at(diags.items()[0], "message").as_string().find("nessuna inversione"), std::string::npos);

    // Anteprima e campioni invariati: nessuno stretch invertito.
    const fits_input in = open_fits_input(src.path.string());
    const fits_preview pv = make_preview(in, {8, 4}, pool);
    EXPECT_EQ(pv.width, 10u);
    EXPECT_EQ(pv.finite_counts.front(), 32u);
    const image decoded = decode_fits(src.bytes, 64 * MiB);
    for (std::uint32_t y = 0; y < g.h; ++y) {
      for (std::uint32_t x = 0; x < g.w; ++x) {
        ASSERT_EQ(decoded.data<float>()[y * g.w + x], static_cast<float>(g.value(0, x, y)));
      }
    }
    EXPECT_STOP(require_operation_allowed(operation_kind::linear_ancestor_required,
                                          process_state{decoded.phase->value, classify_linearity(decoded.lin)}),
                error_category::incompatible, "op.linear_ancestor_missing");

    // Export del riferimento importato.
    const fs::path bundle = p.dir / "bundle";
    p.store->export_slots(bundle, {"imp"});
    const export_manifest manifest = verify_export(bundle);
    EXPECT_EQ(manifest.files.size(), 1u);
  }
}

TEST(Import, T_M2R1_05_import_requires_the_phase_and_stops_on_a_declaration_that_contradicts_the_header) {
  project p = make_project("t05_import");
  const source plain = write_source(p.dir, "plain.fits", thumb_gen());
  const source tagged = write_source(p.dir, "tagged.fits", thumb_gen({phase_card("raw")}));
  memory_pool pool(64 * MiB);
  const auto run = [&](const source& s, const std::string& slot, const fits_read_options& o) {
    import_request req;
    req.path = s.path.string();
    req.digest = s.digest;
    req.read_options = o;
    add_slot(p, slot, make_import_node(req));
    return quiet([&] { return run_import_job(*p.store, slot, req, pool, never()); });
  };
  const job_outcome missing = run(plain, "missing", {});
  EXPECT_EQ(missing.exit_code, 2);
  ASSERT_TRUE(missing.stop);
  EXPECT_EQ(missing.stop->category(), error_category::metadata_missing);
  EXPECT_EQ(missing.stop->code(), "import.phase_missing");
  fits_read_options declared;
  declared.declared_phase = process_phase::prestacked;
  const job_outcome ok = run(plain, "declared", declared);
  ASSERT_EQ(ok.exit_code, 0);
  const json report = parse_json(text_of(p.store->artifact_path(role_of(p, ok, "primary").rel_path)));
  EXPECT_EQ(at(at(at(report, "state"), "phase"), "origin").as_string(), "declared");
  fits_read_options conflict;
  conflict.declared_phase = process_phase::calibrated;
  const job_outcome bad = run(tagged, "conflict", conflict);
  ASSERT_TRUE(bad.stop);
  EXPECT_EQ(bad.stop->category(), error_category::incompatible);
  EXPECT_EQ(bad.stop->code(), "metadata.declared_conflict");
  EXPECT_NE(make_import_node(import_request{plain.path.string(), plain.digest, declared}).key(),
            make_import_node(import_request{plain.path.string(), plain.digest, {}}).key());
  // Digest diverso dal file: integrity, nessuna lettura dell'header.
  import_request wrong;
  wrong.path = tagged.path.string();
  wrong.digest = plain.digest;
  add_slot(p, "wrong", make_import_node(wrong));
  const job_outcome mismatch = quiet([&] { return run_import_job(*p.store, "wrong", wrong, pool, never()); });
  ASSERT_TRUE(mismatch.stop);
  EXPECT_EQ(mismatch.stop->category(), error_category::integrity);
  EXPECT_EQ(mismatch.stop->code(), "input.digest_mismatch");
}


// ---------------------------------------------------------------- T-M2R1-10 (job) e memoria della cache

TEST(Import, T_M2R1_10_a_file_changed_after_the_digest_stops_the_import_thumbnail_and_rebuild_jobs) {
  const preview_request req{8, 4};
  const auto grow = [](const fs::path& path) {
    fx::bytes b = read_file(path);
    b.push_back(0);
    write_file(path, b);
  };
  memory_pool pool(64 * MiB);
  // Import.
  {
    project p = make_project("t10_import");
    const source src = write_source(p.dir, "a.fits", thumb_gen({phase_card("raw")}));
    import_request r;
    r.path = src.path.string();
    r.digest = src.digest;
    r.hook = [&](std::string_view point) {
      if (point == "digest.done") grow(src.path);
    };
    add_slot(p, "imp", make_import_node(r));
    const job_outcome out = quiet([&] { return run_import_job(*p.store, "imp", r, pool, never()); });
    ASSERT_TRUE(out.stop);
    EXPECT_EQ(out.stop->category(), error_category::integrity);
    EXPECT_EQ(out.stop->code(), "input.size_changed");
    EXPECT_EQ(p.store->read_slot("imp").state, freshness::missing);
  }
  // Thumbnail su voce assente e su voce valida; rebuild.
  for (const char* mode : {"computed", "hit", "rebuild"}) {
    SCOPED_TRACE(mode);
    project p = make_project(std::string("t10_thumb_") + mode);
    const source src = write_source(p.dir, "a.fits", thumb_gen());
    cache_options opt;
    opt.quota_bytes = 1 << 20;
    thumbnail_cache cache(p.dir / "project", opt);
    add_slot(p, "thumb", make_thumbnail_node(src.digest, req));
    add_slot(p, "rebuild", make_cache_rebuild_node(src.digest, req, cache_rebuild_cause_user));
    if (std::string(mode) != "computed") {
      ASSERT_EQ(quiet([&] { return run_thumbnail_job(*p.store, "thumb", src.path.string(), src.digest, req, cache, pool, never()); }).exit_code, 0);
    }
    const job_hook hook = [&](std::string_view point) {
      if (point == "digest.done") grow(src.path);
    };
    const job_outcome out = std::string(mode) == "rebuild"
        ? quiet([&] { return run_cache_rebuild_job(*p.store, "rebuild", src.path.string(), src.digest, req, cache_rebuild_cause_user, cache, pool, never(), std::nullopt, hook); })
        : quiet([&] { return run_thumbnail_job(*p.store, "thumb", src.path.string(), src.digest, req, cache, pool, never(), std::nullopt, hook); });
    ASSERT_TRUE(out.stop);
    EXPECT_EQ(out.stop->category(), error_category::integrity);
    EXPECT_EQ(out.stop->code(), "input.size_changed");
  }
}

TEST(Thumbnail, T_M2R1_12_cache_reads_and_rebuilds_reserve_their_memory_in_the_pool) {
  project p = make_project("t12_memory");
  const source src = write_source(p.dir, "a.fits", thumb_gen());
  const preview_request req{8, 4};
  cache_options opt;
  opt.quota_bytes = 1 << 20;
  thumbnail_cache cache(p.dir / "project", opt);
  memory_pool big_pool(64 * MiB);
  fits_input in = open_fits_input(src.path.string());
  (void)cache.get(in, src.digest, req, big_pool);
  const fs::path entry = entries_in(thumbs_dir(p)).front();
  const fx::bytes before = read_file(entry);
  const std::uint64_t size = before.size();
  ASSERT_EQ(size, 36u + 10u * 10u * 8u);

  const auto stops_with_minimum = [&](const std::function<void(memory_pool&)>& op, std::uint64_t minimum) {
    memory_pool small(minimum - 1);
    try {
      op(small);
      ADD_FAILURE() << "stop atteso";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.category(), error_category::budget);
      EXPECT_EQ(e.code(), "sched.job_exceeds_budget");
      EXPECT_EQ(ctx(e, "minimo"), std::to_string(minimum));
    }
    EXPECT_EQ(small.reserved_bytes(), 0u);
  };
  stops_with_minimum([&](memory_pool& pool) { (void)cache.get(in, src.digest, req, pool); }, 2 * size);
  {
    memory_pool exact(2 * size);
    const thumbnail_result hit = cache.get(in, src.digest, req, exact);
    EXPECT_EQ(hit.entry.outcome, cache_outcome::cache_hit);
    EXPECT_EQ(exact.peak_reserved_bytes(), 2 * size);
  }
  std::string name, found;
  stops_with_minimum([&](memory_pool& pool) { (void)cache.rebuild(in, src.digest, req, pool, nullptr, name, found); }, size);
  EXPECT_EQ(read_file(entry), before);
}


TEST(Thumbnail, T_M2R1_12_an_entry_that_grows_while_being_read_stops_instead_of_exceeding_its_reservation) {
  project p = make_project("t12_growth");
  const source src = write_source(p.dir, "a.fits", thumb_gen());
  const preview_request req{8, 4};
  fs::path entry;
  bool grow = false;
  cache_options opt;
  opt.quota_bytes = 1 << 20;
  opt.fault = [&](std::string_view point) {
    if (grow && point == "cache.entry.read") {
      fx::bytes b = read_file(entry);
      b.insert(b.end(), 4096, 0xAB);
      write_file(entry, b);
    }
    return 0;
  };
  thumbnail_cache cache(p.dir / "project", opt);
  memory_pool pool(64 * MiB);
  fits_input in = open_fits_input(src.path.string());
  (void)cache.get(in, src.digest, req, pool);
  entry = entries_in(thumbs_dir(p)).front();
  grow = true;
  EXPECT_STOP(cache.get(in, src.digest, req, pool), error_category::limit_exceeded, "cache.read");
  EXPECT_EQ(pool.reserved_bytes(), 0u);
  std::string name, found;
  EXPECT_STOP(cache.rebuild(in, src.digest, req, pool, nullptr, name, found), error_category::limit_exceeded, "cache.read");
}


TEST(Thumbnail, T_M2R1_12_a_valid_entry_larger_than_the_quota_of_a_reopened_cache_is_not_read) {
  project p = make_project("t12_smaller_quota");
  const source src = write_source(p.dir, "a.fits", thumb_gen());
  const preview_request req{8, 4};
  memory_pool pool(64 * MiB);
  fits_input in = open_fits_input(src.path.string());
  std::uint64_t size = 0;
  {
    cache_options opt;
    opt.quota_bytes = 1 << 20;
    thumbnail_cache cache(p.dir / "project", opt);
    (void)cache.get(in, src.digest, req, pool);
    size = read_file(entries_in(thumbs_dir(p)).front()).size();
  }
  const fx::bytes before = read_file(entries_in(thumbs_dir(p)).front());
  cache_options small;
  small.quota_bytes = size - 1;
  thumbnail_cache reopened(p.dir / "project", small);
  EXPECT_STOP(reopened.get(in, src.digest, req, pool), error_category::budget, "cache.quota_exceeded");
  std::string name, found;
  EXPECT_STOP(reopened.rebuild(in, src.digest, req, pool, nullptr, name, found), error_category::budget, "cache.quota_exceeded");
  EXPECT_EQ(pool.reserved_bytes(), 0u);
  EXPECT_EQ(read_file(entries_in(thumbs_dir(p)).front()), before);
  cache_options exact;
  exact.quota_bytes = size;
  thumbnail_cache ok(p.dir / "project", exact);
  EXPECT_EQ(ok.get(in, src.digest, req, pool).entry.outcome, cache_outcome::cache_hit);
}

}  // namespace
