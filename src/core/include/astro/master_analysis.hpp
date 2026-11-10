#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "astro/batch_classification.hpp"
#include "astro/calibration.hpp"
#include "astro/cancel.hpp"
#include "astro/fault_injector.hpp"
#include "astro/hash.hpp"
#include "astro/json.hpp"
#include "astro/memory_pool.hpp"
#include "astro/sqlite_dag_store.hpp"

namespace astro {

// Analisi dei master (C-02.b §b.2): esclusioni per frame sul set candidato di un ruolo.

// Parametri d'analisi (L4). Le quattro tolleranze comuni stanno al livello superiore di `batch.analysis_params`, gli altri
// nel membro `master`. Un parametro assente resta assente: nessun default.
struct master_analysis_params {
  std::optional<double> exposure_rel_tol, temperature_abs_tol_c;
  std::optional<temperature_policy> temp_policy;
  std::optional<std::uint32_t> decoder_black_abs_tol_adu;
  std::optional<double> saturation_adu, saturated_fraction_max, flat_min_signal_adu, flat_level_rel_tol, offset_adu;
};

// `settings` `batch.analysis_params`, con lo stesso parser dei light. Documento assente: tutti i parametri assenti.
// Chiave sconosciuta (anche dentro `master`), tolleranza comune dentro `master`, `master` non oggetto o valore di tipo
// sbagliato: invalid_input batch.analysis_params.
master_analysis_params read_master_analysis_params(const sqlite_dag_store& store);

// Esito di decodifica fallita di un frame: codice dello stop di decodifica e suo dettaglio.
struct master_decode_failure {
  std::string code;
  std::string detail;
};

// Statistiche di un frame (L2), ingresso puro dell'analisi.
struct master_frame_stats {
  std::optional<master_decode_failure> decode_failure;  // se presente, gli altri campi non contano
  std::uint64_t saturated_samples = 0;  // campioni validi >= saturation_adu
  std::uint64_t pixel_count = 0;        // width * height
  // m_(f,c) per canale: 1 per un mono, 4 per un CFA nell'ordine dei siti (0,0) (0,1) (1,0) (1,1) del motivo. nullopt per un
  // canale senza pixel validi. Vuoto se non richiesti.
  std::vector<std::optional<double>> levels;
};

struct master_stats_request {
  std::optional<double> saturation_adu;  // assente: campioni saturi non contati
  bool flat_levels = false;
  // Offset sottratto ai livelli, obbligatorio con `flat_levels`: per un FITS quello dell'osservazione (header o
  // dichiarato), per un RAW `offset_adu` dei parametri d'analisi.
  std::optional<double> offset_adu;
};

// Lettore FITS verificato (OB-11): stesso percorso di apertura, piano, lettura verificata e decodifica del controllo termico.
// Un errore di piano o decodifica FITS con codice `fits.*` o `image.*` (compreso `fits.checksum`) diventa `decode_failure`,
// dopo la verifica del digest. Integrità OB-11 (`input.*`), I/O (`input.missing`), budget (`sched.*`) e annullamento si
// propagano come stop.
master_frame_stats read_master_frame_stats(const std::string& path, const content_digest& digest, const master_stats_request& request,
                                           memory_pool& pool, const cancel_token& cancel);

struct master_analysis_frame {
  std::uint64_t input_id = 0;
  content_digest digest;
  input_observation observation;
  // Letta solo se il frame arriva al motivo che la usa: `decode_failed` (se presente), `saturated_fraction` con
  // `saturation_adu` dichiarato e `flat_low_signal` per il flat (obbligatoria in questi due casi).
  std::optional<master_frame_stats> stats;
};

struct master_exclusion {
  std::uint64_t input_id = 0;
  content_digest digest;
  std::string reason;  // primo motivo della lista chiusa di C-02.b §b.2
  json values;
};

struct master_screening {
  std::vector<std::string> missing_params;  // `params_missing`: nessuna esclusione, nessun frame utilizzabile
  std::vector<master_exclusion> excluded;   // ordine canonico (digest, poi input_id)
  std::vector<std::uint64_t> usable;        // input_id, ordine canonico
};

// Funzione pura degli ingressi, indipendente dall'ordine dei frame. Parametri richiesti assenti: `missing_params`, senza
// stop. Soglia di saturazione di un frame con `saturation_adu` assente (prima di `params_missing`): invalid_input
// master.param.saturation_adu.
// Parametro fuori dominio (dopo `params_missing`): invalid_input master.param.<nome>. Nessun frame utilizzabile:
// invalid_input master.no_usable_frames. Altrimenti chiama `fault("master.analysis.groups")`
// una volta, dopo le esclusioni.
master_screening screen_master_frames(master_role role, const std::vector<master_analysis_frame>& frames,
                                      const master_analysis_params& params, const fault_injector& fault = {});

}  // namespace astro
