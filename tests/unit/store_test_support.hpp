#pragma once

#include <gtest/gtest.h>
#include <sqlite3.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>

#include "astro/hash.hpp"
#include "astro/job.hpp"
#include "astro/node.hpp"
#include "astro/sqlite_dag_store.hpp"
#include "expect_stop.hpp"

namespace store_test {

using namespace astro;
namespace fs = std::filesystem;

// Cancella anche le directory sorelle `<name>_*` del test: un nome non deve essere prefisso + "_" del nome di un altro
// test, altrimenti a ctest parallelo uno cancella la directory in uso dall'altro.
inline fs::path fresh_dir(const std::string& name) {
  const fs::path dir = fs::path(ASTRO_TEST_TMP) / "m2c" / name;
  fs::create_directories(dir.parent_path());
  for (const auto& e : fs::directory_iterator(dir.parent_path())) {
    const std::string n = e.path().filename().string();
    if (n == name || n.rfind(name + "_", 0) == 0) fs::remove_all(e.path());
  }
  return dir;
}

inline std::vector<std::uint8_t> bytes_of(const std::string& text) {
  return {text.begin(), text.end()};
}

inline std::string read_text(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

inline void write_text(const fs::path& path, const std::string& text) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

inline json params_of(double a) {
  json p = json::object();
  p.set("a", json::number(a));
  return p;
}

inline content_digest fake_digest(char fill) { return {std::string(64, fill), 10}; }

inline recipe_entry entry(const std::string& op, double a, std::vector<recipe_input> inputs) {
  return recipe_entry(op, 1, 1, params_of(a), std::move(inputs), sample_type::f32, "cpu_scalar", std::nullopt);
}

// a -> (b, c) -> d
inline recipe_graph diamond(double b_param) {
  recipe_graph g;
  g.add("a", entry("op.a", 1, {recipe_input{"file", fake_digest('a')}}));
  g.add("b", entry("op.b", b_param, {recipe_input{"in", std::string("a")}}));
  g.add("c", entry("op.c", 3, {recipe_input{"in", std::string("a")}}));
  g.add("d", entry("op.d", 4, {recipe_input{"x", std::string("b")}, recipe_input{"y", std::string("c")}}));
  return g;
}

inline const std::map<std::string, std::string>& diamond_slots() {
  static const std::map<std::string, std::string> slots{{"sa", "a"}, {"sb", "b"}, {"sc", "c"}, {"sd", "d"}};
  return slots;
}

inline std::map<std::string, std::string> keys_of(const std::vector<resolved_node>& nodes) {
  std::map<std::string, std::string> keys;
  for (const auto& n : nodes) keys[n.local_id] = n.spec.key();
  return keys;
}

// Un job completo: avvio, scrittura atomica, commit. Restituisce l'artefatto pubblicato.
inline artifact_record run_and_commit(sqlite_dag_store& store, const std::string& slot, const std::string& content) {
  const job_record job = store.start_job(slot);
  const auto bytes = bytes_of(content);
  const auto files = store.write_files({artifact_content{bytes, "bin", "image", std::nullopt}});
  return store.commit_job(job.id, files).front();
}

struct raw_db {
  sqlite3* db = nullptr;
  explicit raw_db(const fs::path& project, bool readonly = false) {
    const std::string path = (project / "project.db").string();
    int rc;
    if (readonly) {
      rc = sqlite3_open_v2(("file:" + path + "?mode=ro&immutable=1").c_str(), &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, nullptr);
    } else {
      rc = sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr);
    }
    EXPECT_EQ(rc, SQLITE_OK);
  }
  ~raw_db() { sqlite3_close(db); }
  raw_db(const raw_db&) = delete;
  raw_db& operator=(const raw_db&) = delete;

  int exec(const std::string& sql) { return sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr); }

  std::string query(const std::string& sql) {
    sqlite3_stmt* st = nullptr;
    EXPECT_EQ(sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr), SQLITE_OK) << sql;
    std::string out;
    while (sqlite3_step(st) == SQLITE_ROW) {
      for (int c = 0; c < sqlite3_column_count(st); ++c) {
        const auto* t = sqlite3_column_text(st, c);
        out += (t != nullptr ? reinterpret_cast<const char*>(t) : "<null>");
        out += '|';
      }
      out += '\n';
    }
    sqlite3_finalize(st);
    return out;
  }
};

// Stato logico completo (tutte le tabelle, ordinate) di un progetto chiuso.
inline std::string dump_logical(const fs::path& project) {
  raw_db db(project, true);
  std::string out;
  for (const char* sql : {"SELECT * FROM inputs ORDER BY id", "SELECT * FROM nodes ORDER BY node_key",
                          "SELECT * FROM node_edges ORDER BY child_key, ordinal", "SELECT * FROM artifacts ORDER BY id",
                          "SELECT * FROM slots ORDER BY slot_name", "SELECT * FROM jobs ORDER BY id",
                          "SELECT * FROM snapshots ORDER BY id", "SELECT * FROM snapshot_entries ORDER BY snapshot_id, slot_name",
                          "SELECT * FROM history_cursor"}) {
    out += db.query(sql) + "--\n";
  }
  return out;
}

inline std::string nodes_digest(const fs::path& project) {
  raw_db db(project, true);
  const std::string text = db.query("SELECT * FROM nodes ORDER BY node_key") + db.query("SELECT * FROM node_edges ORDER BY child_key, ordinal");
  return sha256_hex({reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
}

inline std::vector<std::string> list_dir(const fs::path& dir) {
  std::vector<std::string> names;
  for (const auto& e : fs::directory_iterator(dir)) names.push_back(e.path().filename().string());
  std::sort(names.begin(), names.end());
  return names;
}

struct child_outcome {
  bool killed = false;
  int exit_code = -1;
};

// Esegue `body` in un processo figlio. L'iniettore passato al corpo uccide il figlio con SIGKILL alla `kill_at`-esima
// chiamata (1-based; 0 = mai). Il figlio esce con 0 se il corpo termina, 70 su qualunque eccezione.
inline child_outcome run_child(const std::function<void(const fault_injector&)>& body, int kill_at) {
  const pid_t pid = ::fork();
  if (pid == 0) {
    int calls = 0;
    const fault_injector injector = [&](std::string_view) {
      if (++calls == kill_at) ::kill(::getpid(), SIGKILL);
      return 0;
    };
    try {
      body(injector);
    } catch (...) {
      ::_exit(70);
    }
    ::_exit(0);
  }
  int status = 0;
  EXPECT_EQ(::waitpid(pid, &status, 0), pid);
  child_outcome out;
  out.killed = WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL;
  out.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  return out;
}

// Elenca i punti di iniezione toccati da `body` eseguito in questo processo.
inline std::vector<std::string> record_points(const std::function<void(const fault_injector&)>& body) {
  std::vector<std::string> points;
  const fault_injector injector = [&](std::string_view p) {
    points.emplace_back(p);
    return 0;
  };
  body(injector);
  return points;
}

}  // namespace store_test
