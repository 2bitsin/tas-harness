#include "tash/watches/number-format.hpp"

namespace tash::watches::detail::number_format
{
  using utilities::Refused;
  using utilities::Result;

  namespace
  {
    constexpr std::uint32_t BITS_PER_BYTE{ 8u };

    [[nodiscard]] constexpr auto ByteAt(std::uint32_t at, std::uint32_t width,
                                        std::uint32_t step,
                                        Endianness endianness) noexcept
      -> std::uint32_t
    {
      switch (endianness)
      {
        case Endianness::BIG:
          return at + step;
        case Endianness::LITTLE:
          return at + width - 1u - step;
        case Endianness::SWAPPED:
          return width == WIDTH_BYTE
                   ? at
                   : at + (step & ~std::uint32_t{ 1 }) + 1u - (step & 1u);
      }
      return at + step;
    }

    [[nodiscard]] auto Signed(std::uint64_t raw, std::uint32_t width) noexcept
      -> std::int64_t
    {
      auto const bits{ width * BITS_PER_BYTE };
      auto const sign{ std::uint64_t{ 1 } << (bits - 1u) };
      if ((raw & sign) == 0u)
        return static_cast<std::int64_t>(raw);
      auto const span{ std::uint64_t{ 1 } << bits };
      return static_cast<std::int64_t>(raw) - static_cast<std::int64_t>(span);
    }
  }

  auto EndiannessOf(std::string_view named) -> Result<Endianness>
  {
    if (named == "big")
      return Endianness::BIG;
    if (named == "little")
      return Endianness::LITTLE;
    if (named == "swapped")
      return Endianness::SWAPPED;
    return Refused("watches: {} is not an endianness; say big, little or "
                   "swapped", named);
  }

  auto NameOf(Endianness endianness) noexcept -> std::string_view
  {
    switch (endianness)
    {
      case Endianness::BIG:     return "big";
      case Endianness::LITTLE:  return "little";
      case Endianness::SWAPPED: return "swapped";
    }
    return "big";
  }

  auto WidthChecked(std::uint32_t width) -> Result<std::uint32_t>
  {
    if (width != WIDTH_BYTE && width != WIDTH_WORD && width != WIDTH_LONG)
      return Refused("watches: {} is not a width; say 1, 2 or 4", width);
    return width;
  }

  auto ReadNumber(std::span<std::byte const> region, std::uint32_t at,
                  NumberFormat const& format) -> Result<std::int64_t>
  {
    Result<std::uint32_t> const width{ WidthChecked(format.width) };
    if (!width)
      return utilities::Forwarded(width);
    if (std::size_t{ at } + *width > region.size())
      return Refused("watches: address {:#x} of width {} is past the "
                     "region's {} bytes",
                     at, *width, region.size());

    std::uint64_t raw{ 0 };
    for (std::uint32_t step{ 0 }; step < *width; ++step)
    {
      auto const from{ ByteAt(at, *width, step, format.endianness) };
      raw = (raw << BITS_PER_BYTE) | std::to_integer<std::uint64_t>(
                                       region[from]);
    }
    return format.is_signed ? Signed(raw, *width)
                            : static_cast<std::int64_t>(raw);
  }
}
