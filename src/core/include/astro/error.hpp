#pragma once

#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace astro {

enum class error_category {
  invalid_input,
  limit_exceeded,
  metadata_missing,
  incompatible,
  unsupported,
  numeric,
  io,
  integrity,
  schema,
  graph,
  stale_generation,
  budget,
  cancelled,
  resume_mismatch,
  security,
};

const char* to_string(error_category category) noexcept;

class stop_error : public std::exception {
 public:
  using context_list = std::vector<std::pair<std::string, std::string>>;

  stop_error(error_category category, std::string code, std::string message,
             context_list context = {});

  error_category category() const noexcept { return category_; }
  const std::string& code() const noexcept { return code_; }
  const std::string& message() const noexcept { return message_; }
  const context_list& context() const noexcept { return context_; }
  const char* what() const noexcept override { return what_.c_str(); }

  void add_context(std::string key, std::string value);

 private:
  void rebuild_what();

  error_category category_;
  std::string code_;
  std::string message_;
  context_list context_;
  std::string what_;
};

namespace detail {
[[noreturn]] void assert_failed(const char* expression, const char* file, int line) noexcept;
}

}  // namespace astro

#define ASTRO_ASSERT(cond) \
  ((cond) ? static_cast<void>(0) : ::astro::detail::assert_failed(#cond, __FILE__, __LINE__))
