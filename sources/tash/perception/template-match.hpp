#pragma once
// Where a pattern sits in a frame, by normalised cross-correlation on grey.
// TM_CCOEFF_NORMED, so the score runs -1 to 1 and 1 is an exact match; the
// position is the pattern's top left corner in the frame's own pixels.

#include "tash/perception/region.hpp"
#include "tash/perception/rgb565-view.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>

namespace tash::perception::detail::template_match
{
  using utilities::Result;

  struct TemplateMatch
  {
    double        score{};
    std::uint32_t x{};
    std::uint32_t y{};
  };

  [[nodiscard]] auto BestMatch(Rgb565View const& frame,
                               Rgb565View const& pattern)
    -> Result<TemplateMatch>;

  [[nodiscard]] auto BestMatch(Rgb565View const& frame,
                               Rgb565View const& pattern,
                               Region const& within)
    -> Result<TemplateMatch>;

  [[nodiscard]] inline auto BestMatch(bus::FrameView const& frame,
                                      Rgb565View const& pattern)
    -> Result<TemplateMatch>
  {
    return BestMatch(ViewOf(frame), pattern);
  }

  [[nodiscard]] inline auto BestMatch(bus::FrameView const& frame,
                                      Rgb565View const& pattern,
                                      Region const& within)
    -> Result<TemplateMatch>
  {
    return BestMatch(ViewOf(frame), pattern, within);
  }
}

namespace tash::perception
{
  using detail::template_match::BestMatch;
  using detail::template_match::TemplateMatch;
}
