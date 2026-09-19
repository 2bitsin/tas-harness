#pragma once
// What a watch anchor asks; the table behind it is the memory module's.

#include <cstdint>
#include <optional>
#include <string_view>

namespace tash::tape::detail::watch_source
{
  class WatchSource
  {
  public:
    WatchSource()                                      = default;
    virtual ~WatchSource()                             = default;
    WatchSource(WatchSource const&)                    = delete;
    auto operator = (WatchSource const&) -> WatchSource& = delete;

    // Empty for a name nobody samples, which refuses the anchor.
    [[nodiscard]] virtual auto Value(std::string_view name) const
      -> std::optional<std::int64_t> = 0;
  };
}

namespace tash::tape
{
  using detail::watch_source::WatchSource;
}
