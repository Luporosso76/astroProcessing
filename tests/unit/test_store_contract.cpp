#include "store_test_support.hpp"

namespace {

using namespace store_test;

enum class backend { memory, sqlite };

class StoreContract : public testing::TestWithParam<backend> {
 protected:
  void SetUp() override {
    if (GetParam() == backend::memory) {
      store_ = std::make_unique<memory_dag_store>();
    } else {
      std::string name = std::string("contract_") + testing::UnitTest::GetInstance()->current_test_info()->name();
      std::replace(name.begin(), name.end(), '/', '_');
      auto sql = sqlite_dag_store::create(fresh_dir(name));
      sql_ = sql.get();
      store_ = std::move(sql);
    }
  }

  // Stessi byte e stesso digest su entrambe le implementazioni; su SQLite il file esiste davvero.
  artifact_record artifact(const std::string& node_key, std::uint64_t generation, const std::string& tag) {
    const auto bytes = bytes_of(tag);
    artifact_record a;
    a.node_key = node_key;
    a.generation = generation;
    a.sha256 = digest_of(bytes).sha256;
    a.size_bytes = bytes.size();
    a.kind = "image";
    if (sql_ != nullptr) a.rel_path = sql_->write_files({artifact_content{bytes, "bin", "image", std::nullopt}})[0].rel_path();
    return a;
  }

