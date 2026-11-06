#include "astro/raw_decode.hpp"

#include <libraw/libraw.h>

#include <array>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "astro/hash.hpp"
#include "astro/json.hpp"
#include "astro/memory_pool.hpp"
#include "astro/third_party.hpp"
#include "dng_internal.hpp"

#ifndef ASTRO_LIBRAW_TARBALL_SHA256
#error "ASTRO_LIBRAW_TARBALL_SHA256 deve venire da cmake/third_party_libs.cmake"
#endif

namespace astro {
namespace {

using context = stop_error::context_list;

[[noreturn]] void unsupported(const char* code, const char* message, context ctx = {}) {
  throw stop_error(error_category::unsupported, code, message, std::move(ctx));
}

// Contesto comune degli stop da codice di ritorno (ADR-08.b §2.5): funzione, valore e testo di LibRaw.
context rc_context(const char* function, int rc) {
  return {{"funzione", function}, {"valore", std::to_string(rc)}, {"libraw", libraw_strerror(rc)}};
}

// Codice di ritorno non nullo di open_buffer, get_decoder_info o unpack (ADR-08.b §2.5).
[[noreturn]] void libraw_stop(const char* function, int rc) {
  error_category category = error_category::invalid_input;
  const char* code = "raw.libraw_error";
  switch (rc) {
    case LIBRAW_FILE_UNSUPPORTED:
      category = error_category::unsupported;
      code = "raw.file_unsupported";
      break;
    case LIBRAW_DATA_ERROR:
    case LIBRAW_IO_ERROR:
    case LIBRAW_BAD_CROP:
      category = error_category::integrity;
      code = "raw.data_error";
      break;
    case LIBRAW_TOO_BIG:
    case LIBRAW_UNSUFFICIENT_MEMORY:
    case LIBRAW_MEMPOOL_OVERFLOW:
      category = error_category::limit_exceeded;
      code = "raw.too_big";
      break;
    case LIBRAW_CANCELLED_BY_CALLBACK:
      category = error_category::cancelled;
      code = "raw.cancelled";
      break;
    default:
      break;
  }
  throw stop_error(category, code, "LibRaw ha restituito un errore", rc_context(function, rc));
}

// Stato dei gestori installati in LibRaw (§2.5). LibRaw li chiama dal thread della decodifica.
struct unpack_watch {
  const cancel_token* cancel = nullptr;
  bool data_error = false;
  std::optional<std::string> file;  // assente se LibRaw passa un nome nullo (sorgente in memoria)
  INT64 offset = 0;
};

// Si registra la prima segnalazione; lo stop avviene dopo il ritorno di unpack, qualunque sia il suo codice.
void on_data_error(void* data, const char* file, const INT64 offset) {
  auto& w = *static_cast<unpack_watch*>(data);
  if (w.data_error) return;
  w.data_error = true;
  if (file != nullptr) w.file = file;
  w.offset = offset;
}

// Valore non nullo: LibRaw interrompe con LIBRAW_CANCELLED_BY_CALLBACK.
int on_progress(void* data, LibRaw_progress, int, int) {
  const auto& w = *static_cast<const unpack_watch*>(data);
  return w.cancel != nullptr && w.cancel->requested() ? 1 : 0;
}

// Avvisi di §2.5 ammessi: non toccano i campioni grezzi.
constexpr unsigned allowed_warnings =
    LIBRAW_WARN_BAD_CAMERA_WB | LIBRAW_WARN_NO_METADATA | LIBRAW_WARN_NO_EMBEDDED_PROFILE | LIBRAW_WARN_NO_INPUT_PROFILE |
    LIBRAW_WARN_BAD_OUTPUT_PROFILE | LIBRAW_WARN_NO_BADPIXELMAP | LIBRAW_WARN_BAD_DARKFRAME_FILE |
    LIBRAW_WARN_BAD_DARKFRAME_DIM | LIBRAW_WARN_PARSEFUJI_PROCESSED;

// Nomi di libraw_const.h per i bit non ammessi; NO_JASPER (1 << 11) è dichiarato solo sotto una macro di LibRaw.
constexpr std::pair<unsigned, const char*> warning_names[] = {
    {LIBRAW_WARN_NO_JPEGLIB, "NO_JPEGLIB"},
    {1u << 11, "NO_JASPER"},
    {LIBRAW_WARN_RAWSPEED_PROBLEM, "RAWSPEED_PROBLEM"},
    {LIBRAW_WARN_RAWSPEED_UNSUPPORTED, "RAWSPEED_UNSUPPORTED"},
    {LIBRAW_WARN_RAWSPEED_PROCESSED, "RAWSPEED_PROCESSED"},
    {LIBRAW_WARN_FALLBACK_TO_AHD, "FALLBACK_TO_AHD"},
    {LIBRAW_WARN_DNGSDK_PROCESSED, "DNGSDK_PROCESSED"},
    {LIBRAW_WARN_DNG_IMAGES_REORDERED, "DNG_IMAGES_REORDERED"},
    {LIBRAW_WARN_DNG_STAGE2_APPLIED, "DNG_STAGE2_APPLIED"},
    {LIBRAW_WARN_DNG_STAGE3_APPLIED, "DNG_STAGE3_APPLIED"},
    {LIBRAW_WARN_RAWSPEED3_PROBLEM, "RAWSPEED3_PROBLEM"},
    {LIBRAW_WARN_RAWSPEED3_UNSUPPORTED, "RAWSPEED3_UNSUPPORTED"},
    {LIBRAW_WARN_RAWSPEED3_PROCESSED, "RAWSPEED3_PROCESSED"},
    {LIBRAW_WARN_RAWSPEED3_NOTLISTED, "RAWSPEED3_NOTLISTED"},
};

// Ogni bit fuori da `allowed_warnings`, noto o sconosciuto, ferma con il suo nome nel contesto.
void check_warnings(unsigned warnings) {
  const unsigned rejected = warnings & ~allowed_warnings;
  if (rejected == 0) return;
  std::string names;
  for (unsigned bit = 0; bit < 32; ++bit) {
    const unsigned mask = 1u << bit;
    if ((rejected & mask) == 0) continue;
    std::string name = "bit_" + std::to_string(bit);
    for (const auto& [value, known] : warning_names)
      if (value == mask) name = known;
    names += names.empty() ? name : " " + name;
  }
  unsupported("raw.warning", "avviso di LibRaw non ammesso", {{"avvisi", names}, {"bit", std::to_string(rejected)}});
}

// §2.6: fuso del processo verificato nel worker prima di open_buffer. Un esito diverso è un difetto dell'eseguibile.
void check_process_tz() {
  const char* tz = std::getenv("TZ");
  ASTRO_ASSERT(tz != nullptr && std::string_view(tz) == "UTC0");
  // timegm di 2026-01-15 12:00:00 e di 2026-07-15 12:00:00: mktime deve coincidere su entrambi.
  for (const auto& [month, expected] : {std::pair<int, std::time_t>{0, 1768478400}, {6, 1784116800}}) {
    std::tm t{};
    t.tm_year = 126;
    t.tm_mon = month;
    t.tm_mday = 15;
    t.tm_hour = 12;
    t.tm_isdst = -1;
    ASTRO_ASSERT(std::mktime(&t) == expected);
  }
}

// §2.1: M = max(1, ceil(2 * raw_width * (raw_height + 8) / 2^20)) MiB, misurato come minimo accettato da unpack.
std::uint64_t raw_memory_mib(std::uint64_t raw_width, std::uint64_t raw_height) {
  constexpr std::uint64_t mib = std::uint64_t{1} << 20;
  const std::uint64_t m = (2 * raw_width * (raw_height + 8) + mib - 1) / mib;
  return m < 1 ? 1 : m;
}

// Convenzione di LibRaw::FC (libraw.h): indice di colore del sito (r, c) codificato in `filters`.
unsigned fc(unsigned filters, unsigned r, unsigned c) {
  return (filters >> ((((r << 1) & 14) | (c & 1)) << 1)) & 3;
}

// Indice di LibRaw → canale di ADR-03 (0 R, 1 G, 2 B): gli indici 1 e 3 sono i due verdi.
int channel_of(unsigned index) { return index == 3 ? 1 : static_cast<int>(index); }

// Cella 2x2 all'origine dell'area visibile come pattern Bayer di ADR-03; assente se `filters` non ripete la stessa cella
// sulle otto righe codificate o se la cella non e' una delle quattro celle Bayer (un rosso, un blu, verdi in diagonale).
std::optional<cfa_pattern> bayer_pattern(unsigned filters) {
  for (unsigned r = 0; r < 8; ++r)
    for (unsigned c = 0; c < 2; ++c)
      if (fc(filters, r, c) != fc(filters, r & 1, c)) return std::nullopt;
  static constexpr std::pair<cfa_pattern, std::array<int, 4>> cells[] = {{cfa_pattern::rggb, {0, 1, 1, 2}},
                                                                         {cfa_pattern::bggr, {2, 1, 1, 0}},
                                                                         {cfa_pattern::grbg, {1, 0, 2, 1}},
                                                                         {cfa_pattern::gbrg, {1, 2, 0, 1}}};
  for (const auto& [pattern, cell] : cells) {
    bool match = true;
    for (unsigned site = 0; site < 4; ++site)
      if (channel_of(fc(filters, site / 2, site % 2)) != cell[site]) match = false;
    if (match) return pattern;
  }
  return std::nullopt;
}

// Stop della tabella di ADR-08.b §2.3 decisi dopo open_buffer, prima di unpack. Restituisce il pattern della cella.
std::optional<cfa_pattern> check_probe(const raw_probe_fields& f) {
  if (f.filters == 9) unsupported("raw.xtrans", "sensore X-Trans non supportato");
  if (f.raw_count > 1)
    unsupported("raw.multi_image", "file con piu' immagini raw", {{"raw_count", std::to_string(f.raw_count)}});
  if (f.is_floating_point != 0) unsupported("raw.float_data", "dati raw in virgola mobile");
  if (f.is_foveon != 0 || f.colors != 3)
    unsupported("raw.cfa_pattern", "sensore non Bayer a tre colori", {{"colors", std::to_string(f.colors)}});
  if (f.pixel_aspect != 1.0 || f.fuji_width != 0) unsupported("raw.geometry", "pixel non quadrati o sensore ruotato");
  // filters = 0: nessuna CFA codificata; la decisione dipende da raw_image dopo unpack.
  if (f.filters == 0) return std::nullopt;
  const std::optional<cfa_pattern> pattern = bayer_pattern(f.filters);
  if (!pattern) unsupported("raw.cfa_pattern", "cella CFA non Bayer 2x2", {{"filters", std::to_string(f.filters)}});
  return pattern;
}

// Motivo di nero dopo unpack: `cblack[4] x cblack[5]` valori da `cblack[6]`; dimensioni nulle = nessun motivo.
// LibRaw arrotonda al pari il margine di un'ActiveArea DNG con origine dispari e riallinea `filters`, non il motivo, che
// resta ancorato all'origine del file (ADR-08.b §2.4): con margine > 0 su un asse il motivo deve essere invariante per un
// passo su quell'asse, altrimenti il nero per posizione non e' determinabile.
std::vector<std::uint32_t> black_pattern(const libraw_data_t& d, std::uint32_t& rows, std::uint32_t& cols) {
  const unsigned* cb = d.color.cblack;
  if (std::uint64_t{cb[4]} * cb[5] == 0) {
    rows = cols = 0;
    return {};
  }
  rows = cb[4];
  cols = cb[5];
  if (std::uint64_t{rows} * cols > LIBRAW_CBLACK_SIZE - 6)
    throw stop_error(error_category::integrity, "raw.data_error", "motivo di nero oltre la capacita' di LibRaw",
                     {{"righe", std::to_string(rows)}, {"colonne", std::to_string(cols)}});
  const auto at = [&](std::uint32_t i, std::uint32_t j) { return cb[6 + i * cols + j]; };
  if (d.idata.dng_version != 0) {
    bool x_invariant = true, y_invariant = true;
    for (std::uint32_t i = 0; i < rows; ++i)
      for (std::uint32_t j = 0; j < cols; ++j) {
        if (at(i, j) != at(i, (j + 1) % cols)) x_invariant = false;
        if (at(i, j) != at((i + 1) % rows, j)) y_invariant = false;
      }
    if ((d.sizes.left_margin > 0 && !x_invariant) || (d.sizes.top_margin > 0 && !y_invariant))
      unsupported("raw.black_pattern_anchor", "motivo di nero non ancorabile all'area visibile",
                  {{"left_margin", std::to_string(d.sizes.left_margin)},
                   {"top_margin", std::to_string(d.sizes.top_margin)}});
  }
  std::vector<std::uint32_t> values;
  values.reserve(std::size_t{rows} * cols);
  for (std::uint32_t i = 0; i < rows; ++i)
    for (std::uint32_t j = 0; j < cols; ++j) values.push_back(at(i, j));
  return values;
}

metadata_source from_libraw(const char* field) { return {metadata_origin::decoder, field}; }

void set_metadata(frame_metadata& m, const libraw_data_t& d, const raw_unpack_fields& u) {
  const std::string make = d.idata.make, model = d.idata.model;
  if (!make.empty() || !model.empty())
    m.camera = metadata_value<std::string>{make.empty() ? model : model.empty() ? make : make + " " + model,
                                           from_libraw("idata.make idata.model")};
  m.saturation_adu = metadata_value<double>{static_cast<double>(d.color.maximum), from_libraw("color.maximum")};
  // Un campo che LibRaw non valorizza (0, o -1000 per la temperatura) resta assente: nessun valore di comodo.
  if (d.other.shutter > 0)
    m.exposure_s = metadata_value<double>{static_cast<double>(d.other.shutter), from_libraw("other.shutter")};
  if (d.other.iso_speed > 0)
    m.gain = metadata_value<double>{static_cast<double>(d.other.iso_speed), from_libraw("other.iso_speed")};
  // §2.6: sotto TZ=UTC0 (verificata prima di open_buffer) il mktime di LibRaw è timegm dei campi civili del file; -1 o
  // 0 (errore o ora assente) lasciano il campo assente.
  if (d.other.timestamp > 0)
    m.capture_time_libraw =
        metadata_value<std::int64_t>{static_cast<std::int64_t>(d.other.timestamp), from_libraw("other.timestamp")};
  if (u.sensor_temperature != -1000.0f)
    m.temperature_c = metadata_value<double>{static_cast<double>(u.sensor_temperature),
                                             from_libraw("makernotes.common.SensorTemperature")};
}

}  // namespace

std::optional<std::array<std::uint8_t, 32>> dng_detail::curve_digest(std::span<const std::uint16_t, 0x10000> curve) {
  bool identity = true;
  for (std::uint32_t i = 0; i < 0x10000; ++i)
    if (curve[i] != i) identity = false;
  if (identity) return std::nullopt;
  std::vector<std::uint8_t> bytes(2 * 0x10000);
  for (std::uint32_t i = 0; i < 0x10000; ++i) {
    bytes[2 * i] = static_cast<std::uint8_t>(curve[i] & 0xFF);
    bytes[2 * i + 1] = static_cast<std::uint8_t>(curve[i] >> 8);
  }
  const std::string hex = sha256_hex(bytes);
  const auto nibble = [](char h) { return static_cast<std::uint8_t>(h <= '9' ? h - '0' : h - 'a' + 10); };
  std::array<std::uint8_t, 32> out{};
  for (std::size_t i = 0; i < out.size(); ++i)
    out[i] = static_cast<std::uint8_t>(nibble(hex[2 * i]) << 4 | nibble(hex[2 * i + 1]));
  return out;
}

image decode_raw_buffer(std::span<const std::uint8_t> bytes, const raw_decode_hooks& hooks,
                        const raw_decode_context& ctx) {
  // §2.1: il limite si decide dalla sola dimensione, senza leggere un byte.
  if (bytes.size() > raw_limits::max_file_bytes)
    throw stop_error(error_category::limit_exceeded, "raw.max_file_bytes", "file RAW oltre il limite",
                     {{"byte", std::to_string(bytes.size())}, {"limite", std::to_string(raw_limits::max_file_bytes)}});
  // U12 obbligo 5: versione e capacita' di LibRaw verificate prima della prima chiamata alla libreria.
  third_party::verify_once();
  check_process_tz();
  // U13 §3: un solo punto di scelta, prima di ogni chiamata a LibRaw o a zlib; un errore di un decoder non porta mai
  // all'altro.
  const dng_detail::route_detail route = dng_detail::route(bytes);
  if (hooks.after_route) hooks.after_route(route.result);
  if (route.result.decoder == raw_decoder_kind::astro_dng) return dng_detail::decode(bytes, route, hooks, ctx);
  const auto returned = [&hooks](const char* function, int rc) {
    return hooks.libraw_return ? hooks.libraw_return(function, rc) : rc;
  };

  // Dichiarati prima di LibRaw: la prenotazione si rilascia e i gestori smettono di esistere solo dopo che l'oggetto
  // LibRaw ha liberato raw_image.
  reservation held;
  unpack_watch watch{ctx.cancel, false, std::nullopt, 0};
  // Un oggetto per decodifica, sullo heap, mai condiviso (ADR-08.b §2.1).
  auto lr = std::make_unique<LibRaw>();
  libraw_data_t& d = lr->imgdata;
  lr->set_dataerror_handler(on_data_error, &watch);
  lr->set_progress_handler(on_progress, &watch);

  // §2.2: il bit e' attivo per default e con esso LibRaw convertirebbe in silenzio i DNG in virgola mobile in interi.
  d.rawparams.options &= ~static_cast<unsigned>(LIBRAW_RAWOPTIONS_CONVERTFLOAT_TO_INT);
  raw_user_params user{d.params.user_black,
                       {d.params.user_cblack[0], d.params.user_cblack[1], d.params.user_cblack[2],
                        d.params.user_cblack[3]},
                       d.params.user_sat,
                       d.rawparams.options};
  if (hooks.before_open) hooks.before_open(user);
  d.params.user_black = user.user_black;
  for (int i = 0; i < 4; ++i) d.params.user_cblack[i] = user.user_cblack[static_cast<std::size_t>(i)];
  d.params.user_sat = user.user_sat;
  // §2.2: nero e saturazione dell'utente restano quelli iniziali, cosi' LibRaw non sostituisce i livelli del file.
  ASTRO_ASSERT(d.params.user_black == -1 && d.params.user_sat == -1);
  for (int v : d.params.user_cblack) ASTRO_ASSERT(v == -1000001);

  if (const int rc = returned("open_buffer", lr->open_buffer(bytes.data(), bytes.size())); rc != LIBRAW_SUCCESS)
    libraw_stop("open_buffer", rc);

  // §2.3: per i formati che LibRaw dichiara non supportati (DNG JPEG XL, VC-5) open_buffer riesce e unpack no.
  libraw_decoder_info_t decoder{};
  if (const int rc = lr->get_decoder_info(&decoder); rc != LIBRAW_SUCCESS) libraw_stop("get_decoder_info", rc);
  if ((decoder.decoder_flags & LIBRAW_DECODER_UNSUPPORTED_FORMAT) != 0)
    unsupported("raw.file_unsupported", "formato non supportato dichiarato da LibRaw",
                {{"funzione", "get_decoder_info"},
                 {"decoder_name", decoder.decoder_name != nullptr ? decoder.decoder_name : "null"},
                 {"decoder_flags", std::to_string(decoder.decoder_flags)}});

  raw_probe_fields probe{d.idata.filters,
                         d.idata.colors,
                         static_cast<int>(d.idata.is_foveon),
                         d.idata.raw_count,
                         d.sizes.pixel_aspect,
                         lr->is_fuji_rotated(),
                         lr->is_floating_point(),
                         d.sizes.raw_width,
                         d.sizes.raw_height,
                         d.other.focal_len,
                         d.lens.makernotes.FocalLengthIn35mmFormat,
                         d.other.parsed_gps.gpsparsed != 0};
  if (hooks.after_open) hooks.after_open(probe);
  const std::optional<cfa_pattern> pattern = check_probe(probe);
  // §2.1: prodotto a 64 bit, limite incluso.
  if (const std::uint64_t pixels = std::uint64_t{probe.raw_width} * probe.raw_height; pixels > ctx.max_pixels)
    throw stop_error(error_category::limit_exceeded, "raw.max_pixels", "dimensioni dichiarate oltre il limite",
                     {{"pixel", std::to_string(pixels)}, {"limite", std::to_string(ctx.max_pixels)}});

  // §2.1, §2.8: il limite di LibRaw coincide con la parte della prenotazione per raw_image; il resto copre i campioni f32
  // dell'area visibile e la maschera di invalidità (uint16 per pixel). I byte del file li ha prenotati la lettura.
  const std::uint64_t m = raw_memory_mib(probe.raw_width, probe.raw_height);
  d.rawparams.max_raw_memory_mb = static_cast<unsigned>(m);
  raw_unpack_plan plan{m << 20, 0, 0};
  const std::uint64_t needed = plan.raw_image_bytes + 6 * std::uint64_t{d.sizes.width} * d.sizes.height;
  if (ctx.pool != nullptr) {
    // I byte che il chiamante tiene già nel pool (`held_bytes`, altrimenti il file): se insieme alla prenotazione
    // superano il budget, l'attesa nel pool non finirebbe mai.
    const std::uint64_t held_by_caller = ctx.held_bytes.value_or(bytes.size());
    if (held_by_caller + needed > ctx.pool->budget_bytes())
      throw stop_error(error_category::budget, "sched.job_exceeds_budget", "budget sotto la prenotazione del decoder",
                       {{"minimo", std::to_string(held_by_caller + needed)},
                        {"file", std::to_string(bytes.size())},
                        {"budget", std::to_string(ctx.pool->budget_bytes())}});
    held = ctx.pool->reserve(needed, "raw_decode", ctx.cancel);
    plan.reserved_bytes = held.bytes();
  }
  plan.max_raw_memory_mb = d.rawparams.max_raw_memory_mb;
  if (hooks.before_unpack_plan) hooks.before_unpack_plan(plan);
  if (hooks.with_handlers)
    hooks.with_handlers(raw_unpack_callbacks{
        [&watch](const char* file, INT64 offset) { on_data_error(&watch, file, offset); },
        [&watch](int stage, int iteration, int expected) {
          return on_progress(&watch, static_cast<LibRaw_progress>(stage), iteration, expected);
        }});

  if (hooks.before_unpack) hooks.before_unpack();
  const int unpack_rc = returned("unpack", lr->unpack());
  // §2.5: un errore di dati segnalato ferma la decodifica anche con unpack riuscito.
  if (watch.data_error) {
    context c = rc_context("unpack", unpack_rc);
    if (watch.file) c.emplace_back("file", *watch.file);
    c.emplace_back("offset", std::to_string(watch.offset));
    throw stop_error(error_category::integrity, "raw.data_error", "LibRaw ha segnalato un errore di dati", std::move(c));
  }
  if (unpack_rc != LIBRAW_SUCCESS) libraw_stop("unpack", unpack_rc);

  // §2.3: stop valutati prima di ogni lettura di raw_image.
  if (d.rawdata.float_image != nullptr) unsupported("raw.float_data", "dati raw in virgola mobile");
  if (d.rawdata.raw_image == nullptr) unsupported("raw.not_cfa", "dati raw a piu' canali per pixel");
  if (!pattern) unsupported("raw.cfa_pattern", "nessuna CFA codificata (filters = 0)");

  raw_unpack_fields levels{{d.color.cblack[0], d.color.cblack[1], d.color.cblack[2], d.color.cblack[3]},
                           d.color.data_maximum,
                           {d.color.linear_max[0], d.color.linear_max[1], d.color.linear_max[2], d.color.linear_max[3]},
                           d.makernotes.common.SensorTemperature,
                           d.process_warnings};
  if (hooks.after_unpack) hooks.after_unpack(levels);
  check_warnings(levels.process_warnings);

  decoder_state st;
  st.source = from_libraw("");
  st.black_pattern = black_pattern(d, st.black_pattern_rows, st.black_pattern_cols);

  // §2.3: area visibile di LibRaw, origine (left_margin, top_margin); righe e colonne mascherate non si importano.
  const libraw_image_sizes_t& sz = d.sizes;
  const std::uint64_t pitch = sz.raw_pitch / 2;
  if (std::uint64_t{sz.left_margin} + sz.width > sz.raw_width ||
      std::uint64_t{sz.top_margin} + sz.height > sz.raw_height || sz.raw_width > pitch)
    throw stop_error(error_category::integrity, "raw.data_error", "area visibile fuori dal buffer grezzo",
                     {{"raw", std::to_string(sz.raw_width) + "x" + std::to_string(sz.raw_height)},
                      {"area", std::to_string(sz.left_margin) + "," + std::to_string(sz.top_margin) + " " +
                                   std::to_string(sz.width) + "x" + std::to_string(sz.height)}});

  st.raw_width = sz.raw_width;
  st.raw_height = sz.raw_height;
  st.window = {sz.left_margin, sz.top_margin, sz.width, sz.height};
  for (unsigned site = 0; site < 4; ++site)
    st.cfa_color_index[site] = static_cast<std::uint8_t>(fc(probe.filters, site / 2, site % 2));
  // §2.4: nero e bianco dichiarati, mai sottratti ne' applicati.
  st.black = d.color.black;
  for (std::size_t i = 0; i < 4; ++i) st.black_per_color[i] = levels.cblack[i];
  st.white = d.color.maximum;
  if (levels.data_maximum != 0) st.data_maximum = levels.data_maximum;
  if (levels.linear_max != std::array<long, 4>{})
    st.linear_max = std::array<std::int64_t, 4>{levels.linear_max[0], levels.linear_max[1], levels.linear_max[2],
                                                levels.linear_max[3]};
  st.curve_sha256 = dng_detail::curve_digest(d.color.curve);
  st.linearization = st.curve_sha256 ? decoder_linearization::decoder_curve : decoder_linearization::identity;
  st.process_warnings = levels.process_warnings;
  for (std::size_t i = 0; i < 2; ++i) {
    const libraw_raw_inset_crop_t& c = sz.raw_inset_crops[i];
    st.raw_inset_crops[i] = {c.cleft, c.ctop, c.cwidth, c.cheight};
  }
  st.flip = sz.flip;
  st.route = route.result;

  image img = make_image(image_geometry(sz.width, sz.height), sample_type::f32, plane_layout::cfa,
                         cfa_layout{*pattern, 0, 0});
  // §2.6: campioni f32 uguali al uint16 letto (esatto: ogni intero <= 65535 e' rappresentabile in f32).
  float* out = img.data<float>();
  const unsigned short* raw = d.rawdata.raw_image;
  for (std::uint64_t r = 0; r < sz.height; ++r)
    for (std::uint64_t c = 0; c < sz.width; ++c)
      out[r * sz.width + c] = static_cast<float>(raw[(sz.top_margin + r) * pitch + sz.left_margin + c]);

  img.unit = pixel_unit::adu;
  img.lin = linearity::linear;
  img.lin_origin.value = metadata_origin::decoder;
  img.phase = metadata_value<process_phase>{process_phase::raw, from_libraw("")};
  img.decoder_state = std::move(st);
  set_metadata(img.metadata, d, levels);
  return img;
}

}  // namespace astro

