#pragma once
// The lines a segment holds: `<frame> <down|up> <channel>`, the one part of a
// tape a human writes by hand, so every refusal carries its line number.

#include "tash/tape/channel.hpp"
#include "tash/tape/names.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tash::tape::detail::transitions
{
  using utilities::Result;
  using names::DOWN_WORD;
  using names::UP_WORD;

  struct Transition
  {
    std::uint64_t   frame{ 0 };
    bool            down{ false };
    channel::Channel channel{ };

    [[nodiscard]] auto operator == (Transition const&) const noexcept
      -> bool = default;
  };

  // Frames must not go backwards: the player walks the list once.
  [[nodiscard]] auto TransitionsFrom(std::string_view text)
    -> Result<std::vector<Transition>>;

  [[nodiscard]] auto TextOf(std::span<Transition const> moves) -> std::string;
}

namespace tash::tape
{
  using detail::transitions::TextOf;
  using detail::transitions::Transition;
  using detail::transitions::TransitionsFrom;
}
