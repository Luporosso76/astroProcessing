#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "astro/calibration.hpp"
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

// Identità del decoder (ADR-16.b §b.7, ADR-08.b §2.9): le quattro chiavi «Libreria» del record di `raw_decoder_record`.
struct decoder_identity {
  std::string libraw_version;
  std::uint32_t libraw_version_number = 0;
  std::uint32_t libraw_capabilities = 0;
  std::string libraw_tarball_sha256;

  decoder_identity() = default;
  decoder_identity(std::string version, std::uint32_t version_number, std::uint32_t capabilities, std::string sha256)
      : libraw_version(std::move(version)),
        libraw_version_number(version_number),
        libraw_capabilities(capabilities),
        libraw_tarball_sha256(std::move(sha256)) {}
  friend bool operator==(const decoder_identity&, const decoder_identity&) = default;
};

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

}  // namespace astro
