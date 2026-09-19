#pragma once
// The borrowed frame as OpenCV reads it: two bytes a pixel, the view's own
// pitch for the step, so the padding a row carries is never copied.

#include "_checked-view.hpp"
#include "tash/perception/rgb565-view.hpp"
#include "tash/utilities/outcome.hpp"

#include <opencv2/core/mat.hpp>

#include <cstddef>
#include <cstdint>

namespace tash::perception::detail::packed_image
{
  using utilities::Result;

  [[nodiscard]] inline auto PackedOf(Rgb565View const& frame)
    -> Result<cv::Mat>
  {
    Result<Rgb565View> const checked{ detail::checked_view::Checked(frame) };
    if (!checked)
      return utilities::Forwarded(checked);
    return cv::Mat{ static_cast<int>(frame.height),
                    static_cast<int>(frame.width), CV_8UC2,
                    const_cast<std::uint8_t*>(frame.bytes.data()),
                    static_cast<std::size_t>(frame.pitch) };
  }
}
