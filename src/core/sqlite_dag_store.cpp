#include <fcntl.h>
#include <sys/stat.h>

#include <algorithm>
#include <cerrno>
#include <set>

#include "astro/sqlite_dag_store.hpp"
#include "sqlite_internal.hpp"

namespace astro {

using namespace sqlint;

namespace {

[[noreturn]] void unknown_slot(const std::string& name) {
  throw stop_error(error_category::invalid_input, "dag.unknown_slot", "slot inesistente", {{"slot", name}});
}

const char* state_text(job_state state) {
  switch (state) {
    case job_state::running: return "running";
    case job_state::completed: return "completed";
    case job_state::failed: return "failed";
    case job_state::cancelled: return "cancelled";
    case job_state::interrupted: return "interrupted";
  }
  std::abort();
}

job_state state_from(const std::string& text) {
  for (const job_state s : {job_state::running, job_state::completed, job_state::failed, job_state::cancelled,
                            job_state::interrupted}) {
    if (text == state_text(s)) return s;
  }
  throw stop_error(error_category::integrity, "store.job_state", "stato di job sconosciuto", {{"stato", text}});
}

error_category category_from(const std::string& text) {
  for (int i = 0; i <= static_cast<int>(error_category::security); ++i) {
    if (text == to_string(static_cast<error_category>(i))) return static_cast<error_category>(i);
  }
  throw stop_error(error_category::integrity, "store.stop_category", "categoria di arresto sconosciuta", {{"categoria", text}});
}

constexpr const char* artifact_columns = "id, node_key, generation, rel_path, size_bytes, sha256, kind, present, output_role, job_id";
constexpr const char* slot_columns = "slot_name, current_generation, target_node_key, artifact_id, detached";

artifact_record artifact_from(const statement& st, int first = 0) {
  artifact_record a;
  a.id = st.u64(first);
  a.node_key = st.text(first + 1);
  a.generation = st.u64(first + 2);
  a.rel_path = st.text(first + 3);
  a.size_bytes = st.u64(first + 4);
  a.sha256 = st.text(first + 5);
  a.kind = st.text(first + 6);
  a.present = st.integer(first + 7) == 1;
  a.output_role = st.text(first + 8);
  a.job_id = st.opt_u64(first + 9);
  return a;
}

slot_record slot_from(const statement& st) {
  return slot_record{st.text(0), st.u64(1), st.text(2), st.opt_u64(3), st.integer(4) == 1};
}

void check_role(const std::string& role) {
  const bool ok = !role.empty() && role.size() <= 32 &&
                  std::all_of(role.begin(), role.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
  if (!ok) throw stop_error(error_category::invalid_input, "store.bad_role", "ruolo di output non valido", {{"ruolo", role}});
}

job_record job_from(const statement& st) {
  job_record j;
  j.id = st.u64(0);
  j.slot_name = st.text(1);
  j.node_key = st.text(2);
  j.generation = st.u64(3);
  j.state = state_from(st.text(4));
  if (const auto cat = st.opt_text(5)) j.stop_category = category_from(*cat);
  j.stop_code = st.text(6);
  return j;
}

constexpr const char* job_columns = "id, slot_name, node_key, generation, state, stop_category, stop_code";

}  // namespace

bool sqlite_dag_store::node_exists_locked(const std::string& node_key) const {
  statement st(db_, "SELECT 1 FROM nodes WHERE node_key=?1");
  st.bind(1, node_key);
  return st.step();
}

void sqlite_dag_store::require_node_locked(const std::string& node_key) const {
  if (!node_exists_locked(node_key)) {
    throw stop_error(error_category::graph, "dag.missing_node", "nodo assente dallo store", {{"nodo", node_key}});
  }
}

slot_record sqlite_dag_store::require_slot_locked(const std::string& slot_name) const {
  statement st(db_, (std::string("SELECT ") + slot_columns + " FROM slots WHERE slot_name=?1").c_str());
  st.bind(1, slot_name);
  if (!st.step()) unknown_slot(slot_name);
  return slot_from(st);
}

void sqlite_dag_store::insert_node_locked(const node_spec& node) {
  statement ins(db_, "INSERT OR IGNORE INTO nodes(node_key, operator_id, algorithm_version, serialization_version, "
                     "params_canonical, precision, backend, seed) VALUES(?1,?2,?3,?4,?5,?6,?7,?8)");
  ins.bind(1, node.key()).bind(2, node.operator_id()).bind_u64(3, node.algorithm_version());
  ins.bind_u64(4, node.serialization_version()).bind(5, canonical_json(node.params()));
  ins.bind(6, node.precision() == sample_type::f32 ? "f32" : "f64").bind(7, node.backend());
  if (node.seed()) ins.bind(8, std::to_string(*node.seed())); else ins.bind_null(8);
  ins.run();
  if (sqlite3_changes(db_) == 0) return;
  statement edge(db_, "INSERT INTO node_edges(child_key, ordinal, input_name, parent_key, input_sha256, input_size) "
                      "VALUES(?1,?2,?3,?4,?5,?6)");
  std::int64_t ordinal = 0;
  for (const auto& in : node.inputs()) {
    edge.bind(1, node.key()).bind(2, ordinal++).bind(3, in.input_name());
    if (in.is_file()) {
      const content_digest d = in.file_digest();
      edge.bind_null(4).bind(5, d.sha256).bind_u64(6, d.size_bytes);
    } else {
      edge.bind(4, in.ref()).bind_null(5).bind_null(6);
    }
    edge.run();
  }
}

void sqlite_dag_store::put_node(const node_spec& node) {
  require_writable();
  std::lock_guard lock(mutex_);
  transaction tx(db_);
  for (const auto& in : node.inputs()) {
    if (!in.is_file() && !node_exists_locked(in.ref())) {
      throw stop_error(error_category::graph, "dag.missing_parent", "padre assente dallo store",
                       {{"padre", in.ref()}, {"input_name", in.input_name()}});
    }
  }
  insert_node_locked(node);
  tx.commit();
}

std::optional<node_spec> sqlite_dag_store::get_node(const std::string& node_key) const {
  std::lock_guard lock(mutex_);
  statement st(db_, "SELECT operator_id, algorithm_version, serialization_version, params_canonical, precision, "
                    "backend, seed FROM nodes WHERE node_key=?1");
  st.bind(1, node_key);
  if (!st.step()) return std::nullopt;
  json doc = json::object();
  doc.set("operator_id", json::string(st.text(0)));
  doc.set("algorithm_version", json::integer(st.integer(1)));
  doc.set("serialization_version", json::integer(st.integer(2)));
  const std::string params = st.text(3);
  doc.set("precision", json::string(st.text(4)));
  doc.set("backend", json::string(st.text(5)));
  doc.set("seed", st.is_null(6) ? json::null() : json::string(st.text(6)));
  statement edges(db_, "SELECT ordinal, input_name, parent_key, input_sha256, input_size FROM node_edges "
                       "WHERE child_key=?1 ORDER BY ordinal");
  edges.bind(1, node_key);
  json inputs = json::array();
  while (edges.step()) {
    json item = json::object();
    item.set("ordinal", json::integer(edges.integer(0)));
    item.set("input_name", json::string(edges.text(1)));
    item.set("ref", json::string(edges.is_null(2) ? "input:" + edges.text(3) + ":" + std::to_string(edges.u64(4))
                                                  : edges.text(2)));
    inputs.push(std::move(item));
  }
  try {
    doc.set("params", parse_json(params));
    doc.set("inputs", std::move(inputs));
    node_spec spec = node_from_json(doc);
    if (spec.key() != node_key) {
      throw stop_error(error_category::integrity, "store.node_key_mismatch", "la riga del nodo non produce la sua chiave",
                       {{"atteso", node_key}, {"calcolata", spec.key()}});
    }
    return spec;
  } catch (const stop_error& e) {
    if (e.category() == error_category::integrity) throw;
    throw stop_error(error_category::integrity, "store.node_row_invalid", "riga del nodo non valida",
                     {{"nodo", node_key}, {"causa", e.code()}});
  }
}

std::vector<std::string> sqlite_dag_store::descendants(const std::string& node_key) const {
  std::lock_guard lock(mutex_);
  require_node_locked(node_key);
  statement st(db_, "WITH RECURSIVE d(k) AS (SELECT child_key FROM node_edges WHERE parent_key=?1 UNION "
                    "SELECT e.child_key FROM node_edges e JOIN d ON e.parent_key=d.k) SELECT k FROM d ORDER BY k");
  st.bind(1, node_key);
  std::vector<std::string> out;
  while (st.step()) out.push_back(st.text(0));
  return out;
}

void sqlite_dag_store::create_slot(const std::string& slot_name, const std::string& target_node_key) {
  require_writable();
  std::lock_guard lock(mutex_);
  transaction tx(db_);
  require_node_locked(target_node_key);
  statement exists(db_, "SELECT 1 FROM slots WHERE slot_name=?1");
  if (exists.bind(1, slot_name).step()) {
    throw stop_error(error_category::invalid_input, "dag.duplicate_slot", "slot gia' esistente", {{"slot", slot_name}});
  }
  statement ins(db_, "INSERT INTO slots(slot_name, current_generation, target_node_key, artifact_id) VALUES(?1,0,?2,NULL)");
  ins.bind(1, slot_name).bind(2, target_node_key).run();
  create_snapshot_locked();
  tx.commit();
}

std::optional<slot_record> sqlite_dag_store::get_slot(const std::string& slot_name) const {
  std::lock_guard lock(mutex_);
  statement st(db_, (std::string("SELECT ") + slot_columns + " FROM slots WHERE slot_name=?1").c_str());
  st.bind(1, slot_name);
  if (!st.step()) return std::nullopt;
  return slot_from(st);
}

std::uint64_t sqlite_dag_store::retarget_slot(const std::string& slot_name, const std::string& target_node_key) {
  require_writable();
  std::lock_guard lock(mutex_);
  transaction tx(db_);
  require_node_locked(target_node_key);
  const slot_record slot = require_slot_locked(slot_name);
  statement up(db_, "UPDATE slots SET target_node_key=?2, current_generation=?3, detached=0, "
                    "artifact_id=CASE WHEN detached=1 THEN (SELECT id FROM artifacts WHERE node_key=?2 AND present=1 "
                    "AND output_role=slots.publish_role ORDER BY id DESC LIMIT 1) ELSE artifact_id END WHERE slot_name=?1");
  up.bind(1, slot_name).bind(2, target_node_key).bind_u64(3, slot.current_generation + 1).run();
  create_snapshot_locked();
  tx.commit();
  return slot.current_generation + 1;
}

job_record sqlite_dag_store::start_job(const std::string& slot_name) {
  require_writable();
  std::lock_guard lock(mutex_);
  transaction tx(db_);
  const slot_record slot = require_slot_locked(slot_name);
  if (slot.detached) {
    throw stop_error(error_category::invalid_input, "dag.slot_detached", "lo slot e' escluso dalla ricetta corrente",
                     {{"slot", slot_name}});
  }
  statement up(db_, "UPDATE slots SET current_generation=?2 WHERE slot_name=?1");
  up.bind(1, slot_name).bind_u64(2, slot.current_generation + 1).run();
  statement ins(db_, "INSERT INTO jobs(slot_name, node_key, generation, state) VALUES(?1,?2,?3,'running')");
  ins.bind(1, slot_name).bind(2, slot.target_node_key).bind_u64(3, slot.current_generation + 1).run();
  job_record job;
  job.id = static_cast<std::uint64_t>(sqlite3_last_insert_rowid(db_));
  job.slot_name = slot_name;
  job.node_key = slot.target_node_key;
  job.generation = slot.current_generation + 1;
  tx.commit();
  return job;
}

void sqlite_dag_store::finish_job(std::uint64_t job_id, job_state state, std::optional<error_category> stop_category,
                                  const std::string& stop_code) {
  require_writable();
  std::lock_guard lock(mutex_);
  transaction tx(db_);
  statement cur(db_, "SELECT state FROM jobs WHERE id=?1");
  if (!cur.bind_u64(1, job_id).step()) {
    throw stop_error(error_category::invalid_input, "dag.unknown_job", "job inesistente", {{"job", std::to_string(job_id)}});
  }
  ASTRO_ASSERT(cur.text(0) == "running");
  ASTRO_ASSERT(state != job_state::running);
  ASTRO_ASSERT(state != job_state::completed || !stop_category.has_value());
  statement up(db_, "UPDATE jobs SET state=?2, stop_category=?3, stop_code=?4 WHERE id=?1");
  up.bind_u64(1, job_id).bind(2, state_text(state));
  if (stop_category) up.bind(3, to_string(*stop_category)); else up.bind_null(3);
  up.bind(4, stop_code).run();
  tx.commit();
}

std::optional<job_record> sqlite_dag_store::get_job(std::uint64_t job_id) const {
  std::lock_guard lock(mutex_);
  statement st(db_, (std::string("SELECT ") + job_columns + " FROM jobs WHERE id=?1").c_str());
  if (!st.bind_u64(1, job_id).step()) return std::nullopt;
  return job_from(st);
}

artifact_record sqlite_dag_store::add_artifact(artifact_record artifact) {
  require_writable();
  check_role(artifact.output_role);
  std::lock_guard lock(mutex_);
  transaction tx(db_);
  require_node_locked(artifact.node_key);
  require_written_locked(artifact);
  statement ins(db_, "INSERT INTO artifacts(node_key, generation, rel_path, size_bytes, sha256, kind, present, output_role) "
                     "VALUES(?1,?2,?3,?4,?5,?6,?7,?8)");
  ins.bind(1, artifact.node_key).bind_u64(2, artifact.generation).bind(3, artifact.rel_path);
  ins.bind_u64(4, artifact.size_bytes).bind(5, artifact.sha256).bind(6, artifact.kind);
  ins.bind(7, std::int64_t{artifact.present ? 1 : 0}).bind(8, artifact.output_role).run();
  artifact.id = static_cast<std::uint64_t>(sqlite3_last_insert_rowid(db_));
  tx.commit();
  return artifact;
}

artifact_record sqlite_dag_store::complete_job(std::uint64_t job_id, artifact_record artifact) {
  require_writable();
  check_role(artifact.output_role);
  std::lock_guard lock(mutex_);
  const stored_file file = require_written_locked(artifact);
  return commit_locked(job_id, {file}, &artifact.node_key).front();
}

void sqlite_dag_store::set_publish_role(const std::string& slot_name, const std::string& role) {
  require_writable();
  check_role(role);
  std::lock_guard lock(mutex_);
  transaction tx(db_);
  const slot_record slot = require_slot_locked(slot_name);
  if (slot.detached) {
    throw stop_error(error_category::invalid_input, "dag.slot_detached", "lo slot e' escluso dalla ricetta corrente",
                     {{"slot", slot_name}});
  }
  statement cur(db_, "SELECT publish_role FROM slots WHERE slot_name=?1");
  if (cur.bind(1, slot_name).step() && cur.text(0) == role) return;
  statement up(db_, "UPDATE slots SET publish_role=?2, current_generation=?3, "
                    "artifact_id=(SELECT id FROM artifacts WHERE node_key=slots.target_node_key AND present=1 "
                    "AND output_role=?2 ORDER BY id DESC LIMIT 1) WHERE slot_name=?1");
  up.bind(1, slot_name).bind(2, role).bind_u64(3, slot.current_generation + 1).run();
  create_snapshot_locked();
  tx.commit();
}

void sqlite_dag_store::set_artifact_present(std::uint64_t artifact_id, bool present) {
  require_writable();
  std::lock_guard lock(mutex_);
  transaction tx(db_);
  statement up(db_, "UPDATE artifacts SET present=?2 WHERE id=?1");
  up.bind_u64(1, artifact_id).bind(2, std::int64_t{present ? 1 : 0}).run();
  if (sqlite3_changes(db_) == 0) {
    throw stop_error(error_category::invalid_input, "dag.unknown_artifact", "artefatto inesistente",
                     {{"artefatto", std::to_string(artifact_id)}});
  }
  tx.commit();
}

std::vector<artifact_record> sqlite_dag_store::artifacts_for(const std::string& node_key) const {
  std::lock_guard lock(mutex_);
  statement st(db_, (std::string("SELECT ") + artifact_columns + " FROM artifacts WHERE node_key=?1 ORDER BY id").c_str());
  st.bind(1, node_key);
  std::vector<artifact_record> out;
  while (st.step()) out.push_back(artifact_from(st));
  return out;
}

void sqlite_dag_store::publish(std::uint64_t job_id, std::uint64_t artifact_id) {
  require_writable();
  std::lock_guard lock(mutex_);
  transaction tx(db_);
  statement js(db_, (std::string("SELECT ") + job_columns + " FROM jobs WHERE id=?1").c_str());
  statement as(db_, (std::string("SELECT ") + artifact_columns + " FROM artifacts WHERE id=?1").c_str());
  const bool have_job = js.bind_u64(1, job_id).step();
  const bool have_artifact = as.bind_u64(1, artifact_id).step();
  if (!have_job || !have_artifact) {
    throw stop_error(error_category::invalid_input, "dag.unknown_artifact", "job o artefatto inesistente",
                     {{"job", std::to_string(job_id)}, {"artefatto", std::to_string(artifact_id)}});
  }
  const job_record job = job_from(js);
  const artifact_record artifact = artifact_from(as);
  if (job.state != job_state::running) {
    throw stop_error(error_category::invalid_input, "dag.job_not_running", "il job non e' in esecuzione",
                     {{"job", std::to_string(job_id)}});
  }
  if (!artifact.present) {
    throw stop_error(error_category::invalid_input, "dag.artifact_not_present", "artefatto non presente",
                     {{"artefatto", std::to_string(artifact_id)}});
  }
  if (artifact.node_key != job.node_key) {
    throw stop_error(error_category::invalid_input, "dag.artifact_node_mismatch",
                     "l'artefatto non appartiene al nodo del job",
                     {{"job_node", job.node_key}, {"artefatto_node", artifact.node_key}});
  }
  const slot_record slot = require_slot_locked(job.slot_name);
  if (slot.current_generation != job.generation) {
    throw stop_error(error_category::stale_generation, "dag.stale_publish",
                     "la generazione dello slot e' cambiata durante il job",
                     {{"slot", slot.slot_name},
                      {"generazione_job", std::to_string(job.generation)},
                      {"generazione_slot", std::to_string(slot.current_generation)}});
  }
  statement role(db_, "SELECT publish_role FROM slots WHERE slot_name=?1");
  if (role.bind(1, slot.slot_name).step() && role.text(0) != artifact.output_role) {
    throw stop_error(error_category::invalid_input, "dag.artifact_role_mismatch",
                     "il ruolo dell'artefatto non e' quello pubblicato dallo slot",
                     {{"slot", slot.slot_name}, {"ruolo_slot", role.text(0)}, {"ruolo_artefatto", artifact.output_role}});
  }
  statement up(db_, "UPDATE slots SET artifact_id=?2 WHERE slot_name=?1");
  up.bind(1, slot.slot_name).bind_u64(2, artifact_id).run();
  tx.commit();
}

slot_view sqlite_dag_store::read_slot(const std::string& slot_name) const {
  std::lock_guard lock(mutex_);
  const slot_record slot = require_slot_locked(slot_name);
  if (slot.detached || !slot.artifact_id) return slot_view{std::nullopt, freshness::missing};
  statement st(db_, (std::string("SELECT ") + artifact_columns + " FROM artifacts WHERE id=?1").c_str());
  if (!st.bind_u64(1, *slot.artifact_id).step()) {
    throw stop_error(error_category::integrity, "store.dangling_artifact", "lo slot punta a un artefatto inesistente",
                     {{"slot", slot_name}});
  }
  artifact_record artifact = artifact_from(st);
  if (!artifact.present) return slot_view{std::nullopt, freshness::missing};
  const freshness state = artifact.node_key == slot.target_node_key ? freshness::current : freshness::stale;
  return slot_view{std::move(artifact), state};
}

std::vector<std::string> sqlite_dag_store::apply_recipe(const std::vector<resolved_node>& nodes,
                                                         const std::map<std::string, std::string>& slot_targets) {
  require_writable();
  std::lock_guard lock(mutex_);
  transaction tx(db_);
  std::set<std::string> pending;
  std::map<std::string, std::string> key_of;
  for (const auto& n : nodes) {
    for (const auto& in : n.spec.inputs()) {
      if (!in.is_file() && pending.count(in.ref()) == 0 && !node_exists_locked(in.ref())) {
        throw stop_error(error_category::graph, "dag.missing_parent", "padre assente dallo store",
                         {{"padre", in.ref()}, {"input_name", in.input_name()}});
      }
    }
    pending.insert(n.spec.key());
    key_of.emplace(n.local_id, n.spec.key());
  }
  for (const auto& [slot_name, local_id] : slot_targets) {
    if (key_of.count(local_id) == 0) {
      throw stop_error(error_category::graph, "dag.missing_parent", "target dello slot assente dalla ricetta",
                       {{"slot", slot_name}, {"id", local_id}});
    }
  }
  for (const auto& n : nodes) insert_node_locked(n.spec);
  std::vector<std::string> changed;
  for (const auto& [slot_name, local_id] : slot_targets) {
    const std::string& key = key_of.at(local_id);
    statement cur(db_, "SELECT target_node_key, current_generation, detached FROM slots WHERE slot_name=?1");
    if (!cur.bind(1, slot_name).step()) {
      statement ins(db_, "INSERT INTO slots(slot_name, current_generation, target_node_key, artifact_id) VALUES(?1,0,?2,NULL)");
      ins.bind(1, slot_name).bind(2, key).run();
      changed.push_back(slot_name);
    } else if (cur.integer(2) == 1 || cur.text(0) != key) {
      statement up(db_, "UPDATE slots SET target_node_key=?2, current_generation=?3, detached=0, "
                        "artifact_id=CASE WHEN detached=1 THEN (SELECT id FROM artifacts WHERE node_key=?2 AND present=1 "
                        "AND output_role=slots.publish_role ORDER BY id DESC LIMIT 1) ELSE artifact_id END WHERE slot_name=?1");
      up.bind(1, slot_name).bind(2, key).bind_u64(3, cur.u64(1) + 1).run();
      changed.push_back(slot_name);
    }
  }
  if (!changed.empty()) create_snapshot_locked();
  tx.commit();
  return changed;
}

void sqlite_dag_store::put_setting(const std::string& key, const std::string& value_canonical) {
  write_setting(key, value_canonical, true);
}

void sqlite_dag_store::insert_setting(const std::string& key, const std::string& value_canonical) {
  write_setting(key, value_canonical, false);
}

void sqlite_dag_store::write_setting(const std::string& key, const std::string& value_canonical, bool replace) {
  require_writable();
  if (key.empty()) {
    throw stop_error(error_category::invalid_input, "store.bad_setting", "chiave di impostazione vuota");
  }
  if (canonical_json(parse_json(value_canonical)) != value_canonical) {
    throw stop_error(error_category::invalid_input, "store.setting_not_canonical",
                     "il valore non e' JSON canonico", {{"key", key}});
  }
  std::lock_guard lock(mutex_);
  transaction tx(db_);
  if (!replace) {
    statement present(db_, "SELECT 1 FROM settings WHERE key=?1");
    present.bind(1, key);
    if (present.step()) {
      throw stop_error(error_category::invalid_input, "store.setting_exists", "impostazione gia' presente", {{"key", key}});
    }
  }
  statement st(db_, "INSERT INTO settings(key, value_canonical) VALUES(?1,?2) "
                    "ON CONFLICT(key) DO UPDATE SET value_canonical=excluded.value_canonical");
  st.bind(1, key).bind(2, value_canonical).run();
  tx.commit();
}

std::optional<std::string> sqlite_dag_store::get_setting(const std::string& key) const {
  std::lock_guard lock(mutex_);
  statement st(db_, "SELECT value_canonical FROM settings WHERE key=?1");
  st.bind(1, key);
  if (!st.step()) return std::nullopt;
  return st.text(0);
}

input_record sqlite_dag_store::add_input(input_record input) {
  require_writable();
  if (!is_sha256_hex(input.sha256) || input.path.empty() || input.role.empty()) {
    throw stop_error(error_category::invalid_input, "store.bad_input", "record di input non valido",
                     {{"path", input.path}});
  }
  struct stat st {};
  if (::stat(input.path.c_str(), &st) != 0) {
    if (errno == ENOENT || errno == ENOTDIR) {
      throw stop_error(error_category::io, "input.missing", "originale assente", {{"path", input.path}});
    }
    throw_errno(error_category::io, "store.stat", "stat dell'originale fallita", errno, {{"path", input.path}});
  }
  if (!S_ISREG(st.st_mode)) {
    throw stop_error(error_category::invalid_input, "store.bad_input", "l'originale non e' un file regolare", {{"path", input.path}});
  }
  input.dev = static_cast<std::uint64_t>(st.st_dev);
  input.ino = static_cast<std::uint64_t>(st.st_ino);
  std::lock_guard lock(mutex_);
  transaction tx(db_);
  statement ins(db_, "INSERT INTO inputs(path, size_bytes, sha256, role, dev, ino) VALUES(?1,?2,?3,?4,?5,?6)");
  ins.bind(1, input.path).bind_u64(2, input.size_bytes).bind(3, input.sha256).bind(4, input.role);
  ins.bind_u64(5, input.dev).bind_u64(6, input.ino).run();
  input.id = static_cast<std::uint64_t>(sqlite3_last_insert_rowid(db_));
  tx.commit();
  return input;
}

std::vector<input_record> sqlite_dag_store::inputs() const {
  std::lock_guard lock(mutex_);
  statement st(db_, "SELECT id, path, size_bytes, sha256, role, dev, ino FROM inputs ORDER BY id");
  std::vector<input_record> out;
  while (st.step()) out.push_back(input_record{st.u64(0), st.text(1), st.u64(2), st.text(3), st.text(4), st.u64(5), st.u64(6)});
  return out;
}

}  // namespace astro
