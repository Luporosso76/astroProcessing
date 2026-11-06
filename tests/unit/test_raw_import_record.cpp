// U4 (M3.a): T-M3A-RAW-12 (chiave del nodo per campo del record) e T-M3A-RAW-13 (iniezione di focale, 35 mm e GPS dopo
// `open_buffer`). Usa simboli di prodotto che il contratto assegna a U4 e che ancora non esistono; il file sta in un
// target a parte (opzione CMake ASTRO_U4_RECORD_TESTS) che non rompe il resto della build:
//   - astro/raw_import_job.hpp: `json raw_decoder_record(const image&)` (record di ADR-08.b §2.9, chiavi in
//     tests/unit/raw_record_keys.hpp) e `node_spec make_raw_import_node(const content_digest&, const json& record)`
//     (il record entra nei parametri del nodo, quindi nella chiave: §2.9 «la chiave del nodo include»; il job decodifica
//     nella preparazione e costruisce il nodo con questa funzione, vedi raw_record_keys.hpp);
//   - raw_probe_fields::focal_len, focal_length_in_35mm_format (float) e parsed_gps_present (bool): i campi che LibRaw
//     espone dopo `open_buffer` e che il decoder non deve usare (§2.6, emendamento L4).
// Valori attesi: dal contratto; nessuno calcolato dal codice sotto test.
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "astro/job_types.hpp"
#include "astro/json.hpp"
#include "astro/raw_decode.hpp"
#include "astro/raw_import_job.hpp"
#include "conformance_inputs.hpp"
#include "raw_record_keys.hpp"

namespace {

using namespace astro;
namespace fx = astro_fixture;
namespace rk = raw_record;
namespace fs = std::filesystem;

constexpr std::uint32_t raw_w = 64, raw_h = 48;

fx::dng_spec base_spec() {
  fx::dng_spec s;
  s.width = raw_w;
  s.height = raw_h;
  s.cfa = {0, 1, 1, 2};
  s.white = 4000;
  s.exposure_time = std::make_pair(std::uint32_t{60}, std::uint32_t{1});
  s.iso = 100;
  s.datetime_original = "2026:10:30 12:34:56";
  std::vector<std::uint16_t> v(raw_w * raw_h);
  for (std::uint32_t r = 0; r < raw_h; ++r)
    for (std::uint32_t c = 0; c < raw_w; ++c) v[r * raw_w + c] = static_cast<std::uint16_t>(100 + (r * 37 + c * 11) % 3500);
  s.pixels = fx::pixels_u16(v);
  return s;
}

struct decoded {
  std::vector<std::uint8_t> bytes;
  image img;
  json record;
  std::string key;
};

// Decodifica con i ganci indicati e costruisce record e chiave sul digest del file.
decoded decode(const fx::dng_spec& spec, const raw_decode_hooks& hooks = {}) {
  decoded d;
  d.bytes = fx::make_dng(spec);
  d.img = decode_raw_buffer(d.bytes, hooks);
  d.record = raw_decoder_record(d.img);
  d.key = make_raw_import_node(digest_of(d.bytes), d.record).key();
  return d;
}

const json* member(const json& o, std::string_view key) { return o.type() == json::kind::object ? o.find(std::string(key)) : nullptr; }

// Copia del record con un campo sostituito da un valore diverso dello stesso significato di «cambiato».
json with_field_changed(const json& record, const std::string& key) {
  json out = json::object();
  for (const std::string& k : record.keys()) {
    const json& v = *record.find(k);
    if (k != key) {
      out.set(k, v);
      continue;
    }
    switch (v.type()) {
      case json::kind::string: out.set(k, json::string(v.as_string() + "x")); break;
      case json::kind::integer: out.set(k, json::integer(v.as_integer() + 1)); break;
      case json::kind::boolean: out.set(k, json::boolean(!v.as_bool())); break;
      default: out.set(k, json::string("cambiato")); break;  // null, array, oggetto, numero
    }
  }
  return out;
}

// ---------------------------------------------------------------- T-M3A-RAW-12

TEST(RawImportRecord, T_M3A_RAW_12_two_decodes_of_the_same_file_are_identical) {
  const decoded a = decode(base_spec());
  const decoded b = decode(base_spec());
  EXPECT_EQ(canonical_json(a.record), canonical_json(b.record));
  EXPECT_EQ(a.key, b.key);
  EXPECT_EQ(sha256_hex(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(a.img.samples.data()),
                                                     a.img.samples.size())),
            sha256_hex(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(b.img.samples.data()),
                                                     b.img.samples.size())));
}

