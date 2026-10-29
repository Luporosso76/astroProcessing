#include "astro/spill.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <vector>

#include "astro/error.hpp"
#include "sqlite_internal.hpp"

namespace astro {

using namespace sqlint;

namespace {

[[noreturn]] void unusable(const std::filesystem::path& dir, const char* why, int err) {
  throw_errno(error_category::io, "spill.scratch_unusable", why, err, {{"scratch_dir", dir.string()}});
}

}  // namespace

spill_area::spill_area(const spill_config& config) : quota_bytes_(config.quota_bytes) {
  dir_fd_ = ::open(config.scratch_dir.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (dir_fd_ < 0) unusable(config.scratch_dir, "directory di scratch assente, symlink o non directory", errno);
  const int probe = ::openat(dir_fd_, ".", O_TMPFILE | O_RDWR | O_CLOEXEC, 0600);
  if (probe < 0) {
    const int err = errno;
    ::close(dir_fd_);
    dir_fd_ = -1;
    unusable(config.scratch_dir, "la directory di scratch non accetta un file O_TMPFILE", err);
  }
  ::close(probe);
}

spill_area::~spill_area() {
  shared_.clear();
  ASTRO_ASSERT(open_files_.load() == 0);
  if (dir_fd_ >= 0) ::close(dir_fd_);
}

spill_file spill_area::create_file() {
  const int fd = ::openat(dir_fd_, ".", O_TMPFILE | O_RDWR | O_CLOEXEC, 0600);
  if (fd < 0) throw_errno(error_category::io, "spill.scratch_unusable", "creazione del file di spill fallita", errno);
  open_files_.fetch_add(1);
  return spill_file(*this, fd);
}

spill_file& spill_area::create_shared_file() {
  spill_file file = create_file();
  std::lock_guard lock(shared_mutex_);
  shared_.push_back(std::move(file));
  return shared_.back();
}

void spill_area::charge(std::uint64_t bytes) {
  std::uint64_t cur = used_.load(std::memory_order_relaxed);
  do {
    if (bytes > quota_bytes_ - std::min(cur, quota_bytes_)) {
      throw stop_error(error_category::budget, "spill.quota_exceeded", "quota dello spill superata",
                       {{"quota", std::to_string(quota_bytes_)}, {"usati", std::to_string(cur)},
                        {"richiesti", std::to_string(bytes)}});
    }
  } while (!used_.compare_exchange_weak(cur, cur + bytes, std::memory_order_acq_rel));
  const std::uint64_t now = cur + bytes;
  std::uint64_t peak = peak_.load(std::memory_order_relaxed);
  while (peak < now && !peak_.compare_exchange_weak(peak, now, std::memory_order_acq_rel)) {
  }
}

void spill_area::uncharge(std::uint64_t bytes) noexcept { used_.fetch_sub(bytes, std::memory_order_acq_rel); }

spill_file::spill_file(spill_file&& other) noexcept
    : area_(other.area_), fd_(std::exchange(other.fd_, -1)), size_(other.size_.exchange(0)) {}

spill_file::~spill_file() {
  if (fd_ < 0) return;
  ::close(fd_);
  area_->uncharge(size_.load());
  area_->open_files_.fetch_sub(1);
}

void spill_file::write(std::uint64_t offset, std::span<const std::byte> data) {
  const std::uint64_t end = offset + data.size();
  if (end < offset) {
    throw stop_error(error_category::limit_exceeded, "spill.offset_overflow", "offset oltre 2^64");
  }
  {
    std::lock_guard lock(grow_mutex_);
    const std::uint64_t cur = size_.load(std::memory_order_acquire);
    if (end > cur) {
      area_->charge(end - cur);
      size_.store(end, std::memory_order_release);
    }
  }
  std::size_t done = 0;
  while (done < data.size()) {
    const ssize_t n = ::pwrite(fd_, data.data() + done, data.size() - done, static_cast<off_t>(offset + done));
    if (n < 0) {
      if (errno == EINTR) continue;  // interruzione da segnale: si riprende lo stesso trasferimento
      throw_errno(error_category::io, "spill.write", "scrittura dello spill fallita", errno,
                  {{"offset", std::to_string(offset + done)}});
    }
    done += static_cast<std::size_t>(n);
  }
}

void spill_file::read(std::uint64_t offset, std::span<std::byte> out) const {
  if (offset + out.size() < offset || offset + out.size() > size_bytes()) {
    throw stop_error(error_category::integrity, "spill.short_read", "lettura oltre l'estensione scritta",
                     {{"offset", std::to_string(offset)}, {"lunghezza", std::to_string(out.size())},
                      {"estensione", std::to_string(size_bytes())}});
  }
  std::size_t done = 0;
  while (done < out.size()) {
    const ssize_t n = ::pread(fd_, out.data() + done, out.size() - done, static_cast<off_t>(offset + done));
    if (n < 0) {
      if (errno == EINTR) continue;  // interruzione da segnale: si riprende la stessa lettura
      throw_errno(error_category::io, "spill.read", "lettura dello spill fallita", errno,
                  {{"offset", std::to_string(offset + done)}});
    }
    if (n == 0) {
      throw stop_error(error_category::integrity, "spill.short_read", "fine file prima dei byte attesi",
                       {{"offset", std::to_string(offset + done)}});
    }
    done += static_cast<std::size_t>(n);
  }
}

std::uint64_t spill_file::allocated_bytes() const {
  struct stat st {};
  if (::fstat(fd_, &st) != 0) throw_errno(error_category::io, "spill.read", "fstat dello spill fallita", errno);
  return static_cast<std::uint64_t>(st.st_blocks) * 512;
}

std::uint64_t spill_file::resident_bytes() const {
  const std::uint64_t length = size_bytes();
  if (length == 0) return 0;
  const std::uint64_t page = static_cast<std::uint64_t>(::sysconf(_SC_PAGESIZE));
  std::vector<unsigned char> pages((length + page - 1) / page);
  void* map = ::mmap(nullptr, length, PROT_READ, MAP_SHARED, fd_, 0);
  if (map == MAP_FAILED) throw_errno(error_category::io, "spill.mincore", "mmap dello spill fallita", errno);
  const struct unmapper {
    void* addr;
    std::uint64_t len;
    ~unmapper() { ::munmap(addr, len); }
  } guard{map, length};
  if (::mincore(map, length, pages.data()) != 0) throw_errno(error_category::io, "spill.mincore", "mincore fallita", errno);
  std::uint64_t resident = 0;
  for (std::size_t i = 0; i < pages.size(); ++i) {
    if (pages[i] & 1) resident += std::min<std::uint64_t>(page, length - i * page);
  }
  return resident;
}

void spill_file::drop_cache(std::uint64_t offset, std::uint64_t length) const {
  if (::sync_file_range(fd_, static_cast<off64_t>(offset), static_cast<off64_t>(length),
                        SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WRITE | SYNC_FILE_RANGE_WAIT_AFTER) != 0) {
    throw_errno(error_category::io, "spill.write", "sync_file_range fallita", errno);
  }
  const int rc = ::posix_fadvise(fd_, static_cast<off_t>(offset), static_cast<off_t>(length), POSIX_FADV_DONTNEED);
  if (rc != 0) throw_errno(error_category::io, "spill.write", "posix_fadvise fallita", rc);
}

}  // namespace astro
