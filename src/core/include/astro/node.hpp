#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "astro/fits.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "astro/json.hpp"

namespace astro {

constexpr const char* node_key_prefix = "astro.node.v1\n";
constexpr std::uint32_t fits_decode_algorithm_version = 1;
constexpr std::uint32_t fits_decode_serialization_version = 1;

// Ingresso ordinato di un nodo: la chiave di un padre oppure il digest di un file ("input:<sha256>:<size>").
class node_input {
 public:
  static node_input from_node(std::string input_name, const std::string& node_key);
  static node_input from_file(std::string input_name, const content_digest& digest);

  const std::string& input_name() const noexcept { return input_name_; }
  const std::string& ref() const noexcept { return ref_; }
  bool is_file() const noexcept;
  // Solo per gli ingressi file.
  content_digest file_digest() const;

 private:
  node_input(std::string input_name, std::string ref) : input_name_(std::move(input_name)), ref_(std::move(ref)) {}
  std::string input_name_;
  std::string ref_;
};

// Nodo immutabile: la chiave e' calcolata alla costruzione da tutti i campi del contratto ADR-05.
class node_spec {
 public:
  // params deve essere un oggetto. Nome operatore o backend vuoti, nomi di ingresso vuoti o duplicati:
  // invalid_input dag.bad_spec.
  node_spec(std::string operator_id, std::uint32_t algorithm_version, std::uint32_t serialization_version,
            json params, std::vector<node_input> inputs, sample_type precision, std::string backend,
            std::optional<std::uint64_t> seed);

  const std::string& operator_id() const noexcept { return operator_id_; }
  std::uint32_t algorithm_version() const noexcept { return algorithm_version_; }
  std::uint32_t serialization_version() const noexcept { return serialization_version_; }
  const json& params() const noexcept { return params_; }
  const std::vector<node_input>& inputs() const noexcept { return inputs_; }
  sample_type precision() const noexcept { return precision_; }
  const std::string& backend() const noexcept { return backend_; }
  const std::optional<std::uint64_t>& seed() const noexcept { return seed_; }

  // Oggetto con tutti i campi del contratto (stesso che entra nella chiave).
  json to_json() const;

  // JCS dell'oggetto con tutti i campi e SHA-256 esadecimale di "astro.node.v1\n" + JCS.
  const std::string& canonical() const noexcept { return canonical_; }
  const std::string& key() const noexcept { return key_; }

 private:
  std::string operator_id_;
  std::uint32_t algorithm_version_;
  std::uint32_t serialization_version_;
  json params_;
  std::vector<node_input> inputs_;
  sample_type precision_;
  std::string backend_;
  std::optional<std::uint64_t> seed_;
  std::string canonical_;
  std::string key_;
};

// Valori dichiarati come oggetto JSON canonico dei parametri; un valore con origine `header` e' un errore
// (invalid_input dag.metadata_not_declared).
json declared_metadata_json(const frame_metadata& declared);

// Nodo `fits.decode`: fits_read_options e i valori dichiarati entrano nei parametri. `declared` deve contenere
// solo valori con origine `declared` (altrimenti invalid_input dag.metadata_not_declared).
node_spec make_fits_decode_node(const content_digest& input, const fits_read_options& options,
                                const frame_metadata& declared, sample_type precision, std::string backend);

// Inverso di node_spec::to_json(): ricostruisce il nodo da un documento con esattamente i campi del contratto.
// Documento malformato: invalid_input dag.bad_spec. La chiave non e' letta dal documento: la ricalcola il chiamante.
node_spec node_from_json(const json& doc);

struct recipe_input {
  std::string input_name;
  // Identificativo locale di un'altra voce della ricetta, oppure il digest di un file.
  std::variant<std::string, content_digest> source;
};

struct recipe_entry {
  // Tutti i campi sono dichiarati dal chiamante; le versioni sono >= 1 (0: dag.bad_spec).
  recipe_entry(std::string operator_id_, std::uint32_t algorithm_version_, std::uint32_t serialization_version_,
               json params_, std::vector<recipe_input> inputs_, sample_type precision_, std::string backend_,
               std::optional<std::uint64_t> seed_)
      : operator_id(std::move(operator_id_)),
        algorithm_version(algorithm_version_),
        serialization_version(serialization_version_),
        params(std::move(params_)),
        inputs(std::move(inputs_)),
        precision(precision_),
        backend(std::move(backend_)),
        seed(seed_) {}

  std::string operator_id;
  std::uint32_t algorithm_version;
  std::uint32_t serialization_version;
  json params;
  std::vector<recipe_input> inputs;
  sample_type precision;
  std::string backend;
  std::optional<std::uint64_t> seed;
};

struct resolved_node {
  std::string local_id;
  node_spec spec;
};

// Ricetta con identificativi locali: risolta in nodi con chiave, in ordine topologico (padri prima dei figli).
class recipe_graph {
 public:
  // Identificativo duplicato: invalid_input dag.duplicate_node.
  void add(std::string local_id, recipe_entry entry);

  // Padre assente: graph dag.missing_parent. Ciclo: graph dag.cycle.
  std::vector<resolved_node> resolve() const;

 private:
  std::map<std::string, recipe_entry> entries_;
};

}  // namespace astro
