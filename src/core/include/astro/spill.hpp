#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <span>
#include <string>

#include "astro/fault_injector.hpp"

namespace astro {

// Spill dichiarato dal job: directory di scratch e quota dei byte logici scritti. Senza spill dichiarato
// non si scrive mai su disco per mancanza di memoria.
struct spill_config {
  std::filesystem::path scratch_dir;
  std::uint64_t quota_bytes = 0;
};

class spill_area;

// File anonimo (O_TMPFILE) dentro la directory di scratch: sparisce con il processo o alla distruzione.
// La quota conta l'estensione logica massima raggiunta, buchi compresi.
class spill_file {
 public:
  spill_file(spill_file&& other) noexcept;
  spill_file& operator=(spill_file&&) = delete;
  spill_file(const spill_file&) = delete;
  spill_file& operator=(const spill_file&) = delete;
  ~spill_file();

  // Quota superata: budget spill.quota_exceeded (nessun byte scritto). Errori di sistema: io spill.write con l'errno.
  // Una scrittura che estende il file e' esclusiva fino all'esito; se fallisce, estensione e quota restano quelle dei
  // byte scritti davvero (anche una scrittura corta).
  void write(std::uint64_t offset, std::span<const std::byte> data);
  // Oltre l'estensione scritta o lettura corta: integrity spill.short_read. Errori di sistema: io spill.read.
  void read(std::uint64_t offset, std::span<std::byte> out) const;

  std::uint64_t size_bytes() const noexcept { return size_.load(std::memory_order_acquire); }
  // Byte effettivamente allocati su disco (st_blocks), distinti dall'estensione logica.
  std::uint64_t allocated_bytes() const;
  // Byte del file presenti nella page cache (mincore). La page cache non e' parte del budget del pool.
  std::uint64_t resident_bytes() const;
  // Scrive su disco e chiede al kernel di liberare la page cache dell'intervallo. Non cambia il contenuto.
  void drop_cache(std::uint64_t offset, std::uint64_t length) const;

 private:
  friend class spill_area;
  spill_file(spill_area& area, int fd) noexcept : area_(&area), fd_(fd) {}

  spill_area* area_;
  int fd_;
  std::atomic<std::uint64_t> size_{0};
  std::mutex grow_mutex_;  // serializza crescita, addebito della quota e aggiornamento di size_
};

class spill_area {
 public:
  // La directory deve esistere, non essere un symlink e accettare O_TMPFILE: altrimenti io spill.scratch_unusable.
  explicit spill_area(const spill_config& config);
  spill_area(const spill_area&) = delete;
  spill_area& operator=(const spill_area&) = delete;
  // Tutti gli spill_file devono essere gia' stati distrutti.
  ~spill_area();

  // File posseduto da chi lo riceve: va distrutto prima dell'area.
  spill_file create_file();
  // File posseduto dall'area e condiviso fra i task del job: vive fino alla distruzione dell'area.
  spill_file& create_shared_file();

  // Come create_shared_file, ma con nome nella directory di scratch: resta su disco dopo la distruzione dell'area
  // (staging di un job con checkpoint, ADR-06 estensione R3.2). `existing` = false: creato o troncato; true: riaperto
  // e la sua estensione addebitata alla quota. Nome [A-Za-z0-9._-]{1,128} diverso da . e .. (invalid_input
  // spill.staging_name); symlink, non regolare o st_nlink != 1: security spill.staging_unsafe; assente con `existing`:
  // metadata_missing sched.checkpoint_missing; altri errori: io spill.staging_open.
  spill_file& open_named_file(const std::string& name, bool existing);

  // Solo test: chiamato con "spill.write" prima di ogni pwrite; un valore diverso da zero e' l'errno della scrittura.
  void set_fault_injector(fault_injector injector) { fault_ = std::move(injector); }

  std::uint64_t quota_bytes() const noexcept { return quota_bytes_; }
  std::uint64_t used_bytes() const noexcept { return used_.load(std::memory_order_acquire); }
  std::uint64_t peak_bytes() const noexcept { return peak_.load(std::memory_order_acquire); }

 private:
  friend class spill_file;
  void charge(std::uint64_t bytes);
  void uncharge(std::uint64_t bytes) noexcept;

  int dir_fd_ = -1;
  std::uint64_t quota_bytes_;
  std::atomic<std::uint64_t> used_{0};
  std::atomic<std::uint64_t> peak_{0};
  std::atomic<std::size_t> open_files_{0};
  std::mutex shared_mutex_;
  std::deque<spill_file> shared_;
  fault_injector fault_;
};

// Rimuove il file nominato `name` da `dir` (assente: nessun errore). Errori: io spill.staging_remove.
void remove_named_spill_file(const std::filesystem::path& dir, const std::string& name);

}  // namespace astro
