#include "tash/perception/region.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <system_error>

namespace tash::perception::detail::region
{
  using utilities::Refused;

  constexpr std::size_t    CORNERS{ 4u };
  constexpr std::ptrdiff_t SEPARATORS{ CORNERS - 1u };

  auto RegionFrom(std::string_view text) -> Result<Region>
  {
    // Three separators exactly, so a number's text never runs off the end.
    if (std::ranges::count(text, SEPARATOR) != SEPARATORS)
      return Refused("a region is x,y,width,height, not '{}'", text);

    std::array<std::uint32_t, CORNERS> corners{ };
    std::size_t at{ 0 };
    for (std::uint32_t& corner : corners)
    {
      std::size_t const end{ std::min(text.find(SEPARATOR, at),
                                      text.size()) };
      auto const [stopped, failed]{ std::from_chars(
        text.data() + at, text.data() + end, corner) };
      if (failed != std::errc{ } || stopped != text.data() + end)
        return Refused("a region is x,y,width,height, not '{}'", text);
      at = end + 1;
    }
    return Region{ corners[0], corners[1], corners[2], corners[3] };
  }
}
