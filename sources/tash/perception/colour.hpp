#pragma once
// A colour in eight bits a channel, the unit a per-colour call is asked in.

#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <string_view>

namespace tash::perception::detail::colour
{
  using utilities::Result;

  inline constexpr char SEPARATOR{ ',' };

  inline constexpr std::uint32_t CHANNEL_MAX{ 255u };

  struct Colour
  {
    std::uint32_t red{};
    std::uint32_t green{};
    std::uint32_t blue{};

    [[nodiscard]] constexpr auto InRange() const noexcept -> bool
    {
      return red <= CHANNEL_MAX && green <= CHANNEL_MAX
             && blue <= CHANNEL_MAX;
    }

    [[nodiscard]] constexpr auto operator==(Colour const&) const noexcept
      -> bool = default;
  };

  // The spelling a caller writes a colour in, `r,g,b`.
  [[nodiscard]] auto ColourFrom(std::string_view text) -> Result<Colour>;
}

namespace tash::perception
{
  using detail::colour::CHANNEL_MAX;
  using detail::colour::Colour;
  using detail::colour::ColourFrom;
}
