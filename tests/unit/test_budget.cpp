#include <gtest/gtest.h>
#include <sqlite3.h>

#include "astro/budget.hpp"
#include "astro/sqlite_dag_store.hpp"
#include "m2d_support.hpp"

namespace {

using namespace m2d;
constexpr std::uint64_t GiB = std::uint64_t{1} << 30;

TEST(Budget, T_M2D_01_manual_grammar_accepts_exact_forms) {
  EXPECT_EQ(parse_manual_budget("512MiB"), (manual_budget{budget_unit::mib, 512, std::nullopt}));
  EXPECT_EQ(parse_manual_budget("4GiB"), (manual_budget{budget_unit::gib, 4, std::nullopt}));
  EXPECT_EQ(parse_manual_budget("50%:mem_total"),
            (manual_budget{budget_unit::percent, 50, budget_denominator::mem_total}));
  EXPECT_EQ(parse_manual_budget("1%:mem_available"),
            (manual_budget{budget_unit::percent, 1, budget_denominator::mem_available}));
  EXPECT_EQ(parse_manual_budget("100%:cgroup_max"),
            (manual_budget{budget_unit::percent, 100, budget_denominator::cgroup_max}));
  for (const char* text : {"1MiB", "17GiB", "100%:mem_total", "7%:cgroup_max"}) {
    EXPECT_EQ(to_string(parse_manual_budget(text)), text);
  }
}

TEST(Budget, T_M2D_01_manual_grammar_rejects_other_units_and_forms) {
  for (const char* text : {"512MB", "4GB", "512M", "4G", "512", "1.5GiB", "512 MiB", " 512MiB", "512MiB ", "-1MiB",
                           "+1MiB", "MiB", "", "512mib", "512KiB", "50%", "50%:", "50%:other", "50:mem_total",
                           "50 %:mem_total", "50%mem_total", "50%:mem_total ", "50%:MEM_TOTAL", "0x10MiB", "1e3MiB",
                           "%:mem_total", "50%%:mem_total"}) {
    SCOPED_TRACE(std::string("[") + text + "]");
    EXPECT_STOP(parse_manual_budget(text), error_category::invalid_input, "budget.syntax");
  }
}

TEST(Budget, T_M2D_01_zero_over_100_percent_and_unrepresentable_values_are_out_of_range) {
  for (const char* text : {"0MiB", "0GiB", "0%:mem_total", "101%:mem_total", "1000%:mem_available",
                           "99999999999999999999MiB", "17592186044416MiB", "17179869184GiB", "18446744073709551616GiB"}) {
    SCOPED_TRACE(text);
    EXPECT_STOP(parse_manual_budget(text), error_category::invalid_input, "budget.out_of_range");
  }
  EXPECT_NO_THROW(parse_manual_budget("17592186044415MiB"));
}

TEST(Budget, T_M2D_01_manual_value_is_checked_against_mem_total_and_denominators) {
  fake_system sys("budget_manual");
  sys.meminfo(8 * 1024 * 1024, 4 * 1024 * 1024);  // 8 GiB totali, 4 GiB disponibili
  const budget_sources src = sys.sources();

  budget_snapshot s = compute_manual_budget(parse_manual_budget("512MiB"), src);
  EXPECT_EQ(s.mode, budget_mode::manual);
  EXPECT_EQ(s.budget_bytes, 512 * MiB);
  EXPECT_EQ(s.manual, parse_manual_budget("512MiB"));
  EXPECT_EQ(s.mem_total_bytes, 8 * GiB);
  EXPECT_EQ(s.mem_available_bytes, 4 * GiB);
  EXPECT_EQ(compute_manual_budget(parse_manual_budget("8GiB"), src).budget_bytes, 8 * GiB);
  EXPECT_EQ(compute_manual_budget(parse_manual_budget("50%:mem_total"), src).budget_bytes, 4 * GiB);
  EXPECT_EQ(compute_manual_budget(parse_manual_budget("25%:mem_available"), src).budget_bytes, 1 * GiB);
  EXPECT_EQ(compute_manual_budget(parse_manual_budget("1%:mem_total"), src).budget_bytes, 81 * MiB);

  EXPECT_STOP(compute_manual_budget(parse_manual_budget("9GiB"), src), error_category::invalid_input,
              "budget.out_of_range");
  EXPECT_STOP(compute_manual_budget(parse_manual_budget("8193MiB"), src), error_category::invalid_input,
              "budget.out_of_range");
  EXPECT_STOP(compute_manual_budget(manual_budget{budget_unit::mib, 0, std::nullopt}, src),
              error_category::invalid_input, "budget.out_of_range");
  EXPECT_STOP(compute_manual_budget(manual_budget{budget_unit::percent, 50, std::nullopt}, src),
              error_category::invalid_input, "budget.syntax");

  EXPECT_STOP(compute_manual_budget(parse_manual_budget("50%:cgroup_max"), src), error_category::metadata_missing,
              "budget.no_cgroup_limit");

  sys.self_cgroup("0::/app\n");
  sys.cgroup("app", "6442450944\n", "1073741824\n");  // 6 GiB
  s = compute_manual_budget(parse_manual_budget("50%:cgroup_max"), src);
  EXPECT_EQ(s.budget_bytes, 3 * GiB);
  EXPECT_EQ(s.cgroup_max_bytes, 6 * GiB);

  sys.cgroup("app", "17179869184\n", "0\n");  // 16 GiB: il 100 % supera mem_total
  EXPECT_STOP(compute_manual_budget(parse_manual_budget("100%:cgroup_max"), src), error_category::invalid_input,
              "budget.out_of_range");
  sys.cgroup("app", "52428800\n", "0\n");  // 50 MiB: l'1 % arrotondato per difetto a 1 MiB e' zero
  EXPECT_STOP(compute_manual_budget(parse_manual_budget("1%:cgroup_max"), src), error_category::invalid_input,
              "budget.out_of_range");
}

TEST(Budget, T_M2D_02_manual_budget_is_persisted_and_revalidated_on_reopen) {
  const fs::path root = fresh_dir("budget_store") / "project";
  {
    auto store = sqlite_dag_store::create(root);
    EXPECT_EQ(load_manual_budget(*store), std::nullopt);
    save_manual_budget(*store, parse_manual_budget("50%:mem_total"));
    EXPECT_EQ(load_manual_budget(*store), parse_manual_budget("50%:mem_total"));
    EXPECT_STOP(save_manual_budget(*store, manual_budget{budget_unit::mib, 0, std::nullopt}),
                error_category::invalid_input, "budget.out_of_range");
    EXPECT_EQ(load_manual_budget(*store), parse_manual_budget("50%:mem_total"));
    EXPECT_STOP(store->put_setting("x", "{ \"a\": 1 }"), error_category::invalid_input, "store.setting_not_canonical");
    EXPECT_EQ(*store->get_setting(manual_budget_setting_key), "{\"spec\":\"50%:mem_total\"}");
    save_manual_budget(*store, parse_manual_budget("512MiB"));
  }
  {
    auto store = sqlite_dag_store::open(root, open_intent::read_write);
    EXPECT_EQ(load_manual_budget(*store), parse_manual_budget("512MiB"));
    fake_system sys("budget_store_sys");
    EXPECT_EQ(compute_manual_budget(*load_manual_budget(*store), sys.sources()).budget_bytes, 512 * MiB);
  }
  {
    auto store = sqlite_dag_store::open(root, open_intent::read_only);
    EXPECT_EQ(load_manual_budget(*store), parse_manual_budget("512MiB"));
    EXPECT_STOP(save_manual_budget(*store, parse_manual_budget("1GiB")), error_category::schema, "store.read_only");
  }
}

TEST(Budget, T_M2D_02_a_tampered_setting_is_rejected_on_read_not_repaired) {
  const fs::path root = fresh_dir("budget_tamper") / "project";
  sqlite_dag_store::create(root);
  const auto tamper = [&](const std::string& value) {
    sqlite3* db = nullptr;
    ASSERT_EQ(sqlite3_open((root / "project.db").c_str(), &db), SQLITE_OK);
    sqlite3_stmt* st = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(db,
                                 "INSERT INTO settings(key, value_canonical) VALUES('budget.manual', ?1) "
                                 "ON CONFLICT(key) DO UPDATE SET value_canonical=excluded.value_canonical",
                                 -1, &st, nullptr),
              SQLITE_OK);
    sqlite3_bind_text(st, 1, value.c_str(), -1, SQLITE_TRANSIENT);
    ASSERT_EQ(sqlite3_step(st), SQLITE_DONE);
    sqlite3_finalize(st);
    sqlite3_close(db);
  };
  tamper("{\"spec\":\"512MB\"}");
  EXPECT_STOP(load_manual_budget(*sqlite_dag_store::open(root, open_intent::read_write)), error_category::invalid_input,
              "budget.syntax");
  tamper("{\"spec\":\"0MiB\"}");
  EXPECT_STOP(load_manual_budget(*sqlite_dag_store::open(root, open_intent::read_write)), error_category::invalid_input,
              "budget.out_of_range");
  for (const char* bad : {"{\"spec\":5}", "{\"spec\":\"1GiB\",\"extra\":1}", "{}", "[]", "\"1GiB\""}) {
    tamper(bad);
    SCOPED_TRACE(bad);
    EXPECT_STOP(load_manual_budget(*sqlite_dag_store::open(root, open_intent::read_write)),
                error_category::invalid_input, "budget.setting_malformed");
  }
  tamper("not json");
  EXPECT_STOP(load_manual_budget(*sqlite_dag_store::open(root, open_intent::read_write)), error_category::invalid_input,
              "json.malformed");
}

