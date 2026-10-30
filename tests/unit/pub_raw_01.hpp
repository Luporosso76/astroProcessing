// Campioni di PUB-RAW-01 per i test di U3 (fuzz e creazioni di thread): percorsi e SHA-256 di
// docs/datasets/PUB-RAW-01.md, nell'ordine del record, sotto $ASTRO_RAW_PIXLS_DIR in sola lettura.
#pragma once

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "astro/hash.hpp"

namespace pub_raw {

struct seed_entry {
  const char* path;  // relativo a $ASTRO_RAW_PIXLS_DIR
  const char* sha256;
};

// Percorsi e SHA-256 di docs/datasets/PUB-RAW-01.md, nell'ordine del record.
const seed_entry kSeeds[] = {
    {"799/Canon - EOS 6D - RAW.CR2", "360842779d08fe4805b4a2fe4979f06fdc6da982c85c3acb181f25bcf4c9f536"},
    {"897/Nikon - D750 - 12bit 12bit compressed (Lossless) (3:2).NEF",
     "baae8dfc05a81a4a4034831a49ed99d129e797858ad876d31173a46d33b7d3ea"},
    {"1581/Sony - ILCE-7S - 14bit 14bit compressed (3:2).ARW",
     "7cc338a0abc8fdad32d61006f1f8f412297b93e040c7efba572eb2bd8f8e8be2"},
    {"1214/Fujifilm - X-A1 - 12bit 12bit uncompressed (3:2).RAF",
     "b2ddb5fe31a3b976d12961c72a2689de8742739df3d8f757614eca65f8df2a61"},
    {"1331/Olympus - E-M10 - 16bit (4:3).ORF", "d7f6ac99ee1f824e81ec0e71599f0f87f9b2e2675f7ddfdb21202281b8bdc5e0"},
    {"831/Pentax - K-x - 12bit (3:2).PEF", "51a3f22d702353b246684dca79f83c79cfdbdf9892702ae5ac55dd469e239bb6"},
    {"2571/Panasonic - DMC-GH4 - 4:3.RW2", "5ea2591972c3757596367e9a40677f9d7ccd7ce33719baeefa1ba12b9ee273a8"},
    {"3098/Fujifilm - X-E1 - 12bit 12bit uncompressed (3:2).RAF",
     "0fa01fa1a674bcfa40f659225b4e66c5bedf15d2f78407ca1966dc8f38cd0e9f"},
};
constexpr std::size_t kSeedCount = sizeof(kSeeds) / sizeof(kSeeds[0]);

inline bool read_file(const std::filesystem::path& path, std::vector<std::uint8_t>& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  std::error_code ec;
  return !out.empty() && std::filesystem::file_size(path, ec) == out.size() && !ec;
}

// Falso con `error` valorizzato: variabile assente, lettura incompleta o hash diverso dal record. Mai un salto.
inline bool load(std::size_t index, std::string& name, std::vector<std::uint8_t>& bytes, std::string& error) {
  const char* dir = std::getenv("ASTRO_RAW_PIXLS_DIR");
  if (dir == nullptr || *dir == '\0') {
    error = "ASTRO_RAW_PIXLS_DIR assente: i campioni di PUB-RAW-01 sono richiesti, il test non salta";
    return false;
  }
  if (index >= kSeedCount) {
    error = "indice del campione fuori intervallo";
    return false;
  }
  const std::filesystem::path path = std::filesystem::path(dir) / kSeeds[index].path;
  name = path.filename().string();
  if (!read_file(path, bytes)) {
    error = "lettura di " + path.string() + " fallita";
    return false;
  }
  const std::string digest = astro::sha256_hex(bytes);
  if (digest != kSeeds[index].sha256) {
    error = "SHA-256 di " + path.string() + ": letto " + digest + ", atteso " + kSeeds[index].sha256;
    return false;
  }
  return true;
}

}  // namespace pub_raw
