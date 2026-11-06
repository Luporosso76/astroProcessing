#pragma once

// Andata e ritorno del record del decoder (ADR-08.b §2.9, U13 §8, F-IO-03 CL-12): `decoder_state_of_record` ricostruisce
// dal record i membri di contenuto di `decoder_state` (l'uguaglianza esclude `source`), anche dal JSON canonico come lo
// persiste il nodo d'import. Una chiave assente o un valore fuori dominio: integrity raw.record_malformed con la chiave nel
// contesto (`chiave`; `segments.<membro>` per un membro di `segments`).
// Usato da test_dng_u13b.cpp (decoder del progetto) e test_raw_import.cpp (LibRaw).

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <string_view>

#include "astro/error.hpp"
#include "astro/image.hpp"
#include "astro/json.hpp"
#include "astro/raw_decode.hpp"
#include "astro/raw_import_job.hpp"

namespace record_roundtrip {

// Nomi dei membri di contenuto diversi fra due stati, nell'ordine di dichiarazione; vuoto se uguali.
inline std::string differing_members(const astro::decoder_state& a, const astro::decoder_state& b) {
  std::string out;
  const auto add = [&out](bool differ, const char* name) {
    if (differ) out += std::string(out.empty() ? "" : ", ") + name;
  };
  add(a.raw_width != b.raw_width, "raw_width");
  add(a.raw_height != b.raw_height, "raw_height");
  add(!(a.window == b.window), "window");
  add(a.black != b.black, "black");
  add(a.cfa_color_index != b.cfa_color_index, "cfa_color_index");
  add(a.black_per_color != b.black_per_color, "black_per_color");
  add(a.black_pattern_rows != b.black_pattern_rows, "black_pattern_rows");
  add(a.black_pattern_cols != b.black_pattern_cols, "black_pattern_cols");
  add(a.black_pattern != b.black_pattern, "black_pattern");
  add(a.white != b.white, "white");
  add(a.data_maximum != b.data_maximum, "data_maximum");
  add(a.linear_max != b.linear_max, "linear_max");
  add(a.linearization != b.linearization, "linearization");
  add(a.curve_sha256 != b.curve_sha256, "curve_sha256");
  add(a.process_warnings != b.process_warnings, "process_warnings");
  add(a.raw_inset_crops != b.raw_inset_crops, "raw_inset_crops");
  add(a.flip != b.flip, "flip");
  add(!(a.route == b.route), "route");
  add(!(a.dng == b.dng), "dng");
  return out;
}

// Stato ricostruito dal record e dal suo JSON canonico uguale a quello dell'immagine decodificata.
inline void expect_state_round_trip(const astro::image& img) {
  ASSERT_TRUE(img.decoder_state.has_value());
  const astro::decoder_state& expected = *img.decoder_state;
  const astro::json rec = astro::raw_decoder_record(img);
  try {
    const astro::decoder_state direct = astro::decoder_state_of_record(rec);
    EXPECT_TRUE(direct == expected) << "membri diversi: " << differing_members(direct, expected);
    const astro::decoder_state persisted = astro::decoder_state_of_record(astro::parse_json(astro::canonical_json(rec)));
    EXPECT_TRUE(persisted == expected) << "dal JSON canonico, membri diversi: " << differing_members(persisted, expected);
  } catch (const astro::stop_error& e) {
    ADD_FAILURE() << "record non ricostruito: " << e.what();
  }
}

// Copia dell'oggetto senza la chiave `key`.
inline astro::json without_key(const astro::json& o, std::string_view key) {
  astro::json out = astro::json::object();
  for (const std::string& k : o.keys())
    if (k != key) out.set(k, *o.find(k));
  return out;
}

// Copia del record con `parent` (oggetto di primo livello) privato della chiave `key`.
inline astro::json without_nested_key(const astro::json& rec, std::string_view parent, std::string_view key) {
  astro::json out = astro::json::object();
  for (const std::string& k : rec.keys()) out.set(k, k == parent ? without_key(*rec.find(k), key) : *rec.find(k));
  return out;
}

// Copia dell'oggetto con la chiave `key` (presente) al valore `value`.
inline astro::json with_key(const astro::json& o, std::string_view key, const astro::json& value) {
  EXPECT_NE(o.find(std::string(key)), nullptr) << "chiave da corrompere assente: " << key;
  astro::json out = astro::json::object();
  for (const std::string& k : o.keys()) out.set(k, k == key ? value : *o.find(k));
  return out;
}

// Copia del record con il membro `key` di `parent` (oggetto di primo livello) al valore `value`.
inline astro::json with_nested_key(const astro::json& rec, std::string_view parent, std::string_view key,
                                   const astro::json& value) {
  astro::json out = astro::json::object();
  for (const std::string& k : rec.keys()) out.set(k, k == parent ? with_key(*rec.find(k), key, value) : *rec.find(k));
  return out;
}

// Insieme esatto delle chiavi di primo livello del record: nessuna mancante, nessuna in più.
template <typename Keys>
inline void expect_exact_keys(const astro::json& rec, const Keys& expected_keys) {
  ASSERT_EQ(rec.type(), astro::json::kind::object);
  std::set<std::string> expected, got;
  for (std::string_view k : expected_keys) EXPECT_TRUE(expected.insert(std::string(k)).second) << "chiave attesa ripetuta: " << k;
  for (const std::string& k : rec.keys()) got.insert(k);
  for (const std::string& k : expected)
    if (!got.count(k)) ADD_FAILURE() << "chiave assente: " << k;
  for (const std::string& k : got)
    if (!expected.count(k)) ADD_FAILURE() << "chiave non attesa: " << k;
}

inline void expect_record_malformed(const astro::json& rec, std::string_view key) {
  try {
    (void)astro::decoder_state_of_record(rec);
    ADD_FAILURE() << "nessuno stop per la chiave " << key;
  } catch (const astro::stop_error& e) {
    EXPECT_EQ(e.category(), astro::error_category::integrity) << e.what();
    EXPECT_EQ(e.code(), std::string("raw.record_malformed")) << e.what();
    std::string got;
    bool found = false;
    for (const auto& [k, v] : e.context())
      if (k == "chiave") {
        got = v;
        found = true;
      }
    EXPECT_TRUE(found) << "contesto senza `chiave`: " << e.what();
    EXPECT_EQ(got, std::string(key)) << e.what();
  }
}

}  // namespace record_roundtrip
