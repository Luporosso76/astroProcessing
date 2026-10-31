#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string_view>
#include <span>

#include "astro/cancel.hpp"
#include "astro/image.hpp"
#include "astro/input_format.hpp"
#include "astro/memory_pool.hpp"

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
  unsigned raw_width = 0;   // sizes.raw_width: base di max_pixels e di M (§2.1)
  unsigned raw_height = 0;  // sizes.raw_height
};

// Campi letti dopo `unpack` (ADR-08.b §2.4, §2.6).
struct raw_unpack_fields {
  std::array<unsigned, 4> cblack{};
  unsigned data_maximum = 0;
  std::array<long, 4> linear_max{};
  float sensor_temperature = -1000.0f;  // -1000: dato assente
  unsigned process_warnings = 0;  // imgdata.process_warnings (§2.5)
};

// Gestori installati in LibRaw (§2.5), esposti ai test: errore di dati (file e offset) e progresso (valore non nullo =
// cancel richiesto).
struct raw_unpack_callbacks {
  std::function<void(const char* file, long long offset)> data_error;
  std::function<int(int stage, int iteration, int expected)> progress;
};
// Piano di memoria subito prima di `unpack` (§2.1, §2.8): parte per `raw_image` (M * 2^20), byte prenotati nel pool (0
// senza pool) e `rawparams.max_raw_memory_mb` letto da LibRaw.
struct raw_unpack_plan {
  std::uint64_t raw_image_bytes = 0;
  std::uint64_t reserved_bytes = 0;
  unsigned max_raw_memory_mb = 0;
};
// Contesto del worker: pool per la prenotazione di §2.8 (nessuna prenotazione se nullo), token di cancel controllato
// nell'attesa del pool e dal gestore di progresso, limite di pixel di §2.1.
struct raw_decode_context {
  memory_pool* pool = nullptr;
  const cancel_token* cancel = nullptr;
  std::uint64_t max_pixels = raw_limits::max_pixels;
};

// Ganci di iniezione dei test: ricevono i campi letti dalla libreria e possono sostituirli prima delle decisioni.
struct raw_decode_hooks {
  std::function<void(raw_user_params&)> before_open;
  std::function<void(raw_probe_fields&)> after_open;
  std::function<void()> before_unpack;
  std::function<void(raw_unpack_fields&)> after_unpack;
  // Riceve il codice di ritorno di `open_buffer` o `unpack` e restituisce quello su cui il prodotto decide.
  std::function<int(std::string_view function, int rc)> libraw_return;
  std::function<void(const raw_unpack_callbacks&)> with_handlers;
  std::function<void(const raw_unpack_plan&)> before_unpack_plan;
};

// Decodifica un RAW gia' letto in memoria (ADR-08.b §2.2-§2.4, §2.6): area visibile di LibRaw in f32 = uint16, layout
// cfa, fase raw, nero e bianco dichiarati e non sottratti in `decoder_state`. `bytes` resta valido per tutta la chiamata.
// Ogni caso non riproducibile esattamente si ferma con il codice del contratto, prima di produrre l'immagine.
// Precondizione (§2.6): il processo ha `TZ=UTC0` dal `main`, prima dei thread; altrimenti ASTRO_ASSERT.
image decode_raw_buffer(std::span<const std::uint8_t> bytes, const raw_decode_hooks& hooks = {},
                         const raw_decode_context& ctx = {});

}  // namespace astro
