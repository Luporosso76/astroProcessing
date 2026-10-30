#include "astro/hash.hpp"

#include <openssl/evp.h>

#include <algorithm>
#include <memory>

#include "astro/error.hpp"
#include "astro/fits.hpp"
#include "astro/input_format.hpp"

namespace astro {

namespace {

struct md_ctx_deleter {
  void operator()(EVP_MD_CTX* ctx) const noexcept { EVP_MD_CTX_free(ctx); }
};

[[noreturn]] void openssl_failure(const char* step) {
  throw stop_error(error_category::io, "hash.openssl", "operazione OpenSSL EVP non riuscita",
                   {{"passo", step}});
}

std::string hex_of(const unsigned char* md, unsigned int len) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string out;
  out.reserve(64);
  for (unsigned int i = 0; i < len; ++i) {
    out.push_back(digits[md[i] >> 4]);
    out.push_back(digits[md[i] & 0x0f]);
  }
  return out;
}

}  // namespace

struct sha256_stream::state {
  std::unique_ptr<EVP_MD_CTX, md_ctx_deleter> ctx;
  bool finished = false;
};

sha256_stream::sha256_stream() : state_(std::make_unique<state>()) {
  state_->ctx.reset(EVP_MD_CTX_new());
  if (!state_->ctx || EVP_DigestInit_ex(state_->ctx.get(), EVP_sha256(), nullptr) != 1) openssl_failure("init");
}

sha256_stream::~sha256_stream() = default;

void sha256_stream::update(std::span<const std::uint8_t> bytes) {
  ASTRO_ASSERT(!state_->finished);
  if (!bytes.empty() && EVP_DigestUpdate(state_->ctx.get(), bytes.data(), bytes.size()) != 1) openssl_failure("update");
  size_ += bytes.size();
}

content_digest sha256_stream::finish() {
  ASTRO_ASSERT(!state_->finished);
  state_->finished = true;
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int len = 0;
  if (EVP_DigestFinal_ex(state_->ctx.get(), md, &len) != 1 || len != 32) openssl_failure("final");
  return content_digest{hex_of(md, len), size_};
}

std::string sha256_hex(std::span<const std::uint8_t> bytes) {
  std::unique_ptr<EVP_MD_CTX, md_ctx_deleter> ctx(EVP_MD_CTX_new());
  if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) openssl_failure("init");
  for (std::size_t offset = 0; offset < bytes.size(); offset += sha256_chunk_bytes) {
    const std::size_t n = std::min(sha256_chunk_bytes, bytes.size() - offset);
    if (EVP_DigestUpdate(ctx.get(), bytes.data() + offset, n) != 1) openssl_failure("update");
  }
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int len = 0;
  if (EVP_DigestFinal_ex(ctx.get(), md, &len) != 1 || len != 32) openssl_failure("final");
  return hex_of(md, len);
}

content_digest digest_of(std::span<const std::uint8_t> bytes) {
  return content_digest{sha256_hex(bytes), bytes.size()};
}

