#pragma once

// Stato del decoder del progetto (U13 §8) e un cambio per ciascun membro di `dng_decoder_fields`, comuni ai test che
// confrontano due stati: `verify_selection` (test_verify_selection.cpp) e l'uguaglianza delle osservazioni
// (test_batch_analysis.cpp). Ogni cambio, applicato allo stato di base, tocca solo il membro nominato.

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "astro/image.hpp"

namespace dng_state_fixture {

struct field_change {
  const char* field;
  std::function<void(astro::decoder_state&)> apply;
};

// Instradamento del decoder del progetto: astro_dng, deflate (8), interi (1), 16 bit.
inline astro::dng_route_result project_route() {
  return astro::dng_route_result{astro::raw_decoder_kind::astro_dng, "", 8, 1, 16};
}

// Campi del decoder del progetto per una striscia di `rows` righe.
inline astro::dng_decoder_fields project_fields(std::uint32_t rows) {
  astro::dng_decoder_fields d;
  d.dng_version = {1, 7, 1, 0};
  d.dng_backward_version = {1, 4, 0, 0};
  d.rows_per_strip = rows;
  d.segment_count = 1;
  d.predictor = 1;
  d.zlib_runtime_version = "1.3.1";
  return d;
}

// Un cambio per membro di `project_fields(rows)`, con `route` invariato; `rows` deve essere diverso da 2.
inline std::vector<field_change> dng_field_changes(std::uint32_t width, std::uint32_t rows) {
  using astro::decoder_state;
  using astro::dng_rational;
  return {
      {"dng.dng_version", [](decoder_state& s) { s.dng->dng_version[1] = 6; }},
      {"dng.dng_backward_version", [](decoder_state& s) { s.dng->dng_backward_version[1] = 3; }},
      {"dng.tiles", [](decoder_state& s) { s.dng->tiles = true; }},
      {"dng.rows_per_strip", [](decoder_state& s) { s.dng->rows_per_strip = 2; }},
      {"dng.tile_width", [](decoder_state& s) { s.dng->tile_width = 16; }},
      {"dng.tile_length", [](decoder_state& s) { s.dng->tile_length = 16; }},
      {"dng.segment_count", [](decoder_state& s) { s.dng->segment_count = 2; }},
      {"dng.predictor", [](decoder_state& s) { s.dng->predictor = 2; }},
      {"dng.little_endian", [](decoder_state& s) { s.dng->little_endian = false; }},
      {"dng.zlib_runtime_version", [](decoder_state& s) { s.dng->zlib_runtime_version = "1.3.2"; }},
      {"dng.orientation", [](decoder_state& s) { s.dng->orientation = 1; }},
      {"dng.default_crop_origin", [](decoder_state& s) {
         s.dng->default_crop_origin = std::array<dng_rational, 2>{dng_rational{0, 1}, dng_rational{0, 1}};
       }},
      {"dng.default_crop_size", [width, rows](decoder_state& s) {
         s.dng->default_crop_size = std::array<dng_rational, 2>{dng_rational{width, 1}, dng_rational{rows, 1}};
       }},
      {"dng.metadata_rejected", [](decoder_state& s) { s.dng->metadata_rejected = {"Orientation"}; }},
      {"dng.opcode_lists", [](decoder_state& s) { s.dng->opcode_lists = {"OpcodeList1"}; }},
  };
}

}  // namespace dng_state_fixture