namespace astro {
namespace {

json integers(const std::vector<std::uint32_t>& values) {
  json a = json::array();
  for (const std::uint32_t v : values) a.push(json::integer(static_cast<std::int64_t>(v)));
  return a;
}

json crop_json(std::uint32_t x0, std::uint32_t y0, std::uint32_t w, std::uint32_t h) {
  json c = json::object();
  c.set("x0", json::integer(x0));
  c.set("y0", json::integer(y0));
  c.set("w", json::integer(w));
  c.set("h", json::integer(h));
  return c;
}

const char* pattern_name(cfa_pattern p) {
  switch (p) {
    case cfa_pattern::rggb: return "rggb";
    case cfa_pattern::bggr: return "bggr";
    case cfa_pattern::grbg: return "grbg";
    case cfa_pattern::gbrg: return "gbrg";
  }
  ASTRO_ASSERT(false);
  return "";
}

std::string hex_of(const std::array<std::uint8_t, 32>& digest) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string out;
  for (const std::uint8_t b : digest) {
    out += digits[b >> 4];
    out += digits[b & 15];
  }
  return out;
}

// Nomi dei campi interpretati e presenti, nell'ordine di frame_metadata.
json metadata_present_names(const frame_metadata& m) {
  json names = json::array();
  const auto add = [&names](const char* name, bool present) {
    if (present) names.push(json::string(name));
  };
  add("exposure_s", m.exposure_s.has_value());
  add("gain", m.gain.has_value());
  add("conversion_gain_e_per_adu", m.conversion_gain_e_per_adu.has_value());
  add("offset_adu", m.offset_adu.has_value());
  add("temperature_c", m.temperature_c.has_value());
  add("filter", m.filter.has_value());
  add("camera", m.camera.has_value());
  add("imagetyp", m.imagetyp.has_value());
  add("saturation_adu", m.saturation_adu.has_value());
  add("pedestal_adu", m.pedestal_adu.has_value());
  add("roi_x", m.roi_x.has_value());
  add("roi_y", m.roi_y.has_value());
  add("readout_mode", m.readout_mode.has_value());
  add("row_order", m.row_order.has_value());
  add("capture_time_libraw", m.capture_time_libraw.has_value());
  return names;
}

const char* to_string(raw_decoder_kind kind) {
  switch (kind) {
    case raw_decoder_kind::libraw: return "libraw";
    case raw_decoder_kind::astro_dng: return "astro_dng";
  }
  std::abort();
}

json optional_integer(const std::optional<std::uint32_t>& v) { return v ? json::integer(*v) : json::null(); }

std::string dotted(const std::array<std::uint8_t, 4>& v) {
  return std::to_string(v[0]) + "." + std::to_string(v[1]) + "." + std::to_string(v[2]) + "." + std::to_string(v[3]);
}

json strings(const std::vector<std::string>& values) {
  json a = json::array();
  for (const std::string& v : values) a.push(json::string(v));
  return a;
}

// Valori interi come interi; un RATIONAL non intero come testo "num/den".
json rational_pair(const std::optional<std::array<dng_rational, 2>>& v) {
  if (!v) return json::null();
  json a = json::array();
  for (const dng_rational& x : *v) {
    ASTRO_ASSERT(x.den != 0);
    a.push(x.num % x.den == 0 ? json::integer(x.num / x.den)
                              : json::string(std::to_string(x.num) + "/" + std::to_string(x.den)));
  }
  return a;
}

// Campi comuni ai due decoder (ADR-08.b §2.9, U13 §8): instradamento, livelli, geometria, metadati.
void set_common(json& r, const image& img) {
  const decoder_state& s = *img.decoder_state;
  r.set("decoder", json::string(to_string(s.route.decoder)));
  // Identità del pacchetto di decoder (ADR-08.b §2.9, riga «Libreria»): vale per entrambi i decoder.
  const decoder_identity identity = current_decoder_identity();
  r.set("libraw_version", json::string(identity.libraw_version));
  r.set("libraw_version_number", json::integer(identity.libraw_version_number));
  r.set("libraw_capabilities", json::integer(identity.libraw_capabilities));
  r.set("libraw_tarball_sha256", json::string(identity.libraw_tarball_sha256));
  r.set("compression", optional_integer(s.route.compression));
  r.set("sample_format", optional_integer(s.route.sample_format));
  r.set("bits_per_sample", optional_integer(s.route.bits_per_sample));
  json black = json::object();
  black.set("scalar", json::integer(s.black));
  black.set("per_color", integers({s.black_per_color.begin(), s.black_per_color.end()}));
  black.set("pattern_rows", json::integer(s.black_pattern_rows));
  black.set("pattern_cols", json::integer(s.black_pattern_cols));
  black.set("pattern", integers(s.black_pattern));
  r.set("black", std::move(black));
  r.set("white", json::integer(s.white));
  r.set("data_maximum", s.data_maximum ? json::integer(*s.data_maximum) : json::null());
  if (s.linear_max) {
    json lm = json::array();
    for (const std::int64_t v : *s.linear_max) lm.push(json::integer(v));
    r.set("linear_max", std::move(lm));
  } else {
    r.set("linear_max", json::null());
  }
  r.set("linearization", json::string(s.linearization == decoder_linearization::decoder_curve ? "decoder_curve" : "identity"));
  r.set("linearization_curve_sha256", s.curve_sha256 ? json::string(hex_of(*s.curve_sha256)) : json::null());
  r.set("crop", crop_json(s.window.x0, s.window.y0, s.window.width, s.window.height));
  r.set("margin_parity", integers({s.window.x0 % 2, s.window.y0 % 2}));
  r.set("cfa_pattern", json::string(pattern_name(img.cfa->pattern)));
  r.set("raw_width", json::integer(s.raw_width));
  r.set("raw_height", json::integer(s.raw_height));
  json color_index = json::array();
  for (const std::uint8_t c : s.cfa_color_index) color_index.push(json::integer(c));
  r.set("cfa_color_index", std::move(color_index));
  r.set("metadata_present", metadata_present_names(img.metadata));
  for (const char* absent : {"sensor", "pixel_size_um", "pixel_scale_arcsec", "wcs"}) r.set(absent, json::null());
  r.set("absent_reason", json::string("not_provided_by_decoder"));
  r.set("capture_time_libraw", img.metadata.capture_time_libraw ? json::integer(img.metadata.capture_time_libraw->value)
                                                                  : json::null());
  r.set("capture_time_clock", json::string("camera_no_zone"));
  // ADR-08.b §2.9: nessun nero né scala applicati, dato raw, categorie non conservate; fuso verificato prima
  // dell'instradamento per entrambi i decoder (U13 §3).
  r.set("black_applied_by_decoder", json::string("none"));
  r.set("scale_applied_by_decoder", json::string("none"));
  json not_kept = json::array();
  for (const char* c : {"makernote", "thumbnail", "profiles", "camera_white_balance"}) not_kept.push(json::string(c));
  r.set("metadata_not_kept", std::move(not_kept));
  r.set("data_state", json::string("raw"));
  r.set("camera_processing", json::string("unknown"));
  r.set("process_tz", json::string("UTC0"));
}

void set_libraw(json& r, const image& img) {
  const decoder_state& s = *img.decoder_state;
  r.set("route_reason", json::string(s.route.reason));
  json insets = json::array();
  for (const auto& c : s.raw_inset_crops) insets.push(crop_json(c[0], c[1], c[2], c[3]));
  r.set("raw_inset_crops", std::move(insets));
  r.set("flip", json::integer(s.flip));
  r.set("process_warnings", json::integer(s.process_warnings));
}

// U13 §8: campi propri del decoder del progetto.
void set_astro_dng(json& r, const dng_decoder_fields& f) {
  r.set("dng_spec", json::string("DNG 1.7.1.0"));
  r.set("dng_version", json::string(dotted(f.dng_version)));
  r.set("dng_backward_version", json::string(dotted(f.dng_backward_version)));
  json seg = json::object();
  seg.set("kind", json::string(f.tiles ? "tiles" : "strips"));
  if (f.tiles) {
    seg.set("tile_width", json::integer(f.tile_width));
    seg.set("tile_length", json::integer(f.tile_length));
  } else {
    seg.set("rows_per_strip", json::integer(f.rows_per_strip));
  }
  seg.set("count", json::integer(static_cast<std::int64_t>(f.segment_count)));
  r.set("segments", std::move(seg));
  r.set("predictor", json::integer(f.predictor));
  r.set("byte_order", json::string(f.little_endian ? "II" : "MM"));
  r.set("zlib_version", json::string(f.zlib_runtime_version));
  r.set("orientation", optional_integer(f.orientation));
  r.set("default_crop_origin", rational_pair(f.default_crop_origin));
  r.set("default_crop_size", rational_pair(f.default_crop_size));
  r.set("metadata_rejected", strings(f.metadata_rejected));
  r.set("opcode_lists", strings(f.opcode_lists));
}

}  // namespace