bool is_sha256_hex(const std::string& text) noexcept {
  return text.size() == 64 && std::all_of(text.begin(), text.end(), [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}

void require_input_digest(const content_digest& found, const content_digest& expected, const std::string& path) {
  if (found == expected) return;
  throw stop_error(error_category::integrity, "input.digest_mismatch", "digest o dimensione del file diversi dal record",
                   {{"percorso", path},
                    {"atteso", expected.sha256 + ":" + std::to_string(expected.size_bytes)},
                    {"letto", found.sha256 + ":" + std::to_string(found.size_bytes)}});
}

void require_planned_header(const fits_plan& planned, const fits_plan& verified, const std::string& path) {
  const auto changed = [&](const char* code, const char* field, long long a, long long b) {
    if (a == b) return;
    throw stop_error(error_category::integrity, code, "un valore di header del piano differisce dal file verificato",
                     {{"percorso", path}, {"campo", field}, {"pianificato", std::to_string(a)}, {"verificato", std::to_string(b)}});
  };
  changed("input.size_changed", "dimensione", static_cast<long long>(planned.file_bytes),
          static_cast<long long>(verified.file_bytes));
  changed("input.geometry_changed", "larghezza", planned.width, verified.width);
  changed("input.geometry_changed", "altezza", planned.height, verified.height);
  changed("input.geometry_changed", "piani", planned.planes, verified.planes);
  changed("input.header_changed", "BITPIX", planned.bitpix, verified.bitpix);
  changed("input.header_changed", "tipo", static_cast<long long>(planned.type), static_cast<long long>(verified.type));
  changed("input.header_changed", "invalid", planned.has_invalid, verified.has_invalid);
  changed("input.header_changed", "varianza", planned.has_variance, verified.has_variance);
}

std::vector<std::uint8_t> read_verified_input(const fits_input& input, const std::string& path,
                                              const content_digest& expected, std::uint64_t budget_bytes,
                                              const fits_plan* planned) {
  ASTRO_ASSERT((planned != nullptr) == input.format().header_plan);
  fits_detail::check_path_names(path, input);
  std::vector<std::uint8_t> bytes = read_input_bytes(input, budget_bytes);
  require_input_digest(digest_of(bytes), expected, path);
  if (planned != nullptr) require_planned_header(*planned, plan_fits(bytes), path);
  fits_detail::check_unchanged(input);
  return bytes;
}

content_digest digest_input(const fits_input& input, std::span<std::byte> work, const std::function<void()>& check_cancel) {
  ASTRO_ASSERT(!work.empty());
  sha256_stream hasher;
  for (std::uint64_t done = 0; done < input.size_bytes();) {
    if (check_cancel) check_cancel();
    const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(work.size(), input.size_bytes() - done));
    fits_detail::read_exact(input, done, work.data(), n);
    hasher.update({reinterpret_cast<const std::uint8_t*>(work.data()), n});
    done += n;
  }
  return hasher.finish();
}

void reverify_input(const std::string& path, const input_format& format, const fits_input* opened,
                    const content_digest& expected, std::span<std::byte> work,
                    const std::function<void()>& check_cancel) {
  ASTRO_ASSERT(opened == nullptr || &opened->format() == &format);
  const fits_input input = open_input(path, format);
  if (input.size_bytes() != expected.size_bytes) {
    throw stop_error(error_category::integrity, "input.size_changed", "la dimensione del file differisce dal record",
                     {{"percorso", path}, {"atteso", std::to_string(expected.size_bytes)},
                      {"trovata", std::to_string(input.size_bytes())}});
  }
  require_input_digest(digest_input(input, work, check_cancel), expected, path);
  fits_detail::check_unchanged(input);
  if (opened != nullptr) fits_detail::check_unchanged(*opened);
}

std::vector<std::uint8_t> read_verified_fits(const std::string& path, const content_digest& expected,
                                             std::uint64_t budget_bytes) {
  fits_input input = open_fits_input(path);
  std::vector<std::uint8_t> bytes = read_input_bytes(input, budget_bytes);
  require_input_digest(digest_of(bytes), expected, path);
  const fits_plan plan = plan_fits(bytes);
  if (plan.peak_bytes > budget_bytes) {
    throw stop_error(error_category::budget, "sched.job_exceeds_budget", "picco di memoria oltre il budget",
                     {{"picco", std::to_string(plan.peak_bytes)}, {"budget", std::to_string(budget_bytes)}});
  }
  return bytes;
}

verified_fits read_verified_fits(const std::string& path, const content_digest& expected, reservation& memory) {
  fits_input input = open_fits_input(path);
  reservation input_memory = memory.carve(input.size_bytes(), "input_read");
  std::vector<std::uint8_t> bytes = read_input_bytes(input, input_memory.bytes());
  require_input_digest(digest_of(bytes), expected, path);
  const fits_plan plan = plan_fits(bytes);
  if (plan.peak_bytes > memory.bytes()) {
    throw stop_error(error_category::budget, "sched.job_exceeds_budget", "picco di memoria oltre la prenotazione",
                     {{"picco", std::to_string(plan.peak_bytes)}, {"prenotati", std::to_string(memory.bytes())}});
  }
  return verified_fits{std::move(input_memory), std::move(bytes)};
}

}  // namespace astro
