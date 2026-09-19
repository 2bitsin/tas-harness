#include "tash/watches/watch-set.hpp"

#include <algorithm>
#include <utility>

namespace tash::watches::detail::watch_set
{
  using utilities::Forwarded;
  using utilities::Refused;
  using utilities::Result;

  WatchSet::WatchSet(std::vector<Watch> watches)
  : _watches{ std::move(watches) }
  {
  }

  auto WatchSet::From(std::vector<WatchSpec> const& specs) -> Result<WatchSet>
  {
    std::vector<Watch> watches;
    watches.reserve(specs.size());
    for (WatchSpec const& spec : specs)
    {
      Result<Watch> const made{ watch_spec::WatchFrom(spec) };
      if (!made)
        return Forwarded(made);
      auto const clash{ std::ranges::find(watches, made->name, &Watch::name) };
      if (clash != watches.end())
        return Refused("watches: two watches are named {}", made->name);
      watches.push_back(*made);
    }
    return WatchSet{ std::move(watches) };
  }

  auto WatchSet::Count() const noexcept -> std::size_t
  {
    return _watches.size();
  }

  auto WatchSet::Empty() const noexcept -> bool
  {
    return _watches.empty();
  }

  auto WatchSet::All() const noexcept -> std::vector<Watch> const&
  {
    return _watches;
  }

  auto WatchSet::At(std::size_t index) const -> Result<Watch>
  {
    if (index >= _watches.size())
      return Refused("watches: no watch {} among {}", index, _watches.size());
    return _watches[index];
  }

  auto WatchSet::IndexOf(std::string_view name) const -> Result<std::size_t>
  {
    auto const found{ std::ranges::find(_watches, name, &Watch::name) };
    if (found == _watches.end())
      return Refused("watches: no watch is named {}", name);
    return static_cast<std::size_t>(found - _watches.begin());
  }
}
