#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <span>

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

  // Quota superata: budget spill.quota_exceeded (nessun byte scritto). Errori di sistema: io spill.write.
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
};

}  // namespace astro
