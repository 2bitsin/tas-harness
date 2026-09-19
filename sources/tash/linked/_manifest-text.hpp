#pragma once
// The run.yaml a recording leaves beside its tape and its trace, so the
// directory is a bundle every tool that reads one can open.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace tash::linked::detail::manifest_text
{
  struct Recorded
  {
    std::string_view             target{ };
    std::string_view             profile{ };
    std::uint64_t                frames{ 0 };
    double                       fps{ 0.0 };
    std::span<std::string const> watches{ };
    std::string_view             outcome{ };
  };

  [[nodiscard]] auto ManifestText(Recorded const& recorded) -> std::string;
}
