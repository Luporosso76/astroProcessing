#pragma once
// VFS SQLite di test per la perdita di energia (ADR-04, D17): delega a `unix` e ricorda, per ogni file del database,
// il contenuto all'ultimo xSync. Le scritture non sincronizzate non entrano nello stato durevole; xDelete e' durevole
// subito; il file -shm non e' mai durevole (SQLite lo ricostruisce dal WAL).
#include <sqlite3.h>

#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>

namespace powerloss {

constexpr const char* vfs_name = "astro_powerloss";

struct recorder {
  std::map<std::string, std::string> durable;  // nome base -> byte all'ultimo xSync
  std::function<void()> on_sync;               // dopo ogni xSync e ogni xDelete riusciti di un file del database
};

inline recorder*& active() {
  static recorder* r = nullptr;
  return r;
}

inline sqlite3_vfs*& real_vfs() {
  static sqlite3_vfs* v = nullptr;
  return v;
}

struct pl_file {
  sqlite3_file base;
  sqlite3_file* real;
  bool tracked;
  char name[256];
};

inline sqlite3_file* real_of(sqlite3_file* f) { return reinterpret_cast<pl_file*>(f)->real; }

inline int pl_sync(sqlite3_file* f, int flags) {
  pl_file* p = reinterpret_cast<pl_file*>(f);
  const int rc = p->real->pMethods->xSync(p->real, flags);
  if (rc != SQLITE_OK || !p->tracked || active() == nullptr) return rc;
  sqlite3_int64 size = 0;
  if (p->real->pMethods->xFileSize(p->real, &size) != SQLITE_OK) return SQLITE_IOERR_FSYNC;
  std::string bytes(static_cast<std::size_t>(size), '\0');
  if (size > 0 && p->real->pMethods->xRead(p->real, bytes.data(), static_cast<int>(size), 0) != SQLITE_OK) {
    return SQLITE_IOERR_FSYNC;
  }
  active()->durable[p->name] = std::move(bytes);
  if (active()->on_sync) active()->on_sync();
  return rc;
}

inline const sqlite3_io_methods* io_methods() {
  static const sqlite3_io_methods m = {
      3,
      [](sqlite3_file* f) { return real_of(f)->pMethods->xClose(real_of(f)); },
      [](sqlite3_file* f, void* b, int n, sqlite3_int64 o) { return real_of(f)->pMethods->xRead(real_of(f), b, n, o); },
      [](sqlite3_file* f, const void* b, int n, sqlite3_int64 o) {
        return real_of(f)->pMethods->xWrite(real_of(f), b, n, o);
      },
      [](sqlite3_file* f, sqlite3_int64 s) { return real_of(f)->pMethods->xTruncate(real_of(f), s); },
      pl_sync,
      [](sqlite3_file* f, sqlite3_int64* s) { return real_of(f)->pMethods->xFileSize(real_of(f), s); },
      [](sqlite3_file* f, int l) { return real_of(f)->pMethods->xLock(real_of(f), l); },
      [](sqlite3_file* f, int l) { return real_of(f)->pMethods->xUnlock(real_of(f), l); },
      [](sqlite3_file* f, int* r) { return real_of(f)->pMethods->xCheckReservedLock(real_of(f), r); },
      [](sqlite3_file* f, int op, void* a) { return real_of(f)->pMethods->xFileControl(real_of(f), op, a); },
      [](sqlite3_file* f) { return real_of(f)->pMethods->xSectorSize(real_of(f)); },
      [](sqlite3_file* f) { return real_of(f)->pMethods->xDeviceCharacteristics(real_of(f)); },
      [](sqlite3_file* f, int pg, int sz, int ext, void volatile** pp) {
        return real_of(f)->pMethods->xShmMap(real_of(f), pg, sz, ext, pp);
      },
      [](sqlite3_file* f, int off, int n, int flags) { return real_of(f)->pMethods->xShmLock(real_of(f), off, n, flags); },
      [](sqlite3_file* f) { real_of(f)->pMethods->xShmBarrier(real_of(f)); },
      [](sqlite3_file* f, int del) { return real_of(f)->pMethods->xShmUnmap(real_of(f), del); },
      [](sqlite3_file* f, sqlite3_int64 o, int n, void** pp) { return real_of(f)->pMethods->xFetch(real_of(f), o, n, pp); },
      [](sqlite3_file* f, sqlite3_int64 o, void* p) { return real_of(f)->pMethods->xUnfetch(real_of(f), o, p); },
  };
  return &m;
}

inline std::string base_name(const char* path) { return std::filesystem::path(path).filename().string(); }

// Registra il VFS (una volta) e lo collega a `r` (nullptr: nessuna registrazione dello stato durevole).
inline void install(recorder* r) {
  static sqlite3_vfs vfs{};
  if (real_vfs() == nullptr) {
    real_vfs() = sqlite3_vfs_find("unix");
    if (real_vfs() == nullptr) throw std::runtime_error("VFS unix assente");
    vfs = *real_vfs();
    vfs.zName = vfs_name;
    vfs.pNext = nullptr;
    vfs.szOsFile = static_cast<int>(sizeof(pl_file)) + real_vfs()->szOsFile;
    vfs.xOpen = [](sqlite3_vfs*, sqlite3_filename name, sqlite3_file* f, int flags, int* out) {
      pl_file* p = reinterpret_cast<pl_file*>(f);
      std::memset(p, 0, sizeof(pl_file));
      p->real = reinterpret_cast<sqlite3_file*>(p + 1);
      const int rc = real_vfs()->xOpen(real_vfs(), name, p->real, flags, out);
      if (rc != SQLITE_OK) return rc;
      p->base.pMethods = io_methods();
      p->tracked = name != nullptr && (flags & (SQLITE_OPEN_MAIN_DB | SQLITE_OPEN_WAL | SQLITE_OPEN_MAIN_JOURNAL)) != 0;
      if (p->tracked) {
        const std::string b = base_name(name);
        if (b.size() >= sizeof(p->name)) return SQLITE_CANTOPEN;
        std::memcpy(p->name, b.c_str(), b.size() + 1);
      }
      return rc;
    };
    vfs.xDelete = [](sqlite3_vfs*, const char* name, int sync_dir) {
      const int rc = real_vfs()->xDelete(real_vfs(), name, sync_dir);
      if (rc == SQLITE_OK && active() != nullptr) {
        active()->durable.erase(base_name(name));
        if (active()->on_sync) active()->on_sync();
      }
      return rc;
    };
    vfs.xAccess = [](sqlite3_vfs*, const char* n, int flags, int* out) { return real_vfs()->xAccess(real_vfs(), n, flags, out); };
    vfs.xFullPathname = [](sqlite3_vfs*, const char* n, int size, char* out) {
      return real_vfs()->xFullPathname(real_vfs(), n, size, out);
    };
    vfs.xRandomness = [](sqlite3_vfs*, int n, char* out) { return real_vfs()->xRandomness(real_vfs(), n, out); };
    vfs.xSleep = [](sqlite3_vfs*, int us) { return real_vfs()->xSleep(real_vfs(), us); };
    vfs.xCurrentTime = [](sqlite3_vfs*, double* t) { return real_vfs()->xCurrentTime(real_vfs(), t); };
    vfs.xGetLastError = [](sqlite3_vfs*, int n, char* out) { return real_vfs()->xGetLastError(real_vfs(), n, out); };
    vfs.xCurrentTimeInt64 = [](sqlite3_vfs*, sqlite3_int64* t) { return real_vfs()->xCurrentTimeInt64(real_vfs(), t); };
    if (sqlite3_vfs_register(&vfs, 0) != SQLITE_OK) throw std::runtime_error("registrazione del VFS fallita");
  }
  active() = r;
}

}  // namespace powerloss
