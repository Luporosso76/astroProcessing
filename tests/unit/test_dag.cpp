#include <gtest/gtest.h>

#include <limits>

#include "astro/dag_store.hpp"
#include "astro/hash.hpp"
#include "astro/node.hpp"
#include "expect_stop.hpp"

namespace {

using namespace astro;

content_digest file_digest(char fill, std::uint64_t size = 10) { return {std::string(64, fill), size}; }

json params_of(double a, double b) {
  json p = json::object();
  p.set("a", json::number(a));
  p.set("b", json::number(b));
  return p;
}

recipe_entry entry(const std::string& op, double a, std::vector<recipe_input> inputs) {
  return recipe_entry(op, 1, 1, params_of(a, 2), std::move(inputs), sample_type::f32, "cpu_scalar", std::nullopt);
}

node_spec base_spec() {
  std::vector<node_input> inputs;
  inputs.push_back(node_input::from_file("light", file_digest('a')));
  inputs.push_back(node_input::from_file("dark", file_digest('b')));
  return node_spec("calib.light", 1, 1, params_of(1, 2), std::move(inputs), sample_type::f32, "cpu_scalar", 7);
}

// a -> (b, c) -> d
recipe_graph diamond(double b_param) {
  recipe_graph g;
  g.add("a", entry("op.a", 1, {recipe_input{"file", file_digest('a')}}));
  g.add("b", entry("op.b", b_param, {recipe_input{"in", std::string("a")}}));
  g.add("c", entry("op.c", 3, {recipe_input{"in", std::string("a")}}));
  g.add("d", entry("op.d", 4, {recipe_input{"x", std::string("b")}, recipe_input{"y", std::string("c")}}));
  return g;
}

std::map<std::string, std::string> keys_of(const std::vector<resolved_node>& nodes) {
  std::map<std::string, std::string> keys;
  for (const auto& n : nodes) keys[n.local_id] = n.spec.key();
  return keys;
}

TEST(Dag, T_M2B_03_every_field_changes_the_key) {
  const node_spec base = base_spec();
  std::set<std::string> keys{base.key()};
  auto expect_new = [&](const node_spec& s) { EXPECT_TRUE(keys.insert(s.key()).second) << s.canonical(); };
  auto inputs_with = [](content_digest first, content_digest second, std::string n1 = "light", std::string n2 = "dark") {
    std::vector<node_input> v;
    v.push_back(node_input::from_file(n1, first));
    v.push_back(node_input::from_file(n2, second));
    return v;
  };
  const auto same_inputs = [&] { return inputs_with(file_digest('a'), file_digest('b')); };
  expect_new(node_spec("calib.dark", 1, 1, params_of(1, 2), same_inputs(), sample_type::f32, "cpu_scalar", 7));
  expect_new(node_spec("calib.light", 2, 1, params_of(1, 2), same_inputs(), sample_type::f32, "cpu_scalar", 7));
  expect_new(node_spec("calib.light", 1, 2, params_of(1, 2), same_inputs(), sample_type::f32, "cpu_scalar", 7));
  expect_new(node_spec("calib.light", 1, 1, params_of(1, 3), same_inputs(), sample_type::f32, "cpu_scalar", 7));
  expect_new(node_spec("calib.light", 1, 1, params_of(1, 2), inputs_with(file_digest('c'), file_digest('b')),
                       sample_type::f32, "cpu_scalar", 7));
  expect_new(node_spec("calib.light", 1, 1, params_of(1, 2), inputs_with(file_digest('a', 11), file_digest('b')),
                       sample_type::f32, "cpu_scalar", 7));
  expect_new(node_spec("calib.light", 1, 1, params_of(1, 2), inputs_with(file_digest('a'), file_digest('b'), "light2"),
                       sample_type::f32, "cpu_scalar", 7));
  expect_new(node_spec("calib.light", 1, 1, params_of(1, 2), same_inputs(), sample_type::f64, "cpu_scalar", 7));
  expect_new(node_spec("calib.light", 1, 1, params_of(1, 2), same_inputs(), sample_type::f32, "cpu_avx2", 7));
  expect_new(node_spec("calib.light", 1, 1, params_of(1, 2), same_inputs(), sample_type::f32, "cpu_scalar", 8));
  expect_new(node_spec("calib.light", 1, 1, params_of(1, 2), same_inputs(), sample_type::f32, "cpu_scalar", std::nullopt));
  expect_new(node_spec("calib.light", 1, 1, params_of(1, 2), same_inputs(), sample_type::f32, "cpu_scalar", 0));
  EXPECT_EQ(base.key(),
            node_spec("calib.light", 1, 1, params_of(1, 2), same_inputs(), sample_type::f32, "cpu_scalar", 7).key());
}

TEST(Dag, key_ignores_parameter_insertion_order_but_depends_on_input_order) {
  json ab = json::object();
  ab.set("alpha", json::number(1.5));
  ab.set("beta", json::string("x"));
  json ba = json::object();
  ba.set("beta", json::string("x"));
  ba.set("alpha", json::number(1.5));
  auto make = [](json p, bool swap) {
    std::vector<node_input> in;
    if (swap) {
      in.push_back(node_input::from_file("dark", file_digest('b')));
      in.push_back(node_input::from_file("light", file_digest('a')));
    } else {
      in.push_back(node_input::from_file("light", file_digest('a')));
      in.push_back(node_input::from_file("dark", file_digest('b')));
    }
    return node_spec("op", 1, 1, std::move(p), std::move(in), sample_type::f32, "cpu_scalar", std::nullopt);
  };
  EXPECT_EQ(make(ab, false).key(), make(ba, false).key());
  EXPECT_NE(make(ab, false).key(), make(ab, true).key());
}

TEST(Dag, T_M2B_11_schema_version_is_not_an_input_of_the_key) {
  // Chiave = SHA-256("astro.node.v1\n" + JCS), con JCS scritto a mano e indipendente da qualunque versione di schema.
  std::vector<node_input> inputs;
  inputs.push_back(node_input::from_file("file", file_digest('a', 5)));
  json p = json::object();
  p.set("k", json::integer(1));
  const node_spec s("op.x", 3, 4, p, std::move(inputs), sample_type::f64, "cpu_scalar", std::nullopt);
  const std::string literal =
      "{\"algorithm_version\":3,\"backend\":\"cpu_scalar\","
      "\"inputs\":[{\"input_name\":\"file\",\"ordinal\":0,\"ref\":\"input:" + std::string(64, 'a') + ":5\"}],"
      "\"operator_id\":\"op.x\",\"params\":{\"k\":1},\"precision\":\"f64\",\"seed\":null,\"serialization_version\":4}";
  EXPECT_EQ(s.canonical(), literal);
  const std::string preimage = "astro.node.v1\n" + literal;
  EXPECT_EQ(s.key(), sha256_hex({reinterpret_cast<const std::uint8_t*>(preimage.data()), preimage.size()}));
  memory_dag_store store;
  store.put_node(s);
  EXPECT_EQ(store.get_node(s.key())->key(), s.key());
}

TEST(Dag, T_M2B_04_diamond_parameter_change_renews_only_node_and_descendants) {
  const auto before = keys_of(diamond(2).resolve());
  const auto after = keys_of(diamond(9).resolve());
  EXPECT_EQ(before.at("a"), after.at("a"));
  EXPECT_NE(before.at("b"), after.at("b"));
  EXPECT_EQ(before.at("c"), after.at("c"));
  EXPECT_NE(before.at("d"), after.at("d"));
}

TEST(Dag, T_M2B_04_apply_recipe_retargets_only_changed_slots_and_stale_is_declared) {
  memory_dag_store store;
  const std::map<std::string, std::string> slots{{"sa", "a"}, {"sb", "b"}, {"sc", "c"}, {"sd", "d"}};
  const auto v1 = diamond(2).resolve();
  EXPECT_EQ(store.apply_recipe(v1, slots).size(), 4u);
  for (const auto& [slot, local] : slots) {
    const job_record job = store.start_job(slot);
    artifact_record art;
    art.node_key = keys_of(v1).at(local);
    art.generation = job.generation;
    art.sha256 = std::string(64, '1');
    store.publish(job.id, store.add_artifact(art).id);
    EXPECT_EQ(store.read_slot(slot).state, freshness::current) << slot;
  }
  const auto v2 = diamond(9).resolve();
  EXPECT_EQ(store.apply_recipe(v2, slots), (std::vector<std::string>{"sb", "sd"}));
  EXPECT_EQ(store.read_slot("sa").state, freshness::current);
  EXPECT_EQ(store.read_slot("sb").state, freshness::stale);
  EXPECT_EQ(store.read_slot("sc").state, freshness::current);
  EXPECT_EQ(store.read_slot("sd").state, freshness::stale);
  EXPECT_EQ(store.read_slot("sb").artifact->node_key, keys_of(v1).at("b"));
  // i nodi di v1 restano nello store: b, c, d di v1 piu' b e d di v2
  EXPECT_EQ(store.descendants(keys_of(v2).at("a")).size(), 5u);
  EXPECT_EQ(store.descendants(keys_of(v2).at("c")).size(), 2u);
}

TEST(Dag, apply_recipe_is_atomic_when_a_slot_target_is_unknown) {
  memory_dag_store store;
  const auto nodes = diamond(2).resolve();
  EXPECT_STOP(store.apply_recipe(nodes, {{"s1", "a"}, {"s2", "inesistente"}}), error_category::graph,
              "dag.missing_parent");
  EXPECT_FALSE(store.get_slot("s1").has_value());
  EXPECT_FALSE(store.get_node(keys_of(nodes).at("a")).has_value());
}

TEST(Dag, publish_requires_running_job_and_present_artifact) {
  memory_dag_store store;
  const auto nodes = diamond(2).resolve();
  store.apply_recipe(nodes, {{"s", "a"}});
  const job_record job = store.start_job("s");
  artifact_record art;
  art.node_key = job.node_key;
  const artifact_record rec = store.add_artifact(art);
  store.set_artifact_present(rec.id, false);
  EXPECT_STOP(store.publish(job.id, rec.id), error_category::invalid_input, "dag.artifact_not_present");
  store.set_artifact_present(rec.id, true);
  store.finish_job(job.id, job_state::failed, error_category::io, "x.y");
  EXPECT_STOP(store.publish(job.id, rec.id), error_category::invalid_input, "dag.job_not_running");
}

TEST(Dag, T_M2B_05_cycle_and_missing_parent_are_rejected) {
  recipe_graph cycle;
  cycle.add("x", entry("op", 1, {recipe_input{"in", std::string("y")}}));
  cycle.add("y", entry("op", 1, {recipe_input{"in", std::string("x")}}));
  EXPECT_STOP(cycle.resolve(), error_category::graph, "dag.cycle");
  recipe_graph self;
  self.add("x", entry("op", 1, {recipe_input{"in", std::string("x")}}));
  EXPECT_STOP(self.resolve(), error_category::graph, "dag.cycle");
  recipe_graph orphan;
  orphan.add("x", entry("op", 1, {recipe_input{"in", std::string("assente")}}));
  EXPECT_STOP(orphan.resolve(), error_category::graph, "dag.missing_parent");
  recipe_graph dup;
  dup.add("x", entry("op", 1, {}));
  EXPECT_STOP(dup.add("x", entry("op", 1, {})), error_category::invalid_input, "dag.duplicate_node");

  memory_dag_store store;
  std::vector<node_input> inputs;
  inputs.push_back(node_input::from_node("in", std::string(64, 'e')));
  const node_spec child("op", 1, 1, json::object(), std::move(inputs), sample_type::f32, "cpu_scalar", std::nullopt);
  EXPECT_STOP(store.put_node(child), error_category::graph, "dag.missing_parent");
  EXPECT_STOP(store.create_slot("s", std::string(64, 'e')), error_category::graph, "dag.missing_node");
}

TEST(Dag, T_M2B_06_late_job_cannot_publish_and_slot_stays_current_on_the_new_result) {
  memory_dag_store store;
  const auto nodes = diamond(2).resolve();
  store.apply_recipe(nodes, {{"s", "a"}});
  const std::string key = keys_of(nodes).at("a");
  const job_record old_job = store.start_job("s");
  const job_record new_job = store.start_job("s");
  EXPECT_LT(old_job.generation, new_job.generation);
  artifact_record fresh;
  fresh.node_key = key;
  fresh.generation = new_job.generation;
  fresh.sha256 = std::string(64, '2');
  fresh.size_bytes = 4;
  const artifact_record fresh_rec = store.add_artifact(fresh);
  store.publish(new_job.id, fresh_rec.id);
  artifact_record late;
  late.node_key = key;
  late.generation = old_job.generation;
  late.sha256 = std::string(64, '3');
  const artifact_record late_rec = store.add_artifact(late);
  EXPECT_STOP(store.publish(old_job.id, late_rec.id), error_category::stale_generation, "dag.stale_publish");
  const slot_view view = store.read_slot("s");
  EXPECT_EQ(view.state, freshness::current);
  EXPECT_EQ(view.artifact->id, fresh_rec.id);
  EXPECT_EQ(store.artifacts_for(key).size(), 2u);  // il vecchio resta in cache sotto la sua chiave
}

TEST(Dag, T_M2B_06_retarget_makes_in_flight_job_stale_and_old_result_never_current) {
  memory_dag_store store;
  const auto v1 = diamond(2).resolve();
  const auto v2 = diamond(9).resolve();
  store.apply_recipe(v1, {{"s", "b"}});
  const job_record job = store.start_job("s");
  store.apply_recipe(v2, {{"s", "b"}});
  artifact_record art;
  art.node_key = job.node_key;
  const artifact_record rec = store.add_artifact(art);
  EXPECT_STOP(store.publish(job.id, rec.id), error_category::stale_generation, "dag.stale_publish");
  EXPECT_EQ(store.read_slot("s").state, freshness::missing);
  const job_record again = store.start_job("s");
  artifact_record wrong;
  wrong.node_key = keys_of(v1).at("b");
  EXPECT_STOP(store.publish(again.id, store.add_artifact(wrong).id), error_category::invalid_input,
              "dag.artifact_node_mismatch");
}

TEST(Dag, T_M2B_06_missing_when_artifact_is_absent_or_not_present) {
  memory_dag_store store;
  const auto nodes = diamond(2).resolve();
  store.apply_recipe(nodes, {{"s", "a"}});
  EXPECT_EQ(store.read_slot("s").state, freshness::missing);
  const job_record job = store.start_job("s");
  artifact_record art;
  art.node_key = keys_of(nodes).at("a");
  const artifact_record rec = store.add_artifact(art);
  store.publish(job.id, rec.id);
  EXPECT_EQ(store.read_slot("s").state, freshness::current);
  store.set_artifact_present(rec.id, false);
  const slot_view view = store.read_slot("s");
  EXPECT_EQ(view.state, freshness::missing);
  EXPECT_FALSE(view.artifact.has_value());
  EXPECT_STOP(store.read_slot("nessuno"), error_category::invalid_input, "dag.unknown_slot");
}

TEST(Dag, T_M2B_13_fits_decode_node_key_follows_read_options_and_declared_metadata) {
  const content_digest in = file_digest('d', 2880);
  auto make = [&](const fits_read_options& o, const frame_metadata& m) {
    return make_fits_decode_node(in, o, m, sample_type::f32, "cpu_scalar");
  };
  const fits_read_options none;
  const frame_metadata no_meta;
  const std::string base = make(none, no_meta).key();
  EXPECT_EQ(make(none, no_meta).key(), base);

  fits_read_options x0;
  x0.declared_bayer_x_offset = 0;
  fits_read_options x1;
  x1.declared_bayer_x_offset = 1;
  fits_read_options y0;
  y0.declared_bayer_y_offset = 0;
  const std::set<std::string> option_keys{base, make(x0, no_meta).key(), make(x1, no_meta).key(), make(y0, no_meta).key()};
  EXPECT_EQ(option_keys.size(), 4u);

  frame_metadata e60;
  declare_metadata(e60.exposure_s, 60.0, "exposure_s");
  frame_metadata e61;
  declare_metadata(e61.exposure_s, 61.0, "exposure_s");
  frame_metadata sat;
  declare_metadata(sat.saturation_adu, 65535.0, "saturation_adu");
  frame_metadata filt;
  declare_metadata(filt.filter, std::string("Ha"), "filter");
  frame_metadata roi;
  declare_metadata(roi.roi_x, std::int64_t{12}, "roi_x");
  const std::set<std::string> meta_keys{base,
                                        make(none, e60).key(),
                                        make(none, e61).key(),
                                        make(none, sat).key(),
                                        make(none, filt).key(),
                                        make(none, roi).key()};
  EXPECT_EQ(meta_keys.size(), 6u);
  frame_metadata e60_again;
  declare_metadata(e60_again.exposure_s, 60.0, "exposure_s");
  EXPECT_EQ(make(none, e60_again).key(), make(none, e60).key());
}

TEST(Dag, T_M2B_13_fits_decode_node_rejects_header_origin_and_nonfinite_values) {
  frame_metadata from_header;
  from_header.exposure_s = metadata_value<double>{60.0, {metadata_origin::header, "EXPTIME"}};
  EXPECT_STOP(make_fits_decode_node(file_digest('d'), {}, from_header, sample_type::f32, "cpu_scalar"),
              error_category::invalid_input, "dag.metadata_not_declared");
  frame_metadata nan_meta;
  declare_metadata(nan_meta.gain, std::numeric_limits<double>::quiet_NaN(), "gain");
  EXPECT_STOP(make_fits_decode_node(file_digest('d'), {}, nan_meta, sample_type::f32, "cpu_scalar"),
              error_category::invalid_input, "dag.param_nonfinite");
}

TEST(Dag, spec_validation_rejects_malformed_fields) {
  auto inputs = [] {
    std::vector<node_input> v;
    v.push_back(node_input::from_file("a", file_digest('a')));
    return v;
  };
  EXPECT_STOP(node_spec("", 1, 1, json::object(), inputs(), sample_type::f32, "cpu_scalar", std::nullopt),
              error_category::invalid_input, "dag.bad_spec");
  EXPECT_STOP(node_spec("op", 1, 1, json::object(), inputs(), sample_type::f32, "", std::nullopt),
              error_category::invalid_input, "dag.bad_spec");
  EXPECT_STOP(node_spec("op", 1, 1, json::array(), inputs(), sample_type::f32, "cpu_scalar", std::nullopt),
              error_category::invalid_input, "dag.bad_spec");
  std::vector<node_input> dup = inputs();
  dup.push_back(node_input::from_file("a", file_digest('b')));
  EXPECT_STOP(node_spec("op", 1, 1, json::object(), std::move(dup), sample_type::f32, "cpu_scalar", std::nullopt),
              error_category::invalid_input, "dag.bad_spec");
  EXPECT_STOP(node_spec("op", 0, 1, json::object(), inputs(), sample_type::f32, "cpu_scalar", std::nullopt),
              error_category::invalid_input, "dag.bad_spec");
  EXPECT_STOP(node_input::from_node("x", "non-una-chiave"), error_category::invalid_input, "dag.bad_spec");
  EXPECT_STOP(node_input::from_file("x", content_digest{"zz", 1}), error_category::invalid_input, "dag.bad_spec");
}

}  // namespace
