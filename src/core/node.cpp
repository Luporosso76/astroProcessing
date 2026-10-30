#include "astro/node.hpp"
#include "astro/operations.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <set>
#include <type_traits>

#include "astro/error.hpp"
#include "astro/job_probe.hpp"

namespace astro {

namespace {

constexpr const char* file_ref_prefix = "input:";

[[noreturn]] void bad_spec(const char* message, stop_error::context_list context = {}) {
  throw stop_error(error_category::invalid_input, "dag.bad_spec", message, std::move(context));
}

void check_ref(const std::string& ref) {
  if (ref.rfind(file_ref_prefix, 0) == 0) {
    const std::size_t colon = ref.find(':', 6);
    if (colon == std::string::npos || !is_sha256_hex(ref.substr(6, colon - 6)) || colon + 1 >= ref.size() ||
        !std::all_of(ref.begin() + static_cast<std::ptrdiff_t>(colon) + 1, ref.end(),
                     [](char c) { return c >= '0' && c <= '9'; })) {
      bad_spec("riferimento a file non valido", {{"ref", ref}});
    }
    return;
  }
  if (!is_sha256_hex(ref)) bad_spec("chiave del padre non valida", {{"ref", ref}});
}

template <class T>
void add_metadata(json& out, const char* name, const std::optional<metadata_value<T>>& field) {
  if (!field) return;
  if (field->source.origin != metadata_origin::declared) {
    throw stop_error(error_category::invalid_input, "dag.metadata_not_declared",
                     "il nodo fits.decode accetta solo valori dichiarati", {{"campo", name}});
  }
  if constexpr (std::is_same_v<T, double>) {
    out.set(name, json::number(field->value));
  } else if constexpr (std::is_same_v<T, std::string>) {
    out.set(name, json::string(field->value));
  } else {
    out.set(name, json::integer(field->value));
  }
}


const char* precision_name(sample_type type) {
  switch (type) {
    case sample_type::f32: return "f32";
    case sample_type::f64: return "f64";
  }
  ASTRO_ASSERT(false);
  return "";
}

json optional_offset(const std::optional<std::uint8_t>& v) {
  return v ? json::uinteger(*v) : json::null();
}

}  // namespace

json declared_metadata_json(const frame_metadata& m) {
  json out = json::object();
  add_metadata(out, "exposure_s", m.exposure_s);
  add_metadata(out, "gain", m.gain);
  add_metadata(out, "conversion_gain_e_per_adu", m.conversion_gain_e_per_adu);
  add_metadata(out, "offset_adu", m.offset_adu);
  add_metadata(out, "temperature_c", m.temperature_c);
  add_metadata(out, "filter", m.filter);
  add_metadata(out, "camera", m.camera);
  add_metadata(out, "imagetyp", m.imagetyp);
  add_metadata(out, "saturation_adu", m.saturation_adu);
  add_metadata(out, "pedestal_adu", m.pedestal_adu);
  add_metadata(out, "roi_x", m.roi_x);
  add_metadata(out, "roi_y", m.roi_y);
  add_metadata(out, "readout_mode", m.readout_mode);
  add_metadata(out, "row_order", m.row_order);
  return out;
}

node_input node_input::from_node(std::string input_name, const std::string& node_key) {
  check_ref(node_key);
  if (node_key.rfind(file_ref_prefix, 0) == 0) bad_spec("from_node richiede una chiave di nodo", {{"ref", node_key}});
  return node_input(std::move(input_name), node_key);
}

node_input node_input::from_file(std::string input_name, const content_digest& digest) {
  std::string ref = std::string(file_ref_prefix) + digest.sha256 + ":" + std::to_string(digest.size_bytes);
  check_ref(ref);
  return node_input(std::move(input_name), std::move(ref));
}

bool node_input::is_file() const noexcept { return ref_.rfind(file_ref_prefix, 0) == 0; }

content_digest node_input::file_digest() const {
  ASTRO_ASSERT(is_file());
  const std::size_t colon = ref_.find(':', 6);
  return content_digest{ref_.substr(6, colon - 6), std::stoull(ref_.substr(colon + 1))};
}

node_spec::node_spec(std::string operator_id, std::uint32_t algorithm_version,
                     std::uint32_t serialization_version, json params, std::vector<node_input> inputs,
                     sample_type precision, std::string backend, std::optional<std::uint64_t> seed)
    : operator_id_(std::move(operator_id)),
      algorithm_version_(algorithm_version),
      serialization_version_(serialization_version),
      params_(std::move(params)),
      inputs_(std::move(inputs)),
      precision_(precision),
      backend_(std::move(backend)),
      seed_(seed) {
  job_probe::notify("node.build", operator_id_);
  if (operator_id_.empty()) bad_spec("operator_id vuoto");
  if (backend_.empty()) bad_spec("backend vuoto");
  if (algorithm_version_ == 0 || serialization_version_ == 0) bad_spec("versione 0 non ammessa");
  if (params_.type() != json::kind::object) bad_spec("params deve essere un oggetto");
  std::set<std::string> names;
  for (const auto& in : inputs_) {
    check_ref(in.ref());
    if (in.input_name().empty() || !names.insert(in.input_name()).second) {
      bad_spec("nome di ingresso vuoto o duplicato", {{"input_name", in.input_name()}});
    }
  }
  canonical_ = canonical_json(to_json());
  const std::string preimage = std::string(node_key_prefix) + canonical_;
  key_ = sha256_hex({reinterpret_cast<const std::uint8_t*>(preimage.data()), preimage.size()});
}

json node_spec::to_json() const {
  json in = json::array();
  for (std::size_t ordinal = 0; ordinal < inputs_.size(); ++ordinal) {
    json item = json::object();
    item.set("ordinal", json::uinteger(ordinal));
    item.set("input_name", json::string(inputs_[ordinal].input_name()));
    item.set("ref", json::string(inputs_[ordinal].ref()));
    in.push(std::move(item));
  }
  json doc = json::object();
  doc.set("operator_id", json::string(operator_id_));
  doc.set("algorithm_version", json::uinteger(algorithm_version_));
  doc.set("serialization_version", json::uinteger(serialization_version_));
  doc.set("params", params_);
  doc.set("inputs", std::move(in));
  doc.set("precision", json::string(precision_name(precision_)));
  doc.set("backend", json::string(backend_));
  doc.set("seed", seed_ ? json::string(std::to_string(*seed_)) : json::null());
  return doc;
}

node_spec make_fits_decode_node(const content_digest& input, const fits_read_options& options,
                                const frame_metadata& declared, sample_type precision, std::string backend) {
  static_assert(sizeof(fits_read_options) == 2 * sizeof(std::optional<std::uint8_t>) +
                                                  sizeof(std::optional<process_phase>) +
                                                  sizeof(std::optional<linearity_class>),
                "nuovo campo in fits_read_options: aggiungerlo ai parametri del nodo fits.decode");
  json read_options = json::object();
  read_options.set("declared_bayer_x_offset", optional_offset(options.declared_bayer_x_offset));
  read_options.set("declared_bayer_y_offset", optional_offset(options.declared_bayer_y_offset));
  // Solo se dichiarate: le chiavi dei nodi senza fase ne' linearita' dichiarate non cambiano (ADR-05, R1).
  if (options.declared_phase) read_options.set("declared_phase", json::string(to_string(*options.declared_phase)));
  if (options.declared_linearity) {
    read_options.set("declared_linearity", json::string(to_string(*options.declared_linearity)));
  }
  json params = json::object();
  params.set("fits_read_options", std::move(read_options));
  params.set("declared_metadata", declared_metadata_json(declared));
  std::vector<node_input> inputs;
  inputs.push_back(node_input::from_file("file", input));
  return node_spec("fits.decode", fits_decode_algorithm_version, fits_decode_serialization_version,
                   std::move(params), std::move(inputs), precision, std::move(backend), std::nullopt);
}

namespace {

const json& require_field(const json& doc, const char* name, json::kind kind) {
  const json* field = doc.find(name);
  if (field == nullptr || field->type() != kind) bad_spec("campo del nodo assente o di tipo errato", {{"campo", name}});
  return *field;
}

std::uint32_t require_version(const json& doc, const char* name) {
  const std::int64_t v = require_field(doc, name, json::kind::integer).as_integer();
  if (v < 1 || v > std::numeric_limits<std::uint32_t>::max()) bad_spec("versione fuori intervallo", {{"campo", name}});
  return static_cast<std::uint32_t>(v);
}

node_input input_from_ref(const std::string& name, const std::string& ref) {
  if (ref.rfind(file_ref_prefix, 0) != 0) return node_input::from_node(name, ref);
  const std::size_t colon = ref.find(':', 6);
  if (colon == std::string::npos) bad_spec("riferimento a file non valido", {{"ref", ref}});
  std::uint64_t size = 0;
  const char* first = ref.data() + colon + 1;
  const char* last = ref.data() + ref.size();
  const auto r = std::from_chars(first, last, size);
  if (r.ec != std::errc() || r.ptr != last) bad_spec("dimensione del file non valida", {{"ref", ref}});
  return node_input::from_file(name, content_digest{ref.substr(6, colon - 6), size});
}

std::optional<std::uint64_t> seed_from_json(const json& field) {
  if (field.type() == json::kind::null) return std::nullopt;
  if (field.type() != json::kind::string) bad_spec("seed non valido");
  const std::string& text = field.as_string();
  std::uint64_t seed = 0;
  const auto r = std::from_chars(text.data(), text.data() + text.size(), seed);
  if (text.empty() || r.ec != std::errc() || r.ptr != text.data() + text.size() || std::to_string(seed) != text) {
    bad_spec("seed non valido", {{"seed", text}});
  }
  return seed;
}

}  // namespace

node_spec node_from_json(const json& doc) {
  if (doc.type() != json::kind::object || doc.keys().size() != 8) bad_spec("il nodo deve essere un oggetto di 8 campi");
  const json& in = require_field(doc, "inputs", json::kind::array);
  std::vector<node_input> inputs;
  for (std::size_t i = 0; i < in.items().size(); ++i) {
    const json& item = in.items()[i];
    if (item.type() != json::kind::object || item.keys().size() != 3 ||
        require_field(item, "ordinal", json::kind::integer).as_integer() != static_cast<std::int64_t>(i)) {
      bad_spec("ingresso del nodo non valido", {{"posizione", std::to_string(i)}});
    }
    inputs.push_back(input_from_ref(require_field(item, "input_name", json::kind::string).as_string(),
                                    require_field(item, "ref", json::kind::string).as_string()));
  }
  const std::string& precision = require_field(doc, "precision", json::kind::string).as_string();
  if (precision != "f32" && precision != "f64") bad_spec("precisione sconosciuta", {{"precisione", precision}});
  const json* seed = doc.find("seed");
  if (seed == nullptr) bad_spec("campo del nodo assente o di tipo errato", {{"campo", "seed"}});
  return node_spec(require_field(doc, "operator_id", json::kind::string).as_string(),
                   require_version(doc, "algorithm_version"), require_version(doc, "serialization_version"),
                   require_field(doc, "params", json::kind::object), std::move(inputs),
                   precision == "f32" ? sample_type::f32 : sample_type::f64,
                   require_field(doc, "backend", json::kind::string).as_string(), seed_from_json(*seed));
}

void recipe_graph::add(std::string local_id, recipe_entry entry) {
  if (!entries_.emplace(local_id, std::move(entry)).second) {
    throw stop_error(error_category::invalid_input, "dag.duplicate_node", "identificativo locale duplicato",
                     {{"id", local_id}});
  }
}

namespace {

struct resolver {
  const std::map<std::string, recipe_entry>& entries;
  std::map<std::string, std::string> keys;
  std::set<std::string> in_progress;
  std::vector<resolved_node> order;

