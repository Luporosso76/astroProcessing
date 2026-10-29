#include "astro/dag_store.hpp"

#include <algorithm>
#include <deque>

namespace astro {

namespace {

[[noreturn]] void unknown_slot(const std::string& name) {
  throw stop_error(error_category::invalid_input, "dag.unknown_slot", "slot inesistente", {{"slot", name}});
}

}  // namespace

void memory_dag_store::require_node(const std::string& node_key) const {
  if (nodes_.count(node_key) == 0) {
    throw stop_error(error_category::graph, "dag.missing_node", "nodo assente dallo store", {{"nodo", node_key}});
  }
}

slot_record& memory_dag_store::slot_ref(const std::string& slot_name) {
  const auto it = slots_.find(slot_name);
  if (it == slots_.end()) unknown_slot(slot_name);
  return it->second;
}

const slot_record& memory_dag_store::slot_ref(const std::string& slot_name) const {
  const auto it = slots_.find(slot_name);
  if (it == slots_.end()) unknown_slot(slot_name);
  return it->second;
}

void memory_dag_store::check_parents(const node_spec& node, const std::set<std::string>& pending) const {
  for (const auto& in : node.inputs()) {
    if (!in.is_file() && nodes_.count(in.ref()) == 0 && pending.count(in.ref()) == 0) {
      throw stop_error(error_category::graph, "dag.missing_parent", "padre assente dallo store",
                       {{"padre", in.ref()}, {"input_name", in.input_name()}});
    }
  }
}

void memory_dag_store::insert_node(const node_spec& node) {
  if (!nodes_.emplace(node.key(), node).second) return;
  for (const auto& in : node.inputs()) {
    if (!in.is_file()) children_[in.ref()].insert(node.key());
  }
}

void memory_dag_store::put_node(const node_spec& node) {
  std::lock_guard lock(mutex_);
  check_parents(node, {});
  insert_node(node);
}

std::optional<node_spec> memory_dag_store::get_node(const std::string& node_key) const {
  std::lock_guard lock(mutex_);
  const auto it = nodes_.find(node_key);
  if (it == nodes_.end()) return std::nullopt;
  return it->second;
}

std::vector<std::string> memory_dag_store::descendants(const std::string& node_key) const {
  std::lock_guard lock(mutex_);
  require_node(node_key);
  std::set<std::string> seen;
  std::deque<std::string> queue{node_key};
  while (!queue.empty()) {
    const std::string current = queue.front();
    queue.pop_front();
    const auto it = children_.find(current);
    if (it == children_.end()) continue;
    for (const auto& child : it->second) {
      if (seen.insert(child).second) queue.push_back(child);
    }
  }
  return {seen.begin(), seen.end()};
}

void memory_dag_store::create_slot(const std::string& slot_name, const std::string& target_node_key) {
  std::lock_guard lock(mutex_);
  require_node(target_node_key);
  if (!slots_.emplace(slot_name, slot_record{slot_name, 0, target_node_key, std::nullopt}).second) {
    throw stop_error(error_category::invalid_input, "dag.duplicate_slot", "slot gia' esistente",
                     {{"slot", slot_name}});
  }
}

std::optional<slot_record> memory_dag_store::get_slot(const std::string& slot_name) const {
  std::lock_guard lock(mutex_);
  const auto it = slots_.find(slot_name);
  if (it == slots_.end()) return std::nullopt;
  return it->second;
}

std::uint64_t memory_dag_store::retarget_slot(const std::string& slot_name, const std::string& target_node_key) {
  std::lock_guard lock(mutex_);
  require_node(target_node_key);
  slot_record& slot = slot_ref(slot_name);
  slot.target_node_key = target_node_key;
  return ++slot.current_generation;
}

job_record memory_dag_store::start_job(const std::string& slot_name) {
  std::lock_guard lock(mutex_);
  slot_record& slot = slot_ref(slot_name);
  job_record job;
  job.id = next_job_id_++;
  job.slot_name = slot_name;
  job.node_key = slot.target_node_key;
  job.generation = ++slot.current_generation;
  jobs_.emplace(job.id, job);
  return job;
}

void memory_dag_store::finish_job(std::uint64_t job_id, job_state state,
                                  std::optional<error_category> stop_category, const std::string& stop_code) {
  std::lock_guard lock(mutex_);
  const auto it = jobs_.find(job_id);
  if (it == jobs_.end()) {
    throw stop_error(error_category::invalid_input, "dag.unknown_job", "job inesistente",
                     {{"job", std::to_string(job_id)}});
  }
  ASTRO_ASSERT(it->second.state == job_state::running);
  ASTRO_ASSERT(state != job_state::running);
  ASTRO_ASSERT(state != job_state::completed || !stop_category.has_value());
  it->second.state = state;
  it->second.stop_category = stop_category;
  it->second.stop_code = stop_code;
}

std::optional<job_record> memory_dag_store::get_job(std::uint64_t job_id) const {
  std::lock_guard lock(mutex_);
  const auto it = jobs_.find(job_id);
  if (it == jobs_.end()) return std::nullopt;
  return it->second;
}

artifact_record memory_dag_store::add_artifact(artifact_record artifact) {
  std::lock_guard lock(mutex_);
  require_node(artifact.node_key);
  artifact.id = next_artifact_id_++;
  artifacts_.emplace(artifact.id, artifact);
  return artifact;
}

void memory_dag_store::set_artifact_present(std::uint64_t artifact_id, bool present) {
  std::lock_guard lock(mutex_);
  const auto it = artifacts_.find(artifact_id);
  if (it == artifacts_.end()) {
    throw stop_error(error_category::invalid_input, "dag.unknown_artifact", "artefatto inesistente",
                     {{"artefatto", std::to_string(artifact_id)}});
  }
  it->second.present = present;
}

std::vector<artifact_record> memory_dag_store::artifacts_for(const std::string& node_key) const {
  std::lock_guard lock(mutex_);
  std::vector<artifact_record> out;
  for (const auto& [id, artifact] : artifacts_) {
    (void)id;
    if (artifact.node_key == node_key) out.push_back(artifact);
  }
  return out;
}

void memory_dag_store::publish(std::uint64_t job_id, std::uint64_t artifact_id) {
  std::lock_guard lock(mutex_);
  const auto job = jobs_.find(job_id);
  const auto artifact = artifacts_.find(artifact_id);
  if (job == jobs_.end() || artifact == artifacts_.end()) {
    throw stop_error(error_category::invalid_input, "dag.unknown_artifact", "job o artefatto inesistente",
                     {{"job", std::to_string(job_id)}, {"artefatto", std::to_string(artifact_id)}});
  }
  if (job->second.state != job_state::running) {
    throw stop_error(error_category::invalid_input, "dag.job_not_running", "il job non e' in esecuzione",
                     {{"job", std::to_string(job_id)}});
  }
  if (!artifact->second.present) {
    throw stop_error(error_category::invalid_input, "dag.artifact_not_present", "artefatto non presente",
                     {{"artefatto", std::to_string(artifact_id)}});
  }
  if (artifact->second.node_key != job->second.node_key) {
    throw stop_error(error_category::invalid_input, "dag.artifact_node_mismatch",
                     "l'artefatto non appartiene al nodo del job",
                     {{"job_node", job->second.node_key}, {"artefatto_node", artifact->second.node_key}});
  }
  slot_record& slot = slot_ref(job->second.slot_name);
  if (slot.current_generation != job->second.generation) {
    throw stop_error(error_category::stale_generation, "dag.stale_publish",
                     "la generazione dello slot e' cambiata durante il job",
                     {{"slot", slot.slot_name},
                      {"generazione_job", std::to_string(job->second.generation)},
                      {"generazione_slot", std::to_string(slot.current_generation)}});
  }
  slot.artifact_id = artifact_id;
}

artifact_record memory_dag_store::complete_job(std::uint64_t job_id, artifact_record artifact) {
  const artifact_record stored = add_artifact(std::move(artifact));
  publish(job_id, stored.id);
  finish_job(job_id, job_state::completed, std::nullopt, "");
  return stored;
}

slot_view memory_dag_store::read_slot(const std::string& slot_name) const {
  std::lock_guard lock(mutex_);
  const slot_record& slot = slot_ref(slot_name);
  if (!slot.artifact_id) return slot_view{std::nullopt, freshness::missing};
  const artifact_record& artifact = artifacts_.at(*slot.artifact_id);
  if (!artifact.present) return slot_view{std::nullopt, freshness::missing};
  return slot_view{artifact, artifact.node_key == slot.target_node_key ? freshness::current : freshness::stale};
}

std::vector<std::string> memory_dag_store::apply_recipe(const std::vector<resolved_node>& nodes,
                                                         const std::map<std::string, std::string>& slot_targets) {
  std::lock_guard lock(mutex_);
  std::set<std::string> pending;
  std::map<std::string, std::string> key_of;
  for (const auto& n : nodes) {
    check_parents(n.spec, pending);
    pending.insert(n.spec.key());
    key_of.emplace(n.local_id, n.spec.key());
  }
  for (const auto& [slot_name, local_id] : slot_targets) {
    if (key_of.count(local_id) == 0) {
      throw stop_error(error_category::graph, "dag.missing_parent", "target dello slot assente dalla ricetta",
                       {{"slot", slot_name}, {"id", local_id}});
    }
  }
  for (const auto& n : nodes) insert_node(n.spec);
  std::vector<std::string> changed;
  for (const auto& [slot_name, local_id] : slot_targets) {
    const std::string& key = key_of.at(local_id);
    const auto it = slots_.find(slot_name);
    if (it == slots_.end()) {
      slots_.emplace(slot_name, slot_record{slot_name, 0, key, std::nullopt});
      changed.push_back(slot_name);
    } else if (it->second.target_node_key != key) {
      it->second.target_node_key = key;
      ++it->second.current_generation;
      changed.push_back(slot_name);
    }
  }
  return changed;
}

}  // namespace astro
