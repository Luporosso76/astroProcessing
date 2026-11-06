// U5 (M3.a), G-OFFSET: stato dell'offset del light e voce `light_offset` (docs/contracts/C-01-calibrazione-light.md §1, §2,
// §9, §10; ADR-16.b §b.5 colonna «Offset del light dopo la catena», §b.10; ADR-08.b §2.4 e §2.9).
// T-M3A-C01-01 (stato dell'offset per catena del light x tipo di light) e T-M3A-C01-03 (voce `light_offset`).
//
// Interfaccia di prodotto che i test presuppongono (nuovo `astro/calibration_analysis.hpp`, implementata in
// `src/core/calibration_analysis.cpp`; nomi scelti dall'autore dei test, il contratto non li fissa):
//   enum class light_chain { none, bias, dark, bias_dark_sub, dark_sub_partial };       // «light.none» ecc.
//   enum class offset_state { removed, known_not_removed, unknown };
//   enum class black_applied_by_decoder { none };                                        // ADR-08.b §2.9
//   struct decoder_identity { std::string libraw_version; std::uint32_t libraw_version_number;
//                             std::uint32_t libraw_capabilities; std::string libraw_tarball_sha256; };
//     Quattro campi (ADR-08.b §2.9: versione, numero di versione, capacità, SHA-256 del tarball); i test li assegnano
//     sempre tutti, con valori espliciti.
//   struct decoder_black_period { std::uint32_t rows, cols; std::vector<std::uint64_t> values; };  // riga per riga
//   decoder_black_period decoder_black_period_of(const decoder_state&);
//   struct offset_remover { master_role role; content_digest digest; };
//   struct light_offset_inputs { std::optional<content_digest> bias, dark, dark_prime;
//                                std::optional<decoder_identity> decoder; };
//   struct light_offset_entry { offset_state state; std::optional<offset_remover> remover;
//                               std::optional<decoder_black_period> black;
//                               std::optional<black_applied_by_decoder> black_applied;
//                               std::optional<decoder_identity> identity; };
//   light_offset_entry make_light_offset_entry(light_chain, const image& light, const light_offset_inputs&);
// Scelte dell'autore dei test, non fissate dal contratto (da ratificare in revisione):
//   - periodo limitato alla finestra visibile: righe = min(lcm(2, righe motivo), altezza finestra), colonne analoghe
//     (ADR-16.b §b.7 «limitato all'area visibile»);
//   - errori `invalid_input`: ingresso richiesto dalla catena assente (`calib.chain_input`), ingresso non richiesto dalla
//     catena o identità del decoder su un light senza decoder_state (`calib.chain_input_not_applicable`, stessa regola
//     dei parametri non applicabili di ADR-16.b §b.5), light RAW senza identità (`calib.decoder_identity`);
//   - `decoder_black_period_of` ferma con `calib.decoder_state` uno stato incoerente (indice colore > 3, motivo del nero
//     con una sola dimensione nulla, con dimensioni diverse dai valori o con più di `decoder_state::max_black_pattern`
//     (4098, come `validate_image`) celle, finestra visibile vuota), con `check` e `window` nel contesto.
// Verità: tabelle scritte a mano dal contratto, mai ottenute chiamando il codice sotto prova.
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "astro/calibration.hpp"
#include "astro/calibration_analysis.hpp"
#include "astro/fits.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "astro/raw_decode.hpp"
#include "expect_stop.hpp"
#include "syn_dng.hpp"
#include "syn_fits.hpp"