  void visit(const std::string& id, const std::string& child) {
    const auto it = entries.find(id);
    if (it == entries.end()) {
      throw stop_error(error_category::graph, "dag.missing_parent", "padre assente dalla ricetta",
                       {{"padre", id}, {"figlio", child}});
    }
    if (keys.count(id) != 0) return;
    if (!in_progress.insert(id).second) {
      throw stop_error(error_category::graph, "dag.cycle", "la ricetta contiene un ciclo", {{"nodo", id}});
    }
    const recipe_entry& e = it->second;
    std::vector<node_input> inputs;
    for (const auto& in : e.inputs) {
      if (const auto* parent = std::get_if<std::string>(&in.source)) {
        visit(*parent, id);
        inputs.push_back(node_input::from_node(in.input_name, keys.at(*parent)));
      } else {
        inputs.push_back(node_input::from_file(in.input_name, std::get<content_digest>(in.source)));
      }
    }
    node_spec spec(e.operator_id, e.algorithm_version, e.serialization_version, e.params, std::move(inputs),
                   e.precision, e.backend, e.seed);
    keys[id] = spec.key();
    in_progress.erase(id);
    order.push_back(resolved_node{id, std::move(spec)});
  }
};

}  // namespace

std::vector<resolved_node> recipe_graph::resolve() const {
  resolver r{entries_, {}, {}, {}};
  for (const auto& [id, entry] : entries_) {
    (void)entry;
    r.visit(id, "");
  }
  return std::move(r.order);
}

}  // namespace astro
