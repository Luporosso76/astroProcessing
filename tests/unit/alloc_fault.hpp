#pragma once

// Sostituisce operator new nel binario di test che include questo header (una sola unita' di traduzione):
// `arm()` fa fallire con bad_alloc la prossima allocazione del thread corrente.
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"

#include <cstdlib>
#include <new>

namespace alloc_fault {
inline thread_local bool armed = false;
inline void arm() { armed = true; }
}  // namespace alloc_fault

void* operator new(std::size_t size) {
  if (alloc_fault::armed) {
    alloc_fault::armed = false;
    throw std::bad_alloc();
  }
  void* p = std::malloc(size == 0 ? 1 : size);
  if (p == nullptr) throw std::bad_alloc();
  return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t size) { return operator new(size); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
  try {
    return operator new(size);
  } catch (const std::bad_alloc&) {
    return nullptr;
  }
}
void* operator new[](std::size_t size, const std::nothrow_t& tag) noexcept { return operator new(size, tag); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
