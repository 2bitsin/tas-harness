#pragma once
#include <string_view>

namespace tash::utilities::detail::version
{
  // A placeholder until the build driver hands a version to the code.
  inline constexpr std::string_view HARNESS_VERSION{ "0.0.0" };
}

namespace tash::utilities
{
  using detail::version::HARNESS_VERSION;
}