namespace {

using namespace astro;
namespace fx = astro_fixture;

constexpr std::uint64_t big_budget = std::uint64_t{1} << 30;

const content_digest digest_b{std::string(64, 'b'), 1111};
const content_digest digest_d{std::string(64, 'd'), 2222};
const content_digest digest_dp{std::string(64, 'e'), 3333};
decoder_identity make_identity_x() {
  decoder_identity id;
  id.libraw_version = "0.21.5-test";
  id.libraw_version_number = 0x00001505u;
  id.libraw_capabilities = 0x0000A5C3u;
  id.libraw_tarball_sha256 = std::string(64, 'a');
  return id;
}
const decoder_identity identity_x = make_identity_x();

enum class light_kind { raw, fits_plain, fits_offset_header, fits_offset_declared, fits_pedestal, raw_pedestal, fits_pedestal_zero };

std::vector<std::uint8_t> dng_bytes(std::uint16_t black) {
  fx::dng_spec s;
  s.width = 64;
  s.height = 48;
  s.white = 4000;
  s.black = {black};
  std::vector<std::uint16_t> v(64u * 48u);
  for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<std::uint16_t>(100 + (i * 7) % 3000);
  s.pixels = fx::pixels_u16(v);
  return fx::make_dng(s);
}

// Light FITS dal file sintetico: `calib_header` scrive sempre la card OFFSET (10), letta con origine header.
image fits_light() {
  const fx::calib_header h;
  return decode_fits(fx::float_frame(4, 3, -32, std::vector<double>(12, 500.0), fx::calib_cards(h), 1), big_budget, {});
}

// Un light per tipo; `fits_plain` toglie il valore di OFFSET dai soli metadati (il campo resta assente, nessun valore di comodo).
image light_of(light_kind kind) {
  switch (kind) {
    case light_kind::raw: {
      image img = decode_raw_buffer(dng_bytes(64));
      return img;
    }
    case light_kind::raw_pedestal: {
      image img = decode_raw_buffer(dng_bytes(64));
      img.metadata.pedestal_adu = metadata_value<double>{16.0, {metadata_origin::declared, {}}};
      return img;
    }
    case light_kind::fits_plain: {
      image img = fits_light();
      img.metadata.offset_adu.reset();
      return img;
    }
    case light_kind::fits_offset_header: {
      image img = fits_light();
      EXPECT_TRUE(img.metadata.offset_adu.has_value());
      if (img.metadata.offset_adu) {
        EXPECT_EQ(img.metadata.offset_adu->source.origin, metadata_origin::header);
      }
      return img;
    }
    case light_kind::fits_offset_declared: {
      image img = fits_light();
      img.metadata.offset_adu = metadata_value<double>{10.0, {metadata_origin::declared, {}}};
      return img;
    }
    case light_kind::fits_pedestal: {
      image img = fits_light();
      img.metadata.offset_adu.reset();
      img.metadata.pedestal_adu = metadata_value<double>{5.0, {metadata_origin::declared, {}}};
      return img;
    }
    case light_kind::fits_pedestal_zero: {
      image img = fits_light();
      img.metadata.offset_adu.reset();
      img.metadata.pedestal_adu = metadata_value<double>{0.0, {metadata_origin::declared, {}}};
      return img;
    }
  }
  return {};
}

// Ingressi esattamente quelli che la catena richiede (ADR-16.b §b.5, colonna «Ingressi»).
light_offset_inputs inputs_for(light_chain chain, bool raw) {
  light_offset_inputs in;
  switch (chain) {
    case light_chain::none: break;
    case light_chain::bias: in.bias = digest_b; break;
    case light_chain::dark: in.dark = digest_d; break;
    case light_chain::bias_dark_sub:
      in.bias = digest_b;
      in.dark_prime = digest_dp;
      break;
    case light_chain::dark_sub_partial: in.dark_prime = digest_dp; break;
  }
  if (raw) in.decoder = identity_x;
  return in;
}

const std::vector<light_chain> all_chains = {light_chain::none, light_chain::bias, light_chain::dark,
                                             light_chain::bias_dark_sub, light_chain::dark_sub_partial};

}  // namespace

// ---------------------------------------------------------------------------------------------- T-M3A-C01-01

// Tabella di ADR-16.b §b.5 con la definizione di C-01.b §1, scritta a mano: `removed` dopo bias, dark e bias+dark_sub per
// ogni light; dopo `light.none` e `light.dark_sub_partial`: `known_not_removed` per il light con `decoder_state`,
// `unknown` per ogni FITS, con o senza `OFFSET` (valore dell'header o dichiarato).
TEST(OffsetState, T_M3A_C01_01_state_table) {
  struct row {
    light_kind kind;
    const char* name;
    offset_state after_none_or_partial;
  };
  const std::vector<row> rows = {
      {light_kind::raw, "RAW con decoder_state", offset_state::known_not_removed},
      {light_kind::fits_plain, "FITS senza OFFSET", offset_state::unknown},
      {light_kind::fits_offset_header, "FITS con OFFSET da header", offset_state::unknown},
      {light_kind::fits_offset_declared, "FITS con offset_adu dichiarato", offset_state::unknown},
      {light_kind::fits_pedestal_zero, "FITS con pedestal_adu = 0", offset_state::unknown},
  };
  for (const row& r : rows) {
    const image light = light_of(r.kind);
    const bool raw = r.kind == light_kind::raw;
    EXPECT_EQ(light.decoder_state.has_value(), raw) << r.name;
    for (const light_chain chain : all_chains) {
      SCOPED_TRACE(std::string(r.name) + " catena " + to_string(chain));
      const offset_state want = (chain == light_chain::none || chain == light_chain::dark_sub_partial)
                                    ? r.after_none_or_partial
                                    : offset_state::removed;
      const light_offset_entry e = make_light_offset_entry(chain, light, inputs_for(chain, raw));
      EXPECT_EQ(e.state, want);
    }
  }
}

