#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "astro/calibration.hpp"

namespace astro {

// Master del prodotto (C-02.a): media per pixel dei frame raw di un gruppo, piano VARIANCE empirico s^2/n.

enum class flat_normalization { none, median_ratio };

constexpr const char* master_operator_id = "master.combine";
constexpr std::uint32_t master_algorithm_version = 1;
constexpr std::uint32_t master_serialization_version = 1;

struct master_frame_spec {
  std::string path;       // solo per il job
  content_digest digest;  // identita' attesa del file; determina l'ordine canonico
  frame_metadata declared;  // solo valori con origine `declared`
  declared_image_fields declared_fields;
  fits_read_options read_options;
};

// Parametri del §1 di C-02.a: nessun default, tranne output_type (f32, registrato in nodo e report).
struct master_recipe {
  master_role role = master_role::dark;
  std::vector<master_frame_spec> frames;
  std::optional<combine_method> combine;
  std::optional<master_rejection> rejection;
  std::optional<flat_normalization> flat_norm;     // solo flat
  std::optional<double> flat_level_rel_tol;        // solo flat, in (0, 1)
  std::optional<double> flat_min_signal_adu;       // solo flat, > 0
  std::optional<double> saturation_adu;            // assente: avviso saturation_not_evaluated
  std::optional<double> saturated_fraction_max;    // in [0, 1], obbligatorio con saturation_adu
  std::optional<double> exposure_rel_tol;
  std::optional<double> temperature_abs_tol_c;
  std::optional<temperature_policy> temp_policy;
  std::optional<simd_request> simd;  // ADR-07: nessun default
  sample_type output_type = sample_type::f32;
};

struct master_result {
  image output;  // campioni M, piano VARIANCE s^2/n, maschera
  json report;
  std::string report_text;
  std::vector<std::string> warnings;
  // Solo flat: level_bias_estimate per pixel in f64 (NaN sui pixel invalidi), la stima del §6.2 su M osservato.
  std::vector<double> level_bias_estimate;
};

// Parametri, numero di frame e digest: nessun accesso ai file.
// Errori: invalid_input master.param.<nome> / master.frames_insufficient; unsupported master.variance_domain /
// calib.shared_input.
void validate_master_recipe(const master_recipe& recipe);

// Nodo DAG del master: parametri canonici, un ingresso file per frame nell'ordine canonico (digest crescente).
node_spec make_master_node(const master_recipe& recipe);

// Descrittore del master derivato dal nodo (ADR-16, aggiunte di R2). Altro operatore: invalid_input
// master.not_master_node.
master_descriptor master_descriptor_from_node(const node_spec& node);

// Preflight completo e combinazione in memoria. `frames[i]` e' il frame di `recipe.frames[i]`.
master_result combine_master(const master_recipe& recipe, std::vector<image> frames);

// Resa testuale dello stesso documento JSON del report del master.
std::string master_report_text(const json& report);

struct master_job_options {
  unsigned worker_count = 1;
  std::uint32_t tile_rows = 64;
  std::size_t queue_capacity = 4;
  // Solo test: chiamato con "frames.read" dopo la lettura completa dei frame e prima dei blocchi di righe.
  std::function<void(std::string_view)> hook;
};

constexpr const char* master_image_role = "primary";
constexpr const char* master_report_role = "report";
constexpr const char* master_report_text_role = "report_text";

// Job su uno store durevole: legge ogni frame per intero uno alla volta (digest, decodifica, preflight), poi combina a
// blocchi di righe con pread sullo stesso descrittore, e pubblica master FITS, report JSON e testo in una transazione.
job_outcome run_master_job(sqlite_dag_store& store, const std::string& slot_name, const master_recipe& recipe,
                           memory_pool& pool, const master_job_options& options, const cancel_token& cancel,
                           const std::optional<budget_snapshot>& budget = std::nullopt);

// Byte per task del job a blocchi (C-02.a, aggiunte di R2): puntatori, righe f64 e bit degli n frame, appoggio della
// lettura. nullopt se il valore non e' rappresentabile in 64 bit.
std::optional<std::uint64_t> master_task_bytes(std::uint64_t frames, std::uint64_t rows, std::uint64_t width);

// Ingresso della calibrazione per il master pubblicato da uno slot (ADR-16, aggiunte di R2).
// Slot non current: invalid_input calib.product_master_not_current.
calibration_input_spec product_master_input(const sqlite_dag_store& store, const std::string& slot_name);

}  // namespace astro
