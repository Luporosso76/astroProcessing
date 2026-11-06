#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "astro/calibration.hpp"
#include "astro/fault_injector.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"

namespace astro {

// Catene del light (ADR-16.b §b.5), nomi testuali dei contratti: compaiono nel ledger e nelle chiavi.
enum class light_chain { none, bias, dark, bias_dark_sub, dark_sub_partial };

// Stato dell'offset del light dopo la catena (C-01.b §1).
enum class offset_state { removed, known_not_removed, unknown };

// Nero applicato dal decoder (ADR-08.b §2.9): il decoder non sottrae mai il nero.
enum class black_applied_by_decoder { none };

const char* to_string(light_chain chain) noexcept;
const char* to_string(offset_state state) noexcept;
const char* to_string(black_applied_by_decoder applied) noexcept;

// Nero effettivo `black + black_per_color[FC(r, c)] + P(r, c)` (H3, ADR-08.b §2.4) in interi a 64 bit esatti, riga per
// riga, su un periodo di `lcm(2, rows) × lcm(2, cols)` siti (dimensione nulla del motivo = 1) limitato all'area visibile.
struct decoder_black_period {
  std::uint32_t rows = 0;
  std::uint32_t cols = 0;
  std::vector<std::uint64_t> values;  // rows * cols, riga per riga
  friend bool operator==(const decoder_black_period&, const decoder_black_period&) = default;
};

// Usa gli indici di `cfa_color_index` dello stato così come sono. Stato incoerente (indice colore > 3, motivo con una
// sola dimensione nulla o con dimensioni diverse dai valori, area visibile vuota): invalid_input calib.decoder_state.
decoder_black_period decoder_black_period_of(const decoder_state& state);

// Master che ha tolto l'offset del light.
struct offset_remover {
  master_role role = master_role::bias;
  content_digest digest;
};

// Ingressi della catena del light: esattamente quelli che la catena richiede (ADR-16.b §b.5, colonna «Ingressi»), più
// l'identità del decoder per un light con `decoder_state`.
struct light_offset_inputs {
  std::optional<content_digest> bias, dark, dark_prime;
  std::optional<decoder_identity> decoder;
};

// Voce `light_offset` (C-01.b §10): il nero non si sottrae mai dai campioni.
struct light_offset_entry {
  offset_state state = offset_state::unknown;
  std::optional<offset_remover> remover;  // assente se nessun master toglie l'offset
  // Solo per un light con `decoder_state`; assenti, non nulli, per ogni altro light.
  std::optional<decoder_black_period> black;
  std::optional<black_applied_by_decoder> black_applied;
  std::optional<decoder_identity> identity;
};

// Stato e voce `light_offset` di un light per una catena (C-01.b §1, §2). `OFFSET` FITS non cambia lo stato.
// Errori: unsupported calib.pedestal (`pedestal_adu` diverso da zero); invalid_input calib.chain_input (ingresso richiesto
// dalla catena assente), calib.chain_input_not_applicable (ingresso non richiesto, o identità del decoder su un light
// senza `decoder_state`), calib.decoder_identity (light con `decoder_state` senza identità), calib.decoder_state.
light_offset_entry make_light_offset_entry(light_chain chain, const image& light, const light_offset_inputs& inputs);

// ---------------------------------------------------------------- analisi G-CAL (ADR-16.b §b.1 punto 1, §b.2, §b.3, §b.7)

// Candidato di un ruolo: immagine letta (header, descrittore, stato del decoder), descrittore e identità del file.
struct analysis_candidate {
  image img;
  master_descriptor descriptor;
  content_digest digest;
};

// Dichiarazioni della ricetta lette dall'analisi. Nessun default: un valore richiesto assente ferma l'analisi.
struct calibration_declarations {
  std::optional<double> exposure_rel_tol;
  std::optional<double> temperature_abs_tol_c;
  std::optional<temperature_policy> temp_policy;
  std::optional<std::uint32_t> decoder_black_abs_tol_adu;
};

struct calibration_analysis_input {
  image light;
  content_digest light_digest;
  std::optional<analysis_candidate> bias, dark, dark_flat, flat;
  calibration_declarations declarations;
};

enum class candidate_status { usable, excluded };

// Esito di un candidato in una catena di §b.5 (nome testuale del contratto); `reason` solo se `excluded`, dalla lista
// chiusa di §b.3.
struct chain_verdict {
  std::string chain;
  candidate_status status = candidate_status::excluded;
  std::optional<std::string> reason;
};

// `usable` se lo è in almeno una catena (§b.1). Le catene sono quelle di §b.5 che accettano il ruolo e lo stato
// dichiarato, in ordine di tabella; una coppia con un candidato assente non si valuta.
struct candidate_analysis {
  master_role role = master_role::bias;
  candidate_status status = candidate_status::excluded;
  std::vector<chain_verdict> chains;
};

struct calibration_analysis {
  std::vector<candidate_analysis> candidates;  // bias, dark, dark_flat, flat, solo i presenti
  // Avvisi di ADR-16.a §2: `imagetyp_unrecognized:<ingresso>:<valore>`, `temperature_comparison_skipped:<coppia>`.
  std::vector<std::string> warnings;
};

// Funzione pura degli ingressi. Un master fuori regola è `excluded` con motivo, mai uno stop. Stop, tutti prima di ogni
// calcolo (`fault` è chiamato con "calib.analysis.pair" prima di ogni coppia; un valore diverso da zero dà io
// calib.analysis.fault):
// - light: metadata_missing calib.metadata.phase / calib.metadata.unit; incompatible op.phase_blocked /
//   op.linearity_blocked (riga `calibrate_light`, `role` nel contesto); unsupported calib.unit, calib.plane_layout,
//   calib.coverage_input, calib.pedestal; incompatible calib.imagetyp_conflict; invalid_input calib.decoder_state;
// - descrittori: invalid_input calib.role_slot, calib.param_not_applicable.flat_normalization_declared,
//   calib.decoder_state (stato di un frame incoerente);
// - dichiarazioni: invalid_input calib.param.<nome> (tolleranza richiesta da una coppia presente, assente o fuori
//   dominio), calib.param.decoder_black_abs_tol_adu (assente con un ingresso con stato del decoder),
//   calib.param_not_applicable.decoder_black_abs_tol_adu (presente senza).
calibration_analysis analyze_calibration(const calibration_analysis_input& input, const fault_injector& fault = {});

}  // namespace astro