decoder_identity current_decoder_identity() {
  return decoder_identity{LibRaw::version(), static_cast<std::uint32_t>(LibRaw::versionNumber()),
                          static_cast<std::uint32_t>(libraw_capabilities()), ASTRO_LIBRAW_TARBALL_SHA256};
}

json raw_decoder_record(const image& img) {
  if (!img.decoder_state || !img.cfa)
    throw stop_error(error_category::invalid_input, "raw.record_source", "immagine non prodotta dal decoder RAW");
  const decoder_state& s = *img.decoder_state;
  json r = json::object();
  set_common(r, img);
  switch (s.route.decoder) {
    case raw_decoder_kind::libraw: set_libraw(r, img); break;
    case raw_decoder_kind::astro_dng:
      if (!s.dng)
        throw stop_error(error_category::invalid_input, "raw.record_source",
                         "immagine del decoder del progetto senza i suoi campi");
      set_astro_dng(r, *s.dng);
      break;
  }
  return r;
}

namespace {

[[noreturn]] void record_malformed(const char* key, const char* why) {
  throw stop_error(error_category::integrity, "raw.record_malformed", "record del decoder non leggibile",
                   {{"chiave", key}, {"motivo", why}});
}

// `name` e' la chiave riportata nella diagnosi: per un membro annidato e' il nome qualificato `<padre>.<chiave>`.
const json& record_nested(const json& record, const char* key, const char* name) {
  const json* v = record.type() == json::kind::object ? record.find(key) : nullptr;
  if (v == nullptr) record_malformed(name, "assente");
  return *v;
}

const json& record_member(const json& record, const char* key) { return record_nested(record, key, key); }

std::int64_t record_integer(const json& v, const char* key, std::int64_t lo, std::int64_t hi) {
  if (v.type() != json::kind::integer || v.as_integer() < lo || v.as_integer() > hi) record_malformed(key, "intero fuori dominio");
  return v.as_integer();
}

std::uint32_t record_u32(const json& record, const char* key) {
  return static_cast<std::uint32_t>(record_integer(record_member(record, key), key, 0, 4294967295LL));
}

std::vector<std::int64_t> record_integers(const json& v, const char* key, std::size_t count, std::int64_t lo, std::int64_t hi) {
  if (v.type() != json::kind::array || v.items().size() != count) record_malformed(key, "array di lunghezza errata");
  std::vector<std::int64_t> out;
  for (const json& item : v.items()) out.push_back(record_integer(item, key, lo, hi));
  return out;
}

std::array<std::uint32_t, 4> record_crop(const json& v, const char* key) {
  const char* names[4] = {"x0", "y0", "w", "h"};
  std::array<std::uint32_t, 4> out{};
  for (std::size_t i = 0; i < 4; ++i) out[i] = static_cast<std::uint32_t>(record_integer(record_member(v, names[i]), key, 0, 4294967295LL));
  return out;
}

std::array<std::uint8_t, 32> record_digest(const json& v, const char* key) {
  if (v.type() != json::kind::string || v.as_string().size() != 64) record_malformed(key, "digest non di 64 cifre esadecimali");
  std::array<std::uint8_t, 32> out{};
  const auto nibble = [key](char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    record_malformed(key, "cifra non esadecimale minuscola");
  };
  for (std::size_t i = 0; i < 32; ++i) out[i] = static_cast<std::uint8_t>(nibble(v.as_string()[2 * i]) * 16 + nibble(v.as_string()[2 * i + 1]));
  return out;
}

const std::string& record_text(const json& record, const char* key) {
  const json& v = record_member(record, key);
  if (v.type() != json::kind::string) record_malformed(key, "non è un testo");
  return v.as_string();
}

std::optional<std::uint32_t> record_optional_u32(const json& record, const char* key) {
  const json& v = record_member(record, key);
  if (v.type() == json::kind::null) return std::nullopt;
  return static_cast<std::uint32_t>(record_integer(v, key, 0, 4294967295LL));
}

std::vector<std::string> record_strings(const json& record, const char* key) {
  const json& v = record_member(record, key);
  if (v.type() != json::kind::array) record_malformed(key, "non è un array");
  std::vector<std::string> out;
  for (const json& item : v.items()) {
    if (item.type() != json::kind::string) record_malformed(key, "elemento non testuale");
    out.push_back(item.as_string());
  }
  return out;
}

// Numero decimale senza segno; la forma canonica si verifica riscrivendo il valore letto.
std::uint32_t decimal_u32(std::string_view text, const char* key) {
  std::uint32_t v = 0;
  const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), v);
  if (ec != std::errc{} || end != text.data() + text.size()) record_malformed(key, "numero non valido");
  return v;
}

