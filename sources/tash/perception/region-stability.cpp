#include "tash/perception/region-stability.hpp"

#include "tash/perception/frame-hash.hpp"

#include <utility>

namespace tash::perception::detail::region_stability
{
  using utilities::Outcome;
  using utilities::Result;
  using utilities::Refused;
  using utilities::Forwarded;

  namespace
  {
    [[nodiscard]] auto Within(std::size_t index, std::size_t count) -> Outcome
    {
      if (index >= count)
        return Refused("perception: no region {} among {}", index, count);
      return Outcome{};
    }
  }

  RegionStability::RegionStability(std::vector<Region> regions)
  : _regions{ std::move(regions) }, _hashes(_regions.size()),
    _stable_frames(_regions.size())
  {
  }

  auto RegionStability::Observe(Rgb565View const& frame) -> Outcome
  {
    std::vector<std::uint64_t> seen(_regions.size());
    for (std::size_t index{ 0 }; index < _regions.size(); ++index)
    {
      auto const hash{ ExactHash(frame, _regions[index]) };
      if (!hash)
        return Forwarded(hash);
      seen[index] = *hash;
    }
    for (std::size_t index{ 0 }; index < _regions.size(); ++index)
    {
      _stable_frames[index] = _seeded && seen[index] == _hashes[index]
                                ? _stable_frames[index] + 1u
                                : 0u;
      _hashes[index] = seen[index];
    }
    _seeded = true;
    return Outcome{};
  }

  auto RegionStability::Reset() noexcept -> void
  {
    _hashes.assign(_regions.size(), 0u);
    _stable_frames.assign(_regions.size(), 0u);
    _seeded = false;
  }

  auto RegionStability::Count() const noexcept -> std::size_t
  {
    return _regions.size();
  }

  auto RegionStability::Seeded() const noexcept -> bool
  {
    return _seeded;
  }

  auto RegionStability::RegionAt(std::size_t index) const -> Result<Region>
  {
    auto const within{ Within(index, _regions.size()) };
    if (!within)
      return Forwarded(within);
    return _regions[index];
  }

  auto RegionStability::StableFrames(std::size_t index) const
    -> Result<std::uint32_t>
  {
    auto const within{ Within(index, _regions.size()) };
    if (!within)
      return Forwarded(within);
    return _stable_frames[index];
  }

  auto RegionStability::LastHash(std::size_t index) const
    -> Result<std::uint64_t>
  {
    auto const within{ Within(index, _regions.size()) };
    if (!within)
      return Forwarded(within);
    return _hashes[index];
  }
}