// I nomi testuali degli stati e delle catene sono quelli del contratto (compaiono nel ledger e nelle chiavi).
TEST(OffsetState, T_M3A_C01_01_names) {
  EXPECT_STREQ(to_string(offset_state::removed), "removed");
  EXPECT_STREQ(to_string(offset_state::known_not_removed), "known_not_removed");
  EXPECT_STREQ(to_string(offset_state::unknown), "unknown");
  EXPECT_STREQ(to_string(light_chain::none), "light.none");
  EXPECT_STREQ(to_string(light_chain::bias), "light.bias");
  EXPECT_STREQ(to_string(light_chain::dark), "light.dark");
  EXPECT_STREQ(to_string(light_chain::bias_dark_sub), "light.bias_dark_sub");
  EXPECT_STREQ(to_string(light_chain::dark_sub_partial), "light.dark_sub_partial");
}

// Il valore di `OFFSET` non cambia lo stato: stessa risposta con l'header, con il valore dichiarato e senza (C-01.b §1).
TEST(OffsetState, T_M3A_C01_01_offset_value_never_changes_state) {
  for (const light_chain chain : {light_chain::none, light_chain::dark_sub_partial}) {
    for (const double value : {0.0, 10.0, 100.0, 65535.0}) {
      image light = light_of(light_kind::fits_plain);
      light.metadata.offset_adu = metadata_value<double>{value, {metadata_origin::header, "OFFSET"}};
      EXPECT_EQ(make_light_offset_entry(chain, light, inputs_for(chain, false)).state, offset_state::unknown)
          << to_string(chain) << " OFFSET=" << value;
    }
  }
}

// Piedistallo diverso da zero: stop `unsupported`, `calib.pedestal`, per ogni catena e per ogni tipo di light (C-01.b §2).
TEST(OffsetState, T_M3A_C01_01_pedestal_stops) {
  for (const light_kind kind : {light_kind::fits_pedestal, light_kind::raw_pedestal}) {
    const image light = light_of(kind);
    const bool raw = kind == light_kind::raw_pedestal;
    for (const light_chain chain : all_chains) {
      SCOPED_TRACE(std::string("catena ") + to_string(chain) + (raw ? " RAW" : " FITS"));
      EXPECT_STOP(make_light_offset_entry(chain, light, inputs_for(chain, raw)), error_category::unsupported,
                  "calib.pedestal");
    }
  }
}

// Un piedistallo presente ma uguale a zero non è un piedistallo: nessuno stop (C-01.b §2, «diverso da zero»).
TEST(OffsetState, T_M3A_C01_01_zero_pedestal_is_accepted) {
  const image light = light_of(light_kind::fits_pedestal_zero);
  EXPECT_NO_THROW((void)make_light_offset_entry(light_chain::none, light, inputs_for(light_chain::none, false)));
}

// Un'immagine RAW senza identità del decoder non si descrive con un valore inventato: errore esplicito (nessun default).
TEST(OffsetState, T_M3A_C01_01_raw_without_identity_fails) {
  const image light = light_of(light_kind::raw);
  EXPECT_STOP(make_light_offset_entry(light_chain::none, light, inputs_for(light_chain::none, false)),
              error_category::invalid_input, "calib.decoder_identity");
}

// Ingresso richiesto dalla catena assente: errore `invalid_input`, nessuna voce (la catena non si degrada da sola).
TEST(OffsetState, T_M3A_C01_01_missing_chain_input_fails) {
  const image light = light_of(light_kind::fits_plain);
  struct bad {
    light_chain chain;
    light_offset_inputs in;
  };
  light_offset_inputs only_dp;
  only_dp.dark_prime = digest_dp;
  light_offset_inputs only_b;
  only_b.bias = digest_b;
  const std::vector<bad> cases = {
      {light_chain::bias, {}},
      {light_chain::dark, {}},
      {light_chain::bias_dark_sub, only_dp},
      {light_chain::bias_dark_sub, only_b},
      {light_chain::dark_sub_partial, {}},
  };
  for (const bad& c : cases) {
    SCOPED_TRACE(to_string(c.chain));
    EXPECT_STOP(make_light_offset_entry(c.chain, light, c.in), error_category::invalid_input, "calib.chain_input");
  }
}

