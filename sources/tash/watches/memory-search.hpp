#pragma once
// The classic memory hunt: take every address as a candidate, then drop the
// ones that did not move the way the value on screen did.

#include "tash/utilities/outcome.hpp"
#include "tash/watches/memory-map.hpp"
#include "tash/watches/number-format.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace tash::watches::detail::memory_search
{
  using utilities::Outcome;
  using utilities::Result;
  using memory_map::MemoryMap;
  using number_format::NumberFormat;

  enum class Comparison : std::uint8_t
  {
    EQUAL,
    CHANGED,
    INCREASED,
    DECREASED,
    VALUE,
  };

  struct Candidate
  {
    std::uint32_t area{ 0 };
    std::uint32_t address{ 0 };
    std::int64_t value{ 0 };

    auto operator==(Candidate const&) const -> bool = default;
  };

  [[nodiscard]] auto ComparisonOf(std::string_view named)
    -> Result<Comparison>;
  [[nodiscard]] auto NameOf(Comparison how) noexcept -> std::string_view;

  class MemorySearch
  {
  public:
    // A stride of zero steps by the width, which is where a number of that
    // width sits on every machine tash drives.
    MemorySearch(MemoryMap memory, NumberFormat format,
                 std::uint32_t stride = 0);

    // Seeds every address the first time; later takes the values the
    // surviving candidates hold now as the baseline the next narrow uses.
    auto Snapshot() -> Outcome;

    auto Narrow(Comparison how, std::int64_t against = 0) -> Outcome;

    [[nodiscard]] auto Candidates() const noexcept
      -> std::vector<Candidate> const&;
    [[nodiscard]] auto Count() const noexcept -> std::size_t;
    [[nodiscard]] auto Seeded() const noexcept -> bool;
    [[nodiscard]] auto AreaName(std::uint32_t area) const -> std::string_view;
    [[nodiscard]] auto Format() const noexcept -> NumberFormat;
    [[nodiscard]] auto Stride() const noexcept -> std::uint32_t;

  private:
    [[nodiscard]] auto Current(Candidate const& candidate) const
      -> Result<std::int64_t>;

    MemoryMap _memory;
    NumberFormat _format;
    std::uint32_t _stride;
    std::vector<Candidate> _candidates;
    bool _seeded{ false };
  };
}

namespace tash::watches
{
  using detail::memory_search::Candidate;
  using detail::memory_search::Comparison;
  using detail::memory_search::ComparisonOf;
  using detail::memory_search::MemorySearch;
  using detail::memory_search::NameOf;
}
