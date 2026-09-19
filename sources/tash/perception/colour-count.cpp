#include "tash/perception/colour-count.hpp"

#include "_checked-view.hpp"
#include "_opencv-guard.hpp"
#include "_packed-image.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>

namespace tash::perception::detail::colour_count
{
  using utilities::Forwarded;
  using utilities::Refused;

  namespace
  {
    // libretro's RGB565 word is what OpenCV calls BGR565, and the expansion
    // this conversion makes is the one a PNG of the frame holds.
    constexpr auto RGB565_TO_COLOUR{ cv::COLOR_BGR5652BGR };

    [[nodiscard]] auto Lowest(Colour const& wanted, std::uint32_t within)
      -> cv::Scalar
    {
      auto const under{ [within](std::uint32_t channel)
        { return static_cast<double>(channel > within ? channel - within
                                                      : 0u); } };
      return cv::Scalar{ under(wanted.blue), under(wanted.green),
                         under(wanted.red) };
    }

    [[nodiscard]] auto Highest(Colour const& wanted, std::uint32_t within)
      -> cv::Scalar
    {
      auto const over{ [within](std::uint32_t channel)
        { return static_cast<double>(std::min(channel + within,
                                              CHANNEL_MAX)); } };
      return cv::Scalar{ over(wanted.blue), over(wanted.green),
                         over(wanted.red) };
    }
  }

  auto ColourCount(Rgb565View const& frame, Colour const& wanted,
                   std::uint32_t within) -> Result<std::uint64_t>
  {
    return detail::opencv_guard::Guarded([&]() -> Result<std::uint64_t> {
      if (!wanted.InRange())
        return Refused("perception: {},{},{} is not a colour, whose channels"
                       " run 0 to {}", wanted.red, wanted.green, wanted.blue,
                       CHANNEL_MAX);

      Result<cv::Mat> const packed{ detail::packed_image::PackedOf(frame) };
      if (!packed)
        return Forwarded(packed);

      cv::Mat colour;
      cv::cvtColor(*packed, colour, RGB565_TO_COLOUR);
      cv::Mat matched;
      cv::inRange(colour, Lowest(wanted, within), Highest(wanted, within),
                  matched);
      return static_cast<std::uint64_t>(cv::countNonZero(matched));
    });
  }

  auto ColourCount(Rgb565View const& frame, Region const& region,
                   Colour const& wanted, std::uint32_t within)
    -> Result<std::uint64_t>
  {
    Result<Rgb565View> const window{
      detail::checked_view::Cropped(frame, region) };
    if (!window)
      return Forwarded(window);
    return ColourCount(*window, wanted, within);
  }
}