// Stato e master derivano dalla catena e dal light, non da quali ingressi sono presenti: un ingresso non richiesto dalla
// catena, o un'identità del decoder su un light senza decoder_state, è un errore (nessuna deduzione dagli ingressi).
TEST(OffsetState, T_M3A_C01_01_superfluous_input_fails) {
  const image fits = light_of(light_kind::fits_plain);
  const image raw = light_of(light_kind::raw);
  light_offset_inputs with_bias;
  with_bias.bias = digest_b;
  light_offset_inputs with_dark_prime;
  with_dark_prime.dark_prime = digest_dp;
  light_offset_inputs bias_and_dark = inputs_for(light_chain::bias, false);
  bias_and_dark.dark = digest_d;
  light_offset_inputs dark_and_bias = inputs_for(light_chain::dark, false);
  dark_and_bias.bias = digest_b;
  light_offset_inputs partial_and_bias = inputs_for(light_chain::dark_sub_partial, false);
  partial_and_bias.bias = digest_b;
  light_offset_inputs fits_with_decoder = inputs_for(light_chain::none, false);
  fits_with_decoder.decoder = identity_x;
  struct bad {
    light_chain chain;
    light_offset_inputs in;
    const image* light;
    const char* name;
  };
  const std::vector<bad> cases = {
      {light_chain::none, with_bias, &fits, "none con bias"},
      {light_chain::none, with_dark_prime, &fits, "none con D'"},
      {light_chain::bias, bias_and_dark, &fits, "bias con dark"},
      {light_chain::dark, dark_and_bias, &fits, "dark con bias"},
      {light_chain::dark_sub_partial, partial_and_bias, &fits, "partial con bias"},
      {light_chain::none, fits_with_decoder, &fits, "light FITS con identità del decoder"},
      {light_chain::none, [&] { auto in = inputs_for(light_chain::none, true); in.bias = digest_b; return in; }(), &raw,
       "light RAW, none con bias"},
  };
  for (const bad& c : cases) {
    SCOPED_TRACE(c.name);
    EXPECT_STOP(make_light_offset_entry(c.chain, *c.light, c.in), error_category::invalid_input,
                "calib.chain_input_not_applicable");
  }
}

// ---------------------------------------------------------------------------------------------- T-M3A-C01-03

namespace {

// Stato del decoder in memoria, riempito campo per campo (ADR-03.b §b.2). La finestra visibile è 40 x 32.
decoder_state make_state(std::uint32_t black, std::array<std::uint32_t, 4> per_color, std::array<std::uint8_t, 4> cfa,
                         std::uint32_t rows, std::uint32_t cols, std::vector<std::uint32_t> pattern,
                         decoder_window window = {0, 0, 40, 32}) {
  decoder_state st;
  st.raw_width = window.width + 8;
  st.raw_height = window.height + 8;
  st.window = window;
  st.black = black;
  st.cfa_color_index = cfa;
  st.black_per_color = per_color;
  st.black_pattern_rows = rows;
  st.black_pattern_cols = cols;
  st.black_pattern = std::move(pattern);
  st.white = 4000;
  st.source.origin = metadata_origin::decoder;
  return st;
}

// Light RAW in memoria coerente con validate_image: parte da un DNG 64 x 48 decodificato (fase raw, cfa rggb, campioni
// allocati) e ne sostituisce lo stato con quello dato, con finestra e dimensioni del buffer grezzo del DNG.
image raw_image_with(decoder_state st) {
  image img = decode_raw_buffer(dng_bytes(64));
  st.raw_width = img.decoder_state->raw_width;
  st.raw_height = img.decoder_state->raw_height;
  st.window = img.decoder_state->window;
  img.decoder_state = std::move(st);
  validate_image(img);
  return img;
}

void expect_period(const decoder_black_period& got, std::uint32_t rows, std::uint32_t cols,
                   const std::vector<std::uint64_t>& want) {
  EXPECT_EQ(got.rows, rows);
  EXPECT_EQ(got.cols, cols);
  ASSERT_EQ(got.values.size(), want.size());
  for (std::size_t i = 0; i < want.size(); ++i)
    EXPECT_EQ(got.values[i], want[i]) << "posizione (" << i / cols << "," << i % cols << ")";
}

}  // namespace