TEST(Budget, T_M2D_03_automatic_formula_without_cgroup_and_psi) {
  fake_system sys("budget_auto");
  const budget_snapshot s = compute_automatic_budget(sys.sources());
  EXPECT_EQ(s.mode, budget_mode::automatic);
  EXPECT_EQ(s.mem_total_bytes, 8 * GiB);
  EXPECT_EQ(s.mem_available_bytes, 4 * GiB);
  EXPECT_EQ(s.cgroup_max_bytes, std::nullopt);
  EXPECT_EQ(s.cgroup_current_bytes, std::nullopt);
  EXPECT_EQ(s.psi_some_avg10, std::nullopt);
  EXPECT_DOUBLE_EQ(s.fraction, 0.375);
  EXPECT_EQ(s.budget_bytes, 1536 * MiB);
  EXPECT_EQ(s.manual, std::nullopt);
}

TEST(Budget, T_M2D_03_automatic_rounds_down_to_a_whole_mebibyte) {
  fake_system sys("budget_round");
  sys.meminfo(8 * 1024 * 1024, 4 * 1024 * 1024 + 8191);  // 4 GiB + 8191 kB: 3/8 = +2.9996 MiB
  EXPECT_EQ(compute_automatic_budget(sys.sources()).budget_bytes, 1536 * MiB + 2 * MiB);
  sys.meminfo(8 * 1024 * 1024, 4 * 1024 * 1024 + 8192);  // +8 MiB: 3/8 = +3 MiB
  EXPECT_EQ(compute_automatic_budget(sys.sources()).budget_bytes, 1536 * MiB + 3 * MiB);
  EXPECT_EQ(compute_automatic_budget(sys.sources()).budget_bytes % MiB, 0u);
}

