#pragma once
// One wording for the two ways a caller hands over pixels that are not there.

#include "tash/perception/region.hpp"
#include "tash/perception/rgb565-view.hpp"
#include "tash/utilities/outcome.hpp"

namespace tash::perception::detail::checked_view
{
  using utilities::Result;
  using utilities::Refused;

  [[nodiscard]] inline auto Checked(Rgb565View const& frame)
    -> Result<Rgb565View>
  {
    if (!frame.Valid())
      return Refused(
        "perception: a {}x{} frame of pitch {} is not {} bytes of pixels",
        frame.width, frame.height, frame.pitch, frame.bytes.size());
    return frame;
  }

  [[nodiscard]] inline auto Cropped(Rgb565View const& frame,
                                    Region const& region) -> Result<Rgb565View>
  {
    if (!region.FitsIn(frame.width, frame.height))
      return Refused(
        "perception: region {}x{}+{}+{} does not fit a {}x{} frame",
        region.width, region.height, region.x, region.y, frame.width,
        frame.height);
    return Checked(frame.Crop(region));
  }
}