// Motivo 2x3: periodo lcm(2,2) x lcm(2,3) = 2 x 6. Colori (0,0)=0, (0,1)=1, (1,0)=1, (1,1)=2, nero per colore 5, 7, 11.
// b(r,c) = 100 + per_color[FC(r,c)] + P[r mod 2][c mod 3], P = {1,2,3 / 40,50,60}, calcolato a mano:
//   riga 0 (colori 0,1,0,1,0,1): 100+5+1, 100+7+2, 100+5+3, 100+7+1, 100+5+2, 100+7+3
//   riga 1 (colori 1,2,1,2,1,2): 100+7+40, 100+11+50, 100+7+60, 100+11+40, 100+7+50, 100+11+60
TEST(LightOffsetEntry, T_M3A_C01_03_pattern_2x3) {
  const decoder_state st = make_state(100, {5, 7, 11, 0}, {0, 1, 1, 2}, 2, 3, {1, 2, 3, 40, 50, 60});
  expect_period(decoder_black_period_of(st), 2, 6,
                {106, 109, 108, 108, 107, 110,  //
                 147, 161, 167, 151, 157, 171});
}

// Motivo 1x4: periodo lcm(2,1) x lcm(2,4) = 2 x 4. Colori (0,0)=2, (0,1)=1, (1,0)=1, (1,1)=0, per colore 100, 200, 300, 400;
// nero scalare 1000, P = {1,2,4,8} (una riga):
//   riga 0 (colori 2,1,2,1): 1000+300+1, 1000+200+2, 1000+300+4, 1000+200+8
//   riga 1 (colori 1,0,1,0): 1000+200+1, 1000+100+2, 1000+200+4, 1000+100+8
TEST(LightOffsetEntry, T_M3A_C01_03_pattern_1x4) {
  const decoder_state st = make_state(1000, {100, 200, 300, 400}, {2, 1, 1, 0}, 1, 4, {1, 2, 4, 8});
  expect_period(decoder_black_period_of(st), 2, 4,
                {1301, 1202, 1304, 1208,  //
                 1201, 1102, 1204, 1108});
}

// Motivo 0x0 (dimensione nulla = 1): periodo 2 x 2 dal solo colore. Colori (0,0)=0, (0,1)=1, (1,0)=1, (1,1)=2, nero per
// colore 0, 16, 32, nero 64: 64+0, 64+16, 64+16, 64+32.
TEST(LightOffsetEntry, T_M3A_C01_03_pattern_0x0) {
  const decoder_state st = make_state(64, {0, 16, 32, 48}, {0, 1, 1, 2}, 0, 0, {});
  expect_period(decoder_black_period_of(st), 2, 2, {64, 80, 80, 96});
}

// Somma di tre uint32 al massimo: 3 * 4294967295 non entra in 32 bit, il nero effettivo è un intero a 64 bit esatto.
// Colori (0,0)=0, (0,1)=1, (1,0)=1, (1,1)=2. Sito (0,0): 0xFFFFFFFF + 0xFFFFFFFF (colore 0) + P(1x1) 0xFFFFFFFF =
// 12884901885; gli altri tre siti hanno nero per colore 0: 0xFFFFFFFF + 0 + 0xFFFFFFFF = 8589934590.
TEST(LightOffsetEntry, T_M3A_C01_03_black_is_exact_64_bit) {
  const decoder_state st = make_state(0xFFFFFFFFu, {0xFFFFFFFFu, 0, 0, 0}, {0, 1, 1, 2}, 1, 1, {0xFFFFFFFFu});
  expect_period(decoder_black_period_of(st), 2, 2, {12884901885ULL, 8589934590ULL, 8589934590ULL, 8589934590ULL});
}

