#include "tash/utilities/executable-directory.hpp"

namespace tash::utilities::detail::executable_directory
{
  auto ExecutableDirectory() -> Result<std::filesystem::path>
  {
    std::error_code failure;
    // Linux only, which is where tash runs (grilling 4); oxbox has no answer.
    auto const program{ std::filesystem::canonical("/proc/self/exe", failure) };
    if (failure)
      return Refused("tash: cannot read /proc/self/exe: {}", failure.message());
    return program.parent_path();
  }
}
