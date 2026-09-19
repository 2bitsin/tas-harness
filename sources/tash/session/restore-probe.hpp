#pragma once
// The check the harness makes of its own restores: the line from a
// checkpoint against the same line played again after restoring it. Running
// it leaves the run standing where the checkpoint was taken.

#include "tash/session/session.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace tash::session::detail::restore_probe
{
  using utilities::Result;

  // Two seconds: on the unpatched core WWF's restored line parts from the
  // straight one at frame 7, and every checkpoint pays twice over.
  inline constexpr std::uint64_t PROBE_FRAMES{ 120 };

  // One probe a checkpoint, the ones that parted, and the checkpoints taken
  // without a probe because they are consumed where they are taken.
  struct ProbeCounts
  {
    std::uint64_t probed{ 0 };
    std::uint64_t parted{ 0 };
    std::uint64_t unprobed{ 0 };

    auto operator == (ProbeCounts const&) const -> bool = default;
  };

  struct RestoreDifference
  {
    std::uint64_t compared{ 0 };

    // Counted from the checkpoint, so the first frame played is frame 1.
    std::optional<std::uint64_t> parted_at{ };

    auto operator == (RestoreDifference const&) const -> bool = default;
  };

  [[nodiscard]] auto FirstDifference(std::span<std::uint64_t const> one,
                                     std::span<std::uint64_t const> other)
    -> RestoreDifference;

  class RestoreProbe
  {
  public:
    RestoreProbe(Session& live, Checkpoint kept);

    [[nodiscard]] auto Run() -> Result<RestoreDifference>;

  private:
    [[nodiscard]] auto Line() -> Result<std::vector<std::uint64_t>>;

    Session&   _live;
    Checkpoint _kept;
  };
}

namespace tash::session
{
  using detail::restore_probe::FirstDifference;
  using detail::restore_probe::PROBE_FRAMES;
  using detail::restore_probe::ProbeCounts;
  using detail::restore_probe::RestoreDifference;
  using detail::restore_probe::RestoreProbe;
}
