#include "astro/sqlite_dag_store.hpp"
#include "sqlite_internal.hpp"

namespace astro {

using namespace sqlint;

void sqlite_dag_store::create_snapshot_locked() {
  statement cur(db_, "SELECT current_snapshot_id FROM history_cursor WHERE id=1");
  if (!cur.step()) throw stop_error(error_category::integrity, "store.history_cursor", "riga history_cursor assente");
  statement ins(db_, "INSERT INTO snapshots(parent_id) VALUES(?1)");
  ins.bind_opt_u64(1, cur.opt_u64(0)).run();
  const auto id = static_cast<std::uint64_t>(sqlite3_last_insert_rowid(db_));
  statement entries(db_, "INSERT INTO snapshot_entries(snapshot_id, slot_name, node_key, publish_role) "
                         "SELECT ?1, slot_name, target_node_key, publish_role FROM slots WHERE detached=0");
  entries.bind_u64(1, id).run();
  statement move(db_, "UPDATE history_cursor SET current_snapshot_id=?1, redo_snapshot_id=NULL WHERE id=1");
  move.bind_u64(1, id).run();
}

history_state sqlite_dag_store::history() const {
  std::lock_guard lock(mutex_);
  statement st(db_, "SELECT current_snapshot_id, redo_snapshot_id FROM history_cursor WHERE id=1");
  if (!st.step()) throw stop_error(error_category::integrity, "store.history_cursor", "riga history_cursor assente");
  return history_state{st.opt_u64(0), st.opt_u64(1)};
}

std::optional<std::uint64_t> sqlite_dag_store::snapshot_parent(std::uint64_t snapshot_id) const {
  std::lock_guard lock(mutex_);
  statement st(db_, "SELECT parent_id FROM snapshots WHERE id=?1");
  if (!st.bind_u64(1, snapshot_id).step()) {
    throw stop_error(error_category::invalid_input, "history.unknown_snapshot", "snapshot inesistente",
                     {{"snapshot", std::to_string(snapshot_id)}});
  }
  return st.opt_u64(0);
}

std::vector<snapshot_entry> sqlite_dag_store::snapshot_entries(std::uint64_t snapshot_id) const {
  std::lock_guard lock(mutex_);
  statement st(db_, "SELECT slot_name, node_key, publish_role FROM snapshot_entries WHERE snapshot_id=?1 ORDER BY slot_name");
  st.bind_u64(1, snapshot_id);
  std::vector<snapshot_entry> out;
  while (st.step()) out.push_back(snapshot_entry{st.text(0), st.text(1), st.text(2)});
  return out;
}

std::vector<std::string> sqlite_dag_store::move_history(bool undo_direction) {
  require_writable();
  std::lock_guard lock(mutex_);
  transaction tx(db_);
  statement cur(db_, "SELECT current_snapshot_id, redo_snapshot_id FROM history_cursor WHERE id=1");
  if (!cur.step()) throw stop_error(error_category::integrity, "store.history_cursor", "riga history_cursor assente");
  const auto current = cur.opt_u64(0);
  const auto redo = cur.opt_u64(1);
  std::optional<std::uint64_t> target;
  if (undo_direction) {
    if (current) {
      statement parent(db_, "SELECT parent_id FROM snapshots WHERE id=?1");
      if (parent.bind_u64(1, *current).step()) target = parent.opt_u64(0);
    }
    if (!target) throw stop_error(error_category::invalid_input, "history.no_undo", "nessuno snapshot precedente");
  } else {
    if (!redo) throw stop_error(error_category::invalid_input, "history.no_redo", "nessuno snapshot da ripristinare");
    target = redo;
  }
  statement dangling(db_, "SELECT e.node_key FROM snapshot_entries e LEFT JOIN nodes n ON n.node_key=e.node_key "
                          "WHERE e.snapshot_id=?1 AND n.node_key IS NULL LIMIT 1");
  if (dangling.bind_u64(1, *target).step()) {
    throw stop_error(error_category::integrity, "history.dangling_node", "lo snapshot cita un nodo assente",
                     {{"nodo", dangling.text(0)}, {"snapshot", std::to_string(*target)}});
  }
  statement entries(db_, "SELECT slot_name, node_key, publish_role FROM snapshot_entries WHERE snapshot_id=?1 ORDER BY slot_name");
  entries.bind_u64(1, *target);
  statement apply(db_, "UPDATE slots SET target_node_key=?2, current_generation=current_generation+1, detached=0, "
                       "publish_role=?3, artifact_id=(SELECT id FROM artifacts WHERE node_key=?2 AND present=1 "
                       "AND output_role=?3 ORDER BY id DESC LIMIT 1) WHERE slot_name=?1");
  std::vector<std::string> slots;
  while (entries.step()) {
    apply.bind(1, entries.text(0)).bind(2, entries.text(1)).bind(3, entries.text(2)).run();
    slots.push_back(entries.text(0));
  }
  statement detach(db_, "UPDATE slots SET target_node_key=NULL, artifact_id=NULL, detached=1, "
                        "current_generation=current_generation+1 WHERE detached=0 AND slot_name NOT IN "
                        "(SELECT slot_name FROM snapshot_entries WHERE snapshot_id=?1)");
  detach.bind_u64(1, *target).run();
  statement move(db_, "UPDATE history_cursor SET current_snapshot_id=?1, redo_snapshot_id=?2 WHERE id=1");
  move.bind_u64(1, *target);
  if (undo_direction) move.bind_opt_u64(2, current); else move.bind_null(2);
  move.run();
  tx.commit();
  return slots;
}

std::vector<std::string> sqlite_dag_store::undo() { return move_history(true); }
std::vector<std::string> sqlite_dag_store::redo() { return move_history(false); }

}  // namespace astro
