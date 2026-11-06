// U8a (M3.a), F-IO-03 CL-04 e F-IO-08 (limite, decisione del coordinatore del 30 ottobre 2026): `add_input` accetta solo
// percorsi assoluti. Un percorso non assoluto, anche vuoto, è `invalid_input` `store.bad_input` e non scrive nulla.
// Il criterio è lo stesso di RL-05 (F-IO-08): il percorso si valuta com'è dato, senza normalizzazione.
#include <gtest/gtest.h>

#include "store_test_support.hpp"

namespace {

using namespace astro;
namespace fs = std::filesystem;

constexpr const char* kSha = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

struct env {
  fs::path dir;
  std::unique_ptr<sqlite_dag_store> store;
  fs::path file;
};

env make_env(const std::string& name) {
  env e;
  e.dir = fs::path(ASTRO_TEST_TMP) / "u8a" / name;
  fs::remove_all(e.dir);
  fs::create_directories(e.dir);
  e.store = sqlite_dag_store::create(e.dir / "project");
  e.file = e.dir / "original.bin";
  store_test::write_text(e.file, "originale");
  return e;
}

input_record record_for(const std::string& path) {
  input_record r;
  r.path = path;
  r.size_bytes = 9;
  r.sha256 = kSha;
  r.role = "batch";
  r.format = "fits";
  return r;
}

// Il rifiuto è quello di `require_absolute_input_path` e non di un altro controllo di `add_input` (formato, sha, assenza):
// stesso codice `store.bad_input` ma messaggio e contesto del solo percorso.
void expect_path_stop(sqlite_dag_store& store, const std::string& path, const std::string& message) {
  SCOPED_TRACE("percorso: \"" + path + "\"");
  try {
    (void)store.add_input(record_for(path));
    ADD_FAILURE() << "stop_error atteso, nessuna eccezione";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::invalid_input) << e.what();
    EXPECT_EQ(e.code(), "store.bad_input") << e.what();
    EXPECT_EQ(e.message(), message) << e.what();
    ASSERT_EQ(e.context().size(), 1u) << e.what();
    EXPECT_EQ(e.context().front().first, "path");
    EXPECT_EQ(e.context().front().second, path);
  }
}

}  // namespace

TEST(InputPaths, add_input_rejects_relative_path_that_resolves_from_the_current_directory) {
  env e = make_env("relative_resolves");
  const fs::path cwd = fs::current_path();
  fs::current_path(e.dir);
  // Il file esiste rispetto alla directory corrente, ma un percorso relativo dipende dalla directory del processo: lo stesso
  // progetto aperto da un'altra directory vedrebbe l'input come assente.
  expect_path_stop(*e.store, "original.bin", "il percorso dell'input non è assoluto");
  expect_path_stop(*e.store, "./original.bin", "il percorso dell'input non è assoluto");
  fs::current_path(cwd);
  EXPECT_TRUE(e.store->inputs().empty());
}

TEST(InputPaths, add_input_rejects_relative_path_with_dotdot_and_empty_path) {
  env e = make_env("relative_dotdot");
  const fs::path cwd = fs::current_path();
  fs::current_path(e.dir / "project");
  expect_path_stop(*e.store, "../original.bin", "il percorso dell'input non è assoluto");
  fs::current_path(cwd);
  expect_path_stop(*e.store, "", "il percorso dell'input è vuoto");
  EXPECT_TRUE(e.store->inputs().empty());
}

TEST(InputPaths, add_input_keeps_accepting_absolute_paths_and_registers_them_as_given) {
  env e = make_env("absolute_ok");
  const input_record got = e.store->add_input(record_for(e.file.string()));
  EXPECT_EQ(got.path, e.file.string());
  ASSERT_EQ(e.store->inputs().size(), 1u);
  EXPECT_EQ(e.store->inputs().front().path, e.file.string());
}

TEST(InputPaths, add_input_absolute_missing_file_stays_an_io_stop_not_a_path_error) {
  env e = make_env("absolute_missing");
  EXPECT_STOP(e.store->add_input(record_for((e.dir / "assente.bin").string())), error_category::io, "input.missing");
}
