#pragma once
// The values a scenario reads by name, and a tape's watch anchors ask for:
// a run hands one object to both. A run without one answers nothing.

#include "tash/tape/watch-source.hpp"

#include <string>
#include <vector>

namespace tash::python::detail::watch_values
{
  class WatchValues : public tape::WatchSource
  {
  public:
    [[nodiscard]] virtual auto Names() const -> std::vector<std::string> = 0;
  };
}

namespace tash::python
{
  using detail::watch_values::WatchValues;
}
