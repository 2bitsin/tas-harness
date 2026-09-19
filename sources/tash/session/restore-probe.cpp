#include "tash/session/restore-probe.hpp"

#include "tash/bus/frame-ring.hpp"
#include "tash/perception/frame-hash.hpp"

#include <algorithm>

namespace tash::session::detail::restore_probe
{
  using utilities::Forwarded;
  using utilities::Outcome;
  using utilities::Refused;

  auto FirstDifference(std::span<std::uint64_t const> one,
                       std::span<std::uint64_t const> other)
    -> RestoreDifference
  {
    auto const compared{ std::min(one.size(), other.size()) };
    for (std::size_t at{ 0 }; at < compared; ++at)
      if (one[at] != other[at])
        return RestoreDifference{ compared, at + 1u };
    if (one.size() != other.size())
      return RestoreDifference{ compared, compared + 1u };
    return RestoreDifference{ compared, std::nullopt };
  }

  RestoreProbe::RestoreProbe(Session& live, Checkpoint kept)
  : _live{ live }, _kept{ kept }
  {
  }

  auto RestoreProbe::Run() -> Result<RestoreDifference>
  {
    Result<std::vector<std::uint64_t>> const straight{ Line() };
    if (!straight)
      return Forwarded(straight);
    if (Outcome const back{ _live.Restore(_kept) }; !back)
      return Forwarded(back);
    Result<std::vector<std::uint64_t>> const restored{ Line() };
    if (!restored)
      return Forwarded(restored);
    if (Outcome const back{ _live.Restore(_kept) }; !back)
      return Forwarded(back);
    return FirstDifference(*straight, *restored);
  }

  auto RestoreProbe::Line() -> Result<std::vector<std::uint64_t>>
  {
    std::vector<std::uint64_t> hashes;
    hashes.reserve(PROBE_FRAMES);
    for (std::uint64_t step{ 0 }; step < PROBE_FRAMES; ++step)
    {
      _live.Step(1);
      std::optional<bus::FrameView> const shown{ _live.Video().Latest() };
      if (!shown)
        return Refused("session: the restore probe stepped frame {} of {} "
                       "and the core drew nothing", step + 1u, PROBE_FRAMES);
      Result<std::uint64_t> const hash{ perception::ExactHash(*shown) };
      if (!hash)
        return Forwarded(hash);
      hashes.push_back(*hash);
    }
    return hashes;
  }
}