// Periodo limitato all'area visibile (ADR-16.b §b.7): lo stato 2x3 con finestra 3 x 3 ha periodo 2 x 3 (lcm 2 x 6 limitato
// alla larghezza), con finestra 1 x 1 il solo sito (0,0).
TEST(LightOffsetEntry, T_M3A_C01_03_period_limited_to_window) {
  const decoder_state wide = make_state(100, {5, 7, 11, 0}, {0, 1, 1, 2}, 2, 3, {1, 2, 3, 40, 50, 60}, {0, 0, 3, 3});
  expect_period(decoder_black_period_of(wide), 2, 3, {106, 109, 108, 147, 161, 167});
  const decoder_state one = make_state(100, {5, 7, 11, 0}, {0, 1, 1, 2}, 2, 3, {1, 2, 3, 40, 50, 60}, {0, 0, 1, 1});
  expect_period(decoder_black_period_of(one), 1, 1, {106});
}

// Voce completa da uno stato in memoria: nero per posizione, `black_applied_by_decoder = none`, identità del decoder
// riportata così com'è, master che rimuove l'offset (ruolo e digest) solo se la catena ne ha uno.
TEST(LightOffsetEntry, T_M3A_C01_03_entry_from_memory_state) {
  const image light = raw_image_with(make_state(64, {0, 16, 32, 48}, {0, 1, 1, 2}, 0, 0, {}));
  for (const light_chain chain : all_chains) {
    SCOPED_TRACE(to_string(chain));
    const light_offset_entry e = make_light_offset_entry(chain, light, inputs_for(chain, true));
    ASSERT_TRUE(e.black.has_value());
    expect_period(*e.black, 2, 2, {64, 80, 80, 96});
    ASSERT_TRUE(e.black_applied.has_value());
    EXPECT_EQ(*e.black_applied, black_applied_by_decoder::none);
    ASSERT_TRUE(e.identity.has_value());
    EXPECT_EQ(*e.identity, identity_x);
    switch (chain) {
      case light_chain::none:
      case light_chain::dark_sub_partial:
        EXPECT_FALSE(e.remover.has_value());
        break;
      case light_chain::bias:
      case light_chain::bias_dark_sub:
        ASSERT_TRUE(e.remover.has_value());
        EXPECT_EQ(e.remover->role, master_role::bias);
        EXPECT_EQ(e.remover->digest, digest_b);
        break;
      case light_chain::dark:
        ASSERT_TRUE(e.remover.has_value());
        EXPECT_EQ(e.remover->role, master_role::dark);
        EXPECT_EQ(e.remover->digest, digest_d);
        break;
    }
  }
}

// Un light FITS non ha nero del decoder: i tre campi RAW restano assenti, non nulli (C-01.b §10, «per un light RAW»).
TEST(LightOffsetEntry, T_M3A_C01_03_fits_light_has_no_decoder_fields) {
  const image light = light_of(light_kind::fits_plain);
  const light_offset_entry e = make_light_offset_entry(light_chain::none, light, inputs_for(light_chain::none, false));
  EXPECT_FALSE(e.black.has_value());
  EXPECT_FALSE(e.black_applied.has_value());
  EXPECT_FALSE(e.identity.has_value());
  EXPECT_FALSE(e.remover.has_value());
}

// Controllo di firma e di effetti: la voce non tocca campioni e stato del light (la regola «mai sottratto» sui campioni
// di uscita la prova T-M3A-C01-04 in M3.c).
TEST(LightOffsetEntry, T_M3A_C01_03_light_never_modified) {
  const image light = light_of(light_kind::raw);
  const std::vector<std::byte> before(light.samples.data(), light.samples.data() + light.samples.size());
  const decoder_state state_before = *light.decoder_state;
  (void)make_light_offset_entry(light_chain::none, light, inputs_for(light_chain::none, true));
  const std::vector<std::byte> after(light.samples.data(), light.samples.data() + light.samples.size());
  EXPECT_EQ(before, after);
  EXPECT_EQ(*light.decoder_state, state_before);
}

