#pragma once

#include <string_view>

namespace tash::tape::detail::mark_observer
{
  // Told of a mark the moment it is made.
  class MarkObserver
  {
  public:
    MarkObserver()                                       = default;
    virtual ~MarkObserver()                              = default;
    MarkObserver(MarkObserver const&)                    = delete;
    auto operator = (MarkObserver const&) -> MarkObserver& = delete;

    virtual auto OnMark(std::string_view text) -> void = 0;
  };
}

namespace tash::tape
{
  using detail::mark_observer::MarkObserver;
}