TEST(Budget, T_M2D_03_pressure_selects_the_lower_fraction_at_the_threshold) {
  fake_system sys("budget_psi");
  sys.psi("some avg10=9.99 avg60=1.00 avg300=0.50 total=12345\nfull avg10=50.00 avg60=0.00 avg300=0.00 total=1\n");
  budget_snapshot s = compute_automatic_budget(sys.sources());
  EXPECT_EQ(s.psi_some_avg10, 9.99);
  EXPECT_DOUBLE_EQ(s.fraction, 0.375);
  EXPECT_EQ(s.budget_bytes, 1536 * MiB);
  sys.psi("some avg10=10.00 avg60=1.00 avg300=0.50 total=12345\n");
  s = compute_automatic_budget(sys.sources());
  EXPECT_EQ(s.psi_some_avg10, 10.0);
  EXPECT_DOUBLE_EQ(s.fraction, 0.25);
  EXPECT_EQ(s.budget_bytes, 1 * GiB);
  sys.psi("some avg10=87.31 avg60=60.00 avg300=0.50 total=12345\n");
  EXPECT_EQ(compute_automatic_budget(sys.sources()).budget_bytes, 1 * GiB);
}

TEST(Budget, T_M2D_03_cgroup_headroom_bounds_the_available_memory) {
  fake_system sys("budget_cgroup");
  sys.self_cgroup("0::/app/job\n");
  sys.cgroup("app/job", "3221225472\n", "1073741824\n");  // 3 GiB limite, 1 GiB in uso: 2 GiB di margine
  budget_snapshot s = compute_automatic_budget(sys.sources());
  EXPECT_EQ(s.cgroup_max_bytes, 3 * GiB);
  EXPECT_EQ(s.cgroup_current_bytes, 1 * GiB);
  EXPECT_EQ(s.budget_bytes, 768 * MiB);  // min(4, 2) GiB * 0.375

  sys.cgroup("app/job", "max\n", "1073741824\n");  // senza limite al livello proprio: nessun cgroup
  s = compute_automatic_budget(sys.sources());
  EXPECT_EQ(s.cgroup_max_bytes, std::nullopt);
  EXPECT_EQ(s.budget_bytes, 1536 * MiB);

  sys.cgroup("app", "1610612736\n", "536870912\n");  // l'antenato ha 1 GiB di margine: stringe piu' del livello proprio
  s = compute_automatic_budget(sys.sources());
  EXPECT_EQ(s.cgroup_max_bytes, 1536 * MiB);
  EXPECT_EQ(s.cgroup_current_bytes, 512 * MiB);
  EXPECT_EQ(s.budget_bytes, 384 * MiB);

  sys.cgroup("app/job", "10737418240\n", "0\n");  // il livello proprio non stringe: resta l'antenato
  EXPECT_EQ(compute_automatic_budget(sys.sources()).budget_bytes, 384 * MiB);
  sys.cgroup("app/job", "2147483648\n", "1610612736\n");  // 512 MiB di margine contro 1 GiB dell'antenato
  s = compute_automatic_budget(sys.sources());
  EXPECT_EQ(s.cgroup_max_bytes, 2 * GiB);
  EXPECT_EQ(s.budget_bytes, 192 * MiB);
}

