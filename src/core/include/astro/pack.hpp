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

// G-PACK (§3.3 punti 1-6), da chiamare nel preflight prima della prima operazione che usa il pack. Legge solo la
// directory della ricetta e i record di audit sotto `audit.evidence_root`; ricalcola ogni hash a ogni chiamata.
// Directory della ricetta e `audit.evidence_root` sono percorsi assoluti: altrimenti invalid_input
// `pack.path_not_absolute`, prima di ogni apertura.
// Ordine dei controlli: directory e manifest, forma canonica, pack_format, schema, digest e identita'; poi l'albero
// della directory in ordine di nome senza seguire link simbolici (voce non elencata, non regolare, dimensione prima
// dell'hash, hash), file elencati assenti; conteggi e indice (le righe del file dati sono verificate da
// `read_pack_cell`, non qui); licenze per componente; scopo e bande. Il primo controllo fallito e' uno stop_error con
// categoria e codice della tabella di §3.4; errori del sistema operativo: io `pack.io`.
// Conteggi: righe del file dati (dimensione / lunghezza del record, resto nullo), `count` del manifest e somma dei
// `count` dei componenti diversi: `pack.count_mismatch`. Indice: dimensione diversa da 24 byte per cella di
// `coverage`, celle diverse o in altro ordine, intervalli non contigui dalla riga 0 o oltre il file dati:
// `pack.index_invalid`; somma delle righe dell'indice diversa dal numero di righe: `pack.count_mismatch`.
verified_pack verify_pack(const pack_recipe& recipe, const pack_use& use, const pack_audit& audit);

// Regione del campo nota nel preflight: calotta ICRS di centro (ra_deg, dec_deg) e raggio radius_deg, in gradi.
struct sky_cap {
  double ra_deg = 0, dec_deg = 0, radius_deg = 0;
};

// §3.3 punto 7 sulla `coverage` del manifest verificato: ogni cella che interseca la calotta allargata del raggio
// angolare massimo di una cella all'ordine di coverage deve essere in coverage, altrimenti incompatible
// `pack.out_of_coverage`. Il controllo e' conservativo (usa il centro della cella e due raggi massimi): puo' rifiutare
// un campo coperto al bordo, mai accettare uno scoperto. Il raggio massimo e' quello di max_pixrad della libreria
// HEALPix (ipotesi: e' la distanza centro-punto piu' grande della cella). Regione con un campo non finito, raggio
// negativo o |dec| > 90: invalid_input `pack.region_invalid`.
void verify_pack_coverage(const verified_pack& pack, const sky_cap& region);

// Una riga letta: values[c] e' il valore della colonna c di manifest.columns convertito in double senza perdita
// (f64 identico, f32 allargato, u16 e u32 esatti); un'assenza e' NaN.
struct pack_row {
  std::vector<double> values;
};

// Righe della cella `cell`, lette dal data_file della ricetta attraverso l'index_file. L'indice e la dimensione del
// file dati sono riverificati su cio' che si legge (hash dell'indice, dimensione dei dati). Ogni riga e' controllata
// secondo §3.1: assenza se e solo se il bit e' 1 e il valore NaN, infiniti, bit non dichiarati: `pack.row_invalid`.
// Cella non in coverage: `pack.out_of_coverage`; una cella coperta puo' avere zero righe.
std::vector<pack_row> read_pack_cell(const pack_recipe& recipe, const verified_pack& pack, std::uint64_t cell);

}  // namespace astro
