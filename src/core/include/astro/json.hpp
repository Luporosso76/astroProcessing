#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace astro {

// Valore JSON per i parametri canonici (RFC 8785). Interi ammessi solo entro +-2^53 (oltre: stringa);
// i numeri non finiti sono rifiutati alla costruzione; le stringhe devono essere UTF-8 valido.
class json {
 public:
  enum class kind { null, boolean, integer, number, string, array, object };

  json() = default;
  static json null() { return json(); }
  static json boolean(bool value);
  // |value| > 2^53: invalid_input dag.param_integer_range.
  static json integer(std::int64_t value);
  static json uinteger(std::uint64_t value);
  // NaN o +-Inf: invalid_input dag.param_nonfinite.
  static json number(double value);
  // UTF-8 non valido: invalid_input dag.param_invalid_utf8.
  static json string(std::string value);
  static json array();
  static json object();

  // Solo su array / oggetto (altrimenti ASTRO_ASSERT). Chiave duplicata: invalid_input dag.param_duplicate_key.
  json& push(json value);
  json& set(std::string key, json value);

  kind type() const noexcept { return kind_; }
  bool as_bool() const;
  std::int64_t as_integer() const;
  double as_number() const;
  const std::string& as_string() const;
  const std::vector<json>& items() const;
  // Chiavi in ordine di inserimento, parallele a items() per gli oggetti.
  const std::vector<std::string>& keys() const;
  const json* find(const std::string& key) const noexcept;

 private:
  kind kind_ = kind::null;
  bool bool_ = false;
  std::int64_t int_ = 0;
  double number_ = 0;
  std::string string_;
  std::vector<json> items_;
  std::vector<std::string> keys_;
};

// Serializzazione JCS (RFC 8785): chiavi ordinate per unita' di codice UTF-16, numeri secondo
// Number::toString di ECMAScript, escape minimi, nessuno spazio.
std::string canonical_json(const json& value);

// Analisi JSON (RFC 8259) rigorosa: UTF-8 valido, chiavi duplicate, profondita' > 64 e oggetti con piu' di 4096
// chiavi rifiutati. Interi entro +-2^53 diventano `integer`, ogni altro numero `number` (finito).
// Errori: invalid_input json.malformed, limit_exceeded json.depth / json.object_keys.
json parse_json(std::string_view text);

// Numero in forma ECMAScript (esposto per i vettori di prova RFC 8785 appendice B).
std::string ecmascript_number(double value);

}  // namespace astro
