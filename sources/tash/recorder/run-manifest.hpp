#pragma once
// What a run bundle says about itself: enough to repeat the run.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tash::recorder::detail::run_manifest
{
  inline constexpr std::uint64_t EVERY_FRAME{ 1 };

  // oxbox writes a std::filesystem::path member but does not read one
  // back, so a path in a manifest is a string.
  struct RunManifest
  {
    friend constexpr auto reflect_scheme(RunManifest*);

    std::string   harness_version{ };
    std::string   core_name{ };
    std::string   core_version{ };
    std::string   rom{ };
    std::string   profile{ };
    std::uint64_t frames{ 0 };
    double        fps{ 0.0 };
    std::string   determinism{ };
    std::string   outcome{ };

    // Absent, not zero or empty, so a bundle written before one still reads.
    std::optional<std::uint64_t>            kept{ };

    std::optional<std::uint64_t>            probed{ };
    std::optional<std::uint64_t>            parted{ };

    // Unprobed: consumed where it was taken, so a probe had nothing to prove.
    std::optional<std::uint64_t>            unprobed{ };

    std::optional<std::vector<std::string>> watches{ };
    std::optional<std::string>              tape{ };

    // One frame in this many was encoded; above one there is no audio.
    std::optional<std::uint64_t>            video_stride{ };

    std::optional<std::string>              replay_of{ };

    auto operator == (RunManifest const&) const -> bool = default;
  };
}

namespace tash::recorder
{
  using detail::run_manifest::EVERY_FRAME;
  using detail::run_manifest::RunManifest;
}