// Ogni campo del record entra nella chiave: un campo alla volta cambiato (stesso digest del file, stessa ricetta), la
// chiave cambia. Copre versione di LibRaw, nero, linearizzazione e digest della curva, crop, parità dei margini,
// pattern, stato, ora di acquisizione e tutti gli altri campi di §2.9.
TEST(RawImportRecord, T_M3A_RAW_12_every_record_field_changes_the_node_key) {
  const decoded base = decode(base_spec());
  const content_digest digest = digest_of(base.bytes);
  ASSERT_EQ(base.record.type(), json::kind::object);
  for (std::string_view key : rk::all_keys) {
    SCOPED_TRACE(std::string(key));
    ASSERT_NE(member(base.record, key), nullptr) << "campo del record assente";
    const std::string changed = make_raw_import_node(digest, with_field_changed(base.record, std::string(key))).key();
    EXPECT_NE(changed, base.key) << "il campo non entra nella chiave del nodo";
  }
}

// Limite dichiarato: versione di LibRaw, stato del dato e parità dei margini (per i DNG vale sempre (0, 0), §2.3) non si cambiano per iniezione nel decoder (U12 verifica la
// versione con ASTRO_ASSERT, lo stato è sempre raw): per questi tre campi vale solo la prova a livello di record sopra.
// Le stesse variazioni per iniezione sul decoder: nero (dopo `unpack`), ora, tabella di linearizzazione e area attiva
// cambiano il record e la chiave, sul medesimo digest del file.
TEST(RawImportRecord, T_M3A_RAW_12_decoder_injections_change_record_and_key) {
  const decoded base = decode(base_spec());
  const content_digest digest = digest_of(base.bytes);
  const auto key_of = [&](const decoded& d) { return make_raw_import_node(digest, d.record).key(); };
  {  // nero per canale iniettato dopo unpack (§2.4: cblack[0..3])
    raw_decode_hooks h;
    h.after_unpack = [](raw_unpack_fields& f) { f.cblack = {1, 10, 0, 10}; };
    const decoded d = decode(base_spec(), h);
    EXPECT_NE(canonical_json(d.record), canonical_json(base.record));
    EXPECT_NE(key_of(d), base.key);
  }
  {  // avviso ammesso iniettato: il record lo registra
    raw_decode_hooks h;
    h.after_unpack = [](raw_unpack_fields& f) { f.process_warnings = 0x10000; };
    const decoded d = decode(base_spec(), h);
    EXPECT_NE(key_of(d), base.key);
  }
  {  // ora di acquisizione diversa nel file
    fx::dng_spec s = base_spec();
    s.datetime_original = "2026:10:30 12:34:57";
    EXPECT_NE(key_of(decode(s)), base.key);
  }
  {  // area attiva con origine dispari (1, 1): crop e pattern diversi, margini portati al pari (parità 0)
    fx::dng_spec s = base_spec();
    s.active_area = fx::dng_active_area{1, 1, 33, 41};
    EXPECT_NE(key_of(decode(s)), base.key);
  }
  {  // seconda tabella non identica: stesso `linearization`, stesso bianco, un solo valore interno diverso: cambia il digest
    std::vector<std::uint16_t> a(4096), b;
    for (std::uint32_t i = 0; i < a.size(); ++i) a[i] = static_cast<std::uint16_t>(3 * i + 5);
    b = a;
    b[100] = static_cast<std::uint16_t>(b[100] + 1);
    fx::dng_spec sa = base_spec(), sb = base_spec();
    sa.linearization = a, sa.white = 12290;
    sb.linearization = b, sb.white = 12290;
    EXPECT_NE(key_of(decode(sa)), key_of(decode(sb)));
  }
  {  // area attiva con origine (2, 2): crop diverso
    fx::dng_spec s = base_spec();
    s.active_area = fx::dng_active_area{2, 2, 34, 42};
    EXPECT_NE(key_of(decode(s)), base.key);
  }
  {  // larghezza e altezza del buffer grezzo diverse (§2.9, emendamento U8b): il record porta il valore, la chiave cambia
    for (const bool wide : {true, false}) {
      fx::dng_spec s = base_spec();
      const std::uint32_t w = wide ? raw_w + 2 : raw_w, h = wide ? raw_h : raw_h + 2;
      s.width = w;
      s.height = h;
      std::vector<std::uint16_t> v(w * h);
      for (std::uint32_t r = 0; r < h; ++r)
        for (std::uint32_t c = 0; c < w; ++c) v[r * w + c] = static_cast<std::uint16_t>(100 + (r * 37 + c * 11) % 3500);
      s.pixels = fx::pixels_u16(v);
      const decoded d = decode(s);
      EXPECT_EQ(member(d.record, rk::raw_width)->as_integer(), static_cast<std::int64_t>(w));
      EXPECT_EQ(member(d.record, rk::raw_height)->as_integer(), static_cast<std::int64_t>(h));
      EXPECT_NE(key_of(d), base.key);
    }
  }
  {  // indice colore diverso (pattern bggr): cfa_color_index cambia nel record e nella chiave
    fx::dng_spec s = base_spec();
    s.cfa = {2, 1, 1, 0};
    const decoded d = decode(s);
    EXPECT_NE(canonical_json(*member(d.record, rk::cfa_color_index)), canonical_json(*member(base.record, rk::cfa_color_index)));
    EXPECT_NE(key_of(d), base.key);
  }
  {  // pattern diverso
    fx::dng_spec s = base_spec();
    s.cfa = {2, 1, 1, 0};
    EXPECT_NE(key_of(decode(s)), base.key);
  }
  {  // tabella di linearizzazione non identica
    fx::dng_spec s = base_spec();
    std::vector<std::uint16_t> t(4096);
    for (std::uint32_t i = 0; i < t.size(); ++i) t[i] = static_cast<std::uint16_t>(3 * i + 5);
    s.linearization = t;
    s.white = 12290;
    EXPECT_NE(key_of(decode(s)), base.key);
  }
}

