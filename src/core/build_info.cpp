#include "astro/build_info.hpp"

namespace astro::detail {
extern const char* const kProjectVersion;
extern const char* const kCompiler;
extern const char* const kBuildFlags;
extern const char* const kBuildType;
extern const char* const kSourceRevision;
extern const bool kSourceDirtyKnown;
extern const bool kSourceDirty;
}  // namespace astro::detail

namespace astro {

BuildInfo build_info() {
  return BuildInfo{detail::kProjectVersion, detail::kCompiler, detail::kBuildType,
                   detail::kBuildFlags, __cplusplus, detail::kSourceRevision,
                   detail::kSourceDirtyKnown ? std::optional<bool>(detail::kSourceDirty) : std::nullopt};
}

}  // namespace astro