TEST(Budget, T_M2D_03_absent_cgroup_sources_are_nullopt_not_errors) {
  fake_system sys("budget_nocgroup");
  sys.self_cgroup("12:memory:/legacy\n1:name=systemd:/legacy\n");  // niente cgroup v2
  EXPECT_EQ(compute_automatic_budget(sys.sources()).cgroup_max_bytes, std::nullopt);
  sys.self_cgroup("0::/missing/dir\n");  // il cgroup non ha file di memoria
  EXPECT_EQ(compute_automatic_budget(sys.sources()).cgroup_max_bytes, std::nullopt);
  EXPECT_EQ(compute_automatic_budget(sys.sources()).budget_bytes, 1536 * MiB);
}

TEST(Budget, T_M2D_03_missing_or_incomplete_meminfo_stops) {
  fake_system sys("budget_meminfo");
  fs::remove(sys.root / "proc" / "meminfo");
  EXPECT_STOP(compute_automatic_budget(sys.sources()), error_category::io, "budget.meminfo");
  write_text(sys.root / "proc" / "meminfo", "MemTotal:   8388608 kB\nMemFree: 12 kB\n");
  EXPECT_STOP(compute_automatic_budget(sys.sources()), error_category::io, "budget.meminfo");
  write_text(sys.root / "proc" / "meminfo", "MemAvailable:   8388608 kB\n");
  EXPECT_STOP(compute_automatic_budget(sys.sources()), error_category::io, "budget.meminfo");
  write_text(sys.root / "proc" / "meminfo", "MemTotal:   8388608 kB\nMemAvailable:   lots kB\n");
  EXPECT_STOP(compute_automatic_budget(sys.sources()), error_category::io, "budget.meminfo");
  write_text(sys.root / "proc" / "meminfo", "MemTotal:   8388608 kB\nMemAvailable:   123 MB\n");
  EXPECT_STOP(compute_automatic_budget(sys.sources()), error_category::io, "budget.meminfo");
  write_text(sys.root / "proc" / "meminfo", "");
  EXPECT_STOP(compute_automatic_budget(sys.sources()), error_category::io, "budget.meminfo");
  EXPECT_STOP(compute_manual_budget(parse_manual_budget("1MiB"), sys.sources()), error_category::io, "budget.meminfo");
}

TEST(Budget, T_M2D_03_cgroup_at_or_over_its_limit_stops_without_subtracting) {
  fake_system sys("budget_over");
  sys.self_cgroup("0::/app\n");
  for (const auto& [max, current] : {std::pair<const char*, const char*>{"1073741824\n", "1073741824\n"},
                                     {"1073741824\n", "1073741825\n"},
                                     {"1\n", "18446744073709551615\n"}}) {
    sys.cgroup("app", max, current);
    try {
      (void)compute_automatic_budget(sys.sources());
      ADD_FAILURE() << "stop attesa";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.category(), error_category::budget);
      EXPECT_EQ(e.code(), "budget.cgroup_over_limit");
      std::string ctx;
      for (const auto& [k, v] : e.context()) ctx += k + "=" + v + ";";
      EXPECT_NE(ctx.find(std::string("cgroup_max=") + std::to_string(std::stoull(max))), std::string::npos) << ctx;
      EXPECT_NE(ctx.find(std::string("cgroup_current=") + std::to_string(std::stoull(current))), std::string::npos)
          << ctx;
    }
  }
}