// Versione "a.b.c.d" come la scrive `dotted`.
std::array<std::uint8_t, 4> record_version(const json& record, const char* key) {
  const std::string& text = record_text(record, key);
  std::array<std::uint8_t, 4> out{};
  std::size_t from = 0;
  for (std::size_t k = 0; k < 4; ++k) {
    const std::size_t dot = k < 3 ? text.find('.', from) : text.size();
    if (dot == std::string::npos) record_malformed(key, "versione non di quattro numeri");
    const std::uint32_t part = decimal_u32(std::string_view(text).substr(from, dot - from), key);
    if (part > 255) record_malformed(key, "componente della versione fuori dominio");
    out[k] = static_cast<std::uint8_t>(part);
    from = dot + 1;
  }
  if (dotted(out) != text) record_malformed(key, "versione non canonica");
  return out;
}

// Inverso di `rational_pair`: intero come {v, 1}, testo "num/den" non intero come scritto.
std::optional<std::array<dng_rational, 2>> record_rational_pair(const json& record, const char* key) {
  const json& v = record_member(record, key);
  if (v.type() == json::kind::null) return std::nullopt;
  if (v.type() != json::kind::array || v.items().size() != 2) record_malformed(key, "non sono due valori");
  std::array<dng_rational, 2> out{};
  for (std::size_t k = 0; k < 2; ++k) {
    const json& item = v.items()[k];
    if (item.type() == json::kind::string) {
      const std::string& text = item.as_string();
      const std::size_t slash = text.find('/');
      if (slash == std::string::npos) record_malformed(key, "razionale senza '/'");
      const dng_rational x{decimal_u32(std::string_view(text).substr(0, slash), key),
                           decimal_u32(std::string_view(text).substr(slash + 1), key)};
      if (x.den == 0 || x.num % x.den == 0) record_malformed(key, "razionale nullo o intero scritto come testo");
      if (std::to_string(x.num) + "/" + std::to_string(x.den) != text) record_malformed(key, "razionale non canonico");
      out[k] = x;
    } else {
      out[k] = {static_cast<std::uint32_t>(record_integer(item, key, 0, 4294967295LL)), 1};
    }
  }
  return out;
}

