#include "astro/workflow.hpp"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <set>
#include <span>

#include "astro/hash.hpp"
#include "sqlite_internal.hpp"
#include "stop_internal.hpp"

namespace astro {

namespace {

std::string setting_key(const workflow& wf) { return "workflow." + wf.name; }

void validate(const workflow& wf) {
  const auto invalid = [&](const char* why) {
    throw stop_error(error_category::invalid_input, "workflow.definition", why, {{"workflow", wf.name}});
  };
  if (wf.name.empty() || wf.name.size() > 64 ||
      !std::all_of(wf.name.begin(), wf.name.end(),
                   [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; })) {
    invalid("nome del workflow non valido");
  }
  if (wf.steps.empty()) invalid("workflow senza passi");
  std::set<std::string> slots;
  for (const auto& s : wf.steps) {
    if (!slots.insert(s.slot_name).second) invalid("slot ripetuto nel workflow");
    if (!s.run) invalid("passo senza esecuzione");
  }
}

std::string definition_canonical(const workflow& wf) {
  json steps = json::array();
  for (const auto& s : wf.steps) {
    json o = json::object();
    o.set("slot_name", json::string(s.slot_name));
    o.set("node_key", json::string(s.node.key()));
    steps.push(std::move(o));
  }
  json doc = json::object();
  doc.set("format", json::string(workflow_format));
  doc.set("name", json::string(wf.name));
  doc.set("steps", std::move(steps));
  return canonical_json(doc);
}

std::string sha256_of(const std::string& text) {
  return sha256_hex(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
}

// Arresto del workflow prima di ogni job (OB-03): registra `original` e lo rilancia invariato. Il record
// e' l'impostazione `<workflow_stop_prefix><sha256 della definizione data>.<esecuzione>`, con l'esecuzione la prima
// libera da 1, e contiene solo stringhe e interi. Registrazione fallita: diagnosi su stderr e arresto originale con
// `registrazione_arresto`, mai l'errore secondario al suo posto.
[[noreturn]] void stop_workflow(sqlite_dag_store& store, const workflow& wf, const char* operation,
                                const std::exception_ptr& original) {
  try {
    const stop_error cause = stopint::cause_of(original);
    const std::string digest = sha256_of(definition_canonical(wf));
    const std::string prefix = workflow_stop_prefix + digest + ".";
    std::uint64_t execution = 1;
    while (store.get_setting(prefix + std::to_string(execution))) ++execution;
    json context = json::array();
    for (const auto& [key, value] : cause.context()) {
      json pair = json::array();
      pair.push(json::string(key));
      pair.push(json::string(value));
      context.push(std::move(pair));
    }
    json record = json::object();
    record.set("format", json::string(workflow_stop_format));
    record.set("workflow", json::string(wf.name));
    record.set("definition_sha256", json::string(digest));
    record.set("execution", json::uinteger(execution));
    record.set("operation", json::string(operation));
    record.set("category", json::string(to_string(cause.category())));
    record.set("code", json::string(cause.code()));
    record.set("message", json::string(cause.message()));
    record.set("context", std::move(context));
    store.insert_setting(prefix + std::to_string(execution), canonical_json(record));
  } catch (...) {
    const std::exception_ptr secondary = std::current_exception();
    std::fprintf(stderr, "registrazione dell'arresto del workflow fallita (%s); si propaga l'arresto: %s\n",
                 stopint::what_of(secondary), stopint::what_of(original));
    stopint::rethrow_with_record_failure(original, stopint::what_of(secondary));
  }
  std::rethrow_exception(original);
}

// Target attuale dello slot del passo; nullopt se lo slot manca o e' staccato.
std::optional<std::string> target_of(const sqlite_dag_store& store, const workflow_step& step) {
  const std::optional<slot_record> slot = store.get_slot(step.slot_name);
  if (!slot || slot->detached) return std::nullopt;
  return slot->target_node_key;
}

// Esegue i passi indicati in ordine; il primo passo fermato chiude il workflow.
void execute(const workflow& wf, const std::vector<bool>& to_run, workflow_result& result) {
  for (std::size_t i = 0; i < wf.steps.size(); ++i) {
    if (!to_run[i]) continue;
    const workflow_step& step = wf.steps[i];
    sqlint::inject(&wf.hook, "workflow.step_begin");
    job_outcome outcome = step.run();
    if (outcome.job.slot_name != step.slot_name || outcome.job.node_key != step.node.key()) {
      throw stop_error(error_category::invalid_input, "workflow.step_job", "il passo ha eseguito il job di un altro slot o nodo",
                       {{"passo", step.slot_name}, {"slot_eseguito", outcome.job.slot_name},
                        {"nodo_atteso", step.node.key()}, {"nodo_eseguito", outcome.job.node_key}});
    }
    const int code = outcome.exit_code;
    result.steps[i].action = step_action::executed;
    result.steps[i].outcome = std::move(outcome);
    sqlint::inject(&wf.hook, "workflow.step_end");
    if (code != 0) {
      result.exit_code = code;
      return;
    }
  }
}

workflow_result empty_result(const workflow& wf) {
  workflow_result r;
  for (const auto& s : wf.steps) r.steps.push_back(workflow_step_result{s.slot_name, step_action::not_reached, std::nullopt});
  return r;
}

// Il contesto del resume vale solo durante l'esecuzione dei passi del resume.
class resume_scope {
 public:
  resume_scope(sqlite_dag_store& store, json context) : store_(store) { store_.set_resume_context(std::move(context)); }
  ~resume_scope() { store_.set_resume_context(std::nullopt); }
  resume_scope(const resume_scope&) = delete;
  resume_scope& operator=(const resume_scope&) = delete;

 private:
  sqlite_dag_store& store_;
};

// Controlli di run_workflow prima del primo passo e registrazione della definizione.
void start_run(sqlite_dag_store& store, const workflow& wf) {
  validate(wf);
  for (const auto& s : wf.steps) {
    const std::optional<std::string> target = target_of(store, s);
    if (!target || *target != s.node.key()) {
      throw stop_error(error_category::invalid_input, "workflow.slot_target", "il target dello slot non e' il nodo del passo",
                       {{"slot", s.slot_name}, {"nodo_passo", s.node.key()}, {"target", target.value_or("")}});
    }
  }
  store.put_setting(setting_key(wf), definition_canonical(wf));
}

struct resume_start {
  std::vector<bool> to_run;
  json context;
};

// Controlli di resume_workflow prima del primo passo: passi da eseguire e contesto del resume.
resume_start plan_resume(sqlite_dag_store& store, const workflow& wf) {
  validate(wf);
  const std::optional<std::string> stored = store.get_setting(setting_key(wf));
  if (!stored) {
    throw stop_error(error_category::resume_mismatch, "resume.unknown_workflow", "nessun workflow registrato con questo nome",
                     {{"workflow", wf.name}});
  }
  const std::string given = definition_canonical(wf);
  if (*stored != given) {
    throw stop_error(error_category::resume_mismatch, "resume.recipe",
                     "la definizione del workflow differisce da quella registrata: non e' una prosecuzione",
                     {{"workflow", wf.name}, {"registrata", *stored}, {"data", given}});
  }
  for (const auto& s : wf.steps) {
    const std::optional<std::string> target = target_of(store, s);
    if (!target || *target != s.node.key()) {
      throw stop_error(error_category::resume_mismatch, "resume.recipe", "il target dello slot non e' il nodo del passo",
                       {{"slot", s.slot_name}, {"atteso", s.node.key()}, {"trovato", target.value_or("")}});
    }
  }
  std::vector<bool> to_run(wf.steps.size(), true);
  json reused = json::array();
  for (std::size_t i = 0; i < wf.steps.size(); ++i) {
    const workflow_step& s = wf.steps[i];
    std::optional<phase_record> phase;
    try {
      phase = store.check_phase(s.slot_name);
    } catch (const stop_error& e) {
      if (e.code() != "store.artifact_missing") throw;
      sqlint::rethrow_as(e, error_category::io, "resume.artifact_missing", "un output della fase e' sparito dopo la recovery");
    }
    if (!phase) continue;
    for (const phase_defect& d : phase->defects) {
      if (d.cause == phase_defect_cause::digest) {
        throw stop_error(error_category::integrity, "resume.artifact_digest",
                         "un output della fase ha digest diverso da quello registrato",
                         {{"slot", s.slot_name}, {"artefatto", std::to_string(d.artifact_id)}, {"rel_path", d.rel_path},
                          {"atteso", d.expected_sha256}, {"trovato", d.found_sha256}});
      }
    }
    if (!phase->defects.empty()) continue;  // solo output absent: il nodo si ricostruisce
    to_run[i] = false;
    json digests = json::array();
    for (const artifact_record& a : store.artifacts_for(phase->node_key)) {
      if (std::find(phase->artifact_ids.begin(), phase->artifact_ids.end(), a.id) != phase->artifact_ids.end()) {
        digests.push(json::string(a.sha256));
      }
    }
    json item = json::object();
    item.set("slot_name", json::string(s.slot_name));
    item.set("node_key", json::string(phase->node_key));
    item.set("generation", json::uinteger(phase->generation));
    item.set("artifacts", std::move(digests));
    reused.push(std::move(item));
  }
  json context = json::object();
  context.set("workflow", json::string(wf.name));
  context.set("definition_sha256", json::string(sha256_of(*stored)));
  context.set("reused", std::move(reused));
  return resume_start{std::move(to_run), std::move(context)};
}

}  // namespace

workflow_result run_workflow(sqlite_dag_store& store, const workflow& wf) {
  try {
    start_run(store, wf);
  } catch (...) {
    stop_workflow(store, wf, "run", std::current_exception());
  }
  workflow_result result = empty_result(wf);
  execute(wf, std::vector<bool>(wf.steps.size(), true), result);
  return result;
}

workflow_result resume_workflow(sqlite_dag_store& store, const workflow& wf) {
  std::optional<resume_start> start;
  try {
    start.emplace(plan_resume(store, wf));
  } catch (...) {
    stop_workflow(store, wf, "resume", std::current_exception());
  }
  workflow_result result = empty_result(wf);
  for (std::size_t i = 0; i < wf.steps.size(); ++i) {
    if (!start->to_run[i]) result.steps[i].action = step_action::reused;
  }
  const resume_scope scope(store, std::move(start->context));
  execute(wf, start->to_run, result);
  return result;
}

}  // namespace astro
