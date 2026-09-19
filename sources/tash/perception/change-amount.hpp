#pragma once
// How much two frames of the same geometry differ, on grey. Both numbers are
// fractions in [0, 1], so a threshold means the same thing on any target.

#include "tash/perception/region.hpp"
#include "tash/perception/rgb565-view.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>

namespace tash::perception::detail::change_amount
{
  using utilities::Result;

  inline constexpr double GREY_LEVEL_MAX{ 255.0 };

  struct ChangeAmount
  {
    double changed_ratio{};
    double mean_absolute_difference{};
  };

  // Zero suits an emulator's exact output; grilling 3b leaves the rest open.
  [[nodiscard]] auto ChangeBetween(Rgb565View const& before,
                                   Rgb565View const& after,
                                   std::uint32_t tolerance = 0u)
    -> Result<ChangeAmount>;

  [[nodiscard]] auto ChangeBetween(Rgb565View const& before,
                                   Rgb565View const& after,
                                   Region const& region,
                                   std::uint32_t tolerance = 0u)
    -> Result<ChangeAmount>;

  [[nodiscard]] inline auto ChangeBetween(bus::FrameView const& before,
                                          bus::FrameView const& after,
                                          std::uint32_t tolerance = 0u)
    -> Result<ChangeAmount>
  {
    return ChangeBetween(ViewOf(before), ViewOf(after), tolerance);
  }

  [[nodiscard]] inline auto ChangeBetween(bus::FrameView const& before,
                                          bus::FrameView const& after,
                                          Region const& region,
                                          std::uint32_t tolerance = 0u)
    -> Result<ChangeAmount>
  {
    return ChangeBetween(ViewOf(before), ViewOf(after), region, tolerance);
  }
}

namespace tash::perception
{
  using detail::change_amount::ChangeAmount;
  using detail::change_amount::ChangeBetween;
  using detail::change_amount::GREY_LEVEL_MAX;
}