// U13 §8: campi propri del decoder del progetto, inverso di `set_astro_dng`.
dng_decoder_fields record_astro_dng(const json& record) {
  if (record_text(record, "dng_spec") != "DNG 1.7.1.0") record_malformed("dng_spec", "specifica diversa da DNG 1.7.1.0");
  dng_decoder_fields f;
  f.dng_version = record_version(record, "dng_version");
  f.dng_backward_version = record_version(record, "dng_backward_version");
  const json& seg = record_member(record, "segments");
  const auto seg_u32 = [&seg](const char* key, const char* name) {
    return static_cast<std::uint32_t>(record_integer(record_nested(seg, key, name), name, 0, 4294967295LL));
  };
  const json& kind_v = record_nested(seg, "kind", "segments.kind");
  if (kind_v.type() != json::kind::string) record_malformed("segments.kind", "non è un testo");
  const std::string& kind = kind_v.as_string();
  if (kind != "tiles" && kind != "strips") record_malformed("segments.kind", "valore sconosciuto");
  // Il decoder lascia a 0 le dimensioni dell'altra disposizione (struttura del segmento, dng_decode.cpp).
  f.tiles = kind == "tiles";
  if (f.tiles) {
    f.tile_width = seg_u32("tile_width", "segments.tile_width");
    f.tile_length = seg_u32("tile_length", "segments.tile_length");
  } else {
    f.rows_per_strip = seg_u32("rows_per_strip", "segments.rows_per_strip");
  }
  f.segment_count =
      static_cast<std::uint64_t>(record_integer(record_nested(seg, "count", "segments.count"), "segments.count", 0, INT64_MAX));
  f.predictor = record_u32(record, "predictor");
  const std::string& order = record_text(record, "byte_order");
  if (order != "II" && order != "MM") record_malformed("byte_order", "valore sconosciuto");
  f.little_endian = order == "II";
  f.zlib_runtime_version = record_text(record, "zlib_version");
  f.orientation = record_optional_u32(record, "orientation");
  f.default_crop_origin = record_rational_pair(record, "default_crop_origin");
  f.default_crop_size = record_rational_pair(record, "default_crop_size");
  f.metadata_rejected = record_strings(record, "metadata_rejected");
  f.opcode_lists = record_strings(record, "opcode_lists");
  return f;
}

}  // namespace

