#pragma once

// Osservatore delle aperture di file, copiato da tests/unit/test_pack.cpp (U10) senza le chiamate di rete. Le funzioni
// libc di apertura sono interposte dall'eseguibile che include questa intestazione (una sola unità di traduzione per
// eseguibile; libastrocore è statica) e registrano solo mentre una `open_probe::window` è viva. Collegare con dl.
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace open_probe {

inline std::atomic<bool> active{false};
inline std::mutex mu;
inline std::vector<std::string> opens;
inline thread_local bool busy = false;

inline std::string resolve(int dirfd, const char* path) {
  namespace fs = std::filesystem;
  const fs::path p(path);
  if (p.is_absolute()) return p.lexically_normal().string();
  fs::path base;
  if (dirfd == AT_FDCWD) {
    base = fs::current_path();
  } else {
    char buf[4096];
    const ssize_t n = ::readlink(("/proc/self/fd/" + std::to_string(dirfd)).c_str(), buf, sizeof buf - 1);
    base = n > 0 ? fs::path(std::string(buf, static_cast<std::size_t>(n))) : fs::path("<dirfd-sconosciuto>");
  }
  return (base / p).lexically_normal().string();
}

inline void note(int dirfd, const char* path) {
  if (!active.load() || busy || path == nullptr) return;
  busy = true;
  const std::string r = resolve(dirfd, path);
  {
    std::lock_guard<std::mutex> lock(mu);
    opens.push_back(r);
  }
  busy = false;
}

class window {
 public:
  window() {
    std::lock_guard<std::mutex> lock(mu);
    opens.clear();
    active = true;
  }
  ~window() { active = false; }
  window(const window&) = delete;
  window& operator=(const window&) = delete;
  std::vector<std::string> seen() const {
    std::lock_guard<std::mutex> lock(mu);
    return opens;
  }
};

// O_TMPFILE include O_DIRECTORY: il modo c'è solo con O_CREAT o con tutti i bit di O_TMPFILE.
inline bool takes_mode(int flags) { return (flags & O_CREAT) != 0 || (flags & O_TMPFILE) == O_TMPFILE; }

template <class F>
F next_symbol(const char* name) {
  void* p = ::dlsym(RTLD_NEXT, name);
  if (p == nullptr) {
    std::fprintf(stderr, "simbolo libc non trovato: %s\n", name);
    std::_Exit(70);
  }
  return reinterpret_cast<F>(p);
}

}  // namespace open_probe

extern "C" {

int open(const char* path, int flags, ...) {
  mode_t mode = 0;
  if (open_probe::takes_mode(flags)) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }
  open_probe::note(AT_FDCWD, path);
  static auto real = open_probe::next_symbol<int (*)(const char*, int, ...)>("open");
  return real(path, flags, mode);
}

int open64(const char* path, int flags, ...) {
  mode_t mode = 0;
  if (open_probe::takes_mode(flags)) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }
  open_probe::note(AT_FDCWD, path);
  static auto real = open_probe::next_symbol<int (*)(const char*, int, ...)>("open64");
  return real(path, flags, mode);
}

int openat(int dirfd, const char* path, int flags, ...) {
  mode_t mode = 0;
  if (open_probe::takes_mode(flags)) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }
  open_probe::note(dirfd, path);
  static auto real = open_probe::next_symbol<int (*)(int, const char*, int, ...)>("openat");
  return real(dirfd, path, flags, mode);
}

int openat64(int dirfd, const char* path, int flags, ...) {
  mode_t mode = 0;
  if (open_probe::takes_mode(flags)) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }
  open_probe::note(dirfd, path);
  static auto real = open_probe::next_symbol<int (*)(int, const char*, int, ...)>("openat64");
  return real(dirfd, path, flags, mode);
}

// Varianti con _FORTIFY_SOURCE (build release del core).
int __open_2(const char* path, int flags) {
  open_probe::note(AT_FDCWD, path);
  static auto real = open_probe::next_symbol<int (*)(const char*, int)>("__open_2");
  return real(path, flags);
}

int __open64_2(const char* path, int flags) {
  open_probe::note(AT_FDCWD, path);
  static auto real = open_probe::next_symbol<int (*)(const char*, int)>("__open64_2");
  return real(path, flags);
}

int __openat_2(int dirfd, const char* path, int flags) {
  open_probe::note(dirfd, path);
  static auto real = open_probe::next_symbol<int (*)(int, const char*, int)>("__openat_2");
  return real(dirfd, path, flags);
}

int __openat64_2(int dirfd, const char* path, int flags) {
  open_probe::note(dirfd, path);
  static auto real = open_probe::next_symbol<int (*)(int, const char*, int)>("__openat64_2");
  return real(dirfd, path, flags);
}

FILE* fopen(const char* path, const char* mode) {
  open_probe::note(AT_FDCWD, path);
  static auto real = open_probe::next_symbol<FILE* (*)(const char*, const char*)>("fopen");
  return real(path, mode);
}

FILE* fopen64(const char* path, const char* mode) {
  open_probe::note(AT_FDCWD, path);
  static auto real = open_probe::next_symbol<FILE* (*)(const char*, const char*)>("fopen64");
  return real(path, mode);
}

DIR* opendir(const char* path) {
  open_probe::note(AT_FDCWD, path);
  static auto real = open_probe::next_symbol<DIR* (*)(const char*)>("opendir");
  return real(path);
}

}  // extern "C"
