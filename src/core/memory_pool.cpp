#include "astro/memory_pool.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>

namespace astro {

struct reservation::state {
  memory_pool* pool = nullptr;    // prenotazione di primo livello
  state* parent = nullptr;        // sotto-prenotazione
  std::uint64_t bytes = 0;
  std::string label;
  std::atomic<std::uint64_t> committed{0};
};

namespace {

constexpr auto wait_tick = std::chrono::milliseconds(20);

// Impegna `bytes` su `owner` se il residuo basta.
void commit(reservation::state& owner, std::uint64_t bytes, const char* what) {
  std::uint64_t cur = owner.committed.load(std::memory_order_relaxed);
  do {
    if (bytes > owner.bytes - cur) {
      throw stop_error(error_category::budget, "pool.reservation_exceeded",
                       "allocazione oltre la prenotazione dichiarata",
                       {{"prenotazione", owner.label}, {"cosa", what}, {"prenotati", std::to_string(owner.bytes)},
                        {"impegnati", std::to_string(cur)}, {"richiesti", std::to_string(bytes)}});
    }
  } while (!owner.committed.compare_exchange_weak(cur, cur + bytes, std::memory_order_acq_rel));
}

void uncommit(reservation::state& owner, std::uint64_t bytes) noexcept {
  owner.committed.fetch_sub(bytes, std::memory_order_acq_rel);
}

}  // namespace

reservation::reservation() noexcept = default;
reservation::reservation(std::unique_ptr<state> s) noexcept : state_(std::move(s)) {}
reservation::reservation(reservation&& other) noexcept = default;

reservation& reservation::operator=(reservation&& other) noexcept {
  if (this != &other) {
    release();
    state_ = std::move(other.state_);
  }
  return *this;
}

reservation::~reservation() { release(); }

std::uint64_t reservation::bytes() const noexcept { return state_ ? state_->bytes : 0; }

std::uint64_t reservation::committed_bytes() const noexcept {
  return state_ ? state_->committed.load(std::memory_order_acquire) : 0;
}

const std::string& reservation::label() const {
  ASTRO_ASSERT(state_ != nullptr);
  return state_->label;
}

reservation reservation::carve(std::uint64_t bytes, std::string label) {
  ASTRO_ASSERT(state_ != nullptr);
  auto child = std::make_unique<state>();
  child->parent = state_.get();
  child->bytes = bytes;
  child->label = std::move(label);
  commit(*state_, bytes, "sotto-prenotazione");
  return reservation(std::move(child));
}

void reservation::release() noexcept {
  if (!state_) return;
  ASTRO_ASSERT(state_->committed.load() == 0);
  if (state_->parent != nullptr) {
    uncommit(*state_->parent, state_->bytes);
  } else {
    state_->pool->give_back(state_->bytes);
  }
  state_.reset();
}

pool_buffer::pool_buffer(reservation& owner, std::size_t bytes) : owner_(owner.state_.get()) {
  ASTRO_ASSERT(owner_ != nullptr);
  commit(*owner_, bytes, "pool_buffer");
  try {
    buffer_ = aligned_buffer(bytes);
  } catch (...) {
    uncommit(*owner_, bytes);
    throw;
  }
}

pool_buffer::pool_buffer(pool_buffer&& other) noexcept
    : owner_(std::exchange(other.owner_, nullptr)), buffer_(std::move(other.buffer_)) {}

pool_buffer::~pool_buffer() {
  if (owner_ != nullptr) uncommit(*owner_, buffer_.size());
}

memory_pool::memory_pool(std::uint64_t budget_bytes) : budget_bytes_(budget_bytes) {}

memory_pool::~memory_pool() { ASTRO_ASSERT(reserved_ == 0); }

std::uint64_t memory_pool::reserved_bytes() const {
  std::lock_guard lock(mutex_);
  return reserved_;
}

std::uint64_t memory_pool::peak_reserved_bytes() const {
  std::lock_guard lock(mutex_);
  return peak_;
}

std::size_t memory_pool::waiting_count() const {
  std::lock_guard lock(mutex_);
  return waiters_.size();
}

reservation memory_pool::grant_locked(std::uint64_t bytes, std::string label) {
  auto s = std::make_unique<reservation::state>();
  s->pool = this;
  s->bytes = bytes;
  s->label = std::move(label);
  reserved_ += bytes;
  peak_ = std::max(peak_, reserved_);
  return reservation(std::move(s));
}

void memory_pool::give_back(std::uint64_t bytes) noexcept {
  {
    std::lock_guard lock(mutex_);
    ASTRO_ASSERT(reserved_ >= bytes);
    reserved_ -= bytes;
  }
  cv_.notify_all();
}

namespace {

[[noreturn]] void exceeds(std::uint64_t bytes, std::uint64_t budget, const std::string& label) {
  throw stop_error(error_category::budget, "pool.exceeds_budget", "la prenotazione supera il budget del pool",
                   {{"etichetta", label}, {"richiesti", std::to_string(bytes)}, {"budget", std::to_string(budget)}});
}

}  // namespace

std::optional<reservation> memory_pool::try_reserve(std::uint64_t bytes, std::string label) {
  if (bytes > budget_bytes_) exceeds(bytes, budget_bytes_, label);
  std::lock_guard lock(mutex_);
  if (!waiters_.empty() || bytes > budget_bytes_ - reserved_) return std::nullopt;
  return grant_locked(bytes, std::move(label));
}

reservation memory_pool::reserve(std::uint64_t bytes, std::string label, const cancel_token* cancel,
                                 std::size_t max_waiters) {
  if (bytes > budget_bytes_) exceeds(bytes, budget_bytes_, label);
  std::unique_lock lock(mutex_);
  if (waiters_.empty() && bytes <= budget_bytes_ - reserved_) return grant_locked(bytes, std::move(label));
  if (waiters_.size() >= max_waiters) {
    throw stop_error(error_category::budget, "sched.queue_full", "coda dei job piena",
                     {{"etichetta", label}, {"capacita", std::to_string(max_waiters)}});
  }
  const std::uint64_t ticket = next_ticket_++;
  waiters_.push_back(ticket);
  const auto leave_queue = [&] {
    waiters_.erase(std::find(waiters_.begin(), waiters_.end(), ticket));
    cv_.notify_all();
  };
  const auto ready = [&] { return waiters_.front() == ticket && bytes <= budget_bytes_ - reserved_; };
  while (!ready()) {
    if (cancel != nullptr && cancel->requested()) {
      leave_queue();
      throw stop_error(error_category::cancelled, "sched.cancelled", "annullamento durante l'attesa del pool",
                       {{"etichetta", label}});
    }
    cv_.wait_for(lock, wait_tick);
  }
  waiters_.pop_front();
  reservation granted = grant_locked(bytes, std::move(label));
  cv_.notify_all();
  return granted;
}

}  // namespace astro
