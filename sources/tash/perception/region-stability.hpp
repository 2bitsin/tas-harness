#pragma once
// How long each watched region has stood still, in frames.

#include "tash/perception/region.hpp"
#include "tash/perception/rgb565-view.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tash::perception::detail::region_stability
{
  using utilities::Outcome;
  using utilities::Result;

  class RegionStability
  {
  public:
    explicit RegionStability(std::vector<Region> regions);

    // The first frame only seeds the hashes; a count rises from the second.
    auto Observe(Rgb565View const& frame) -> Outcome;

    auto Observe(bus::FrameView const& frame) -> Outcome
    {
      return Observe(ViewOf(frame));
    }

    auto Reset() noexcept -> void;

    [[nodiscard]] auto Count() const noexcept -> std::size_t;
    [[nodiscard]] auto Seeded() const noexcept -> bool;
    [[nodiscard]] auto RegionAt(std::size_t index) const -> Result<Region>;
    [[nodiscard]] auto StableFrames(std::size_t index) const
      -> Result<std::uint32_t>;
    [[nodiscard]] auto LastHash(std::size_t index) const
      -> Result<std::uint64_t>;

  private:
    std::vector<Region>        _regions;
    std::vector<std::uint64_t> _hashes;
    std::vector<std::uint32_t> _stable_frames;
    bool                       _seeded{ false };
  };
}

namespace tash::perception
{
  using detail::region_stability::RegionStability;
}