decoder_state decoder_state_of_record(const json& record) {
  decoder_state s;
  s.raw_width = record_u32(record, "raw_width");
  s.raw_height = record_u32(record, "raw_height");
  const json& crop = record_member(record, "crop");
  const std::array<std::uint32_t, 4> c = record_crop(crop, "crop");
  s.window = decoder_window{c[0], c[1], c[2], c[3]};
  const json& black = record_member(record, "black");
  s.black = record_u32(black, "scalar");
  const std::vector<std::int64_t> per_color = record_integers(record_member(black, "per_color"), "black.per_color", 4, 0, 4294967295LL);
  for (std::size_t i = 0; i < 4; ++i) s.black_per_color[i] = static_cast<std::uint32_t>(per_color[i]);
  s.black_pattern_rows = record_u32(black, "pattern_rows");
  s.black_pattern_cols = record_u32(black, "pattern_cols");
  const json& pattern = record_member(black, "pattern");
  if (pattern.type() != json::kind::array) record_malformed("black.pattern", "non è un array");
  for (const json& item : pattern.items()) s.black_pattern.push_back(static_cast<std::uint32_t>(record_integer(item, "black.pattern", 0, 4294967295LL)));
  const std::vector<std::int64_t> color_index = record_integers(record_member(record, "cfa_color_index"), "cfa_color_index", 4, 0, 3);
  for (std::size_t i = 0; i < 4; ++i) s.cfa_color_index[i] = static_cast<std::uint8_t>(color_index[i]);
  s.white = record_u32(record, "white");
  const json& data_maximum = record_member(record, "data_maximum");
  if (data_maximum.type() != json::kind::null) s.data_maximum = static_cast<std::uint32_t>(record_integer(data_maximum, "data_maximum", 0, 4294967295LL));
  const json& linear_max = record_member(record, "linear_max");
  if (linear_max.type() != json::kind::null) {
    const std::vector<std::int64_t> lm = record_integers(linear_max, "linear_max", 4, INT64_MIN, INT64_MAX);
    s.linear_max = std::array<std::int64_t, 4>{lm[0], lm[1], lm[2], lm[3]};
  }
  const json& lin = record_member(record, "linearization");
  if (lin.type() != json::kind::string || (lin.as_string() != "identity" && lin.as_string() != "decoder_curve")) {
    record_malformed("linearization", "valore sconosciuto");
  }
  s.linearization = lin.as_string() == "decoder_curve" ? decoder_linearization::decoder_curve : decoder_linearization::identity;
  const json& curve = record_member(record, "linearization_curve_sha256");
  if (curve.type() != json::kind::null) s.curve_sha256 = record_digest(curve, "linearization_curve_sha256");
  const std::string& decoder = record_text(record, "decoder");
  if (decoder == to_string(raw_decoder_kind::libraw)) {
    s.route.decoder = raw_decoder_kind::libraw;
  } else if (decoder == to_string(raw_decoder_kind::astro_dng)) {
    s.route.decoder = raw_decoder_kind::astro_dng;
  } else {
    record_malformed("decoder", "valore sconosciuto");
  }
  s.route.compression = record_optional_u32(record, "compression");
  s.route.sample_format = record_optional_u32(record, "sample_format");
  s.route.bits_per_sample = record_optional_u32(record, "bits_per_sample");
  switch (s.route.decoder) {
    case raw_decoder_kind::libraw: {
      s.route.reason = record_text(record, "route_reason");
      if (s.route.reason != "not_tiff" && s.route.reason != "tiff_not_dng" && s.route.reason != "dng_libraw_compression") {
        record_malformed("route_reason", "valore sconosciuto");
      }
      s.process_warnings = record_u32(record, "process_warnings");
      const json& insets = record_member(record, "raw_inset_crops");
      if (insets.type() != json::kind::array || insets.items().size() != 2) record_malformed("raw_inset_crops", "non sono due ritagli");
      for (std::size_t i = 0; i < 2; ++i) s.raw_inset_crops[i] = record_crop(insets.items()[i], "raw_inset_crops");
      s.flip = static_cast<std::int32_t>(record_integer(record_member(record, "flip"), "flip", INT32_MIN, INT32_MAX));
      break;
    }
    // Motivo vuoto (U13 §3); `process_warnings`, `raw_inset_crops` e `flip` sono di LibRaw: il decoder del progetto
    // non li scrive e restano a zero come nello stato che produce.
    case raw_decoder_kind::astro_dng: s.dng = record_astro_dng(record); break;
  }
  s.source = metadata_source{metadata_origin::decoder, "decoder_record"};
  return s;
}

}  // namespace astro
