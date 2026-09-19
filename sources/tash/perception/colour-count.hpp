#pragma once
// How many pixels of a frame are one colour, within a per-channel tolerance:
// what a ghost, a bar or a lamp is read by.

#include "tash/perception/colour.hpp"
#include "tash/perception/region.hpp"
#include "tash/perception/rgb565-view.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>

namespace tash::perception::detail::colour_count
{
  using utilities::Result;

  // The 5/6/5 channels are widened to eight bits the way the PNG a shot
  // writes is, so a count and a picture of the same crop agree.
  [[nodiscard]] auto ColourCount(Rgb565View const& frame, Colour const& wanted,
                                 std::uint32_t within)
    -> Result<std::uint64_t>;

  [[nodiscard]] auto ColourCount(Rgb565View const& frame, Region const& region,
                                 Colour const& wanted, std::uint32_t within)
    -> Result<std::uint64_t>;

  [[nodiscard]] inline auto ColourCount(bus::FrameView const& frame,
                                        Colour const& wanted,
                                        std::uint32_t within)
    -> Result<std::uint64_t>
  {
    return ColourCount(ViewOf(frame), wanted, within);
  }

  [[nodiscard]] inline auto ColourCount(bus::FrameView const& frame,
                                        Region const& region,
                                        Colour const& wanted,
                                        std::uint32_t within)
    -> Result<std::uint64_t>
  {
    return ColourCount(ViewOf(frame), region, wanted, within);
  }
}

namespace tash::perception
{
  using detail::colour_count::ColourCount;
}
