#pragma once
// The guest's memory as named spans. Taken once: retro_get_memory_data
// hands back the same pointers for the life of a session.

#include "tash/session/session.hpp"
#include "tash/utilities/outcome.hpp"
#include "tash/watches/number-format.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tash::watches::detail::memory_map
{
  using utilities::Result;

  struct MemoryArea
  {
    std::string name;
    std::span<std::byte const> bytes;

    // How the core lays the region out, which a string read needs and a
    // number read is told by the watch instead.
    number_format::Endianness order{ number_format::Endianness::BIG };
  };

  class MemoryMap
  {
  public:
    MemoryMap() = default;
    explicit MemoryMap(std::vector<MemoryArea> areas);

    [[nodiscard]] static auto Of(session::Session const& run) -> MemoryMap;

    [[nodiscard]] auto Empty() const noexcept -> bool;
    [[nodiscard]] auto Areas() const noexcept
      -> std::vector<MemoryArea> const&;
    [[nodiscard]] auto Area(std::string_view name) const
      -> Result<std::span<std::byte const>>;
    [[nodiscard]] auto Order(std::string_view name) const
      -> Result<number_format::Endianness>;

  private:
    // The one refusal that lists what the core does expose.
    [[nodiscard]] auto Unknown(std::string_view name) const
      -> Result<std::span<std::byte const>>;

    std::vector<MemoryArea> _areas;
  };
}

namespace tash::watches
{
  using detail::memory_map::MemoryArea;
  using detail::memory_map::MemoryMap;
}
