#pragma once

#include "tash/trace/format.hpp"

#include <cstdint>
#include <string>

namespace tash::trace::detail::header
{
  struct Header
  {
    std::uint16_t format_version{ format::FORMAT_VERSION };
    std::int64_t  creation_time{ 0 };  // nanoseconds since the unix epoch, UTC
    std::string   producer{ };

    [[nodiscard]] static auto Now(std::string producer) -> Header;

    auto operator == (Header const&) const -> bool = default;
  };
}

namespace tash::trace
{
  using detail::header::Header;
}
