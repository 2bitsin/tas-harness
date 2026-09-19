#include "_grey-image.hpp"

#include "_packed-image.hpp"

#include <opencv2/imgproc.hpp>

namespace tash::perception::detail::grey_image
{
  using utilities::Result;
  using utilities::Forwarded;

  namespace
  {
    // libretro's RGB565 word is RRRRRGGGGGGBBBBB in native order, which is
    // what OpenCV calls BGR565; its grey conversion carries the 601 weights.
    constexpr auto RGB565_TO_GREY{ cv::COLOR_BGR5652GRAY };
  }

  auto GreyOf(Rgb565View const& frame) -> Result<cv::Mat>
  {
    Result<cv::Mat> const packed{ detail::packed_image::PackedOf(frame) };
    if (!packed)
      return Forwarded(packed);
    cv::Mat grey;
    cv::cvtColor(*packed, grey, RGB565_TO_GREY);
    return grey;
  }

  auto GreyScaledTo(Rgb565View const& frame, int width, int height)
    -> Result<cv::Mat>
  {
    auto const grey{ GreyOf(frame) };
    if (!grey)
      return Forwarded(grey);
    cv::Mat scaled;
    cv::resize(*grey, scaled, cv::Size{ width, height }, 0.0, 0.0,
               cv::INTER_AREA);
    return scaled;
  }
}
