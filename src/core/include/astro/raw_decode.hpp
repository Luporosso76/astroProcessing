#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <span>

#include "astro/image.hpp"

namespace astro {

// Parametri utente di LibRaw letti subito prima di `open_buffer` (ADR-08.b §2.2): il decoder li lascia ai valori
// iniziali di LibRaw 0.21.5 (-1; -1000001 per `user_cblack`) e verifica che lo siano; `rawparams_options` sono le
// opzioni effettive.
struct raw_user_params {
  int user_black = -1;
  std::array<int, 4> user_cblack{-1000001, -1000001, -1000001, -1000001};
  int user_sat = -1;
  unsigned rawparams_options = 0;
};

// Campi letti dopo `open_buffer`, su cui si decidono gli stop di ADR-08.b §2.3 prima di `unpack`.
struct raw_probe_fields {
  unsigned filters = 0;
  int colors = 0;
  int is_foveon = 0;
  unsigned raw_count = 0;
  double pixel_aspect = 1.0;
  int fuji_width = 0;  // is_fuji_rotated()
  int is_floating_point = 0;
};

// Campi letti dopo `unpack` (ADR-08.b §2.4, §2.6).
struct raw_unpack_fields {
  std::array<unsigned, 4> cblack{};
  unsigned data_maximum = 0;
  std::array<long, 4> linear_max{};
  float sensor_temperature = -1000.0f;  // -1000: dato assente
};

// Ganci di iniezione dei test: ricevono i campi letti dalla libreria e possono sostituirli prima delle decisioni.
struct raw_decode_hooks {
  std::function<void(raw_user_params&)> before_open;
  std::function<void(raw_probe_fields&)> after_open;
  std::function<void()> before_unpack;
  std::function<void(raw_unpack_fields&)> after_unpack;
};

// Decodifica un RAW gia' letto in memoria (ADR-08.b §2.2-§2.4, §2.6): area visibile di LibRaw in f32 = uint16, layout
// cfa, fase raw, nero e bianco dichiarati e non sottratti in `decoder_state`. `bytes` resta valido per tutta la chiamata.
// Ogni caso non riproducibile esattamente si ferma con il codice del contratto, prima di produrre l'immagine.
image decode_raw_buffer(std::span<const std::uint8_t> bytes, const raw_decode_hooks& hooks = {});

}  // namespace astro