// ---------------------------------------------------------------- T-M3A-RAW-13

// `other.focal_len`, `FocalLengthIn35mmFormat` e `other.parsed_gps` valorizzati dopo `open_buffer` non producono sensore,
// dimensione del pixel, scala pixel o WCS: stesso record e stessa chiave di una decodifica senza iniezione, chiavi `null`
// con `absent_reason`, nessun campo di `frame_metadata` oltre quelli di §2.6.
TEST(RawImportRecord, T_M3A_RAW_13_injected_focal_length_and_gps_never_become_sensor_fields) {
  const decoded plain = decode(base_spec());
  raw_decode_hooks h;
  h.after_open = [](raw_probe_fields& f) {
    f.focal_len = 50.0f;
    f.focal_length_in_35mm_format = 75.0f;
    f.parsed_gps_present = true;
  };
  const decoded injected = decode(base_spec(), h);
  for (std::string_view key : {rk::sensor, rk::pixel_size_um, rk::pixel_scale_arcsec, rk::wcs}) {
    const json* v = member(injected.record, key);
    ASSERT_NE(v, nullptr) << key;
    EXPECT_EQ(v->type(), json::kind::null) << key;
  }
  const json* reason = member(injected.record, rk::absent_reason);
  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(reason->as_string(), "not_provided_by_decoder");
  EXPECT_EQ(canonical_json(injected.record), canonical_json(plain.record));
  EXPECT_EQ(injected.key, plain.key);
  const frame_metadata& m = injected.img.metadata;
  EXPECT_FALSE(m.conversion_gain_e_per_adu.has_value());
  EXPECT_FALSE(m.offset_adu.has_value());
  EXPECT_FALSE(m.temperature_c.has_value());
  EXPECT_FALSE(m.filter.has_value());
  EXPECT_FALSE(m.imagetyp.has_value());
  EXPECT_FALSE(m.pedestal_adu.has_value());
  EXPECT_FALSE(m.roi_x.has_value());
  EXPECT_FALSE(m.roi_y.has_value());
  EXPECT_FALSE(m.readout_mode.has_value());
  EXPECT_FALSE(m.row_order.has_value());
  EXPECT_EQ(m, plain.img.metadata);
}