  std::unique_ptr<dag_store> store_;
  sqlite_dag_store* sql_ = nullptr;
};

TEST_P(StoreContract, T_M2B_06_late_job_cannot_publish_and_slot_stays_on_the_new_result) {
  const auto nodes = diamond(2).resolve();
  store_->apply_recipe(nodes, {{"s", "a"}});
  const std::string key = keys_of(nodes).at("a");
  const job_record old_job = store_->start_job("s");
  const job_record new_job = store_->start_job("s");
  EXPECT_LT(old_job.generation, new_job.generation);
  const artifact_record fresh = store_->add_artifact(artifact(key, new_job.generation, "fresh"));
  store_->publish(new_job.id, fresh.id);
  const artifact_record late = store_->add_artifact(artifact(key, old_job.generation, "late"));
  EXPECT_STOP(store_->publish(old_job.id, late.id), error_category::stale_generation, "dag.stale_publish");
  const slot_view view = store_->read_slot("s");
  EXPECT_EQ(view.state, freshness::current);
  EXPECT_EQ(view.artifact->id, fresh.id);
  EXPECT_EQ(store_->artifacts_for(key).size(), 2u);
}

TEST_P(StoreContract, T_M2B_06_retarget_makes_in_flight_job_stale_and_wrong_node_is_refused) {
  const auto v1 = diamond(2).resolve();
  const auto v2 = diamond(9).resolve();
  store_->apply_recipe(v1, {{"s", "b"}});
  const job_record job = store_->start_job("s");
  store_->apply_recipe(v2, {{"s", "b"}});
  const artifact_record rec = store_->add_artifact(artifact(job.node_key, job.generation, "x"));
  EXPECT_STOP(store_->publish(job.id, rec.id), error_category::stale_generation, "dag.stale_publish");
  EXPECT_EQ(store_->read_slot("s").state, freshness::missing);
  const job_record again = store_->start_job("s");
  const artifact_record wrong = store_->add_artifact(artifact(keys_of(v1).at("b"), 1, "wrong"));
  EXPECT_STOP(store_->publish(again.id, wrong.id), error_category::invalid_input, "dag.artifact_node_mismatch");
}

TEST_P(StoreContract, T_M2B_06_missing_when_artifact_is_not_present_and_unknown_slot_is_refused) {
  const auto nodes = diamond(2).resolve();
  store_->apply_recipe(nodes, {{"s", "a"}});
  EXPECT_EQ(store_->read_slot("s").state, freshness::missing);
  const job_record job = store_->start_job("s");
  const artifact_record rec = store_->add_artifact(artifact(keys_of(nodes).at("a"), job.generation, "x"));
  store_->publish(job.id, rec.id);
  EXPECT_EQ(store_->read_slot("s").state, freshness::current);
  store_->set_artifact_present(rec.id, false);
  const slot_view view = store_->read_slot("s");
  EXPECT_EQ(view.state, freshness::missing);
  EXPECT_FALSE(view.artifact.has_value());
  EXPECT_STOP(store_->read_slot("nessuno"), error_category::invalid_input, "dag.unknown_slot");
  EXPECT_STOP(store_->set_artifact_present(999, true), error_category::invalid_input, "dag.unknown_artifact");
}

TEST_P(StoreContract, T_M2B_04_apply_recipe_retargets_only_changed_slots_and_declares_stale) {
  const auto v1 = diamond(2).resolve();
  EXPECT_EQ(store_->apply_recipe(v1, diamond_slots()).size(), 4u);
  for (const auto& [slot, local] : diamond_slots()) {
    const job_record job = store_->start_job(slot);
    store_->publish(job.id, store_->add_artifact(artifact(keys_of(v1).at(local), job.generation, slot)).id);
    EXPECT_EQ(store_->read_slot(slot).state, freshness::current) << slot;
  }
  const auto v2 = diamond(9).resolve();
  EXPECT_EQ(store_->apply_recipe(v2, diamond_slots()), (std::vector<std::string>{"sb", "sd"}));
  EXPECT_EQ(store_->read_slot("sa").state, freshness::current);
  EXPECT_EQ(store_->read_slot("sb").state, freshness::stale);
  EXPECT_EQ(store_->read_slot("sc").state, freshness::current);
  EXPECT_EQ(store_->read_slot("sd").state, freshness::stale);
  EXPECT_EQ(store_->read_slot("sb").artifact->node_key, keys_of(v1).at("b"));
  EXPECT_EQ(store_->descendants(keys_of(v2).at("a")).size(), 5u);
  EXPECT_EQ(store_->descendants(keys_of(v2).at("c")).size(), 2u);
}

TEST_P(StoreContract, apply_recipe_is_atomic_when_a_slot_target_is_unknown) {
  const auto nodes = diamond(2).resolve();
  EXPECT_STOP(store_->apply_recipe(nodes, {{"s1", "a"}, {"s2", "inesistente"}}), error_category::graph, "dag.missing_parent");
  EXPECT_FALSE(store_->get_slot("s1").has_value());
  EXPECT_FALSE(store_->get_node(keys_of(nodes).at("a")).has_value());
}

TEST_P(StoreContract, publish_requires_running_job_and_present_artifact) {
  const auto nodes = diamond(2).resolve();
  store_->apply_recipe(nodes, {{"s", "a"}});
  const job_record job = store_->start_job("s");
  const artifact_record rec = store_->add_artifact(artifact(job.node_key, job.generation, "x"));
  store_->set_artifact_present(rec.id, false);
  EXPECT_STOP(store_->publish(job.id, rec.id), error_category::invalid_input, "dag.artifact_not_present");
  store_->set_artifact_present(rec.id, true);
  store_->finish_job(job.id, job_state::failed, error_category::io, "x.y");
  EXPECT_STOP(store_->publish(job.id, rec.id), error_category::invalid_input, "dag.job_not_running");
  const job_record got = *store_->get_job(job.id);
  EXPECT_EQ(got.state, job_state::failed);
  EXPECT_EQ(got.stop_category, error_category::io);
  EXPECT_EQ(got.stop_code, "x.y");
  EXPECT_FALSE(store_->get_job(999).has_value());
  EXPECT_STOP(store_->finish_job(999, job_state::failed, std::nullopt, ""), error_category::invalid_input, "dag.unknown_job");
}

TEST_P(StoreContract, nodes_slots_and_descendants_follow_the_graph_rules) {
  std::vector<node_input> inputs;
  inputs.push_back(node_input::from_node("in", std::string(64, 'e')));
  const node_spec child("op", 1, 1, json::object(), std::move(inputs), sample_type::f32, "cpu_scalar", std::nullopt);
  EXPECT_STOP(store_->put_node(child), error_category::graph, "dag.missing_parent");
  EXPECT_STOP(store_->create_slot("s", std::string(64, 'e')), error_category::graph, "dag.missing_node");
  EXPECT_STOP(store_->descendants(std::string(64, 'e')), error_category::graph, "dag.missing_node");
  const auto nodes = diamond(2).resolve();
  store_->apply_recipe(nodes, {});
  const std::string a = keys_of(nodes).at("a");
  store_->put_node(nodes.front().spec);  // idempotente
  store_->create_slot("s", a);
  EXPECT_STOP(store_->create_slot("s", a), error_category::invalid_input, "dag.duplicate_slot");
  EXPECT_EQ(store_->retarget_slot("s", keys_of(nodes).at("b")), 1u);
  EXPECT_STOP(store_->retarget_slot("s", std::string(64, 'e')), error_category::graph, "dag.missing_node");
  EXPECT_EQ(store_->get_slot("s")->target_node_key, keys_of(nodes).at("b"));
  EXPECT_EQ(store_->start_job("s").generation, 2u);
  EXPECT_FALSE(store_->get_slot("nessuno").has_value());
  EXPECT_EQ(store_->get_node(a)->canonical(), nodes.front().spec.canonical());
}

INSTANTIATE_TEST_SUITE_P(Backends, StoreContract, testing::Values(backend::memory, backend::sqlite),
                         [](const testing::TestParamInfo<backend>& info) {
                           return info.param == backend::memory ? "memory" : "sqlite";
                         });

}  // namespace
