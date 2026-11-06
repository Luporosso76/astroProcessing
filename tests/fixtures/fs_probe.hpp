#pragma once

// Osservatore di stat e aperture, nel modello di open_probe.hpp: le funzioni libc sono interposte dall'eseguibile che
// include questa intestazione (una sola unità di traduzione per eseguibile; libastrocore è statica) e registrano solo
// mentre una `fs_probe::window` è viva. Collegare con dl. Non si include insieme a open_probe.hpp.
// Limite: si interpongono stat, open, opendir, openat; `scandir`, `nftw`, `glob` e `realpath` chiamano funzioni interne di
// libc senza passare dalla PLT e sfuggono all'osservazione.
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace fs_probe {

inline std::atomic<bool> active{false};
inline std::mutex mu;
inline std::vector<std::string> events;  // «stat <percorso>», «open <percorso>», «opendir <percorso>»
inline thread_local bool busy = false;

inline void note(const char* kind, int dirfd, const char* path) {
  if (!active.load() || busy || path == nullptr) return;
  busy = true;
  std::string shown;
  if (path[0] == '/' || dirfd == AT_FDCWD) {
    namespace fs = std::filesystem;
    const fs::path p(path);
    shown = (p.is_absolute() ? p : fs::current_path() / p).string();
  } else {
    shown = std::string("<dirfd>/") + path;
  }
  {
    std::lock_guard<std::mutex> lock(mu);
    events.push_back(std::string(kind) + " " + shown);
  }
  busy = false;
}

class window {
 public:
  window() {
    std::lock_guard<std::mutex> lock(mu);
    events.clear();
    active = true;
  }
  ~window() { active = false; }
  window(const window&) = delete;
  window& operator=(const window&) = delete;
  std::vector<std::string> seen() const {
    std::lock_guard<std::mutex> lock(mu);
    return events;
  }
};

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

}  // namespace fs_probe

extern "C" {

int stat(const char* path, struct stat* buf) __THROW {
  fs_probe::note("stat", AT_FDCWD, path);
  static auto real = fs_probe::next_symbol<int (*)(const char*, struct stat*)>("stat");
  return real(path, buf);
}

int stat64(const char* path, struct stat64* buf) __THROW {
  fs_probe::note("stat", AT_FDCWD, path);
  static auto real = fs_probe::next_symbol<int (*)(const char*, struct stat64*)>("stat64");
  return real(path, buf);
}

int lstat(const char* path, struct stat* buf) __THROW {
  fs_probe::note("stat", AT_FDCWD, path);
  static auto real = fs_probe::next_symbol<int (*)(const char*, struct stat*)>("lstat");
  return real(path, buf);
}

int lstat64(const char* path, struct stat64* buf) __THROW {
  fs_probe::note("stat", AT_FDCWD, path);
  static auto real = fs_probe::next_symbol<int (*)(const char*, struct stat64*)>("lstat64");
  return real(path, buf);
}

int fstatat(int dirfd, const char* path, struct stat* buf, int flags) __THROW {
  fs_probe::note("stat", dirfd, path);
  static auto real = fs_probe::next_symbol<int (*)(int, const char*, struct stat*, int)>("fstatat");
  return real(dirfd, path, buf, flags);
}

int fstatat64(int dirfd, const char* path, struct stat64* buf, int flags) __THROW {
  fs_probe::note("stat", dirfd, path);
  static auto real = fs_probe::next_symbol<int (*)(int, const char*, struct stat64*, int)>("fstatat64");
  return real(dirfd, path, buf, flags);
}

int statx(int dirfd, const char* path, int flags, unsigned int mask, struct statx* buf) __THROW {
  fs_probe::note("stat", dirfd, path);
  static auto real = fs_probe::next_symbol<int (*)(int, const char*, int, unsigned int, struct statx*)>("statx");
  return real(dirfd, path, flags, mask, buf);
}

int open(const char* path, int flags, ...) {
  mode_t mode = 0;
  if (fs_probe::takes_mode(flags)) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }
  fs_probe::note("open", AT_FDCWD, path);
  static auto real = fs_probe::next_symbol<int (*)(const char*, int, ...)>("open");
  return real(path, flags, mode);
}

int open64(const char* path, int flags, ...) {
  mode_t mode = 0;
  if (fs_probe::takes_mode(flags)) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }
  fs_probe::note("open", AT_FDCWD, path);
  static auto real = fs_probe::next_symbol<int (*)(const char*, int, ...)>("open64");
  return real(path, flags, mode);
}

int openat(int dirfd, const char* path, int flags, ...) {
  mode_t mode = 0;
  if (fs_probe::takes_mode(flags)) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }
  fs_probe::note("open", dirfd, path);
  static auto real = fs_probe::next_symbol<int (*)(int, const char*, int, ...)>("openat");
  return real(dirfd, path, flags, mode);
}

int openat64(int dirfd, const char* path, int flags, ...) {
  mode_t mode = 0;
  if (fs_probe::takes_mode(flags)) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }
  fs_probe::note("open", dirfd, path);
  static auto real = fs_probe::next_symbol<int (*)(int, const char*, int, ...)>("openat64");
  return real(dirfd, path, flags, mode);
}

int __open_2(const char* path, int flags) {
  fs_probe::note("open", AT_FDCWD, path);
  static auto real = fs_probe::next_symbol<int (*)(const char*, int)>("__open_2");
  return real(path, flags);
}

int __open64_2(const char* path, int flags) {
  fs_probe::note("open", AT_FDCWD, path);
  static auto real = fs_probe::next_symbol<int (*)(const char*, int)>("__open64_2");
  return real(path, flags);
}

int __openat_2(int dirfd, const char* path, int flags) {
  fs_probe::note("open", dirfd, path);
  static auto real = fs_probe::next_symbol<int (*)(int, const char*, int)>("__openat_2");
  return real(dirfd, path, flags);
}

int __openat64_2(int dirfd, const char* path, int flags) {
  fs_probe::note("open", dirfd, path);
  static auto real = fs_probe::next_symbol<int (*)(int, const char*, int)>("__openat64_2");
  return real(dirfd, path, flags);
}

FILE* fopen(const char* path, const char* mode) {
  fs_probe::note("open", AT_FDCWD, path);
  static auto real = fs_probe::next_symbol<FILE* (*)(const char*, const char*)>("fopen");
  return real(path, mode);
}

FILE* fopen64(const char* path, const char* mode) {
  fs_probe::note("open", AT_FDCWD, path);
  static auto real = fs_probe::next_symbol<FILE* (*)(const char*, const char*)>("fopen64");
  return real(path, mode);
}

DIR* opendir(const char* path) {
  fs_probe::note("opendir", AT_FDCWD, path);
  static auto real = fs_probe::next_symbol<DIR* (*)(const char*)>("opendir");
  return real(path);
}

}  // extern "C"
