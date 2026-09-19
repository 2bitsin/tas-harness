#include "tash/perception/perceptual-hash.hpp"

#include "_checked-view.hpp"
#include "_grey-image.hpp"
#include "_opencv-guard.hpp"

#include <opencv2/core.hpp>

#include <algorithm>
#include <array>
#include <cstddef>

namespace tash::perception::detail::perceptual_hash
{
  using utilities::Result;
  using utilities::Forwarded;

  namespace
  {
    using detail::checked_view::Cropped;
    using detail::grey_image::GreyScaledTo;
    using detail::opencv_guard::Guarded;

    constexpr int         HASH_SIDE{ 8 };
    constexpr int         HASH_BITS{ HASH_SIDE * HASH_SIDE };
    constexpr int         DIFFERENCE_WIDTH{ HASH_SIDE + 1 };
    constexpr int         DCT_SIDE{ 32 };
    constexpr std::size_t DCT_KEPT{ HASH_BITS - 1 };

    [[nodiscard]] constexpr auto BitAt(int index) noexcept -> std::uint64_t
    {
      return std::uint64_t{ 1 } << (HASH_BITS - 1 - index);
    }
  }

  auto DifferenceHash(Rgb565View const& frame) -> Result<std::uint64_t>
  {
    return Guarded([&frame]() -> Result<std::uint64_t> {
      auto const scaled{ GreyScaledTo(frame, DIFFERENCE_WIDTH, HASH_SIDE) };
      if (!scaled)
        return Forwarded(scaled);
      std::uint64_t bits{ 0 };
      int           index{ 0 };
      for (int y{ 0 }; y < HASH_SIDE; ++y)
        for (int x{ 0 }; x < HASH_SIDE; ++x, ++index)
          if (scaled->at<std::uint8_t>(y, x)
              > scaled->at<std::uint8_t>(y, x + 1))
            bits |= BitAt(index);
      return bits;
    });
  }

  auto DifferenceHash(Rgb565View const& frame, Region const& region)
    -> Result<std::uint64_t>
  {
    auto const window{ Cropped(frame, region) };
    if (!window)
      return Forwarded(window);
    return DifferenceHash(*window);
  }

  auto PerceptualHash(Rgb565View const& frame) -> Result<std::uint64_t>
  {
    return Guarded([&frame]() -> Result<std::uint64_t> {
      auto const scaled{ GreyScaledTo(frame, DCT_SIDE, DCT_SIDE) };
      if (!scaled)
        return Forwarded(scaled);
      cv::Mat real;
      scaled->convertTo(real, CV_32F);
      cv::Mat frequencies;
      cv::dct(real, frequencies);

      std::array<float, DCT_KEPT> kept{};
      std::size_t                 taken{ 0 };
      for (int y{ 0 }; y < HASH_SIDE; ++y)
        for (int x{ 0 }; x < HASH_SIDE; ++x)
          if (y != 0 || x != 0)
            kept[taken++] = frequencies.at<float>(y, x);

      auto       ordered{ kept };
      auto const middle{ ordered.begin() + DCT_KEPT / 2 };
      std::nth_element(ordered.begin(), middle, ordered.end());
      auto const median{ *middle };

      std::uint64_t bits{ 0 };
      taken = 0;
      for (int y{ 0 }; y < HASH_SIDE; ++y)
        for (int x{ 0 }; x < HASH_SIDE; ++x)
          if (y != 0 || x != 0)
            if (kept[taken++] > median)
              bits |= BitAt(y * HASH_SIDE + x);
      return bits;
    });
  }

  auto PerceptualHash(Rgb565View const& frame, Region const& region)
    -> Result<std::uint64_t>
  {
    auto const window{ Cropped(frame, region) };
    if (!window)
      return Forwarded(window);
    return PerceptualHash(*window);
  }
}
