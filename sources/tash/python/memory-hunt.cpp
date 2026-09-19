#include "tash/python/memory-hunt.hpp"

#include <algorithm>
#include <iterator>
#include <span>
#include <utility>

namespace tash::python::detail::memory_hunt
{
  using utilities::Forwarded;
  using utilities::Outcome;
  using utilities::Refused;
  using watches::Comparison;
  using watches::MemoryArea;
  using watches::MemoryMap;
  using watches::MemorySearch;
  using watches::NumberFormat;

  MemoryHunt::MemoryHunt(MemoryMap region, NumberFormat format,
                         std::uint32_t stride)
  : _region{ std::move(region) }, _format{ format }, _stride{ stride },
    _search{ _region, format, stride }
  {
  }

  auto MemoryHunt::Of(MemoryMap const& memory, std::string region,
                      NumberFormat format, std::uint32_t stride)
    -> Result<MemoryHunt>
  {
    if (Result<std::uint32_t> const width{ watches::WidthChecked(
          format.width) };
        !width)
      return Forwarded(width);

    Result<std::span<std::byte const>> const area{ memory.Area(region) };
    if (!area)
      return Forwarded(area);

    std::vector<MemoryArea> one;
    one.push_back(MemoryArea{ std::move(region), *area });
    return MemoryHunt{ MemoryMap{ std::move(one) }, format, stride };
  }

  auto MemoryHunt::Step(std::string_view how,
                        std::optional<std::int64_t> value)
    -> Result<std::size_t>
  {
    Result<Comparison> const comparison{ watches::ComparisonOf(how) };
    if (!comparison)
      return Forwarded(comparison);
    if (*comparison == Comparison::VALUE && !value)
      return Refused("python: a value step needs the number on the screen");
    if (*comparison != Comparison::VALUE && value)
      return Refused("python: {} reads the step before it, so it takes no "
                     "value; say value {} to look for a number",
                     watches::NameOf(*comparison), *value);

    if (!_search.Seeded())
    {
      if (Outcome const taken{ _search.Snapshot() }; !taken)
        return Forwarded(taken);
      return _search.Count();
    }

    if (Outcome const narrowed{ _search.Narrow(*comparison,
                                               value.value_or(0)) };
        !narrowed)
      return Forwarded(narrowed);
    return _search.Count();
  }

  auto MemoryHunt::Candidates(std::size_t limit) const
    -> std::vector<watches::Candidate>
  {
    std::vector<watches::Candidate> const& survivors{ _search.Candidates() };
    auto const shown{ std::min(limit, survivors.size()) };
    return std::vector<watches::Candidate>{
      survivors.begin(),
      std::next(survivors.begin(), static_cast<std::ptrdiff_t>(shown)) };
  }

  auto MemoryHunt::Count() const noexcept -> std::size_t
  {
    return _search.Count();
  }

  auto MemoryHunt::Seeded() const noexcept -> bool
  {
    return _search.Seeded();
  }

  auto MemoryHunt::Reset() -> void
  {
    _search = MemorySearch{ _region, _format, _stride };
  }

  // Of is the only way in and it puts exactly one area in the map.
  auto MemoryHunt::Region() const noexcept -> std::string const&
  {
    return _region.Areas().front().name;
  }

  auto MemoryHunt::Format() const noexcept -> NumberFormat
  {
    return _format;
  }

  auto MemoryHunt::Stride() const noexcept -> std::uint32_t
  {
    return _search.Stride();
  }
}