// Dai DNG di T-M3A-RAW-02 decodificati dal prodotto: il nero della voce è uguale al valore scritto nel file (verità
// indipendente: il valore passato allo scrittore TIFF), non alla formula del contratto applicata dal test.
namespace {

struct dng_black_case {
  const char* name;
  std::uint32_t origin_x;
  std::uint32_t origin_y;
  std::vector<std::uint16_t> written;  // 1 valore = scalare, 4 = motivo 2x2 riga per riga
};

const std::vector<std::vector<std::uint8_t>>& cfa_layouts() {
  static const std::vector<std::vector<std::uint8_t>> layouts = {{0, 1, 1, 2}, {2, 1, 1, 0}, {1, 0, 2, 1}, {1, 2, 0, 1}};
  return layouts;
}

void check_dng_black(const dng_black_case& c, const std::vector<std::uint8_t>& cfa) {
  SCOPED_TRACE(std::string(c.name) + " cfa " + std::to_string(cfa[0]) + std::to_string(cfa[1]) + std::to_string(cfa[2]) +
               std::to_string(cfa[3]));
  fx::dng_spec s;
  s.width = 64;
  s.height = 48;
  s.cfa = cfa;
  s.white = 3900;
  s.active_area = fx::dng_active_area{c.origin_y, c.origin_x, c.origin_y + 32, c.origin_x + 40};
  if (c.written.size() > 1) s.black_dim = std::make_pair<std::uint16_t, std::uint16_t>(2, 2);
  s.black = c.written;
  std::vector<std::uint16_t> v(64u * 48u);
  for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<std::uint16_t>(100 + (i * 13) % 3500);
  s.pixels = fx::pixels_u16(v);
  const image light = decode_raw_buffer(fx::make_dng(s));
  ASSERT_TRUE(light.decoder_state.has_value());
  const light_offset_entry e = make_light_offset_entry(light_chain::none, light, inputs_for(light_chain::none, true));
  EXPECT_EQ(e.state, offset_state::known_not_removed);
  ASSERT_TRUE(e.black.has_value());
  ASSERT_TRUE(e.black_applied.has_value());
  EXPECT_EQ(*e.black_applied, black_applied_by_decoder::none);
  // Il periodo è 2 x 2 per ogni caso qui sotto (motivo scritto 2x2 o scalare).
  ASSERT_EQ(e.black->rows, 2u);
  ASSERT_EQ(e.black->cols, 2u);
  ASSERT_EQ(e.black->values.size(), 4u);
  for (std::uint32_t r = 0; r < 2; ++r)
    for (std::uint32_t col = 0; col < 2; ++col) {
      const std::uint64_t want = c.written.size() == 1 ? c.written[0] : c.written[r * 2 + col];
      EXPECT_EQ(e.black->values[r * 2 + col], want) << "posizione (" << r << "," << col << ")";
    }
}

// Stato incoerente: stop `calib.decoder_state` con categoria, codice e contesto (`check`, `window`).
void expect_state_stop(const decoder_state& st, const std::string& check, const std::string& window) {
  try {
    (void)decoder_black_period_of(st);
    ADD_FAILURE() << "stop_error atteso, nessuna eccezione (check " << check << ")";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::invalid_input) << e.what();
    EXPECT_EQ(e.code(), "calib.decoder_state") << e.what();
    std::map<std::string, std::string> ctx(e.context().begin(), e.context().end());
    ASSERT_TRUE(ctx.count("check")) << e.what();
    ASSERT_TRUE(ctx.count("window")) << e.what();
    EXPECT_EQ(ctx["check"], check);
    EXPECT_EQ(ctx["window"], window);
  }
}

}  // namespace

// Stato del decoder incoerente: nessun periodo inventato, stop `calib.decoder_state`. Indice colore maggiore di 3 (ogni
// posizione del CFA, con motivo e finestra validi).
TEST(LightOffsetEntry, T_M3A_C01_03_state_color_index_above_3_stops) {
  for (std::size_t i = 0; i < 4; ++i) {
    SCOPED_TRACE(i);
    std::array<std::uint8_t, 4> cfa = {0, 1, 1, 2};
    cfa[i] = 4;
    expect_state_stop(make_state(100, {5, 7, 11, 0}, cfa, 2, 3, {1, 2, 3, 40, 50, 60}), "cfa_color_index", "40x32");
  }
  expect_state_stop(make_state(100, {5, 7, 11, 0}, {0, 1, 1, 255}, 0, 0, {}), "cfa_color_index", "40x32");
}

// Motivo del nero incoerente: una sola dimensione nulla, oppure valori in numero diverso da righe x colonne.
TEST(LightOffsetEntry, T_M3A_C01_03_state_pattern_inconsistent_stops) {
  expect_state_stop(make_state(100, {5, 7, 11, 0}, {0, 1, 1, 2}, 2, 0, {}), "black_pattern", "40x32");
  expect_state_stop(make_state(100, {5, 7, 11, 0}, {0, 1, 1, 2}, 0, 3, {}), "black_pattern", "40x32");
  expect_state_stop(make_state(100, {5, 7, 11, 0}, {0, 1, 1, 2}, 2, 3, {1, 2, 3, 40, 50}), "black_pattern", "40x32");
  expect_state_stop(make_state(100, {5, 7, 11, 0}, {0, 1, 1, 2}, 2, 3, {1, 2, 3, 40, 50, 60, 70}), "black_pattern", "40x32");
  expect_state_stop(make_state(100, {5, 7, 11, 0}, {0, 1, 1, 2}, 0, 0, {1}), "black_pattern", "40x32");
}

