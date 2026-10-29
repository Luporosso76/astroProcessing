#include "astro/hash.hpp"

#include <openssl/evp.h>

#include <algorithm>
#include <memory>

#include "astro/error.hpp"
#include "astro/fits.hpp"

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

std::vector<std::uint8_t> read_verified_fits(const std::string& path, const content_digest& expected,
                                             std::uint64_t budget_bytes) {
  fits_input input = open_fits_input(path);
  std::vector<std::uint8_t> bytes = read_input_bytes(input, budget_bytes);
  const content_digest actual = digest_of(bytes);
  if (actual != expected) {
    throw stop_error(error_category::integrity, "input.digest_mismatch",
                     "digest o dimensione del file diversi dal record",
                     {{"percorso", path},
                      {"atteso", expected.sha256 + ":" + std::to_string(expected.size_bytes)},
                      {"letto", actual.sha256 + ":" + std::to_string(actual.size_bytes)}});
  }
  const fits_plan plan = plan_fits(bytes);
  if (plan.peak_bytes > budget_bytes) {
    throw stop_error(error_category::budget, "sched.job_exceeds_budget", "picco di memoria oltre il budget",
                     {{"picco", std::to_string(plan.peak_bytes)}, {"budget", std::to_string(budget_bytes)}});
  }
  return bytes;
}

}  // namespace astro
