#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "astro/calibration.hpp"
#include "astro/master_kappa.hpp"

namespace astro {

// Master del prodotto (C-02.a, C-02.b): combinazione per pixel dei frame raw di un gruppo con il metodo della ricetta e
// il piano VARIANCE del metodo (C-02.b §b.6).

enum class flat_normalization { none, median_ratio };

constexpr const char* master_operator_id = "master.combine";
constexpr std::uint32_t master_algorithm_version = 3;
constexpr std::uint32_t master_serialization_version = 1;

struct master_frame_spec {
  std::string path;       // solo per il job
  content_digest digest;  // identità attesa del file; determina l'ordine canonico
  frame_metadata declared;  // solo valori con origine `declared`
  declared_image_fields declared_fields;
  fits_read_options read_options;
  std::optional<double> weight;  // solo `weighted_mean` (C-02.b §b.10, L5): peso del frame, legato al suo digest
  // Solo frame RAW (ADR-16.b §b.7): stato e identità del decoder osservati dalla decodifica dell'analisi. Entrano nei
  // parametri del nodo come `frame_decoder_states`, nell'ordine canonico degli ingressi.
  std::optional<frame_decoder_state> decoder;
};

// Parametri del §1 di C-02.a: nessun default, tranne output_type (f32, registrato in nodo e report).
struct master_recipe {
  master_role role = master_role::dark;
  std::vector<master_frame_spec> frames;
  std::optional<combine_method> combine;
  std::optional<master_rejection> rejection;
  std::optional<flat_normalization> flat_norm;     // solo flat
  std::optional<double> flat_level_rel_tol;        // solo flat con `none`, in (0, 1); vietato con `median_ratio`
  std::optional<double> flat_min_signal_adu;       // solo flat, > 0
  // Solo flat con `median_ratio` (C-02.b-flat §f.2), vietati altrimenti: A >= 0 per ogni metodo; budget in (0, 1) per
  // ogni metodo tranne `mean`, che lo vieta.
  std::optional<double> flat_additive_bound_adu;
  std::optional<double> flat_norm_rel_budget;
  std::optional<double> saturation_adu;            // assente: avviso saturation_not_evaluated
  std::optional<double> saturated_fraction_max;    // in [0, 1], obbligatorio con saturation_adu
  std::optional<double> exposure_rel_tol;
  std::optional<double> temperature_abs_tol_c;
  std::optional<temperature_policy> temp_policy;
  std::optional<simd_request> simd;  // ADR-07: nessun default
  // C-02.b §b.1, §b.10: parametri del metodo scelto, vietati per gli altri. winsor_g, clip_max_iter e clip_min_kept
  // sono interi dichiarati come double: un valore non intero è `master.param.<nome>`.
  std::optional<double> winsor_g;          // solo winsorized, >= 1
  std::optional<double> clip_kappa_low;    // solo sigma_clip, > 0
  std::optional<double> clip_kappa_high;   // solo sigma_clip, > 0
  std::optional<double> clip_max_iter;     // solo sigma_clip, >= 1
  std::optional<double> clip_min_kept;     // solo sigma_clip, in [2, n]
  // Modello di rumore di `single` (C-02.b §b.6).
  std::optional<double> offset_adu;
  std::optional<double> conversion_gain_e_per_adu;
  std::optional<double> read_noise_adu;
  // Tolleranza del nero effettivo fra frame RAW (ADR-16.b §b.7, C-02.b §b.2): obbligatoria con frame RAW, vietata
  // altrimenti.
  std::optional<std::uint32_t> decoder_black_abs_tol_adu;
  // Esito canonico dell'analisi da cui il nodo nasce (L8): oggetto JSON oppure null per un nodo non proposto.
  json analysis;
  sample_type output_type = sample_type::f32;
};

struct master_result {
  image output;  // campioni M, piano VARIANCE del metodo, maschera
  json report;
  std::string report_text;
  std::vector<std::string> warnings;
  // Solo flat: level_bias_estimate per pixel in f64 (NaN sui pixel invalidi), la stima del §6.2 su M osservato; con
  // `single` non è definita (C-02.b §b.7) e con `median_ratio` non si calcola (motivo `normalized`, C-02.b-flat §f.13
  // punto 3): in entrambi i casi è NaN ovunque.
  std::vector<double> level_bias_estimate;
};

// Parametri, numero di frame e digest: nessun accesso ai file.
// Errori: invalid_input master.param.<nome>; metadata_missing / invalid_input var.source.<campo> / var.param.<nome> (solo `single`);
// unsupported master.variance_domain / calib.shared_input.
void validate_master_recipe(const master_recipe& recipe);

// Nodo DAG del master: parametri canonici, un ingresso file per frame nell'ordine canonico (digest crescente).
node_spec make_master_node(const master_recipe& recipe);

// Descrittore del master derivato dal nodo (ADR-16, aggiunte di R2). Altro operatore: invalid_input
// master.not_master_node.
master_descriptor master_descriptor_from_node(const node_spec& node);

// Preflight completo e combinazione in memoria. `frames[i]` è il frame di `recipe.frames[i]`.
master_result combine_master(const master_recipe& recipe, std::vector<image> frames);

// Resa testuale dello stesso documento JSON del report del master.
std::string master_report_text(const json& report);

// Blocco della riverifica del digest e statistiche per pixel del report: termini D e K del modello del master
// (ADR-06, estensione R2.r), condivisi da pianificatore e job.
constexpr std::uint64_t master_digest_chunk_bytes = std::uint64_t{1} << 20;
constexpr std::uint64_t master_stats_bytes_per_pixel = 32;

struct master_job_options {
  // Worker, tile e coda dei task li sceglie plan_resources con frame_count = numero di frame, dal budget del pool; i
  // worker_count e tile_rows espliciti restano intatti. Configurazione e origini entrano nella provenance solo se il
  // chiamante passa lo snapshot del budget: senza snapshot non viene registrato nulla.
  resource_config resources;
  // Solo test: chiamato con "frames.read" dopo la lettura completa dei frame e prima dei blocchi di righe.
  std::function<void(std::string_view)> hook;
  // Solo test: il job_plan effettivamente passato allo scheduler.
  std::function<void(const job_plan&)> on_job_plan;
  // Solo test: riceve il κ dei metodi robusti calcolato nel preflight, dopo robust_kappa e prima di
  // check_kappa_accuracy e di ogni lettura di campioni; può alterarne la stima d'errore (C-02.b §b.6).
  std::function<void(kappa_result&)> on_kappa;
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
// lettura. nullopt se il valore non è rappresentabile in 64 bit.
std::optional<std::uint64_t> master_task_bytes(std::uint64_t frames, std::uint64_t rows, std::uint64_t width);

// Ingresso della calibrazione per il master pubblicato da uno slot (ADR-16, aggiunte di R2).
// Slot non current: invalid_input calib.product_master_not_current.
calibration_input_spec product_master_input(const sqlite_dag_store& store, const std::string& slot_name);

}  // namespace astro
