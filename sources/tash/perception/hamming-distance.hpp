#pragma once

#include <bit>
#include <cstdint>

namespace tash::perception::detail::hamming_distance
{
  [[nodiscard]] constexpr auto HammingDistance(std::uint64_t left,
                                               std::uint64_t right) noexcept
    -> std::uint32_t
  {
    return static_cast<std::uint32_t>(std::popcount(left ^ right));
  }
}

namespace tash::perception
{
  using detail::hamming_distance::HammingDistance;
}
