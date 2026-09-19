#pragma once
// A named number in the guest's memory: which region holds it, where in
// that region it sits, and the format its bytes read back in.

#include "tash/utilities/outcome.hpp"
#include "tash/watches/number-format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace tash::watches::detail::watch
{
  using utilities::Result;
  using number_format::NumberFormat;

  inline constexpr std::string_view SYSTEM_RAM{ "system" };

  struct Watch
  {
    std::string name;
    std::string region{ SYSTEM_RAM };
    std::uint32_t address{ 0 };
    NumberFormat format{};

    auto operator==(Watch const&) const -> bool = default;
  };

  [[nodiscard]] inline auto ReadWatch(Watch const& definition,
                                      std::span<std::byte const> region)
    -> Result<std::int64_t>
  {
    return number_format::ReadNumber(region, definition.address,
                                     definition.format);
  }
}

namespace tash::watches
{
  using detail::watch::ReadWatch;
  using detail::watch::SYSTEM_RAM;
  using detail::watch::Watch;
}
