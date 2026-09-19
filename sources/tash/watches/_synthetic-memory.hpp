#pragma once
// Memory the tests own: a vector of bytes and a map over it, so the same
// code path a session drives runs with no core loaded.

#include "tash/watches/memory-map.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace tash::watches::testing
{
  inline constexpr std::size_t AREA_BYTES{ 64u };

  class SyntheticMemory
  {
  public:
    explicit SyntheticMemory(std::size_t bytes = AREA_BYTES)
    : _bytes(bytes, std::byte{ 0 })
    {
    }

    auto Poke(std::size_t at, std::initializer_list<std::uint8_t> values)
      -> void
    {
      std::size_t step{ 0 };
      for (std::uint8_t const value : values)
        _bytes[at + step++] = std::byte{ value };
    }

    auto Write16(std::size_t at, std::uint16_t value) -> void
    {
      _bytes[at] = static_cast<std::byte>(value >> 8);
      _bytes[at + 1u] = static_cast<std::byte>(value & 0xFFu);
    }

    [[nodiscard]] auto Bytes() noexcept -> std::vector<std::byte>&
    {
      return _bytes;
    }

    [[nodiscard]] auto View() const noexcept -> std::span<std::byte const>
    {
      return _bytes;
    }

    [[nodiscard]] auto Map(std::string name = "system") const -> MemoryMap
    {
      std::vector<MemoryArea> areas;
      areas.push_back(MemoryArea{ std::move(name), View() });
      return MemoryMap{ std::move(areas) };
    }

  private:
    std::vector<std::byte> _bytes;
  };
}
