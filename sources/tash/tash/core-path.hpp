#pragma once

#include "tash/utilities/outcome.hpp"

#include <filesystem>
#include <string_view>

namespace tash::cli::detail::core_path
{
  using utilities::Result;

  // A bare name is the core beside the binary; anything with a separator or
  // a suffix is taken as the path it is.
  [[nodiscard]] auto ResolvedCore(std::string_view named)
    -> Result<std::filesystem::path>;
}

namespace tash::cli
{
  using detail::core_path::ResolvedCore;
}
