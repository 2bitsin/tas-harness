#pragma once

#include "tash/utilities/outcome.hpp"

#include <filesystem>

namespace tash::utilities::detail::executable_directory
{
  // What `lib/` and a core beside the binary are relative to.
  [[nodiscard]] auto ExecutableDirectory() -> Result<std::filesystem::path>;
}

namespace tash::utilities
{
  using detail::executable_directory::ExecutableDirectory;
}
