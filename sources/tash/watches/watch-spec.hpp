#pragma once
// What a profile spells out, all scalars a YAML file can hold; an address
// is a string so it can be written the way memory is read, in hex.

#include "tash/utilities/outcome.hpp"
#include "tash/watches/watch.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace tash::watches::detail::watch_spec
{
  using utilities::Result;
  using watch::Watch;

  struct WatchSpec
  {
    friend constexpr auto reflect_scheme(WatchSpec*);

    std::string name;
    std::string region{ std::string{ watch::SYSTEM_RAM } };
    std::string address;
    std::uint32_t width{ number_format::WIDTH_WORD };
    std::string endian{ "big" };
    bool is_signed{ false };
  };

  [[nodiscard]] auto AddressOf(std::string_view text) -> Result<std::uint32_t>;

  [[nodiscard]] auto WatchFrom(WatchSpec const& spec) -> Result<Watch>;
}

namespace tash::watches
{
  using detail::watch_spec::AddressOf;
  using detail::watch_spec::WatchFrom;
  using detail::watch_spec::WatchSpec;
}
