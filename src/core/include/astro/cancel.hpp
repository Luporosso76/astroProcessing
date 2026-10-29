#pragma once

#include <atomic>

namespace astro {

// Richiesta di annullamento condivisa fra chi la imposta (CLI, gestore di segnale) e chi la controlla.
// Solo operazioni atomiche lock-free: `request` e' ammesso in un gestore di segnale.
class cancel_token {
 public:
  void request() noexcept { flag_.store(true, std::memory_order_release); }
  bool requested() const noexcept { return flag_.load(std::memory_order_acquire); }

 private:
  std::atomic<bool> flag_{false};
};

}  // namespace astro
