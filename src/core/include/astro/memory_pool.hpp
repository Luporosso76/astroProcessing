#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "astro/budget.hpp"
#include "astro/cancel.hpp"
#include "astro/image.hpp"

namespace astro {

class memory_pool;

// Prenotazione RAII di byte del pool. Il pool limita le prenotazioni, non l'RSS.
// Una prenotazione deve sopravvivere a tutte le sotto-prenotazioni e ai pool_buffer che ne derivano.
class reservation {
 public:
  struct state;

  reservation() noexcept;
  reservation(reservation&& other) noexcept;
  reservation& operator=(reservation&& other) noexcept;
  reservation(const reservation&) = delete;
  reservation& operator=(const reservation&) = delete;
  ~reservation();

  explicit operator bool() const noexcept { return state_ != nullptr; }
  std::uint64_t bytes() const noexcept;
  // Byte della prenotazione impegnati da sotto-prenotazioni e pool_buffer vivi.
  std::uint64_t committed_bytes() const noexcept;
  const std::string& label() const;

  // Sotto-prenotazione presa in una volta sola dalla parte non impegnata: oltre il residuo,
  // budget pool.reservation_exceeded. Alla distruzione i byte tornano a questa prenotazione.
  reservation carve(std::uint64_t bytes, std::string label);

  // Rilascio anticipato; idempotente.
  void release() noexcept;

 private:
  friend class memory_pool;
  friend class pool_buffer;
  explicit reservation(std::unique_ptr<state> state) noexcept;

  std::unique_ptr<state> state_;
};

// Buffer allineato allocabile solo dentro una prenotazione. Superarla: budget pool.reservation_exceeded.
class pool_buffer {
 public:
  pool_buffer(reservation& owner, std::size_t bytes);
  pool_buffer(pool_buffer&& other) noexcept;
  pool_buffer& operator=(pool_buffer&&) = delete;
  pool_buffer(const pool_buffer&) = delete;
  pool_buffer& operator=(const pool_buffer&) = delete;
  ~pool_buffer();

  std::byte* data() noexcept { return buffer_.data(); }
  const std::byte* data() const noexcept { return buffer_.data(); }
  std::size_t size() const noexcept { return buffer_.size(); }

 private:
  reservation::state* owner_;
  aligned_buffer buffer_;
};

class memory_pool {
 public:
  static constexpr std::size_t unlimited_waiters = SIZE_MAX;

  explicit memory_pool(std::uint64_t budget_bytes);
  explicit memory_pool(const budget_snapshot& snapshot) : memory_pool(snapshot.budget_bytes) {}
  memory_pool(const memory_pool&) = delete;
  memory_pool& operator=(const memory_pool&) = delete;
  // Tutte le prenotazioni devono essere gia' state rilasciate.
  ~memory_pool();

  std::uint64_t budget_bytes() const noexcept { return budget_bytes_; }
  std::uint64_t reserved_bytes() const;
  // Massimo di reserved_bytes osservato dentro il pool da quando esiste.
  std::uint64_t peak_reserved_bytes() const;
  std::size_t waiting_count() const;

  // Tutto o niente, senza attesa: nullopt se non entra subito o se qualcuno e' in coda davanti.
  // Oltre il budget: budget pool.exceeds_budget.
  std::optional<reservation> try_reserve(std::uint64_t bytes, std::string label);

  // Attende in coda FIFO stretta finche' la richiesta entra. Con `max_waiters` richieste gia' in coda:
  // budget sched.queue_full. `cancel` e' controllato durante l'attesa: cancelled sched.cancelled.
  reservation reserve(std::uint64_t bytes, std::string label, const cancel_token* cancel = nullptr,
                      std::size_t max_waiters = unlimited_waiters);

 private:
  friend class reservation;
  reservation grant_locked(std::uint64_t bytes, std::string label);
  void give_back(std::uint64_t bytes) noexcept;

  const std::uint64_t budget_bytes_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::uint64_t reserved_ = 0;
  std::uint64_t peak_ = 0;
  std::uint64_t next_ticket_ = 0;
  std::deque<std::uint64_t> waiters_;
};

}  // namespace astro
