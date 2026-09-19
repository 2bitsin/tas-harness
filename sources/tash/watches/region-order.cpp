#include "tash/watches/region-order.hpp"

namespace tash::watches::detail::region_order
{
  namespace
  {
    inline constexpr std::uint32_t SWAP{ 1u };

    [[nodiscard]] auto OffsetOf(std::uint32_t address,
                                number_format::Endianness order) noexcept
      -> std::size_t
    {
      return order == number_format::Endianness::SWAPPED ? address ^ SWAP
                                                         : address;
    }
  }

  auto RegionOrder(std::string_view core, std::string_view region) noexcept
    -> number_format::Endianness
  {
    if (core == SWAPPED_CORE && region == SWAPPED_REGION)
      return number_format::Endianness::SWAPPED;
    return number_format::Endianness::BIG;
  }

  auto TextIn(std::span<std::byte const> region, std::uint32_t address,
              std::size_t count, number_format::Endianness order)
    -> std::string
  {
    std::string read;
    read.reserve(count);
    for (std::size_t step{ 0 }; step < count; ++step)
    {
      std::size_t const at{ OffsetOf(
        address + static_cast<std::uint32_t>(step), order) };
      if (at >= region.size())
        break;
      char const letter{ static_cast<char>(region[at]) };
      if (letter == '\0')
        break;
      read.push_back(letter);
    }
    return read;
  }
}