// Tetto sulle celle del motivo del nero (`decoder_state::max_black_pattern`, 4098, lo stesso di validate_image): 4098 celle
// con valori coerenti non fermano e il periodo usa i valori giusti; 4099 celle (con altrettanti valori) fermano.
TEST(LightOffsetEntry, T_M3A_C01_03_state_pattern_cell_cap_boundary) {
  ASSERT_EQ(decoder_state::max_black_pattern, 4098u);
  std::vector<std::uint32_t> at_cap(4098);
  for (std::size_t i = 0; i < at_cap.size(); ++i) at_cap[i] = static_cast<std::uint32_t>(i);
  const decoder_black_period p = decoder_black_period_of(make_state(100, {5, 7, 11, 0}, {0, 1, 1, 2}, 2, 2049, at_cap));
  ASSERT_EQ(p.rows, 2u);
  ASSERT_EQ(p.cols, 40u);
  ASSERT_EQ(p.values.size(), 80u);
  EXPECT_EQ(p.values[0], 105u);                 // (0,0): 100 + 5 + 0
  EXPECT_EQ(p.values[1 * 40 + 39], 2199u);      // (1,39): 100 + 11 (indice colore 2) + 2049 + 39
  const decoder_black_period col = decoder_black_period_of(make_state(100, {5, 7, 11, 0}, {0, 1, 1, 2}, 4098, 1, at_cap));
  EXPECT_EQ(col.rows, 32u);
  EXPECT_EQ(col.cols, 2u);

  std::vector<std::uint32_t> over(4099, 1);
  expect_state_stop(make_state(100, {5, 7, 11, 0}, {0, 1, 1, 2}, 1, 4099, over), "black_pattern", "40x32");
  expect_state_stop(make_state(100, {5, 7, 11, 0}, {0, 1, 1, 2}, 4099, 1, over), "black_pattern", "40x32");
}

// Finestra visibile vuota (larghezza o altezza nulla), con motivo e colori validi.
TEST(LightOffsetEntry, T_M3A_C01_03_state_empty_window_stops) {
  expect_state_stop(make_state(100, {5, 7, 11, 0}, {0, 1, 1, 2}, 2, 3, {1, 2, 3, 40, 50, 60}, {0, 0, 0, 32}), "window", "0x32");
  expect_state_stop(make_state(100, {5, 7, 11, 0}, {0, 1, 1, 2}, 2, 3, {1, 2, 3, 40, 50, 60}, {0, 0, 40, 0}), "window", "40x0");
  expect_state_stop(make_state(100, {5, 7, 11, 0}, {0, 1, 1, 2}, 0, 0, {}, {0, 0, 0, 0}), "window", "0x0");
}

// Casi di T-M3A-RAW-02 accettati dal decoder: scalare con le quattro origini, motivo non uniforme con origine (0,0), motivi
// invarianti sull'asse spostato con origine dispari (ADR-08.b §2.4); quelli non invarianti si fermano nel decoder.
TEST(LightOffsetEntry, T_M3A_C01_03_black_from_decoded_dng) {
  const std::vector<dng_black_case> cases = {
      {"scalare 64 origine (0,0)", 0, 0, {64}},
      {"scalare 64 origine (1,0)", 1, 0, {64}},
      {"scalare 64 origine (0,1)", 0, 1, {64}},
      {"scalare 64 origine (1,1)", 1, 1, {64}},
      {"motivo {64,72,80,96} origine (0,0)", 0, 0, {64, 72, 80, 96}},
      {"motivo {64,64,80,80} origine (1,0)", 1, 0, {64, 64, 80, 80}},
      {"motivo {64,72,64,72} origine (0,1)", 0, 1, {64, 72, 64, 72}},
      {"motivo uniforme {64,64,64,64} origine (1,1)", 1, 1, {64, 64, 64, 64}},
  };
  for (const auto& cfa : cfa_layouts())
    for (const auto& c : cases) check_dng_black(c, cfa);
}
