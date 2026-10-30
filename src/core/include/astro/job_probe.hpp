#pragma once

#include <functional>
#include <string_view>

namespace astro::job_probe {

// Solo test (suite di conformita', docs/contracts/BASE-confini.md §3.2): osservatore dei punti del confine dei job.
// Si imposta prima del job e non cambia durante l'esecuzione; e' chiamato anche dai thread dei worker, quindi deve
// essere thread-safe. Vuoto: nessun effetto. Punti e dettaglio:
// - `node.build` (operator_id): costruzione di un node_spec;
// - `input.open` (percorso): ingresso in open_fits_input; `input.read`: ogni lettura di un input aperto;
// - `input.samples`: uso dei campioni di un input, con `ino:<inode>` del file aperto oppure `sha256:<digest>` dei byte
//   decodificati da memoria (decodifica, righe, blocchi, anteprima, controlli per campione);
// - `sched.plan` (job_plan serializzato): ingresso in job_scheduler::run; `sched.prepare`: prima di `prepare`;
//   `sched.task` (indice): subito prima di `run_task`; `sched.task_done` (indice): dopo il ritorno di `run_task`;
//   `sched.finalize`: prima di `finalize`.
using observer = std::function<void(std::string_view point, std::string_view detail)>;

void set_observer(observer o);
// Vero se un osservatore e' impostato: i dettagli costosi da calcolare si calcolano solo allora.
bool active();
void notify(std::string_view point, std::string_view detail = {});

}  // namespace astro::job_probe
