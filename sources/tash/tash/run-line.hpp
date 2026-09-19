#pragma once
// The line a run ends with, so a replay counts its frames the same way.

#include "tash/clock/determinism.hpp"
#include "tash/session/restore-probe.hpp"
#include "tash/session/session.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <string>

namespace tash::cli::detail::run_line
{
  // A run the clock timed at nothing still has to divide.
  inline constexpr double LEAST_SECONDS{ 1e-9 };

  [[nodiscard]] inline auto RanLine(session::Session const& run,
                                    std::uint64_t kept,
                                    session::ProbeCounts probes)
    -> std::string
  {
    return std::format(
      "ran    {} frames ({} kept, probed {} checkpoints, {} parted, {} "
      "unprobed) in {:.2f} s, {:.1f} fps, {:.2f}x real time ({:.4f} fps "
      "emulated, {})\n",
      run.Frames(), kept, probes.probed, probes.parted, probes.unprobed,
      run.WallSeconds(),
      run.Frames() / std::max(run.WallSeconds(), LEAST_SECONDS),
      run.AchievedRate(), run.Fps(), clock::Named(run.DeterminismLevel()));
  }
}

namespace tash::cli
{
  using detail::run_line::RanLine;
}