// ---------------------------------------------------------------- chiave del nodo sul percorso del prodotto

// Il job riuscito costruisce il nodo con `make_raw_import_node(digest, record)`: la chiave per campo provata sopra vale
// per il percorso del prodotto, perché `node_key` in provenance coincide con quella ricavata dal record in `details.decoder`.
TEST(RawImportRecord, T_M3A_RAW_12_job_node_key_equals_key_from_record_and_digest) {
  const job_type_entry* entry = nullptr;
  for (const job_type_entry& e : registered_job_types()) {
    if (std::string(e.operator_id) == "io.raw_import") entry = &e;
  }
  ASSERT_NE(entry, nullptr) << "tipo io.raw_import assente dal registro";
  const fs::path dir = fs::absolute(fs::path(ASTRO_TEST_TMP) / "raw_import_record" / "job_key");
  fs::remove_all(dir);
  fs::create_directories(dir / "scratch");
  const std::vector<std::uint8_t> bytes = fx::make_dng(base_spec());
  const fs::path file = dir / "input.dng";
  std::ofstream(file, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  const content_digest digest = digest_of(bytes);
  auto store = sqlite_dag_store::create(dir / "store");
  const job_type_inputs inputs{{"file", job_type_input{file.string(), digest}}};
  const minimal_job job = entry->make(inputs, entry->variants.at(0).name, store->root());
  store->put_node(job.node);
  store->create_slot("s", job.node.key());
  resource_config res;
  res.scratch = dir_quota{dir / "scratch", std::uint64_t{64} << 20};
  res.worker_count = 2;
  res.tile_rows = 1;
  memory_pool pool(std::uint64_t{256} << 20);
  cancel_token cancel;
  const job_outcome out = job.run(job_type_env{*store, "s", pool, cancel, std::nullopt, res, "ckpt", false});
  ASSERT_EQ(out.exit_code, 0);
  std::optional<json> provenance;
  for (const artifact_record& a : store->artifacts_for(job.node.key())) {
    if (a.output_role != "provenance") continue;
    std::ifstream in(store->artifact_path(a.rel_path), std::ios::binary);
    std::ostringstream o;
    o << in.rdbuf();
    provenance = parse_json(o.str());
  }
  ASSERT_TRUE(provenance.has_value());
  const json* details = member(*provenance, "details");
  ASSERT_NE(details, nullptr);
  const json* record = member(*details, "decoder");
  ASSERT_NE(record, nullptr);
  const json* node_key = member(*provenance, "node_key");
  ASSERT_NE(node_key, nullptr);
  ASSERT_EQ(node_key->type(), json::kind::string);
  EXPECT_EQ(node_key->as_string(), make_raw_import_node(digest, *record).key());
}

}  // namespace
