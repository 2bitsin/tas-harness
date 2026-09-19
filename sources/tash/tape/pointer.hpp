#pragma once
// Where a pointer went: `<frame> <device> <x>,<y>` lines, a block of its own
// beside a segment's transitions, because a position moves every frame the
// hand does and a button does not.

#include "tash/tape/channel.hpp"
#include "tash/tape/names.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tash::tape::detail::pointer
{
  using utilities::Result;
  using names::POINT_SEPARATOR;

  struct PointerMove
  {
    std::uint64_t frame{ 0 };
    std::size_t   port{ 0 };
    std::int32_t  x{ 0 };
    std::int32_t  y{ 0 };

    [[nodiscard]] constexpr auto operator == (PointerMove const&)
      const noexcept -> bool = default;
  };

  // Frames must not go backwards: the player walks the list once.
  [[nodiscard]] auto PointerMovesFrom(std::string_view text)
    -> Result<std::vector<PointerMove>>;

  [[nodiscard]] auto TextOf(std::span<PointerMove const> moves) -> std::string;
}

namespace tash::tape
{
  using detail::pointer::PointerMove;
  using detail::pointer::PointerMovesFrom;
  using detail::pointer::TextOf;
}
