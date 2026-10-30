#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "astro/build_info.hpp"
#include "astro/dag_store.hpp"
#include "astro/hash.hpp"
#include "astro/job.hpp"
#include "astro/provenance.hpp"
#include "expect_stop.hpp"
#include "no_crash_report.hpp"
#include "syn_fits.hpp"

namespace {

using namespace astro;
namespace fs = std::filesystem;
namespace fx = astro_fixture;

constexpr std::uint64_t big_budget = std::uint64_t{1} << 40;

node_spec decode_node(const content_digest& d) {
  return make_fits_decode_node(d, {}, {}, sample_type::f32, "cpu_scalar");
}

job_result result_for(const job_record& job) {
  artifact_record a;
  a.node_key = job.node_key;
  a.generation = job.generation;
  a.sha256 = std::string(64, '5');
  a.size_bytes = 8;
  a.kind = "image";
  return job_result{a, {}};
}

using work_fn = std::function<job_result(const job_record&)>;

job_outcome run_quiet(dag_store& store, const std::string& slot, const work_fn& work) {
  testing::internal::CaptureStderr();
  try {
    job_outcome out = run_job(store, slot, work);
    testing::internal::GetCapturedStderr();
    return out;
  } catch (...) {
    testing::internal::GetCapturedStderr();
    throw;
  }
}

std::string setup_single_slot(memory_dag_store& store) {
  const node_spec node = decode_node(content_digest{std::string(64, '7'), 2880});
  store.put_node(node);
  store.create_slot("s", node.key());
  return node.key();
}

// T-M2B-09 e' PARZIALE: qui si verificano le primitive (errore dell'input, confine del job, descendants);
// il blocco automatico dei soli dipendenti e' dello scheduler (M2.d).
TEST(Job, T_M2B_09_missing_input_stops_its_job_and_descendants_identify_the_dependent_branch) {
  const auto good_file = fx::make_synth_frame(fx::frame_kind::dark, 8, 6, 0).file;
  const fs::path dir = fs::path(ASTRO_TEST_TMP) / "job";
  fs::create_directories(dir);
  const fs::path good_path = dir / "good.fits";
  {
    std::ofstream out(good_path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(good_file.data()), static_cast<std::streamsize>(good_file.size()));
  }
  const fs::path missing_path = dir / "missing.fits";
  fs::remove(missing_path);
  const content_digest good_digest = digest_of(good_file);
  const content_digest missing_digest{std::string(64, '9'), 100};

  auto make = [](const char* op, recipe_input input) {
    return recipe_entry(op, 1, 1, json::object(), {std::move(input)}, sample_type::f32, "cpu_scalar", std::nullopt);
  };
  const recipe_entry dec_ok = make("fits.decode", recipe_input{"file", good_digest});
  const recipe_entry dec_missing = make("fits.decode", recipe_input{"file", missing_digest});
  auto child = [&](const char* parent) { return make("calib.child", recipe_input{"in", std::string(parent)}); };
  recipe_graph g;
  g.add("dec_ok", dec_ok);
  g.add("dec_missing", dec_missing);
  g.add("child_ok", child("dec_ok"));
  g.add("child_missing", child("dec_missing"));
  memory_dag_store store;
  store.apply_recipe(g.resolve(), {{"dec_ok", "dec_ok"}, {"dec_missing", "dec_missing"},
                                   {"child_ok", "child_ok"}, {"child_missing", "child_missing"}});

  const job_outcome failed = run_quiet(store, "dec_missing", [&](const job_record&) -> job_result {
    (void)read_verified_fits(missing_path.string(), missing_digest, big_budget);
    return {};
  });
  EXPECT_EQ(failed.exit_code, exit_code_stop);
  EXPECT_EQ(failed.stop->category(), error_category::io);
  EXPECT_EQ(failed.stop->code(), "input.missing");
  EXPECT_EQ(store.descendants(failed.job.node_key),
            std::vector<std::string>{store.get_slot("child_missing")->target_node_key});

  const job_outcome ok = run_quiet(store, "dec_ok", [&](const job_record& job) {
    (void)read_verified_fits(good_path.string(), good_digest, big_budget);
    return result_for(job);
  });
  EXPECT_EQ(ok.exit_code, 0);
  EXPECT_EQ(store.read_slot("dec_ok").state, freshness::current);
  EXPECT_EQ(store.read_slot("dec_missing").state, freshness::missing);
  EXPECT_EQ(store.read_slot("child_missing").state, freshness::missing);
}

TEST(Job, T_M2B_12_every_category_exits_2_and_cancelled_exits_3_with_job_marked) {
  const error_category all[] = {
      error_category::invalid_input, error_category::limit_exceeded, error_category::metadata_missing,
      error_category::incompatible, error_category::unsupported, error_category::numeric, error_category::io,
      error_category::integrity, error_category::schema, error_category::graph, error_category::stale_generation,
      error_category::budget, error_category::cancelled, error_category::resume_mismatch, error_category::security};
  for (const error_category category : all) {
    memory_dag_store store;
    setup_single_slot(store);
    testing::internal::CaptureStderr();
    const job_outcome out = run_job(store, "s", [&](const job_record&) -> job_result {
      throw stop_error(category, "test.condition", "guasto iniettato", {{"k", "v"}});
    });
    const std::string err = testing::internal::GetCapturedStderr();
    const bool cancelled = category == error_category::cancelled;
    EXPECT_EQ(out.exit_code, cancelled ? 3 : 2) << to_string(category);
    EXPECT_EQ(out.exit_code, exit_code_for(category));
    EXPECT_EQ(out.job.state, cancelled ? job_state::cancelled : job_state::failed) << to_string(category);
    EXPECT_EQ(out.job.stop_category, category);
    EXPECT_EQ(out.job.stop_code, "test.condition");
    ASSERT_TRUE(out.stop.has_value());
    EXPECT_EQ(out.stop->context().size(), 1u);
    EXPECT_NE(err.find("test.condition"), std::string::npos);
    EXPECT_EQ(store.get_job(out.job.id)->state, out.job.state);
    EXPECT_EQ(store.read_slot("s").state, freshness::missing);
    EXPECT_EQ(out.provenance.state, out.job.state);
    ASSERT_TRUE(out.provenance.stop.has_value());
    EXPECT_EQ(out.provenance.stop->category(), category);
    EXPECT_EQ(out.provenance.generation, out.job.generation);
  }
}

TEST(Job, T_M2B_12_stale_publish_at_the_boundary_is_a_stop_not_a_success) {
  memory_dag_store store;
  setup_single_slot(store);
  const job_outcome out = run_quiet(store, "s", [&](const job_record& job) {
    store.start_job("s");
    return result_for(job);
  });
  EXPECT_EQ(out.exit_code, exit_code_stop);
  EXPECT_EQ(out.job.stop_category, error_category::stale_generation);
  EXPECT_EQ(out.job.stop_code, "dag.stale_publish");
  EXPECT_EQ(store.read_slot("s").state, freshness::missing);
}

TEST(Job, T_M2B_12_non_stop_exceptions_mark_the_job_failed_and_propagate) {
  memory_dag_store store;
  setup_single_slot(store);
  EXPECT_THROW(run_quiet(store, "s", [](const job_record&) -> job_result { throw std::runtime_error("bug"); }),
               std::runtime_error);
  const auto job = store.get_job(1);
  ASSERT_TRUE(job.has_value());
  EXPECT_EQ(job->state, job_state::failed);
  EXPECT_EQ(job->stop_code, "job.unexpected_exception");
}

// Nodo dello slot non leggibile dopo l'avvio del job: assente, oppure lettura che lancia un'eccezione estranea.
class store_with_unreadable_node : public memory_dag_store {
 public:
  explicit store_with_unreadable_node(bool throws) : throws_(throws) {}
  std::optional<node_spec> get_node(const std::string& node_key) const override {
    if (throws_) throw std::runtime_error("lettura del nodo non riuscita");
    (void)node_key;
    return std::nullopt;
  }

