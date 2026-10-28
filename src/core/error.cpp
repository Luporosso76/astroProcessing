#include "astro/error.hpp"

#include <cstdio>
#include <cstdlib>

namespace astro {

const char* to_string(error_category category) noexcept {
  switch (category) {
    case error_category::invalid_input: return "invalid_input";
    case error_category::limit_exceeded: return "limit_exceeded";
    case error_category::metadata_missing: return "metadata_missing";
    case error_category::incompatible: return "incompatible";
    case error_category::unsupported: return "unsupported";
    case error_category::numeric: return "numeric";
    case error_category::io: return "io";
    case error_category::integrity: return "integrity";
    case error_category::schema: return "schema";
    case error_category::graph: return "graph";
    case error_category::stale_generation: return "stale_generation";
    case error_category::budget: return "budget";
    case error_category::cancelled: return "cancelled";
    case error_category::resume_mismatch: return "resume_mismatch";
    case error_category::security: return "security";
  }
  std::abort();
}

stop_error::stop_error(error_category category, std::string code, std::string message,
                       context_list context)
    : category_(category),
      code_(std::move(code)),
      message_(std::move(message)),
      context_(std::move(context)) {
  rebuild_what();
}

void stop_error::add_context(std::string key, std::string value) {
  context_.emplace_back(std::move(key), std::move(value));
  rebuild_what();
}

void stop_error::rebuild_what() {
  what_ = std::string(to_string(category_)) + " " + code_ + ": " + message_;
  for (const auto& [key, value] : context_) what_ += " [" + key + "=" + value + "]";
}

namespace detail {

void assert_failed(const char* expression, const char* file, int line) noexcept {
  std::fprintf(stderr, "ASTRO_ASSERT fallita: %s (%s:%d)\n", expression, file, line);
  std::abort();
}

}  // namespace detail

}  // namespace astro
