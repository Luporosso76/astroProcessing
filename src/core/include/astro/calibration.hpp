#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "astro/fits.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "astro/job.hpp"
#include "astro/resources.hpp"
#include "astro/json.hpp"
#include "astro/memory_pool.hpp"
#include "astro/node.hpp"
#include "astro/sqlite_dag_store.hpp"

namespace astro {

// Ricetta M2 `dark_matched_flat_dark_flat` (ADR-16.a) con varianza diagonale (ADR-17.a).

enum class master_role { bias, dark, dark_flat, flat };
enum class additive_state { bias_included, bias_subtracted, unknown };
enum class scaling_state { unscaled, scaled, unknown };
enum class flat_state { raw, corrected_unnormalized, calibrated, unknown };
enum class combine_method { mean, median, other };
enum class normalization_mode { global_median, cfa_channel_median };
enum class temperature_policy { compare, not_applicable_declared };
enum class variance_source { plane, noise_model };
enum class covariance_model { diagonal_independent };
enum class master_rejection { none, sigma_clip };
enum class variance_estimator { empirical };

constexpr const char* calibration_operator_id = "calib.dark_matched_flat_dark_flat";
constexpr std::uint32_t calibration_algorithm_version = 1;
constexpr std::uint32_t calibration_serialization_version = 1;
constexpr const char* calibration_backend = "cpu_scalar";

// Stato dichiarato di un master. I campi non pertinenti al ruolo restano nullopt.
struct master_descriptor {
  master_role role = master_role::dark;
  std::optional<additive_state> additive;
  std::optional<scaling_state> scaling;
  std::optional<flat_state> flat;
  std::optional<std::uint32_t> combine_count;
  std::optional<combine_method> combine;
  // Master del prodotto (ADR-16, aggiunte di R2): presenti nel descrittore derivato dal nodo del master.
  std::optional<master_rejection> rejection;
  std::optional<variance_estimator> estimator;
  friend bool operator==(const master_descriptor&, const master_descriptor&) = default;
};

// Valori che il formato FITS di M2 non porta in frame_metadata e che la ricetta puo' dichiarare.
// Un valore dichiarato diverso da quello letto: incompatible, metadata.declared_conflict.
struct declared_image_fields {
  std::optional<std::uint32_t> binning_x;
  std::optional<std::uint32_t> binning_y;
  std::optional<pixel_unit> unit;
  std::optional<linearity> lin;
};

struct variance_spec {
  std::optional<variance_source> source;
  std::optional<double> read_noise_adu;  // solo noise_model
};

struct calibration_input_spec {
  std::string path;       // solo per il livello file
  content_digest digest;  // identita' attesa del file
  std::optional<master_descriptor> descriptor;  // assente per il light
  frame_metadata declared;                      // solo valori con origine `declared`
  declared_image_fields declared_fields;
  fits_read_options read_options;
  variance_spec variance;
  // Master del prodotto: chiave del nodo del master, ingresso `from_node` del nodo di calibrazione.
  std::optional<std::string> master_node_key;
};

struct calibration_recipe {
  calibration_input_spec light, dark, flat, dark_flat;
  std::optional<normalization_mode> normalization;
  std::optional<double> q_min;
  std::optional<double> q_rel_sigma_max;
  std::optional<double> exposure_rel_tol;
  std::optional<double> temperature_abs_tol_c;
  std::optional<temperature_policy> temp_policy;
};

struct calibration_images {
  image light, dark, flat, dark_flat;
};

// Stima della normalizzazione per un canale (ADR-16 §6, ADR-17).
struct channel_normalization {
  std::string label;
  std::uint64_t n_valid = 0;
  std::uint64_t fc_nonpositive = 0;
  double n = 0;
  double rel_var_estimate = 0;
  std::optional<double> fc_cv;
  bool iid_not_verified = true;
};

struct calibration_result {
  image output;
  json report;
  std::string report_text;
  std::vector<std::string> warnings;
};

// Parametri e immagini gia' verificati, con l'uscita da riempire per righe. Costruita solo da prepare_calibration.
class prepared_calibration {
 public:
  prepared_calibration(prepared_calibration&&) noexcept;
  prepared_calibration& operator=(prepared_calibration&&) noexcept;
  ~prepared_calibration();

  std::uint32_t width() const noexcept;
  std::uint32_t height() const noexcept;
  const std::vector<channel_normalization>& normalization() const noexcept;
  const std::vector<std::string>& warnings() const noexcept;

