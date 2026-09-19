#include "tash/watches/watch-spec.hpp"

#include <charconv>

namespace tash::watches::detail::watch_spec
{
  using utilities::Forwarded;
  using utilities::Refused;
  using utilities::Result;

  namespace
  {
    constexpr std::string_view HEX_PREFIX{ "0x" };
    constexpr int HEX_BASE{ 16 };
    constexpr int DECIMAL_BASE{ 10 };
  }

  auto AddressOf(std::string_view text) -> Result<std::uint32_t>
  {
    auto const hexadecimal{ text.starts_with(HEX_PREFIX) };
    auto const digits{ hexadecimal ? text.substr(HEX_PREFIX.size()) : text };
    std::uint32_t address{ 0 };
    auto const read{ std::from_chars(
      digits.data(), digits.data() + digits.size(), address,
      hexadecimal ? HEX_BASE : DECIMAL_BASE) };
    if (read.ec != std::errc{} || read.ptr != digits.data() + digits.size())
      return Refused("watches: {} is not an address; say 0xff0000 or 16711680",
                     text);
    return address;
  }

  auto WatchFrom(WatchSpec const& spec) -> Result<Watch>
  {
    if (spec.name.empty())
      return Refused("watches: a watch with no name cannot be read back");

    Result<std::uint32_t> const address{ AddressOf(spec.address) };
    if (!address)
      return Forwarded(address);
    Result<std::uint32_t> const width{ number_format::WidthChecked(
      spec.width) };
    if (!width)
      return Forwarded(width);
    Result<number_format::Endianness> const endianness{
      number_format::EndiannessOf(spec.endian) };
    if (!endianness)
      return Forwarded(endianness);

    return Watch{ spec.name, spec.region, *address,
                  number_format::NumberFormat{ *width, *endianness,
                                               spec.is_signed } };
  }
}
