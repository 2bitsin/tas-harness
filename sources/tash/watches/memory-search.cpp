#include "tash/watches/memory-search.hpp"

#include <utility>

namespace tash::watches::detail::memory_search
{
  using utilities::Forwarded;
  using utilities::Refused;
  using utilities::Result;

  namespace
  {
    [[nodiscard]] auto Keeps(Comparison how, std::int64_t before,
                             std::int64_t now, std::int64_t against) noexcept
      -> bool
    {
      switch (how)
      {
        case Comparison::EQUAL:     return now == before;
        case Comparison::CHANGED:   return now != before;
        case Comparison::INCREASED: return now > before;
        case Comparison::DECREASED: return now < before;
        case Comparison::VALUE:     return now == against;
      }
      return false;
    }
  }

  auto ComparisonOf(std::string_view named) -> Result<Comparison>
  {
    if (named == "equal")
      return Comparison::EQUAL;
    if (named == "changed")
      return Comparison::CHANGED;
    if (named == "increased")
      return Comparison::INCREASED;
    if (named == "decreased")
      return Comparison::DECREASED;
    if (named == "value")
      return Comparison::VALUE;
    return Refused("watches: {} is not a comparison; say equal, changed, "
                   "increased, decreased or value",
                   named);
  }

  auto NameOf(Comparison how) noexcept -> std::string_view
  {
    switch (how)
    {
      case Comparison::EQUAL:     return "equal";
      case Comparison::CHANGED:   return "changed";
      case Comparison::INCREASED: return "increased";
      case Comparison::DECREASED: return "decreased";
      case Comparison::VALUE:     return "value";
    }
    return "unknown";
  }

  MemorySearch::MemorySearch(MemoryMap memory, NumberFormat format,
                             std::uint32_t stride)
  : _memory{ std::move(memory) }, _format{ format },
    _stride{ stride == 0u ? format.width : stride }
  {
  }

  auto MemorySearch::Snapshot() -> Outcome
  {
    if (Result<std::uint32_t> const width{ number_format::WidthChecked(
          _format.width) };
        !width)
      return Forwarded(width);

    if (_seeded)
    {
      for (Candidate& candidate : _candidates)
      {
        Result<std::int64_t> const now{ Current(candidate) };
        if (!now)
          return Forwarded(now);
        candidate.value = *now;
      }
      return {};
    }

    if (_memory.Empty())
      return Refused("watches: this core exposes no memory to search");

    for (std::size_t area{ 0 }; area < _memory.Areas().size(); ++area)
    {
      auto const bytes{ _memory.Areas()[area].bytes.size() };
      for (std::size_t at{ 0 }; at + _format.width <= bytes; at += _stride)
      {
        Candidate candidate{ static_cast<std::uint32_t>(area),
                             static_cast<std::uint32_t>(at), 0 };
        Result<std::int64_t> const now{ Current(candidate) };
        if (!now)
          return Forwarded(now);
        candidate.value = *now;
        _candidates.push_back(candidate);
      }
    }
    _seeded = true;
    return {};
  }

  auto MemorySearch::Narrow(Comparison how, std::int64_t against) -> Outcome
  {
    if (!_seeded)
      return Refused("watches: narrow {} before any snapshot", NameOf(how));

    std::vector<Candidate> kept;
    kept.reserve(_candidates.size());
    for (Candidate const& candidate : _candidates)
    {
      Result<std::int64_t> const now{ Current(candidate) };
      if (!now)
        return Forwarded(now);
      if (Keeps(how, candidate.value, *now, against))
        kept.push_back(Candidate{ candidate.area, candidate.address, *now });
    }
    _candidates = std::move(kept);
    return {};
  }

  auto MemorySearch::Current(Candidate const& candidate) const
    -> Result<std::int64_t>
  {
    return number_format::ReadNumber(_memory.Areas()[candidate.area].bytes,
                                     candidate.address, _format);
  }

  auto MemorySearch::Candidates() const noexcept
    -> std::vector<Candidate> const&
  {
    return _candidates;
  }

  auto MemorySearch::Count() const noexcept -> std::size_t
  {
    return _candidates.size();
  }

  auto MemorySearch::Seeded() const noexcept -> bool
  {
    return _seeded;
  }

  auto MemorySearch::AreaName(std::uint32_t area) const -> std::string_view
  {
    return area < _memory.Areas().size() ? _memory.Areas()[area].name
                                         : std::string_view{ "unknown" };
  }

  auto MemorySearch::Format() const noexcept -> NumberFormat
  {
    return _format;
  }

  auto MemorySearch::Stride() const noexcept -> std::uint32_t
  {
    return _stride;
  }
}