 private:
  bool throws_;
};

// R5.d: un nodo assente dopo l'avvio chiude il job `failed` graph dag.missing_node con la chiave del nodo e lo stop si
// propaga; un'eccezione estranea nella lettura chiude il job `failed` job.unexpected_exception e si rilancia
// invariata. Il lavoro non parte in nessuno dei due casi. Ogni caso gira in un processo figlio con SIGSEGV e SIGABRT
// convertiti in uscite: un nodo assente dereferenziato non abbatte il processo dei test e non produce un core.
TEST(Job, T_M2B_12_unreadable_node_after_start_marks_the_job_failed_and_propagates) {
  for (const bool throws : {false, true}) {
    const auto child = [throws] {
      struct sigaction action {};
      action.sa_handler = [](int) { _exit(139); };
      sigemptyset(&action.sa_mask);
      if (sigaction(SIGSEGV, &action, nullptr) != 0) _exit(2);
      exit_instead_of_core_dump();
      store_with_unreadable_node store(throws);
      const std::string key = setup_single_slot(store);
      bool worked = false;
      const work_fn work = [&](const job_record& job) {
        worked = true;
        return result_for(job);
      };
      bool propagated = false;
      try {
        (void)run_job(store, "s", work);
      } catch (const stop_error& e) {
        propagated = !throws && e.category() == error_category::graph && e.code() == "dag.missing_node" &&
                     e.context() == stop_error::context_list{{"nodo", key}};
      } catch (const std::runtime_error& e) {
        propagated = throws && std::string(e.what()) == "lettura del nodo non riuscita";
      }
      const auto job = store.get_job(1);
      const bool recorded = job.has_value() && job->state == job_state::failed &&
                            job->stop_category == (throws ? error_category::integrity : error_category::graph) &&
                            job->stop_code == (throws ? "job.unexpected_exception" : "dag.missing_node") &&
                            job->node_key == key;
      if (!propagated) std::fprintf(stderr, "eccezione propagata diversa dall'originale\n");
      if (!recorded) std::fprintf(stderr, "job non registrato come fermato\n");
      if (worked) std::fprintf(stderr, "lavoro del job eseguito\n");
      _exit(propagated && recorded && !worked && store.artifacts_for(key).empty() ? 0 : 1);
    };
    EXPECT_EXIT(child(), testing::ExitedWithCode(0), "") << throws;
  }
}

// Slot inesistente: l'avvio del job fallisce, nessun job esiste e lo stop si propaga invariato.
class store_counting_starts : public memory_dag_store {
 public:
  job_record start_job(const std::string& slot_name) override {
    ++starts;
    return memory_dag_store::start_job(slot_name);
  }
  int starts = 0;
};

// Avvio fallito: una sola chiamata a start_job (nessun nuovo tentativo per registrare), errore e contesto invariati.
TEST(Job, T_M2B_12_failed_job_start_propagates_unchanged_without_a_job) {
  store_counting_starts store;
  setup_single_slot(store);
  try {
    (void)run_quiet(store, "assente", [](const job_record& job) { return result_for(job); });
    ADD_FAILURE() << "stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.code(), "dag.unknown_slot");
    EXPECT_EQ(e.context(), (stop_error::context_list{{"slot", "assente"}}));
  }
  EXPECT_EQ(store.starts, 1);
  EXPECT_FALSE(store.get_job(1).has_value());
}

// Nodo assente e chiusura del job fallita: lo stop originale si propaga con tutto il suo contesto (`nodo`) piu' il solo
// campo `registrazione_arresto` con l'errore secondario.
class store_failing_finish_without_node : public memory_dag_store {
 public:
  std::optional<node_spec> get_node(const std::string&) const override { return std::nullopt; }
  void finish_job(std::uint64_t, job_state, std::optional<error_category>, const std::string&) override {
    throw stop_error(error_category::io, "test.finish_failed", "chiusura del job non riuscita");
  }
};

TEST(Job, T_M2B_12_failed_record_keeps_the_original_context_and_adds_only_the_secondary_failure) {
  store_failing_finish_without_node store;
  const std::string key = setup_single_slot(store);
  try {
    (void)run_quiet(store, "s", [](const job_record& job) { return result_for(job); });
    ADD_FAILURE() << "stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::graph);
    EXPECT_EQ(e.code(), "dag.missing_node");
    EXPECT_EQ(e.message(), "nodo del job assente dallo store");
    EXPECT_EQ(e.context(), (stop_error::context_list{
                               {"nodo", key},
                               {"registrazione_arresto", "io test.finish_failed: chiusura del job non riuscita"}}));
  }
}

TEST(Job, T_M2B_10_runner_provenance_for_a_completed_job) {
  memory_dag_store store;
  const std::string key = setup_single_slot(store);
  const job_outcome out = run_quiet(store, "s", [](const job_record& job) {
    job_result r = result_for(job);
    r.warnings = {"avviso uno"};
    return r;
  });
  EXPECT_EQ(out.exit_code, 0);
  EXPECT_EQ(out.job.state, job_state::completed);
  EXPECT_FALSE(out.stop.has_value());
  EXPECT_EQ(store.read_slot("s").state, freshness::current);
  const provenance_record& p = out.provenance;
  EXPECT_EQ(p.spec.key(), key);
  EXPECT_EQ(p.state, job_state::completed);
  EXPECT_EQ(p.generation, out.job.generation);
  EXPECT_EQ(p.warnings, std::vector<std::string>{"avviso uno"});
  ASSERT_EQ(p.output_digests.size(), 1u);
  EXPECT_EQ(p.output_digests[0], (content_digest{std::string(64, '5'), 8}));
  EXPECT_FALSE(p.stop.has_value());
  EXPECT_EQ(p.versions.source_revision, build_info().revision);
  const std::string text = provenance_canonical(p);
  EXPECT_NE(text.find("\"node_key\":\"" + key + "\""), std::string::npos);
  EXPECT_NE(text.find("\"output_digests\":[{\"sha256\":\"" + std::string(64, '5') + "\",\"size_bytes\":8}]"),
            std::string::npos);
  EXPECT_NE(text.find("\"warnings\":[\"avviso uno\"]"), std::string::npos);
}

TEST(Job, T_M2B_10_runner_provenance_for_a_stopped_job) {
  memory_dag_store store;
  const std::string key = setup_single_slot(store);
  const job_outcome out = run_quiet(store, "s", [](const job_record&) -> job_result {
    throw stop_error(error_category::io, "input.missing", "file assente", {{"percorso", "/x"}});
  });
  const std::string text = provenance_canonical(out.provenance);
  EXPECT_EQ(out.provenance.state, job_state::failed);
  EXPECT_NE(text.find("\"state\":\"failed\""), std::string::npos);
  EXPECT_NE(text.find("\"stop\":{\"category\":\"io\",\"code\":\"input.missing\","
                      "\"context\":[{\"key\":\"percorso\",\"value\":\"/x\"}],\"message\":\"file assente\"}"),
            std::string::npos);
  EXPECT_EQ(text.find("output_digests"), std::string::npos);
  EXPECT_NE(text.find("\"node_key\":\"" + key + "\""), std::string::npos);
}

class store_failing_after_finish : public memory_dag_store {
 public:
  std::optional<job_record> get_job(std::uint64_t job_id) const override {
    if (armed) throw std::runtime_error("lettura del job non riuscita");
    return memory_dag_store::get_job(job_id);
  }
  bool armed = false;
};

TEST(Job, exception_after_completion_is_not_remarked_as_failed) {
  store_failing_after_finish store;
  setup_single_slot(store);
  EXPECT_THROW(run_quiet(store, "s", [&](const job_record& job) {
                 store.armed = true;
                 return result_for(job);
               }),
               std::runtime_error);
  store.armed = false;
  EXPECT_EQ(store.get_job(1)->state, job_state::completed);
}

TEST(Job, successful_job_is_completed_and_current) {
  memory_dag_store store;
  setup_single_slot(store);
  const job_outcome out = run_quiet(store, "s", [](const job_record& job) { return result_for(job); });
  EXPECT_EQ(out.exit_code, 0);
  EXPECT_EQ(store.get_job(out.job.id)->state, job_state::completed);
}

library_versions fixed_versions() {
  return {"1.2.3", std::string(40, 'c'), true, "4.6.3", "3.46.1", "OpenSSL 3.5.5"};
}

TEST(Provenance, T_M2B_10_completed_record_has_every_field) {
  const content_digest in{std::string(64, 'a'), 2880};
  const node_spec node = decode_node(in);
  provenance_record r{node, fixed_versions(), 4, job_state::completed, {"avviso uno"},
                      {content_digest{std::string(64, 'b'), 16}}, std::nullopt};
  const std::string expected =
      "{\"generation\":4,\"input_digests\":[{\"sha256\":\"" + in.sha256 + "\",\"size_bytes\":2880}],"
      "\"node_key\":\"" + node.key() + "\",\"node_spec\":" + node.canonical() + ","
      "\"output_digests\":[{\"sha256\":\"" + std::string(64, 'b') + "\",\"size_bytes\":16}],"
      "\"state\":\"completed\","
      "\"versions\":{\"cfitsio\":\"4.6.3\",\"openssl\":\"OpenSSL 3.5.5\",\"product\":\"1.2.3\","
      "\"source_dirty\":true,\"source_revision\":\"" + std::string(40, 'c') + "\",\"sqlite\":\"3.46.1\"},"
      "\"warnings\":[\"avviso uno\"]}";
  EXPECT_EQ(provenance_canonical(r), expected);
}

TEST(Provenance, unknown_dirty_state_is_declared_null) {
  const node_spec node = decode_node(content_digest{std::string(64, 'a'), 2880});
  library_versions v = fixed_versions();
  v.source_dirty = std::nullopt;
  provenance_record r{node, v, 1, job_state::completed, {}, {}, std::nullopt};
  EXPECT_NE(provenance_canonical(r).find("\"source_dirty\":null"), std::string::npos);
}

TEST(Provenance, empty_source_revision_is_rejected) {
  const node_spec node = decode_node(content_digest{std::string(64, 'a'), 2880});
  library_versions v = fixed_versions();
  v.source_revision.clear();
  provenance_record r{node, v, 1, job_state::completed, {}, {}, std::nullopt};
  EXPECT_STOP(provenance_json(r), error_category::invalid_input, "dag.bad_provenance");
}

TEST(Provenance, T_M2B_10_incoherent_records_are_rejected) {
  const node_spec node = decode_node(content_digest{std::string(64, 'a'), 2880});
  provenance_record stopped_without_stop{node, fixed_versions(), 1, job_state::failed, {}, {}, std::nullopt};
  EXPECT_STOP(provenance_json(stopped_without_stop), error_category::invalid_input, "dag.bad_provenance");
  provenance_record completed_with_stop{node, fixed_versions(), 1, job_state::completed, {}, {},
                                        stop_error(error_category::io, "x.y", "m")};
  EXPECT_STOP(provenance_json(completed_with_stop), error_category::invalid_input, "dag.bad_provenance");
  provenance_record stopped_with_output{node, fixed_versions(), 1, job_state::failed, {},
                                        {content_digest{std::string(64, 'b'), 1}},
                                        stop_error(error_category::io, "x.y", "m")};
  EXPECT_STOP(provenance_json(stopped_with_output), error_category::invalid_input, "dag.bad_provenance");
  provenance_record wrong_state{node, fixed_versions(), 1, job_state::cancelled, {}, {},
                                stop_error(error_category::io, "x.y", "m")};
  EXPECT_STOP(provenance_json(wrong_state), error_category::invalid_input, "dag.bad_provenance");
  provenance_record running{node, fixed_versions(), 1, job_state::running, {}, {}, std::nullopt};
  EXPECT_STOP(provenance_json(running), error_category::invalid_input, "dag.bad_provenance");
}

// R5.d: record dell'arresto prima del lavoro. Testo canonico esatto per `cancelled` con `resume`; stati non finali o
// incoerenti con la categoria e revisione assente rifiutati.
TEST(Provenance, T_M2B_12_stop_before_work_record_text_and_rejections) {
  json resume = json::object();
  resume.set("workflow", json::string("wf"));
  const stop_before_work_record cancelled{"k1", fixed_versions(), 3, job_state::cancelled,
                                          stop_error(error_category::cancelled, "sched.cancelled", "m", {{"a", "b"}}),
                                          resume};
  EXPECT_EQ(stop_before_work_canonical(cancelled),
            "{\"generation\":3,\"node_key\":\"k1\",\"resume\":{\"workflow\":\"wf\"},\"state\":\"cancelled\","
            "\"stop\":{\"category\":\"cancelled\",\"code\":\"sched.cancelled\",\"context\":[{\"key\":\"a\",\"value\":\"b\"}],"
            "\"message\":\"m\"},\"stop_before_work\":true,\"versions\":{\"cfitsio\":\"4.6.3\",\"openssl\":\"OpenSSL 3.5.5\","
            "\"product\":\"1.2.3\",\"source_dirty\":true,\"source_revision\":\"" + std::string(40, 'c') +
                "\",\"sqlite\":\"3.46.1\"}}");
  const stop_error io(error_category::io, "x.y", "m");
  for (const job_state state : {job_state::running, job_state::completed, job_state::interrupted}) {
    EXPECT_STOP(stop_before_work_json(stop_before_work_record{"k", fixed_versions(), 1, state, io}),
                error_category::invalid_input, "dag.bad_provenance");
  }
  EXPECT_STOP(stop_before_work_json(stop_before_work_record{"k", fixed_versions(), 1, job_state::cancelled, io}),
              error_category::invalid_input, "dag.bad_provenance");
  EXPECT_STOP(stop_before_work_json(stop_before_work_record{"k", fixed_versions(), 1, job_state::failed,
                                                            stop_error(error_category::cancelled, "c.d", "m")}),
              error_category::invalid_input, "dag.bad_provenance");
  library_versions no_revision = fixed_versions();
  no_revision.source_revision.clear();
  EXPECT_STOP(stop_before_work_json(stop_before_work_record{"k", no_revision, 1, job_state::failed, io}),
              error_category::invalid_input, "dag.bad_provenance");
}

TEST(Provenance, current_library_versions_are_all_reported) {
  const library_versions v = current_library_versions();
  EXPECT_FALSE(v.product.empty());
  EXPECT_EQ(v.cfitsio, "4.6.3");
  EXPECT_FALSE(v.source_revision.empty());
  EXPECT_FALSE(v.sqlite.empty());
  EXPECT_NE(v.openssl.find("OpenSSL"), std::string::npos);
}

}  // namespace
