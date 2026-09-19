#pragma once
// A rectangle in pixels, the unit every per-region call is scoped by.

#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <string_view>

namespace tash::perception::detail::region
{
  using utilities::Result;

  inline constexpr char SEPARATOR{ ',' };

  struct Region
  {
    std::uint32_t x{};
    std::uint32_t y{};
    std::uint32_t width{};
    std::uint32_t height{};

    [[nodiscard]] constexpr auto Empty() const noexcept -> bool
    {
      return width == 0u || height == 0u;
    }

    [[nodiscard]] constexpr auto FitsIn(std::uint32_t frame_width,
                                        std::uint32_t frame_height) const
      noexcept -> bool
    {
      return !Empty() && x + width <= frame_width && y + height <= frame_height;
    }

    [[nodiscard]] constexpr auto operator==(Region const&) const noexcept
      -> bool = default;
  };

  // The spelling a caller writes a crop in, `x,y,width,height`.
  [[nodiscard]] auto RegionFrom(std::string_view text) -> Result<Region>;
}

namespace tash::perception
{
  using detail::region::Region;
  using detail::region::RegionFrom;
}
