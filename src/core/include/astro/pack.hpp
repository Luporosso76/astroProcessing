#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "astro/json.hpp"

namespace astro {

// Catalog pack (docs/contracts/ADR-08-decoder-pack.md §3). La ricetta nomina un solo pack con identita' attesa e un
// solo percorso della directory: nessun percorso di ricerca, nessun pack di default, nessun download (§3.2).
struct pack_recipe {
  std::string pack_id;
  std::string pack_version;
  std::string pack_digest;  // SHA-256 dei byte canonici di manifest.json
  std::string directory;
};

// Uso del pack da parte dell'operazione: scopo (`astrometry` per C-18, `photometry` per C-16) e bande richieste.
struct pack_use {
  std::string purpose;
  std::vector<std::string> bands;
};

// Radice sotto cui `license_evidence` nomina il record di audit `docs/datasets/PUB-*.md` (§4 punto 2).
struct pack_audit {
  std::string evidence_root;
};

struct pack_band {
  std::string name, system;
};

// `license`: identificativo SPDX o LicenseRef-public-domain; `attribution` e' l'unico campo che puo' essere vuoto.
struct pack_component {
  std::string name, source, release, license, license_file, license_evidence, attribution;
  std::vector<pack_band> bands;
  std::string frame, epoch, selection;  // frame: ICRS
  std::uint64_t count = 0;
};

// Colonna del file dati; null_bit presente se e solo se nullable (solo f32/f64), indice di bit in `flags`.
struct pack_column {
  std::string name, type, unit, component;  // type: f64, f32, u32, u16
  bool nullable = false;
  std::optional<std::uint32_t> null_bit;
};

struct pack_flag_bit {
  std::uint32_t index = 0;
  std::string meaning;
};

struct pack_file {
  std::string path;  // relativo alla directory del pack, senza `.` o `..`
  std::uint64_t size_bytes = 0;
  std::string sha256;
};

struct pack_builder {
  std::string name, version;
  std::vector<std::string> inputs_sha256;
};

// Manifest di pack_format 1 (§3.1, chiavi dei sotto-oggetti in §3.6).
struct pack_manifest {
  std::uint32_t pack_format = 0;
  std::string pack_id, pack_version;
  std::vector<std::string> purpose;
  std::vector<pack_component> components;
  std::vector<pack_column> columns;
  std::vector<pack_flag_bit> flag_bits;
  std::string data_file, index_file;
  std::uint32_t coverage_order = 0;
  std::vector<std::uint64_t> coverage_cells;  // HEALPix NESTED, crescenti
  std::uint32_t index_order = 0;
  std::vector<pack_file> files;
  std::uint64_t count = 0;
  pack_builder builder;
};

struct verified_pack {
  std::string pack_digest;
  pack_manifest manifest;
  // {pack_digest, pack_id, pack_version}: va nei `details` della provenance del job che usa il pack.
  json provenance;
};

// G-PACK (§3.3 punti 1-4 e 6), da chiamare nel preflight prima della prima operazione che usa il pack. Legge solo la
// directory della ricetta e i record di audit sotto `audit.evidence_root`; ricalcola ogni hash a ogni chiamata.
// Ordine dei controlli: directory e manifest, forma canonica, pack_format, schema, digest e identita'; poi l'albero
// della directory in ordine di nome senza seguire link simbolici (voce non elencata, non regolare, dimensione prima
// dell'hash, hash), file elencati assenti; licenze per componente; scopo e bande. Il primo controllo fallito
// e' uno stop_error con categoria e codice della tabella di §3.4; errori del sistema operativo: io `pack.io`.
// Conteggi, indice, righe e copertura (§3.3 punti 5 e 7) non sono verificati qui.
verified_pack verify_pack(const pack_recipe& recipe, const pack_use& use, const pack_audit& audit);

}  // namespace astro
