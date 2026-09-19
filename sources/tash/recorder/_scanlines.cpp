#include "tash/recorder/_scanlines.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace tash::recorder::detail::scanlines
{
  auto Darken(std::span<std::uint8_t> luma, std::uint32_t pitch,
              std::uint32_t width, std::uint32_t height, std::uint32_t scale,
              double fraction) -> void
  {
    // Below a scale of 2 the upscale left no spare row to give to a line.
    if (scale < 2u || pitch < width || !(fraction > 0.0))
      return;
    double const kept{ std::clamp(1.0 - fraction, 0.0, 1.0) };

    for (std::uint32_t y{ scale - 1u }; y < height; y += scale)
    {
      std::size_t const row{ std::size_t{ pitch } * y };
      if (row + width > luma.size())
        return;
      for (std::uint32_t x{ 0 }; x < width; ++x)
      {
        std::uint8_t const was{ luma[row + x] };
        // swscale writes YUV in the studio range, where black is 16.
        if (was <= STUDIO_BLACK)
          continue;
        auto const above{ static_cast<double>(was - STUDIO_BLACK) };
        luma[row + x] = static_cast<std::uint8_t>(
          STUDIO_BLACK + std::lround(above * kept));
      }
    }
  }
}
