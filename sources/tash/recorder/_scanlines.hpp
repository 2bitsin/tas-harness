#pragma once
// The scanline pass a film gets: rows taken a fraction of the way to black.

#include <cstdint>
#include <span>

namespace tash::recorder::detail::scanlines
{
  inline constexpr std::uint8_t STUDIO_BLACK{ 16 };

  auto Darken(std::span<std::uint8_t> luma, std::uint32_t pitch,
              std::uint32_t width, std::uint32_t height, std::uint32_t scale,
              double fraction) -> void;
}
