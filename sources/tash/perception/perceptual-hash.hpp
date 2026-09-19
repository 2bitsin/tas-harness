#pragma once
// Two 64-bit hashes over a grey downscale: a 9x8 horizontal gradient, and
// the 8x8 DCT corner of a 32x32 at the median of its 63 non-DC terms, whose
// own bit stays clear. Both are MSB first, the first bit compared in bit 63.

#include "tash/perception/region.hpp"
#include "tash/perception/rgb565-view.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>

namespace tash::perception::detail::perceptual_hash
{
  using utilities::Result;

  [[nodiscard]] auto DifferenceHash(Rgb565View const& frame)
    -> Result<std::uint64_t>;

  [[nodiscard]] auto DifferenceHash(Rgb565View const& frame,
                                    Region const& region)
    -> Result<std::uint64_t>;

  [[nodiscard]] auto PerceptualHash(Rgb565View const& frame)
    -> Result<std::uint64_t>;

  [[nodiscard]] auto PerceptualHash(Rgb565View const& frame,
                                    Region const& region)
    -> Result<std::uint64_t>;

  [[nodiscard]] inline auto DifferenceHash(bus::FrameView const& frame)
    -> Result<std::uint64_t>
  {
    return DifferenceHash(ViewOf(frame));
  }

  [[nodiscard]] inline auto DifferenceHash(bus::FrameView const& frame,
                                           Region const& region)
    -> Result<std::uint64_t>
  {
    return DifferenceHash(ViewOf(frame), region);
  }

  [[nodiscard]] inline auto PerceptualHash(bus::FrameView const& frame)
    -> Result<std::uint64_t>
  {
    return PerceptualHash(ViewOf(frame));
  }

  [[nodiscard]] inline auto PerceptualHash(bus::FrameView const& frame,
                                           Region const& region)
    -> Result<std::uint64_t>
  {
    return PerceptualHash(ViewOf(frame), region);
  }
}

namespace tash::perception
{
  using detail::perceptual_hash::DifferenceHash;
  using detail::perceptual_hash::PerceptualHash;
}
