#include "tash/perception/colour.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <system_error>

namespace tash::perception::detail::colour
{
  using utilities::Refused;

  constexpr std::size_t    CHANNELS{ 3u };
  constexpr std::ptrdiff_t SEPARATORS{ CHANNELS - 1u };

  auto ColourFrom(std::string_view text) -> Result<Colour>
  {
    // Two separators exactly, so a channel's text never runs off the end.
    if (std::ranges::count(text, SEPARATOR) != SEPARATORS)
      return Refused("a colour is r,g,b, not '{}'", text);

    std::array<std::uint32_t, CHANNELS> channels{ };
    std::size_t at{ 0 };
    for (std::uint32_t& channel : channels)
    {
      std::size_t const end{ std::min(text.find(SEPARATOR, at),
                                      text.size()) };
      auto const [stopped, failed]{ std::from_chars(
        text.data() + at, text.data() + end, channel) };
      if (failed != std::errc{ } || stopped != text.data() + end)
        return Refused("a colour is r,g,b, not '{}'", text);
      at = end + 1;
    }

    Colour const read{ channels[0], channels[1], channels[2] };
    if (!read.InRange())
      return Refused("a channel runs 0 to {}, so '{}' is not a colour",
                     CHANNEL_MAX, text);
    return read;
  }
}
