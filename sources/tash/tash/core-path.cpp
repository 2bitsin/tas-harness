#include "tash/tash/core-path.hpp"

#include "tash/utilities/executable-directory.hpp"

#include <format>
#include <string>
#include <vector>

namespace tash::cli::detail::core_path
{
  using utilities::Result;
  using utilities::Refused;

  auto ResolvedCore(std::string_view named) -> Result<std::filesystem::path>
  {
    if (named.empty())
      return Refused("no core named");

    std::filesystem::path const asked{ named };
    if (asked.has_parent_path() || asked.extension() == ".so")
    {
      if (!std::filesystem::is_regular_file(asked))
        return Refused("no core at {}", asked.string());
      return std::filesystem::canonical(asked);
    }

    Result<std::filesystem::path> const beside{
      utilities::ExecutableDirectory() };
    if (!beside)
      return std::unexpected{ beside.error() };

    std::string const file{ std::format("{}_libretro.so", named) };
    std::vector<std::filesystem::path> const tried{ *beside / "lib" / file,
                                                    *beside / file };
    for (auto const& candidate : tried)
      if (std::filesystem::is_regular_file(candidate))
        return candidate;
    return Refused("no core called {}: neither {} nor {} is there", named,
                   tried.front().string(), tried.back().string());
  }
}
