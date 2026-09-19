#pragma once
// The order a region's bytes sit in, which the core decides, and the text a
// run of them reads back as. SWAPPED is address R at offset R xor 1, which
// is how Genesis Plus GX hands 68000 work ram over; BIG is address R at R.

#include "tash/watches/number-format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace tash::watches::detail::region_order
{
  inline constexpr std::string_view SWAPPED_CORE{ "Genesis Plus GX" };
  inline constexpr std::string_view SWAPPED_REGION{ "system" };

  [[nodiscard]] auto RegionOrder(std::string_view core,
                                 std::string_view region) noexcept
    -> number_format::Endianness;

  // The bytes from that address in address order, cut at the first null.
  [[nodiscard]] auto TextIn(std::span<std::byte const> region,
                            std::uint32_t address, std::size_t count,
                            number_format::Endianness order) -> std::string;
}

namespace tash::watches
{
  using detail::region_order::RegionOrder;
  using detail::region_order::TextIn;
}