  // Elenco JSON dei quattro ingressi letti (digest, fase, linearita', keyword non interpretate) per la provenance.
  json observed_inputs() const;

  // Byte di scratch per riga richiesti da calibrate_rows (per task).
  std::uint64_t row_scratch_bytes() const noexcept;

  // Calcola le righe [row_begin, row_end); thread-safe su intervalli disgiunti. `scratch` ha almeno row_scratch_bytes().
  void calibrate_rows(std::uint32_t row_begin, std::uint32_t row_end, void* scratch);

  // Digest SHA-256 esadecimale dei byte di campioni, maschera e varianza prodotti per le righe indicate.
  std::string rows_digest(std::uint32_t row_begin, std::uint32_t row_end) const;

  struct impl;

 private:
  friend prepared_calibration prepare_calibration(const calibration_recipe&, calibration_images);
  friend calibration_result finish_calibration(prepared_calibration&&);
  explicit prepared_calibration(std::unique_ptr<impl> impl);
  std::unique_ptr<impl> impl_;
};

// Parametri, ruoli, stati dei master, digest e sorgenti di varianza: nessun accesso ai file.
// Errori: invalid_input calib.param.<nome> / calib.role_slot / var.param.<nome>; metadata_missing
// calib.master_state_unknown / var.source.<campo>; unsupported calib.master_state_m2 / calib.shared_input /
// var.combine_method.
void validate_calibration_recipe(const calibration_recipe& recipe);

// Fase di un ingresso (ADR-03, R1): assente = metadata_missing calib.metadata.phase; non raw = incompatible
// op.phase_blocked, con `input` nel contesto. Solo metadati: nessun campione e' stato letto.
void require_calibration_phase(const char* input_name, const std::optional<metadata_value<process_phase>>& phase);

// Linearita' di un ingresso dai metadati (header e dichiarazione della ricetta): conflitto = incompatible
// metadata.declared_conflict; assente = metadata_missing calib.metadata.linearity; non lineare = unsupported
// calib.linearity. Con `input` nel contesto.
void require_calibration_linearity(const char* input_name, const std::optional<linearity>& header,
                                   const std::optional<linearity>& declared);

// Preflight completo sulle immagini (nessun calcolo prima che tutto sia verificato) e normalizzazione N.
prepared_calibration prepare_calibration(const calibration_recipe& recipe, calibration_images images);

// Dopo l'ultima riga: maschera, conteggi, report.
calibration_result finish_calibration(prepared_calibration&& prepared);

// Preflight e calcolo sequenziale in memoria.
calibration_result calibrate(const calibration_recipe& recipe, calibration_images images);

// Resa testuale leggibile dello stesso documento JSON del report.
std::string calibration_report_text(const json& report);

// Nodo DAG della calibrazione: parametri canonici della ricetta, un ingresso file per ruolo.
node_spec make_calibration_node(const calibration_recipe& recipe);

struct calibration_job_options {
  unsigned worker_count = 1;
  std::uint32_t tile_rows = 64;
  std::size_t queue_capacity = 4;
  // Se presente, worker, tile e coda dei task li sceglie plan_resources dal budget del pool (i valori espliciti della
  // configurazione restano intatti) e i campi sopra sono ignorati. Configurazione e origini entrano nella provenance
  // solo se il chiamante passa anche lo snapshot del budget: senza snapshot non viene registrato nulla.
  std::optional<resource_config> resources;
  // Solo test: chiamato con "plan.done" dopo il piano dei file (prima della prenotazione) e con "preflight.done"
  // dopo il preflight e prima della riverifica dei file.
  std::function<void(std::string_view)> hook;
  // Solo test: il job_plan effettivamente passato allo scheduler (dopo plan_resources, se presente).
  std::function<void(const job_plan&)> on_job_plan;
};

constexpr const char* calibration_image_role = "primary";
constexpr const char* calibration_report_role = "report";
constexpr const char* calibration_report_text_role = "report_text";

// Job completo su uno store durevole: legge e verifica i file, prenota la memoria del calcolo nel pool, calcola per
// righe con lo scheduler e pubblica FITS calibrato, report JSON e report testuale in un'unica transazione.
// Gli errori sono confinati da run_job_files (exit 2, 3 per cancelled).
job_outcome run_calibration_job(sqlite_dag_store& store, const std::string& slot_name,
                                const calibration_recipe& recipe, memory_pool& pool,
                                const calibration_job_options& options, const cancel_token& cancel,
                                const std::optional<budget_snapshot>& budget = std::nullopt);

}  // namespace astro
