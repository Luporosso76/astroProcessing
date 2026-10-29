#include <sys/stat.h>

#include "store_test_support.hpp"

namespace {

using namespace store_test;

struct fixture {
  fs::path project;
  std::unique_ptr<sqlite_dag_store> store;
};

// Progetto con sb e sd correnti (catena a -> b, c -> d), 4 nodi nel manifest.
fixture make_fixture(const std::string& name) {
  fixture f;
  f.project = fresh_dir(name);
  f.store = sqlite_dag_store::create(f.project);
  f.store->apply_recipe(diamond(2).resolve(), diamond_slots());
  run_and_commit(*f.store, "sb", "payload b");
  run_and_commit(*f.store, "sd", "payload d");
  return f;
}

fs::path export_to(fixture& f, const std::string& name) {
  const fs::path dir = f.project.parent_path() / (name + "_export");
  fs::remove_all(dir);
  f.store->export_slots(dir, {"sb", "sd"});
  return dir;
}

std::string manifest_text(const std::vector<export_file_entry>& files, const std::vector<node_spec>& nodes) {
  json file_array = json::array();
  for (const auto& e : files) {
    json o = json::object();
    o.set("rel_path", json::string(e.rel_path));
    o.set("size_bytes", json::uinteger(e.size_bytes));
    o.set("sha256", json::string(e.sha256));
    o.set("slot_name", json::string(e.slot_name));
    o.set("generation", json::uinteger(e.generation));
    o.set("node_key", json::string(e.node_key));
    file_array.push(std::move(o));
  }
  json node_array = json::array();
  for (const auto& n : nodes) {
    json digests = json::array();
    for (const auto& in : n.inputs()) {
      if (!in.is_file()) continue;
      json d = json::object();
      d.set("sha256", json::string(in.file_digest().sha256));
      d.set("size_bytes", json::uinteger(in.file_digest().size_bytes));
      digests.push(std::move(d));
    }
    json o = json::object();
    o.set("node_key", json::string(n.key()));
    o.set("node_spec", n.to_json());
    o.set("input_digests", std::move(digests));
    o.set("correction_ledger", json::null());
    node_array.push(std::move(o));
  }
  json doc = json::object();
  doc.set("format", json::string("astro.export.v1"));
  json versions = json::object();
  for (const char* k : {"product", "source_revision", "cfitsio", "sqlite", "openssl"}) versions.set(k, json::string("x"));
  versions.set("source_dirty", json::null());
  doc.set("versions", std::move(versions));
  doc.set("files", std::move(file_array));
  doc.set("nodes", std::move(node_array));
  return canonical_json(doc);
}

void write_bundle(const fs::path& dir, const std::string& manifest) {
  fs::create_directories(dir);
  write_text(dir / "manifest.json", manifest);
  const auto bytes = bytes_of(manifest);
  write_text(dir / "COMMIT", sha256_hex(bytes));
}

TEST(Export, T_M2A_readdir_failure_while_verifying_stops_as_io_and_names_errno) {
  fixture f = make_fixture("readdir");
  const fs::path dir = export_to(f, "readdir");
  f.store.reset();
  int calls = 0;
  const fault_injector hook = [&](std::string_view p) { return p == "dir.readdir" ? (++calls, EIO) : 0; };
  try {
    verify_export(dir, hook);
    ADD_FAILURE() << "stop_error atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::io);
    EXPECT_EQ(e.code(), "export.open");
    bool found = false;
    for (const auto& kv : e.context()) found = found || (kv.first == "errno" && kv.second == std::to_string(EIO));
    EXPECT_TRUE(found) << "errno=EIO assente dal contesto";
  }
  EXPECT_EQ(calls, 1);
  const fault_injector throwing = [](std::string_view p) -> int {
    if (p == "dir.readdir") throw std::runtime_error("callback fallita");
    return 0;
  };
  EXPECT_THROW(verify_export(dir, throwing), std::runtime_error);  // il DIR* aperto si chiude per RAII (LeakSanitizer)
  EXPECT_EQ(verify_export(dir).files.size(), 2u);
}

TEST(Export, T_M2C_19_manifest_lets_another_process_rebuild_keys_and_digests) {
  fixture f = make_fixture("t19");
  const fs::path dir = export_to(f, "t19");
  f.store.reset();
  const child_outcome out = run_child(
      [&](const fault_injector&) {
        const export_manifest m = verify_export(dir);
        if (m.nodes.size() != 4 || m.files.size() != 2) throw std::runtime_error("manifest incompleto");
        for (const auto& file : m.files) {
          if (sha256_hex(bytes_of(read_text(dir / file.rel_path))) != file.sha256) throw std::runtime_error("digest");
        }
      },
      0);
  EXPECT_FALSE(out.killed);
  EXPECT_EQ(out.exit_code, 0);
  const export_manifest m = verify_export(dir);
  std::set<std::string> expected;
  for (const auto& n : diamond(2).resolve()) expected.insert(n.spec.key());
  std::set<std::string> got;
  for (const auto& n : m.nodes) got.insert(n.key());
  EXPECT_EQ(got, expected);
  ASSERT_EQ(m.files.size(), 2u);
  EXPECT_EQ(m.files[0].rel_path, "sb.bin");
  EXPECT_EQ(m.files[0].sha256, digest_of(bytes_of("payload b")).sha256);
  EXPECT_EQ(m.files[1].rel_path, "sd.bin");
  EXPECT_EQ(m.files[1].node_key, keys_of(diamond(2).resolve()).at("d"));
  EXPECT_EQ(m.manifest_sha256, read_text(dir / "COMMIT"));
}

TEST(Export, T_M2C_20_sigkill_at_every_export_step_leaves_a_bundle_verify_declares_incomplete) {
  fixture dry = make_fixture("t20_dry");
  const std::vector<std::string> points = record_points([&](const fault_injector& f) {
    dry.store->set_fault_injector(f);
    dry.store->export_slots(dry.project.parent_path() / "t20_dry_export", {"sb", "sd"});
  });
  dry.store.reset();
  ASSERT_GE(points.size(), 16u);
  EXPECT_EQ(points.front(), "export.mkdir");
  EXPECT_EQ(points.back(), "export.commit.fsync_dir");
  for (std::size_t i = 1; i <= points.size(); ++i) {
    SCOPED_TRACE("kill prima di " + points[i - 1]);
    fixture f = make_fixture("t20");
    const fs::path dir = f.project.parent_path() / "t20_export";
    fs::remove_all(dir);
    const fs::path project = f.project;
    f.store.reset();
    const child_outcome out = run_child(
        [&](const fault_injector& inj) {
          open_options options;
          options.fault = inj;
          sqlite_dag_store::open(project, open_intent::read_write, options)->export_slots(dir, {"sb", "sd"});
        },
        static_cast<int>(i));
    ASSERT_TRUE(out.killed);
    if (i == 1) {
      EXPECT_FALSE(fs::exists(dir));
    } else if (i == points.size()) {
      EXPECT_EQ(verify_export(dir).files.size(), 2u);
    } else {
      EXPECT_STOP(verify_export(dir), error_category::integrity, "export.incomplete");
    }
    EXPECT_EQ(sqlite_dag_store::open(project, open_intent::read_write)->read_slot("sd").state, freshness::current);
  }
}

TEST(Export, T_M2C_21_altered_bundles_are_declared_incomplete) {
  fixture f = make_fixture("t21");
  const fs::path good = export_to(f, "t21");
  EXPECT_EQ(verify_export(good).files.size(), 2u);
  const std::vector<std::pair<std::string, std::function<void(const fs::path&)>>> mutations{
      {"byte alterato", [](const fs::path& d) { std::string t = read_text(d / "sb.bin"); t[0] ^= 1; write_text(d / "sb.bin", t); }},
      {"file troncato", [](const fs::path& d) { write_text(d / "sb.bin", "x"); }},
      {"file rimosso", [](const fs::path& d) { fs::remove(d / "sd.bin"); }},
      {"manifest alterato", [](const fs::path& d) { write_text(d / "manifest.json", read_text(d / "manifest.json") + " "); }},
      {"COMMIT alterato", [](const fs::path& d) { write_text(d / "COMMIT", std::string(64, '0')); }},
      {"COMMIT rimosso", [](const fs::path& d) { fs::remove(d / "COMMIT"); }},
      {"manifest rimosso", [](const fs::path& d) { fs::remove(d / "manifest.json"); }},
      {"file in piu'", [](const fs::path& d) { write_text(d / "extra.txt", "x"); }},
      {"directory in piu'", [](const fs::path& d) { fs::create_directory(d / "extra_dir"); }},
      {"symlink in piu'", [](const fs::path& d) { fs::create_symlink("sb.bin", d / "alias"); }},
      {"temporaneo residuo", [](const fs::path& d) { write_text(d / ".tmp-1234", "x"); }},
  };
  for (const auto& [name, mutate] : mutations) {
    SCOPED_TRACE(name);
    const fs::path copy = good.parent_path() / "t21_copy";
    fs::remove_all(copy);
    fs::copy(good, copy, fs::copy_options::recursive | fs::copy_options::copy_symlinks);
    mutate(copy);
    EXPECT_STOP(verify_export(copy), error_category::integrity, "export.incomplete");
  }
}

TEST(Export, T_M2C_22_non_current_slots_existing_or_forbidden_destinations_stop) {
  fixture f = make_fixture("t22");
  const fs::path base = f.project.parent_path();
  const fs::path original = base / "t22_original.fits";
  write_text(original, "original");
  f.store->add_input(input_record{0, original.string(), 8, digest_of(bytes_of("original")).sha256, "light"});

  EXPECT_STOP(f.store->export_slots(base / "t22_a", {"sa"}), error_category::stale_generation, "export.not_current");
  f.store->apply_recipe(diamond(9).resolve(), diamond_slots());
  EXPECT_STOP(f.store->export_slots(base / "t22_a", {"sb"}), error_category::stale_generation, "export.not_current");
  EXPECT_STOP(f.store->export_slots(base / "t22_a", {"sc", "sa"}), error_category::stale_generation, "export.not_current");
  EXPECT_FALSE(fs::exists(base / "t22_a"));
  EXPECT_STOP(f.store->export_slots(base / "t22_a", {"nessuno"}), error_category::invalid_input, "dag.unknown_slot");
  EXPECT_STOP(f.store->export_slots(base / "t22_a", {"a/b"}), error_category::invalid_input, "export.slot_name");
  EXPECT_STOP(f.store->export_slots(base / "t22_a", {}), error_category::invalid_input, "export.no_slots");

  f.store->undo();
  fs::create_directory(base / "t22_dir");
  write_text(base / "t22_file", "x");
  fs::create_directory(base / "t22_real");
  fs::create_symlink(base / "t22_real", base / "t22_link");
  for (const char* existing : {"t22_dir", "t22_file", "t22_link"}) {
    SCOPED_TRACE(existing);
    EXPECT_STOP(f.store->export_slots(base / existing, {"sb"}), error_category::security, "export.target_exists");
  }
  EXPECT_TRUE(fs::is_empty(base / "t22_dir"));
  EXPECT_TRUE(fs::is_empty(base / "t22_real"));
  for (const fs::path& inside : {f.project / "exp", f.project / "artifacts" / "x", f.project / "artifacts" / ".." / "exp2", f.project}) {
    SCOPED_TRACE(inside.string());
    EXPECT_STOP(f.store->export_slots(inside, {"sb"}), error_category::security, "export.target_forbidden");
  }
  EXPECT_STOP(f.store->export_slots(original, {"sb"}), error_category::security, "export.target_forbidden");
  EXPECT_EQ(read_text(original), "original");
  EXPECT_FALSE(fs::exists(f.project / "exp"));
  f.store->export_slots(base / "t22_ok", {"sb"});
  EXPECT_EQ(verify_export(base / "t22_ok").files.size(), 1u);
}

TEST(Export, T_M2C_24_malicious_manifests_are_refused_without_reading_outside_the_directory) {
  fixture f = make_fixture("t24");
  const export_manifest base = verify_export(export_to(f, "t24"));
  const fs::path root = f.project.parent_path() / "t24_evil";
  const fs::path outside = f.project.parent_path() / "t24_outside";
  fs::remove_all(outside);
  fs::create_directories(outside);
  write_text(outside / "secret", "secret");
  auto with_path = [&](const std::string& rel) {
    std::vector<export_file_entry> files{base.files[0]};
    files[0].rel_path = rel;
    return manifest_text(files, base.nodes);
  };
  auto expect_path_refused = [&](const std::string& rel, const char* label, const std::function<void(const fs::path&)>& prepare = {}) {
    SCOPED_TRACE(label);
    fs::remove_all(root);
    write_bundle(root, with_path(rel));
    if (prepare) prepare(root);
    EXPECT_STOP(verify_export(root), error_category::security, "export.manifest_path");
  };
  expect_path_refused("../x", "..");
  expect_path_refused("..", "solo ..");
  expect_path_refused("a/../b", "a/../b");
  expect_path_refused("/etc/passwd", "assoluto");
  expect_path_refused(std::string("a\0b", 3), "NUL");
  expect_path_refused("a//b", "a//b");
  expect_path_refused("./a", "./a");
  expect_path_refused("a/./b", "a/./b");
  expect_path_refused("a/", "slash finale");
  expect_path_refused("", "vuoto");
  expect_path_refused(std::string(256, 'a'), "componente lungo");
  expect_path_refused(std::string(4097, 'a'), "percorso lungo");
  expect_path_refused("manifest.json", "riservato");
  expect_path_refused("COMMIT", "riservato");
  expect_path_refused("link", "symlink verso fuori", [&](const fs::path& d) { fs::create_symlink(outside / "secret", d / "link"); });
  expect_path_refused("d/secret", "directory symlink", [&](const fs::path& d) { fs::create_directory_symlink(outside, d / "d"); });
  expect_path_refused("fifo", "fifo", [](const fs::path& d) { ASSERT_EQ(::mkfifo((d / "fifo").c_str(), 0600), 0); });
  expect_path_refused("sub", "directory", [](const fs::path& d) { fs::create_directory(d / "sub"); });
  expect_path_refused("../t24_outside/secret", "uscita", {});

  {
    std::vector<export_file_entry> twice{base.files[0], base.files[0]};
    fs::remove_all(root);
    write_bundle(root, manifest_text(twice, base.nodes));
    EXPECT_STOP(verify_export(root), error_category::security, "export.manifest_path");
  }
  fs::remove_all(root);
  fs::create_directory_symlink(outside, root);
  EXPECT_STOP(verify_export(root), error_category::security, "export.manifest_path");
  fs::remove(root);

  fs::create_directories(root);
  {
    write_text(root / "manifest.json", "");
    ASSERT_EQ(::truncate((root / "manifest.json").c_str(), static_cast<off_t>(export_manifest_max_bytes) + 1), 0);
    EXPECT_STOP(verify_export(root), error_category::limit_exceeded, "export.manifest_size");
  }
  const std::string good = manifest_text(base.files, base.nodes);
  const json parsed = parse_json(good);
  auto replaced = [&](const std::string& from, const std::string& to) {
    std::string t = good;
    const auto pos = t.find(from);
    EXPECT_NE(pos, std::string::npos) << from;
    t.replace(pos, from.size(), to);
    return t;
  };
  const std::vector<std::pair<const char*, std::string>> malformed{
      {"non json", "not json"},
      {"vuoto", ""},
      {"oggetto vuoto", "{}"},
      {"formato", replaced("astro.export.v1", "astro.export.v2")},
      {"files non array", replaced("\"files\":[", "\"files\":{\"x\":[")},
      {"chiave duplicata", replaced("\"versions\":{", "\"versions\":{\"product\":\"y\",\"product\":\"y\",")},
      {"campo extra nel file", replaced("\"slot_name\"", "\"extra\":1,\"slot_name\"")},
      {"sha256 non valido", replaced(base.files[0].sha256, "XYZ")},
      {"size negativo", replaced("\"size_bytes\":" + std::to_string(base.files[0].size_bytes) + ",\"slot_name\"", "\"size_bytes\":-1,\"slot_name\"")},
      {"size frazionario", replaced("\"size_bytes\":" + std::to_string(base.files[0].size_bytes) + ",\"slot_name\"", "\"size_bytes\":1.5,\"slot_name\"")},
      {"nodo del file ignoto", replaced("\"node_key\":\"" + base.files[0].node_key + "\",\"rel_path\"", "\"node_key\":\"" + std::string(64, '0') + "\",\"rel_path\"")},
      {"troncato", good.substr(0, good.size() / 2)},
      {"ledger non null", replaced("\"correction_ledger\":null", "\"correction_ledger\":[]")},
  };
  for (const auto& [label, text] : malformed) {
    SCOPED_TRACE(label);
    fs::remove_all(root);
    write_bundle(root, text);
    EXPECT_STOP(verify_export(root), error_category::invalid_input, "export.manifest_malformed");
  }
  fs::remove_all(root);
  const std::string c_key = keys_of(diamond(2).resolve()).at("c");
  write_bundle(root, replaced("\"node_key\":\"" + c_key + "\",\"node_spec\"", "\"node_key\":\"" + std::string(64, '0') + "\",\"node_spec\""));
  EXPECT_STOP(verify_export(root), error_category::integrity, "export.node_key_mismatch");
  EXPECT_EQ(read_text(outside / "secret"), "secret");
  (void)parsed;
}

}  // namespace
