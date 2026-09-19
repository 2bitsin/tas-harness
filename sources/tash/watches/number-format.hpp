#pragma once
// How a run of bytes in the guest's memory reads back as a number: how many
// of them, which way round, and whether the top bit is a sign.

#include "tash/utilities/outcome.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace tash::watches::detail::number_format
{
  using utilities::Result;

  inline constexpr std::uint32_t WIDTH_BYTE{ 1u };
  inline constexpr std::uint32_t WIDTH_WORD{ 2u };
  inline constexpr std::uint32_t WIDTH_LONG{ 4u };

  enum class Endianness : std::uint8_t
  {
    LITTLE,
    BIG,
    // Little-endian 16-bit words in big-endian order, which is how a 68000
    // longword reads back out of Genesis Plus GX's byte-swapped work ram.
    SWAPPED,
  };

  struct NumberFormat
  {
    std::uint32_t width{ WIDTH_WORD };
    Endianness endianness{ Endianness::BIG };
    bool is_signed{ false };

    auto operator==(NumberFormat const&) const -> bool = default;
  };

  [[nodiscard]] auto EndiannessOf(std::string_view named)
    -> Result<Endianness>;

  [[nodiscard]] auto NameOf(Endianness endianness) noexcept
    -> std::string_view;

  [[nodiscard]] auto WidthChecked(std::uint32_t width) -> Result<std::uint32_t>;

  [[nodiscard]] auto ReadNumber(std::span<std::byte const> region,
                                std::uint32_t at, NumberFormat const& format)
    -> Result<std::int64_t>;
}

namespace tash::watches
{
  using detail::number_format::Endianness;
  using detail::number_format::EndiannessOf;
  using detail::number_format::NameOf;
  using detail::number_format::NumberFormat;
  using detail::number_format::ReadNumber;
  using detail::number_format::WIDTH_BYTE;
  using detail::number_format::WIDTH_LONG;
  using detail::number_format::WIDTH_WORD;
  using detail::number_format::WidthChecked;
}
