#include "tash/perception/change-amount.hpp"

#include "_checked-view.hpp"
#include "_grey-image.hpp"
#include "_opencv-guard.hpp"

#include <opencv2/core.hpp>

namespace tash::perception::detail::change_amount
{
  using utilities::Result;
  using utilities::Refused;
  using utilities::Forwarded;

  auto ChangeBetween(Rgb565View const& before, Rgb565View const& after,
                     std::uint32_t tolerance) -> Result<ChangeAmount>
  {
    return detail::opencv_guard::Guarded([&]() -> Result<ChangeAmount> {
      if (before.width != after.width || before.height != after.height)
        return Refused(
          "perception: a {}x{} frame and a {}x{} one are not comparable",
          before.width, before.height, after.width, after.height);

      auto const earlier{ detail::grey_image::GreyOf(before) };
      if (!earlier)
        return Forwarded(earlier);
      auto const later{ detail::grey_image::GreyOf(after) };
      if (!later)
        return Forwarded(later);

      cv::Mat difference;
      cv::absdiff(*earlier, *later, difference);
      auto const pixels{ static_cast<double>(before.width) * before.height };
      auto const changed{
        cv::countNonZero(difference > static_cast<int>(tolerance)) };
      return ChangeAmount{ static_cast<double>(changed) / pixels,
                           cv::sum(difference)[0] / pixels / GREY_LEVEL_MAX };
    });
  }

  auto ChangeBetween(Rgb565View const& before, Rgb565View const& after,
                     Region const& region, std::uint32_t tolerance)
    -> Result<ChangeAmount>
  {
    auto const earlier{ detail::checked_view::Cropped(before, region) };
    if (!earlier)
      return Forwarded(earlier);
    auto const later{ detail::checked_view::Cropped(after, region) };
    if (!later)
      return Forwarded(later);
    return ChangeBetween(*earlier, *later, tolerance);
  }
}
