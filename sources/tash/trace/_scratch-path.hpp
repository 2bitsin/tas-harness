#pragma once

// A directory of its own under the system temp area for one test, and gone
// again when the test is.

#include <cstdint>
#include <filesystem>
#include <format>
#include <random>
#include <string_view>
#include <system_error>

namespace tash::trace::detail::scratch_path
{
  class ScratchPath
  {
  public:
    ScratchPath()
    {
      std::random_device source{ };
      auto const stamp{ (static_cast<std::uint64_t>(source()) << 32)
                        | source() };
      _root = std::filesystem::temp_directory_path()
              / std::format("tash-trace-{:016x}", stamp);
      std::filesystem::create_directories(_root);
    }

    ScratchPath(ScratchPath const&) = delete;
    auto operator = (ScratchPath const&) -> ScratchPath& = delete;
    ScratchPath(ScratchPath&&) = delete;
    auto operator = (ScratchPath&&) -> ScratchPath& = delete;

    ~ScratchPath()
    {
      std::error_code ignored{ };
      std::filesystem::remove_all(_root, ignored);
    }

    [[nodiscard]] auto File(std::string_view name) const
      -> std::filesystem::path
    { return _root / name; }

  private:
    std::filesystem::path _root{ };
  };
}
