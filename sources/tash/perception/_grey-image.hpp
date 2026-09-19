#pragma once
// OpenCV stays behind the module: private, so no consumer needs its includes.

#include "tash/perception/rgb565-view.hpp"
#include "tash/utilities/outcome.hpp"

#include <opencv2/core/mat.hpp>

namespace tash::perception::detail::grey_image
{
  using utilities::Result;

  [[nodiscard]] auto GreyOf(Rgb565View const& frame) -> Result<cv::Mat>;

  [[nodiscard]] auto GreyScaledTo(Rgb565View const& frame, int width,
                                  int height) -> Result<cv::Mat>;
}
