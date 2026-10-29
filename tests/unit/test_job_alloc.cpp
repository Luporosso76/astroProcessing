// Gestione secondaria dell'eccezione del job in condizione di memoria esaurita: l'originale non si perde.
#include <cstdlib>
#include <new>

#include "store_test_support.hpp"

namespace {

thread_local bool fail_allocations = false;

void* checked_alloc(std::size_t n) {
  if (fail_allocations) throw std::bad_alloc();
  if (void* p = std::malloc(n == 0 ? 1 : n)) return p;
  throw std::bad_alloc();
}

}  // namespace

void* operator new(std::size_t n) { return checked_alloc(n); }
void* operator new[](std::size_t n) { return checked_alloc(n); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
  return fail_allocations ? nullptr : std::malloc(n == 0 ? 1 : n);
}
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept {
  return fail_allocations ? nullptr : std::malloc(n == 0 ? 1 : n);
}
void* operator new(std::size_t n, std::align_val_t a) {
  if (fail_allocations) throw std::bad_alloc();
  const std::size_t al = static_cast<std::size_t>(a);
  if (void* p = std::aligned_alloc(al, (n + al - 1) / al * al)) return p;
  throw std::bad_alloc();
}
void* operator new[](std::size_t n, std::align_val_t a) { return operator new(n, a); }
void operator delete(void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {

using namespace store_test;

// Senza la correzione la diagnosi secondaria costruiva una std::string dentro una funzione noexcept: con le
// allocazioni fallite il bad_alloc causava std::terminate e il test andava in crash invece di vedere l'originale.
TEST(JobAlloc, secondary_failure_with_exhausted_memory_still_rethrows_the_original_exception) {
  auto store = sqlite_dag_store::create(fresh_dir("jalloc"));
  store->apply_recipe(diamond(2).resolve(), {{"s", "a"}});
  store->set_fault_injector([](std::string_view p) -> int {
    if (p != "artifact.fsync_file") return 0;
    const std::logic_error secondary("eccezione secondaria con messaggio oltre la soglia SSO");
    fail_allocations = true;
    throw secondary;
  });
  testing::internal::CaptureStderr();
  bool original = false;
  try {
    run_job_files(*store, "s", [](const job_record&) -> job_files_result { throw std::runtime_error("originale"); });
  } catch (const std::runtime_error& ex) {
    fail_allocations = false;
    original = std::string(ex.what()) == "originale";
  } catch (...) {
    fail_allocations = false;
  }
  fail_allocations = false;
  testing::internal::GetCapturedStderr();
  EXPECT_TRUE(original);
}

// Con uno stop_error originario la diagnosi secondaria costruiva uno stop_error (allocazione): senza la protezione in
// stopped() usciva bad_alloc al posto dell'arresto originale.
TEST(JobAlloc, secondary_failure_with_exhausted_memory_still_returns_the_original_stop) {
  auto store = sqlite_dag_store::create(fresh_dir("jalloc_stop"));
  store->apply_recipe(diamond(2).resolve(), {{"s", "a"}});
  store->set_fault_injector([](std::string_view p) -> int {
    if (p != "artifact.fsync_file") return 0;
    const std::logic_error secondary("eccezione secondaria con messaggio oltre la soglia SSO");
    fail_allocations = true;
    throw secondary;
  });
  testing::internal::CaptureStderr();
  bool original = false;
  try {
    run_job_files(*store, "s", [](const job_record&) -> job_files_result {
      throw stop_error(error_category::invalid_input, "test.condition", "guasto iniettato");
    });
  } catch (const stop_error& ex) {
    fail_allocations = false;
    original = ex.category() == error_category::invalid_input && ex.code() == "test.condition";
  } catch (...) {
    fail_allocations = false;
  }
  fail_allocations = false;
  testing::internal::GetCapturedStderr();
  EXPECT_TRUE(original);
}

}  // namespace
