#pragma once

#include <string>

namespace astro {

struct BuildInfo {
  std::string version;
  std::string compiler;
  std::string build_type;
  std::string flags;
  long cplusplus;
};

BuildInfo build_info();

}  // namespace astro