TEST(Budget, T_M2D_03_unparseable_cgroup_and_psi_files_stop) {
  fake_system sys("budget_parse");
  sys.self_cgroup("0::/app\n");
  sys.cgroup("app", "lots\n", "1\n");
  EXPECT_STOP(compute_automatic_budget(sys.sources()), error_category::invalid_input, "budget.cgroup_parse");
  sys.cgroup("app", "1048576 \n", "1\n");
  EXPECT_STOP(compute_automatic_budget(sys.sources()), error_category::invalid_input, "budget.cgroup_parse");
  sys.cgroup("app", "-5\n", "1\n");
  EXPECT_STOP(compute_automatic_budget(sys.sources()), error_category::invalid_input, "budget.cgroup_parse");
  sys.cgroup("app", "99999999999999999999999\n", "1\n");
  EXPECT_STOP(compute_automatic_budget(sys.sources()), error_category::invalid_input, "budget.cgroup_parse");
  sys.cgroup("app", "1073741824\n", "");  // memory.current assente
  EXPECT_STOP(compute_automatic_budget(sys.sources()), error_category::invalid_input, "budget.cgroup_parse");
  sys.cgroup("app", "1073741824\n", "abc\n");
  EXPECT_STOP(compute_automatic_budget(sys.sources()), error_category::invalid_input, "budget.cgroup_parse");
  sys.self_cgroup("0::/../etc\n");
  EXPECT_STOP(compute_automatic_budget(sys.sources()), error_category::invalid_input, "budget.cgroup_parse");

  for (const char* bad : {"garbage\n", "x:y:/a\n", "0:/a\n", "0::/app\nnonsense\n", ":memory:/a\n",
                          "0:memory:/app\n", "0::app\n", "0::\n", "0::/a\n0::/b\n", "1::/a\n", "1:memory:\n",
                          "1:memory:rel\n", "01:memory:/a\n", "-1:memory:/a\n"}) {
    SCOPED_TRACE(bad);
    sys.self_cgroup(bad);
    EXPECT_STOP(compute_automatic_budget(sys.sources()), error_category::invalid_input, "budget.cgroup_parse");
  }

  sys.self_cgroup("");
  for (const char* bad : {"", "garbage\n", "full avg10=1.00\n", "some avg60=1.00\n", "some avg10=abc avg60=1\n",
                          "some avg10= avg60=1\n", "some avg10=-1.0\n", "some avg10=nan\n", "some avg10=1e999\n",
                          "some avg10=1.0x\n"}) {
    sys.psi(bad);
    SCOPED_TRACE(bad);
    EXPECT_STOP(compute_automatic_budget(sys.sources()), error_category::invalid_input, "budget.psi_parse");
  }
}

TEST(Budget, snapshot_json_records_every_field) {
  fake_system sys("budget_json");
  sys.psi("some avg10=12.50 avg60=1.00 avg300=0.50 total=1\n");
  sys.self_cgroup("0::/app\n");
  sys.cgroup("app", "3221225472\n", "1073741824\n");
  const std::string automatic = canonical_json(budget_json(compute_automatic_budget(sys.sources())));
  EXPECT_EQ(automatic,
            "{\"budget_bytes\":536870912,\"cgroup_current_bytes\":1073741824,\"cgroup_max_bytes\":3221225472,"
            "\"fraction\":0.25,\"mem_available_bytes\":4294967296,\"mem_total_bytes\":8589934592,\"mode\":\"automatic\","
            "\"psi_some_avg10\":12.5}");
  const std::string manual = canonical_json(budget_json(compute_manual_budget(parse_manual_budget("2GiB"), sys.sources())));
  EXPECT_EQ(manual,
            "{\"budget_bytes\":2147483648,\"cgroup_current_bytes\":1073741824,\"cgroup_max_bytes\":3221225472,"
            "\"manual\":\"2GiB\",\"mem_available_bytes\":4294967296,\"mem_total_bytes\":8589934592,\"mode\":\"manual\","
            "\"psi_some_avg10\":12.5}");
}

TEST(Budget, real_system_sources_give_a_whole_mebibyte_budget) {
  const budget_snapshot s = compute_automatic_budget();
  EXPECT_GT(s.mem_total_bytes, 0u);
  EXPECT_LE(s.budget_bytes, s.mem_total_bytes);
  EXPECT_EQ(s.budget_bytes % MiB, 0u);
}

}  // namespace
