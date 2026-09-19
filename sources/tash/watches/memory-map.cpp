#include "tash/watches/memory-map.hpp"

#include "tash/watches/region-order.hpp"

#include <algorithm>
#include <format>
#include <utility>

namespace tash::watches::detail::memory_map
{
  using utilities::Refused;
  using utilities::Result;

  MemoryMap::MemoryMap(std::vector<MemoryArea> areas)
  : _areas{ std::move(areas) }
  {
  }

  auto MemoryMap::Of(session::Session const& run) -> MemoryMap
  {
    std::string_view const core{ run.CoreOf().Information().name };
    std::vector<MemoryArea> areas;
    for (session::MemoryRegion const& region : run.Memory())
      areas.push_back(MemoryArea{
        std::string{ region.name },
        std::span<std::byte const>{ region.bytes },
        region_order::RegionOrder(core, region.name) });
    return MemoryMap{ std::move(areas) };
  }

  auto MemoryMap::Empty() const noexcept -> bool
  {
    return _areas.empty();
  }

  auto MemoryMap::Areas() const noexcept -> std::vector<MemoryArea> const&
  {
    return _areas;
  }

  auto MemoryMap::Area(std::string_view name) const
    -> Result<std::span<std::byte const>>
  {
    auto const found{ std::ranges::find(_areas, name, &MemoryArea::name) };
    if (found != _areas.end())
      return found->bytes;
    return Unknown(name);
  }

  auto MemoryMap::Order(std::string_view name) const
    -> Result<number_format::Endianness>
  {
    auto const found{ std::ranges::find(_areas, name, &MemoryArea::name) };
    if (found != _areas.end())
      return found->order;
    return std::unexpected{ Unknown(name).error() };
  }

  auto MemoryMap::Unknown(std::string_view name) const
    -> Result<std::span<std::byte const>>
  {
    std::string known;
    for (MemoryArea const& area : _areas)
      known += std::format("{}{}", known.empty() ? "" : ", ", area.name);
    return Refused("watches: this core has no {} memory; it has {}", name,
                   known.empty() ? "none" : known);
  }
}
