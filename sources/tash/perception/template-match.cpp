#include "tash/perception/template-match.hpp"

#include "_checked-view.hpp"
#include "_grey-image.hpp"
#include "_opencv-guard.hpp"

#include <opencv2/imgproc.hpp>

namespace tash::perception::detail::template_match
{
  using utilities::Result;
  using utilities::Refused;
  using utilities::Forwarded;

  auto BestMatch(Rgb565View const& frame, Rgb565View const& pattern)
    -> Result<TemplateMatch>
  {
    return detail::opencv_guard::Guarded([&]() -> Result<TemplateMatch> {
      if (pattern.width > frame.width || pattern.height > frame.height)
        return Refused(
          "perception: a {}x{} pattern does not fit a {}x{} frame",
          pattern.width, pattern.height, frame.width, frame.height);

      auto const haystack{ detail::grey_image::GreyOf(frame) };
      if (!haystack)
        return Forwarded(haystack);
      auto const needle{ detail::grey_image::GreyOf(pattern) };
      if (!needle)
        return Forwarded(needle);

      cv::Mat scores;
      cv::matchTemplate(*haystack, *needle, scores, cv::TM_CCOEFF_NORMED);
      double    best{ 0.0 };
      cv::Point at;
      cv::minMaxLoc(scores, nullptr, &best, nullptr, &at);
      return TemplateMatch{ best, static_cast<std::uint32_t>(at.x),
                            static_cast<std::uint32_t>(at.y) };
    });
  }

  auto BestMatch(Rgb565View const& frame, Rgb565View const& pattern,
                 Region const& within) -> Result<TemplateMatch>
  {
    auto const window{ detail::checked_view::Cropped(frame, within) };
    if (!window)
      return Forwarded(window);
    auto found{ BestMatch(*window, pattern) };
    if (!found)
      return found;
    found->x += within.x;
    found->y += within.y;
    return found;
  }
}
